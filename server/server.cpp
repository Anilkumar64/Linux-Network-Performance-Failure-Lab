#include "server.h"
#include "connection.h"
#include "socket_utils.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

// ---------- constructor ----------

Server::Server(int listen_fd) : listen_fd_(listen_fd) {
  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) {
    std::perror("epoll_create1");
    std::exit(EXIT_FAILURE);
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = listen_fd_;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
    std::perror("epoll_ctl ADD listen_fd");
    std::exit(EXIT_FAILURE);
  }
}

// ---------- epoll helpers ----------

void Server::add_fd_to_epoll(int fd, uint32_t events) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
    std::perror("epoll_ctl ADD");
    ::close(fd);
  }
}

void Server::mod_fd_epoll(int fd, uint32_t events) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
    std::perror("epoll_ctl MOD");
    ::close(fd);
  }
}

void Server::remove_fd_from_epoll(int fd) {
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

// ---------- accept ----------

void Server::handle_accept() {
  while (true) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);

    int client_fd =
        ::accept(listen_fd_, reinterpret_cast<sockaddr *>(&addr), &len);

    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;

      std::perror("accept");
      break;
    }

    set_nonblocking(client_fd);

    connections_.emplace(client_fd, Connection{client_fd});

    add_fd_to_epoll(client_fd, EPOLLIN);

    std::cout << "Accepted client fd=" << client_fd << "\n";
  }
}

// ---------- read ----------

void Server::handle_client_read(int fd) {
  char buf[4096];

  while (true) {
    ssize_t n = ::read(fd, buf, sizeof(buf));

    if (n > 0) {
      auto it = connections_.find(fd);
      if (it == connections_.end()) {
        // Should never happen, but protects us from corruption
        remove_fd_from_epoll(fd);
        ::close(fd);
        return;
      }

      auto &conn = it->second;

      conn.write_buffer.insert(conn.write_buffer.end(), buf, buf + n);

      mod_fd_epoll(fd, EPOLLIN | EPOLLOUT);
    } else if (n == 0) {
      std::cout << "Client closed fd=" << fd << "\n";
      remove_fd_from_epoll(fd);
      ::close(fd);
      connections_.erase(fd);
      return;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;

      std::perror("read");
      remove_fd_from_epoll(fd);
      ::close(fd);
      connections_.erase(fd);
      return;
    }
  }
}

// ---------- write ----------

void Server::handle_client_write(int fd) {
  auto it = connections_.find(fd);
  if (it == connections_.end()) {
    // Should never happen, but protects us from corruption
    remove_fd_from_epoll(fd);
    ::close(fd);
    return;
  }

  auto &conn = it->second;

  while (!conn.write_buffer.empty()) {
    ssize_t n = ::write(fd, conn.write_buffer.data(), conn.write_buffer.size());

    if (n > 0) {
      conn.write_buffer.erase(conn.write_buffer.begin(),
                              conn.write_buffer.begin() + n);
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;

      std::perror("write");
      remove_fd_from_epoll(fd);
      ::close(fd);
      connections_.erase(fd);
      return;
    }
  }

  mod_fd_epoll(fd, EPOLLIN);
}

// ---------- event loop ----------

void Server::run() {
  std::cout << "epoll event loop started\n";

  constexpr int MAX_EVENTS = 16;
  epoll_event events[MAX_EVENTS];

  while (true) {
    int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);

    if (n < 0) {
      if (errno == EINTR)
        continue;

      std::perror("epoll_wait");
      std::exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n; ++i) {
      int fd = events[i].data.fd;
      uint32_t ev = events[i].events;

      if (fd == listen_fd_) {
        handle_accept();
        continue;
      }

      if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        remove_fd_from_epoll(fd);
        ::close(fd);
        connections_.erase(fd);
        continue;
      }

      if (ev & EPOLLIN)
        handle_client_read(fd);

      if (ev & EPOLLOUT)
        handle_client_write(fd);
    }
  }
}
