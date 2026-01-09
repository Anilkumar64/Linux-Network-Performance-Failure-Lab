#pragma once
#include <vector>

struct Connection {
  int fd;
  std::vector<char> write_buffer;

  explicit Connection(int fd_) : fd(fd_), write_buffer() {}
};
