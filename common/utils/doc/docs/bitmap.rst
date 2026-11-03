.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0

GDAKIS Bitmaps
==============

GDAKIS uses bitmaps in many of its components.
This library provides different bitmap implementations that fit the needs of GDAKIS.

Embedded and Static Bitmaps
---------------------------

An embedded bitmap is an arbitrary length bitmap data type suitable to be embedded within other data types.
As such, the data type using the embedded bitmap is responsible to allocate the necessary memory to store the contents of the bitmap.

.. code-block:: cpp
    :caption: Embedding a bitmap into another data structure
    :name: bitmap_embedding

    struct foo_type {
        unsigned                        attribute_;
        /* Embedded bitmap MUST be the last field in the structure */
        gdakis::embedded_bitmap<>::type bitmap_;
    };

The code in :numref:`bitmap_embedding` shows how to embed a bitmap in another data structure.
The field holding the bitmap **must** be the last field in the data type.
This restriction comes from the definition of the bitmap type, ``_Tp[]``.

.. code-block:: cpp
    :caption: Allocation of a data type with an embedded bitmap
    :name: bitmap_allocation

    foo_type *
    allocate_foo_type(std::size_t number_of_bits) noexcept {
        void * ret = ::malloc(sizeof(foo_type) + gdakis::embedded_bitmap<>::memory_size(number_of_bits));
        if(ret != nullptr) { new (ret) foo_type(); }
        return ret;
    }

Without further action, the bitmap in the data structure has a zero size and, thus, cannot hold any bit.
The usage of ``embedded_bitmap<>`` requires custom allocation of the embedding data type, as shown in :numref:`bitmap_allocation`.
The code allocates enough memory to hold all the fields in the type (e.g., ``sizeof(foo_type)``) plus the amount of memory required to hold
the bitmap (e.g., ``gdakis::embedded_bitmap<>::memory_size(number_of_bits)``).
Because this custom allocation requires placement ``new``, a more convenient pattern is to hold the data type within a C++ smart pointer,
such as ``std::unique_ptr`` or ``std::shared_ptr``, as shown in :numref:`bitmap_smart`.

.. code-block:: cpp
    :caption: Allocation of a data type with an embedded bitmap using smart pointers
    :name: bitmap_smart

    using foo_ptr = std::unique_ptr<foo_type, std::function<void(foo_type *)>>;

    foo_ptr
    allocate_foo_type(std::size_t number_of_bits) noexcept {
        void * ret = ::malloc(sizeof(foo_type) + gdakis::embedded_bitmap<>::memory_size(number_of_bits));
        if(ret != nullptr) { new (ret) foo_type(); }
        return foo_ptr(ret, [](foo_ptr * ptr) {
            if(ptr == nullptr) return;
            ptr->~foo_type();
            ::free(ptr);
        });
    }

A static bitmap is a free standing embedded bitmap.
The static bitmap type contains a single field, which is a pointer to an embedded bitmap.
The constructor allocates this field based on the requested number of bits, and the remaining API methods are simple wrappers to call
the embedded bitmap logic.


Atomic Bitmap
-------------

The atomic bitmap type provides a short-sized bitmap (typically 32 or 64 bits) that supports a variety of atomic operations
Upon construction, this bitmap is initially clear, and it offers standard APIs to set and clear bits.
Additionally, the atomic bitmap also provide warp-collective APIs, that take the mask of active threads within the warp, to
deliver higher performance on CUDA code.

GDAKIS code often uses an atomic bitmap to track available resources.
For this purpose, the atomic bitmap implements an ``alloc`` API to set any clear bit and return its position.
