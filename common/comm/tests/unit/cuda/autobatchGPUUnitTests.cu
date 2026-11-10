/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cooperative_groups.h>
#include <cuda_runtime.h>
#include <gdakis/autobatch>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Helper macro for CUDA error checking
#define CUDA_CHECK(call, msg)                                                                                          \
    do {                                                                                                               \
        auto err = call;                                                                                               \
        ASSERT_EQ(err, cudaSuccess) << msg << ": " << cudaGetErrorString(err);                                         \
    } while (0)

// Default timeout values
namespace defaults {
constexpr size_t MAX_WAIT_MS = 50000; // 50 seconds
constexpr size_t POLL_SLEEP_MS = 100; // 100ms polling interval
}

// Global counters for diagnostics
__device__ gdakis::atomic<unsigned> g_commit_count;
__device__ gdakis::atomic<unsigned> g_push_count; // Track number of successful pushes
__device__ gdakis::atomic<unsigned> g_attempt_count; // Track number of attempts

// Helper class to monitor kernel execution with timeout
class KernelTimeoutMonitor {
public:
    KernelTimeoutMonitor() {
        cudaError_t err = cudaEventCreate(&event_);
        if (err != cudaSuccess) {
            throw std::runtime_error("Failed to create CUDA event");
        }

        // Create a separate stream for diagnostics
        err = cudaStreamCreateWithFlags(&diagnostic_stream_, cudaStreamNonBlocking);
        if (err != cudaSuccess) {
            cudaEventDestroy(event_);
            throw std::runtime_error("Failed to create diagnostic stream");
        }
        is_valid_ = true;
    }
    KernelTimeoutMonitor(const KernelTimeoutMonitor &) = delete;
    KernelTimeoutMonitor & operator=(const KernelTimeoutMonitor &) = delete;
    KernelTimeoutMonitor(KernelTimeoutMonitor && other) noexcept {
        if (this != &other) {
            is_valid_ = other.is_valid_;
            event_ = other.event_;
            diagnostic_stream_ = other.diagnostic_stream_;
            other.is_valid_ = false;
        }
    }
    KernelTimeoutMonitor & operator=(KernelTimeoutMonitor && other) noexcept {
        if (this != &other) {
            is_valid_ = other.is_valid_;
            event_ = other.event_;
            diagnostic_stream_ = other.diagnostic_stream_;
            other.is_valid_ = false;
        }
        return *this;
    }

    ~KernelTimeoutMonitor() {
        if (is_valid_) {
            cudaStreamDestroy(diagnostic_stream_);
            cudaEventDestroy(event_);
            is_valid_ = false;
        }
    }

    // Record event after kernel launch
    void recordKernelStart() {
        std::clog << "Recording kernel start" << std::endl;
        CUDA_CHECK(cudaEventRecord(event_), "Failed to record CUDA event");
    }

    // Wait for kernel completion with configurable timeout
    // Returns true if kernel completed successfully, false if timeout or error
    bool waitForCompletion(std::string & error_msg, size_t timeout_ms = defaults::MAX_WAIT_MS,
        size_t poll_interval_ms = defaults::POLL_SLEEP_MS) {
        size_t elapsed_ms = 0;
        cudaError_t event_status;
        std::clog << "Waiting for kernel completion" << std::endl;

        while ((event_status = cudaEventQuery(event_)) == cudaErrorNotReady && elapsed_ms < timeout_ms) {
            std::clog << "Kernel not ready, elapsed time: " << elapsed_ms << "ms" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
            elapsed_ms += poll_interval_ms;
        }
        std::clog << "Kernel completion status: " << event_status << std::endl;

        if (event_status != cudaSuccess || elapsed_ms >= timeout_ms) {
            std::stringstream ss;
            ss << "Kernel execution " << (event_status != cudaSuccess ? "failed" : "timed out") << " after "
               << elapsed_ms << "ms";

            if (event_status != cudaSuccess && event_status != cudaErrorNotReady) {
                ss << ": " << cudaGetErrorString(event_status);
            }

            error_msg = ss.str();
            return false;
        }

        return true;
    }

