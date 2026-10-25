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
    def select(self, names, expected, override=''):
        with tempfile.TemporaryDirectory(prefix='stories-compiler-') as temporary:
            root = pathlib.Path(temporary)
            directories = []
            # Put cc's directory first: compiler name preference must win over PATH order.
            for name in reversed(names):
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
file(WRITE "{(root / 'selected').as_posix()}" "${{CMAKE_C_COMPILER}}")
""", encoding='utf-8')
            environment = {key: value for key, value in os.environ.items() if key not in ['CC', 'CMAKE_TOOLCHAIN_FILE']}
            result = subprocess.run([CMAKE, '-P', str(script)], env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(pathlib.Path((root / 'selected').read_text()).name, expected)

    def test_compiler_preference_and_fallbacks(self):
        for names, expected in [(['clang', 'gcc', 'cc'], 'clang'), (['gcc', 'cc'], 'gcc'), (['cc'], 'cc')]:
            with self.subTest(available=names):
                self.select(names, expected)

    def test_explicit_compiler_is_preserved(self):
        self.select(['clang', 'gcc', 'cc'], 'custom-compiler', 'set(CMAKE_C_COMPILER /explicit/custom-compiler)')

    def test_environment_compiler_is_left_for_cmake(self):
        self.select(['clang', 'gcc', 'cc'], '', 'set(ENV{CC} /explicit/compiler)')

    def test_toolchain_file_is_left_for_cmake(self):
        self.select(['clang', 'gcc', 'cc'], '', 'set(CMAKE_TOOLCHAIN_FILE /explicit/toolchain.cmake)')


if __name__ == '__main__':
    unittest.main()
