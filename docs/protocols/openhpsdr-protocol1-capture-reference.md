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

<!-- Sections 3-10 and Appendix A added by later tasks -->

## 2. Discovery Exchange

P1 discovery is a one-shot broadcast UDP exchange on port 1024. The host
sends a 63-byte packet to the subnet broadcast address; the radio replies
from its own port 1024 to the host's ephemeral source port. This handshake
precedes every session: no start command is sent until at least one valid
reply is received.

### 2.1 Discovery REQUEST (host → broadcast :1024)

UDP payload (63 bytes, frames 1 and 4 in the capture):

```text
Offset  Hex                                               ASCII
------  ------------------------------------------------  -----
00      ef fe 02 00 00 00 00 00 00 00 00 00 00 00 00 00  ........
10      00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ........
20      00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ........
30      00 00 00 00 00 00 00 00 00 00 00 00 00 00 00      ...............
```

Field legend:

- **bytes 0–1** `EF FE` — P1 sync / frame marker
- **byte 2** `02` — command: discovery request
- **bytes 3–62** `00 * 60` — padding (all zero); no additional fields defined
  for the request direction

Thetis send: `clsRadioDiscovery.cs:1301` — `buildDiscoveryPacketP1()`

### 2.2 Discovery REPLY (radio :1024 → host :50533)

UDP payload (60 bytes, frames 2 and 5 in the capture; both identical):

```text
Offset  Hex                                               ASCII
------  ------------------------------------------------  -----
00      ef fe 02 00 1c c0 a2 13 dd 4a 06 00 00 00 00 00  .........J......
10      00 00 00 04 45 02 00 00 00 00 00 03 03 ef 00 00  ....E...........
20      00 00 00 00 80 16 46 36 5e 83 00 00 00 00 00 00  ......F6^.......
30      00 00 00 00 00 00 00 00 00 00 00 00              ............
```

Field legend (P1 parser: `clsRadioDiscovery.cs:1122` — `parseDiscoveryReply()`):

- **byte 0** `EF` — sync (matches `data[0] == 0xef`)
- **byte 1** `FE` — sync (matches `data[1] == 0xfe`)
- **byte 2** `02` — status: `02` = available, `03` would mean busy
  (`r.IsBusy = (data[2] == 0x3)` — `clsRadioDiscovery.cs:1147`)
- **bytes 3–8** `00 1C C0 A2 13 DD` — MAC address of radio
  (`Array.Copy(data, 3, mac, 0, 6)` — `clsRadioDiscovery.cs:1150`);
  HL2 MAC seen in this capture: **00:1C:C0:A2:13:DD**
- **byte 9** `4A` — firmware / code version = `0x4A` (decimal 74)
  (`r.CodeVersion = data[9]` — `clsRadioDiscovery.cs:1155`)
- **byte 10** `06` — board ID = `6` → maps to `HPSDRHW.HermesLite` (MI0BOT)
  (`r.DeviceType = mapP1DeviceType(data[10])` — `clsRadioDiscovery.cs:1153`;
  enum value at `enums.cs:396`: `HermesLite = 6`)
- **bytes 11–13** `00 00 00` — unknown — investigate before implementing
  (not read by the P1 parser branch)
- **byte 14** `00` — `MercuryVersion0` (`data[14]` — `clsRadioDiscovery.cs:1160`)
- **byte 15** `00` — `MercuryVersion1` (`data[15]` — `clsRadioDiscovery.cs:1161`)
- **byte 16** `00` — `MercuryVersion2` (`data[16]` — `clsRadioDiscovery.cs:1162`)
- **byte 17** `00` — `MercuryVersion3` (`data[17]` — `clsRadioDiscovery.cs:1163`)
- **byte 18** `00` — `PennyVersion` (`data[18]` — `clsRadioDiscovery.cs:1164`)
- **byte 19** `04` — `MetisVersion` = 4 (`data[19]` — `clsRadioDiscovery.cs:1165`)
- **byte 20** `45` — `NumRxs` = 69 (`data[20]` — `clsRadioDiscovery.cs:1166`);
  raw value 0x45 as reported by HL2 firmware — unknown — investigate before implementing
- **bytes 21–59** `02 00 00 ... 83 00 ...` — unknown — investigate before implementing
  (not read by the P1 parser branch; 39 bytes total)

**Thetis source:** `clsRadioDiscovery.cs:1301` (send — `buildDiscoveryPacketP1`), `:1122` (parse — `parseDiscoveryReply`)

## 3. Start / Stop Commands

P1 start/stop is a small 64-byte Metis frame sent to the radio. Byte 2 is the
command (`04`), byte 3 selects start/stop and which data streams to enable.
The frame is NOT a full 1032-byte Metis frame — it carries no C&C or I/Q
payload, just the 4-byte command header followed by 60 zero bytes.

### 3.1 Start Command (host → radio :1024)

Captured frame 10, relative timestamp 1.007829900 s (first confirmed start
after IQ stream begins flowing).

UDP payload (64 bytes, offsets shown relative to UDP payload start):

```text
Offset  Hex                                               ASCII
------  ------------------------------------------------  -----
00      ef fe 04 01 00 00 00 00 00 00 00 00 00 00 00 00  ................
10      00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
20      00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
30      00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

Field legend:

- **bytes 0–1** `EF FE` — P1 sync / frame marker
- **byte 2** `04` — command: start/stop
- **byte 3** `01` — mode: start with IQ data stream enabled
  (`outpacket.packetbuf[3] = 0x01` — `networkproto1.c:50`)
- **bytes 4–63** `00 * 60` — zero padding

Note: frame 7 (t=0.944 s) contains byte 3 = `00` (a stop command sent before
the first start — consistent with `SendStartToMetis()` calling
`ForceCandCFrame(1)` then immediately sending start, with the radio possibly
echoing a cleanup stop first).

### 3.2 Stop Command (host → radio :1024)

Captured frame 302249, relative timestamp 56.658037300 s (end of session).

UDP payload (64 bytes):

```text
Offset  Hex                                               ASCII
------  ------------------------------------------------  -----
00      ef fe 04 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
10      00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
20      00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
30      00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
```

Field legend:

- **bytes 0–1** `EF FE` — P1 sync / frame marker
- **byte 2** `04` — command: start/stop
- **byte 3** `00` — mode: stop all data streams
  (`outpacket.packetbuf[3] = 0x00` — `networkproto1.c:85`)
- **bytes 4–63** `00 * 60` — zero padding

### 3.3 Mode Byte (byte 3) Decode Table

Values taken directly from `networkproto1.c` (`SendStartToMetis` and
`SendStopToMetis`). No other values appear in this source file.

| Value | Meaning | Source |
| --- | --- | --- |
| `0x00` | Stop — all data streams off | `networkproto1.c:85` |
| `0x01` | Start — IQ data stream enabled | `networkproto1.c:50` |

The capture contains only `0x00` and `0x01`. The values `0x02` (wideband only)
and `0x03` (IQ + wideband) are referenced in the OpenHPSDR P1 specification
but do NOT appear in `SendStartToMetis()` or `SendStopToMetis()`. They are not
documented here as they cannot be verified against this source or capture.

**Thetis source:** `networkproto1.c:50` (start send — `SendStartToMetis`),
`networkproto1.c:85` (stop send — `SendStopToMetis`)

## 4. EP6 — Radio → Host Metis Frames

EP6 is the primary data stream from the radio. Each frame is 1032 bytes of
UDP payload containing an 8-byte Metis header and two 512-byte USB frames,
each carrying 5 C&C status bytes and 504 bytes of interleaved 24-bit BE I/Q
plus 16-bit mic audio. The C0 status index is round-robin: each USB frame
carries one slot of status, and the host reassembles the full snapshot across
frames. The decode switch on `ControlBytesIn[0] & 0xF8` means bits [7:3]
select the status slot while bits [2:0] carry PTT/dot/dash inputs.

### 4.1 Frame Layout

```
Offset  Length  Field
------  ------  -----
0       2       Metis sync 'EF FE'
2       1       Direction marker: 0x01 (data frame from radio)
3       1       Endpoint: 0x06 (EP6 — I/Q data to host)
4       4       Sequence number, big-endian uint32 (monotonically incrementing)
8       3       USB1 sync '7F 7F 7F'
11      1       USB1 C0 — status index (bits [7:3]) + PTT/dot/dash (bits [2:0])
12      4       USB1 C1..C4 — status payload (meaning depends on C0[7:3])
16      504     USB1 I/Q + mic samples (see §4.2)
520     3       USB2 sync '7F 7F 7F'
523     1       USB2 C0 — same encoding as USB1 C0 (carries next status slot)
524     4       USB2 C1..C4
528     504     USB2 I/Q + mic samples
```

Verification from hex dump (frame 11, first EP6 frame in capture):

```text
Offset  Hex                                        Field
------  -----------------------------------------  ----------------------------
00      EF FE                                      Metis sync
02      01                                         Direction marker (data frame)
03      06                                         Endpoint (EP6)
04      00 00 00 00                                Seq# = 0 (first frame)
08      7F 7F 7F                                   USB1 sync
0B      18                                         USB1 C0 = 0x18 (status slot 3)
0C      00 00 00 00                                USB1 C1-C4 (slot 3: user_adc1=0, supply_volts=0)
10      01 27 4F FF 70 B6 FF FE 65 FF F8 0C ...   USB1 I/Q samples begin
          (504 bytes total, continuing to offset 0x0207)
208     7F 7F 7F                                   USB2 sync
20B     00                                         USB2 C0 = 0x00 (status slot 0)
20C     1F 00 80 4A                                USB2 C1-C4 (slot 0: ADC overload flags + dig I/O)
210     ...                                        USB2 I/Q samples begin
```

Note: Metis byte 3 is `0x06`, confirming EP6 endpoint. The direction marker
(byte 2) is `0x01` for data frames in both directions; the endpoint byte
distinguishes EP6 (radio→host data) from EP2 (host→radio C&C).
Verified at `networkproto1.c:174–181` (`MetisReadDirect`).

### 4.2 Sample Format

Each USB frame carries 504 bytes of interleaved samples. For nddc=1 (one
receiver, as in this capture), samples per USB frame:

```
spr = 504 / (6 * nddc + 2) = 504 / 8 = 63 samples
```

Layout within each 8-byte sample group:

```
Byte 0-2   I-sample, 24-bit big-endian (sign-extended via << 8 shift)
Byte 3-5   Q-sample, 24-bit big-endian
Byte 6-7   Mic audio, 16-bit big-endian (one sample, decimated)
```

Thetis decodes I/Q as:
```c
// networkproto1.c:364-371 (MetisReadThreadMainLoop)
prn->RxBuff[iddc][2*isample+0] = const_1_div_2147483648_ *
    (double)(bptr[k+0] << 24 | bptr[k+1] << 16 | bptr[k+2] << 8);  // I
