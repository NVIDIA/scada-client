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

The release described here contains source only. It does not distribute compiled
SCADA libraries, system/runtime libraries, a CUDA Toolkit, a driver, or a container.
The distinction between separately executed tools and libraries incorporated into
a consumer build is important even when neither is included in this source tree.

## Library and Runtime Dependencies

| Dependency | Version selection | Use and interaction | Bundled? |
| --- | --- | --- | --- |
| CUDA Toolkit headers and CUDA runtime (`cudart`) | 13+ for the swap toolchain; selected by the build environment | CUDA headers; `CUDA::cudart` is linked by the shared `gdakis-umem` target. CUDA consumers can also acquire compiler-selected runtime/device-runtime code; see linkage details below | No |
| NVIDIA user-mode driver library (`libcuda.so`) | Selected for the deployed GPU/CUDA environment | `CUDA::cuda_driver` is a link dependency of CUDA-enabled allocation support; the installed driver supplies the implementation at runtime | No |
| CCCL | 3.2+ selected through `CCCL_DIR`/CMake discovery | libcu++, Thrust and CUDA C++ headers/templates included in client code | No |
| Linux UAPI headers | Supplied by the selected Linux development packages | User-space kernel interface declarations for file descriptors, DMA buffers and other system operations; no SCADA kernel module is built by this source tree | No |
| GNU C Library (`glibc`, including `libc`, `libm`, POSIX threads, `dl` and `rt` interfaces) | Supplied by the selected GNU/Linux distribution | System headers and C runtime for allocation, sockets, shared memory and file descriptors. POSIX is an interface standard, not a separate library. On glibc 2.34+, pthread/dl functionality is integrated into libc; compatibility libraries may still appear | No |
| GNU C++ standard library (`libstdc++`) | Supplied by the GCC-compatible host toolchain | C++ headers/templates and standard-library runtime used by client support libraries and consuming applications | No |
| GCC low-level runtime (`libgcc`, including `libgcc_s`) | Supplied by the GCC-compatible host toolchain | Compiler-generated support routines; the compiler/linker selects static archive code or a shared-runtime dependency | No |
| Compatible SCADA server | Supplied separately by the server provider | Service protocol over local sockets, shared memory and transferred file descriptors | No |

The system-library rows describe the GNU/Linux and GCC reference toolchain, not
an instruction to bundle those libraries. They carry different licenses:
Linux UAPI files retain their per-file licenses and `Linux-syscall-note` where
applicable; glibc uses LGPL-2.1-or-later with additional upstream file-specific
notices; applicable libstdc++ and libgcc files use GPL-3.0-or-later WITH
GCC-exception-3.1. License and source references are in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). A different C library or C++
runtime must be inventoried under its own terms rather than assigned these
GNU-runtime licenses.

### Linkage and version evidence

`BUILD_SHARED_LIBS` selects static or shared SCADA support libraries; it does
not select the linkage of every external dependency. In
`common/umem/CMakeLists.txt`, CUDA-enabled builds expose `CUDA::cuda_driver` to
consumers. The shared allocation library additionally links `CUDA::cuda_driver`
and the shared `CUDA::cudart` target to resolve its own CUDA calls. A static SCADA
archive is not by itself a bundle of all of its link dependencies. Final
applications resolve those dependencies when they link.

The project does not override CMake's `CMAKE_CUDA_RUNTIME_LIBRARY` selection.
The selected CUDA compiler can therefore add its default runtime, including
static CUDA runtime/device-runtime code, to CUDA consumers. Similarly, C/C++
headers and templates can contribute code even when a runtime library is loaded
dynamically. Before distributing a binary, inspect its actual link commands,
archive contents and dynamic dependencies; do not infer its notice requirements
from `BUILD_SHARED_LIBS` alone.

The retained standalone static/shared qualification used Linux AArch64, GCC
13.3, CUDA 13.0.88, NVIDIA driver 580.178.04 and CCCL 3.2.0. These are reference
build versions, not additional minimum-version constraints. Exact OS package
revisions for glibc, libstdc++ and libgcc were not captured in that qualification
record. This source inventory identifies those components and their applicable
license families; it does not claim an artifact-specific binary inventory.

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
| GCC host compiler | Supplied by the selected GNU/Linux toolchain; reference qualification used 13.3 | Separately executed C/C++ compiler; GPL-3.0-or-later. Its runtime libraries are listed separately above | No |
| CUDA compiler (`nvcc`, device assembler and device linker) | CUDA Toolkit 13+; reference qualification used 13.0.88 | Separately executed CUDA toolchain under the CUDA Toolkit EULA; generated runtime code is accounted for separately above | No |
| Host assembler, linker and archive tools | Supplied by the selected toolchain; GNU binutils for the GNU reference toolchain | Separately executed `as`, `ld`, `ar` and `ranlib`; GNU binutils executables use GPL-3.0-or-later | No |
| CMake build backend | Selected by the CMake generator | GNU Make (GPL-3.0-or-later) or Ninja (Apache-2.0), executed separately to drive compilation | No |
| Python | Python 3 from the development environment | Distribution and CMake helper checks; optional plotting scripts | No |
| Git | Supplied by the development/CI environment | Checkout and Git-based dependency-management operations | No |
| Bash | Supplied by the Linux development/CI environment | Interprets `ci/check-client.sh`; GNU Bash 4+ uses GPL-3.0-or-later. Shell builtins run within Bash | No |
| GNU coreutils | Supplied by the Linux development/CI environment | `dirname`, `mktemp`, `mkdir`, `cp` and `rm` are separately executed by the check script; GPL-3.0-or-later | No |
| `nvidia-smi` (optional architecture detection) | Installed NVIDIA driver utilities | Separately executed by `cmake/ScadaCudaArchitectures.cmake` when architectures are not specified; NVIDIA driver terms | No |
| `actions/checkout` | v4.2.2, commit `11bd71901bbe5b1630ceea73d27597364c9af683` | Separately executed GitHub Actions checkout step | No |

The CI build checks do not publish binary artifacts. A build/test environment
may contain additional tools or dependencies beyond this direct inventory.
The compiler and build tools are not loaded as SCADA libraries. This distinction
does not exclude their generated runtime code or linked libraries from review.
If Clang is selected instead of GCC, record its Apache-2.0 WITH LLVM-exception
terms and the actual runtime libraries it uses; Clang does not imply libc++.

Optional documentation builds accept a separately supplied `SCADA_DOC_ENGINE`
source directory. No documentation engine is fetched or bundled by this tree;
its source identity, dependencies and license must be recorded if that optional
configuration is used to produce a distribution.

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
