# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Source versions belong to the component/distribution, not the embedding
# application's Git history. Release assemblers can override them explicitly.
if(FETCHVERSION_SKIP)
    set(PROJECT_VERSION
        "0.0.0"
        CACHE STRING "" FORCE
    )
elseif(DEFINED SCADA_SOURCE_VERSION)
    if(NOT SCADA_SOURCE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "SCADA_SOURCE_VERSION must have the form major.minor.patch")
    endif()
    set(PROJECT_VERSION "${SCADA_SOURCE_VERSION}")
else()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _scada_source_version)
    else()
        file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" _scada_source_version)
    endif()
    string(STRIP "${_scada_source_version}" _scada_source_version)
    if(NOT _scada_source_version MATCHES "^v?([0-9]+)(\\.([0-9]+))?(\\.([0-9]+))?$")
        message(FATAL_ERROR "Invalid SCADA VERSION metadata: ${_scada_source_version}")
    endif()
    set(_scada_major "${CMAKE_MATCH_1}")
    set(_scada_minor "${CMAKE_MATCH_3}")
    set(_scada_patch "${CMAKE_MATCH_5}")
    if(_scada_minor STREQUAL "")
        set(_scada_minor 0)
    endif()
    if(_scada_patch STREQUAL "")
        set(_scada_patch 0)
    endif()
    set(PROJECT_VERSION "${_scada_major}.${_scada_minor}.${_scada_patch}")
endif()
if(NOT DEFINED PROJECT_VERSION_RC)
    set(PROJECT_VERSION_RC 1)
endif()
message(STATUS "SCADA source version ${PROJECT_VERSION}-rc${PROJECT_VERSION_RC}")
