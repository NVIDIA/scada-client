/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "profiler/metrics/cycles.hpp"
#include "profiler/profiled_autobatch"
#include <algorithm>
#include <array> // For std::array
#include <chrono> // For std::chrono clocks
#include <cmath> // For std::sqrt
#include <cstdint> // For std::uint64_t
#include <cstdlib>
#include <gdakis/atomic>
#include <gdakis/autobatch>
#include <iostream>
#include <memory> // For std::unique_ptr
#include <mutex>
#include <numeric> // For std::accumulate
#include <string> // For std::string
#include <vector> // For std::vector

// CUDA includes
#include <cooperative_groups.h>
#include <cuda_runtime.h>

// Result collection for summary output
namespace {
struct BenchmarkResult {
    std::size_t n_threads;
    std::size_t n_requests;
    std::size_t n_buffers;
    unsigned block_size;
    unsigned n_pushes;
    bool use_cg;
    double median_pushes_per_sec;
    double min_cycles;
    double max_cycles;
    double mean_cycles;
    double median_cycles;
    double stddev_cycles;
    double system_time_sec;
};

static std::vector<BenchmarkResult> g_results;
static std::mutex g_results_mtx;

inline void add_result(const BenchmarkResult & r) {
    std::lock_guard<std::mutex> lk(g_results_mtx);
    g_results.emplace_back(r);
}
}

template<std::size_t _N>
struct DummyBuffer {
    std::array<size_t, _N> values_;

    template<typename... Args>
    __device__ __host__ DummyBuffer(Args &&... args) {}
    __device__ __host__ void reset() {
        for (size_t i = 0; i < _N; i++) {
            values_[i] = 0;
        }
    }
    __device__ __host__ void set(size_t i, size_t value) { values_[i] = value; }
    __device__ __host__ void commit(size_t count) {
        //noop
    }
    __device__ __host__ void acquire() {
        //noop
    }

    __device__ __host__ constexpr auto size() const { return _N; }
};

// Templated kernel to test the profiled autobatch
template<typename Autobatch, typename _PushPayload, bool UseCg>
__global__ void test_kernel(Autobatch * autobatch, int n_threads, _PushPayload payload) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= n_threads)
        return;

    // Use the profiled autobatch
    if constexpr (UseCg) {
        auto block = cooperative_groups::this_thread_block();
        auto warp = cooperative_groups::tiled_partition<32>(block);
        autobatch->push(warp, id * 2 + payload);
    } else {
        autobatch->push(id * 2 + payload);
    }
}

// Templated kernel to test the profiled autobatch
// Push N elements in a loop
template<unsigned N, bool UseCg, typename Autobatch, typename _PushPayload>
__global__ void test_kernel_N(Autobatch * autobatch, int n_threads, _PushPayload payload) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= n_threads)
        return;

    // Use the profiled autobatch
    if constexpr (UseCg) {
        auto block = cooperative_groups::this_thread_block();
        auto warp = cooperative_groups::tiled_partition<32>(block);
        autobatch->push_N<N>(warp, id * 2 + payload);
    } else {
        autobatch->push_N<N>(id * 2 + payload);
    }
}

template<class T>
__attribute__((noinline)) auto get_clock(T & value) {
    // Make the compiler think we actually use / modify the value.
    // It can't "see" what is going on inside the assembly string.
    __asm__ __volatile__("" : "+g"(value));
    return std::chrono::high_resolution_clock::now();
}
// Template function to run benchmark for different autobatch versions
template<typename AutobatchType, size_t N_THREADS, size_t N_REQUESTS, unsigned BLOCK_SIZE, unsigned N_PUSHES,
    bool UseCg = false>
