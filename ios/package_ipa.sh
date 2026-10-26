#!/usr/bin/env bash
# Package only the real game bundle. No ISO, XBE, lifted C or build log leaves it.
set -euo pipefail
app=${1:?usage: bash ios/package_ipa.sh GAME_APP OUTPUT_DIRECTORY IOS_TOOL_DIRECTORY}
out=${2:?usage: bash ios/package_ipa.sh GAME_APP OUTPUT_DIRECTORY IOS_TOOL_DIRECTORY}
tools=${3:?usage: bash ios/package_ipa.sh GAME_APP OUTPUT_DIRECTORY IOS_TOOL_DIRECTORY}
test -f "$app/NinjaGaidenBlack"
test -f "$app/Frameworks/MoltenVK.framework/MoltenVK"
plutil -lint "$app/Info.plist"
python3 - "$app/Info.plist" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'rb') as stream:
    metadata = plistlib.load(stream)
    icons = metadata.get('CFBundleIcons', {}).get('CFBundlePrimaryIcon', {})
from pathlib import Path
import re
revision = metadata.get('NGBSourceCommit', '')
version = metadata.get('CFBundleVersion', '')
if not re.fullmatch('[0-9a-f]{40}', revision) or not re.fullmatch(r'2\.[0-9]+', version):
    raise SystemExit('A unique build version and source revision are required')
binary = Path(sys.argv[1]).with_name('NinjaGaidenBlack').read_bytes()
if revision.encode() not in binary or version.encode() not in binary:
    raise SystemExit('App metadata does not match the linked runtime build identifiers')
print('Packaged runtime build:', version, revision)
if metadata.get('NGBBuildConfiguration') != 'Release':
    raise SystemExit('Only a verified Release configuration may be delivered')
if metadata.get('CFBundleDisplayName') != 'Ninja Gaiden Black' or metadata.get('CFBundleName') != 'Ninja Gaiden Black':
    raise SystemExit('The release app must be named Ninja Gaiden Black')
if metadata.get('NGBIOSPortDiagnosticsEnabled') is not False:
    raise SystemExit('Release delivery must disable the added iOS-port diagnostics')
print('Packaged configuration: Release; app name: Ninja Gaiden Black; iOS-port diagnostics: disabled; original diagnostics: preserved')
if icons.get('CFBundleIconName') != 'AppIcon' or not icons.get('CFBundleIconFiles'):
    raise SystemExit('The Ninja Gaiden Black iPhone icon catalog must be packaged')
print('Ninja Gaiden Black app icon metadata: OK')
PY
xcrun --sdk iphoneos vtool -show-build "$app/NinjaGaidenBlack" > "$app/../platform-private.log"
grep -Eq 'platform[[:space:]]+IOS([[:space:]]|$)' "$app/../platform-private.log"
xcrun --sdk iphoneos nm -g "$app/NinjaGaidenBlack" > "$app/../symbols-private.log"
python3 - "$app/../symbols-private.log" <<'PY'
import re, sys
from pathlib import Path
symbols = Path(sys.argv[1]).read_text()
count = len(re.findall(r'\bT _sub_[0-9A-Fa-f]{8}\b', symbols))
if count < 1000 or not re.search(r'\bT _SDL_main\b', symbols):
    raise SystemExit('The linked game engine and SDL UIKit entry point are required; refusing a harness IPA')
print('Linked lifted game functions:', count)
PY
if find "$app" -type f \( -iname '*.xbe' -o -iname '*.iso' -o -iname '*.afs' -o -iname 'recomp_*.c' \) | grep -q .; then
    echo 'Refusing to package raw game data or generated C.' >&2
    exit 1
fi
mkdir -p "$out/Payload"
out=$(cd "$out" && pwd)
ditto "$app" "$out/Payload/NinjaGaidenBlack.app"
bundle="$out/Payload/NinjaGaidenBlack.app"
for object in "$bundle/Frameworks/MoltenVK.framework" "$bundle"; do
    if codesign -d "$object" >/dev/null 2>&1; then codesign --remove-signature "$object"; fi
done
find "$bundle" -type d -name _CodeSignature -prune -exec rm -rf {} +
for notice in "$tools"/vcpkg/installed/arm64-ios/share/*/copyright; do
    if [ -f "$notice" ]; then
        port=$(basename "$(dirname "$notice")")
        cp "$notice" "$bundle/LICENSES/$port.txt"
    fi
done
python3 "$(dirname "$0")/clean_bundle.py" "$bundle"
(cd "$out" && zip -qry NinjaGaidenBlack-ios-arm64-unsigned.ipa Payload)
python3 - "$out/NinjaGaidenBlack-ios-arm64-unsigned.ipa" <<'PY'
from pathlib import Path
import hashlib, sys, zipfile
path = Path(sys.argv[1])
with zipfile.ZipFile(path) as archive:
    assert archive.testzip() is None
    port_files={'build-info.txt','compile-private.log','configure-private.log','platform-private.log','symbols-private.log'}
    assert not any(Path(name).name in port_files for name in archive.namelist())
    import plistlib
    metadata=plistlib.loads(archive.read('Payload/NinjaGaidenBlack.app/Info.plist'))
    executable=archive.read('Payload/NinjaGaidenBlack.app/NinjaGaidenBlack')
    assert metadata['NGBIOSPortDiagnosticsEnabled'] is False
    assert metadata['NGBSourceCommit'].encode() in executable and metadata['CFBundleVersion'].encode() in executable
    print('Final IPA iOS-port diagnostic-file audit: passed; original diagnostics preserved')
print('Unsigned game IPA bytes:', path.stat().st_size)
print('Unsigned game IPA SHA-256:', hashlib.sha256(path.read_bytes()).hexdigest())
print('No physical-device or LiveContainer startup is implied by packaging.')
PY
