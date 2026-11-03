/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#if __has_include(<linux/udmabuf.h>)

#  include <gdakis/umem/cpu>

#  include <iostream>
#  include <mutex>

#  include <fcntl.h>
#  include <linux/udmabuf.h>
#  include <sys/ioctl.h>

namespace gdakis::umem {

static std::once_flag fd_ctrl_flag_;
static int fd_ctrl_ = -1;
static const char * udmabuf_dev = "/dev/udmabuf";

int cpu::get_control_fd() noexcept {
    std::call_once(fd_ctrl_flag_, [&] { fd_ctrl_ = ::open(udmabuf_dev, O_RDWR); });
    return fd_ctrl_;
}

int cpu::alloc_memfd(std::size_t & size) noexcept {
    int ctrl_fd = get_control_fd();
    if (ctrl_fd < 0) {
        return ctrl_fd;
    }

    int mem_fd = memfd_create(udma_mem_fd_name, MFD_ALLOW_SEALING);
    if (mem_fd < 0) {
        return mem_fd;
    }

    int err = fcntl(mem_fd, F_ADD_SEALS, F_SEAL_SHRINK);
    if (err < 0) {
        ::close(mem_fd);
        return err;
    }

    auto n_pages = (size + get_page_size() - 1) / get_page_size();
    auto map_size = n_pages * get_page_size();

    err = ftruncate(mem_fd, map_size);
    if (err < 0) {
        ::close(mem_fd);
        return err;
    }

    size = map_size;
    return mem_fd;
}

int cpu::get_dma_buf_from_memfd(int mem_fd, std::size_t map_size) noexcept {
    int ctrl_fd = get_control_fd();
    if (ctrl_fd < 0)
        return ctrl_fd;

    ::udmabuf_create create;
    create.memfd = mem_fd;
    create.flags = 0;
    create.offset = 0;
    create.size = map_size;

    return ::ioctl(ctrl_fd, UDMABUF_CREATE, &create);
}

}

#endif