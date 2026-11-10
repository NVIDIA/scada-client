#[[
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
]]
include(DefineTest)

define_test(alloc alloc.cpp LIBRARIES adsc)

if(CMAKE_CUDA_COMPILER)
    define_test(cu-alloc alloc.cu LIBRARIES adsc)
endif()