void run_benchmark_once(const std::string & version_name, double & out_pushes_per_sec, double & out_min_cycles,
    double & out_max_cycles, double & out_mean_cycles, double & out_median_cycles, double & out_stddev_cycles,
    double & out_system_time_sec) {
    std::cout << "Testing autobatch " << version_name << " with N_THREADS=" << N_THREADS
              << ", N_REQUESTS=" << N_REQUESTS << ", N_BUFFERS=" << AutobatchType::number_of_buffers()
              << ", BLOCK_SIZE=" << BLOCK_SIZE << ", N_PUSHES(per thread)=" << N_PUSHES << ", UseCg=" << UseCg << "..."
              << std::endl;

    std::uint64_t dummything = 0;
    auto start = get_clock(dummything);

    // Allocate device counters using cudaMalloc
    clock_t * d_counters = nullptr;
    cudaMalloc(reinterpret_cast<void **>(&d_counters), N_THREADS * sizeof(clock_t));

    // Allocate host counters using std::vector
    std::vector<clock_t> h_counters(N_THREADS);

    // Create the metrics collector
    profiler::metrics::Cycles cycles_metrics;
    cycles_metrics.setDst(d_counters);

    // Set up kernel parameters
    const int block_size = BLOCK_SIZE;
    dim3 blocks((N_THREADS + block_size - 1) / block_size);
    dim3 threads(block_size);

    // Create host autobatch
    using ProfiledAutobatchType = profiler::ProfiledAutobatch<AutobatchType, profiler::metrics::Cycles>;
    auto h_autobatch = std::make_unique<ProfiledAutobatchType>(
        [](auto &&... args) { return DummyBuffer<N_REQUESTS> { std::forward<decltype(args)>(args)... }; });
    h_autobatch->setProfiler(cycles_metrics);

    // Allocate the autobatch on the device using cudaMalloc
    ProfiledAutobatchType * d_autobatch = nullptr;
    cudaMalloc(&d_autobatch, sizeof(ProfiledAutobatchType));

    // Copy the host autobatch to device
    cudaMemcpy(d_autobatch, h_autobatch.get(), sizeof(ProfiledAutobatchType), cudaMemcpyHostToDevice);

    //create a cuda event
    cudaEvent_t start_event, stop_event;
    cudaEventCreate(&start_event);
    cudaEventCreate(&stop_event);
    cudaEventRecord(start_event);

    // Launch the kernel
    if constexpr (N_PUSHES == 1) {
        test_kernel<ProfiledAutobatchType, size_t, UseCg><<<blocks, threads>>>(d_autobatch, N_THREADS, size_t(1));
    } else {
        test_kernel_N<N_PUSHES, UseCg, ProfiledAutobatchType, size_t>
            <<<blocks, threads>>>(d_autobatch, N_THREADS, size_t(1));
    }
    cudaEventRecord(stop_event);
    cudaEventSynchronize(stop_event);
    if (cudaGetLastError() != cudaSuccess) {
        std::cerr << "Kernel launch failed" << std::endl;
        return;
    }
    float milliseconds = 0;
    cudaEventElapsedTime(&milliseconds, start_event, stop_event);
    //total pushes per second
    double total_pushes_per_second = 1000.0 * N_THREADS * N_PUSHES / milliseconds;
    std::cout << "Total pushes per second: " << total_pushes_per_second << std::endl;

    // Copy the profiling results to host
    cudaMemcpy(h_counters.data(), d_counters, N_THREADS * sizeof(clock_t), cudaMemcpyDeviceToHost);

    // Compute min, max, mean, median, stddev of the profiling results
    std::sort(h_counters.begin(), h_counters.end());
    auto min = h_counters[0];
    auto max = h_counters[N_THREADS - 1];
    auto mean = std::accumulate(h_counters.begin(), h_counters.end(), 0.0) / N_THREADS;
    auto median = h_counters[N_THREADS / 2];

    double variance = 0.0;
    for (size_t i = 0; i < N_THREADS; i++) {
        variance += (h_counters[i] - mean) * (h_counters[i] - mean);
    }
    variance /= N_THREADS;
    auto stddev = std::sqrt(variance);

    //cycles per thread
    std::cout << "Min: " << min << ", Max: " << max << ", Mean: " << mean << ", Median: " << median
              << ", Stddev: " << stddev << std::endl;

    // Free device memory
    cudaFree(d_counters);
    cudaFree(d_autobatch);
    cudaEventDestroy(start_event);
    cudaEventDestroy(stop_event);

    auto end = get_clock(dummything);
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "System time taken: " << duration.count() / 1e6f << " seconds" << std::endl;

    // Return aggregates for this single run
    out_pushes_per_sec = total_pushes_per_second;
    out_min_cycles = static_cast<double>(min);
    out_max_cycles = static_cast<double>(max);
    out_mean_cycles = static_cast<double>(mean);
    out_median_cycles = static_cast<double>(median);
    out_stddev_cycles = static_cast<double>(stddev);
    out_system_time_sec = duration.count() / 1e6;
}

