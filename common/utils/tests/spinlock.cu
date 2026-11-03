/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <cooperative_groups.h>
#include <cuda/atomic>
#include <gdakis/atomic>
#include <gdakis/spinlock>

#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

using namespace gdakis::utils;

class CUDASpinLock : public ::testing::Test {
protected:
    static constexpr unsigned block_size = 128;
    void SetUp() override {
        int gpu_count = 0;
        if (cudaGetDeviceCount(&gpu_count) != cudaSuccess || gpu_count <= 0) {
            GTEST_SKIP() << "No CUDA capable GPUs present. Skipping GPU tests";
        }
    }
};

static std::size_t gpu_size_set[] = { 32, 64, 128, 1024 };

// Test for parallel_critical_section
__global__ static void spinlock_cg_parallel_critical_section_kernel(size_t size,
    gdakis::spinlock<gdakis::thread_scope_device> * lock,
    gdakis::atomic<unsigned, gdakis::thread_scope_device> * counter) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;

    auto group = cooperative_groups::coalesced_threads();

    // Test parallel_critical_section with cooperative groups
    // All threads in the group execute in parallel within the critical section
    lock->parallel_critical_section(group, [&] {
        // Each thread increments the counter
        (*counter)++;
    });
}

// Test for single_thread_critical_section
__global__ static void spinlock_cg_single_thread_critical_section_kernel(size_t size,
    gdakis::spinlock<gdakis::thread_scope_device> * lock,
    gdakis::atomic<unsigned, gdakis::thread_scope_device> * counter) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;

    auto group = cooperative_groups::coalesced_threads();

    // Test single_thread_critical_section
    // Only one thread per cooperative group will execute this
    lock->single_thread_critical_section(group, [&] {
        // Only one thread per group will execute this
        (*counter)++;
    });
}

// Test structure to verify correctness of concurrent access
struct TestData {
    unsigned value; // Value being modified
    unsigned signature; // Should always match value * 3
};

#ifdef NDEBUG

// Combined reader/writer for testing thread safety of spinlock
__global__ static void spinlock_combined_rw_kernel(size_t size, gdakis::spinlock<gdakis::thread_scope_device> * lock,
    TestData * data, gdakis::atomic<unsigned, gdakis::thread_scope_device> * operations_counter,
    gdakis::atomic<unsigned, gdakis::thread_scope_device> * inconsistent_reads,
    gdakis::atomic<bool, gdakis::thread_scope_device> * keep_running) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;

    // First thread acts as the writer that updates both values
    if (id == 0) {
        // Writer loop
        for (int i = 0; i < 50; i++) {
            lock->critical_section([&] {
                // Update both values atomically within the critical section
                data->value = i;
                data->signature = i * 3;
            });
            // Small pause to allow readers to get some work
            gdakis::this_thread::nanosleep(50);
        }
        // Signal all threads to complete
        keep_running->store(false, gdakis::memory_order_release);
    } else {
        // All other threads act as readers
        auto group = cooperative_groups::coalesced_threads();

        // Reader loop
        while (keep_running->load(gdakis::memory_order_acquire)) {
            unsigned value = 0;
            unsigned signature = 0;

            // Use parallel_critical_section - all threads execute in parallel
            lock->parallel_critical_section(group, [&] {
                // Each thread reads both values (in parallel)
                value = data->value;
                signature = data->signature;
            });

            // Check consistency - the signature should always equal value*3
            if (signature != value * 3) {
                inconsistent_reads->fetch_add(1, gdakis::memory_order_relaxed);
            }
            // Count read operations
            operations_counter->fetch_add(1, gdakis::memory_order_relaxed);
        }
    }
}

#endif // NDEBUG

TEST_F(CUDASpinLock, ParallelCriticalSectionTest) {
    using spinlock_t = gdakis::spinlock<gdakis::thread_scope_device>;

    for (const auto & size : gpu_size_set) {
        spinlock_t * lock;
        ASSERT_EQ(cudaMalloc(&lock, sizeof(spinlock_t)), cudaSuccess);
        ASSERT_EQ(cudaMemset(lock, 0, sizeof(spinlock_t)), cudaSuccess);

        gdakis::atomic<unsigned, gdakis::thread_scope_device> * counter;
        ASSERT_EQ(cudaMalloc(&counter, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)), cudaSuccess);
        ASSERT_EQ(cudaMemset(counter, 0, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)), cudaSuccess);

        dim3 db = size < CUDASpinLock::block_size ? size : CUDASpinLock::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        // Test parallel_critical_section
        spinlock_cg_parallel_critical_section_kernel<<<dg, db>>>(size, lock, counter);
        ASSERT_EQ(cudaGetLastError(), cudaSuccess);

        unsigned h_counter;
        ASSERT_EQ(cudaMemcpy(&h_counter, counter, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);

        // Each thread should increment exactly once
        EXPECT_EQ(h_counter, size);

        EXPECT_EQ(cudaFree(counter), cudaSuccess);
        EXPECT_EQ(cudaFree(lock), cudaSuccess);
    }
}

