# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

include(CheckLanguage)
check_language(CUDA)
enable_language(CUDA)

include(GoogleTest)

add_executable(unit_tests_gpu_autobatch autobatchGPUUnitTests.cu)
target_link_libraries(unit_tests_gpu_autobatch tests gdakis-comm GTest::gtest_main)

# Ensure test discovery happens after build
gtest_discover_tests(
    unit_tests_gpu_autobatch
    PROPERTIES
    TIMEOUT 600 DISCOVERY_MODE PRE_TEST
)

add_executable(unit_tests_gpu_autobatch_group_reservation autobatchGroupReservationGPUUnitTests.cu)
target_link_libraries(unit_tests_gpu_autobatch_group_reservation gdakis-comm GTest::gtest_main)

# Preserve the short deadlock timeout without competing with the context
# startup storm from the other CUDA tests under ctest -j$(nproc).
gtest_discover_tests(
    unit_tests_gpu_autobatch_group_reservation
    PROPERTIES
    TIMEOUT 30 RUN_SERIAL TRUE DISCOVERY_MODE PRE_TEST
)

# Keep test-only CUDA options off consumer targets and global toolchain flags.
foreach(test_target IN ITEMS unit_tests_gpu_autobatch unit_tests_gpu_autobatch_group_reservation)
    target_compile_options(
        ${test_target} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
                               $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CUDA>>:-g;-G>
    )
endforeach()
