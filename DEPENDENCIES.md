<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Dependencies

The swap API links its service contract, ADSC, SCADA core, communication, I/O,
result, allocation and utility targets through CMake. These are in-tree SCADA
components. Applications link `SCADA::swap`; they should not reconstruct the
dependency list manually. The API is header-based and uses compiled common
support libraries for allocation and UUIDs. Those support libraries can be built
as static or shared libraries.

This inventory covers direct library, test, benchmark, build and CI dependencies,
including dependencies obtained separately. "Bundled" refers to material
checked into this source distribution, not packages downloaded into a build
directory or installed in the development environment.

## Library and Runtime Dependencies

| Dependency | Version selection | Use and interaction | Bundled? |
| --- | --- | --- | --- |
| CUDA Toolkit | 13+ for the swap toolchain; selected by the build environment | CUDA compilation and headers; CUDA runtime/driver libraries linked according to the selected targets/configuration | No |
| NVIDIA driver | Selected for the deployed GPU/CUDA environment | Runtime driver interfaces used by CUDA allocation and DMA-buffer operations | No |
| CCCL | 3.2+ selected through `CCCL_DIR`/CMake discovery | libcu++, Thrust and CUDA C++ headers/templates included in client code | No |
| Linux UAPI, POSIX, threads and C/C++ runtime | Supplied by the selected OS and compiler | System headers and linked system/runtime libraries for sockets, shared memory, file descriptors and DMA buffers | No |
| Compatible SCADA server | Supplied separately by the server provider | Service protocol over local sockets, shared memory and transferred file descriptors | No |

## Optional Tests and Benchmarks

| Dependency | Version selection | Use and interaction | Bundled? |
| --- | --- | --- | --- |
| GoogleTest | 1.14.0; source archive pinned by SHA-256 in `cmake/FetchGoogleTest.cmake` | Compiled and statically linked into optional component tests | No; downloaded on demand |
| cxxopts | 3.1.1; source archive pinned by SHA-256 in `common/coaio/perf/CMakeLists.txt` | C++ header dependency of the optional I/O benchmark configuration | No; downloaded on demand |
| Matplotlib | Unpinned by this repository | Python import in the communication benchmark plotting scripts | No; installed separately |
| NumPy | Unpinned by this repository | Python import in the communication benchmark plotting scripts | No; installed separately |
| pandas | Unpinned by this repository | Python import in the communication benchmark plotting scripts | No; installed separately |

The Python plotting scripts are under `common/comm/perf/script/`. These packages
are not needed for the client library, framing check or CMake helper tests.
No plotting-package version constraints or compatibility guarantees are introduced
by this inventory.

## Build and CI Tools

| Dependency | Version selection | Use and interaction | Bundled? |
| --- | --- | --- | --- |
| CMake | 3.26+ | Build configuration, dependency retrieval, compilation and installation | No |
| CPM.cmake | 0.40.5 | CMake dependency-management helper; in-tree components use source directories, optional external packages use pinned archives | Yes, `cmake/CPM.cmake` |
| C++ compiler and CUDA compiler | Supplied by the selected toolchain; `nvcc` from CUDA Toolkit | Compile/link tools and their associated C/C++ standard libraries | No |
| Python | Python 3 from the development environment | Distribution and CMake helper checks; optional plotting scripts | No |
| Git | Supplied by the development/CI environment | Checkout and Git-based dependency-management operations | No |
| Bash and shell utilities | Supplied by the Linux development/CI environment | Run `ci/check-client.sh` and its temporary-file/copy/cleanup operations | No |
| `actions/checkout` | v4.2.2, commit `11bd71901bbe5b1630ceea73d27597364c9af683` | Separately executed GitHub Actions checkout step | No |

The CI build checks do not publish binary artifacts. A build/test environment
may contain additional tools or dependencies beyond this direct inventory.

## Bundled Third-Party Documents

The adapted Contributor Covenant 1.4 in `CODE_OF_CONDUCT.md` and the unmodified
Developer Certificate of Origin 1.1 in `CONTRIBUTING.md` retain their own terms.
They are contribution/governance documents, not linked library dependencies.

## Version and License Evidence

For each release inventory, record the actual selected package/toolchain versions,
their source identities and license files, and the enabled build/test/benchmark
options. Include the resolved transitive packages and any third-party libraries
bundled in installed wheels or other downloaded artifacts. The unpinned plotting
packages above are not a resolved environment SBOM; their upstream license
summaries do not describe every dependency or bundled component.

Client builds do not require UNVMe, Storage Lender, server execution kernels,
profiling libraries, or a running server. Selected memory operations can require
a GPU driver and device permissions even without a server request. Independent
utility components retain their existing requirements; not every utility needs
the complete swap toolchain.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for upstream license references
and bundled-material notices. This source release does not redistribute the
separately obtained dependencies. Distributing binaries or dependency bundles
requires reviewing and including the terms applicable to that distribution.
