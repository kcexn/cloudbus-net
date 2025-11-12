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
 * @file cppnet.hpp
 * @brief This file exports the public cppnet interface.
 */
#pragma once
#ifndef CPPNET_HPP
#define CPPNET_HPP
/** @brief This is the root namespace of cppnet. */
namespace net {}                         // namespace net
#include "service/async_context.hpp"     // IWYU pragma: export
#include "service/async_tcp_service.hpp" // IWYU pragma: export
#include "service/async_udp_service.hpp" // IWYU pragma: export
#include "service/context_thread.hpp"    // IWYU pragma: export
#include "timers/interrupt.hpp"          // IWYU pragma: export
#include "timers/timers.hpp"             // IWYU pragma: export
#endif                                   // CPPNET_HPP
