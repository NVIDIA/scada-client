.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0

Multi-Target Programming Patterns
=================================

The GDAKIS communication library uses some common programming patterns across its API.
The goal is to offer an homogeneous API to simplify its usage.

Many GDAKIS abstractions **must** be available to different compute domains (e.g., CPU, GPU, DPU).
To minimize code duplication, when possible, we aim to provide a single implementation for each abstraction.
For instance, imagine we have a type ``foo`` defined as follows:

.. code-block:: cpp
    :caption: Example of type supporting CPU and GPU execution
    :name: foo

    class foo {
    private:
        unsigned internal_state_;
    public:
        __host__ __device__
        foo(unsigned initial_value) noexcept;

        __host__ __device__
        unsigned get() const noexcept;
      
        __host__ __device__
        void     set(const unsigned) noexcept;
    };

 
Compiler Compatibility
----------------------
 
All methods in the type are decorated with ``__host__ __device__`` so they are available both on CPU and GPU.
This presents a problem since we cannot guarantee that a user will use NVCC to compile this code, and other compilers would fail to identify those keywords.
We use a the :cpp:any:`__comm_cuda_compat` header file to conditionally define those keywords based on the target compiler.


Persistance
-----------

The declaration in :numref:`foo` allows us to construct objects of type ``foo`` in both CPU and GPU.
Furthermore, due to the implicit copy constructor is also possible to construct an object of type ``foo`` on the CPU,
pass it as by-value parameter to a GPU kernel, and use it within the GPU kernel.
However, passing objects by-value to a kernel implies that each single thread in the GPU kernel gets its own copy of the object.

To create a GPU-accessible object of type ``foo`` in the CPU that is shared across all GPU threads, we need to build it in GPU memory.
Both, the CUDA Runtime and CUDA Driver API provide means to accomplish this.
However, this is a tedious process prone to programming errors, such as memory leaks.
Instead, whenever we need objects to be accessible on multiple compute domains, we declare a inner ``handle`` class to manage the lifetime of the objects.
Since we require the object to be constructible on multiple compute domains, this ``handle`` type is typically a **template** class that takes as a parameter an **allocator**.

For instance, :numref:`foo-handle` shows the implementation of a ``handle`` for ``foo``. 

.. code-block:: cpp
    :caption: Example of type supporting CPU and GPU execution using a ``handle``
    :name: foo-handle

    class foo {
    public:
        template<typename _Allocator>
        class handle {
        protected:
            gdakis::umem::ptr<foo>  foo_;

            handle(unsigned initial_value) noexcept;
        public:

            psa::result<handle, int>
            build(unsigned initial_value) noexcept;

            constexpr foo *
            get() const noexcept;
        };
    private:
        unsigned internal_state_;
    public:
        __host__ __device__
        foo(unsigned initial_value) noexcept;

        __host__ __device__
        unsigned get() const noexcept;
      
        __host__ __device__
        void     set(const unsigned) noexcept;
    };

When using ``handle`` types, the code assumes the existence of a ``build`` static method that returns a ``psa::result`` with the handle,
if the operation succeeds, or an error code.
Once built, a ``handle`` object always implements a ``get()`` method that returns an object, or a pointer to an object of the outer class.
As a general rule, ``.get()`` returns an object if the size of the outer class is smaller than a pointer type, and returns a pointer to an object
of the outer class otherwise.

A key aspect of ``handle`` types is that they own the resources of the outer class, and, thus, the ``.build`` method **must** allocate any resource
required by the outer class prior returning a ``handle`` object.
Another consequence of this policy is that the lifetime of those resources is bound to the lifetime of the ``handle`` object.

.. warning::
    Avoid storing the return value of the ``.get()`` method in local variables.
    The pointer returned by a ``handle`` object becomes invalid once the handle goes out of scope, unless explicitly moved.
    The compiler is pretty good at realizing that calls to ``.get()`` always return the same value, hosting multiple calls into a single one.