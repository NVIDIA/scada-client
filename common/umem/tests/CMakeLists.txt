# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(../../../cmake/TestCudaArchitectures.cmake)

include(../../../cmake/FetchGoogleTest.cmake)

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set_source_files_properties(dma.cpp PROPERTIES LANGUAGE CUDA)
endif()

add_executable(mem-test mem.cpp)
target_link_libraries(mem-test gdakis-umem GTest::gtest_main)

add_executable(dma-test dma.cpp)
target_link_libraries(dma-test gdakis-umem GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mem-test)
gtest_discover_tests(dma-test)
