<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# SCADA client

SCADA (SCaled Accelerated Data Access) provides C++ and CUDA APIs for accessing
storage through a compatible local server. This repository contains client APIs,
shared service contracts and their underlying utilities, for embedding in
applications such as cuGraph GNN.

**Building the client does not require a server. Using the swap API does.**
A server implementation is not included. A compatible server must implement the
existing handshake, shared-memory, file-descriptor and CPU/CUDA request contracts.
The contract headers are not a complete SDK or interoperability specification.

## Layout

| Directory | Purpose |
| --- | --- |
| `services/scada-swap/api/` | Store, slab, portal, pointer and reference application APIs |
| `services/scada-swap/contract/` | Operation types, status, identities and protocol constants |
| `common/adsc/` | Application-defined software cache |
| `common/coaio/` | Coroutine I/O, synchronous sockets and asynchronous scheduling |
| `common/comm/` | IPC, shared memory, client autobatching and buffer pools |
| `common/result/` | Result and future helpers |
| `common/scada-core/` | Common SCADA types and UUID support |
| `common/umem/` | Host, CUDA and DMA-buffer memory allocation |
| `common/utils/` | Atomics, synchronization and low-level utilities |
| `cmake/` | Build helpers and package configuration templates |
| `tests/` | Contract checks and source/installed API compile checks |

Common components retain independent CMake entry points and test options, and
may also be used by servers. New services add `services/<service>/api` and
`contract` directories and reuse the common targets.

## Requirements

- Linux, a C++17 compiler and CMake 3.26 or newer.
- CUDA Toolkit 13 or newer, including `nvcc` and driver link libraries.
- CCCL 3.2 or newer, discoverable through CMake.
- Python 3 for development checks; GoogleTest is downloaded for component tests.

Runtime requirements depend on the APIs and allocators used. GPU access needs a
compatible GPU and driver; DMA-buffer allocation can require the NVIDIA open
driver or access to `/dev/udmabuf`. Coordinate permissions, GPU and storage setup
with the server provider. See [SECURITY.md](SECURITY.md).

## Build and install

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CUDA_ARCHITECTURES=90 -DCCCL_DIR=/path/to/cccl/lib/cmake/cccl
cmake --build build --parallel
cmake --install build --prefix /path/to/scada-client-install
```

Choose the CUDA architectures for your GPUs; `90` is a Hopper example. Set
`BUILD_SHARED_LIBS=ON` for shared support libraries. The swap API is header-based
and links compiled common dependencies, including allocation and UUID support.

## Consume from source

Add this repository as a submodule or another source dependency:

```cmake
cmake_minimum_required(VERSION 3.26)
project(my_application LANGUAGES CXX CUDA)
add_subdirectory(external/scada-client)
add_executable(my_application main.cu)
target_link_libraries(my_application PRIVATE SCADA::swap)
```

CPM can also add this repository at a reviewed commit. Existing include spellings,
such as `<scada/swap/store>`, are retained.

## Consume an installation

```cmake
find_package(scada-swap CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE SCADA::swap)
```

Pass `-DCMAKE_PREFIX_PATH=/path/to/scada-client-install/cmake` and the same
CCCL/CUDA configuration. This repository provides source and CMake installation
targets. Combined client/server Debian packages are separate. Historical
per-component package recipes are development inputs, not qualified release packages.

## Development

```sh
python3 ci/check_distribution.py
python3 -m unittest discover -s cmake/tests -p 'test_*.py'
CCCL_DIR=/path/to/cccl/lib/cmake/cccl CMAKE_CUDA_ARCHITECTURES=90 bash ci/check-client.sh
```

The last command builds and installs static and shared configurations, builds
installed consumers, and runs the framing check. The swap API probe is compiled
and linked without being executed or contacting a server. See
[CONTRIBUTING.md](CONTRIBUTING.md) and [DEPENDENCIES.md](DEPENDENCIES.md).

## License and community

NVIDIA-owned files are licensed under [Apache 2.0](LICENSE). Copyright remains
with NVIDIA CORPORATION & AFFILIATES. Third-party files retain their terms; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
The adapted [Contributor Covenant](CODE_OF_CONDUCT.md) uses CC-BY-4.0, whose
complete text is in [LICENSES/CC-BY-4.0.txt](LICENSES/CC-BY-4.0.txt).
The vendored [CPM.cmake](cmake/CPM.cmake) retains its upstream MIT license.

Use [GitHub Issues](https://github.com/NVIDIA/scada-client/issues) for
non-security questions and reports. See [SUPPORT.md](SUPPORT.md),
[GOVERNANCE.md](GOVERNANCE.md) and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
