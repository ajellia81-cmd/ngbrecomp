#!/usr/bin/env bash
# Public source only; no game files, recipes or device traces.
set -euo pipefail
if [ "$#" -ne 2 ]; then echo 'Usage: build_moltenvk.sh SCRATCH_SOURCE OUTPUT_DIRECTORY' >&2; exit 2; fi
source_dir="$1"
output_dir="$2"
script_dir="$(cd "$(dirname "$0")" && pwd)"
revision=db66022459ffb663aa2b50f6b018bc2e124f5edf
if [ -e "$source_dir" ]; then echo 'Source output must be a new scratch path' >&2; exit 2; fi
git clone --depth 1 --branch v1.4.2 https://github.com/KhronosGroup/MoltenVK.git "$source_dir"
test "$(git -C "$source_dir" rev-parse HEAD)" = "$revision"
python3 "$script_dir/patch_moltenvk.py" "$source_dir"
cmake -S "$source_dir" -B "$source_dir/build-ngb" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DMVK_CONFIG_LOG_LEVEL=warn \
  -DCPM_SOURCE_CACHE="$source_dir/dependencies" \
  -DMVK_USE_METAL_PRIVATE_API=OFF -DMVK_EXCLUDE_CEREAL=OFF \
  -DMVK_BUILD_SHADER_CONVERTER_TOOL=OFF -DMVK_EXCLUDE_SPIRV_TOOLS=ON
cmake --build "$source_dir/build-ngb" --target MoltenVK -j 3
mkdir -p "$output_dir"
cp -L "$source_dir/build-ngb/MoltenVK/libMoltenVK.dylib" "$output_dir/libMoltenVK.dylib"
install_name_tool -id '@rpath/libMoltenVK.dylib' "$output_dir/libMoltenVK.dylib"
codesign --force --sign - "$output_dir/libMoltenVK.dylib"
cp "$source_dir/LICENSE" "$output_dir/MoltenVK-LICENSE.txt"
cp "$script_dir/MOLTENVK-NOTICE.txt" "$output_dir/MoltenVK-NOTICE.txt"
for dependency in "$source_dir"/build-ngb/_deps/*-src; do
  for license_name in LICENSE LICENSE.txt LICENSE.md; do
    if [ -f "$dependency/$license_name" ]; then
      cp "$dependency/$license_name" "$output_dir/$(basename "$dependency")-$license_name.txt"
    fi
  done
done
shasum -a 256 "$output_dir/libMoltenVK.dylib"

python3 - "$source_dir/dependencies" "$output_dir" <<'PYLICENSE'
import hashlib,sys
from pathlib import Path
for source in Path(sys.argv[1]).rglob('*'):
    if source.is_file() and source.name.lower() in ('license','license.txt','license.md','copying'):
        data=source.read_bytes()
        (Path(sys.argv[2])/('Dependency-LICENSE-'+hashlib.sha256(data).hexdigest()[:12]+'.txt')).write_bytes(data)
PYLICENSE
