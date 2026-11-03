/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <psa/coaio/scheduler/epoll>
#include <psa/futures>

#include <fcntl.h>

#if defined(__NVCC__)
#  define GTEST(name, test) TEST(NVCC##name, test)
#else
#  define GTEST(name, test) TEST(name, test)
#endif

using scheduler = psa::coaio::scheduler::epoll;

GTEST(Epoll, NullSync) {
    auto sched = scheduler::create();
    sched.sync();
}

#if defined(__psa_use_coroutines)
psa::futures::result<unsigned, int> fd_co_read(scheduler & sched, int fd) {
    unsigned ret;
    auto handle = sched.register_file_descriptor(fd);
    EXPECT_TRUE(handle.is_ok());
    co_return co_await handle.ok().await(scheduler::event_type::read, [fd, &ret] {
        int err = ::read(fd, &ret, sizeof(ret));
        if (err >= 0) {
            return scheduler::result<unsigned>::ok(ret);
        }
        return scheduler::result<unsigned>::err(errno == EWOULDBLOCK ? EAGAIN : errno);
    });
}

GTEST(Epoll, PipeRead) {
    int pfd[2];
    unsigned value = 0xcafecafe;
    ASSERT_EQ(::pipe(pfd), 0);

    int flags = fcntl(pfd[0], F_GETFL, 0);
    fcntl(pfd[0], F_SETFL, flags | O_NONBLOCK);

    auto sched = scheduler::create();
    ASSERT_TRUE(sched.is_ok());
    auto aret = fd_co_read(sched.ok(), pfd[0]);

    auto written = ::write(pfd[1], &value, sizeof(value));
    ASSERT_EQ(written, sizeof(value));

    auto ret = aret.sync();
    ASSERT_TRUE(ret.is_ok());
    ASSERT_EQ(ret.ok(), 0xcafecafe);
}

psa::futures::result<unsigned, int> fd_co_read_err(scheduler & sched, int fd) {
    auto handle = sched.register_file_descriptor(fd);
    EXPECT_TRUE(handle.is_ok());
    co_return co_await handle.ok().await(
        scheduler::event_type::read, [] { return scheduler::result<unsigned>::err(EINVAL); });
}

GTEST(Epoll, PipeReadError) {
    int pfd[2];
    ASSERT_EQ(::pipe(pfd), 0);

    int flags = fcntl(pfd[0], F_GETFL, 0);
    fcntl(pfd[0], F_SETFL, flags | O_NONBLOCK);

    auto sched = scheduler::create();
    ASSERT_TRUE(sched.is_ok());
    auto aret = fd_co_read_err(sched.ok(), pfd[0]);

    auto ret = aret.sync();
    ASSERT_TRUE(ret.is_err());
    ASSERT_EQ(ret.err(), EINVAL);
}
#endif
