/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdio>
#include <gdakis/umem/cpu>
#include <gdakis/umem/mem>
#include <gtest/gtest.h>

#include <gdakis/adsc/cache>
#include <gdakis/atomic>

constexpr auto scope = gdakis::thread_scope_device;
using val_type = uint8_t[128];

constexpr unsigned cl_size = 1;

TEST(ADSCTest_invalid_key, CpuMem) {
    using key_type = unsigned;
    using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;
    EXPECT_EQ(cl_ref_type::invalid_key<key_type>(), static_cast<key_type>(-1));
}

TEST(ADSCTest_invalid_cl_id, CpuMem) {
    using key_type = unsigned;
    using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;
    EXPECT_EQ(cl_ref_type::invalid_cl_id(), static_cast<key_type>(-1));
}

TEST(ADSCTest_cl_buffer_offset, CpuMem) {
    using key_type = unsigned;
    using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;
    key_type cl_id = -1;
    cl_ref_type test_cl(10, cl_id, nullptr, nullptr, nullptr);
    EXPECT_EQ(test_cl.cl_buffer_offset(), 10);
    cl_ref_type test_cl1(-1, cl_id, nullptr, nullptr, nullptr);
    EXPECT_EQ(test_cl1.cl_buffer_offset(), -1);
}

TEST(ADSCTest_cl_id, CpuMem) {
    using key_type = unsigned;
    using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;
    key_type cl_id = 0;
    cl_ref_type test_cl(0, cl_id, nullptr, nullptr, nullptr);
    EXPECT_EQ(test_cl.cl_id(), 0);
    cl_id = -1;
    cl_ref_type test_cl1(0, cl_id, nullptr, nullptr, nullptr);
    EXPECT_EQ(test_cl1.cl_id(), -1);
    cl_id = 10;
    cl_ref_type test_cl2(0, cl_id, nullptr, nullptr, nullptr);
    EXPECT_EQ(test_cl2.cl_id(), 10);
}

TEST(ADSCTest_vals, CpuMem) {
    using key_type = unsigned;
    using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;
    key_type cl_id = 0;
    val_type test_val;
    cl_ref_type test_cl(0, cl_id, &test_val, nullptr, nullptr);
    EXPECT_EQ(test_cl.vals(), &test_val);
    cl_ref_type test_cl1(0, cl_id, nullptr, nullptr, nullptr);
    EXPECT_EQ(test_cl1.vals(), nullptr);
}
TEST(ADSCTest_dirty, CpuMem) {
    using key_type = unsigned;
    using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;
    key_type cl_id = 0;
    cl_ref_type test_cl(0, cl_id, nullptr, nullptr, nullptr);
    ASSERT_FALSE(test_cl.is_dirty());
    test_cl.mark_dirty();
    ASSERT_TRUE(test_cl.is_dirty());
}

TEST(ADSCTest_clean, CpuMem) {
    using key_type = unsigned;
    using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;
    key_type cl_id = 0;
    cl_ref_type test_cl(0, cl_id, nullptr, nullptr, nullptr);
    test_cl.mark_dirty();
    ASSERT_TRUE(test_cl.is_dirty());
    test_cl.mark_clean();
    ASSERT_FALSE(test_cl.is_dirty());
}

TEST(ADSCTest_reset, CpuMem) {
    using key_type = unsigned;
    using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;
    key_type cl_id = 0;
    cl_ref_type test_cl(0, cl_id, nullptr, nullptr, nullptr);
    test_cl.mark_dirty();
    ASSERT_TRUE(test_cl.is_dirty());
    test_cl.reset();
    ASSERT_FALSE(test_cl.is_dirty());
}

TEST(ADSCTest_invalidate, CpuMem) {
    using key_type = unsigned;
    using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;
    key_type cl_id = 10;
    cl_ref_type test_cl(0, cl_id, nullptr, nullptr, nullptr);
    test_cl.invalidate();
    EXPECT_EQ(test_cl.cl_id(), test_cl.invalid_cl_id());
    ASSERT_FALSE(test_cl.is_dirty());
}
