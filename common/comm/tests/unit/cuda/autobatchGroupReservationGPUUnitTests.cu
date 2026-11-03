/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cooperative_groups.h>
#include <cuda_runtime.h>
#include <gdakis/impl/buffer>
#include <gtest/gtest.h>

#include <cassert>
#include <limits>
#include <sstream>

namespace gdakis::comm {

template<typename T, thread_scope _Sco>
struct buffer_test_access {
    using buffer_type = buffer<T, _Sco>;

    __device__ static uint64_t snapshot_before_close_release(buffer_type * buffer) {
        return buffer->in_.load(memory_order_relaxed) & ~buffer_type::COMMIT_BIT_MASK;
    }

    __device__ static unsigned publish_exit(buffer_type * buffer, unsigned count) {
        return buffer->out_.fetch_add(count, memory_order_release) + count;
    }

    __device__ static void finish_exit(buffer_type * buffer, unsigned out_count, unsigned epoch, uint64_t in_count) {
        buffer->finish_exit(out_count, epoch, in_count);
    }
};

} // namespace gdakis::comm

namespace {

namespace cg = cooperative_groups;

constexpr unsigned buffer_capacity = 4;
constexpr unsigned prefill_count = 2;
constexpr unsigned group_size = 4;

template<unsigned _Capacity>
struct test_storage {
    __host__ __device__ constexpr unsigned size() const noexcept { return _Capacity; }

    __host__ __device__ void commit(unsigned) noexcept {}

