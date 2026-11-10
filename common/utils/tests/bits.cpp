/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/bits>
#include <gtest/gtest.h>

template<typename _Tp>
struct Bits : testing::Test {};

using test_types = testing::Types<std::uint32_t, std::uint64_t>;

TYPED_TEST_SUITE(Bits, test_types);

TYPED_TEST(Bits, ZerosPopCount) {
    TypeParam value = 0;
    ASSERT_EQ(gdakis::popc(value), 0);
}

TYPED_TEST(Bits, OnesPopCount) {
    const size_t num_bits = sizeof(TypeParam) << 3;
    TypeParam value = ~(static_cast<TypeParam>(0));
    ASSERT_EQ(gdakis::popc(value), num_bits);
}

TYPED_TEST(Bits, GroupPopCount) {
    const size_t num_bits = sizeof(TypeParam) << 3;
    for (auto n = 1ul; n < num_bits; ++n) {
        TypeParam value = (static_cast<TypeParam>(1) << n) - 1;
        for (auto m = 0ul; m < num_bits - n + 1; m += n) {
            ASSERT_EQ(gdakis::popc(value << m), n)
                << "bits = " << num_bits << " n = " << n << " value = 0x" << std::hex << value << std::dec;
        }
    }
}

TYPED_TEST(Bits, OnePopCount) {
    const size_t num_bits = sizeof(TypeParam) << 3;
    for (auto n = 1ul; n < num_bits; ++n) {
        TypeParam value = 0;
        for (auto m = 0ul; m < num_bits; m += (n << 1)) {
            value = (value << (n << 1)) | ((static_cast<TypeParam>(1) << n) - 1);
        }
        auto set_bits = num_bits / (n << 1) * n;
        auto extra = num_bits % (n << 1);
        extra = extra > n ? n : extra;
        set_bits += extra;
        ASSERT_EQ(gdakis::popc(value), set_bits) << "n = " << n << " value = 0x" << std::hex << value << std::dec;
    }
}

TYPED_TEST(Bits, ByteSwap) {
    TypeParam value = 0;
    for (auto n = 0ul; n < sizeof(TypeParam); n++)
        value = (value << 8) | 0xf;
    ASSERT_EQ(gdakis::bswap(value), value) << "num_bits = " << (sizeof(TypeParam) << 3) << " value = " << std::hex
                                           << value << " swap = " << gdakis::bswap(value) << std::dec;
}

TYPED_TEST(Bits, HalfSwap) {
    TypeParam value = 0;
    for (auto n = 0ul; n < sizeof(TypeParam) >> 1; n++)
        value = (value << 16) | 0xff;
    ASSERT_EQ(gdakis::bswap(value) & value, 0) << "num_bits = " << (sizeof(TypeParam) << 3) << " value = " << std::hex
                                               << value << " swap = " << gdakis::bswap(value) << std::dec;
    ASSERT_EQ(gdakis::bswap(value) | value, ~(static_cast<TypeParam>(0)))
        << "num_bits = " << (sizeof(TypeParam) << 3) << " value = " << std::hex << value
        << " swap = " << gdakis::bswap(value) << std::dec;
}

namespace {

template<typename _Tp>
static inline constexpr _Tp shift_word(const _Tp &) noexcept;

template<>
inline constexpr std::uint32_t shift_word<std::uint32_t>(const std::uint32_t & value) noexcept {
    return 0;
}

template<>
inline constexpr std::uint64_t shift_word<std::uint64_t>(const std::uint64_t & value) noexcept {
    return value << 32;
}

}

TYPED_TEST(Bits, WordSwap) {
    TypeParam value = 0;
    for (auto n = 0ul; n < sizeof(TypeParam) >> 2; n++) {
        value = shift_word(value) | 0xffff;
    }
    ASSERT_EQ(gdakis::bswap(value) & value, 0) << "num_bits = " << (sizeof(TypeParam) << 3) << " value = " << std::hex
                                               << value << " swap = " << gdakis::bswap(value) << std::dec;
    ASSERT_EQ(gdakis::bswap(value) | value, ~(static_cast<TypeParam>(0)))
        << "num_bits = " << (sizeof(TypeParam) << 3) << " value = " << std::hex << value
        << " swap = " << gdakis::bswap(value) << std::dec;
}

TYPED_TEST(Bits, SingleBit) {
    const size_t num_bits = sizeof(TypeParam) << 3;
    for (unsigned n = 0; n < num_bits; ++n) {
        TypeParam value = static_cast<TypeParam>(1) << n;
        ASSERT_EQ(gdakis::ffs(value), n + 1);
        ASSERT_EQ(gdakis::fls(value), n + 1)
            << "num_bits = " << num_bits << " value = 0x" << std::hex << value << std::dec;
    }
}

TYPED_TEST(Bits, ReflectedBits) {
    const size_t num_bits = sizeof(TypeParam) << 3;
    for (unsigned n = 0; n < num_bits >> 1; ++n) {
        TypeParam value = static_cast<TypeParam>(1) << n | static_cast<TypeParam>(1) << (num_bits - 1 - n);
        ASSERT_EQ(gdakis::ffs(value), n + 1);
        ASSERT_EQ(gdakis::fls(value), num_bits - n)
            << "num_bits = " << num_bits << " value = 0x" << std::hex << value << std::dec;
    }
}

TYPED_TEST(Bits, GroupBits) {
    const size_t num_bits = sizeof(TypeParam) << 3;
    for (auto n = 1ul; n < num_bits; ++n) {
        TypeParam value = (static_cast<TypeParam>(1) << n) - 1;
        for (auto m = 0ul; m < num_bits - n + 1; m += n) {
            ASSERT_EQ(gdakis::ffs(value << m), m + 1)
                << "bits = " << num_bits << " n = " << n << " value = 0x" << std::hex << value << std::dec;
            ASSERT_EQ(gdakis::fls(value << m), (m + n))
                << "bits = " << num_bits << " n = " << n << " value = 0x" << std::hex << value << std::dec;
        }
    }
}
