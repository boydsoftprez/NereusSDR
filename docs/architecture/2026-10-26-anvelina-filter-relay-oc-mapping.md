# Anvelina PRO III: band filter, relay and open collector bit mapping, NereusSDR vs Thetis

Date: 2026-10-26. Branch `claude/anvelina-filter-oc-dc22e7`, NereusSDR cites are at `78b191936`
(the head after the fix wave). Plan: [2026-10-26-anvelina-filter-oc-plan.md](2026-10-26-anvelina-filter-oc-plan.md).

## Seven-model idle-input approval, 2026-10-27

JJ approved input 2 following Slice B when no receiver is assigned to input 2 on
Orion MkII, ANAN-7000D, ANAN-8000D, G2, G2 1K, Anvelina Pro 3 and RedPitaya.
An assigned receiver keeps its existing filter rule. This extends the Anvelina
correction in Task 7; it does not approve other mapping changes. The slice VFO
remains the frequency input, as JJ previously approved.

The source extension is prepared against notice-repaired parent `f0fa34849`.
Focused build/test execution and radio/operator verification remain pending.
The historical checks below do not establish execution evidence for this extension.
The receive preselector decision also applies during transmit; no I/Q, WDSP or
audio processing is changed.

References, all read-only:

- Stock Thetis v2.10.3.15 (`3759d096`), the `Thetis v2.10.3.15` column.
- Yuri's fork (EU2AV, eu2av/OpenHPSDR-Thetis-Enhanced, `567a7ec`, the primary reference per JJ, 2026-10-26),
  the `Thetis-Enhanced (EU2AV)` column. `ChannelMaster/*`, `Alex.cs`, `Penny.cs`, `NetworkIO.cs` and
  `NetworkIOImports.cs` are identical to stock, so their lines are the same. `console.cs`, `setup.cs` and
  `setup.designer.cs` differ (PureSignal target level, detector calibration, palettes, DPI, APF defaults,
  phase rotator, nothing in filter, Alex, OC or antenna logic), so each stock line was looked up in Yuri's file
  and is listed under its own number.
- Anvelina gateware `n1gp-Anvelina_PROIII` `8e86a61`, facts only, no logic ported.
- The Yuri spec "Open_Collector_Anvelina_DX for Thetis" (EU2AV, 2026-05-21) for byte 1397.

Path prefixes: `cs/` = `Project Files/Source/Console/`, `cm/` = `Project Files/Source/ChannelMaster/`, `gw/` =
gateware root. NereusSDR files given without a directory: `AlexFilterMap.{h,cpp}`, `P1Codec*.cpp`, `P2Codec*.cpp`,
`CodecContext.h` are in `src/core/codec/`; `RadioModel.cpp` is in `src/models/`; `P1RadioConnection.cpp`,
`P2RadioConnection.cpp`, `RadioConnection.h`, `OcMatrix.cpp` are in `src/core/`.

Board identity in both: Thetis `ANVELINAPRO3` sets `SetMKIIBPF(1)`, `HPSDRHW.OrionMKII` (cs/clsHardwareSpecific.cs:178-183);
NereusSDR `mkiiBpf = true`, `effectiveBoard = OrionMKII` (src/core/HardwareProfile.cpp:187-193). So both take the band-pass
function (`setBPF1ForOrionIISaturn`, cs/console.cs:6827-6837, Yuri :6854-6864; NereusSDR `usesBpf1Preselector`,
src/core/codec/AlexFilterMap.cpp:225-236) and the Mk II relay split in `SetAntBits`.

Selection-word encoding (both): `SetAlexHPFBits` argument 0x01 13 MHz is Alex0 bit 1, 0x02 20 MHz bit 2, 0x04 9.5 MHz
bit 4, 0x08 6.5 MHz bit 5, 0x10 1.5 MHz bit 6, 0x20 bypass bit 12, 0x40 6 m preamp bit 3 (cm/netInterface.c:605-621;
src/core/codec/P2CodecOrionMkII.cpp:575-581). Hex below is the Alex0 receive half (bytes 1434-1435) unless stated.

## Board fact correction: Alex0 bit 10 is the BYPS relay, bit 11 is an LED

The gateware table lists Alex0 bit 10 as "NC" (gw/SPI.v:48) and bit 11 as "RX BYPASS OUT" (gw/SPI.v:49). On the
Anvelina PRO III that is wrong. The board schematic (sheet "Rev-3.6.8A SDR_BPF1.BPF2.PA100Watts", YL2GVC package of
2025-09-26) shows bit 10 energizes BYPS relay K20, which connects jack J5 straight to the ADC1 cable (J4
RX1_IN_ADC1), and bit 11 (net D3_IN) only lights LED D21; bit 14 ("1w") drives no relay. Source:
`docs/architecture/2026-10-25-puresignal-anvelina-rx-bypass-on-tx.md` section 4c, on branch
`claude/puresignal-feedback-40m-55f15e` (not on this branch). Both Thetis (cm/netInterface.c:463-466, :479) and NereusSDR set bits
10 and 11 for "Rx BYPASS on Tx", so that setting works on the board. Rows D38, D39 and F56 carry the corrected
gateware fact; the gateware column elsewhere quotes the table as printed.

## How to read the tables

Verdict and outcome: `Match` needs nothing. `Deliberate deviation` is an existing NereusSDR choice with a cite.
`Mismatch, fixed` names the task and commit. `Mismatch`, flagged, names why and whose call it is. Row numbers are
this document's own (74 rows; the working draft's own count line said 76).

## A. Receive band filter, input 1 (Alex0 bits 0-15, BPF1)