    __host__ __device__ void acquire() noexcept {}
};

template<unsigned _Capacity>
using test_buffer = gdakis::comm::buffer<test_storage<_Capacity>, gdakis::thread_scope_device>;

struct reservation_results {
    bool prefill_succeeded_ = true;
    unsigned prefill_epoch_ = {};
    bool first_entered_[group_size] = {};
    unsigned first_position_[group_size] = {};
    unsigned first_epoch_[group_size] = {};
    bool retry_entered_[group_size] = {};
    unsigned retry_position_[group_size] = {};
    unsigned retry_epoch_[group_size] = {};
};

__global__ void reserve_group_across_buffer_boundary(
    test_buffer<buffer_capacity> * buffer, reservation_results * results) {
    if (threadIdx.x == 0) {
        results->prefill_succeeded_ = true;
        for (unsigned i = 0; i < prefill_count; ++i) {
            unsigned position = std::numeric_limits<unsigned>::max();
            unsigned epoch = std::numeric_limits<unsigned>::max();
            results->prefill_succeeded_ &= buffer->try_enter(position, epoch);
            results->prefill_epoch_ = epoch;
        }
    }
    __syncthreads();

    auto block = cg::this_thread_block();
    auto group = cg::tiled_partition<group_size>(block);
    unsigned position = std::numeric_limits<unsigned>::max();
    unsigned epoch = std::numeric_limits<unsigned>::max();
    auto entered = buffer->try_enter_group(group, position, epoch);

    auto lane = group.thread_rank();
    results->first_entered_[lane] = entered;
    results->first_position_[lane] = position;
    results->first_epoch_[lane] = epoch;
    group.sync();

    if (lane == 0) {
        buffer->exit_impl(prefill_count, results->prefill_epoch_);
    }
    group.sync();

    position = std::numeric_limits<unsigned>::max();
    epoch = std::numeric_limits<unsigned>::max();
    entered = buffer->try_enter_group(group, position, epoch);

    results->retry_entered_[lane] = entered;
    results->retry_position_[lane] = position;
    results->retry_epoch_[lane] = epoch;
}

constexpr unsigned concurrent_capacity = 6;
constexpr unsigned concurrent_prefill = 4;
constexpr unsigned concurrent_group_count = 4;
constexpr unsigned concurrent_thread_count = concurrent_group_count * group_size;

struct concurrent_reservation_results {
    bool prefill_succeeded_ = true;
    unsigned prefill_epoch_ = {};
    bool first_entered_[concurrent_thread_count] = {};
    bool second_entered_[concurrent_thread_count] = {};
    unsigned second_position_[concurrent_thread_count] = {};
    unsigned second_epoch_[concurrent_thread_count] = {};
    bool final_probe_entered_ = false;
    unsigned final_probe_position_ = {};
    unsigned final_probe_epoch_ = {};
};

__global__ void reserve_groups_concurrently(
    test_buffer<concurrent_capacity> * buffer, concurrent_reservation_results * results) {
    if (threadIdx.x == 0) {
        results->prefill_succeeded_ = true;
        for (unsigned i = 0; i < concurrent_prefill; ++i) {
            unsigned position = std::numeric_limits<unsigned>::max();
            unsigned epoch = std::numeric_limits<unsigned>::max();
            results->prefill_succeeded_ &= buffer->try_enter(position, epoch);
            results->prefill_epoch_ = epoch;
        }
    }
    __syncthreads();

    auto block = cg::this_thread_block();
    auto group = cg::tiled_partition<group_size>(block);
    unsigned position = std::numeric_limits<unsigned>::max();
    unsigned epoch = std::numeric_limits<unsigned>::max();
    auto entered = buffer->try_enter_group(group, position, epoch);

    results->first_entered_[threadIdx.x] = entered;
    __syncthreads();

    if (threadIdx.x == 0) {
        buffer->exit_impl(concurrent_prefill, results->prefill_epoch_);
    }
    __syncthreads();

    position = std::numeric_limits<unsigned>::max();
    epoch = std::numeric_limits<unsigned>::max();
    entered = buffer->try_enter_group(group, position, epoch);

    results->second_entered_[threadIdx.x] = entered;
    results->second_position_[threadIdx.x] = position;
    results->second_epoch_[threadIdx.x] = epoch;
    __syncthreads();

    if (entered) {
        buffer->exit_group(group, epoch);
    }
    __syncthreads();

    if (threadIdx.x == 0) {
        results->final_probe_entered_ = buffer->try_enter(results->final_probe_position_, results->final_probe_epoch_);
        if (results->final_probe_entered_) {
            buffer->exit_impl(1, results->final_probe_epoch_);
        }
    }
}

constexpr unsigned scalar_capacity = 6;
constexpr unsigned scalar_prefill = 4;

struct scalar_overshoot_results {
    bool prefill_succeeded_ = true;
    unsigned prefill_epoch_ = {};
    bool scalar_entered_[group_size] = {};
    unsigned scalar_position_[group_size] = {};
    bool retry_entered_[group_size] = {};
    unsigned retry_position_[group_size] = {};
    unsigned retry_epoch_[group_size] = {};
};

__global__ void reserve_scalar_group_across_buffer_boundary(
    test_buffer<scalar_capacity> * buffer, scalar_overshoot_results * results) {
    if (threadIdx.x == 0) {
        results->prefill_succeeded_ = true;
        for (unsigned i = 0; i < scalar_prefill; ++i) {
            unsigned position = std::numeric_limits<unsigned>::max();
            unsigned epoch = std::numeric_limits<unsigned>::max();
            results->prefill_succeeded_ &= buffer->try_enter(position, epoch);
            results->prefill_epoch_ = epoch;
        }
    }
    __syncthreads();

    auto block = cg::this_thread_block();
    auto group = cg::tiled_partition<group_size>(block);
    unsigned position = std::numeric_limits<unsigned>::max();
    unsigned epoch = std::numeric_limits<unsigned>::max();
    auto entered = buffer->try_enter(position, epoch);
    auto lane = group.thread_rank();

    results->scalar_entered_[lane] = entered;
    results->scalar_position_[lane] = position;
    group.sync();

    if (lane == 0) {
        buffer->exit_impl(scalar_prefill, results->prefill_epoch_);
    }
    group.sync();

    if (entered) {
        buffer->exit(epoch);
    }
    group.sync();

    position = std::numeric_limits<unsigned>::max();
    epoch = std::numeric_limits<unsigned>::max();
    entered = buffer->try_enter_group(group, position, epoch);

    results->retry_entered_[lane] = entered;
    results->retry_position_[lane] = position;
    results->retry_epoch_[lane] = epoch;
}

constexpr unsigned offset_capacity = 8;
constexpr unsigned commit_slots = 2;
constexpr unsigned unwritten_value = std::numeric_limits<unsigned>::max();

struct write_observer {
    unsigned values_[offset_capacity] = {};
    unsigned committed_counts_[commit_slots] = {};
    bool every_committed_slot_written_[commit_slots] = {};
    unsigned commits_ = {};
};

struct observed_storage {
    write_observer * observer_;

