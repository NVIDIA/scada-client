/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <psa/coaio/detached>
#include <psa/coaio/scheduler/epoll>
#include <psa/coaio/socket>
#include <psa/futures>

#include <algorithm>
#include <barrier>
#include <filesystem>
#include <optional>
#include <random>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>

#include <process>

namespace io {
using scheduler = ::psa::coaio::scheduler::epoll;
using socket = ::psa::coaio::socket<scheduler>;
using detached = ::psa::coaio::detached;

template<typename _Tp = ::psa::coaio::none_type>
using result = psa::futures::result<_Tp, int>;
}

static std::optional<size_t> open_file_descriptor_count() noexcept {
    std::error_code error;
    size_t count = 0;
    std::filesystem::directory_iterator iterator("/proc/self/fd", error);
    const std::filesystem::directory_iterator end;
    while (!error and iterator != end) {
        ++count;
        iterator.increment(error);
    }
    return error ? std::nullopt : std::optional<size_t>(count);
}

struct server {
    io::socket socket_;
    std::optional<::sockaddr_un> address_;

    inline server(io::socket && socket, ::sockaddr_un && address) noexcept
        : socket_(std::move(socket)),
          address_(std::move(address)) {}
    inline server(server && other) noexcept : socket_(std::move(other.socket_)), address_(std::move(other.address_)) {
        other.address_ = std::nullopt;
    }

    inline ~server() noexcept {
        if (address_.has_value()) {
            ::unlink(address_.value().sun_path);
        }
    }

    inline io::result<io::socket> accept(io::scheduler & sched) noexcept {
        return socket_.accept(sched, nullptr, nullptr);
    }

    static inline io::result<server> make(
        io::scheduler & sched, const std::string & name, unsigned backlog = 32) noexcept {
        auto sock = co_await io::socket::make(sched, io::socket::domain::local, io::socket::type::stream);
        EXPECT_TRUE(sock.is_ok());
        if (sock.is_err())
            co_return io::result<server>::sync_type::err(sock.err());

        auto addr = io::socket::address<io::socket::domain::local>::from_string(name);
        EXPECT_TRUE(addr.is_ok());
        if (addr.is_err())
            co_return io::result<server>::sync_type::err(addr.err());
        int err = ::unlink(addr.ok().sun_path);
        EXPECT_TRUE(err == 0 or errno == ENOENT);
        if (err < 0 and errno != ENOENT)
            co_return io::result<server>::sync_type::err(errno);
        auto sock_addr = addr.ok();
        auto res = co_await sock.ok()
                       .bind(reinterpret_cast<::sockaddr *>(&sock_addr), sizeof(sock_addr))
                       .and_then([&](auto) { return sock.ok().listen(backlog); })
                       .or_else([&](auto err) {
                           ::unlink(sock_addr.sun_path);
                           return io::result<>::sync_type::err(err);
                       });
        EXPECT_TRUE(res.is_ok());
        if (res.is_err())
            co_return io::result<server>::sync_type::err(res.err());

        co_return io::result<server>::sync_type::ok(sock.take_ok(), addr.take_ok());
    }
};

struct client {
    io::socket socket_;

    inline client(io::socket && socket) noexcept : socket_(std::move(socket)) {}
    client(client &&) noexcept = default;

    static inline io::result<client> connect(io::scheduler & sched, const std::string & name) noexcept {
        auto sock = co_await io::socket::make(sched, io::socket::domain::local, io::socket::type::stream);
        EXPECT_TRUE(sock.is_ok());
        if (sock.is_err())
            co_return io::result<client>::sync_type::err(sock.err());

        auto addr = io::socket::address<io::socket::domain::local>::from_string(name);
        EXPECT_TRUE(addr.is_ok());
        if (addr.is_err())
            co_return io::result<client>::sync_type::err(addr.err());
        auto sock_addr = addr.ok();
        auto res = co_await sock.ok()
                       .connect(reinterpret_cast<::sockaddr *>(&sock_addr), sizeof(sock_addr))
                       .and_then([&](auto) { return io::result<client>::sync_type::ok(sock.take_ok()); });
        EXPECT_TRUE(res.is_ok()) << "error = " << res.err();
        co_return res;
    }
};

