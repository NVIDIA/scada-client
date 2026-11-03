#[[
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
]]
include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
endif()

# Add the new profiling framework
add_library(profiler INTERFACE)
target_include_directories(profiler INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Benchmarks
if(CMAKE_CUDA_COMPILER)
    add_executable(timing_bench bench/timing_bench.cu)
    target_include_directories(timing_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(timing_bench profiler gdakis-comm PSA::result)
    set_property(TARGET timing_bench PROPERTY CXX_STANDARD 20)
    set_property(TARGET timing_bench PROPERTY CUDA_STANDARD 20)
    # More benchmarks will be added as they are implemented
endif()
# Benchmarks will be added as they are implemented
# For now, we're just setting up the structure
