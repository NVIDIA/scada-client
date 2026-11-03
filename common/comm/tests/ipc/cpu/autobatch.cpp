/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/ipc/autobatch>
#include <gdakis/ipc/protocol>
#include <gdakis/protocol>
#include <gdakis/umem/cpu>
#include <tests/utility>

namespace ns = gdakis::comm::ipc;
TEST(CPUIPCAutobatchTest, Create) {
    auto creator = tests::process([] {
        auto autobatch
            = ns::autobatch<gdakis::umem::cpu, unsigned, gdakis::thread_scope_device, 1>::handle::build(1024);
        ASSERT_TRUE(autobatch.is_ok());
    });
    ASSERT_TRUE(creator.wait());
}

TEST(CPUIPCAutobatchTest, ExportTo) {
    auto creator = tests::process([] {
        auto autobatch
            = ns::autobatch<gdakis::umem::cpu, unsigned, gdakis::thread_scope_device, 1>::handle::build(1024);
        ASSERT_TRUE(autobatch.is_ok());
        auto msg = autobatch.ok().request();
        ASSERT_TRUE(msg.is_ok());
        ASSERT_EQ(msg.ok().get().header_.op_, gdakis::comm::operation::register_request_buffers);
        ASSERT_EQ(msg.ok().get().data_.req.num_buffers, 1);
        ASSERT_EQ(msg.ok().get().data_.req.entry_size, sizeof(unsigned));
        ASSERT_EQ(msg.ok().get().data_.req.buffer_size, 1024);
        ASSERT_NE(std::get<0>(msg.ok().ancillary()), -1);
        ASSERT_NE(std::get<1>(msg.ok().ancillary()), -1);
    });
    ASSERT_TRUE(creator.wait());
}
