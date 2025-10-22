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

#include <gtest/gtest.h>

#include <condition_variable>
#include <list>
#include <mutex>

using namespace net::service;

class AsyncContextTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(AsyncContextTest, SignalTest)
{
  int handled = 0;
  async_context ctx{};

  ctx.interrupt = [&] { handled++; };
  ctx.signal(ctx.terminate);
  ASSERT_EQ(handled, 1);
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
  auto list = std::list<async_service<test_service>>{};
  auto &service = list.emplace_back();
  ASSERT_FALSE(static_cast<bool>(service.interrupt));

  std::mutex mtx;
  std::condition_variable cvar;

  service.start(mtx, cvar);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return static_cast<bool>(service.interrupt); });
  }
  EXPECT_TRUE(static_cast<bool>(service.interrupt));
  service.signal(service.terminate);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return service.stopped.load(); });
  }
  EXPECT_TRUE(service.stopped);
  list.erase(list.begin());
  EXPECT_TRUE(list.empty());
}

TEST_F(AsyncContextTest, TestUser1Signal)
{
  auto list = std::list<async_service<test_service>>{};
  auto &service = list.emplace_back();

  std::mutex mtx;
  std::condition_variable cvar;

  service.start(mtx, cvar);
  {
    auto lock = std::unique_lock{mtx};
    cvar.wait(lock, [&] { return static_cast<bool>(service.interrupt); });
  }
  ASSERT_TRUE(static_cast<bool>(service.interrupt));
  service.signal(service.user1);
  {
    auto lock = std::unique_lock{test_mtx};
    test_cv.wait(lock, [&] { return test_signal == service.user1; });
  }
  EXPECT_EQ(test_signal, service.user1);
}
// NOLINTEND
