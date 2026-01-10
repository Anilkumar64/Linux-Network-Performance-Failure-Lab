#pragma once
#include "connection.h"
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <chrono>

struct Metrics
{
  uint64_t connections_accepted = 0;
  uint64_t connections_closed = 0;
  uint64_t bytes_read = 0;
  uint64_t bytes_written = 0;
  uint64_t frames_received = 0;
};

class Server
{
public:
  Server(int listen_fd, int max_connections);
  void run();
  void stop();

private:
  void handle_accept();
  void handle_client_read(int fd);
  void handle_client_write(int fd);
  void handle_message(Connection &conn, const std::vector<uint8_t> &msg);
  void queue_frame(Connection &conn, const std::vector<uint8_t> &payload);
  void on_frame_received(Connection &, const std::vector<uint8_t> &);
  void close_connection(int fd, const char *reason);

  void add_fd_to_epoll(int fd, uint32_t events);
  void mod_fd_epoll(int fd, uint32_t events);
  void remove_fd_from_epoll(int fd);

  int listen_fd_;
  int epoll_fd_;
  std::unordered_map<int, Connection> connections_;
  std::atomic<bool> running_;
  int max_connections_;

  static constexpr std::chrono::seconds IDLE_TIMEOUT{30};
  Metrics metrics_;
};
