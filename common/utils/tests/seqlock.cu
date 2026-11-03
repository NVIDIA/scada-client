/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <cooperative_groups.h>
#include <cuda/atomic>
#include <gdakis/atomic>
#include <gdakis/seqlock>
#include <gdakis/thread>

#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

using namespace gdakis::utils;

class CUDASeqLock : public ::testing::Test {
protected:
    static constexpr unsigned block_size = 128;
    void SetUp() override {
        int gpu_count = 0;
        if (cudaGetDeviceCount(&gpu_count) != cudaSuccess || gpu_count <= 0) {
            GTEST_SKIP() << "No CUDA capable GPUs present. Skipping GPU tests";
        }
    }
};

static std::size_t gpu_size_set[] = { 32, 64, 128, 1024, 2763 };

// Test for write_critical_section
__global__ static void seqlock_cg_write_critical_section_kernel(
    size_t size, seqlock<gdakis::thread_scope_device> * lock, unsigned * out) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;

    auto group = cooperative_groups::coalesced_threads();

    // Test write_critical_section with cooperative groups
    // Only one thread per tile will acquire the lock at a time, but all threads will execute in sequence
    lock->write_critical_section(group, [&] {
        atomic_thread_fence(gdakis::memory_order_acquire, gdakis::thread_scope_device);
        // Each thread in the tile will increment the counter once, but they do it sequentially. Nevertheless we need to use a thread fence to ensure that the write is visible to other threads.
        (*out)++;
        atomic_thread_fence(gdakis::memory_order_release, gdakis::thread_scope_device);
    });
}

// Structure to test atomic reads
struct TestData {
    unsigned counter;
    unsigned checksum; // Should always equal counter * 2
};

