/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * cppnet is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cppnet is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cppnet.  If not, see <https://www.gnu.org/licenses/>.
 */

// NOLINTBEGIN
#include "net/service/async_udp_service.hpp"
#include "net/service/context_thread.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
using namespace net::service;

struct echo_service : public async_udp_service<echo_service> {
  using Base = async_udp_service<echo_service>;
  using socket_message = io::socket::socket_message<>;

  template <typename T>
  explicit echo_service(socket_address<T> address) : Base(address)
  {}

  bool initialized = false;
  auto initialize(const socket_handle &sock) -> std::error_code
  {
    if (initialized)
      return std::make_error_code(std::errc::invalid_argument);

    initialized = true;
    return {};
  }

  auto echo(async_context &ctx, const socket_dialog &socket,
            const std::shared_ptr<read_context> &rctx,
            socket_message msg) -> void
  {
    using namespace io::socket;
    using namespace stdexec;

    sender auto sendmsg = io::sendmsg(socket, msg, 0) |
                          then([&, socket, rctx, msg](auto &&len) mutable {
                            reader(ctx, socket, std::move(rctx));
                          }) |
                          upon_error([](auto &&error) {});

    ctx.scope.spawn(std::move(sendmsg));
  }

  auto operator()(async_context &ctx, const socket_dialog &socket,
                  std::shared_ptr<read_context> rctx,
                  std::span<const std::byte> buf) -> void
  {
    using namespace io::socket;
    if (!rctx)
      return;

    auto address = *rctx->msg.address;
    if (address->sin6_family == AF_INET)
    {
      const auto *ptr =
          reinterpret_cast<struct sockaddr *>(std::addressof(*address));
      address = socket_address<sockaddr_in>(ptr);
    }
    echo(ctx, socket, rctx, {.address = address, .buffers = buf});
  }
};

class AsyncUdpServiceV4Test : public ::testing::Test {};

TEST_F(AsyncUdpServiceV4Test, StartTest)
{
  using namespace io::socket;

  auto ctx = async_context{};
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  auto service = echo_service{addr};

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
    if (sigmask & (1 << ctx.terminate))
      ctx.scope.request_stop();
  };

  service.start(ctx);
  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncUdpServiceV4Test, EchoTest)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);
  auto service = echo_service(addr);

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
    if (sigmask & (1 << ctx.terminate))
      ctx.scope.request_stop();
  };

  ASSERT_FALSE(service.initialized);
  service.start(ctx);
  {
    ASSERT_TRUE(service.initialized);
    ASSERT_FALSE(ctx.scope.get_stop_token().stop_requested());

    using namespace io;
    auto sock = socket_handle(AF_INET, SOCK_DGRAM, 0);
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message<sockaddr_in>{
        .address = {socket_address<sockaddr_in>()}, .buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      ASSERT_EQ(sendmsg(sock,
                        socket_message{.address = {addr},
                                       .buffers = std::span(it, 1)},
                        0),
                1);
      ctx.poller.wait();
      ASSERT_EQ(recvmsg(sock, msg, 0), 1);
      EXPECT_EQ(*msg.address, addr);
      EXPECT_EQ(buf[0], *it);
    }
  }

  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncUdpServiceV4Test, InitializeError)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);
  auto service = echo_service(addr);
  service.initialized = true;

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
    if (sigmask & (1 << ctx.terminate))
      ctx.scope.request_stop();
  };

  service.start(ctx);
  EXPECT_TRUE(ctx.scope.get_stop_token().stop_requested());

  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncUdpServiceV4Test, AsyncServiceTest)
{
  using namespace io::socket;
  using service_type = context_thread<echo_service>;

  auto service = service_type{};

  std::mutex mtx;
  std::condition_variable cvar;
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);

  service.start(mtx, cvar, addr);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.interrupt || service.stopped; });
  }
  ASSERT_FALSE(service.stopped.load());
  {
    using namespace io;
    auto sock = socket_handle(AF_INET, SOCK_DGRAM, 0);
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message<sockaddr_in>{
        .address = {socket_address<sockaddr_in>()}, .buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      ASSERT_EQ(sendmsg(sock,
                        socket_message<sockaddr_in>{
                            .address = {addr}, .buffers = std::span(it, 1)},
                        0),
                1);
      ASSERT_EQ(recvmsg(sock, msg, 0), 1);
      EXPECT_EQ(*msg.address, addr);
      EXPECT_EQ(buf[0], *it);
    }
  }
}

