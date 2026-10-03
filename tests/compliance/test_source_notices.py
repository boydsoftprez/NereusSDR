"""Tests for scripts/collect-source-notices.py (R-R3-50).

These use small C sources written into tmp_path, then run the presets that
need only files in this repository against the committed *-notices.txt
files, so a vendor update that changes a notice fails here until the file
is regenerated.
"""
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parent.parent.parent
SOURCE_SCRIPT = REPO / "scripts" / "collect-source-notices.py"


def _load(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


sn = _load(SOURCE_SCRIPT, "collect_source_notices")

BSD_CONDITIONS = """\
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
"""

LICENCE_TEXT = """\
Copyright (c) 2020, Example Org

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
"""


# --------------------------------------------------------------------------
# Notice blocks

def test_block_runs_from_the_copyright_line_across_adjacent_comments():
    text = ("/* foo.c: a file */\n"
            "/* Copyright (c) 2022 Amazon\n   Written by Someone */\n"
            "/*\n" + BSD_CONDITIONS + "*/\n\n#include \"foo.h\"\n")
    blocks = sn.extract_blocks(text)
    assert len(blocks) == 1
    assert blocks[0].startswith("/* Copyright (c) 2022 Amazon\n")
    assert blocks[0].endswith("are met:\n*/")
    assert "#include" not in blocks[0]


def test_block_keeps_bytes_and_crlf(tmp_path):
    src = tmp_path / "crlf.h"
    src.write_bytes(b"/*\r\n * Copyright (c) 2013 Caf\xc3\xa9 Ltd\r\n */\r\nint x;\r\n")
    blocks = sn.extract_blocks(sn.read_source(src))
    assert blocks == [" * Copyright (c) 2013 Café Ltd\r\n */"]


def test_licence_conditions_alone_are_not_a_notice():
    text = "/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS */\nint x;\n"
    assert sn.extract_blocks(text) == []


def test_comment_opener_inside_a_string_is_ignored():
    text = 'const char *s = "/* Copyright (c) 1999 Nobody";\nint y;\n'
    assert sn.extract_blocks(text) == []


def test_nereussdr_modification_history_is_left_out():
    # The WDSP layout: upstream header, the upstream dual-licensing
    # statement right under it, a NereusSDR modification history after a
    # blank line, then another upstream header straight after that.
    upstream = ("/*  cfcomp.c\n\nCopyright (C) 2017, 2021 Warren Pratt, NR0V\n\n"
                "This program is free software; you can redistribute it.\n*/\n"
                "//\n"
                "// Dual-Licensing Statement (Richard Samphire MW0LGE)\n"
                "// reserves the right to license his code under other terms.\n")
    history = ("\n"
               "//\n"
               "// =============================================================\n"
               "// Modification history (NereusSDR):\n"
               "//   2026-04-30 - Synced from Thetis. J.J. Boyd (KG4VCF).\n"
               "// =============================================================\n")
    second = "/* Copyright (C) 2025 Warren Pratt, NR0V */\n"
    text = upstream + history + second + "\n#include \"comm.h\"\n"
    blocks = sn.extract_blocks(text)
    assert blocks == [
        "Copyright (C) 2017, 2021 Warren Pratt, NR0V\n\n"
        "This program is free software; you can redistribute it.\n*/\n"
        "//\n"
        "// Dual-Licensing Statement (Richard Samphire MW0LGE)\n"
        "// reserves the right to license his code under other terms.",
        "/* Copyright (C) 2025 Warren Pratt, NR0V */",
    ]
    assert not any("NereusSDR" in b or "KG4VCF" in b for b in blocks)


def test_nereussdr_original_file_header_is_left_out():
    text = ("/*\n * Copyright (C) 2026 J.J. Boyd, KG4VCF (NereusSDR-original glue)\n"
            " * GPL-3.0-or-later.\n */\nint x;\n")
    assert sn.extract_blocks(text) == []


# --------------------------------------------------------------------------
# Carried by the licence text or not

def test_block_matching_the_licence_text_is_carried():
    block = "/* Copyright (c) 2020 Example Org\n" + BSD_CONDITIONS + "*/"
    assert sn.is_carried(block, [LICENCE_TEXT])


def test_pointer_to_the_licence_file_is_carried():
    block = " * Copyright (c) 2020, Example Org\n * See the LICENSE file.\n */"
    assert sn.is_carried(block, [LICENCE_TEXT])


def test_other_holder_is_not_carried():
    block = "/* Copyright (c) 2020 Another Org\n" + BSD_CONDITIONS + "*/"
    assert not sn.is_carried(block, [LICENCE_TEXT])


def test_other_years_are_not_carried():
    block = "/* Copyright (c) 2018-2020 Example Org */"
    assert not sn.is_carried(block, [LICENCE_TEXT])


def test_continuation_holder_line_is_checked():
    block = "/* Copyright (c) 2020 Example Org\n                 2021 Other Person */"
    assert not sn.is_carried(block, [LICENCE_TEXT])


def test_extra_licence_terms_are_not_carried():
    block = (" * Copyright (c) 2020 Example Org\n"
             " * Permission is also granted to do something else.\n */")
    assert not sn.is_carried(block, [LICENCE_TEXT])


# --------------------------------------------------------------------------
# Collecting over files

def test_same_notice_in_two_layouts_is_listed_once(tmp_path):
    a = tmp_path / "a.c"
    b = tmp_path / "b.c"
    a.write_text("/* Copyright (c) 2021 Other Org\n   All rights reserved. */\n")
    b.write_text("/*\n * Copyright (c) 2021 Other Org\n * All rights reserved.\n */\n")
    c = tmp_path / "c.c"
    c.write_text("/* Copyright (c) 2020 Example Org */\n")
    notices, counts = sn.collect([(a, "a.c"), (b, "b.c"), (c, "c.c")], [LICENCE_TEXT])
    assert len(notices) == 1
    assert notices[0].files == ["a.c", "b.c"]
    assert notices[0].text == "/* Copyright (c) 2021 Other Org\n   All rights reserved. */"
    assert counts["distinct"] == 2 and counts["carried"] == 1


def test_includes_are_followed_within_the_tree(tmp_path):
    (tmp_path / "inc").mkdir()
    main = tmp_path / "main.c"
    main.write_text('#include "local.h"\n#include <inc_only.h>\n#include <stdio.h>\n')
    (tmp_path / "local.h").write_text('#include "inc/deeper.h"\n')
    (tmp_path / "inc" / "deeper.h").write_text("int d;\n")
    (tmp_path / "inc" / "inc_only.h").write_text("int i;\n")
    files = sn.resolve_includes([main], [tmp_path / "inc"])
    names = sorted(p.relative_to(tmp_path.resolve()).as_posix() for p in files)
    assert names == ["inc/deeper.h", "inc/inc_only.h", "local.h", "main.c"]


# --------------------------------------------------------------------------
# The committed files match what the script writes

@pytest.mark.parametrize("library", ["fftw3", "rade", "r8brain", "wdsp"])
def test_committed_notices_are_current(library):
    target = REPO / "packaging" / "third-party-licenses" / f"{library}-notices.txt"
    result = subprocess.run(
        [sys.executable, str(SOURCE_SCRIPT), library, "--check", str(target)],
        capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def test_rnnoise_notices_are_current_when_fetched():
    build = REPO / "build-licences"
    if not (build / "_deps" / "rnnoise_upstream-src").is_dir():
        pytest.skip("no configured build with rnnoise fetched")
    target = REPO / "packaging" / "third-party-licenses" / "rnnoise-notices.txt"
    result = subprocess.run(
        [sys.executable, str(SOURCE_SCRIPT), "rnnoise", "--build-dir", str(build),
         "--check", str(target)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def test_check_fails_on_a_stale_file(tmp_path):
    stale = tmp_path / "rade-notices.txt"
    stale.write_text("out of date\n")
    result = subprocess.run(
        [sys.executable, str(SOURCE_SCRIPT), "rade", "--check", str(stale)],
        capture_output=True, text=True)
    assert result.returncode == 1
    assert "out of date" in result.stderr


def test_every_listed_notice_is_verbatim_in_its_files():
    source_set, notices, _ = sn.build("rade", REPO, None)
    assert notices
    for notice in notices:
        for name in notice.files:
            text = sn.read_source(source_set.base / name)
            assert notice.text in text or sn.normalise(notice.text) in sn.normalise(text)
        assert notice.text in sn.read_source(source_set.base / notice.files[0])


# --------------------------------------------------------------------------
# Fetched libraries: every platform's sources from the CMake lists, so the
# output does not depend on which platform's tree is given

def test_cmake_values_reads_set_and_list_append():
    text = """
SET(PA_PLATFORM_SOURCES src/os/win/a.c src/os/win/b.c) # comment src/x.c
set(PA_PLATFORM_SOURCES ${PA_PLATFORM_SOURCES} src/os/win/c.c)
list(APPEND usrsctp_sources
\tnetinet/d.c
)
set(LIBJUICE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/e.c)
unset(PA_PLATFORM_SOURCES)
"""
    assert sn.cmake_values(text, "PA_PLATFORM_SOURCES") == [
        "src/os/win/a.c", "src/os/win/b.c", "src/os/win/c.c"]
    assert sn.cmake_values(text, "usrsctp_sources") == ["netinet/d.c"]
    assert sn.cmake_values(text, "LIBJUICE_SOURCES") == ["${CMAKE_CURRENT_SOURCE_DIR}/src/e.c"]


def test_cmake_keep_branch_takes_the_option_and_keeps_platform_branches():
    text = """
if(WIN32)
  list(APPEND S win.c)
elseif(APPLE)
  list(APPEND S mac.c)
else()
  list(APPEND S unix.c)
endif()
if(ENABLE_OPENSSL)
  list(APPEND S ossl.c)
elseif(ENABLE_MBEDTLS)
  list(APPEND S mbedtls.c)
else()
  if(NESTED)
    list(APPEND S nested.c)
  endif()
  list(APPEND S builtin.c)
endif()
"""
    kept = sn.cmake_keep_branch(text, "ENABLE_OPENSSL")
    assert sn.cmake_values(kept, "S") == ["win.c", "mac.c", "unix.c", "ossl.c"]
    assert sn.cmake_values(sn.cmake_keep_branch(text, "NOTHING"), "S") == [
        "win.c", "mac.c", "unix.c", "ossl.c", "mbedtls.c", "nested.c", "builtin.c"]


def _notice(holder: str) -> str:
    return f"/*\n * Copyright (c) 2001 {holder}\n */\n"


PORTAUDIO_CMAKE = """\
SET(PA_COMMON_SOURCES src/common/pa_front.c)
SET(PA_SKELETON_SOURCES src/hostapi/skeleton/pa_hostapi_skeleton.c)
IF(WIN32)
  SET(PA_PLATFORM_SOURCES src/os/win/pa_win_hostapis.c)
  IF(MSVC)
    SET(PA_PLATFORM_SOURCES ${PA_PLATFORM_SOURCES} src/os/win/pa_x86_plain_converters.c)
  ENDIF()
  IF(PA_USE_ASIO)
    SET(PA_ASIO_SOURCES src/hostapi/asio/pa_asio.cpp)
  ENDIF()
  SET(PA_DS_SOURCES src/hostapi/dsound/pa_win_ds.c)
  SET(PA_WMME_SOURCES src/hostapi/wmme/pa_win_wmme.c)
  SET(PA_WASAPI_SOURCES src/hostapi/wasapi/pa_win_wasapi.c)
  SET(PA_WDMKS_SOURCES src/hostapi/wdmks/pa_win_wdmks.c)
ELSE()
  SET(PA_PLATFORM_SOURCES src/os/unix/pa_unix_util.c)
  IF(APPLE)
    SET(PA_COREAUDIO_SOURCES src/hostapi/coreaudio/pa_mac_core.c)
  ELSEIF(UNIX)
    SET(PA_JACK_SOURCES src/hostapi/jack/pa_jack.c)
    SET(PA_ALSA_SOURCES src/hostapi/alsa/pa_linux_alsa.c)
  ENDIF()
ENDIF()
"""

PORTAUDIO_FILES = {
    "src/common/pa_front.c": '#include "pa_util.h"\n' + _notice("Common Holder"),
    "src/common/pa_util.h": _notice("Header Holder"),
    "src/hostapi/skeleton/pa_hostapi_skeleton.c": _notice("Skeleton Holder"),
    "src/os/win/pa_win_hostapis.c": _notice("Windows Holder"),
    "src/os/win/pa_x86_plain_converters.c": _notice("Converter Holder"),
    "src/hostapi/asio/pa_asio.cpp": _notice("Asio Holder"),
    "src/hostapi/dsound/pa_win_ds.c": _notice("DirectSound Holder"),
    "src/hostapi/wmme/pa_win_wmme.c": _notice("Mme Holder"),
    "src/hostapi/wasapi/pa_win_wasapi.c": _notice("Wasapi Holder"),
    "src/hostapi/wdmks/pa_win_wdmks.c": _notice("Wdmks Holder"),
    "src/os/unix/pa_unix_util.c": _notice("Unix Holder"),
    "src/hostapi/coreaudio/pa_mac_core.c": _notice("CoreAudio Holder"),
    "src/hostapi/jack/pa_jack.c": _notice("Jack Holder"),
    "src/hostapi/alsa/pa_linux_alsa.c": _notice("Alsa Holder"),
}

DATACHANNEL_FILES = {
    "CMakeLists.txt": (
        "set(LIBDATACHANNEL_SOURCES\n\t${CMAKE_CURRENT_SOURCE_DIR}/src/global.cpp\n)\n"
        "set(LIBDATACHANNEL_IMPL_SOURCES\n\t${CMAKE_CURRENT_SOURCE_DIR}/src/impl/sctp.cpp\n)\n"),
    "src/global.cpp": _notice("Datachannel Holder"),
    "src/impl/sctp.cpp": '#include "usrsctp.h"\n' + _notice("Impl Holder"),
    "deps/libjuice/CMakeLists.txt":
        "set(LIBJUICE_SOURCES\n\t${CMAKE_CURRENT_SOURCE_DIR}/src/agent.c\n)\n",
    "deps/libjuice/src/agent.c": _notice("Juice Holder"),
    "deps/usrsctp/usrsctplib/CMakeLists.txt": "list(APPEND usrsctp_sources\n\tuser_socket.c\n)\n",
    "deps/usrsctp/usrsctplib/user_socket.c": _notice("Sctp Holder"),
    "deps/usrsctp/usrsctplib/usrsctp.h": _notice("Public Header Holder"),
    "deps/libsrtp/CMakeLists.txt": (
        "set(SOURCES_C srtp/srtp.c)\nset(CIPHERS_SOURCES_C crypto/cipher/cipher.c)\n"
        "if(ENABLE_OPENSSL)\n  list(APPEND CIPHERS_SOURCES_C crypto/cipher/aes_icm_ossl.c)\n"
        "elseif(ENABLE_MBEDTLS)\n  list(APPEND CIPHERS_SOURCES_C crypto/cipher/aes_icm_mbedtls.c)\n"
        "else()\n  list(APPEND CIPHERS_SOURCES_C crypto/cipher/aes.c)\nendif()\n"
        "set(HASHES_SOURCES_C crypto/hash/auth.c)\nset(KERNEL_SOURCES_C crypto/kernel/key.c)\n"
        "set(MATH_SOURCES_C crypto/math/datatypes.c)\nset(REPLAY_SOURCES_C crypto/replay/rdb.c)\n"),
    "deps/libsrtp/srtp/srtp.c": _notice("Srtp Holder"),
    "deps/libsrtp/crypto/cipher/cipher.c": _notice("Cipher Holder"),
    "deps/libsrtp/crypto/cipher/aes_icm_ossl.c": _notice("Openssl Holder"),
    "deps/libsrtp/crypto/cipher/aes_icm_mbedtls.c": _notice("Mbedtls Holder"),
    "deps/libsrtp/crypto/cipher/aes.c": _notice("Builtin Holder"),
    "deps/libsrtp/crypto/hash/auth.c": _notice("Auth Holder"),
    "deps/libsrtp/crypto/kernel/key.c": _notice("Key Holder"),
    "deps/libsrtp/crypto/math/datatypes.c": _notice("Math Holder"),
    "deps/libsrtp/crypto/replay/rdb.c": _notice("Replay Holder"),
}


def _write_tree(build: Path, compiled: list[str]) -> None:
    """One platform's configured tree: the same fetched sources, and a
    compile_commands.json naming only what that platform compiled."""
    portaudio = build / "_deps" / "portaudio-src"
    for name, text in {"CMakeLists.txt": PORTAUDIO_CMAKE, **PORTAUDIO_FILES}.items():
        (portaudio / name).parent.mkdir(parents=True, exist_ok=True)
        (portaudio / name).write_text(text)
    dc = build / "_deps" / "nereus_libdatachannel-src"
    for name, text in DATACHANNEL_FILES.items():
        (dc / name).parent.mkdir(parents=True, exist_ok=True)
        (dc / name).write_text(text)
    commands = [{"directory": str(build), "file": str(portaudio / name),
                 "command": f"cc -I{portaudio}/src/common -c {name}"} for name in compiled]
    (build / "compile_commands.json").write_text(json.dumps(commands))


def _generate(library: str, build: Path) -> str:
    import argparse
    args = argparse.Namespace(build_dir=build, opus_source=None)
    source_set, notices, _ = sn.build(library, REPO, args)
    return sn.render(source_set, notices, library)


@pytest.mark.parametrize("library", ["portaudio", "libdatachannel", "libjuice", "usrsctp",
                                     "libsrtp"])
def test_fetched_notices_do_not_depend_on_the_platforms_tree(tmp_path, library):
    macos = tmp_path / "macos"
    linux = tmp_path / "linux"
    windows = tmp_path / "windows"
    _write_tree(macos, ["src/common/pa_front.c", "src/os/unix/pa_unix_util.c",
                        "src/hostapi/coreaudio/pa_mac_core.c"])
    _write_tree(linux, ["src/common/pa_front.c", "src/os/unix/pa_unix_util.c",
                        "src/hostapi/jack/pa_jack.c", "src/hostapi/alsa/pa_linux_alsa.c"])
    _write_tree(windows, ["src/common/pa_front.c", "src/os/win/pa_win_hostapis.c"])
    (windows / "compile_commands.json").unlink()  # not even needed
    outputs = {tree.name: _generate(library, tree) for tree in (macos, linux, windows)}
    assert outputs["macos"] == outputs["linux"] == outputs["windows"]
    assert outputs["macos"]


def test_fetched_notices_cover_every_platform_and_only_the_build_options(tmp_path):
    _write_tree(tmp_path, ["src/common/pa_front.c", "src/hostapi/coreaudio/pa_mac_core.c"])
    portaudio = _generate("portaudio", tmp_path)
    for holder in ("Common", "Header", "Skeleton", "Windows", "Converter", "DirectSound",
                   "Mme", "Wasapi", "Wdmks", "Unix", "CoreAudio", "Jack", "Alsa"):
        assert f"Copyright (c) 2001 {holder} Holder" in portaudio, holder
    assert "Asio Holder" not in portaudio  # NereusSDR forces PA_USE_ASIO OFF

    libsrtp = _generate("libsrtp", tmp_path)
    assert "Openssl Holder" in libsrtp
    assert "Mbedtls Holder" not in libsrtp and "Builtin Holder" not in libsrtp

    assert "Juice Holder" in _generate("libjuice", tmp_path)

    usrsctp = _generate("usrsctp", tmp_path)
    assert "Sctp Holder" in usrsctp
    assert "Public Header Holder" in usrsctp  # reached through libdatachannel

    libdatachannel = _generate("libdatachannel", tmp_path)
    assert "Datachannel Holder" in libdatachannel and "Impl Holder" in libdatachannel
    assert "Sctp Holder" not in libdatachannel  # deps/ has its own files


# --------------------------------------------------------------------------
# Dedications (libsodium): public-domain and CC0 comments are notices too

DEDICATED = """\
/*
version 20080912
D. J. Bernstein
Public domain.
*/

/* a plain comment */
int x;
"""

WAIVED = """\
/*
 * Written by Someone. To the extent possible under law, the
 * author has waived all copyright and related or neighboring rights.
 *
 * Copyright (c) 2015 Someone
 */
"""


def test_dedications_are_read_only_when_asked(tmp_path):
    assert sn.extract_blocks(DEDICATED) == []
    assert sn.extract_dedications(DEDICATED) == [DEDICATED.split("\n\n")[0]]
    src = tmp_path / "salsa.c"
    src.write_text(DEDICATED)
    plain, _ = sn.collect([(src, "salsa.c")], [LICENCE_TEXT])
    assert plain == []
    notices, _ = sn.collect([(src, "salsa.c")], [LICENCE_TEXT], dedications=True)
    assert [n.text for n in notices] == [DEDICATED.split("\n\n")[0]]


def test_a_waiver_above_the_copyright_line_is_kept_with_it():
    # Without dedications the block starts at the copyright line; with
    # them, at the comment's start, so the waiver travels with it.
    assert sn.extract_blocks(WAIVED)[0].startswith(" * Copyright (c) 2015 Someone")
    assert sn.extract_blocks(WAIVED, dedications=True) == [WAIVED.rstrip("\n")]
    # And it is not listed a second time as a bare dedication.
    assert sn.extract_dedications(WAIVED) == []


def test_libsodium_headers_are_read_where_the_archive_has_them(tmp_path):
    build = tmp_path / "build"
    src = build / "_deps" / "nereus_libsodium-src" / "src" / "libsodium"
    copy = build / "_deps" / "nereus_libsodium-include"
    (src / "crypto_x").mkdir(parents=True)
    (src / "include" / "sodium").mkdir(parents=True)
    (copy / "sodium").mkdir(parents=True)
    (src / "crypto_x" / "x.c").write_text('#include "sodium/x.h"\n')
    (src / "include" / "sodium" / "x.h").write_text("/* Public domain. */\n")
    (copy / "sodium" / "x.h").write_text("/* Public domain. */\n")
    commands = [{"directory": str(build), "file": str(src / "crypto_x" / "x.c"),
                 "command": f"cc -I{copy} -c x.c"}]
    (build / "compile_commands.json").write_text(json.dumps(commands))
    args = type("Args", (), {"build_dir": build})()
    source_set = sn.PRESETS["libsodium"](REPO, args)
    names = sorted(f.relative_to(source_set.base.resolve()).as_posix()
                   for f in source_set.files)
    assert names == ["src/libsodium/crypto_x/x.c", "src/libsodium/include/sodium/x.h"]
    assert source_set.dedications


def test_libsodium_notices_are_current_when_fetched():
    for name in ("build-lane-b", "build-integration", "build", "build-licences"):
        build = REPO / name
        if (build / "_deps" / "nereus_libsodium-src").is_dir() \
                and (build / "compile_commands.json").is_file():
            break
    else:
        pytest.skip("no configured build with libsodium fetched")
    target = REPO / "packaging" / "third-party-licenses" / "libsodium-notices.txt"
    result = subprocess.run(
        [sys.executable, str(SOURCE_SCRIPT), "libsodium", "--build-dir", str(build),
         "--check", str(target)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
