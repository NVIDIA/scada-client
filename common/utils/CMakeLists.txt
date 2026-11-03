# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.26)
include(../../cmake/FetchVersion.cmake)
project(
    "gdakis-utils"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Utilities used by different GDAKIS components"
)

include(../../cmake/CommonCompilerOptions.cmake)
include(../../cmake/CPM.cmake)
include(GNUInstallDirs)

include(../../cmake/ScadaCudaArchitectures.cmake)

CPMAddPackage(
    NAME gdakis-umem
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../umem
    OPTIONS "GDAKIS_UMEM_BUILD_TESTS FALSE" "GDAKIS_UMEM_BUILD_DOCS FALSE"
)

add_library(${PROJECT_NAME} INTERFACE)
add_library(GDAKIS::utils ALIAS ${PROJECT_NAME})
target_include_directories(
    ${PROJECT_NAME} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)
target_compile_options(
    ${PROJECT_NAME}
    INTERFACE $<$<CXX_COMPILER_ID:Clang>:-Wno-unsupported-friend>
              # atomic_thread_fence triggers this warning in clang but may be actually
              # used in other compilation units.
              $<$<CXX_COMPILER_ID:Clang>:-Wno-unneeded-internal-declaration>
              $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
              $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options($<$<COMPILE_LANGUAGE:CUDA>:-G>) # Enable debug info for CUDA
endif()
target_link_libraries(${PROJECT_NAME} INTERFACE GDAKIS::umem)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME utils)

# Add tests
if(PROJECT_IS_TOP_LEVEL)
    option(GDAKIS_UTILS_BUILD_TESTS "Enable building tests." ON)
else()
    option(GDAKIS_UTILS_BUILD_TESTS "Enable building tests." OFF)
endif()
if(GDAKIS_UTILS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Add documentation
option(GDAKIS_UTILS_BUILD_DOCS "Enable building documentation." OFF)
if(GDAKIS_UTILS_BUILD_DOCS)
    message(STATUS "Enable building documentation")
    include(../../cmake/ScadaDocumentation.cmake)
    scada_add_documentation()
endif()

# Compiler performance experiments
option(GDAKIS_UTILS_BUILD_PERF "Enable building performance experiments." OFF)
if(GDAKIS_UTILS_BUILD_PERF)
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

message(STATUS "Generating packages/PKGBUILD file")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/PKGBUILD.in ${PROJECT_BINARY_DIR}/packages/PKGBUILD @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/meta.yaml.in ${PROJECT_BINARY_DIR}/packages/conda/meta.yaml @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/build.sh ${PROJECT_BINARY_DIR}/packages/conda/build.sh COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/cpack.cmake.in ${PROJECT_BINARY_DIR}/packages/cpack.cmake @ONLY)
include(${PROJECT_BINARY_DIR}/packages/cpack.cmake)
