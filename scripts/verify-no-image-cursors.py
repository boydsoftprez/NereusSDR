#!/usr/bin/env python3
"""Fail if src/ uses a cursor shape Qt draws from an image on macOS.

Operator's Mac, 2026-09-25 and 2026-09-26: the desktop window crashed with
EXC_BREAKPOINT in CGImageCreate under QCocoaCursor::createCursorData, reached
from ContainerWidget's title bar on hover. Qt 6.11.0 draws a few cursor
shapes from its own ICC-tagged PNGs on macOS (SizeAllCursor, WaitCursor, and
BusyCursor when no private native cursor exists), and QImage::toCGImage()
frees the image's colour space before CGImageCreate uses it. Native NSCursor
shapes (arrow, I-beam, hands, resize arrows, ...) never take that path.

The fix swapped the four-way move cursor for the native open / closed hand.
This check stops the image-path shapes coming back, along with custom
bitmap / pixmap cursors, which go through the same conversion.

A justified use carries `native-cursor-ok: <reason>` in a comment on the
same line.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

PATTERN = re.compile(
    r"\b(SizeAllCursor|WaitCursor|BusyCursor|BitmapCursor)\b"
    r"|QCursor\s*\(\s*(QPixmap|QBitmap)\b"
    r"|\bcursor\s*:\s*(move|all-scroll|wait|progress)\b"
)
ESCAPE = "native-cursor-ok:"
SUFFIXES = {".cpp", ".h", ".hpp", ".mm", ".qss", ".css", ".ui"}


def main() -> int:
    failures = []
    for path in sorted(SRC.rglob("*")):
        if not path.is_file() or path.suffix not in SUFFIXES:
            continue
        rel = path.relative_to(ROOT).as_posix()
        text = path.read_text(errors="replace")
        for num, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            if PATTERN.search(line) and ESCAPE not in line:
                failures.append(f"{rel}:{num}: {stripped}")
    if failures:
        print("verify-no-image-cursors: image-drawn cursor shape in src/:")
        for f in failures:
            print(f"  {f}")
        print("Qt 6.11.0 crashes drawing these on macOS. Use a native shape")
        print("(Qt::OpenHandCursor / Qt::ClosedHandCursor for move), or mark a")
        print("justified use with 'native-cursor-ok: <reason>' on the line.")
        return 1
    print("verify-no-image-cursors: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
