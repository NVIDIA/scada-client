# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
with section("parse"):
    # Teach cmake-format the CPM.cmake API so keyword/value pairs stay together.
    additional_commands = {
        "cpmaddpackage": {
            "kwargs": {
                "NAME": 1,
                "FORCE": 1,
                "VERSION": 1,
                "GIT_TAG": 1,
                "DOWNLOAD_ONLY": 1,
                "GITHUB_REPOSITORY": 1,
                "GITLAB_REPOSITORY": 1,
                "BITBUCKET_REPOSITORY": 1,
                "GIT_REPOSITORY": 1,
                "SOURCE_DIR": 1,
                "FIND_PACKAGE_ARGUMENTS": 1,
                "NO_CACHE": 1,
                "SYSTEM": 1,
                "GIT_SHALLOW": 1,
                "EXCLUDE_FROM_ALL": 1,
                "SOURCE_SUBDIR": 1,
                "CUSTOM_CACHE_KEY": 1,
                "URL": "*",
                "OPTIONS": "*",
                "DOWNLOAD_COMMAND": "*",
                "PATCHES": "*",
            }
        }
    }

with section("format"):
    tab_size = 4
    use_tabchars = False
    line_width = 120
    max_subgroups_hwrap = 2
    max_pargs_hwrap = 6
    always_wrap = []
    enable_sort = True
    autosort = False
    dangle_parens = True
    dangle_align = "prefix"
    command_case = "lower"
    # Keywords (ON, OFF, PUBLIC, PRIVATE, ...) preserve existing case
    keyword_case = "unchanged"

with section("markup"):
    bullet_char = "*"
    enum_char = "."
    first_comment_is_literal = True
    literal_comment_pattern = None
    fence_pattern = "^\\s*([`~]{3}[`~]*)(.*)$"
    ruler_pattern = "^\\s*[^\\w\\s]{3}.*[^\\w\\s]{3}$"
    explicit_trailing_pattern = "#<"
    hashruler_min_length = 10
    canonicalize_hashrulers = True
    # CMake comments often contain code or deliberately wrapped commands.
    enable_markup = False

with section("lint"):
    disabled_codes = ["C0113"]
    function_pattern = "[0-9a-z_]+"
    macro_pattern = "[0-9A-Z_]+"
    global_var_pattern = "[A-Z][0-9A-Z_]+"
    internal_var_pattern = "_[A-Z][0-9A-Z_]+"
    local_var_pattern = "[a-z][a-z0-9_]+"
    private_var_pattern = "_[0-9a-z_]+"
    public_var_pattern = "[A-Z][0-9A-Z_]+"
    argument_var_pattern = "[a-z][a-z0-9_]+"
    keyword_pattern = "[A-Z][0-9A-Z_]+"
    max_conditionals_custom_parser = 2
    min_statement_spacing = 1
    max_statement_spacing = 2
    max_returns = 6
    max_branches = 12
    max_arguments = 10
    max_localvars = 15
    max_statements = 50

with section("misc"):
    # CPM.cmake's public API uses CamelCase; keep that spelling while
    # normalizing built-in CMake commands to lower case.
    per_command = {"CPMAddPackage": {"command_case": "unchanged"}}
