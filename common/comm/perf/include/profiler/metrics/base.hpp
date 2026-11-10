/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PROFILER_METRICS_BASE_HPP
#define PROFILER_METRICS_BASE_HPP

namespace profiler::metrics {

/**
 * @brief Base class for metrics collectors using CRTP
 * 
 * This class provides the interface for metrics collectors using
 * the Curiously Recurring Template Pattern (CRTP) for static polymorphism.
 * 
 * @tparam Derived The derived metrics collector type
 */
template<typename _Derived>
class Base {
public:
    /**
     * @brief Start timing an operation
     * 
     * @return A token representing the start time
     */
    __device__ __host__ inline auto start() { return static_cast<_Derived *>(this)->startImpl(); }

    /**
     * @brief End timing an operation
     * 
     * @param token The token returned from start()
     */
    __device__ __host__ inline void end(auto token) { static_cast<_Derived *>(this)->endImpl(token); }

    /**
     * @brief Reset all metrics
     */
    __device__ __host__ inline void reset() { static_cast<_Derived *>(this)->resetImpl(); }
};

/**
 * @brief Null metrics collector that does nothing
 * 
 * This class can be used when profiling is disabled.
 */
class NoMetrics : public Base<NoMetrics> {
public:
    __device__ __host__ inline auto startImpl() { return 0; }
    __device__ __host__ inline void endImpl(auto) {}
    __device__ __host__ inline void resetImpl() {}
};

} // namespace profiler::metrics

#endif // PROFILER_METRICS_BASE_HPP