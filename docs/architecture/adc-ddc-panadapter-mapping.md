# ADC-to-DDC-to-Panadapter Mapping

**Status:** Phase 2 -- Architecture Analysis
**Date:** 2026-04-09
**Author:** JJ Boyd ~KG4VCF, Co-Authored with Claude Code

---

## Overview

This document describes the complete signal chain from hardware ADC through
DDC to client-side panadapter display in NereusSDR, reconciled against the
Thetis source code. It answers: how is each ADC mapped to each DDC, how does
a DDC feed a panadapter, and how much spectrum can each pan actually show?

**Key insight:** Each ADC captures the full 0-61.44 MHz band, but each DDC
narrows this to a window determined by its sample rate (max 384 kHz). The
panadapter displays this narrow window, NOT the full ADC bandwidth.

---

## 1. The Full Signal Chain

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   ADC        │───>│   DDC        │───>│  Receiver    │───>│  FFTEngine   │───>│  Panadapter  │
│  (hardware)  │    │  (hardware)  │    │  (client)    │    │  (client)    │    │  (client)    │
│              │    │              │    │              │    │              │    │              │
│ 122.88 MHz   │    │ Tunes to any │    │ WDSP channel │    │ FFTW3 4096pt │    │ Display view │
│ sample rate  │    │ freq in ADC  │    │ I/Q -> audio │    │ I/Q -> dBm   │    │ into FFT data│
│ 0-61.44 MHz  │    │ bandwidth    │    │              │    │ bins         │    │              │
│ full Nyquist │    │ Outputs at   │    │              │    │              │    │              │
│              │    │ 48k-384k Hz  │    │              │    │              │    │              │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
     1 or 2              up to 7           up to 7              1 per RX           up to 4
   per radio           per radio         (logical)                              (NereusSDR)
