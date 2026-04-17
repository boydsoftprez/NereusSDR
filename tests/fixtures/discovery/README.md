# tests/fixtures/discovery — OpenHPSDR Discovery Reply Fixtures

These hex files contain raw OpenHPSDR UDP discovery reply packets used by the
`tst_radio_discovery_parse` unit test suite.  Each file holds one hex-encoded
packet (space- or newline-separated octets) representing the exact bytes a
radio would send in response to a discovery broadcast on UDP port 1024.

---

## File provenance

### `p1_angelia_reply.hex`

**Source:** Captured from a real **Apache Labs ANAN-100D** (Angelia board,
firmware version 21) running OpenHPSDR Protocol 1.

The capture was taken by J.J. Boyd (KG4VCF) during radio-model identification
work for Phase 3I-5 (commit `4e72376`).  Byte layout verified against:
- `docs/protocols/openhpsdr-protocol1-capture-reference.md` §2.2
- `mi0bot clsRadioDiscovery.cs:1145-1195` (the parseDiscoveryReply P1 branch)

Device-type byte is `0x04` (Angelia). This matches the `mapP1DeviceType()`
`case 4 → Angelia` path in `RadioDiscovery.cpp`.

---

### `p1_hermeslite_reply.hex`

**Source:** Captured from a real **Hermes Lite 2** (firmware version 72)
running OpenHPSDR Protocol 1.

The capture was taken by J.J. Boyd (KG4VCF) during the same Phase 3I-5 work.
Byte layout verified against the same reference as above.

---

### `p2_saturn_reply.hex`

**Source:** **Hand-crafted from the OpenHPSDR Protocol 2 specification.**
No live ANAN-G2 (Saturn) P2 capture was available at the time this fixture
was authored.

The fixture was constructed to exercise the `parseP2Reply()` parser (commit
`4e72376`) by encoding the expected P2 discovery reply format per:
- `mi0bot clsRadioDiscovery.cs:1201-1226` (the parseDiscoveryReply P2 branch)
- `docs/protocols/openhpsdr-protocol2.md`

**Reviewer note:** This fixture should be replaced with a real ANAN-G2 / Saturn
P2 capture when one becomes available.  Until then, treat the P2 parser test
as spec-compliance verification rather than hardware-verified behavior.

---

## Copyright note

OpenHPSDR discovery reply packets consist entirely of factual wire-protocol
data (MAC address, firmware version byte, device-type byte, in-use flag).
These facts are not subject to copyright.  The arrangement and annotation of
these fixture files are original NereusSDR work, licensed GPL-2.0-or-later.
