# CW transmit

Date: 2026-10-25 (America/New_York), spec written 2026-10-26.

Status: approved by JJ on October 26, 2026. He approved the mockup ("the cw
transmit mockup looks good") and called for this spec ("Yes, write the CW
transmit spec now"), both relayed by the manager session. JJ handed the open
design calls to the manager session ("move my project forward"), so decisions
C1 to C14 were made by the designer with a reason each; the calls settled
while writing this spec are D1 to D16. All of them are listed for JJ's veto in
section 11. Requirement IDs R-CW-01 to R-CW-52 are new here. Nothing is built.

Companion design: FM transmit, designed in the same brainstorm, which waits
on JJ's look at its mockup and lands separately. CW came first because OZ1CT, who asked for
transverter support, works EME, which is almost all CW. The transverter design
lists both as must-haves.

Mockup: [cw-mockup.html](2026-10-25-cw-transmit-design/cw-mockup.html), built
on real captures of NereusSDR's preview tool in the workspace look, with new
parts cut from the real components' pixels and listed in the page's "Where
each piece came from" table. Radio, window, band and a held paddle drive every
screen, and the Setup tree opens every page. Section 9 lists where this spec
differs from the mockup.

**Flags.** This change touches what makes the hardware transmit (C1, C4, C5,
C7, R-CW-05 to R-CW-22, R-CW-30), the receive path (C2, R-CW-23 to R-CW-29),
and the thread layout (two new threads, R-CW-31 and R-CW-36). Section 10 lists
each one.

## 1. Goal

An operator on any supported radio can send CW: with a paddle or straight key
on the radio's KEY jack, from NereusSDR's CWX sender, from a logger through CAT
or TCI, or from a serial line, with break-in (off, semi, or full QSK where the
radio can do it) and a sidetone, and the carrier lands exactly on the station
they tuned to. A remote window does everything a local one does. Nothing about
CW can key the radio while a transmit check would refuse it.

## 2. What NereusSDR has today (main 36f4a279e)

| Area | State |
| --- | --- |
| Transmit gate | `BandPlanGuard::isModeAllowedForTx` has no CWL or CWU, so `checkMoxAllowed` refuses with "CW transmit is not available on this Core" (`src/core/safety/BandPlanGuard.cpp:872-929`). |
| Protocol 2 | CmdTx bytes 5 to 13 and 17 and the high-priority cwx/dot/dash bits are encoded (`P2CodecOrionMkII.cpp:252, 369-387`), but `P2RadioConnection::m_cw` and `m_tx[0].cwx/dot/dash` are never assigned outside the reset loop, so the radio always gets zeros, edge length 7 (`P2RadioConnection.h:1054-1090`). |
| Protocol 1 | Banks 13 and 14 send C0 only; bank 12 C3/C4 are 0 in `P1CodecStandard` (`:346-352, 502-521`), `P1CodecRedPitaya` (its own `bank12`, `:52-60`) and `P1CodecHl2` ("HL2 has no CW state surface yet", `:369-389`). `P1CodecAnvelinaPro3` inherits Standard. No key bits in the TX I/Q words. The HL2 I/Q low-bit mask is applied in `sendTxIq` (`P1RadioConnection.cpp:3216-3222, 3265-3267`). `CodecContext` has CW fields for P2 only. |
| Radio status | Only the PTT bit is parsed: P1 `P1RadioConnection.cpp:6119` (emit `:6143`), P2 `P2RadioConnection.cpp:5218-5219`. One signal, `micPttFromRadio(bool)` (`RadioConnection.h:1195-1222`). |
| MoxController | Break-in timer declared and never started (`MoxController.h:1842`, empty slot `.cpp:2469-2480`); `onCwPtt` refuses (`.cpp:3137-3143`); a radio PTT status keys MOX in every mode, CW included (`.cpp:1786-1791`); MOX starts the TX DSP channel in every mode (`txReady` to `openTxRfGate` to `setRunningAsync(true)`, `RadioModel.cpp:29930-29960, 35213-35239`). Delays are `constexpr` with a test-only setter (`MoxController.h:400-416, 596-601`). |
| Pitch | Station key `CWPitch` (default 600), read in three copy-pasted places clamped 100..2000 (`SliceModel.cpp:2809, 2934, 3066`); hard-coded 600 in `RadioModel::kApfCwPitchHz` (`RadioModel.h:1197-1201`), the APF push (`RadioModel.cpp:12166-12172`), the TUNE tone (`kCwPitch`, `RadioModel.cpp:30321-30326`) and the Setup APF range. CAT `ZZCL` is refused. No control sets it. |
| Receive convention | No CW term in the receiver shift (`RadioModel::composedShiftHz`, `RadioModel.cpp:24470-24527`): the receiver demodulates at the dial and the CW filter sits a pitch above it, so a station heard at the pitch is one pitch off the dial. The TCI note says "Revisit with CW transmit (3M-2)" (`TciProtocol.h:840-862`). |
| UI | Phone/CW applet CW page is a placeholder sentence (`PhoneCwApplet.cpp:648-671`). Setup > DSP > CW has hidden Keyer and Timing groups with placeholder fields, including a "Bug" mode, weight 10 to 90 and delay 0 to 2000 (`DspSetupPages.cpp:2119-2192`). `CwxApplet` is laid out and never constructed (`MainWindow.cpp:11784`). Thirteen `UnbuiltFeature::Cwx` sites hide all of it. |
| CAT, TCI | Every CW CAT command answers `?;` (`CatGlobalCommands.cpp:306-721`). TCI dispatches only `cw_macros_speed_up`, `cw_macros_speed_down` and `cw_msg`, all logging stubs (`TciProtocol.cpp:2216-2221, 4116-4138`); `keyer`, `cw_macros`, `cw_terminal` and `cw_macros_stop` are not dispatched. |
| Serial | CAT PTT input reads CTS/DSR by polling every 20 ms (`CatSerialTransport.cpp:45, 147-160`). Nothing reads dot or dash from serial. |
| Sidetone | `RadioModel::computeSidetoneExpected` only, for the radio speaker amplifier (R-SPK-15). `BoardCapabilities::hasSidetoneGenerator` is true only for the two HL2 rows (`BoardCapabilities.cpp:1044, 1115`), with a cite that does not hold (mi0bot `IoBoardHl2.cs` has no sidetone). The TX monitor audio comes only from the TX channel's siphon (`TxChannel.cpp:3922-3935`), which does not run in CW. |
| Phone | Setup > DSP > CW exists as a Core-described page (APF only, `resources/setup/dsp.json:1752-1790`). A Core offering the `cwx` tool shows on the phone as an unknown, disabled tool (`ios/NereusApp/Tests/ToolsPagesTests.swift:97-116`). |

## 3. How the radios do CW (sources)

Thetis paths are under `Project Files/Source/Console/` and `ChannelMaster/`
("nP1" `networkproto1.c`, "nP2" `network.c`, "nI" `netInterface.c`) at
v2.10.3.15 (`3759d09`); mi0bot-Thetis at v2.10.3.13-beta2; Orion gateware at
`../n1gp-Anvelina_PROIII/` (`8e86a61`); deskHPSDR at `f3d857c`. The Protocol 1
gateware sources in `../TAPR-OpenHPSDR-Firmware/` (`e7c6584`) hold no keyer
source, so P1 radio behavior that only gateware could answer is checked on the
bench (section 8).

- **The radio makes CW.** On MOX in CWL/CWU Thetis does not start the TX DSP
  channel and does not wait `rf_delay` (`console.cs:29640-29650`). The radio's
  keyer shapes the envelope from the KEY jack or from three host key bits.
  TUNE is the exception (a WDSP tone with the keyer off).
- **Key bits.** P1: while `cw_enable`, every 16-bit I and Q sample of every
  EP2 frame is **replaced** by `dot<<2 | dash<<1 | cwx` (nP1:738-741), written
  at frame build, in receive too. mi0bot on HL2 adds `cwx_ptt<<3` with mask
  `0x0F` (mi0bot nP1:1247-1258). P2: high-priority byte 5,
  `dash<<2 | dot<<1 | cwx` (nP2:928-933), sent at once on a change
  (nI:1150-1181). The dot and dash order differs between protocols. Thetis
  drives CWX from CWX and TCI (`cwx.cs:302`, `TCIServer.cs:9295`) and dot and
  dash from serial paddles (`CAT/SDRSerialPortII.cs:261-262`).
- **P1 fields.** Bank indices 12, 13, 14 (C0 `0x16`, `0x1E`, `0x20` with the MOX
  bit clear; register addresses `0x0B`, `0x0F`, `0x10`):

  | Bank | Byte | Bits | Field | Cite |
  | --- | --- | --- | --- | --- |
  | 12 | C2 | 6 | reverse paddles | nP1:619-620 |
  | 12 | C3 | 5:0 | keyer speed, WPM | nP1:628 |
  | 12 | C3 | 7:6 | `00` straight, `01` iambic A, `10` iambic B | nP1:622-627 |
  | 12 | C4 | 6:0 | weight | nP1:629 |
  | 12 | C4 | 7 | strict spacing | nP1:629 |
  | 13 | C1 | byte | CW enable (internal keyer) | nP1:633-635 |
  | 13 | C2 | byte | sidetone level | nP1:636 |
  | 13 | C3 | byte | RF delay (key-down to RF), ms | nP1:637 |
  | 14 | C1 | 7:0 | hang [9:2] | nP1:643 |
  | 14 | C2 | 1:0 | hang [1:0] | nP1:644 |
  | 14 | C3 | 7:0 | sidetone frequency [11:4] | nP1:645 |
  | 14 | C4 | 3:0 | sidetone frequency [3:0] | nP1:646 |

  P1 has no break-in bit, no sidetone enable and no edge length; sidetone off
  is level 0 (`console.cs:13046-13089`).
- **P2 fields.** CmdTx byte 5 bits: 0 EER, 1 CW enable, 2 reverse, 3 iambic,
  4 sidetone, 5 mode B, 6 strict spacing, 7 break-in; byte 6 sidetone level;
  7-8 sidetone frequency; 9 speed; 10 weight; 11-12 hang; 13 RF delay; 17 edge
  length (nP2:1190-1222; gateware `Tx_specific_C&C.v:158-174`, which does not
  parse byte 17). Thetis sends edge 9 (`setup.cs:29318-29330`). CmdTx bytes
  57-59 carry the ADC attenuation the radio applies while it transmits.
- **Limits.** Speed 1 to 60 (gateware `iambic.v:150`; 0 divides by zero,
  `iambic.v:189`). Weight 33 to 66 (`iambic.v:153`). RF delay one byte, and
  shorter than one dot (`profile.v:34`; deskHPSDR clamps it to `900 / wpm`,
  `old_protocol.c:2823-2831`, `new_protocol.c:1688-1690`). Hang 10 bits on
  both protocols (nP1:643-644; Orion keeps `hang[9:0]`, `Tx_specific_C&C.v:172-173`).
  Thetis's UI allows RF delay 0 to 1000 and hang up to 1500, both of which
  overflow.
- **Break-in** (`console.cs:14753-14781`). Manual: break-in bit 0, the
  operator holds MOX. Semi: break-in 1; the radio asserts PTT on key-down and
  holds it for its hang time, which is break-in delay plus key-up delay when
  break-in is on, else 0 (`console.cs:18523-18533`). Thetis enters MOX on the
  radio's PTT status in CW (`console.cs:25517-25522`) and leaves it when the
  radio drops PTT (`:25596-25601`); its `break_in_timer` is never used
  (`console.cs:271, 855`). In CW it waits neither `key_up_delay` nor
  `mox_delay` on the host (`console.cs:29660-29670`). QSK: break-in 1, PollPTT
  skips every PTT source (`console.cs:25470`), and enabling it sets AGC
  Custom, a hang threshold of 100 if below 70, ATT on TX 31 and break-in delay
  0, restoring them on disable (`console.cs:13000-13044`). QSK is offered on
  Orion or Orion MkII with firmware 17 or later and the same TX and RX
  antenna, or on Saturn (`setup.cs:8835-8850`, "G8NJJ Saturn has QSK
  capability in any version"). Orion gateware:
  `FPGA_PTT = debounce_IO5 && ((break_in && CW_PTT) || PC_PTT)` (`Orion.v:2338-2342`).
- **Delays** (`setup.designer.cs`). Break-in delay 0 to 1000, default 300
  (`:38555-38588`). Key-Up 0 to 500, default 10 (`:9421-9478`). Key-Down
  (`udHWKeyDownDelay`, the radio's RF delay field) 0 to 1000, default 7
  (`:9487-9508`). The host `rf_delay` is not used in CW.
- **Keyer defaults.** Speed 25 (1 to 60, `console.Designer.cs:1358-1359`),
  weight 50 (`setup.designer.cs:38410`), iambic on (`:38497`), mode B on
  (`:38462`), strict spacing and reverse off, hardware sidetone on (`:38435`),
  software sidetone off (`:38319`), auto mode switch off with a return after
  1 to 10000 ms, default 2000 (`:38362-38388`, `console.cs:25684-25750`).
  CW enable is true only in CWL/CWU (`console.cs:34551-34576`).
- **Dial = carrier.** Thetis moves the DDC by the pitch (CWU minus, CWL plus)
  and, with the radio's keyer, transmits on the dial plus XIT
  (`console.cs:31773-31838`). Filters are baseband values centered on the
  pitch.
- **Pitch.** 200 to 2250 Hz, step 10, default 600 (`setup.designer.cs:38256-38285`).
  A change recenters every CW filter preset keeping its width, re-tunes,
  re-applies the filter, re-sends the APF center and the radio sidetone
  frequency (`console.cs:18212-18311`). The TUNE tone uses the pitch in every
  mode (`console.cs:30078-30086`), and TUNE in CW changes only the TX mode
  (`console.cs:30107-30121`).
- **Sidetone.** Radio sidetone level from the TX AF (monitor) level: x1.27 in
  Semi, x0.73 in Manual, a separate QSK level in QSK (`console.cs:13046-13089`).
  The computer sidetone is ChannelMaster `sidetone.c` (NR0V, GPL v2 or later),
  keyed by CWX (`netInterface.c:1172-1181`), run in the TX channel's output
  (`cmaster.c:384-397`).
- **CWX.** One element is `1200 / wpm` ms on a periodic timer (`cwx.cs:257-271`);
  nine memories; a `"` in a memory repeats it after a delay of 0 to 3600 s,
  default 3; its own speed 1 to 99, default 22 (`cwx.cs:1331-1436, 2440-2470`).
- **Hermes Lite 2.** mi0bot drives only bit 0 (CWX) and bit 3 (CWX PTT), and
  stores CWX only when break-in, MOX or CWX PTT is on (mi0bot nI:1193-1224,
  `cwx.cs:289`). Whether a given HL2 build has a paddle keyer and a sidetone is
  not in the reference set; it is a bench check.
- **Thetis status bugs.** P1 dot and dash are read with left shifts and are
  always 0 (nP1:329-331, same in mi0bot). The correct read is dash
  `(C0 >> 1) & 1`, dot `(C0 >> 2) & 1` (deskHPSDR `old_protocol.c:1666-1667`).
  On P2 Thetis stores dot and dash swapped against its own reader
  (nP2:686-693 vs `console.cs:25634, 25644`); the gateware sends dot in bit 1,
  dash in bit 2, key-out in bit 7 (`CC_encoder.v:14, 135`).
- **TCI keyer.** `keyer:rx,bool[,ms]`; a press with no release is dropped
  3000 ms after the press (`TCIServer.cs:8233, 9066-9093`, polled every 50 ms,
  `:8287`). In Semi it asserts MOX itself (`:9212-9238`).
- **PureSignal** does nothing in CW: the TX channel does not run, and TX ATT is
  forced to 31 dB in CW whatever the PS-A state (`console.cs:29612-29616`).
  NereusSDR already does this (`StepAttenuatorController::shouldForce31Db`).

## 4. Decisions

Each call says whether it follows Thetis or is NereusSDR's own, and why. Calls
that touch what makes the hardware transmit, transmit routing, or the receive
path are flagged **[TX]**, **[ROUTING]** or **[RX PATH]**.

**C1. The radio's keyer makes CW on every radio. [TX]** In CWL/CWU, MOX does not
start the TX DSP channel; NereusSDR sends keyer settings and key bits. Follows
Thetis: it is how the hardware works, and it gives the shaped, low-latency
keying operators expect. `sidetone.c`'s host CW generator (`run_tx`) is not
used.

**C2. The dial shows the carrier; the receiver shifts by the pitch. [RX PATH]**
The CW term goes into `composedShiftHz` (CWU minus pitch, CWL plus pitch), the
filters stay centered on the pitch, the passband is drawn around the dial, and
TX goes out on the dial plus XIT. Follows Thetis and every commercial radio in
effect; the mechanism differs (Thetis moves the DDC, NereusSDR moves the WDSP
shift), which keeps NereusSDR's spectrum RF-true so clicks, spots and the grid
need no compensation. Without it the radio-keyed reply lands one pitch from the
station tuned in. Saved CW frequencies keep their number and are now read as
the carrier.

**C3. One pitch, adjustable.** 200 to 2250 Hz in 10 Hz steps, default 600
(Thetis `udDSPCWPitch`). It sets the sidetone frequency, the filter centers,
the APF center, the receiver shift and the TUNE tone; the hard-coded 600s go.
Station setting on the Core, as `CWPitch` is today.

**C4. Break-in: Off, Semi, QSK. [TX]** Default Semi, delay 300 ms, as Thetis.
In Semi the radio's PTT status puts NereusSDR into MOX and the radio's own
hang time takes it out (D1). In QSK NereusSDR never enters MOX and receive
keeps running, as Thetis, with Thetis's QSK changes applied (R-CW-21). QSK is
offered on Thetis's radios and conditions (D2) and refused on a transverter
band (NereusSDR's own: transverter and sequencer relays are not made to switch
between dots). QSK always shows, greyed with the reason when unavailable.

**C5. The radio's keyer is enabled only while NereusSDR would allow the
transmit. [TX]** NereusSDR's own safety rule. In Semi and QSK the radio keys
itself from the jack before the computer hears about it, so refusing MOX
cannot stop it. The CW enable and break-in bits are sent as 1 only while the
transmitting slice is in CW and every transmit check passes for its carrier.
Thetis already disables the keyer outside CW modes (`console.cs:34555-34557`);
this extends the idea to the other checks.

**C6. Keying sources in this build.** Radio jack (paddle or straight key);
CWX; CAT `KY`/`ZZKY` and TCI `cw_msg`/`cw_macros` through CWX; TCI `keyer:`
(direct key with Thetis's 3 s release watchdog); serial key input (paddles on
DSR and CTS to the radio's keyer, a straight-key line, a PTT line). MIDI
keying waits for the MIDI feature. Thetis's `cwkeyer.cs` is dead code and is
not used.

**C7. CWX timing runs on the Core. [TX]** The sender times elements next to the
radio on its own precise timer thread, so a remote window or the phone sends
text, never dots and dashes. On HL2 it also drives mi0bot's CWX PTT bit.

**C8. Sidetone.** The radio sidetone (headphone jack, follows the paddle with
no delay) and the computer sidetone (in NereusSDR's audio, follows CWX, CAT,
TCI, the TCI key and the serial straight-key line) are both offered, both on by
default, level from the monitor volume as Thetis. Thetis leaves its software
sidetone off by default; NereusSDR turns it on because remote and computer-only
operators otherwise hear nothing from CWX. The computer sidetone does not
follow the paddle: a network round trip would put it tens of milliseconds
behind the hand, which is why Thetis leaves that source unused.

**C9. One sidetone switch, shown where Thetis and AetherSDR put it.** The CW
page carries a Sidetone button with its level, where AetherSDR's CW page has it
(`AetherSDR/src/gui/PhoneCwApplet.cpp:556-610`). The TX applet's MON button and
level drive the same switch and level in CW modes, as Thetis's MON does
(`console.cs:29056-29063`). VOX, LEV, EQ, CFC and TX BW grey out in CW with the
reason (no microphone audio is sent).

**C10. Radio field limits are honored.** Key-down-to-RF delay is limited to 0 to
255 ms and to less than one dot on the wire; the hang value is clamped to
1023 ms. NereusSDR's own fix of a Thetis overflow, stated in the page note.

**C11. Dot and dash status are read correctly. [TX]** Fixes both Thetis bugs.
It lights the KEY lamp and makes "switch to CW when the paddles are used" work.
P2 also reads key-out (status bit 7).

**C12. Hermes Lite 2.** Keyer settings still reach the radio, with a page note
that a standard HL2 KEY jack takes a straight key or an external keyer. No
computer paddle keyer is built (section 7).

**C13. Remote windows and the phone.** Every CW setting is a Core setting
mirrored on the `transmit` object; the remote desktop window gets the full CW
page, Setup page and CWX applet. The phone gets the operator functions: speed,
pitch, break-in and delay, sidetone level, and CWX send, stop and memories.

**C14. Transverter sequencing.** For OZ1CT's EME station the key-down-to-RF
delay and the key-up delay are the sequencing controls, and PTT out leads RF.
The transverter design owns which outputs switch; this design guarantees the
timing fields reach the radio intact (C10).

Calls settled while writing the spec, each from the facts in section 3:

**D1. No host break-in timer. [TX]** Follows Thetis: the radio holds PTT for
its hang time and NereusSDR leaves MOX when the radio's PTT status drops. The
design's "break-in timer returns it" is withdrawn; the unused timer member
stays as it is (it was not added by this change).

**D2. SaturnMKII counts as Saturn for QSK.** Thetis's comment says Saturn has
QSK "in any version", no Thetis model resolves to SaturnMKII, and NereusSDR's
`kSaturnMKII` row already inherits the Saturn row for the same reason
(`BoardCapabilities.cpp:1198-1230`).

**D3. QSK needs Protocol 2.** P1 has no break-in bit (section 3), so QSK is
offered only on a P2 connection.

**D4. TCI `dds` and `if` stay RF-true. [RX PATH]** NereusSDR's DDC does not
move (C2), and TCI IQ streams are centered on the real stream center, so
reporting Thetis's shifted `dds` would mislabel the IQ. Both lines stay as
today and `vfo = dds + if` holds. NereusSDR's own; the `TciProtocol.h` note is
rewritten to say so. (Restoring the term on `if` alone would break the
invariant.)

**D5. Passband origin is one helper on every client.** `SliceModel` gains
`passbandOriginHz()` = dial + CW offset, computed from the mode and the
mirrored `CWPitch` on the desktop and the phone, used by every drawer; drags
convert back by subtracting it. A mirrored offset property would need a wire
change for a value every client can compute.

**D6. TUNE in CW changes only the TX mode. [TX]** Follows Thetis
(`console.cs:30107-30121`): the slice stays in CW (shift, filter and flag
unchanged) and only the transmit channel runs USB or LSB for the tone.
NereusSDR today switches the slice's mode.

**D7. A pitch change never moves the dial or the carrier.** With the radio's
keyer the carrier is the dial, so a pitch change moves only the receiver
shift, the filters, the APF and the sidetone, at any time, keyed or not. TUNE
uses the pitch latched when TUNE started. Thetis moves the VFO by the change
while in MOX; NereusSDR does not need to.

**D8. CW filters follow the pitch, including the operator's own.** Follows
Thetis: every CW preset, the operator's overridden slots and the stored
per-band CW filters are recentered on the new pitch keeping their width, and
the current filter is re-applied.

**D9. Key-down delay is clamped twice.** The control runs 0 to 255 ms; the wire
value is the smaller of that and `900 / wpm` (deskHPSDR's workaround for the
gateware's one-dot rule). When the second clamp applies, the page note says
what the radio is using.

**D10. Off on P1 is checked on the bench.** P1 carries no break-in bit and the
P1 keyer gateware is not in the reference set. NereusSDR sends hang 0 in Off,
as Thetis, and the bench (V-HW-3) records whether the radio keys from the jack
without MOX; the C5 gate makes either answer safe.

**D11. Eight CWX memories.** The approved CWX applet and mockup have M1 to M8.
CAT `ZZKM` 1 to 8 send them; 9 answers `?;`. Thetis has nine.

**D12. CWX speed and repeat.** CWX has its own speed, 5 to 60 WPM (the applet's
range), default 22 (Thetis). Repeat is the applet's Rpt switch with a delay of
1 to 60 s, default 3 (Thetis's default); a `"` at the end of a memory also
repeats it, as Thetis. CAT and TCI speeds outside 5 to 60 are clamped.

**D13. CAT `KY` switches to CW only on a set.** Thetis switches the mode on a
read too, which a logger polling the buffer would trip; `ZZKY` already works
the NereusSDR way in Thetis. Fixed in our copy with a note.

**D14. The serial key reader is edge driven, on its own thread.
[ARCHITECTURE]** A 20 ms poll is a whole dot at 60 WPM. Linux waits with
`TIOCMIWAIT`, Windows with `WaitCommEvent`, macOS polls every 1 ms (no modem
line wait there).

**D15. The computer sidetone is produced on the audio drain.** It fills a block
into the TX monitor slot from `AudioEngine::drainMixes`, so it follows MON
volume and output and reaches a remote owner mix the way MON does. It keeps
running in QSK (Thetis silences it there because the radio mixes its own
sidetone into the radio's audio; NereusSDR's operators may not be at the
radio).

**D16. An amplifier NereusSDR sequences limits CW to Off. [TX]** When an
amplifier interlock is armed, NereusSDR holds RF until the amplifier grants it
(`RadioModel.cpp:29930-29960`). In Semi and QSK the radio makes RF from the
jack before NereusSDR hears the key, so that hold cannot work. While such an
interlock is armed, Semi and QSK are greyed with the reason, and in Off the
keyer is enabled only once the interlock has granted. An amplifier keyed from
the radio's PTT out is not affected. NereusSDR's own.

## 5. Requirements

### Settings

R-CW-01. `TransmitModel` gains these Core settings, each a `Q_PROPERTY` with a
NOTIFY signal, clamped and idempotent in its setter, saved under
`hardware/<mac>/tx/` like `voxHangTimeMs` (`TransmitModel.cpp:3094-3106,
2175-2190`), booleans as `"True"`/`"False"`:

| Property | Type, range | Default | Key | Source |
| --- | --- | --- | --- | --- |
| `cwKeyerSpeed` | int 1..60 WPM | 25 | `CW_KeyerSpeed` | `console.Designer.cs:1358-1359` |
| `cwKeyerMode` | int 0 straight, 1 iambic A, 2 iambic B | 2 | `CW_KeyerMode` | `setup.designer.cs:38462, 38497` |
| `cwKeyerWeight` | int 33..66 | 50 | `CW_KeyerWeight` | `setup.designer.cs:38410` |
| `cwReversePaddles` | bool | False | `CW_ReversePaddles` | Thetis default |
| `cwStrictSpacing` | bool | False | `CW_StrictSpacing` | Thetis default |
| `cwBreakInMode` | int 0 Off, 1 Semi, 2 QSK | 1 | `CW_BreakInMode` | `console.cs:14752` |
| `cwBreakInDelayMs` | int 0..1000 | 300 | `CW_BreakInDelay` | `setup.designer.cs:38555-38588` |
| `cwKeyDownDelayMs` | int 0..255 | 7 | `CW_KeyDownDelay` | `setup.designer.cs:9487-9508`, C10 |
| `cwKeyUpDelayMs` | int 0..500 | 10 | `CW_KeyUpDelay` | `setup.designer.cs:9421-9478` |
| `cwSidetone` | bool | True | `CW_Sidetone` | `console.Designer.cs:1461` |
| `cwRadioSidetone` | bool | True | `CW_RadioSidetone` | `setup.designer.cs:38435` |
| `cwComputerSidetone` | bool | True | `CW_ComputerSidetone` | C8 |
| `cwAutoModeSwitch` | bool | False | `CW_AutoModeSwitch` | `console.cs:18624` |
| `cwAutoModeReturn` | bool | False | `CW_AutoModeReturn` | `console.cs:25684` |
| `cwAutoModeReturnMs` | int 1..10000, step 10 | 2000 | `CW_AutoModeReturnMs` | `setup.designer.cs:38362-38388` |
| `cwSerialPort` | string, empty = none | empty | `CW_SerialPort` | D14 |
| `cwSerialPaddles` | bool | False | `CW_SerialPaddles` | `SDRSerialPortII.cs:259-263` |
| `cwSerialKeyLine` | int 0 none, 1 DSR, 2 CTS | 0 | `CW_SerialKeyLine` | `SDRSerialPortII.cs:246-257` |
| `cwSerialPttLine` | int 0 none, 1 DSR, 2 CTS | 0 | `CW_SerialPttLine` | same |
| `cwxSpeed` | int 5..60 WPM | 22 | `CWX_Speed` | D12 |
| `cwxRepeat` | bool | False | `CWX_Repeat` | D12 |
| `cwxRepeatDelayS` | int 1..60 | 3 | `CWX_RepeatDelay` | D12 |
| `cwxMemories` | QStringList, 8 entries | Thetis's first eight defaults (`cwx.cs:895-903`) | `CWX_Memory1`..`8` | D11 |

The monitor level is the existing `monitorVolume` (one level, C9). The QSK
restore values (R-CW-21) are saved beside these under `CW_QskSaved*`.

R-CW-02. The pitch gains a property: `RadioModel::cwPitchHz` (int) with
`setCwPitchHz` and `cwPitchChanged`, backed by the station key `CWPitch`
(`SettingsScope::Station`, unchanged). The setter clamps to 200..2250 and
rounds to 10 Hz; a stored value outside that range reads clamped. On a remote
window it reads the catalogue-mirrored key (`StationServer.cpp:1580-1586`) and
emits on change. The three `SliceModel` read sites use one accessor with the
new clamp; `kApfCwPitchHz`, the APF push, the Setup APF range and the TUNE
tone's `kCwPitch` use the live pitch. CAT `ZZCL` sets and reads it (Thetis
`CAT/CATCommands.cs:1753-1773`, clamped as `setup.cs:5731-5736`).

R-CW-03. Every R-CW-01 property mirrors `Bidirectional` on the `transmit`
object (`MirrorPolicy.cpp:511-600`) under `transmitSettingsVersion` 16 (it is
15 today, `StationServer.cpp:13437-13518`). A window or phone on an older Core
shows the CW controls disabled with the existing update reason; the Core
refuses writes from an older peer. Remote writes out of range are refused with
operator words through `settingRangeRefusal` (for example "Choose a keyer speed
from 1 to 60 WPM.").

R-CW-04. The serial key settings name a port on the Core's computer. The Core
refuses a port already used by CAT or the CAT PTT input, with the reason, as
the PTT input does today (`CatService.cpp:917-922`).

### The keyer gate

R-CW-05. **[TX]** A Core object, `CwKeyerGate` (in `src/core/safety/`, no GUI
includes), answers "may the radio's keyer run" with a bool and refusal words.
It is true only when all of these hold:

- a transmit slice exists and its mode is CWL or CWU;
- the band plan allows CW on the carrier (`txFrequencyForSlice`, carrier only,
  as Thetis `console.cs:6778-6813`), including region, extended, US 60 m and
  "prevent TX on a band different from RX";
- the station is not receive only (setting, board, or Core policy and role);
- transmit inhibit is off, the PA has not tripped, the radio link is up, no
  radio change is under way;
- no emergency stop or TX time-out latch is set;
- `TxInterlockPolicy::evaluateTxRequest` passes (amplifier standby, SWR), and
  under D16 an armed amplifier interlock has granted.

The PC microphone check is not part of it (no microphone in CW), and neither is
the transmit holder (R-CW-33 applies the holder to computer keying requests).

R-CW-06. **[TX]** The gate re-evaluates on every change of its inputs: the
signals listed in the transmit checks today (slice frequency, mode, XIT, TX
slice, receive only, inhibit, PA trip, link, interlock, latches) and three new
ones for `BandPlanRegion`, `ExtendedTransmit` and `PreventTxOnDifferentBandToRx`,
emitted by `RadioModel` from every path that writes those keys (Setup and
CAT), since `AppSettings` has no signals.

R-CW-07. **[TX]** The connection's CW enable (P1 bank 13 C1, P2 CmdTx byte 5
bit 1) and break-in bit (P2 byte 5 bit 7) are 1 only while the gate is true
and, for break-in, the mode is Semi or QSK. When the gate turns false the new
state goes out at once: P2 sends CmdTx and high priority immediately; P1 puts
bank 13 next in the round robin (as `m_forceBank11Next` does today) and stops
the key-bit overlay on the next frame.

R-CW-08. **[TX]** The transmit cut latch (`RadioConnection::transmitCut()`) is
read on the connection's send thread wherever these bits are built: under the
cut, CW enable, break-in and every key bit read 0 whatever the gate says, and
P2 sends CmdTx and high priority at once. This is the off-main-thread form of
C5, so a stuck main thread cannot leave the keyer on.

R-CW-09. A key-down the gate refuses (a dot, dash or PTT seen in the radio's
status, a CWX send, a CAT or TCI key) reports the gate's words through the
existing `moxRejected` and `moxRefused` signals, once per hold (the existing
quiet-repeat rule), and the CW page's KEY lamp and status line show them.

R-CW-10. The TX time-out counts while MOX is on and, in QSK, while the radio
reports key-out (P2 status bit 7). When it fires it latches as today, which
turns the gate false. Thetis has no QSK time-out; NereusSDR's own.

### Protocol 1

R-CW-11. **[TX]** `P1CodecStandard` (and so `P1CodecAnvelinaPro3`),
`P1CodecRedPitaya` (in its own `bank12`) and `P1CodecHl2` fill banks 12, 13
and 14 per the section 3 table from new `CodecContext` P1 CW fields. The
settings are sent at all times, as Thetis; CW enable follows R-CW-07. Cites use
v2.10.3.15 line numbers. The ADC attenuation bits in bank 12 C1 and C2 are
unchanged.

R-CW-12. **[TX]** The P1 key bits are written on the `P1Ep2Sender` thread in
`composeCommandFrame`, after the two `fillTxZone` calls: while the frame's CW
enable is 1, every 16-bit I and Q sample in both zones is replaced by the key
value (`dot<<2 | dash<<1 | cwx`; on HL2 `cwx_ptt<<3 | dot<<2 | dash<<1 | cwx` with mask `0x0F`).
The key state is an atomic the CWX sender, the TCI key and the serial reader
write. When CW enable is 0 the words are left as `fillTxZone` wrote them,
including the HL2 low-bit mask. Key bits are not put in the TX I/Q ring.

R-CW-13. Wire clamps on both protocols: speed 1..60 (never 0); weight 33..66;
RF delay `min(cwKeyDownDelayMs, 255, 900 / cwKeyerSpeed)`; hang
`min(cwBreakInDelayMs + cwKeyUpDelayMs, 1023)` in Semi, `min(cwKeyUpDelayMs, 1023)`
in QSK (delay 0, R-CW-21), 0 in Off; sidetone level 0..127; sidetone frequency
the pitch.

R-CW-14. The legacy compose paths (`NEREUS_USE_LEGACY_P1_CODEC`,
`NEREUS_USE_LEGACY_P2_CODEC`) are not changed and send no CW. The P1 and P2
regression baselines (`tests/data/p1_baseline_bytes.json`,
`p2_baseline_bytes.json`) are updated for the CW defaults now sent, with a
note in each freeze test's header, as was done for bank 10 mic boost.

### Hermes Lite 2

R-CW-15. **[TX]** On HL2 the CWX sender also drives `cwx_ptt` (bit 3) for the
length of a message plus its hang (mi0bot `cwx.cs:289`), and the CWX bit is
stored only while break-in, MOX or CWX PTT is on (mi0bot nI:1193-1224). This
rule is HL2 only. QSK is never offered on HL2. The Setup page notes that a
standard HL2 KEY jack takes a straight key or an external keyer; iambic
settings stay enabled and are sent.

### Protocol 2

R-CW-16. **[TX]** `P2RadioConnection` assigns `m_cw` and `m_tx[0].cwx/dot/dash`
from the settings, the gate and the key state. CmdTx byte 5 is composed with
bit 0 (EER) kept as today; edge length is 9 (Thetis `setup.cs:29327`). A key
change sends high priority at once; a setting or gate change sends CmdTx at
once. While QSK is active and the gate is true, CmdTx bytes 57 to 59 carry 31
for every ADC (the radio applies them while it transmits, and NereusSDR never
flips MOX in QSK).

### Radio status

R-CW-17. **[TX]** P1 reads dash as `(C0 >> 1) & 1` and dot as `(C0 >> 2) & 1`,
skipping I2C response subframes as the PTT read does. P2 reads dot bit 1, dash
bit 2 and key-out bit 7 of status byte 4. Both publish them with a new
`RadioConnection` signal `radioKeyStatus(bool dot, bool dash, bool keyOut)`
(P1 reports key-out as false), emitted on change beside `micPttFromRadio`.
Each read carries a note that it differs from Thetis and why (CLAUDE.md, a
shared upstream bug).

### MOX and break-in

R-CW-18. **[TX]** In CWL/CWU, MOX flips the hardware (`hardwareFlipped`, the
MOX bit, the existing 31 dB rule) and does not start the TX DSP channel: the
`txReady` consumer skips `openTxRfGate`. RX to TX waits no host RF delay. TX to
RX skips the drain wait and the key-up and MOX delays and keeps
`ptt_out_delay`, as Thetis (`console.cs:29651-29679`). The delays become
production settings where CW needs them (`cwKeyUpDelayMs` reaches the hang,
R-CW-13); the `constexpr` values for voice modes are not changed.

R-CW-19. **[TX]** In Off and Semi, the radio's PTT status keys MOX in CW as
`PttMode::Cw` with the keyer identity "the radio's key", and MOX ends when the
radio drops PTT (D1). The serial PTT line keys MOX only in Semi
(`console.cs:25473`). `MoxController::onCwPtt` stops refusing and carries
these. A refused gate (R-CW-05) means the radio's keyer is off, so no RF can
follow a PTT status in CW.

R-CW-20. **[TX]** QSK ignores every PTT source (radio, CAT, TCI, VOX, serial;
`console.cs:25470`) and never enters MOX. It is offered only when all hold:
P2 connection (D3); Orion or Orion MkII board with firmware 17 or later, or
Saturn or SaturnMKII (D2); the TX slice's TX antenna equals its RX antenna;
not a transverter band; not HL2; no armed amplifier interlock (D16). Otherwise
it shows greyed with the first failing reason. The rule re-runs on an antenna
or band change; if QSK stops being allowed while selected, NereusSDR falls back
to Semi and says why. Leaving CW turns QSK off and remembers it for the next
time the slice enters CW (`console.cs:34566-34576`).

R-CW-21. **[TX]** Entering QSK applies Thetis's changes (`console.cs:13016-13029`)
to the TX slice's receiver: AGC mode Custom, and a hang threshold below 70
raised to 100. The values it replaces are saved under `CW_QskSaved*` and put
back on leaving QSK, also after a restart in QSK. The break-in delay setting is
not overwritten: the hang sent in QSK uses a delay of 0 (R-CW-13), and the
Delay control greys with the reason. TX attenuation is 31 dB (R-CW-16).

R-CW-22. **[TX]** Under D16, while an amplifier interlock is armed, Semi and
QSK are greyed with "NereusSDR switches the amplifier, so the radio can't key
it between characters. Use Off, or key the amplifier from the radio's PTT
out." A selected Semi or QSK falls back to Off with that reason.

### Receive shift and pitch

R-CW-23. **[RX PATH]** `RadioModel::composedShiftHz(slice, centre)` adds the CW
term (CWU minus pitch, CWL plus pitch), never `shiftOffsetHz`.
`SliceModel::demodulatedRxFrequency()` adds the same term. A pitch change
re-pushes the shift of every CW slice (beside the triggers at
`RadioModel.cpp:25474-25486`). `txFrequencyForSlice` is unchanged: TX is the
dial plus XIT.

R-CW-24. Every passband drawer uses `SliceModel::passbandOriginHz()` (D5): the
pan filter state, peak blobs and peak dBm in the passband, the waterfall
overlay, the GPU overlay, own, other and foreign slice markers, and filter-edge
grab and drag (drags subtract the offset before `setFilter`); on the phone,
`BandSlice`, foreign slice markers and the slices models. The VFO line and TX
marker stay on the dial.

R-CW-25. Notches keep their mechanism: no `GetDSPcwPitchShiftToZero` term, as
today. +TNF in CW lands on the carrier through the R-CW-23 term. The NotchModel
comments that say the CW term is absent from the shift are rewritten.

R-CW-26. TCI `dds` and `if` are unchanged (D4); the `TciProtocol.h` note is
rewritten to give the reason.

R-CW-27. The APF center is the pitch plus the APF tune offset; a pitch change
re-pushes it. The Setup APF Center range is the pitch plus or minus 500.

R-CW-28. A pitch change recenters CW presets, overridden slots and stored
per-band CW filters keeping their width (D8), re-applies the current CW filter,
re-pushes the shift, APF and radio sidetone frequency, and never moves the dial
or carrier (D7).

R-CW-29. Saved CW frequencies are not migrated: the number is read as the
carrier. The release notes say, in operator words, that a station saved while
listening in CW is now heard one pitch off and needs one retune.

R-CW-30. **[TX]** TUNE: the tone is plus or minus the pitch in every mode
(`console.cs:30078-30086`), latched at TUNE start for the NCO compensation; in
CW only the TX channel's mode changes to USB or LSB (D6).

### CWX sender

R-CW-31. **[TX] [ARCHITECTURE]** A Core `CwxSender` runs its element timing on
its own `std::thread` (like `RemoteTxCutGuard`) with a high-resolution sleep
and a new `ThreadRole::CwxSender`. One element is `1200 / cwxSpeed` ms
(`cwx.cs:404-407`). The main thread gives it text, memory numbers, stop and
key commands; it writes the key state atomic (R-CW-12) and, on P2, wakes an
immediate high-priority send without a lock. `docs/architecture/overview.md`'s
thread table is updated with every thread that exists today plus this one and
R-CW-36's.

R-CW-32. CWX sends text, memories 1 to 8, prosigns (`|SK|` and the others,
`TCIServer.cs:8659-8676`), the `"` repeat marker and the Rpt switch (D12),
speed changes (`>` and `<`, plus or minus 5 WPM), stop (aborts and clears),
and Key (holds the key while held; a hold whose release never arrives is
dropped 3 s after its last press, the TCI rule). It refuses outside CWL and
CWU with words, and leaving CW aborts it. In Semi and QSK the radio keys from
the CWX bit; in Off CWX keys only while MOX is held, and the status line says
so (Thetis).

R-CW-33. Computer keying requests (CWX from any device, CAT, TCI, the CWX Key)
pass the existing keying gate (who may key) at request time, then R-CW-05.

### CAT and TCI

R-CW-34. CAT, after Thetis `CAT/CATCommands.cs` with the field sizes of
`CATStructs.xml`: `KY` and `ZZKY` send through CWX (read answers the buffer
state as Thetis; `KY` switches to CW only on a set, D13); `KS` and `ZZKS` the
CWX speed; `ZZKM` memories 1 to 8; `ZZKO` reports and toggles the CWX applet
where a GUI exists and answers `?;` on a headless Core; `ZZSS` stop; `ZZCS` the
keyer speed; `ZZCL` the pitch; `ZZCB` break-in on or off (on restores Semi);
`ZZQK` QSK where R-CW-20 allows it, else `?;`; `ZZCD` the break-in delay,
clamped to 0..1000 (Thetis clamps 150 to 5000 against a UI of 0 to 1000; ours
follows the UI, with a note); `ZZCI` iambic. `ZZCM` and `ZZCF` keep answering
`?;` (section 7).

R-CW-35. TCI, after `TCIServer.cs` `TCICWController`: `cw_macros`, `cw_msg`
(both forms), `cw_terminal`, `cw_macros_stop`, `cw_macros_speed` (set, query,
up, down), `cw_macros_delay`, `cw_keyer_speed`, and `keyer:rx,bool[,ms]` with
the 3000 ms watchdog polled every 50 ms and MOX asserted in Semi; the
`cw_macros_empty` notification; real values in the init burst instead of the
fixed 30s. Every macro goes through the CWX sender.

### Serial key input

R-CW-36. **[TX] [ARCHITECTURE]** A Core `CwSerialKeyInput` reads DSR and CTS of
`cwSerialPort` on its own thread, edge driven (D14), with a new
`ThreadRole::CwSerialKey`. Paddles: DSR is dot, CTS is dash, sent as the host
dot and dash bits to the radio's keyer (`SDRSerialPortII.cs:259-263`). The
straight-key line keys the CWX bit; the PTT line keys MOX in Semi (R-CW-19). The
gate (R-CW-05) and the cut (R-CW-08) apply to all three.

R-CW-37. With `cwAutoModeSwitch`, a dot or dash (radio status or serial
paddles) while the TX slice is not in CW switches it to CWL from LSB or DIGL
and to CWU otherwise; with `cwAutoModeReturn` it returns to the old mode
`cwAutoModeReturnMs` after the last dot or dash (`console.cs:25698-25750`,
"[2.10.1.0]MW0LGE implements #70").

### Sidetone

R-CW-38. The radio sidetone level is 0 unless `cwSidetone` and `cwRadioSidetone`
are on and RADIO is not muted (R-SPK-09 note: RADIO mute silences the
radio's sidetone too); otherwise `round(monitorVolume * 100 * 1.27)` in Semi and QSK
and `round(monitorVolume * 100 * 0.73)` in Off (`console.cs:13046-13089`;
NereusSDR uses one level in QSK so the CW page has one slider). P2 byte 5 bit 4
is set when the level is above 0. The frequency is the pitch.

R-CW-39. The computer sidetone ports `sidetone.c` (`create_sidetone`,
`xsidetone`, `keySidetone`, the raised-cosine 5 ms edge, `cmaster.c:234-251`)
into the audio engine, producing into `kTxMonitorSlotId` from `drainMixes`
(D15). It runs while the TX slice is in CW, `cwSidetone` and
`cwComputerSidetone` are on; its volume is `monitorVolume`; it is keyed through
an atomic by the CWX sender, the TCI key and the serial straight-key line, never
by the paddle. It reaches a remote owner mix as MON does.

R-CW-40. `BoardCapabilities::hasSidetoneGenerator` is fixed in the table that
derives it: true for every board whose Thetis path sends the sidetone level,
false for the two HL2 rows until the bench check (V-HW-6) says otherwise, with
the wrong `IoBoardHl2.cs` cite replaced. On HL2 the Setup page says the radio
sidetone may not be present.

### Screens

R-CW-41. **CW page** (Phone/CW applet, as the mockup): Delay (break-in delay,
greyed in Off and QSK with the reason), Speed (1 to 60), Sidetone button with
its level, the L/R pan, then Break-in (steps Off, Semi, QSK, skipping an
unavailable QSK and saying why), Iambic, and Pitch with its arrows (10 Hz).
Under them the KEY and TX lamps and the keyer's state line (keyer ready,
keyer off and why, or the refusal words). No ALC gauge.

R-CW-42. **TX applet in CW:** MON and Mon Vol show and set `cwSidetone` and
`monitorVolume`; VOX, LEV, EQ, CFC and TX BW are greyed with "Not used in CW:
the radio makes the signal." through a fourth gate layer with its own saved
keys, applied in `onMoxModeChanged`, so it stacks with the receive-only and
remote layers. The MOX tooltip in CW stops saying CW is unavailable.

R-CW-43. **Setup > DSP > CW** (as the mockup): Keyer (pitch, speed, mode
Straight/Iambic A/Iambic B, weight 33 to 66, reverse paddles, strict spacing,
HL2 note); Break-in (mode, delay, key-down to RF 0 to 255 with the D9 note, key
up 0 to 500, the derived radio hang time); Sidetone (radio, computer, level,
the HL2 note under R-CW-40; the line says it follows CWX, CAT, TCI and serial
keying); Key input on a serial port (port, paddles, straight-key line, PTT
line); Options (switch to CW on the paddles, return after); APF unchanged. The
placeholder "Bug", "Letter Spacing" and "Dot-Dash Ratio" go.

R-CW-44. **CWX applet:** constructed and wired (it is never built today); text,
Send, Stop and Key on the Send row, Speed, M1 to M8, Rpt with its delay,
KB to CW; sent and current characters shown in the text; right-click a memory
to edit it. Tools > CWX and the status-bar CWX label work.

R-CW-45. **Slice flag and pan:** the flag reads the dial, which is the carrier;
the passband sits around it (R-CW-24); the TX marker is on the dial.

R-CW-46. `UnbuiltFeature::Cwx` becomes built, and every site listed in the
research (menu, status bar, CW tab, page 1, Setup groups, station tool `cwx`,
CAT comments, the preview plate) shows the built surface; the tests that
expect the hidden surfaces are updated.

R-CW-47. Operator words: every refusal and greyed reason above is plain
English with no cites, codes or internal names (CLAUDE.md and the wording
sweep test).

### Remote windows and the phone

R-CW-48. A remote desktop window builds the same CW page, Setup page and CWX
applet, bound to the mirrored `transmit` object and the station pitch.

R-CW-49. New session verbs under a new capability `remoteCwVersion` 1:
`cw.send` (text), `cw.memory` (1 to 8), `cw.stop`, and `cw.key` (down or up,
with the 3 s release watchdog on the Core). The Core runs the sender; the
reply carries the refusal as `TxRefusal`. A peer without the capability gets
the CWX controls disabled with the update reason.

R-CW-50. The phone gets, behind `transmitSettingsVersion` 16 and
`remoteCwVersion` 1: speed, pitch, break-in mode and delay, and sidetone level
on its transmit panel in CW; a CWX page under Tools for the `cwx` tool (text,
send, stop, memories). The new Setup rows stay out of `resources/setup/dsp.json`
so the phone's described Setup page does not grow (C13); the description parity
test allows a named desktop-only list for them.

### Attribution

R-CW-51. Every port carries its upstream header byte for byte in the commit
that adds it, `// From` cites at v2.10.3.15 (mi0bot at v2.10.3.13-beta2,
deskHPSDR at `f3d857c`), preserved inline tags, and provenance rows:
`sidetone.c`/`sidetone.h` (NR0V; `sidetone.h` uses NEL line terminators,
which are converted to newlines without changing the header text), `cwx.cs`,
`TCIServer.cs`, `SDRSerialPortII.cs`, `netInterface.c`, `networkproto1.c`,
`network.c`, `console.cs`, `setup.cs`, and the mi0bot files.

R-CW-52. The stale headers that credit Longpath code trunk does not have
(`RadioModel.cpp:1675-1680`, `RadioModel.h:816-817`, `SliceModel.cpp:313-318`,
`SliceModel.h:287-290`) are corrected when those files gain the real code, and
the touched cites at v2.10.3.13 are refreshed (`setup.cs:17112`,
`console.cs:30082/30085`, the `MoxController.h` delay cites, the TCI keyer
watchdog cite).

## 6. Errors and refusals

| Situation | What the operator sees |
| --- | --- |
| Paddle pressed, gate refused | No RF (keyer off). KEY lamp lit, TX lamp dark, status line and toast with the gate's words, once per hold. |
| CWX send outside CW | "Switch the slice to CW to send." |
| CWX send while another device holds transmit | The keying gate's existing words. |
| QSK unavailable | QSK step greyed with the first reason (protocol, radio, firmware, antenna, transverter, HL2, amplifier). |
| QSK stops being allowed while selected | Falls back to Semi (Off under D16), toast with the reason. |
| Key-down delay clamped by speed | Note under the control: "At 30 WPM the radio uses at most 30 ms." |
| Serial port busy or missing | Setup row shows the reason; no keying from serial. |
| Link lost while keyed | The existing link-down cut turns the gate false and the cut latch zeroes the bits on the send thread. |
| TCI or CWX Key held with no release | Released 3 s after the last press. |

## 7. Out of scope

- A computer paddle keyer (iambic timing on the computer) for radios without
  one, such as a standard HL2. Its timing over USB and the network needs its
  own study.
- MIDI keying (waits for MIDI), CAT `ZZCM` and `ZZCF`, "Show TX frequency" and
  the CW zero line.
- PureSignal in CW: nothing to do (section 3).
- CW receive noise reduction. TAPR WDSP 2.11 (2026-10-15) adds CW-trained NNR
  models; NereusSDR vendors WDSP 2.10 with voice NNR only, and taking 2.11 is a
  re-merge of the vendored WDSP plus NNR slot changes: receive-path work with
  its own design, from TAPR's code and the WDSP guide.
- Transverter output switching (the transverter design).

## 8. Verification

Software tests (this machine, offscreen):

- V-SW-1. Codecs: bank 12, 13 and 14 bytes for every P1 codec and P2 CmdTx bytes
  5 to 13, 17 and 57 to 59 and high-priority byte 5, for each mode, clamp and
  default, against bytes worked out from Thetis and mi0bot; EER kept; HL2 bit 3.
  (R-CW-11, R-CW-13, R-CW-15, R-CW-16)
- V-SW-2. P1 key overlay: with CW enable 1 every I and Q word of both zones is the
  key value; with 0 the words are untouched (HL2 mask kept); under the cut they
  are 0. (R-CW-08, R-CW-12)
- V-SW-3. Gate truth table: each input alone turns it false with its words; the
  three new setting signals re-evaluate it; CW enable and break-in follow it on
  both protocols, sent at once. (R-CW-05 to R-CW-07)
- V-SW-4. Status parsing: P1 dash and dot from C0 bits 1 and 2; P2 dot, dash and
  key-out from bits 1, 2 and 7; I2C subframes skipped. (R-CW-17)
- V-SW-5. MoxController in CW: radio PTT keys and unkeys MOX in Off and Semi, the
  TX channel never starts, release has no key-up or MOX wait; QSK ignores every
  source; the time-out runs on key-out in QSK. (R-CW-10, R-CW-18 to R-CW-20)
- V-SW-6. QSK availability for every board, protocol, firmware, antenna,
  transverter, HL2 and interlock case; save and restore of AGC and hang threshold
  across a restart; D16 fallbacks. (R-CW-20 to R-CW-22)
- V-SW-7. Receive shift: CWU and CWL rows in the notch tune, notch add, TNF and
  TX-frequency tests (demod = dial minus or plus pitch, TX = dial plus XIT, +TNF
  on the carrier); TCI `dds`/`if` rows in CW unchanged; a pitch change re-pushes.
  (R-CW-23, R-CW-25, R-CW-26)
- V-SW-8. Pitch: clamp and step; presets, overrides and stored filters recenter;
  APF follows; TUNE tone follows in every mode and stays latched; TUNE in CW keeps
  the slice mode. (R-CW-02, R-CW-27, R-CW-28, R-CW-30)
- V-SW-9. CWX timing: element lengths at 5, 22 and 60 WPM within 1 ms on the
  sender thread; repeat, prosigns, speed marks, stop; Key watchdog; HL2 CWX PTT
  and store rule. (R-CW-15, R-CW-31, R-CW-32)
- V-SW-10. CAT and TCI rows for every command in R-CW-34 and R-CW-35 against
  `tests/data/cat/` and the TCI tests, including the D13 difference.
- V-SW-11. Serial reader: edges on DSR and CTS reach dot, dash, CWX and PTT with a
  fake port; port conflict refused. (R-CW-04, R-CW-36)
- V-SW-12. Sidetone: radio level table and RADIO mute; computer sidetone fills the
  monitor slot only when keyed and switched on, at the pitch, with the 5 ms edge.
  (R-CW-38, R-CW-39)
- V-SW-13. Mirror and remote: the R-CW-01 properties round-trip; an older peer is
  refused and sees disabled controls; `cw.*` verbs run on the Core and refuse
  through the keying gate. (R-CW-03, R-CW-33, R-CW-49)
- V-SW-14. Gates and wiring: unbuilt-feature, station-tools, remote-gating,
  controls-connected, setup-controls-connected, description parity (desktop-only
  list), wording sweep; `tst_core_has_no_gui_includes` stays green.
  (R-CW-42 to R-CW-47, R-CW-50)
- V-SW-15. Phone: transmit panel and CWX page tests behind the two versions.
  (R-CW-50)

UI checks (rendered pixels, by the repository's UI gate, each theme): the CW page
in Semi, QSK-unavailable and refused states; the TX applet in CW; Setup > DSP > CW
(and its scrolled or min variants if it outgrows the dialog); the CWX applet; the
pan and flag in CWU with the passband around the dial (the cw-digital scene, whose
captures change); an HL2 pose. Compared against the mockup.

Hardware (JJ at the radio into a dummy load, a second receiver or scope for the
carrier and the keying shape):

- V-HW-1. ANAN-G2 (Saturn, P2): paddle and straight key in Off and Semi, CWX,
  CAT `KY`, TCI `keyer`, serial paddles, both sidetones; carrier on the dial.
- V-HW-2. ANAN-G2 QSK: receive heard between dots; AGC and attenuation restored
  when QSK goes off.
- V-HW-3. A P1 radio: the same as V-HW-1, plus whether Off keys from the jack
  without MOX (D10).
- V-HW-4. Gate on air: a paddle held while tuning out of band stops RF at the band
  edge; transmit inhibit and a link pull stop RF.
- V-HW-5. Key-down and key-up delays measured at PTT out and RF for OZ1CT's
  sequencing case (C14).
- V-HW-6. Hermes Lite 2: straight key, CWX with the CWX PTT hold, and whether the
  radio has a paddle keyer and a sidetone (R-CW-15, R-CW-40).
- V-HW-7. Remote window and phone through a Core: CWX send, Key release on a
  dropped link.

## 9. Where this spec differs from the mockup

- HL2: Iambic is no longer greyed; the settings are sent and the page notes the
  standard KEY jack (R-CW-15).
- Speed runs 1 to 60 (the mockup's slider starts at 5).
- Rpt delay defaults to 3 s (the mockup shows 6).
- Delay greys in Off and QSK with the reason; the mockup greys it in Off only.
- QSK and Semi greyed under an armed amplifier interlock (D16) is a new state the
  mockup does not draw.
- The Setup sidetone line names serial keying too.
- The key-down note gains the speed clamp (D9).

## 10. What this change touches (flags)

| Flag | Where |
| --- | --- |
| What makes the hardware transmit | C1, C4, C5, C7, D1, D6, D16; R-CW-05 to R-CW-22, R-CW-30 to R-CW-33, R-CW-36 |
| Receive path | C2, D4; R-CW-23 to R-CW-29 |
| Threads (architecture) | R-CW-31 (`CwxSender`), R-CW-36 (`CwSerialKeyInput`), D14, D15 |
| Wire protocol version | R-CW-03 (`transmitSettingsVersion` 16), R-CW-49 (`remoteCwVersion` 1) |
| User-facing defaults | R-CW-01 (computer sidetone on, unlike Thetis), D12 |
| Open PRs nearby | #662 changes the force-31 gates the CW rule rides on; #668 is a design doc for the remote transmit stop. Whichever lands second rebases. |

## 11. Calls made for JJ's veto

1. C2: the dial shows the carrier in CW, by shifting the receiver.
2. C5: the radio's keyer is enabled only while NereusSDR would allow the transmit.
3. C4: QSK also refused on transverter bands.
4. C9: one sidetone switch on the CW page, with MON driving it in CW.
5. C10, D9: key-down-to-RF limited to 255 ms and to less than one dot.
6. C11: dot and dash read correctly (Thetis bugs fixed).
7. C8, D15: computer sidetone on by default, follows computer keying only, keeps
   running in QSK.
8. C6: serial key input in; MIDI keying and a computer paddle keyer out.
9. C13: the phone gets the operator functions, not the Setup page.
10. D1: no host break-in timer; the radio's hang time ends Semi.
11. D2, D3: SaturnMKII counts as Saturn; QSK needs Protocol 2.
12. D4: TCI `dds` and `if` stay RF-true.
13. D6: TUNE in CW changes only the TX mode.
14. D7, D8: a pitch change never moves the dial; CW filters follow it.
15. D11, D12: eight CWX memories; CWX speed 5 to 60; repeat delay default 3 s.
16. D13: CAT `KY` switches to CW only on a set.
17. D14: an edge-driven serial reader on its own thread.
18. D16: an amplifier NereusSDR sequences limits CW to Off.
