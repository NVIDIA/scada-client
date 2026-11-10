/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/atomic>
#include <gdakis/autobatch>
#include <gdakis/barrier>
#include <gdakis/umem/cpu>
#include <gdakis/umem/mem>
#include <tests/utility>

#include <thread>
#include <vector>

class doorbell {
    gdakis::atomic<int, gdakis::thread_scope_system> db_;

    __host__ __device__ inline void __notify(int value) noexcept {
        int old = 0;
        while (db_.compare_exchange_strong(old, value, gdakis::memory_order_release, gdakis::memory_order_acquire)
            == false) {
#if defined(__CUDA_ARCH__)
            gdakis::this_thread::nanosleep(1);
#else
            std::this_thread::yield();
#endif

            old = 0;
        }
    }

public:
    __host__ __device__ doorbell() noexcept : db_(0) {}

    template<typename _Fn>
    __host__ __device__ inline void ack(_Fn && fn) noexcept {
        int val = 0;
        while (val >= 0) {
            val = db_.load(gdakis::memory_order_acquire);
            while (val == 0) {
#if defined(__CUDA_ARCH__)
                gdakis::this_thread::nanosleep(1);
#else
                std::this_thread::yield();
#endif
                val = db_.load(gdakis::memory_order_acquire);
            }
            if (val < 0)
                break;
            fn(val);
            db_.store(0, gdakis::memory_order_release);
        }
    }

    __host__ __device__ inline int load(gdakis::memory_order mem_order) const noexcept { return db_.load(mem_order); }

    __host__ __device__ inline bool notify(unsigned pos) noexcept {
        if (pos > 0)
            __notify(pos);
        return pos > 0;
    }

    __host__ __device__ inline void cancel() noexcept { __notify(-1); }
};

struct entry {
    unsigned id_;
    unsigned value_;
};

struct buffer {
    unsigned size_;
    entry * data_;
    doorbell * db_;

    __host__ __device__ inline buffer(unsigned size, entry * data, doorbell * db) noexcept
        : size_(size),
          data_(data),
          db_(db) {}

    __host__ __device__ inline void commit(unsigned pos) noexcept { db_->notify(pos); }

    __host__ __device__ inline void acquire() noexcept { return; }

    __host__ __device__ inline unsigned size() const noexcept { return size_; }

    template<typename _Fn>
    __host__ __device__ inline void set(unsigned pos, _Fn && fn) {
        fn(data_[pos]);
    }
};

template<typename _Ab>
class AutobatchTest : public ::testing::Test {};

using autobatch_test_types = ::testing::Types<gdakis::comm::autobatch<buffer, gdakis::thread_scope_device, 2>,
    gdakis::comm::autobatch<buffer, gdakis::thread_scope_device, 4>,
    gdakis::comm::autobatch<buffer, gdakis::thread_scope_device, 8>,
    gdakis::comm::autobatch<buffer, gdakis::thread_scope_device, 16>>;
TYPED_TEST_SUITE(AutobatchTest, autobatch_test_types);

TYPED_TEST(AutobatchTest, Create) {
    const unsigned n_entries = 512;
    auto data = gdakis::umem::mem<gdakis::umem::cpu, entry[]>::alloc(TypeParam::number_of_buffers() * n_entries);
    ASSERT_NE(data, nullptr);
    auto db = gdakis::umem::mem<gdakis::umem::cpu, doorbell[]>::alloc(TypeParam::number_of_buffers());
    ASSERT_NE(db, nullptr);

    TypeParam autobatch(
        [&] __host__(std::size_t i) { return buffer(n_entries, data.get() + (n_entries * i), db.get() + i); });
}

template<typename _Ab>
__host__ __device__ static inline void push(_Ab & autobatch, const entry & value) noexcept {
    autobatch.push([value] __host__ __device__(entry & e) {
        e.id_ = value.id_;
        e.value_ = value.value_;
    });
}

