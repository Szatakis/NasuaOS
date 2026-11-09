#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
scheduler_binary="$(mktemp "${TMPDIR:-/tmp}/nasua-scheduler-test.XXXXXX")"
memory_binary="$(mktemp "${TMPDIR:-/tmp}/nasua-memory-test.XXXXXX")"
trap 'rm -f "$scheduler_binary" "$memory_binary"' EXIT

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
    -o "$scheduler_binary"

"$scheduler_binary"
printf '[host-tests] scheduler regression tests passed\n'

printf '[host-tests] compiling memory regression tests with %s\n' "${CXX:-c++}"
"${CXX:-c++}" \
    -std=gnu++20 \
    -O2 \
    -Wall \
    -Wextra \
    -Werror \
    -DHEAP_TEST \
    -I"$repo_root/kernel_64bit/src" \
    -I"$repo_root/kernel_64bit/limine-protocol/include" \
    -I"$repo_root/resources" \
    "$repo_root/tests/memory_test.cpp" \
    "$repo_root/resources/drivers/memory/functions/heap.cpp" \
    "$repo_root/resources/drivers/memory/functions/pmm.cpp" \
    -o "$memory_binary"

"$memory_binary"
printf '[host-tests] memory regression tests passed\n'