// Run benchmark for v3 autobatch with specific configuration
template<size_t N_THREADS, size_t N_REQUESTS, size_t N_BUFFERS, unsigned BLOCK_SIZE, unsigned N_PUSHES, bool UseCg>
void run_v3_benchmark() {
    using autobatch_type = gdakis::comm::autobatch<DummyBuffer<N_REQUESTS>, gdakis::thread_scope_device, N_BUFFERS>;
    constexpr int kRepeats = 5;
    std::array<double, kRepeats> pushes {};
    double min_c = 0.0, max_c = 0.0, mean_c = 0.0, med_c = 0.0, std_c = 0.0, sys_t = 0.0;
    for (int i = 0; i < kRepeats; ++i) {
        double pps = 0.0, mi = 0.0, ma = 0.0, me = 0.0, md = 0.0, sd = 0.0, st = 0.0;
        run_benchmark_once<autobatch_type, N_THREADS, N_REQUESTS, BLOCK_SIZE, N_PUSHES, UseCg>(
            "v3", pps, mi, ma, me, md, sd, st);
        pushes[i] = pps;
        // keep last iteration stats for context (not used for summary math)
        min_c = mi;
        max_c = ma;
        mean_c = me;
        med_c = md;
        std_c = sd;
        sys_t = st;
    }
    std::sort(pushes.begin(), pushes.end());
    double median_pps = pushes[kRepeats / 2];

    BenchmarkResult rec;
    rec.n_threads = N_THREADS;
    rec.n_requests = N_REQUESTS;
    rec.n_buffers = N_BUFFERS;
    rec.block_size = BLOCK_SIZE;
    rec.n_pushes = N_PUSHES;
    rec.use_cg = UseCg;
    rec.median_pushes_per_sec = median_pps;
    rec.min_cycles = min_c;
    rec.max_cycles = max_c;
    rec.mean_cycles = mean_c;
    rec.median_cycles = med_c;
    rec.stddev_cycles = std_c;
    rec.system_time_sec = sys_t;
    add_result(rec);
}

// Template utilities for compile-time parameter sweeping
template<size_t... Ts>
struct ThreadList {};
template<size_t... Rs>
struct RequestList {};
template<size_t... Bs>
struct BufferList {};
template<unsigned... Ps>
struct PushList {};
template<unsigned... BSs>
struct BlockSizeList {};
template<bool... CGs>
struct CGList {};

// (removed duplicate summary block)

// Main sweep template that handles all combinations recursively
template<typename TList, typename RList, typename BList, typename PList, typename BSList, typename CGList>
struct ParameterSweep;

// Specialization for ThreadList
template<size_t T, size_t... Ts, typename RList, typename BList, typename PList, typename BSList, typename CGList>
struct ParameterSweep<ThreadList<T, Ts...>, RList, BList, PList, BSList, CGList> {
    static void run() {
        // Process current thread value with all other parameters
        ParameterSweep<ThreadList<T>, RList, BList, PList, BSList, CGList>::run();
        // Continue with remaining thread values
        ParameterSweep<ThreadList<Ts...>, RList, BList, PList, BSList, CGList>::run();
    }
};

// Specialization for RequestList
template<size_t T, size_t R, size_t... Rs, typename BList, typename PList, typename BSList, typename CGList>
struct ParameterSweep<ThreadList<T>, RequestList<R, Rs...>, BList, PList, BSList, CGList> {
    static void run() {
        // Process current request value with all other parameters
        ParameterSweep<ThreadList<T>, RequestList<R>, BList, PList, BSList, CGList>::run();
        // Continue with remaining request values
        ParameterSweep<ThreadList<T>, RequestList<Rs...>, BList, PList, BSList, CGList>::run();
    }
};

// Specialization for BufferList
template<size_t T, size_t R, size_t B, size_t... Bs, typename PList, typename BSList, typename CGList>
struct ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B, Bs...>, PList, BSList, CGList> {
    static void run() {
        // Process current buffer value with all other parameters
        ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PList, BSList, CGList>::run();
        // Continue with remaining buffer values
        ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<Bs...>, PList, BSList, CGList>::run();
    }
};

// Specialization for PushList
template<size_t T, size_t R, size_t B, unsigned P, unsigned... Ps, typename BSList, typename CGList>
struct ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P, Ps...>, BSList, CGList> {
    static void run() {
        // Process current push value with all other parameters
        ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BSList, CGList>::run();
        // Continue with remaining push values
        ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<Ps...>, BSList, CGList>::run();
    }
};

// Specialization for BlockSizeList
template<size_t T, size_t R, size_t B, unsigned P, unsigned BS, unsigned... BSs, typename CGList>
struct ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BlockSizeList<BS, BSs...>, CGList> {
    static void run() {
        // Process current block size value with all other parameters
        ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BlockSizeList<BS>, CGList>::run();
        // Continue with remaining block size values
        ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BlockSizeList<BSs...>, CGList>::run();
    }
};

// Specialization for CGList
template<size_t T, size_t R, size_t B, unsigned P, unsigned BS, bool CG, bool... CGs>
struct ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BlockSizeList<BS>,
    CGList<CG, CGs...>> {
    static void run() {
        // Process current CG value
        ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BlockSizeList<BS>, CGList<CG>>::run();
        // Continue with remaining CG values
        ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BlockSizeList<BS>,
            CGList<CGs...>>::run();
    }
};

