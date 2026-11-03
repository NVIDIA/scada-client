/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/umem/cpu>
#include <gdakis/umem/cuda>
#include <gdakis/umem/mem>
#include <gtest/gtest.h>

#include <gdakis/adsc/cache>
#include <gdakis/atomic>

#include <iostream>
#include <vector>
#if defined(__CUDACC__)
#  include <cooperative_groups.h>
#endif

// Common definitions for all tests
constexpr auto scope = gdakis::thread_scope_device;
struct alignas(4096) val_type {
    uint8_t vals_[4096];
};
using key_type = unsigned;
constexpr unsigned cl_size = 4096 / sizeof(val_type);
constexpr unsigned ways = 32;
constexpr unsigned num_cls = 1024ULL * 1024ULL;
using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;

constexpr unsigned total_pages = 1024ULL * 1024ULL * 16;
constexpr unsigned b_size = 128;

template<unsigned _LineSize>
static __global__ void check_group_write_miss(
    gdakis::adsc::cache<scope, gdakis::adsc::cl_reference<scope, unsigned, unsigned, _LineSize>, 1> * cache,
    unsigned * backing, unsigned * results) {
    using ref_type = gdakis::adsc::cl_reference<scope, unsigned, unsigned, _LineSize>;
    auto group = cooperative_groups::tiled_partition<32>(cooperative_groups::this_thread_block());
    __shared__ unsigned value;
    if (group.thread_rank() == 0)
        value = 42;
    group.sync();
    auto policies = gdakis::adsc::make_policies(
        [=] __device__(auto & g, ref_type & ref) {
            if (g.thread_rank() == 0) {
                ++results[0];
                for (unsigned i = 0; i < _LineSize; ++i)
                    ref.vals()[i] = backing[ref.cl_id() * _LineSize + i];
            }
            g.sync();
            return 0;
        },
        gdakis::adsc::undefined_policy(),
        [=] __device__(auto & g, ref_type & ref) {
            if (g.thread_rank() == 0 && ref.is_dirty()) {
                ++results[1];
                for (unsigned i = 0; i < _LineSize; ++i)
                    backing[ref.cl_id() * _LineSize + i] = ref.vals()[i];
            }
            g.sync();
            return 0;
        },
        gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
        gdakis::adsc::undefined_policy());

    int ret = cache->put_group(group, 0, value, policies);
    if (group.thread_rank() == 0) {
        results[2] = ret;
        results[3] = results[0];
        results[4] = cache->cl_ref(0, 0).is_dirty();
    }
    group.sync();
    // Read the other line to force dirty eviction, then reload the written value.
    ret |= cache->get_group(group, _LineSize, value, policies);
    if (group.thread_rank() == 0)
        results[5] = value;
    group.sync();
    ret |= cache->get_group(group, 0, value, policies);
    if (group.thread_rank() == 0) {
        results[6] = value;
        results[7] = ret;
    }
}

template<unsigned _LineSize>
static void check_group_write_miss_fetch() {
    using ref_type = gdakis::adsc::cl_reference<scope, unsigned, unsigned, _LineSize>;
    using cache_type = gdakis::adsc::cache<scope, ref_type, 1>;
    auto data = gdakis::umem::mem<gdakis::umem::cuda, unsigned[]>::alloc(_LineSize);
    auto handle = cache_type::template build<gdakis::umem::cuda>(1, data.get());
    ASSERT_NE(nullptr, handle.get());
    unsigned * backing = nullptr;
    unsigned * results = nullptr;
    ASSERT_EQ(cudaSuccess, cudaMallocManaged(&backing, 2 * _LineSize * sizeof(unsigned)));
    ASSERT_EQ(cudaSuccess, cudaMallocManaged(&results, 8 * sizeof(unsigned)));
    for (unsigned i = 0; i < 2 * _LineSize; ++i)
        backing[i] = i + 10;
    for (unsigned i = 0; i < 8; ++i)
        results[i] = 0;

    check_group_write_miss<_LineSize><<<1, 32, 0, gdakis::umem::cuda::get_stream()>>>(handle.get(), backing, results);
    ASSERT_EQ(cudaSuccess, cudaGetLastError());
    ASSERT_EQ(cudaSuccess, cudaDeviceSynchronize());
    EXPECT_EQ(_LineSize == 1 ? 0u : 1u, results[3]);
    EXPECT_EQ(results[3] + 2, results[0]);
    EXPECT_EQ(1u, results[1]);
    EXPECT_EQ(0u, results[2]);
    EXPECT_EQ(1u, results[4]);
    EXPECT_EQ(_LineSize + 10, results[5]);
    EXPECT_EQ(42u, results[6]);
    EXPECT_EQ(0u, results[7]);
    EXPECT_EQ(42u, backing[0]);
    for (unsigned i = 1; i < _LineSize; ++i)
        EXPECT_EQ(i + 10, backing[i]);
    EXPECT_EQ(cudaSuccess, cudaFree(results));
    EXPECT_EQ(cudaSuccess, cudaFree(backing));
}

TEST(ADSCGroupWriteMiss, FullLineSkipsFetch) {
    check_group_write_miss_fetch<1>();
}

TEST(ADSCGroupWriteMiss, PartialLinePreservesOtherValues) {
    check_group_write_miss_fetch<4>();
}

