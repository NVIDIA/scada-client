/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

#include <psa/coaio/detached>
#include <psa/coaio/scheduler/epoll>
#include <psa/coaio/socket>
#include <psa/futures>
#include <psa/result>

#include <sys/wait.h>

namespace io {
using scheduler = psa::coaio::scheduler::epoll;
using socket = psa::coaio::socket<scheduler>;

template<typename _Tp = psa::coaio::none_type>
using result = psa::result<_Tp, int>;

using detached = psa::coaio::detached;

namespace futures {
template<typename _Tp = psa::coaio::none_type>
using result = psa::futures::result<_Tp, int>;
}

struct value_type {
    unsigned value_;
    inline value_type(unsigned val = -1) : value_(val) {}
    inline unsigned ack() const noexcept { return value_; }
};
}

class server {
protected:
    io::scheduler scheduler_;
    io::socket socket_;
    unsigned backlog_;

public:
    inline server(io::scheduler && scheduler, io::socket && socket, unsigned backlog) noexcept
        : scheduler_(std::move(scheduler)),
          socket_(std::move(socket)),
          backlog_(backlog) {}
    server(server &&) noexcept = default;

    static inline io::result<server> build(const std::string & name, unsigned backlog) noexcept {
        auto scheduler = io::scheduler::create();
        if (scheduler.is_err())
            return io::result<server>::err(scheduler.err());

        auto socket = io::socket::make(scheduler.ok(), io::socket::domain::local, io::socket::type::stream).sync();
        if (socket.is_err())
            return io::result<server>::err(socket.err());

        // Make sure only one server is listening here
        auto addr = io::socket::address<io::socket::domain::local>::from_string(name);
        if (addr.is_err())
            return io::result<server>::err(addr.err());
        int err = ::unlink(addr.ok().sun_path);
        if (err < 0 and errno != ENOENT)
            return io::result<server>::err(errno);

        auto socket_addr = addr.ok();
        auto res = socket.ok()
                       .bind(reinterpret_cast<::sockaddr *>(&socket_addr), sizeof(socket_addr))
                       .and_then([&socket, backlog = backlog](auto) { return socket.ok().listen(backlog); })
                       .or_else([&socket_addr](auto err) {
                           ::unlink(socket_addr.sun_path);
                           return io::result<>::err(err);
                       })
                       .sync();

        return res.is_ok() ? io::result<server>::ok(scheduler.take_ok(), socket.take_ok(), backlog)
                           : io::result<server>::err(res.err());
    }

    inline io::futures::result<> run() noexcept {
        std::cerr << "Server waiting for connections" << std::endl;
        size_t count = 0;
        std::once_flag timer_start;
        using time_point = std::chrono::time_point<std::chrono::high_resolution_clock>;
        time_point start_time, end_time;
        co_await psa::coaio::pooled::spawn(backlog_, [&, this]() -> io::futures::result<> {
            auto res = co_await socket_.accept(scheduler_, nullptr, nullptr)
                           .and_then([&](auto client) -> io::futures::result<> {
                               std::call_once(timer_start,
                                   [&start_time] { start_time = std::chrono::high_resolution_clock::now(); });
                               //std::cerr << "Server accepted connection. Start measuring throughput" << std::endl;
                               io::value_type value;
                               auto res = io::result<>::err(EINVAL);
                               do {
                                   res = co_await client.read(value).and_then(
                                       [&value, &client](auto err) { return client.write(value.ack()); });
                                   ++count;
                               } while (res.is_ok());
                               co_return res.err() == ENODATA ? io::result<>::ok<>() : io::result<>::err(res.err());
                           });
            if (res.is_err())
                std::cerr << "Server cannot accept connection (err = " << res.err() << ")" << std::endl;
            co_return io::result<>::ok<>();
        });
        end_time = std::chrono::high_resolution_clock::now();
        std::cerr << "Server connection closed. Reporting " << count << " operations" << std::endl;
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        std::cerr << "Server run for " << duration_us.count() << "us (" << 1.0f * count / duration_us.count()
                  << " MRPS)" << std::endl;
        co_return io::result<>::ok<>();
    }
};

class client {
protected:
    io::scheduler scheduler_;
    io::socket socket_;
    unsigned count_;
    unsigned id_;

public:
    inline client(io::scheduler && scheduler, io::socket && socket, unsigned count, unsigned id) noexcept
        : scheduler_(std::move(scheduler)),
          socket_(std::move(socket)),
          count_(count),
          id_(id) {}

