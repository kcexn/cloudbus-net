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

/**
 * @file async_context_impl.hpp
 * @brief This file defines the asynchronous context.
 */
#pragma once
#ifndef CPPNET_ASYNC_CONTEXT_IMPL_HPP
#define CPPNET_ASYNC_CONTEXT_IMPL_HPP
#include "net/service/async_context.hpp"

#include <cassert>
namespace net::service {
/** @brief Internal net::service implementation details. */
namespace detail {
/**
 * @brief Computes the time in ms poller.wait_for() should block for
 * before waking for the next event.
 * @note This helper method was primarily added for the purposes
 * of test coverage, it isn't really reusable outside of the
 * async_context class.
 * @tparam Rep The duration tick type.
 * @tparam Period the tick period.
 * @param duration The duration to convert to milliseconds.
 * Must be in the range of [-1, duration.max()].
 * @returns -1 If the duration is -1, otherwise it returns the
 * duration count in milliseconds.
 */
template <class Rep, class Period = std::ratio<1>>
auto to_millis(std::chrono::duration<Rep, Period> duration) -> int
{
  using namespace std::chrono;
  assert(duration.count() >= -1 &&
         "duration must be in the interval of -1 to duration.max()");
  return (duration.count() < 0) ? duration.count()
                                : duration_cast<milliseconds>(duration).count();
}
} // namespace detail.

inline auto async_context::signal(int signum) -> void
{
  assert(signum >= 0 && signum < END && "signum must be a valid signal.");
  sigmask.fetch_or(1 << signum);
  interrupt();
}

/** @brief Calls the timers interrupt. */
inline auto async_context::interrupt() const noexcept -> void
{
  static_cast<const timers_type::interrupt_source_t &>(timers).interrupt();
}

template <typename Fn>
  requires std::is_invocable_r_v<bool, Fn>
auto async_context::isr(const socket_dialog &socket, Fn routine) -> void
{
  using namespace io::socket;
  using namespace stdexec;
  using socket_message = socket_message<sockaddr_in>;

  static constexpr auto BUFLEN = 1024UL;
  static auto buffer = std::array<std::byte, BUFLEN>{};
  static auto msg = socket_message{.buffers = buffer};

  if (!routine())
    return;

  sender auto recvmsg = io::recvmsg(socket, msg, 0) |
                        then([this, socket, func = std::move(routine)](auto) {
                          isr(socket, std::move(func));
                        }) |
                        upon_error([](auto) noexcept {});
  scope.spawn(std::move(recvmsg));
}

inline auto async_context::run() -> void
{
  using namespace stdexec;
  using namespace std::chrono;
  using namespace detail;

  auto is_empty = std::atomic_flag();
  scope.spawn(poller.on_empty() |
              then([&]() noexcept { is_empty.test_and_set(); }));

  while (poller.wait_for(to_millis(timers.resolve())) || !is_empty.test());
}

} // namespace net::service
#endif // CPPNET_ASYNC_CONTEXT_IMPL_HPP
