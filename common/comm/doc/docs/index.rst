.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0

GPU Direct Access Kernel Initialed Storage -- Communication
============================================================

This library implements the basic communication mechanisms between GDAKIS clients and servers.

GDAKIS Communication Abstractions
---------------------------------

GDAKIS is built arounds three main abstractions: memory buffers, I/O memory pools, and autobatch.
Memory buffers are regions of memory accessible from the server.
I/O memory pools are collections of block/status pairs accesible from the server and I/O devices.
Autobatch is a collection of buffers containing multiple work request entries that are made available to the server in a single batch.

GDAKIS abstractions provide a unified API which is service (e.g., swap, key-value), device (e.g., CPU, CUDA) and transport (e.g., local, IB), independent.
However, the data types for each specific service, device, and transport are different, since those are template parameters of the generic types.

GDAKIS defines a common protocol to register/deregister these abstractions.
Although the operations and some information in this protocol is common for all transports, many fields are transport-dependent.


Memory Buffers
^^^^^^^^^^^^^^

Memory buffers are client memory areas that are accessible (whether directly or through RDMA) to the server code, but not necessarily to I/O devices.
The intended usage of memory buffers are to communicate input parameters and output return values between clients and servers.
For instance, consider a hypothetical service that offers remote filesystem access, and a client application that requires the list of files present on a given path.
Work request entries in GDAKIS are fixed sized, and typically limited to few bytes.
As such, the client cannot pass a variable-sized path value directly through a work request entry.
Instead, the client would pass the memory buffer identifier where the requested path is available to the server, as well as the memory buffer identifier where it expects the server to make the list of files available.

Each individual transport in GDAKIS provides its own memory buffer implementation.
However, all implementations share a common API to register the memory buffer with the server, obtain the assigned identifier, and access the underlying memory.

I/O Buffer Pools
^^^^^^^^^^^^^^^^^

I/O buffer pools are collections of memory block / status pairs that reside in the client's memory.
Each pair in the pool is intended as source/destination of a data transfer.
Both, memory block and status have user-defined size and type.
All memory blocks are accessible from both server, and memory blocks are also addressable from I/O devices.

Autobatch
^^^^^^^^^

Autobatch is a collection of buffers composed by multiple work request entries.
The number of buffers in the autobatch is user-defined, but must be a power of two.
The client code allocates work request entries in each buffer are contiguously, and marks them as valid once it updates its contents.
Autobatch submits a single batch consisting of all contiguous valid work entries to the server.

The format of work request entries only depends on the server.
However, the underlying mechanism to execute the submission of a batch is transport-dependent.


GDAKIS Communication Protocol
-----------------------------

GDAKIS communication is always client initiated and consists of a simple request - response exchange.
The client applications sends a request, and, upon completion of the request, the server replies with an acknowledge.


GDAKIS Requests
^^^^^^^^^^^^^^^

Request messages have a common header that fully defines the type of operation the client is requesting.
After the common header, the request includes operation-specific fields, which are common for all transports, and transport-specific fields,
which are specific for a given operation-transport combination.

+---------------------------------+--------------------+-------------------+
| Header                          |  Operation Payload | Transport Payload |
+------------+--------+-----------+--------------------+-------------------+
|  operation | device | transport | Variable           | Variable          |
+------------+--------+-----------+                    |                   |
| 16 bits    | 8 bits | 8 bits    | Size               | Size              |
+------------+--------+-----------+--------------------+-------------------+

As of the time of this writing, GDAKIS defines two device types: CPU and CUDA, and one transport type IPC.
However, we expect the number of devices and transports to increase in the future.

Besides the data exchange, GDAKIS also supports requests to attach ancillary data to hand capabilities from the client to the server.
The transport (e.g., UNIX domain sockets) **must** support the exchange of ancillary data for GDAKIS to use it.


GDAKIS Responses
^^^^^^^^^^^^^^^^

Response messages follow a similar structure to requests, including a common header, an operation-specific and operation-transport specific responses.
Often time, both operation-specific and operation-transport specific payload for responses is void and, thus, takes zero bits.

+-------------------+--------------------+-------------------+
| Header            |  Operation Payload | Transport Payload |
+-------------------+--------------------+-------------------+
|  status | id      |  Variable          | Variable          |
+---------+---------+                    |                   |
| 32 bits | 32 bits |  Size              | Size              |
+---------+---------+--------------------+-------------------+


GDAKIS operations
^^^^^^^^^^^^^^^^^

GDAKIS defines two basic operations per communication abstraction: register and deregister.
Register operations make a given abstraction available to the server, their response returns an identifier to reference the abstraction.
Deregister operations take the identifier assigned to the abstraction, and upon completion, the server no longer have access to the abstraction.
GDAKIS does not enforce any particular policy on how identifiers are assigned to abstractions, and it is possible that different abstractions receive the same identifier.

.. toctree::
    :hidden:
    :maxdepth: 1
    :caption: Technical Documentation

    GDAKIS Communication Library <self>
    Programming Patterns <patterns>
    Autobatch Operation <autobatch>
    Autobatch Failure Analysis <autobatch_failure_condition>

.. toctree::
    :hidden:
    :maxdepth: 1
    :caption: API Documents

    Namespaces <${repo_docs_api_path}/namespaces>
    Classes <${repo_docs_api_path}/classes>