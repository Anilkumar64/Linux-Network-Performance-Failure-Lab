#include "config.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

static void print_usage(const char *prog) {
  std::cerr << "Usage: " << prog << " [options]\n"
            << "Options:\n"
            << "  --port <port>               Listening port (>=1024)\n"
            << "  --max-connections <num>     Maximum concurrent connections\n"
            << "  --backlog <num>             listen() backlog\n"
            << "  --recv-buffer <bytes>       Socket receive buffer size\n"
            << "  --send-buffer <bytes>       Socket send buffer size\n"
            << "  --log-level <debug|info|warn|error>\n";
}

static bool parse_int(const char *s, int &out) {
  char *end = nullptr;
  long v = std::strtol(s, &end, 10);
  if (!s || *end != '\0') {
    return false;
  }
  out = static_cast<int>(v);
  return true;
}

static bool validate_config(const ServerConfig &cfg) {
  if (cfg.port < 1024) {
    std::cerr << "Invalid port: must be >= 1024\n";
    return false;
  }

  if (cfg.max_connections <= 0) {
    std::cerr << "max_connections must be > 0\n";
    return false;
  }

  if (cfg.backlog <= 0) {
    std::cerr << "backlog must be > 0\n";
    return false;
  }

  if (cfg.backlog > cfg.max_connections) {
    std::cerr << "backlog cannot exceed max_connections\n";
    return false;
  }

  if (cfg.recv_buffer_bytes < 4096 || cfg.send_buffer_bytes < 4096) {
    std::cerr << "socket buffer sizes must be >= 4096 bytes\n";
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  ServerConfig cfg = ServerConfig::defaults();

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--port") == 0) {
      if (++i >= argc) {
        std::cerr << "Missing --port value\n";
        return EXIT_FAILURE;
      }

      int port = 0;
      if (!parse_int(argv[i], port)) {
        std::cerr << "Invalid --port value\n";
        return EXIT_FAILURE;
      }

      if (port < 0 || port > 65535) {
        std::cerr << "Port out of range\n";
        return EXIT_FAILURE;
      }

      cfg.port = static_cast<uint16_t>(port);
    }

    else if (std::strcmp(argv[i], "--max-connections") == 0) {
      if (++i >= argc || !parse_int(argv[i], cfg.max_connections)) {
        std::cerr << "Invalid --max-connections value\n";
        return EXIT_FAILURE;
      }
    } else if (std::strcmp(argv[i], "--backlog") == 0) {
      if (++i >= argc || !parse_int(argv[i], cfg.backlog)) {
        std::cerr << "Invalid --backlog value\n";
        return EXIT_FAILURE;
      }
    } else if (std::strcmp(argv[i], "--recv-buffer") == 0) {
      if (++i >= argc || !parse_int(argv[i], cfg.recv_buffer_bytes)) {
        std::cerr << "Invalid --recv-buffer value\n";
        return EXIT_FAILURE;
      }
    } else if (std::strcmp(argv[i], "--send-buffer") == 0) {
      if (++i >= argc || !parse_int(argv[i], cfg.send_buffer_bytes)) {
        std::cerr << "Invalid --send-buffer value\n";
        return EXIT_FAILURE;
      }
    } else if (std::strcmp(argv[i], "--log-level") == 0) {
      if (++i >= argc) {
        std::cerr << "Missing --log-level value\n";
        return EXIT_FAILURE;
      }
      if (std::strcmp(argv[i], "debug") == 0) {
        cfg.log_level = ServerConfig::LogLevel::DEBUG;
      } else if (std::strcmp(argv[i], "info") == 0) {
        cfg.log_level = ServerConfig::LogLevel::INFO;
      } else if (std::strcmp(argv[i], "warn") == 0) {
        cfg.log_level = ServerConfig::LogLevel::WARN;
      } else if (std::strcmp(argv[i], "error") == 0) {
        cfg.log_level = ServerConfig::LogLevel::ERROR;
      } else {
        std::cerr << "Invalid log level\n";
        return EXIT_FAILURE;
      }
    } else {
      std::cerr << "Unknown option: " << argv[i] << "\n";
      print_usage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (!validate_config(cfg)) {
    std::cerr << "Configuration validation failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "network_server starting with validated configuration\n";
  std::cout << "port=" << cfg.port << " backlog=" << cfg.backlog
            << " max_connections=" << cfg.max_connections << "\n";

  return EXIT_SUCCESS;
}
