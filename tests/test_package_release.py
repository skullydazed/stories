"""Check release CLI validation against the targets actually used by CI."""

import contextlib
import importlib.util
import json
import pathlib
import re
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = pathlib.Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location('package_release', ROOT / 'tools/package-release.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class InstallationReached(Exception):
    """Stop before installing files; these tests exercise the CLI boundary."""


class ReleaseArguments(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='stories-package-arguments-')
        self.addCleanup(temporary.cleanup)
        self.root = pathlib.Path(temporary.name)
        stack = contextlib.ExitStack()
        self.addCleanup(stack.close)
        stack.enter_context(patch.object(PACKAGE, '__file__', str(self.root / 'tools/package-release.py')))
        stack.enter_context(patch.dict('os.environ', {'RELEASE_TAG': 'v1.2.3'}))
        self.metadata = stack.enter_context(patch.object(PACKAGE.subprocess, 'check_output', return_value=json.dumps({'version': '1.2.3'})))
        self.install = stack.enter_context(patch.object(PACKAGE, 'run', side_effect=InstallationReached))

    def invoke(self, target):
        with patch.object(sys, 'argv', ['package-release.py', str(self.root / 'build'), str(self.root / 'dependencies'), target]):
            PACKAGE.main()

    def test_ci_targets_pass_validation(self):
        workflow = (ROOT / '.github/workflows/build.yml').read_text(encoding='utf-8')
        targets = re.findall(r'^\s+- target:\s+(\S+)', workflow, re.MULTILINE)
        self.assertTrue(targets, 'no release targets found in the build workflow')
        for target in targets:
            with self.subTest(target=target):
                self.install.reset_mock()
                with self.assertRaises(InstallationReached):
                    self.invoke(target)
                self.assertEqual(self.install.call_args.args[:2], ('meson', 'install'))

    def test_unsafe_targets_are_rejected_before_running_tools(self):
        for target in ['../linux-x86_64', '/linux-x86_64', '-linux-x86_64', 'linux/x86_64', 'linux-x86_64\njunk']:
            with self.subTest(target=target):
                with self.assertRaisesRegex(SystemExit, 'invalid release target'):
                    self.invoke(target)
        self.metadata.assert_not_called()
        self.install.assert_not_called()


if __name__ == '__main__':
    unittest.main()
