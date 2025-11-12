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
 * @file context_thread.hpp
 * @brief This file declares an asynchronous service.
 */
#pragma once
#ifndef CPPNET_CONTEXT_THREAD_HPP
#define CPPNET_CONTEXT_THREAD_HPP
#include "async_context.hpp"

#include <mutex>
#include <thread>
/** @brief This namespace is for network services. */
namespace net::service {
/**
 * @brief A threaded asynchronous service.
 *
 * This class runs the provided service in a separate thread
 * with an asynchronous context.
 *
 * @tparam Service The service to run.
 */
template <ServiceLike Service> class context_thread : public async_context {
public:
  /** @brief Default constructor. */
  context_thread() = default;
  /** @brief Deleted copy constructor. */
  context_thread(const context_thread &) = delete;
  /** @brief Deleted move constructor. */
  context_thread(context_thread &&) = delete;
  /** @brief Deleted copy assignment. */
  auto operator=(const context_thread &) -> context_thread & = delete;
  /** @brief Deleted move assignment. */
  auto operator=(context_thread &&) -> context_thread & = delete;

  /**
   * @brief Start the asynchronous service.
   * @details This starts the provided service in a separate thread
   * with the provided asynchronous context.
   * @tparam Args Argument types for constructing the Service.
   * @param args The arguments to forward to the Service constructor.
   */
  template <typename... Args> auto start(Args &&...args) -> void;

  /** @brief The destructor signals the thread before joining it. */
  ~context_thread();

private:
  /** @brief The thread that serves the asynchronous service. */
  std::thread server_;
  /** @brief Mutex for thread-safety. */
  std::mutex mtx_;
  /** @brief Flag that guards against starting a thread twice. */
  bool started_{false};

  /** @brief Called when the async_service is stopped. */
  auto stop() noexcept -> void;
};

} // namespace net::service

#include "impl/context_thread_impl.hpp" // IWYU pragma: export

#endif // CPPNET_CONTEXT_THREAD_HPP
