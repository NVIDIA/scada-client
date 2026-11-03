/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/umem/cpu>
#include <gdakis/umem/mem>
#include <gtest/gtest.h>

#include <gdakis/adsc/cache>
#include <gdakis/atomic>

#include <cstring>

constexpr auto scope = gdakis::thread_scope_device;
using val_type = uint8_t[128];
using key_type = unsigned;
constexpr unsigned cl_size = 1;
constexpr unsigned ways = 32;
constexpr unsigned num_cls = 128;
using cl_ref_type = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size>;

template<typename... Args>
using Policies_ = gdakis::adsc::policies_type<Args...>;

//Simple Test verify Put and and Get and see if the get value matches the put value
TEST(ADSCTestPutAndGet, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());

    EXPECT_NE(nullptr, cache_handle.get());

    val_type put_val;
    for (auto i = 0ul; i < sizeof(val_type); i++) {
        put_val[i] = static_cast<uint8_t>(i);
    }
    auto ret = cache_handle.get()->put(1, put_val);
    EXPECT_EQ(0, ret);

    val_type get_val;
    ret = cache_handle.get()->get(1, get_val);
    EXPECT_EQ(0, ret);

    for (auto i = 0ul; i < sizeof(val_type); i++) {
        EXPECT_EQ(put_val[i], get_val[i]);
    }
}

template<unsigned _LineSize>
static void check_write_miss_fetch() {
    using ref_type = gdakis::adsc::cl_reference<scope, unsigned, unsigned, _LineSize>;
    using cache_type = gdakis::adsc::cache<scope, ref_type, 1>;
    auto data = gdakis::umem::mem<gdakis::umem::cpu, unsigned[]>::alloc(_LineSize);
    auto handle = cache_type::template build<gdakis::umem::cpu>(1, data.get());
    ASSERT_NE(nullptr, handle.get());

    unsigned backing[2 * _LineSize];
    for (unsigned i = 0; i < 2 * _LineSize; ++i)
        backing[i] = i + 10;
    unsigned fetches = 0, writebacks = 0;
    auto policies = gdakis::adsc::make_policies(
        [&](ref_type & ref) {
            ++fetches;
            std::memcpy(ref.vals(), backing + ref.cl_id() * _LineSize, _LineSize * sizeof(unsigned));
            return 0;
        },
        gdakis::adsc::undefined_policy(),
        [&](ref_type & ref) {
            if (ref.is_dirty()) {
                ++writebacks;
                std::memcpy(backing + ref.cl_id() * _LineSize, ref.vals(), _LineSize * sizeof(unsigned));
            }
            return 0;
        },
        gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
        gdakis::adsc::undefined_policy());

    ASSERT_EQ(0, handle.get()->put(0, 42, policies));
    EXPECT_EQ(_LineSize == 1 ? 0u : 1u, fetches);
    EXPECT_TRUE(handle.get()->cl_ref(0, 0).is_dirty());
    unsigned value = 0;
    ASSERT_EQ(0, handle.get()->get(0, value, policies));
    EXPECT_EQ(42u, value);
    for (unsigned i = 1; i < _LineSize; ++i) {
        ASSERT_EQ(0, handle.get()->get(i, value, policies));
        EXPECT_EQ(i + 10, value);
    }

    // A read miss evicts the dirty line and still fetches the requested line.
    unsigned before_read = fetches;
    ASSERT_EQ(0, handle.get()->get(_LineSize, value, policies));
    EXPECT_EQ(before_read + 1, fetches);
    EXPECT_EQ(_LineSize + 10, value);
    EXPECT_EQ(1u, writebacks);
    EXPECT_EQ(42u, backing[0]);
    for (unsigned i = 1; i < _LineSize; ++i)
        EXPECT_EQ(i + 10, backing[i]);

    ASSERT_EQ(0, handle.get()->get(0, value, policies));
    EXPECT_EQ(42u, value);
}

TEST(ADSCWriteMiss, FullLineSkipsFetch) {
    check_write_miss_fetch<1>();
}

