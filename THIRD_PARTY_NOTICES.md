<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Third-party notices

NVIDIA-owned files are Apache-2.0 licensed; see [LICENSE](LICENSE). The following
material retains its own copyright and terms.

## CPM.cmake

Location: [cmake/CPM.cmake](cmake/CPM.cmake), version 0.40.5.
Copyright (c) 2019-2023 Lars Melchior and contributors. MIT License.
The upstream file and complete license are preserved. An adjacent `.license`
file records SPDX metadata without changing the upstream file.
Source: <https://github.com/cpm-cmake/CPM.cmake>.

## Contributor Covenant 1.4

[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) adapts the Contributor Covenant by
Coraline Ada Ehmke and contributors under CC-BY-4.0. Its attribution and license
links are retained. Those terms apply to that document.
Source: <https://www.contributor-covenant.org/version/1/4/code-of-conduct.html>.

## Developer Certificate of Origin 1.1

Location: [CONTRIBUTING.md](CONTRIBUTING.md), which reproduces the full certificate
without modification.

```text
Copyright (C) 2004, 2006 The Linux Foundation and its contributors.

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.
```

These copying terms apply to the certificate text. The DCO is a contribution
attestation and does not change the license of SCADA client code.
Source: <https://developercertificate.org/>.

## Separately obtained dependencies

| Software | License information |
| --- | --- |
| CCCL | Apache-2.0 with component-specific exceptions, including LLVM-exception for libcu++; see the [3.2.0 license](https://github.com/NVIDIA/cccl/blob/v3.2.0/LICENSE) and notices for the selected version |
| GoogleTest | BSD-3-Clause; see <https://github.com/google/googletest/blob/v1.14.0/LICENSE> |
| cxxopts, for optional I/O benchmarks | MIT; see <https://github.com/jarro2783/cxxopts/blob/v3.1.1/LICENSE> |
| Matplotlib, for optional benchmark plots | Matplotlib license (PSF-based), with additional component notices; see <https://matplotlib.org/stable/project/license.html> |
| NumPy, for optional benchmark plots | BSD-3-Clause, with additional bundled-component licenses; see <https://numpy.org/doc/stable/license.html> |
| pandas, for optional benchmark plots | BSD-3-Clause, with additional bundled-component licenses; see <https://github.com/pandas-dev/pandas/blob/main/LICENSE> and the selected distribution's license files |
| CUDA Toolkit headers, compiler tools and CUDA runtime (`cudart`/device runtime) | NVIDIA CUDA Toolkit EULA, not Apache-2.0; see the [CUDA 13.0 agreement and component notices](https://docs.nvidia.com/cuda/archive/13.0.0/eula/index.html) and terms accompanying the selected Toolkit |
| NVIDIA user-mode driver (`libcuda.so`) and optional `nvidia-smi` utility | NVIDIA driver license accompanying the installed distribution; these are separate from the Toolkit runtime and from the open GPU kernel-module source. See [NVIDIA driver licensing](https://www.nvidia.com/en-us/drivers/nvidia-license/) |
| Linux UAPI headers | Per-file upstream terms; commonly GPL-2.0-only WITH Linux-syscall-note, with other licenses where declared. Preserve the selected headers' license identifiers and exceptions; see [Linux kernel licensing rules](https://www.kernel.org/doc/html/latest/process/license-rules.html) |
| GNU C Library (`glibc`: C runtime, math and POSIX interfaces) | LGPL-2.1-or-later for the library, with additional file-specific upstream notices. See [glibc COPYING.LIB](https://sourceware.org/git/?p=glibc.git;a=blob;f=COPYING.LIB;hb=glibc-2.39) and [additional glibc notices](https://sourceware.org/git/?p=glibc.git;a=blob;f=LICENSES;hb=glibc-2.39); retain the selected OS package's complete copyright/license record for any binary redistribution |
| GNU C++ standard library (`libstdc++`) | GPL-3.0-or-later WITH GCC-exception-3.1 for applicable library files; preserve other per-file notices. See the [libstdc++ license](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html) and [GCC Runtime Library Exception 3.1](https://www.gnu.org/licenses/gcc-exception-3.1.html) |
| GCC low-level runtime (`libgcc`, including `libgcc_s`) | GPL-3.0-or-later WITH GCC-exception-3.1 for applicable runtime files; see the [GCC runtime exception coverage and conditions](https://www.gnu.org/licenses/gcc-exception-3.1-faq.html). This is distinct from executing the GCC compiler |
| CMake, build tool | BSD-3-Clause; see <https://cmake.org/licensing/> |
| GCC, separately executed compiler | GPL-3.0-or-later; see [GCC copying terms](https://gcc.gnu.org/onlinedocs/gcc/Copying.html). Runtime exceptions apply to designated runtime files, not to the compiler executable as a whole |
| Clang, if selected as the host compiler | Apache-2.0 WITH LLVM-exception; see [LLVM licensing](https://llvm.org/docs/DeveloperPolicy.html#license). Record the actual C/C++ runtime separately |
| GNU binutils, separately executed host tools | GPL-3.0-or-later for the assembler/linker/archive executables; see [GNU binutils copying terms](https://sourceware.org/binutils/docs/binutils/Copying.html) and selected tool notices |
| GNU Make, when using a Makefiles generator | GPL-3.0-or-later; see [GNU Make copying terms](https://www.gnu.org/software/make/manual/html_node/Copying.html) |
| Ninja, when using a Ninja generator | Apache-2.0; see [Ninja COPYING](https://github.com/ninja-build/ninja/blob/master/COPYING) |
| Python, development/benchmark tool | PSF license and additional component notices; see <https://docs.python.org/3/license.html> |
| Git, development/CI tool | GPL-2.0; see <https://git-scm.com/about> |
| GNU Bash 4+, development/CI interpreter | GPL-3.0-or-later; see [Bash licensing](https://www.gnu.org/software/bash/). Older or alternative host interpreters require their own version-specific record |
| GNU coreutils, development/CI executables | GPL-3.0-or-later; see [coreutils COPYING](https://github.com/coreutils/coreutils/blob/master/COPYING). Used commands are listed in DEPENDENCIES.md |
| `actions/checkout`, CI tool | MIT; see the [license at the pinned commit](https://github.com/actions/checkout/blob/11bd71901bbe5b1630ceea73d27597364c9af683/LICENSE) |

These separately obtained dependencies are not bundled in the source snapshot.
The initial release is source-only and does not distribute compiled SCADA
libraries, a Toolkit, a driver, system/runtime libraries or a container.
Header/template inclusion and static runtime linkage can incorporate upstream
code into a consumer's output even when no dependency source is vendored here.
Binary distributions must be assessed against their actual contents and linkage
and carry the applicable notices and terms; this table does not qualify a
binary redistribution or replace a selected package's complete license record.
See [DEPENDENCIES.md](DEPENDENCIES.md) for versions, purposes and interactions.
The optional plotting packages are unpinned. Resolve their actual versions and
transitive dependencies before treating an environment inventory as complete;
consult the license files shipped with those selected versions. Upstream license
links are references, not dependency version constraints or a claim that all
components in a downloaded artifact have the same license.
