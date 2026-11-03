/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <mutex>
#include <psa/result>

#if defined(__cpp_lib_coroutine)
#  include "scheduler"
#endif

#if defined(__NVCC__)
#  define NAME(name) NVCC##name
#else
#  define NAME(name) name
#endif

template<typename _Tp = bool>
using result_type = psa::result<_Tp, int>;

TEST(NAME(SyncResultBuild), Ok) {
    auto ret = result_type<>::ok(true);
    EXPECT_TRUE(ret.is_ok());
    EXPECT_FALSE(ret.is_err());
    EXPECT_EQ(ret.ok(), true);
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(SyncResultBuild), OkCtor) {
    auto ret = result_type<std::tuple<>>::ok<>();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_FALSE(ret.is_err());
    EXPECT_EQ(ret.ok(), std::tuple<>());
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(SyncResultBuild), Err) {
    auto ret = result_type<>::err(0xdeadbeef);
    EXPECT_FALSE(ret.is_ok());
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), 0xdeadbeef);
    EXPECT_THROW(ret.ok(), std::bad_variant_access);
}

TEST(NAME(SyncResultBuild), ErrCtor) {
    using result_type = psa::result<int, std::tuple<>>;
    auto ret = result_type::err<>();
    EXPECT_TRUE(ret.is_err());
    EXPECT_FALSE(ret.is_ok());
    EXPECT_EQ(ret.err(), std::tuple<>());
    EXPECT_THROW(ret.ok(), std::bad_variant_access);
}

TEST(NAME(SyncResultMap), Ok) {
    auto ret = result_type<>::ok(true).map([](bool v) { return !v; });
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), false);
}

TEST(NAME(SyncResult), OkSharedPtr) {
    using ptr_type = std::shared_ptr<unsigned>;
    auto ret = result_type<ptr_type>::ok(std::make_shared<unsigned>(4));
    EXPECT_TRUE(ret.is_ok());
    EXPECT_FALSE(ret.is_err());
    EXPECT_NE(ret.ok(), nullptr);
    EXPECT_EQ(*ret.ok(), 4);
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(SyncResult), OkSharedPtrMove) {
    using ptr_type = std::shared_ptr<unsigned>;
    auto ret = result_type<ptr_type>::ok(std::make_shared<unsigned>(4));
    EXPECT_TRUE(ret.is_ok());
    EXPECT_NE(ret.ok(), nullptr);
    EXPECT_EQ(*ret.ok(), 4);

    auto other = result_type<ptr_type>::ok(ret.take_ok());
    EXPECT_TRUE(other.is_ok());
    EXPECT_NE(other.ok(), nullptr);
    EXPECT_EQ(*other.ok(), 4);

    EXPECT_EQ(ret.ok(), nullptr);
}

TEST(NAME(SyncResult), OkSharedPtrCopy) {
    using ptr_type = std::shared_ptr<unsigned>;
    auto ret = result_type<ptr_type>::ok(std::make_shared<unsigned>(4));
    EXPECT_TRUE(ret.is_ok());
    EXPECT_NE(ret.ok(), nullptr);
    EXPECT_EQ(*ret.ok(), 4);

    auto other = result_type<ptr_type>::ok(ret.ok());
    EXPECT_TRUE(other.is_ok());
    EXPECT_NE(other.ok(), nullptr);
    EXPECT_EQ(*other.ok(), 4);

    EXPECT_NE(ret.ok(), nullptr);
    EXPECT_EQ(*ret.ok(), 4);
}

TEST(NAME(SyncResult), OkUniquePtr) {
    using ptr_type = std::unique_ptr<unsigned>;
    auto ret = result_type<ptr_type>::ok(std::make_unique<unsigned>(4));
    EXPECT_TRUE(ret.is_ok());
    EXPECT_FALSE(ret.is_err());
    EXPECT_NE(ret.ok(), nullptr);
    EXPECT_EQ(*ret.ok(), 4);
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(SyncResult), OkUniquePtrMove) {
    using ptr_type = std::unique_ptr<unsigned>;
    auto ret = result_type<ptr_type>::ok(std::make_unique<unsigned>(4));
    EXPECT_TRUE(ret.is_ok());
    EXPECT_NE(ret.ok(), nullptr);
    EXPECT_EQ(*ret.ok(), 4);

    auto other = result_type<ptr_type>::ok(ret.take_ok());
    EXPECT_TRUE(other.is_ok());
    EXPECT_NE(other.ok(), nullptr);
    EXPECT_EQ(*other.ok(), 4);

    EXPECT_EQ(ret.ok(), nullptr);
}

class complex_type {
protected:
    std::mutex mutex_;
    unsigned value_;

public:
    inline complex_type(unsigned value) noexcept : value_(value) {}

    inline complex_type(complex_type && other) noexcept : value_(other.value_) { other.value_ = ~value_; }

    inline unsigned get() const noexcept { return value_; }
};

TEST(NAME(SyncResult), OkComplexType) {
    auto ret = result_type<complex_type>::ok(0xfeedcafe);
    EXPECT_TRUE(ret.is_ok());
    EXPECT_FALSE(ret.is_err());
    EXPECT_EQ(ret.ok().get(), 0xfeedcafe);
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(SyncResult), OkComplexTypeMove) {
    auto ret = result_type<complex_type>::ok(0xdeadbeef);
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok().get(), 0xdeadbeef);

    auto other = result_type<complex_type>::ok(ret.take_ok());
    EXPECT_TRUE(other.is_ok());
    EXPECT_EQ(other.ok().get(), 0xdeadbeef);

    EXPECT_EQ(ret.ok().get(), ~0xdeadbeef);
}

TEST(NAME(SyncResult), ErrSimple) {
    auto ret = result_type<>::err(0xfeedbeef);
    EXPECT_FALSE(ret.is_ok());
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), 0xfeedbeef);
    EXPECT_THROW(ret.ok(), std::bad_variant_access);
}

TEST(NAME(SyncResult), MapOk) {
    auto ret = result_type<bool>::ok(true).map([](bool v) { return !v; });
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), false);
}

TEST(NAME(SyncResult), MapErr) {
    auto ret = result_type<>::err(EINVAL).map([](bool) { return true; });
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(SyncResultMap), Err) {
    auto ret = result_type<>::err(EINVAL).map([](bool) { return true; });
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(SyncResultMapErr), Ok) {
    auto ret = result_type<>::ok(true).map_err([](int err) { return err == EINVAL ? true : false; });
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}

TEST(NAME(SyncResultMapErr), Err) {
    auto ret = result_type<>::err(EINVAL).map_err([](int err) { return err == EINVAL ? true : false; });
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), true);
}

TEST(NAME(SyncResultAndThen), Ok) {
    auto ret = result_type<>::ok(true).and_then([](bool val) { return result_type<>::ok(!val); });
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), false);
}

TEST(NAME(SyncResultAndThen), Err) {
    auto ret = result_type<>::ok(true).and_then([](bool) { return result_type<>::err(EINVAL); });
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(SyncResultAndThen), None) {
    auto ret = result_type<>::err(EINVAL).and_then([](int) { return result_type<>::ok(true); });
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(SyncResultOrElse), Ok) {
    auto ret = result_type<>::err(EINVAL).or_else([](int) { return result_type<>::ok(true); });
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}

TEST(NAME(SyncResultOrElse), Err) {
    auto ret = result_type<>::err(EINVAL).or_else([](int) { return result_type<>::err(EIO); });
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EIO);
}

TEST(NAME(SyncResultOrElse), None) {
    auto ret = result_type<>::ok(true).or_else([](bool) { return result_type<>::err(EINVAL); });
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}