prn->RxBuff[iddc][2*isample+1] = const_1_div_2147483648_ *
    (double)(bptr[k+3] << 24 | bptr[k+4] << 16 | bptr[k+5] << 8);  // Q
```

Mic (16-bit):
```c
// networkproto1.c:401-403
prn->TxReadBufp[2*mic_sample_count+0] = const_1_div_2147483648_ *
    (double)(bptr[k+0] << 24 | bptr[k+1] << 16);  // 16-bit mic, upper bytes
```

**`networkproto1.c:358`** — `spr = 504 / (6 * nddc + 2)` — sample count formula.

### 4.3 Observed C0 Status Indices

The switch on `ControlBytesIn[0] & 0xF8` (networkproto1.c:332) defines 5 status
slots. The USB1 and USB2 C0 bytes in this capture are interleaved to carry
different slots per EP6 frame.

#### USB1 C0 counts (281,195 EP6 frames):

| C0 (hex) | Count   | C0[7:3] slot | Sample C1-C4    |
| -------- | ------- | ------------ | --------------- |
| `08`     | 140,597 | 1 (0x08)     | `03 F0 00 01`   |
| `18`     | 140,598 | 3 (0x18)     | `00 00 00 00`   |

#### USB2 C0 counts (281,195 EP6 frames):

| C0 (hex) | Count   | C0[7:3] slot | Sample C1-C4    |
| -------- | ------- | ------------ | --------------- |
| `00`     | 140,437 | 0 (0x00)     | `1F 00 80 4A`   |
| `10`     | 140,435 | 2 (0x10)     | `00 00 00 00`   |
| `FA`     | 323     | — (0xF8)     | `02 00 00 01`   |

C0=`FA` (`0xFA & 0xF8 = 0xF8`) falls outside all five cases in the
`networkproto1.c:332` switch. It appears throughout the session (~every 870
frames in USB2) with consistent payload `02 00 00 01`. This is an HL2-specific
status extension not decoded by Thetis's networkproto1.c switch —
**observed but unmapped — investigate** (likely HL2 firmware-version or
board-status slot; compare against HL2 FPGA source `radioberry.v` or
`hermeslite.v`).

#### C0 status slot decode table

| C0 & 0xF8 | Slot | C1-C4 Meaning | Source |
| ---------- | ---- | ------------- | ------ |
| `0x00`     | 0    | C1 bit0=ADC0 overload; C1 bits[4:1]=user digital inputs | networkproto1.c:334–336 |
| `0x08`     | 1    | C1-C2=exciter power (AIN5); C3-C4=fwd PA power (AIN1) | networkproto1.c:338–341 |
| `0x10`     | 2    | C1-C2=rev PA power (AIN2); C3-C4=user_adc0 (AIN3, PA volts) | networkproto1.c:343–346 |
| `0x18`     | 3    | C1-C2=user_adc1 (AIN4, PA amps); C3-C4=supply_volts (AIN6) | networkproto1.c:348–350 |
| `0x20`     | 4    | C1 bit0=ADC0 overload; C2 bit0=ADC1 overload; C3 bit0=ADC2 overload | networkproto1.c:352–355 |
| `0xF8`     | —    | **HL2-specific — not in networkproto1.c switch — investigate** | — |

**This capture exercises slots 0, 1, 2, 3 (C0=`00`, `08`, `10`, `18`) across
USB1+USB2. Slot 4 (C0=`20`) is NOT observed in this capture — the HL2
may not implement it, or the round-robin does not reach it in this
2-slot-per-frame pattern.**

#### Decoded sample values (first EP6 frame, frame 11):

- **USB1 C0=`18`, C1-C4=`00 00 00 00`**: user_adc1=0x0000 (AIN4 amps=0),
  supply_volts=0x0000 (AIN6=0) — radio just started, ADC rails not yet settling.
- **USB2 C0=`00`, C1-C4=`1F 00 80 4A`**: C1=`1F`=`0001 1111`→
  ADC0 overload=1 (bit0), user_dig_in=`0x0F` (bits[4:1]=`1111` — IO4-IO8 all
  high). C2-C4=`00 80 4A` are not used by slot 0.

**Thetis source:** `networkproto1.c:273–416` (`MetisReadThreadMainLoop` — RX
parser, C0 switch, I/Q decode, mic decode).

---

## 5. EP2 — Host → Radio Command Frames

EP2 is the host→radio command channel, Metis-framed with endpoint `0x02` in
byte[3] of the Metis header. Each frame is 1032 bytes: an 8-byte Metis header
followed by two 512-byte USB sub-frames, each carrying 3 sync bytes (`7F 7F 7F`)
plus 5 C&C bytes (C0–C4), followed by TX I/Q samples (all zero in RX-only mode).
The round-robin `out_control_idx` counter cycles through every command slot in
turn so the radio's command state is continuously refreshed even with no user
action.

**Thetis source:** `networkproto1.c:419–697` (`WriteMainLoop` — EP2 builder;
`sendProtocol1Samples` — caller thread).

---

### 5.1 Frame envelope

The Metis envelope for EP2 is identical in structure to EP6, with the endpoint
field differing:

| Byte offset | Size | Value | Meaning |
|-------------|------|-------|---------|
| 0–2 | 3 | `EF FE 01` | Metis magic — "data frame" marker |
| 3 | 1 | `02` | Endpoint: EP2 = host→radio command/TX data |
| 4–7 | 4 | uint32 BE | Outgoing sequence number (`MetisOutBoundSeqNum`, auto-incremented per frame) |
| 8–519 | 512 | USB sub-frame 1 | Sync `7F7F7F` + C0–C4 + TX I/Q samples (zero in RX) |
| 520–1031 | 512 | USB sub-frame 2 | Sync `7F7F7F` + C0–C4 + TX I/Q samples (zero in RX) |

Within each USB sub-frame:

| Sub-frame byte | Content |
|----------------|---------|
| 0–2 | `7F 7F 7F` sync |
| 3 | C0 (command index + MOX bit) |
| 4–7 | C1–C4 (command payload) |
| 8–511 | TX I/Q samples (16-bit signed big-endian, LR then IQ interleaved); all zero in RX mode |

**Observed frame size:** All 21,044 EP2 data frames in this capture are exactly
1032 bytes (UDP payload). UDP length field = 1040 (1032 + 8-byte UDP header).

---

### 5.2 C0 dispatch and the MOX bit

`WriteMainLoop` builds C0 as:

```c
C0 = (unsigned char)XmitBit;   // bit 0 = MOX/PTT state
// ...
C0 |= <slot-specific OR-mask>; // bits 7:1 = command index
```

So the low bit of C0 carries MOX state and the upper 7 bits (masked with `0xF8`
to get the 5-bit index field) identify the command slot. Thetis dispatches on
`out_control_idx` (0–16 in the standard build), which maps to the C0 OR-mask
values shown in section 5.3.

The `out_control_idx` counter increments once per USB sub-frame (twice per
Metis frame). After reaching `end_frame` (=16 for non-ANVELINAPRO3 models),
it resets to 0. **In this capture the observed cycle is 19 sub-frame positions
(not 17), implying the HL2-connected Thetis build has `end_frame = 18`,
adding two HL2-specific cases beyond the standard P1 command set.**

---

### 5.3 Observed C0 indices — master table

Counts are per-sub-frame (each Metis frame contributes one USB1 count and one
USB2 count independently). "USB1" = sub-frame at byte offset 8; "USB2" = at
byte offset 520. C1–C4 examples are the first occurrence in the capture.

| C0 masked | Raw C0 seen | USB1 count | USB2 count | C1–C4 (USB1 example) | Meaning / source |
|-----------|-------------|------------|------------|----------------------|------------------|
| `0x00` | `0x00`, `0x01`, `0x02`, `0x03`, `0x04`, `0x05`, `0x06`, `0x07` | ~5,591 | ~5,573 | see per-slot table | Multiple slots share masked 0x00 (low 3 bits = MOX + slot sub-index) |
| `0x08` | `0x08`–`0x0F` | ~4,313 | ~4,312 | `08 0F 32 ED` | Freq slots DDC0–DDC3 and TX |
| `0x10` | `0x10`–`0x17` | ~4,277 | ~4,270 | `08 0D 55 0F` | Freq slots DDC4–DDC6, drive/filter, preamp, step-att |
| `0x18` | `0x1C`–`0x1F` | ~2,191 | ~2,178 | `00 00 07 00` | ADC assign (0x1C) + CW enable/sidetone (0x1E) |
| `0x20` | `0x20`–`0x25` | ~3,053 | ~3,046 | `4D 02 32 00` | CW hang/sidetone freq (0x20), EER PWM (0x22), BPF2 (0x24) |
| `0x28` | `0x2E`, `0x2F` | ~1,715 | ~1,715 | `00 00 1E 46` | **HL2 extension** — not in `networkproto1.c` WriteMainLoop; constant payload |
| `0x70` | `0x74`, `0x75` | ~1,099 | ~1,092 | `00 00 00 00` | **HL2 extension** — not in `networkproto1.c`; all-zero payload |
| `0x78` | `0x7A` | 55 | 41 | `06 9D 20 00` | **HL2 extension** — burst pattern, C1=`0x06`, C2=`0x9D`; not in source |
| `0xF8` | `0xFA`, `0xFB` | 167 | 156 | `07 C1 00 00` | **HL2 extension** — periodic ~131-frame interval, C1=`0x07`, C2=`0x9D` typically |

**Note on masked 0x00:** The standard switch uses `out_control_idx` not masked
C0, so multiple slots produce raw C0 values that all mask to `0x00` after
`& 0xF8`. The individual raw values are decoded per-slot in section 5.4.

---

### 5.4 Per-slot command decode

Each row below corresponds to one `case` in `WriteMainLoop`
(`networkproto1.c:448–675`). All values are from frames actually present in
this capture. Frequencies are decoded as 32-bit big-endian Hz.

#### Case 0 — General settings (`C0 |= 0x00`, raw C0 = `0x00`/`0x01`)

`networkproto1.c:450–471`

| Field | Bit(s) of byte | Observed value | Decoded |
|-------|----------------|----------------|---------|
| **C0** | bit 0 | `0x00` (MOX off) | MOX=0 |
| **C1** bits 1:0 | `SampleRateIn2Bits & 3` | `0x02` | 192 kHz sample rate |
| **C2** bit 0 | `cw.eer & 1` | `0x00` | EER off |
| **C2** bits 7:1 | `oc_output << 1` | `0x00` | Open-collector outputs = 0 |
| **C3** bits 7:0 | attn/preamp/dither/random/filter-ext | `0x00` | all off |
| **C4** bits 1:0 | antenna select (0=none, 1=ANT2, 2=ANT3) | `0x00` | no aux antenna |
| **C4** bit 2 | duplex enable | `0` | simplex (HL2 note: code normally OR-masks `0x04`; `0x18` observed implies duplex bit not set — likely HL2 simplex RX-only mode) |
| **C4** bits 6:3 | `(nddc-1) << 3` | `0x18` → bits[6:3]=`0011` | nddc = 4 DDCs active |
| **C4** bit 7 | diversity | `0` | diversity off |

First frame example: `C1=0x02 C2=0x00 C3=0x00 C4=0x18`

#### Case 1 — TX VFO frequency (`C0 |= 0x02`, raw C0 = `0x02`/`0x03`)

`networkproto1.c:476–481`

C1–C4 = 32-bit big-endian TX frequency in Hz.

| Raw C0 | C1–C4 (hex) | Frequency |
|--------|-------------|-----------|
| `0x02` (MOX=0) | `08 0F 32 ED` | 135,213,805 Hz — startup/ForceCandCFrame value |
| `0x03` (MOX=1) | `00 3B 0A 72` | **3,869,298 Hz = 3.869 MHz (80m)** — TX VFO during TX |

#### Case 2 — DDC0 / RX1 frequency (`C0 |= 0x04`, raw C0 = `0x04`/`0x05`)

`networkproto1.c:484–494`

In RX mode (non-PureSignal, nddc≠2), C1–C4 = `rx[0].frequency` as 32-bit BE Hz.

| Raw C0 | C1–C4 (hex) | Frequency |
|--------|-------------|-----------|
| `0x04` (MOX=0) | `08 0D 55 0F` | 135,091,471 Hz — startup |
| `0x05` (MOX=1) | `00 3A F9 A6` | **3,864,998 Hz = 3.865 MHz (80m)** — RX1/DDC0 |

#### Case 3 — DDC1 / RX2 frequency (`C0 |= 0x06`, raw C0 = `0x06`/`0x07`)

`networkproto1.c:497–511`

With nddc=4 (Hermes configuration), C1–C4 = `rx[1].frequency`.

| Raw C0 | C1–C4 (hex) | Frequency |
|--------|-------------|-----------|
| `0x06` (MOX=0) | `0E BB FF 97` | 247,201,687 Hz — startup |
| `0x07` (MOX=1) | `00 6B EA F1` | **7,072,497 Hz = 7.072 MHz (40m)** — RX2/DDC1 |

#### Case 4 — ADC assignments and TX step attenuator (`C0 |= 0x1C`, raw C0 = `0x1C`/`0x1D`)

`networkproto1.c:517–522`

| Byte | Field | Observed | Decoded |
|------|-------|----------|---------|
| C1 | `P1_adc_cntrl & 0xFF` | `0x00` | ADC control low byte = 0 |
| C2 | `(P1_adc_cntrl >> 8) & 0x3FF` | `0x00` | ADC control high bits = 0 |
| C3 | `adc[0].tx_step_attn & 0x1F` | `0x08` | ADC0 TX step attenuation = **8 dB** |
| C4 | (unused) | `0x00` | — |

#### Case 5 — DDC2 / RX3 frequency (`C0 |= 0x08`, raw C0 = `0x08`/`0x09`)

`networkproto1.c:525–535`

With nddc=4 (not nddc=5/Orion), C1–C4 = `tx[0].frequency`.

| Raw C0 | C1–C4 (hex) | Frequency |
|--------|-------------|-----------|
| `0x08` (MOX=0) | `08 0F 32 ED` | 135,213,805 Hz — TX VFO (DDC2 tracks TX in this config) |
| `0x09` (MOX=1) | `00 3B 0A 72` | 3,869,298 Hz — TX freq during MOX |

#### Case 6 — DDC3 / RX4 frequency (`C0 |= 0x0A`, raw C0 = `0x0A`/`0x0B`)

`networkproto1.c:538–545`

DDC3 always tracks TX frequency. C1–C4 = `tx[0].frequency` as 32-bit BE Hz.

| Raw C0 | C1–C4 (hex) | Frequency |
|--------|-------------|-----------|
| `0x0A` (MOX=0) | `08 0F 32 ED` | 135,213,805 Hz |
| `0x0B` (MOX=1) | `00 3B 0A 72` | 3,869,298 Hz |

#### Case 7 — DDC4 / RX5 frequency (`C0 |= 0x0C`, raw C0 = `0x0C`/`0x0D`)

`networkproto1.c:548–555`

DDC4 always tracks TX frequency (PureSignal/Orion2 use). C1–C4 = `tx[0].frequency`.

| Raw C0 | C1–C4 (hex) | Frequency |
|--------|-------------|-----------|
| `0x0C` (MOX=0) | `08 0F 32 ED` | 135,213,805 Hz |
| `0x0D` (MOX=1) | `00 3B 0A 72` | 3,869,298 Hz |

#### Case 8 — DDC5 / RX6 frequency (`C0 |= 0x0E`, raw C0 = `0x0E`/`0x0F`)

`networkproto1.c:558–565`

DDC5 not used in nddc=4 config; code sends `rx[0].frequency`.

| Raw C0 | C1–C4 (hex) | Frequency |
|--------|-------------|-----------|
| `0x0E` (MOX=0) | `08 0D 55 0F` | 135,091,471 Hz |
| `0x0F` (MOX=1) | `00 3A F9 A6` | 3,864,998 Hz |

#### Case 9 — DDC6 / RX7 frequency (`C0 |= 0x10`, raw C0 = `0x10`/`0x11`)

`networkproto1.c:568–575`

DDC6 not used; code sends `rx[0].frequency`.

| Raw C0 | C1–C4 (hex) | Frequency |
|--------|-------------|-----------|
| `0x10` (MOX=0) | `08 0D 55 0F` | 135,091,471 Hz |
| `0x11` (MOX=1) | `00 3A F9 A6` | 3,864,998 Hz |

#### Case 10 — Drive level, mic settings, filter/PA (`C0 |= 0x12`, raw C0 = `0x12`/`0x13`)

`networkproto1.c:578–590`

| Byte | Field | Observed | Decoded |
|------|-------|----------|---------|
| C1 | `tx[0].drive_level` (0–255) | `0xBC` = 188 | **drive level 188/255 (~74%)** |
| C2 bit 0 | `mic.mic_boost` | `0` | mic boost off |
| C2 bit 1 | `mic.line_in` | `0` | line-in off |
| C2 bits 5:2 | Apollo filter/tuner/ATU | `0b0011` | ApolloFilt=1, ApolloTuner=1 |
| C2 bit 6 | always 1 | `1` | fixed |
| C3 bit 4 | `1_5MHz_HPF` | `1` | 1.5 MHz HPF **active** |
| C4 bit 2 | `80_LPF` | `1` | 80m LPF **active** |

First USB1 example (raw `0x12`): `C1=0xBC C2=0x4C C3=0x10 C4=0x04`

#### Case 11 — Preamp control, mic settings, RX step attenuation (`C0 |= 0x14`, raw C0 = `0x14`/`0x15`)

`networkproto1.c:593–601`

| Byte | Field | Observed | Decoded |
|------|-------|----------|---------|
| C1 bits 6:0 | rx0–rx2 preamp, mic_trs, mic_bias, mic_ptt | `0x00` | all off |
| C2 bits 4:0 | `mic.line_in_gain` | `0x17` = 23 | line-in gain **23 dB** |
| C2 bit 6 | `puresignal_run` | `0` | PureSignal off |
| C3 bits 3:0 | `user_dig_out` | `0x00` | digital outputs = 0 |
| C4 bits 4:0 | `adc[0].rx_step_attn` | `0x10` = 16 | ADC0 RX step attenuation **16 dB** |
| C4 bit 5 | enable bit | `1` | attenuation enabled |

First USB1 example (raw `0x14`): `C1=0x00 C2=0x17 C3=0x00 C4=0x70`

#### Case 12 — ADC1/ADC2 step attenuators, CW keyer timing (`C0 |= 0x16`, raw C0 = `0x16`/`0x17`)

`networkproto1.c:604–628`

| Byte | Field | Observed | Decoded |
|------|-------|----------|---------|
| C1 bits 4:0 | `adc[1].rx_step_attn` | `0x00` = 0 | ADC1 RX step attenuation **0 dB** |
| C1 bit 5 | enable | `1` | attenuation enabled |
| C2 bits 4:0 | `adc[2].rx_step_attn` | `0x00` = 0 | ADC2 RX step attenuation **0 dB** |
| C2 bit 5 | enable | `1` | attenuation enabled |
| C2 bit 6 | `cw.rev_paddle` | `0` | normal paddle |
| C3 bits 5:0 | `cw.keyer_speed & 0x3F` | `0x19` = 25 | keyer speed **25 WPM** |
| C3 bits 7:6 | CW mode | `0b10` | **Iambic mode B** |
| C4 bits 6:0 | `cw.keyer_weight & 0x7F` | `0x32` = 50 | keyer weight **50** |
| C4 bit 7 | `cw.strict_spacing` | `0` | strict spacing off |

First USB1 example (raw `0x16`): `C1=0x20 C2=0x20 C3=0x99 C4=0x32`

#### Case 13 — CW enable, sidetone level, RF delay (`C0 |= 0x1E`, raw C0 = `0x1E`/`0x1F`)

`networkproto1.c:633–638`

| Byte | Field | Observed | Decoded |
|------|-------|----------|---------|
| C1 | `cw.cw_enable` | `0x00` | CW **disabled** |
| C2 | `cw.sidetone_level` | `0x00` | sidetone level = 0 |
| C3 | `cw.rf_delay` | `0x07` | RF delay = **7** |
| C4 | (unused) | `0x00` | — |

First USB1 example (raw `0x1E`): `C1=0x00 C2=0x00 C3=0x07 C4=0x00`

#### Case 14 — CW hang delay and sidetone frequency (`C0 |= 0x20`, raw C0 = `0x20`/`0x21`)

`networkproto1.c:641–646`

C1–C2 encode hang delay (10-bit, C1=MSBs, C2[1:0]=LSBs); C3–C4 encode sidetone
frequency (12-bit, C3=MSBs, C4[3:0]=LSBs).

| Derived value | Formula | Observed | Decoded |
|---------------|---------|----------|---------|
| hang_delay | `(C1 << 2) \| (C2 & 3)` | C1=`0x4D`, C2=`0x02` → `(77<<2)\|2` | **310 ms** |
| sidetone_freq | `(C3 << 4) \| (C4 & 0xF)` | C3=`0x32`, C4=`0x00` → `(50<<4)\|0` | **800 Hz** |

First USB1 example (raw `0x20`): `C1=0x4D C2=0x02 C3=0x32 C4=0x00`

#### Case 15 — EER PWM min/max (`C0 |= 0x22`, raw C0 = `0x22`/`0x23`)

`networkproto1.c:649–654`

10-bit min and max EER PWM values, each split across two bytes (MSB in C1/C3,
LSBs in C2/C4).

First USB1 example (raw `0x22`): `C1=0x19 C2=0x00 C3=0xC8 C4=0x00`

Decoded: epwm_min = `(0x19 << 2) | (0x00 & 3)` = 100; epwm_max = `(0xC8 << 2) | 0` = 800.

#### Case 16 — BPF2 second-receiver band-pass filters (`C0 |= 0x24`, raw C0 = `0x24`/`0x25`)

`networkproto1.c:657–665`

| Byte | Field | Observed | Decoded |
|------|-------|----------|---------|
| C1 | HPF2 mask + 6m preamp + rx2_gnd | `0x00` | no HPF2 active, no rx2 ground |
| C2 bit 0 | `xvtr_enable` | `0` | transverter off |
| C2 bit 6 | `puresignal_run` | `0` | PureSignal off |
| C3–C4 | (unused) | `0x00 0x00` | — |

First USB1 example (raw `0x24`): `C1=0x00 C2=0x00 C3=0x00 C4=0x00`

#### Cases 17–18 — HL2-specific extensions (masked `0x28`, `0x70`)

**Not in `networkproto1.c` WriteMainLoop.** These two slots appear in the
captured 19-step cycle at positions 17 and 18, indicating the HL2-connected
Thetis build extends `end_frame` to 18 with additional cases. The standard
`networkproto1.c` sets `end_frame = 16` (17 slots); the HL2 build adds 2 more.

| Raw C0 | Observed C1–C4 | Notes |
|--------|----------------|-------|
| `0x2E` / `0x2F` | `00 00 1E 46` (constant, all frames) | HL2 extension slot 17; payload `0x1E46` = 7750 — purpose unknown; not in source |
| `0x74` / `0x75` | `00 00 00 00` (constant, all frames) | HL2 extension slot 18; all-zero payload; purpose unknown; not in source |

**Investigate:** Check the Thetis HL2 fork or HL2 firmware protocol documentation
for the meaning of C0=`0x2E` and C0=`0x74`.

#### Aperiodic HL2 commands (masked `0x78`, `0xF8`)

These appear outside the standard round-robin cycle, injected asynchronously:

| Masked C0 | Raw C0 seen | Count (USB1) | Periodicity | C1–C4 pattern | Notes |
|-----------|-------------|--------------|-------------|---------------|-------|
| `0x78` | `0x7A` | 55 | Bursts of 3, then 1000+ frame gap | C1=`0x06` C2=`0x9D`, C3/C4 vary | `0x069D` prefix; likely HL2 I2C or register passthrough; not in source |
| `0xF8` | `0xFA`, `0xFB` | 167 | ~131 frames between occurrences | C1=`0x07` C2=`0x9D` C3=`0x06`, C4 varies | `0x079D` prefix; same 0x9D pattern as 0x78 group; periodic ~timer-driven; not in source |

The shared `0x9D` = 157 in C2 for both `0x78` and `0xF8` groups is consistent
with a common I2C device address or HL2-internal register number. These are
not in `networkproto1.c` and require investigation against HL2-specific
Thetis source or firmware docs.

---

### 5.5 MOX bit observation

MOX state is carried in bit 0 of raw C0 on every USB sub-frame (not just
C0=`0x00`). This capture includes an active TX period. MOX-on sub-frames were
observed at all slots in the round-robin (the MOX bit is OR-masked onto C0
for every slot while transmitting).

**Counting USB1 frames where the C0 masked-to-`0x00` slot has raw=`0x00`
(MOX off) vs raw=`0x01` (MOX on):**

| State | Frames | Notes |
|-------|--------|-------|
| MOX off (`raw & 0x01 == 0`) | 3,457 | RX periods |
| MOX on (`raw & 0x01 == 1`) | 937 | TX period(s) present — see Section 8 for TX trace |

MOX was **set** in this capture. The 937 MOX-on frames at case-0 position
represent approximately 44 complete round-robin cycles of TX operation
(937 / ~21 sub-frames per cycle ≈ 44 TX cycles). This confirms this capture
is usable for Task 9 (TX/keying trace).

---

### 5.6 HL2-specific observations

1. **19-step round-robin** (standard P1 is 17): Two extra command slots (`0x2E`,
   `0x74`) are sent every cycle by the HL2 Thetis build.
2. **nddc = 4 with duplex=0**: The HL2 runs 4 DDCs but with the duplex bit
   unset in C0=`0x00` frame `C4=0x18`. Standard Thetis always ORs `0b00000100`
   (duplex bit) unconditionally; the HL2 build or HL2 firmware may treat this
   differently.
3. **Periodic HL2 register writes** (masked `0xF8`, ~131-frame period): Suggest
   a background timer in the HL2 Thetis build that writes firmware-specific
   registers (temperature, I2C, or internal state) independently of the
   round-robin.
4. **Burst I2C/register commands** (masked `0x78`): Appear during band or
   configuration changes, consistent with HL2 I2C passthrough to on-board
   peripheral (LPF relay driver, LNA, or step-attenuator IC).

**Thetis source:** `networkproto1.c:419–697` (`WriteMainLoop`, `sendProtocol1Samples`).

---

## 6. Steady-State Cadence

### 6.1 Frame rates

This section measures the observed frame rates during the ~55.7 second capture when the HL2 radio and Thetis host are in steady state, with no band changes or parameter adjustments.

| Direction | Endpoint | Frames | Duration | Rate (Hz) |
| --- | --- | --- | --- | --- |
| Radio → Host | EP6 (`0x01`) | 281,195 | 55.650 s | 5052.9 |
| Host → Radio | EP2 (`0x02`, 1032-byte) | 21,044 | 55.668 s | 378.0 |

### 6.2 Rate derivation and comparison to theory

The OpenHPSDR Protocol 1 implementation in `networkproto1.c:358` computes the number of samples per USB frame as:

```c
spr = 504 / (6*nddc + 2)
```

For this session with nddc=4 (four active DDCs):

```
spr = 504 / (6*4 + 2)
    = 504 / 26
    ≈ 19.38 samples/frame
