# P1 Capture-Reference Document Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce `docs/protocols/openhpsdr-protocol1-capture-reference.md` — a complete, byte-level annotated reference of an HL2↔Thetis Protocol 1 conversation extracted from `HL2_Packet_Capture.pcapng`, so Phase 3L can implement P1 support against ground truth instead of inference.

**Architecture:** This is a documentation-only deliverable. Each section of the target doc is built by one task: a tshark extraction step → a Thetis cross-reference step → a markdown drafting step → a commit. No source code is modified except a small link/absorb edit to the existing P1 placeholder doc at the end.

**Tech Stack:** `tshark` (already installed at `/opt/homebrew/bin/tshark`), `awk`, plain markdown. Source citations come from `/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs` per the project's READ → SHOW → TRANSLATE rule (CLAUDE.md).

**Working files:**
- Capture (extracted, do not re-extract): `/tmp/hl2cap/HL2_Packet_Capture.pcapng` (320 MB)
- Filtered subset (created in Task 1): `/tmp/hl2cap/hl2_session.pcapng`
- Spec being implemented: `docs/superpowers/specs/2026-04-12-p1-capture-reference-design.md`


**Thetis P1 source map** — discovered during Task 3. Most P1 byte-level code lives in the **unmanaged ChannelMaster C source**, not in the C# `NetworkIO.cs` file. Subagents must grep the right file for what they need:

