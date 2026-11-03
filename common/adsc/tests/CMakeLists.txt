# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(../../../cmake/TestCudaArchitectures.cmake)

include(../../../cmake/FetchGoogleTest.cmake)

add_library(tests INTERFACE)
target_include_directories(tests INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

add_executable(cl_reference cl_reference.cpp)
target_link_libraries(cl_reference gdakis-adsc GTest::gtest_main)

add_executable(policies policies.cpp)
target_link_libraries(policies gdakis-adsc GTest::gtest_main)

add_executable(cache cache.cpp)
target_link_libraries(cache gdakis-adsc GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(cache)
gtest_discover_tests(cl_reference)
gtest_discover_tests(policies)

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    add_executable(cache-cuda cache.cu)
    target_link_libraries(cache-cuda gdakis-adsc GTest::gtest_main)

    # Seven of these cases build a cache of num_cls * cl_size val_type, which is
    # 4 GiB of device memory each. Under `ctest -j$(nproc)` they all start at once
    # and ask for 28 GiB, against the 22.5 GiB of the A10G the CI gpu tier leases.
    # Whichever loses the race gets cudaErrorMemoryAllocation, and because the
    # allocation helpers check with EXPECT rather than ASSERT the case carries on
    # with a null pointer and reports cudaErrorIllegalAddress instead.
    #
    # A shared lock serialises this target and leaves the rest of the suite
    # parallel. It costs about 90 seconds. Sizing the cases to the card would be
    # the better fix and is not one to make from the CI side. SCADA-379.
    gtest_discover_tests(cache-cuda PROPERTIES RESOURCE_LOCK gpu_memory)
endif()
