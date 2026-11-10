<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# C++ User-level Memory Allocation

C++ allocator classes to create objects in host/device/system/dma-buf memory

## Requirements

CPU DMA buffer allocation relies on `udmabuf`. Most Linux distributions include this kernel module and load it at boot time. However, the control device (`/dev/udmabuf`) is only accessible to `root`.

GPU DMA buffer allocation relines on Open-RM (`nvidia-open` in most distributions). 