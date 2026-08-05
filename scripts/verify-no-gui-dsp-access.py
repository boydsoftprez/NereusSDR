#!/usr/bin/env python3
"""Fail if src/gui/ reaches into WdspEngine for an RX channel.

Phase 3F Sub-Epic J. The per-slice pipeline is SliceModel property ->
RadioModel push -> rxChannel(slice->sliceIndex()). Controls that call
rxChannel() from the GUI bypass it and, historically, hardcode channel 0,
which is how ANF on slice B ended up toggling slice A.

Allowlist entries are files whose rxChannel() use is engine-internal rather
than a control write.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUI = ROOT / "src" / "gui"
PATTERN = re.compile(r"rxChannel\s*\(")
ALLOWLIST = {
    # Meter driver: resolves per-slice channels for polling, not a control
    # write. Lives under src/gui/ for packaging reasons.
    "src/gui/meters/MeterPoller.cpp",
}

def main() -> int:
    failures = []
    for path in sorted(GUI.rglob("*.cpp")):
        rel = path.relative_to(ROOT).as_posix()
        if rel in ALLOWLIST:
            continue
        for num, line in enumerate(path.read_text().splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            if PATTERN.search(line):
                failures.append(f"{rel}:{num}: {stripped}")
    if failures:
        print("[gui-dsp-access] GUI code must not call rxChannel() directly.")
        print("Route through SliceModel; RadioModel pushes to the right channel.")
        for f in failures:
            print(f"  {f}")
        return 1
    print(f"[gui-dsp-access] OK: no direct rxChannel() use in src/gui/")
    return 0

if __name__ == "__main__":
    sys.exit(main())