| Topic | Authoritative file |
| --- | --- |
| Discovery request build / reply parse | `/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/clsRadioDiscovery.cs` (C#, ~lines 1100-1310) |
| Metis start/stop, EP2 send, EP6 receive, sequence numbers | `/Users/j.j.boyd/Thetis/Project Files/Source/ChannelMaster/networkproto1.c` (C, 749 lines, by W5WC) |
| Metis socket setup, port binding, generic frame I/O | `/Users/j.j.boyd/Thetis/Project Files/Source/ChannelMaster/network.c` (C, 1494 lines) |
| C# P/Invoke surface (function names, signatures) | `/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIOImports.cs` |
| C# wrapper layer (state, callbacks) | `/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs` |
| Board-ID enum | `/Users/j.j.boyd/Thetis/Project Files/Source/Console/enums.cs` |

**Citation rule:** cite whichever file actually contains the byte layout being decoded. For Tasks 4-9 this will USUALLY be `networkproto1.c`. The grep commands shown in subsequent tasks use `NetworkIO.cs` but were written before the source layout was understood — if `NetworkIO.cs` returns no matches, retry against `networkproto1.c` and `network.c`. The `READ → SHOW → TRANSLATE` rule requires the citation to point at the file the code actually lives in.

**Conventions:**
- All hex dumps in the doc come from `tshark -x` output, trimmed to the relevant payload range, fenced with ` ```` `.
- Every byte-layout claim cites `NetworkIO.cs:<line>` (or another file with line number).
- Every tshark command used appears verbatim in Appendix A.
- Commit message prefix: `docs(p1):`.
- Co-author trailer per user memory: `Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>`. GPG-sign commits.

---

## Task 1: Subset the capture and confirm Thetis source availability

**Files:**
- Create: `/tmp/hl2cap/hl2_session.pcapng`
- Read-only: `/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs`

- [ ] **Step 1: Verify Thetis clone is present**

Run: `ls "/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs"`
Expected: file exists.
If missing, STOP and tell the user — per CLAUDE.md, do not invent protocol details. Ask where the Thetis clone lives.

- [ ] **Step 2: Build the filtered session pcap**

Run:
```bash
tshark -r /tmp/hl2cap/HL2_Packet_Capture.pcapng \
  -Y 'udp.port == 1024' \
  -w /tmp/hl2cap/hl2_session.pcapng
```
Expected: no errors. New file ~324 MB (P1 traffic dominates).

- [ ] **Step 3: Sanity-check the subset**

Run: `tshark -r /tmp/hl2cap/hl2_session.pcapng -q -z conv,udp`
Expected: shows the `169.254.105.135:50534 ↔ 169.254.19.221:1024` conversation with ~302k frames and the discovery exchange on `:50533`.

- [ ] **Step 4: No commit** (working file is in `/tmp`, not in repo).

---

## Task 2: Scaffold the target doc with the front-matter and metadata table

**Files:**
- Create: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Create the file with this exact content**

```markdown
---
date: 2026-04-12
author: JJ Boyd (KG4VCF)
status: Reference — derived from HL2_Packet_Capture.pcapng
related:
  - docs/protocols/openhpsdr-protocol1.md
  - docs/architecture/radio-abstraction.md
  - docs/superpowers/specs/2026-04-12-p1-capture-reference-design.md
---

# OpenHPSDR Protocol 1 — Annotated Capture Reference (Hermes Lite 2)

This document is the byte-level ground-truth reference for OpenHPSDR
Protocol 1 as implemented by Hermes Lite 2 firmware, derived from a
single live capture of a Thetis-class host talking to an HL2 over a
direct link-local Ethernet connection. Every layout claim in this doc
is backed by a hex dump from the capture and a Thetis `NetworkIO.cs`
source citation. NereusSDR Phase 3L (P1 support) implements against
this document.

## 1. Capture Metadata

| Property | Value |
| --- | --- |
| File | `HL2_Packet_Capture.pcapng` (~324 MB) |
| Frames | 302,256 total (302,252 IPv4/UDP, 4 ARP) |
| Duration | ~55.7 s session (+ ~4 s DHCP tail) |
| Host | `169.254.105.135` (link-local) |
| Radio (HL2) | `169.254.19.221`, UDP port `1024` |
| Discovery | host `:50533` → broadcast `:1024`, reply from radio |
| Session | host `:50534` ↔ radio `:1024` |
| Direction split | 281,195 frames radio→host (EP6), 21,049 frames host→radio (EP2) |

The capture is a single clean session: discovery → start → steady-state
RX → stop. Subsequent sections walk through each phase.

<!-- Sections 2-10 and Appendix A added by later tasks -->
```

- [ ] **Step 2: Commit**

```bash
cd /Users/j.j.boyd/NereusSDR
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): scaffold HL2 capture reference doc

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Section 2 — Discovery exchange

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Extract the four discovery frames**

Run:
```bash
tshark -r /tmp/hl2cap/HL2_Packet_Capture.pcapng \
  -Y 'udp.port == 1024 and (ip.src == 169.254.105.135 or ip.dst == 169.254.105.135) and udp.srcport == 50533 or udp.dstport == 50533' \
  -x | head -200
```
Expected: 4 packets — 2 host→broadcast, 2 radio→host. First two bytes of each payload should be `ef fe`. The discovery request payload starts `ef fe 02 00...`; the reply should start `ef fe 02 0X` or `ef fe 03 ...` (record what you actually see).

- [ ] **Step 2: Locate Thetis discovery code**

Run: `grep -n -i 'discover\|0xeffe\|0xef, 0xfe' "/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs" | head -40`
Record the line numbers of the discovery send and discovery reply parser.

- [ ] **Step 3: Append Section 2 to the doc**

Add a new `## 2. Discovery Exchange` section that contains:
- A short prose description.
- The hex dump of one host→radio discovery request frame, fenced as ```` ```text ```` ````, with bytes annotated by adding a second fenced block beneath it labeling each field (sync `EF FE`, command `02`, padding).
- The hex dump of one radio→host discovery reply, with byte annotations: status, board ID byte, firmware version bytes, MAC bytes — using the *actual* values seen.
- A "Cross-reference" line: `Thetis: NetworkIO.cs:<line> (send), NetworkIO.cs:<line> (reply parse)`.
- Do NOT invent fields. If a byte's meaning is unclear from `NetworkIO.cs`, mark it `unknown — investigate before implementing`.

- [ ] **Step 4: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): document HL2 discovery request/reply

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Section 3 — Start / Stop commands

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Find the start frame**

Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.payload[0:3] == ef:fe:04' \
  -x | head -80
```
Expected: at least one frame near the start of the session. Record its frame number (`-T fields -e frame.number`).

- [ ] **Step 2: Find the stop frame**

Run the same query against the end of the capture:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.payload[0:3] == ef:fe:04' \
  -T fields -e frame.number -e frame.time_relative
```
Expected: a list of `04` frames; the last one(s) should be the stop. The byte at offset 3 distinguishes start vs stop variants — record what you see.

- [ ] **Step 3: Locate Thetis start/stop send code**

Run: `grep -n 'MetisStartStop\|metis_start\|0x04' "/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs" | head -20`
Record the function name and line number.

- [ ] **Step 4: Append Section 3 to the doc**

Add `## 3. Start / Stop Commands` containing:
- Hex dump of the start frame payload (first ~16 bytes are enough; pad bytes after that).
- Decoded fields: sync `EF FE`, command `04`, mode byte (and what its bits mean per `NetworkIO.cs`), padding.
- Hex dump of the stop frame.
- Cross-reference line citing `NetworkIO.cs:<line>`.

- [ ] **Step 5: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): document Metis start/stop commands

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Section 4 — EP6 radio→host Metis frame anatomy

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Confirm EP6 frame size and shape**

Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221 and udp.payload[0:3] == ef:fe:01' \
  -T fields -e frame.len | sort -u
