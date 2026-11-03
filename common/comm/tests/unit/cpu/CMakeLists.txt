# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_executable(unit_tests_cpu_autobatch autobatchCPUUnitTests.cpp)
target_link_libraries(unit_tests_cpu_autobatch tests gdakis-comm GTest::gtest_main)

add_executable(unit_tests_cpu_protocol protocolCPUUnitTests.cpp)
target_link_libraries(unit_tests_cpu_protocol tests gdakis-comm GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests_cpu_autobatch)
gtest_discover_tests(unit_tests_cpu_protocol)
