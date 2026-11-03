<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Application Defined Software Cache Core

This project implements the core of the application defined softare cache (ADSC). for CUDA/C++ applications.
ADSC allows an application to leverage fast (but lower capacity) near-memory for data reuse and slower far-memory for capacity.
The goal of the library is to provide key core-mechanisms and allow the application to customize the cache, through application defined policies, for optimized performance.
Currently the project is exploring what the key core-mechanisms are, how they should be optimized,  and what the right APIs are such that the final goal can be realized.