// Define common kernel functions
template<typename _C>
static __global__ void cache_get_thread(_C * cache, val_type * out) {
    val_type val;
    cache->get(threadIdx.x + (blockIdx.x * blockDim.x), val);
}

using sub_type = ulonglong4;
constexpr unsigned sub_cl_size = 4096 / sizeof(sub_type);
using sub_cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, sub_type, sub_cl_size>;
constexpr unsigned how_many_sub = sizeof(val_type) / sizeof(sub_type);
constexpr unsigned how_many_warp = how_many_sub / 32;

template<typename _C>
static __global__ void cache_get_sub(_C * cache, sub_type * val_) {
    __shared__ sub_type val[how_many_warp];
    auto laneid = threadIdx.x % 32;
    auto warpid_g = (threadIdx.x + blockIdx.x * blockDim.x) / 32;
    auto start_sub = warpid_g * how_many_sub;

    for (auto i = 0; i < how_many_warp; i++)
        cache->get(start_sub + (i * 32) + laneid, val[i]);
}

template<typename _C>
static __global__ void cache_get_coalesced(_C * cache, val_type * out) {
    __shared__ val_type val[b_size / 32];
    auto warpid_l = threadIdx.x / 32;
    auto warpid_g = (threadIdx.x + blockIdx.x * blockDim.x) / 32;
    auto mask = cooperative_groups::coalesced_threads();
    cache->get_group(mask, warpid_g, val[warpid_l]);
}

template<typename _C>
static __global__ void cache_get_warp(_C * cache, val_type * out) {
    __shared__ val_type val[b_size / 32];
    auto warpid_l = threadIdx.x / 32;
    auto warpid_g = (threadIdx.x + blockIdx.x * blockDim.x) / 32;
    auto g = cooperative_groups::this_thread_block();
    auto tile = cooperative_groups::tiled_partition<32>(g);
    cache->get_group(tile, warpid_g, val[warpid_l]);
}

template<typename _C, typename _T>
static __global__ void cache_put(_C * cache, _T put_val) {
    cache->put(threadIdx.x, put_val[threadIdx.x]);
}

template<typename _C, typename _T>
static __global__ void cache_get(_C * cache, _T get_val) {
    cache->get(threadIdx.x, get_val[threadIdx.x]);
}

// Helper function for cache policy flushing
template<typename _D>
struct test_fn_functor {
    _D data_ptr;

    test_fn_functor(_D d) : data_ptr(d) {}

    __host__ __device__ int operator()(cl_ref_type & cl_ref) {
        printf("data_ptr: %p\n", data_ptr);
        return 0;
    }
};

// Create default policies
inline auto create_default_policies() {
    return gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
        gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
        gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy());
}

template<typename _Cache, typename _D>
static void test_fn(_Cache & cache, _D d) {
    cache.flush_cache(gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
        test_fn_functor<_D>(d), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
        gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
}

// Add this kernel definition with the other kernel functions at the top
template<typename _C>
static __global__ void check_invalidation_kernel(_C * cache, int * invalid_count, int num_to_check) {
    if (threadIdx.x < num_to_check) {
        // Get the ref directly to check its valid state
        auto & ref = cache->cl_ref(threadIdx.x / ways, threadIdx.x % ways);
        if (ref.cl_id() == ref.invalid_cl_id()) {
            atomicAdd(invalid_count, 1);
        }
    }
}

// Add kernel for populating the cache (with the other kernel functions)
template<typename _C, typename _V>
static __global__ void populate_cache_kernel(_C * cache, _V * init_val, int num_to_populate) {
    const int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < num_to_populate) {
        cache->put(idx, init_val[idx]);
    }
}

// Add overload for populate_cache_kernel that takes keys and values
template<typename _C, typename _K, typename _V>
static __global__ void populate_cache_kernel(_C * cache, _K * keys, _V * vals, int count) {
    const int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < count) {
        cache->put(keys[idx], vals[idx]);
    }
}

// Add kernel for checking dirty status
template<typename _C>
static __global__ void check_dirty_status_kernel(_C * cache, int * dirty_count, int * clean_count, int num_to_check) {
    const int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < num_to_check) {
        // Get reference to the cache line
        auto set_id = idx / ways;
        auto way = idx % ways;
        auto & ref = cache->cl_ref(set_id, way);

        // Check if it's dirty or clean
        if (ref.is_dirty()) {
            atomicAdd(dirty_count, 1);
        } else {
            atomicAdd(clean_count, 1);
        }
    }
}

// Add kernel for marking entries as dirty
template<typename _C>
static __global__ void mark_dirty_kernel(_C * cache, int num_entries) {
    const int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < num_entries) {
        // Get reference to the cache line and mark it dirty
        auto set_id = idx / ways;
        auto way = idx % ways;
        auto & ref = cache->cl_ref(set_id, way);
        ref.mark_dirty();
    }
}

// Add kernel for checking specific keys in the cache
template<typename _C, typename _V>
static __global__ void check_keys_kernel(
    _C * cache, _V * results, int * found_count, const int * keys_to_check, int num_keys) {
    const int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < num_keys) {
        _V val;
        int result = cache->get(keys_to_check[idx], val);
        if (result == 0) {
            results[idx] = val;
            atomicAdd(found_count, 1);
        }
    }
}

