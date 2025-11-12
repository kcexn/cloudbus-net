# Cpp Network Utils (cppnet)

[![Tests](https://github.com/kcexn/cloudbus-net/actions/workflows/tests.yml/badge.svg)](https://github.com/kcexn/cloudbus-net/actions/workflows/tests.yml)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/d4cf3af8003c430fa2c058bd4aa8da14)](https://app.codacy.com/gh/kcexn/cppnet/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Codacy Badge](https://app.codacy.com/project/badge/Coverage/d4cf3af8003c430fa2c058bd4aa8da14)](https://app.codacy.com/gh/kcexn/cppnet/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_coverage)

A header-only library for building asynchronous network services.

## Features

- **Header-only** - No compilation required, just include and go
- **TCP and UDP support** - Both protocols with the same consistent API

## Requirements

- **C++20 compiler** (GCC 10+, Clang 13+, MSVC 2022+)
- **CMake 3.28+**
- **Ninja** (recommended build system)
- Linux (other platforms may work but are untested)

## Installation

### Using CPM (Recommended)

Add cppnet to your CMake project using [CPM](https://github.com/cpm-cmake/CPM.cmake):

```cmake
include(cmake/CPM.cmake)

CPMAddPackage("gh:kcexn/cloudbus-net@0.4.1")

target_link_libraries(your_target PRIVATE cppnet)
```

### Using FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  cppnet
  GIT_REPOSITORY https://github.com/kcexn/cloudbus-net.git
  GIT_TAG v0.4.1
)
FetchContent_MakeAvailable(cppnet)

target_link_libraries(your_target PRIVATE cppnet)
```

### Manual Installation

```bash
git clone https://github.com/kcexn/cloudbus-net.git
cd cloudbus-net
cmake --preset release
sudo cmake --install build/release
```

## Quick Start

### TCP Echo Server

```cpp
#include <net/cppnet.hpp>
#include <arpa/inet.h>

using namespace net::service;

// Define your service by inheriting from async_tcp_service
struct echo_service : public async_tcp_service<echo_service> {
  using Base = async_tcp_service<echo_service>;

  // Constructor passes address to base class
  template <typename T>
  explicit echo_service(socket_address<T> address) : Base(address) {}

  // Optional: configure socket options
  auto initialize(const socket_handle &socket) -> std::error_code {
    // Set socket options here if needed
    return {};
  }

  // Optional: handle graceful shutdown
  auto stop() -> void {
    // Cleanup code here
  }

  // Handle incoming data - MUST call reader() to continue
  auto operator()(async_context &ctx, const socket_dialog &socket,
                  std::shared_ptr<read_context> rctx,
                  std::span<const std::byte> buf) -> void {
    using namespace io::socket;
    using namespace stdexec;

    // Echo the data back
    sender auto echo_sender =
      io::sendmsg(socket, socket_message{.buffers = buf}, 0) |
      then([&, socket, rctx](auto &&) {
        reader(ctx, socket, std::move(rctx)); // Continue reading
      }) |
      upon_error([](auto &&) {});

    ctx.scope.spawn(std::move(echo_sender));
  }
};

int main() {
  // Create IPv4 address on port 8080
  auto addr = io::socket::socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;
  addr->sin_port = htons(8080);

  // Create and start the service
  auto ctx = context_thread<echo_service>();
  auto mtx = std::mutex();
  auto cv = std::condition_variable();

  ctx.start(mtx, cv, addr);

  // Wait for termination signal
  std::unique_lock lock(mtx);
  cv.wait(lock, [&] { return ctx.stopped.load(); });

  return 0;
}
```

### UDP Echo Server

```cpp
#include <net/cppnet.hpp>
#include <arpa/inet.h>

using namespace net::service;

struct udp_echo_service : public async_udp_service<udp_echo_service> {
  using Base = async_udp_service<udp_echo_service>;

  template <typename T>
  explicit udp_echo_service(socket_address<T> address) : Base(address) {}

  auto operator()(async_context &ctx, const socket_dialog &socket,
                  std::shared_ptr<read_context> rctx,
                  std::span<const std::byte> buf) -> void {
    using namespace io::socket;
    using namespace stdexec;

    // Echo back to sender (address is in rctx->msg.address)
    sender auto echo_sender =
      io::sendmsg(socket, rctx->msg, 0) |
      then([&, socket, rctx](auto &&) {
        reader(ctx, socket, std::move(rctx));
      }) |
      upon_error([](auto &&) {});

    ctx.scope.spawn(std::move(echo_sender));
  }
};

int main() {
  auto addr = io::socket::socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;
  addr->sin_port = htons(8080);

  auto ctx = context_thread<udp_echo_service>();
  auto mtx = std::mutex();
  auto cv = std::condition_variable();

  ctx.start(mtx, cv, addr);

  std::unique_lock lock(mtx);
  cv.wait(lock, [&] { return ctx.stopped.load(); });

  return 0;
}
```

## Building from Source

```bash
# Clone the repository
git clone https://github.com/kcexn/cloudbus-net.git
cd cloudbus-net

# Debug build (with tests and coverage)
cmake --preset debug
cmake --build --preset debug

# Release build (optimized)
cmake --preset release
cmake --build --preset release

# Run tests
ctest --preset debug --output-on-failure
```

## Documentation

Generate API documentation with Doxygen:

```bash
cmake --preset debug -DCPPNET_BUILD_DOCS=ON
cmake --build --preset debug --target doxygen
```

Documentation will be in `build/debug/docs/html/index.html`.

## Architecture

The library uses the CRTP (Curiously Recurring Template Pattern) for services:

- **`async_context`** - Execution context with async_scope, I/O multiplexer, and signal handling
- **`context_thread<Service>`** - Runs a service in a dedicated thread
- **`async_tcp_service<Handler>`** - TCP server base class with accept/read loop
- **`async_udp_service<Handler>`** - UDP server base class with read loop

Your service inherits from the appropriate template and implements:

- `operator()` to handle received data (required)
- `initialize()` to configure the socket (optional)
- `stop()` for graceful shutdown (optional, TCP only)

## Signal Handling

Services support two signals:

- `terminate` (0) - Graceful shutdown
- `user1` (1) - Custom application signal

Send signals via `async_context::signal(int signum)`.

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please ensure:

- Code follows C++20 best practices
- All tests pass (`ctest --preset debug`)
- New features include tests and documentation
- Code coverage is maintained

## Dependencies

All dependencies are automatically fetched via CPM:

- [NVIDIA stdexec](https://github.com/NVIDIA/stdexec) - Sender/receiver framework
- [AsyncBerkeley](https://github.com/kcexn/async-berkeley) - Async socket operations
- [GoogleTest](https://github.com/google/googletest) - Testing framework (tests only)
