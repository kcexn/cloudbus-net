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
 * @file concepts.hpp
 * @brief This file defines concepts for cloudbus.
 */
#pragma once
#ifndef CLOUDBUS_CONCEPTS_HPP
#define CLOUDBUS_CONCEPTS_HPP
#include <concepts>
// Forward declarations
namespace cloudbus::service {
struct async_context;
} // namespace cloudbus::service

/**
 * @namespace cloudbus
 * @brief The root namespace for all cloudbus components.
 */
namespace cloudbus {
template <typename S>
concept ServiceLike = requires(S service, service::async_context &ctx) {
  { service.signal_handler(1) } noexcept -> std::same_as<void>;
  { service.start(ctx) } noexcept -> std::same_as<void>;
};
} // namespace cloudbus
#endif // CLOUDBUS_CONCEPTS_HPP
