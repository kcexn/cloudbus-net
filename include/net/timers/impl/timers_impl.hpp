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
#pragma once
#ifndef CPPNET_TIMERS_IMPL_HPP
#define CPPNET_TIMERS_IMPL_HPP
#include "net/detail/with_lock.hpp"
#include "net/timers/timers.hpp"
namespace net::timers {

namespace detail {
/**
 * @brief The spaceship operator to determine event_ref ordering.
 * @param lhs The left side of the comparison.
 * @param rhs The right side of the comparison.
 * @returns A strong_ordering of the `event_ref`'s based on the `expires_at`
 * property.
 */
inline auto operator<=>(const event_ref &lhs,
                        const event_ref &rhs) -> std::strong_ordering
{
  return lhs.expires_at <=> rhs.expires_at;
}
/**
 * @brief An equality operator to determine event_ref ordering
 * @param lhs The left event_ref.
 * @param rhs The right event_ref.
 * @param returns true if lhs expires at the same time as the rhs, false
 * otherwise.
 */
inline auto operator==(const event_ref &lhs, const event_ref &rhs) -> bool
{
  return (lhs <=> rhs) == 0;
}
} // namespace detail.

/** @brief Move constructor. */
template <InterruptSource Interrupt>
timers<Interrupt>::timers(timers &&other) noexcept : timers()
{
  swap(*this, other);
}

/** @brief Move assignment. */
template <InterruptSource Interrupt>
auto timers<Interrupt>::operator=(timers &&other) noexcept -> timers &
{
  swap(*this, other);
  return *this;
}

/** @brief Swap function. */
template <InterruptSource I>
auto swap(timers<I> &lhs, timers<I> &rhs) noexcept -> void
{
  using std::swap;
  if (&lhs == &rhs)
    return;

  auto lock = std::scoped_lock(lhs.mtx_, rhs.mtx_);
  swap(lhs.state_, rhs.state_);
  swap(static_cast<timers<I>::interrupt_type &>(lhs),
       static_cast<timers<I>::interrupt_type &>(rhs));
}

/**
 * @brief Add a new timer.
 * @param when The time at which the handler is invoked.
 * @param handler The callable that is invoked when the timer fires.
 * @param period The periodicity at which the timer fires. Only used for
 * periodic timers.
 */
template <InterruptSource Interrupt>
auto timers<Interrupt>::add(timestamp when, handler_t handler,
                            duration period) -> timer_id
{
  auto lock = std::lock_guard(mtx_);
  auto &[events, eventq, free_ids] = state_;

  // Add a new event. If an ID is free prefer that one.
  timer_id tid = events.size();
  if (!free_ids.empty())
  {
    tid = free_ids.top();
    free_ids.pop();
  }

  auto &event = (tid == events.size()) ? events.emplace_back() : events[tid];

  event.handler = std::move(handler);
  event.id = tid;
  event.start = when;
  event.period = period;
  event.armed.test_and_set();

  eventq.push({.expires_at = when, .id = tid});

  // Notify the interrupt sink of a new event.
  Interrupt::interrupt();

  return tid;
}

/**
 * @brief Overloaded `add` function that uses a `std::chrono::duration`
 * instead of a `time_point` for the first timeout.
 */
template <InterruptSource Interrupt>
template <class Rep, class Period>
auto timers<Interrupt>::add(std::chrono::duration<Rep, Period> when,
                            handler_t handler, duration period) -> timer_id
{
  using namespace std::chrono;
  auto timeout = clock::now() + duration_cast<duration>(when);
  return add(timeout, std::move(handler), period);
}

/**
 * @brief Overloaded `add` function that uses a uint64_t instead of a
 * `time_point` for the first timeout and the period.
 */
template <InterruptSource Interrupt>
auto timers<Interrupt>::add(std::uint64_t when, handler_t handler,
                            std::uint64_t period) -> timer_id
{
  return add(duration(when), std::move(handler), duration(period));
}

/** @brief Removes the timer with the given id. */
template <InterruptSource Interrupt>
auto timers<Interrupt>::remove(timer_id tid) noexcept -> timer_id
{
  auto lock = std::lock_guard(mtx_);
  if (tid >= state_.events.size())
    return tid;

  // The timer id will be added to the free_ids list once
  // the event propagates out of the eventq.
  state_.events[tid].armed.clear();
  return INVALID_TIMER;
}

/**
 * @brief Dequeues timers from an eventq.
 * @details Dequeues timers from the internal eventq of a
 * timers class state object. Armed timers that are dequeued
 * will be returned in a new vector. Unarmed timers that are
 * dequeued are pushed onto the internal free timer_id stack.
 * @tparam The timers class state.
 * @param state The internal state of a timers class.
 * @returns A vector of armed timer events.
 */
template <typename TimersState>
auto dequeue_timers(TimersState &state) -> std::vector<detail::event_ref>
{
  using namespace detail;
  auto &[events, eventq, free_ids] = state;

  const auto now = clock::now();
  auto tmp = std::vector<event_ref>(events.get_allocator());
  tmp.reserve(events.size());

  while (!eventq.empty())
  {
    auto &next = eventq.top();

    // If the event is not armed then put the timer on the free_ids list.
    if (!events[next.id].armed.test())
    {
      events[next.id].handler = nullptr;
      free_ids.push(next.id);
      eventq.pop();
      continue;
    }

    if (now < next.expires_at)
      break;

    tmp.push_back(next);
    eventq.pop();
  }

  return tmp;
} // GCOVR_EXCL_LINE

/**
 * @brief Updates the timer state.
 * @details This method updates the internal state of a timers object.
 * It takes a reference to a state object, and three iterators into a
 * std::vector<detail::event_ref> divided into a [begin, unarmed) armed
 * timers section and an [unarmed, end) unarmed timers section.
 * @tparam TimersState The timers class state.
 * @tparam Iterator A random-access iterator.
 * @param state The current timer state.
 * @param begin The beginning of the armed section.
 * @param unarmed The armed section end and the unarmed section beginning.
 * @param end The end of the unarmed section.
 * @returns duration(-1) if the updated eventq is empty, otherwise returns a
 * non-negative duration until the next timer event.
 */
template <typename TimersState, typename Iterator>
auto update_timers(TimersState &state, Iterator begin, Iterator unarmed,
                   Iterator end) -> duration
{
  auto &[events, eventq, free_ids] = state;

  for (auto it = begin; it != unarmed; ++it)
  {
    it->expires_at += events[it->id].period;
    eventq.push(*it);
  }

  for (auto it = unarmed; it != end; ++it)
  {
    events[it->id].handler = nullptr;
    free_ids.push(it->id);
  }

  if (eventq.empty())
    return duration(-1);

  auto next = duration_cast<duration>(eventq.top().expires_at - clock::now());
  return std::max(duration(0), next);
}

/** @brief Resolves all armed and expired event handles. */
template <InterruptSource Interrupt>
auto timers<Interrupt>::resolve() -> duration
{
  using net::detail::with_lock;
  using namespace detail;
  using namespace std::chrono;
  auto &[events, eventq, free_ids] = state_;

  auto timers = with_lock(mtx_, [&] { return dequeue_timers(state_); });

  // Run handlers and remove unarmed timers.
  auto [unarmed, end] = std::ranges::remove_if(timers, [&](event_ref ref) {
    auto &event = events[ref.id];

    if (event.armed.test())
      event.handler(ref.id);

    if (event.period.count() == 0)
      event.armed.clear();

    return !event.armed.test();
  });

  return with_lock(mtx_, [&] {
    return update_timers(state_, timers.begin(), unarmed, end);
  });
}
} // namespace net::timers
#endif // CPPNET_TIMERS_IMPL_HPP
