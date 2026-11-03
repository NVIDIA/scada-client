/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>
#include <gdakis/ipc/memory_buffer>
#include <gdakis/ipc/shmem>
#include <gdakis/protocol>
#include <tests/utility>
#include <unistd.h>
namespace ns = gdakis::comm::ipc;
class cpu_ipc_io_memory_buffer : public ::testing::Test {
protected:
    void SetUp() override {
        if (ns::shmem<gdakis::umem::cpu, void>::supports_dma() == false) {
            GTEST_SKIP() << "CPU DMA memory no available";
        }
    }
};

TEST(CPUIPCMemoryBuffer, Create) {
    auto buffer = ns::memory_buffer<unsigned>::template handle<gdakis::umem::cpu>::build(
        1024, [](unsigned id, unsigned * value) { *value = id; });
    ASSERT_TRUE(buffer.is_ok());
    ASSERT_NE(buffer.ok().get().data(), nullptr);
    for (unsigned n = 0; n < 1024; ++n) {
        ASSERT_EQ(buffer.ok().get().data()[n], n);
    }
}

TEST(CPUIPCMemoryBuffer, SendToServer) {
    auto buffer = ns::memory_buffer<unsigned>::template handle<gdakis::umem::cpu>::build(
        1024, [](unsigned id, unsigned * value) { *value = id; });
    ASSERT_TRUE(buffer.is_ok());
    ASSERT_NE(buffer.ok().get().data(), nullptr);

    static const std::string socket_name = "cpu-ipc-memory-buffer-test.sock";
    auto barrier = tests::ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok());

    auto server = tests::process([&]() {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());
        auto server = tests::server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(server.is_ok());

        barrier.ok().arrive_and_wait();
        auto client = server.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok());

        std::array<int, 2> fds { -1, -1 };
        gdakis::comm::message<gdakis::comm::operation::register_memory_buffer, ns::cpu::backend_type> message;
        auto res = client.ok().receive_file_descriptor_array(fds, message).sync();
        ASSERT_TRUE(res.is_ok()) << "err = " << res.err();
        ASSERT_EQ(res.ok(), 2);
        for (const auto fd : fds) {
            ASSERT_GE(fd, 0);
            ASSERT_EQ(::close(fd), 0);
        }
        ASSERT_EQ(message.get().header_.op_, gdakis::comm::operation::register_memory_buffer);
        ASSERT_EQ(message.get().header_.device_, gdakis::comm::device_type::cpu);
        ASSERT_EQ(message.get().header_.transport_, gdakis::comm::transport_type::ipc);
        ASSERT_NE(std::get<0>(message.ancillary()), -1);
        ASSERT_NE(std::get<1>(message.ancillary()), -1);

        gdakis::comm::acknowledge<gdakis::comm::operation::register_memory_buffer, ns::cpu::backend_type> ack;
        ack.status = gdakis::comm::status_type::ok;
        ack.id = 0xcafe;
        auto ret = client.ok().write(ack).sync();
        ASSERT_TRUE(ret.is_ok());
        barrier.ok().arrive_and_wait();
    });

    auto sched = tests::io::scheduler::create();
    ASSERT_TRUE(sched.is_ok());

    barrier.ok().arrive_and_wait();
    auto client = tests::client::connect(sched.ok(), socket_name).sync();
    ASSERT_TRUE(client.is_ok());

    auto res = buffer.ok().send_to_server(client.ok().socket_).sync();
    ASSERT_TRUE(res.is_ok());
    ASSERT_TRUE(buffer.ok().id().has_value());
    ASSERT_EQ(buffer.ok().id().value(), 0xcafe);
    barrier.ok().arrive_and_wait();
}
