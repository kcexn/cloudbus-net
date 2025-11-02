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
 * @brief This file defines the asynchronous service.
 */
#pragma once
#ifndef CPPNET_ASYNC_CONTEXT_IMPL_HPP
#define CPPNET_ASYNC_CONTEXT_IMPL_HPP
#include "net/detail/with_lock.hpp"
#include "net/service/context_thread.hpp"

#include <stdexec/execution.hpp>
namespace net::service {

inline auto async_context::signal(int signum) -> void
{
  assert(signum >= 0 && signum < END && "signum must be a valid signal.");
  sigmask.fetch_or(1 << signum);
  interrupt();
}

inline auto async_context::interrupt_type::operator()() const -> void
{
  using namespace detail;
  auto func = with_lock(std::unique_lock{mtx_}, [&] { return fn_; });
  if (func)
    func();
}

inline auto async_context::interrupt_type::operator=(
    std::function<void()> func) noexcept -> interrupt_type &
{
  std::lock_guard lock{mtx_};
  fn_ = std::move(func);
  return *this;
}
} // namespace net::service
#endif // CPPNET_ASYNC_CONTEXT_IMPL_HPP