TEST(Message, WriteRead) {
    std::random_device random_device;
    std::mt19937 random_engine { random_device() };
    std::uniform_int_distribution<int> uniform_distribution { 1, 128 };
    auto random_generator = [&uniform_distribution, &random_engine]() { return uniform_distribution(random_engine); };
    std::vector<int> values(1024);
    std::generate(values.begin(), values.end(), random_generator);

    auto barrier = ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok());
    static const std::string socket_name = "message-test.sock";

    auto server_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto srv = server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(srv.is_ok());
        std::cerr << "Waiting for client..." << std::endl;
        barrier.ok().arrive_and_wait();
        std::cerr << "Accepting connections..." << std::endl;
        auto client = srv.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok());
        std::cerr << "Server test start" << std::endl;

        int value;
        for (auto pos = values.begin(); pos != values.end(); ++pos) {
            auto res = client.ok().read(value).sync();
            ASSERT_TRUE(res.is_ok());
            ASSERT_EQ(value, *pos);
        }
    });
    ASSERT_TRUE(server_process.is_ok());

    auto client_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        std::cerr << "Waiting for server..." << std::endl;
        barrier.ok().arrive_and_wait();
        auto clt = client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(clt.is_ok());
        for (auto pos = values.begin(); pos != values.end(); ++pos) {
            auto res = clt.ok().socket_.write(*pos).sync();
            ASSERT_TRUE(res.is_ok());
        }
    });
    if (client_process.is_err())
        server_process.ok().kill();
    ASSERT_TRUE(client_process.is_ok());

    client_process.ok().wait();
    server_process.ok().wait();
}

TEST(Message, SendRecvFd) {
    std::random_device random_device;
    std::mt19937 random_engine { random_device() };
    std::uniform_int_distribution<int> uniform_distribution { 1, 128 };
    auto random_generator = [&uniform_distribution, &random_engine]() { return uniform_distribution(random_engine); };
    std::vector<int> values(1024);
    std::generate(values.begin(), values.end(), random_generator);

    auto barrier = ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok());
    static const std::string socket_name = "message-fd-test.sock";

    auto server_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto srv = server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(srv.is_ok());
        barrier.ok().arrive_and_wait();
        std::cerr << "Accepting connections..." << std::endl;
        auto client = srv.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok());
        std::cerr << "Server test start" << std::endl;

        struct {
            int head;
            int tail;
        } value;
        std::cerr << "Server waiting for message/ancillary data" << std::endl;
        int fd = -1;
        auto res = client.ok().receive_file_descriptor(fd, value).sync();
        ASSERT_TRUE(res.is_ok());
        ASSERT_EQ(value.head, 0xbadcafe);
        ASSERT_EQ(value.tail, 0xbeef00d);
        ASSERT_GE(fd, 0);
        std::cerr << "Server acquired file descriptor " << fd << std::endl;
        barrier.ok().arrive_and_wait();

        for (auto pos = values.begin(); pos != values.end(); ++pos) {
            int value;
            auto res = client.ok().read(value).sync();
            ASSERT_TRUE(res.is_ok()) << "client read error = " << res.err();
            ASSERT_EQ(value, *pos);
            int err = write(fd, &value, sizeof(value));
            ASSERT_EQ(err, sizeof(value));
        }
    });
    ASSERT_TRUE(server_process.is_ok());

    auto client_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        struct message {
            int header;
            int value;
        };
        message message { 0xbadcafe, 0xbeef00d };

        int pfd[2];
        int err = ::pipe(pfd);
        ASSERT_GE(err, 0);

        std::cerr << "Waiting for server..." << std::endl;
        barrier.ok().arrive_and_wait();
        auto clt = client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(clt.is_ok());
        auto res = clt.ok().socket_.send_file_descriptor(pfd[1], message).sync();
        ASSERT_TRUE(res.is_ok());
        barrier.ok().arrive_and_wait();
        std::cerr << "Client closing write end of the pipe" << std::endl;
        ::close(pfd[1]);
        for (auto pos = values.begin(); pos != values.end(); ++pos) {
            auto res = clt.ok().socket_.write(*pos).sync();
            ASSERT_TRUE(res.is_ok());
            int value;
            int err = read(pfd[0], &value, sizeof(value));
            ASSERT_EQ(err, sizeof(value));
            ASSERT_EQ(value, *pos);
        }
        ::close(pfd[0]);
    });
    if (client_process.is_err())
        server_process.ok().kill();
    ASSERT_TRUE(client_process.is_ok());

    ASSERT_TRUE(client_process.ok().wait());
    ASSERT_TRUE(server_process.ok().wait());
}

