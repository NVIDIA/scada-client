/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/protocol>
#include <gtest/gtest.h>
#include <sstream>

namespace protocol = gdakis::comm;

TEST(TestProtocolDeviceType, ostreamInvalid) {
    std::stringstream out;
    auto dev = protocol::device_type::invalid;
    out << dev;
    EXPECT_EQ("INVALID", out.str());
}

TEST(TestProtocolDeviceType, ostreamCPU) {
    std::stringstream out;
    auto dev = protocol::device_type::cpu;
    out << dev;
    EXPECT_EQ("CPU", out.str());
}

TEST(TestProtocolDeviceType, ostreamCUDA) {
    std::stringstream out;
    auto dev = protocol::device_type::cuda;
    out << dev;
    EXPECT_EQ("CUDA", out.str());
}

TEST(TestProtocolTransportType, ostreamInvalid) {
    std::stringstream out;
    auto transport = protocol::transport_type::invalid;
    out << transport;
    EXPECT_EQ("INVALID", out.str());
}

TEST(TestProtocolTransportType, ostreamIPC) {
    std::stringstream out;
    auto transport = protocol::transport_type::ipc;
    out << transport;
    EXPECT_EQ("IPC", out.str());
}

TEST(TestProtocolTransportType, ostreamRDMA) {
    std::stringstream out;
    auto transport = protocol::transport_type::rdma;
    out << transport;
    EXPECT_EQ("RDMA", out.str());
}

TEST(TestProtocolHeaderType, Constructor) {
    auto header = protocol::header_type(
        protocol::operation::register_request_buffers, protocol::device_type::cpu, protocol::transport_type::ipc);
    EXPECT_EQ(protocol::operation::register_request_buffers, header.op_);
    EXPECT_EQ(protocol::device_type::cpu, header.device_);
    EXPECT_EQ(protocol::transport_type::ipc, header.transport_);
}

TEST(TestProtocolHeaderType, ConstructorDefaultInvalid) {
    auto header = protocol::header_type();
    EXPECT_EQ(protocol::operation::invalid, header.op_);
    EXPECT_EQ(protocol::device_type::invalid, header.device_);
    EXPECT_EQ(protocol::transport_type::invalid, header.transport_);
}

struct backend_type {
    static constexpr protocol::device_type device_type = protocol::device_type::cpu;
    static constexpr protocol::transport_type transport_type = protocol::transport_type::ipc;

    template<protocol::operation _Op>
    using request_type = protocol::request<_Op>;

    template<protocol::operation _Op>
    using response_type = protocol::response<_Op>;
};

TEST(TestProtocolMessage, Constructor) {
    [[maybe_unused]] protocol::message<protocol::operation::register_request_buffers, backend_type> test_message;
}

TEST(TestProtocolMessage, MessageSize) {
    protocol::message<protocol::operation::register_request_buffers, backend_type> test_message;
    EXPECT_NE(test_message.size(), 0);
}

TEST(TestProtocolMessage, UpdateAndReadData) {
    protocol::message<protocol::operation::register_request_buffers, backend_type> test_message;
    const auto & data_reader = test_message.data();
    auto & data_writer = test_message.data();
    data_writer.req.num_buffers = 5;
    data_writer.req.entry_size = 128;
    data_writer.req.buffer_size = 32;
    EXPECT_EQ(data_reader.req.num_buffers, 5);
    EXPECT_EQ(data_reader.req.entry_size, 128);
    EXPECT_EQ(data_reader.req.buffer_size, 32);
}

TEST(TESTProtocolMessage, UpdateMessage) {
    protocol::message<protocol::operation::register_request_buffers, backend_type> test_message1;
    protocol::message<protocol::operation::register_request_buffers, backend_type> test_message2;
    const auto & data_reader1 = test_message1.data();
    auto & data_writer1 = test_message1.data();
    data_writer1.req.num_buffers = 5;
    const auto & data_reader2 = test_message2.data();
    auto & data_writer2 = test_message2.data();
    data_writer2.req.num_buffers = 6;
    ASSERT_EQ(data_reader1.req.num_buffers, 5);
    ASSERT_EQ(data_reader2.req.num_buffers, 6);

    const auto & msg1_content = test_message1.get();
    auto & msg2_content = test_message2.get();
    msg2_content.data_.req.num_buffers = msg1_content.data_.req.num_buffers;

    EXPECT_EQ(test_message1.get().data_.req.num_buffers, 5);
    EXPECT_EQ(test_message2.get().data_.req.num_buffers, 5);
}
