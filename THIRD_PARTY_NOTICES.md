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

## Separately obtained dependencies

| Software | License information |
| --- | --- |
| CCCL | Apache-2.0 with component-specific exceptions, including LLVM-exception for libcu++; see <https://github.com/NVIDIA/cccl/blob/main/LICENSE> |
| GoogleTest | BSD-3-Clause; see <https://github.com/google/googletest/blob/v1.14.0/LICENSE> |
| cxxopts, for optional I/O benchmarks | MIT; see <https://github.com/jarro2783/cxxopts/blob/v3.1.1/LICENSE> |
| CUDA Toolkit and NVIDIA driver | NVIDIA terms accompanying the installed distribution |
| Linux headers and system libraries | Component-specific terms, including Linux-syscall-note where applicable |

These dependencies are not bundled in the source snapshot. Distributions
including their code or binaries must carry the applicable notices and terms.
