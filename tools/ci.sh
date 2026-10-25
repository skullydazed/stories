#!/bin/sh
# Provider-neutral Linux CI entrypoint; use a full Clang toolchain.
set -eu

task_build_dir=${1:-build-ci}
task_fuzz_dir=${2:-build-ci-fuzz}
export CC=${CC:-clang}

if [ -f "$task_build_dir/meson-private/coredata.dat" ]; then
	meson setup --reconfigure "$task_build_dir" -Dtests=true -Dwerror=true -Db_sanitize=address,undefined -Db_lundef=false
else
	meson setup "$task_build_dir" -Dtests=true -Dwerror=true -Db_sanitize=address,undefined -Db_lundef=false
fi
meson compile -C "$task_build_dir"
meson test -C "$task_build_dir" --print-errorlogs

if [ -f "$task_fuzz_dir/meson-private/coredata.dat" ]; then
	meson setup --reconfigure "$task_fuzz_dir" -Dfuzzing=true -Dtests=false -Dwerror=true -Db_lundef=false
else
	meson setup "$task_fuzz_dir" -Dfuzzing=true -Dtests=false -Dwerror=true -Db_lundef=false
fi
meson compile -C "$task_fuzz_dir"
meson test -C "$task_fuzz_dir" --suite fuzz --print-errorlogs