TEST(Message, SendRecvFdSet) {
    std::random_device random_device;
    std::mt19937 random_engine { random_device() };
    std::uniform_int_distribution<int> uniform_distribution { 1, 128 };
    auto random_generator = [&uniform_distribution, &random_engine]() { return uniform_distribution(random_engine); };
    std::vector<int> values(1024);
    std::generate(values.begin(), values.end(), random_generator);

    struct message {
        int header;
        int value;
    };

    auto barrier = ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok());
    static const std::string socket_name = "message-fd-set-test.sock";

    auto server_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto srv = server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(srv.is_ok());
        std::cerr << "Waiting for client..." << std::endl;
        barrier.ok().arrive_and_wait();
        std::cerr << "Accepting connections..." << std::endl;
        auto client = srv.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok());
        std::cerr << "Server test start" << std::endl;

        struct {
            message valid;
            message invalid;
        } message;

        message.invalid.header = 0xdeadbeef;
        message.invalid.value = 0xbeefdead;

        // After reading the header we get the file descriptor set
        // and the rest of the message
        std::array<int, 2> fd_array;
        auto ret = client.ok().receive_file_descriptor_array(fd_array, &message, sizeof(message)).sync();
        ASSERT_TRUE(ret.is_ok());
        ASSERT_EQ(std::get<0>(ret.ok()), sizeof(message.valid)) << "err = " << ret.err();
        ASSERT_EQ(std::get<1>(ret.ok()), 2) << " expected = 2, received = " << std::get<1>(ret.ok());
        ASSERT_EQ(message.valid.header, 0xbadcafe);
        ASSERT_EQ(message.valid.value, 0xbeef00d);
        ASSERT_EQ(message.invalid.header, 0xdeadbeef);
        ASSERT_EQ(message.invalid.value, 0xbeefdead);
        std::cerr << "fd[0] = " << std::get<0>(fd_array) << " fd[1] = " << std::get<1>(fd_array) << std::endl;
        int ack = 0xdeadbeed;
        auto res = client.ok().write(ack).sync();
        ASSERT_TRUE(res.is_ok());

        int err;
        for (auto pos = values.begin(); pos != values.end(); ++pos) {
            int value;
            err = write(std::get<1>(fd_array), &(*pos), sizeof(int));
            ASSERT_EQ(err, sizeof(int));
            err = read(std::get<0>(fd_array), &value, sizeof(value));
            ASSERT_EQ(err, sizeof(value));
            ASSERT_EQ(*pos, value);
        }
    });
    ASSERT_TRUE(server_process.is_ok());

    auto client_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        message message { 0xbadcafe, 0xbeef00d };

        std::array<int, 2> fd_array;
        int err = ::pipe(fd_array.data());
        ASSERT_GE(err, 0);

        std::cerr << "Waiting for server..." << std::endl;
        barrier.ok().arrive_and_wait();
        auto clt = client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(clt.is_ok());
        auto res = clt.ok().socket_.send_file_descriptor_array(fd_array, message).sync();
        ASSERT_TRUE(res.is_ok());

        int value;
        res = clt.ok().socket_.read(value).sync();
        ASSERT_TRUE(res.is_ok());
        ASSERT_EQ(value, 0xdeadbeed);

        ::close(std::get<0>(fd_array));
        ::close(std::get<1>(fd_array));
    });
    if (client_process.is_err())
        server_process.ok().kill();
    ASSERT_TRUE(client_process.is_ok());

    ASSERT_TRUE(client_process.ok().wait());
    ASSERT_TRUE(server_process.ok().wait());
}

