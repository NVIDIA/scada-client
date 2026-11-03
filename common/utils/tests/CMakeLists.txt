# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(../../../cmake/TestCudaArchitectures.cmake)

CPMAddPackage(
    NAME psa-result
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../result
    OPTIONS "RESULT_BUILD_TESTS FALSE" "RESULT_BUILD_DOCS FALSE"
)

include(../../../cmake/FetchGoogleTest.cmake)

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set_source_files_properties(bits.cpp PROPERTIES LANGUAGE CUDA)
    set_source_files_properties(bitmap.cpp PROPERTIES LANGUAGE CUDA)
    set_source_files_properties(seqlock.cpp PROPERTIES LANGUAGE CUDA)
    set_source_files_properties(spinlock.cpp PROPERTIES LANGUAGE CUDA)
endif()

add_library(gdakis-utils-test INTERFACE)
target_include_directories(gdakis-utils-test INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gdakis-utils-test INTERFACE gdakis-utils PSA::result GTest::gtest_main)

add_executable(barrier barrier.cpp)
target_link_libraries(barrier gdakis-utils-test)

add_executable(spinbar spinbar.cpp)
target_link_libraries(spinbar gdakis-utils-test)

add_executable(mpbar mpbar.cpp)
target_link_libraries(mpbar gdakis-utils-test)

add_executable(bits bits.cpp)
target_link_libraries(bits gdakis-utils-test)

add_executable(bitmap bitmap.cpp)
target_compile_options(
    bitmap
    PRIVATE
        # Allow variable length arrays compiler extension.
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,18.0>>:-Wno-vla-cxx-extension>
)
target_link_libraries(bitmap gdakis-utils-test)

add_executable(seqlock seqlock.cpp)
target_link_libraries(seqlock gdakis-utils-test)

add_executable(spinlock spinlock.cpp)
target_link_libraries(spinlock gdakis-utils-test)

# Add new CUDA-specific test for cooperative groups functionality
if(CMAKE_CUDA_COMPILER)
    add_executable(seqlock_cg seqlock.cu)
    target_link_libraries(seqlock_cg gdakis-utils-test)

    add_executable(spinlock_cg spinlock.cu)
    target_link_libraries(spinlock_cg gdakis-utils-test)

    add_executable(bitmap_cg bitmap.cu)
    target_link_libraries(bitmap_cg gdakis-utils-test)
endif()

include(GoogleTest)
gtest_discover_tests(barrier)
gtest_discover_tests(spinbar)
gtest_discover_tests(mpbar)
gtest_discover_tests(bits)
gtest_discover_tests(bitmap)
gtest_discover_tests(seqlock)
gtest_discover_tests(spinlock)

# Add test discovery for the new CG test
if(CMAKE_CUDA_COMPILER)
    gtest_discover_tests(seqlock_cg)
    gtest_discover_tests(spinlock_cg)
    gtest_discover_tests(bitmap_cg)
endif()
