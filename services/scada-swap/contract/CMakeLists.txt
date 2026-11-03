# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.26)
include(../../../cmake/FetchVersion.cmake)
project(
    scada-swap-contract
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX
)
include(../../../cmake/CPM.cmake)
include(GNUInstallDirs)
CPMAddPackage(
    NAME scada-core
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/scada-core
    OPTIONS "SCADA_CORE_BUILD_TESTS FALSE" "SCADA_CORE_BUILD_DOCS FALSE"
)

add_library(scada-swap-contract INTERFACE)
add_library(SCADA::swap-contract ALIAS scada-swap-contract)
set_target_properties(scada-swap-contract PROPERTIES EXPORT_NAME swap-contract)
target_include_directories(
    scada-swap-contract INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(scada-swap-contract INTERFACE cxx_std_17)
target_link_libraries(scada-swap-contract INTERFACE SCADA::core)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/scada-swap-contractConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/scada-swap-contractConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/scada-swap-contractConfig.cmake" @ONLY
)
install(
    TARGETS scada-swap-contract
    EXPORT scada-swap-contract_Targets
    COMPONENT client
)
install(
    EXPORT scada-swap-contract_Targets
    NAMESPACE SCADA::
    DESTINATION cmake/scada-swap-contract
    COMPONENT client
)
install(
    FILES "${PROJECT_BINARY_DIR}/scada-swap-contractConfig.cmake"
          "${PROJECT_BINARY_DIR}/scada-swap-contractConfigVersion.cmake"
    DESTINATION cmake/scada-swap-contract
    COMPONENT client
)
install(
    DIRECTORY include/scada
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT client
)
