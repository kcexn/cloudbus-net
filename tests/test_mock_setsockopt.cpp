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
#include "net/service/async_tcp_service.hpp"
#include "net/service/async_udp_service.hpp"
#include "net/service/context_thread.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
using namespace net::service;

static int error = 0;
int setsockopt(int __fd, int level, int optname, const void *optval,
               socklen_t optlen)
{
  errno = static_cast<int>(std::errc::interrupted);
  error = errno;
  return -1;
}

class AsyncTcpServiceTest : public ::testing::Test {};

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
            const std::shared_ptr<read_context> &rmsg,
            socket_message msg) -> void
  {
    using namespace io::socket;
    using namespace stdexec;

    sender auto sendmsg =
        io::sendmsg(socket, msg, 0) |
        then([&, socket, msg, rmsg](auto &&len) mutable {
          if (auto buffers = std::move(msg.buffers); buffers += len)
            return echo(ctx, socket, rmsg, {.buffers = buffers});

          reader(ctx, socket, std::move(rmsg));
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

TEST_F(AsyncTcpServiceTest, SetSockOptError)
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
    if (sigmask & (1 << ctx.terminate))
      ctx.scope.request_stop();
  };

  service.start(ctx);
  EXPECT_TRUE(ctx.scope.get_stop_token().stop_requested());
  EXPECT_EQ(error, static_cast<int>(std::errc::interrupted));

  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

TEST_F(AsyncTcpServiceTest, ServiceNoHang)
{
  using namespace io::socket;
  auto service = context_thread<echo_block_service>();

  std::mutex mtx;
  std::condition_variable cvar;

  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);

  ASSERT_FALSE(service.scope.get_stop_token().stop_requested());
  service.start(mtx, cvar, addr);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.state != service.PENDING; });
  }
  ASSERT_EQ(service.state, service.STARTED);
}

class AsyncUdpServiceTest : public ::testing::Test {};

struct echo_udp_service : public async_udp_service<echo_udp_service> {
  using Base = async_udp_service<echo_udp_service>;
  using socket_message = io::socket::socket_message<>;

  template <typename T>
  explicit echo_udp_service(socket_address<T> address) : Base(address)
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

TEST_F(AsyncUdpServiceTest, SetSockOptError)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);
  auto service = echo_udp_service(addr);

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
  EXPECT_EQ(error, static_cast<int>(std::errc::interrupted));

  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}
// NOLINTEND
