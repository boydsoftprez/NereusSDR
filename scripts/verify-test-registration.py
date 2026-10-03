#!/usr/bin/env python3
"""Fail when tests/CMakeLists.txt touches a test that may not exist.

PR #327's first CI run (2026-09-27, run 36327470069): all four Linux shards
failed to configure. nereus_add_test() registers a test only in its own
shard (NEREUS_TEST_SHARDS / NEREUS_TEST_SHARD, hashed on the name) and in
every other shard makes a stub object library and returns without calling
add_test(). A set_tests_properties() written straight after it then names a
test that does not exist in three shards out of four, and CMake stops with
"set_tests_properties Can not find test to add properties to". A developer
configure is unsharded, so the mistake only shows in CI.

The rule this checks: every statement that names a test as a test
(set_tests_properties, set_property(TEST ...), get_test_property, and an
add_test whose COMMAND runs another test's executable) must be reachable
only when that test exists:

  * a test made by nereus_add_test() may be sharded away, so the statement
    must sit inside `if(TEST <name>)`;
  * a test made by a plain add_test() exists whenever its own enclosing
    if() branches were taken, so the statement must sit inside those same
    branches (or inside `if(TEST <name>)`);
  * a name no add_test() or nereus_add_test() in the file defines is an
    error on its own.

Names written through a variable (${name}) are not checked: they live in
the helper function itself or in loops over names checked where they were
listed.

Usage: python3 scripts/verify-test-registration.py [path/to/CMakeLists.txt]
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_FILE = ROOT / "tests" / "CMakeLists.txt"

COMMAND_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(")
ARG_RE = re.compile(r'"(?:[^"\\]|\\.)*"|[^\s()]+|\(|\)')


def strip_comments(text: str) -> str:
    """Blank CMake line comments, keeping quoted strings and line numbers."""
    out = []
    for line in text.split("\n"):
        buf = []
        in_quote = False
        i = 0
        while i < len(line):
            c = line[i]
            if in_quote:
                buf.append(c)
                if c == "\\" and i + 1 < len(line):
                    buf.append(line[i + 1])
                    i += 1
                elif c == '"':
                    in_quote = False
            elif c == '"':
                in_quote = True
                buf.append(c)
            elif c == "#":
                break
            else:
                buf.append(c)
            i += 1
        out.append("".join(buf))
    return "\n".join(out)


def commands(text: str):
    """Yield (line, name_lower, args) for each top-level command call."""
    pos = 0
    n = len(text)
    while pos < n:
        m = COMMAND_RE.search(text, pos)
        if not m:
            return
        # A command name must start a statement: only whitespace before it
        # on its line.
        line_start = text.rfind("\n", 0, m.start()) + 1
        if text[line_start:m.start()].strip():
            pos = m.end()
            continue
        depth = 1
        i = m.end()
        in_quote = False
        while i < n and depth:
            c = text[i]
            if in_quote:
                if c == "\\":
                    i += 1
                elif c == '"':
                    in_quote = False
            elif c == '"':
                in_quote = True
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            i += 1
        body = text[m.end():i - 1]
        args = [a for a in ARG_RE.findall(body) if a not in ("(", ")")]
        line = text.count("\n", 0, m.start()) + 1
        yield line, m.group(1).lower(), args, body
        pos = i


def literal(arg: str) -> str | None:
    """The test name an argument spells, or None when it is not literal."""
    arg = arg.strip('"')
    if "${" in arg or "$<" in arg or not arg:
        return None
    return arg


def guarded_by_test(cond_texts, name: str) -> bool:
    pat = re.compile(r"(?<![A-Za-z0-9_])TEST\s+\"?" + re.escape(name) + r"\"?(?![A-Za-z0-9_])")
    for text in cond_texts:
        if pat.search(text) and not re.search(r"\bNOT\s+TEST\s+\"?" + re.escape(name) + r"\b", text):
            return True
    return False


def check(path: pathlib.Path) -> list[str]:
    text = strip_comments(path.read_text(encoding="utf-8"))
    # Frame: [block_id, branch_index, condition_text]
    stack: list[list] = []
    block_counter = 0
    func_depth = 0
    defs: dict[str, tuple[bool, tuple, int]] = {}  # name -> (sharded, frames, line)
    refs: list[tuple[str, int, tuple, list[str], str]] = []

    for line, cmd, args, body in commands(text):
        if cmd in ("function", "macro"):
            func_depth += 1
            continue
        if cmd in ("endfunction", "endmacro"):
            func_depth -= 1
            continue
        if func_depth:
            continue
        if cmd == "if":
            block_counter += 1
            stack.append([block_counter, 0, body])
            continue
        if cmd in ("elseif", "else"):
            if stack:
                stack[-1][1] += 1
                stack[-1][2] = body if cmd == "elseif" else ""
            continue
        if cmd == "endif":
            if stack:
                stack.pop()
            continue
        if cmd in ("foreach", "while", "endforeach", "endwhile"):
            continue

        frames = tuple((f[0], f[1]) for f in stack)
        conds = [f[2] for f in stack]

        if cmd == "nereus_add_test" and args:
            name = literal(args[0])
            if name:
                defs[name] = (True, frames, line)
        elif cmd == "add_test" and args:
            if args[0] == "NAME" and len(args) > 1:
                name = literal(args[1])
                if name:
                    defs[name] = (False, frames, line)
                if "COMMAND" in args:
                    idx = args.index("COMMAND")
                    if idx + 1 < len(args):
                        target = literal(args[idx + 1])
                        if target:
                            refs.append((target, line, frames, conds, "add_test COMMAND"))
            else:
                name = literal(args[0])
                if name:
                    defs[name] = (False, frames, line)
        elif cmd == "set_tests_properties":
            for a in args:
                if a == "PROPERTIES":
                    break
                name = literal(a)
                if name:
                    refs.append((name, line, frames, conds, cmd))
        elif cmd == "get_test_property" and args:
            name = literal(args[0])
            if name:
                refs.append((name, line, frames, conds, cmd))
        elif cmd == "set_property" and args and args[0] == "TEST":
            for a in args[1:]:
                if a in ("APPEND", "APPEND_STRING", "PROPERTY", "DIRECTORY"):
                    break
                name = literal(a)
                if name:
                    refs.append((name, line, frames, conds, "set_property(TEST)"))

    errors = []
    rel = path.relative_to(ROOT) if path.is_relative_to(ROOT) else path
    for name, line, frames, conds, what in refs:
        if guarded_by_test(conds, name):
            continue
        d = defs.get(name)
        if d is None:
            if what == "add_test COMMAND":
                continue  # a plain executable or tool, not a test
            errors.append(f"{rel}:{line}: {what} names '{name}', which no "
                          f"add_test() or nereus_add_test() in this file defines")
            continue
        sharded, dframes, dline = d
        if what == "add_test COMMAND" and not sharded:
            continue  # a plain test's executable exists whenever it does
        if sharded:
            errors.append(f"{rel}:{line}: {what} names '{name}', made by "
                          f"nereus_add_test() at line {dline}, which a sharded "
                          f"configure (NEREUS_TEST_SHARDS) may skip; wrap it in "
                          f"if(TEST {name})")
            continue
        if frames[:len(dframes)] != dframes:
            errors.append(f"{rel}:{line}: {what} names '{name}', defined at "
                          f"line {dline} under different if() conditions; "
                          f"wrap it in if(TEST {name}) or move it beside "
                          f"its add_test()")
    return errors


def main(argv: list[str]) -> int:
    path = pathlib.Path(argv[1]) if len(argv) > 1 else DEFAULT_FILE
    errors = check(path.resolve())
    for e in errors:
        print(f"FAIL {e}")
    if errors:
        print(f"\n{len(errors)} test reference(s) not guarded by the test's "
              f"own registration")
        return 1
    print("verify-test-registration: every test reference is guarded")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
