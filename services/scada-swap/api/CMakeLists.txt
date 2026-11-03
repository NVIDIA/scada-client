# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.26)
include(../../../cmake/FetchVersion.cmake)
project(
    scada-swap
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX
)
include(../../../cmake/CommonCompilerOptions.cmake)
include(../../../cmake/CPM.cmake)
include(../../../cmake/ScadaCudaArchitectures.cmake)
include(GNUInstallDirs)
find_package(CUDAToolkit REQUIRED)
enable_language(CUDA)
find_package(CCCL 3.2 CONFIG REQUIRED)
if(NOT TARGET SCADA::swap-contract)
    add_subdirectory(../contract ${CMAKE_CURRENT_BINARY_DIR}/contract)
endif()
CPMAddPackage(
    NAME gdakis-adsc
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/adsc
    OPTIONS "ADSC_BUILD_TESTS FALSE" "GDAKIS_ADSC_BUILD_DOCS FALSE"
)
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(
    ${PROJECT_NAME} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)
target_compile_options(
    ${PROJECT_NAME}
    INTERFACE $<$<CXX_COMPILER_ID:Clang>:-Wno-unsupported-friend>
              $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
              $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>
              $<$<AND:$<COMPILE_LANGUAGE:CUDA>,$<CONFIG:Debug>>:-G>
              $<$<AND:$<COMPILE_LANGUAGE:CUDA>,$<CONFIG:Debug>>:-O0>
)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME swap)
target_link_libraries(
    ${PROJECT_NAME} INTERFACE GDAKIS::adsc SCADA::swap-contract SCADA::core GDAKIS::comm PSA::coaio-sync PSA::result
)

add_library(SCADA::swap ALIAS scada-swap)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/scada-swapConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/scada-swapConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/scada-swapConfig.cmake" @ONLY
)
install(
    TARGETS scada-swap
    EXPORT scada-swap_Targets
    COMPONENT client
)
install(
    EXPORT scada-swap_Targets
    FILE scada-swapTargets.cmake
    NAMESPACE SCADA::
    DESTINATION cmake/scada-swap
    COMPONENT client
)
install(
    FILES "${PROJECT_BINARY_DIR}/scada-swapConfig.cmake" "${PROJECT_BINARY_DIR}/scada-swapConfigVersion.cmake"
    DESTINATION cmake/scada-swap
    COMPONENT client
)
install(
    DIRECTORY include/scada
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT client
)
