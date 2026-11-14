# no-port-check: NereusSDR-original.
"""The text corpus (wire spec 6.10, design 12.4): every case is cleaned or
refused as tests/data/text-corpus.json says, both by messages.clean and
when posted to the server, which stores and sends the cleaned text."""

from __future__ import annotations

import json
import unicodedata
from pathlib import Path

import pytest

from inproc import Harness, presence, slice_
from nereus_roundtable import messages
from nereus_roundtable.config import Config

CORPUS = json.loads((Path(__file__).resolve().parent / "data" / "text-corpus.json").read_text("utf-8"))
CASES = CORPUS["cases"]
ROOM = "usb-14212000"


def test_the_corpus_is_well_formed():
    names = [c["name"] for c in CASES]
    assert len(names) == len(set(names))
    for case in CASES:
        assert set(case) <= {"name", "text", "cleaned", "refused", "links", "note"}, case["name"]
        assert (case["cleaned"] is None) != (case["refused"] is None), case["name"]
        assert case["refused"] in (None, "too-long", "not-allowed"), case["name"]
        # Each can travel in a `post` (a text of 1 to 4000 bytes).
        assert 1 <= len(case["text"].encode("utf-8")) <= 4000, case["name"]


def test_the_corpus_covers_every_format_character_cleaning_removes_and_the_two_it_keeps():
    texts = "".join(c["text"] for c in CASES)
    for cp in messages.REMOVED_FORMAT:
        assert chr(cp) in texts, hex(cp)
    for cp in (0x200C, 0x200D):
        assert any(chr(cp) in (c["cleaned"] or "") for c in CASES), hex(cp)
    assert messages.REMOVED_FORMAT == frozenset(
        [0x061C, 0x200B, 0x200E, 0x200F, 0x202A, 0x202B, 0x202C, 0x202D, 0x202E, 0x2060, 0x2066, 0x2067, 0x2068, 0x2069, 0xFEFF]
    )


@pytest.mark.parametrize("case", CASES, ids=[c["name"] for c in CASES])
def test_clean(case):
    cleaned = messages.clean_text(case["text"])
    if case["refused"] is None:
        assert messages.clean(case["text"]) == case["cleaned"]
        assert messages.check(cleaned) is None
        # Nothing cleaning removes is left.
        assert not any(ord(c) in messages.REMOVED_FORMAT for c in cleaned)
        assert not any(unicodedata.category(c) == "Cc" and c != "\n" for c in cleaned)
    else:
        assert messages.clean(case["text"]) is None
        assert messages.check(cleaned) == case["refused"]
    if "links" in case:
        assert messages.links(case["cleaned"]) == case["links"]


def test_cleaning_twice_changes_nothing():
    for case in CASES:
        if case["cleaned"] is not None:
            # NFC runs after the removals, so what is stored is already NFC.
            assert unicodedata.is_normalized("NFC", case["cleaned"]), case["name"]
            assert messages.clean_text(case["cleaned"]) == case["cleaned"], case["name"]


@pytest.fixture(scope="module")
def posted():
    """Every corpus case posted by one member; returns, by name, what the
    sender got back and the stored row."""
    config = Config()
    config.messages_per_minute = 1000
    config.messages_per_hour = 1000
    h = Harness(config)
    try:
        a = h.sign_in()
        a.send(presence("K4ABC", "EM73", [slice_(0, 14212000)]))
        h.advance(10000)
        a.take()
        out = {}
        for i, case in enumerate(CASES):
            a.send({"type": "post", "room": ROOM, "text": case["text"], "clientId": f"c{i}"})
            got = a.take()
            answer = got[0]
            assert answer["type"] == "posted" and answer["clientId"] == f"c{i}", got
            row = h.store.message_get(answer["msg"]) if answer["ok"] else None
            deltas = [m for m in got if m["type"] == "delta"]
            out[case["name"]] = (answer, row, deltas)
        yield out
    finally:
        h.close()


@pytest.mark.parametrize("case", CASES, ids=[c["name"] for c in CASES])
def test_posted(case, posted):
    answer, row, deltas = posted[case["name"]]
    if case["refused"] is not None:
        assert answer["ok"] is False and answer["code"] == case["refused"] and answer["msg"] is None
        assert deltas == []
        return
    assert answer["ok"] is True and answer["code"] is None
    assert row["text"] == case["cleaned"] and row["state"] == "shown"
    assert [m["text"] for d in deltas for m in d["messages"]] == [case["cleaned"]]
