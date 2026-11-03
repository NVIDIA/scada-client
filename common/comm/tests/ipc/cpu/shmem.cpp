/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/ipc/shmem>
#include <gdakis/umem/cpu>
#include <tests/utility>

#include <algorithm>
#include <random>
#include <vector>

namespace ns = gdakis::comm::ipc;

TEST(CPUIPCTest, Alloc) {
    auto ptr = ns::shmem<gdakis::umem::cpu, unsigned>::alloc();
    EXPECT_NE(ptr, nullptr) << "Failed to allocate memory";
}

TEST(CPUIPCTest, AllocMany) {
    static constexpr std::size_t n_elems = 4096;
    auto ptr = ns::shmem<gdakis::umem::cpu, unsigned[]>::alloc(n_elems, [](unsigned, unsigned *) {});
    ASSERT_NE(ptr, nullptr) << "Failed to allocate memory";
}

TEST(CPUIPCTest, Import) {
    auto ptr = ns::shmem<gdakis::umem::cpu, unsigned>::alloc();
    ASSERT_NE(ptr, nullptr) << "Failed to allocate memory";
    *(ptr.get()) = 0xcafe;

    auto handle = ptr.handle();
    ASSERT_TRUE(handle.is_ok()) << "Failed to obtain a handle";
    auto other = ns::shmem<gdakis::umem::cpu, unsigned>::import(handle.take_ok(), ptr.ancillary());
    ASSERT_NE(other, nullptr) << "Failed to import memory";
    ASSERT_EQ(*(other.get()), 0xcafe) << "Inconsistent data";
}

TEST(CPUIPCTest, ImportMany) {
    static constexpr std::size_t n_elems = 4096;
    auto ptr
        = ns::shmem<gdakis::umem::cpu, unsigned[]>::alloc(n_elems, [](unsigned id, unsigned * value) { *value = id; });
    ASSERT_NE(ptr, nullptr);

    auto handle = ptr.handle();
    ASSERT_TRUE(handle.is_ok());
    auto other = ns::shmem<gdakis::umem::cpu, unsigned[]>::import(handle.take_ok(), ptr.ancillary(), n_elems);
    ASSERT_NE(other, nullptr);
    for (unsigned n = 0; n < n_elems; ++n) {
        ASSERT_EQ(ptr.get()[n], other.get()[n]);
    }
}

TEST(CPUIPCTest, AllocManyInhereit) {
    std::random_device random_device;
    std::mt19937 mersenne_engine { random_device() };
    std::uniform_int_distribution<unsigned> dist(0);
    static constexpr std::size_t vector_size = 4096;
    std::vector<unsigned> values(vector_size);
    std::generate(std::begin(values), std::end(values), [&]() { return dist(mersenne_engine); });

    auto buffer = ns::shmem<gdakis::umem::cpu, unsigned[]>::alloc(
        vector_size, [](unsigned id, unsigned * value) { *value = 0xdeadbeef; });
    ASSERT_NE(buffer, nullptr) << "Buffer allocation failed";

    auto generator = tests::process([&] {
        unsigned * ptr = buffer.get();
        for (const auto & value : values) {
            *ptr = value;
            ++ptr;
        }
    });
    ASSERT_TRUE(generator.wait());

    unsigned * ptr = buffer.get();
    for (const auto & value : values) {
        ASSERT_EQ(value, *ptr);
        ++ptr;
    }
}

