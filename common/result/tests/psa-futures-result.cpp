/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "scheduler"

#if defined(__NVCC__)
#  define NAME(name) NVCC##name
#else
#  define NAME(name) name
#endif

TEST(NAME(AsyncResultBuild), Ok) {
    auto ret = scheduler<>::result_type::ok(true).sync();
    EXPECT_FALSE(ret.is_err());
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(AsyncResultBuild), Err) {
    auto ret = scheduler<>::result_type::err(0xdeadbeef).sync();
    EXPECT_FALSE(ret.is_ok());
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), 0xdeadbeef);
    EXPECT_THROW(ret.ok(), std::bad_variant_access);
}

TEST(NAME(AsyncResultBuild), OkCtor) {
    using result_type = psa::futures::result<std::tuple<>, int>;
    auto ret = result_type::ok<>().sync();
    EXPECT_FALSE(ret.is_err());
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), std::tuple<>());
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(AsyncResultBuild), ErrCtor) {
    using result_type = psa::futures::result<int, std::tuple<>>;
    auto ret = result_type::err<>().sync();
    EXPECT_FALSE(ret.is_ok());
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), std::tuple<>());
    EXPECT_THROW(ret.ok(), std::bad_variant_access);
}

TEST(NAME(AsyncResultSched), Ok) {
    scheduler<> sched;
    auto ret = sched.async_ok(true).sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_FALSE(ret.is_err());
    EXPECT_EQ(ret.ok(), true);
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(AsyncResultSched), Err) {
    scheduler<> sched;
    auto ret = sched.async_err(0xdeadbeef).sync();
    EXPECT_FALSE(ret.is_ok());
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), 0xdeadbeef);
    EXPECT_THROW(ret.ok(), std::bad_variant_access);
}

template<typename _Ok = bool, typename _Err = int>
struct sched_wrapper {
    scheduler<_Ok, _Err> sched;

    using result_type = typename scheduler<_Ok, _Err>::result_type;

    result_type async_ok(_Ok && value) { return sched.async_ok(std::forward<_Ok>(value)); }

    result_type async_err(_Err && value) { return sched.async_err(std::forward<_Err>(value)); }
};

template<typename _Sched>
static scheduler<>::result_type async_ok(_Sched & sched) {
    co_return co_await sched.async_ok(true);
}

template<typename _Sched>
static scheduler<>::result_type async_err(_Sched & sched) {
    co_return co_await sched.async_err(0xdeadbeef);
}

TEST(NAME(AsyncResultCowait), ChainSchedulerOk) {
    scheduler<> sched;
    auto pending = async_ok(sched);
    auto ret = pending.sync();

    EXPECT_TRUE(ret.is_ok());
    EXPECT_FALSE(ret.is_err());
    EXPECT_EQ(ret.ok(), true);
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(AsyncResultCowait), IndirectSchedulerOk) {
    sched_wrapper sched;
    auto pending = async_ok(sched);
    auto ret = pending.sync();

    EXPECT_TRUE(ret.is_ok());
    EXPECT_FALSE(ret.is_err());
    EXPECT_EQ(ret.ok(), true);
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(AsyncResultCowait), ChainSchedulerErr) {
    scheduler<> sched;
    auto pending = async_err(sched);
    auto ret = pending.sync();

    EXPECT_FALSE(ret.is_ok());
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), 0xdeadbeef);
    EXPECT_THROW(ret.ok(), std::bad_variant_access);
}

TEST(NAME(AsyncResultCowait), IndirectSchedulerErr) {
    sched_wrapper sched;
    auto pending = async_err(sched);
    auto ret = pending.sync();

    EXPECT_FALSE(ret.is_ok());
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), 0xdeadbeef);
    EXPECT_THROW(ret.ok(), std::bad_variant_access);
}

using result_type = psa::result<bool, int>;
using int_result_type = psa::result<int, int>;

static sched_wrapper<>::result_type async_result_invert(sched_wrapper<> & sched) {
    auto ret = co_await sched.async_ok(true);
    co_return ret.is_ok() ? result_type::ok(!ret.ok()) : std::move(ret);
}

TEST(NAME(AsyncResultCoawait), Chain) {
    sched_wrapper<> sched;
    auto ret = async_result_invert(sched).sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_FALSE(ret.is_err());
    EXPECT_EQ(ret.ok(), false);
    EXPECT_THROW(ret.err(), std::bad_variant_access);
}

TEST(NAME(AsyncResultMap), Ok) {
    scheduler<> sched;
    auto ret = sched.async_ok(true).map([](bool) { return 0xcafe; }).sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), 0xcafe);
}

