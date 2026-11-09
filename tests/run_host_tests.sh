#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_binary="$(mktemp "${TMPDIR:-/tmp}/nasua-scheduler-test.XXXXXX")"
trap 'rm -f "$test_binary"' EXIT

printf '[host-tests] compiling scheduler regression tests with %s\n' "${CXX:-c++}"
"${CXX:-c++}" \
    -std=gnu++20 \
    -O2 \
    -Wall \
    -Wextra \
    -Werror \
    -DSCHEDULER_TEST \
    -I"$repo_root/kernel_64bit/src" \
    -I"$repo_root/resources" \
    "$repo_root/tests/scheduler_test.cpp" \
    "$repo_root/kernel_64bit/src/system/scheduler/functions/scheduler.cpp" \
    "$repo_root/kernel_64bit/src/system/scheduler/pr_map/pr_map.cpp" \
    -o "$test_binary"

"$test_binary"
printf '[host-tests] scheduler regression tests passed\n'
