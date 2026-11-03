# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

"""Configure-only regression checks; no CUDA compiler, GPU or storage IO required."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


HELPERS = Path(__file__).resolve().parents[1]


class ConfigurationTests(unittest.TestCase):
    def check_source_version(self, setting, expected):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.26)\nproject(consumer VERSION 9.8.7 LANGUAGES NONE)\n'
                + setting
                + f'include("{HELPERS / "FetchVersion.cmake"}")\n'
                + f'if(NOT PROJECT_VERSION STREQUAL "{expected}")\n'
                + 'message(FATAL_ERROR "SCADA inherited the consumer version")\nendif()\n',
                encoding="ascii",
            )
            subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build")],
                           check=True, capture_output=True, text=True)

    def test_archive_version_is_independent_of_parent(self):
        self.check_source_version("", "0.1.0")

    def test_explicit_source_version_override(self):
        self.check_source_version('set(SCADA_SOURCE_VERSION "1.2.3")\n', "1.2.3")

    def configure(self, prelude="", expected=None, probe="printf '9.0\\n'", environment=None, helper=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            smi = root / "nvidia-smi"
            smi.write_text("#!/bin/sh\n" + probe + "\n", encoding="ascii")
            smi.chmod(0o700)
            check = (
                'if(DEFINED CMAKE_CUDA_ARCHITECTURES)\nmessage(FATAL_ERROR "Unexpected architecture")\nendif()'
                if expected is None
                else 'if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR NOT "${CMAKE_CUDA_ARCHITECTURES}" STREQUAL "' + expected
                + '")\nmessage(FATAL_ERROR "Architecture changed: ${CMAKE_CUDA_ARCHITECTURES}")\nendif()'
            )
            script = root / "check.cmake"
            script.write_text(
                f'set(SCADA_NVIDIA_SMI "{smi}")\n' + prelude + "\n"
                + f'include("{HELPERS / (helper or "ScadaCudaArchitectures.cmake")}")\n'
                + check + "\n", encoding="ascii"
            )
            env = os.environ.copy()
            env.pop("CUDAARCHS", None)
            env.update(environment or {})
            subprocess.run(["cmake", "-P", str(script)], env=env, check=True, capture_output=True, text=True)

    def test_native(self):
        self.configure(expected="native")

    def test_multiple_gpus(self):
        self.configure(expected="native", probe="printf '9.0\\n10.3\\n'")

    def test_parent_architectures(self):
        self.configure('set(CMAKE_CUDA_ARCHITECTURES "90;103")', "90;103")

    def test_cache_architectures(self):
        self.configure('set(CMAKE_CUDA_ARCHITECTURES 100 CACHE STRING "")', "100")

    def test_off(self):
        self.configure('set(CMAKE_CUDA_ARCHITECTURES OFF)', "OFF")

    def test_empty(self):
        self.configure('set(CMAKE_CUDA_ARCHITECTURES "")', "")

    def test_environment(self):
        self.configure(environment={"CUDAARCHS": "90;103"})

    def test_toolchain(self):
        self.configure('set(CMAKE_TOOLCHAIN_FILE toolchain.cmake)')

    def test_cross_compiling(self):
        self.configure('set(CMAKE_CROSSCOMPILING TRUE)')

    def test_foreign_system(self):
        self.configure('set(CMAKE_SYSTEM_NAME Generic)')

    def test_no_gpu(self):
        self.configure(probe="exit 1")

    def test_empty_probe(self):
        self.configure(probe="exit 0")

    def test_malformed_probe(self):
        self.configure(probe="printf 'No CUDA devices found.\\n'")

    def test_partial_numeric_probe(self):
        self.configure(probe="printf '9.0\\nInvalid device\\n'")

    def test_test_helper_preserves_selection(self):
        self.configure('set(CMAKE_CUDA_ARCHITECTURES "90;103")', "90;103", helper="TestCudaArchitectures.cmake")

    def test_clangd_preserves_flags_and_launcher(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.26)\nproject(policy NONE)\n'
                'set(CMAKE_CUDA_FLAGS "consumer-flags")\n'
                'set(CMAKE_CUDA_COMPILER_LAUNCHER "consumer-launcher")\n'
                f'include("{HELPERS / "cuda-clangd.cmake"}")\n'
                'if(NOT CMAKE_CUDA_FLAGS STREQUAL "consumer-flags" OR\n'
                '   NOT CMAKE_CUDA_COMPILER_LAUNCHER STREQUAL "consumer-launcher")\n'
                'message(FATAL_ERROR "Consumer configuration changed")\nendif()\n', encoding="ascii"
            )
            subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build")],
                           check=True, capture_output=True, text=True)

    def test_warning_policy_default(self):
        self.warning_policy(True)

    def test_define_test_preserves_target_architectures(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "test.cpp").write_text("int main() { return 0; }\n", encoding="ascii")
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.26)\nproject(policy CXX)\n'
                'set(CMAKE_CUDA_ARCHITECTURES "90;103")\n'
                'set(CMAKE_CUDA_COMPILER "configure-only-fixture")\n'
                f'include("{HELPERS / "DefineTest.cmake"}")\n'
                'define_test(consumer test.cpp)\n'
                'get_target_property(architectures consumer CUDA_ARCHITECTURES)\n'
                'if(NOT architectures STREQUAL "90;103")\n'
                'message(FATAL_ERROR "Test target overrides consumer architectures")\nendif()\n',
                encoding="ascii"
            )
            subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build")],
                           check=True, capture_output=True, text=True)

    def test_warning_policy_opt_out(self):
        self.warning_policy(False)

    def warning_policy(self, enabled):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            setting = "ON" if enabled else "OFF"
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.26)\nproject(policy NONE)\n'
                + ("" if enabled else f'set(SCADA_CXX_WARNINGS_AS_ERRORS {setting})\n'
                   f'set(SCADA_CUDA_WARNINGS_AS_ERRORS {setting})\n')
                + f'include("{HELPERS / "CommonCompilerOptions.cmake"}")\n'
                + 'get_directory_property(flags COMPILE_OPTIONS)\n'
                + f'if({"NOT" if enabled else ""} flags MATCHES "-Werror")\n'
                + 'message(FATAL_ERROR "Warning policy changed")\nendif()\n', encoding="ascii"
            )
            subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build")],
                           check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
