#[[
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
]]

# Keep perf configuration resilient when the optional benchmark module is unavailable.
find_file(CBF_TEST_MODULE CBFTest.cmake PATHS ${CMAKE_MODULE_PATH})
if(CBF_TEST_MODULE)
    include(CBFTest)
else()
    message(STATUS "CBFTest not found, skipping CBF benchmark setup")
endif()