TEST(CPUIPCTest, ShareAndExit) {
    std::random_device random_device;
    std::mt19937 mersenne_engine { random_device() };
    std::uniform_int_distribution<unsigned> dist(0);
    static constexpr std::size_t vector_size = 4096;
    std::vector<unsigned> values(vector_size);
    std::generate(std::begin(values), std::end(values), [&]() { return dist(mersenne_engine); });

    static const std::string socket_name = "cpu-shmem-ipc-shared-and-exit.sock";
    auto barrier = tests::ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok()) << "Shared barrier allocation failed";

    auto producer = tests::process([&] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok()) << "Failed to create I/O scheduler on consumer process";
        auto server = tests::server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(server.is_ok()) << "Failed to create server err = " << server.err();
        barrier.ok().arrive_and_wait();

        std::cerr << "Producer accepting connections" << std::endl;
        auto client = server.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to accept connection err = " << client.err();

        std::array<int, 1> ancillary;
        ns::ptr<gdakis::umem::cpu, unsigned>::handle_type handle;
        auto res = client.ok().receive_file_descriptor_array(ancillary, handle).sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to receive file descriptors err = " << res.err();
        ASSERT_NE(std::get<0>(ancillary), -1) << "Received invalid ancillary data";
        barrier.ok().arrive_and_wait();

        auto buffer = ns::shmem<gdakis::umem::cpu, unsigned[]>::import(std::move(handle), ancillary, vector_size);
        ASSERT_NE(buffer.get(), nullptr) << "Failed to import buffer";

        unsigned * ptr = buffer.get();
        for (const auto & value : values) {
            ASSERT_EQ(value, *ptr);
            ++ptr;
        }
    });
    barrier.ok().arrive_and_wait();

    std::cerr << "Staring generator process" << std::endl;
    auto generator = tests::process([&] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok()) << "Failed to create I/O scheduler on generator process err = " << sched.err();

        auto client = tests::client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to connect to '" << socket_name << "' err = " << client.err();
        auto buffer = ns::shmem<gdakis::umem::cpu, unsigned[]>::alloc(
            vector_size, [&](unsigned id, unsigned * value) { *value = values[id]; });
        ASSERT_NE(buffer, nullptr) << "Buffer allocation failed";

        auto handle = buffer.handle();
        ASSERT_TRUE(handle.is_ok()) << "Failed to get IPC handle err = " << handle.err();
        auto ancillary = buffer.ancillary();
        ASSERT_NE(std::get<0>(ancillary), -1) << "Invalid ancillary data for buffer";

        auto res = client.ok().socket_.send_file_descriptor_array(ancillary, handle.ok()).sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to send buffer err = " << res.err();
    });

    EXPECT_TRUE(generator.wait());
    barrier.ok().arrive_and_wait();
    EXPECT_TRUE(producer.wait());
}

