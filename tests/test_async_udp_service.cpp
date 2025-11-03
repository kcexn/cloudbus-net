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

class AsyncUDPServiceTest : public ::testing::Test {
protected:
  auto SetUp() -> void override
  {
    using namespace stdexec;

    ctx = std::make_unique<async_context>();

    addr_v4 = socket_address<sockaddr_in>();
    addr_v4->sin_family = AF_INET;
    addr_v4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr_v4->sin_port = htons(port++);
    service_v4 = std::make_unique<echo_service>(addr_v4);
    server_v4 = std::make_unique<server_type>();

    addr_v6 = socket_address<sockaddr_in6>();
    addr_v6->sin6_family = AF_INET6;
    addr_v6->sin6_addr = in6addr_loopback;
    addr_v6->sin6_port = htons(port++);
    service_v6 = std::make_unique<echo_service>(addr_v6);
    server_v6 = std::make_unique<server_type>();

    int err =
        ::socketpair(AF_UNIX, SOCK_STREAM, 0, ctx->interrupt.sockets.data());
    ASSERT_EQ(err, 0);

    isr(ctx->poller.emplace(ctx->interrupt.sockets[0]), [&] {
      auto sigmask = ctx->sigmask.exchange(0);
      for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
      {
        if (mask & (1 << 0))
        {
          service_v4->signal_handler(signum);
          service_v6->signal_handler(signum);
        }
      }
      return !(sigmask & (1 << ctx->terminate));
    });

    is_empty = false;
    auto mark = std::atomic(false);
    wait_empty = std::jthread([&] {
      mark = true;
      cvar.notify_all();
      sync_wait(ctx->scope.on_empty() | then([&] { is_empty = true; }));
    });
    {
      auto lock = std::unique_lock(mtx);
      cvar.wait(lock, [&] { return mark.load(); });
    }
  }

  template <typename T> using socket_address = io::socket::socket_address<T>;
  using socket_dialog =
      io::socket::socket_dialog<io::execution::poll_multiplexer>;

  template <typename Fn>
    requires std::is_invocable_r_v<bool, Fn>
  auto isr(const socket_dialog &socket, Fn &&handler) -> void
  {
    using namespace stdexec;
    using namespace io::socket;
    static auto buf = std::array<std::byte, 1024>();
    static auto msg = socket_message{.buffers = buf};

    if (!handler())
    {
      ctx->scope.request_stop();
      return;
    }

    sender auto recvmsg =
        io::recvmsg(socket, msg, 0) |
        then([this, socket, handler](auto len) { isr(socket, handler); }) |
        upon_error([](auto &&error) {
          if constexpr (std::is_same_v<std::decay_t<decltype(error)>, int>)
          {
            auto msg = std::error_code(error, std::system_category()).message();
          }
        });

    ctx->scope.spawn(std::move(recvmsg));
  }

  using server_type = context_thread<echo_service>;

  unsigned short port = 8800;

  std::unique_ptr<async_context> ctx;
  std::atomic<bool> is_empty;
  std::jthread wait_empty;
  std::unique_ptr<echo_service> service_v4;
  std::unique_ptr<echo_service> service_v6;
  std::unique_ptr<server_type> server_v4;
  std::unique_ptr<server_type> server_v6;

  socket_address<sockaddr_in> addr_v4;
  socket_address<sockaddr_in6> addr_v6;
  std::mutex mtx;
  std::condition_variable cvar;

  auto TearDown() -> void override
  {
    if (ctx->interrupt.sockets[1] != io::socket::INVALID_SOCKET)
      io::socket::close(ctx->interrupt.sockets[1]);
    if (!is_empty)
    {
      ctx->signal(ctx->terminate);
      ctx->poller.wait();
    }
    service_v4.reset();
    service_v6.reset();
    ctx.reset();
    server_v4.reset();
    server_v6.reset();
  }
};

TEST_F(AsyncUDPServiceTest, StartTest)
{
  service_v4->start(*ctx);
  service_v6->start(*ctx);
  ctx->signal(ctx->terminate);

  auto n = 0UL;
  while (ctx->poller.wait_for(100))
  {
    ASSERT_LE(n++, 3);
  }
  EXPECT_GT(n, 0);
}

TEST_F(AsyncUDPServiceTest, EchoTest)
{
  service_v4->start(*ctx);
  service_v6->start(*ctx);

  {
    using namespace io;
    using namespace io::socket;

    auto sock_v4 = socket_handle(AF_INET, SOCK_DGRAM, 0);
    auto sock_v6 = socket_handle(AF_INET6, SOCK_DGRAM, 0);

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message{.buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      auto len = sendmsg(sock_v4,
                         socket_message<sockaddr_in>{
                             .address = {addr_v4}, .buffers = std::span(it, 1)},
                         0);
      ASSERT_EQ(len, 1);
      len = sendmsg(sock_v6,
                    socket_message<sockaddr_in6>{.address = {addr_v6},
                                                 .buffers = std::span(it, 1)},
                    0);
      ASSERT_EQ(len, 1);

      auto n = ctx->poller.wait_for(50);
      ASSERT_GT(n, 0);

      len = recvmsg(sock_v4, msg, 0);
      ASSERT_EQ(len, 1);
      EXPECT_EQ(buf[0], *it);

      len = recvmsg(sock_v6, msg, 0);
      ASSERT_EQ(len, 1);
      EXPECT_EQ(buf[0], *it);
    }
  }

  ctx->signal(ctx->terminate);
  auto n = 0UL;
  while (ctx->poller.wait_for(100))
  {
    ASSERT_LE(n++, 2);
  }
  ASSERT_GT(n, 0);
}

TEST_F(AsyncUDPServiceTest, InitializeError)
{
  using namespace io::socket;
  service_v4->initialized = true;
  service_v4->start(*ctx);
  EXPECT_TRUE(ctx->scope.get_stop_token().stop_requested());
}

TEST_F(AsyncUDPServiceTest, AsyncServerTest)
{
  using namespace io;
  using namespace io::socket;
  using enum async_context::signals;
  using enum async_context::context_states;

  server_v4->start(mtx, cvar, addr_v4);
  server_v6->start(mtx, cvar, addr_v6);
  {
    auto lock = std::unique_lock(mtx);
    cvar.wait(lock, [&] {
      return server_v4->state != PENDING && server_v6->state != PENDING;
    });
  }
  ASSERT_EQ(server_v4->state, STARTED);
  ASSERT_EQ(server_v6->state, STARTED);

  {
    auto sock_v4 = socket_handle(AF_INET, SOCK_DGRAM, 0);
    auto sock_v6 = socket_handle(AF_INET6, SOCK_DGRAM, 0);

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message{.buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      auto len = sendmsg(sock_v4,
                         socket_message<sockaddr_in>{
                             .address = {addr_v4}, .buffers = std::span(it, 1)},
                         0);
      ASSERT_EQ(len, 1);
      len = sendmsg(sock_v6,
                    socket_message<sockaddr_in6>{.address = {addr_v6},
                                                 .buffers = std::span(it, 1)},
                    0);
      ASSERT_EQ(len, 1);

      len = recvmsg(sock_v4, msg, 0);
      ASSERT_EQ(len, 1);
      EXPECT_EQ(buf[0], *it);

      len = recvmsg(sock_v6, msg, 0);
      ASSERT_EQ(len, 1);
      EXPECT_EQ(buf[0], *it);
    }
  }
}
// NOLINTEND
