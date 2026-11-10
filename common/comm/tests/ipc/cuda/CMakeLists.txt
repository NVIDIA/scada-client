# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)

    add_executable(tests-cuda-barrier barrier.cu)
    target_link_libraries(tests-cuda-barrier tests-sync gdakis-comm GTest::gtest_main)

    add_executable(tests-cuda-shmem shmem.cu)
    target_compile_options(
        tests-cuda-shmem PRIVATE # Suppress calling host function from host/device code, used in result class.
                                 $<$<COMPILE_LANGUAGE:CUDA>:-diag-suppress 20011>
    )
    target_link_libraries(tests-cuda-shmem tests-sync gdakis-comm GTest::gtest_main)

    add_executable(tests-cuda-autobatch autobatch.cu)
    target_link_libraries(tests-cuda-autobatch tests-sync gdakis-comm GTest::gtest_main)

    add_executable(tests-cuda-memory-buffer memory-buffer.cu)
    target_compile_definitions(tests-cuda-memory-buffer PRIVATE -DFORCE_NVCC_COROUTINES)
    target_compile_options(
        tests-cuda-memory-buffer PRIVATE # Suppress calling host function from host/device code, used in result class.
                                         $<$<COMPILE_LANGUAGE:CUDA>:-diag-suppress 20011>
    )
    target_link_libraries(tests-cuda-memory-buffer tests-sync gdakis-comm GTest::gtest_main)

    add_executable(tests-cuda-io-buffer-pool io-buffer-pool.cu)
    target_compile_definitions(tests-cuda-io-buffer-pool PRIVATE -DFORCE_NVCC_COROUTINES)
    target_compile_options(
        tests-cuda-io-buffer-pool PRIVATE # Suppress calling host function from host/device code, used in result class.
                                          $<$<COMPILE_LANGUAGE:CUDA>:-diag-suppress 20011>
    )
    target_link_libraries(tests-cuda-io-buffer-pool tests-sync gdakis-comm GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(tests-cuda-barrier)
    gtest_discover_tests(tests-cuda-shmem)
    gtest_discover_tests(tests-cuda-autobatch)
    gtest_discover_tests(tests-cuda-memory-buffer)
    gtest_discover_tests(tests-cuda-io-buffer-pool)
endif()
