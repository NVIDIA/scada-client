/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/ipc/memory_buffer>
#include <gdakis/ipc/protocol>
#include <gdakis/ipc/shmem>
#include <gdakis/protocol>
#include <tests/utility>

#include <cuda/atomic>

namespace ns = gdakis::comm::ipc::cuda;
static inline psa::result<gdakis::comm::ipc::memory_buffer<unsigned>::template handle<gdakis::umem::cuda>, int>
make_memory_buffer(size_t size) {
    return gdakis::comm::ipc::memory_buffer<unsigned>::template handle<gdakis::umem::cuda>::build(
        size, [] __host__ __device__(unsigned id, unsigned * value) { *value = id; });
}

static __global__ void check_memory_buffer_kernel(
    unsigned * data, size_t size, cuda::atomic<unsigned, cuda::thread_scope_device> * errors) {
    auto id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id >= size)
        return;
    else if (data[id] != id)
        errors->fetch_add(1, cuda::memory_order_relaxed);
}

static inline void check_memory_buffer(unsigned * data, size_t size) {
    cudaError_t cuda_ret;
    cuda::atomic<unsigned, cuda::thread_scope_device> * errors;
    cuda_ret = cudaMalloc(&errors, sizeof(*errors));
    ASSERT_EQ(cuda_ret, cudaSuccess);

    cuda_ret = cudaMemset(errors, 0, sizeof(*errors));
    ASSERT_EQ(cuda_ret, cudaSuccess);

    dim3 db = size < 128 ? size : 128;
    dim3 dg = (size + db.x - 1) / db.x;
    check_memory_buffer_kernel<<<dg, db>>>(data, size, errors);
    cuda_ret = cudaDeviceSynchronize();
    ASSERT_EQ(cuda_ret, cudaSuccess);

    cuda::atomic<unsigned, cuda::thread_scope_device> out;
    cuda_ret = cudaMemcpy(&out, errors, sizeof(*errors), cudaMemcpyDeviceToHost);
    ASSERT_EQ(cuda_ret, cudaSuccess);

    cuda_ret = cudaFree(errors);
    ASSERT_EQ(cuda_ret, cudaSuccess);

    ASSERT_EQ(out.load(), 0);
}

class CUDAIPCMemoryBuffer : public ::tests::CUDATest {
protected:
    void SetUp() override {
        if (gdakis::comm::ipc::shmem<gdakis::umem::cuda, void>::supports_dma() == false) {
            GTEST_SKIP() << "CUDA DMA memory no available";
        }
    }
};

TEST(CUDAIPCMemoryBuffer, Create) {
    auto process = tests::process([] {
        auto buffer = make_memory_buffer(1024);
        ASSERT_TRUE(buffer.is_ok());
        ASSERT_NE(buffer.ok().get().data(), nullptr);
        check_memory_buffer(buffer.ok().get().data(), 1024);
    });
    ASSERT_TRUE(process.wait());
}

TEST(CUDAIPCMemoryBuffer, SendToServer) {
    auto process = tests::process([] {
        static const std::string socket_name = "cuda-ipc-memory-buffer-test.sock";
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

            gdakis::comm::message<gdakis::comm::operation::register_memory_buffer, ns::backend_type> msg;
            std::array<int, 2> ancillary;
            auto res = client.ok().receive_file_descriptor_array(ancillary, msg.get()).sync();
            ASSERT_TRUE(res.is_ok());
            ASSERT_EQ(msg.get().header_.op_, gdakis::comm::operation::register_memory_buffer);
            ASSERT_EQ(msg.get().header_.device_, gdakis::comm::device_type::cuda);
            ASSERT_EQ(msg.get().header_.transport_, gdakis::comm::transport_type::ipc);
            ASSERT_NE(std::get<0>(msg.ancillary()), -1);
            ASSERT_NE(std::get<1>(msg.ancillary()), -1);

            gdakis::comm::acknowledge<gdakis::comm::operation::register_memory_buffer, ns::backend_type> ack;
            ack.status = gdakis::comm::status_type::ok;
            ack.id = 0xcafe;
            auto ret = client.ok().write(ack).sync();
            ASSERT_TRUE(ret.is_ok());
            barrier.ok().arrive_and_wait();
        });

        // Delay CUDA initialization
        auto buffer = make_memory_buffer(1024);
        ASSERT_TRUE(buffer.is_ok());
        ASSERT_NE(buffer.ok().get().data(), nullptr);

        auto sched = tests::io::scheduler::create();
        ASSERT_TRUE(sched.is_ok());

        barrier.ok().arrive_and_wait();
        auto client = tests::client::connect(sched.ok(), socket_name).sync();
        ASSERT_TRUE(client.is_ok());

        std::cerr << "Sending memory buffer " << std::endl;
        auto res = buffer.ok().send_to_server(client.ok().socket_).sync();
        if (res.is_err()) {
            std::cerr << "Send err = " << res.err() << std::endl;
        }
        ASSERT_TRUE(res.is_ok());
        std::cerr << "Memory buffer registered" << std::endl;
        ASSERT_TRUE(buffer.ok().id().has_value());
        ASSERT_EQ(buffer.ok().id().value(), 0xcafe);
        barrier.ok().arrive_and_wait();
    });
    ASSERT_TRUE(process.wait());
}