TEST(Message, RejectsOversizedFdSetWithoutLeakingFds) {
    auto barrier = ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok());
    static const std::string socket_name = "message-fd-oversized-test.sock";

    auto server_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto srv = server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(srv.is_ok());
        barrier.ok().arrive_and_wait();
        auto client = srv.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok());

        auto descriptor_count = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count.has_value());
        ASSERT_GT(descriptor_count.value(), 0);
        std::array<int, 1> fd_array;
        int value;
        auto res = client.ok().receive_file_descriptor_array(fd_array, value).sync();
        ASSERT_TRUE(res.is_err());
        ASSERT_EQ(res.err(), ECOMM);
        ASSERT_EQ(fd_array[0], -1);
        auto descriptor_count_after = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count_after.has_value());
        ASSERT_GT(descriptor_count_after.value(), 0);
        ASSERT_EQ(descriptor_count_after.value(), descriptor_count.value());
    });
    ASSERT_TRUE(server_process.is_ok());

    auto client_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        int fd_array[2];
        ASSERT_EQ(::pipe(fd_array), 0);
        barrier.ok().arrive_and_wait();
        auto clt = client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(clt.is_ok());
        int value = 0;
        auto res = clt.ok()
                       .socket_.send_file_descriptor_array(std::array<int, 2> { fd_array[0], fd_array[1] }, value)
                       .sync();
        ASSERT_TRUE(res.is_ok());
        ::close(fd_array[0]);
        ::close(fd_array[1]);
    });
    if (client_process.is_err())
        server_process.ok().kill();
    ASSERT_TRUE(client_process.is_ok());

    ASSERT_TRUE(client_process.ok().wait());
    ASSERT_TRUE(server_process.ok().wait());
}

TEST(Message, RejectsTruncatedFdSetWithoutLeakingFds) {
    auto barrier = ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok());
    static const std::string socket_name = "message-fd-truncated-test.sock";

    auto server_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto srv = server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(srv.is_ok());
        barrier.ok().arrive_and_wait();
        auto client = srv.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok());

        auto descriptor_count = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count.has_value());
        ASSERT_GT(descriptor_count.value(), 0);
        std::array<int, 1> fd_array;
        int value;
        auto res = client.ok().receive_file_descriptor_array(fd_array, value).sync();
        ASSERT_TRUE(res.is_err());
        ASSERT_EQ(res.err(), EMSGSIZE);
        ASSERT_EQ(fd_array[0], -1);
        auto descriptor_count_after = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count_after.has_value());
        ASSERT_GT(descriptor_count_after.value(), 0);
        ASSERT_EQ(descriptor_count_after.value(), descriptor_count.value());
    });
    ASSERT_TRUE(server_process.is_ok());

    auto client_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        std::array<int, 4> descriptors;
        ASSERT_EQ(::pipe(descriptors.data()), 0);
        ASSERT_EQ(::pipe(descriptors.data() + 2), 0);
        barrier.ok().arrive_and_wait();
        auto clt = client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(clt.is_ok());
        int value = 0;
        auto res = clt.ok()
                       .socket_
                       .send_file_descriptor_array(
                           std::array<int, 3> { descriptors[0], descriptors[1], descriptors[2] }, value)
                       .sync();
        ASSERT_TRUE(res.is_ok());
        for (auto descriptor : descriptors)
            ::close(descriptor);
    });
    if (client_process.is_err())
        server_process.ok().kill();
    ASSERT_TRUE(client_process.is_ok());

    ASSERT_TRUE(client_process.ok().wait());
    ASSERT_TRUE(server_process.ok().wait());
}

