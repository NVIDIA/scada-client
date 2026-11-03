# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Callers pin each source archive with URL_HASH.
function(scada_third_party_urls org repo tag out_var)
    set(${out_var}
        "https://github.com/${org}/${repo}/archive/refs/tags/${tag}.tar.gz"
        PARENT_SCOPE
    )
endfunction()
