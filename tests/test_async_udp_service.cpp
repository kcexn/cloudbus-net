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

// NOLINTBEGIN
#include "test_udp_fixture.hpp"

TEST_F(AsyncUDPServiceTest, StartTest)
{
  service_v4->start(*ctx);
  service_v6->start(*ctx);
  ctx->signal(ctx->terminate);

  auto n = 0UL;
  while (ctx->poller.wait_for(100))
  {
    ASSERT_LE(n++, 3);
  }
  EXPECT_GT(n, 0);
}

TEST_F(AsyncUDPServiceTest, EchoTest)
{
  service_v4->start(*ctx);
  service_v6->start(*ctx);

  {
    using namespace io;
    using namespace io::socket;

    auto sock_v4 = socket_handle(AF_INET, SOCK_DGRAM, 0);
    auto sock_v6 = socket_handle(AF_INET6, SOCK_DGRAM, 0);

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message{.buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      auto len = sendmsg(sock_v4,
                         socket_message<sockaddr_in>{
                             .address = {addr_v4}, .buffers = std::span(it, 1)},
                         0);
      ASSERT_EQ(len, 1);
      len = sendmsg(sock_v6,
                    socket_message<sockaddr_in6>{.address = {addr_v6},
                                                 .buffers = std::span(it, 1)},
                    0);
      ASSERT_EQ(len, 1);

      auto n = ctx->poller.wait_for(50);
      ASSERT_GT(n, 0);

      len = recvmsg(sock_v4, msg, 0);
      ASSERT_EQ(len, 1);
      EXPECT_EQ(buf[0], *it);

      len = recvmsg(sock_v6, msg, 0);
      ASSERT_EQ(len, 1);
      EXPECT_EQ(buf[0], *it);
    }
  }

  ctx->signal(ctx->terminate);
  auto n = 0UL;
  while (ctx->poller.wait_for(100))
  {
    ASSERT_LE(n++, 2);
  }
  ASSERT_GT(n, 0);
}

TEST_F(AsyncUDPServiceTest, InitializeError)
{
  using namespace io::socket;
  service_v4->initialized = true;
  service_v4->start(*ctx);
  EXPECT_TRUE(ctx->scope.get_stop_token().stop_requested());
}

TEST_F(AsyncUDPServiceTest, AsyncServerTest)
{
  using namespace io;
  using namespace io::socket;
  using enum async_context::signals;
  using enum async_context::context_states;

  server_v4->start(addr_v4);
  server_v6->start(addr_v6);
  server_v6->state.wait(PENDING);
  server_v4->state.wait(PENDING);
  ASSERT_EQ(server_v4->state, STARTED);
  ASSERT_EQ(server_v6->state, STARTED);

  {
    auto sock_v4 = socket_handle(AF_INET, SOCK_DGRAM, 0);
    auto sock_v6 = socket_handle(AF_INET6, SOCK_DGRAM, 0);

    auto buf = std::array<char, 1>{'x'};
    auto msg = socket_message{.buffers = buf};

    const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    auto *end = alphabet + 26;

    for (auto *it = alphabet; it != end; ++it)
    {
      auto len = sendmsg(sock_v4,
                         socket_message<sockaddr_in>{
                             .address = {addr_v4}, .buffers = std::span(it, 1)},
                         0);
      ASSERT_EQ(len, 1);
      len = sendmsg(sock_v6,
                    socket_message<sockaddr_in6>{.address = {addr_v6},
                                                 .buffers = std::span(it, 1)},
                    0);
      ASSERT_EQ(len, 1);

      len = recvmsg(sock_v4, msg, 0);
      ASSERT_EQ(len, 1);
      EXPECT_EQ(buf[0], *it);

      len = recvmsg(sock_v6, msg, 0);
      ASSERT_EQ(len, 1);
      EXPECT_EQ(buf[0], *it);
    }
  }
}
// NOLINTEND
