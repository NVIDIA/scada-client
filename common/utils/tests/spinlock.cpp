/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <gdakis/atomic>
#include <gdakis/spinlock>

TEST(SpinLock, ManyInc) {
    const unsigned n_threads = 32;

    gdakis::spinlock<gdakis::thread_scope_device> lock;

    unsigned val = 0;
    std::vector<std::thread> threads;
    for (auto n = 0ul; n < n_threads; ++n) {
        threads.emplace_back([&] { lock.critical_section([&] { val++; }); });
    }
    for (auto & thread : threads)
        thread.join();
    EXPECT_EQ(val, n_threads);
}

#if defined(__CUDACC__)

#  include <cuda/atomic>

#  include <thrust/device_vector.h>
#  include <thrust/host_vector.h>

class CUDASpinLock : public ::testing::Test {
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

namespace cg = cooperative_groups;

__global__ static void spinlock_many_increment_kernel(
    size_t size, gdakis::spinlock<gdakis::thread_scope_device> * lock, unsigned * out) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;
    lock->critical_section([&] { out[0]++; });

    lock->critical_section(cg::tiled_partition<32>(cg::this_thread_block()), [&] { out[1]++; });
}

TEST_F(CUDASpinLock, ManyInc) {
    using spinlock = gdakis::spinlock<gdakis::thread_scope_device>;
    for (const auto & size : gpu_size_set) {
        spinlock * lock;
        auto cuda_ret = cudaMalloc(&lock, sizeof(spinlock));
        ASSERT_EQ(cuda_ret, cudaSuccess);

        cuda_ret = cudaMemset(lock, 0, sizeof(spinlock));
        ASSERT_EQ(cuda_ret, cudaSuccess);

        dim3 db = size < CUDASpinLock::block_size ? size : CUDASpinLock::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        thrust::device_vector<unsigned> out(2, 0);
        spinlock_many_increment_kernel<<<dg, db>>>(size, lock, out.data().get());
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);
        thrust::host_vector<unsigned> ret(out);

        EXPECT_EQ(ret[0], size);
        EXPECT_EQ(ret[1], size);

        EXPECT_EQ(cudaFree(lock), cudaSuccess);
    }
}

#endif