    __host__ __device__ constexpr unsigned size() const noexcept { return offset_capacity; }

    __host__ __device__ void set(unsigned position, unsigned value) noexcept { observer_->values_[position] = value; }

    __host__ __device__ void commit(unsigned count) noexcept {
#if defined(__CUDA_ARCH__)
        auto commit_index = atomicAdd(&observer_->commits_, 1U);
#else
        auto commit_index = observer_->commits_++;
#endif
        if (commit_index >= commit_slots) {
            return;
        }

        observer_->committed_counts_[commit_index] = count;
        auto complete = true;
        for (unsigned position = 0; position < count; ++position) {
            complete &= observer_->values_[position] != unwritten_value;
        }
        observer_->every_committed_slot_written_[commit_index] = complete;

        for (unsigned position = 0; position < offset_capacity; ++position) {
            observer_->values_[position] = unwritten_value;
        }
    }

    __host__ __device__ void acquire() noexcept {}
};

using observed_buffer = gdakis::comm::buffer<observed_storage, gdakis::thread_scope_device>;
using observed_buffer_test_access = gdakis::comm::buffer_test_access<observed_storage, gdakis::thread_scope_device>;

struct offset_reservation_results {
    bool prefill_succeeded_ = true;
    unsigned prefill_epoch_ = {};
    bool first_entered_[group_size] = {};
    unsigned first_position_[group_size] = {};
    unsigned first_epoch_[group_size] = {};
    bool retry_entered_[group_size] = {};
    unsigned retry_position_[group_size] = {};
    unsigned retry_epoch_[group_size] = {};
};

__global__ void exercise_group_at_offset(
    observed_buffer * buffer, write_observer * observer, offset_reservation_results * results, unsigned prefill) {
    if (threadIdx.x == 0) {
        results->prefill_succeeded_ = true;
        for (unsigned index = 0; index < prefill; ++index) {
            unsigned position = unwritten_value;
            unsigned epoch = unwritten_value;
            results->prefill_succeeded_ &= buffer->try_enter(position, epoch);
            results->prefill_epoch_ = epoch;
            buffer->set(position, index);
        }
    }
    __syncthreads();

    auto block = cg::this_thread_block();
    auto group = cg::tiled_partition<group_size>(block);
    auto lane = group.thread_rank();
    unsigned position = unwritten_value;
    unsigned epoch = unwritten_value;
    auto entered = buffer->try_enter_group(group, position, epoch);

    results->first_entered_[lane] = entered;
    results->first_position_[lane] = position;
    results->first_epoch_[lane] = epoch;
    if (entered) {
        buffer->set(position, offset_capacity + lane);
    }
    group.sync();

    if (entered) {
        buffer->exit_group(group, epoch);
    }
    group.sync();

    if (lane == 0 && prefill != 0) {
        buffer->exit_impl(prefill, results->prefill_epoch_);
    }
    group.sync();

    position = unwritten_value;
    epoch = unwritten_value;
    entered = buffer->try_enter_group(group, position, epoch);

    results->retry_entered_[lane] = entered;
    results->retry_position_[lane] = position;
    results->retry_epoch_[lane] = epoch;
    if (entered) {
        buffer->set(position, 2 * offset_capacity + lane);
    }
    group.sync();

    if (entered) {
        buffer->exit_group(group, epoch);
    }
    group.sync();

    if (lane == 0) {
        assert(observer->commits_ == commit_slots);
    }
}

constexpr unsigned missed_finalizer_prefill = 6;

struct missed_finalizer_results {
    bool prefill_succeeded_ = true;
    bool close_group_entered_[group_size] = {};
    bool retry_entered_[group_size] = {};
    unsigned retry_position_[group_size] = {};
    unsigned retry_epoch_[group_size] = {};
};

__global__ void exercise_missed_finalizer_interleaving(observed_buffer * buffer, missed_finalizer_results * results) {
    unsigned prefill_epoch = {};
    if (threadIdx.x == 0) {
        results->prefill_succeeded_ = true;
        for (unsigned index = 0; index < missed_finalizer_prefill; ++index) {
            unsigned position = unwritten_value;
            unsigned epoch = unwritten_value;
            results->prefill_succeeded_ &= buffer->try_enter(position, epoch);
            prefill_epoch = epoch;
            buffer->set(position, index);
        }
    }
    __syncthreads();

    auto block = cg::this_thread_block();
    auto group = cg::tiled_partition<group_size>(block);
    auto lane = group.thread_rank();
    unsigned position = unwritten_value;
    unsigned epoch = unwritten_value;
    results->close_group_entered_[lane] = buffer->try_enter_group(group, position, epoch);
    group.sync();

    if (lane == 0) {
        // Force the allowed stale snapshot from immediately before publish_close() set the commit bit. The closer has
        // already sampled out_, so only this last completing producer can finalize the six-entry prefix.
        auto stale_in = observed_buffer_test_access::snapshot_before_close_release(buffer);
        auto out_count = observed_buffer_test_access::publish_exit(buffer, missed_finalizer_prefill);
        observed_buffer_test_access::finish_exit(buffer, out_count, prefill_epoch, stale_in);
    }
    group.sync();

    position = unwritten_value;
    epoch = unwritten_value;
    auto entered = buffer->try_enter_group(group, position, epoch);
    results->retry_entered_[lane] = entered;
    results->retry_position_[lane] = position;
    results->retry_epoch_[lane] = epoch;
}

std::string describe(const reservation_results & results) {
    std::ostringstream out;
    out << "observed group reservation:";
    for (unsigned lane = 0; lane < group_size; ++lane) {
        out << "\n  lane " << lane << ": first_entered=" << std::boolalpha << results.first_entered_[lane]
            << ", first_position=" << results.first_position_[lane] << ", first_epoch=" << results.first_epoch_[lane]
            << ", retry_entered=" << results.retry_entered_[lane]
            << ", retry_position=" << results.retry_position_[lane] << ", retry_epoch=" << results.retry_epoch_[lane];
    }
    return out.str();
}

TEST(AutobatchGroupReservation, StraddlingCapacityIsRejectedForEveryLaneAndBufferRemainsUsable) {
    int device_count = 0;
    auto status = cudaGetDeviceCount(&device_count);
    if (status != cudaSuccess || device_count == 0) {
        GTEST_SKIP() << "No CUDA-capable GPU is available: " << cudaGetErrorString(status);
    }

    test_buffer<buffer_capacity> host_buffer { test_storage<buffer_capacity> {} };
    test_buffer<buffer_capacity> * device_buffer = nullptr;
    reservation_results * device_results = nullptr;

    ASSERT_EQ(cudaMalloc(&device_buffer, sizeof(*device_buffer)), cudaSuccess);
    ASSERT_EQ(cudaMalloc(&device_results, sizeof(reservation_results)), cudaSuccess);
    ASSERT_EQ(cudaMemcpy(device_buffer, &host_buffer, sizeof(host_buffer), cudaMemcpyHostToDevice), cudaSuccess);
    ASSERT_EQ(cudaMemset(device_results, 0, sizeof(reservation_results)), cudaSuccess);

    reserve_group_across_buffer_boundary<<<1, group_size>>>(device_buffer, device_results);
    ASSERT_EQ(cudaGetLastError(), cudaSuccess);
    ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

    reservation_results results;
    ASSERT_EQ(cudaMemcpy(&results, device_results, sizeof(results), cudaMemcpyDeviceToHost), cudaSuccess);
    EXPECT_EQ(cudaFree(device_results), cudaSuccess);
    EXPECT_EQ(cudaFree(device_buffer), cudaSuccess);

    ASSERT_TRUE(results.prefill_succeeded_);
    auto observed = describe(results);
    for (unsigned lane = 0; lane < group_size; ++lane) {
        EXPECT_FALSE(results.first_entered_[lane]) << observed;
        EXPECT_TRUE(results.retry_entered_[lane]) << observed;
        EXPECT_EQ(results.retry_position_[lane], lane) << observed;
        EXPECT_EQ(results.retry_epoch_[lane], 1) << observed;
    }
}

TEST(AutobatchGroupReservation, ReservationsBehindStraddlingGroupDoNotPreventClose) {
    int device_count = 0;
    auto status = cudaGetDeviceCount(&device_count);
    if (status != cudaSuccess || device_count == 0) {
        GTEST_SKIP() << "No CUDA-capable GPU is available: " << cudaGetErrorString(status);
    }

    test_buffer<concurrent_capacity> host_buffer { test_storage<concurrent_capacity> {} };
    test_buffer<concurrent_capacity> * device_buffer = nullptr;
    concurrent_reservation_results * device_results = nullptr;

    ASSERT_EQ(cudaMalloc(&device_buffer, sizeof(*device_buffer)), cudaSuccess);
    ASSERT_EQ(cudaMalloc(&device_results, sizeof(*device_results)), cudaSuccess);
    ASSERT_EQ(cudaMemcpy(device_buffer, &host_buffer, sizeof(host_buffer), cudaMemcpyHostToDevice), cudaSuccess);
    ASSERT_EQ(cudaMemset(device_results, 0, sizeof(*device_results)), cudaSuccess);

    reserve_groups_concurrently<<<1, concurrent_thread_count>>>(device_buffer, device_results);
    ASSERT_EQ(cudaGetLastError(), cudaSuccess);
    ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

    concurrent_reservation_results results;
    ASSERT_EQ(cudaMemcpy(&results, device_results, sizeof(results), cudaMemcpyDeviceToHost), cudaSuccess);
    EXPECT_EQ(cudaFree(device_results), cudaSuccess);
    EXPECT_EQ(cudaFree(device_buffer), cudaSuccess);

    ASSERT_TRUE(results.prefill_succeeded_);
    unsigned successful_groups = 0;
    for (unsigned group_index = 0; group_index < concurrent_group_count; ++group_index) {
        auto first_lane = group_index * group_size;
        EXPECT_FALSE(results.first_entered_[first_lane]);

        auto group_entered = results.second_entered_[first_lane];
        successful_groups += group_entered;
        for (unsigned lane = 0; lane < group_size; ++lane) {
            auto thread = first_lane + lane;
            EXPECT_EQ(results.first_entered_[thread], results.first_entered_[first_lane]);
            EXPECT_EQ(results.second_entered_[thread], group_entered);
            if (group_entered) {
                EXPECT_EQ(results.second_position_[thread], lane);
                EXPECT_EQ(results.second_epoch_[thread], 1);
            }
        }
    }
    EXPECT_EQ(successful_groups, 1);
    EXPECT_TRUE(results.final_probe_entered_);
    EXPECT_EQ(results.final_probe_position_, 0);
    EXPECT_EQ(results.final_probe_epoch_, 2);
}

TEST(AutobatchGroupReservation, ScalarOvershootStillCommitsAtCapacityWithoutClose) {
    int device_count = 0;
    auto status = cudaGetDeviceCount(&device_count);
    if (status != cudaSuccess || device_count == 0) {
        GTEST_SKIP() << "No CUDA-capable GPU is available: " << cudaGetErrorString(status);
    }

    test_buffer<scalar_capacity> host_buffer { test_storage<scalar_capacity> {} };
    test_buffer<scalar_capacity> * device_buffer = nullptr;
    scalar_overshoot_results * device_results = nullptr;

    ASSERT_EQ(cudaMalloc(&device_buffer, sizeof(*device_buffer)), cudaSuccess);
    ASSERT_EQ(cudaMalloc(&device_results, sizeof(*device_results)), cudaSuccess);
    ASSERT_EQ(cudaMemcpy(device_buffer, &host_buffer, sizeof(host_buffer), cudaMemcpyHostToDevice), cudaSuccess);
    ASSERT_EQ(cudaMemset(device_results, 0, sizeof(*device_results)), cudaSuccess);

    reserve_scalar_group_across_buffer_boundary<<<1, group_size>>>(device_buffer, device_results);
    ASSERT_EQ(cudaGetLastError(), cudaSuccess);
    ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

    scalar_overshoot_results results;
    ASSERT_EQ(cudaMemcpy(&results, device_results, sizeof(results), cudaMemcpyDeviceToHost), cudaSuccess);
    EXPECT_EQ(cudaFree(device_results), cudaSuccess);
    EXPECT_EQ(cudaFree(device_buffer), cudaSuccess);

    ASSERT_TRUE(results.prefill_succeeded_);
    for (unsigned lane = 0; lane < group_size; ++lane) {
        EXPECT_EQ(results.scalar_entered_[lane], lane < scalar_capacity - scalar_prefill);
        EXPECT_EQ(results.scalar_position_[lane], scalar_prefill + lane);
        EXPECT_TRUE(results.retry_entered_[lane]);
        EXPECT_EQ(results.retry_position_[lane], lane);
        EXPECT_EQ(results.retry_epoch_[lane], 1);
    }
}

TEST(AutobatchGroupReservation, EveryOffsetCommitsOnlyFullyWrittenSlotsAndMakesProgress) {
    int device_count = 0;
    auto status = cudaGetDeviceCount(&device_count);
    if (status != cudaSuccess || device_count == 0) {
        GTEST_SKIP() << "No CUDA-capable GPU is available: " << cudaGetErrorString(status);
    }

    for (unsigned prefill = 0; prefill <= offset_capacity; ++prefill) {
        observed_buffer * device_buffer = nullptr;
        write_observer * device_observer = nullptr;
        offset_reservation_results * device_results = nullptr;

        ASSERT_EQ(cudaMalloc(&device_buffer, sizeof(*device_buffer)), cudaSuccess);
        ASSERT_EQ(cudaMalloc(&device_observer, sizeof(*device_observer)), cudaSuccess);
        ASSERT_EQ(cudaMalloc(&device_results, sizeof(*device_results)), cudaSuccess);

        write_observer host_observer;
        for (auto & value : host_observer.values_) {
            value = unwritten_value;
        }
        observed_buffer host_buffer { observed_storage { device_observer } };

        ASSERT_EQ(cudaMemcpy(device_buffer, &host_buffer, sizeof(host_buffer), cudaMemcpyHostToDevice), cudaSuccess);
        ASSERT_EQ(
            cudaMemcpy(device_observer, &host_observer, sizeof(host_observer), cudaMemcpyHostToDevice), cudaSuccess);
        ASSERT_EQ(cudaMemset(device_results, 0, sizeof(*device_results)), cudaSuccess);

        exercise_group_at_offset<<<1, group_size>>>(device_buffer, device_observer, device_results, prefill);
        ASSERT_EQ(cudaGetLastError(), cudaSuccess);
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess) << "prefill=" << prefill;

        offset_reservation_results results;
        ASSERT_EQ(cudaMemcpy(&results, device_results, sizeof(results), cudaMemcpyDeviceToHost), cudaSuccess);
        ASSERT_EQ(
            cudaMemcpy(&host_observer, device_observer, sizeof(host_observer), cudaMemcpyDeviceToHost), cudaSuccess);

        EXPECT_EQ(cudaFree(device_results), cudaSuccess);
        EXPECT_EQ(cudaFree(device_observer), cudaSuccess);
        EXPECT_EQ(cudaFree(device_buffer), cudaSuccess);

        ASSERT_TRUE(results.prefill_succeeded_) << "prefill=" << prefill;
        auto first_group_fits = prefill + group_size <= offset_capacity;
        for (unsigned lane = 0; lane < group_size; ++lane) {
            EXPECT_EQ(results.first_entered_[lane], first_group_fits) << "prefill=" << prefill;
            if (first_group_fits) {
                EXPECT_EQ(results.first_position_[lane], prefill + lane) << "prefill=" << prefill;
                EXPECT_EQ(results.first_epoch_[lane], 0) << "prefill=" << prefill;
            }
            EXPECT_TRUE(results.retry_entered_[lane]) << "prefill=" << prefill;
            EXPECT_EQ(results.retry_position_[lane], lane) << "prefill=" << prefill;
            EXPECT_EQ(results.retry_epoch_[lane], 1) << "prefill=" << prefill;
        }

        ASSERT_EQ(host_observer.commits_, commit_slots) << "prefill=" << prefill;
        EXPECT_EQ(host_observer.committed_counts_[0], first_group_fits ? prefill + group_size : prefill)
            << "prefill=" << prefill;
        EXPECT_EQ(host_observer.committed_counts_[1], group_size) << "prefill=" << prefill;
        EXPECT_TRUE(host_observer.every_committed_slot_written_[0]) << "prefill=" << prefill;
        EXPECT_TRUE(host_observer.every_committed_slot_written_[1]) << "prefill=" << prefill;
    }
}