    // Get diagnostic information asynchronously using a separate stream
    template<typename AB>
    bool getDiagnosticInfo(unsigned & attempts, unsigned & pushes, void * autobatch_state, const AB * d_autobatch) {
        // Queue async memory copies in diagnostic stream
        auto copy_err1 = cudaMemcpyFromSymbolAsync(
            &attempts, g_attempt_count, sizeof(unsigned), 0, cudaMemcpyDeviceToHost, diagnostic_stream_);
        auto copy_err2 = cudaMemcpyFromSymbolAsync(
            &pushes, g_push_count, sizeof(unsigned), 0, cudaMemcpyDeviceToHost, diagnostic_stream_);

        // Copy the entire autobatch state
        auto copy_err3
            = cudaMemcpyAsync(autobatch_state, d_autobatch, sizeof(AB), cudaMemcpyDeviceToHost, diagnostic_stream_);

        // Synchronize just the diagnostic stream
        auto sync_err = cudaStreamSynchronize(diagnostic_stream_);

        return copy_err1 == cudaSuccess && copy_err2 == cudaSuccess && copy_err3 == cudaSuccess
            && sync_err == cudaSuccess;
    }

private:
    cudaEvent_t event_;
    cudaStream_t diagnostic_stream_;
    bool is_valid_ = false;
};

// Define a simple work request type for testing
template<unsigned _WrbSize>
struct WorkRequestBuffer {
    std::array<unsigned, _WrbSize> requests_ = { 0 };
    unsigned * commitDst_ = nullptr;

    __host__ __device__ explicit WorkRequestBuffer(unsigned * commitDst) : commitDst_(commitDst) {}

    // Return the size of the work request
    __host__ __device__ unsigned size() const noexcept { return _WrbSize; }

    // Add the value to the work request at the given position
    __host__ __device__ void set(unsigned position, unsigned value) noexcept {
        assert(position < _WrbSize);
        requests_[position] += value;
    }
    __host__ __device__ void acquire() noexcept {
        // Simulate waiting for the work request to complete
        for (unsigned i = 0; i < _WrbSize; ++i) {
            requests_[i] = 0;
        }
    }

    // Commit the work request (simulate processing)
    __host__ __device__ void commit(size_t count) noexcept {
        assert(commitDst_ != nullptr);
        auto offset = g_commit_count.fetch_add(count, gdakis::memory_order_relaxed);

        for (unsigned i = 0; i < count; ++i) {
            assert(requests_[i] != 0);
            commitDst_[offset + i] = requests_[i];
        }
    }
};

// Define a test fixture for autobatch
template<size_t _NumBuffers, unsigned _WrbSize>
using Autobatch = gdakis::comm::autobatch<WorkRequestBuffer<_WrbSize>, gdakis::thread_scope_device, _NumBuffers>;

// GPU kernel to test autobatch functionality
template<size_t _NumBuffers, unsigned _WrbSize, bool _UseCg = false>
__global__ void test_autobatch_kernel(Autobatch<_NumBuffers, _WrbSize> * batch, unsigned n) {
    unsigned tid = threadIdx.x + blockIdx.x * blockDim.x;
    if (tid >= n)
        return;

    g_attempt_count.fetch_add(1, gdakis::memory_order_relaxed);
    if constexpr (_UseCg) {
        auto block = cooperative_groups::this_thread_block();
        auto warp = cooperative_groups::tiled_partition<32>(block);
        batch->push(warp, tid * 2 + 1);
    } else {
        batch->push(tid * 2 + 1);
    }

    g_push_count.fetch_add(1, gdakis::memory_order_relaxed);
}