TEST(Message, ReceivesShorterFdSet) {
    auto barrier = ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok());
    static const std::string socket_name = "message-fd-short-set-test.sock";

    auto server_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto srv = server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(srv.is_ok());
        barrier.ok().arrive_and_wait();
        auto client = srv.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok());

        std::array<int, 2> fd_array;
        int value;
        auto res = client.ok().receive_file_descriptor_array(fd_array, value).sync();
        ASSERT_TRUE(res.is_ok());
        ASSERT_EQ(res.ok(), 1);
        ASSERT_GE(fd_array[0], 0);
        ASSERT_EQ(fd_array[1], -1);
        auto descriptor_flags = ::fcntl(fd_array[0], F_GETFD);
        ASSERT_GE(descriptor_flags, 0);
        ASSERT_NE(descriptor_flags & FD_CLOEXEC, 0);
        ::close(fd_array[0]);
    });
    ASSERT_TRUE(server_process.is_ok());

    auto client_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        int descriptors[2];
        ASSERT_EQ(::pipe(descriptors), 0);
        barrier.ok().arrive_and_wait();
        auto clt = client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(clt.is_ok());
        int value = 0;
        auto res = clt.ok().socket_.send_file_descriptor(descriptors[0], value).sync();
        ASSERT_TRUE(res.is_ok());
        ::close(descriptors[0]);
        ::close(descriptors[1]);
    });
    if (client_process.is_err())
        server_process.ok().kill();
    ASSERT_TRUE(client_process.is_ok());

    ASSERT_TRUE(client_process.ok().wait());
    ASSERT_TRUE(server_process.ok().wait());
}

TEST(Message, RejectsShortTypedPayloadWithoutLeakingFd) {
    auto barrier = ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok());
    static const std::string socket_name = "message-fd-short-payload-test.sock";

    auto server_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto srv = server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(srv.is_ok());
        barrier.ok().arrive_and_wait();
        auto client = srv.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok());

        auto descriptor_count = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count.has_value());
        ASSERT_GT(descriptor_count.value(), 0);
        int fd = -1;
        int value;
        auto res = client.ok().receive_file_descriptor(fd, value).sync();
        ASSERT_TRUE(res.is_err());
        ASSERT_EQ(res.err(), EIO);
        ASSERT_EQ(fd, -1);
        auto descriptor_count_after = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count_after.has_value());
        ASSERT_GT(descriptor_count_after.value(), 0);
        ASSERT_EQ(descriptor_count_after.value(), descriptor_count.value());
    });
    ASSERT_TRUE(server_process.is_ok());

    auto client_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        int descriptors[2];
        ASSERT_EQ(::pipe(descriptors), 0);
        barrier.ok().arrive_and_wait();
        auto clt = client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(clt.is_ok());
        char value = 0;
        auto res = clt.ok().socket_.send_file_descriptor(descriptors[0], value).sync();
        ASSERT_TRUE(res.is_ok());
        ::close(descriptors[0]);
        ::close(descriptors[1]);
    });
    if (client_process.is_err())
        server_process.ok().kill();
    ASSERT_TRUE(client_process.is_ok());

    ASSERT_TRUE(client_process.ok().wait());
    ASSERT_TRUE(server_process.ok().wait());
}

