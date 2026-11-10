# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.26)
include(cmake/FetchVersion.cmake)
project(
    scada-client
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX
)

include(cmake/ScadaCudaArchitectures.cmake)
find_package(CUDAToolkit REQUIRED)
enable_language(CUDA)
find_package(CCCL 3.2 CONFIG REQUIRED)

option(SCADA_CLIENT_BUILD_TESTS "Build standalone client contract/API compile checks." OFF)
add_subdirectory(services/scada-swap/api)
include(GNUInstallDirs)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/scada-client)
install(FILES cmake/CPM.cmake LICENSES/CC-BY-4.0.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/scada-client/third-party
)
if(SCADA_CLIENT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
