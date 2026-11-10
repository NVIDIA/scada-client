# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_compile_options(-Wall -Wextra -Wno-unknown-pragmas -Wno-unused-parameter)

option(SCADA_CXX_WARNINGS_AS_ERRORS "Treat C++ compiler warnings as errors" ON)
if(SCADA_CXX_WARNINGS_AS_ERRORS)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Werror>)
endif()

option(SCADA_CUDA_WARNINGS_AS_ERRORS "Treat CUDA compiler warnings as errors" ON)
if(SCADA_CUDA_WARNINGS_AS_ERRORS)
    add_compile_options($<$<COMPILE_LANGUAGE:CUDA>:-Werror=all-warnings>)
endif()

# Host-side C++ coverage instrumentation.
# Only applied to CXX targets — CUDA (nvcc) does not support --coverage and
# would produce a build error if this flag were passed to the device compiler.
option(SCADA_ENABLE_COVERAGE "Enable host-side C++ coverage instrumentation (GCC/Clang --coverage)" OFF)
if(SCADA_ENABLE_COVERAGE)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:--coverage>)
    add_link_options($<$<COMPILE_LANGUAGE:CXX>:--coverage>)
endif()
