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
#pragma once
#ifndef CPPNET_TEST_TCP_FIXTURE_HPP
#define CPPNET_TEST_TCP_FIXTURE_HPP
#include "net/service/async_tcp_service.hpp"
#include "net/service/context_thread.hpp"

#include <gtest/gtest.h>
#include <io/io.hpp>

using namespace net::service;

static std::atomic<int> test_counter{};
struct tcp_echo_service : public async_tcp_service<tcp_echo_service> {
  using Base = async_tcp_service<tcp_echo_service>;
  using socket_message = io::socket::socket_message<>;

  template <typename T>
  explicit tcp_echo_service(socket_address<T> address) : Base(address)
  {}

  bool initialized = false;
  auto initialize(const socket_handle &sock) -> std::error_code
  {
    if (initialized)
      return std::make_error_code(std::errc::invalid_argument);

    initialized = true;
    return {};
  }

  auto stop() noexcept -> void { test_counter++; }

  auto echo(async_context &ctx, const socket_dialog &socket,
            const std::shared_ptr<read_context> &rctx,
            socket_message msg) -> void
  {
    using namespace io::socket;
    using namespace stdexec;

    sender auto sendmsg = io::sendmsg(socket, msg, 0) |
                          then([&, socket, rctx](auto &&len) {
                            submit_recv(ctx, socket, rctx);
                          }) |
                          upon_error([](auto &&error) {});

    ctx.scope.spawn(std::move(sendmsg));
  }

  auto service(async_context &ctx, const socket_dialog &socket,
               std::shared_ptr<read_context> rctx,
               std::span<const std::byte> buf) -> void
  {
    echo(ctx, socket, rctx, {.buffers = buf});
  }
};

class AsyncTcpServiceTest : public ::testing::Test {
protected:
  using server_type = context_thread<tcp_echo_service>;
  template <typename T> using socket_address = io::socket::socket_address<T>;
  using socket_dialog =
      io::socket::socket_dialog<io::execution::poll_multiplexer>;

  auto SetUp() -> void override
  {
    using namespace stdexec;

    ctx = std::make_unique<async_context>();

    constexpr auto PORT_MIN = 8000UL;
    unsigned short port = PORT_MIN + std::rand() % (UINT16_MAX - PORT_MIN + 1);

    addr_v4 = socket_address<sockaddr_in>();
    addr_v4->sin_family = AF_INET;
    addr_v4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr_v4->sin_port = htons(port++);
    service_v4 = std::make_unique<tcp_echo_service>(addr_v4);
    server_v4 = std::make_unique<server_type>();

    addr_v6 = socket_address<sockaddr_in6>();
    addr_v6->sin6_family = AF_INET6;
    addr_v6->sin6_addr = in6addr_loopback;
    addr_v6->sin6_port = htons(port++);
    service_v6 = std::make_unique<tcp_echo_service>(addr_v6);
    server_v6 = std::make_unique<server_type>();

    int err = ::socketpair(AF_UNIX, SOCK_STREAM, 0, ctx->timers.sockets.data());
    ASSERT_EQ(err, 0);

    isr(ctx->poller.emplace(ctx->timers.sockets[0]), [&] {
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

    auto mark = std::atomic_signed_lock_free(0);
    wait_empty = std::jthread([&] {
      mark++;
      mark.notify_all();
      sync_wait(ctx->scope.on_empty() | then([&] { is_empty = true; }));
    });
    mark.wait(0);
  }

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

  std::unique_ptr<async_context> ctx;
  std::atomic<bool> is_empty;
  std::jthread wait_empty;
  std::unique_ptr<tcp_echo_service> service_v4;
  std::unique_ptr<tcp_echo_service> service_v6;
  std::unique_ptr<server_type> server_v4;
  std::unique_ptr<server_type> server_v6;

  socket_address<sockaddr_in> addr_v4;
  socket_address<sockaddr_in6> addr_v6;
  std::mutex mtx;
  std::condition_variable cvar;

  auto TearDown() -> void override
  {
    if (ctx->timers.sockets[1] != io::socket::INVALID_SOCKET)
      io::socket::close(ctx->timers.sockets[1]);
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
#endif // CPPNET_TEST_TCP_FIXTURE_HPP
// NOLINTEND
