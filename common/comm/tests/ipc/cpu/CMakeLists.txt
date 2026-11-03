# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_executable(tests-cpu-shmem shmem.cpp)
target_link_libraries(tests-cpu-shmem tests gdakis-comm GTest::gtest_main)

add_executable(tests-cpu-autobatch autobatch.cpp)
target_link_libraries(tests-cpu-autobatch tests gdakis-comm GTest::gtest_main)

add_executable(tests-cpu-memory-buffer memory-buffer.cpp)
target_link_libraries(tests-cpu-memory-buffer tests gdakis-comm GTest::gtest_main)

add_executable(tests-cpu-io-buffer-pool io-buffer-pool.cpp)
target_link_libraries(tests-cpu-io-buffer-pool tests gdakis-comm GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tests-cpu-shmem)
gtest_discover_tests(tests-cpu-autobatch)
gtest_discover_tests(tests-cpu-memory-buffer)
gtest_discover_tests(tests-cpu-io-buffer-pool)
