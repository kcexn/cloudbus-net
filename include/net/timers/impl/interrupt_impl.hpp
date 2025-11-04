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
 * @file interrupt_impl.hpp
 * @brief This file defines the interrupt sources.
 */
#pragma once
#ifndef CPPNET_INTERRUPT_IMPL_HPP
#define CPPNET_INTERRUPT_IMPL_HPP
#include "net/timers/interrupt.hpp"
/** @brief This namespace is for timers and interrupts. */
namespace net::timers {

inline auto socketpair_interrupt_source_t::interrupt() const noexcept -> void
{
  using namespace io::socket;
  static constexpr auto buf = std::array<char, 1>{'x'};
  static const auto msg = socket_message<sockaddr_in>{.buffers = buf};

  ::io::sendmsg(sockets[1], msg, MSG_NOSIGNAL);
}

template <InterruptSource Interrupt>
inline auto interrupt<Interrupt>::operator()() const noexcept -> void
{
  Interrupt::interrupt();
}

} // namespace net::timers

#endif // CPPNET_INTERRUPT_IMPL_HPP