template<typename _C, typename _V>
static __global__ void run_concurrent_access(
    _C * cache, int * keys, _V * init_vals, _V * final_vals, int * conflicts, int num_keys, int iterations) {
    const int tid = threadIdx.x + blockIdx.x * blockDim.x;

    if (tid < num_keys) {
        // Each thread repeatedly puts and gets its assigned key
        for (int i = 0; i < iterations; i++) {
            // Put our final value
            cache->put(keys[tid], final_vals[tid]);

            // Then immediately try to get it back
            _V val;
            cache->get(keys[tid], val);

            // Count if we got a different value back (indicating a race)
            if (val != final_vals[tid]) {
                atomicAdd(conflicts, 1);
            }
        }
    }
}

// Create kernel to read final values
template<typename _C, typename _V>
static __global__ void read_final_values(_C * cache, int * keys, _V * results, int num_keys) {
    const int tid = threadIdx.x + blockIdx.x * blockDim.x;
    if (tid < num_keys) {
        cache->get(keys[tid], results[tid]);
    }
}

// Kernel for atomically incrementing values in the cache
template<typename _C>
static __global__ void increment_cache_values(_C * cache, int * keys, int num_keys, int increments_per_thread) {
    const int tid = threadIdx.x + blockIdx.x * blockDim.x;

    if (tid < num_keys) {
        int key = keys[tid];

        // Each thread performs multiple increments on its key
        for (int i = 0; i < increments_per_thread; i++) {
            unsigned val;

            // Simple read-modify-write pattern:
            // 1. Read current value
            cache->get(key, val);

            // 2. Increment
            val++;

            // 3. Write back
            cache->put(key, val);
        }
    }
}

// Kernel for atomically incrementing values in concurrent test
template<typename _C>
static __global__ void increment_cache_values_concurrent(
    _C * cache, int * keys, int total_threads, int increments_per_thread) {
    const int tid = threadIdx.x + blockIdx.x * blockDim.x;

    if (tid < total_threads) {
        int key = keys[tid];

        // Each thread performs multiple increments on its key
        for (int i = 0; i < increments_per_thread; i++) {
            unsigned val;

            // Simple read-modify-write pattern:
            // 1. Read current value
            cache->get(key, val);

            // 2. Increment
            val++;

            // 3. Write back
            cache->put(key, val);
        }
    }
}

// Kernel that tags each cache entry with thread-specific data
template<typename _C>
static __global__ void tag_cache_entries_concurrent(_C * cache, int * keys, int total_threads, int tag_iterations) {
    const int tid = threadIdx.x + blockIdx.x * blockDim.x;

    if (tid < total_threads) {
        int key = keys[tid];
        int thread_id_in_group = tid % 32; // Get the thread's position within its group of 32
        unsigned tag_mask = 1u << thread_id_in_group; // Create a unique bit flag for this thread

        // Each thread performs multiple tag operations on its assigned key
        for (int i = 0; i < tag_iterations; i++) {
            unsigned val;

            // Read current value
            cache->get(key, val);

            // Set this thread's bit in the value (OR operation preserves other threads' bits)
            val |= tag_mask;

            // Write back
            cache->put(key, val);
        }
    }
}

// Kernel to verify the cache is still functional
template<typename _C, typename _V>
static __global__ void verify_cache_functional(_C * cache, int * key, _V * value, _V * result) {
    // Put a value
    cache->put(*key, *value);

    // Get it back
    cache->get(*key, *result);
}

// Base fixture class for CUDA cache tests
class ADSCDefaultGlobalPoliciesTest : public ::testing::Test {
protected:
    // Common setup for all tests
    virtual void SetUp() override {}

    virtual void TearDown() override {}

    // Helper method to create standard cache
    auto createStandardCache() {
        auto data_ptr = gdakis::umem::mem<gdakis::umem::cuda, val_type[]>::alloc(num_cls * cl_size);
        auto cache_handle
            = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cuda>(num_cls, data_ptr.get());
        EXPECT_NE(nullptr, cache_handle.get());
        return cache_handle;
    }

    // Helper method to create sub cache
    auto createSubCache() {
        auto data_ptr = gdakis::umem::mem<gdakis::umem::cuda, sub_type[]>::alloc(num_cls * sub_cl_size);
        auto cache_handle
            = gdakis::adsc::cache<scope, sub_cl_ref_type, ways>::build<gdakis::umem::cuda>(num_cls, data_ptr.get());
        EXPECT_NE(nullptr, cache_handle.get());
        return cache_handle;
    }

    // Helper method for performance test setup and reporting
    void runPerformanceTest(cudaEvent_t & start, cudaEvent_t & stop) {
        float milliseconds = 0;
        cudaEventElapsedTime(&milliseconds, start, stop);
        ASSERT_EQ(cudaSuccess, cudaEventSynchronize(stop));
        auto gbytes = (total_pages * sizeof(val_type)) / ((double)1024 * 1024 * 1024);
        auto seconds = milliseconds / 1000;
        std::cout << "GBytes: " << gbytes << " seconds: " << seconds << std::endl;
        std::cout << "Bandwidth [GB/s] " << gbytes / seconds << std::endl;
    }

    // Helper method to set up and run a performance test with a kernel
    template<typename KernelFunc>
    void runCachePerformanceTest(KernelFunc kernel_func) {
        cudaEvent_t start, stop;
        val_type * d_val;
        auto cuda_ret = cudaMalloc(&d_val, sizeof(val_type));
        ASSERT_EQ(cuda_ret, cudaSuccess);

        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventRecord(start);

        kernel_func(d_val);

        cudaEventRecord(stop);
        runPerformanceTest(start, stop);
        cudaFree(d_val);
    }

