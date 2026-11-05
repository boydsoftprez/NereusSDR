#!/usr/bin/env python3
"""Fail if a signal's address crosses a Windows DLL on an unexported class.

No-port-check: NereusSDR-original check. 2026-11-05 J.J. Boyd (KG4VCF),
AI-assisted via Anthropic Claude Code.

On Windows NereusCore and NereusSDRLib are separate DLLs. `&Class::signal`
taken in another binary, when Class has no NEREUS_CORE_EXPORT or
NEREUS_GUI_EXPORT, is MinGW's auto-import thunk and not the address moc
compares. QSignalSpy then says "Null signal is not valid" and connect() says
"signal not found". The macros are empty on macOS and Linux, so only Windows
sees it, and Windows CI runs only its audio tests.

The class that must be exported is the one declaring the signal, so inherited
signals resolve to the base. A use in tests/ always crosses a DLL; a use in
src/gui/ crosses for a class in src/core/ or src/models/; a use in src/*.cpp
(the executables) crosses for any class. Text parsing only, no build.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXEMPT = {"PipeWireStream": "built on Linux only"}

CLASS = re.compile(
    r"^[ \t]*class[ \t]+(NEREUS_(?:GUI|CORE)_EXPORT[ \t]+)?(\w+)"
    r"(?:[ \t]+final)?\s*(:[^{;]*)?\{",
    re.M,
)
SIGNALS = re.compile(
    r"(?:^|\n)\s*(?:signals|Q_SIGNALS)\s*:(.*?)"
    r"(?=\n\s*(?:public|private|protected|signals|Q_SIGNALS)\b[^:\n]*:|\Z)",
    re.S,
)
ADDRESS = re.compile(r"&\s*(?:\w+::)*?(\w+)::(\w+)\b")
LOCAL = re.compile(r"\b(?:class|struct)\s+(\w+)[^;{]*\{")


def read(path):
    text = path.read_text(errors="replace")
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m[0].count("\n"), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def parse_classes():
    classes = {}
    for path in sorted((ROOT / "src").rglob("*.h")):
        text = read(path)
        rel = path.relative_to(ROOT).as_posix()
        for match in CLASS.finditer(text):
            end, depth = match.end(), 1
            while end < len(text) and depth:
                depth += {"{": 1, "}": -1}.get(text[end], 0)
                end += 1
            signals = set()
            for section in SIGNALS.finditer(text[match.end():end]):
                signals |= set(re.findall(r"\bvoid\s+(\w+)\s*\(", section[1]))
            classes.setdefault(match[2], {
                "exported": bool(match[1]),
                "file": rel,
                "signals": signals,
                "bases": re.findall(
                    r"(?:public|protected|private)\s+(?:\w+::)*(\w+)", match[3] or ""),
            })
    return classes


def declaring_class(classes, name, signal, seen=()):
    info = classes.get(name)
    if info is None or name in seen:
        return None
    if signal in info["signals"]:
        return name
    for base in info["bases"]:
        found = declaring_class(classes, base, signal, seen + (name,))
        if found:
            return found
    return None


def in_core(rel):
    return rel.startswith(("src/core/", "src/models/"))


def crosses(user, owner_file):
    if user.startswith(("tests/", "src/gui/")) or "/" not in user[len("src/"):]:
        return not (user.startswith("src/gui/") and not in_core(owner_file))
    return False


def main():
    classes = parse_classes()
    failures = []
    paths = [p for top in ("src", "tests") for p in sorted((ROOT / top).rglob("*"))
             if p.suffix in {".cpp", ".h", ".mm"} and p.is_file()]
    for path in paths:
        rel = path.relative_to(ROOT).as_posix()
        text = read(path)
        local = set(LOCAL.findall(text)) if rel.startswith("tests/") else set()
        for match in ADDRESS.finditer(text):
            if match[1] in local:
                continue
            owner = declaring_class(classes, match[1], match[2])
            if not owner or classes[owner]["exported"] or owner in EXEMPT:
                continue
            if crosses(rel, classes[owner]["file"]):
                line = text.count("\n", 0, match.start()) + 1
                failures.append(f"{rel}:{line}: {match[1]}::{match[2]}"
                                f" needs an export on {owner} ({classes[owner]['file']})")
    if failures:
        print("verify-dll-signal-exports: a signal's address crosses a Windows DLL:")
        for failure in failures:
            print(f"  {failure}")
        print("Add NEREUS_CORE_EXPORT or NEREUS_GUI_EXPORT to the class. See")
        print('"Windows DLL Boundaries" in CONTRIBUTING.md, including its')
        print("static constexpr rule, which an export can newly trigger.")
        return 1
    print("verify-dll-signal-exports: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
