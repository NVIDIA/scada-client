.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0

GDAKIS Utilities Library
========================

This library provides commonly used data structures and algorithms.
Most GDAKIS libraries depend on this library and use some of its functionality.

Atomic
------

The ``<gdakis/atomic>`` header file implements a set of wrappers around ``std::atomic`` and ``cuda::atomic`` to provide a unified API for
atomic operations, whether CUDA is enabled or not.
GDAKIS atomics follow the same API as CUDA atomics, and are available even if CUDA header files and compilers are not present in the system.


Barrier and Multi-Process Barrier
---------------------------------

The ``<gdakis/barrier>`` header file provides a barrier implementation that is API compatible with C++20 ``std::barrier``.
This header file falls back to ``std::barrier`` whenever C++20 (or higher) is enabled, and uses its custom implementation otherwise.
The header files allows GDAKIS code to use the same barrier abstraction, and thus the same code, independently of the C++ version
enabled during compilation.

The ``<gdakis/mpbar>`` header files provides a barrier that works across user processes.
This barrier is not API-compatible with ``std::barrier``, and its main usage is to coordinate forked child processes on GDAKIS tests.


Bitmaps
-------

The ``<gdakis/bitmap>`` header file provides different `bitmap implementations <bitmap>`.

Seqlock
-------

A seqlock is a synchronization mechanism that incurs in very low read-lock overhead, at the cost of requiring readers to re-execute
their critical section if a writer acquires the lock while the read critical section was being executed.



.. toctree::
    :hidden:
    :maxdepth: 1
    :caption: Technical Documentation

    GDAKIS Utilities Library <self>
    Bitmaps <bitmap>


.. toctree::
    :hidden:
    :maxdepth: 1
    :caption: API Documents

    Namespaces <${repo_docs_api_path}/namespaces>
    Classes <${repo_docs_api_path}/classes>