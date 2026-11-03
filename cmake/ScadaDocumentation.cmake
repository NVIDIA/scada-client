# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
function(scada_add_documentation)
    if(NOT DEFINED SCADA_DOC_ENGINE OR NOT EXISTS "${SCADA_DOC_ENGINE}/CMakeLists.txt")
        message(FATAL_ERROR "Documentation builds require -DSCADA_DOC_ENGINE=<repo-doc source directory>.")
    endif()
    add_subdirectory("${SCADA_DOC_ENGINE}" "${CMAKE_BINARY_DIR}/doc")
endfunction()