TYPED_TEST(AutobatchTest, SingleThread) {
    const unsigned n_entries = 32;
    auto data = gdakis::umem::mem<gdakis::umem::cpu, entry[]>::alloc(TypeParam::number_of_buffers() * n_entries);
    ASSERT_NE(data, nullptr);
    auto db = gdakis::umem::mem<gdakis::umem::cpu, doorbell[]>::alloc(TypeParam::number_of_buffers());
    ASSERT_NE(db, nullptr);
    gdakis::umem::memset<gdakis::umem::cpu>(db, 0, TypeParam::number_of_buffers() * sizeof(doorbell));

    TypeParam autobatch(
        [&] __host__(std::size_t i) { return buffer(n_entries, data.get() + (n_entries * i), db.get() + i); });

    push<TypeParam>(autobatch, entry { 1, 0xcafe });

    EXPECT_EQ(db.get()->load(gdakis::memory_order_acquire), 1);
    EXPECT_EQ(data.get()->id_, 1);
    EXPECT_EQ(data.get()->value_, 0xcafe);
}

TYPED_TEST(AutobatchTest, MultiThread) {
    const unsigned n_entries = 32;
    auto data = gdakis::umem::mem<gdakis::umem::cpu, entry[]>::alloc(TypeParam::number_of_buffers() * n_entries);
    ASSERT_NE(data, nullptr);
    auto db = gdakis::umem::mem<gdakis::umem::cpu, doorbell[]>::alloc(TypeParam::number_of_buffers());
    ASSERT_NE(db, nullptr);
    gdakis::umem::memset<gdakis::umem::cpu>(db, 0, TypeParam::number_of_buffers() * sizeof(doorbell));

    std::atomic<unsigned> db_count(0);
    gdakis::barrier control(TypeParam::number_of_buffers() + 1);
    std::vector<std::thread> ctrl;
    for (auto n = 0u; n < TypeParam::number_of_buffers(); ++n) {
        ctrl.emplace_back([&, n] {
            control.arrive_and_wait();
            db.get()[n].ack([&](int value) {
                for (auto m = 0; m < value; ++m) {
                    EXPECT_EQ(data.get()[(n * n_entries) + m].id_, 1);
                    EXPECT_EQ(data.get()[(n * n_entries) + m].value_, 0xcafe);
                }
                db_count.fetch_add(value, std::memory_order_release);
            });
        });
    }
    control.arrive_and_wait();

    TypeParam autobatch(
        [&] __host__(std::size_t i) { return buffer(n_entries, data.get() + (n_entries * i), db.get() + i); });

    gdakis::barrier start(n_entries);
    std::vector<std::thread> threads;
    for (auto n = 0u; n < n_entries; ++n) {
        threads.emplace_back(std::thread([&] {
            start.arrive_and_wait();
            push<TypeParam>(autobatch, entry { 1, 0xcafe });
        }));
    }

    for (auto & thread : threads)
        thread.join();

    for (auto n = 0u; n < TypeParam::number_of_buffers(); n++) {
        db.get()[n].cancel();
    }

    for (auto & thread : ctrl)
        thread.join();
    EXPECT_EQ(db_count.load(std::memory_order_acquire), n_entries);
}

#if defined(__CUDACC__)

#  include <cooperative_groups.h>
#  include <gdakis/umem/cuda>

template<typename _Ab>
struct CUDAAutobatchTest : public ::tests::CUDATest {};

using cuda_autobatch_test_types = ::testing::Types<gdakis::autobatch<buffer, gdakis::thread_scope_device, 1>,
    gdakis::autobatch<buffer, gdakis::thread_scope_device, 2>,
    gdakis::autobatch<buffer, gdakis::thread_scope_device, 3>, gdakis::autobatch<buffer, gdakis::thread_scope_device, 4>
    //    gdakis::autobatch<buffer, gdakis::thread_scope_device, 5>
    //    gdakis::autobatch<buffer, gdakis::thread_scope_device, 6>
    //    gdakis::autobatch<buffer, gdakis::thread_scope_device, 7>
    //    gdakis::autobatch<buffer, gdakis::thread_scope_device, 8>
    >;
TYPED_TEST_SUITE(CUDAAutobatchTest, cuda_autobatch_test_types);

