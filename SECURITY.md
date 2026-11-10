<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Security policy

Do not report vulnerabilities through public issues or pull requests.
Use the [NVIDIA submission form](https://www.nvidia.com/object/submit-security-vulnerability.html)
or email <psirt@nvidia.com>. Include the version or commit, reproduction, impact
and configuration with secrets removed. See
[NVIDIA PSIRT policies](https://www.nvidia.com/en-us/security/psirt-policies/).

The client uses local Unix sockets, file-descriptor transfer, shared memory and
GPU-visible request buffers. Socket access grants the operations implemented by
that service. Administrators must restrict socket directories, device permissions
and admission to trusted applications. The library does not establish isolation
between hostile tenants sharing those resources.

The server provider owns storage provisioning, driver permissions, device
lifecycle and server authorization. A client build or framing check does not
validate deployment safety or server compatibility. Reports may concern API
lifetimes, IPC, file descriptors, allocation or server interactions. Identify
affected versions explicitly; no long-term support window is promised.
