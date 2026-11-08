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
signals resolve to the base. Each file's binary comes from CMakeLists.txt's
source lists, not its folder: src/core/mmio/ is in NereusSDRLib, not NereusCore.
A use crosses when it is in another binary than the class. tests/ and the
executables are always another binary. A header with no listed source of its
own falls back to its folder.

A macro must also match its class's binary: inside its own DLL the other DLL's
macro means dllimport, and moc's staticMetaObject then does not compile.
Text parsing only, no build.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXEMPT = {"PipeWireStream": "built on Linux only"}

CLASS = re.compile(
    r"^[ \t]*(?:class|struct)[ \t]+(NEREUS_(?:GUI|CORE)_EXPORT[ \t]+)?(\w+)"
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


# Comments and literals, so that a brace or a "&Foo::bar" in one means nothing.
NOISE = re.compile(
    r"""/\*.*?\*/|//[^\n]*|R"(\w*)\(.*?\)\1"|"(?:\\.|[^"\\\n])*"|(?<![0-9a-fA-F])'(?:\\.|[^'\\\n])*'""",
    re.S,
)
CORE, GUI = "NereusCore", "NereusSDRLib"
MACRO = {CORE: "NEREUS_CORE_EXPORT", GUI: "NEREUS_GUI_EXPORT"}
LIST = re.compile(r"\b(?:set|list)\s*\(\s*(?:APPEND\s+)?(\w+)\s([^()]*)\)")


def read(path):
    text = path.read_text(errors="replace")
    return NOISE.sub(lambda m: "\n" * m[0].count("\n"), text)


def source_lists():
    """Maps each source in CMakeLists.txt's library lists to its library."""
    text = re.sub(r"#[^\n]*", "", (ROOT / "CMakeLists.txt").read_text())
    lists = {}
    for match in LIST.finditer(text):
        lists.setdefault(match[1], []).extend(match[2].split())

    def expand(name, seen=()):
        for item in lists.get(name, []):
            inner = re.fullmatch(r"\$\{(\w+)\}", item)
            if inner and inner[1] not in seen:
                yield from expand(inner[1], seen + (name,))
            elif item.startswith("src/"):
                yield item

    return {source: library
            for names, library in ((("CORE_SOURCES", "MODEL_SOURCES"), CORE),
                                   (("GUI_SOURCES",), GUI))
            for name in names for source in expand(name)}


def binary_of(rel, sources):
    """The library holding `rel`; a test or an executable names itself."""
    if rel.startswith("tests/") or "/" not in rel[len("src/"):]:
        return rel
    stem = rel.rsplit(".", 1)[0]
    for candidate in (rel, stem + ".cpp", stem + ".mm", stem + ".h"):
        if candidate in sources:
            return sources[candidate]
    return CORE if rel.startswith(("src/core/", "src/models/")) else GUI


def is_listed(rel, sources):
    stem = rel.rsplit(".", 1)[0]
    return any(c in sources for c in (rel, stem + ".cpp", stem + ".mm"))


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
            if match[2] in classes and (classes[match[2]]["signals"] or not signals):
                continue
            classes[match[2]] = {
                "macro": (match[1] or "").strip(),
                "file": rel,
                "signals": signals,
                "bases": re.findall(
                    r"(?:^|,)\s*(?:(?:public|protected|private|virtual)\s+)*(?:\w+::)*(\w+)",
                    (match[3] or "").lstrip(":")),
            }
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


def main():
    classes = parse_classes()
    sources = source_lists()
    failures = []
    for name, info in sorted(classes.items()):
        owner_binary = binary_of(info["file"], sources)
        if (info["macro"] and info["macro"] != MACRO[owner_binary]
                and is_listed(info["file"], sources)):
            failures.append(f"{info['file']}: {name} has {info['macro']} but is built into"
                            f" {owner_binary}, where that means dllimport; use"
                            f" {MACRO[owner_binary]} or none")
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
            if not owner or classes[owner]["macro"] or owner in EXEMPT:
                continue
            owner_binary = binary_of(classes[owner]["file"], sources)
            if binary_of(rel, sources) != owner_binary:
                line = text.count("\n", 0, match.start()) + 1
                failures.append(f"{rel}:{line}: {match[1]}::{match[2]} needs"
                                f" {MACRO[owner_binary]} on {owner} ({classes[owner]['file']},"
                                f" built into {owner_binary})")
    if failures:
        print("verify-dll-signal-exports: a signal's address crosses a Windows DLL,"
              " or an export macro names the wrong DLL:")
        for failure in failures:
            print(f"  {failure}")
        print('See "Windows DLL Boundaries" in CONTRIBUTING.md, including its')
        print("static constexpr rule, which an export can newly trigger.")
        return 1
    print("verify-dll-signal-exports: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
