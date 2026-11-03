# SPDX-FileCopyrightText: Copyright (c) 2023-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.18)

macro(define_test name files)
    cmake_parse_arguments(test_${name} "" "" "LIBRARIES" ${ARGN})
    set(exclude_list "")
    if(NOT CMAKE_CUDA_COMPILER)
        set(exclude_list ${files})
        list(FILTER exclude_list INCLUDE REGEX "\\.cu$")
        if(exclude_list)
            message(STATUS "Skipping test ${name} because CUDA is not enabled")
        endif()
    endif()
    # If no files exclude the test, we go ahead and compile it
    if(NOT exclude_list)
        add_executable(${name} ${files})
        if(CMAKE_CUDA_COMPILER)
            set_property(TARGET ${name} PROPERTY CUDA_SEPARABLE_COMPILATION ON)
        endif()
        if(test_${name}_LIBRARIES)
            target_link_libraries(${name} PRIVATE ${test_${name}_LIBRARIES})
        endif()
    endif()
endmacro()
