/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <scada/io>
#include <scada/traits>
#include <scada/uuid>

TEST(UUID, Empty) {
    [[maybe_unused]] scada::store_traits<scada::core::io::socket::domain::local> t;
    scada::uuid uuid("");
    ASSERT_EQ(uuid.to_string(), "da39a3ee-5e6b-4b0d-3255-bfef95601890");
}

TEST(UUID, Simple) {
    scada::uuid uuid("test-string");
    ASSERT_EQ(uuid.to_string(), "4f49d696-13b1-86e7-1104-c7ca1b26c1e5");
}