// Define a parameterized test fixture
template<unsigned _NumEntries, unsigned _NumThreads, size_t _NumBuffers, unsigned _BlockSize, bool _UseCg = false>
class AutobatchGPUTest : public ::testing::Test {
protected:
    void runTest(size_t num_iterations = 1, size_t timeout_ms = defaults::MAX_WAIT_MS,
        size_t poll_interval_ms = defaults::POLL_SLEEP_MS) {
        for (size_t iter = 0; iter < num_iterations; ++iter) {
            printf("\rRunning iteration %zu/%zu\n", iter + 1, num_iterations);

            try {
                runSingleIteration(timeout_ms, poll_interval_ms);
            } catch (const std::runtime_error & e) {
                FAIL() << "\nFailed on iteration " << iter + 1 << "/" << num_iterations << ": " << e.what();
            }

            cudaDeviceReset();
        }
        if (num_iterations > 1) {
            printf("\nAll %zu iterations completed successfully\n", num_iterations);
        }
    }

private:
    void runSingleIteration(
        size_t timeout_ms = defaults::MAX_WAIT_MS, size_t poll_interval_ms = defaults::POLL_SLEEP_MS) {
        using AB = Autobatch<_NumBuffers, _NumEntries>;
        KernelTimeoutMonitor kernel_monitor;

        // Check device capabilities first
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, 0);

        // Calculate grid dimensions
        int grid_size = (_NumThreads + _BlockSize - 1) / _BlockSize;

        // Verify launch parameters against device limits
        ASSERT_LE(_BlockSize, prop.maxThreadsPerBlock) << "Block size " << _BlockSize << " exceeds device limit of "
                                                       << prop.maxThreadsPerBlock << " threads per block";

        ASSERT_LE(grid_size, prop.maxGridSize[0]) << "Grid size " << grid_size << " exceeds device limit of "
                                                  << prop.maxGridSize[0] << " blocks in X dimension";

        // Initialize atomic counters
        unsigned zero = 0;
        CUDA_CHECK(cudaMemcpyToSymbol(g_commit_count, &zero, sizeof(unsigned)), "Failed to initialize commit count");
        CUDA_CHECK(cudaMemcpyToSymbol(g_push_count, &zero, sizeof(unsigned)), "Failed to initialize push count");
        CUDA_CHECK(cudaMemcpyToSymbol(g_attempt_count, &zero, sizeof(unsigned)), "Failed to initialize attempt count");

        // Allocate results array
        unsigned * d_results;
        CUDA_CHECK(
            cudaMalloc(&d_results, _NumThreads * sizeof(unsigned)), "Failed to allocate device memory for results");
        CUDA_CHECK(cudaMemset(d_results, 0x0, _NumThreads * sizeof(unsigned)), "Failed to initialize results array");

        // Create autobatch on the host
        AB h_autobatch([d_results](auto p) { return WorkRequestBuffer<_NumEntries> { d_results }; });

        // Allocate device memory for autobatch and copy the host autobatch to the device
        AB * d_autobatch;
        CUDA_CHECK(cudaMalloc(&d_autobatch, sizeof(AB)), "Failed to allocate device memory for autobatch");
        CUDA_CHECK(cudaMemcpy(d_autobatch, &h_autobatch, sizeof(AB), cudaMemcpyHostToDevice),
            "Failed to copy autobatch to device");

        // Launch kernel and start monitoring
        test_autobatch_kernel<_NumBuffers, _NumEntries, _UseCg><<<grid_size, _BlockSize>>>(d_autobatch, _NumThreads);
        CUDA_CHECK(cudaGetLastError(), "Kernel launch failed");

        kernel_monitor.recordKernelStart();

        // Wait for kernel completion
        std::string error_msg;
        if (!kernel_monitor.waitForCompletion(error_msg, timeout_ms, poll_interval_ms)) {
            unsigned h_attempt_count = 0, h_push_count = 0;
            alignas(AB) char state_buffer[sizeof(AB)];

            std::stringstream ss;
            ss << error_msg;

            if (kernel_monitor.getDiagnosticInfo(h_attempt_count, h_push_count, state_buffer, d_autobatch)) {
                auto state = reinterpret_cast<AB *>(state_buffer);
                ss << "\nAttempted pushes: " << h_attempt_count << "\nSuccessful pushes: " << h_push_count
                   << "\nExpected pushes: " << _NumThreads;
            } else {
                ss << "\nUnable to retrieve diagnostic information";
            }

            error_msg = ss.str();
            throw std::runtime_error(error_msg);
        }

        // Copy results back to host and verify
        std::vector<unsigned> h_results(_NumThreads);
        CUDA_CHECK(cudaMemcpy(h_results.data(), d_results, h_results.size() * sizeof(unsigned), cudaMemcpyDeviceToHost),
            "Failed to copy results from device");

