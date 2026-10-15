#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/../.." && pwd)"
mode="${1:-mac}"
native_build="${SEGARALLY_NATIVE_BUILD:-$task_root/build/segarally-timed}"
if [[ "$mode" == "ios" ]]; then
    shift
    exec python3 "$task_root/scripts/segarally_ios.py" "$@"
elif [[ "$mode" == "mac" ]]; then
    build_dir="$task_root/build/segarally-mac"
    cmake -S "$task_root/platform/segarally" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
        -DSEGARALLY_NATIVE_BUILD="$native_build"
    cmake --build "$build_dir" -j 6
else
    echo "Usage: $0 mac | ios --native-build DIR --output build/NEW [options]" >&2
    exit 2
fi
