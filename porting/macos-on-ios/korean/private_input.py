#!/usr/bin/env python3
"""Prepare private recompilation inputs on a disposable build runner.

Never print download links or upload this script's output directory as an artifact.
The complete disc is not required for compilation: extract the supported XBE,
then reuse tools/pipeline.sh. Runtime installation still needs the full game data.
"""
import argparse
import contextlib
import hashlib
import os
from pathlib import Path
import re
import shutil
import sys
import time
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "xboxrecomp"))
from tools.xiso.xdvdfs import Xiso  # noqa: E402

XBE_SIZE = 4571136
XBE_FNV = 0x3602BA6A6079C8CD


def drive_id(url):
    parsed = urlsplit(url)
    if parsed.scheme != "https" or parsed.hostname != "drive.google.com":
        raise ValueError("NGB_DISC_URL must be an HTTPS Google Drive file link")
    match = re.fullmatch(r"/file/d/([A-Za-z0-9_-]+)(?:/view)?/?", parsed.path)
    value = match.group(1) if match else parse_qs(parsed.query).get("id", [""])[0]
    if not re.fullmatch(r"[A-Za-z0-9_-]+", value):
        raise ValueError("Google Drive link has no valid file ID")
    return value


def fnv1a(data):
    value = 0xCBF29CE484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def prepare(iso_path, destination, expected_sha256=None):
    digest = hashlib.sha256()
    with iso_path.open("rb") as source:
        for chunk in iter(lambda: source.read(4 << 20), b""):
            digest.update(chunk)
    actual = digest.hexdigest()
    if expected_sha256 and actual != expected_sha256.lower():
        raise ValueError("Disc SHA-256 differs from NGB_DISC_SHA256")
    with iso_path.open("rb") as source:
        raw_xbe = source.read(4) == b"XBEH"
    if raw_xbe:
        if iso_path.stat().st_size != XBE_SIZE:
            raise ValueError("Input executable has the wrong size")
        executable = iso_path.read_bytes()
    else:
        with Xiso(iso_path) as disc:
            entry = disc.find("default.xbe")
            if entry is None or entry.is_dir or entry.size != XBE_SIZE:
                raise ValueError("Disc does not contain the supported retail USA executable")
            executable = disc.read(entry)
    if len(executable) != XBE_SIZE or fnv1a(executable) != XBE_FNV:
        raise ValueError("Executable fingerprint differs from src/install.c")
    # Use our fixed filename, never an untrusted disc entry as an output path.
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "default.xbe").write_bytes(executable)
    return actual


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", "--input", dest="iso", type=Path, required=True)
    parser.add_argument("--download", action="store_true")
    args = parser.parse_args()
    stage = "validate download link"
    try:
        if args.download:
            file_id = drive_id(os.environ.get("NGB_DISC_URL", ""))
            # Leave space for the image and generated C; avoid extracting the full disc.
            stage = "check runner storage"
            free = shutil.disk_usage(args.iso.parent).free
            print("Runner free storage (GiB):", round(free / (1 << 30), 2), flush=True)
            if free < 10 * (1 << 30):
                raise ValueError("Runner needs at least 10 GiB free before download")
            stage = "import downloader"
            import gdown
            public_output = sys.stdout
            reported = [0]

            def progress(done, total):
                if done - reported[0] >= 512 * (1 << 20):
                    reported[0] = done
                    print("Downloaded disc MiB:", done // (1 << 20), file=public_output, flush=True)

            # Download diagnostics can contain the private URL. Keep them private.
            with (args.iso.parent / "download-private.log").open("w") as log:
                with contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
                    stage = "download disc from Drive"
                    for attempt in range(3):
                        try:
                            result = gdown.download(id=file_id, output=str(args.iso),
                                                    quiet=True, use_cookies=False,
                                                    progress=progress, timeout=(30, 120), retries=1)
                            break
                        except gdown.exceptions.FileURLRetrievalError:
                            if attempt == 2:
                                raise
                            time.sleep(30 * (attempt + 1))
            if result is None:
                raise ValueError("Drive download failed; check sharing or download quota")
            print("Downloaded private input bytes:", args.iso.stat().st_size, flush=True)
        stage = "validate disc and retail executable"
        digest = prepare(args.iso, ROOT / "game_files",
                         os.environ.get("NGB_DISC_SHA256") or None)
        print("Supported retail executable verified; private compilation input prepared.")
        print("Input SHA-256:", digest)
        if not os.environ.get("NGB_DISC_SHA256"):
            print("No independently supplied disc hash; this is a computed hash only.")
    except Exception as error:
        # Provider exceptions and XisoError can contain URLs or data paths.
        # Never print their text into the public Actions log.
        print("Private input preparation failed at stage:", stage, file=sys.stderr)
        print("Failure category:", type(error).__name__, file=sys.stderr)
        # Classify common provider failures without emitting their private URL.
        # gdown's outer message always mentions permissions. Its chained
        # provider error is more specific; never misreport boilerplate as proof.
        detail = str(error.__cause__ or error).lower()
        if "too many users" in detail or "quota" in detail:
            print("Drive reports a download quota/rate limit.", file=sys.stderr)
        elif isinstance(error, ValueError):
            pass
        elif "anyone with the link" in detail or "permission" in detail:
            print("Drive did not supply an anonymous download URL; the error alone does not establish a permission problem.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
