/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/atomic>
#include <gdakis/bitmap>
#include <gtest/gtest.h>

template<typename _Tp>
struct Bitmap : testing::Test {
    using type = gdakis::utils::embedded_bitmap<_Tp>;
};

using test_types = testing::Types<std::uint32_t, std::uint64_t>;

TYPED_TEST_SUITE(Bitmap, test_types);

static std::size_t size_set[] = { 32, 64, 128, 1024, 8192 };

TYPED_TEST(Bitmap, SetAndTestAndClear) {
    using bitmap_type = typename Bitmap<TypeParam>::type;
    for (const auto & size : size_set) {
        typename bitmap_type::word bitmap[bitmap_type::word_count(size)];
        ASSERT_EQ(bitmap_type::memory_size(size), sizeof(bitmap));
        ::memset(bitmap, 0, bitmap_type::memory_size(size));
        for (auto n = 0ul; n < size; ++n)
            ASSERT_FALSE(bitmap_type::check(bitmap, n, gdakis::memory_order_relaxed));
        for (auto n = 0ul; n < size; ++n) {
            auto set = bitmap_type::set(bitmap, n, gdakis::memory_order_release);
            ASSERT_TRUE(set);
            for (auto m = 0ul; m < n + 1; ++m)
                ASSERT_TRUE(bitmap_type::check(bitmap, m, gdakis::memory_order_acquire));
            for (auto m = n + 1; m < size; ++m)
                ASSERT_FALSE(bitmap_type::check(bitmap, m, gdakis::memory_order_acquire));
        }

        for (auto n = 0ul; n < size; ++n) {
            auto clear = bitmap_type::clear(bitmap, n, gdakis::memory_order_release);
            ASSERT_TRUE(clear);
            for (auto m = 0ul; m < n + 1; ++m)
                ASSERT_FALSE(bitmap_type::check(bitmap, m, gdakis::memory_order_acquire));
            for (auto m = n + 1; m < size; ++m)
                ASSERT_TRUE(bitmap_type::check(bitmap, m, gdakis::memory_order_acquire));
        }
    }
}

TYPED_TEST(Bitmap, SetManyOffset) {
    using bitmap_type = typename Bitmap<TypeParam>::type;
    typename bitmap_type::word bitmap[bitmap_type::word_count(4096)];
    auto bit_count = sizeof(typename bitmap_type::word) << 2;
    for (auto n = 0ul; n < bitmap_type::word_count(4096) - bit_count; ++n) {
        ::memset(bitmap, 0, bitmap_type::memory_size(4096));
        auto ret = bitmap_type::set(bitmap, n, bit_count, gdakis::memory_order_relaxed);
        ASSERT_EQ(ret, 0);
    }
}

TYPED_TEST(Bitmap, SetManyOver) {
    using bitmap_type = typename Bitmap<TypeParam>::type;
    typename bitmap_type::word bitmap[bitmap_type::word_count(4096)];
    auto bit_count = sizeof(typename bitmap_type::word) << 2;

    ::memset(bitmap, 0, bitmap_type::memory_size(4096));
    auto ret = bitmap_type::set(bitmap, 0, bit_count, gdakis::memory_order_relaxed);
    ASSERT_EQ(ret, 0);

    for (auto n = 1ul; n < 4096 - bit_count; ++n) {
        auto ret = bitmap_type::set(bitmap, n, bit_count, gdakis::memory_order_relaxed);
        ASSERT_EQ(ret, bit_count - 1);
    }
}

TYPED_TEST(Bitmap, SetAndClearMany) {
    using bitmap_type = typename Bitmap<TypeParam>::type;
    typename bitmap_type::word bitmap[bitmap_type::word_count(4096)];
    auto bit_count = sizeof(typename bitmap_type::word) << 2;
    for (auto n = 0ul; n < bitmap_type::word_count(4096) - bit_count; ++n) {
        ::memset(bitmap, 0, bitmap_type::memory_size(4096));
        auto ret = bitmap_type::set(bitmap, n, bit_count, gdakis::memory_order_relaxed);
        EXPECT_EQ(ret, 0);

        auto set = bitmap_type::check(bitmap, n, bit_count, gdakis::memory_order_relaxed);
        ASSERT_TRUE(set);

        ret = bitmap_type::clear(bitmap, n, bit_count, gdakis::memory_order_relaxed);
        EXPECT_EQ(ret, bit_count);

        for (auto m = 0ul; m < bit_count; ++m) {
            EXPECT_FALSE(bitmap_type::check(bitmap, n + m, gdakis::memory_order_relaxed));
        }
    }
}

template<typename _Tp>
struct AtomicBitmap : testing::Test {
    using type = gdakis::utils::atomic_bitmap<_Tp>;
};