// Combined reader/writer kernel for testing read_single_thread_critical_section
__global__ static void seqlock_combined_rw_single_thread_kernel(size_t size,
    seqlock<gdakis::thread_scope_device> * lock, TestData * data,
    gdakis::atomic<unsigned, gdakis::thread_scope_device> * read_counter,
    gdakis::atomic<unsigned, gdakis::thread_scope_device> * inconsistent_reads,
    gdakis::atomic<bool, gdakis::thread_scope_device> * keep_running) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;

    // First thread acts as the writer
    if (id == 0) {
        unsigned iteration = 0;
        // Writer loop with iterations
        for (int i = 0; i < 1000; i++) {
            lock->write_critical_section([&] {
                // Update both values atomically within write lock
                data->counter = ++iteration;
                data->checksum = iteration * 2;
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

            unsigned counter_value = 0;
            unsigned checksum_value = 0;
            // Use read_single_thread_critical_section to read values atomically
            lock->read_single_thread_critical_section(group, [&] {
                // Read both values atomically
                counter_value = data->counter;
                checksum_value = data->checksum;
            });
            // Check consistency - the checksum should always equal counter*2
            if (checksum_value != counter_value * 2) {
                inconsistent_reads->fetch_add(1, gdakis::memory_order_relaxed);
            }
            // Count valid reads
            read_counter->fetch_add(1, gdakis::memory_order_relaxed);
        }
    }
}

// Combined reader/writer kernel for testing read_parallel_critical_section
__global__ static void seqlock_combined_rw_parallel_kernel(size_t size, seqlock<gdakis::thread_scope_device> * lock,
    TestData * data, gdakis::atomic<unsigned, gdakis::thread_scope_device> * read_counter,
    gdakis::atomic<unsigned, gdakis::thread_scope_device> * inconsistent_reads,
    gdakis::atomic<bool, gdakis::thread_scope_device> * keep_running) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;

    // First thread acts as the writer
    if (id == 0) {
        unsigned iteration = 0;
        // Writer loop with iterations
        for (int i = 0; i < 1000; i++) {
            lock->write_critical_section([&] {
                // Update both values atomically within write lock
                data->counter = ++iteration;
                data->checksum = iteration * 2;
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
            unsigned counter_value = 0;
            unsigned checksum_value = 0;

            // Use read_parallel_critical_section - all threads execute in parallel
            lock->read_parallel_critical_section(group, [&] {
                // Each thread reads both values (in parallel)
                counter_value = data->counter;
                checksum_value = data->checksum;
            });

            // Check consistency - the checksum should always equal counter*2
            if (checksum_value != counter_value * 2) {
                inconsistent_reads->fetch_add(1, gdakis::memory_order_relaxed);
            }
            // Count valid reads
            read_counter->fetch_add(1, gdakis::memory_order_relaxed);
        }
    }
}

// Test for write_single_thread_critical_section
__global__ static void seqlock_cg_write_single_thread_kernel(
    size_t size, seqlock<gdakis::thread_scope_device> * lock, unsigned * out) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;

    auto group = cooperative_groups::this_thread_block();
    auto tile = cooperative_groups::tiled_partition<32>(group);

    // Test write_single_thread_critical_section
    // Only one thread per tile will execute this
    lock->write_single_thread_critical_section(tile, [&] {
        // Only one thread per tile will execute this
        (*out)++;
    });
}

TEST_F(CUDASeqLock, WriteTest) {
    using seqlock_t = seqlock<gdakis::thread_scope_device>;

    for (const auto & size : gpu_size_set) {
        seqlock_t * lock;
        ASSERT_EQ(cudaMalloc(&lock, sizeof(seqlock_t)), cudaSuccess);
        ASSERT_EQ(cudaMemset(lock, 0, sizeof(seqlock_t)), cudaSuccess);

        unsigned * d_out;
        ASSERT_EQ(cudaMalloc(&d_out, sizeof(unsigned)), cudaSuccess);
        ASSERT_EQ(cudaMemset(d_out, 0, sizeof(unsigned)), cudaSuccess);
        dim3 db = size < CUDASeqLock::block_size ? size : CUDASeqLock::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        // Test write_critical_section
        seqlock_cg_write_critical_section_kernel<<<dg, db>>>(size, lock, d_out);
        ASSERT_EQ(cudaGetLastError(), cudaSuccess);

        unsigned h_out;
        ASSERT_EQ(cudaMemcpy(&h_out, d_out, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);
        EXPECT_EQ(h_out, size);

        ASSERT_EQ(cudaFree(lock), cudaSuccess);
        ASSERT_EQ(cudaFree(d_out), cudaSuccess);
    }
}

TEST_F(CUDASeqLock, ReadSingleThreadTest) {
    using seqlock_t = seqlock<gdakis::thread_scope_device>;

    for (const auto & size : gpu_size_set) {
        if (size < 2)
            continue; // Need at least 2 threads (1 writer + 1 reader)

        seqlock_t * lock;
        ASSERT_EQ(cudaMalloc(&lock, sizeof(seqlock_t)), cudaSuccess);
        ASSERT_EQ(cudaMemset(lock, 0, sizeof(seqlock_t)), cudaSuccess);

        // Allocate test data structure
        TestData * test_data;
        ASSERT_EQ(cudaMalloc(&test_data, sizeof(TestData)), cudaSuccess);
        ASSERT_EQ(cudaMemset(test_data, 0, sizeof(TestData)), cudaSuccess);

        // Atomic counters for reads and consistency checks
        gdakis::atomic<unsigned, gdakis::thread_scope_device> * read_counter;
        gdakis::atomic<unsigned, gdakis::thread_scope_device> * inconsistent_reads;
        ASSERT_EQ(
            cudaMalloc(&read_counter, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)), cudaSuccess);
        ASSERT_EQ(
            cudaMemset(read_counter, 0, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)), cudaSuccess);
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
        dim3 db = size < CUDASeqLock::block_size ? size : CUDASeqLock::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        seqlock_combined_rw_single_thread_kernel<<<dg, db>>>(
            size, lock, test_data, read_counter, inconsistent_reads, keep_running);

        // Wait for kernel to finish
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

        // Read results
        unsigned total_reads = 0;
        unsigned total_inconsistent = 0;
        ASSERT_EQ(cudaMemcpy(&total_reads, read_counter, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);
        ASSERT_EQ(
            cudaMemcpy(&total_inconsistent, inconsistent_reads, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);

        // Expect some reads to have occurred
        EXPECT_GT(total_reads, 0u);
        // Expect NO inconsistent reads - that's the whole point of the seqlock!
        EXPECT_EQ(total_inconsistent, 0u);

        // Cleanup
        EXPECT_EQ(cudaFree(keep_running), cudaSuccess);
        EXPECT_EQ(cudaFree(inconsistent_reads), cudaSuccess);
        EXPECT_EQ(cudaFree(read_counter), cudaSuccess);
        EXPECT_EQ(cudaFree(test_data), cudaSuccess);
        EXPECT_EQ(cudaFree(lock), cudaSuccess);
    }
}

TEST_F(CUDASeqLock, ReadParallelTest) {
    using seqlock_t = seqlock<gdakis::thread_scope_device>;

    for (const auto & size : gpu_size_set) {
        if (size < 2)
            continue; // Need at least 2 threads (1 writer + 1 reader)

        seqlock_t * lock;
        ASSERT_EQ(cudaMalloc(&lock, sizeof(seqlock_t)), cudaSuccess);
        ASSERT_EQ(cudaMemset(lock, 0, sizeof(seqlock_t)), cudaSuccess);

        // Allocate test data structure
        TestData * test_data;
        ASSERT_EQ(cudaMalloc(&test_data, sizeof(TestData)), cudaSuccess);
        ASSERT_EQ(cudaMemset(test_data, 0, sizeof(TestData)), cudaSuccess);

        // Atomic counters for reads and consistency checks
        gdakis::atomic<unsigned, gdakis::thread_scope_device> * read_counter;
        gdakis::atomic<unsigned, gdakis::thread_scope_device> * inconsistent_reads;
        ASSERT_EQ(
            cudaMalloc(&read_counter, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)), cudaSuccess);
        ASSERT_EQ(
            cudaMemset(read_counter, 0, sizeof(gdakis::atomic<unsigned, gdakis::thread_scope_device>)), cudaSuccess);
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
        dim3 db = size < CUDASeqLock::block_size ? size : CUDASeqLock::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        seqlock_combined_rw_parallel_kernel<<<dg, db>>>(
            size, lock, test_data, read_counter, inconsistent_reads, keep_running);

        // Wait for kernel to finish
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

        // Read results
        unsigned total_reads = 0;
        unsigned total_inconsistent = 0;
        ASSERT_EQ(cudaMemcpy(&total_reads, read_counter, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);
        ASSERT_EQ(
            cudaMemcpy(&total_inconsistent, inconsistent_reads, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);

        // Expect many reads to have occurred
        EXPECT_GT(total_reads, 0u);
        // Expect NO inconsistent reads - that's the whole point of the seqlock!
        EXPECT_EQ(total_inconsistent, 0u);

        // Cleanup
        EXPECT_EQ(cudaFree(keep_running), cudaSuccess);
        EXPECT_EQ(cudaFree(inconsistent_reads), cudaSuccess);
        EXPECT_EQ(cudaFree(read_counter), cudaSuccess);
        EXPECT_EQ(cudaFree(test_data), cudaSuccess);
        EXPECT_EQ(cudaFree(lock), cudaSuccess);
    }
}

TEST_F(CUDASeqLock, WriteSingleThreadTest) {
    using seqlock_t = seqlock<gdakis::thread_scope_device>;

    for (const auto & size : gpu_size_set) {
        seqlock_t * lock;
        ASSERT_EQ(cudaMalloc(&lock, sizeof(seqlock_t)), cudaSuccess);
        ASSERT_EQ(cudaMemset(lock, 0, sizeof(seqlock_t)), cudaSuccess);

        unsigned * out;
        ASSERT_EQ(cudaMalloc(&out, sizeof(unsigned)), cudaSuccess);
        ASSERT_EQ(cudaMemset(out, 0, sizeof(unsigned)), cudaSuccess);

        dim3 db = size < CUDASeqLock::block_size ? size : CUDASeqLock::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        // Test write_single_thread_critical_section
        seqlock_cg_write_single_thread_kernel<<<dg, db>>>(size, lock, out);
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

        unsigned h_out;
        ASSERT_EQ(cudaMemcpy(&h_out, out, sizeof(unsigned), cudaMemcpyDeviceToHost), cudaSuccess);
        // Expect number of tiles (size/32 rounded up) increments
        unsigned expected_tiles = (size + 31) / 32;
        EXPECT_EQ(h_out, expected_tiles);

        EXPECT_EQ(cudaFree(lock), cudaSuccess);
        EXPECT_EQ(cudaFree(out), cudaSuccess);
    }
}