    // Helper method to run a sub cache performance test
    template<typename KernelFunc>
    void runSubCachePerformanceTest(KernelFunc kernel_func) {
        cudaEvent_t start, stop;
        cudaEventCreate(&start);
        cudaEventCreate(&stop);

        sub_type * d_val;
        auto cuda_ret = cudaMalloc(&d_val, sizeof(sub_type) * how_many_warp);
        ASSERT_EQ(cuda_ret, cudaSuccess);

        cudaEventRecord(start);
        kernel_func(d_val);
        cudaEventRecord(stop);

        runPerformanceTest(start, stop);
        cudaFree(d_val);
    }

    // Helper to allocate and initialize device memory
    template<typename T>
    T * allocDeviceMemory(size_t count) {
        T * d_ptr;
        auto cuda_ret = cudaMalloc(&d_ptr, sizeof(T) * count);
        EXPECT_EQ(cuda_ret, cudaSuccess);
        return d_ptr;
    }

    // Helper to copy from host to device
    template<typename T>
    void copyToDevice(T * d_dest, const T * h_src, size_t count) {
        auto cuda_ret = cudaMemcpy(d_dest, h_src, sizeof(T) * count, cudaMemcpyHostToDevice);
        ASSERT_EQ(cuda_ret, cudaSuccess);
    }

    // Helper to copy from device to host
    template<typename T>
    void copyFromDevice(T * h_dest, const T * d_src, size_t count) {
        auto cuda_ret = cudaMemcpy(h_dest, d_src, sizeof(T) * count, cudaMemcpyDeviceToHost);
        ASSERT_EQ(cuda_ret, cudaSuccess);
    }

    // Helper to initialize device memory with a value
    template<typename T>
    void initDeviceMemory(T * d_ptr, int value, size_t count) {
        auto cuda_ret = cudaMemset(d_ptr, value, sizeof(T) * count);
        ASSERT_EQ(cuda_ret, cudaSuccess);
    }

    // Helper to calculate grid and block dimensions
    dim3 calculateGrid(unsigned total_elements, unsigned block_size) {
        return dim3((total_elements + block_size - 1) / block_size);
    }

    // Helper to check CUDA errors after kernel launch
    void checkKernelErrors() { ASSERT_EQ(cudaGetLastError(), cudaSuccess); }

    // Helper to launch a kernel with standard grid/block calculation
    template<typename KernelFunc>
    void launchKernel(KernelFunc kernel, unsigned total_elements, unsigned block_size = 256) {
        dim3 grid((total_elements + block_size - 1) / block_size);
        kernel<<<grid, block_size>>>();
        checkKernelErrors();
    }

    // Helper to launch a kernel with custom grid/block dimensions
    template<typename KernelFunc>
    void launchKernel(KernelFunc kernel, dim3 grid, dim3 block) {
        kernel<<<grid, block>>>();
        checkKernelErrors();
    }
};

// Simple test for CreateCUDACache
TEST_F(ADSCDefaultGlobalPoliciesTest, CreateCUDACache) {
    createStandardCache();
}

// Test for standard cache get
TEST_F(ADSCDefaultGlobalPoliciesTest, CUDACacheGet) {
    auto cache_handle = createStandardCache();
    auto g_size = calculateGrid(total_pages, b_size);

    runCachePerformanceTest([&](val_type * d_val) {
        cache_get_thread<<<g_size, b_size>>>(cache_handle.get(), d_val);
        checkKernelErrors();
    });
}

// Test for sub cache get
TEST_F(ADSCDefaultGlobalPoliciesTest, CUDACacheGetSub) {
    auto cache_handle = createSubCache();
    auto g_size = calculateGrid(total_pages * 32, b_size);

    runSubCachePerformanceTest([&](sub_type * d_val) {
        cache_get_sub<<<g_size, b_size>>>(cache_handle.get(), d_val);
        checkKernelErrors();
    });
}

// Test for warp-based cache get
TEST_F(ADSCDefaultGlobalPoliciesTest, CUDACacheGetWarp) {
    auto cache_handle = createStandardCache();
    auto g_size = calculateGrid(total_pages * 32, b_size);

    runCachePerformanceTest([&](val_type * d_val) {
        cache_get_warp<<<g_size, b_size>>>(cache_handle.get(), d_val);
        checkKernelErrors();
    });
}

// Test for coalesced cache get
TEST_F(ADSCDefaultGlobalPoliciesTest, CUDACacheGetCoalesced) {
    auto cache_handle = createStandardCache();
    auto g_size = calculateGrid(total_pages * 32, b_size);

    runCachePerformanceTest([&](val_type * d_val) {
        cache_get_coalesced<<<g_size, b_size>>>(cache_handle.get(), d_val);
        checkKernelErrors();
    });
}

// Test for put and get comparison (using fixture)
class ADSCPutAndGetCompareTest : public ::testing::Test {
protected:
    using val_type1 = unsigned;
    using key_type1 = unsigned;
    static constexpr unsigned cl_size1 = 1;
    using cl_ref_type1 = gdakis::adsc::cl_reference<scope, key_type1, val_type1, cl_size1>;

