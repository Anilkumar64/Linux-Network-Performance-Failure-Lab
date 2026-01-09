#pragma once

#include <cstdint>
#include <string>

struct ServerConfig {
  uint16_t port;
  int max_connections;
  int backlog;
  int recv_buffer_bytes;
  int send_buffer_bytes;

  enum class LogLevel { DEBUG, INFO, WARN, ERROR } log_level;

  static ServerConfig defaults() {
    ServerConfig cfg{};
    cfg.port = 8080;
    cfg.max_connections = 10000;
    cfg.backlog = 1024;
    cfg.recv_buffer_bytes = 64 * 1024;
    cfg.send_buffer_bytes = 64 * 1024;
    cfg.log_level = LogLevel::INFO;
    return cfg;
  }
};
