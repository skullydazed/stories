#!/bin/sh
# Provider-neutral Linux CI entrypoint; use a full Clang toolchain.
set -eu

task_build_dir=${1:-build-ci}
task_fuzz_dir=${2:-build-ci-fuzz}
task_tsan_dir=${3:-build-ci-tsan}

cmake -B "$task_build_dir" -G Ninja -DBUILD_TESTING=ON -DSTORIES_WARNINGS_AS_ERRORS=ON -DSTORIES_SANITIZERS=ON
cmake --build "$task_build_dir" --parallel 2
ctest --test-dir "$task_build_dir" --output-on-failure --parallel 3

cmake -B "$task_tsan_dir" -G Ninja -DBUILD_TESTING=ON -DSTORIES_WARNINGS_AS_ERRORS=ON -DSTORIES_THREAD_SANITIZER=ON
cmake --build "$task_tsan_dir" --parallel 2
ctest --test-dir "$task_tsan_dir" --output-on-failure --parallel 3

cmake -B "$task_fuzz_dir" -G Ninja -DBUILD_TESTING=OFF -DSTORIES_WARNINGS_AS_ERRORS=ON -DSTORIES_FUZZING=ON
cmake --build "$task_fuzz_dir" --parallel 2
ctest --test-dir "$task_fuzz_dir" --label-regex fuzz --output-on-failure --parallel 3
