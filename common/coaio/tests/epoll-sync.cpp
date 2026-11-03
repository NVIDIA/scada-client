/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <psa/coaio/scheduler/epoll>
#include <psa/result>

#include <version>
#if defined(__cpp_lib_coroutine) || defined(__cpp_impl_coroutine)
#  error "This test MUST have coroutine support disabled"
#endif

#if defined(__NVCC__)
#  define GTEST(name, test) TEST(NVCC##name, test)
#else
#  define GTEST(name, test) TEST(name, test)
#endif

using scheduler = psa::coaio::scheduler::epoll;

GTEST(EpollSync, NullSync) {
    auto sched = scheduler::create();
    ASSERT_TRUE(sched.is_ok());
    sched.sync();
}

psa::result<unsigned, int> fd_read(scheduler & sched, int fd) {
    unsigned ret;
    auto handle = sched.register_file_descriptor(fd);
    EXPECT_TRUE(handle.is_ok());
    int err = ::read(fd, &ret, sizeof(ret));
    EXPECT_EQ(err, sizeof(ret)) << "fd = " << fd << " errno = " << errno;
    return err < 0 ? psa::result<unsigned, int>::err(errno) : psa::result<unsigned, int>::ok(ret);
}

GTEST(EpollSync, PipeRead) {
    int pfd[2];
    unsigned value = 0xcafecafe;
    ASSERT_EQ(::pipe(pfd), 0);

    auto written = ::write(pfd[1], &value, sizeof(value));
    ASSERT_EQ(written, sizeof(value));

    auto sched = scheduler::create();
    ASSERT_TRUE(sched.is_ok());

    auto ret = fd_read(sched.ok(), pfd[0]);
    ASSERT_TRUE(ret.is_ok());
    ASSERT_EQ(ret.ok(), 0xcafecafe);
}
