#pragma once

#include <cstdint>

// Creates, binds, and listens on a TCP socket.
// Returns listening fd on success.
// Exits the program on failure.
int create_listening_socket(uint16_t port, int backlog, int recv_buf_bytes,
                            int send_buf_bytes);
void set_nonblocking(int fd);