```
Expected: a single value (1074 = 14 eth + 20 ip + 8 udp + 1032 payload, or similar). Record it. If multiple sizes appear, document each one in the section instead of just one.

- [ ] **Step 2: Pull one representative EP6 frame as hex**

Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221 and udp.payload[0:3] == ef:fe:01' \
  -c 1 -x
```
Save the full hex output — this is the annotated example for Section 4.

- [ ] **Step 3: Enumerate all C0 status indices observed in EP6**

The C0 byte is at offset 11 in the UDP payload (8 Metis header + 3 sync `7F 7F 7F`). Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221 and udp.payload[0:3] == ef:fe:01' \
  -T fields -e udp.payload \
  | awk '{print toupper(substr($0, 23, 2))}' \
  | sort | uniq -c | sort -rn
```
(Offsets in `awk`: each hex byte is 2 chars, so byte 11 starts at char position 23.)
Expected: a small set of C0 values with counts. Record the table.

- [ ] **Step 4: Locate Thetis EP6 parser**

Run: `grep -n 'ProcessMetisData\|EP6\|0x7F, 0x7F, 0x7F\|c1_addr' "/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs" | head -40`
Record the function and line range that parses incoming EP6 frames and decodes C1–C4 by C0 index.

- [ ] **Step 5: Append Section 4 to the doc**

Write `## 4. EP6 — Radio → Host Metis Frames` containing:
- Frame-size note from Step 1.
- The annotated hex dump from Step 2, with a per-byte legend covering: 8-byte Metis header (`EF FE 01 06 <seq32>`), USB frame 1 sync `7F 7F 7F`, C0, C1–C4, then the 504-byte I/Q+mic payload (just describe the payload structure: 24-bit BE I, 24-bit BE Q, 16-bit BE mic, repeated; samples-per-frame depends on receiver count — note what this capture's count appears to be from the start frame in Section 3).
- USB frame 2 (offsets 520–1031) — same structure.
- Table of every C0 status index seen in this capture, with C1–C4 meaning per Thetis source. Mark any unrecognized index as `observed but unmapped — investigate`.
- Cross-reference line.

- [ ] **Step 6: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): document EP6 Metis frame anatomy and C0 status map

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Section 5 — EP2 host→radio command frames

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Confirm EP2 frame shape**

Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024 and udp.payload[0:3] == ef:fe:02' \
  -T fields -e frame.len | sort -u
```
Expected: a single frame size. Record it.

- [ ] **Step 2: Enumerate every C0 round-robin index sent by the host**

The C0 byte for EP2 is at the same offset (8-byte Metis header, then 3-byte sync `7F 7F 7F`, then C0 at offset 11). Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024 and udp.payload[0:3] == ef:fe:02' \
  -T fields -e udp.payload \
  | awk '{print toupper(substr($0, 23, 2))}' \
  | sort | uniq -c | sort -rn
```
Record the table — this is the universe of host→radio commands the capture exercises.

- [ ] **Step 3: For each distinct C0, pull one representative frame and grab C1–C4**

For each C0 value `XX` from Step 2, run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y "ip.src == 169.254.105.135 and udp.dstport == 1024 and udp.payload[11] == XX" \
  -c 1 -x
```
Record the C0 and the four C1–C4 bytes from the first USB frame.

- [ ] **Step 4: Locate Thetis EP2 send code**

Run: `grep -n 'NetworkSendC\|MetisCommand\|c0 = \|round_robin' "/Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs" | head -60`
Identify the function(s) that build the C&C bytes per C0 index, with line numbers.

- [ ] **Step 5: Append Section 5 to the doc**

Write `## 5. EP2 — Host → Radio Command Frames` containing:
- Frame-size note.
- Annotated hex dump of one EP2 frame (Metis header + USB1 sync + C0/C1–C4 + payload-pad + USB2 sync + C0/C1–C4 + payload-pad).
- A decode table with one row per C0 index observed. Columns: `C0 (hex)`, `Name`, `C1`, `C2`, `C3`, `C4`, `Decoded meaning (Thetis ref)`. Names and decoding come from `NetworkIO.cs` — cite the line. Anything not covered by `NetworkIO.cs` is marked `unknown — HL2-specific; investigate`.
- A short paragraph naming each command class observed: sample rate, OC outputs, ADC atten/preamp/dither/random, RX1..N freq, TX freq, drive level, Alex HPF/LPF, MOX/PTT, etc. — but only mention classes that actually appear in the table, do not invent.

