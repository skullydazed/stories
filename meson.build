project('stories', 'c', version: '0.1.0', meson_version: '>=1.3.0', default_options: ['c_std=c11', 'warning_level=3'])

cc = meson.get_compiler('c')
python = import('python').find_installation('python3')
add_project_arguments('-D_POSIX_C_SOURCE=200809L', '-D_XOPEN_SOURCE=700', language: 'c')
if host_machine.system() == 'darwin'
  # Darwin hides O_NOFOLLOW behind its native feature namespace.
  add_project_arguments('-D_DARWIN_C_SOURCE', language: 'c')
endif

civetweb = dependency('civetweb', required: false)
if not civetweb.found()
  civetweb = cc.find_library('civetweb')
endif
md4c = dependency('md4c', required: false)
if not md4c.found()
  md4c = cc.find_library('md4c')
endif
threads = dependency('threads')
foreach item : [['civetweb.h', civetweb], ['md4c.h', md4c]]
  if not cc.has_header(item[0], dependencies: item[1])
    error('Install the development headers for ' + item[0])
  endif
endforeach
if not cc.has_function('mg_get_server_ports', prefix: '#include <civetweb.h>', dependencies: civetweb)
  error('CivetWeb must provide mg_get_server_ports(). Install CivetWeb 1.16 or newer.')
endif
if not cc.has_header_symbol('md4c.h', 'MD_FLAG_TASKLISTS', dependencies: md4c)
  error('MD4C must support task lists. Install MD4C 0.4.8 or newer.')
endif

build_configuration = configuration_data()
build_configuration.set_quoted('STORIES_VERSION', meson.project_version())
build_configuration.set_quoted('STORIES_ASSET_DIR', get_option('prefix') / get_option('datadir') / 'stories/assets')
build_configuration.set_quoted('STORIES_SOURCE_ASSET_DIR', meson.current_source_dir() / 'assets')
configure_file(output: 'build_config.h', configuration: build_configuration)

entities = custom_target('html-entities', input: 'tools/generate_entities.py', output: 'entities.inc', command: [python, '@INPUT@', '@OUTPUT@'])
includes = include_directories('src', '.')
core_sources = files('src/buffer.c', 'src/config.c', 'src/url.c', 'src/document.c', 'src/entities.c', 'src/render.c')
core = static_library('stories-core', core_sources, entities, include_directories: includes, dependencies: md4c)
core_dependency = declare_dependency(link_with: core, include_directories: includes, dependencies: md4c)
stories = executable('stories', 'src/main.c', 'src/server.c', dependencies: [core_dependency, civetweb, threads], install: true)
install_data('assets/pico.min.css', 'assets/LICENSE.pico.md', install_dir: get_option('datadir') / 'stories/assets')
install_data('stories.conf.example', 'examples/stories.freebsd', 'examples/nginx.conf', install_dir: get_option('datadir') / 'stories/examples')
install_data('LICENSE.md', install_dir: get_option('datadir') / 'licenses/stories')

fixture_directory = meson.current_source_dir() / 'tests/fixtures'
fixture_argument = '-DTEST_FIXTURE_DIR="' + fixture_directory + '"'
if get_option('tests')
  unit_tests = executable('test-subsystems', 'tests/test_subsystems.c', dependencies: core_dependency, c_args: fixture_argument)
  test('subsystems', unit_tests, suite: 'unit')
  test('http', python, args: [files('tests/test_http.py'), stories.full_path()], depends: stories, timeout: 90, suite: 'integration')
endif

if get_option('fuzzing')
  fuzz_flags = ['-fsanitize=fuzzer-no-link,address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=undefined', '-UNDEBUG', '-g', '-O1']
  fuzz_link_flags = ['-fsanitize=fuzzer,address,undefined', '-fno-sanitize-recover=undefined']
  if not cc.links('#include <stdint.h>\n#include <stddef.h>\nint LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) { (void)data; (void)size; return 0; }', args: fuzz_link_flags, name: 'libFuzzer with ASan and UBSan')
    error('Fuzzing requires Clang with the libFuzzer, ASan, and UBSan runtimes. Select a full LLVM Clang installation with CC.')
  endif
  fuzz_core = static_library('stories-fuzz-core', core_sources, entities, include_directories: includes, dependencies: md4c, c_args: fuzz_flags)
  foreach target : ['url_resolver', 'link_rewriter', 'config_parser']
    fuzzer = executable('fuzz_' + target, 'fuzz/fuzz_' + target + '.c', include_directories: includes, link_with: fuzz_core, dependencies: md4c, c_args: fuzz_flags + [fixture_argument], link_args: fuzz_link_flags)
    test('fuzz-' + target, python, args: [files('fuzz/smoke.py'), fuzzer.full_path(), meson.current_source_dir() / 'fuzz/corpus' / target, meson.current_build_dir() / 'fuzz-artifacts' / target, '30'], timeout: 50, suite: 'fuzz')
  endforeach
endif
