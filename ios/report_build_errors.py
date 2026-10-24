"""Emit compiler error locations, never lifted code snippets or private links."""
from pathlib import Path
import re
import sys

def summaries(text):
    for line in text.splitlines():
        line = re.sub(r"\x1b\[[0-9;]*m", "", line)
        if "://" in line:
            continue
        if "src/recomp/gen/" in line:
            if "error:" in line:
                yield "Generated game C error (private diagnostic withheld)."
            continue
        if re.search(r":\d+:\d+: (?:fatal )?error:", line):
            # Discard source context lines and redact quoted expressions/values.
            yield re.sub(r"'[^']*'|\"[^\"]*\"", "<symbol>", line)[:300]
        elif re.fullmatch(r'\s*"_[A-Za-z][A-Za-z0-9_]*", referenced from:', line) and '"_sub_' not in line:
            yield line.strip()  # Public host API names only; no source snippets.
        elif line.startswith(("CMake Error", "Undefined symbols for architecture", "ld: ", "clang: error:")):
            yield re.sub(r"'[^']*'|\"[^\"]*\"", "<symbol>", line)[:300]

if __name__ == "__main__":
    errors = list(summaries(Path(sys.argv[1]).read_text(errors="replace")))
    print("Private engine build failed. Sanitized error summary:")
    print("\n".join(errors[:30]) if errors else "No safely reportable compiler error line; inspect the build stage.")