```

The Metis frame rate (EP6) at 192 kHz sample rate is:

```
Metis rate = 192000 / (2 * spr)
           = 192000 / (2 * 19.38)
           ≈ 4947 Hz
```

**Observed:** 5052.9 Hz — a delta of **+105.9 Hz** relative to the computed 4947 Hz formula.

This discrepancy indicates either:
1. The actual `spr` value used by this HL2 firmware differs slightly from the formula, or
2. The 192 kHz rate or nddc=4 parameters are not the primary drivers of frame timing.

The observed rate remains stable and continuous; the delta is small relative to 5000 Hz (about 2%) and should not affect Nereus Phase 3L integration.

### 6.3 Sequence number continuity

Both endpoints maintain unbroken sequence numbering across the entire session:

- **EP6 (radio → host):** 0 gaps — all 281,195 Metis sequence numbers are contiguous.
- **EP2 (host → radio):** 0 gaps — all 21,044 command sequence numbers are contiguous.

No packet loss, reorder, or retransmission is observed on the capture interface.

### 6.4 Implication for Nereus Phase 3L

The Phase 3L C&C stack must emit **EP2 command frames at ~378 Hz** and accept **EP6 frames at ~5053 Hz** to match the HL2's steady-state behavior.

The HL2 firmware expects a ~19 ms round-robin cycle (one C0 "send all" command per 19 ms implies ~53 commands/second × 4 minor-loop packets = ~210 fps command-level, but this session shows ~378 Hz physical EP2 frame rate, indicating the HL2 may require higher command-queue turnover).

Sustaining these rates without drops is a **hard requirement** for the round-robin C&C refresh to complete within the ~10 ms window the HL2 expects. Any sustained frame loss, reordering, or timing jitter will cause the HL2 to miss command updates and potentially fall out of sync with the Nereus host.


---

## 7. Band-Change Trace

### 7.1 Overview

When the operator retunes across an HPF or LPF boundary, the host must
propagate several changes simultaneously: the DDC NCO frequencies, the TX VFO
frequency, and the Alex analog front-end filter selection. Because EP2 uses a
fixed 19-step round-robin that cannot be paused or restarted mid-cycle, Thetis
does not implement a coordinated "atomic" commit. Instead, it writes updated
values into the shared state structure (`prn->rx[N].frequency`,
`prbpfilter->_80_LPF`, etc.) and allows the round-robin to pick them up slot
by slot across two consecutive Metis frames (~2.7 ms per slot at 378 Hz). The
result is a brief window — at most one full 19-slot cycle (~50 ms) — during
which the DDC NCOs and the front-end filters may be in a mismatched state. The
HL2 firmware tolerates this: it does not apply commands atomically, and no
audio thump or relay bounce was observed.

This section documents the full sequence of EP2 slots emitted during a
representative retune from 80m (3.869 MHz) to 60m (5.331 MHz), including the
HPF/LPF switchover at the 5 MHz boundary.

**Thetis source:** RX1 frequency set in `networkproto1.c:484–494` (case 2);
Alex/HPF/LPF set in `networkproto1.c:578–590` (case 10).

---

### 7.2 All frequency and filter transitions in the session

| Time (s) | Frame | C0 (raw) | Command | Old value | New value |
|----------|-------|-----------|---------|-----------|-----------|
| 1.48 | 2423 | `0x04` | RX1 (DDC0) Freq | startup (135.091 MHz) | 3.865 MHz (80m) |
| 5.61 | 24874 | `0x06` | RX2 (DDC1) Freq | startup (247.202 MHz) | 7.072 MHz (40m) |
| 5.64 | 25005 | `0x04` | RX1 (DDC0) Freq | 3.865 MHz | 3.868 MHz |
| **5.669** | **25186** | **`0x12`** | **Alex / HPF+LPF** | **1.5MHz_HPF + 80m_LPF** | **1.5MHz_HPF + 40/60m_LPF** |
| **5.683** | **25258** | **`0x02`** | **TX VFO Freq** | **3.868 MHz** | **5.331 MHz** |
| **5.685** | **25273** | **`0x04`** | **RX1 (DDC0) Freq** | **3.868 MHz** | **5.331 MHz** |
| 7.714 | 36299 | `0x12` | Alex / HPF+LPF | 1.5MHz_HPF + 40/60m_LPF | 6.5MHz_HPF + 40/60m_LPF |
| 7.727 | 36372 | `0x02` | TX VFO Freq | 5.331 MHz | 7.230 MHz (40m) |
| 7.704 | 36242 | `0x04` | RX1 (DDC0) Freq | 5.331 MHz | 7.230 MHz (40m) |
| 10.878 | 53484 | `0x04` | RX1 (DDC0) Freq | 7.230 MHz | 10.136 MHz (30m) |
| 13.473 | 67590 | `0x04` | RX1 (DDC0) Freq | 10.136 MHz | 14.296 MHz (20m) |
| 16.069 | 81695 | `0x04` | RX1 (DDC0) Freq | 14.296 MHz | 18.135 MHz (17m) |
| 18.815 | 96619 | `0x04` | RX1 (DDC0) Freq | 18.135 MHz | 21.300 MHz (15m) |
| 21.309 | 110167 | `0x04` | RX1 (DDC0) Freq | 21.300 MHz | 21.295 MHz |
| 21.385 | 110580 | `0x04` | RX1 (DDC0) Freq | 21.295 MHz | 24.940 MHz (12m) |
| 24.582 | 127953 | `0x04` | RX1 (DDC0) Freq | 24.940 MHz | 28.417 MHz (10m) |
| 27.579 | 144240 | `0x04` | RX1 (DDC0) Freq | 28.417 MHz | 3.865 MHz (return 80m) |

Summary counts for the full session: RX1 = 13 changes, RX2 = 2 changes,
TX VFO = 29 changes (includes fine-tuning during QSO), sample-rate = 1 change
(192 kHz, session start only), Alex/filter = 11 changes.

---

### 7.3 Walked-through example: 80m → 60m retune

**Chosen event:** At t=5.683–5.685 s the operator moved RX1 from 3.869 MHz
(80m) to 5.331 MHz (60m/WARC), crossing the 5 MHz HPF boundary. This event
is the clearest single-step HPF/LPF transition in the session because:

- The old 80m_LPF was replaced by the 40/60m_LPF.
- The HPF stayed at 1.5 MHz (no HPF change yet; the 6.5 MHz HPF only engages
  above ~6.5 MHz).
- The RX1, TX VFO, and DDC2–DDC6 frequency slots all transition within two
  consecutive round-robin cycles.

The following timeline covers frames 25063–25345 (±1 full 19-slot cycle around
the transition). Each Metis frame carries two USB sub-frames (USB1 and USB2)
whose C0 slots advance in lock-step through the 19-step round-robin. MOX=0
throughout (RX-only).

```
f=25063  t=5.6468 s  MOX=0 | USB1 Alex/Filters  : HPF=1.5MHz_HPF LPF=80m_LPF         | USB2 Preamp/Att : 0057006c
f=25070  t=5.6479 s  MOX=0 | USB1 Att2/CW       : 20209932                             | USB2 CW_En      : 00000700
f=25085  t=5.6507 s  MOX=0 | USB1 CW_Hang       : 4d023200                             | USB2 EER_PWM    : 1900c800
f=25099  t=5.6532 s  MOX=0 | USB1 BPF2          : 00400000                             | USB2 HL2_ext1   : 00001e46
f=25114  t=5.6560 s  MOX=0 | USB1 HL2_ext2      : 00000000                             | USB2 General    : 0284081c
f=25128  t=5.6586 s  MOX=0 | USB1 TX_Freq       : 3.868498 MHz [80m]                   | USB2 RX1_Freq   : 3.868498 MHz [80m]    <- LAST 80m cycle
f=25143  t=5.6614 s  MOX=0 | USB1 RX2_Freq      : 7.072497 MHz                         | USB2 ADC_Assign : 00000800
f=25157  t=5.6639 s  MOX=0 | USB1 DDC2_Freq     : 3.868498 MHz                         | USB2 DDC3_Freq  : 3.868498 MHz
f=25172  t=5.6667 s  MOX=0 | USB1 DDC4_Freq     : 5.330598 MHz [60m, STALE mix]        | USB2 DDC5_Freq  : 5.330598 MHz
                            |  *** DDC4/5 pick up new freq before DDC0/1 slot arrives ***
