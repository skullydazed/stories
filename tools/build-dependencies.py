#!/usr/bin/env python3
"""Build the latest released static libraries for CI and release archives.

Normal CMake builds continue to use installed libraries. This opt-in helper needs
CMake, Ninja, Git, a C/C++ toolchain, and network access to GitHub.
"""

import hashlib
import pathlib
import re
import shutil
import subprocess
import sys
import urllib.request


DEPENDENCIES = [
    ('civetweb', 'civetweb/civetweb', [
        '-DCIVETWEB_BUILD_TESTING=OFF', '-DCIVETWEB_ENABLE_SERVER_EXECUTABLE=OFF', '-DCIVETWEB_ENABLE_CXX=OFF', '-DCIVETWEB_ENABLE_SSL=OFF', '-DCIVETWEB_ENABLE_ASAN=OFF',
        # Access records are formatted on worker threads before Stories' logging callback.
        '-DCMAKE_C_FLAGS=-DREENTRANT_TIME',
    ]),
    ('md4c', 'mity/md4c', [
        '-DBUILD_MD2HTML_EXECUTABLE=OFF',
    ]),
]


def run(*command):
    subprocess.run(command, check=True)


def latest_release(repository):
    # Both projects publish version tags; not all tags have a GitHub Release.
    output = subprocess.check_output(['git', 'ls-remote', '--tags', 'https://github.com/' + repository + '.git'], text=True, timeout=120)
    refs = {}
    versions = []
    for line in output.splitlines():
        commit, ref = line.split()
        refs[ref] = commit
        match = re.fullmatch(r'refs/tags/(?:v|release-)?(\d+(?:\.\d+)+)', ref)
        if match:
            versions.append((tuple(int(part) for part in match[1].split('.')), ref))
    if not versions:
        raise SystemExit('no release version tags found for ' + repository)
    _, ref = max(versions)
    # Peel annotated tags and download the resolved commit, even if a tag moves.
    commit = refs.get(ref + '^{}', refs[ref])
    if not re.fullmatch(r'[0-9a-f]{40,64}', commit):
        raise SystemExit('invalid release commit for ' + repository)
    return ref[len('refs/tags/'):], commit


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: build-dependencies.py PREFIX')
    prefix = pathlib.Path(sys.argv[1]).resolve()
    sources = prefix.parent / (prefix.name + '-sources')
    sources.mkdir(parents=True, exist_ok=True)
    licenses = prefix / 'licenses'
    licenses.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, repository, options in DEPENDENCIES:
        version, commit = latest_release(repository)
        print(f'Building {name} {version} ({commit})', flush=True)
        url = f'https://codeload.github.com/{repository}/tar.gz/{commit}'
        archive = sources / (name + '-' + commit + '.tar.gz')
        if not archive.exists():
            with urllib.request.urlopen(url, timeout=120) as response:
                archive.write_bytes(response.read())
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        source = sources / (name + '-' + commit)
        source.mkdir(exist_ok=True)
        run('tar', '-xzf', str(archive), '-C', str(source), '--strip-components=1')
        build = sources / (name + '-' + commit + '-build')
        run('cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5', '-DCMAKE_POSITION_INDEPENDENT_CODE=ON', '-DBUILD_SHARED_LIBS=OFF', '-DCMAKE_INSTALL_LIBDIR=lib', '-DCMAKE_INSTALL_PREFIX=' + str(prefix), *options)
        run('cmake', '--build', str(build), '--parallel', '2')
        run('cmake', '--install', str(build))
        shutil.copyfile(source / 'LICENSE.md', licenses / (name + '.md'))
        manifest.append(f'{name} {version}\nCommit: {commit}\nSource: {url}\nSHA256: {digest}\n')
    (licenses / 'DEPENDENCIES.txt').write_text('\n'.join(manifest), encoding='utf-8')


if __name__ == '__main__':
    main()
