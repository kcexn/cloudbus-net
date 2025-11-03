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
 * @file interrupt.hpp
 * @brief This file declares interrupt related types..
 */
#pragma once
#ifndef CPPNET_INTERRUPT_HPP
#define CPPNET_INTERRUPT_HPP
#include <io/io.hpp>
/** @brief This namespace is for timers and interrupts. */
namespace net::timers {
/** @brief A concept for constraining interrupt sources. */
template <typename Tag>
concept InterruptSource = requires(const Tag tag) {
  { tag.interrupt() } noexcept -> std::same_as<void>;
};

/** @brief A socketpair interrupt source. */
struct socketpair_interrupt_source_t {
  /** @brief The native socket type. */
  using socket_type = io::socket::native_socket_type;
  /** @brief The invalid socket constant. */
  static constexpr auto INVALID_SOCKET = io::socket::INVALID_SOCKET;
  /** @brief The socket pair. */
  std::array<socket_type, 2> sockets{INVALID_SOCKET, INVALID_SOCKET};
  /**
   * @brief The interrupt method.
   * @details This method is needed to comply with the InterruptSource
   * concept.
   */
  inline auto interrupt() const noexcept -> void;
};

/**
 * @brief An interrupt is an immediately run timer event.
 * @tparam Interrupt An interrupt source tag compliant with the InterruptSoruce
 * concept.
 * @details Interrupts are used to awaken sleeping event-loops.
 */
template <InterruptSource Interrupt> struct interrupt : public Interrupt {
  /** @brief Calls the underlying interrupt. */
  inline auto operator()() const noexcept -> void;
};

} // namespace net::timers

#include "impl/interrupt_impl.hpp" // IWYU pragma: export

#endif // CPPNET_INTERRUPT_HPP
