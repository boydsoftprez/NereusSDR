# no-port-check: NereusSDR-original.
"""Message text and callsigns: cleaning a posted message, the base
callsign, mentions, and the link runs screens show (wire spec 4.5, 6.10).

Cleaning runs in the wire spec's order: Unicode NFC; CR LF and a lone CR
become LF; every other control character (category Cc) is removed; the
invisible and direction-changing format characters of 6.10 are removed;
leading and trailing white space is removed. U+200C and U+200D stay. The
cleaned text must then be 1 to 500 code points and at most 10 lines.

Nothing here logs or keeps the text.
"""

from __future__ import annotations

import re
import unicodedata
from typing import Iterable, List, Optional, Set

# Wire spec 9.1 defaults; the service passes the configured values.
CODE_POINTS = 500
LINES = 10

# Wire spec 6.10: the format characters cleaning removes. U+200C and U+200D
# (zero width non-joiner and joiner) are not here: emoji sequences and some
# scripts need them.
REMOVED_FORMAT = frozenset(
    [0x200B, 0x200E, 0x200F]
    + list(range(0x202A, 0x202E + 1))
    + [0x2060]
    + list(range(0x2066, 0x2069 + 1))
    + [0xFEFF]
)

# Wire spec 6.10: a word is a run of A-Z a-z 0-9 / (an optional leading @
# is outside the run, so it needs no rule of its own).
_WORD = re.compile(r"[A-Za-z0-9/]+")
_LINK_START = re.compile(r"https?://")


def clean_text(text: str) -> str:
    """The cleaning of 6.10, with no length rule: what the server stores
    when the result passes check()."""
    text = unicodedata.normalize("NFC", text)
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    out = []
    for c in text:
        if c != "\n" and unicodedata.category(c) == "Cc":
            continue
        if ord(c) in REMOVED_FORMAT:
            continue
        out.append(c)
    return "".join(out).strip()


def check(cleaned: str, code_points: int = CODE_POINTS, lines: int = LINES) -> Optional[str]:
    """None when cleaned text may be posted, else the refusal's code:
    `too-long` (over the code points or the lines) or `not-allowed`
    (nothing left)."""
    if not cleaned:
        return "not-allowed"
    if len(cleaned) > code_points or cleaned.count("\n") > lines - 1:
        return "too-long"
    return None


def clean(text: str, code_points: int = CODE_POINTS, lines: int = LINES) -> Optional[str]:
    """The cleaned text, or None when it is over the code points or the
    lines, or empty, after cleaning."""
    cleaned = clean_text(text)
    return None if check(cleaned, code_points, lines) else cleaned


def base_callsign(call: str) -> str:
    """Wire spec 4.5: the longest of the callsign's `/`-separated parts, the
    first of them on a tie, upper case (`W4XYZ` for `VE3/W4XYZ/P`)."""
    best = ""
    for part in call.upper().split("/"):
        if len(part) > len(best):
            best = part
    return best


def words(text: str) -> List[str]:
    """The base callsign of every word of the text, upper case, in order,
    each once."""
    seen: List[str] = []
    for m in _WORD.finditer(text):
        base = base_callsign(m.group(0))
        if base and base not in seen:
            seen.append(base)
    return seen


def mentions(text: str, callsigns: Iterable[str]) -> Set[str]:
    """The callsigns (as given) that the text mentions: a word with the same
    base callsign, ignoring case (6.10)."""
    bases = set(words(text))
    return {call for call in callsigns if base_callsign(call) in bases}


def links(text: str) -> List[str]:
    """The link runs a screen shows (6.10 "Formatting"): from `http://` or
    `https://` up to the next white space or the next format character
    (category Cf)."""
    out: List[str] = []
    i = 0
    while True:
        m = _LINK_START.search(text, i)
        if m is None:
            return out
        end = m.end()
        while end < len(text) and not text[end].isspace() and unicodedata.category(text[end]) != "Cf":
            end += 1
        out.append(text[m.start():end])
        i = end