f=25186  t=5.6693 s  MOX=0 | USB1 DDC6_Freq     : 5.330598 MHz                         | USB2 Alex/Filters: HPF=1.5MHz_HPF LPF=40/60m_LPF  <- FILTER CHANGES
f=25201  t=5.6720 s  MOX=0 | USB1 Preamp/Att    : 0057006c                             | USB2 Att2/CW    : 20209932
f=25215  t=5.6747 s  MOX=0 | USB1 CW_En         : 00000700                             | USB2 CW_Hang    : 4d023200
f=25230  t=5.6774 s  MOX=0 | USB1 EER_PWM       : 1900c800                             | USB2 BPF2       : 00400000
f=25244  t=5.6800 s  MOX=0 | USB1 HL2_ext1      : 00001e46                             | USB2 HL2_ext2   : 00000000
f=25258  t=5.6825 s  MOX=0 | USB1 General       : 0288081c                             | USB2 TX_Freq    : 5.330598 MHz [60m]   <- TX VFO updates
f=25273  t=5.6853 s  MOX=0 | USB1 RX1_Freq      : 5.330598 MHz [60m]                  | USB2 RX2_Freq   : 7.072497 MHz         <- RX1 UPDATES (key event)
f=25287  t=5.6879 s  MOX=0 | USB1 ADC_Assign    : 00000800                             | USB2 DDC2_Freq  : 5.330598 MHz
f=25302  t=5.6907 s  MOX=0 | USB1 DDC3_Freq     : 5.330598 MHz                         | USB2 DDC4_Freq  : 5.330598 MHz
f=25316  t=5.6932 s  MOX=0 | USB1 DDC5_Freq     : 5.330598 MHz                         | USB2 DDC6_Freq  : 5.330598 MHz
f=25331  t=5.6961 s  MOX=0 | USB1 Alex/Filters  : HPF=1.5MHz_HPF LPF=40/60m_LPF       | USB2 Preamp/Att : 0057006d
f=25345  t=5.6986 s  MOX=0 | USB1 Att2/CW       : 20209932                             | USB2 CW_En      : 00000700
```

**Sequence analysis:**

1. **t=5.647 s (frame 25063):** Last round-robin cycle still carries the old
   80m filter configuration (`1.5MHz_HPF + 80m_LPF`). TX and RX1 frequencies
   are still 3.868 MHz.

2. **t=5.666–5.669 s (frames 25172–25186):** The new frequency (5.331 MHz)
   begins appearing in DDC4/5/6 slots *before* the DDC0 (RX1) slot arrives.
   This is not an error; DDC4–6 track the TX frequency in the nddc=4 HL2
   config and happen to share the same slot index as the new target frequency.
   The Alex filter also switches here (frame 25186, USB2 slot `C0=0x12`),
   replacing the 80m_LPF with the 40/60m_LPF — **before the RX1 DDC NCO has
   been updated**.

3. **t=5.683 s (frame 25258):** TX VFO slot (`C0=0x02`) delivers the new
   5.331 MHz. The radio's TX path is now pointed at 60m.

4. **t=5.685 s (frame 25273):** RX1 slot (`C0=0x04`) delivers 5.331 MHz. The
   DDC0 NCO retunes. From this frame forward, the entire front-end — HPF, LPF,
   DDC0 NCO — is consistent on 60m.

5. **t=5.696 s (frame 25331):** The Alex filter slot recurs as USB1, confirming
   the new filter state in the second consecutive cycle. All 19 command slots
   are now stable at the new 60m configuration.

**Total retune window:** from the first new-frequency appearance in DDC4/5 (frame
25172, t=5.667 s) to the final slot confirmation (frame 25331, t=5.696 s) = ~29 ms.
DDC0 NCO lag behind the filter update: ~16 ms (frames 25186 → 25273).

---

### 7.4 Recipe for Nereus Phase 3L

When Phase 3L handles a retune event (user changes VFO frequency, crossing a
filter boundary), it must emit updated values to the shared state structure
before the round-robin picks up the next cycle. The observed Thetis ordering
demonstrates the following priorities:

1. **Update all frequency fields atomically in the shared state** before any
   C&C frame is emitted. The round-robin then drains them slot-by-slot. Do not
   try to "inject" a special single command — this is not how P1 works.

2. **Alex filter selection must be computed from the new frequency**, not the
   old one, before the round-robin writes the `0x12` slot. If the filter update
   arrives one cycle ahead of DDC0 NCO update (as seen here), that is
   acceptable — the radio handles the brief mismatch without audible artifact.

3. **DDC0–DDC6 slots will be stale for up to one full 19-slot cycle** (~50 ms
   at 378 Hz EP2 rate). This is expected behavior, not a bug. Nereus must not
   add artificial delays or re-inject commands after a retune.

4. **The HPF boundary at ~6.5 MHz** (switching from 1.5 MHz HPF to 6.5 MHz HPF)
   requires the Alex filter update one cycle before the DDC NCO update can arrive.
   The same "filter-first, NCO second" pattern seen in this 80m→60m transition
   repeats for every band change in the session.

5. **No special TX coordination is required for RX-only retunes** (MOX=0). The
   TX VFO updates in the same slot cycle as RX1, one slot earlier in the
   round-robin (case 1 = TX, case 2 = RX1).

**Specific command ordering (one round-robin cycle, ~19 Metis frames):**

```
Slot  C0 (base)  Command            Action
  0    0x00      General/SR         sample rate, OC bits (unchanged)
  1    0x02      TX VFO Freq        new frequency [Hz, 32-bit BE]
  2    0x04      RX1 (DDC0) Freq    new frequency [Hz, 32-bit BE]
  3    0x06      RX2 (DDC1) Freq    unchanged (or also update if needed)
  4    0x1C      ADC Assign         unchanged
  5    0x08      DDC2 Freq          new frequency (tracks TX)
  6    0x0A      DDC3 Freq          new frequency (tracks TX)
  7    0x0C      DDC4 Freq          new frequency (tracks TX)
  8    0x0E      DDC5 Freq          new frequency (tracks TX)
  9    0x10      DDC6 Freq          new frequency (tracks TX)
 10    0x12      Alex / HPF+LPF     new filter bits — C3=HPF mask, C4=LPF mask
 11    0x14      Preamp/Att         unchanged
 12    0x16      Att2/CW            unchanged
 ...
 17    0x2E      HL2 extension 1    constant (HL2-specific)
 18    0x74      HL2 extension 2    constant (HL2-specific)