    static inline io::result<client> build(unsigned id, const std::string & name, unsigned count) noexcept {
        auto scheduler = io::scheduler::create();
        if (scheduler.is_err())
            return io::result<client>::err(scheduler.err());

        auto socket = io::socket::make(scheduler.ok(), io::socket::domain::local, io::socket::type::stream).sync();
        if (socket.is_err()) {
            std::cerr << "Failed to create client socket (errno = " << socket.err() << ")" << std::endl;
            return io::result<client>::err(socket.err());
        }

        auto addr = io::socket::address<io::socket::domain::local>::from_string(name);
        if (addr.is_err()) {
            std::cerr << "Invalid UNIX socket address '" << name << "'" << std::endl;
            return io::result<client>::err(addr.err());
        }

        //std::cerr << "Connecting client #" << id << " to server" << std::endl;
        auto socket_addr = addr.ok();
        auto ret = socket.ok().connect(reinterpret_cast<::sockaddr *>(&socket_addr), sizeof(socket_addr)).sync();
        if (ret.is_err()) {
            std::cerr << "Failed to connect to server (errno = " << ret.err() << ")" << std::endl;
            return io::result<client>::err(ret.err());
        }
        //std::cerr << "Client #" << id << " connected and ready" << std::endl;

        return io::result<client>::ok(scheduler.take_ok(), socket.take_ok(), count, id);
    }

    inline io::futures::result<> run() noexcept {
        //std::cerr << "Client #" << id_ << " starts running" << std::endl;
        for (auto n = 0u; n < count_; ++n) {
            io::value_type value(n);
            co_await socket_.write(value).and_then([&](auto) -> io::futures::result<> {
                unsigned ack;
                auto ret = co_await socket_.read(ack);
                if (ret.is_err())
                    co_return ret;
                co_return ack == n ? io::result<>::ok<>() : io::result<>::err(EINVAL);
            });
        }
        //std::cerr << "Client #" << id_ << " completes " << count_ << " operations" << std::endl;
        co_return io::result<>::ok<>();
    }
};

class process {
protected:
    std::function<void(void)> fn_;
    pid_t pid_;

public:
    template<typename _Fn>
    inline process(_Fn && fn) noexcept : fn_(std::move(fn)),
                                         pid_(-1) {}

    inline int wait() const noexcept {
        if (pid_ == 0)
            return -EINVAL;
        else if (pid_ > 0)
            ::waitpid(pid_, nullptr, 0);
        return 0;
    }

    io::result<> run() noexcept {
        pid_ = ::fork();
        if (pid_ < 0)
            return io::result<>::err(errno);
        else if (pid_ > 0)
            return io::result<>::ok<>();

        fn_();
        ::exit(0);
    }
};

static const char * unix_socket_name = "coaio-perf.sock";

int main(int argc, char * argv[]) {
    auto n_requests = 32 * 1024;
    auto n_cores = std::thread::hardware_concurrency();
    if (n_cores > 1)
        n_cores--;

    std::cerr << "Launching server" << std::endl;
    auto server = server::build(unix_socket_name, n_cores);
    if (server.is_err()) {
        std::cerr << "Failed to build server (errno = " << server.err() << ")" << std::endl;
        return EXIT_FAILURE;
    }
    std::cerr << "Starting server" << std::endl;
    ;
    auto future = server.ok().run();

    std::cerr << "Launching client process" << std::endl;
    auto client = process([&]() {
        std::cerr << "Running client " << getpid() << " on " << n_cores << " CPUs" << std::endl;
        std::vector<std::thread> workers;
        std::atomic<unsigned> count = 0;
        for (auto n = 0u; n < n_cores; ++n) {
            workers.emplace_back([&, n]() {
                auto client = client::build(n, unix_socket_name, n_requests);
                if (client.is_err()) {
                    std::cerr << "Failed to build client (errno = " << client.err() << ")" << std::endl;
                    ::exit(errno);
                }
                count.fetch_add(1, std::memory_order_release);
                client.ok().run().sync();
            });
        }
        for (auto & worker : workers)
            worker.join();
        assert(count.load(std::memory_order_acquire) == n_cores);
        ::exit(0);
    });
    client.run();

    std::cerr << "Server process waiting for connections " << getpid() << std::endl;
    auto ret = future.sync();
    if (ret.is_ok()) {
        std::cerr << "Server completes successfully" << std::endl;
    } else {
        std::cerr << "Server completes with error " << ret.err() << std::endl;
    }

    std::cerr << "Waiting for client process" << std::endl;
    client.wait();

    return EXIT_SUCCESS;
}
