---
date: 2026-04-12
author: JJ Boyd (KG4VCF) + Claude Code
status: Draft — awaiting review
related:
  - docs/protocols/openhpsdr-protocol1.md
  - docs/architecture/radio-abstraction.md
  - Phase 3L (Protocol 1 support)
---

# Design: Protocol 1 Packet-Capture Reference Document

## Purpose

Phase 3L of NereusSDR adds OpenHPSDR Protocol 1 support so the application
can talk to Hermes Lite 2 (HL2) and other legacy P1 radios. The current
`docs/protocols/openhpsdr-protocol1.md` is a 32-line placeholder. We need
ground-truth byte-level documentation of an actual host↔HL2 conversation
before writing P1 connection code, so the implementation can follow the
NereusSDR "READ → SHOW → TRANSLATE" rule against verifiable evidence rather
than inferred behavior.

The user has provided `HL2_Packet_Capture.pcapng` (320 MB, captured between
a Thetis-class host and a Hermes Lite 2). This spec defines the deliverable
that turns that capture into a permanent reference asset.

## Capture Recon (already performed)

Confirmed via `tshark -q -z conv,udp` and `-z io,phs`:

| Property | Value |
| --- | --- |
| File | `HL2_Packet_Capture.pcapng` (320 MB, ~302,256 frames) |
| Host | `169.254.105.135` (link-local, no DHCP — direct cable) |
| Radio (HL2) | `169.254.19.221`, port `1024` |
| Session ports | host `:50534` ↔ radio `:1024` (281,195 frames RX, 21,049 frames TX-direction) |
| Discovery | host `:50533` → `255.255.255.255:1024` and `169.254.255.255:1024` (2 frames), reply from `169.254.19.221:1024` (2 frames) |
| Duration | ~55.7 s session + ~4 s DHCP tail |
| Other traffic | 4 ARP frames, 2 DHCP frames (host bring-up) — not part of P1 |

This is a clean single-session capture: discovery → start → steady-state →
stop, with the session running long enough (~55 s) to observe sequence
wraparounds, band changes, and any keying events.

## Deliverable

A single new markdown file:

**`docs/protocols/openhpsdr-protocol1-capture-reference.md`**

The existing `docs/protocols/openhpsdr-protocol1.md` will be updated to:

- Link to the new capture-reference doc as the primary P1 source-of-truth
- Absorb any short byte-layout summaries that belong in the canonical
  reference (so the placeholder file becomes a real spec stub, not the
  full analysis)

No source code is written or modified by this work.

## Document Structure

The capture-reference doc has the following sections, in order. Each
section is grounded in actual frames extracted from the pcap with
`tshark`, with annotated hex dumps and line-number cross-references to
Thetis `Project Files/Source/Console/NetworkIO.cs` (and related files)
per the project's READ → SHOW → TRANSLATE rule.

### 1. Capture Metadata
The recon table above plus a per-port frame/byte breakdown and a short
"how this capture was taken" note. Lets future readers use the document
without needing the pcap.

### 2. Discovery Exchange
Decoded discovery request (`0xEFFE 0x02` + padding) and reply (`0xEFFE
0x02/0x03` + board-id + version + MAC). One annotated hex dump per
direction. Cross-ref Thetis discovery code path.

### 3. Start / Stop Commands
The `0xEFFE 0x04` start command and corresponding stop. Document the
"start IQ" vs "start IQ + wideband" variants if both appear, with the
exact byte that selects sample rate behavior. One hex dump each.

### 4. EP6 — Radio → Host Metis Frames (1032 bytes)
Full anatomy of the data frame:

- 8-byte Metis header (`EFFE 01 06 <seq32>`)
- USB frame 1: 3-byte sync `7F7F7F`, C0 round-robin status index, C1–C4
  status bytes, 504 bytes of interleaved 24-bit BE I/Q + 16-bit mic
- USB frame 2: same layout

Single annotated hex dump of one representative frame. Status-byte
decode table covering every C0 index actually observed in the capture,
not the spec-theoretical set. Document mic-sample interleaving for the
HL2 case (HL2 carries mic in EP6 even when not transmitting).

### 5. EP2 — Host → Radio Command Frames (1032 bytes)
Same Metis envelope, but USB frames carry C&C *commands* instead of
status. Section enumerates every distinct C0 round-robin index seen in
the host→radio direction across the session, with C1–C4 decoded:

- Sample rate / open collector outputs
- ADC attenuation, preamp, dither, random
- RX1..RXn frequency (per-DDC, 32-bit BE Hz)
- TX frequency
- Drive level / mic boost / line-in
- Alex / HPF / LPF / TX relay band selection
- MOX, PTT, CW keyer state
- Anything else that appears

