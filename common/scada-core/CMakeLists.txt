# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.26)
include(../../cmake/FetchVersion.cmake)
project(
    "scada-core"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Common Types and Machinery Using for all SCADA Services"
)

include(../../cmake/CommonCompilerOptions.cmake)
include(../../cmake/CPM.cmake)
include(GNUInstallDirs)
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
    NAME gdakis-utils
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    OPTIONS "GDAKIS_UTILS_BUILD_TESTS FALSE" "GDAKIS_UTILS_BUILD_DOCS FALSE"
)
CPMAddPackage(
    NAME gdakis-comm
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../comm
    OPTIONS "GDAKIS_COMM_BUILD_TESTS FALSE" "GDAKIS_COMM_BUILD_DOCS FALSE"
)

add_library(${PROJECT_NAME} src/uuid.cpp)
add_library(SCADA::core ALIAS ${PROJECT_NAME})
target_include_directories(
    ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME core)
target_link_libraries(${PROJECT_NAME} PUBLIC PSA::result PSA::coaio-sync GDAKIS::utils GDAKIS::comm)

# Add tests
if(PROJECT_IS_TOP_LEVEL)
    option(SCADA_CORE_BUILD_TESTS "Enable building tests." ON)
else()
    option(SCADA_CORE_BUILD_TESTS "Enable building tests." OFF)
endif()
if(SCADA_CORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Add documentation
option(SCADA_CORE_BUILD_DOCS "Enable building documentation." OFF)
if(SCADA_CORE_BUILD_DOCS)
    message(STATUS "Enable building documentation")
    include(../../cmake/ScadaDocumentation.cmake)
    scada_add_documentation()
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

# Namespace SCADA
install(
    EXPORT ${PROJECT_NAME}_Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE SCADA::
    DESTINATION cmake/${PROJECT_NAME}
)

# Install generated .cmake files
install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
              "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake" DESTINATION cmake/${PROJECT_NAME}
)

# Install header files
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/scada DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(SCADA_CORE_BUILD_DOCS)
    # Install documentation
    install(DIRECTORY ${PROJECT_BINARY_DIR}/doc/_build/docs/${PROJECT_NAME}/latest DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

message(STATUS "Generating packages/PKGBUILD file")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/PKGBUILD.in ${PROJECT_BINARY_DIR}/packages/PKGBUILD @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/meta.yaml.in ${PROJECT_BINARY_DIR}/packages/conda/meta.yaml @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/build.sh ${PROJECT_BINARY_DIR}/packages/conda/build.sh COPYONLY)

include(pkg/cpack.cmake)
