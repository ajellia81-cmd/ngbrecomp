#!/usr/bin/env bash
# Regenerate src/recomp/gen from the disc.
#
#   tools/pipeline.sh [path/to/disc.iso]
#
# The disc (or the .7z it came in) is only read; the extracted files go to
# game_files/ and the toolkit's analysis output stays in xboxrecomp/ (ignored),
# where its tools expect it. Needs python3 with capstone: the first run makes
# a venv in xboxrecomp/ for that.
set -euo pipefail

here=$(cd "$(dirname "$0")/.." && pwd)
toolkit=${XBOXRECOMP_DIR:-$here/xboxrecomp}
toolkit=$(cd "$toolkit" && pwd)
iso=${1:-$here/disc/ngb.xiso}
files=$here/game_files
xbe=${NGB_RECOMP_XBE:-$files/default.xbe}
gen_dir=${NGB_RECOMP_GEN_DIR:-$here/src/recomp/gen}

if [ ! -x "$toolkit/.venv/bin/python" ]; then
    python3 -m venv "$toolkit/.venv"
    "$toolkit/.venv/bin/pip" -q install capstone
fi
py=$toolkit/.venv/bin/python
cd "$toolkit"

if [ ! -f "$xbe" ]; then
    echo "== unpacking $iso"
    "$py" -m tools.xiso unpack "$iso" -o "$files"
fi

# The analysis JSON stays out of game_files: the title enumerates its own
# root directory at boot, and a stray file there is one it never shipped with.
analysis=${NGB_RECOMP_ANALYSIS:-$here/build/analysis/default_analysis.json}
mkdir -p "$(dirname "$analysis")"
echo "== parse";    "$py" -m tools.xbe_parser "$xbe" --json "$analysis" >/dev/null
# Indirect-branch targets measured by earlier runs (tools/feedback.sh) seed
# function detection; the database is cumulative, so each pass reaches further.
seeds=()
# The database lives in this repo (src/icall_targets.json), next to the code
# it seeds; feedback.sh merges new targets into it.
db=$here/src/icall_targets.json
[ -f "$db" ] && seeds=(--seed-functions "$db")
[ -n "${NGB_RECOMP_EXTRA_SEEDS:-}" ] && seeds+=(--seed-functions "$NGB_RECOMP_EXTRA_SEEDS")
echo "== disasm";   "$py" -m tools.disasm "$xbe" --force --analysis-json "$analysis" "${seeds[@]}" >/dev/null
echo "== func_id";  "$py" -m tools.func_id "$xbe" >/dev/null
echo "== abi";      "$py" -m tools.abi_analysis "$xbe" >/dev/null
echo "== recomp"
# --exclude-manual: functions defined by hand in recomp_manual.c are not
# generated, so the hand-written body is the one that links;
# src/manual_functions.json lists those defined elsewhere (src/fibers.c).
"$py" -m tools.recomp "$xbe" --all --split 250 \
    --game-name "Ninja Gaiden Black" \
    --exclude-manual "$here/src/recomp_manual.c" \
    --manual-functions "${NGB_RECOMP_MANUAL:-$here/src/manual_functions.json}" \
    --gen-dir "$gen_dir" > "$here/recomp_log.txt" 2>&1 \
    || { tail -20 "$here/recomp_log.txt"; exit 1; }
tail -3 "$here/recomp_log.txt"
