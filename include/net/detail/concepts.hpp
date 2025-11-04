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
 * @file concepts.hpp
 * @brief This file defines concepts for cppnet.
 */
#pragma once
#ifndef CPPNET_CONCEPT_HPP
#define CPPNET_CONCEPT_HPP
#include <concepts>
// Forward declarations
namespace net::service {
struct async_context;
} // namespace net::service

/**
 * @namespace net
 * @brief The root namespace for all cppnet components.
 */
namespace net {
/** @brief ServiceLike describes types that behave like an application or
 * service. */
template <typename S>
concept ServiceLike = requires(S service, service::async_context &ctx) {
  { service.signal_handler(1) } noexcept -> std::same_as<void>;
  { service.start(ctx) } noexcept -> std::same_as<void>;
};

/** @brief This namespace is for timers and interrupts. */
namespace timers {
/** @brief A concept for constraining interrupt sources. */
template <typename Tag>
concept InterruptSource = requires(const Tag tag) {
  { tag.interrupt() } noexcept -> std::same_as<void>;
};
} // namespace timers.

} // namespace net
#endif // CPPNET_CONCEPT_HPP
