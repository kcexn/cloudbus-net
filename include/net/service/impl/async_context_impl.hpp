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
#include "net/service/context_thread.hpp"
namespace net::service {

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

inline auto async_context::run() -> void
{
  using namespace stdexec;
  using namespace std::chrono;

  auto next = timers.resolve();
  auto wait_ms = (next.count() < 0) ? next.count()
                                    : duration_cast<milliseconds>(next).count();

  auto is_empty = std::atomic_flag();
  scope.spawn(poller.on_empty() |
              then([&]() noexcept { is_empty.test_and_set(); }));

  while (poller.wait_for(static_cast<int>(wait_ms)) || !is_empty.test())
  {
    next = timers.resolve();
    wait_ms = (next.count() < 0) ? next.count()
                                 : duration_cast<milliseconds>(next).count();
  }
}

} // namespace net::service
#endif // CPPNET_ASYNC_CONTEXT_IMPL_HPP
