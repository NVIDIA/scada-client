# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Include before enabling CUDA. Explicit cache, parent, environment and toolchain
# choices take precedence. Headless builds retain CMake's compiler default;
# reproducible CI and distribution builds should specify their target list.
if(DEFINED CMAKE_CUDA_ARCHITECTURES OR DEFINED ENV{CUDAARCHS})
    return()
endif()

# A toolchain may not have been loaded yet (before project()). Never guess its
# target from a GPU attached to the build host.
if(CMAKE_TOOLCHAIN_FILE
   OR CMAKE_CROSSCOMPILING
   OR (CMAKE_SYSTEM_NAME AND NOT CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
)
    return()
endif()

find_program(SCADA_NVIDIA_SMI nvidia-smi)
if(SCADA_NVIDIA_SMI)
    execute_process(
        COMMAND "${SCADA_NVIDIA_SMI}" --query-gpu=compute_cap --format=csv,noheader
        RESULT_VARIABLE _scada_smi_result
        OUTPUT_VARIABLE _scada_smi_output
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
        TIMEOUT 5
    )
    if(_scada_smi_result STREQUAL "0" AND _scada_smi_output MATCHES "^[0-9]+\\.[0-9]+(\n[0-9]+\\.[0-9]+)*$")
        set(CMAKE_CUDA_ARCHITECTURES native)
    endif()
    unset(_scada_smi_result)
    unset(_scada_smi_output)
endif()
