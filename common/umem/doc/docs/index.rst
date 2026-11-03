.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0

GDAKIS Memory Allocators
========================

The GDAKIS memory allocators are types that define what memory to allocate.
Other code heavily relies on these allocator types as template parameters to determine whether an object lives in CPU or CUDA memory.
We might add other allocator type in the future, e.g., to allocate memory on the DPU.

This project includes user-level memory allocation (``gdakis::umem``) and user-level DMA memory allocation (``udma``).
The former uses standard C and CUDA APIs and should work on all systems.
The latter depends on the correct installation of ``udmabuf`` for CPU DMA memory allocations, and OpenRM for CUDA DMA memory allocations.

DMA Memory allocations
----------------------

We abstract out DMA memory allocations using Linux ``dma-buf`` objects.
To user-level code, a ``dma-buf`` object is a file descriptor.
As such, user-level code can give access to a ``dma-buf`` to any other user-level process passing the file descriptor through a UNIX domain socket.

The ``udma`` types also map DMA buffers into the address space of the compute unit where the memory resides.
As such, host code can dereference CPU DMA memory allocations, and CUDA code can dereference CUDA DMA memory allocations.


.. toctree::
    :hidden:
    :maxdepth: 1
    :caption: Technical Documentation

    GDAKIS Memory Allocation <self>
 

.. toctree::
    :hidden:
    :maxdepth: 1
    :caption: API Documents

    Namespaces <${repo_docs_api_path}/namespaces>
    Classes <${repo_docs_api_path}/classes>