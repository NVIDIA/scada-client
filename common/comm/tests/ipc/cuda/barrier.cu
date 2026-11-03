/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/ipc/cuda/barrier>
#include <psa/coaio/scheduler/epoll>
#include <psa/coaio/socket>
#include <tests/utility>

namespace io {
using scheduler = ::psa::coaio::scheduler::epoll;
using socket = ::psa::coaio::socket<scheduler>;

using result = ::psa::result<::psa::coaio::none_type, int>;
}

namespace ns = gdakis::comm::ipc::cuda;
class CUDABarrierTest : public ::tests::CUDATest {};

TEST(CUDABarrierTest, Create) {
    auto creator = tests::process([] {
        auto sh_barrier = ns::barrier<gdakis::thread_scope_device>::build(2);
        ASSERT_NE(sh_barrier, nullptr);
    });
    ASSERT_TRUE(creator.wait());
}

static __global__ void cuda_barrier_arrive_kernel(ns::barrier<gdakis::thread_scope_device> * barrier) {
    barrier->arrive_and_wait();
}

io::result cuda_barrier_import(io::scheduler & sched) noexcept {
    using barrier = ns::barrier<gdakis::thread_scope_device>;

    auto socket = io::socket::make(sched, io::socket::domain::local, io::socket::type::stream);
    if (socket.is_err())
        return io::result::sync_type::err(socket.err());

    auto addr = io::socket::address<io::socket::domain::local>::from_string("cuda-barrier.server");
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

    unsigned count = 0;
    barrier::ptr_type::ancillary_type ancillary;
    auto read = client.ok()
                    .receive_file_descriptor_array(ancillary, count)
                    .and_then([&client, &ancillary, &count](auto) mutable {
                        EXPECT_EQ(count, 2);
                        auto ptr = barrier::import(ancillary);
                        EXPECT_NE(ptr, nullptr);
                        if (ptr == nullptr)
                            return io::result::err(ENOMEM);
                        cuda_barrier_arrive_kernel<<<1, 1>>>(ptr.get());
                        EXPECT_EQ(cudaDeviceSynchronize(), cudaSuccess);
                        return io::result::ok<>();
                    })
                    .sync();
    EXPECT_TRUE(read.is_ok());
    return io::result::sync_type::ok<>();
}

io::result cuda_barrier_export(io::scheduler & sched) {
    using barrier = ns::barrier<gdakis::thread_scope_device>;

    auto socket = io::socket::make(sched, io::socket::domain::local, io::socket::type::stream);
    if (socket.is_err())
        return io::result::sync_type::err(socket.err());

    auto addr = io::socket::address<io::socket::domain::local>::from_string("cuda-barrier.server");
    if (addr.is_err())
        return io::result::sync_type::err(addr.err());

    unsigned count = 2;
    auto ptr = barrier::build(count);
    EXPECT_NE(ptr, nullptr);
    if (ptr == nullptr) {
        return io::result::sync_type::err(ENOMEM);
    }

    auto socket_addr = addr.ok();
    auto res = socket.ok()
                   .connect(reinterpret_cast<::sockaddr *>(&socket_addr), sizeof(socket_addr))
                   .and_then([&socket, &ptr, &count](
                                 auto) { return socket.ok().send_file_descriptor_array(ptr.ancillary(), count); })
                   .sync();
    if (res.is_err())
        return io::result::sync_type::err(res.err());

    cuda_barrier_arrive_kernel<<<1, 1>>>(ptr.get());
    auto cuda_ret = cudaDeviceSynchronize();
    EXPECT_EQ(cuda_ret, cudaSuccess);
    if (cuda_ret != cudaSuccess)
        return io::result::sync_type::err(EINVAL);

    return io::result::sync_type::ok<>();
}

TEST(CUDABarrier, HandOff) {
    auto importer = ::tests::process([] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());
        auto res = cuda_barrier_import(sched.ok()).sync();
        EXPECT_TRUE(res.is_ok());
    });

    auto exporter = ::tests::process([] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());
        auto res = cuda_barrier_export(sched.ok()).sync();
        EXPECT_TRUE(res.is_ok());
    });
    ASSERT_TRUE(importer.wait());
    ASSERT_TRUE(exporter.wait());
}