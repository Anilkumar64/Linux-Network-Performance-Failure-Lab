#include "server.h"
#include "connection.h"
#include "socket_utils.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>
#include <unistd.h>
static std::atomic<bool> dump_metrics_requested{false};

// ---------- constructor ----------

void Server::stop()
{
  std::cout << "\nShutdown signal received. Stopping server...\n";

  running_ = false;

  // Stop accepting new connections
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, listen_fd_, nullptr);
  ::close(listen_fd_);
}

static Server *g_server = nullptr;
static void handle_signal(int sig)
{
  if (sig == SIGUSR1)
  {
    dump_metrics_requested.store(true, std::memory_order_relaxed);
    return;
  }

  if (g_server)
  {
    g_server->stop();
  }
}

Server::Server(int listen_fd, int max_connections)
    : listen_fd_(listen_fd), running_(true), max_connections_(max_connections)
{
  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0)
  {
    std::perror("epoll_create1");
    std::exit(EXIT_FAILURE);
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = listen_fd_;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0)
  {
    std::perror("epoll_ctl ADD listen_fd");
    std::exit(EXIT_FAILURE);
  }
}

void Server::add_fd_to_epoll(int fd, uint32_t events)
{
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0)
  {
    std::perror("epoll_ctl ADD");
    ::close(fd);
  }
}

void Server::mod_fd_epoll(int fd, uint32_t events)
{
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0)
  {
    std::perror("epoll_ctl MOD");
    ::close(fd);
  }
}

void Server::remove_fd_from_epoll(int fd)
{
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

void Server::handle_accept()
{
  while (true)
  {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);

    int client_fd =
        ::accept(listen_fd_, reinterpret_cast<sockaddr *>(&addr), &len);

    if (client_fd < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;

      std::perror("accept");
      break;
    }

    // 🔒 ENFORCE LIMIT — FIRST THING
    if ((int)connections_.size() >= max_connections_)
    {
      std::cerr << "Rejecting client fd=" << client_fd
                << " (max_connections reached: " << connections_.size()
                << ")\n";
      ::close(client_fd);
      continue; // IMPORTANT
    }

    set_nonblocking(client_fd);

    connections_.emplace(client_fd, Connection(client_fd));
    metrics_.connections_accepted++;

    add_fd_to_epoll(client_fd, EPOLLIN);

    std::cout << "Accepted client fd=" << client_fd
              << " (active=" << connections_.size() << ")\n";
  }
}

void Server::on_frame_received(Connection &conn,
                               const std::vector<uint8_t> &frame)
{
  auto now = Connection::Clock::now();
  if (now - conn.window_start > std::chrono::seconds(1))
  {
    conn.frames_in_window = 0;
    conn.window_start = now;
  }

  if (++conn.frames_in_window > 1000)
  {
    std::cerr << "[ABUSE] frame flood fd=" << conn.fd << "\n";
    remove_fd_from_epoll(conn.fd);
    ::close(conn.fd);
    connections_.erase(conn.fd);
    metrics_.connections_closed++;
    return;
  }

  metrics_.bytes_read += frame.size();
  conn.last_activity = Connection::Clock::now();

  std::string cmd(reinterpret_cast<const char *>(frame.data()),
                  frame.size());

  // Trim trailing whitespace
  while (!cmd.empty() &&
         (cmd.back() == '\n' || cmd.back() == '\r' || cmd.back() == ' '))
  {
    cmd.pop_back();
  }

  if (cmd == "PING")
  {
    const std::string resp = "PONG";
    queue_frame(conn,
                std::vector<uint8_t>(resp.begin(), resp.end()));
    return;
  }

  if (cmd.rfind("ECHO ", 0) == 0)
  {
    const std::string payload = cmd.substr(5);
    queue_frame(conn,
                std::vector<uint8_t>(payload.begin(), payload.end()));
    return;
  }

  if (cmd == "STATS")
  {
    std::string out;
    out += "connections=" + std::to_string(connections_.size()) + "\n";
    out += "accepted=" + std::to_string(metrics_.connections_accepted) + "\n";
    out += "closed=" + std::to_string(metrics_.connections_closed) + "\n";
    out += "frames=" + std::to_string(metrics_.frames_received) + "\n";
    out += "bytes_read=" + std::to_string(metrics_.bytes_read) + "\n";
    out += "bytes_written=" + std::to_string(metrics_.bytes_written);

    queue_frame(conn,
                std::vector<uint8_t>(out.begin(), out.end()));
    return;
  }

  if (cmd == "CLOSE")
  {
    const std::string resp = "OK";
    queue_frame(conn,
                std::vector<uint8_t>(resp.begin(), resp.end()));

    // EPOLLOUT will flush, then client closes
    return;
  }
  // Shutdown button
  if (cmd == "SHUTDOWN")
  {
    const std::string resp = "OK";
    queue_frame(conn,
                std::vector<uint8_t>(resp.begin(), resp.end()));

    std::cout << "[CONTROL] shutdown requested\n";
    stop(); // sets running_ = false, removes listen fd
    return;
  }

  const std::string err = "ERR unknown command";
  queue_frame(conn,
              std::vector<uint8_t>(err.begin(), err.end()));
}

