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
| CUDA Toolkit and NVIDIA driver | NVIDIA terms accompanying the installed distribution |
| Linux headers, C/C++ runtime and system libraries | Component-specific terms and exceptions from the selected OS/compiler distribution, including Linux-syscall-note where applicable |
| CMake, build tool | BSD-3-Clause; see <https://cmake.org/licensing/> |
| Python, development/benchmark tool | PSF license and additional component notices; see <https://docs.python.org/3/license.html> |
| Git, development/CI tool | GPL-2.0; see <https://git-scm.com/about> |
| Bash and shell utilities, development/CI tools | Terms of the selected host packages; these tools are not bundled with this source distribution |
| `actions/checkout`, CI tool | MIT; see the [license at the pinned commit](https://github.com/actions/checkout/blob/11bd71901bbe5b1630ceea73d27597364c9af683/LICENSE) |

These dependencies are not bundled in the source snapshot. Distributions
including their code or binaries must carry the applicable notices and terms.
See [DEPENDENCIES.md](DEPENDENCIES.md) for versions, purposes and interactions.
The optional plotting packages are unpinned. Resolve their actual versions and
transitive dependencies before treating an environment inventory as complete;
consult the license files shipped with those selected versions. Upstream license
links are references, not dependency version constraints or a claim that all
components in a downloaded artifact have the same license.
