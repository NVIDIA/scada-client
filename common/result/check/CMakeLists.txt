#[[
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
]]

add_executable(sync-future sync-future.cpp)
target_link_libraries(sync-future psa-result)
target_compile_features(sync-future PRIVATE cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    target_compile_options(sync-future PRIVATE -fcoroutines)
endif()
