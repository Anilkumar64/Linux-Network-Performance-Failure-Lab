#include "config.h"
#include <cstdlib>
#include <iostream>

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
    std::cerr << "backlog cannot be greater than max_connections\n";
    return false;
  }

  if (cfg.recv_buffer_bytes < 4096 || cfg.send_buffer_bytes < 4096) {
    std::cerr << "socket buffer sizes must be >= 4096 bytes\n";
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // Step 1: create config with defaults
  ServerConfig config = ServerConfig::defaults();

  // Step 2: validate config (CLI override comes next)
  if (!validate_config(config)) {
    std::cerr << "Server configuration invalid. Exiting.\n";
    return EXIT_FAILURE;
  }

  std::cout << "network_server starting with validated configuration\n";
  std::cout << "port=" << config.port << " backlog=" << config.backlog
            << " max_connections=" << config.max_connections << "\n";

  return EXIT_SUCCESS;
}
