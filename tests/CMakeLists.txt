# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
add_executable(scada-client-frame-contract frame_contract.cpp)
target_link_libraries(scada-client-frame-contract PRIVATE SCADA::swap)
add_test(NAME scada-client-frame-contract COMMAND scada-client-frame-contract)

# Compile/link the API paths used by cuGraph. This target has no runtime test:
# its kernels and connection setup are never invoked during qualification.
add_executable(scada-client-api-compile api_compile.cu)
target_link_libraries(scada-client-api-compile PRIVATE SCADA::swap)
set_target_properties(scada-client-api-compile PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON)