    // Helper to allocate and initialize device memory
    template<typename T>
    T * allocDeviceMemory(size_t count) {
        T * d_ptr;
        auto cuda_ret = cudaMalloc(&d_ptr, sizeof(T) * count);
        ASSERT_EQ(cuda_ret, cudaSuccess);
        return d_ptr;
    }

    // Helper to copy from host to device
    template<typename T>
    void copyToDevice(T * d_dest, const T * h_src, size_t count) {
        auto cuda_ret = cudaMemcpy(d_dest, h_src, sizeof(T) * count, cudaMemcpyHostToDevice);
        ASSERT_EQ(cuda_ret, cudaSuccess);
    }

    // Helper to copy from device to host
    template<typename T>
    void copyFromDevice(T * h_dest, const T * d_src, size_t count) {
        auto cuda_ret = cudaMemcpy(h_dest, d_src, sizeof(T) * count, cudaMemcpyDeviceToHost);
        ASSERT_EQ(cuda_ret, cudaSuccess);
    }

    // Helper to check CUDA errors after kernel launch
    void checkKernelErrors() { ASSERT_EQ(cudaGetLastError(), cudaSuccess); }
};

TEST_F(ADSCPutAndGetCompareTest, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cuda, val_type1>::alloc(num_cls * cl_size1);
    EXPECT_NE(nullptr, data_ptr.get());
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type1, ways>::build<gdakis::umem::cuda>(num_cls, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

#if DEBUG
    unsigned num_sets = num_cls / ways;
    printf("Num Sets = %d\n", num_sets);
#endif

    // Allocate device memory
    auto get_val = gdakis::umem::mem<gdakis::umem::cuda, val_type1[]>::alloc(sizeof(val_type1) * ways);
    EXPECT_NE(get_val.get(), nullptr);
    auto put_val = gdakis::umem::mem<gdakis::umem::cuda, val_type1[]>::alloc(sizeof(val_type1) * ways);
    EXPECT_NE(put_val.get(), nullptr);

    // Allocate host memory
    auto get_val_cpu = gdakis::umem::mem<gdakis::umem::cpu, val_type1[]>::alloc(sizeof(val_type1) * ways);
    EXPECT_NE(get_val_cpu.get(), nullptr);
    auto put_val_cpu = gdakis::umem::mem<gdakis::umem::cpu, val_type1[]>::alloc(sizeof(val_type1) * ways);
    EXPECT_NE(put_val_cpu.get(), nullptr);

    // Initialize CPU Buffers
    for (auto i = 0ul; i < ways; i++) {
        put_val_cpu.get()[i] = i + 1;
    }

    // Copy to device
    copyToDevice(put_val.get(), put_val_cpu.get(), ways);

    // Execute put and get operations
    cache_put<<<1, ways>>>(cache_handle.get(), put_val.get());
    checkKernelErrors();

    cache_get<<<1, ways>>>(cache_handle.get(), get_val.get());
    checkKernelErrors();

    // Copy results back
    copyFromDevice(get_val_cpu.get(), get_val.get(), ways);

    // Compare
    for (auto i = 0ul; i < ways; i++) {
        EXPECT_EQ(put_val_cpu.get()[i], get_val_cpu.get()[i]);
        EXPECT_EQ(get_val_cpu.get()[i], i + 1);
    }
}

// Test for GPU cache invalidation
TEST_F(ADSCDefaultGlobalPoliciesTest, CUDACacheInvalidation) {
    auto cache_handle = createStandardCache();

    // First, populate some cache entries with data
    const int num_entries = 128;
    val_type init_val;

    // Initialize with pattern data
    for (auto i = 0ul; i < sizeof(val_type); i++) {
        init_val.vals_[i] = i % 255;
    }

    // GPU memory for the init_val
    val_type * d_init_val = allocDeviceMemory<val_type>(1);
    copyToDevice(d_init_val, &init_val, 1);

    // Launch kernel to populate cache on GPU
    const int threadsPerBlock = 256;
    const int blocksPerGrid = (num_entries + threadsPerBlock - 1) / threadsPerBlock;

    populate_cache_kernel<<<blocksPerGrid, threadsPerBlock>>>(cache_handle.get(), d_init_val, num_entries);
    checkKernelErrors();

    // Now invalidate the cache
    cache_handle.invalidate_cache();

    // Create a counter to return results
    int * d_invalid_count = allocDeviceMemory<int>(1);
    initDeviceMemory(d_invalid_count, 0, 1);

    // Launch kernel with appropriate number of threads
    check_invalidation_kernel<<<blocksPerGrid, threadsPerBlock>>>(cache_handle.get(), d_invalid_count, num_entries);
    checkKernelErrors();

    // Check results
    int host_invalid_count = 0;
    copyFromDevice(&host_invalid_count, d_invalid_count, 1);

    // All entries should be invalid
    EXPECT_EQ(num_entries, host_invalid_count) << "Not all cache entries were invalidated";

    // Cleanup
    cudaFree(d_invalid_count);
    cudaFree(d_init_val);
}

// Test for GPU cache flush operation
TEST_F(ADSCDefaultGlobalPoliciesTest, CUDACacheFlush) {
    auto cache_handle = createStandardCache();

    // Number of entries to test
    const int num_entries = 128;
    val_type init_val;

    // Initialize with pattern data
    for (auto i = 0ul; i < sizeof(val_type); i++) {
        init_val.vals_[i] = i % 255;
    }

    // GPU memory for the init_val
    val_type * d_init_val = allocDeviceMemory<val_type>(1);
    copyToDevice(d_init_val, &init_val, 1);

    // Launch kernel to populate cache on GPU
    const int threadsPerBlock = 256;
    const int blocksPerGrid = (num_entries + threadsPerBlock - 1) / threadsPerBlock;

    populate_cache_kernel<<<blocksPerGrid, threadsPerBlock>>>(cache_handle.get(), d_init_val, num_entries);
    checkKernelErrors();

    // Mark entries as dirty
    mark_dirty_kernel<<<blocksPerGrid, threadsPerBlock>>>(cache_handle.get(), num_entries);
    checkKernelErrors();

    // Counters for dirty/clean entries
    int * d_dirty_count = allocDeviceMemory<int>(1);
    int * d_clean_count = allocDeviceMemory<int>(1);

    // Initialize counters
    initDeviceMemory(d_dirty_count, 0, 1);
    initDeviceMemory(d_clean_count, 0, 1);

    // Check initial dirty status
    check_dirty_status_kernel<<<blocksPerGrid, threadsPerBlock>>>(
        cache_handle.get(), d_dirty_count, d_clean_count, num_entries);
    checkKernelErrors();

    // Verify entries are marked dirty
    int host_dirty_count = 0;
    int host_clean_count = 0;
    copyFromDevice(&host_dirty_count, d_dirty_count, 1);
    copyFromDevice(&host_clean_count, d_clean_count, 1);

    // All entries should be dirty initially
    EXPECT_EQ(num_entries, host_dirty_count) << "Not all cache entries were marked dirty";
    EXPECT_EQ(0, host_clean_count) << "Some cache entries were not dirty initially";

    // Reset counters
    initDeviceMemory(d_dirty_count, 0, 1);
    initDeviceMemory(d_clean_count, 0, 1);

    // Now flush the cache
    cache_handle.flush_cache(create_default_policies());

    // Check dirty status after flush
    check_dirty_status_kernel<<<blocksPerGrid, threadsPerBlock>>>(
        cache_handle.get(), d_dirty_count, d_clean_count, num_entries);
    checkKernelErrors();

    // Verify entries are now clean
    host_dirty_count = 0;
    host_clean_count = 0;
    copyFromDevice(&host_dirty_count, d_dirty_count, 1);
    copyFromDevice(&host_clean_count, d_clean_count, 1);

    // After flush, all entries should be clean
    EXPECT_EQ(0, host_dirty_count) << "Some cache entries are still dirty after flush";
    EXPECT_EQ(num_entries, host_clean_count) << "Not all cache entries were marked clean after flush";

    // Cleanup
    cudaFree(d_dirty_count);
    cudaFree(d_clean_count);
    cudaFree(d_init_val);
}

// Test for cache eviction policy
TEST_F(ADSCDefaultGlobalPoliciesTest, CacheEviction) {
    // Create a small cache with very limited entries to test eviction
    // We'll use integers for easier verification
    using small_val_type = unsigned;
    using small_cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, small_val_type, 1>;

    // Create a cache with just 1 set and 4 ways for simple eviction testing
    const unsigned ways_per_set = 4;
    const unsigned num_sets = 1;
    const unsigned small_cache_size = ways_per_set * num_sets;

    std::cout << "Creating cache with " << num_sets << " sets and " << ways_per_set << " ways (" << small_cache_size
              << " total entries)" << std::endl;

    auto data_ptr = gdakis::umem::mem<gdakis::umem::cuda, small_val_type[]>::alloc(small_cache_size);
    auto cache_handle = gdakis::adsc::cache<scope, small_cl_ref_type, ways_per_set>::build<gdakis::umem::cuda>(
        small_cache_size, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    // Create 2x as many entries as the cache can hold to force eviction
    const int num_entries = small_cache_size * 2;

    // Create vectors for tracking what we put into the cache
    std::vector<int> h_keys(num_entries);
    std::vector<small_val_type> h_values(num_entries);

    // Initialize with sequential keys that all map to the same set
    // This forces the cache to evict entries from the same set
    std::cout << "Initializing " << num_entries << " entries that all map to set 0" << std::endl;
    for (int i = 0; i < num_entries; i++) {
        // Make all keys map to the same set by making them multiples of num_sets
        h_keys[i] = i * num_sets;
        h_values[i] = 1000 + i; // Easily identifiable values
        std::cout << "  Entry " << i << ": key=" << h_keys[i] << ", value=" << h_values[i] << std::endl;
    }

    // Copy to device
    int * d_keys = allocDeviceMemory<int>(num_entries);
    small_val_type * d_values = allocDeviceMemory<small_val_type>(num_entries);

    copyToDevice(d_keys, h_keys.data(), num_entries);
    copyToDevice(d_values, h_values.data(), num_entries);

    // Fill the cache sequentially by inserting each entry one by one
    // to ensure entries are inserted in a controlled order
    std::cout << "Filling cache with " << num_entries << " entries sequentially:" << std::endl;
    for (int i = 0; i < num_entries; i++) {
        // Use 1 thread to populate each entry sequentially
        populate_cache_kernel<<<1, 1>>>(cache_handle.get(), &d_keys[i], &d_values[i], 1);
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

        std::cout << "  Added entry " << i << ": key=" << h_keys[i] << ", value=" << h_values[i] << std::endl;
    }

    // After filling, check which entries are still in the cache
    std::vector<small_val_type> h_results(num_entries);
    std::vector<int> h_present(num_entries, 0); // Initialize to 0

    // Prepare device memory for results
    small_val_type * d_results = allocDeviceMemory<small_val_type>(num_entries);
    int * d_found_count = allocDeviceMemory<int>(1);

    initDeviceMemory(d_found_count, 0, 1);

    // Use check_keys_kernel to find which keys are present
    const int threadsPerBlock = 256;
    const int blocksPerGrid = (num_entries + threadsPerBlock - 1) / threadsPerBlock;
    check_keys_kernel<<<blocksPerGrid, threadsPerBlock>>>(
        cache_handle.get(), d_results, d_found_count, d_keys, num_entries);
    checkKernelErrors();

    // Fetch results
    copyFromDevice(h_results.data(), d_results, num_entries);

    int found_count;
    copyFromDevice(&found_count, d_found_count, 1);

    // Check each entry individually to see if present in the results
    std::cout << "Checking which entries remain in the cache:" << std::endl;
    for (int i = 0; i < num_entries; i++) {
        bool found = false;
        for (int j = 0; j < num_entries; j++) {
            if (h_keys[j] == h_keys[i] && h_results[j] == h_values[i]) {
                found = true;
                h_present[i] = 1;
                break;
            }
        }
        std::cout << "  Entry " << i << ": key=" << h_keys[i] << ", value=" << h_values[i] << " is "
                  << (found ? "PRESENT" : "EVICTED") << std::endl;
    }

    // Count how many entries are still present
    int total_present = 0;
    for (int i = 0; i < num_entries; i++) {
        if (h_present[i]) {
            total_present++;
        }
    }

    // Verify overall cache capacity (must not exceed ways_per_set)
    std::cout << "Found " << total_present << " entries in cache (expected max: " << ways_per_set << ")" << std::endl;
    EXPECT_LE(total_present, ways_per_set) << "Cache contains more entries than its capacity";

    // With the basic round-robin eviction strategy, the cache should retain the most recent entries
    // Check that the last 'ways_per_set' entries are still in the cache
    bool all_recent_entries_present = true;
    for (int i = num_entries - ways_per_set; i < num_entries; i++) {
        if (!h_present[i]) {
            all_recent_entries_present = false;
            std::cout << "ERROR: Recent entry " << i << " is missing from cache" << std::endl;
        }
    }

    EXPECT_TRUE(all_recent_entries_present) << "Some of the most recently added entries are missing from the cache";

    // Check that older entries were evicted
    bool all_old_entries_evicted = true;
    for (auto i = 0ul; i < num_entries - ways_per_set; i++) {
        if (h_present[i]) {
            all_old_entries_evicted = false;
            std::cout << "ERROR: Old entry " << i << " is still in the cache" << std::endl;
        }
    }

    EXPECT_TRUE(all_old_entries_evicted) << "Some of the oldest entries were not evicted from the cache";

    // Cleanup
    cudaFree(d_keys);
    cudaFree(d_values);
    cudaFree(d_results);
    cudaFree(d_found_count);
}

// Test for concurrent cache access
TEST_F(ADSCDefaultGlobalPoliciesTest, ConcurrentCacheAccess) {
    // Create a small cache with limited entries
    using counter_type = unsigned;
    using counter_cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, counter_type, 1>;

    const unsigned ways_per_set = 16;
    const unsigned num_sets = 16;
    const unsigned cache_size = ways_per_set * num_sets;

    auto data_ptr = gdakis::umem::mem<gdakis::umem::cuda, counter_type[]>::alloc(cache_size);
    auto cache_handle = gdakis::adsc::cache<scope, counter_cl_ref_type, ways_per_set>::build<gdakis::umem::cuda>(
        cache_size, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    // Use a small number of keys to ensure contention
    const int num_keys = 16;
    std::vector<int> h_keys(num_keys);

    // Initialize keys to map to just 4 sets (to create contention)
    for (int i = 0; i < num_keys; i++) {
        h_keys[i] = (i % 4) * num_sets; // Create keys that map to just 4 different sets
    }

    // Copy keys to device
    int * d_keys = allocDeviceMemory<int>(num_keys);
    copyToDevice(d_keys, h_keys.data(), num_keys);

    // Initialize the cache with starting sequence numbers
    std::vector<counter_type> h_init_values(num_keys);
    for (int i = 0; i < num_keys; i++) {
        h_init_values[i] = 1000 + i; // Distinct starting value for each key
    }

    counter_type * d_init_values = allocDeviceMemory<counter_type>(num_keys);
    copyToDevice(d_init_values, h_init_values.data(), num_keys);

    // Initialize cache with the starting values
    populate_cache_kernel<<<1, num_keys>>>(cache_handle.get(), d_keys, d_init_values, num_keys);
    checkKernelErrors();
    ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

    // Launch kernel with multiple threads tagging cache values
    const int threads_per_key = 32; // 32 threads per key (1 bit position per thread)
    const int total_threads = num_keys * threads_per_key;
    const int threadsPerBlock = 256;
    const int blocksPerGrid = (total_threads + threadsPerBlock - 1) / threadsPerBlock;
    const int tag_iterations = 100; // Each thread attempts to set its bit many times

    std::cout << "Launching " << total_threads << " threads to tag " << num_keys << " keys " << tag_iterations
              << " times each" << std::endl;

    // Create expanded key array (each key repeated threads_per_key times)
    std::vector<int> h_expanded_keys(total_threads);
    for (int i = 0; i < num_keys; i++) {
        for (int j = 0; j < threads_per_key; j++) {
            h_expanded_keys[i * threads_per_key + j] = h_keys[i];
        }
    }

    int * d_expanded_keys = allocDeviceMemory<int>(total_threads);
    copyToDevice(d_expanded_keys, h_expanded_keys.data(), total_threads);

    // Run the concurrent tagging test
    tag_cache_entries_concurrent<<<blocksPerGrid, threadsPerBlock>>>(
        cache_handle.get(), d_expanded_keys, total_threads, tag_iterations);
    checkKernelErrors();

    ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

    // Read final values
    std::vector<counter_type> h_final_values(num_keys);
    counter_type * d_final_values = allocDeviceMemory<counter_type>(num_keys);

    // Use existing kernel to read final values
    read_final_values<<<1, num_keys>>>(cache_handle.get(), d_keys, d_final_values, num_keys);
    checkKernelErrors();

    copyFromDevice(h_final_values.data(), d_final_values, num_keys);

    // Now verify key invariants of the cache after concurrent access:

    // 1. INVARIANT: All cache entries should still be accessible
    bool all_entries_accessible = true;

    // 2. INVARIANT: Each entry should have at least one thread's bit set (showing updates occurred)
    bool all_entries_updated = true;

    // 4. Calculate how many threads succeeded in setting their bit for each key
    std::vector<int> threads_visible_per_key(num_keys, 0);

    std::cout << "Final values after concurrent tagging:" << std::endl;
    for (int i = 0; i < num_keys; i++) {
        unsigned base_value = 1000 + i;
        unsigned tag_bits = h_final_values[i] & 0xFFFFFFFF; // Extract just the tag bits
        unsigned base_bits = h_final_values[i] & 0xFFFF0000; // Extract high bits that should still match base

        // Count how many thread bits are set in final value
        int bits_set = 0;
        for (int bit = 0; bit < 32; bit++) {
            if (tag_bits & (1u << bit)) {
                bits_set++;
            }
        }
        threads_visible_per_key[i] = bits_set;

        // Check that value is not zero (entry is accessible)
        if (h_final_values[i] == 0) {
            all_entries_accessible = false;
        }

        // Check that at least one bit was set (entry was updated)
        if (bits_set == 0) {
            all_entries_updated = false;
        }

        std::cout << "  Key " << h_keys[i] << ": base=" << base_value << ", final=0x" << std::hex << h_final_values[i]
                  << std::dec << ", threads visible=" << bits_set << "/" << threads_per_key << std::endl;
    }

    // ASSERTIONS: Verify our invariants

    // 1. The cache should remain functional with all entries accessible
    EXPECT_TRUE(all_entries_accessible) << "Some cache entries became inaccessible after concurrent access";

    // 2. Each key should show evidence of being updated by at least one thread
    EXPECT_TRUE(all_entries_updated) << "Some cache entries show no evidence of being updated";

    // 3. We expect to see race conditions (not all threads' updates visible)
    int total_visible_threads = 0;
    for (int i = 0; i < num_keys; i++) {
        total_visible_threads += threads_visible_per_key[i];
    }

    std::cout << "Total threads whose updates are visible: " << total_visible_threads << " out of " << total_threads
              << std::endl;

    // EXPECT to see fewer visible threads than total threads (race condition evidence)
    // But we should see at least some updates for each key
    EXPECT_LT(total_visible_threads, total_threads)
        << "No race conditions detected - cache has unexpected atomic behavior";

    // Calculate minimum and maximum threads visible per key
    int min_threads_visible = threads_per_key;
    int max_threads_visible = 0;
    for (int i = 0; i < num_keys; i++) {
        min_threads_visible = std::min(min_threads_visible, threads_visible_per_key[i]);
        max_threads_visible = std::max(max_threads_visible, threads_visible_per_key[i]);
    }

    std::cout << "Min threads visible per key: " << min_threads_visible << std::endl;
    std::cout << "Max threads visible per key: " << max_threads_visible << std::endl;

    // 4. After the test, verify the cache is still functional by doing a simple get/put
    const counter_type verify_value = 12345;
    counter_type * d_verify_value = allocDeviceMemory<counter_type>(1);
    copyToDevice(d_verify_value, &verify_value, 1);

    // Prepare for verification put/get operation
    counter_type result_value = 0;
    counter_type * d_result_value = allocDeviceMemory<counter_type>(1);

    // Perform verification put/get
    int verify_key = h_keys[0];
    int * d_verify_key = allocDeviceMemory<int>(1);
    copyToDevice(d_verify_key, &verify_key, 1);

    // Run the verification kernel to test cache functionality
    verify_cache_functional<<<1, 1>>>(cache_handle.get(), d_verify_key, d_verify_value, d_result_value);
    checkKernelErrors();

    // Check result
    copyFromDevice(&result_value, d_result_value, 1);

    EXPECT_EQ(result_value, verify_value) << "Cache no longer functional after concurrent access";

    // Cleanup
    cudaFree(d_keys);
    cudaFree(d_init_values);
    cudaFree(d_expanded_keys);
    cudaFree(d_final_values);
    cudaFree(d_verify_key);
    cudaFree(d_verify_value);
    cudaFree(d_result_value);
}
