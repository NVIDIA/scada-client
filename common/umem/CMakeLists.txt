# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.26)
include(../../cmake/FetchVersion.cmake)
project(
    gdakis-umem
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "C++ Memory allocators to build objects in CPU/GPU memory or dma-buf"
    LANGUAGES C CXX
)
set(CMAKE_CXX_STANDARD 17)

include(../../cmake/CommonCompilerOptions.cmake)
include(../../cmake/CPM.cmake)

include(CheckLanguage)
include(FindThreads)
include(GNUInstallDirs)

include(../../cmake/ScadaCudaArchitectures.cmake)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        include_directories($<$<COMPILE_LANGUAGE:CXX>:${CUDAToolkit_INCLUDE_DIRS}>)
    endif()
endif()

add_library(${PROJECT_NAME} src/cpu.cpp src/cuda.cpp)
add_library(GDAKIS::umem ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(
    ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_options(${PROJECT_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>)

if(CUDAToolkit_FOUND)
    set(gdakis_umem_use_cuda_toolkit TRUE)
    target_link_libraries(${PROJECT_NAME} INTERFACE CUDA::cuda_driver)
    get_target_property(gdakis_umem_library_type ${PROJECT_NAME} TYPE)
    if(gdakis_umem_library_type STREQUAL "SHARED_LIBRARY")
        # The shared C++ implementation must resolve its own CUDA calls.
        target_link_libraries(${PROJECT_NAME} PRIVATE CUDA::cuda_driver CUDA::cudart)
    endif()
else()
    set(gdakis_umem_use_cuda_toolkit FALSE)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME umem)

# Add tests
if(PROJECT_IS_TOP_LEVEL)
    option(GDAKIS_UMEM_BUILD_TESTS "Enable building tests." ON)
else()
    option(GDAKIS_UMEM_BUILD_TESTS "Enable building tests." OFF)
endif()
if(GDAKIS_UMEM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Add documentation
option(GDAKIS_UMEM_BUILD_DOCS "Enable building documentation." OFF)
if(GDAKIS_UMEM_BUILD_DOCS)
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
if(GDAKIS_UMEM_BUILD_DOCS)
    # Install documentation
    install(DIRECTORY ${PROJECT_BINARY_DIR}/doc/_build/docs/${PROJECT_NAME}/latest DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/PKGBUILD.in ${PROJECT_BINARY_DIR}/packages/PKGBUILD @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/meta.yaml.in ${PROJECT_BINARY_DIR}/packages/conda/meta.yaml @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/conda/build.sh ${PROJECT_BINARY_DIR}/packages/conda/build.sh COPYONLY)

include(pkg/cpack.cmake)