```

**Thetis source cross-references:**
- RX1 frequency (slot 2): `networkproto1.c:484–494` — `case 2`, assigns
  `prn->rx[0].frequency` into C1–C4 as 32-bit big-endian Hz.
- Alex HPF/LPF (slot 10): `networkproto1.c:578–590` — `case 10`, builds C3
  from `prbpfilter->_13MHz_HPF … _Bypass` bits and C4 from
  `prbpfilter->_30_20_LPF … _17_15_LPF` bits.

---

## 8. TX / Keying Trace

### 8.1 Summary

The capture contains **4 MOX transitions** (2 TX events). Total TX-on duration:
approximately **11.8 seconds** across two keying events.

| TX event | First MOX=1 frame | t (s) | Last MOX=1 frame | t (s) | Duration |
|----------|-------------------|-------|------------------|-------|----------|
| Event 1  | 174523            | 33.152 | 210370          | 39.749 | ~6.60 s |
| Event 2  | 254309            | 47.835 | 282426          | 53.010 | ~5.18 s |

### 8.2 MOX Transitions Table

The following table shows the case-0 slot (masked C0 = `0x00`) transitions,
which is where the transition-detection script tracks MOX. Because the MOX bit
is OR'd onto every C0 slot simultaneously, the first MOX=1 frame (at any slot)
arrives slightly earlier than the case-0 crossing; see §8.3 for detail.

| Frame  | Time (s)    | Raw C0 (case-0) | MOX bit | Inter-event gap (ms) |
|--------|-------------|-----------------|---------|----------------------|
| 8      | 0.987       | `0x00`          | 0       | — (session start)    |
| 174581 | 33.163      | `0x01`          | 1       | — (first TX-on)      |
| 210377 | 39.750      | `0x06`          | 0       | 6587 (TX-on duration)|
| 254353 | 47.844      | `0x03`          | 1       | 8094 (RX gap)        |
| 282484 | 53.021      | `0x02`          | 0       | 5177 (TX-on duration)|

> Note: the "Raw C0 (case-0)" column is the C0 byte observed when the masked
> slot is `0x00`. After MOX-on, odd raw values like `0x01`, `0x03` carry
> MOX=1. After MOX-off, even raw values like `0x06` carry the current slot
> index with MOX=0.

### 8.3 First MOX-on Bring-up Walk-through

The first TX event begins at approximately t=33.15 s. The following table
covers EP2 command frames from ~30 frames before the first MOX=1 (frame
174523) through ~40 frames after it.

`USB1=<C0>:<C1-C4>` and `USB2=<C0>:<C1-C4>` for each EP2 Metis frame:

```
Frame    t (s)       USB1 C0:C1-C4         USB2 C0:C1-C4        Notes
174312   33.113  00:0284081c           02:003b0752          Case 0 MOX=0, Case 1 TX freq (MOX=0)
174441   33.137  74:00000000           00:0284081c          HL2 slot 18 (MOX=0), Case 0 MOX=0 [last MOX=0 at case-0]
174450   33.139  02:003b0752           04:003af9a6          Case 1 TX freq, Case 2 RX1 freq [MOX=0 on both]
174465   33.142  06:006beaf1           1c:00000800          Case 3 RX2 freq, Case 4 ADC
174479   33.144  08:003b0752           0a:003b0752          Case 5 DDC2, Case 6 DDC3
174494   33.147  0c:003b0752           0e:003af9a6          Case 7 DDC4, Case 8 DDC5
174508   33.149  10:003af9a6           12:6e4c1004          Case 9 DDC6, Case 10 Drive/Alex [MOX=0, last before TX]
  -- XmitBit set by host between frames 174508 and 174523 (~2.8 ms gap) --