void Server::handle_message(Connection &conn,
                            const std::vector<uint8_t> &msg)
{
  metrics_.frames_received++;
  on_frame_received(conn, msg);
}

void Server::queue_frame(Connection &conn,
                         const std::vector<uint8_t> &payload)
{
  uint32_t len = htonl(payload.size());

  size_t off = conn.write_buffer.size();
  conn.write_buffer.resize(off + 4 + payload.size());

  std::memcpy(conn.write_buffer.data() + off, &len, 4);
  std::memcpy(conn.write_buffer.data() + off + 4,
              payload.data(),
              payload.size());

  if (conn.write_buffer.size() > Connection::WRITE_HIGH_WATER)
  {
    std::cerr << "[BACKPRESSURE] fd=" << conn.fd << " write buffer overflow\n";
    remove_fd_from_epoll(conn.fd);
    ::close(conn.fd);
    connections_.erase(conn.fd);
    metrics_.connections_closed++;
    return;
  }

  // arm EPOLLOUT only if connection survives
  mod_fd_epoll(conn.fd, EPOLLIN | EPOLLOUT);
}

// ---------- read ----------

void Server::handle_client_read(int fd)
{
  auto it = connections_.find(fd);
  if (it == connections_.end())
    return;

  Connection &conn = it->second;
  uint8_t buf[4096];

  while (true)
  {
    ssize_t n = ::read(fd, buf, sizeof(buf));

    if (n > 0)
    {
      conn.last_activity = Connection::Clock::now();

      conn.read_buffer.insert(conn.read_buffer.end(), buf, buf + n);
    }
    else if (n == 0)
    {
      close_connection(fd, "client FIN");
      return;
    }
    else
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;

      std::perror("read");
      close_connection(fd, "read error");
      return;
    }

    // ---------- framing state machine ----------
    while (true)
    {
      // Step 1: read length
      if (conn.state == Connection::ReadState::READ_LEN)
      {
        if (conn.read_buffer.size() < sizeof(uint32_t))
          break;

        uint32_t netlen;
        std::memcpy(&netlen, conn.read_buffer.data(), sizeof(uint32_t));
        conn.expected_len = ntohl(netlen);

        // Defensive limit
        if (conn.expected_len == 0 || conn.expected_len > 1024 * 1024)
        {
          std::cerr << "Protocol violation fd=" << fd
                    << " len=" << conn.expected_len << "\n";
          close_connection(fd, "write on unknown fd");
          return;
        }

        conn.read_buffer.erase(conn.read_buffer.begin(),
                               conn.read_buffer.begin() + sizeof(uint32_t));

        conn.state = Connection::ReadState::READ_BODY;
      }

      // Step 2: read payload
      if (conn.state == Connection::ReadState::READ_BODY)
      {
        if (conn.read_buffer.size() < conn.expected_len)
          break;

        std::vector<uint8_t> frame(conn.read_buffer.begin(),
                                   conn.read_buffer.begin() +
                                       conn.expected_len);

        conn.read_buffer.erase(conn.read_buffer.begin(),
                               conn.read_buffer.begin() + conn.expected_len);

        conn.state = Connection::ReadState::READ_LEN;
        conn.expected_len = 0;

        // 🔼 Deliver frame upward (Phase 3.1 = no-op)
        on_frame_received(conn, frame);
      }
    }
  }
}

