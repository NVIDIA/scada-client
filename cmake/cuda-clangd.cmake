# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force CMake to emit compile commands in a clangd-friendly way
set(CMAKE_CUDA_USE_RESPONSE_FILE_FOR_INCLUDES OFF)
set(CMAKE_CUDA_USE_RESPONSE_FILE_FOR_LIBRARIES OFF)
set(CMAKE_CUDA_USE_RESPONSE_FILE_FOR_OBJECTS OFF)

# Ensure CUDA compilation database is complete
set(CMAKE_CUDA_COMPILE_WHOLE_COMPILATION ON)

# Function to normalize include paths for clangd
function(normalize_include_paths target)
    if(NOT TARGET ${target})
        return()
    endif()

    get_target_property(includes ${target} INTERFACE_INCLUDE_DIRECTORIES)
    if(includes)
        set(normalized_includes "")
        foreach(inc ${includes})
            # Skip generator expressions
            if(inc MATCHES "\\$<")
                list(APPEND normalized_includes ${inc})
                continue()
            endif()

            # Convert relative paths to absolute
            if(NOT IS_ABSOLUTE ${inc})
                get_filename_component(abs_inc ${inc} ABSOLUTE)
                list(APPEND normalized_includes ${abs_inc})
            else()
                list(APPEND normalized_includes ${inc})
            endif()
        endforeach()
        set_target_properties(${target} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${normalized_includes}")
    endif()
endfunction()

# Hook to normalize paths after project configuration.
# The function itself checks whether the target exists when the deferred call runs.
if(PROJECT_NAME)
    cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL normalize_include_paths ${PROJECT_NAME})
endif()
