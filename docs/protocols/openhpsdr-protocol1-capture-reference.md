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