TEST(NAME(AsyncResultMap), Err) {
    scheduler<> sched;
    auto ret = sched.async_err(EINVAL).map([](bool) { return 0xcafe; }).sync();
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(AsyncResultMapErr), Ok) {
    scheduler<> sched;
    auto ret = sched.async_ok(true).map_err([](int err) { return err == EINVAL ? true : false; }).sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}

TEST(NAME(AsyncResultMapErr), Err) {
    scheduler<> sched;
    auto ret = sched.async_err(EINVAL).map_err([](int err) { return err == EINVAL ? true : false; }).sync();
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), true);
}

TEST(NAME(AsyncResultAndThen), OkOk) {
    scheduler<> sched;
    auto ret = sched.async_ok(false).and_then([](bool value) { return int_result_type::ok(0xcafe); }).sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), 0xcafe);
}

TEST(NAME(AsyncResultAndThen), OkErr) {
    scheduler<> sched;
    auto ret = sched.async_ok(true).and_then([](bool) { return result_type::err(EINVAL); }).sync();
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(AsyncResultAndThen), Err) {
    scheduler<> sched;
    auto ret = sched.async_err(EINVAL).and_then([](bool value) { return result_type::ok(value); }).sync();
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(AsyncResultAndThen), OkOkOk) {
    scheduler<> sched;
    auto ret = sched.async_ok(false)
                   .and_then([](bool) { return int_result_type::ok(0xcafe); })
                   .and_then([](int) { return result_type::ok(true); })
                   .sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}

TEST(NAME(AsyncResultAndThen), OkOkErr) {
    scheduler<> sched;
    auto ret = sched.async_ok(false)
                   .and_then([](bool) { return int_result_type::ok(0xcafe); })
                   .and_then([](int) { return result_type::err(EINVAL); })
                   .sync();
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(AsyncResultAndThen), OkErrOk) {
    scheduler<> sched;
    auto ret = sched.async_ok(false)
                   .and_then([](bool) { return result_type::err(EINVAL); })
                   .and_then([](bool value) { return int_result_type::ok(0xcafe); })
                   .sync();
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(AsyncResultAndThen), ChainOk) {
    sched_wrapper<> sched;
    auto ret = async_result_invert(sched).and_then([](bool value) { return int_result_type::ok(0xcafe); }).sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), 0xcafe);
}

TEST(NAME(AsyncResultAndThen), ChainErr) {
    sched_wrapper<> sched;
    auto ret = async_result_invert(sched).and_then([](bool) { return result_type::err(EINVAL); }).sync();
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EINVAL);
}

TEST(NAME(AsyncResultOrElse), ErrErr) {
    scheduler<> sched;
    auto ret = sched.async_err(EINVAL).or_else([](int value) { return result_type::err(EIO); }).sync();
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EIO);
}

TEST(NAME(AsyncResultOrElse), ErrOk) {
    scheduler<> sched;
    auto ret = sched.async_err(EINVAL).or_else([](int) { return result_type::ok(true); }).sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}

TEST(NAME(AsyncResultOrElse), Ok) {
    scheduler<> sched;
    auto ret = sched.async_ok(true).or_else([](int) { return result_type::err(EIO); }).sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}

TEST(NAME(AsyncResultOrElse), ErrErrErr) {
    scheduler<> sched;
    auto ret = sched.async_err(EINVAL)
                   .or_else([](int) { return result_type::err(EIO); })
                   .or_else([](int) { return result_type::err(EFAULT); })
                   .sync();
    EXPECT_TRUE(ret.is_err());
    EXPECT_EQ(ret.err(), EFAULT);
}

TEST(NAME(AsyncResultOrElse), ErrErrOk) {
    scheduler<> sched;
    auto ret = sched.async_err(EINVAL)
                   .or_else([](int) { return result_type::err(EIO); })
                   .or_else([](int) { return result_type::ok(true); })
                   .sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}

TEST(NAME(AsyncResultOrElse), ErrOkErr) {
    scheduler<> sched;
    auto ret = sched.async_err(EINVAL)
                   .or_else([](int) { return result_type::ok(true); })
                   .or_else([](int) { return result_type::err(EIO); })
                   .sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}

TEST(NAME(AsyncResultThenElse), OkErrOk) {
    sched_wrapper<> sched;
    auto ret = async_result_invert(sched)
                   .and_then([](bool) { return result_type::err(EINVAL); })
                   .or_else([](int err) { return result_type::ok(err == EINVAL); })
                   .sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), true);
}

TEST(NAME(AsyncResultThenElse), OkOkOk) {
    sched_wrapper<> sched;
    auto ret = async_result_invert(sched)
                   .and_then([](bool) { return int_result_type::ok(0xcafe); })
                   .or_else([](int) { return int_result_type::ok(0xdead); })
                   .sync();
    EXPECT_TRUE(ret.is_ok());
    EXPECT_EQ(ret.ok(), 0xcafe);
}
