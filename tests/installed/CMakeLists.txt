# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.26)
project(scada-installed-client LANGUAGES CXX CUDA)
find_package(scada-swap CONFIG REQUIRED)
add_executable(installed-client-compile ../api_compile.cu)
target_link_libraries(installed-client-compile PRIVATE SCADA::swap)
set_target_properties(installed-client-compile PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON)
