/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/ipc/autobatch>
#include <gdakis/ipc/protocol>
#include <gdakis/protocol>
#include <gdakis/umem/cuda>
#include <tests/utility>

namespace io {
using scheduler = ::psa::coaio::scheduler::epoll;
using socket = ::psa::coaio::socket<scheduler>;

using result = ::psa::result<::psa::coaio::none_type, int>;
}

namespace ns = gdakis::comm::ipc;

class CUDAIPCAutobatchTest : public ::tests::CUDATest {};

TEST(CUDAIPCAutobatchTest, Create) {
    auto creator = tests::process([] {
        auto autobatch
            = ns::autobatch<gdakis::umem::cuda, unsigned, gdakis::thread_scope_device, 2>::handle::build(1024);
        ASSERT_TRUE(autobatch.is_ok());
    });
    ASSERT_TRUE(creator.wait());
}

TEST(CUDAIPCAutobatchTest, ExportTo) {
    auto creator = tests::process([] {
        auto autobatch
            = ns::autobatch<gdakis::umem::cuda, unsigned, gdakis::thread_scope_device, 2>::handle::build(1024);
        ASSERT_TRUE(autobatch.is_ok());
        auto msg = autobatch.ok().request();
        ASSERT_TRUE(msg.is_ok());
        EXPECT_EQ(msg.ok().get().header_.op_, gdakis::comm::operation::register_request_buffers);
        EXPECT_EQ(msg.ok().get().data_.req.num_buffers, 2);
        EXPECT_EQ(msg.ok().get().data_.req.entry_size, sizeof(unsigned));
        EXPECT_EQ(msg.ok().get().data_.req.buffer_size, 1024);
    });
    ASSERT_TRUE(creator.wait());
}

io::result::sync_type cuda_autobatch_import(io::scheduler & sched) noexcept {
    using autobatch_type = ns::autobatch<gdakis::umem::cuda, unsigned, gdakis::thread_scope_device, 2>;

    auto socket = io::socket::make(sched, io::socket::domain::local, io::socket::type::stream);
    if (socket.is_err())
        return io::result::sync_type::err(socket.err());

    auto addr = io::socket::address<io::socket::domain::local>::from_string("cuda-ipc-autobatch-test.sock");
    if (addr.is_err())
        return io::result::sync_type::err(addr.err());
    int err = ::unlink(addr.ok().sun_path);
    if (err < 0 and errno != ENOENT)
        return io::result::sync_type::err(errno);

    auto socket_addr = addr.ok();
    auto res = socket.ok()
                   .bind(reinterpret_cast<::sockaddr *>(&socket_addr), sizeof(socket_addr))
                   .and_then([&socket](auto) { return socket.ok().listen(7232); })
                   .or_else([&socket_addr](auto err) {
                       ::unlink(socket_addr.sun_path);
                       return io::result::err(err);
                   })
                   .sync();
    if (res.is_err())
        return io::result::sync_type::err(res.err());

    auto client = socket.ok().accept(sched, nullptr, nullptr).sync();
    if (client.is_err())
        return io::result::sync_type::err(client.err());

    std::array<int, 2> ancillary;
    gdakis::comm::message<gdakis::comm::operation::register_request_buffers, gdakis::comm::ipc::cuda::backend_type> msg;
    auto ret = client.ok().receive_file_descriptor_array(ancillary, msg.get()).sync();
    EXPECT_TRUE(res.is_ok());
    if (ret.is_err())
        return io::result::sync_type::err(res.err());

    std::cerr << "op = " << msg.get().header_.op_ << " device = " << msg.get().header_.device_
              << " transport = " << msg.get().header_.transport_ << std::endl;
    EXPECT_EQ(msg.get().header_.op_, gdakis::comm::operation::register_request_buffers);
    if (msg.get().header_.op_ != gdakis::comm::operation::register_request_buffers)
        return io::result::sync_type::err(EIO);
    EXPECT_EQ(msg.get().header_.device_, gdakis::comm::device_type::cuda);
    if (msg.get().header_.device_ != gdakis::comm::device_type::cuda)
        return io::result::sync_type::err(EIO);
    EXPECT_EQ(msg.get().header_.transport_, gdakis::comm::transport_type::ipc);
    if (msg.get().header_.transport_ != gdakis::comm::transport_type::ipc)
        return io::result::sync_type::err(EIO);

    std::cerr << "recv ctrl = " << std::get<0>(ancillary) << " recv status = " << std::get<1>(ancillary) << std::endl;
    auto autobatch = autobatch_type::handle::import(msg.data().req.num_buffers, std::move(msg.data().backend.control),
        std::array<int, 1> { std::get<0>(ancillary) }, std::move(msg.data().backend.buffer),
        std::array<int, 1> { std::get<1>(ancillary) });
    EXPECT_TRUE(autobatch.is_ok());
    if (autobatch.is_err())
        return io::result::sync_type::err(autobatch.err());

    gdakis::comm::acknowledge<gdakis::comm::operation::register_request_buffers, gdakis::comm::ipc::cuda::backend_type>
        ack;
    ack.status = gdakis::comm::status_type::ok;
    ack.id = 0xcafe;
    res = client.ok().write(ack).sync();
    EXPECT_TRUE(res.is_ok());
    if (res.is_err())
        return io::result::sync_type::err(res.err());

    return io::result::sync_type::ok<>();
}