- [ ] **Step 6: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): document EP2 command frame round-robin C0 map

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Section 6 — Steady-state cadence

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Compute radio→host frame rate**

Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221 and udp.payload[0:3] == ef:fe:01' \
  -T fields -e frame.time_relative \
  | awk 'BEGIN{first=0; last=0; n=0} {if(first==0)first=$1; last=$1; n++} END{printf "n=%d duration=%.3f rate=%.1f Hz\n", n, last-first, n/(last-first)}'
```
Record the result.

- [ ] **Step 2: Compute host→radio frame rate**

Same query with source/dest swapped:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024 and udp.payload[0:3] == ef:fe:02' \
  -T fields -e frame.time_relative \
  | awk 'BEGIN{first=0; last=0; n=0} {if(first==0)first=$1; last=$1; n++} END{printf "n=%d duration=%.3f rate=%.1f Hz\n", n, last-first, n/(last-first)}'
```
Record the result.

- [ ] **Step 3: Check sequence-number continuity for EP6**

Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221 and udp.payload[0:3] == ef:fe:01' \
  -T fields -e udp.payload \
  | awk '{seq=strtonum("0x" substr($0, 9, 8)); if(prev!="" && seq != prev+1) print NR, prev, seq; prev=seq}' \
  | head -20
```
Expected: empty (no gaps) OR a list of discontinuities. Either way, record what you find.

- [ ] **Step 4: Append Section 6 to the doc**

Write `## 6. Steady-State Cadence` with:
- Two-row table: direction, frames, duration, rate.
- Sequence-number observation (continuous across the session, or N gaps at specific points).
- Short prose: "the host emits ~X commands/s during steady-state RX; the radio emits ~Y EP6 frames/s, matching the expected rate at sample-rate Z (cite the rate seen in the start frame in Section 3)."

- [ ] **Step 5: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): document steady-state cadence and sequence continuity

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Section 7 — Band-change trace

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Find frequency-change events**

The C0 indices that carry RX frequency are documented in `NetworkIO.cs` (typically `0x02/0x04/0x06/...` per receiver). Use the decode table built in Task 6 to identify the C0 for RX1 freq, then run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y "ip.src == 169.254.105.135 and udp.dstport == 1024 and udp.payload[11] == <RX1_C0>" \
  -T fields -e frame.number -e frame.time_relative -e udp.payload \
  | awk '{f=strtonum("0x" substr($3, 25, 8)); if(f!=prev){print $1, $2, f; prev=f}}'
```
Expected: one row per *change* in RX1 frequency. Record the timestamps and frequencies.

- [ ] **Step 2: For one band-change event, dump the surrounding ±20 EP2 frames**

Pick the most interesting band change (one that crosses an HPF/LPF boundary if possible). Get its frame number `N`, then:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y "ip.src == 169.254.105.135 and udp.dstport == 1024 and frame.number >= N-20 and frame.number <= N+20" \
  -T fields -e frame.number -e frame.time_relative \
  -e udp.payload
```
Record the C0 sequence around the change.

- [ ] **Step 3: Append Section 7 to the doc**

Write `## 7. Band-Change Trace` containing:
- The list of frequency changes from Step 1 as a table.
- For one walked-through change: a timeline showing each EP2 command in order with relative timestamp, C0 name, and decoded value. This becomes the recipe Phase 3L follows when the user retunes across an HPF/LPF boundary.

- [ ] **Step 4: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): document band-change command sequence

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Section 8 — TX / keying trace (conditional)

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Detect MOX events**

