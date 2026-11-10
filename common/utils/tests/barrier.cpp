/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/barrier>
#include <gtest/gtest.h>

#include <thread>

TEST(Barrier, ArriveAndWait) {
    unsigned value = 0xdeadbeef;
    gdakis::barrier barrier(2);

    std::thread worker([&] {
        value = 0xcafe;
        barrier.arrive_and_wait();
    });

    barrier.arrive_and_wait();
    EXPECT_EQ(value, 0xcafe);
    worker.join();
}

TEST(Barrier, ArriveAndWaitReuse) {
    unsigned value = 0xdead;
    gdakis::barrier barrier(2);

    std::thread worker([&] {
        EXPECT_EQ(value, 0xdead);
        barrier.arrive_and_wait();

        barrier.arrive_and_wait();
        EXPECT_EQ(value, 0xbeef);

        value = 0xcafe;
        barrier.arrive_and_wait();
    });

    barrier.arrive_and_wait();
    EXPECT_EQ(value, 0xdead);

    value = 0xbeef;
    barrier.arrive_and_wait();

    barrier.arrive_and_wait();
    EXPECT_EQ(value, 0xcafe);
    worker.join();
}

TEST(Barrier, ArriveAndDropOne) {
    unsigned value = 0xdead;
    gdakis::barrier barrier(2);

    std::thread worker([&] {
        EXPECT_EQ(value, 0xdead);
        barrier.arrive_and_wait();
        value = 0xcafe;
        barrier.arrive_and_drop();
    });

    EXPECT_EQ(value, 0xdead);
    barrier.arrive_and_wait();

    barrier.arrive_and_wait();
    EXPECT_EQ(value, 0xcafe);

    barrier.arrive_and_wait();
    value = 0xbeef;
    barrier.arrive_and_wait();
    EXPECT_EQ(value, 0xbeef);

    worker.join();
}

TEST(Barrier, ArriveAndDropMany) {
    unsigned value = 0xdead;
    std::vector<std::thread> threads;
    gdakis::barrier barrier(16);
    for (unsigned n = 0; n < 16; ++n) {
        threads.emplace_back(std::thread([n, &barrier, &value] {
            for (unsigned m = 0; m <= n; ++m) {
                EXPECT_EQ(value, 0xdead + m);

                barrier.arrive_and_wait();

                if (m == n) {
                    ++value;
                    barrier.arrive_and_drop();
                } else {
                    barrier.arrive_and_wait();
                }
            }
        }));
    }
    for (auto & thread : threads)
        thread.join();
}