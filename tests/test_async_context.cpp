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
#include "net/service/context_thread.hpp"

#include <gtest/gtest.h>

#include <condition_variable>
#include <list>
#include <mutex>

using namespace net::service;

class AsyncContextTest : public ::testing::Test {};

TEST_F(AsyncContextTest, SignalTest)
{
  auto ctx = async_context{};

  int err = ::socketpair(AF_UNIX, SOCK_STREAM, 0, ctx.interrupt.sockets.data());
  ASSERT_EQ(err, 0);

  ctx.signal(ctx.terminate);

  auto buf = std::array<char, 5>();
  auto msg = io::socket::socket_message<sockaddr_in>{.buffers = buf};
  auto len = io::recvmsg(ctx.interrupt.sockets[0], msg, 0);
  EXPECT_EQ(len, 1);
}

std::mutex test_mtx;
std::condition_variable test_cv;
static int test_signal = 0;
static int test_started = 0;
struct test_service {
  auto signal_handler(int signum) noexcept -> void
  {
    std::lock_guard lock{test_mtx};
    test_signal = signum;
    test_cv.notify_all();
  }
  auto start(async_context &ctx) noexcept -> void
  {
    std::lock_guard lock{test_mtx};
    test_started = 1;
    test_cv.notify_all();
  }
};

TEST_F(AsyncContextTest, AsyncServiceTest)
{
  auto service = context_thread<test_service>();

  std::mutex mtx;
  std::condition_variable cvar;

  service.start(mtx, cvar);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.state != service.PENDING; });
  }
  ASSERT_EQ(service.state, service.STARTED);
  service.signal(service.terminate);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.state != service.STARTED; });
  }
  ASSERT_EQ(service.state, service.STOPPED);
}

TEST_F(AsyncContextTest, StartTwiceTest)
{
  auto service = context_thread<test_service>{};

  std::mutex mtx;
  std::condition_variable cvar;

  service.start(mtx, cvar);
  EXPECT_THROW(service.start(mtx, cvar), std::invalid_argument);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.state != service.PENDING; });
  }
  ASSERT_EQ(service.state, service.STARTED);
  service.signal(service.terminate);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.state != service.STARTED; });
  }
  ASSERT_EQ(service.state, service.STOPPED);
}

TEST_F(AsyncContextTest, TestUser1Signal)
{
  auto list = std::list<context_thread<test_service>>{};
  auto &service = list.emplace_back();

  std::mutex mtx;
  std::condition_variable cvar;

  service.start(mtx, cvar);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.state != service.PENDING; });
  }
  ASSERT_EQ(service.state, service.STARTED);
  service.signal(service.user1);
  {
    auto lock = std::unique_lock{test_mtx};
    test_cv.wait(lock, [&] { return test_signal == service.user1; });
  }
  EXPECT_EQ(test_signal, service.user1);
}
// NOLINTEND
