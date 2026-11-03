#[[
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
]]
cmake_minimum_required(VERSION 3.26)

option(GDAKIS_COMM_USE_CLANGD "Enable clangd support for IDE." OFF)
if(GDAKIS_COMM_USE_CLANGD)
    include(../../cmake/cuda-clangd.cmake)
endif()
include(../../cmake/FetchVersion.cmake)
project(
    "gdakis-comm"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Communication utilities used by GDAKIS"
)

include(../../cmake/CommonCompilerOptions.cmake)
include(../../cmake/CPM.cmake)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options($<$<COMPILE_LANGUAGE:CUDA>:-G>) # Enable debug info for CUDA
endif()

option(GDAKIS_COMM_BUILD_CUDA_ARCH_PRINT "Enable CUDA architecture print." OFF)
if(GDAKIS_COMM_BUILD_CUDA_ARCH_PRINT)
    add_compile_definitions($<$<COMPILE_LANGUAGE:CUDA>:-D__CUDA_ARCH_PRINT__>)
endif()

include(CheckIncludeFileCXX)

include(../../cmake/ScadaCudaArchitectures.cmake)

CPMAddPackage(
    NAME psa-result
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../result
    OPTIONS "RESULT_BUILD_TESTS FALSE" "RESULT_BUILD_DOCS FALSE"
)
CPMAddPackage(
    NAME psa-coaio
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../coaio
    OPTIONS "COAIO_BUILD_TESTS FALSE" "COAIO_BUILD_DOCS FALSE" "COAIO_BUILD_PERF_TESTS FALSE"
)
CPMAddPackage(
    NAME gdakis-umem
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../umem
    OPTIONS "GDAKIS_UMEM_BUILD_TESTS FALSE" "GDAKIS_UMEM_BUILD_DOCS FALSE"
)
CPMAddPackage(
    NAME gdakis-utils
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    OPTIONS "GDAKIS_UTILS_BUILD_TESTS FALSE" "GDAKIS_UTILS_BUILD_DOCS FALSE"
)

check_include_file_cxx(linux/udmabuf.h UDMABUF_HEADER)
if(NOT UDMABUF_HEADER)
    message(FATAL_ERROR "Cannot locate linux/udmabuf.h header file")
endif()

add_library(${PROJECT_NAME} INTERFACE)
add_library(GDAKIS::comm ALIAS ${PROJECT_NAME})
target_include_directories(
    ${PROJECT_NAME} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)
target_compile_definitions(${PROJECT_NAME} INTERFACE -D_GNU_SOURCE)
target_compile_options(
    ${PROJECT_NAME}
    INTERFACE $<$<CXX_COMPILER_ID:Clang>:-Wno-unsupported-friend>
              $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
              $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>
              $<$<AND:$<COMPILE_LANGUAGE:CUDA>,$<CONFIG:Debug>>:-G>
              $<$<AND:$<COMPILE_LANGUAGE:CUDA>,$<CONFIG:Debug>>:-O0>
)

target_link_libraries(${PROJECT_NAME} INTERFACE PSA::result PSA::coaio-sync GDAKIS::umem GDAKIS::utils)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME comm)

# Add tests

if(PROJECT_IS_TOP_LEVEL)
    option(GDAKIS_COMM_BUILD_TESTS "Enable building tests." ON)
else()
    option(GDAKIS_COMM_BUILD_TESTS "Enable building tests." OFF)
endif()

if(GDAKIS_COMM_BUILD_TESTS)
    include(CTest)
    enable_testing()
    set(CTEST_OUTPUT_ON_FAILURE ON)
    set(CTEST_TIMEOUT 600)
    add_subdirectory(tests)
endif()

# Add documentation
option(GDAKIS_COMM_BUILD_DOCS "Enable building documentation." OFF)
if(GDAKIS_COMM_BUILD_DOCS)
    message(STATUS "Enable building documentation")
    include(../../cmake/ScadaDocumentation.cmake)
    scada_add_documentation()
endif()

# Compiler performance experiments
option(GDAKIS_COMM_BUILD_PERF "Enable building performance experiments." OFF)
if(GDAKIS_COMM_BUILD_PERF)
    add_subdirectory(perf)
endif()

# Generate version file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Configure where to install package file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake" @ONLY
)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}_Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_RUNDIR}
)

# Namespace GDAKIS
install(
    EXPORT ${PROJECT_NAME}_Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE GDAKIS::
    DESTINATION cmake/${PROJECT_NAME}
)

# Install generated .cmake files
install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
              "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake" DESTINATION cmake/${PROJECT_NAME}
)

# Install header files
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/gdakis DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(GDAKIS_COMM_BUILD_DOCS)
    # Install documentation
    install(DIRECTORY ${PROJECT_BINARY_DIR}/doc/_build/docs/${PROJECT_NAME}/latest DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

message(STATUS "Generating packages/PKGBUILD file")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/PKGBUILD.in ${PROJECT_BINARY_DIR}/packages/PKGBUILD @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/meta.yaml.in ${PROJECT_BINARY_DIR}/packages/conda/meta.yaml @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/build.sh ${PROJECT_BINARY_DIR}/packages/conda/build.sh COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/cpack.cmake.in ${PROJECT_BINARY_DIR}/packages/cpack.cmake @ONLY)
include(${PROJECT_BINARY_DIR}/packages/cpack.cmake)