io::result::sync_type cuda_autobatch_export(io::scheduler & sched) {
    using autobatch_type = ns::autobatch<gdakis::umem::cuda, unsigned, gdakis::thread_scope_device, 1>;

    auto socket = io::socket::make(sched, io::socket::domain::local, io::socket::type::stream);
    if (socket.is_err())
        return io::result::sync_type::err(socket.err());

    auto addr = io::socket::address<io::socket::domain::local>::from_string("cuda-ipc-autobatch-test.sock");
    if (addr.is_err())
        return io::result::sync_type::err(addr.err());

    auto autobatch = autobatch_type::handle::build(1024);
    EXPECT_TRUE(autobatch.is_ok());
    if (autobatch.is_err()) {
        std::cerr << "Failed to build autobatch" << std::endl;
        return io::result::sync_type::err(autobatch.err());
    }

    auto msg = autobatch.ok().request();
    EXPECT_TRUE(msg.is_ok());
    if (msg.is_err()) {
        std::cerr << "Failed to get message" << std::endl;
        return io::result::sync_type::err(msg.err());
    }

    std::cerr << "send ctrl = " << std::get<0>(msg.ok().ancillary())
              << " send status = " << std::get<1>(msg.ok().ancillary()) << std::endl;

    auto socket_addr = addr.ok();
    auto res = socket.ok()
                   .connect(reinterpret_cast<::sockaddr *>(&socket_addr), sizeof(socket_addr))
                   .and_then([&socket, &msg](auto) {
                       return socket.ok().send_file_descriptor_array(msg.ok().ancillary(), msg.ok().get());
                   })
                   .sync();
    if (res.is_err()) {
        std::cerr << "Failed to send autobatch" << std::endl;
        return io::result::sync_type::err(res.err());
    }

    autobatch_type::handle::response_type ack;
    res = socket.ok().read(ack).sync();
    if (res.is_err())
        return io::result::sync_type::err(res.err());
    EXPECT_EQ(ack.id, 0xcafe);
    EXPECT_EQ(ack.status, gdakis::comm::status_type::ok);

    return io::result::sync_type::ok<>();
}

TEST(CUDAIPCAutobatchTest, ImportFrom) {
    auto importer = ::tests::process([] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());
        auto res = cuda_autobatch_import(sched.ok()).sync();
        EXPECT_TRUE(res.is_ok());
    });

    auto exporter = ::tests::process([] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());
        auto res = cuda_autobatch_export(sched.ok()).sync();
        EXPECT_TRUE(res.is_ok());
    });
    ASSERT_TRUE(importer.wait());
    ASSERT_TRUE(exporter.wait());
}