TEST(Message, RejectsShortTypedPayloadWithoutLeakingFdSet) {
    auto barrier = ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok());
    static const std::string socket_name = "message-fd-set-short-payload-test.sock";

    auto server_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto srv = server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(srv.is_ok());
        barrier.ok().arrive_and_wait();
        auto client = srv.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok());

        auto descriptor_count = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count.has_value());
        ASSERT_GT(descriptor_count.value(), 0);
        std::array<int, 2> fd_array;
        int value;
        auto res = client.ok().receive_file_descriptor_array(fd_array, value).sync();
        ASSERT_TRUE(res.is_err());
        ASSERT_EQ(res.err(), EIO);
        ASSERT_EQ(fd_array[0], -1);
        ASSERT_EQ(fd_array[1], -1);
        auto descriptor_count_after = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count_after.has_value());
        ASSERT_GT(descriptor_count_after.value(), 0);
        ASSERT_EQ(descriptor_count_after.value(), descriptor_count.value());
    });
    ASSERT_TRUE(server_process.is_ok());

    auto client_process = process([&] {
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        int descriptors[2];
        ASSERT_EQ(::pipe(descriptors), 0);
        barrier.ok().arrive_and_wait();
        auto clt = client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(clt.is_ok());
        char value = 0;
        auto res = clt.ok().socket_.send_file_descriptor(descriptors[0], value).sync();
        ASSERT_TRUE(res.is_ok());
        ::close(descriptors[0]);
        ::close(descriptors[1]);
    });
    if (client_process.is_err())
        server_process.ok().kill();
    ASSERT_TRUE(client_process.is_ok());

    ASSERT_TRUE(client_process.ok().wait());
    ASSERT_TRUE(server_process.ok().wait());
}

TEST(Message, RejectsAdditionalAncillaryMessageWithoutLeakingFd) {
    int transport[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, transport), 0);
    int enabled = 1;
    ASSERT_EQ(::setsockopt(transport[0], SOL_SOCKET, SO_PASSCRED, &enabled, sizeof(enabled)), 0);

    auto server_process = process([&] {
        ::close(transport[1]);
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());
        auto handle = sched.ok().register_file_descriptor(transport[0]);
        ASSERT_TRUE(handle.is_ok());
        io::socket receiver(std::move(handle.take_ok()));

        auto descriptor_count = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count.has_value());
        ASSERT_GT(descriptor_count.value(), 0);
        // Deliberately oversized: SCM_CREDENTIALS plus one SCM_RIGHTS needs 56 bytes on Linux; 16 entries provide 80.
        std::array<int, 16> fd_array;
        int value;
        auto res = receiver.receive_file_descriptor_array(fd_array, value).sync();
        ASSERT_TRUE(res.is_err());
        ASSERT_EQ(res.err(), ECOMM);
        for (auto fd : fd_array)
            ASSERT_EQ(fd, -1);
        auto descriptor_count_after = open_file_descriptor_count();
        ASSERT_TRUE(descriptor_count_after.has_value());
        ASSERT_GT(descriptor_count_after.value(), 0);
        ASSERT_EQ(descriptor_count_after.value(), descriptor_count.value());
    });
    ASSERT_TRUE(server_process.is_ok());

    auto client_process = process([&] {
        ::close(transport[0]);
        auto sched = io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());
        auto handle = sched.ok().register_file_descriptor(transport[1]);
        ASSERT_TRUE(handle.is_ok());
        io::socket sender(std::move(handle.take_ok()));

        int descriptors[2];
        ASSERT_EQ(::pipe(descriptors), 0);
        int value = 0;
        auto res = sender.send_file_descriptor(descriptors[0], value).sync();
        ASSERT_TRUE(res.is_ok());
        ::close(descriptors[0]);
        ::close(descriptors[1]);
    });
    ASSERT_TRUE(client_process.is_ok());

    ::close(transport[0]);
    ::close(transport[1]);
    ASSERT_TRUE(client_process.ok().wait());
    ASSERT_TRUE(server_process.ok().wait());
}