For each: source-line citation in `NetworkIO.cs`. If the capture shows
HL2-specific values (e.g. extended C&C indices the wiki doesn't fully
document), call them out.

### 6. Steady-State Cadence
Quantitative section produced from tshark exports:

- Observed packet rate radio→host and host→radio (Hz)
- Sequence-number progression, any gaps, any wraparound
- Inter-frame timing distribution (mean / p50 / p99) for both directions
- Behavior at band changes (does the host pause TX-direction commands?)

### 7. Band-Change Trace
Walk through one or more band-change events found in the capture (e.g.
RX frequency moved across an HPF/LPF boundary). Show the exact sequence
of EP2 commands the host emits, in order, with timestamps relative to
the band change. Useful as a recipe for the Nereus implementation.

### 8. TX / Keying Trace (conditional)
Detection method: scan all EP2 frames for the MOX bit being set. If
found, document the full TX bring-up sequence (MOX-on, drive level,
TX freq, Alex relay change, TX I/Q frames if in EP2 payload, MOX-off).
If no MOX events are found, this section says so explicitly with the
detection query used, so a future reader knows the gap is *known*, not
overlooked.

### 9. HL2-Specific Quirks
Anything in the capture that diverges from the generic OpenHPSDR P1
wiki spec: HL2 firmware version reported in discovery, supported sample
rates actually exercised, ADC overload bit usage, IO pin / PTT-out
behavior, and any non-standard C&C indices.

### 10. Nereus Implementation Checklist
Concrete TODO list for Phase 3L derived from gaps the capture exposes.
Each item links back to the section that motivates it. This is how the
document earns its keep — it should turn into the Phase 3L plan's
backbone.

### Appendix A — Reproducibility
The exact `tshark` invocations used to extract every hex dump and stat
in the document, as fenced code blocks, so a future maintainer can
re-run the analysis on a different capture without guessing.

## Method

1. **Filtered subsetting.** Use `tshark -Y 'udp.port == 1024'` writing
   to a smaller pcap, then run all subsequent passes against the subset
   to keep iteration fast.
2. **Shape enumeration.** Group EP2 frames by `(length, first 16 bytes
   signature, C0 index)` and pick one representative per group. Same
   for EP6 (expected to be one shape but verify).
3. **Byte decode.** For each representative, dump as hex with `tshark
   -x` and annotate fields against `NetworkIO.cs`. Cite line numbers.
4. **Stats.** Use `tshark -T fields` for sequence/timing data, pipe
   through `awk` for percentiles.
5. **TX detection.** Filter EP2 frames where the MOX bit (C0 index 0,
   C1 bit 0 in the standard P1 layout) is set; if any matches, expand
   into Section 8.
6. **Keep raw artifacts inline.** Hex dumps and tshark commands live in
   the document as fenced blocks so the analysis is self-contained.

## Out of Scope

- Writing or modifying any C++ source. This work produces a reference
  document only. Phase 3L implementation comes after, in its own plan.
- Wideband ADC samples (separate UDP path on HL2) unless they appear
  in this capture.
- PureSignal feedback (HL2 has no PS feedback DDC).
- Protocol 2 — already covered separately.
- Any feature work not directly motivated by getting a P1 radio to a
  working RX state in Nereus.

## Success Criteria

- A new reader can implement P1 discovery, start, RX I/Q parsing, RX
  tuning, and band selection from this document plus `NetworkIO.cs`,
  without referring back to the pcap.
- Every byte layout claim in the doc is backed by a hex dump from the
  capture and a Thetis source citation.
- The Phase 3L implementation checklist (Section 10) is complete enough
  to seed the Phase 3L plan when that work begins.
- The reproducibility appendix lets a maintainer re-run the same
  analysis on a different capture in under an hour.

## Open Questions Resolved

- **Filename / location:** `docs/protocols/openhpsdr-protocol1-capture-reference.md`.
- **TX coverage:** Operator may have keyed up; capture will be scanned
  for MOX events and Section 8 either populated or marked "no TX events
  found, detection query: …".

## Risks

- **Capture is one HL2, one host, one firmware revision.** Findings may
  not generalize to all P1 radios. The doc will call out HL2-specific
  observations in Section 9 to avoid over-generalization.
- **HL2 firmware quirks** may make some bytes look like spec violations
  when they are HL2 conventions. Mitigation: every divergence from the
  wiki spec gets explicitly flagged rather than silently documented as
  canonical P1.
