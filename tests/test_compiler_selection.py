"""Exercise compiler preference and explicit overrides through CMake itself."""

import os
import pathlib
import subprocess
import sys
import tempfile
import unittest


CMAKE = sys.argv.pop(1)
SELECTION = pathlib.Path(__file__).resolve().parent.parent / 'cmake/SelectCompiler.cmake'


class CompilerSelection(unittest.TestCase):
    def select(self, names, expected, override='', language='C'):
        with tempfile.TemporaryDirectory(prefix='stories-compiler-') as temporary:
            root = pathlib.Path(temporary)
            directories = []
            # Put cc's directory first: compiler name preference must win over PATH order.
            other_compiler = 'clang++' if language == 'C' else 'clang'
            for name in reversed(names + [other_compiler]):
                directory = root / name
                directory.mkdir()
                compiler = directory / name
                compiler.write_text('#!/bin/sh\nexit 0\n', encoding='ascii')
                compiler.chmod(0o755)
                directories.append(directory.as_posix())
            script = root / 'select.cmake'
            script.write_text(f"""set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)
set(CMAKE_PROGRAM_PATH "{';'.join(directories)}")
{override}
include("{SELECTION.as_posix()}")
file(WRITE "{(root / 'selected').as_posix()}" "${{CMAKE_{language}_COMPILER}}")
""", encoding='utf-8')
            environment = {key: value for key, value in os.environ.items() if key not in ['CC', 'CXX', 'CMAKE_TOOLCHAIN_FILE']}
            result = subprocess.run([CMAKE, '-P', str(script)], env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(pathlib.Path((root / 'selected').read_text()).name, expected)

    def test_compiler_preference_and_fallbacks(self):
        for names, expected in [(['clang', 'gcc', 'cc'], 'clang'), (['gcc', 'cc'], 'gcc'), (['cc'], 'cc')]:
            with self.subTest(available=names):
                self.select(names, expected)
        for names, expected in [(['clang++', 'g++', 'c++'], 'clang++'), (['g++', 'c++'], 'g++'), (['c++'], 'c++')]:
            with self.subTest(available=names):
                self.select(names, expected, language='CXX')

    def test_explicit_compiler_is_preserved(self):
        self.select(['clang', 'gcc', 'cc'], 'custom-compiler', 'set(CMAKE_C_COMPILER /explicit/custom-compiler)')
        self.select(['clang++', 'g++', 'c++'], 'custom-compiler', 'set(CMAKE_CXX_COMPILER /explicit/custom-compiler)', language='CXX')

    def test_environment_compiler_is_left_for_cmake(self):
        self.select(['clang', 'gcc', 'cc'], '', 'set(ENV{CC} /explicit/compiler)')
        self.select(['clang++', 'g++', 'c++'], '', 'set(ENV{CXX} /explicit/compiler)', language='CXX')

    def test_toolchain_file_is_left_for_cmake(self):
        self.select(['clang', 'gcc', 'cc'], '', 'set(CMAKE_TOOLCHAIN_FILE /explicit/toolchain.cmake)')
        self.select(['clang++', 'g++', 'c++'], '', 'set(CMAKE_TOOLCHAIN_FILE /explicit/toolchain.cmake)', language='CXX')

    def select_companion(self, c_driver, cxx_driver, source='variable', override='', companion=True):
        with tempfile.TemporaryDirectory(prefix='stories-toolchain-') as temporary:
            root = pathlib.Path(temporary)
            selected = root / 'selected toolchain'
            fallback = root / 'path'
            selected.mkdir()
            fallback.mkdir()
            # PATH's clang++ represents Apple Clang; selecting an off-PATH LLVM C compiler must win.
            drivers = [selected / c_driver, fallback / 'clang++']
            if companion:
                drivers.append(selected / cxx_driver)
            for driver in drivers:
                driver.write_text('#!/bin/sh\nexit 0\n', encoding='ascii')
                driver.chmod(0o755)
            if source == 'environment':
                compiler = f'set(ENV{{CC}} [=["{selected / c_driver}" -target test-target]=])'
            else:
                compiler = f'set(CMAKE_C_COMPILER "{selected / c_driver}")'
            script = root / 'select.cmake'
            script.write_text(f'''set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)
set(CMAKE_PROGRAM_PATH "{fallback}")
{compiler}
{override}
include("{SELECTION.as_posix()}")
file(WRITE "{root / 'result'}" "${{CMAKE_CXX_COMPILER}}")
''', encoding='utf-8')
            environment = {key: value for key, value in os.environ.items() if key not in ['CC', 'CXX', 'CMAKE_TOOLCHAIN_FILE']}
            result = subprocess.run([CMAKE, '-P', str(script)], env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            if override:
                expected = pathlib.Path('/explicit/custom-cxx')
            elif companion:
                expected = selected / cxx_driver
            else:
                expected = fallback / 'clang++'
            self.assertEqual((root / 'result').read_text(), str(expected))

    def test_selected_c_driver_prefers_its_companion(self):
        for c_driver, cxx_driver in [('clang', 'clang++'), ('clang-23', 'clang++-23'), ('gcc', 'g++'), ('gcc-14', 'g++-14'), ('cc', 'c++')]:
            with self.subTest(c_driver=c_driver):
                self.select_companion(c_driver, cxx_driver)

    def test_environment_c_driver_prefers_its_companion(self):
        self.select_companion('clang', 'clang++', source='environment')

    def test_explicit_cxx_overrides_companion_selection(self):
        self.select_companion('clang', 'clang++', override='set(CMAKE_CXX_COMPILER /explicit/custom-cxx)')

    def test_missing_companion_uses_normal_cxx_preference(self):
        self.select_companion('clang', 'clang++', companion=False)


if __name__ == '__main__':
    unittest.main()
