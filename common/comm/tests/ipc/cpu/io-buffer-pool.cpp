/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/atomic>
#include <gdakis/ipc/io_buffer_pool>
#include <gdakis/ipc/protocol>
#include <gdakis/ipc/shmem>
#include <gdakis/protocol>
#include <tests/utility>

namespace ns = gdakis::comm::ipc;
class CPUIPCIOBufferPool : public ::testing::Test {
protected:
    void SetUp() override {
        if (ns::shmem<gdakis::umem::cpu, void>::supports_dma() == false) {
            GTEST_SKIP() << "CPU DMA memory no available";
        }
    }
};

TEST(CPUIPCIOBufferPool, Create) {
    using io_buffer_pool_type = ns::io_buffer_pool<unsigned, gdakis::comm::status_type>;
    auto buffer = ns::io_buffer_pool<unsigned, gdakis::comm::status_type>::template handle<gdakis::umem::cpu>::build(
        1024, [](unsigned id, unsigned * value) { *value = id; },
        [](unsigned id, io_buffer_pool_type::status_type * status) {
            gdakis::atomic_ref<gdakis::comm::status_type>(*status).store(
                gdakis::comm::status_type::ok, gdakis::memory_order_relaxed);
        });
    ASSERT_TRUE(buffer.is_ok()) << "Failed to create I/O buffer pool err = " << buffer.err();
    ASSERT_NE(buffer.ok().get().data(), nullptr) << "I/O buffer pool with null data";
    ASSERT_NE(buffer.ok().get().status(), nullptr) << "I/O buffer pool with null status";
    for (unsigned n = 0; n < 1024; ++n) {
        ASSERT_EQ(buffer.ok().get().data()[n], n);
        ASSERT_EQ(buffer.ok().get().status()[n], gdakis::comm::status_type::ok);
    }
}

TEST(CPUIPCIOBufferPool, SendToServer) {
    using io_buffer_pool_type = ns::io_buffer_pool<unsigned, gdakis::comm::status_type>;
    auto buffer = ns::io_buffer_pool<unsigned, gdakis::comm::status_type>::template handle<gdakis::umem::cpu>::build(
        1024, [](unsigned id, unsigned * value) { *value = id; },
        [](unsigned id, io_buffer_pool_type::status_type * status) {
            gdakis::atomic_ref<gdakis::comm::status_type>(*status).store(
                gdakis::comm::status_type::ok, gdakis::memory_order_relaxed);
        });
    ASSERT_TRUE(buffer.is_ok()) << "Failed to create I/O buffer pool err = " << buffer.err();
    ASSERT_NE(buffer.ok().get().data(), nullptr) << "I/O buffer pool with null data";
    ASSERT_NE(buffer.ok().get().status(), nullptr) << "I/O buffer pool with null status";

    static const std::string socket_name = "cpu-ipc-io-buffer-pool-test.sock";
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

        std::array<int, 4> fd_array;
        gdakis::comm::message<gdakis::comm::operation::register_io_buffer_pool, ns::cpu::backend_type> message;
        auto res = client.ok().receive_file_descriptor_array(fd_array, message).sync();
        ASSERT_TRUE(res.is_ok());
        ASSERT_EQ(res.ok(), 4);
        ASSERT_EQ(message.get().header_.op_, gdakis::comm::operation::register_io_buffer_pool);
        ASSERT_EQ(message.get().header_.device_, gdakis::comm::device_type::cpu);
        ASSERT_EQ(message.get().header_.transport_, gdakis::comm::transport_type::ipc);
        ASSERT_NE(std::get<0>(message.ancillary()), -1);
        ASSERT_NE(std::get<1>(message.ancillary()), -1);
        ASSERT_NE(std::get<2>(message.ancillary()), -1);
        ASSERT_NE(std::get<3>(message.ancillary()), -1);

        gdakis::comm::acknowledge<gdakis::comm::operation::register_io_buffer_pool, ns::cpu::backend_type> ack;
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
    ASSERT_TRUE(res.is_ok()) << "error = " << res.err();
    ASSERT_TRUE(buffer.ok().id().has_value());
    ASSERT_EQ(buffer.ok().id().value(), 0xcafe);
    barrier.ok().arrive_and_wait();
}