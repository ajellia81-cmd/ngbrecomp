#!/usr/bin/env bash
# Compile the real generated game translation units, not placeholder functions.
# Keep output and diagnostics private: they contain disc-derived material.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:?usage: bash ios/compile_generated.sh PRIVATE_OUTPUT_DIRECTORY}
mkdir -p "$out"
out=$(cd "$out" && pwd)
sdk=$(xcrun --sdk iphoneos --show-sdk-path)
shopt -s nullglob
sources=("$root"/src/recomp/gen/*.c)
if [ "${#sources[@]}" -eq 0 ] || [ ! -s "$root/src/recomp/gen/recomp_types.h" ]; then
    echo 'Real generated game sources and headers are required.' >&2
    exit 1
fi
index=0
for source in "${sources[@]}"; do
    index=$((index + 1))
    # One compiler at a time: lifted chunks use substantial compiler memory.
    if ! xcrun --sdk iphoneos clang -target arm64-apple-ios15.0 -isysroot "$sdk" \
        -std=gnu11 -O2 -fno-strict-aliasing -fwrapv \
        -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable \
        -Wno-parentheses-equality -Wno-tautological-compare \
        -Wno-incompatible-pointer-types-discards-qualifiers \
        -I"$root/src/recomp/gen" -I"$root/xboxrecomp/include" \
        -I"$root/xboxrecomp/src" -I"$root/xboxrecomp/src/kernel" \
        -c "$source" -o "$out/game-$index.o" > "$out/compiler-$index.log" 2>&1; then
        echo "Generated game compilation failed at translation unit $index/${#sources[@]}." >&2
        echo 'Raw compiler diagnostics remain private; no stubs were substituted.' >&2
        exit 1
    fi
    echo "Compiled generated game translation unit $index/${#sources[@]} for iPhoneOS ARM64."
done
xcrun --sdk iphoneos ar rcs "$out/libngb-generated.a" "$out"/game-*.o
echo 'All generated game C compiled. Archive still needs the handwritten engine, renderer and application link.'
