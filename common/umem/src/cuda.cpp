/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/umem/cuda>
#include <mutex>

namespace gdakis::umem {

#if __has_include(<cuda_runtime_api.h>)

static std::mutex cuda_stream_mutex_;

struct cuda_stream_t {
    cudaStream_t stream_;
    cuda_stream_t() noexcept {
        auto ret = cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking);
        if (ret != cudaSuccess)
            stream_ = nullptr;
    }
    cudaStream_t get() noexcept { return stream_; }
    ~cuda_stream_t() noexcept {
        if (stream_)
            cudaStreamDestroy(stream_);
    }
};

static std::unique_ptr<cuda_stream_t> cuda_stream_;

namespace {

bool ensure_runtime_stream_ready() noexcept {
    int device = 0;
    auto status = cudaGetDevice(&device);
    if (status != cudaSuccess) {
        return false;
    }

    // Force lazy runtime initialization before creating the shared stream.
    status = cudaFree(0);
    return status == cudaSuccess;
}

} // namespace

cudaStream_t cuda::get_stream() noexcept {
    std::lock_guard<std::mutex> lock(cuda_stream_mutex_);
    if (cuda_stream_) {
        return cuda_stream_->get();
    }

    if (!ensure_runtime_stream_ready()) {
        return nullptr;
    }

    auto stream = std::make_unique<cuda_stream_t>();
    if (stream == nullptr || stream->get() == nullptr) {
        return nullptr;
    }
    cuda_stream_ = std::move(stream);

    return cuda_stream_ ? cuda_stream_->get() : nullptr;
}

#endif

}
