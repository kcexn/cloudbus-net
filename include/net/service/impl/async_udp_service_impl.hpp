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
 * @file async_udp_service.hpp
 * @brief This file declares an asynchronous udp service.
 */
#pragma once
#ifndef CPPNET_ASYNC_UDP_SERVICE_IMPL_HPP
#define CPPNET_ASYNC_UDP_SERVICE_IMPL_HPP
#include "net/service/async_udp_service.hpp"
namespace net::service {

template <typename UDPStreamHandler>
template <typename T>
async_udp_service<UDPStreamHandler>::async_udp_service(
    socket_address<T> address) noexcept
    : address_{address}
{}

template <typename UDPStreamHandler>
auto async_udp_service<UDPStreamHandler>::signal_handler(int signum) noexcept
    -> void
{
  if (signum == terminate)
    stop_();
}

template <typename UDPStreamHandler>
auto async_udp_service<UDPStreamHandler>::start(async_context &ctx) noexcept
    -> void
{
  using namespace io;
  using namespace io::socket;

  auto sock = socket_handle(address_->sin6_family, SOCK_DGRAM, 0);
  if (auto error = initialize_(sock))
  {
    ctx.scope.request_stop();
    return;
  }

  server_sockfd_ = static_cast<socket_type>(sock);

  reader(ctx, ctx.poller.emplace(std::move(sock)),
         std::make_shared<read_context>());
}

template <typename UDPStreamHandler>
auto async_udp_service<UDPStreamHandler>::reader(
    async_context &ctx, const socket_dialog &socket,
    std::shared_ptr<read_context> rctx) -> void
{
  using namespace stdexec;
  using namespace io::socket;

  sender auto recvmsg =
      io::recvmsg(socket, rctx->msg, 0) |
      then([&, socket, rctx](auto &&len) mutable {
        using size_type = std::size_t;

        auto buf = std::span{rctx->buffer.data(), static_cast<size_type>(len)};
        emit(ctx, socket, std::move(rctx), buf);
      }) |
      upon_error([&, socket](auto &&error) { emit(ctx, socket); });

  ctx.scope.spawn(std::move(recvmsg));
}

template <typename UDPStreamHandler>
auto async_udp_service<UDPStreamHandler>::emit(
    async_context &ctx, const socket_dialog &socket,
    std::shared_ptr<read_context> rctx, std::span<const std::byte> buf) -> void
{
  auto &handle = static_cast<UDPStreamHandler &>(*this);
  handle(ctx, socket, std::move(rctx), buf);
}

template <typename UDPStreamHandler>
[[nodiscard]] auto async_udp_service<UDPStreamHandler>::initialize_(
    const socket_handle &socket) -> std::error_code
{
  using namespace io;
  using namespace io::socket;

  if (auto reuse = socket_option<int>(1);
      setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reuse))
  {
    return {errno, std::system_category()};
  }

  if constexpr (requires(UDPStreamHandler handler) {
                  {
                    handler.initialize(socket)
                  } -> std::same_as<std::error_code>;
                })
  {
    if (auto error = static_cast<UDPStreamHandler *>(this)->initialize(socket))
      return error;
  }

  if (bind(socket, address_))
    return {errno, std::system_category()};

  address_ = getsockname(socket, address_);

  return {};
}

template <typename UDPStreamHandler>
auto async_udp_service<UDPStreamHandler>::stop_() -> void
{
  using namespace io::socket;

  auto sockfd = server_sockfd_.exchange(INVALID_SOCKET);
  shutdown(sockfd, SHUT_RD);
}

} // namespace net::service
#endif // CPPNET_ASYNC_UDP_SERVICE_IMPL_HPP
