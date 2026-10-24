#!/usr/bin/env bash
# Build open-source iPhoneOS dependencies on a disposable macOS runner.
# This script never reads the owner's disc or generated game code.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
toolroot=${1:?usage: bash ios/build_dependencies.sh TOOL_DIRECTORY}
revision=0c2a9ace1964b818b15f7ae50ba1c42404918a7b
export VCPKG_DISABLE_METRICS=1
export VCPKG_MAX_CONCURRENCY=2
mkdir -p "$toolroot"
toolroot=$(cd "$toolroot" && pwd)
if [ ! -d "$toolroot/vcpkg/.git" ]; then
    git init -q "$toolroot/vcpkg"
    git -C "$toolroot/vcpkg" remote add origin https://github.com/microsoft/vcpkg.git
fi
git -C "$toolroot/vcpkg" fetch --depth 1 origin "$revision"
git -C "$toolroot/vcpkg" checkout --detach FETCH_HEAD
bash "$toolroot/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
"$toolroot/vcpkg/vcpkg" install --triplet arm64-ios \
    'sdl2[vulkan]' glib glslang
bash "$root/xboxrecomp/third_party/fetch.sh"
echo 'iPhoneOS SDL2, GLib and glslang prepared. MoltenVK runtime packaging remains separate.'
