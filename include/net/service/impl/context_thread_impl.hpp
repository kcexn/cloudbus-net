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
 * @file async_service_impl.hpp
 * @brief This file defines the asynchronous service.
 */
#pragma once
#ifndef CPPNET_CONTEXT_THREAD_IMPL_HPP
#define CPPNET_CONTEXT_THREAD_IMPL_HPP
#include "net/detail/with_lock.hpp"
#include "net/service/context_thread.hpp"

#include <stdexec/execution.hpp>
namespace net::service {
template <ServiceLike Service>
template <typename Fn>
  requires std::is_invocable_r_v<bool, Fn>
auto context_thread<Service>::isr(async_scope &scope,
                                  const socket_dialog &socket,
                                  Fn handle) -> void
{
  using namespace io::socket;
  using namespace stdexec;

  static constexpr auto BUFSIZE = 1024UL;
  static auto buffer = std::array<char, BUFSIZE>{};
  static auto msg = socket_message{.buffers = buffer};

  if (!handle())
  {
    scope.request_stop();
    return;
  }

  auto recvmsg =
      io::recvmsg(socket, msg, 0) |
      then([=, &scope](auto len) noexcept { isr(scope, socket, handle); }) |
      upon_error([](auto &&err) noexcept {});
  scope.spawn(std::move(recvmsg));
}

template <ServiceLike Service>
auto context_thread<Service>::stop(socket_type socket) noexcept -> void
{
  interrupt = std::function<void()>{};
  if (socket != io::socket::INVALID_SOCKET)
    io::socket::close(socket);
  stopped = true;
}

template <ServiceLike Service>
template <typename... Args>
auto context_thread<Service>::start(std::mutex &mtx,
                                    std::condition_variable &cvar,
                                    Args &&...args) -> void
{
  server_ = std::thread([&]() noexcept {
    using namespace detail;
    using namespace io::socket;

    auto service = Service{std::forward<Args>(args)...};
    auto isockets = std::array<socket_type, 2>{INVALID_SOCKET, INVALID_SOCKET};
    if (!socketpair(AF_UNIX, SOCK_STREAM, 0, isockets.data()))
    {
      with_lock(std::unique_lock{mtx}, [&]() noexcept {
        interrupt = [&, socket = isockets[1]]() noexcept {
          static constexpr auto message = std::array<char, 1>{};
          io::sendmsg(socket, socket_message{.buffers = message}, 0);
        };
      });

      isr(scope, poller.emplace(isockets[0]), [&]() noexcept {
        auto sigmask_ = sigmask.exchange(0);
        for (int signum = 0; auto mask = (sigmask_ >> signum); ++signum)
        {
          if (mask & (1 << 0))
            service.signal_handler(signum);
        }
        return !(sigmask_ & (1 << terminate));
      });
      cvar.notify_all();

      service.start(static_cast<async_context &>(*this));

      if (scope.get_stop_token().stop_requested())
        signal(terminate);

      while (poller.wait());
    }

    with_lock(std::unique_lock{mtx}, [&]() noexcept { stop(isockets[1]); });
    cvar.notify_all();
  });
}

template <ServiceLike Service> context_thread<Service>::~context_thread()
{
  signal(terminate);
  server_.join();
}
} // namespace net::service
#endif // CPPNET_CONTEXT_THREAD_IMPL_HPP
