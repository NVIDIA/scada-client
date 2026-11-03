// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
// Build/link probe only. The assessment never executes this program.
#include <scada/io>
#include <scada/swap/apply_sm>
#include <scada/swap/portal>
#include <scada/swap/store>
#include <scada/thrust/mdspan/iterator>

using portal_type = scada::swap::portal<float, 1024, scada::device_type::cuda, gdakis::thread_scope_device, 8>;

__global__ void probe_kernel(portal_type * portal, float * output) {
    auto result = portal->get({ 0, 0 });
    if (result.is_ok()) {
        output[0] = result.ok();
        portal->put({ 0, 0 }, output[0]);
    }
}

__global__ void probe_collective_kernel(portal_type * portal, float * output) {
    auto tile = cooperative_groups::tiled_partition<32>(cooperative_groups::this_thread_block());
    float value = 0;
    portal->get_group(tile, { 0, 0 }, value);
    portal->put_group(tile, { 0, 0 }, value);
    if (tile.thread_rank() == 0)
        output[0] = value;
}

__global__ void probe_apply_kernel(portal_type * portal, float * output) {
    auto tile = cooperative_groups::tiled_partition<32>(cooperative_groups::this_thread_block());
    using state_type = scada::swap::apply_sm::state_t;
    __shared__ alignas(state_type) unsigned char raw[32 * sizeof(state_type)];
    auto * states = reinterpret_cast<state_type *>(raw);
    new (&states[threadIdx.x]) state_type();
    auto callback = [] __device__(auto &, float &, unsigned) noexcept {};
    portal->coop_apply_target<32, 32>(tile, states, { 0, 0 }, output, callback, true);
}

void compile_client_api() {
    auto store = scada::swap::store<scada::io::socket::domain::local>::build("/tmp/scada-audit-unused.sock");
    if (store.is_err())
        return;
    auto slab = store.ok().alloc<float>(1024);
    if (slab.is_err())
        return;
    auto portal = portal_type::build(store.ok(), 4096, 1024);
    if (portal.is_err())
        return;
    auto span = portal.ok().make_span(slab.ok());
    auto first = scada::thrust::begin(span);
    auto last = scada::thrust::end(span);
    (void)(last - first);
    probe_kernel<<<1, 1>>>(portal.ok().get(), nullptr);
    probe_collective_kernel<<<1, 32>>>(portal.ok().get(), nullptr);
    probe_apply_kernel<<<1, 32>>>(portal.ok().get(), nullptr);
    return;
}

int main() {
    return 0;
}
