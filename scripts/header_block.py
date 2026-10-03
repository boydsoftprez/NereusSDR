"""The header a source file's license check reads.

Shared by scripts/verify-thetis-headers.py (the merge gate) and
scripts/compliance-inventory.py. A file's header is its leading comment
block: every comment line, blank line and ``#pragma once`` at the top of
the file, up to the first line of code. The check reads the whole block
however long a file's modification history grows, and never less than
HEADER_WINDOW lines, so files that passed under the old fixed window
still pass.

NereusSDR-original. 2026-09-24, R-R3-47 fix wave (L2): the fixed
160-line window failed RadioModel.cpp once its history pushed the GPL
marker to line 160. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
Claude Code.
"""
from __future__ import annotations

HEADER_WINDOW = 160

_PY_SUFFIXES = {".py"}


def leading_comment_block_end(lines: list[str], suffix: str) -> int:
    """Index of the first line after the leading comment block."""
    in_block_comment = False
    for index, raw in enumerate(lines):
        line = raw.strip()
        if in_block_comment:
            if "*/" in line:
                in_block_comment = False
            continue
        if not line:
            continue
        if suffix in _PY_SUFFIXES:
            if line.startswith("#"):
                continue
            return index
        if line.startswith("//"):
            continue
        if line.startswith("/*"):
            if "*/" not in line[2:]:
                in_block_comment = True
            continue
        if line == "#pragma once":
            continue
        return index
    return len(lines)


def header_lines(text: str, suffix: str, window: int = HEADER_WINDOW) -> list[str]:
    """The lines a header check reads: the leading comment block, and at
    least the first ``window`` lines."""
    lines = text.splitlines()
    return lines[:max(window, leading_comment_block_end(lines, suffix))]


def header_text(text: str, suffix: str, window: int = HEADER_WINDOW) -> str:
    return "\n".join(header_lines(text, suffix, window))
