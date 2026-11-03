.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0

C++ Coroutine Asynchronous I/O
******************************

``coaio`` is a C++20 [#f1]_ header-only library that aims to simplify asynchronous I/O programming using C++ coroutines.

I/O Schedulers
==============

``coaio`` manages asynchronous I/O using schedulers.
Most (if not all) I/O abstractions require an scheduler upon construction.
All schedulers provide a common API that ``coaio`` abstractions rely upon to request asynchronous I/O.
Currently, we only offer an scheduler implementation for Linux systems based on the `epoll <https://en.wikipedia.org/wiki/Epoll>`_.

An I/O scheduler handles I/O operations from the abstractions using them.
Applications can create any number of I/O schedulers and bound operations to different schedulers.
This is useful, for instance, when developing asynchronous forking servers.
For each incoming connection the application creates a new I/O scheduler that the forked process inherits.
Within the child process the code invokes a coroutine to handle the client, and upon return synchronizes with the I/O scheduler.
This synchronization becomes local because it only waits for the I/O abstractions (e.g., sockets) the process creates/inherits.

I/O operations
==============

All I/O operations in ``coaio`` return a ``psa::futures::result<>`` type.
This type supports two operations to materialize the I/O operation to obtain a ``psa::result<>`` value with the result of the I/O operation:

 * ``.sync()`` : Calling this method blocks the calling threads until the operation completes
 * ``co_await`` : This blocks the calling coroutine until the I/O operation completes.


.. toctree::
    :hidden:
    :maxdepth: 1
    :caption: API Documents

    Namespaces <${repo_docs_api_path}/namespaces>
    Classes <${repo_docs_api_path}/classes>


.. rubric:: Footnotes
.. [#f1] ``coaio`` is also compatible with C++17, but in this case all I/O operations become synchronous.