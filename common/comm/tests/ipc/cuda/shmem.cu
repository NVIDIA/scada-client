/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/ipc/shmem>
#include <gdakis/umem/cuda>
#include <tests/utility>

#include <psa/coaio/scheduler/epoll>
#include <psa/coaio/socket>

#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

namespace ns = gdakis::comm::ipc;
class CUDAIPCTest : public ::tests::CUDATest {};

class CUDADMATest : public ::tests::CUDATest {
protected:
    void SetUp() override {
        if (ns::shmem<gdakis::umem::cuda, void>::supports_dma() == false) {
            GTEST_SKIP() << "CUDA DMA memory no available";
        }
    }
};

TEST(CUDAIPCTest, Alloc) {
    auto child = tests::process([] {
        auto ptr = ns::shmem<gdakis::umem::cuda, unsigned>::alloc();
        EXPECT_NE(ptr, nullptr);
    });
    ASSERT_TRUE(child.wait());
}

static inline ns::ptr<gdakis::umem::cuda, unsigned> alloc_many_zero(std::size_t n_elems) {
    return ns::shmem<gdakis::umem::cuda, unsigned[]>::alloc(
        n_elems, [] __host__ __device__(unsigned, unsigned * value) { *value = 0; });
}

TEST(CUDAIPCTest, AllocMany) {
    static constexpr std::size_t n_elems = 4096;
    auto child = tests::process([] {
        auto ptr = alloc_many_zero(n_elems);
        EXPECT_NE(ptr, nullptr);
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);
    });
    ASSERT_TRUE(child.wait());
}

static __global__ void cuda_ipc_test_write_kernel(unsigned * value) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    value[id] = 0xdeadbeef;
}

TEST(CUDAIPCTest, AllocAndUpdate) {
    auto child = tests::process([] {
        auto ptr = ns::shmem<gdakis::umem::cuda, unsigned>::alloc();
        ASSERT_NE(ptr, nullptr);

        cuda_ipc_test_write_kernel<<<1, 1>>>(ptr.get());
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

        unsigned value;
        auto cuda_ret = cudaMemcpy(&value, ptr.get(), sizeof(unsigned), cudaMemcpyDeviceToHost);
        ASSERT_EQ(cuda_ret, cudaSuccess);
        ASSERT_EQ(value, 0xdeadbeef);
    });
    ASSERT_TRUE(child.wait());
}

static inline ns::ptr<gdakis::umem::cuda, unsigned> alloc_many_identity(std::size_t n_elems) {
    return ns::shmem<gdakis::umem::cuda, unsigned[]>::alloc(
        n_elems, [] __host__ __device__(unsigned id, unsigned * value) { *value = id; });
}

static __global__ void cuda_ipc_test_set_kernel(unsigned * value) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    value[id] = id << 1;
}

TEST(CUDAIPCTest, AllocManyAndUpdate) {
    static constexpr std::size_t n_elems = 4096;
    auto child = tests::process([] {
        auto ptr = alloc_many_identity(n_elems);
        ASSERT_NE(ptr, nullptr);
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

        unsigned * host = new unsigned[n_elems];
        ASSERT_NE(host, nullptr);

        auto cuda_ret = cudaMemcpy(host, ptr.get(), n_elems * sizeof(unsigned), cudaMemcpyDeviceToHost);
        ASSERT_EQ(cuda_ret, cudaSuccess);
        for (unsigned n = 0; n < n_elems; ++n) {
            ASSERT_EQ(host[n], n);
        }

        cuda_ipc_test_set_kernel<<<32, 128>>>(ptr.get());
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);

        cuda_ret = cudaMemcpy(host, ptr.get(), n_elems * sizeof(unsigned), cudaMemcpyDeviceToHost);
        ASSERT_EQ(cuda_ret, cudaSuccess);
        for (unsigned n = 0; n < n_elems; ++n) {
            ASSERT_EQ(host[n], n << 1);
        }

        delete[] host;
    });
    ASSERT_TRUE(child.wait());
}

TEST(CUDAIPCTest, GetHandle) {
    auto child = tests::process([] {
        auto ptr = ns::shmem<gdakis::umem::cuda, unsigned>::alloc();
        ASSERT_NE(ptr, nullptr);
        auto handle = ptr.handle();
        ASSERT_TRUE(handle.is_ok());
    });
    ASSERT_TRUE(child.wait());
}

static __global__ void cuda_ipc_read_kernel(unsigned * value, unsigned * out) {
    unsigned id = threadIdx.x + blockDim.x * blockIdx.x;
    out[id] = value[id];
}

