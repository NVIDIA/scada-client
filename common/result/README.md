<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Freestanding Implementation of a Result Type

This is a freestanding implementation of a C++ `result`type similar to the Rust `Result` type.

The `result` type encapsulates either a success (`ok`) or a failure (`err`).
This type provides a `.is_ok()` method to check for success, and a `.is_err()` to check for a failure.
On success, the `.ok()` method returns the success value, while the `.err()` method returns error condition.
Calling `.ok()` on a failure value, or `.err()` on a success value, throws as `std::bad_variant_access` exception.


* [Documentation source](doc/docs/index.rst)
