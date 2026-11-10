#[[
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
]]

cmake_minimum_required(VERSION 3.26)
include(../../cmake/FetchVersion.cmake)
message(STATUS "Using version ${PROJECT_VERSION}")
project(
    "psa-result"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Rust-inspired Result types"
)
include(../../cmake/CommonCompilerOptions.cmake)
include(../../cmake/CPM.cmake)
include(GNUInstallDirs)
include(CheckLanguage)
if(NOT TARGET CCCL::CCCL)
    find_package(CCCL CONFIG QUIET)
    if(NOT TARGET CCCL::CCCL)
        find_path(
            CCCL_INCLUDE_DIR
            NAMES cuda/std/version
            PATHS /usr/local/cuda/include /usr/include /usr/local/cuda/include/cccl
            NO_DEFAULT_PATH REQUIRED
        )
        add_library(CCCL INTERFACE IMPORTED GLOBAL)
        set_target_properties(CCCL PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CCCL_INCLUDE_DIR}")
        add_library(CCCL::CCCL ALIAS CCCL)
    endif()
endif()

# Only enable CUDA compilation if nvcc is found
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    include(../../cmake/ScadaCudaArchitectures.cmake)
    enable_language(CUDA)
    message(STATUS "CUDA compilation enabled")
endif()

add_library(${PROJECT_NAME} INTERFACE)
add_library(PSA::result ALIAS ${PROJECT_NAME})
target_include_directories(
    ${PROJECT_NAME} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)
target_compile_options(
    ${PROJECT_NAME}
    INTERFACE $<$<CXX_COMPILER_ID:Clang>:-Wno-unsupported-friend>
              $<$<CXX_COMPILER_ID:Clang>:-ftemplate-backtrace-limit=0> $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
              $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>
)
target_link_libraries(${PROJECT_NAME} INTERFACE CCCL::CCCL)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME result)

# Add tests
if(PROJECT_IS_TOP_LEVEL)
    option(RESULT_BUILD_TESTS "Enable building tests." ON)
else()
    option(RESULT_BUILD_TESTS "Enable building tests." OFF)
endif()
if(RESULT_BUILD_TESTS)
    message(STATUS "Enable building tests")
    enable_testing()
    add_subdirectory(tests)
    # Compile debug checks
    if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
        add_subdirectory(check)
    endif()
endif()

# Add documentation
option(RESULT_BUILD_DOCS "Enable building documentation." OFF)
if(RESULT_BUILD_DOCS)
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
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}_Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} DESTINATION ${CMAKE_INSTALL_LIBDIR}
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

if(RESULT_BUILD_DOCS)
    # Install documentation
    install(DIRECTORY ${PROJECT_BINARY_DIR}/doc/_build/docs/${PROJECT_NAME}/latest DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

if(PROJECT_IS_TOP_LEVEL)
    message(STATUS "Generating packages/PKGBUILD file for ${PROJECT_NAME}")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/PKGBUILD.in ${PROJECT_BINARY_DIR}/packages/PKGBUILD @ONLY)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/meta.yaml.in ${PROJECT_BINARY_DIR}/packages/conda/meta.yaml @ONLY
    )
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/build.sh ${PROJECT_BINARY_DIR}/packages/conda/build.sh COPYONLY
    )
    include(${CMAKE_CURRENT_SOURCE_DIR}/pkg/cpack.cmake)
endif()
