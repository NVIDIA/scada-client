/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PROFILER_METRICS_CYCLES_HPP
#define PROFILER_METRICS_CYCLES_HPP
#include "base.hpp"
#include <cuda_runtime.h>

namespace profiler::metrics {
class Cycles : public Base<Cycles> {
    clock_t * dst_ { 0 };

public:
    __device__ __host__ void setDst(clock_t * dst) { dst_ = dst; }

    __device__ __host__ inline void resetImpl() {
        //noop
    }

    __device__ __host__ inline clock_t startImpl() { return clock(); }

    __device__ __host__ inline void endImpl(clock_t start_time) {
        auto end_time = clock();
        auto cycles = end_time - start_time;
#if defined(__CUDA_ARCH__)
        dst_[threadIdx.x + blockIdx.x * blockDim.x] = cycles;
#else
        *dst_ = cycles;
#endif
    }
};
}
#endif
