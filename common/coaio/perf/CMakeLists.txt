# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

find_package(Threads REQUIRED)
include(../../../cmake/ThirdPartySources.cmake)
scada_third_party_urls(jarro2783 cxxopts v3.1.1 CXXOPTS_URLS)
CPMAddPackage(
    NAME cxxopts
    VERSION 3.1.1
    URL ${CXXOPTS_URLS} URL_HASH SHA256=523175f792eb0ff04f9e653c90746c12655f10cb70f1d5e6d6d9491420298a08
        OVERRIDE_FIND_PACKAGE
)

add_executable(unix-socket unix-socket.cpp)
target_link_libraries(unix-socket psa-coaio-async Threads::Threads)
