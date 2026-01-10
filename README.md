# Linux Networking Lab — Epoll-Based TCP Command Server

## Overview

This project is a **production-grade TCP command server for Linux**, written in **C++17**, built from first principles using **non-blocking sockets and epoll**.

It is **not a tutorial** and **not a toy echo server**.

The goal of this project is to **demonstrate real systems and network engineering competence** by building a server that is:

* Correct under TCP stream semantics
* Robust against real-world failure modes
* Hardened against abuse
* Observable and debuggable
* Architecturally sound and maintainable

This repository represents the **completed result** of a multi-phase Linux networking lab.

---

## Key Characteristics

* **Language**: C++17
* **Platform**: Linux only
* **I/O Model**: Non-blocking, epoll-based
* **Architecture**: Single-threaded event loop
* **Protocol**: Length-prefixed framed TCP protocol
* **Focus**: Correctness, robustness, lifecycle management

No GUI, no frameworks, no blocking I/O.

---

## What This Project Is (and Is Not)

### ✅ This project **is**

* A hardened TCP command server
* A demonstration of epoll-based server design
* A study in TCP correctness (FIN, RST, partial reads/writes)
* A proof of systems-level engineering skill

### ❌ This project **is not**

* A chat application
* An HTTP server
* A GUI application
* A toy echo server

The **infrastructure** is the product.

---

## Architecture Overview

### High-Level Flow

```
Client
  ↓
TCP Socket (non-blocking)
  ↓
epoll event loop
  ↓
Per-connection state machine
  ↓
Framed protocol parsing
  ↓
Command handling
  ↓
Queued non-blocking response
```

### Core Components

```
server/
├── main.cpp            # Process startup, CLI parsing, signal handling
├── server.h/.cpp       # epoll loop, accept/read/write, protocol logic
├── connection.h/.cpp   # Per-connection state and buffers
├── socket_utils.h/.cpp # Socket setup utilities
├── config.h            # Configuration and validation
```

---

## Connection Model

* One `Server` instance
* One `Connection` object per client file descriptor
* Each connection tracks:

  * Read buffer
  * Write buffer
  * Framing state
  * Activity timestamps
  * Flood counters

Connections are fully lifecycle-managed and cleaned up through a **single centralized teardown path**.

---

## Protocol Definition

The server implements a **length-prefixed framed TCP protocol**:

```
[4 bytes length][payload]
```

* Length is a 32-bit unsigned integer (network byte order)
* Payload is ASCII command text
* TCP packet boundaries are never assumed

### Supported Commands

| Command      | Description                      |
| ------------ | -------------------------------- |
| `PING`       | Returns `PONG`                   |
| `ECHO <msg>` | Echoes `<msg>` back              |
| `STATS`      | Returns server metrics           |
| `CLOSE`      | Closes the client connection     |
| `SHUTDOWN`   | Gracefully shuts down the server |

---

## Example Interaction

```bash
# Start server
./network_server --port 9090
```

```bash
# Send PING
printf "\x00\x00\x00\x04PING" | nc localhost 9090
```

Response:

```
PONG
```

---

## Hardening & Safety Features

This server was intentionally hardened against real-world failure modes.

### Implemented Protections

* Max connections limit
* Max frame size enforcement (≤ 1 MB)
* Write buffer backpressure
* Flood protection (frames/sec per connection)
* Idle connection eviction
* Immediate disconnect on protocol violations
* Clean fd lifecycle management

---

## TCP Correctness & Chaos Testing

This project was **chaos tested**, not just unit tested.

### Verified Failure Modes

* Partial frame delivery (byte-by-byte sends)
* Abrupt client resets (RST storms)
* Graceful client shutdowns (FIN handling)
* Half-open connections
* Write-side backpressure overflow
* Idle connections
* Shutdown during load

### Validation Guarantees

* No crashes
* No fd leaks
* No busy loops
* No double closes
* Metrics always converge (`accepted == closed`)

---

## Metrics & Observability

The server maintains internal metrics including:

* Connections accepted
* Connections closed
* Bytes read / written
* Frames received
* Active connections

Metrics can be queried at runtime using:

```
STATS
```

Or via signal:

```bash
kill -USR1 <server_pid>
```

---

## Clean Shutdown Semantics

The server supports **two clean shutdown paths**:

1. **Signal-based**: `SIGINT`, `SIGTERM`
2. **Protocol-based**: `SHUTDOWN` command

In both cases:

* New connections stop
* Existing connections drain
* Event loop exits cleanly
* All file descriptors are closed

---

## Build & Run Instructions

### Requirements

* Linux
* CMake ≥ 3.x
* GCC or Clang with C++17 support

### Build

```bash
mkdir build
cd build
cmake ..
make -j
```

### Run

```bash
./bin/network_server --port 9090
```

---

## Client Code

The repository includes a minimal TCP client used **only for testing**.

The client is **not part of the core product**.
It exists to validate protocol behavior and server correctness.

In real usage, the server can be driven by:

* `nc`
* Python scripts
* Monitoring tools
* A future GUI controller

---

## What This Project Demonstrates

By completing this project, the following skills are demonstrated:

* Linux epoll programming
* TCP stream semantics
* Non-blocking I/O design
* Correct fd lifecycle management
* Defensive protocol design
* Systems debugging mindset
* Failure-driven testing

This project reflects **real infrastructure engineering**, not academic examples.

---

## Project Status

✅ **Completed and verified**

The core server is considered **feature-complete and correct**.
Future extensions (GUI, TLS, multithreading) would be built **on top**, not inside this code.

---



### Final Note

> The absence of crashes is the success.
> Correctness under failure is the goal.

---


