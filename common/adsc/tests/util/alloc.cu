/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <bear/util/alloc/cuda_allocator.h>

int main() {
    using type = int;
    const unsigned n_elems = 1024;

    adsc::util::alloc::cuda_allocator<type> allocator();

    auto ptr = allocator.allocate(n_elems);

    allocator.deallocate(ptr);
}