class AsyncUdpServiceV6Test : public ::testing::Test {};

TEST_F(AsyncUdpServiceV6Test, StartTestV6)
{
  using namespace io::socket;

  auto ctx = async_context{};
  auto addr = socket_address<sockaddr_in6>();
  addr->sin6_family = AF_INET6;
  auto service = echo_service{addr};

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
    if (sigmask & (1 << ctx.terminate))
      ctx.scope.request_stop();
  };

  service.start(ctx);
  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncUdpServiceV6Test, EchoTest)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in6>();
  addr->sin6_family = AF_INET6;
  addr->sin6_port = htons(8081);
  auto service = echo_service(addr);

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
    if (sigmask & (1 << ctx.terminate))
      ctx.scope.request_stop();
  };

  ASSERT_FALSE(service.initialized);
  service.start(ctx);
  {
    ASSERT_TRUE(service.initialized);
    ASSERT_FALSE(ctx.scope.get_stop_token().stop_requested());

    using namespace io;
    auto sock = socket_handle(AF_INET6, SOCK_DGRAM, 0);
    addr->sin6_addr = IN6ADDR_LOOPBACK_INIT;

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message<sockaddr_in6>{
        .address = {socket_address<sockaddr_in6>()}, .buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      ASSERT_EQ(sendmsg(sock,
                        socket_message<sockaddr_in6>{
                            .address = {addr}, .buffers = std::span(it, 1)},
                        0),
                1);
      ctx.poller.wait();
      ASSERT_EQ(recvmsg(sock, msg, 0), 1);
      EXPECT_EQ(*msg.address, addr);
      EXPECT_EQ(buf[0], *it);
    }
  }

  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncUdpServiceV6Test, InitializeError)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in6>();
  addr->sin6_family = AF_INET6;
  addr->sin6_port = htons(8081);
  auto service = echo_service(addr);
  service.initialized = true;

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
    if (sigmask & (1 << ctx.terminate))
      ctx.scope.request_stop();
  };

  service.start(ctx);
  EXPECT_TRUE(ctx.scope.get_stop_token().stop_requested());

  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncUdpServiceV6Test, AsyncServiceTest)
{
  using namespace io::socket;
  using service_type = context_thread<echo_service>;

  auto service = service_type{};

  std::mutex mtx;
  std::condition_variable cvar;
  auto addr = socket_address<sockaddr_in6>();
  addr->sin6_family = AF_INET6;
  addr->sin6_port = htons(8081);

  service.start(mtx, cvar, addr);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.interrupt || service.stopped; });
  }
  ASSERT_FALSE(service.stopped.load());
  {
    using namespace io;
    auto sock = socket_handle(AF_INET6, SOCK_DGRAM, 0);
    addr->sin6_addr = IN6ADDR_LOOPBACK_INIT;

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message<sockaddr_in6>{
        .address = {socket_address<sockaddr_in6>()}, .buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      ASSERT_EQ(sendmsg(sock,
                        socket_message<sockaddr_in6>{
                            .address = {addr}, .buffers = std::span(it, 1)},
                        0),
                1);
      ASSERT_EQ(recvmsg(sock, msg, 0), 1);
      EXPECT_EQ(*msg.address, addr);
      EXPECT_EQ(buf[0], *it);
    }
  }
}
// NOLINTEND