The MOX bit lives in C&C C0=0x00, C1 bit 0 per the standard P1 layout (verify against `NetworkIO.cs` while you're in Task 6 — the line should already be cited). Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024 and udp.payload[11] == 00' \
  -T fields -e frame.number -e frame.time_relative -e udp.payload \
  | awk '{c1=strtonum("0x" substr($3, 25, 2)); mox=(c1 % 2); if(mox != prev){print $1, $2, "MOX="mox; prev=mox}}'
```
Expected: either an empty list (no keying) or a sequence of MOX-on/MOX-off transitions.

- [ ] **Step 2A: If MOX events were found, document the bring-up**

For the first MOX-on event at frame `N`, dump frames `N-10..N+30` of EP2 traffic with full decode (same approach as Task 8 Step 2). Then write `## 8. TX / Keying Trace` containing:
- The list of MOX events with timestamps.
- A walked-through bring-up timeline: every C0 the host sends across the MOX-on transition, in order, with decoded values (drive level, TX freq, Alex relay, then MOX).
- Note any change in EP6 status bytes during TX (the C0 status index map may shift).
- Note: HL2 carries TX I/Q out via the payload region of EP2 frames during TX — describe what the payload looks like during the keyed window (first/last few bytes of one mid-TX EP2 frame).

- [ ] **Step 2B: If NO MOX events were found, document the gap explicitly**

Write `## 8. TX / Keying Trace` containing only:
- A note that the capture contains **no MOX-on events**.
- The exact tshark detection query used (the one from Step 1).
- A line: "Phase 3L will need a separate capture covering a keying event before TX paths can be implemented from ground truth."

- [ ] **Step 3: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): document TX/keying trace (or absence thereof)

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Section 9 — HL2-specific quirks

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Pull HL2 firmware/version from the discovery reply**

Re-use the discovery reply hex dump from Section 2. Per `NetworkIO.cs`, the reply contains a board-id byte and a firmware version byte at known offsets — cite the line numbers and record the actual values.

- [ ] **Step 2: List sample rates exercised**

The sample-rate field lives in C0=0x00 C1 (per `NetworkIO.cs` — verify). Run:
```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024 and udp.payload[11] == 00' \
  -T fields -e udp.payload \
  | awk '{print toupper(substr($0, 25, 2))}' \
  | sort -u
```
Decode each value against the spec (00=48k, 01=96k, 02=192k, 03=384k typical). Record what's exercised.

- [ ] **Step 3: Check ADC overload bit usage in EP6 status**

Look at the EP6 C0=0x00 status frames pulled in Task 5. The overload bit lives in C1 — cite the `NetworkIO.cs` line and record whether it ever sets in this capture.

- [ ] **Step 4: Append Section 9 to the doc**

Write `## 9. HL2-Specific Observations` containing the firmware version, board ID, sample rates exercised, ADC overload bit observation, and any C0 indices the table from Task 6 marked `unknown — HL2-specific`. Be explicit that these are observations from one HL2 on one firmware revision and may not generalize.

- [ ] **Step 5: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): document HL2-specific observations

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 11: Section 10 — Nereus Phase 3L implementation checklist

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Re-read every preceding section and extract action items**

For each section, ask: "what does Nereus need to *do* to match what this section observes?" Examples (do not copy verbatim — derive from the actual sections written):
- Send a discovery broadcast matching the layout in Section 2.
- Recognize the discovery reply and extract MAC + firmware version per Section 2.
- Build start/stop frames per Section 3, including the sample-rate mode bit.
- Parse 1032-byte EP6 frames per Section 4, deinterleave 24-bit BE I/Q, route C0=0x00 status to the radio model (overload bit).
- Build EP2 round-robin commands per Section 5, including each C0 actually observed.
- Maintain the cadence in Section 6 — emit EP2 at the observed rate, not faster.
- Implement the band-change command sequence from Section 7 atomically when retuning across HPF/LPF boundaries.
- (If Section 8 has TX content) implement MOX bring-up in the order shown.
- (If Section 8 is the gap note) flag TX as blocked on a future capture.

- [ ] **Step 2: Append Section 10 to the doc**

Write `## 10. Nereus Implementation Checklist (Phase 3L)` as a checkbox list. Each item is a one-line task and links to the section that motivates it (e.g., "see §4"). Group by phase: discovery → start → RX steady-state → tuning → band switching → TX.

