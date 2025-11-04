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
#include "net/timers/interrupt.hpp"
#include "net/timers/timers.hpp"

#include <gtest/gtest.h>

using namespace net::timers;

using interrupt_source = socketpair_interrupt_source_t;
using timers_type = timers<interrupt_source>;

TEST(TimersTests, MoveConstructor)
{
  auto timers0 = timers_type();
  auto timers1 = timers_type(std::move(timers0));
}

TEST(TimersTests, MoveAssignment)
{
  auto timers0 = timers_type();
  auto timers1 = timers_type();
  timers1 = std::move(timers0);
}

TEST(TimersTests, Swap)
{
  auto timers0 = timers_type();
  auto timers1 = timers_type();

  swap(timers0, timers1);
  swap(timers0, timers0);
  swap(timers1, timers1);
}

TEST(TimersTests, EventRefEquality)
{
  using event_ref = detail::event_ref;
  auto now = clock::now();
  auto ref0 = event_ref{.expires_at = now};
  auto ref1 = event_ref{.expires_at = now};
  EXPECT_EQ(ref0, ref1);
}

TEST(TimersTests, TimerAdd)
{
  auto timers = timers_type();
  auto timer = timers.add(100, [](timer_id) {});
  ASSERT_EQ(timer, 0);
}

TEST(TimersTests, ReuseTimerID)
{
  auto timers = timers_type();

  timers.remove(INVALID_TIMER); // Make sure this doesn't break.

  auto timer0 = timers.add(100, [](timer_id) {});
  ASSERT_EQ(timer0, 0);
  timers.remove(timer0);
  timers.resolve();
  auto timer1 = timers.add(100, [](timer_id) {});
  EXPECT_EQ(timer0, timer1);
}

TEST(TimersTests, PeriodicTimer)
{
  using namespace std::chrono;

  auto timers = timers_type();
  auto timer0 = timers.add(100, [](timer_id) {}, 100);
  ASSERT_EQ(timer0, 0);
  std::this_thread::sleep_for(milliseconds(1));
  auto next = timers.resolve();
  EXPECT_NE(next.count(), -1);
}
// NOLINTEND
