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
 * @file async_tcp_service_impl.hpp
 * @brief This file defines an asynchronous tcp service.
 */
#pragma once
#ifndef CPPNET_ASYNC_TCP_SERVICE_IMPL_HPP
#define CPPNET_ASYNC_TCP_SERVICE_IMPL_HPP
#include "net/service/async_tcp_service.hpp"

#include <system_error>
namespace net::service {
template <typename TCPStreamHandler, std::size_t Size>
template <typename T>
async_tcp_service<TCPStreamHandler, Size>::async_tcp_service(
    socket_address<T> address) noexcept
    : address_{address}
{}

template <typename TCPStreamHandler, std::size_t Size>
auto async_tcp_service<TCPStreamHandler, Size>::signal_handler(
    int signum) noexcept -> void
{
  if (signum == terminate)
  {
    if constexpr (requires(TCPStreamHandler handler) {
                    { handler.stop() } -> std::same_as<void>;
                  })
    {
      static_cast<TCPStreamHandler *>(this)->stop();
    }

    stop_();
  }
}

template <typename TCPStreamHandler, std::size_t Size>
auto async_tcp_service<TCPStreamHandler, Size>::start(
    async_context &ctx) noexcept -> void
{
  using namespace io;
  using namespace io::socket;

  auto sock = socket_handle(address_->sin6_family, SOCK_STREAM, 0);
  if (auto error = initialize_(sock))
  {
    ctx.scope.request_stop();
    return;
  }

  acceptor_sockfd_ = static_cast<socket_type>(sock);

  acceptor(ctx, ctx.poller.emplace(std::move(sock)));
}

template <typename TCPStreamHandler, std::size_t Size>
auto async_tcp_service<TCPStreamHandler, Size>::acceptor(
    async_context &ctx, const socket_dialog &socket) -> void
{
  using namespace stdexec;

  sender auto accept = io::accept(socket) | then([&, socket](auto accepted) {
                         auto [dialog, addr] = std::move(accepted);
                         emit(ctx, dialog, std::make_shared<read_context>());
                         acceptor(ctx, socket);
                       }) |
                       upon_error([](auto &&error) {});

  ctx.scope.spawn(std::move(accept));
}

template <typename TCPStreamHandler, std::size_t Size>
auto async_tcp_service<TCPStreamHandler, Size>::submit_recv(
    async_context &ctx, const socket_dialog &socket,
    std::shared_ptr<read_context> rctx) -> void
{
  using namespace stdexec;
  using namespace io::socket;
  if (!rctx)
    return;

  sender auto recvmsg =
      io::recvmsg(socket, rctx->msg, 0) |
      then([&, socket, rctx](auto &&len) mutable {
        if (!len)
          return emit(ctx, socket);

        auto buf =
            std::span{rctx->buffer.data(), static_cast<std::size_t>(len)};
        emit(ctx, socket, std::move(rctx), buf);
      }) |
      upon_error([&, socket](auto &&error) { emit(ctx, socket); });

  ctx.scope.spawn(std::move(recvmsg));
}

template <typename TCPStreamHandler, std::size_t Size>
auto async_tcp_service<TCPStreamHandler, Size>::emit(
    async_context &ctx, const socket_dialog &socket,
    std::shared_ptr<read_context> rctx, std::span<const std::byte> buf) -> void
{
  static_cast<TCPStreamHandler *>(this)->service(ctx, socket, std::move(rctx),
                                                 buf);
}

template <typename TCPStreamHandler, std::size_t Size>
auto async_tcp_service<TCPStreamHandler, Size>::initialize_(
    const socket_handle &socket) -> std::error_code
{
  using namespace io;
  using namespace io::socket;

  if (auto reuse = socket_option<int>(1);
      setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reuse))
  {
    return {errno, std::system_category()};
  }

  if constexpr (requires(TCPStreamHandler handler) {
                  {
                    handler.initialize(socket)
                  } -> std::same_as<std::error_code>;
                })
  {
    if (auto error = static_cast<TCPStreamHandler *>(this)->initialize(socket))
      return error;
  }

  if (bind(socket, address_))
    return {errno, std::system_category()};

  address_ = getsockname(socket, address_);

  if (listen(socket, SOMAXCONN))
    return {errno, std::system_category()};

  return {};
}

template <typename TCPStreamHandler, std::size_t Size>
auto async_tcp_service<TCPStreamHandler, Size>::stop_() -> void
{
  using namespace io::socket;

  auto sockfd = acceptor_sockfd_.exchange(INVALID_SOCKET);
  if (sockfd != INVALID_SOCKET)
    shutdown(sockfd, SHUT_RD);
}

} // namespace net::service
#endif // CPPNET_ASYNC_TCP_SERVICE_IMPL_HPP
