# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(../../../cmake/FetchGoogleTest.cmake)
add_executable(uuid uuid.cpp)

target_link_libraries(uuid ${PROJECT_NAME} GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(uuid)