// Base case: single set of parameters - actual benchmark execution
template<size_t T, size_t R, size_t B, unsigned P, unsigned BS, bool CG>
struct ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BlockSizeList<BS>, CGList<CG>> {
    static void run() { run_v3_benchmark<T, R, B, BS, P, CG>(); }
};

// Base cases for empty parameter lists
template<typename RList, typename BList, typename PList, typename BSList, typename CGList>
struct ParameterSweep<ThreadList<>, RList, BList, PList, BSList, CGList> {
    static void run() {} // End of recursion
};

template<size_t T, typename BList, typename PList, typename BSList, typename CGList>
struct ParameterSweep<ThreadList<T>, RequestList<>, BList, PList, BSList, CGList> {
    static void run() {} // End of recursion
};

template<size_t T, size_t R, typename PList, typename BSList, typename CGList>
struct ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<>, PList, BSList, CGList> {
    static void run() {} // End of recursion
};

template<size_t T, size_t R, size_t B, typename BSList, typename CGList>
struct ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<>, BSList, CGList> {
    static void run() {} // End of recursion
};

template<size_t T, size_t R, size_t B, unsigned P, typename CGList>
struct ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BlockSizeList<>, CGList> {
    static void run() {} // End of recursion
};

template<size_t T, size_t R, size_t B, unsigned P, unsigned BS>
struct ParameterSweep<ThreadList<T>, RequestList<R>, BufferList<B>, PushList<P>, BlockSizeList<BS>, CGList<>> {
    static void run() {} // End of recursion
};

int main(int argc, char ** argv) {
    cudaError_t cuda_ret;
    cudaDeviceProp cuda_prop;

    cuda_ret = cudaGetDeviceProperties(&cuda_prop, 0);
    if (cuda_ret != cudaSuccess) {
        std::cerr << "ERROR: " << cudaGetErrorString(cuda_ret) << std::endl;
        return -1;
    }

    std::cout << "Running benchmark on " << cuda_prop.name << std::endl;

    // Mode selection: default to short
    bool large_mode = false;
    bool no_summary = false;
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "large" || arg1 == "--large" || arg1 == "-l") {
            large_mode = true;
        } else if (arg1 == "--no-summary") {
            no_summary = true;
        }
    }
    if (argc > 2) {
        std::string arg2 = argv[2];
        if (arg2 == "--no-summary")
            no_summary = true;
        if (arg2 == "large" || arg2 == "--large" || arg2 == "-l")
            large_mode = true;
    }
    std::cout << "Mode: " << (large_mode ? "large" : "short") << std::endl;

    // Define parameter sets for v3 autobatch benchmarking
    if (large_mode) {
        using Threads = ThreadList<1024, 65536, 1048576>;
        using Requests = RequestList<32, 128, 2048>;
        using Buffers = BufferList<4, 16, 32>;
        using Pushes = PushList<1, 16, 256>;
        using BlockSizes = BlockSizeList<32, 64, 128, 256>;
        using CGs = CGList<false, true>;
        // Run v3 benchmarks
        ParameterSweep<Threads, Requests, Buffers, Pushes, BlockSizes, CGs>::run();
    } else {
        // Short mode: fewer combinations
        using Threads = ThreadList<1024, 1048576>;
        using Requests = RequestList<32, 128>;
        using Buffers = BufferList<4, 16>;
        using Pushes = PushList<1, 16>;
        using BlockSizes = BlockSizeList<64, 128>;
        using CGs = CGList<false, true>;
        ParameterSweep<Threads, Requests, Buffers, Pushes, BlockSizes, CGs>::run();
    }

    // Summary (unless disabled)
    if (!no_summary) {
        std::cout << "===== Summary =====" << std::endl;
        std::cout << "Total runs: " << g_results.size() << std::endl;
        if (!g_results.empty()) {
            const BenchmarkResult * best = &g_results[0];
            double sum_medians = 0.0;
            for (const auto & r : g_results) {
                if (r.median_pushes_per_sec > best->median_pushes_per_sec)
                    best = &r;
                sum_medians += r.median_pushes_per_sec;
            }
            double avg_median = sum_medians / static_cast<double>(g_results.size());

            std::cout << "Best median pushes/s: " << best->median_pushes_per_sec << " [threads=" << best->n_threads
                      << ", requests=" << best->n_requests << ", buffers=" << best->n_buffers
                      << ", block=" << best->block_size << ", pushes/thread=" << best->n_pushes
                      << ", useCg=" << (best->use_cg ? 1 : 0) << "]" << std::endl;

            std::cout << "Average of medians (pushes/s): " << avg_median << std::endl;
        }
    }

    return 0;
}