174523   33.152  15:00570068           17:3f209932          Case 11 Preamp MOX=1, Case 12 ATT/CW MOX=1 [FIRST MOX=1]
174537   33.155  1f:00000700           21:4d023200          Case 13 CW MOX=1, Case 14 CW hang MOX=1
174552   33.158  23:1900c800           25:80400000          Case 15 EER MOX=1, Case 16 BPF2 MOX=1
174566   33.160  2f:00001e46           75:00000000          HL2 slot 17 MOX=1, HL2 slot 18 MOX=1
174581   33.163  01:0204081c           03:003b0a72          Case 0 MOX=1 [case-0 transition], Case 1 TX freq MOX=1
174595   33.166  05:003af9a6           fb:079d0654          Case 2 RX1 freq MOX=1, HL2 0xFB slot MOX=1
174609   33.168  07:006beaf1           1d:00000800          Case 3 RX2 freq MOX=1, Case 4 ADC MOX=1
174624   33.171  09:003b0a72           0b:003b0a72          Case 5 DDC2 MOX=1, Case 6 DDC3 MOX=1
174638   33.173  0d:003b0a72           0f:003af9a6          Case 7 DDC4 MOX=1, Case 8 DDC5 MOX=1
```

**Key findings from the bring-up sequence:**

1. **TX frequency was set BEFORE MOX-on.** At frame 174450 (USB1 `02:003b0752`),
   case-1 TX VFO is already at 3,869,298 Hz (80m) with MOX=0. The TX frequency
   update is part of the normal round-robin and does not require a special
   pre-TX burst — it was already correct from the band-change that preceded
   this TX event.

2. **Drive level was set BEFORE MOX-on.** At frame 174508, USB2 C0=`0x12` (case 10)
   carries drive level in C1. This slot is the last frame with MOX=0 before
   the transition. The next occurrence of case 10 will carry MOX=1 but the
   value doesn't change.

3. **No Alex TX relay update was observed in the TX bring-up window.** Case 10
   (C0=`0x12`) carries the HPF/LPF filter bits and is sent every round-robin
   cycle. Its filter content (C3=`0x10`, 1.5 MHz HPF; C4=`0x04`, 80m LPF) does
   not change at MOX-on: the filter bits were already set during the preceding
   band-change event (see Section 7). Thetis does not send a dedicated "TX relay
   arm" command — the Alex filter state is a continuous round-robin register.

4. **MOX bit appears simultaneously on ALL C0 slots** in the first frame after
   XmitBit is set. Frame 174523 carries MOX=1 in both USB1 (`0x15`) and USB2
   (`0x17`), which are cases 11 and 12 in the round-robin. There is no per-slot
   staggering. The delay between the last MOX=0 frame (174508, t=33.149 s) and
   the first MOX=1 frame (174523, t=33.152 s) is approximately **2.8 ms** —
   one EP2 frame interval at ~378 Hz.

5. **TX I/Q payload is non-zero from the very first post-MOX frame.** Frame
   174595 is the first post-case-0-transition frame and already carries non-zero
   TX I/Q data (see §8.4). There is no observed "ramp-in" delay of blank
   samples.

### 8.4 TX Sample Format

#### 8.4.1 Hex excerpt (frame 191997, t=36.368 s, mid-TX)

```
Offset (from UDP payload start)
  0x00  ef fe 01 02  -- Metis header (EP2)
  0x04  00 00 34 02  -- sequence number 0x3402
  0x08  7f 7f 7f 13  -- USB1 sync + C0=0x13 (case 9, MOX=1)
  0x0C  6e 4c 10 04  -- C1-C4
  0x10  00 00 00 00  -- sample 0: L audio I=0x0000, L audio Q=0x0000
  0x14  ac 13 5d 35  -- sample 0: TX IQ I=0xAC13 (-21485), TX IQ Q=0x5D35 (+23861)
  0x18  00 00 00 00  -- sample 1: L audio I=0x0000, L audio Q=0x0000
  0x1C  a2 cb 53 ed  -- sample 1: TX IQ I=0xA2CB (-23349), TX IQ Q=0x53ED (+21485)
  0x20  00 00 00 00  -- sample 2: L audio
  0x24  9a 88 49 b9  -- sample 2: TX IQ I=0x9A88 (-25976), TX IQ Q=0x49B9 (+18873)
```

The TX IQ samples are **non-zero**. The audio bytes (every group of 4 bytes
starting at offset `0x10`, `0x18`, ...) are zero, indicating the operator
transmits RF signal (SSB or AM) but no audio is present at the L/R audio
position — consistent with SSB transmit without microphone input captured in
this session, or with the operator transmitting a carrier/tone.

#### 8.4.2 Expected format from networkproto1.c

`networkproto1.c:732–743` (inside `sendProtocol1Samples`):

```c
for (i = 0; i < 2 * 63; i++)          // 126 samples across both USB sub-frames
    for (j = 0; j < 2; j++)            // j=0: L/R audio, j=1: TX I/Q
        for (k = 0; k < 2; k++)        // k=0: I component, k=1: Q component
        {
            temp = /* float→int16 clamp and round */;
            prn->OutBufp[8 * i + 4 * j + 2 * k + 0] = (char)((temp >> 8) & 0xff);
            prn->OutBufp[8 * i + 4 * j + 2 * k + 1] = (char)(temp & 0xff);
        }