TEST(AutobatchGroupReservation, MissedFinalizerInterleavingCommitsAndMakesProgress) {
    int device_count = 0;
    auto status = cudaGetDeviceCount(&device_count);
    if (status != cudaSuccess || device_count == 0) {
        GTEST_SKIP() << "No CUDA-capable GPU is available: " << cudaGetErrorString(status);
    }

    observed_buffer * device_buffer = nullptr;
    write_observer * device_observer = nullptr;
    missed_finalizer_results * device_results = nullptr;

    ASSERT_EQ(cudaMalloc(&device_buffer, sizeof(*device_buffer)), cudaSuccess);
    ASSERT_EQ(cudaMalloc(&device_observer, sizeof(*device_observer)), cudaSuccess);
    ASSERT_EQ(cudaMalloc(&device_results, sizeof(*device_results)), cudaSuccess);

    write_observer host_observer;
    for (auto & value : host_observer.values_) {
        value = unwritten_value;
    }
    observed_buffer host_buffer { observed_storage { device_observer } };

    ASSERT_EQ(cudaMemcpy(device_buffer, &host_buffer, sizeof(host_buffer), cudaMemcpyHostToDevice), cudaSuccess);
    ASSERT_EQ(cudaMemcpy(device_observer, &host_observer, sizeof(host_observer), cudaMemcpyHostToDevice), cudaSuccess);
    ASSERT_EQ(cudaMemset(device_results, 0, sizeof(*device_results)), cudaSuccess);

    exercise_missed_finalizer_interleaving<<<1, group_size>>>(device_buffer, device_results);
    ASSERT_EQ(cudaGetLastError(), cudaSuccess);
    ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

    missed_finalizer_results results;
    ASSERT_EQ(cudaMemcpy(&results, device_results, sizeof(results), cudaMemcpyDeviceToHost), cudaSuccess);
    ASSERT_EQ(cudaMemcpy(&host_observer, device_observer, sizeof(host_observer), cudaMemcpyDeviceToHost), cudaSuccess);

    EXPECT_EQ(cudaFree(device_results), cudaSuccess);
    EXPECT_EQ(cudaFree(device_observer), cudaSuccess);
    EXPECT_EQ(cudaFree(device_buffer), cudaSuccess);

    ASSERT_TRUE(results.prefill_succeeded_);
    for (unsigned lane = 0; lane < group_size; ++lane) {
        EXPECT_FALSE(results.close_group_entered_[lane]);
        EXPECT_TRUE(results.retry_entered_[lane]);
        EXPECT_EQ(results.retry_position_[lane], lane);
        EXPECT_EQ(results.retry_epoch_[lane], 1);
    }
    ASSERT_EQ(host_observer.commits_, 1);
    EXPECT_EQ(host_observer.committed_counts_[0], missed_finalizer_prefill);
    EXPECT_TRUE(host_observer.every_committed_slot_written_[0]);
}

} // namespace
