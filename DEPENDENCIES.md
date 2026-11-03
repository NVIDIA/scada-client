<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Dependencies

The swap API links its service contract, ADSC, SCADA core, communication, I/O,
result, allocation and utility targets through CMake. Applications link
`SCADA::swap`; they should not reconstruct the dependency list manually.

| Dependency | Use | Bundled? |
| --- | --- | --- |
| CUDA Toolkit 13+ | CUDA compilation, runtime and driver interfaces | No |
| CCCL 3.2+ | libcu++, Thrust and CUDA C++ types | No |
| Linux UAPI, POSIX and threads | Sockets, shared memory, file descriptors, DMA buffers | No |
| CMake 3.26+ | Build and install configuration | No |
| CPM.cmake 0.40.5 | Source dependency configuration | Yes, MIT-licensed helper |
| GoogleTest 1.14.0 | Optional component tests | Downloaded on demand |
| cxxopts 3.1.1 | Optional I/O benchmarks | Downloaded on demand |
| Python 3 | Development checks | No |
| Compatible SCADA server | Execute swap requests | No |

Client builds do not require UNVMe, Storage Lender, server execution kernels,
profiling libraries, or a running server. Selected memory operations can require
a GPU driver and device permissions even without a server request. Independent
utility components retain their existing requirements; not every utility needs
the complete swap toolchain.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Distributing binaries also
requires checking the terms of dependencies included in that distribution.
