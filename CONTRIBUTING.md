<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Contributing

We welcome focused bug reports, documentation corrections and code contributions.
Follow [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Report vulnerabilities privately
under [SECURITY.md](SECURITY.md).

Discuss changes to public APIs, request framing, shared-memory layout,
file-descriptor ownership and service handshakes with maintainers before
implementation. Client and server must agree on these contracts. Keep generic
utilities independent of services and storage backends.

Contributions use the applicable file's license, normally Apache-2.0. Every
commit must have a Developer Certificate of Origin 1.1 sign-off:

```sh
git commit --signoff -m "Describe the change"
```

This certifies the [Developer Certificate of Origin](https://developercertificate.org/),
including your right to submit the contribution.

For NVIDIA-owned files, use this header, preserving the existing creation year
and extending the range for substantial modifications:

```cpp
/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
```

Use appropriate comment syntax for other formats. Files that cannot contain
comments use adjacent `.license` files. Preserve third-party copyrights,
license terms and attribution.

Use four-space C++ indentation and the checked-in formatting configurations.
Run the checks in [README.md](README.md), add targeted tests for behavior changes,
and explain checks that could not run. A client compile check should not execute
a server or storage workload. Maintainers review correctness, compatibility,
security, dependencies and licensing; submission does not guarantee acceptance.
