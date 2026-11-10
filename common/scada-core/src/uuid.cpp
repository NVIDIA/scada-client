/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <scada/uuid>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <type_traits>

#include <cstdint>
#include <cstring>

#include <iostream>

SCADA_CORE_SIMPLE_NS {
SCADA_CORE_UNIT(uuid) {

namespace {

static constexpr std::array<std::uint32_t, 5> H = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };

template<std::size_t I, typename = void>
struct R;

template<std::size_t I>
struct R<I, std::enable_if_t<(I >= 0 and I <= 19)>> {
    static constexpr std::uint32_t k = 0x5A827999;
    static inline constexpr std::uint32_t f(
        const std::uint32_t & B, const std::uint32_t & C, const std::uint32_t & D) noexcept {
        return (B & C) | ((~B) & D);
    }
};

template<std::size_t I>
struct R<I, std::enable_if_t<(I >= 20 and I <= 39)>> {
    static constexpr std::uint32_t k = 0x6ED9EBA1;
    static inline constexpr std::uint32_t f(
        const std::uint32_t & B, const std::uint32_t & C, const std::uint32_t & D) noexcept {
        return B ^ C ^ D;
    }
};

template<std::size_t I>
struct R<I, std::enable_if_t<(I >= 40 and I <= 59)>> {
    static constexpr std::uint32_t k = 0x8F1BBCDC;
    static inline constexpr std::uint32_t f(
        const std::uint32_t & B, const std::uint32_t & C, const std::uint32_t & D) noexcept {
        return (B & C) | (B & D) | (C & D);
    }
};

template<std::size_t I>
struct R<I, std::enable_if_t<(I >= 60 and I <= 79)>> {

    static constexpr std::uint32_t k = 0xCA62C1D6;
    static inline constexpr std::uint32_t f(
        const std::uint32_t & B, const std::uint32_t & C, const std::uint32_t & D) noexcept {
        return B ^ C ^ D;
    }
};

template<std::size_t _B>
static constexpr inline std::uint32_t shift(const std::uint32_t & value) noexcept {
    return (value << _B) | (value >> (32 - _B));
}

template<std::size_t T = 0, typename = void>
struct I;

template<std::size_t T>
struct I<T, std::enable_if_t<(T > 79)>> {
    static inline constexpr void run(std::array<std::uint32_t, 5> & N, std::array<std::uint32_t, 16> & W) noexcept {}
};

template<std::size_t T>
struct I<T, std::enable_if_t<(T >= 16 and T <= 79)>> {
    static inline constexpr void run(std::array<std::uint32_t, 5> & N, std::array<std::uint32_t, 16> & W) noexcept {
        auto s = T & 0xf;
        W[s] = shift<1>(W[(s + 13) & 0xf] ^ W[(s + 8) & 0xf] ^ W[(s + 2) & 0xf] ^ W[s]);
        auto temp = shift<5>(N[0]) + R<T>::f(N[1], N[2], N[3]) + N[4] + W[s] + R<T>::k;
        N = { temp, N[0], shift<30>(N[1]), N[2], N[3] };
        I<T + 1>::run(N, W);
    }
};

template<std::size_t T>
struct I<T, std::enable_if_t<(T >= 0 and T < 16)>> {
    static inline constexpr void run(std::array<std::uint32_t, 5> & N, std::array<std::uint32_t, 16> & W) noexcept {
        auto temp = shift<5>(N[0]) + R<T>::f(N[1], N[2], N[3]) + N[4] + W[T] + R<T>::k;
        N = { temp, N[0], shift<30>(N[1]), N[2], N[3] };
        I<T + 1>::run(N, W);
    }
};

static inline std::array<std::uint32_t, 5> sha1(const std::string & message) noexcept {
    std::array<std::uint8_t, 64> B;
    std::array<std::uint32_t, 5> N = H;

    std::uint64_t len = message.length();
    auto number_of_blocks = (len + sizeof(std::uint64_t) + sizeof(B)) / sizeof(B);

    for (std::size_t n = 0; n < number_of_blocks; ++n) {
        auto offset = n * sizeof(B);
        if (offset <= len) {
            auto size = (len - offset) < sizeof(B) ? len - offset : sizeof(B);
            ::memcpy(B.data(), message.c_str() + offset, size);
            if (size != sizeof(B)) {
                B[size] = 0x80;
                ::memset(B.data() + size + 1, 0, sizeof(B) - size - 1);
            }
        } else {
            ::memset(B.data(), 0, sizeof(B));
        }

        // TODO: Check for endianness to avoid this
        std::uint32_t * ptr = reinterpret_cast<std::uint32_t *>(B.data());
        for (auto n = 0; n < 16; ++n) {
            ptr[n] = __builtin_bswap32(ptr[n]);
        }

        if ((offset + sizeof(B) - sizeof(std::uint64_t)) > len) {
            *reinterpret_cast<std::uint32_t *>(B.data() + sizeof(B) - sizeof(std::uint64_t)) = (len << 3) >> 32;
            ;
            *reinterpret_cast<std::uint32_t *>(B.data() + sizeof(B) - sizeof(std::uint32_t)) = (len << 3) & 0xffffffff;
        }

        I<>::run(N, reinterpret_cast<std::array<std::uint32_t, 16> &>(B));

        N = { N[0] + H[0], N[1] + H[1], N[2] + H[2], N[3] + H[3], N[4] + H[4] };
    }

    return N;
}

}

uuid::uuid(const std::string & message) noexcept {
    auto hash = sha1(message);
    ::memcpy(bytes_.data(), hash.data(), sizeof(bytes_));
}

uuid::uuid(value_type && value) noexcept : bytes_(std::move(value)) {}

const typename uuid::value_type & uuid::value() const noexcept {
    return bytes_;
}

std::string uuid::to_string() const noexcept {
    std::stringstream ss;
    const std::array<std::uint16_t, 8> & half = reinterpret_cast<const std::array<std::uint16_t, 8> &>(bytes_);

    ss << std::setbase(16) << half[1] << half[0] << "-" << half[3] << "-" << half[2] << "-" << half[5] << "-" << half[4]
       << half[7] << half[6];

    return ss.str();
}

bool uuid::operator==(const uuid & other) const noexcept {
    return ::memcmp(bytes_.data(), other.bytes_.data(), sizeof(value_type)) == 0;
}

bool uuid::operator!=(const uuid & other) const noexcept {
    return ::memcmp(bytes_.data(), other.bytes_.data(), sizeof(value_type)) != 0;
}

}}
