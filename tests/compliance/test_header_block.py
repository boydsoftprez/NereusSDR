"""Tests for scripts/header_block.py: the header the license checks read.

L2 (R-R3-47 fix wave): the checks read a fixed 160 lines, and
RadioModel.cpp's GPL marker sat on line 160, so one more history line
would fail the gate. The header is now the whole leading comment block.
"""
import importlib.util
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
_SPEC = importlib.util.spec_from_file_location(
    "header_block", REPO / "scripts" / "header_block.py")
header_block = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(header_block)


def _long_header(history_lines: int, marker: str) -> str:
    lines = ["// ====", "// src/models/Example.cpp  (NereusSDR)", "// ===="]
    lines += [f"//   2026-09-24  history line {i}" for i in range(history_lines)]
    lines += [f"// {marker}", "// ===="]
    lines += ["", '#include "Example.h"', "", "int f() { return 0; }"]
    lines += ["// a comment in the code, far below"] * 300
    return "\n".join(lines)


def test_a_long_header_passes():
    text = _long_header(400, "General Public License")
    head = header_block.header_text(text, ".cpp")
    assert "General Public License" in head
    assert '#include "Example.h"' not in head


def test_the_old_window_still_counts_for_short_headers():
    # A marker that the old fixed window found stays found.
    text = "\n".join(["// short header", "#include <x>"] + ["int a;"] * 50
                     + ["// Copyright (C) somewhere in the first 160 lines"])
    assert "Copyright (C)" in header_block.header_text(text, ".cpp")


def test_block_comments_and_pragma_once_are_part_of_the_header():
    text = "\n".join(["#pragma once", "/*", " * Copyright (C) x"]
                     + [" * line"] * 300
                     + [" * General Public License", " */", "", "namespace x {}"])
    head = header_block.header_text(text, ".h")
    assert "General Public License" in head


def test_code_after_the_header_is_not_read_past_the_window():
    text = "\n".join(["// header", "int x;"] + ["int y;"] * 300
                     + ["// General Public License"])
    assert "General Public License" not in header_block.header_text(text, ".cpp")


def test_python_headers():
    text = "\n".join(["#!/usr/bin/env python3"] + ["# line"] * 300
                     + ["# General Public License", "import os"])
    assert "General Public License" in header_block.header_text(text, ".py")


def test_the_gate_passes_the_tree():
    # The merge gate and the inventory both pass with the shared rule.
    for cmd in (["python3", "scripts/verify-thetis-headers.py"],
                ["python3", "scripts/compliance-inventory.py", "--fail-on-unclassified"]):
        result = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
