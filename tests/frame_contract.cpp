/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <gdakis/ipc/frame_traits>
#include <scada/swap/types>

#include <iostream>

template<gdakis::comm::operation _Op, typename _Backend>
void print_frame() {
    using traits = gdakis::comm::ipc::frame_traits<_Op, _Backend>;
    static_assert(traits::frame_size >= sizeof(gdakis::comm::header_type));
    std::cout << static_cast<unsigned>(_Backend::device_type) << ' ' << static_cast<unsigned>(_Op) << ' '
              << traits::frame_size << ' ' << traits::fd_count << '\n';
}

template<typename _Backend, std::size_t _MemoryDescriptors>
void print_frames() {
    using op = gdakis::comm::operation;
    static_assert(gdakis::comm::ipc::frame_traits<op::get_protocol_version, _Backend>::fd_count == 0);
    static_assert(gdakis::comm::ipc::frame_traits<op::register_request_buffers, _Backend>::fd_count == 2);
    static_assert(
        gdakis::comm::ipc::frame_traits<op::register_memory_buffer, _Backend>::fd_count == _MemoryDescriptors);
    static_assert(gdakis::comm::ipc::frame_traits<op::register_io_buffer_pool, _Backend>::fd_count == 4);
    static_assert(gdakis::comm::ipc::frame_traits<op::deregister_request_buffers, _Backend>::fd_count == 0);
    static_assert(gdakis::comm::ipc::frame_traits<op::deregister_memory_buffer, _Backend>::fd_count == 0);
    static_assert(gdakis::comm::ipc::frame_traits<op::deregister_io_buffer_pool, _Backend>::fd_count == 0);
    print_frame<op::get_protocol_version, _Backend>();
    print_frame<op::register_request_buffers, _Backend>();
    print_frame<op::deregister_request_buffers, _Backend>();
    print_frame<op::register_memory_buffer, _Backend>();
    print_frame<op::deregister_memory_buffer, _Backend>();
    print_frame<op::register_io_buffer_pool, _Backend>();
    print_frame<op::deregister_io_buffer_pool, _Backend>();
}

int main() {
    print_frames<gdakis::comm::ipc::cpu::backend_type, 2>();
    print_frames<gdakis::comm::ipc::cuda::backend_type, 2>();
}