- [ ] **Step 3: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): add Phase 3L implementation checklist derived from capture

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 12: Appendix A — reproducibility

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1-capture-reference.md`

- [ ] **Step 1: Append Appendix A**

Add `## Appendix A — Reproducing This Analysis` containing:
- A one-sentence preamble: "Every hex dump and statistic in this document was produced with the commands below, run against `HL2_Packet_Capture.pcapng` on macOS with Wireshark's `tshark` 4.x."
- The exact commands from Tasks 1, 3, 4, 5, 6, 7, 8, 9, 10 — each in its own fenced `bash` block with a one-line caption above it explaining what it produces.
- A closing line: "To re-run this analysis on a different P1 capture, replace the source IP in the `-Y` filters with the new radio's IP and re-run each block in order."

- [ ] **Step 2: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1-capture-reference.md
git commit -S -m "$(cat <<'EOF'
docs(p1): add reproducibility appendix with tshark commands

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 13: Update the existing P1 placeholder doc to point at the new reference

**Files:**
- Modify: `docs/protocols/openhpsdr-protocol1.md`

- [ ] **Step 1: Read the current placeholder**

Run: `cat docs/protocols/openhpsdr-protocol1.md`
The current file is the 32-line placeholder identified in the spec.

- [ ] **Step 2: Replace its content with this**

```markdown
# OpenHPSDR Protocol 1

## Status: Reference

Protocol 1 is the original OpenHPSDR protocol used by Metis, Hermes, Angelia,
Orion, and Hermes Lite 2 boards. UDP-only on port 1024, 1032-byte Metis
frames, 24-bit big-endian interleaved I/Q.

## Primary Reference

For byte-level layouts, command/status decode, cadence, and HL2-specific
quirks, see the annotated capture reference:

**[openhpsdr-protocol1-capture-reference.md](openhpsdr-protocol1-capture-reference.md)**

That document is derived from a live HL2↔Thetis capture and is the
authoritative source for NereusSDR Phase 3L (P1 implementation).

## Quick Summary

- **Transport:** UDP only, port 1024
- **Frame size:** 1032 bytes (Metis frame: 8-byte header + two 512-byte USB frames)
- **I/Q format:** 24-bit big-endian, interleaved
- **Control:** C&C bytes in USB frame headers (C0 round-robin index, C1–C4 payload)
- **Discovery:** UDP broadcast to port 1024

## Official Specification

See: https://openhpsdr.org/wiki/index.php?title=Protocol_1
```

- [ ] **Step 3: Commit**

```bash
git add docs/protocols/openhpsdr-protocol1.md
git commit -S -m "$(cat <<'EOF'
docs(p1): point placeholder doc at the new capture reference

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 14: Update CLAUDE.md documentation index

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 1: Read the current Protocol Reference table**

Run: `grep -n 'openhpsdr-protocol' CLAUDE.md`
Expected: two lines in the `### Protocol Reference` table.

- [ ] **Step 2: Add a row for the new reference doc**

Edit `CLAUDE.md` in the `### Protocol Reference (`docs/protocols/`)` table. After the existing `openhpsdr-protocol1.md` row, insert:

```markdown
| [openhpsdr-protocol1-capture-reference.md](docs/protocols/openhpsdr-protocol1-capture-reference.md) | Annotated HL2↔Thetis capture: discovery, start, EP6/EP2 frames, C0 maps, cadence, band/TX traces, Phase 3L checklist |
```

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -S -m "$(cat <<'EOF'
docs(p1): index the capture-reference doc in CLAUDE.md

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 15: Final review

**Files:**
- Read-only: all of the above.

- [ ] **Step 1: Re-read the new doc end-to-end**

Run: `wc -l docs/protocols/openhpsdr-protocol1-capture-reference.md` — expect 600–1200 lines.
Then `cat` it (or open in editor) and verify:
- No `TBD`, `TODO`, `XXX`, or `FIXME`.
- Every section has at least one hex dump or table backed by the capture.
- Every byte-layout claim has a `NetworkIO.cs:<line>` citation.
- Section 10's checklist references every preceding section.
- Appendix A's commands are the actual ones used (copy-paste from earlier task steps if needed).

- [ ] **Step 2: Verify no source code was modified**

Run: `git log --since='2026-04-12' --name-only` — expect only `.md` files in `docs/` and `CLAUDE.md`.

- [ ] **Step 3: Report back to the user**

Summary line: "P1 capture reference complete: <N> lines, <M> commits, all sections grounded in HL2_Packet_Capture.pcapng. Section 8 (TX) was [populated / marked as a gap]. Ready for Phase 3L planning."

- [ ] **Step 4: No commit** — review is read-only.
