#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
check_dir="$(mktemp -d "${TMPDIR:-/tmp}/scada-client-check.XXXXXX")"
trap 'rm -rf "${check_dir}"' EXIT
mkdir -p "${check_dir}/source"
cp -R "${repo_root}/." "${check_dir}/source/"

for linkage in static shared; do
  build_shared=OFF
  if [[ "${linkage}" == shared ]]; then
    build_shared=ON
  fi
  case_dir="${check_dir}/${linkage}"
  cmake -S "${check_dir}/source" -B "${case_dir}/build" \
    -DSCADA_CLIENT_BUILD_TESTS=ON \
    -DBUILD_SHARED_LIBS="${build_shared}" \
    -DCMAKE_SHARED_LINKER_FLAGS=-Wl,--no-undefined \
    -DCCCL_DIR="${CCCL_DIR:?Set CCCL_DIR to the qualified CCCL CMake package}" \
    -DCMAKE_CUDA_ARCHITECTURES="${CMAKE_CUDA_ARCHITECTURES:-90}" \
    -DCMAKE_BUILD_TYPE=Release
  cmake --build "${case_dir}/build" --parallel "${SCADA_BUILD_JOBS:-2}"
  # Run the framing check once. Shared CUDA libraries cannot be loaded on the
  # driverless compile runners, where both application probes remain build-only.
  if [[ "${linkage}" == static ]]; then
    ctest --test-dir "${case_dir}/build" --output-on-failure
  fi
  cmake --install "${case_dir}/build" --prefix "${case_dir}/install"

  for private_header in gdakis/queue gdakis/doorbell scada/swap/runtime_configuration scada/swap/compile_configuration; do
    test ! -e "${case_dir}/install/include/${private_header}"
  done
  cmake -S "${check_dir}/source/tests/installed" -B "${case_dir}/consumer" \
    -DCMAKE_PREFIX_PATH="${case_dir}/install/cmake" -DCCCL_DIR="${CCCL_DIR}" \
    -DCMAKE_CUDA_ARCHITECTURES="${CMAKE_CUDA_ARCHITECTURES:-90}"
  cmake --build "${case_dir}/consumer" --parallel "${SCADA_BUILD_JOBS:-2}"
done