| # | Area / band or range | NereusSDR at 78b191936 (file:line) | Thetis v2.10.3.15 (file:line) | Thetis-Enhanced EU2AV @567a7ec (file:line) | Gateware fact | Verdict and outcome |
| --- | --- | --- | --- | --- | --- | --- |
| A1 | f < 1.5 MHz (no row) | 0x20 → `0x1000` bypass; `selectAlexHpfRow` returns `kBypassBits` regardless of `rowBypass` (src/core/codec/AlexFilterMap.cpp:338-349, :348). A lone slice here keeps 0x20 (AlexFilterMap.h:343-345) | else branch `SetAlexHPFBits(0x20)` → `0x1000` (cs/console.cs:7061-7064) | else branch `SetAlexHPFBits(0x20)` → `0x1000` (cs/console.cs:7088-7091) | bit 12 "HPF BYPASS" (gw/SPI.v:50); bench: passes nothing on JJ's unit | Match |
| A2 | 1.5 to 2.099999 MHz (160 m) | 0x10 → `0x0040` (AlexFilterMap.cpp:256-258, :312) | `BPF1_1_5Start/End` 1.5 / 2.099999 (cs/setup.designer.cs:24982, :25023) → 0x10 (cs/console.cs:6972-6984) | `BPF1_1_5Start/End` 1.5 / 2.099999 (cs/setup.designer.cs:24983, :25024) → 0x10 (cs/console.cs:6999-7011) | bit 6 "1.5 MHz HPF" (gw/SPI.v:44) | Match |
| A3 | 2.1 to 5.499999 MHz (80/60 m) | 0x08 → `0x0020` (AlexFilterMap.cpp:312) | 2.1 / 5.499999 (setup.designer.cs:25064, :25105) → 0x08 (console.cs:6987-6999) | 2.1 / 5.499999 (setup.designer.cs:25065, :25106) → 0x08 (console.cs:7014-7026) | bit 5 "6.5 MHz HPF" (gw/SPI.v:43) | Match |
| A4 | 5.5 to 10.999999 MHz (40/30 m) | 0x04 → `0x0010` (AlexFilterMap.cpp:312) | 5.5 / 10.999999 (setup.designer.cs:25146, :25187) → 0x04 (console.cs:7002-7014) | 5.5 / 10.999999 (setup.designer.cs:25147, :25188) → 0x04 (console.cs:7029-7041) | bit 4 "9.5 MHz HPF" (gw/SPI.v:42) | Match |
| A5 | 11 to 21.999999 MHz (20/17/15 m) | 0x01 → `0x0002` (AlexFilterMap.cpp:313) | 11 / 21.999999 (setup.designer.cs:25440, :25247) → 0x01 (console.cs:7017-7029) | 11 / 21.999999 (setup.designer.cs:25441, :25248) → 0x01 (console.cs:7044-7056) | bit 1 "13 MHz HPF" (gw/SPI.v:39) | Match |
| A6 | 22 to 34.999999 MHz (12/10 m) | 0x02 → `0x0004` (AlexFilterMap.cpp:313) | 22 / 34.999999 (setup.designer.cs:25277, :25217) → 0x02 (console.cs:7032-7044) | 22 / 34.999999 (setup.designer.cs:25278, :25218) → 0x02 (console.cs:7059-7071) | bit 2 "20 MHz HPF" (gw/SPI.v:40) | Match |
| A7 | 35 to 61.44 MHz (6 m) | 0x40 → `0x0008` (AlexFilterMap.cpp:313) | 35 / 61.44 (setup.designer.cs:25481, :25522) → 0x40 unless `bpf1_6bp_bypass`, `disable_6m_lna_on_rx`, or keyed with `disable_6m_lna_on_tx` (console.cs:7046-7059) | 35 / 61.44 (setup.designer.cs:25482, :25523) → 0x40 unless `bpf1_6bp_bypass`, `disable_6m_lna_on_rx`, or keyed with `disable_6m_lna_on_tx` (console.cs:7073-7086) | bit 3 "6M Preamp ... 12M/10M/6M LNA" (gw/SPI.v:41), an amplifier, not a filter | Match |
| A8 | f > 61.44 MHz | 0x20 → `0x1000` (AlexFilterMap.cpp:348) | else → 0x20 (console.cs:7061-7064) | else → 0x20 (console.cs:7088-7091) |  | Match |
| A9 | Edge arithmetic | whole hertz, `>= start && <= end` (AlexFilterMap.cpp:266-269, :341-346); 1 Hz gaps fall into no row only for fractional-Hz input | `freq >= Start && freq <= End` on doubles (console.cs:6972-7046) | `freq >= Start && freq <= End` on doubles (console.cs:6999-7073) |  | Match (sub-Hz only) |
| A10 | Frequency that drives Alex0's selection | each counted slice's VFO, `s->frequency()` (src/models/RadioModel.cpp:26453), lowest on the chain (:26849); before a slice binds, the primary DDC frequency (src/core/P2RadioConnection.cpp:1219-1222) | `setAlex1HPF(_rx1_dds_freq)` (console.cs:15401): the DDS frequency, which with click-tune (CTUN) is the display centre, not the VFO (console.cs:31894-31910) | `setAlex1HPF(_rx1_dds_freq)` (console.cs:15428): the DDS frequency, which with click-tune (CTUN) is the display centre, not the VFO (console.cs:31964-31980) |  | **Mismatch**, kept as a deliberate deviation. JJ ruled 2026-10-26 ("yes keep the nereus way"): the band-pass follows the slice's VFO, not the click-tune display centre. No code change. Receive relay bits only; differs only when VFO and centre fall in different rows (M4). |
| A11 | Row "Bypass" boxes (`bpf1_*bp_bypass`) | row's own bypass sends `rowBypass` = five filters `0x0076` on this radio (AlexFilterMap.cpp:345, :407-412; src/models/RadioModel.cpp:26367-26380) | `SetAlexHPFBits(0x20)` → `0x1000` (console.cs:6974-6978 and siblings) | `SetAlexHPFBits(0x20)` → `0x1000` (console.cs:7001-7005 and siblings) |  | Deliberate deviation (src/core/codec/AlexFilterMap.h:44-49, :321-347) |
| A12 | Several slices on input 1, different rows, Auto policy | rows ORed (e.g. 40 m + 20 m → `0x0012`); bypass and 6 m/12-10 m rules (src/models/RadioModel.cpp:26845-26848; AlexFilterMap.cpp:415-427) | n/a: one receiver per input, `UpdateAlexRXFilter` is a no-op with `_rx2_preamp_present` true (console.cs:14847-14850, :15500-15510) | n/a: one receiver per input, `UpdateAlexRXFilter` is a no-op with `_rx2_preamp_present` true (console.cs:14874-14877, :15527-15537) | bench sweep (memory `reference_anvelina_bypass_dead`) | Deliberate deviation (AlexFilterMap.h:293-319, :349-386; RadioModel.cpp:26231-26239) |
| A13 | Force bypass / wideband (extended) view | `receiveBypass` = `0x0076` (RadioModel.cpp:26817) | WB form sets `AlexHPFBypass = true` → `0x1000` (console.cs:43545-43565) | WB form sets `AlexHPFBypass = true` → `0x1000` (console.cs:43615-43635) |  | Deliberate deviation (RadioModel.cpp:26237-26239) |

## B. Bypass switches and "on Tx" filter options

