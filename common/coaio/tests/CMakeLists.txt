# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(../../../cmake/TestCudaArchitectures.cmake)

include(../../../cmake/FetchGoogleTest.cmake)

add_executable(epoll-sync epoll-sync.cpp)
target_link_libraries(epoll-sync psa-coaio-sync GTest::gtest_main)

add_executable(epoll epoll.cpp)
target_link_libraries(epoll psa-coaio-async GTest::gtest_main)

add_executable(socket socket.cpp)
target_include_directories(socket PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(socket psa-coaio-async GTest::gtest_main)

add_executable(message message.cpp)
target_compile_features(message PRIVATE cxx_std_20)
target_include_directories(message PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(message psa-coaio-async GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(epoll-sync)
gtest_discover_tests(epoll)
gtest_discover_tests(socket)
gtest_discover_tests(message)

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)

    # TODO: Uncomment once NVCC handles std::variant properly when compiling C++20
    file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/epoll-sync.cpp ${CMAKE_CURRENT_BINARY_DIR}/epoll-sync.cu)
    add_executable(nvcc-epoll-sync ${CMAKE_CURRENT_BINARY_DIR}/epoll-sync.cu)
    target_link_libraries(nvcc-epoll-sync psa-coaio-sync GTest::gtest_main)
    gtest_discover_tests(nvcc-epoll-sync)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(
            STATUS
                "Disabling NVCC C++20 Tests due to known bugs when using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
        )
    else()

        file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/epoll.cpp ${CMAKE_CURRENT_BINARY_DIR}/epoll.cu)
        add_executable(nvcc-epoll ${CMAKE_CURRENT_BINARY_DIR}/epoll.cu)
        target_compile_definitions(nvcc-epoll PRIVATE -DFORCE_NVCC_COROUTINES)
        target_compile_options(
            nvcc-epoll
            PRIVATE # Use of lambdas in await() calls instantiate types dependent on
                    # types with no linkage (the lambda).
                    # This could cause an ODR violation, but not in this case of a
                    # single test compilation unit.
                    $<$<CXX_COMPILER_ID:Clang>:-Wno-subobject-linkage>
        )
        target_link_libraries(nvcc-epoll psa-coaio-async GTest::gtest_main)
        gtest_discover_tests(nvcc-epoll)

        if(TRUE)
            message(STATUS "Disabled NVCC asynchronous socket tests due to known bugs on NVCC")
        else()
            file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/socket.cpp ${CMAKE_CURRENT_BINARY_DIR}/socket.cu)
            add_executable(nvcc-socket ${CMAKE_CURRENT_BINARY_DIR}/socket.cu)
            target_include_directories(nvcc-socket PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
            target_compile_definitions(nvcc-socket PRIVATE -DFORCE_NVCC_COROUTINES)
            target_link_libraries(nvcc-socket psa-coaio-async GTest::gtest_main)
            gtest_discover_tests(nvcc-socket)
        endif()
    endif()
endif()