// ---------- write ----------

void Server::handle_client_write(int fd)
{
  auto it = connections_.find(fd);
  if (it == connections_.end())
  {
    close_connection(fd, "write on unknown fd");
    ::close(fd);
    return;
  }

  auto &conn = it->second;

  constexpr size_t MAX_WRITE_PER_TICK = 64 * 1024;
  size_t written_this_tick = 0;

  while (!conn.write_buffer.empty() &&
         written_this_tick < MAX_WRITE_PER_TICK)
  {

    ssize_t n = ::write(fd,
                        conn.write_buffer.data(),
                        conn.write_buffer.size());

    if (n > 0)
    {
      conn.last_activity = Connection::Clock::now();

      written_this_tick += static_cast<size_t>(n);

      conn.write_buffer.erase(
          conn.write_buffer.begin(),
          conn.write_buffer.begin() + n);
    }
    else
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // Kernel buffer full — wait for next EPOLLOUT
        return;
      }

      std::perror("write");
      close_connection(fd, "write error");
      return;
    }
  }

  // Stop EPOLLOUT if nothing left to write
  if (conn.write_buffer.empty())
  {
    mod_fd_epoll(fd, EPOLLIN);
  }
}

// ---------- event loop ----------

void Server::run()
{
  g_server = this;

  // ---------- signal setup ----------
  struct sigaction sa{};
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGUSR1, &sa, nullptr);

  std::cout << "epoll event loop started\n";

  constexpr int MAX_EVENTS = 16;
  epoll_event events[MAX_EVENTS];

  auto last_log = Connection::Clock::now();

  // ---------- main loop ----------
  while (running_)
  {

    auto now = Connection::Clock::now();

    // ---------- idle timeout sweep ----------
    for (auto it = connections_.begin(); it != connections_.end();)
    {
      if (now - it->second.last_activity > IDLE_TIMEOUT)
      {
        std::cout << "Closing idle fd=" << it->first << "\n";
        remove_fd_from_epoll(it->first);
        ::close(it->first);
        it = connections_.erase(it);
        metrics_.connections_closed++;
      }
      else
      {
        ++it;
      }
    }

    // ---------- metrics logging ----------
    if (dump_metrics_requested.exchange(false))
    {
      std::cout << "[metrics dump] "
                << "active=" << connections_.size()
                << " accepted=" << metrics_.connections_accepted
                << " closed=" << metrics_.connections_closed
                << " frames=" << metrics_.frames_received
                << " read_bytes=" << metrics_.bytes_read
                << " written_bytes=" << metrics_.bytes_written
                << "\n";
      last_log = now;
    }

    // ---------- wait for I/O ----------
    int ready = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);

    if (ready < 0)
    {
      if (errno == EINTR)
        continue;

      std::perror("epoll_wait");
      std::exit(EXIT_FAILURE);
    }

    // ---------- handle events ----------
    for (int i = 0; i < ready; ++i)
    {
      int fd = events[i].data.fd;
      uint32_t ev = events[i].events;

      if (fd == listen_fd_)
      {
        handle_accept();
        continue;
      }

      if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
      {
        close_connection(fd, "epoll error/hup");
        continue;
      }

      if (ev & EPOLLIN)
        handle_client_read(fd);

      if (ev & EPOLLOUT)
        handle_client_write(fd);
    }
  }

  // ---------- shutdown ----------
  std::cout << "Draining connections...\n";

  for (auto &[fd, conn] : connections_)
  {
    ::close(fd);
  }

  connections_.clear();
  ::close(epoll_fd_);

  std::cout << "Server shutdown complete.\n";
}
void Server::close_connection(int fd, const char *reason)
{
  auto it = connections_.find(fd);
  if (it == connections_.end())
    return;

  std::cerr << "[CLOSE] fd=" << fd;
  if (reason)
    std::cerr << " reason=" << reason;
  std::cerr << "\n";

  close_connection(fd, "idle timeout");
}