TEST(ADSCWriteMiss, PartialLinePreservesOtherValues) {
    check_write_miss_fetch<4>();
}

//Test Get with default policy
TEST(ADSCSetupDefaultGlobalPolicies, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());

    EXPECT_NE(nullptr, cache_handle.get());

    val_type val;
    auto ret = cache_handle.get()->get(1, val);

    EXPECT_EQ(0, ret);
}

//Test Get with Custom Miss Policy
TEST(ADSCSetupCustomGlobalMissPolicy, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    int x = 0;
    key_type key;

    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());

    EXPECT_NE(nullptr, cache_handle.get());

    val_type val;
    key = 1;
    auto ret = cache_handle.get()->get(1, val,
        gdakis::adsc::make_policies(
            [&x](cl_ref_type & r) {
                x = r.cl_id();
                return 0;
            },
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));

    EXPECT_EQ(0, ret);
    EXPECT_EQ(key / cl_size, x);

    //Change the cache size
    constexpr unsigned cl_size1 = 4;
    using cl_ref_type1 = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size1>;
    auto data_ptr1 = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size1);
    x = 0;

    auto cache_handle1
        = gdakis::adsc::cache<scope, cl_ref_type1, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr1.get());

    EXPECT_NE(nullptr, cache_handle1.get());

    key = 1024;
    ret = cache_handle1.get()->get(key, val,
        gdakis::adsc::make_policies(
            [&x](cl_ref_type1 & r) {
                x = r.cl_id();
                return 0;
            },
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));

    EXPECT_EQ(0, ret);
    EXPECT_EQ(key / cl_size1, x);

    key = 11117;
    ret = cache_handle1.get()->get(key, val,
        gdakis::adsc::make_policies(
            [&x](cl_ref_type1 & r) {
                x = r.cl_id();
                return 0;
            },
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));

    EXPECT_EQ(0, ret);
    EXPECT_EQ(key / cl_size1, x);
}

TEST(ADSCSetupCustomAccessPolicies, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    int x = 0;
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());

    EXPECT_NE(nullptr, cache_handle.get());

    val_type val;
    auto ret = cache_handle.get()->get(1, val,
        gdakis::adsc::make_policies(
            [&x](cl_ref_type & r) {
                x = r.cl_id();
                return 0;
            },
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ(0, ret);

    ret = cache_handle.get()->get(1, val,
        gdakis::adsc::make_policies(
            [&x](cl_ref_type & r) {
                x = r.cl_id() + 1;
                return 0;
            },
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, x);
}

TEST(ADSCTestHandleNumCls, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);

    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    EXPECT_EQ(cache_handle.get()->num_cls(), num_cls);
}

TEST(ADSCTestInvalidateCache, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);

    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    EXPECT_EQ(cache_handle.get()->num_cls(), num_cls);

    cache_handle.invalidate_cache();

    cl_ref_type & cl_ref0 = cache_handle.get()->cl_ref(0, 0);
#if DEBUG
    printf("Inside test addr of cl_ref[0] = %p, sizeof(cl_ref) %d\n", &cl_ref0, sizeof(cl_ref_type));
#endif
    cl_ref_type * cl_refs = &cl_ref0;
#if DEBUG
    printf("&cl_ref[0] = %p, &cl_ref[1] = %p\n", &cl_refs[0], &cl_refs[1]);
#endif
    for (auto i = 0ul; i < num_cls; i++) {
        auto cl_id = cl_refs[i].cl_id();
        EXPECT_EQ(cl_id, cl_ref_type::invalid_key<key_type>());
    }
}

TEST(ADSCTestFlushCache, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);

    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    EXPECT_EQ(cache_handle.get()->num_cls(), num_cls);

    cl_ref_type & cl_ref0 = cache_handle.get()->cl_ref(0, 0);
    cl_ref_type * cl_refs = &cl_ref0;
    //Mark Cache as Dirty
    for (auto i = 0ul; i < num_cls; i++) {
        cl_refs[i].mark_dirty();
        EXPECT_EQ(cl_refs[i].is_dirty(), true);
    }

    //Flush Cache
    cache_handle.flush_cache(gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(),
        gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
        gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));

    //Expect it to be not dirty
    for (auto i = 0ul; i < num_cls; i++) {
        EXPECT_EQ(cl_refs[i].is_dirty(), false);
    }
}

