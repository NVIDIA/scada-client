/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <gdakis/atomic>
#include <gdakis/seqlock>

using namespace gdakis::utils;
TEST(SeqLock, ManyRead) {
    const unsigned n_threads = 32;

    unsigned canary = 0xcafe;
    seqlock<gdakis::thread_scope_device> lock;

    std::vector<std::thread> threads;
    for (auto n = 0ul; n < n_threads; ++n) {
        threads.emplace_back([&] {
            auto start = lock.read_lock();
            EXPECT_EQ(canary, 0xcafe);
            auto end = lock.read_unlock();
            EXPECT_EQ(start, end);
        });
    }
    for (auto & thread : threads)
        thread.join();
}

TEST(SeqLock, SelfReadFail) {
    seqlock<gdakis::thread_scope_device> lock;

    auto start = lock.read_lock();
    lock.write_lock();
    auto end = lock.read_unlock();
    ASSERT_NE(end, start);

    start = lock.read_lock();
    lock.write_unlock();
    end = lock.read_unlock();
    ASSERT_NE(end, start);

    start = lock.read_lock();
    ASSERT_EQ(end, start);
    end = lock.read_unlock();
    ASSERT_EQ(end, start);
}

TEST(SeqLock, SingleWriter) {
    const unsigned n_threads = 4;
    seqlock<gdakis::thread_scope_device> lock;

    lock.write_lock();

    {
        std::vector<std::thread> threads;
        for (auto n = 0ul; n < n_threads; ++n) {
            threads.emplace_back([&] {
                auto start = lock.read_lock();
                auto end = lock.read_unlock();
                ASSERT_NE(start, end);
            });
        }
        for (auto & thread : threads)
            thread.join();
    }

    lock.write_unlock();

    {
        std::vector<std::thread> threads;
        for (auto n = 0ul; n < n_threads; ++n) {
            threads.emplace_back([&] {
                auto start = lock.read_lock();
                auto end = lock.read_unlock();
                ASSERT_EQ(start, end);
            });
        }
        for (auto & thread : threads)
            thread.join();
    }
}

#if defined(__CUDACC__)

#  include <cuda/atomic>

#  include <thrust/device_vector.h>
#  include <thrust/host_vector.h>

class CUDASeqLock : public ::testing::Test {
protected:
    static constexpr unsigned block_size = 128;
    void SetUp() override {
        int gpu_count = 0;
        if (cudaGetDeviceCount(&gpu_count) != cudaSuccess or gpu_count <= 0) {
            GTEST_SKIP() << "No CUDA capable GPUs present. Skipping GPU tests";
        }
    }
};

static std::size_t gpu_size_set[] = { 32, 64, 128, 1024, 8192, 64 * 1024, 512 * 1024 };

__global__ static void seqlock_cg_read_critical_section_kernel(
    size_t size, seqlock<gdakis::thread_scope_device> * lock, cuda::atomic<unsigned, cuda::thread_scope_device> * out) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;

    // Using cooperative groups
    auto group = cooperative_groups::coalesced_threads();

    // Test read_critical_section with cooperative groups
    lock->read_critical_section(group, [&] {
        // Have one thread increment the counter by the group size
        cooperative_groups::invoke_one(group, [&] {
            // Get total threads in the group and increment by that amount
            unsigned group_size = group.size();
            out[0].fetch_add(group_size, cuda::memory_order_relaxed);
        });
    });
    //increment out here to check both outputs should be the same
    out[1].fetch_add(1, cuda::memory_order_relaxed);
}

TEST_F(CUDASeqLock, ManyRead) {
    using seqlock = seqlock<gdakis::thread_scope_device>;
    for (const auto & size : gpu_size_set) {
        seqlock * lock;
        auto cuda_ret = cudaMalloc(&lock, sizeof(seqlock));
        ASSERT_EQ(cuda_ret, cudaSuccess);

        cuda_ret = cudaMemset(lock, 0, sizeof(seqlock));
        ASSERT_EQ(cuda_ret, cudaSuccess);

        dim3 db = size < CUDASeqLock::block_size ? size : CUDASeqLock::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        thrust::device_vector<unsigned> out(4, 0);
        seqlock_cg_read_critical_section_kernel<<<dg, db>>>(
            size, lock, reinterpret_cast<cuda::atomic<unsigned, cuda::thread_scope_device> *>(out.data().get()));
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);
        thrust::host_vector<unsigned> ret(out);

        EXPECT_EQ(ret[0], size);
        EXPECT_EQ(ret[1], size);

        EXPECT_EQ(cudaFree(lock), cudaSuccess);
    }
}

#endif
