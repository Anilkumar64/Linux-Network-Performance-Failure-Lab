#pragma once
#include "connection.h"
#include <cstdint>
#include <unordered_map>

class Server {
public:
  explicit Server(int listen_fd);
  void run();

private:
  void handle_accept();
  void handle_client_read(int fd);
  void handle_client_write(int fd);

  void add_fd_to_epoll(int fd, uint32_t events);
  void mod_fd_epoll(int fd, uint32_t events);
  void remove_fd_from_epoll(int fd);

  int listen_fd_;
  int epoll_fd_;
  std::unordered_map<int, Connection> connections_;
};
