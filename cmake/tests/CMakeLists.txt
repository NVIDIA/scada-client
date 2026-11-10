# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.26)
project(scada-cmake-policy LANGUAGES NONE)
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME scada_cmake_configuration COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_configuration.py)
