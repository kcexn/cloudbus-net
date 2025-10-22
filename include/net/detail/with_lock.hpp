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
 * @file with_lock.hpp
 * @brief This file defines with_lock.
 */
#pragma once
#ifndef CPPNET_WITH_LOCK_HPP
#define CPPNET_WITH_LOCK_HPP
#include <mutex>
#include <type_traits>
/** @brief This namespace provides internal cppnet implementation details. */
namespace net::detail {
/**
 * @brief Runs the supplied functor while holding the acquired lock.
 * @tparam Fn The functor type.
 * @param lock The lock for thread-safety.
 * @param func The functor to invoke.
 * @returns The return value of func.
 */
template <typename Fn>
  requires std::is_invocable_v<Fn>
auto with_lock(std::unique_lock<std::mutex> lock, Fn &&func) -> decltype(auto)
{
  return std::forward<Fn>(func)();
}

} // namespace net::detail
#endif // CPPNET_WITH_LOCK_HPP
