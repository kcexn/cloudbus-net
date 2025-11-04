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
#include "test_tcp_fixture.hpp"

static int error = 0;
int listen(int __fd, int __n)
{
  errno = static_cast<int>(std::errc::interrupted);
  error = errno;
  return -1;
}

TEST_F(AsyncTcpServiceTest, ListenError)
{
  service_v4->start(*ctx);
  service_v6->start(*ctx);
  EXPECT_EQ(error, static_cast<int>(std::errc::interrupted));

  auto n = 0UL;
  ctx->signal(ctx->terminate);
  while (ctx->poller.wait_for(2000))
  {
    ASSERT_LE(n++, 2);
  }
  ASSERT_GT(n, 0);
}
// NOLINTEND