TEST_F(CUDASpinLock, SingleThreadCriticalSectionTest) {
    using spinlock_t = gdakis::spinlock<gdakis::thread_scope_device>;

    for (const auto & size : gpu_size_set) {
        spinlock_t * lock;
        ASSERT_EQ(cudaMalloc(&lock, sizeof(spinlock_t)), cudaSuccess);
        ASSERT_EQ(cudaMemset(lock, 0, sizeof(spinlock_t)), cudaSuccess);

        gdakis::atomic<unsigned, gdakis::thread_scope_device> * counter;
        ASSERT_EQ(cudaMalloc(&counter, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)), cudaSuccess);
        ASSERT_EQ(cudaMemset(counter, 0, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)), cudaSuccess);

        dim3 db = size < CUDASpinLock::block_size ? size : CUDASpinLock::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        // Test single_thread_critical_section
        spinlock_cg_single_thread_critical_section_kernel<<<dg, db>>>(size, lock, counter);
        ASSERT_EQ(cudaGetLastError(), cudaSuccess);

        unsigned h_counter;
        ASSERT_EQ(cudaMemcpy(&h_counter, counter, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);

        // Number of increments should be number of coalesced thread groups formed
        // This is harder to predict exactly, but should be less than the total size
        EXPECT_GT(h_counter, 0u);
        EXPECT_LT(h_counter, size);

        EXPECT_EQ(cudaFree(counter), cudaSuccess);
        EXPECT_EQ(cudaFree(lock), cudaSuccess);
    }
}

TEST_F(CUDASpinLock, ConcurrentAccessTest) {
    using spinlock_t = gdakis::spinlock<gdakis::thread_scope_device>;
#ifndef NDEBUG
    GTEST_SKIP() << "Skipping spinlock test in debug mode";
#else
    for (const auto & size : gpu_size_set) {
        if (size < 64)
            continue; // Need at least 2 warps

        spinlock_t * lock;
        ASSERT_EQ(cudaMalloc(&lock, sizeof(spinlock_t)), cudaSuccess);
        ASSERT_EQ(cudaMemset(lock, 0, sizeof(spinlock_t)), cudaSuccess);

        // Allocate test data structure
        TestData * test_data;
        ASSERT_EQ(cudaMalloc(&test_data, sizeof(TestData)), cudaSuccess);
        ASSERT_EQ(cudaMemset(test_data, 0, sizeof(TestData)), cudaSuccess);

        // Atomic counters for operations and consistency checks
        gdakis::atomic<unsigned, gdakis::thread_scope_device> * operations_counter;
        gdakis::atomic<unsigned, gdakis::thread_scope_device> * inconsistent_reads;
        ASSERT_EQ(cudaMalloc(&operations_counter, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)),
            cudaSuccess);
        ASSERT_EQ(cudaMemset(operations_counter, 0, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)),
            cudaSuccess);
        ASSERT_EQ(cudaMalloc(&inconsistent_reads, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)),
            cudaSuccess);
        ASSERT_EQ(cudaMemset(inconsistent_reads, 0, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)),
            cudaSuccess);

        // Control flag for reader threads
        gdakis::atomic<bool, gdakis::thread_scope_device> * keep_running;
        ASSERT_EQ(cudaMalloc(&keep_running, sizeof(gdakis::atomic<bool, gdakis::thread_scope_device>)), cudaSuccess);
        bool host_true = true;
        ASSERT_EQ(cudaMemcpy(keep_running, &host_true, sizeof(bool), cudaMemcpyHostToDevice), cudaSuccess);

        // Launch the combined reader/writer kernel
        dim3 db = size < CUDASpinLock::block_size ? size : CUDASpinLock::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        spinlock_combined_rw_kernel<<<dg, db>>>(
            size, lock, test_data, operations_counter, inconsistent_reads, keep_running);

        ASSERT_EQ(cudaGetLastError(), cudaSuccess);

        // Read results
        unsigned total_operations = 0;
        unsigned total_inconsistent = 0;
        ASSERT_EQ(
            cudaMemcpy(&total_operations, operations_counter, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);
        ASSERT_EQ(
            cudaMemcpy(&total_inconsistent, inconsistent_reads, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);

        // Expect some operations to have occurred
        EXPECT_GT(total_operations, 0u);
        // Expect NO inconsistent reads - that's the whole point of the spinlock!
        EXPECT_EQ(total_inconsistent, 0u);

        // Cleanup
        EXPECT_EQ(cudaFree(keep_running), cudaSuccess);
        EXPECT_EQ(cudaFree(inconsistent_reads), cudaSuccess);
        EXPECT_EQ(cudaFree(operations_counter), cudaSuccess);
        EXPECT_EQ(cudaFree(test_data), cudaSuccess);
        EXPECT_EQ(cudaFree(lock), cudaSuccess);
    }
#endif
}