TYPED_TEST_SUITE(AtomicBitmap, test_types);

TYPED_TEST(AtomicBitmap, SetAndTestAndClear) {
    using bitmap_type = typename AtomicBitmap<TypeParam>::type;
    bitmap_type bitmap;
    auto size = sizeof(TypeParam) << 3;

    for (auto n = 0ul; n < size; ++n)
        ASSERT_FALSE(bitmap.get(n, gdakis::memory_order_relaxed));

    for (auto n = 0ul; n < size; ++n) {
        auto set = bitmap.set(n, gdakis::memory_order_release);
        ASSERT_TRUE(set);
        for (auto m = 0ul; m < n + 1; ++m)
            ASSERT_TRUE(bitmap.get(m, gdakis::memory_order_acquire));
        for (auto m = n + 1; m < size; ++m)
            ASSERT_FALSE(bitmap.get(m, gdakis::memory_order_acquire));
    }

    for (auto n = 0ul; n < size; ++n) {
        auto clear = bitmap.clear(n, gdakis::memory_order_release);
        ASSERT_TRUE(clear);
        for (auto m = 0ul; m < n + 1; ++m)
            ASSERT_FALSE(bitmap.get(m, gdakis::memory_order_acquire));
        for (auto m = n + 1; m < size; ++m)
            ASSERT_TRUE(bitmap.get(m, gdakis::memory_order_acquire));
    }
}

#if defined(__CUDACC__)

#  include <cuda/atomic>

#  include <thrust/device_vector.h>
#  include <thrust/host_vector.h>

class CUDATest : public ::testing::Test {
protected:
    void SetUp() override {
        int gpu_count = 0;
        if (cudaGetDeviceCount(&gpu_count) != cudaSuccess or gpu_count <= 0) {
            GTEST_SKIP() << "No CUDA capable GPUs present. Skipping GPU tests";
        }
    }
};

template<typename _Tp>
struct CUDABitmap : CUDATest {
    using type = gdakis::utils::embedded_bitmap<_Tp>;
    static constexpr unsigned block_size = 128;
};

TYPED_TEST_SUITE(CUDABitmap, test_types);

static std::size_t gpu_size_set[] = { 32, 64, 128, 1024, 8192, 64 * 1024, 512 * 1024 };

template<typename bitmap_type>
__global__ static void bitmap_set_and_test_and_clear_kernel(
    size_t size, typename bitmap_type::word * bitmap, cuda::atomic<unsigned, cuda::thread_scope_device> * out) {
    auto id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;

    if (bitmap_type::check(bitmap, id, gdakis::memory_order_relaxed) == false) {
        out[0].fetch_add(1, cuda::memory_order_relaxed);
    }

    if (bitmap_type::set(bitmap, id, gdakis::memory_order_release)) {
        out[1].fetch_add(1, cuda::memory_order_relaxed);
    }
    if (bitmap_type::check(bitmap, id, gdakis::memory_order_relaxed)) {
        out[0].fetch_add(1, cuda::memory_order_relaxed);
    }

    if (bitmap_type::clear(bitmap, id, gdakis::memory_order_release)) {
        out[2].fetch_add(1, cuda::memory_order_relaxed);
    }
    if (bitmap_type::check(bitmap, id, gdakis::memory_order_relaxed) == false) {
        out[0].fetch_add(1, cuda::memory_order_relaxed);
    }
}

TYPED_TEST(CUDABitmap, SetAndTestAndClear) {
    using bitmap_type = typename CUDABitmap<TypeParam>::type;
    for (const auto & size : gpu_size_set) {
        typename bitmap_type::word * bitmap;
        auto cuda_ret = cudaMalloc(&bitmap, bitmap_type::memory_size(size));
        ASSERT_EQ(cuda_ret, cudaSuccess);

        cuda_ret = cudaMemset(bitmap, 0, bitmap_type::memory_size(size));
        ASSERT_EQ(cuda_ret, cudaSuccess);

        dim3 db = size < CUDABitmap<TypeParam>::block_size ? size : CUDABitmap<TypeParam>::block_size;
        dim3 dg = (size + db.x - 1) / db.x;

        thrust::device_vector<unsigned> out(3, 0);
        bitmap_set_and_test_and_clear_kernel<bitmap_type><<<dg, db>>>(
            size, bitmap, reinterpret_cast<cuda::atomic<unsigned, cuda::thread_scope_device> *>(out.data().get()));
        thrust::host_vector<unsigned> ret(out);
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

        EXPECT_EQ(ret[0], 3 * size);
        EXPECT_EQ(ret[1], size);
        EXPECT_EQ(ret[2], size);

        EXPECT_EQ(cudaFree(bitmap), cudaSuccess);
    }
}

#endif
