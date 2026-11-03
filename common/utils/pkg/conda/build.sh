# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

PROJECT_DIR="$(pwd)/common/utils"
cmake -DCMAKE_BUILD_TYPE:STRING=Release -DGDAKIS_UTILS_BUILD_TESTS:BOOL=OFF -S$PROJECT_DIR -B$PROJECT_DIR/build -DCMAKE_INSTALL_PREFIX=$PREFIX
cmake --build $PROJECT_DIR/build --config Release --target all -j`nproc`
cmake --install $PROJECT_DIR/build
