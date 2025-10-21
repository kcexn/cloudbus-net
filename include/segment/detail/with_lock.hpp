/* Copyright (C) 2025 Kevin Exton (kevin.exton@pm.me)
 *
 * Cloudbus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cloudbus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Cloudbus.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file with_lock.hpp
 * @brief This file defines with_lock.
 */
#pragma once
#ifndef CLOUDBUS_WITH_LOCK_HPP
#define CLOUDBUS_WITH_LOCK_HPP
#include <mutex>
#include <type_traits>
/** @brief This namespace provides internal cloudbus implementation details. */
namespace cloudbus::detail {
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

} // namespace cloudbus::detail
#endif // CLOUDBUS_WITH_LOCK_HPP
