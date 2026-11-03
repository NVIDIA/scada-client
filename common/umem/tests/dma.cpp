/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/umem/cpu>
#include <gdakis/umem/cuda>
#include <gdakis/umem/mem>
#include <gtest/gtest.h>

class CPUDMATest : public ::testing::Test {
protected:
    void SetUp() override {
        if (gdakis::umem::cpu::supports_dma_buf_from_driver() == false) {
            GTEST_SKIP() << "CPU driver does not support dma-buf";
        }
    }
};

TEST_F(CPUDMATest, Alloc) {
    auto buffer = gdakis::umem::mem<gdakis::umem::cpu, unsigned[]>::alloc_dma_buffer(1024);
    ASSERT_EQ(buffer.retval(), 0);
    ASSERT_GE(buffer.handle(), 0);
    ASSERT_NE(buffer.get(), nullptr);
}

#if defined(__CUDACC__)

class CUDATest : public ::testing::Test {
protected:
    void SetUp() override {
        int gpu_count = 0;
        if (cudaGetDeviceCount(&gpu_count) != cudaSuccess or gpu_count <= 0) {
            GTEST_SKIP() << "No CUDA capable GPUs present. Skipping GPU tests";
        }
        if (gdakis::umem::cuda::supports_dma_buf_from_driver() == false) {
            GTEST_SKIP() << "CUDA driver does not support dma-buf";
        }
    }
};

class CUDAUDMATest : public CUDATest {};

TEST_F(CUDAUDMATest, Alloc) {
    cudaError_t err = cudaSetDevice(0);
    ASSERT_EQ(err, CUDA_SUCCESS);

    auto buffer = gdakis::umem::mem<gdakis::umem::cuda, unsigned[]>::alloc_dma_buffer(1024);
    ASSERT_EQ(buffer.retval(), 0);
    ASSERT_GE(buffer.handle(), 0);
    ASSERT_NE(buffer.get(), nullptr);
}

#endif
