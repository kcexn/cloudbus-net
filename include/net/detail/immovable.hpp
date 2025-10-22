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
 * @file immovable.hpp
 * @brief This file defines immovable.
 */
#pragma once
#ifndef CPPNET_IMMOVABLE_HPP
#define CPPNET_IMMOVABLE_HPP
/** @brief This namespace provides internal cppnet implementation details. */
namespace net::detail {
/**
 * @brief This struct can be used as a base class to make derived
 *        classes immovable.
 */
struct immovable {
  /** @brief Default constructor. */
  immovable() = default;
  /** @brief Deleted copy constructor. */
  immovable(const immovable &) = delete;
  /** @brief Deleted move constructor. */
  immovable(immovable &&) = delete;
  /** @brief Deleted copy assignment. */
  auto operator=(const immovable &) -> immovable & = delete;
  /** @brief Deleted move assignment. */
  auto operator=(immovable &&) -> immovable & = delete;
  /** @brief Default destructor. */
  ~immovable() = default;
};
} // namespace net::detail
#endif // CPPNET_IMMOVABLE_HPP