```

**Link 1: ADC to DDC** -- Configured by the client, executed in FPGA firmware.
Each DDC can be assigned to either ADC (on 2-ADC boards). This is the
`rx_adc_ctrl` register.

**Link 2: DDC to Receiver** -- The client decides which DDC index carries data
for which logical receiver. In Protocol 1 this is implicit (DDC ordering in
the EP6 stream). In Protocol 2 it is explicit (per-DDC enable/frequency in
the CmdRx packet).

**Link 3: Receiver to FFTEngine** -- One FFTEngine per receiver. I/Q samples
flow from `ReceiverManager::feedIqData()` to `FFTEngine::feedIQ()` on the
spectrum worker thread.

**Link 4: FFTEngine to PanadapterApplet** -- The FFTRouter maps receiverId to
one or more panIds. Multiple pans can share one receiver's FFT output.

---

## 2. Spectrum Bandwidth Per Panadapter

| DDC Sample Rate | Usable Display Bandwidth | Typical Use |
|----------------|------------------------|-------------|
| 48,000 Hz | ~48 kHz | Narrow: single SSB/CW signal |
| 96,000 Hz | ~96 kHz | Medium: see nearby signals |
| 192,000 Hz | ~192 kHz | Wide: see good portion of a band |
| 384,000 Hz | ~384 kHz | Maximum: see most of a band segment |

- **Max visible spectrum per panadapter = ~384 kHz** (at max DDC sample rate)
- The pan can zoom IN within this range but never wider than the DDC provides
- Two pans showing the SAME receiver can show different zoom levels of the
  same ~384 kHz window
- Two pans showing DIFFERENT receivers can show completely different
  frequencies/bands

Display bandwidth calculation (from Thetis `console.cs:9814`):

```
bin_width = sample_rate_rx / fft_size
display_bandwidth = sample_rate_rx  (the full FFT covers the sample rate)
```

---

## 3. Hardware: ADC Count by Board Type

| Board Type | ADCs | Max DDCs | Max RX | Radios |
|-----------|------|----------|--------|--------|
| Metis | 1 | 3 | 3 | Metis board |
| Hermes | 1 | 4 | 4 | ANAN-10/100 |
| HermesII | 1 | 2 | 2 | ANAN-10E/100B |
| Angelia | 2 | 7 | 7 | ANAN-100D |
| Orion | 2 | 7 | 7 | ANAN-200D |
| Orion MkII | 2 | 7 | 7 | ANAN-7000DLE/8000DLE |
| Saturn | 2 | 7 | 7 | ANAN-G2 |
| Hermes Lite | 1 | 4 | 4 | Hermes Lite 2 |

Source: Thetis `clsHardwareSpecific.cs:85-185`, `NetworkIO.SetRxADC(n)`

---

## 4. DDC-to-ADC Assignment Registers (Thetis)

Thetis uses three bitfield registers to assign which ADC feeds each DDC.

### 4.1 Register Encoding

| Register | Bits | Protocol | Scope | Format |
|----------|------|----------|-------|--------|
| `rx_adc_ctrl1` | 8 bits | P2 (ETH) | DDC0-DDC3 | 2 bits per DDC: `33221100` |
| `rx_adc_ctrl2` | 6 bits | P2 (ETH) | DDC4-DDC7 | 2 bits per DDC: `--776655` |
| `rx_adc_ctrl_P1` | 14 bits | P1 (USB) | DDC0-DDC6 | 2 bits per DDC: `66554433221100` |

Each 2-bit field selects: `00` = ADC0, `01` = ADC1, `10` = ADC2 (PS feedback
on some boards), `11` = reserved.

### 4.2 Default Values

Default `rx_adc_ctrl1 = 4` (binary `00000100`):
- DDC0: bits [1:0] = `00` -> ADC0
- DDC1: bits [3:2] = `01` -> ADC1
- DDC2: bits [5:4] = `00` -> ADC0
- DDC3: bits [7:6] = `00` -> ADC0

These are user-configurable via the Setup dialog's ADC tab. The
`UpdateDDCs()` function (console.cs:8186) overrides specific fields based
on operating mode (diversity, PureSignal).

### 4.3 Extracting ADC Assignment

From Thetis `console.cs:15083`:

```csharp
public int GetADCInUse(int ddc)
{
    int adcControl = ddc < 4 ? RXADCCtrl1 : RXADCCtrl2;
    if (ddc >= 4) ddc -= 4;
    int mask = 3 << (ddc * 2);
    return (adcControl & mask) >> (ddc * 2);
}
```

---

## 5. DDC Assignment Strategy by Board Type and Mode

### 5.1 Two-ADC Boards (ANAN-100D, 200D, 7000DLE, 8000DLE, G2)

From Thetis `console.cs:8210-8295`:

| State | DDC0 | DDC1 | DDC2 | DDC3 | DDCEnable | SyncEnable |
|-------|------|------|------|------|-----------|------------|
| **Normal RX** | idle | idle | **RX1** | RX2 | DDC2 | none |
| **RX + Diversity** | RX1 (ADC0) | RX1 sync (ADC1) | idle | RX2 | DDC0 | DDC1 |
| **TX, no PS** | idle | idle | RX1 | RX2 | DDC2 | none |
| **TX + PureSignal** | PS fwd (synced) | PS rev (synced) | RX1 | RX2 | DDC0+DDC2 | DDC1 |
| **TX + Div + PS** | PS fwd (synced) | PS rev (synced) | RX1 | RX2 | DDC0+DDC2 | DDC1 |
| **TX + Diversity** | RX1 (ADC0) | RX1 sync (ADC1) | idle | RX2 | DDC0 | DDC1 |

**Critical: DDC2 is the primary RX1 on 2-ADC boards**, not DDC0. DDC0/DDC1
are reserved for diversity and PureSignal.

When RX2 is enabled, DDC3 is added to DDCEnable in all cases.

### 5.2 One-ADC Boards (Hermes, ANAN-10, ANAN-100)

From Thetis `console.cs:8378-8448`:

| State | DDC0 | DDC1 | DDCEnable | SyncEnable |
|-------|------|------|-----------|------------|
| **Normal RX** | **RX1** | RX2 | DDC0 (+DDC1 if RX2) | none |
| **RX + Diversity** | RX1 (diversity) | RX1 sync | DDC0 | DDC1 |
| **TX + PureSignal** | PS feedback | PS feedback sync | DDC0 | DDC1 |

### 5.3 HermesII Boards (ANAN-10E, ANAN-100B)

From Thetis `console.cs:8451-8521`:

Only 2 DDCs available. Same pattern as Hermes but with `P1_rxcount = 2`.
PureSignal feedback uses DDC0+DDC1 synced (no room for independent RX2
during PS TX).

### 5.4 PureSignal ADC Override

When PureSignal is active during TX, `UpdateDDCs()` modifies cntrl1:

```csharp
cntrl1 = (rx_adc_ctrl1 & 0xf3) | 0x08;
// Clears bits [3:2] (DDC1 field) and sets to 10 = ADC2
// Routes DDC1 to PA feedback ADC for linearization
```

Mask `0xf3` = `11110011` zeros out DDC1's 2-bit field.
`0x08` = `00001000` sets DDC1 to ADC2.

### 5.5 The GetDDC() Lookup

Thetis `console.cs:8540-8635` provides a centralized lookup that returns
DDC assignments for the current operating state:

```csharp
public void GetDDC(out int DDCrx1, out int DDCrx2,
                   out int DDCsync1, out int DDCsync2,
                   out int DDCpsrx, out int DDCpstx)