TEST(CPUIPCTest, ReSharing) {
    std::random_device random_device;
    std::mt19937 mersenne_engine { random_device() };
    std::uniform_int_distribution<unsigned> dist(0);
    static constexpr std::size_t vector_size = 4096;

    std::vector<unsigned> initial_values(vector_size);
    std::generate(std::begin(initial_values), std::end(initial_values), [&]() { return dist(mersenne_engine); });

    std::vector<unsigned> final_values(vector_size);
    std::generate(std::begin(final_values), std::end(final_values), [&]() { return dist(mersenne_engine); });

    static const std::string socket_first = "cpu-shmem-reshare-1.sock";
    static const std::string socket_second = "cpu-shmem-reshare-2.sock";
    auto first = tests::ipc_barrier::make(2);
    ASSERT_TRUE(first.is_ok()) << "Failed to create first barrier err = " << first.err();

    auto second = tests::ipc_barrier::make(2);
    ASSERT_TRUE(second.is_ok()) << "Failed to create second barrier err = " << second.err();

    auto generator = tests::process([&] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok()) << "Failed to create I/O scheduler on generator process err = " << sched.err();
        first.ok().arrive_and_wait();

        auto client = tests::client::connect(sched.ok(), socket_first).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to connect to server on generator process err = " << client.err();
        auto buffer = ns::shmem<gdakis::umem::cpu, unsigned[]>::alloc(
            vector_size, [&](unsigned id, unsigned * value) { *value = initial_values[id]; });
        ASSERT_NE(buffer, nullptr) << "Failed to allocate buffer on generator process";

        auto handle = buffer.handle();
        ASSERT_TRUE(handle.is_ok()) << "Invalid buffer handle err = " << handle.err();
        auto ancillary = buffer.ancillary();
        ASSERT_NE(std::get<0>(ancillary), -1) << "Invalid ancillary data for buffer";

        auto res = client.ok().socket_.send_file_descriptor_array(ancillary, handle.ok()).sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to send file descriptor err = " << res.err();

        first.ok().arrive_and_wait();

        unsigned * ptr = buffer.get();
        for (const auto & value : final_values) {
            ASSERT_EQ(value, *ptr);
            ++ptr;
        }
    });

    auto middle = tests::process([&] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok()) << "Failed to create I/O buffer on middle process err = " << sched.err();

        // Server receives shared buffer
        auto server = tests::server::make(sched.ok(), socket_first).sync();
        ASSERT_TRUE(server.is_ok()) << "Failed to create server on middle process err = " << server.err();

        first.ok().arrive_and_wait();

        auto client = server.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to accept connection on middle process err = " << client.err();

        std::array<int, 1> fds;
        ns::ptr<gdakis::umem::cpu, unsigned>::handle_type handle;
        auto res = client.ok().receive_file_descriptor_array(fds, handle).sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to receive file descriptor on middle process err = " << res.err();
        ASSERT_NE(std::get<0>(fds), -1) << "Received invalid file descriptor on middle process";

        ns::ptr<gdakis::umem::cpu, unsigned>::ancillary_type ancillary { fds };
        auto buffer = ns::shmem<gdakis::umem::cpu, unsigned[]>::import(std::move(handle), ancillary, vector_size);
        ASSERT_NE(buffer.get(), nullptr) << "Failed to import buffer on middle process";

        // Server checks data integrity and transforms them
        for (auto n = 0ul; n < vector_size; ++n) {
            ASSERT_EQ(buffer.get()[n], initial_values[n]);
            buffer.get()[n] = final_values[n];
        }
        first.ok().arrive_and_wait(); // Trigger check on generator

        // Wait for final consumer to be ready to accept connections
        second.ok().arrive_and_wait();

        auto next = tests::client::connect(sched.ok(), socket_second).sync();
        ASSERT_TRUE(next.is_ok()) << "Middle process failed to connect to last process err = " << next.err();

        auto other_handle = buffer.handle();
        ASSERT_TRUE(other_handle.is_ok()) << "Invalid buffer handle err = " << other_handle.err();
        auto other_ancillary = buffer.ancillary();
        ASSERT_NE(std::get<0>(other_ancillary), -1) << "Invalid ancillary data for buffer";

        auto ret = next.ok().socket_.send_file_descriptor_array(other_ancillary, other_handle.ok()).sync();
        ASSERT_TRUE(ret.is_ok()) << "Failed to send file descriptor to last process err = " << ret.err();
    });

    auto last = tests::process([&] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok()) << "Failed to create I/O scheduler on last process err = " << sched.err();
        auto server = tests::server::make(sched.ok(), socket_second).sync();
        ASSERT_TRUE(server.is_ok()) << "Failed to create server on last process err = " << server.err();
        second.ok().arrive_and_wait(); // Ready to accept connections

        auto client = server.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to accept connection on last process err = " << client.err();

        std::array<int, 1> fds;
        ns::ptr<gdakis::umem::cpu, unsigned>::handle_type handle;
        auto res = client.ok().receive_file_descriptor_array(fds, handle).sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to receive file descriptor on last process err = " << res.err();
        ASSERT_NE(std::get<0>(fds), -1) << "Received invalid file descriptor on last process";

        ns::ptr<gdakis::umem::cpu, unsigned>::ancillary_type ancillary { fds };
        auto buffer = ns::shmem<gdakis::umem::cpu, unsigned[]>::import(std::move(handle), ancillary, vector_size);
        ASSERT_NE(buffer.get(), nullptr) << "Failed to import buffer on last process";

        unsigned * ptr = buffer.get();
        for (const auto & value : final_values) {
            ASSERT_EQ(value, *ptr);
            ++ptr;
        }
    });

    ASSERT_TRUE(generator.wait()) << "Error waiting for generator process";
    ASSERT_TRUE(middle.wait()) << "Error waiting for middle process";
    ASSERT_TRUE(last.wait()) << "Error waiting for last process";
}

class CPUDMATest : public ::testing::Test {
protected:
    void SetUp() override {
        if (ns::shmem<gdakis::umem::cpu, void>::supports_dma() == false) {
            GTEST_SKIP() << "CPU DMA memory no available";
        }
    }
};

