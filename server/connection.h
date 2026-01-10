#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <chrono>

struct Connection
{
  using Clock = std::chrono::steady_clock;

  int fd;
  bool write_blocked = false;

  Clock::time_point last_activity;

  std::vector<uint8_t> read_buffer;
  std::vector<uint8_t> write_buffer;

  static constexpr size_t WRITE_HIGH_WATER = 512 * 1024; // 512 KB
  static constexpr size_t WRITE_LOW_WATER = 128 * 1024;  // 128 KB

  enum class ReadState
  {
    READ_LEN,
    READ_BODY
  };
  ReadState state;
  uint32_t expected_len;

  explicit Connection(int fd_)
      : fd(fd_),
        write_blocked(false),
        last_activity(Clock::now()),
        state(ReadState::READ_LEN),
        expected_len(0) {}
  uint32_t frames_in_window = 0;
  Clock::time_point window_start = Clock::now();
};
