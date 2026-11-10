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

This release contains source only; separately installed dependencies are not bundled.

## Library and Runtime Dependencies

| Dependency | Version selection | Use and interaction | Bundled? |
| --- | --- | --- | --- |
| CUDA Toolkit headers and CUDA runtime (`cudart`) | 13+ | CUDA headers; shared `gdakis-umem` links `CUDA::cudart` | No |
| NVIDIA user-mode driver library (`libcuda.so`) | Installed driver version | CUDA-enabled allocation support links `CUDA::cuda_driver` | No |
| CCCL | 3.2+ selected through `CCCL_DIR`/CMake discovery | libcu++, Thrust and CUDA C++ headers/templates included in client code | No |
| Linux UAPI headers | Selected Linux development packages | Kernel interface declarations for file descriptors, DMA buffers and system operations | No |
| GNU C Library (`glibc`: `libc`, `libm`, pthread, `dl`, `rt` interfaces) | Selected OS packages | System headers and linked C/POSIX runtime | No |
| GNU C++ standard library (`libstdc++`) | Selected host toolchain | C++ headers/templates and linked standard-library runtime | No |
| GCC low-level runtime (`libgcc`, `libgcc_s`) | Selected host toolchain | Compiler support routines, linked statically or dynamically | No |
| Compatible SCADA server | Supplied separately by the server provider | Service protocol over local sockets, shared memory and transferred file descriptors | No |

The system-library entries describe the GNU/Linux toolchain. License terms and
exceptions are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
`BUILD_SHARED_LIBS` selects SCADA linkage; the CUDA compiler can independently
add static runtime/device-runtime code to consumers.

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
| GCC or Clang host compiler | Selected host toolchain | Separately executed C/C++ compiler | No |
| CUDA compiler (`nvcc`, device assembler/linker) | CUDA Toolkit 13+ | Separately executed CUDA toolchain | No |
| GNU binutils host tools | Selected host toolchain | Separately executed `as`, `ld`, `ar` and `ranlib` | No |
| GNU Make or Ninja | Selected CMake generator | Separately executed build backend | No |
| Python | Python 3 from the development environment | Distribution and CMake helper checks; optional plotting scripts | No |
| Git | Supplied by the development/CI environment | Checkout and Git-based dependency-management operations | No |
| Bash | Selected Linux environment | Interprets `ci/check-client.sh` | No |
| GNU coreutils | Selected Linux environment | Separately executed `dirname`, `mktemp`, `mkdir`, `cp` and `rm` | No |
| `nvidia-smi` (optional) | Installed driver version | Separately executed for automatic GPU architecture detection | No |
| `actions/checkout` | v4.2.2, commit `11bd71901bbe5b1630ceea73d27597364c9af683` | Separately executed GitHub Actions checkout step | No |

The CI build checks do not publish binary artifacts. A build/test environment
may contain additional tools or dependencies beyond this direct inventory.
Tool licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
Optional documentation builds require a user-supplied `SCADA_DOC_ENGINE`.

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
