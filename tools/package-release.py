#!/usr/bin/env python3
"""Stage, check, and archive a tested release build; no publication side effects."""

import hashlib
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile


def run(*command, **kwargs):
    return subprocess.run(command, check=True, **kwargs)


def main():
    if len(sys.argv) != 4:
        raise SystemExit('usage: package-release.py BUILD_DIRECTORY DEPENDENCY_PREFIX TARGET')
    build = pathlib.Path(sys.argv[1]).resolve()
    dependencies = pathlib.Path(sys.argv[2]).resolve()
    target = sys.argv[3]
    if not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9_]+)+', target):
        raise SystemExit('invalid release target: ' + target)
    version = (build / 'stories-version.txt').read_text(encoding='ascii').strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[a-zA-Z0-9.-]+)?', version):
        raise SystemExit('invalid release version: ' + version)
    if (os.environ.get('RELEASE_TAG') or 'v' + version) != 'v' + version:
        raise SystemExit('packaged version does not match the selected release tag: v' + version)
    root = pathlib.Path(__file__).resolve().parent.parent
    output = root / 'build-artifacts'
    output.mkdir(exist_ok=True)
    name = f'stories-{version}-{target}'
    with tempfile.TemporaryDirectory(prefix='stories-package-') as temporary:
        staging = pathlib.Path(temporary) / 'stage'
        # Override the install prefix so staging never writes to system paths.
        run('cmake', '--install', str(build), '--prefix', str(staging))
        shutil.copytree(dependencies / 'licenses', staging / 'share/licenses/dependencies')
        shutil.copyfile(root / 'README.md', staging / 'README.md')
        archive = output / (name + '.tar.gz')
        with tarfile.open(archive, 'w:gz') as stream:
            stream.add(staging, arcname=name)
        # Exercise the actual archive after extraction, including executable mode.
        extracted = pathlib.Path(temporary) / 'extracted'
        run('tar', '-xzf', str(archive), '-C', str(pathlib.Path(temporary)))
        (pathlib.Path(temporary) / name).rename(extracted)
        binary = extracted / 'bin/stories'
        if sys.platform == 'darwin':
            linked = subprocess.check_output(['otool', '-L', str(binary)], text=True)
            libraries = [line.strip().split(' (', 1)[0] for line in linked.splitlines()[1:]]
            allowed = lambda library: library.startswith(('/usr/lib/', '/System/Library/'))
        else:
            linked = subprocess.check_output(['ldd', str(binary)], text=True)
            libraries = re.findall(r'=>\s+(\S+)', linked)
            allowed = lambda library: library.startswith(('/lib/', '/lib64/', '/usr/lib/'))
        if not libraries or any(not allowed(library) for library in libraries) or re.search(r'lib(?:civetweb|md4c)', linked):
            raise SystemExit('release has unexpected runtime dependencies:\n' + linked)
        run(str(binary), '--version')
        run(sys.executable, str(root / 'tests/test_http.py'), str(binary))
        # Verify the installed example and stylesheet together with authored content.
        config = extracted / 'stories.conf'
        content = extracted / 'content'
        content.mkdir()
        (content / 'index.md').write_text('# Release smoke test\n', encoding='utf-8')
        example = (extracted / 'share/stories/examples/stories.conf.example').read_text(encoding='utf-8')
        example = example.replace('root = docroot', 'root = content').replace('root = assets', 'root = share/stories/assets')
        example = example.replace('template = templates/page.html', 'template = share/stories/templates/page.html')
        config.write_text(example, encoding='utf-8')
        if (extracted / 'share/stories/assets/pico.min.css').read_bytes() != (root / 'assets/pico.min.css').read_bytes():
            raise SystemExit('installed stylesheet differs from the bundled asset')
        if (extracted / 'share/stories/templates/page.html').read_bytes() != (root / 'templates/page.html').read_bytes():
            raise SystemExit('installed page template differs from the source template')
        run(str(binary), '--config', str(config), '--check-config')
        run(str(binary), '--config', str(extracted / 'share/stories/examples/docs.conf.example'), '--check-config')
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_name(archive.name + '.sha256').write_text(f'{checksum}  {archive.name}\n', encoding='ascii')
    print(archive)


if __name__ == '__main__':
    main()
