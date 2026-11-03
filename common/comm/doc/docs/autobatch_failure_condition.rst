.. SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0

===================================================================
Autobatch Algorithm: Race Condition Failure Analysis
===================================================================

Executive Summary
=================

This document describes a critical race condition in the autobatch algorithm that can lead to data corruption under specific timing conditions. The failure occurs when an "evil thread" experiences a delayed execution during the buffer exit process, combined with epoch counter rollover and precise timing of victim thread operations.

Background
==========

The autobatch algorithm is a high-throughput work submission mechanism that uses multiple buffers with atomic counters to coordinate concurrent thread access. Each buffer maintains:

- ``in_`` counter: 64-bit value tracking epoch (bits 60-62), commit bit (bit 63), and entry count (lower bits)
- ``out_`` counter: tracks validated entries in the buffer

Failure Scenario Description
============================

Thread Configuration
---------------------

- **Thread_0**: Evil thread (causes the race condition)
- **Thread_1-8**: Committer threads (normal operation)  
- **Thread_9**: Victim thread (suffers data corruption)

Sequence of Events
------------------

Phase 1: Evil Thread Setup (Epoch 0)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Evil thread obtains entry token to buffer

2. Stores IO request in buffer

3. Calls ``exit()`` and obtains local variables:

   - ``out_count = lower(in_count) = 1``

4. **Critical**: Thread goes to sleep before completing ``compare_exchange`` operation

Phase 2: Buffer Commits and Epoch Rollover
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. **Thread_1** enters buffer after evil thread sleeps

2. Obtains ``out_count = lower(in_count) = 2``

3. Commits buffer (including evil thread's request)

4. Buffer resets, epoch increments to 1

5. **Threads_2-8** each commit buffers individually

6. Epoch rolls over exactly ``size(epoch)`` times, returning to 0

Phase 3: Failure Event
~~~~~~~~~~~~~~~~~~~~~~~

1. **Thread_9 (victim)** enters buffer successfully

2. Victim thread delays before storing IO request

3. Buffer state: ``in_ = 1`` (epoch 0, count 1, commit 0)

4. **Evil thread wakes up** and proceeds with ``compare_exchange``

5. Evil thread's local ``in_count`` matches current ``in_`` value

6. Evil thread successfully sets commit bit and commits **empty buffer**

7. Epoch increments

8. **Victim thread** later stores IO request, potentially overwriting other data

Root Cause Analysis
===================

The corruption occurs due to the convergence of four critical conditions:

1. **Delayed Thread Execution**: Evil thread sleeps between obtaining local state and executing atomic operation

2. **Precise Epoch Rollover**: Exactly ``size(epoch)`` commits occur during evil thread's sleep

3. **Victim Thread Timing**: Victim reaches same ``in_count`` but hasn't stored data yet

4. **No Interference**: No other threads increment ``in_count`` between victim and evil thread operations

Technical Impact
================

- **Data Corruption**: IO requests may be lost or overwritten
- **Silent Failure**: No immediate error indication  
- **Probabilistic**: Requires precise timing alignment

Proposed Mitigation
===================

Immediate Solution: Increase Epoch Bits
----------------------------------------

**Current State**: 3 bits for epoch (8 distinct values)

**Proposed**: 7 bits for epoch (128 distinct values)

Bit Allocation Analysis
~~~~~~~~~~~~~~~~~~~~~~~

- Total available bits in ``in_`` counter: 63 bits
- Required for counter portion: 56 bits (pessimistic calculation)

  - GPU threads: ~400,000 concurrent
  - Duration: 64 seconds continuous operation
  - Frequency: 2 GHz
  - Max value: 2^31 Hz × 2^19 threads × 2^6 seconds = 2^56

- Available for epoch: 63 - 56 = 7 bits

Benefits
~~~~~~~~

- **16x reduction** in failure probability
- **No performance impact**
- **Maintains algorithmic structure**
- **Simple implementation**

Implementation Recommendations
==============================

1. **Immediate**: Implement epoch bit reallocation

2. **Long-term**: Consider additional synchronization mechanisms

3. **Testing**: Develop stress tests for race condition scenarios

4. **Monitoring**: Add detection mechanisms for epoch rollover events

Conclusion
==========

While this race condition requires extremely precise timing to manifest, the potential for silent data corruption makes it a critical issue worth considering and at least documenting. The proposed epoch bit reallocation provides a practical, low-risk mitigation that significantly reduces the probability of occurrence while maintaining system performance.

---

