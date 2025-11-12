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
 * @file async_context.hpp
 * @brief This file declares an asynchronous execution context.
 */
#pragma once
#ifndef CPPNET_ASYNC_CONTEXT_HPP
#define CPPNET_ASYNC_CONTEXT_HPP
#include "net/detail/immovable.hpp"
#include "net/timers/timers.hpp"

#include <exec/async_scope.hpp>
#include <io/io.hpp>

#include <atomic>
#include <cstdint>
/** @brief This namespace is for network services. */
namespace net::service {

/** @brief An asynchronous execution context. */
struct async_context : detail::immovable {
  /** @brief Asynchronous scope type. */
  using async_scope = exec::async_scope;
  /** @brief The io multiplexer type. */
  using multiplexer_type = io::execution::poll_multiplexer;
  /** @brief The socket dialog type. */
  using socket_dialog = io::socket::socket_dialog<multiplexer_type>;
  /** @brief The io triggers type. */
  using triggers = io::execution::basic_triggers<multiplexer_type>;
  /** @brief The signal mask type. */
  using signal_mask = std::uint64_t;
  /** @brief Interrupt source type. */
  using interrupt_source = timers::socketpair_interrupt_source_t;
  /** @brief The timers type. */
  using timers_type = timers::timers<interrupt_source>;

  /** @brief An enum of all valid async context signals. */
  enum signals : std::uint8_t { terminate = 0, user1, END };
  /** @brief An enum of valid context states. */
  enum context_states : std::uint8_t { PENDING = 0, STARTED, STOPPED };

  /** @brief The asynchronous scope. */
  async_scope scope;
  /** @brief The poll triggers. */
  triggers poller;
  /** @brief A counter that tracks the context state. */
  std::atomic<context_states> state{PENDING};
  /** @brief The active signal mask. */
  std::atomic<signal_mask> sigmask;
  /** @brief The event loop timers. */
  timers_type timers;

  /**
   * @brief Sets the signal mask, then interrupts the service.
   * @param signum The signal to send. Must be in range of
   *               enum signals.
   */
  inline auto signal(int signum) -> void;

  /** @brief Calls the timers interrupt. */
  inline auto interrupt() const noexcept -> void;

  /**
   * @brief An interrupt service routine for the poller.
   * @details When invoked, `isr()` installs an event
   * handler on socket events received on `socket`. The routine will be
   * continuously re-installed in a loop until it returns false.
   * @tparam Fn A callable to run upon receiving an interrupt.
   * @param socket The listening socket for interrupts. Its lifetime is
   * tied to the lifetime of `routine`.
   * @param routine The routine to run upon receiving a poll interrupt on
   * `socket`.
   * @code
   * isr(poller.emplace(sockets[0]), [&]() noexcept {
   *   auto sigmask_ = sigmask.exchange(0);
   *   for (int signum = 0; auto mask = (sigmask_ >> signum); ++signum)
   *   {
   *     if (mask & (1 << 0))
   *       service.signal_handler(signum);
   *   }
   *   return !(sigmask_ & (1 << terminate));
   * });
   * @endcode
   */
  template <typename Fn>
    requires std::is_invocable_r_v<bool, Fn>
  auto isr(const socket_dialog &socket, Fn routine) -> void;

  /** @brief Runs the event loop. */
  inline auto run() -> void;
};

} // namespace net::service

#include "impl/async_context_impl.hpp" // IWYU pragma: export

#endif // CPPNET_ASYNC_CONTEXT_HPP
