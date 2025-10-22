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
#include "net/service/async_service.hpp"
#include "net/service/async_tcp_service.hpp"

#include <gtest/gtest.h>

#include <cassert>
#include <list>

#include <arpa/inet.h>
using namespace net::service;

struct echo_block_service : public async_tcp_service<echo_block_service> {
  using Base = async_tcp_service<echo_block_service>;
  using socket_message = io::socket::socket_message<>;

  template <typename T>
  explicit echo_block_service(socket_address<T> address) : Base(address)
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

    sender auto sendmsg =
        io::sendmsg(socket, msg, 0) |
        then([&, socket, rctx, bufs = msg.buffers](auto &&len) mutable {
          if (bufs += len)
            return echo(ctx, socket, std::move(rctx), {.buffers = bufs});

          reader(ctx, socket, std::move(rctx));
        }) |
        upon_error([](auto &&error) {});

    ctx.scope.spawn(std::move(sendmsg));
  }

  auto operator()(async_context &ctx, const socket_dialog &socket,
                  std::shared_ptr<read_context> rmsg,
                  std::span<const std::byte> buf) -> void
  {
    echo(ctx, socket, rmsg, {.buffers = buf});
  }
};

class AsyncTcpServiceTest : public ::testing::Test {};

TEST_F(AsyncTcpServiceTest, StartTest)
{
  using namespace io::socket;

  auto ctx = async_context{};
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  auto service = echo_block_service{addr};

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
  };

  service.start(ctx);
  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncTcpServiceTest, EchoTest)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);
  auto service = echo_block_service(addr);

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
  };

  ASSERT_FALSE(service.initialized);
  service.start(ctx);
  {
    ASSERT_TRUE(service.initialized);
    ASSERT_FALSE(ctx.scope.get_stop_token().stop_requested());

    using namespace io;
    auto sock = socket_handle(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");

    ASSERT_EQ(connect(sock, addr), 0);
    ctx.poller.wait();

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message{.buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      ASSERT_EQ(sendmsg(sock, socket_message{.buffers = std::span(it, 1)}, 0),
                1);
      ctx.poller.wait();
      ASSERT_EQ(recvmsg(sock, msg, 0), 1);
      EXPECT_EQ(buf[0], *it);
    }
  }

  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncTcpServiceTest, InitializeError)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);
  auto service = echo_block_service(addr);
  service.initialized = true;

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
  };

  service.start(ctx);
  EXPECT_TRUE(ctx.scope.get_stop_token().stop_requested());

  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncTcpServiceTest, AsyncServiceTest)
{
  using namespace io::socket;
  using service_type = async_service<echo_block_service>;

  auto list = std::list<service_type>{};
  auto &service = list.emplace_back();

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
  ASSERT_TRUE(static_cast<bool>(service.interrupt));
  {
    using namespace io;
    auto sock = socket_handle(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");

    ASSERT_EQ(connect(sock, addr), 0);

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message{.buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      ASSERT_EQ(sendmsg(sock, socket_message{.buffers = std::span(it, 1)}, 0),
                1);
      ASSERT_EQ(recvmsg(sock, msg, 0), 1);
      EXPECT_EQ(buf[0], *it);
    }
  }
}
// NOLINTEND