template<typename _Ab>
static inline gdakis::umem::ptr<_Ab> build_device_autobatch(unsigned n_entries, entry * data_ptr, doorbell * db_ptr) {
    return gdakis::umem::mem<gdakis::umem::cuda, _Ab>::alloc([n_entries, data_ptr, db_ptr]
        __host__ __device__(std::size_t i) { return buffer(n_entries, data_ptr + (n_entries * i), db_ptr + i); });
}

TYPED_TEST(CUDAAutobatchTest, Create) {
    const unsigned n_entries = 512;
    auto data = gdakis::umem::mem<gdakis::umem::cuda, entry[]>::alloc(TypeParam::number_of_buffers() * n_entries);
    ASSERT_NE(data, nullptr);
    auto db = gdakis::umem::mem<gdakis::umem::cuda, doorbell[]>::alloc_system(TypeParam::number_of_buffers());
    ASSERT_NE(db, nullptr);

    auto data_ptr = data.get();
    auto db_ptr = db.get();
    auto autobatch = build_device_autobatch<TypeParam>(n_entries, data.get(), db.get());
    ASSERT_NE(autobatch, nullptr);
}

template<typename _Ab>
__global__ static void cuda_autobatch_push_test_kernel(_Ab * queue, unsigned length) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= length)
        return;

    auto group = cooperative_groups::coalesced_threads();
    queue->push(group, [](entry & e) {
        e.id_ = 1;
        e.value_ = 0xcafe;
    });
}

TYPED_TEST(CUDAAutobatchTest, Push) {
    const unsigned n_entries = 512;
    auto data
        = gdakis::umem::mem<gdakis::umem::cuda, entry[]>::alloc_dma_buffer(TypeParam::number_of_buffers() * n_entries);
    ASSERT_NE(data, nullptr);
    auto db = gdakis::umem::mem<gdakis::umem::cuda, doorbell[]>::alloc_system(TypeParam::number_of_buffers());
    ::memset(db.get(), 0, TypeParam::number_of_buffers() * sizeof(doorbell));
    ASSERT_NE(db, nullptr);

    cudaError_t cuda_ret;
    cudaStream_t cuda_stream;
    cuda_ret = cudaStreamCreate(&cuda_stream);
    ASSERT_EQ(cuda_ret, cudaSuccess);
    std::vector<entry> host_entries(n_entries);

    std::atomic<unsigned> db_count(0);
    gdakis::barrier control(TypeParam::number_of_buffers() + 1);
    std::vector<std::thread> ctrl;
    for (auto n = 0; n < TypeParam::number_of_buffers(); ++n) {
        ctrl.emplace_back([&, n] {
            control.arrive_and_wait();
            db.get()[n].ack([&](int value) {
                const void * device_addr = reinterpret_cast<void *>(data.get() + n * n_entries);
                auto cuda_ret = cudaMemcpyAsync(
                    host_entries.data(), device_addr, value * sizeof(entry), cudaMemcpyDeviceToHost, cuda_stream);
                ASSERT_EQ(cuda_ret, cudaSuccess);
                cuda_ret = cudaStreamSynchronize(cuda_stream);
                ASSERT_EQ(cuda_ret, cudaSuccess);

                for (unsigned m = 0; m < value; ++m) {
                    EXPECT_EQ(host_entries[m].id_, 1);
                    EXPECT_EQ(host_entries[m].value_, 0xcafe);
                }
                db_count.fetch_add(value, std::memory_order_release);
            });
        });
    }
    control.arrive_and_wait();

    auto autobatch = build_device_autobatch<TypeParam>(n_entries, data.get(), db.get());
    ASSERT_NE(autobatch, nullptr);

    const unsigned n_threads = n_entries * TypeParam::number_of_buffers();
    dim3 block_dim(n_threads < 128 ? n_threads : 128);
    dim3 grid_dim((n_threads + block_dim.x - 1) / block_dim.x);
    cuda_autobatch_push_test_kernel<<<grid_dim, block_dim>>>(autobatch.get(), n_threads);
    ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

    for (auto n = 0; n < TypeParam::number_of_buffers(); n++) {
        db.get()[n].cancel();
    }

    for (auto & thread : ctrl)
        thread.join();
    EXPECT_EQ(db_count.load(std::memory_order_acquire), n_threads);
}

#endif
