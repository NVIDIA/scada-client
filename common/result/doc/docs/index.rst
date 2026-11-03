.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0

The Result Library
==================

The ``result`` type is inspired by Rust's ``Result``, and you can see it as a preview of the upcoming ``std::expected`` `type <https://en.cppreference.com/w/cpp/utility/expected>`_ type in C++23.
The API in the current version is based on Rust's syntax, but we are planning to offer full compatibility with ``std::expected``.

The ``result`` type encapsulates either a success or a failure.
We implement two different versions, ``psa::result`` and ``psa::futures::result``.
The former offers a synchronous interface, while the latter enables asynchronous execution using C++ `coroutines <https://en.cppreference.com/w/cpp/language/coroutines>`_.

.. toctree::
    :hidden:
    :maxdepth: 1
    :caption: API Documents

    Namespaces <${repo_docs_api_path}/namespaces>
    Classes <${repo_docs_api_path}/classes>