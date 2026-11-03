# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(../../../cmake/TestCudaArchitectures.cmake)

include(../../../cmake/FetchGoogleTest.cmake)
add_executable(psa-result-test psa-result.cpp)
target_link_libraries(psa-result-test PSA::result GTest::gtest_main)

add_executable(psa-futures-result-async-test psa-futures-result.cpp)
target_link_libraries(psa-futures-result-async-test PSA::result GTest::gtest_main)
target_compile_features(psa-futures-result-async-test PRIVATE cxx_std_20)
target_compile_options(psa-futures-result-async-test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fcoroutines>)

include(GoogleTest)
gtest_discover_tests(psa-result-test)
gtest_discover_tests(psa-futures-result-async-test)

# NVCC Tests

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)

    file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/psa-result.cpp ${CMAKE_CURRENT_BINARY_DIR}/psa-result.cu)
    add_executable(nvcc-psa-result-test ${CMAKE_CURRENT_BINARY_DIR}/psa-result.cu)
    target_link_libraries(nvcc-psa-result-test psa-result GTest::gtest_main)
    gtest_discover_tests(nvcc-psa-result-test)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(
            STATUS
                "Disabling NVCC C++20 Tests due to known bugs when using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
        )
    else()
        file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/psa-futures-result.cpp
             ${CMAKE_CURRENT_BINARY_DIR}/psa-futures-result.cu
        )
        add_executable(nvcc-psa-futures-result-async-test ${CMAKE_CURRENT_BINARY_DIR}/psa-futures-result.cu)
        target_link_libraries(nvcc-psa-futures-result-async-test psa-result GTest::gtest_main)
        target_include_directories(nvcc-psa-futures-result-async-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_definitions(nvcc-psa-futures-result-async-test PRIVATE -DFORCE_NVCC_COROUTINES)
        target_compile_features(nvcc-psa-futures-result-async-test PRIVATE cxx_std_20)
        target_compile_options(
            nvcc-psa-futures-result-async-test
            PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fcoroutines>
                    # Use of lambdas in map() calls instantiate types dependent on
                    # types with no linkage (the lambda).
                    # This could cause an ODR violation, but not in this case of a
                    # single test compilation unit.
                    $<$<CXX_COMPILER_ID:Clang>:-Wno-subobject-linkage>
        )
        gtest_discover_tests(nvcc-psa-futures-result-async-test)
    endif()

endif()
