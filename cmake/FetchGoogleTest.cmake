# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# The one place googletest is pinned.
#
# This block used to be copy-pasted into seventeen CMakeLists.txt files, all
# identical, which made a version bump seventeen edits and one of them easy to
# miss. Include this instead, from anywhere that needs GTest:
#
#   include(../../../cmake/FetchGoogleTest.cmake)
#
# Not named GoogleTest.cmake. CMake ships a module by that name, the one that
# provides gtest_discover_tests, and several CMakeLists here call
# `include(GoogleTest)` to get it. A file of ours with that name shadows it and
# the build dies with "Unknown CMake command gtest_discover_tests", which is
# what happened on the first attempt at this.
#
# Nothing here changes what is fetched. What makes the fetch cheap is
# CPM_SOURCE_CACHE, set in .gitlab-ci.yml, which gives CPM a directory outside
# the cloned tree to keep sources in. Without it every job of every pipeline
# clones googletest again: a full pipeline is around a hundred jobs, and on
# 2026-08-21 that burst was answered by GitHub with
#
#   fatal: could not read Username for 'https://github.com'
#
# on 27 of them inside six minutes, while GitHub was reachable from the same
# hosts. Reaching a public forge once per host instead of once per job is the
# point; see SCADA-366.
#
# Requires CPM to be included first, which every module already does through
# cmake/CPM.cmake.

include(${CMAKE_CURRENT_LIST_DIR}/ThirdPartySources.cmake)
scada_third_party_urls(google googletest v1.14.0 GOOGLETEST_URLS)

# Build GoogleTest from source (system version has incompatible ABI)
CPMAddPackage(
    NAME googletest
    VERSION 1.14.0
    URL ${GOOGLETEST_URLS} URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
    OPTIONS "INSTALL_GTEST OFF" "INSTALL_GMOCK OFF" "BUILD_SHARED_LIBS OFF"
)
message(STATUS "Building GoogleTest from source (static)")
