/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/ipc/io_buffer_pool>
#include <gdakis/ipc/protocol>
#include <gdakis/ipc/shmem>
#include <gdakis/protocol>
#include <tests/utility>

namespace ns = gdakis::comm::ipc::cuda;
static inline psa::result<
    gdakis::comm::ipc::io_buffer_pool<unsigned, gdakis::comm::status_type>::template handle<gdakis::umem::cuda>, int>
make_io_buffer_pool(size_t size) {
    using io_buffer_pool_type = gdakis::comm::ipc::io_buffer_pool<unsigned, gdakis::comm::status_type>;
    return gdakis::comm::ipc::io_buffer_pool<unsigned, gdakis::comm::status_type>::template handle<gdakis::umem::cuda>::
        build(
            size, [] __host__ __device__(unsigned id, unsigned * value) { *value = id; },
            [] __host__ __device__(unsigned id, io_buffer_pool_type::status_type * value) {
                gdakis::atomic_ref<gdakis::comm::status_type, gdakis::thread_scope_device>(*value).store(
                    gdakis::comm::status_type::ok, gdakis::memory_order_relaxed);
            });
}

static __global__ void check_io_buffer_pool_kernel(unsigned * data, gdakis::comm::status_type * status, size_t size,
    cuda::atomic<unsigned, cuda::thread_scope_device> * errors) {
    auto id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;
    else if (data[id] != id
        or gdakis::atomic_ref<gdakis::comm::status_type, gdakis::thread_scope_device>(status[id]).load()
            != gdakis::comm::status_type::ok)
        errors->fetch_add(1, cuda::memory_order_relaxed);
}

static inline void check_io_buffer_pool(unsigned * data, gdakis::comm::status_type * status, size_t size) {
    cudaError_t cuda_ret;
    cuda::atomic<unsigned, cuda::thread_scope_device> * errors;
    cuda_ret = cudaMalloc(&errors, sizeof(*errors));
    ASSERT_EQ(cuda_ret, cudaSuccess);

    cuda_ret = cudaMemset(errors, 0, sizeof(*errors));
    ASSERT_EQ(cuda_ret, cudaSuccess);

    dim3 db = size < 128 ? size : 128;
    dim3 dg = (size + db.x - 1) / db.x;
    check_io_buffer_pool_kernel<<<dg, db>>>(data, status, size, errors);
    cuda_ret = cudaDeviceSynchronize();
    ASSERT_EQ(cuda_ret, cudaSuccess);

    cuda::atomic<unsigned, cuda::thread_scope_device> out;
    cuda_ret = cudaMemcpy(&out, errors, sizeof(*errors), cudaMemcpyDeviceToHost);
    ASSERT_EQ(cuda_ret, cudaSuccess);

    cuda_ret = cudaFree(errors);
    ASSERT_EQ(cuda_ret, cudaSuccess);

    ASSERT_EQ(out.load(), 0);
}

class CUDAIPCIOBufferPool : public ::tests::CUDATest {
protected:
    void SetUp() override {
        if (gdakis::comm::ipc::shmem<gdakis::umem::cuda, void>::supports_dma() == false) {
            GTEST_SKIP() << "CUDA DMA memory no available";
        }
    }
};

TEST(CUDAIPCIOBufferPool, Create) {
    auto process = tests::process([] {
        auto buffer = make_io_buffer_pool(1024);
        ASSERT_TRUE(buffer.is_ok());
        ASSERT_NE(buffer.ok().get().data(), nullptr);
        ASSERT_NE(buffer.ok().get().status(), nullptr);
        check_io_buffer_pool(buffer.ok().get().data(), buffer.ok().get().status(), 1024);
    });
    ASSERT_TRUE(process.wait());
}

TEST(CUDAIPCIOBufferPool, SendToServer) {
    auto process = tests::process([] {
        static const std::string socket_name = "cuda-ipc-io-buffer-pool-test.sock";
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
            gdakis::comm::message<gdakis::comm::operation::register_io_buffer_pool, ns::backend_type> message;
            auto res = client.ok().receive_file_descriptor_array(fd_array, message).sync();
            ASSERT_TRUE(res.is_ok());
            ASSERT_EQ(res.ok(), 4);
            ASSERT_EQ(message.get().header_.op_, gdakis::comm::operation::register_io_buffer_pool);
            ASSERT_EQ(message.get().header_.device_, gdakis::comm::device_type::cuda);
            ASSERT_EQ(message.get().header_.transport_, gdakis::comm::transport_type::ipc);
            ASSERT_NE(std::get<0>(message.ancillary()), -1);
            ASSERT_NE(std::get<1>(message.ancillary()), -1);
            ASSERT_NE(std::get<2>(message.ancillary()), -1);
            ASSERT_NE(std::get<3>(message.ancillary()), -1);

            gdakis::comm::acknowledge<gdakis::comm::operation::register_io_buffer_pool, ns::backend_type> ack;
            ack.status = gdakis::comm::status_type::ok;
            ack.id = 0xcafe;
            auto ret = client.ok().write(ack).sync();
            ASSERT_TRUE(ret.is_ok());
            barrier.ok().arrive_and_wait();
        });

        auto buffer = make_io_buffer_pool(1024);
        ASSERT_NE(buffer.ok().get().data(), nullptr);
        ASSERT_NE(buffer.ok().get().status(), nullptr);

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
    });
    ASSERT_TRUE(process.wait());
}