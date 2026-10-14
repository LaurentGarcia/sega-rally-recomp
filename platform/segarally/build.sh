#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/../.." && pwd)"
mode="${1:-mac}"
native_build="${SEGARALLY_NATIVE_BUILD:-$task_root/build/segarally-timed}"
if [[ "$mode" == "ios" ]]; then
    build_dir="$task_root/build/segarally-ios"
    cmake -S "$task_root/platform/segarally" -B "$build_dir" -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DSEGARALLY_NATIVE_BUILD="$native_build" \
        '-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG' '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG'
    xcodebuild -project "$build_dir/SegaRallyNative.xcodeproj" -scheme segarally_native \
        -configuration Release -destination "platform=iOS,id=${SEGARALLY_DEVICE:-00008110-0018696621D9401E}" \
        -jobs 6 -allowProvisioningUpdates build
elif [[ "$mode" == "mac" ]]; then
    build_dir="$task_root/build/segarally-mac"
    cmake -S "$task_root/platform/segarally" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
        -DSEGARALLY_NATIVE_BUILD="$native_build"
    cmake --build "$build_dir" -j 6
else
    echo "Usage: $0 [mac|ios]" >&2
    exit 2
fi
