/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <psa/coaio/detached>
#include <psa/coaio/scheduler/epoll>
#include <psa/coaio/socket>
#include <psa/futures>

#include <process>

namespace io {

using scheduler = ::psa::coaio::scheduler::epoll;
using socket = ::psa::coaio::socket<scheduler>;
using detached = ::psa::coaio::detached;
template<typename _Tp = ::psa::coaio::none_type>
using result = psa::futures::result<_Tp, int>;
}

TEST(Socket, Make) {
    auto sched = io::scheduler::create();
    ASSERT_TRUE(sched.is_ok());
    auto ret = io::socket::make(sched.ok(), io::socket::domain::local, io::socket::type::stream).sync();
    EXPECT_TRUE(ret.is_ok());
}

static typename io::result<io::socket>::sync_type make_socket(io::scheduler & scheduler) noexcept {
    return io::socket::make(scheduler, io::socket::domain::local, io::socket::type::stream).sync();
}

TEST(Socket, MakeIndirect) {
    auto sched = io::scheduler::create();
    ASSERT_TRUE(sched.is_ok());
    auto ret = make_socket(sched.ok());
    EXPECT_TRUE(ret.is_ok());
}

TEST(Socket, Bind) {
    auto sched = io::scheduler::create();
    ASSERT_TRUE(sched.is_ok());

    auto res = io::socket::make(sched.ok(), io::socket::domain::local, io::socket::type::stream).sync();
    ASSERT_TRUE(res.is_ok());
    io::socket sock(res.take_ok());

    const std::string path("test.server");
    auto addr = io::socket::address<io::socket::domain::local>::from_string(path).ok();
    int err = ::unlink(addr.sun_path);
    ASSERT_TRUE(err == 0 or errno == ENOENT);

    auto ret = sock.bind(reinterpret_cast<::sockaddr *>(&addr), sizeof(addr))
                   .and_then([&sock](auto) { return sock.listen(128).sync(); })
                   .sync();
    EXPECT_TRUE(ret.is_ok());

    ::unlink(addr.sun_path);
}

#if defined(__psa_use_coroutines)

static io::detached ping_pong_logic(io::scheduler & sched, io::socket && client) {
    unsigned value;
    auto socket = std::move(client);
    std::cerr << "Server will read" << std::endl;
    auto res = socket.read(value).and_then([&value, &socket](auto) {
        std::cerr << "Server will write" << std::endl;
        return socket.write(~value);
    });
    auto ret = co_await res;
    EXPECT_TRUE(ret.is_ok());
    std::cerr << "Server completes" << std::endl;
    co_return;
}

static io::result<> ping_pong_server(io::scheduler & sched) {
    auto sock = co_await io::socket::make(sched, io::socket::domain::local, io::socket::type::stream);
    EXPECT_TRUE(sock.is_ok());
    if (sock.is_err())
        co_return io::result<>::sync_type::err(sock.err());

    auto addr = io::socket::address<io::socket::domain::local>::from_string("ping.server");
    EXPECT_TRUE(addr.is_ok()) << "error code : " << addr.err();
    if (addr.is_err())
        co_return io::result<>::sync_type::err(addr.err());

    int err = ::unlink(addr.ok().sun_path);
    EXPECT_TRUE(err == 0 or errno == ENOENT);
    if (err < 0 and errno != ENOENT)
        co_return io::result<>::sync_type::err(errno);

    auto sock_addr = addr.ok();
    auto res = sock.ok()
                   .bind(reinterpret_cast<::sockaddr *>(&sock_addr), sizeof(sock_addr))
                   .and_then([&sock](auto) { return sock.ok().listen(1024); })
                   .or_else([&sock_addr](auto err) {
                       ::unlink(sock_addr.sun_path);
                       return io::result<>::sync_type::err(err);
                   });
    std::cerr << "SYNCHRONIZING SOCKET" << std::endl;
    auto ret = res.sync();
    EXPECT_TRUE(ret.is_ok()) << "error code : " << ret.err();
    if (ret.is_err()) {
        co_return io::result<>::sync_type::err(ret.err());
    }

    while (true) {
        std::cerr << "ACCEPTING CONNECTION" << std::endl;
        auto client = co_await sock.ok().accept(sched, nullptr, nullptr);
        std::cerr << "NEW CLIENT" << std::endl;
        EXPECT_TRUE(client.is_ok()) << "error code : " << client.err();
        if (client.is_err()) {
            ::unlink(sock_addr.sun_path);
            co_return io::result<>::sync_type::err(client.err());
        }
        std::cerr << "LAUNCHING SERVER LOGIC" << std::endl;
        ping_pong_logic(sched, client.take_ok());
    }
    ::unlink(sock_addr.sun_path);
    co_return io::result<>::sync_type::ok<>();
}

static io::result<> ping_pong_client(io::scheduler & sched) {
    auto sock = co_await io::socket::make(sched, io::socket::domain::local, io::socket::type::stream);
    EXPECT_TRUE(sock.is_ok());
    if (sock.is_err())
        co_return io::result<>::sync_type::err(sock.err());

    auto addr = io::socket::address<io::socket::domain::local>::from_string("ping.server");
    EXPECT_TRUE(addr.is_ok()) << "error code : " << addr.err();
    if (addr.is_err())
        co_return io::result<>::sync_type::err(addr.err());

    std::cerr << "Client will connect" << std::endl;
    auto sock_addr = addr.ok();
    auto ret = co_await sock.ok()
                   .connect(reinterpret_cast<::sockaddr *>(&sock_addr), sizeof(sock_addr))
                   .and_then([&sock](auto) {
                       std::cerr << "Client will write" << std::endl;
                       unsigned value = 0xdeadbeef;
                       auto res = sock.ok().write(value).and_then([&sock](auto) {
                           std::cerr << "Client will read" << std::endl;
                           unsigned value;
                           auto ret = sock.ok().read(value).sync();
                           std::cerr << "Client will finish" << std::endl;
                           EXPECT_TRUE(ret.is_ok()) << "error code : " << ret.err();
                           EXPECT_EQ(value, ~0xdeadbeef) << "value = 0x" << std::hex << value << std::dec;
                           return ret;
                       });
                       std::cerr << "Client synchronizes write" << std::endl;
                       auto ret = res.sync();
                       EXPECT_TRUE(ret.is_ok()) << "error code : " << ret.err();
                       return ret;
                   });
    EXPECT_TRUE(ret.is_ok()) << "error code : " << ret.err();
    co_return io::result<>::sync_type::ok<>();
}

TEST(Socket, Server) {
    auto sched = io::scheduler::create();
    auto ret = ping_pong_server(sched.ok());
    auto res = ping_pong_client(sched.ok()).sync();
    EXPECT_TRUE(res.is_ok());
}

#endif