/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <gdakis/umem/cpu>
#include <gdakis/umem/mem>

template<typename _Allocator>
struct Mem : testing::Test {};

using test_types = ::testing::Types<gdakis::umem::cpu>;
TYPED_TEST_SUITE(Mem, test_types);

template<typename _Allocator, typename _Fn>
struct checker;

template<typename _Allocator, typename _Fn>
checker<_Allocator, _Fn> check(_Fn && fn) {
    return checker<_Allocator, _Fn>(std::forward<_Fn>(fn));
}

template<typename _Fn>
struct checker<gdakis::umem::cpu, _Fn> {
    _Fn fn_;

    checker(_Fn && fn) noexcept : fn_(std::move(fn)) {}

    template<typename... Args>
    inline auto eval(Args &&... args) {
        return fn_(std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline auto eval_foreach(std::size_t n_elems, Args &&... args) {
        std::vector<std::invoke_result_t<_Fn, std::size_t, Args...>> ret;
        for (auto n = 0ul; n < n_elems; ++n) {
            ret.emplace_back(fn_(n, std::forward<Args>(args)...));
        }
        return ret;
    }
};

struct object {
    unsigned value_;
    unsigned not_value_;

    inline object() noexcept : value_(0), not_value_(~0) {}

    inline object(unsigned value) noexcept : value_(value), not_value_(~value) {};
};

TYPED_TEST(Mem, Alloc) {
    auto ptr = gdakis::umem::mem<TypeParam, object>::alloc(0xcafecafe);
    auto value_check = check<TypeParam>([](object * obj) { return obj->value_ == 0xcafecafe; }).eval(ptr.get());
    ASSERT_TRUE(value_check);
    auto not_value_check
        = check<TypeParam>([](object * obj) { return obj->value_ == ~(obj->not_value_); }).eval(ptr.get());
    ASSERT_TRUE(not_value_check);
}

TYPED_TEST(Mem, AllocNotify) {
    object * ref = nullptr;
    object * raw = nullptr;
    {
        auto ptr = gdakis::umem::mem<TypeParam, object>::alloc_notify([&raw](object * ptr) { raw = ptr; }, 0xdeadbeef);
        auto value_check = check<TypeParam>([](object * obj) { return obj->value_ == 0xdeadbeef; }).eval(ptr.get());
        ASSERT_TRUE(value_check);
        auto not_value_check
            = check<TypeParam>([](object * obj) { return obj->value_ == ~(obj->not_value_); }).eval(ptr.get());
        ASSERT_TRUE(not_value_check);
        ref = ptr.get();
        ASSERT_NE(raw, ref);
    }
    ASSERT_EQ(raw, ref);
}

TYPED_TEST(Mem, AllocSystem) {
    auto ptr = gdakis::umem::mem<TypeParam, object>::alloc_system(0xcafecafe);
    auto value_check = check<TypeParam>([](object * obj) { return obj->value_ == 0xcafecafe; }).eval(ptr.get());
    ASSERT_TRUE(value_check);
    auto not_value_check
        = check<TypeParam>([](object * obj) { return obj->value_ == ~(obj->not_value_); }).eval(ptr.get());
    ASSERT_TRUE(not_value_check);
}

TYPED_TEST(Mem, AllocArray) {
    auto ptr = gdakis::umem::mem<TypeParam, object[]>::alloc(1024);
    gdakis::umem::memset<TypeParam>(ptr, 0, 1024 * sizeof(object));
    auto value_check = check<TypeParam>([](std::size_t n, object * obj) {
        return obj[n].value_ == 0;
    }).eval_foreach(1024, ptr.get());
    for (const auto v : value_check) {
        ASSERT_TRUE(v);
    }

    auto not_value_check = check<TypeParam>([](std::size_t n, object * obj) {
        return obj[n].not_value_ == 0;
    }).eval_foreach(1024, ptr.get());
    for (const auto v : not_value_check) {
        ASSERT_TRUE(v);
    }
}

TYPED_TEST(Mem, AllocArrayMap) {
    auto ptr = gdakis::umem::mem<TypeParam, object[]>::alloc(1024, [](std::size_t id, object * obj) {
        obj->value_ = id;
        obj->not_value_ = ~0;
    });
    auto value_check = check<TypeParam>([](std::size_t n, object * obj) {
        return obj[n].value_ == n;
    }).eval_foreach(1024, ptr.get());
    for (const auto v : value_check) {
        ASSERT_TRUE(v);
    }

    auto not_value_check = check<TypeParam>([](std::size_t n, object * obj) {
        return obj[n].not_value_ == ~0u;
    }).eval_foreach(1024, ptr.get());
    for (const auto v : not_value_check) {
        ASSERT_TRUE(v);
    }
}

TYPED_TEST(Mem, AllocSystemArray) {
    auto ptr = gdakis::umem::mem<TypeParam, object[]>::alloc_system(1024);
    // Cast to avoid -Wclass-memaccess compiler warning.
    // OK because this test wants to explicitly check the bits before the
    // ctor touches the instance vars.
    ::memset(static_cast<void *>(ptr.get()), 0, 1024 * sizeof(object));
    auto value_check = check<TypeParam>([](std::size_t n, object * obj) {
        return obj[n].value_ == 0;
    }).eval_foreach(1024, ptr.get());
    for (const auto v : value_check) {
        ASSERT_TRUE(v);
    }

    auto not_value_check = check<TypeParam>([](std::size_t n, object * obj) {
        return obj[n].not_value_ == 0;
    }).eval_foreach(1024, ptr.get());
    for (const auto v : not_value_check) {
        ASSERT_TRUE(v);
    }
}

TYPED_TEST(Mem, AllocSystemArrayMap) {
    auto ptr = gdakis::umem::mem<TypeParam, object[]>::alloc_system(1024, [](std::size_t id, object * obj) {
        obj->value_ = id;
        obj->not_value_ = ~0;
    });
    auto value_check = check<TypeParam>([](std::size_t n, object * obj) {
        return obj[n].value_ == n;
    }).eval_foreach(1024, ptr.get());
    for (const auto v : value_check) {
        ASSERT_TRUE(v);
    }

    auto not_value_check = check<TypeParam>([](std::size_t n, object * obj) {
        return obj[n].not_value_ == ~0u;
    }).eval_foreach(1024, ptr.get());
    for (const auto v : not_value_check) {
        ASSERT_TRUE(v);
    }
}
