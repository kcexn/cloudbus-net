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
#ifndef CPPNET_TIMERS_HPP
#define CPPNET_TIMERS_HPP
#include "interrupt.hpp"
#include "net/detail/concepts.hpp"

#include <chrono>
#include <functional>
#include <queue>
#include <stack>
namespace net::timers {

/** @brief timer_id type. */
using timer_id = std::size_t;
/** @brief Invalid timer_id. */
static constexpr timer_id INVALID_TIMER = -1;
/** @brief handler type. */
using handler_t = std::function<void(timer_id)>;
/** @brief clock type. */
using clock = std::chrono::steady_clock;
/** @brief time type. */
using timestamp = std::chrono::time_point<clock>;
/** @brief duration type. */
using duration = std::chrono::microseconds;

/** @brief Internal timer implementation details. */
namespace detail {
/** @brief The event structure. */
struct event {
  /** @brief An event handler. */
  handler_t handler;
  /** @brief The timer id. */
  timer_id id = INVALID_TIMER;
  /** @brief The first time the event fired. */
  timestamp start;
  /** @brief The timer period. */
  duration period{};
  /** @brief A flag to determine if the timer is armed. */
  std::atomic_flag armed;
};

/** @brief event_ref to be inserted into the priority queue. */
struct event_ref {
  /** @brief The timer expiry time. */
  timestamp expires_at;
  /** @brief The associated timer id. */
  timer_id id = INVALID_TIMER;
};

/**
 * @brief The spaceship operator to determine event_ref ordering.
 * @param lhs The left side of the comparison.
 * @param rhs The right side of the comparison.
 * @returns A strong_ordering of the `event_ref`'s based on the `expires_at`
 * property.
 */
inline auto operator<=>(const event_ref &lhs,
                        const event_ref &rhs) -> std::strong_ordering;
/**
 * @brief An equality operator to determine event_ref ordering
 * @param lhs The left event_ref.
 * @param rhs The right event_ref.
 * @param returns true if lhs expires at the same time as the rhs, false
 * otherwise.
 */
inline auto operator==(const event_ref &lhs, const event_ref &rhs) -> bool;
} // namespace detail.

/**
 * @brief Provides event-loop timers.
 * @tparam Interrupt An interrupt source that satisfies the InterruptSource
 * concept.
 * @details `timers` is the spritual successor to
 * [cpptime](https://github.com/eglimi/cpptime) and has been modified for
 * integration with the cppnet `context_thread` event-loop. As such, it exposes
 * the same API as a CppTime::Timer for adding and removing timers. Unlike a
 * CppTime::Timer, `timers` does not execute the timer callbacks in a separate
 * thread. Instead, to resolve the timer event callbacks an event loop must
 * explicitly call the public `resolve` method. The `resolve` method return a
 * `std::chrono` duration until the next event timeout. If there are no more
 * events in the internal event queue, then the resolve method returns
 * `duration(-1)`, otherwise the returned duration contains a strictly
 * non-negative count.
 */
template <InterruptSource Interrupt>
class timers : public interrupt<Interrupt> {
public:
  /** @brief The base interrupt type. */
  using interrupt_type = interrupt<Interrupt>;

  /** @brief Default constructor. */
  timers() = default;
  /** @brief Deleted copy constructor. */
  timers(const timers &other) = delete;
  /** @brief Move constructor. */
  timers(timers &&other) noexcept;

  /** @brief Deleted copy assignment. */
  auto operator=(const timers &other) = delete;
  /** @brief Move assignment. */
  auto operator=(timers &&other) noexcept -> timers &;

  /** @brief Swap function. */
  template <InterruptSource I>
  friend auto swap(timers<I> &lhs, timers<I> &rhs) noexcept -> void;

  /**
   * @brief Add a new timer.
   * @param when The time at which the handler is invoked.
   * @param handler The callable that is invoked when the timer fires.
   * @param period The periodicity at which the timer fires. Only used for
   * periodic timers.
   * @returns The id associated with this timer.
   */
  auto add(timestamp when, handler_t handler,
           duration period = duration::zero()) -> timer_id;

  /**
   * @brief Overloaded `add` function that uses a `std::chrono::duration`
   * instead of a `time_point` for the first timeout.
   * @tparam Rep The arithmetic tick type for a `std::chrono::duration`.
   * @tparam Period The `std::ratio` of a `std::chrono::duration`.
   * @param when The time until the timer times out.
   * @param handler The timer event handler.
   * @param period The time between events for a periodic timer.
   * @returns The id associated with this timer.
   */
  template <class Rep, class Period>
  auto add(std::chrono::duration<Rep, Period> when, handler_t handler,
           duration period = duration::zero()) -> timer_id;

  /**
   * @brief Overloaded `add` function that uses a uint64_t instead of a
   * `time_point` for the first timeout and the period.
   * @param when The number of microseconds until the event times out.
   * @param handler The event handler.
   * @param period The number of microseconds between events for a periodic
   * timer.
   * @returns The id associated with this timer.
   */
  auto add(std::uint64_t when, handler_t handler,
           std::uint64_t period = 0) -> timer_id;

  /**
   * @brief Removes the timer with the given id.
   * @param tid The timer_id to remove.
   */
  auto remove(timer_id tid) noexcept -> void;

  /**
   * @brief Resolves all expired event handles.
   * @returns The duration until the next event times out. Returns
   * `duration(-1)` if the internal eventq is empty.
   */
  auto resolve() -> duration;

  /** @brief Default destructor. */
  ~timers() = default;

private:
  /** @brief The min-heap type for storing the event_references. */
  template <typename T>
  using minheap = std::priority_queue<T, std::vector<T>, std::greater<>>;

  /** @brief Internal state. */
  struct {
    /** @brief The vector that holds all active events. */
    std::deque<detail::event> events;
    /** @brief The minheap that stores timeouts. */
    minheap<detail::event_ref> eventq;
    /** @brief A pool of recyclable timer_ids */
    std::stack<timer_id> free_ids;
  } state_;

  /** @brief mutex for thread-safety. */
  mutable std::mutex mtx_;
};

} // namespace net::timers

#include "impl/timers_impl.hpp" // IWYU pragma: export

#endif // CPPNET_TIMERS_HPP
