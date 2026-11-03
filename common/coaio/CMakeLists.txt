# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.26)
include(../../cmake/FetchVersion.cmake)
project(
    "psa-coaio"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Coroutine based C++ I/O Library"
)

include(../../cmake/CommonCompilerOptions.cmake)
include(../../cmake/CPM.cmake)
include(GNUInstallDirs)

CPMAddPackage(
    NAME psa-result
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../result
    OPTIONS "RESULT_BUILD_TESTS FALSE" "RESULT_BUILD_DOCS FALSE"
)

include(../../cmake/ScadaCudaArchitectures.cmake)

add_library(${PROJECT_NAME}-internal INTERFACE)
target_include_directories(
    ${PROJECT_NAME}-internal INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                       $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(${PROJECT_NAME}-internal INTERFACE PSA::result)

add_library(${PROJECT_NAME}-sync INTERFACE)
add_library(PSA::coaio-sync ALIAS ${PROJECT_NAME}-sync)
target_compile_features(${PROJECT_NAME}-sync INTERFACE cxx_std_17)
target_link_libraries(${PROJECT_NAME}-sync INTERFACE ${PROJECT_NAME}-internal)
set_target_properties(${PROJECT_NAME}-sync PROPERTIES EXPORT_NAME coaio-sync)

add_library(${PROJECT_NAME}-async INTERFACE)
add_library(PSA::coaio-async ALIAS ${PROJECT_NAME}-async)
target_compile_features(${PROJECT_NAME}-async INTERFACE cxx_std_20)
target_link_libraries(${PROJECT_NAME}-async INTERFACE ${PROJECT_NAME}-internal)
if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    target_compile_options(${PROJECT_NAME}-async INTERFACE -fcoroutines)
endif()
set_target_properties(${PROJECT_NAME}-async PROPERTIES EXPORT_NAME coaio-async)

# Add tests
if(PROJECT_IS_TOP_LEVEL)
    option(COAIO_BUILD_TESTS "Enable building tests." ON)
else()
    option(COAIO_BUILD_TESTS "Enable building tests." OFF)
endif()

if(COAIO_BUILD_TESTS)
    message(STATUS "Enable building tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# Performance tests
option(COAIO_BUILD_PERF_TESTS "Enable building performance tests." ON)
if(COAIO_BUILD_PERF_TESTS)
    add_subdirectory(perf)
endif()

# Add documentation
option(COAIO_BUILD_DOCS "Enable building documentation." OFF)
if(COAIO_BUILD_DOCS)
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

# Export targets for install
install(
    TARGETS ${PROJECT_NAME}-internal ${PROJECT_NAME}-sync ${PROJECT_NAME}-async RUNTIME_DEPENDENCY_SET psa-result
    EXPORT ${PROJECT_NAME}_Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_RUNDIR}
)

install(
    EXPORT ${PROJECT_NAME}_Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE PSA::
    DESTINATION cmake/${PROJECT_NAME}
)

# Install generated .cmake files
install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
              "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake" DESTINATION cmake/${PROJECT_NAME}
)

# Install header files
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/psa DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(COAIO_BUILD_DOCS)
    # Install documentation
    install(DIRECTORY ${PROJECT_BINARY_DIR}/doc/_build/docs/${PROJECT_NAME}/latest DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

message(STATUS "Generating packages/PKGBUILD file for ${PROJECT_NAME}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/PKGBUILD.in ${PROJECT_BINARY_DIR}/packages/PKGBUILD @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/meta.yaml.in ${PROJECT_BINARY_DIR}/packages/conda/meta.yaml @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/build.sh ${PROJECT_BINARY_DIR}/packages/conda/build.sh COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/cpack.cmake.in ${PROJECT_BINARY_DIR}/packages/cpack.cmake @ONLY)
include(${PROJECT_BINARY_DIR}/packages/cpack.cmake)
