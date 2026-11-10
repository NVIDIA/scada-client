<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# CI

Source checks run on GitHub-hosted Ubuntu runners for pull requests and pushes
to `main`. They check copyright/license coverage, public distribution content
and CMake helper behavior without CUDA or a server. Components retain their own
CMake test switches for targeted development.

The optional CUDA job requires a Linux runner labeled `scada-client-cuda`,
CMake 3.26+, CUDA 13+, and CCCL 3.2+. Set `CCCL_DIR` in the runner environment,
optionally set `CMAKE_CUDA_ARCHITECTURES` (default `90`), and enable the repository
variable `SCADA_CLIENT_CUDA_CI_ENABLED=true`. Maintainers start it through
`workflow_dispatch`. The runner compiles and links static/shared source and
installed consumers, and runs only the CPU framing check. It does not run the
application probe, a server, or storage operations.

The job is manual until runner provisioning and trusted contribution execution
are configured. Use `ci/check-client.sh` locally for the same build checks.
