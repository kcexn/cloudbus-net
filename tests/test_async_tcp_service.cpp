/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * Cloudbus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cloudbus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Cloudbus.  If not, see <https://www.gnu.org/licenses/>.
 */

// NOLINTBEGIN
#include "segment/service/async_service.hpp"
#include "segment/service/async_tcp_service.hpp"

#include <gtest/gtest.h>

#include <cassert>
#include <list>

#include <arpa/inet.h>
using namespace cloudbus::service;

struct async_noop_service : public async_tcp_service<async_noop_service> {
  using Base = async_tcp_service<async_noop_service>;

  template <typename T>
  explicit async_noop_service(socket_address<T> address) : Base(address)
  {}

  auto operator()(async_context &ctx, const socket_dialog &socket,
                  std::shared_ptr<read_context> rmsg,
                  std::span<const std::byte> buf) -> void
  {
    msg.insert(msg.end(), buf.begin(), buf.end());
    reader(ctx, socket, std::move(rmsg));
  }

  std::vector<std::byte> msg;
};

class AsyncTcpServiceTest : public ::testing::Test {};

TEST_F(AsyncTcpServiceTest, StartTest)
{
  using namespace io::socket;

  auto ctx = async_context{};
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  auto service = async_noop_service{addr};

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

TEST_F(AsyncTcpServiceTest, ReadTest)
{
  using namespace io::socket;

  auto ctx = async_context();
  auto addr = socket_address<sockaddr_in>();
  addr->sin_family = AF_INET;
  addr->sin_port = htons(8080);
  auto service = async_noop_service(addr);

  ctx.interrupt = [&] {
    auto sigmask = ctx.sigmask.exchange(0);
    for (int signum = 0; auto mask = (sigmask >> signum); ++signum)
    {
      if (mask & (1 << 0))
        service.signal_handler(signum);
    }
  };

  service.start(ctx);
  {
    using namespace io;
    auto sock = socket_handle(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");

    ASSERT_EQ(connect(sock, addr), 0);
    ctx.poller.wait();

    auto buf = std::array<char, 1>{'x'};
    ASSERT_EQ(sendmsg(sock, socket_message{.buffers = buf}, 0), 1);
    ctx.poller.wait();
  }

  EXPECT_EQ(static_cast<char>(service.msg.at(0)), 'x');
  ctx.signal(ctx.terminate);
  while (ctx.poller.wait());
}

class write_queue {
public:
  static constexpr auto BUFSIZE = 4096UL;
  using buffer_type = std::array<std::byte, BUFSIZE>;
  using queue_type = std::list<buffer_type>;
  using iterator = queue_type::iterator;
  using size_type = std::size_t;

  auto append(std::span<const std::byte> buf) -> void
  {
    for (auto remaining = buf.size(), len = 0UL; remaining > 0;
         remaining -= len)
    {
      len = std::min<size_type>(remaining, epptr_ - pptr_);
      auto *begin = buf.data() + (buf.size() - remaining);

      pptr_ = static_cast<std::byte *>(std::memcpy(pptr_, begin, len)) + len;
      if (epptr_ == pptr_)
      {
        auto &back = queue_.emplace_back();
        pbase_ = pptr_ = back.data();
        epptr_ = back.data() + back.size();
      }
    }
  }

  auto get() -> std::span<const std::byte>
  {
    size_type len = 0;
    while (!(len = egptr_ - gptr_) && egptr_ != pptr_)
    {
      if (egptr_ == eback_ + BUFSIZE)
      {
        auto &front = *queue_.erase(queue_.begin());
        eback_ = gptr_ = front.data();
      }

      egptr_ = (eback_ == pbase_) ? pptr_ : eback_ + BUFSIZE;
    }

    return {gptr_, len};
  }

  auto size() -> size_type
  {
    return (queue_.size() - 1) * BUFSIZE + (pptr_ - pbase_) - (gptr_ - eback_);
  }

  auto pop(size_type n) -> void
  {
    for (size_type len = 0; n; n -= len)
    {
      len = std::min<size_type>(egptr_ - gptr_, n);
      if ((gptr_ += len) == egptr_)
      {
        if (egptr_ == eback_ + BUFSIZE)
        {
          auto &front = *queue_.erase(queue_.begin());
          eback_ = gptr_ = front.data();
        }
        egptr_ = (eback_ == pbase_) ? pptr_ : eback_ + BUFSIZE;
      }
    }
  }

private:
  queue_type queue_{1};

  std::byte *pbase_{queue_.back().data()}, *pptr_{queue_.back().data()},
      *epptr_{queue_.back().data() + BUFSIZE};
  std::byte *eback_{pptr_}, *gptr_{pptr_}, *egptr_{pptr_};
};

struct echo_service : public async_tcp_service<echo_service> {
  using Base = async_tcp_service<echo_service>;

  template <typename T>
  explicit echo_service(socket_address<T> address) : Base(address)
  {}

  auto operator()(async_context &ctx, const socket_dialog &socket,
                  std::shared_ptr<read_context> rmsg,
                  std::span<const std::byte> buf) -> void
  {
    queue.append(buf);
    if (queue.size() == buf.size())
      echo(ctx, socket);
    reader(ctx, socket, std::move(rmsg));
  }

  auto echo(async_context &ctx, const socket_dialog &socket) -> void
  {
    using namespace io::socket;
    using namespace stdexec;
    if (queue.size() == 0)
      return;

    sender auto sendmsg =
        io::sendmsg(socket, socket_message{.buffers = {queue.get()}}, 0) |
        then([&, socket](auto &&len) {
          queue.pop(len);
          echo(ctx, socket);
        }) |
        upon_error([](auto &&error) {});

    ctx.scope.spawn(std::move(sendmsg));
  }

  write_queue queue;
};

TEST_F(AsyncTcpServiceTest, EchoTest)
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
  };

  service.start(ctx);
  {
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

TEST_F(AsyncTcpServiceTest, EchoBlockTest)
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

TEST_F(AsyncTcpServiceTest, TestInitializeError)
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
