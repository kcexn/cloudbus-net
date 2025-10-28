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
 * @file async_tcp_service.hpp
 * @brief This file declares an asynchronous tcp service.
 */
#pragma once
#ifndef CPPNET_ASYNC_TCP_SERVICE_HPP
#define CPPNET_ASYNC_TCP_SERVICE_HPP
#include "context_thread.hpp"
namespace net::service {
/**
 * @brief A ServiceLike Async TCP Service.
 * @tparam StreamHandler The StreamHandler type that derives from
 * async_tcp_service.
 * @note The default constructor of async_tcp_service is protected
 * so async_tcp_service can't be constructed without a stream handler
 * (which would be UB).
 * @details async_tcp_service is a CRTP base class compliant with the
 * ServiceLike concept. It must be used with an inheriting CRTP specialization
 * that defines what the service should do with bytes it reads off the wire.
 * StreamHandler must define an operator() overload that eventually calls
 * reader to restart the read loop. It also optionally specifies an initialize
 * member that can be used to configure the service socket. See `noop_service`
 * below for an example of how to specialize async_tcp_service.
 * @code
 * struct noop_service : public async_tcp_service<noop_service>
 * {
 *   using Base = async_tcp_service<noop_service>;
 *
 *   template <typename T>
 *   explicit noop_service(socket_address<T> address): Base(address)
 *   {}
 *
 *   // Optional.
 *   auto initialize(const socket_handle &socket) -> std::error_code
 *   {
 *     return {};
 *   }
 *
 *   // Optional.
 *   auto stop() -> void
 *   {
 *     return {};
 *   }
 *
 *   auto operator()(async_context &ctx, const socket_dialog &socket,
 *                   std::shared_ptr<read_context> rctx,
 *                   std::span<const std::byte> buf) -> void
 *   {
 *     reader(ctx, socket, std::move(rctx));
 *   }
 * };
 * @endcode
 */
template <typename TCPStreamHandler> class async_tcp_service {
public:
  /** @brief Templated socket address type. */
  template <typename T> using socket_address = io::socket::socket_address<T>;
  /** @brief The async context type. */
  using async_context = service::async_context;
  /** @brief The async scope type. */
  using async_scope = async_context::async_scope;
  /** @brief The io multiplexer type. */
  using multiplexer_type = async_context::multiplexer_type;
  /** @brief The socket handle type. */
  using socket_handle = io::socket::socket_handle;
  /** @brief The socket dialog type. */
  using socket_dialog = io::socket::socket_dialog<multiplexer_type>;
  /** @brief Re-export the async_context signals. */
  using enum async_context::signals;

  /** @brief A read context. */
  struct read_context {
    /** @brief The read buffer size. */
    static constexpr std::size_t BUFSIZE = 1024UL;
    /** @brief The read buffer type. */
    using buffer_type = std::array<std::byte, BUFSIZE>;
    /** @brief The socket message type. */
    using socket_message = io::socket::socket_message<>;

    /** @brief The read buffer. */
    buffer_type buffer{};
    /** @brief The read socket message. */
    socket_message msg{.buffers = buffer};
  };

  /**
   * @brief handle signals.
   * @param signum The signal number to handle.
   */
  auto signal_handler(int signum) noexcept -> void;
  /**
   * @brief Start the service on the context.
   * @param ctx The async context to start the service on.
   */
  auto start(async_context &ctx) noexcept -> void;
  /**
   * @brief Accept new connections on a listening socket.
   * @param ctx The async context to start the acceptor on.
   * @param socket The socket to listen for connections on.
   */
  auto acceptor(async_context &ctx, const socket_dialog &socket) -> void;
  /**
   * @brief Read data from a connected socket.
   * @param ctx The async context to start the reader on.
   * @param socket the socket to read data from.
   * @param rmsg A shared pointer to a mutable read buffer.
   */
  auto reader(async_context &ctx, const socket_dialog &socket,
              std::shared_ptr<read_context> rctx) -> void;

protected:
  /** @brief Default constructor. */
  async_tcp_service() = default;
  /**
   * @brief Socket address constructor.
   * @tparam T The socket address type.
   * @param address The service address to bind.
   */
  template <typename T>
  explicit async_tcp_service(socket_address<T> address) noexcept;

private:
  /**
   * @brief Emits a span of bytes buf read from socket that must be handled by
   * the derived stream handler.
   * @param ctx The async context.
   * @param socket The socket the bytes in buf were read from.
   * @param rmsg The buffer and socket message associated with the socket
   * reader.
   * @param buf The data read from the socket in the last recvmsg.
   */
  auto emit(async_context &ctx, const socket_dialog &socket,
            std::shared_ptr<read_context> rctx = {},
            std::span<const std::byte> buf = {}) -> void;

  /**
   * @brief Initializes the server socket with options. Delegates to
   * StreamHandler::initialize if it is defined.
   * @details The base class initialize_ always sets the SO_REUSEADDR flag,
   * so that the TCP server can be restarted quickly.
   * @param socket The socket handle to configure.
   * @return A default constructed error code if successful, otherwise a system
   * error code.
   */
  [[nodiscard]] auto
  initialize_(const socket_handle &socket) -> std::error_code;

  /** @brief Stop the service. */
  auto stop_() -> void;
  /**
   * @brief The service address.
   * @note sockaddr_in6 is large enough to store either an IPV4 or an IPV6
   * address.
   */
  socket_address<sockaddr_in6> address_;
};

} // namespace net::service

#include "impl/async_tcp_service_impl.hpp" // IWYU pragma: export
#endif                                     // CPPNET_ASYNC_TCP_SERVICE_HPP
