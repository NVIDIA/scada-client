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

Contributions use the applicable file's license, normally Apache-2.0.

## Signing Off Your Work

We require that all contributors "sign-off" on their commits. This certifies
that the contribution is your original work, or you have rights to submit it
under the same license, or a compatible license.

Any contribution which contains commits that are not Signed-Off will not be accepted.

To sign off on a commit you simply use the `--signoff` (or `-s`) option when
committing your changes:

```sh
git commit -s -m "Add cool feature."
```

This will append the following to your commit message:

```text
Signed-off-by: Your Name <your@email.com>
```

Full text of the [Developer Certificate of Origin](https://developercertificate.org/):

```text
Developer Certificate of Origin
Version 1.1

Copyright (C) 2004, 2006 The Linux Foundation and its contributors.

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.


Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

## Copyright and Development Checks

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
