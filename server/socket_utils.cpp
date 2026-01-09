#include "socket_utils.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <fcntl.h>

void set_nonblocking(int fd) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    std::perror("fcntl(F_GETFL)");
    ::close(fd);
    std::exit(EXIT_FAILURE);
  }

  if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    std::perror("fcntl(F_SETFL)");
    ::close(fd);
    std::exit(EXIT_FAILURE);
  }
}

int create_listening_socket(uint16_t port, int backlog, int recv_buf_bytes,
                            int send_buf_bytes) {
  // 1. socket()
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    std::perror("socket");
    std::exit(EXIT_FAILURE);
  }

  // 2. SO_REUSEADDR
  int one = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
    std::perror("setsockopt(SO_REUSEADDR)");
    ::close(fd);
    std::exit(EXIT_FAILURE);
  }

  // 3. Set socket buffers
  if (::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_bytes,
                   sizeof(recv_buf_bytes)) < 0) {
    std::perror("setsockopt(SO_RCVBUF)");
    ::close(fd);
    std::exit(EXIT_FAILURE);
  }

  if (::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buf_bytes,
                   sizeof(send_buf_bytes)) < 0) {
    std::perror("setsockopt(SO_SNDBUF)");
    ::close(fd);
    std::exit(EXIT_FAILURE);
  }

  // 4. bind()
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    std::perror("bind");
    ::close(fd);
    std::exit(EXIT_FAILURE);
  }

  // 5. listen()
  if (::listen(fd, backlog) < 0) {
    std::perror("listen");
    ::close(fd);
    std::exit(EXIT_FAILURE);
  }

  return fd;
}
