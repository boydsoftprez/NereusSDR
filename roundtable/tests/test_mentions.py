# no-port-check: NereusSDR-original.
"""Mentions (wire spec 4.5, 5.3.11, 5.4.2, 6.10): the base callsign, what
a message mentions, the live `mention` after the deltas, and the unread
mentions a station's next `welcome` lists."""

from __future__ import annotations

from cryptography.hazmat.primitives.asymmetric import ec

from inproc import Harness, presence, slice_
from nereus_roundtable import messages
from nereus_roundtable.config import Config

ROOM = "usb-14212000"


def test_the_base_callsign_is_the_longest_part_the_first_on_a_tie_upper_case():
    assert messages.base_callsign("VE3/W4XYZ/P") == "W4XYZ"
    assert messages.base_callsign("kg4vcf/p") == "KG4VCF"
    assert messages.base_callsign("W4XYZ/MM") == "W4XYZ"
    assert messages.base_callsign("AB1/CD2") == "AB1"
    assert messages.base_callsign("K4ABC") == "K4ABC"


def test_a_word_mentions_a_station_with_the_same_base_callsign():
    calls = ["KG4VCF", "W4XYZ/P", "K4ABC"]
    assert messages.mentions("thanks kg4vcf/p", calls) == {"KG4VCF"}
    assert messages.mentions("KG4VCFX was loud", calls) == set()
    assert messages.mentions("@KG4VCF, and ve3/w4xyz too", calls) == {"KG4VCF", "W4XYZ/P"}
    assert messages.mentions("xKG4VCF", calls) == set()
    assert messages.mentions("KG4VCF-KG4VCF", calls) == {"KG4VCF"}
    assert messages.mentions("nobody here", calls) == set()


def _station(harness, callsign, n, key=None):
    s = harness.sign_in(key=key, address=f"192.0.2.{n}")
    s.callsign = callsign
    s.send(presence(callsign, "EM73", [slice_(0, 14212000)]))
    return s


def _post(station, text, client_id="c1"):
    station.send({"type": "post", "room": ROOM, "text": text, "clientId": client_id})


def test_a_connected_station_gets_mention_after_the_deltas_and_the_sender_none():
    h = Harness()
    try:
        a = _station(h, "K4ABC", 1)
        b = _station(h, "KG4VCF/P", 2)
        c = _station(h, "W4XYZ", 3)
        h.advance(10000)
        for s in (a, b, c):
            s.take()
        _post(a, "thanks kg4vcf, and k4abc too")
        got = b.take()
        assert [m["type"] for m in got] == ["delta", "mention"]
        mention = got[1]
        assert mention["room"] == ROOM
        assert mention["message"]["text"] == "thanks kg4vcf, and k4abc too"
        assert mention["message"]["callsign"] == "K4ABC"
        assert mention["message"]["id"] == got[0]["messages"][0]["id"]
        assert "mention" not in [m["type"] for m in a.take()]
        assert "mention" not in [m["type"] for m in c.take()]
        # A near miss mentions nobody.
        h.advance(1000)
        _post(a, "KG4VCFX is not you", "c2")
        assert [m["type"] for m in b.take()] == ["delta"]
    finally:
        h.close()


def test_an_offline_station_gets_its_mentions_in_its_next_welcome_until_it_marks_them_seen():
    h = Harness()
    try:
        key = ec.generate_private_key(ec.SECP256R1())
        a = _station(h, "K4ABC", 1)
        b = _station(h, "KG4VCF/P", 2, key=key)
        h.advance(10000)
        a.take()
        b.disconnect()
        _post(a, "thanks kg4vcf/p")
        h.advance(1000)
        _post(a, "KG4VCFX is someone else", "c2")
        h.advance(1000)
        _post(a, "@KG4VCF 73", "c3")
        a.take()
        b = h.sign_in(key=key, address="192.0.2.2")
        listed = b.welcome["mentions"]
        assert [m["message"]["text"] for m in listed] == ["thanks kg4vcf/p", "@KG4VCF 73"]
        assert all(m["room"] == ROOM for m in listed)
        last_t = listed[-1]["message"]["t"]
        b.send({"type": "mentionsSeen", "t": last_t})
        assert b.take() == []
        b.disconnect()
        assert h.sign_in(key=key, address="192.0.2.2").welcome["mentions"] == []
        # An earlier `t` never moves the mark back.
        b = h.sign_in(key=key, address="192.0.2.2")
        b.send({"type": "mentionsSeen", "t": 0})
        b.disconnect()
        h.advance(1000)
        _post(a, "kg4vcf again", "c4")
        b = h.sign_in(key=key, address="192.0.2.2")
        assert [m["message"]["text"] for m in b.welcome["mentions"]] == ["kg4vcf again"]
    finally:
        h.close()


def test_welcome_lists_the_newest_50_oldest_first_and_no_deleted_message():
    config = Config()
    config.messages_per_minute = 1000
    config.messages_per_hour = 1000
    h = Harness(config)
    try:
        key = ec.generate_private_key(ec.SECP256R1())
        a = _station(h, "K4ABC", 1)
        b = _station(h, "KG4VCF", 2, key=key)
        h.advance(10000)
        b.disconnect()
        for i in range(53):
            _post(a, f"KG4VCF number {i}", f"c{i}")
            h.advance(1)
        ids = [m["msg"] for m in a.take() if m["type"] == "posted"]
        assert len(ids) == 53
        a.send({"type": "delete", "msg": ids[-1]})
        a.take()
        b = h.sign_in(key=key, address="192.0.2.2")
        texts = [m["message"]["text"] for m in b.welcome["mentions"]]
        assert texts == [f"KG4VCF number {i}" for i in range(2, 52)]
    finally:
        h.close()


def test_a_station_s_own_message_is_never_its_mention():
    h = Harness()
    try:
        key = ec.generate_private_key(ec.SECP256R1())
        a = _station(h, "KG4VCF", 1, key=key)
        h.advance(10000)
        _post(a, "this is kg4vcf")
        assert "mention" not in [m["type"] for m in a.take()]
        a.disconnect()
        assert h.sign_in(key=key, address="192.0.2.1").welcome["mentions"] == []
    finally:
        h.close()


def test_a_hidden_station_gets_no_live_mention_but_its_next_welcome_lists_it():
    h = Harness()
    try:
        key = ec.generate_private_key(ec.SECP256R1())
        a = _station(h, "K4ABC", 1)
        b = _station(h, "KG4VCF", 2, key=key)
        h.advance(10000)
        a.take()
        b.send({"type": "hidden"})
        b.take()
        _post(a, "kg4vcf are you there")
        assert b.take() == []
        b.disconnect()
        b = h.sign_in(key=key, address="192.0.2.2")
        assert [m["message"]["text"] for m in b.welcome["mentions"]] == ["kg4vcf are you there"]
    finally:
        h.close()
