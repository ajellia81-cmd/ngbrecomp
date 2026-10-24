#!/usr/bin/env bash
# Build the original game target with real generated sources and the device SDK.
# Keep compiler output private; only sanitized error summaries are public.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tools=${1:?usage: bash ios/build_engine.sh IOS_TOOL_DIRECTORY PRIVATE_BUILD_DIRECTORY}
build=${2:?usage: bash ios/build_engine.sh IOS_TOOL_DIRECTORY PRIVATE_BUILD_DIRECTORY}
mkdir -p "$build"
build=$(cd "$build" && pwd)
export PKG_CONFIG_LIBDIR="$tools/vcpkg/installed/arm64-ios/lib/pkgconfig:$tools/vcpkg/installed/arm64-ios/share/pkgconfig"
if ! cmake -S "$root" -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$tools/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=arm64-ios -DVCPKG_OVERLAY_TRIPLETS="$root/ios/triplets" \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_SYSTEM_PROCESSOR=arm64 \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT=iphoneos \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE -DCMAKE_BUILD_TYPE=Release \
    -DNGB_MOLTENVK_ROOT="$tools/moltenvk/MoltenVK" \
    -DNGB_IOS_KOREAN=ON -DNGB_KOREAN_ASSET_DIR="${NGB_KOREAN_ASSET_DIR:?Verified Korean capsule required}" \
    -DNGB_NV2A_VK=ON > "$build/configure-private.log" 2>&1; then
    python3 "$root/ios/report_build_errors.py" "$build/configure-private.log"
    exit 1
fi
if ! cmake --build "$build" --target ninja_gaiden_black --parallel 2 \
    > "$build/compile-private.log" 2>&1; then
    python3 "$root/ios/report_build_errors.py" "$build/compile-private.log"
    exit 1
fi
test -f "$build/NinjaGaidenBlack.app/NinjaGaidenBlack"
test -f "$build/NinjaGaidenBlack.app/Frameworks/MoltenVK.framework/MoltenVK"
echo 'Real iPhoneOS game app bundle linked with MoltenVK; physical-device startup remains unverified.'