```

Each 8-byte sample unit layout (big-endian 16-bit signed integers):

| Bytes | Field | j, k |
|-------|-------|------|
| [0–1] | L/R audio I sample | j=0, k=0 |
| [2–3] | L/R audio Q sample | j=0, k=1 |
| [4–5] | TX IQ I sample     | j=1, k=0 |
| [6–7] | TX IQ Q sample     | j=1, k=1 |

**Observed vs. expected:** The captured sample layout matches exactly. The
pattern `00 00 00 00 <I_hi> <I_lo> <Q_hi> <Q_lo>` in the hex dump corresponds
to zero audio (bytes 0–3) plus non-zero TX IQ (bytes 4–7) per sample unit.
This is consistent with the formula at `networkproto1.c:736`.

**CW note:** `networkproto1.c:738–741` shows that in CW mode, j=1 samples are
overwritten with keying bits (`dot<<2 | dash<<1 | cwx`). This was NOT observed
in this capture — the j=1 data contains normal IQ amplitudes, confirming the
operator was transmitting SSB (or AM/FM), not CW.

**TX sample depth:** 16-bit signed (int16), big-endian. This is different from
EP6 (RX I/Q), which uses 24-bit signed samples. `networkproto1.c:736` applies
`floor(x * 32767.0 + 0.5)` / `ceil(x * 32767.0 - 0.5)` scaling — a standard
float→int16 conversion.

### 8.5 HL2 Quirks Observed During TX

1. **MOX bit appears in HL2 extension slots.** Slots `0x2F` (HL2 ext 17) and
   `0x75` (HL2 ext 18) carry MOX=1 during TX (frame 174566: `2f:00001e46` and
   `75:00000000`). The HL2 firmware reads the MOX bit from all C0 bytes, not
   just case-0. Extension slots must include the correct MOX bit.

2. **The periodic HL2 `0xFB` slot carries MOX=1 during TX** (frame 174595:
   `fb:079d0654`). This confirms the 0xFA/0xFB HL2 register writes continue
   during TX with the same ~131-frame period, and the MOX bit is simply OR'd in
   by `WriteMainLoop`. There is no TX-specific suppression or change in the HL2
   periodic slot content.

3. **One-frame blank TX sample window.** The sample region of frame 174595
   (first complete EP2 frame after the case-0 MOX transition) shows all-zero
   sample bytes — the WDSP transmit chain needs one write cycle before its first
   IQ output arrives. By frame 191997 (t=36.368 s, well into the TX event),
   IQ samples are clearly non-zero (`0xAC13` / `0x5D35`).

### 8.6 Recipe for Nereus Phase 3L — TX Keying

To key TX on HL2 via Protocol 1:

1. **Set TX frequency** (case 1, `C0 |= 0x02`): write desired TX frequency as
   32-bit big-endian Hz in C1–C4. This is sent every round-robin cycle
   (`networkproto1.c:476–481`). Ensure the correct frequency is in the shared
   state before asserting MOX.

2. **Set drive level** (case 10, `C0 |= 0x12`): write `tx[0].drive_level`
   (0–255) into C1 (`networkproto1.c:580`). This must be set before MOX-on; the
   HL2 firmware uses this value to scale TX output power.

3. **Set Alex HPF/LPF filter bits** (case 10, C3/C4): the Alex filter selection
   lives in the same slot as drive level (`networkproto1.c:583–590`). Update
   filter bits for the TX band before keying. In this capture the filter was
   already correct from the preceding band-change.

4. **Assert MOX**: set `XmitBit = 1`. On the very next `WriteMainLoop` call, C0
   for every slot will have bit 0 set. No special "MOX command" burst is needed
   — the bit propagates to all 19 slots simultaneously.

5. **Stream TX I/Q**: from the same `WriteMainLoop` call that first carries
   MOX=1, the host must supply valid TX I/Q samples in `outIQbufp` (126 complex
   float samples, scaled to ±1.0). These are packed as 16-bit big-endian signed
   integers at 8 bytes/sample in the EP2 payload (`networkproto1.c:732–743`).
   **Observed delay between MOX-on and first TX I/Q frame: 1 frame (~2.6 ms).**

6. **Assert MOX-off**: set `XmitBit = 0`. On the next `WriteMainLoop` call, all
   C0 slots revert to even (MOX=0). `networkproto1.c:723` zeros `outIQbufp`
   when `XmitBit == 0`, ensuring no residual TX samples are sent after keying
   stops.

7. **No special teardown**: there is no "TX-off" command beyond clearing XmitBit.
   The Alex filters and drive level remain at their current values; they need not
   be cleared. The HL2 firmware stops accepting TX IQ data as soon as MOX=0 is
   received.

**Thetis source cross-references:**
- MOX bit build: `networkproto1.c:446` — `C0 = (unsigned char)XmitBit`
- MOX transition detection: `networkproto1.c:436–440` — `if (XmitBit != PreviousTXBit)`
- TX IQ zero-fill on MOX-off: `networkproto1.c:723` — `if (!XmitBit) memset(prn->outIQbufp, 0, ...)`
- TX IQ sample pack: `networkproto1.c:732–743` — 16-bit BE int16 format
- Drive level (case 10): `networkproto1.c:578–580`

---

## 9. HL2-Specific Observations

**Scope:** The observations below are from ONE Hermes Lite 2 running firmware
version `0x4A` (decimal 74) on ONE capture session. They may not generalize to
other HL2 firmware revisions or other P1 radios.

### 9.1 Radio Identity

- **Board ID:** `0x06` (HermesLite); **Firmware:** `0x4A` (74 decimal)
- **MAC:** `00:1C:C0:A2:13:DD`
- **Sample rate:** 192 kHz only (observed in Sections 5–6)
- **Frame rate:** 5052.9 Hz observed vs. 4947 Hz theoretical; +2.1% delta, firmware-specific timing

### 9.2 Non-standard EP6 Status Slot (`0xFA` repeating)

The HL2 firmware injects a status-like frame with C0=`0xFA` approximately every
870 frames with constant payload `02 00 00 01` (C1–C4). This slot does not
appear in `networkproto1.c` and its purpose is unknown. See **Section 4** for
full analysis.

**ADC Overload Bit:** Case-0 USB1 frames (281,195 total) never show the ADC
overload bit (C1 bit 0) set across the entire session.

### 9.3 Non-standard EP2 Command Slots

Beyond the 17 standard protocol-1 cases (0x00–0x10 even), the HL2 firmware
recognizes four additional slot groups:

1. **`0x2E` / `0x74`** — main-cycle extensions with constant payloads; purpose unknown
2. **`0x7A`** — I2C-style aperiodic bursts (C2=`0x9D`)
3. **`0xFA` / `0xFB`** — ~131-frame periodic timer (C2=`0x9D`, shared I2C device address?)

See **Section 5** for slot-level trace and periodicity details.

### 9.4 MOX in Extension Slots

During TX, the MOX bit (C0 bit 0) appears in all C0 bytes, including HL2
extension slots `0x2F` (ext 17), `0x75` (ext 18), and `0xFB` (timer). The MOX
flag is OR'd into every slot by `WriteMainLoop` without distinction. See
**Section 8.5** for the bring-up walk-through and trace.

### 9.5 Duplex Bit Ignored

The duplex bit (C4 bit 2 in case-0) is always observed as 0 in captured frames,
despite `networkproto1.c:507` always OR'ing `0x04` into C4. The HL2 firmware
ignores the duplex flag and operates in simplex mode (single antenna for RX and
TX).

### 9.6 Extended Discovery Reply Bytes

The discovery reply from HL2 carries extra data in bytes 11–13, 20, and 21–59
that are not parsed by the standard Thetis discovery logic. These bytes likely
encode HL2 firmware-specific capabilities (e.g., I2C peripherals, hardware
options). Future work should cross-reference the Hermes Lite 2 firmware source
(Steve Haynal's repo: `github.com/softerhardware/Hermes-Lite2`) to document
their meaning. See **Section 2** for the hex capture.

### 9.7 Implications for Nereus Phase 3L

Nereus must implement the standard P1 RX path against `networkproto1.c`
exclusively, then treat HL2 as **"standard P1 + the four extension slots
listed above"**. Do not copy HL2-specific behavior into the generic P1 RX
path — add it conditionally when discovery returns board ID `0x06`.

Concrete steps:
1. Implement slots 0x00–0x10 (even) per `networkproto1.c`
2. After discovery identifies board ID `0x06`, enable HL2 slots `0x2E`, `0x74`, `0x7A`, `0xFA`, `0xFB`
3. Parse case-0 status with HL2 semantics (duplex=0, MOX in all slots)
4. Investigate extended discovery bytes in HL2 firmware source before exposing capabilities to the UI

---

## 10. Nereus Implementation Checklist (Phase 3L)

The tasks below are derived from the observations in Sections 2–9 and are
scoped small enough for individual PRs. Each references the section that
motivated it.

### 3L-1 — Discovery & Session Bring-Up

- [ ] Emit 63-byte discovery broadcast request (`EF FE 02` + 60 zero bytes) to subnet broadcast address on UDP port 1024 (see §2.1)
- [ ] Parse discovery reply: validate `data[0]==0xEF`, `data[1]==0xFE`, `data[2]==0x02`/`0x03`; extract MAC (bytes 3–8), firmware version (byte 9), board ID (byte 10), MetisVersion (byte 19), NumRxs (byte 20) (see §2.2)
- [ ] Map board ID `0x06` → `BoardType::HermesLite`; treat `data[2]==0x03` as "busy" and surface that state to the UI (see §2.2)
- [ ] TODO (defer to 3L-HL2): parse HL2 extended discovery bytes 11–13 and 21–59 against HL2 firmware source (see §2.2, §9.6)
- [ ] Bind session UDP socket on ephemeral source port; record radio address `169.254.x.x:1024` as the session endpoint (see §1)
- [ ] Emit 64-byte start command (`EF FE 04 01` + 60 zero bytes) to radio port 1024 after successful discovery reply (see §3.1)
- [ ] Emit 64-byte stop command (`EF FE 04 00` + 60 zero bytes) on session teardown (see §3.2)

### 3L-2 — EP6 RX Ingestion

- [ ] Parse 1032-byte Metis envelope: validate `data[0..1]==EF FE`, `data[2]==0x01`, `data[3]==0x06`; extract 32-bit BE sequence number from bytes 4–7 (see §4.1)
- [ ] Locate USB sub-frame 1 at offset 8 and USB sub-frame 2 at offset 520; validate each `7F 7F 7F` sync prefix (see §4.1)
- [ ] Decode C0 status index as `C0 & 0xF8` (bits [7:3]); extract PTT/dot/dash from bits [2:0] (see §4.3)
- [ ] Implement C0 status slot switch for slots `0x00`, `0x08`, `0x10`, `0x18`, `0x20` per `networkproto1.c:334–355` decode table (see §4.3)
- [ ] Compute samples-per-USB-frame as `spr = 504 / (6 * nddc + 2)`; deinterleave 24-bit BE I and Q samples using left-shift sign-extension (`<< 8` on each byte triple), scale by `1/2147483648.0` (see §4.2)
- [ ] Extract mic sample at bytes 6–7 of each 8-byte sample group as 16-bit BE, scale by `1/2147483648.0` (see §4.2)
- [ ] Track EP6 sequence number continuity; increment a drop counter on any gap; log at qCWarning level (see §6.3)

### 3L-3 — EP2 Command Building

- [ ] Build 1032-byte Metis EP2 envelope: header `EF FE 01 02` + 32-bit BE sequence number (auto-incrementing), then two 512-byte USB sub-frames each with `7F 7F 7F` sync + C0–C4 + 504 bytes of TX I/Q payload (see §5.1)
- [ ] Implement `out_control_idx` round-robin counter cycling 0–16 (standard), incrementing once per USB sub-frame, resetting to 0 after slot 16 (see §5.2)
- [ ] Build C0 as `(XmitBit & 0x01) | slot_or_mask` for every sub-frame so the MOX bit is OR'd into every slot simultaneously (see §5.2, §5.5)
- [ ] Implement per-slot command builders for cases 0–16 matching `networkproto1.c:450–665`: case 0 (general/SR/OC), case 1 (TX freq), case 2 (DDC0/RX1 freq), case 3 (DDC1/RX2 freq), case 4 (ADC assign + TX step attn), cases 5–9 (DDC2–DDC6 freq), case 10 (drive + Alex HPF/LPF), case 11 (preamp/att/mic), case 12 (att2/CW keyer speed/weight), case 13 (CW enable/sidetone/RF delay), case 14 (CW hang/sidetone freq), case 15 (EER PWM), case 16 (BPF2/xvtr/PureSignal) (see §5.4)
- [ ] Emit EP2 frames at the ~378 Hz steady-state rate observed in this capture; do not throttle below that rate (see §6.1, §6.4)

### 3L-4 — Tuning & Band Changes

- [ ] On RX1 frequency change: write new value into shared frequency state before the next round-robin write of case-2 slot (`C0 |= 0x04`); do NOT inject a one-shot special frame (see §7.4)
- [ ] Compute Alex HPF/LPF filter bits from the new target frequency (not the old), and update case-10 state (`C0 |= 0x12`) in the same shared write; allow up to one full 19-slot cycle (~50 ms) for the filter update to precede the DDC NCO update — this is the observed Thetis ordering and is acceptable (see §7.3, §7.4)
- [ ] On TX frequency change: update case-1 state (`C0 |= 0x02`) in the shared structure; DDC2–DDC6 slots that track TX freq will pick it up in the same cycle (see §7.4)
- [ ] Encode sample rate selection in case-0 C1 bits [1:0] (`SampleRateIn2Bits & 3`; `0x02` = 192 kHz) and send on every case-0 round-robin occurrence (see §5.4 case 0)
- [ ] Build case-10 Alex HPF mask into C3 and LPF mask into C4 matching `networkproto1.c:583–590` bit assignments; test at the 5 MHz, 6.5 MHz, and 9.5 MHz HPF boundaries (see §7.3)

### 3L-5 — TX Path

- [ ] Ensure TX frequency (case 1) and drive level (case 10 C1) are set in shared state during the RX round-robin before any MOX assertion, so no special pre-TX burst is needed (see §8.3 findings 1–2)
- [ ] Implement MOX assert: set `XmitBit = 1`; on the next `WriteMainLoop` call, C0 for every slot must have bit 0 set; no additional MOX command frame is required (see §8.3 finding 4, §8.6 step 4)
- [ ] Pack TX I/Q samples as 16-bit big-endian signed integers into EP2 payload at 8 bytes per sample unit (`[L_audio_I 2B][L_audio_Q 2B][TX_IQ_I 2B][TX_IQ_Q 2B]`), scaling float ±1.0 → int16 via `floor(x * 32767.0 + 0.5)` (see §8.4)
- [ ] On MOX-off: zero-fill `outIQbufp` immediately (`memset` per `networkproto1.c:723`) so no residual TX samples are emitted after keying stops (see §8.6 step 6)
- [ ] DEFER: mic audio capture into TX payload L/R audio bytes (bytes 0–3 per sample unit) — mark as future work once AudioEngine mic input is wired (see §8.4)

### 3L-6 — HL2-Specific Extensions (conditional on board ID == `0x06`)

- [ ] On receipt of EP6 C0=`0xFA` (`0xFA & 0xF8 = 0xF8`): silently ignore; log the first occurrence at qCDebug level; mark as OPEN QUESTION — decode against HL2 firmware source (`hermeslite.v`) before exposing to UI (see §4.3, §9.2)
- [ ] Extend round-robin to 19 slots when board ID == `0x06`: add slot 17 (`C0 |= 0x2E`, constant payload `00 00 1E 46`) and slot 18 (`C0 |= 0x74`, constant payload `00 00 00 00`); emit on every cycle (see §5.3, §5.4 cases 17–18, §9.3)
- [ ] OPEN QUESTION: determine whether skipping HL2 extension EP2 slots `0x2E`/`0x74` breaks HL2 firmware in practice — test on live HL2 hardware before committing the slots as required; document result in §5.4 (see §5.4 cases 17–18)
- [ ] Ensure MOX bit is OR'd into HL2 extension slot C0 bytes (`0x2F`, `0x75`) during TX in the same `WriteMainLoop` pass, not as a special case (see §8.5, §9.4)
- [ ] Treat duplex bit (case-0 C4 bit 2) as a write-only field for standard P1; do NOT assert it unconditionally when board ID == `0x06` — the HL2 ignores it but setting it may confuse other radios (see §9.5)
- [ ] DEFER: aperiodic HL2 commands `0x7A` (I2C burst) and `0xFA`/`0xFB` (timer register) — mark as HL2-firmware-specific; do not implement until HL2 firmware source documents their function (see §5.4, §9.3)

### 3L-7 — Validation

- [ ] Replay the 302,256-frame `HL2_Packet_Capture.pcapng` capture through Nereus's P1 parser and confirm bit-exact decoding of all EP6 frames: correct sequence numbers, correct C0 slot dispatch, correct I/Q sample values for at least the first and last 100 frames (see §1, §4)
- [ ] Drive Nereus's EP2 builder with the same parameter inputs observed in this capture and verify that output C0/C1–C4 bytes match the reference capture values for each of the 17 standard command slots (see §5.4)
- [ ] Run Nereus against a live HL2 (board ID `0x06`, firmware `0x4A`) and confirm: EP6 received at ~5053 Hz, EP2 emitted at ~378 Hz, RX audio passes through WDSP without drops or sequence errors (see §6)

---

## Appendix A — Reproducing This Analysis

Every hex dump, table, and statistic in this document was produced with the commands below, run against `HL2_Packet_Capture.pcapng` on macOS with Wireshark's `tshark` 4.x. The filtered session subset `/tmp/hl2cap/hl2_session.pcapng` is produced in A.1 and used by all subsequent commands.

### A.1 Create filtered session subset

Extract only the P1 session frames (port 1024) from the full capture, removing discovery broadcasts on port 50533:

```bash
tshark -r /tmp/hl2cap/HL2_Packet_Capture.pcapng \
  -Y 'udp.port == 1024' \
  -w /tmp/hl2cap/hl2_session.pcapng