        // Create array to track which thread IDs we've seen
        std::vector<bool> h_results_valid(_NumThreads, false);

        // Verify each result from the GPU
        for (unsigned i = 0; i < _NumThreads; ++i) {
            if (h_results[i] == 0)
                continue; // Skip empty slots

            auto tid = (h_results[i] - 1) / 2;

            // Verify thread ID is within valid range
            ASSERT_LT(tid, h_results_valid.size())
                << "Invalid thread ID " << tid << " at position " << i << " (max allowed: " << (_NumThreads - 1) << ")";

            // Mark this thread ID as seen
            h_results_valid[tid] = true;
        }

        // Count how many thread IDs were never seen
        auto missing_count = std::count(h_results_valid.begin(), h_results_valid.end(), false);
        EXPECT_EQ(missing_count, 0) << missing_count << " thread IDs were never seen in the results";

        // Cleanup
        cudaFree(d_autobatch);
        cudaFree(d_results);
    }
};

// Define configurations as individual types
class AutobatchGPUTest32_1M_8_256 : public AutobatchGPUTest<32, 1024 * 1024, 8, 256> {};
class AutobatchGPUTest16_512K_8_256 : public AutobatchGPUTest<16, 512 * 512, 8, 256> {};
class AutobatchGPUTest31_1K_8_256 : public AutobatchGPUTest<31, 1024, 8, 256> {};
class AutobatchGPUTest47_1K_8_256 : public AutobatchGPUTest<47, 1024, 8, 256> {};
class AutobatchGPUTest32_1K_2_256 : public AutobatchGPUTest<32, 1024, 2, 256> {};
class AutobatchGPUTest32_1K_4_256 : public AutobatchGPUTest<32, 1024, 4, 256> {};
class AutobatchGPUTest32_1K_8_256 : public AutobatchGPUTest<32, 1024, 8, 256> {};
class AutobatchGPUTest30_1K_8_256 : public AutobatchGPUTest<30, 1024, 8, 256> {};
class AutobatchGPUTest32_32_2_32 : public AutobatchGPUTest<32, 32, 2, 32> {};

class AutobatchGPUTest30_1K_2_256 : public AutobatchGPUTest<30, 1024, 2, 256> {}; //hangs
class AutobatchGPUTest32_1K_1_256 : public AutobatchGPUTest<32, 1024, 1, 256> {}; //hangs
class AutobatchGPUTest32_32_1_32 : public AutobatchGPUTest<32, 32, 1, 32> {}; //hangs
class AutobatchGPUTest64_32_1_32 : public AutobatchGPUTest<64, 32, 1, 32> {}; // doesn't validate properly

//high num buffers tests
class AutobatchGPUTest1024_1M_256_64 : public AutobatchGPUTest<1024, 1024 * 1024, 256, 64> {};
class AutobatchGPUTest1024_1M_256_256 : public AutobatchGPUTest<1024, 1024 * 1024, 256, 256> {};
class AutobatchGPUTest16_1M_256_256 : public AutobatchGPUTest<16, 1024 * 1024, 256, 256> {};

//create some tests testing the use of cooperative groups in push operations
class AutobatchGPUTest32_1K_8_256_cg : public AutobatchGPUTest<32, 1024, 8, 256, true> {};
class AutobatchGPUTest32K_1M_8_128_cg : public AutobatchGPUTest<32 * 1024, 1024 * 1024, 8, 128, true> {};

// Enhanced device capability check with detailed feedback
struct DeviceCapabilityResult {
    bool can_run;
    const char * skip_reason;
};

