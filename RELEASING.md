<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Releasing

1. Select a reviewed commit. Update root `VERSION` and the changelog; review
   component versions separately rather than assuming they match.
2. Run distribution and CMake helper checks, then static/shared source builds,
   framing checks, installs and installed consumers with `ci/check-client.sh`.
3. Validate affected components and downstream consumers from that commit.
   Identify server versions for any runtime compatibility results.
4. Review the source archive, copyrights and third-party notices. Include real
   documentation images, and exclude build outputs and unrelated source.
5. After maintainer approval, tag the commit and publish source artifacts with
   checksums and release notes. Tagging and artifact publication are separate actions.

The initial distribution is source-only. Combined client/server Debian packages
and server implementations are separate. Historical per-component package
recipes are development inputs, not qualified release outputs.