TEST(CUDAIPCTest, ImportHandle) {
    static const std::string socket_name = "cuda-shmem-ipc-import.sock";
    auto barrier = tests::ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok()) << "Shared barrier allocation failed";

    auto importer = tests::process([&] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());
        auto server = tests::server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(server.is_ok()) << "Failed to create server err = " << server.err();
        barrier.ok().arrive_and_wait();
        auto client = server.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to accept connection err = " << client.err();

        std::array<int, 1> ancillary;
        ns::ptr<gdakis::umem::cuda, unsigned>::handle_type handle;
        auto res = client.ok().receive_file_descriptor_array(ancillary, handle).sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to receive file descriptors err = " << res.err();
        ASSERT_NE(std::get<0>(ancillary), -1) << "Received invalid ancillary data";

        auto ptr = ns::shmem<gdakis::umem::cuda, unsigned>::import(std::move(handle), ancillary);
        ASSERT_NE(ptr, nullptr);
        cuda_ipc_test_write_kernel<<<1, 1>>>(ptr.get());
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);
        unsigned ack = 0xcafe;
        auto ret = client.ok().write(ack).sync();
        ASSERT_TRUE(ret.is_ok());
    });

    barrier.ok().arrive_and_wait();

    auto exporter = tests::process([] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto client = tests::client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to connect to '" << socket_name << "' err = " << client.err();
        auto ptr = ns::shmem<gdakis::umem::cuda, unsigned>::alloc();
        ASSERT_NE(ptr, nullptr) << "Buffer allocation failed";

        auto handle = ptr.handle();
        ASSERT_TRUE(handle.is_ok()) << "Failed to get IPC handle err = " << handle.err();
        auto ancillary = ptr.ancillary();
        ASSERT_NE(std::get<0>(ancillary), -1) << "Invalid ancillary data for buffer";

        unsigned ack = 0;
        auto res = client.ok()
                       .socket_.send_file_descriptor_array(ancillary, handle.ok())
                       .and_then([&client, &ack](auto) { return client.ok().socket_.read(ack); })
                       .sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to send buffer err = " << res.err();
        ASSERT_EQ(ack, 0xcafe);

        thrust::device_vector<unsigned> out(1);
        cuda_ipc_read_kernel<<<1, 1>>>(ptr.get(), out.data().get());
        auto cuda_ret = cudaDeviceSynchronize();
        ASSERT_EQ(cuda_ret, cudaSuccess);

        thrust::host_vector<unsigned> host_out(out);
        ASSERT_EQ(out[0], 0xdeadbeef);
    });

    EXPECT_TRUE(importer.wait());
    EXPECT_TRUE(exporter.wait());
}

static inline ns::ptr<gdakis::umem::cuda, unsigned> alloc_dma_many_zero(std::size_t n_elems) {
    return ns::shmem<gdakis::umem::cuda, unsigned[]>::alloc_dma(
        n_elems, [] __host__ __device__(unsigned, unsigned * value) { *value = 0; });
}

TEST(CUDADMATest, AllocMany) {
    static constexpr std::size_t n_elems = 4096;
    auto child = tests::process([] {
        auto ptr = alloc_many_zero(n_elems);
        EXPECT_NE(ptr, nullptr);
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);
    });
    ASSERT_TRUE(child.wait());
}

TEST(CUDADMATest, ImportHandle) {
    static constexpr std::size_t n_elems = 4096;
    static const std::string socket_name = "cuda-shmem-dma-import.sock";
    auto barrier = tests::ipc_barrier::make(2);
    ASSERT_TRUE(barrier.is_ok()) << "Shared barrier allocation failed";

    auto importer = tests::process([&] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());
        auto server = tests::server::make(sched.ok(), socket_name).sync();
        ASSERT_TRUE(server.is_ok()) << "Failed to create server err = " << server.err();
        barrier.ok().arrive_and_wait();
        auto client = server.ok().accept(sched.ok()).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to accept connection err = " << client.err();

        std::array<int, 2> ancillary;
        ns::ptr<gdakis::umem::cuda, unsigned>::handle_type handle;
        auto res = client.ok().receive_file_descriptor_array(ancillary, handle).sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to receive file descriptors err = " << res.err();
        ASSERT_NE(std::get<0>(ancillary), -1) << "Received invalid ancillary data";

        auto ptr = ns::shmem<gdakis::umem::cuda, unsigned>::import_dma(std::move(handle), ancillary);
        ASSERT_NE(ptr, nullptr);
        cuda_ipc_test_write_kernel<<<32, 128>>>(ptr.get());
        ASSERT_EQ(cudaDeviceSynchronize(), cudaSuccess);
        unsigned ack = 0xcafe;
        auto ret = client.ok().write(ack).sync();
        ASSERT_TRUE(ret.is_ok());
    });

    barrier.ok().arrive_and_wait();

    auto exporter = tests::process([] {
        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        auto client = tests::client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(client.is_ok()) << "Failed to connect to '" << socket_name << "' err = " << client.err();
        auto ptr = alloc_dma_many_zero(n_elems);
        ASSERT_NE(ptr, nullptr) << "Buffer allocation failed";

        auto handle = ptr.handle();
        ASSERT_TRUE(handle.is_ok()) << "Failed to get IPC handle err = " << handle.err();
        auto ancillary = ptr.ancillary();
        ASSERT_NE(std::get<0>(ancillary), -1) << "Invalid ancillary data for buffer";

        unsigned ack = 0;
        auto res = client.ok()
                       .socket_.send_file_descriptor_array(ancillary, handle.ok())
                       .and_then([&client, &ack](auto) { return client.ok().socket_.read(ack); })
                       .sync();
        ASSERT_TRUE(res.is_ok()) << "Failed to send buffer err = " << res.err();
        ASSERT_EQ(ack, 0xcafe);

        thrust::device_vector<unsigned> out(n_elems);
        cuda_ipc_read_kernel<<<32, 128>>>(ptr.get(), out.data().get());
        auto cuda_ret = cudaDeviceSynchronize();
        ASSERT_EQ(cuda_ret, cudaSuccess);

        thrust::host_vector<unsigned> host_out(out);
        for (const auto & v : host_out) {
            ASSERT_EQ(v, 0xdeadbeef);
        }
    });

    EXPECT_TRUE(importer.wait());
    EXPECT_TRUE(exporter.wait());
}