DeviceCapabilityResult canDeviceHandleTest(size_t numThreads, size_t blockSize) {
    cudaDeviceProp prop;
    auto err = cudaGetDeviceProperties(&prop, 0);
    if (err != cudaSuccess) {
        return { false, cudaGetErrorString(err) };
    }

    size_t gridSize = (numThreads + blockSize - 1) / blockSize;

    if (blockSize > static_cast<std::size_t>(prop.maxThreadsPerBlock)) {
        std::stringstream ss;
        ss << "Block size " << blockSize << " exceeds device limit of " << prop.maxThreadsPerBlock
           << " threads per block";
        static std::string msg = ss.str();
        return { false, msg.c_str() };
    }

    if (gridSize > static_cast<std::size_t>(prop.maxGridSize[0])) {
        std::stringstream ss;
        ss << "Grid size " << gridSize << " exceeds device limit of " << prop.maxGridSize[0]
           << " blocks in X dimension";
        static std::string msg = ss.str();
        return { false, msg.c_str() };
    }

    if (numThreads > static_cast<std::size_t>(prop.maxThreadsPerBlock * prop.maxGridSize[0])) {
        std::stringstream ss;
        ss << "Total thread count " << numThreads << " exceeds device maximum of "
           << prop.maxThreadsPerBlock * prop.maxGridSize[0] << " threads";
        static std::string msg = ss.str();
        return { false, msg.c_str() };
    }

    return { true, "" };
}

// Modify test registration to use enhanced capability check
#define REGISTER_GPU_TEST(test_class, num_threads, block_size, timeout_ms, poll_ms)                                    \
    TEST_F(test_class, BasicTest) {                                                                                    \
        auto result = canDeviceHandleTest(num_threads, block_size);                                                    \
        if (!result.can_run) {                                                                                         \
            GTEST_SKIP() << "Test skipped: " << result.skip_reason;                                                    \
        }                                                                                                              \
        this->runTest(1, timeout_ms, poll_ms);                                                                         \
    }

#ifdef NDEBUG
#  define REGISTER_STRESS_TEST(test_class, num_threads, block_size, iterations, timeout_ms, poll_ms)                   \
      TEST_F(test_class, StressTest_##iterations) {                                                                    \
          auto result = canDeviceHandleTest(num_threads, block_size);                                                  \
          if (!result.can_run) {                                                                                       \
              GTEST_SKIP() << "Test skipped: " << result.skip_reason;                                                  \
          }                                                                                                            \
          this->runTest(iterations, timeout_ms, poll_ms);                                                              \
      }
#else
#  define REGISTER_STRESS_TEST(test_class, num_threads, block_size, iterations, timeout_ms, poll_ms)                   \
      TEST_F(test_class, StressTest_##iterations) {                                                                    \
          GTEST_SKIP() << "Skipping stress test in debug mode";                                                        \
      }
#endif

// Register tests with enhanced capability checks
REGISTER_GPU_TEST(AutobatchGPUTest32_1M_8_256, 1024 * 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest16_512K_8_256, 512 * 512, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest31_1K_8_256, 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest47_1K_8_256, 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest32_1K_2_256, 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest32_1K_4_256, 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest32_1K_8_256, 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest30_1K_8_256, 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest32_32_2_32, 32, 32, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)

//high num buffers tests
REGISTER_GPU_TEST(AutobatchGPUTest1024_1M_256_64, 1024 * 1024, 64, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest1024_1M_256_256, 1024 * 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest16_1M_256_256, 1024 * 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)

REGISTER_GPU_TEST(AutobatchGPUTest32_1K_1_256, 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest32_32_1_32, 32, 32, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest64_32_1_32, 32, 32, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)

REGISTER_GPU_TEST(AutobatchGPUTest32_1K_8_256_cg, 1024, 256, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_GPU_TEST(AutobatchGPUTest32K_1M_8_128_cg, 1024 * 1024, 128, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)

// Run stress tests on configurations known to have issues
REGISTER_STRESS_TEST(AutobatchGPUTest32_1K_2_256, 1024, 256, 200, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_STRESS_TEST(AutobatchGPUTest32_1K_1_256, 1024, 256, 200, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_STRESS_TEST(AutobatchGPUTest30_1K_2_256, 1024, 256, 200, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_STRESS_TEST(
    AutobatchGPUTest32K_1M_8_128_cg, 1024 * 1024, 128, 200, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
REGISTER_STRESS_TEST(AutobatchGPUTest32_1K_8_256_cg, 1024, 256, 200, defaults::MAX_WAIT_MS, defaults::POLL_SLEEP_MS)