TEST(ADSCTest_cl_ref, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    unsigned way;
    unsigned set_id;
    unsigned expected_cl_ref;
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    EXPECT_EQ(cache_handle.get()->num_cls(), num_cls);

    cl_ref_type & cl_ref0 = cache_handle.get()->cl_ref(0, 0);
    cl_ref_type * cl_refs = &cl_ref0;

    way = 8;
    set_id = 1;
    expected_cl_ref = set_id * ways + way;

    cl_ref_type & cl_ref = cache_handle.get()->cl_ref(set_id, way);
    EXPECT_EQ(&cl_refs[expected_cl_ref], &cl_ref);

//Out of Bounds check
//TODO:: Update this after discussions. Ideally, this should throw
#if 0
    way = 11;
    set_id = 8;
    expected_cl_ref = set_id * ways + way;
    cl_ref_type& cl_ref1 = cache_handle.get()->cl_ref(set_id, way);
    EXPECT_EQ(&cl_refs[expected_cl_ref], &cl_ref1);
#endif
}

TEST(ADSCTest_cl_id_from_key, CpuMem) {
    constexpr unsigned cl_size = 1;
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    key_type test_clid;
    key_type key;
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    EXPECT_EQ(cache_handle.get()->num_cls(), num_cls);

    key = 0;
    test_clid = cache_handle.get()->cl_id(0,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((key_type)key / cl_size, test_clid);

    key = 12;
    test_clid = cache_handle.get()->cl_id(key / cl_size,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((key_type)key / cl_size, test_clid);

    //test custom functor
    test_clid = cache_handle.get()->cl_id(0,
        gdakis::adsc::make_policies(
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            [](key_type key) { return 5; }, gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy()));

    EXPECT_EQ((key_type)5, test_clid);

    //Change the cl_size
    constexpr unsigned cl_size1 = 4;
    using cl_ref_type1 = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size1>;
    auto data_ptr1 = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size1);
    auto cache_handle1
        = gdakis::adsc::cache<scope, cl_ref_type1, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr1.get());
    EXPECT_NE(nullptr, cache_handle1.get());

    EXPECT_EQ(cache_handle1.get()->num_cls(), num_cls);
    key = 0;
    test_clid = cache_handle1.get()->cl_id(key,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((key_type)key / cl_size1, test_clid);

    key = 128;
    test_clid = cache_handle1.get()->cl_id(key,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((key_type)key / cl_size1, test_clid);
}

TEST(ADSCTest_cl_offset, CpuMem) {
    constexpr unsigned cl_size = 1;
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    unsigned test_cl_offset;
    key_type key;
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    EXPECT_EQ(cache_handle.get()->num_cls(), num_cls);

    key = 0;
    test_cl_offset = cache_handle.get()->cl_offset(key,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((unsigned)(key % cl_size), test_cl_offset);

    key = 12;
    test_cl_offset = cache_handle.get()->cl_offset(key,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((unsigned)(key % cl_size), test_cl_offset);

    //test custom functor
    test_cl_offset = cache_handle.get()->cl_offset(0,
        gdakis::adsc::make_policies(
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), [](key_type key) { return 5; }, gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy()));

    EXPECT_EQ((unsigned)5, test_cl_offset);

    //Change the cl_size
    constexpr unsigned cl_size1 = 4;
    using cl_ref_type1 = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size1>;
    auto data_ptr1 = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size1);
    auto cache_handle1
        = gdakis::adsc::cache<scope, cl_ref_type1, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr1.get());
    EXPECT_NE(nullptr, cache_handle1.get());

    EXPECT_EQ(cache_handle1.get()->num_cls(), num_cls);
    key = 0;
    test_cl_offset = cache_handle1.get()->cl_offset(key,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((unsigned)(key % cl_size1), test_cl_offset);

    key = 125;
    test_cl_offset = cache_handle1.get()->cl_offset(key,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((unsigned)(key % cl_size1), test_cl_offset);
}

TEST(ADSCTest_set_id_from_clid, CpuMem) {
    constexpr unsigned cl_size = 1;
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    gdakis::adsc::cache<scope, cl_ref_type, ways>::set_id_type test_set_id;
    unsigned cl_id;
    unsigned num_sets;
    unsigned set_id;
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    EXPECT_EQ(cache_handle.get()->num_cls(), num_cls);

    cl_id = 0;
    num_sets = num_cls / ways;
    set_id = cl_id % (num_sets);
    test_set_id = cache_handle.get()->set_id(cl_id,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((unsigned)(set_id), test_set_id);

    cl_id = 10;
    num_sets = num_cls / ways;
    set_id = cl_id % (num_sets);
    test_set_id = cache_handle.get()->set_id(cl_id,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((unsigned)(set_id), test_set_id);

    //test custom functor
    test_set_id = cache_handle.get()->set_id(cl_id,
        gdakis::adsc::make_policies(
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            [&cl_id](unsigned _cl_id, unsigned num_sets) {
                EXPECT_EQ(cl_id, _cl_id);
                EXPECT_EQ(num_cls / ways, num_sets);
                return 5;
            },
            gdakis::adsc::undefined_policy()));
    EXPECT_EQ(test_set_id, 5);

    //Change the cl_size
    constexpr unsigned cl_size1 = 4;
    using cl_ref_type1 = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size1>;
    auto data_ptr1 = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size1);
    auto cache_handle1
        = gdakis::adsc::cache<scope, cl_ref_type1, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr1.get());
    EXPECT_NE(nullptr, cache_handle1.get());

    EXPECT_EQ(cache_handle1.get()->num_cls(), num_cls);
    cl_id = 17;
    num_sets = num_cls / ways;
    set_id = cl_id % (num_sets);
    test_set_id = cache_handle1.get()->set_id(cl_id,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((unsigned)(set_id), test_set_id);

    cl_id = 100;
    num_sets = num_cls / ways;
    set_id = cl_id % (num_sets);
    test_set_id = cache_handle1.get()->cl_offset(cl_id,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((unsigned)(set_id), test_set_id);
}

TEST(ADSCTest_nth_key_of_clid, CpuMem) {
    constexpr unsigned cl_size = 1;
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    unsigned cl_id;
    unsigned n;
    key_type key, test_key_id;
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    EXPECT_NE(nullptr, cache_handle.get());

    EXPECT_EQ(cache_handle.get()->num_cls(), num_cls);

    cl_id = 0;
    n = 0;
    key = cl_id + n;
    test_key_id = cache_handle.get()->nth_key_of_cl(cl_id, n,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));

    EXPECT_EQ((test_key_id), key);

    cl_id = 0;
    n = 15;
    key = cl_id + n;
    test_key_id = cache_handle.get()->nth_key_of_cl(cl_id, n,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((test_key_id), key);

    //Change the cl_size
    constexpr unsigned cl_size1 = 4;
    using cl_ref_type1 = gdakis::adsc::cl_reference<scope, key_type, val_type, cl_size1>;
    auto data_ptr1 = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size1);
    auto cache_handle1
        = gdakis::adsc::cache<scope, cl_ref_type1, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr1.get());
    EXPECT_NE(nullptr, cache_handle1.get());

    EXPECT_EQ(cache_handle1.get()->num_cls(), num_cls);

    //test custom functor
    cl_id = 110;
    n = 3;
    key = cl_id + n;
    test_key_id = cache_handle1.get()->nth_key_of_cl(cl_id, n,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), [&cl_id, &n](unsigned _cl_id, unsigned _n) {
                EXPECT_EQ(cl_id, _cl_id);
                EXPECT_EQ(n, _n);
                return 5;
            }));
    EXPECT_EQ((test_key_id), 5);

    cl_id = 111;
    n = 3;
    key = cl_id * cl_size1 + n;
    test_key_id = cache_handle1.get()->nth_key_of_cl(cl_id, n,
        gdakis::adsc::make_policies(gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy(),
            gdakis::adsc::undefined_policy(), gdakis::adsc::undefined_policy()));
    EXPECT_EQ((test_key_id), key);
}

#ifdef ENABLE_FAILING_TESTS
//The below testcase is supposed to fill up all ways in a cache and expect all of them to be part of the cache

TEST(ADSCTest_putAndget_for_n_ways, CpuMem) {
    using val_type1 = unsigned;
    using key_type1 = unsigned;
    constexpr unsigned cl_size1 = 1;
    constexpr unsigned ways = 32;
    using cl_ref_type1 = gdakis::adsc::cl_reference<scope, key_type1, val_type1, cl_size1>;
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type1>::alloc(num_cls * cl_size1);
    EXPECT_NE(nullptr, data_ptr.get());
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type1, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    unsigned num_sets = num_cls / ways;
    EXPECT_NE(nullptr, cache_handle.get());
#  if 1
    printf("Num Sets = %d\n", num_sets);
#  endif
    unsigned put_val[ways];
    for (unsigned int i = 0; i < ways; i++) {
        put_val[i] = i + 1;
        auto ret = cache_handle.get()->put(i * num_sets, put_val[i]);
        EXPECT_EQ(0, ret);
    }
    printf("Here\n");

    val_type1 get_val[ways];
    for (int i = 0; i < ways; i++) {
        auto ret = cache_handle.get()->get(i * num_sets, get_val[i]);
        EXPECT_EQ(0, ret);
    }

    for (int i = 0; i < ways; i++) {
        EXPECT_EQ(put_val[i], get_val[i]);
    }
}

#endif

TEST(ADSCTestPutAndGetTestForNWaysValTypeArray, CpuMem) {
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type[]>::alloc(num_cls * cl_size);
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());

    EXPECT_NE(nullptr, cache_handle.get());

    val_type put_val[ways];
    for (auto i = 0ul; i < ways; i++) {
        for (auto j = 0ul; j < sizeof(val_type); j++)
            put_val[i][j] = i * ways + j + 1;
    }

    for (unsigned i = 0; i < ways; i++) {
        cache_handle.get()->put(i, put_val[i]);
    }

    val_type get_val[ways];
    for (unsigned i = 0; i < ways; i++) {
        cache_handle.get()->get(i, get_val[i]);
    }

    for (auto i = 0ul; i < ways; i++) {
        for (auto j = 0ul; j < sizeof(val_type); j++)
            EXPECT_EQ(put_val[i][j], get_val[i][j]);
    }
}

