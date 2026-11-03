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

using val_type = uint8_t[128];

template<typename... Args>
using Policies_ = gdakis::adsc::policies_type<Args...>;

TEST(ADSCTest_undefined_policy, CpuMem) {
    EXPECT_EQ(gdakis::adsc::undefined_policy(), nullptr);
}

TEST(ADSCTest_undefined_policy_handlers, CpuMem) {
    auto test_policy = gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
        gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
        gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy());

    EXPECT_EQ(test_policy.miss(), gdakis::adsc::undefined_policy());
    EXPECT_EQ(test_policy.evict(), gdakis::adsc::undefined_policy());
    EXPECT_EQ(test_policy.victim(), gdakis::adsc::undefined_policy());
    EXPECT_EQ(test_policy.key_to_cl_id(), gdakis::adsc::undefined_policy());
    EXPECT_EQ(test_policy.key_to_cl_offset(), gdakis::adsc::undefined_policy());
    EXPECT_EQ(test_policy.cl_to_set(), gdakis::adsc::undefined_policy());
    EXPECT_EQ(test_policy.nth_key_of_cl(), gdakis::adsc::undefined_policy());
}
enum class miss_handler_expectations : unsigned {
    default_value = 0,
    miss_value,
    evict_value,
    victim_value,
    key_to_cl_id_value,
    key_to_cl_offset_value,
    cl_to_set_value,
    nth_key_of_cl_value
};
TEST(ADSCTest_miss_handlers, CpuMem) {
    miss_handler_expectations miss_value = miss_handler_expectations::default_value;
    miss_handler_expectations evict_value = miss_handler_expectations::default_value;
    miss_handler_expectations victim_value = miss_handler_expectations::default_value;
    miss_handler_expectations key_to_cl_id_value = miss_handler_expectations::default_value;
    miss_handler_expectations key_to_cl_offset_value = miss_handler_expectations::default_value;
    miss_handler_expectations cl_to_set_value = miss_handler_expectations::default_value;
    miss_handler_expectations nth_key_of_cl_value = miss_handler_expectations::default_value;

    auto test_policy
        = gdakis::adsc::make_policies([&miss_value]() { miss_value = miss_handler_expectations::miss_value; },
            [&evict_value]() { evict_value = miss_handler_expectations::evict_value; },
            [&victim_value]() { victim_value = miss_handler_expectations::victim_value; },
            [&key_to_cl_id_value]() { key_to_cl_id_value = miss_handler_expectations::key_to_cl_id_value; },
            [&key_to_cl_offset_value]() { key_to_cl_offset_value = miss_handler_expectations::key_to_cl_offset_value; },
            [&cl_to_set_value]() { cl_to_set_value = miss_handler_expectations::cl_to_set_value; },
            [&nth_key_of_cl_value]() { nth_key_of_cl_value = miss_handler_expectations::nth_key_of_cl_value; });

    test_policy.miss()();
    EXPECT_EQ(miss_value, miss_handler_expectations::miss_value);

    test_policy.evict()();
    EXPECT_EQ(evict_value, miss_handler_expectations::evict_value);

    test_policy.victim()();
    EXPECT_EQ(victim_value, miss_handler_expectations::victim_value);

    test_policy.key_to_cl_id()();
    EXPECT_EQ(key_to_cl_id_value, miss_handler_expectations::key_to_cl_id_value);

    test_policy.key_to_cl_offset()();
    EXPECT_EQ(key_to_cl_offset_value, miss_handler_expectations::key_to_cl_offset_value);

    test_policy.cl_to_set()();
    EXPECT_EQ(cl_to_set_value, miss_handler_expectations::cl_to_set_value);

    test_policy.nth_key_of_cl()();
    EXPECT_EQ(nth_key_of_cl_value, miss_handler_expectations::nth_key_of_cl_value);
}
