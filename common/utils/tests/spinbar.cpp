/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/atomic>
#include <gdakis/spinbar>
#include <gtest/gtest.h>

#include <thread>
#include <utility>

TEST(Spinbar, ArriveAndWait) {
    unsigned value = 0xdeadbeef;
    gdakis::spinbar<gdakis::thread_scope_device> bar(2);
    std::thread worker([&] {
        value = 0xcafe;
        bar.arrive_and_wait();
    });

    bar.arrive_and_wait();
    EXPECT_EQ(value, 0xcafe);
    worker.join();
}

TEST(Spinbar, LastArrivalAcquiresPriorArrivals) {
    unsigned value = 0xdeadbeef;
    gdakis::atomic<bool, gdakis::thread_scope_system> first_arrived(false);
    gdakis::spinbar<gdakis::thread_scope_device> bar(2);

    std::thread worker([&] {
        value = 0xcafe;
        auto token = bar.arrive();
        // Keep the scheduling handshake relaxed so only spinbar publishes value to the last arrival.
        first_arrived.store(true, gdakis::memory_order_relaxed);
        EXPECT_TRUE(bar.wait(std::move(token)));
    });

    while (!first_arrived.load(gdakis::memory_order_relaxed))
        std::this_thread::yield();

    EXPECT_TRUE(bar.arrive_and_wait());
    EXPECT_EQ(value, 0xcafe);
    worker.join();
}

TEST(Spinbar, ArriveAndWaitReuse) {
    unsigned value = 0xdead;
    gdakis::spinbar<gdakis::thread_scope_device> bar(2);

    std::thread worker([&] {
        EXPECT_EQ(value, 0xdead);
        EXPECT_TRUE(bar.arrive_and_wait());

        EXPECT_TRUE(bar.arrive_and_wait());
        EXPECT_EQ(value, 0xbeef);

        value = 0xcafe;
        EXPECT_TRUE(bar.arrive_and_wait());
    });

    EXPECT_TRUE(bar.arrive_and_wait());
    EXPECT_EQ(value, 0xdead);
    value = 0xbeef;
    EXPECT_TRUE(bar.arrive_and_wait());
    EXPECT_TRUE(bar.arrive_and_wait());
    EXPECT_EQ(value, 0xcafe);
    worker.join();
}

TEST(Spinbar, Cancel) {
    gdakis::spinbar<gdakis::thread_scope_device> bar(2);
    std::thread worker([&] { EXPECT_FALSE(bar.arrive_and_wait()); });
    EXPECT_TRUE(bar.cancel<gdakis::umem::cpu>());
    worker.join();
}