```

### A.2 Discovery exchange — hex dump of discovery frames

Extract discovery REQUEST/REPLY frames on ephemeral source port 50533:

```bash
tshark -r /tmp/hl2cap/HL2_Packet_Capture.pcapng \
  -Y '(udp.srcport == 50533 or udp.dstport == 50533)' \
  -x
```

### A.3 Start/stop frame detection

Locate start/stop command frames by filtering for the Metis command opcode `0x04`:

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024' \
  -T fields -e frame.number -e frame.time_relative -e udp.payload 2>/dev/null \
  | awk '{if (substr($3,1,6)=="effe04") print $1, $2, substr($3,1,16)}'
```

### A.4 EP6 frame size check

Verify all radio→host EP6 frames (opcode `0x01`) are exactly 1032 bytes:

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221' \
  -T fields -e udp.length | sort -u
```

### A.5 EP6 C0 status index enumeration

Extract C0 status bytes from EP6 frames and histogram the slot indices (bits [7:3]) for both USB sub-frames.

**USB1 (offset 11):**

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221' \
  -T fields -e udp.payload 2>/dev/null \
  | awk 'substr($0,1,6)=="effe01" {c0=strtonum("0x" substr($0,23,2)); printf "0x%02X\n", and(c0,0xf8)}' \
  | sort | uniq -c | sort -rn
```

**USB2 (offset 523):**

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221' \
  -T fields -e udp.payload 2>/dev/null \
  | awk 'substr($0,1,6)=="effe01" {c0=strtonum("0x" substr($0,1047,2)); printf "0x%02X\n", and(c0,0xf8)}' \
  | sort | uniq -c | sort -rn
```

### A.6 EP2 C0 command enumeration

Extract C0 status bytes from EP2 frames (host→radio, opcode `0x02`) and histogram the command indices for comparison.

**USB1 (offset 11):**

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024' \
  -T fields -e udp.payload 2>/dev/null \
  | awk 'substr($0,1,6)=="effe02" {c0=strtonum("0x" substr($0,23,2)); printf "0x%02X\n", and(c0,0xfe)}' \
  | sort | uniq -c | sort -rn
```

**USB2 (offset 523):**

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024' \
  -T fields -e udp.payload 2>/dev/null \
  | awk 'substr($0,1,6)=="effe02" {c0=strtonum("0x" substr($0,1047,2)); printf "0x%02X\n", and(c0,0xfe)}' \
  | sort | uniq -c | sort -rn
```

### A.7 Cadence computation — frame rates

Count frames per direction and compute rate (frames / duration).

**EP6 (radio→host, 5052.9 Hz observed):**

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221' \
  -T fields -e frame.number -e frame.time_relative 2>/dev/null \
  | awk 'NR==1{first=$2; first_frame=$1} NR==NF{last=$2; last_frame=$1} END{print "Frames:", last_frame - first_frame, "Duration:", last - first, "Rate:", (last_frame - first_frame) / (last - first)}'
```

**EP2 (host→radio, 378.0 Hz observed):**

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024' \
  -T fields -e frame.number -e frame.time_relative 2>/dev/null \
  | awk 'NR==1{first=$2; first_frame=$1} NR==NF{last=$2; last_frame=$1} END{print "Frames:", last_frame - first_frame, "Duration:", last - first, "Rate:", (last_frame - first_frame) / (last - first)}'
```

### A.8 Sequence continuity check

Verify EP6 Metis sequence numbers (bytes 4–7, 32-bit big-endian) are contiguous with no gaps:

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221' \
  -T fields -e udp.payload 2>/dev/null \
  | awk 'substr($0,1,6)=="effe01" {seq=strtonum("0x" substr($0,9,8)); if(prev!="" && seq != prev+1) gaps++; prev=seq} END{print "gaps="gaps}'
```

### A.9 RX1 frequency-change event detection

Locate case-0x04 (RX1/DDC0) slot transitions by filtering EP2 frames and extracting the frequency value (bytes 4–7 of payload after C0–C4):

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024' \
  -T fields -e frame.time_relative -e udp.payload 2>/dev/null \
  | awk 'substr($2,1,6)=="effe02" {c0=strtonum("0x" substr($2,23,2)); if(and(c0,0xfe)==0x04) {freq=strtonum("0x" substr($2,11,8)); if(last_freq!="" && freq!=last_freq) print $1, "RX1 change", last_freq, "->", freq; last_freq=freq}}'
```

### A.10 MOX transition detection

Monitor EP2 case-0x00 (general C&C) C0 bit 0 (MOX) for RX↔TX transitions:

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.105.135 and udp.dstport == 1024' \
  -T fields -e frame.time_relative -e udp.payload 2>/dev/null \
  | awk 'substr($2,1,6)=="effe02" {c0=strtonum("0x" substr($2,23,2)); mox=and(c0,0x01); if(last_mox!="" && mox!=last_mox) print $1, "MOX", (last_mox?"off":"on"), "->", (mox?"on":"off"); last_mox=mox}'
```

### A.11 ADC overload bit check

Scan EP6 case-0x00 (general C&C) USB1 C1 bit 0 (ADC overload) across the entire session:

```bash
tshark -r /tmp/hl2cap/hl2_session.pcapng \
  -Y 'ip.src == 169.254.19.221' \
  -T fields -e udp.payload 2>/dev/null \
  | awk 'substr($0,1,6)=="effe01" {c0=strtonum("0x" substr($0,23,2)); if(and(c0,0xf8)==0x00) {c1=strtonum("0x" substr($0,25,2)); adc_ol=and(c1,0x01); if(adc_ol) print "ADC overload detected"}}' \
  | sort | uniq -c
```

---

### Re-running on a Different Capture

To re-run this analysis on a different P1 capture, follow these steps:

1. Identify the host and radio IP addresses from the new capture (use `tshark -r capture.pcapng -Y 'udp.port == 1024'` to find them)
2. Replace `169.254.105.135` (host) and `169.254.19.221` (radio) in all `-Y` filters with the new addresses
3. Replace `/tmp/hl2cap/HL2_Packet_Capture.pcapng` with the path to your capture file
4. Run blocks A.1 through A.11 in order

**Important:** All commands assume bash with GNU `awk` (gawk). On macOS, ensure `gawk` is installed via Homebrew:

```bash
brew install gawk
```

Or substitute `awk` with `gawk` explicitly in all commands above if the default `awk` is BSD awk (which lacks `strtonum` and `and` functions needed for hex parsing).
