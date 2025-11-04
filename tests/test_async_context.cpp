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
#include <mutex>

using namespace net::service;

class AsyncContextTest : public ::testing::Test {};

TEST_F(AsyncContextTest, SignalTest)
{
  auto ctx = async_context{};

  int err = ::socketpair(AF_UNIX, SOCK_STREAM, 0, ctx.timers.sockets.data());
  ASSERT_EQ(err, 0);

  ctx.signal(ctx.terminate);

  auto buf = std::array<char, 5>();
  auto msg = io::socket::socket_message<sockaddr_in>{.buffers = buf};
  auto len = io::recvmsg(ctx.timers.sockets[0], msg, 0);
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
  using enum async_context::context_states;

  auto service = context_thread<test_service>();
  service.start();
  service.state.wait(PENDING);
  ASSERT_EQ(service.state, STARTED);

  service.signal(service.terminate);
  service.state.wait(STARTED);
  ASSERT_EQ(service.state, STOPPED);
}

TEST_F(AsyncContextTest, StartTwiceTest)
{
  using enum async_context::context_states;

  auto service = context_thread<test_service>{};

  service.start();
  EXPECT_THROW(service.start(), std::invalid_argument);
  service.state.wait(PENDING);
  ASSERT_EQ(service.state, STARTED);

  service.signal(service.terminate);
  service.state.wait(STARTED);
  ASSERT_EQ(service.state, service.STOPPED);
}

TEST_F(AsyncContextTest, TestUser1Signal)
{
  using enum async_context::context_states;

  auto service = context_thread<test_service>();

  service.start();
  service.state.wait(PENDING);
  ASSERT_EQ(service.state, STARTED);

  service.signal(service.user1);
  {
    auto lock = std::unique_lock{test_mtx};
    test_cv.wait(lock, [&] { return test_signal == service.user1; });
  }
  EXPECT_EQ(test_signal, service.user1);
}
// NOLINTEND