TEST(CPUDMATest, Alloc) {
    static constexpr std::size_t n_elems = 4096;
    auto ptr = ns::shmem<gdakis::umem::cpu, unsigned[]>::alloc_dma(n_elems, [](unsigned, unsigned *) {});
    EXPECT_NE(ptr, nullptr);
}

TEST(CPUDMATest, Import) {
    static constexpr std::size_t n_elems = 4096;
    auto ptr = ns::shmem<gdakis::umem::cpu, unsigned[]>::alloc_dma(
        n_elems, [](unsigned id, unsigned * value) { *value = id; });
    ASSERT_NE(ptr, nullptr);

    auto handle = ptr.handle();
    ASSERT_TRUE(handle.is_ok());
    auto other = ns::shmem<gdakis::umem::cpu, unsigned[]>::import_dma(handle.take_ok(), ptr.ancillary(), n_elems);
    ASSERT_NE(other, nullptr);
    for (unsigned n = 0; n < n_elems; ++n) {
        ASSERT_EQ(ptr.get()[n], other.get()[n]);
    }
}

TEST(CPUDMATest, ShareAndExit) {
    std::random_device random_device;
    std::mt19937 mersenne_engine { random_device() };
    std::uniform_int_distribution<unsigned> dist(0);
    static constexpr std::size_t vector_size = 4096;
    std::vector<unsigned> values(vector_size);
    std::generate(std::begin(values), std::end(values), [&]() { return dist(mersenne_engine); });

    static const std::string socket_name = "cpu-shmem-dma-shared-and-exit.sock";
    auto barrier = tests::ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok()) << "Shared barrier allocation failed";

    auto producer = tests::process([&] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok()) << "Failed to create I/O scheduler on consumer process";
        auto server = tests::server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(server.is_ok()) << "Failed to create server err = " << server.err();
        barrier.ok().arrive_and_wait();

        auto client = server.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to accept connection err = " << client.err();

        std::array<int, 2> ancillary;
        ns::ptr<gdakis::umem::cpu, unsigned>::handle_type handle;
        auto res = client.ok().receive_file_descriptor_array(ancillary, handle).sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to receive file descriptors err = " << res.err();
        ASSERT_NE(std::get<0>(ancillary), -1) << "Received invalid shared memory ancillary data";
        ASSERT_NE(std::get<1>(ancillary), -1) << "Received invalid DMA ancillary data";
        barrier.ok().arrive_and_wait();

        auto buffer = ns::shmem<gdakis::umem::cpu, unsigned[]>::import_dma(std::move(handle), ancillary, vector_size);
        ASSERT_NE(buffer.get(), nullptr) << "Failed to import buffer";

        unsigned * ptr = buffer.get();
        for (const auto & value : values) {
            ASSERT_EQ(value, *ptr);
            ++ptr;
        }
    });
    barrier.ok().arrive_and_wait();

    auto generator = tests::process([&] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok()) << "Failed to create I/O scheduler on generator process err = " << sched.err();

        auto client = tests::client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to connect to '" << socket_name << "' err = " << client.err();
        auto buffer = ns::shmem<gdakis::umem::cpu, unsigned[]>::alloc_dma(
            vector_size, [&](unsigned id, unsigned * value) { *value = values[id]; });
        ASSERT_NE(buffer, nullptr) << "Buffer allocation failed";

        auto handle = buffer.handle();
        ASSERT_TRUE(handle.is_ok()) << "Failed to get IPC handle err = " << handle.err();
        auto ancillary = buffer.ancillary();
        ASSERT_NE(std::get<0>(ancillary), -1) << "Invalid shared memory ancillary data for buffer";
        ASSERT_NE(std::get<1>(ancillary), -1) << "Invalid DMA ancillary data for buffer";

        auto res = client.ok().socket_.send_file_descriptor_array(ancillary, handle.ok()).sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to send buffer err = " << res.err();
    });

    EXPECT_TRUE(generator.wait());
    barrier.ok().arrive_and_wait();
    EXPECT_TRUE(producer.wait());
}
