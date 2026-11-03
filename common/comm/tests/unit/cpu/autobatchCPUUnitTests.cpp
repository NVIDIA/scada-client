/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/atomic>
#include <gdakis/autobatch>
#include <gdakis/umem/cpu>
#include <gdakis/umem/mem>
#include <gtest/gtest.h>
#include <iostream>

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

TEST(AutobatchTest, Constructor) {
    // Test the constructor with a basic functor
    auto data = gdakis::umem::mem<gdakis::umem::cpu, entry[]>::alloc(1);
    ASSERT_NE(data, nullptr);
    auto db = gdakis::umem::mem<gdakis::umem::cpu, doorbell[]>::alloc(1);
    ASSERT_NE(db, nullptr);
    auto fn = [&](std::size_t i) { return buffer(i, data.get(), db.get()); };
    gdakis::comm::autobatch<buffer, gdakis::thread_scope_system, 1> autobatch_instance(fn);
}

TEST(AutobatchTest, NumberOfBuffers) {
    constexpr size_t numBuffers = 16;
    auto data = gdakis::umem::mem<gdakis::umem::cpu, entry[]>::alloc(1);
    ASSERT_NE(data, nullptr);
    auto db = gdakis::umem::mem<gdakis::umem::cpu, doorbell[]>::alloc(1);
    ASSERT_NE(db, nullptr);
    auto fn = [&](std::size_t i) { return buffer(i, data.get(), db.get()); };
    gdakis::comm::autobatch<buffer, gdakis::thread_scope_system, numBuffers> autobatch_instance(fn);
    ASSERT_EQ(autobatch_instance.number_of_buffers(), numBuffers);
}

template<typename _Ab>
class AutobatchTestNumBuffers : public ::testing::Test {};
template<int _V>
using autobatch_test_base_type = gdakis::comm::autobatch<buffer, gdakis::thread_scope_system, _V>;
using autobatch_test_types = ::testing::Types<autobatch_test_base_type<2>, autobatch_test_base_type<4>,
    autobatch_test_base_type<8>, autobatch_test_base_type<16>, autobatch_test_base_type<32>,
    autobatch_test_base_type<64>, autobatch_test_base_type<128>, autobatch_test_base_type<256>>;
TYPED_TEST_SUITE(AutobatchTestNumBuffers, autobatch_test_types);

TYPED_TEST(AutobatchTestNumBuffers, Constructor) {
    // Test the constructor with a basic functor
    auto data = gdakis::umem::mem<gdakis::umem::cpu, entry[]>::alloc(1);
    ASSERT_NE(data, nullptr);
    auto db = gdakis::umem::mem<gdakis::umem::cpu, doorbell[]>::alloc(1);
    ASSERT_NE(db, nullptr);
    auto fn = [&](std::size_t i) { return buffer(i, data.get(), db.get()); };
    TypeParam autobatch_instance(fn);
}
