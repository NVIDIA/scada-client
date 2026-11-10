/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/mpbar>
#include <tests/utility>

#include <thread>
#include <vector>

TEST(MPBar, Create) {
    gdakis::mpbar bar(2);
}

TEST(MPBar, ArriveWaitThreads) {
    static constexpr size_t n_threads = 32;
    gdakis::mpbar bar(n_threads);

    std::vector<std::thread> threads;
    for (auto n = 0ul; n < n_threads; ++n) {
        threads.emplace_back(std::thread([&bar]() { EXPECT_TRUE(bar.arrive_and_wait()); }));
    }
    for (auto & thread : threads)
        thread.join();
}

TEST(MPBar, ArriveWaitProcess) {
    static constexpr size_t n_processes = 8;

    auto bar = tests::shmem<gdakis::mpbar>::make(n_processes);
    ASSERT_TRUE(bar.is_ok());

    std::vector<tests::process_handle> processes;
    for (auto n = 0ul; n < n_processes; ++n) {
        processes.emplace_back(tests::process([&bar] { ASSERT_TRUE(bar.ok().get().arrive_and_wait()); }));
    }

    for (auto & process : processes) {
        ASSERT_TRUE(process.wait());
    }
}

TEST(MPBar, ArriveDropThreads) {
    static constexpr size_t n_threads = 8;
    gdakis::mpbar bar(n_threads);

    std::vector<std::thread> threads;
    for (auto n = 0ul; n < n_threads; ++n) {
        threads.emplace_back(std::thread([&] {
            for (auto m = 0ul; m < n; ++m) {
                ASSERT_TRUE(bar.arrive_and_wait());
            }
            bar.arrive_and_drop();
        }));
    }
    for (auto & thread : threads)
        thread.join();
}

TEST(MPBar, ArriveDropProcess) {
    static constexpr size_t n_processes = 8;
    auto bar = tests::shmem<gdakis::mpbar>::make(n_processes);
    std::vector<tests::process_handle> processes;
    for (auto n = 0ul; n < n_processes; ++n) {
        processes.emplace_back(tests::process([&] {
            for (auto m = 0ul; m < n; ++m) {
                ASSERT_TRUE(bar.ok().get().arrive_and_wait());
            }
            bar.ok().get().arrive_and_drop();
        }));
    }
    for (auto & process : processes) {
        ASSERT_TRUE(process.wait());
    }
}

TEST(MPBar, ArriveCancelProcess) {
    static constexpr size_t n_processes = 4;

    auto bar = tests::shmem<gdakis::mpbar>::make(n_processes + 1);
    std::vector<tests::process_handle> processes;
    for (auto n = 0ul; n < n_processes; ++n) {
        processes.emplace_back(tests::process([&bar] { ASSERT_FALSE(bar.ok().get().arrive_and_wait()); }));
    }

    bar.ok().get().cancel();
    for (auto & process : processes) {
        ASSERT_TRUE(process.wait());
    }
}

TEST(MPBar, ArriveWaitAndCancelAfterDeath) {
    auto bar = tests::shmem<gdakis::mpbar>::make(2);
    auto process = tests::process([&bar] { bar.ok().get().arrive_and_wait(); });
    process.kill(SIGTERM);
    bar.ok().get().cancel();
    process.wait();
}

TEST(MPBar, ArriveWaitManyAndCancelAfterDeath) {
    static constexpr size_t num_iter = 32;
    auto bar = tests::shmem<gdakis::mpbar>::make(2);
    auto process = tests::process([&bar] {
        for (auto n = 0ul; n < num_iter; ++n) {
            ASSERT_TRUE(bar.ok().get().arrive_and_wait());
        }
    });
    for (auto n = 0ul; n < (num_iter >> 1); ++n) {
        ASSERT_TRUE(bar.ok().get().arrive_and_wait());
    };
    process.kill(SIGTERM);
    bar.ok().get().cancel();
    process.wait();
}
