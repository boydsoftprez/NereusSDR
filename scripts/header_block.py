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

import re

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


# Pinned TCPIPcatServer.cs has a six-line author/inspiration notice, not a
# per-file copyright/GPL grant. Only these single-source derivatives use it;
# ordinary and multi-source ports retain every usual header requirement.
CAT_TCP_NOTICE_PATHS = {
    f"src/core/cat/{stem}{suffix}"
    for stem in ("CatTcpTransport", "CatStreamFramer", "CatSession")
    for suffix in (".h", ".cpp")
}
CAT_TCP_SOURCE = "Project Files/Source/Console/CAT/TCPIPcatServer.cs"
CAT_TCP_NOTICE = (
    "//=================================================================\n"
    "// MW0LGE 2022\n"
    "//=================================================================\n\n"
    "// inspiration from https://www.codeproject.com/Articles/5733/A-TCP-IP-Server-written-in-C\n"
    "//\n"
)
CAT_TCP_PROJECT_LICENSE = (
    "Upstream source has an author/inspiration notice; "
    "project-level GNU General Public License applies."
)


def uses_exact_tcp_notice(relative: str, head: str, source_cell: str | None) -> bool:
    if relative not in CAT_TCP_NOTICE_PATHS:
        return False
    sources = re.findall(r"^// Ported from Thetis (.+)$", head, re.MULTILINE)
    return (sources == [CAT_TCP_SOURCE]
            and (source_cell is None or source_cell == CAT_TCP_SOURCE)
            and head.lstrip("\ufeff").startswith(CAT_TCP_NOTICE))