| # | Area / band or range | NereusSDR at 78b191936 (file:line) | Thetis v2.10.3.15 (file:line) | Thetis-Enhanced EU2AV @567a7ec (file:line) | Gateware fact | Verdict and outcome |
| --- | --- | --- | --- | --- | --- | --- |
| B14 | "HPF/BPF Bypass" (`alex_hpf_bypass`), keyed or not | `switches.receiveBypass` = `0x0076` (AlexFilterMap.cpp:743-745; RadioConnection.h:660) | `SetAlexHPFBits(0x20)` → `0x1000` (console.cs:6965-6970) | `SetAlexHPFBits(0x20)` → `0x1000` (console.cs:6992-6997) |  | Deliberate deviation (AlexFilterMap.h:645-647) |
| B15 | "BPF ByPass on TX" (`disable_hpf_on_tx`), keyed | `0x1000` (AlexFilterMap.cpp:731-734); default false (src/core/RadioConnection.h:1606) | `if (_mox && (disable_hpf_on_tx || ...)) SetAlexHPFBits(0x20)` (console.cs:6957-6963); default unchecked (setup.designer.cs:23782 no Checked) | `if (_mox && (disable_hpf_on_tx || ...)) SetAlexHPFBits(0x20)` (console.cs:6984-6990); default unchecked (setup.designer.cs:23783 no Checked) |  | Match |
| B16 | "BPF ByPass on PS" (`disable_hpf_on_ps`), keyed with PureSignal on | `0x1000` (AlexFilterMap.cpp:731-734, band-pass boards only); default true (RadioConnection.h:1602) | same line (console.cs:6957); `chkDisableHPFonPSb.Checked = true` (setup.designer.cs:23724) | same line (console.cs:6984); `chkDisableHPFonPSb.Checked = true` (setup.designer.cs:23725) |  | Match |
| B17 | PureSignal condition | `m_puresignalRun` (P2RadioConnection.cpp:4511) | `PureSignalEnabled` = PS button (console.cs:43808-43812), same event that calls `SetPureSignal` (cs/PSForm.cs:234-257) | `PureSignalEnabled` = PS button (console.cs:43878-43882), same event that calls `SetPureSignal` (cs/PSForm.cs:234-257) |  | Match |
| B18 | "Disable 6m LNA on RX", single 6 m slice | `0x40` → `0x20` → `0x1000` (AlexFilterMap.cpp:757-765); default false (RadioConnection.h:1640) | console.cs:7050; default false (console.cs:18719-18720) | console.cs:7077; default false (console.cs:18746-18747) |  | Match |
| B19 | "Disable 6m LNA on RX", 6 m slice sharing input with HF | LNA dropped, other rows kept (AlexFilterMap.cpp:763-764) | n/a (no shared input) | n/a (no shared input) |  | Deliberate deviation (AlexFilterMap.h:641-644) |
| B20 | "Disable 6m LNA on TX", keyed on 6 m | `0x1000` (AlexFilterMap.cpp:758-765); default true (RadioConnection.h:1641) | console.cs:7050; default true (console.cs:18741; setup.designer.cs:23751) | console.cs:7077; default true (console.cs:18768; setup.designer.cs:23752) |  | Match |
| B21 | "6m/ByPass on RX" (`lpf_bypass`) | not offered on this model (AlexFilterMap.cpp:368-381) | hidden and unchecked for ANVELINAPRO3 (cs/setup.cs:6190-6205) | hidden and unchecked for ANVELINAPRO3 (cs/setup.cs:6316-6331) |  | Match |
| B22 | Alex-2 master bypass (`alex2_hpf_bypass`) on input 2 | `rowBypass` = `0x0076` in Alex1 (AlexFilterMap.cpp:358-365) | `SetAlex2HPFBits(0x20)` → Alex1 bit 12 (console.cs:7073-7077) | `SetAlex2HPFBits(0x20)` → Alex1 bit 12 (console.cs:7100-7104) | "HPF BYPASS_2" (gw/SPI.v:68) | Deliberate deviation (AlexFilterMap.h:44-49) |

## C. Transmit low-pass (Alex0 / Alex1 bits 20-23, 29-31)