```

For 2-ADC P2 boards in normal operation: `rx1=2, rx2=3`.
For diversity: `sync1=0, sync2=1, rx2=3`.

Reference spreadsheet:
https://docs.google.com/spreadsheets/d/1DbOAXuhHRE1hMBwz8PyxL5FblW5BoiAUzksPaGM0NXY

---

## 6. Concrete Example: ANAN-G2, Normal RX, RX2 Enabled

```
ADC0 (0-61.44 MHz)  --- DDC2 (14.225 MHz, 192kHz SR) --- Receiver 0 --- WDSP Ch 0
                                                                |
                                                           FFTEngine 0
                                                                |
                                                    +-- fftReady(0, bins) --+
                                                    |                       |
                                               Pan 0 (14.225 MHz,     Pan 2 (14.225 MHz,
                                                192kHz full view)       48kHz zoom view)

ADC1 (0-61.44 MHz)  --- DDC3 (7.150 MHz, 96kHz SR) --- Receiver 1 --- WDSP Ch 1
                                                                |
                                                           FFTEngine 1
                                                                |
                                                           fftReady(1, bins)
                                                                |
                                                           Pan 1 (7.150 MHz,
                                                            96kHz bandwidth)
```

- DDC0 and DDC1 are idle (reserved for diversity/PureSignal)
- Pan 0 and Pan 2 share Receiver 0's FFT data at different zoom levels
- Pan 1 shows a completely independent frequency range from Receiver 1
- Pan 0 displays 192 kHz of spectrum (not the full 61.44 MHz ADC bandwidth)
- Pan 2 shows a 48 kHz subset of Pan 0's data (client-side FFT bin windowing)

---

## 7. Spectrum Visibility: What You Can and Cannot Do

### What you CAN do

- **4 independent panadapters**, each showing up to ~384 kHz of spectrum
- **Each pan on a different receiver/DDC** -- see 4 different frequency ranges
  simultaneously
- **Two pans on the same receiver** -- same DDC data, different zoom levels
- **Any DDC assigned to either ADC** (on 2-ADC boards) -- e.g., put RX1 on
  ADC2 if ADC1 is noisy
- **Each DDC independently tunable** -- 7 DDCs = 7 different frequencies
  within 0-61.44 MHz
- **P2 independent DDC streams** -- no bandwidth penalty for multiple
  receivers

### What you CANNOT do

- **See more than ~384 kHz per pan** -- the DDC sample rate caps the view
- **See the full 61.44 MHz ADC bandwidth at once** -- no wideband bandscope
  in standard operation (wideband data IS available on P2 ports 1027-1034
  but Thetis does not use it for the main panadapter display)
- **Exceed 7 simultaneous DDCs** -- hardware limit
- **Run all 7 DDCs at 384 kHz on P1** -- P1 multiplexes all receivers into
  one EP6 stream; more receivers = fewer samples per frame per receiver

---

## 8. P1 vs P2 DDC Configuration Differences

| Aspect | Protocol 1 | Protocol 2 |
|--------|-----------|-----------|
| DDC-to-ADC encoding | `rx_adc_ctrl_P1` (14-bit packed) in C&C bytes | Per-DDC `rxAdc` byte in CmdRx packet |
| DDC enable | `DDCEnable` bitmask via `EnableRxs()` | Per-DDC enable bit in CmdRx byte 7 |
| DDC sync | `SyncEnable` bitmask via `EnableRxSync()` | `sync` field in CmdRx byte 1363 |
| DDC sample rate | Per-DDC via `SetDDCRate()` | Per-DDC 2-byte field in CmdRx packet |
| DDC configuration | `P1_DDCConfig` integer (0-6) preset | Individual per-DDC fields (more flexible) |
| I/Q data delivery | All DDCs multiplexed in EP6 stream | Each DDC gets own UDP port (1035-1041) |
| Bandwidth penalty | More DDCs = fewer samples per DDC | **No penalty** -- independent streams |
| Frequency encoding | Phase word: `freq * 2^32 / 122880000` | Direct Hz (32-bit integer) |

### P1 Samples Per Frame vs Receiver Count

| Active RX | Bytes/Block | Samples/Frame/RX | P2 Impact |
|-----------|-------------|-------------------|-----------|
| 1 | 10 | 50 | None |
| 2 | 16 | 31 | None |
| 3 | 22 | 22 | None |
| 4 | 28 | 18 | None |
| 5 | 34 | 14 | None |
| 6 | 40 | 12 | None |
| 7 | 46 | 10 | None |

---

## 9. NereusSDR Implementation Gaps

### 9.1 Missing: DDC-to-ADC Assignment Layer

`ReceiverManager` maps logical receivers to hardware RX indices but has no
concept of ADC assignment. The masterplan notes this as a known issue:
> "DDC mapping hardcoded -- should port full UpdateDDCs() from Thetis
> console.cs:8186"

**Needed:** A `DdcAssignmentStrategy` or extension to `ReceiverManager` that:
- Tracks which ADC feeds each DDC (the `rx_adc_ctrl1/2` bitfields)
- Implements the `UpdateDDCs()` state machine (diversity/PS mode switching)
- Sends `SetADC_cntrl1/2` to the radio via the CmdRx packet
- Exposes per-DDC ADC assignment for the Setup UI
- Handles the `GetADCInUse(ddc)` query

### 9.2 Missing: DDC Rate Management

Each DDC needs an independent sample rate. `UpdateDDCs()` sets `Rate[0..7]`
per-DDC and calls `SetDDCRate(i, Rate[i])`. Currently NereusSDR only has a
global `setReceiverSampleRate()`.

### 9.3 Missing: DDC Enable/Sync Logic

The `DDCEnable` bitmask and `SyncEnable` logic from `UpdateDDCs()` controls
which DDCs are active and which are synchronized (for diversity or PureSignal).
Not addressed in current architecture docs.

### 9.4 Gap in multi-panadapter.md

The multi-panadapter doc describes `FFTRouter::mapPanToReceiver(panId,
receiverId)` but does not mention the receiver-to-DDC-to-ADC chain that
determines what spectrum the pan can actually show. Should reference this
document.

### 9.5 Stale P2 Interface in radio-abstraction.md

The `P2RadioConnection` class interface still shows `QTcpSocket* m_tcpSocket`
despite the Phase 3A correction that P2 is UDP-only. The implemented code is
correct but the design doc was not fully updated.

### 9.6 Future: Wideband Bandscope

P2 sends wideband ADC data on ports 1027-1034 (1028-byte packets). This could
enable a bandscope view showing the full 0-61.44 MHz ADC bandwidth. Thetis
does not use this for the main display. Not blocking for parity; potential
NereusSDR enhancement.

---

## 10. Implementation Recommendations

1. **Port `UpdateDDCs()` from Thetis** -- the single most important function
   for correct multi-receiver operation. Source: `console.cs:8186-8538`.
2. **Add `rx_adc_ctrl1/2` management** to `ReceiverManager` or a new
   `DdcManager` class.
3. **Add per-DDC sample rate** rather than a single global rate.
4. **Update multi-panadapter.md** to reference the DDC/ADC layer as the
   hardware foundation beneath the FFTRouter abstraction.
5. **Fix P2RadioConnection interface** in radio-abstraction.md (remove TCP
   references).
6. **Consider wideband bandscope** as a future enhancement.

This fits naturally into Phase 3E (Multi-Panadapter Layout) or Phase 3I
(Protocol 1 Support, which needs the P1-specific `rx_adc_ctrl_P1` encoding).

---

## 11. Key Thetis Source Files for Porting

| File | Lines | What to Port |
|------|-------|-------------|
| `console.cs` | 8186-8538 | `UpdateDDCs()` -- core DDC assignment state machine |
| `console.cs` | 8540-8640 | `GetDDC()` -- DDC assignment lookup for current state |
| `console.cs` | 15072-15130 | `rx_adc_ctrl1/2/P1` properties and `GetADCInUse()` |
| `clsHardwareSpecific.cs` | 85-185 | `SetRxADC()` per board type (1 or 2 ADCs) |
| `setup.cs` | 21041-21140 | `UpdateDDCTab()` -- DDC assignment UI display |
| `NetworkIOImports.cs` | 340-360 | `SetADC_cntrl1/2`, `SetADC_cntrl_P1` DLL imports |
