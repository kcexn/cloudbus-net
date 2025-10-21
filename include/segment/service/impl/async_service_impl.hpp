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
 * @file async_service_impl.hpp
 * @brief This file defines the asynchronous service.
 */
#pragma once
#ifndef CLOUDBUS_ASYNC_SERVICE_IMPL_HPP
#define CLOUDBUS_ASYNC_SERVICE_IMPL_HPP
#include "segment/detail/with_lock.hpp"
#include "segment/service/async_service.hpp"

#include <stdexec/execution.hpp>
namespace cloudbus::service {

auto async_context::signal(int signum) -> void
{
  assert(signum >= 0 && signum < END && "signum must be a valid signal.");
  sigmask.fetch_or(1 << signum);
  if (interrupt)
    interrupt();
}

auto async_context::interrupt_type::operator()() const -> void
{
  using namespace detail;
  auto func = with_lock(std::unique_lock{mtx_}, [&] { return fn_; });
  func();
}

auto async_context::interrupt_type::operator=(
    std::function<void()> func) noexcept -> interrupt_type &
{
  std::lock_guard lock{mtx_};
  fn_ = std::move(func);
  return *this;
}

async_context::interrupt_type::operator bool() const noexcept
{
  std::lock_guard lock{mtx_};
  return static_cast<bool>(fn_);
}

template <ServiceLike Service>
template <typename Fn>
  requires std::is_invocable_r_v<bool, Fn>
auto async_service<Service>::isr(async_scope &scope,
                                 const socket_dialog &socket, Fn handle) -> void
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

  auto recvmsg = io::recvmsg(socket, msg, 0) |
                 then([=, &scope](auto len) noexcept {
                   isr(scope, socket, std::move(handle));
                 }) |
                 upon_error([](auto &&err) noexcept {});
  scope.spawn(std::move(recvmsg));
}

template <ServiceLike Service>
auto async_service<Service>::stop(socket_type socket) noexcept -> void
{
  interrupt = std::function<void()>{};
  stopped = true;
  if (socket != io::socket::INVALID_SOCKET)
    io::socket::close(socket);
}

template <ServiceLike Service>
template <typename... Args>
auto async_service<Service>::start(std::mutex &mtx,
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
        interrupt = [socket = isockets[1]]() noexcept {
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
      while (poller.wait());
    }

    with_lock(std::unique_lock{mtx}, [&]() noexcept { stop(isockets[1]); });
    cvar.notify_all();
  });
}

template <ServiceLike Service> async_service<Service>::~async_service()
{
  signal(terminate);
  server_.join();
}
} // namespace cloudbus::service
#endif // CLOUDBUS_ASYNC_SERVICE_IMPL_HPP