| # | Area / band or range | NereusSDR at 78b191936 (file:line) | Thetis v2.10.3.15 (file:line) | Thetis-Enhanced EU2AV @567a7ec (file:line) | Gateware fact | Verdict and outcome |
| --- | --- | --- | --- | --- | --- | --- |
| C23 | 0 to 2.5 MHz | 0x08 → bit 23 `0x00800000` (AlexFilterMap.cpp:460, :484) | `udAlex160mLPF` 0 / 2.5 (setup.designer.cs:24881, :24851) → 0x08 (console.cs:7209-7214) | `udAlex160mLPF` 0 / 2.5 (setup.designer.cs:24882, :24852) → 0x08 (console.cs:7236-7241) | bit 39 "BPF0 160M" (gw/SPI.v:81) | Match |
| C24 | 2.500001 to 5 MHz | 0x04 → bit 22 `0x00400000` | 2.500001 / 5 (:24821, :24791) → 0x04 (console.cs:7202-7207) | 2.500001 / 5 (:24822, :24792) → 0x04 (console.cs:7229-7234) | bit 38 "BPF1 80M" (gw/SPI.v:80) | Match |
| C25 | 5.000001 to 8 MHz | 0x02 → bit 21 `0x00200000` | 5.000001 / 8 (:24761, :24731) → 0x02 (console.cs:7195-7200) | 5.000001 / 8 (:24762, :24732) → 0x02 (console.cs:7222-7227) | bit 37 "60/40M" (gw/SPI.v:79) | Match |
| C26 | 8.000001 to 16.5 MHz | 0x01 → bit 20 `0x00100000` | 8.000001 / 16.5 (:24611, :24671) → 0x01 (console.cs:7188-7193) | 8.000001 / 16.5 (:24612, :24672) → 0x01 (console.cs:7215-7220) | bit 36 "30/20M" (gw/SPI.v:78) | Match |
| C27 | 16.500001 to 24 MHz | 0x40 → bit 31 `0x80000000` | 16.500001 / 24 (:24641, :24701) → 0x40 (console.cs:7230-7235) | 16.500001 / 24 (:24642, :24702) → 0x40 (console.cs:7257-7262) | bit 47 "17/15M" (gw/SPI.v:89) | Match |
| C28 | 24.000001 to 35.6 MHz | 0x20 → bit 30 `0x40000000` | 24.000001 / 35.6 (:24522, :24492) → 0x20 (console.cs:7223-7228) | 24.000001 / 35.6 (:24523, :24493) → 0x20 (console.cs:7250-7255) | bit 46 "12/10M" (gw/SPI.v:88) | Match |
| C29 | 35.600001 to 61.44 MHz, and no row | 0x10 → bit 29 `0x20000000` (AlexFilterMap.cpp:466, :502) | → 0x10 (console.cs:7216-7221, else 7237-7240) | → 0x10 (console.cs:7243-7248, else 7264-7267) | bit 45 is "BYPASS(BPF Bypass)" on this board (gw/SPI.v:87): 6 m and above go out unfiltered in both | Match |
| C30 | Test order | 20, 40, 80, 160, 6, 10/12, 15/17 (AlexFilterMap.cpp:480-488) | same (console.cs:7188-7235) | same (console.cs:7215-7262) |  | Match |
| C31 | Alex0 LPF unkeyed | receive selection (P2RadioConnection.cpp:1865), one slice = its own | `setAlexLPF(rx1_dds_freq_mhz, false)` (console.cs:15487-15498) | `setAlexLPF(rx1_dds_freq_mhz, false)` (console.cs:15514-15525) | used on receive (gw/Orion.v:2348) | Match (one slice; several slices: NereusSDR-original highest-slice rule, n/a) |
| C32 | Alex0 LPF keyed | `lpfBitsTx` (P2RadioConnection.cpp:1865) | `SetAlexLPFBits(bits, true, isMox)` writes Alex0 when `isMox` (cm/netInterface.c:706-719) | `SetAlexLPFBits(bits, true, isMox)` writes Alex0 when `isMox` (cm/netInterface.c:706-719) |  | Match |
| C33 | Alex1 LPF keyed | `alexLpfBitsTx` (P2CodecOrionMkII.cpp:676-682) | Alex1 written on `isMox || isTX` (cm/netInterface.c:686-704) and Alex0's upper half copied in at key-down (cm/netInterface.c:489-490) | Alex1 written on `isMox || isTX` (cm/netInterface.c:686-704) and Alex0's upper half copied in at key-down (cm/netInterface.c:489-490) | Alex1 upper used only keyed with one TX ANT bit (gw/Orion.v:2346-2348) | Match |
| C34 | Alex1 LPF unkeyed | always the TX selection (P2CodecOrionMkII.cpp:676-682) | `SetAntBits` copies Alex0's upper half (RX low-pass) into Alex1 on every antenna update (cm/netInterface.c:489-490); `SetAlexLPFBits` only rewrites Alex1 when `Alex1LPFMask` changes (:689) | `SetAntBits` copies Alex0's upper half (RX low-pass) into Alex1 on every antenna update (cm/netInterface.c:489-490); `SetAlexLPFBits` only rewrites Alex1 when `Alex1LPFMask` changes (:689) | inert unkeyed (gw/Orion.v:2348) | **Mismatch** (inert; M3) Outcome: no change. Inert on this radio, as the draft found (Alex1's upper half is used only while keyed with one TX ANT bit, gw/Orion.v:2346-2348, and keyed both send the TX row). Flagged for completeness; JJ's call if he wants the Thetis copy ported. |

## D. Alex0 / Alex1 full layout, Protocol 2 (high-priority bytes 1428-1435)

| # | Area / band or range | NereusSDR at 78b191936 (file:line) | Thetis v2.10.3.15 (file:line) | Thetis-Enhanced EU2AV @567a7ec (file:line) | Gateware fact | Verdict and outcome |
| --- | --- | --- | --- | --- | --- | --- |
| D35 | Byte placement | Alex1 BE32 at 1428, Alex0 at 1432 (P2CodecOrionMkII.cpp:344-345) | cm/network.c:1041-1050 | cm/network.c:1041-1050 | 1428-1429 → `Alex_Tx_data`, 1430-1431 → Rx1 filters, 1432-1435 → Alex0 (gw/High_Priority_CC.v:257-264) | Match |
| D36 | CmdGeneral byte 59 (Alex enable) | `0x03` (P2CodecOrionMkII.cpp:241) | `prbpfilter->enable | prbpfilter2->enable` = 1 / 2 (cm/network.c:906; netInterface.c:1551-1555) | `prbpfilter->enable | prbpfilter2->enable` = 1 / 2 (cm/network.c:906; netInterface.c:1551-1555) | SPI enable = `Alex_enable[0]` (gw/Orion.v:2530) | Match |
| D37 | Alex0 bits 24-26 ANT1-3 | from `trxAnt`, 4 → none (P2CodecOrionMkII.cpp:441-447; P2RadioConnection.cpp:1756-1757) | cm/netInterface.c:483-485 (`trx_ant` 4 → no bit) | cm/netInterface.c:483-485 (`trx_ant` 4 → no bit) | SPI 40-42 (gw/SPI.v:82-84) | Match |
| D38 | Alex0 bits 8-10 RX-only mux | P2CodecOrionMkII.cpp:488-495 | cm/netInterface.c:479-481 | cm/netInterface.c:479-481 | bit 8 XVTR, bit 9 EXT1 (gw/SPI.v:46-47). Board correction: the gateware table lists bit 10 as "NC" (gw/SPI.v:48), but on the Anvelina the board schematic shows Alex0 bit 10 energizes BYPS relay K20 (jack J5 straight to the ADC1 cable, J4 RX1_IN_ADC1). Source: docs/architecture/2026-10-25-puresignal-anvelina-rx-bypass-on-tx.md section 4c (branch claude/puresignal-feedback-40m-55f15e), from the "Rev-3.6.8A SDR_BPF1.BPF2.PA100Watts" sheet (YL2GVC package, 2025-09-26). | Match. NereusSDR and Thetis both write bit 10 (and 9, 8) the same way; the SPI.v "NC" label is the part that is wrong for this board. |
| D39 | Alex0 bit 11 (RX BYPASS OUT RL17) / bit 14 (RX MASTER IN SEL RL22) | Mk II split: BYPS or keyed → bit 11 = rxOut, else bit 14 = rxOut (P2CodecOrionMkII.cpp:535-545) | cm/netInterface.c:461-472 | cm/netInterface.c:461-472 | gw/SPI.v:49 lists bit 11 as "RX BYPASS OUT" and :52 bit 14 as "RX MASTER IN SELECT"; the board schematic differs: bit 11 (IC7 DRAIN3, net D3_IN) lights LED D21 only, bit 14 ("1w") drives no relay. The BYPS relay K20 is bit 10 (row D38). Source: docs/architecture/2026-10-25-puresignal-anvelina-rx-bypass-on-tx.md section 4c (branch claude/puresignal-feedback-40m-55f15e). | Match. Both sides set bits 10 and 11 for "Rx BYPASS on Tx", so that setting works on the board; the gateware table (SPI.v:48 "NC", :49 "RX BYPASS OUT") is wrong for this board. |
| D40 | Alex0 bits 13-14 attenuator | suppressed on Mk II (P2CodecOrionMkII.cpp:558-561) | `SetAlexAtten` returns on `mkiibpf` (cm/netInterface.c:421-423) | `SetAlexAtten` returns on `mkiibpf` (cm/netInterface.c:421-423) | bit 13 NC (gw/SPI.v:51) | Match |
| D41 | Alex0 bits 27 T/R, 18 trx_status | keyed and PA not disabled (P2CodecOrionMkII.cpp:483-486) | `SetTRXrelay` (cm/netInterface.c:374-383; console.cs:29121) | `SetTRXrelay` (cm/netInterface.c:374-383; console.cs:29177) | bit 43 T/R (gw/SPI.v:85), forced from FPGA_PTT (gw/Orion.v:2350); bit 34 "N.C. (100W)" (gw/SPI.v:76) | Match |
| D42 | Alex0 bits 0, 7, 15, 19, 28 (LEDs/NC) | never set | never set by any setter | never set by any setter |  | Match |
| D43 | Alex1 bit 18 | keyed and PA not disabled (P2CodecOrionMkII.cpp:616-621) | `prbpfilter2->_trx_status = prbpfilter->_TR_Relay` (cm/netInterface.c:381) | `prbpfilter2->_trx_status = prbpfilter->_TR_Relay` (cm/netInterface.c:381) | `Alex_Tx_data[2]` → SPI 34 NC | Match |
| D44 | Alex1 bit 27 | never | only through the Alex0 copy (cm/netInterface.c:489-490, :692-693) when it runs while keyed; not at key-down (relay set after the copy, console.cs:29110-29121); G2E pcap keyed Alex1 `0x01440100` (P2CodecOrionMkII.cpp:576) | only through the Alex0 copy (cm/netInterface.c:489-490, :692-693) when it runs while keyed; not at key-down (relay set after the copy, console.cs:29166-29177); G2E pcap keyed Alex1 `0x01440100` (P2CodecOrionMkII.cpp:576) | OR'd with FPGA_PTT (gw/Orion.v:2350): no effect | Match (steady state; transient copies inert) |
| D45 | Alex1 bits 24-26 TX ANT | P2CodecOrionMkII.cpp:650-657 | cm/netInterface.c:493-495 | cm/netInterface.c:493-495 | must be exactly one bit for Alex1 upper to be used keyed (gw/Orion.v:2346) | Match |
| D46 | Alex1 bit 8 `_rx2_gnd` keyed | `reg |= (1u << 8)` only when `ctx.bpf2GndOnTx` (src/core/codec/P2CodecOrionMkII.cpp:623-646, set at :644-645; field `CodecContext::bpf2GndOnTx{true}` src/core/codec/CodecContext.h:348). Setting `alex/master/bpf2GndOnTx`, default `"True"` (src/core/AlexSettingsKeys.h:105-112; applied at src/models/RadioModel.cpp:34110-34114). Taken at the TX edge (src/core/P2RadioConnection.cpp:1672, :4409). Box "Gnd BPF2 on Tx", ticked at construction (src/gui/setup/hardware/AntennaAlexAntennaControlTab.cpp:760-769). Remote: transmit hardware key, radioHardwareVersion 14 (src/core/session/StationServer.cpp:2334, :12435) | `if (bpf2_gnd) SetBPF2Gnd(1)` keyed, cleared on RX (cs/console.cs:29091, :29170); field starts `true` (cs/console.cs:10903). User box `chkBPF2Gnd` "Gnd BPF2 on Tx", shown on this model (cs/setup.cs:6208). `defaultAlexSettings()` (cs/setup.cs:403) ticks it: the control list ends with `chkBPF2Gnd` (cs/setup.cs:33171-33187, at :33177); startup pass pushes it (cs/setup.cs:2867, :18214-18218). The designer gives the box no `Checked` line (cs/setup.designer.cs:5736-5745); the default comes from `defaultAlexSettings()` | same code: cs/console.cs:29147, :29226, field :10930; cs/setup.cs:6334; `defaultAlexSettings()` call cs/setup.cs:432, list cs/setup.cs:33736-33752 (`chkBPF2Gnd` at :33742); startup pass cs/setup.cs:2926, handler :18373-18377; designer cs/setup.designer.cs:5737-5746 | bit 24 "RX2_GROUND (100W PA)" (gw/SPI.v:64) | **Mismatch, fixed.** Task 5 d3c2d2db8 added the box and made bit 8 follow it; the first version defaulted unticked from a misread designer file, so 31d48af45 made it default ticked, as Thetis `defaultAlexSettings()` does (a fresh Thetis install sets the bit on every key, hidden box included). Wire at the default is unchanged from before the branch. P1 bank 16 `_rx2_gnd` (row H73) is still not sent: flagged for JJ. No phone Setup row for the box: flagged for JJ (remote parity). |
| D47 | Alex1 bits 0-6, 12 with a slice on input 2 | BPF2 rows (same defaults as BPF1) via `computeAlex2Hpf` (AlexFilterMap.cpp:314, :358-365; RadioModel.cpp:26373-26376) | `setAlex2HPF(rx2_dds_freq_mhz)` (console.cs:15435-15444, :7069-7175); BPF2 defaults equal BPF1 (setup.designer.cs:26483-26891) | `setAlex2HPF(rx2_dds_freq_mhz)` (console.cs:15462-15471, :7096-7202); BPF2 defaults equal BPF1 (setup.designer.cs:26484-26892) | Rx1 filters SPI 16-31 (gw/SPI.v:56-71) | Match |
| D48 | Alex1 bits 0-6, 12 with no slice on input 2 | On every Alex-2 board (Orion MkII, 7000D, 8000D, G2, G2 1K, Anvelina Pro 3, RedPitaya; `codec::alex::usesAlex2Hpf`): Alex1's receive half carries Slice B's input-2 row when no slice is on input 2 (src/models/RadioModel.cpp:26809-26815, `input2SliceBHz`; the chain decision at :26829-26831, `preselectorFor(1, input2SliceBHz)`). Otherwise the mirror of Alex0's selection without bypass (src/core/codec/P2CodecOrionMkII.cpp:723-737); keyed with a bypass switch, Alex0's pre-switch selection (src/core/P2RadioConnection.cpp:4508-4518) | always `setAlex2HPF(rx2_dds_freq_mhz)`, the VFO B / RX2 DDS frequency, whether or not RX2 is on (console.cs:15424-15444; RX2DDSFreq set from VFO B at console.cs:32906-32921) | always `setAlex2HPF(rx2_dds_freq_mhz)`, the VFO B / RX2 DDS frequency, whether or not RX2 is on (console.cs:15451-15471; RX2DDSFreq set from VFO B at console.cs:32976-32991) |  | **Source correction prepared; verification pending.** Task 7 (`0c11d94f3`) covered Anvelina; JJ approved the other six on 2026-10-27: on every board in Thetis's `setAlex2HPF` list, idle input 2 follows Slice B (Thetis RX2 / VFO B), keyed or not. An assigned receiver keeps its existing filter rule. Boards outside the list keep the mirror, as Thetis never writes prbpfilter2 from VFO B for them. Setup's input-2 display does not name Slice B: flagged for JJ. |

## E. Antenna and RX-out routing (Alex.cs port)

| # | Area / band or range | NereusSDR at 78b191936 (file:line) | Thetis v2.10.3.15 (file:line) | Thetis-Enhanced EU2AV @567a7ec (file:line) | Gateware fact | Verdict and outcome |
| --- | --- | --- | --- | --- | --- | --- |
| E49 | Receive: `rx_only_ant`, XVTR mapping, `rx_out` | src/models/RadioModel.cpp:26103-26132 | cs/HPSDR/Alex.cs:348-366 | cs/HPSDR/Alex.cs:348-366 | Thetis labels BYPS / EXT1 / XVTR on this model (setup.cs:20406-20409) | Match (kept-band divergence D61 aside) |
| E50 | Keyed: Ext2OutOnTx → 1, Ext1OutOnTx → 2, `rx_out` | RadioModel.cpp:26090-26099 | Alex.cs:339-347; "Rx BYPASS on Tx" / "Ext 1 on Tx" labels (setup.cs:20412-20413) | Alex.cs:339-347; "Rx BYPASS on Tx" / "Ext 1 on Tx" labels (setup.cs:20571-20572) |  | Match |
| E51 | `rx_out_override` logic | RadioModel.cpp:26135-26146 | Alex.cs:368-375 | Alex.cs:368-375 |  | Match |
| E52 | `rx_out_override` control on this model | hidden for the Anvelina: `hasRxBypassUi = false` (src/core/SkuUiProfile.cpp:154, ANAN7000D / ANVELINAPRO3 case), applied to the box at src/gui/setup/hardware/AntennaAlexAntennaControlTab.cpp:988 | `chkDisableRXOut.Visible = false` on Mk II models incl. ANVELINAPRO3 (setup.cs:6190-6207) | `chkDisableRXOut.Visible = false` on Mk II models incl. ANVELINAPRO3 (setup.cs:6316-6333) |  | Match. The draft listed this as a mismatch (M12) from an old reading of AntennaAlexAntennaControlTab.cpp:741-767; the plan's Task 2 found it already matches and was dropped (no commit). |
| E53 | Aries `LimitTXRXAntenna` clamp | not ported (RadioModel.cpp:26148-26152) | Alex.cs:381-382 | Alex.cs:381-382 |  | Mismatch, external Aries only (M11) Outcome: flagged, not built. Applies only with an external Aries (M11); NereusSDR's own note defers it. JJ's call. |
| E54 | `alex_enabled` false → `SetAntBits(0,0,0,0,false)` | gated on `caps.hasAlex` (RadioModel.cpp:26073-26083) | Alex.cs:312-316 on `alex_ant_ctrl_enabled` | Alex.cs:312-316 on `alex_ant_ctrl_enabled` |  | Not settled (U2) Outcome: flagged, not settled (U2). Whether the control tab clears routing the same way when Alex antenna control is off was not traced; needs a maintainer decision and a trace. |
| E55 | Byte 1400 bit 0 (XVTR enable) | always 0 (P2CodecOrionMkII.cpp:331) | `xvtr_enable` from "Enable XVTR on HF" (shown on this model, setup.cs:6209) or a transverter with T/R relay (console.cs:31602-31636; cm/network.c:1028) | `xvtr_enable` from "Enable XVTR on HF" (shown on this model, setup.cs:6335) or a transverter with T/R relay (console.cs:31672-31706; cm/network.c:1028) | `XVTR_ENABLE = DLE_outputs[0]` (gw/Orion.v:2329) | Mismatch, defaults equal (M9) Outcome: flagged, not built (M9). Equal at the defaults (no transverter, box off); a transverter with a T/R relay would differ. JJ's call. |

## F. PureSignal keyed relay bits (receive half, 40 m, PS on, "BPF ByPass on PS" on)

| # | Area / band or range | NereusSDR at 78b191936 (file:line) | Thetis v2.10.3.15 (file:line) | Thetis-Enhanced EU2AV @567a7ec (file:line) | Gateware fact | Verdict and outcome |
| --- | --- | --- | --- | --- | --- | --- |
| F56 | "Rx BYPASS on Tx" | bits 10, 11, 12 → `0x1C00` (RadioModel.cpp:26092; P2CodecOrionMkII.cpp:489-540; AlexFilterMap.cpp:731-734) | Alex.cs:341-345; netInterface.c:463-466, :479; console.cs:6957-6959 → `0x1C00` | Alex.cs:341-345; netInterface.c:463-466, :479; console.cs:6984-6986 → `0x1C00` | SPI.v:48 "NC" and :49 "RX BYPASS OUT" are wrong for this board. On the Anvelina bit 10 drives BYPS relay K20 (jack J5 to the ADC1 cable) and bit 11 only lights LED D21, so bit 10 alone selects the BYPS jack. Source: docs/architecture/2026-10-25-puresignal-anvelina-rx-bypass-on-tx.md section 4c (branch claude/puresignal-feedback-40m-55f15e). | Match. Both set bits 10, 11 and 12, so the setting works on this board. |
| F57 | "Ext 1 on Tx" | bits 9, 11, 12 → `0x1A00` | same path → `0x1A00` | same path → `0x1A00` |  | Match |
| F58 | "RX out on Tx" | bits 11, 12 → `0x1800` | → `0x1800` | → `0x1800` |  | Match |
| F59 | none | bit 12 → `0x1000` | → `0x1000` | → `0x1000` |  | Match |
| F60 | "BPF ByPass on PS" off | band selection (one slice) | band row (console.cs:6972+) | band row (console.cs:6999+) |  | Match |
| F61 | DDC0/DDC1 at TX frequency keyed with PS | P2CodecOrionMkII.cpp:275-289 | cm/network.c:936-945 | cm/network.c:936-945 |  | Match |
| F62 | Bits 12/14 on Alex1 keyed with PS | bypass never mirrored to Alex1 (P2CodecOrionMkII.cpp:684-737) | `SetAlexHPFBits` writes Alex0 only (netInterface.c:605-621) | `SetAlexHPFBits` writes Alex0 only (netInterface.c:605-621) |  | Match (Alex1 low half itself: see M2) |

## G. Open-collector outputs

| # | Area / band or range | NereusSDR at 78b191936 (file:line) | Thetis v2.10.3.15 (file:line) | Thetis-Enhanced EU2AV @567a7ec (file:line) | Gateware fact | Verdict and outcome |
| --- | --- | --- | --- | --- | --- | --- |
| G63 | P2 byte 1401 | `(ocByte << 1) & 0xfe` (P2CodecOrionMkII.cpp:304) | `(prn->oc_output << 1) & 0xfe` (cm/network.c:1031) | `(prn->oc_output << 1) & 0xfe` (cm/network.c:1031) | USEROUT0-6 = `Open_Collector[1..7]` while running (gw/Orion.v:913-919) | Match |
| G64 | OC byte source, default pin actions | `OcMatrix::maskFor(band, mox)` on the TX slice's band keyed, RX1 stand-in's band unkeyed (P2RadioConnection.cpp:2150-2163) | `Penny.UpdateExtCtrl`: `RXABitMasks[idx]` / `TXABitMasks[idx or idxb]` (cs/HPSDR/Penny.cs:156-198) | `Penny.UpdateExtCtrl`: `RXABitMasks[idx]` / `TXABitMasks[idx or idxb]` (cs/HPSDR/Penny.cs:156-198) |  | Match (defaults) |
| G65 | OC TX pin actions (MOX/TUNE/2-tone), "consider PA", split pins | applied: `OcMatrix::maskFor(band, tx, tune, twoTone)` runs the pin-action step after the raw bits (src/core/OcMatrix.cpp:147-176; `adjustForRx` :196, `adjustForTxAction` :226; `pinAction()` :280). TUNE and two-tone flags reach the connection through `setOcTxModes` (src/core/RadioConnection.h:733-739; src/models/RadioModel.cpp:23707-23716, `wireOcTxModes` :23718 with the unkey-order hold `m_ocTuneHold`); the byte is composed at src/core/P2RadioConnection.cpp:2150-2161 | `adjustForTXAction` / `adjustForRX` / `SplitPins` (Penny.cs:162-186, :262-358) | `adjustForTXAction` / `adjustForRX` / `SplitPins` (Penny.cs:162-186, :262-358) |  | **Mismatch, fixed** (Task 1, efbea9e93; 37618b4e1 made TUNE's OC flag follow RadioModel's unkey order, as Thetis clears `_tuning` after MOX off). Consider-PA arrays are reduced to their false default and SplitPins / VFOBTX are not ported (one band in). A pin-action edit made while keyed applies at once; Thetis waits for the next MOX or frequency edge (PR note). A frame or two can carry TUNE's pins after a re-key during the unkey walk. |
| G66 | P2 byte 1397 (Open Collector DX) | sent for the Anvelina only: `buf[1397] = ctx.ocExtrasByte & kDxWireMask` (src/core/codec/P2CodecOrionMkII.cpp:314-320; `kDxWireMask = 0x1E` src/core/codec/CodecContext.h:35-53; `ocExtrasByte` :480). Model and setter: src/core/AnvelinaDxOutputs.cpp:34 (`setOutputOn`), :82 (`wireByte`); `RadioConnection::setOcExtrasByte` src/core/RadioConnection.h:750, P2 resend on change. UI: Setup > OC Outputs > "Anvelina DX Outputs" (src/gui/setup/hardware/OcOutputsTab.cpp:398). Remote: "ocDx" reload, radioHardwareVersion 14 (src/models/RadioModel.cpp:33818-33822; gate src/gui/setup/HardwarePage.cpp:492-494) | `oc_output_extras & 0x0f` on ANVELINAPRO3 (cm/network.c:1016-1020); nothing in Console calls `SetOCExtraBits` (netInterface.c:410-417), so `0x00` | `oc_output_extras & 0x0f` on ANVELINAPRO3 (cm/network.c:1016-1020); nothing in Console calls `SetOCExtraBits` (netInterface.c:410-417), so `0x00` | 1397 → `Open_Collector_Anvelina_DX[4:0]` (gw/High_Priority_CC.v:251); USEROUT7-10 (gw/Orion.v:920-923) | **Mismatch, fixed** (the DX Outputs feature, af7db05b9 model and wire, 0f5c6649e Setup group, 0abc168ba remote gate at radioHardwareVersion 14). Deliberate deviation from both Thetis trees: mask `0x1E` per the Yuri spec page 2 and Orion.v:923 [@8e86a61], not `0x0f`, so DX OUT 10 (bit 4) reaches the radio (JJ ruled "yes" 2026-10-26). Thetis and Thetis-Enhanced have the plumbing and no UI, and nothing calls `SetOCExtraBits`. |

## H. Protocol 1 (only if an Anvelina P1 firmware exists; see U1)

| # | Area / band or range | NereusSDR at 78b191936 (file:line) | Thetis v2.10.3.15 (file:line) | Thetis-Enhanced EU2AV @567a7ec (file:line) | Gateware fact | Verdict and outcome |
| --- | --- | --- | --- | --- | --- | --- |
| H67 | Bank rotation 0..17 | `maxBank() 17` (src/core/codec/P1CodecAnvelinaPro3.h:65; P1RadioConnection.cpp:4154, :5628) | `end_frame = ... ANVELINAPRO3 ? 17 : 16` (cm/networkproto1.c:682) | `end_frame = ... ANVELINAPRO3 ? 17 : 16` (cm/networkproto1.c:682) | pinned gateware is P2 only (gw/README.org:4) | Match |
| H68 | Bank 0 C2 OC | `(ocByte << 1) & 0xFE` (P1CodecStandard.cpp:414) | networkproto1.c:452 | networkproto1.c:452 |  | Match |
| H69 | Bank 0 C3 bits 5-6 RX-only mux | P1CodecStandard.cpp:425-430 | networkproto1.c:456-461 | networkproto1.c:456-461 |  | Match |
| H70 | Bank 0 C3 bit 7 / bit 0 on a Mk II board | bit 7 = `rxOut` always, bit 0 = attenuator (P1CodecStandard.cpp:431-436) | bit 7 = `_Rx_1_Out`, bit 0 = `_10_dB_Atten` after the Mk II split (networkproto1.c:453-455; netInterface.c:461-472) | bit 7 = `_Rx_1_Out`, bit 0 = `_10_dB_Atten` after the Mk II split (networkproto1.c:453-455; netInterface.c:461-472) |  | **Mismatch** (M6) Outcome: flagged, not built (M6). Applies only if an Anvelina Protocol 1 image exists (U1); needs `ctx.mkiiBpf` filled on P1. |
| H71 | Bank 10 C3 HPF, C4 LPF | same encoding (P1CodecStandard.cpp:463-464) | networkproto1.c:583-590 | networkproto1.c:583-590 |  | Match (filter bits) |
| H72 | Bank 10 C3 bit 7 | `!trxRelay || txPaDisabled`, so 1 on receive (P1CodecStandard.cpp:462-463) | `prn->tx[0].pa` only (networkproto1.c:586) | `prn->tx[0].pa` only (networkproto1.c:586) |  | **Mismatch**, every P1 Alex board (M7) Outcome: flagged, not built (M7). Generic to every Protocol 1 Alex board; firmware effect on receive is likely nil. JJ's call. |
| H73 | Bank 16 C1 BPF2 + rx2_gnd, C2 bit 0 xvtr | C1 = 0, C2 = ps_run only (P1CodecStandard.cpp:384-390) | networkproto1.c:657-666 | networkproto1.c:657-666 |  | **Mismatch**, flagged, not built (M8; P1 applicability U1). `_rx2_gnd` (bank 16 C1 bit 7) is not sent even though the "Gnd BPF2 on Tx" box now exists: JJ's call. |
| H74 | Bank 17 C1 | `ctx.ocExtrasByte & kDxWireMask`, the DX byte (src/core/codec/P1CodecAnvelinaPro3.cpp:50-71) | `prn->oc_output_extras & 0x0f`, always 0 (networkproto1.c:668-674) | `prn->oc_output_extras & 0x0f`, always 0 (networkproto1.c:668-674) |  | **Mismatch, fixed** (Task 3, af7db05b9): bank 17 C1 now carries the DX outputs' own byte, not the band OC byte. Mask `0x1E` like P2; the bank 17 layout follows the P2 spec by analogy, unverified on a radio (no P1 Anvelina gateware is known, U1). |

## Mismatches and what became of them

| ID | Mismatch | Outcome |
| --- | --- | --- |
| M1 | Alex1 bit 8 (`_rx2_gnd`, "Gnd BPF2 on Tx") always set while keyed | **Fixed.** d3c2d2db8 (box and setting, bit 8 follows it) and 31d48af45 (default ticked, as Thetis `defaultAlexSettings()` ticks it: cs/setup.cs:403 and :33171-33187, Yuri :432 and :33736-33752). The draft's reading that the box starts unticked came from the designer file, which has no `Checked` line; the startup pass pushes the ticked default. Row D46. |
| M2 | Input 2's band-pass with no slice on input 2 | **Source correction prepared; verification pending** on every board Thetis's `setAlex2HPF` list covers (Orion MkII, 7000D, 8000D, G2, G2 1K, Anvelina Pro 3, RedPitaya; console.cs:15435-15444 [v2.10.3.15], EU2AV console.cs:15462-15471 [@567a7ec]): it follows Slice B (Thetis RX2 / VFO B), unkeyed and keyed. Anvelina in 0c11d94f3 (Task 7), the other six in the JJ-approved 2026-10-27 follow-on, with execution and operator checks pending. Boards outside the list keep the mirror. Setup's input-2 display (AlexController chain 1) does not name Slice B: **flagged for JJ**. Row D48. |
| M3 | Alex1 low-pass while not keyed | **No change.** Inert (gw/Orion.v:2346-2348). Row C34. |
| M4 | Which frequency selects Alex0's band-pass under click-tune | **Kept**, JJ ruled "yes keep the nereus way" (2026-10-26): the slice's VFO, not the display centre. Row A10. |
| M5 | P1 bank 17 sent the band OC pins | **Fixed**, af7db05b9: bank 17 C1 is the DX byte, masked `0x1E`. Row H74. |
| M6 | P1 bank 0 C3 lacks the Mk II relay split | **Flagged, not built.** Only matters if a Protocol 1 Anvelina image exists (U1). Row H70. |
| M7 | P1 bank 10 C3 bit 7 set on receive | **Flagged, not built.** Generic to every P1 Alex board. Row H72. |
| M8 | P1 bank 16 C1 (BPF2 + `_rx2_gnd`) and C2 bit 0 are 0 | **Flagged, not built**, JJ's call; `_rx2_gnd` is the one that now has a Setup box. Row H73. |
| M9 | XVTR enable (P2 byte 1400 bit 0) never sent | **Flagged, not built.** Equal at the defaults. Row E55. |
| M10 | OC TX pin actions, consider-PA and split pins not applied | **Fixed**, efbea9e93 (actions applied on the wire) and 37618b4e1 (TUNE's flag follows RadioModel's unkey order). Consider-PA is reduced to its false default; SplitPins / VFOBTX are not ported. Row G65. |
| M11 | Aries antenna clamp not ported | **Flagged, not built.** External Aries only. Row E53. |
| M12 | "Disable BYPASS" offered on this model | **Not a mismatch.** `hasRxBypassUi` is false for the Anvelina (SkuUiProfile.cpp:154, AntennaAlexAntennaControlTab.cpp:988); the plan's Task 2 was dropped with no commit. Row E52. |
| New | Byte 1397 (DX outputs) never sent | **Fixed**, af7db05b9 (model and wire), 0f5c6649e (Setup group), 0abc168ba (remote gate at radioHardwareVersion 14). Mask `0x1E` (JJ ruled "yes", 2026-10-26) versus Thetis `0x0f`. Row G66. |
| New | Penny.cs cites and inline comments in `OcMatrix::maskFor` | 78b191936 (cites Penny.cs:156-186; `[original inline comment from Penny.cs:162, 271, 285, 306, 320]`). |

## Could not settle

- **U1.** Does any Anvelina firmware speak Protocol 1? The pinned gateware is "Protocol 2 Application Source Code" (gw/README.org:4) and NereusSDR's OrionMKII row is Protocol 2 (src/core/BoardCapabilities.cpp:705), yet Thetis has P1 bank 17 for this model and NereusSDR builds `P1CodecAnvelinaPro3` (P1RadioConnection.cpp:4154). Section H matters only if a P1 image exists. The bank 17 bit layout is by analogy with the P2 spec, unverified on a radio.
- **U2.** Alex antenna control off. Thetis sends all antenna bits 0 when `alex_ant_ctrl_enabled` is false (cs/Alex.cs:312-316); NereusSDR gates on `caps.hasAlex` (RadioModel.cpp:26073-26083). Not traced further. Row E54.
- **U3.** JJ's Thetis "Gnd BPF2 on Tx". M1 rests on the fresh-install default; a saved Thetis database may differ. A Thetis pcap of one key on the Anvelina would settle what he is used to.
- **U4.** Input 2's board in bypass. Only input 1 was measured; the deliberate deviation on Alex1 (row B, Alex-2 master bypass) assumes input 2 behaves the same (AlexFilterMap.h:314-317).
- **U5.** Byte 1397 bit 4 / USEROUT10. Thetis masks `0x0f` so bit 4 is never set; the gateware maps `Open_Collector_Anvelina_DX[4]` to USEROUT10 (gw/Orion.v:923). NereusSDR now sends it (`0x1E`), so DX OUT 10 works; not yet seen on a radio.
- **U6.** Thetis Alex1 bit 27 transients: copies of Alex0 into Alex1 while keyed (cm/netInterface.c:489-490, :692-693) can set Alex1 bit 27 until the next copy after unkey. Inert per gw/Orion.v:2350; not worth porting, not observed.

## Flagged, not built (JJ's call)

- P1 bank 16 `_rx2_gnd` (C1 bit 7, cm/networkproto1.c:662) is still sent as 0 (row H73).
- Setup's input-2 display does not name Slice B (row D48).
- No phone Setup row for "Gnd BPF2 on Tx" (remote parity: a window offered radioHardwareVersion 14 can write the key, the phone's Setup description has no row for it).
- No phone Setup rows for the Anvelina DX Outputs.
- P1 mismatches M6 and M7, XVTR enable M9, Aries clamp M11, `alex_enabled` U2, as listed above.
- Pin-action edit or TUNE / two-tone rising while keyed applies at once; Thetis recomputes only on the next MOX or frequency edge. A frame or two can carry TUNE's pins after a re-key during the unkey walk (row G65).
- The Antenna Control capture baseline: the "Gnd BPF2 on Tx" box now shows ticked on the models that show it, so any stored visual baseline of that tab recorded unticked will differ.

## Appendix: Anvelina DX open collector outputs (byte 1397)

| Item | Spec (EU2AV, 2026-05-21) | Thetis v2.10.3.15 | Thetis-Enhanced (EU2AV) @567a7ec | Gateware | NereusSDR at 78b191936 | Outcome |
| --- | --- | --- | --- | --- | --- | --- |
| Wire: P2 byte 1397, bits 1-4 to USEROUT7-10 | mask `0x1F`, bit 0 always 0 (page 2) | `packetbuf[1397] = prn->oc_output_extras & 0x0f;  // [4:0]` gated on ANVELINAPRO3 (cm/network.c:1016-1020); mask drops bit 4 | same lines (cm/network.c:1016-1020) | byte 1397 to `Open_Collector_Anvelina_DX[4:0]` (gw/High_Priority_CC.v:98, :251); USEROUT7-10 = running ? bits : 0 (gw/Orion.v:920-923) | `buf[1397] = ocExtrasByte & kDxWireMask` (P2CodecOrionMkII.cpp:314-320); `kDxWireMask = 0x1E` (CodecContext.h:35-53) | Fixed, af7db05b9. Deliberate deviation: `0x1E` not `0x0f` (JJ "yes"). |
| Wire: P1 bank 17 | not in spec | cm/networkproto1.c:668-674 | same | no P1 gateware pinned | P1CodecAnvelinaPro3.cpp:50-71 | Fixed, af7db05b9 (by analogy, unverified). |
| Setter | `update_anvelina_dx_oc(bits)` | `SetOCExtraBits`, cm/netInterface.c:409-418, cs/HPSDR/NetworkIOImports.cs:333; never called from Console | same | | `RadioConnection::setOcExtrasByte` (RadioConnection.h:750); P2 resends at once when running | Fixed, af7db05b9. |
| UI: Setup > OC Outputs > "Anvelina DX Outputs", DX OUT 7-10, names, All on / All off, disabled when the radio is stopped | yes | none | none | outputs forced low when run = 0 | `OcOutputsTab::buildDxGroup` (OcOutputsTab.cpp:398), `applyDxRunning` :527, `applyDxVisibility` :560; model `AnvelinaDxOutputs` (AnvelinaDxOutputs.cpp:34, :82) | Fixed, 0f5c6649e. Shown only for the Anvelina; stopped state disabled with a reason. |
| Remote Core | n/a | n/a | n/a | | `oc/dx/...` applies as the `"ocDx"` reload (RadioModel.cpp:33818-33822); window gate at radioHardwareVersion 14 (HardwarePage.cpp:492-494, `IStationLink::dxOutputsUnavailableReason`) | Fixed, af7db05b9 and 0abc168ba. Disabled with a reason, never hidden, on an older Core. |
| Macro `SET_ANVELINA_DX_OUT` | yes | none | none | | none; macro buttons are unbuilt (UnbuiltFeatureList MacroButtons) | Not built (macro buttons do not exist yet); JJ's mockup sign-off carried no macro. |