#ifdef ENABLE_FAILING_TESTS
TEST(ADSCTest_multiplePutsAndOneGet, CpuMem) {
    using val_type1 = unsigned;
    using key_type1 = unsigned;
    constexpr unsigned cl_size1 = 1;
    constexpr unsigned ways = 1;
    constexpr unsigned num_cls = 1;
    using cl_ref_type1 = gdakis::adsc::cl_reference<scope, key_type1, val_type1, cl_size1>;
    auto data_ptr = gdakis::umem::mem<gdakis::umem::cpu, val_type1>::alloc(num_cls * cl_size1);
    EXPECT_NE(nullptr, data_ptr.get());
    auto cache_handle
        = gdakis::adsc::cache<scope, cl_ref_type1, ways>::build<gdakis::umem::cpu>(num_cls, data_ptr.get());
    unsigned num_sets = num_cls / ways;
    EXPECT_NE(nullptr, cache_handle.get());
#  if DEBUG
    printf("Num Sets = %d\n", num_sets);
#  endif
    val_type1 put_val = 0, get_val = 0;

    put_val = 1;
    auto ret = cache_handle.get()->put(0, put_val);
    EXPECT_EQ(0, ret);

    put_val = 2;
    ret = cache_handle.get()->put(1, put_val);
    EXPECT_EQ(0, ret);

    ret = cache_handle.get()->get(0, get_val);

    EXPECT_NE(0, ret);

    ret = cache_handle.get()->get(1, get_val);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(put_val, get_val);
}

#endif
