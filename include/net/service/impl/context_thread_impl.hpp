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
 * @file context_thread_impl.hpp
 * @brief This file defines the asynchronous service.
 */
#pragma once
#ifndef CPPNET_CONTEXT_THREAD_IMPL_HPP
#define CPPNET_CONTEXT_THREAD_IMPL_HPP
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
    return;

  auto recvmsg =
      io::recvmsg(socket, msg, 0) |
      then([=, &scope](auto len) noexcept { isr(scope, socket, handle); }) |
      upon_error([](auto &&err) noexcept {});
  scope.spawn(std::move(recvmsg));
}

template <ServiceLike Service>
auto context_thread<Service>::stop() noexcept -> void
{
  auto socket = timers.sockets[1];
  timers.sockets[1] = timers.INVALID_SOCKET;
  if (socket != timers.INVALID_SOCKET)
    io::socket::close(socket);
  state = STOPPED;
}

template <ServiceLike Service>
template <typename... Args>
auto context_thread<Service>::start(Args &&...args) -> void
{
  auto lock = std::lock_guard{mtx_};
  if (started_)
    throw std::invalid_argument("context_thread can't be started twice.");

  server_ = std::thread([&]() noexcept {
    using namespace detail;
    using namespace io::socket;
    using namespace std::chrono;

    auto service = Service{std::forward<Args>(args)...};
    auto &sockets = timers.sockets;
    if (!socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()))
    {
      const auto token = scope.get_stop_token();

      isr(scope, poller.emplace(sockets[0]), [&]() noexcept {
        auto sigmask_ = sigmask.exchange(0);
        for (int signum = 0; auto mask = (sigmask_ >> signum); ++signum)
        {
          if (mask & (1 << 0))
            service.signal_handler(signum);
        }

        if (sigmask_ & (1 << terminate))
        {
          scope.request_stop();
          timers.add(
              seconds(1),
              [&](timers::timer_id) { service.signal_handler(terminate); },
              seconds(1));
        }

        return !token.stop_requested();
      });

      service.start(static_cast<async_context &>(*this));
      state = STARTED;

      if (token.stop_requested())
      {
        state = STOPPED;
        signal(terminate);
      }

      state.notify_all();
      run(service, token);
    }

    stop();
    state.notify_all();
  });

  started_ = true;
}

template <ServiceLike Service> context_thread<Service>::~context_thread()
{
  if (!started_)
    return;

  signal(terminate);
  server_.join();
}

template <ServiceLike Service>
template <typename StopToken>
auto context_thread<Service>::run(Service &service,
                                  const StopToken &token) -> void
{
  using namespace stdexec;
  using namespace std::chrono;

  auto next = timers.resolve();
  int wait_ms =
      (next.count() < 0) ? next.count() : duration_cast<duration>(next).count();

  auto is_empty = std::atomic_flag();
  scope.spawn(poller.on_empty() |
              then([&]() noexcept { is_empty.test_and_set(); }));

  while (poller.wait_for(wait_ms) || !is_empty.test())
  {
    next = timers.resolve();
    wait_ms = (next.count() < 0) ? next.count()
                                 : duration_cast<duration>(next).count();
  }
}
} // namespace net::service
#endif // CPPNET_CONTEXT_THREAD_IMPL_HPP
