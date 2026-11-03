# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(../../../cmake/TestCudaArchitectures.cmake)

include(../../../cmake/FetchGoogleTest.cmake)

CPMAddPackage(
    NAME psa-coaio
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../coaio
    OPTIONS "COAIO_BUILD_TESTS FALSE" "COAIO_BUILD_DOCS FALSE"
)

add_library(tests INTERFACE)
target_include_directories(tests INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(tests INTERFACE PSA::coaio-async)

add_library(tests-sync INTERFACE)
target_include_directories(tests-sync INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(tests-sync INTERFACE PSA::coaio-sync)

add_executable(autobatch autobatch.cpp)
target_compile_features(autobatch PRIVATE cxx_std_20)
target_link_libraries(autobatch tests-sync gdakis-comm GTest::gtest_main)

add_subdirectory(ipc)
add_subdirectory(unit)

include(GoogleTest)
gtest_discover_tests(autobatch)
