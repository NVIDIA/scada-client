# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

CPMAddPackage(
    NAME psa-result
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../result
    OPTIONS "RESULT_BUILD_TESTS FALSE" "RESULT_BUILD_DOCS FALSE"
)

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)

    add_executable(atomic-scaling atomic-scaling.cu)
    target_link_libraries(atomic-scaling gdakis-utils psa-result)
endif()
