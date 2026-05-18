// =================================================================
// src/core/codec/CodecContext.h  (NereusSDR)
// =================================================================
//
// POD aggregating the live state every IP1Codec subclass needs to
// produce a 5-byte C&C bank payload. Lifted from P1RadioConnection
// member variables to make codec subclasses pure functions of
// {ctx × bank} — trivially unit-testable without a live socket.
//
// NereusSDR-original. No Thetis port; no PROVENANCE row.
// Independently implemented from Protocol 1 interface design.
//
// no-port-check: NereusSDR-original aggregator. Field-level comments may
//   cite Thetis for default-value origins (e.g. create_rnet defaults from
//   netInterface.c:1416) without making the file a port.
// =================================================================

#pragma once

#include <QtGlobal>
#include <QMetaType>
#include <cstdint>

namespace NereusSDR {

struct CodecContext {
    // MOX (transmit) bit — OR'd into low bit of C0.
    bool    mox{false};

    // RX-side step attenuator per ADC, 0-63 dB (clamped per board).
    int     rxStepAttn[3]{};

    // TX-side step attenuator per ADC. HL2 sends txStepAttn[0] when MOX=1
    // (mi0bot networkproto1.c:1099-1102).
    int     txStepAttn[3]{};

    // Per-RX preamp enable bits.
    bool    rxPreamp[3]{};

    // Per-ADC dither + random bits — default ON to match
    // P1RadioConnection legacy state (m_dither[3]{true,true,true},
    // m_random[3]{true,true,true}). Bank 0 C3 bits 3 + 4.
    bool    dither[3]{true, true, true};
    bool    random[3]{true, true, true};

    // ── P2-specific fields ──────────────────────────────────────────────
    //
    // Saturn BPF1 band-edge override bits — populated by
    // P2RadioConnection::setReceiverFrequency when the connected board
    // is ANAN-G2 / G2-1K AND user has configured Saturn BPF1 edges
    // distinct from Alex defaults (see Phase 3P-B spec §7.1, Phase F
    // mockup page-alex1-filters.html). When 0, P2CodecSaturn falls back
    // to Alex bits computed by AlexFilterMap.
    quint8  p2SaturnBpfHpfBits{0};
    quint8  p2SaturnBpfLpfBits{0};

    // Per-receiver antenna routing (RX1/RX2/RX3). Phase F antenna
    // assignment populates these from the per-band Alex matrix; Phase B
    // codecs read them but the Antenna Control sub-sub-tab UI lands in
    // Phase F.
    int     p2RxAnt[3]{0, 0, 0};

    // Per-ADC RX1 preamp toggle (OrionMKII-family + Saturn-family
    // dual-ADC boards). Phase B Task 10 wires the RxApplet UI for this;
    // P2CodecOrionMkII reads it for byte 1403 bit 1 in CmdHighPriority.
    bool    p2Rx1Preamp{false};

    // Alex HPF / LPF bits — recomputed by P1RadioConnection on freq change
    // via AlexFilterMap. Codec only emits them.
    quint8  alexHpfBits{0};
    quint8  alexLpfBits{0};

    // P2 run state — prn->run. Set by P2RadioConnection on start/stop.
    bool    p2Running{false};

    // P2 PTT out + CW keying bits — prn->tx[0].ptt_out / cwx / dot / dash.
    // Mapped to CmdHighPriority byte 4 (PTT/run) and byte 5 (CW bits).
    int     p2PttOut{0};
    int     p2Cwx{0};
    int     p2Dot{0};
    int     p2Dash{0};

    // P2 PureSignal run flag — prn->puresignal_run.  When this AND
    // p2PttOut are both true, CmdHighPriority bytes 9-16 (DDC0 + DDC1
    // frequencies) are overridden to TX freq instead of RX freqs, so
    // both DDCs sample at the TX frequency for PA-feedback / TX-monitor
    // capture.  From Thetis network.c:936-945 [v2.10.3.13]:
    //   packetbuf[9..12] = (prn->tx[0].ptt_out && prn->puresignal_run)
    //                      ? tx_freq_phase_word : rx0_freq_phase_word;
    //   packetbuf[13..16] = same gate, rx1 freq fallback.
    bool    puresignalRun{false};

    // P2 TX drive level — prn->tx[0].drive_level. Byte 345 of CmdHighPriority.
    int     p2DriveLevel{0};

    // P2 TX PA enable — prn->tx[0].pa. Byte 58 of CmdGeneral (inverted).
    // Default 1 = PA enabled → byte 58 = 0 ((!pa) & 0x01).
    int     p2TxPa{1};

    // P2 TX phase shift — prn->tx[0].phase_shift. Bytes 26-27 of CmdTx.
    int     p2TxPhaseShift{0};

    // P2 TX sampling rate — prn->tx[0].sampling_rate (kHz). Bytes 14-15 CmdTx.
    // Default 192 per Thetis create_rnet (netInterface.c:1513).
    int     p2TxSamplingRate{192};

    // P2 number of ADCs / DACs — prn->num_adc / num_dac. Byte 4 of CmdRx/CmdTx.
    int     p2NumAdc{1};
    int     p2NumDac{1};

    // P2 RX per-DDC state (up to 7 DDCs). Mapped from m_rx[] in composeCmdRx.
    // p2RxEnable: enable bit per DDC.  p2RxAdc: ADC assignment.
    // p2RxSamplingRate: rate in kHz. p2RxBitDepth: bit depth (24 default).
    int     p2RxEnable[7]{0, 0, 0, 0, 0, 0, 0};
    int     p2RxAdcAssign[7]{0, 0, 0, 0, 0, 0, 0};
    int     p2RxSamplingRate[7]{48, 48, 48, 48, 48, 48, 48};
    int     p2RxBitDepth[7]{24, 24, 24, 24, 24, 24, 24};
    int     p2RxSync{0};  // prn->rx[0].sync — byte 1363 of CmdRx

    // P2 custom port base — prn->p2_custom_port_base. Default 1025.
    int     p2CustomPortBase{1025};

    // P2 wideband settings — prn->wb_samples_per_packet / wb_sample_size /
    // wb_update_rate / wb_packets_per_frame. From Thetis create_rnet defaults.
    int     p2WbSamplesPerPacket{512};
    int     p2WbSampleSize{16};
    int     p2WbUpdateRate{70};
    int     p2WbPacketsPerFrame{32};

    // P2 watchdog timer — prn->wdt. 0 = disabled. Byte 38 of CmdGeneral.
    int     p2Wdt{0};

    // P2 CW state — prn->cw.*. Used in CmdTx bytes 5-13, 17.
    unsigned char p2CwModeControl{0};
    int     p2CwSidetoneLevel{0};
    int     p2CwSidetoneFreq{0};
    int     p2CwKeyerSpeed{0};
    int     p2CwKeyerWeight{0};
    int     p2CwHangDelay{0};
    int     p2CwRfDelay{0};
    int     p2CwEdgeLength{7};  // From Thetis create_rnet:1454

    // P2 Mic state — prn->mic.*. Used in CmdTx bytes 50-51.
    unsigned char p2MicControl{0};
    int     p2MicLineInGain{0};

    // P2 Alex antenna selection — rxAnt/txAnt for Alex0/Alex1 register.
    // 1=ANT1, 2=ANT2, 3=ANT3. Default 1.
    int     p2AlexRxAnt{1};
    int     p2AlexTxAnt{1};

    // TX drive level (0-255).
    int     txDrive{0};

    // PA enable — still used for HPF/LPF routing and meter scaling logic.
    // NOTE: paEnabled no longer controls bank-10 C3 bit 7 on the wire;
    // that bit is now driven by trxRelay (see below).  Retain paEnabled
    // for any caller that needs the PA-profile flag independently.
    bool    paEnabled{false};

    // Alex T/R relay engage state (bank 10 C3 bit 7, INVERTED on the wire).
    // true  = relay engaged (normal TX path) → bit 7 = 0
    // false = relay disabled (PA bypass / RX protect) → bit 7 = 1
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
    //   if (txband->disablePA || !pa_enabled)
    //       output_buffer[C3] |= 0x80; // disable Alex T/R relay
    // Populated by buildCodecContext() from P1RadioConnection::m_trxRelay.
    bool    trxRelay{false};

    // P1 mic-jack boost bit — bank 10 (C0=0x12) C2 bit 0 (0x01).
    // Polarity: 1 = boost on (no inversion).
    // Source: Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
    //   C2 = ((prn->mic.mic_boost & 1) | ...)
    // Populated by buildCodecContext() from RadioConnection::m_micBoost.
    bool    p1MicBoost{false};

    // P1 mic-jack line-in bit — bank 10 (C0=0x12) C2 bit 1 (0x02).
    // Polarity: 1 = line in active (no inversion).
    // Source: Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
    //   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ...)
    // Populated by buildCodecContext() from RadioConnection::m_lineIn.
    bool    p1LineIn{false};

    // P1 mic-jack Tip/Ring polarity — bank 11 (C0=0x14) C1 bit 4 (0x10), INVERTED.
    // NereusSDR convention: true = Tip is mic (intuitive).
    // POLARITY INVERSION: wire bit is written as !p1MicTipRing.
    //   p1MicTipRing = true  → Tip is mic  → wire bit 4 = 0
    //   p1MicTipRing = false → Tip is BIAS → wire bit 4 = 1
    // Source: Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
    //   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ...
    //   mic_trs: 1 = Tip is BIAS/PTT (ring is mic), 0 = Tip is mic (normal).
    // Populated by buildCodecContext() from RadioConnection::m_micTipRing.
    bool    p1MicTipRing{true};

    // P1 mic-jack phantom power (bias) — bank 11 (C0=0x14) C1 bit 5 (0x20).
    // Polarity: 1 = bias on (no inversion).
    // Source: Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
    //   C1 = ... | ((prn->mic.mic_bias & 1) << 5) | ...
    // Populated by buildCodecContext() from RadioConnection::m_micBias.
    bool    p1MicBias{false};

    // P1 mic-jack PTT disable flag — bank 11 (C0=0x14) C1 bit 6 (0x40), direct.
    // Wire convention matches Thetis byte-for-byte:
    //   p1MicPTTDisabled = false → wire bit 6 = 0 → PTT enabled  (default)
    //   p1MicPTTDisabled = true  → wire bit 6 = 1 → PTT disabled
    //
    // From Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13+501e3f51]:
    //   C1 = ... | ((prn->mic.mic_ptt & 1) << 6);
    // From Thetis console.cs:19757-19764 [v2.10.3.13+501e3f51]:
    //   private bool mic_ptt_disabled = false;
    //   NetworkIO.SetMicPTT(Convert.ToInt32(mic_ptt_disabled));
    //
    // Field renamed from p1MicPTT for issue #182 to make the storage name
    // match Thetis MicPTTDisabled / mic_ptt_disabled exactly.  This eliminates
    // the prior P1/P2 setter polarity mismatch where one took "enabled" and
    // the other took "disabled" under the same setMicPTT(bool) signature.
    // Populated by buildCodecContext() from RadioConnection::m_micPTTDisabled.
    bool    p1MicPTTDisabled{false};

    // Mic line-in gain — prn->mic.line_in_gain, 5-bit (0-31).
    // Source: Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    // P1 wire: bank 11 C2 low 5 bits.
    // P2 wire: byte 51 of CmdHighPriority (already plumbed via P2's m_mic.lineInGain).
    int     p1LineInGain{0};

    // PureSignal feedback running flag — prn->puresignal_run.
    // Source: Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
    // P1 wire: bank 11 C2 bit 6.
    // Distinct from BoardCapabilities.hasPureSignal (capability) and from
    // TransmitModel::puresignalEnabled (user toggle) — this tracks whether
    // the PureSignal feedback DDC routing is currently *active* on the wire.
    // Wired in Task 2.5 to TransmitModel::puresignalEnabled until 3M-4 lights
    // up the actual feedback DDC routing.
    bool    p1PuresignalRun{false};

    // Phase 3M-4 Task 17 P1 follow-up: PureSignal nDdc state for the
    // bank-2/3 (RX1/RX2 VFO DDC) freq-override gate.  Captured from
    // P1RadioConnection::m_psNDdc by buildCodecContext() — itself populated
    // from PsDdcConfig.nDdc when the per-board P1 codec
    // (P1CodecHl2/P1CodecStandard) runs applyPureSignalDdcConfig.
    //
    // Source: mi0bot ChannelMaster/networkproto1.c:985 + 1000
    // [v2.10.3.13-beta2] (byte-for-byte identical to ramdor :487 + :502
    // [v2.10.3.13]):
    //   if ((nddc == 2) && (XmitBit == 1) && (prn->puresignal_run))
    //       ddc_freq = prn->tx[0].frequency;
    // Triggers on HermesII / ANAN10E / ANAN100B (nddc==2 boards) only;
    // HL2 / Hermes / ANAN10 / ANAN100 (nddc==4) skip the override because
    // the firmware handles freq routing internally via cntrl1=4 ADC-to-DDC
    // steering set in P1CodecHl2::applyPureSignalDdcConfig from mi0bot
    // console.cs:8486 [v2.10.3.13-beta2].
    int     p1PsNDdc{2};

    // User digital outputs — prn->user_dig_out, low 4 bits (0-15).
    // Source: Thetis ChannelMaster/networkproto1.c:601 [v2.10.3.13+501e3f51]
    //   C3 = prn->user_dig_out & 0b00001111;
    // P1 wire: bank 11 C3 low 4 bits.
    // Drives the 4 user-controllable digital pins on the radio's accessory
    // header (Penny / Hermes Ctrl board). UI added in Task 4.3 (Setup →
    // Hardware → OC Outputs → "User Dig Out 1..4"), gated on
    // BoardCapabilities.hasPennyLane.
    quint8  p1UserDigOut{0};

    // RX VFO frequency words (Hz, raw, no phase-word conversion on P1).
    quint64 rxFreqHz[7]{};
    quint64 txFreqHz{0};

    // P2 NCO phase-word frequency-correction factor.
    // Source: setup.cs:14036-14050 udHPSDRFreqCorrectFactor_ValueChanged →
    //   NetworkIO.FreqCorrectionFactor (0.999... / 1.000... trims ppm error).
    //   P2RadioConnection populates this from CalibrationController::
    //   effectiveFreqCorrectionFactor(); codecs fold it into hzToPhaseWord().
    //   Default 1.0 → byte-identical to pre-calibration output. [@501e3f5]
    double  freqCorrectionFactor{1.0};

    // Sample rate code (0=48k, 1=96k, 2=192k, 3=384k).
    int     sampleRateCode{0};

    // Number of active DDCs (NDDC), 1..7.
    int     activeRxCount{1};

    // OC output byte (bank 0 C2). Phase D will drive this from OcMatrix.
    quint8  ocByte{0};

    // ADC-to-DDC routing — historical NereusSDR field that absorbs the
    // P2 cntrl1 + cntrl2 bytes from `UpdateDDCs` (Thetis console.cs:8531
    // `NetworkIO.SetADC_cntrl1` / 8532 `SetADC_cntrl2` [v2.10.3.13]).
    // On P2 it correctly drives the per-DDC ADC-assign bytes (CmdRx bytes
    // 17/23/29/35) via composeCmdRx. NOT used by the P1 wire (see
    // `p1AdcCntrl` below): NereusSDR P1 codecs used to read this for
    // bank 4 C1/C2 too, which conflated UpdateDDCs's cntrl1 with the
    // separate `P1_adc_cntrl` Thetis global — a real port-fidelity bug
    // surfaced while diagnosing #263. The P1 codecs now read
    // `p1AdcCntrl` for bank 4; `adcCtrl` is retained for any
    // P2-specific code path still reading it.
    quint16 adcCtrl{0};

    // P1-specific ADC routing (Thetis `P1_adc_cntrl` global; 14 bits,
    // 2 bits per DDC, layout 6655...1100 per console.cs:15120 [v2.10.3.13]).
    //
    // Source-of-truth for P1 bank 4 C1/C2 wire bytes per Thetis
    // networkproto1.c:519-520 [v2.10.3.13]:
    //   C1 = P1_adc_cntrl & 0xFF;
    //   C2 = (P1_adc_cntrl >> 8) & 0b0011111111;
    //
    // Thetis `P1_adc_cntrl` lives in netInterface.c as the storage
    // target of SetADC_cntrl_P1, which is called only from
    // console.cs:7325-7328 UpdateRXADCCtrlP1(), which fires only when
    // the user touches `radP1DDC*ADC*` radio buttons in Setup. The
    // cntrl1 value UpdateDDCs computes (and which we still carry through
    // `adcCtrl` for P2) is independent — that path maps to
    // prn->rx[i].rx_adc (P2 wire bytes) only.
    //
    // P1RadioConnection populates this from m_p1AdcCntrl, which defaults
    // to 0 for 1-ADC boards (Hermes / HermesII / HL2) and 4 for 2-ADC
    // boards (Angelia / Orion / OrionMkII / Saturn / G2). The 0 default
    // for 1-ADC matches the wire bytes observed on a working
    // Thetis-driven ANAN-10E on 2026-05-09; the 4 default for 2-ADC
    // matches Thetis fresh-install (console.cs:15120 initializer). Per-MAC
    // AppSettings override via `hardware/<mac>/p1AdcCntrl`.
    quint16 p1AdcCntrl{0};

    // Antenna selection (bank 0 C4 low 2 bits).
    int     antennaIdx{0};

    // RX-only input mux (bank 0 C3 bits 5-6).
    // 0=none, 1=RX1_In (bit5), 2=RX2_In (bit6), 3=XVTR_Rx_In (bits5+6).
    // Source: Thetis netInterface.c:479-481, networkproto1.c:456-461
    // [v2.10.3.13 @501e3f5]
    int     rxOnlyAnt{0};

    // _Rx_1_Out relay (RX-Bypass-Out). Bank 0 C3 bit 7.
    // Source: Thetis networkproto1.c:455 [v2.10.3.13 @501e3f5]
    bool    rxOut{false};

    // Duplex / diversity (bank 0 C4 bits 2 + 7).
    bool    duplex{true};
    bool    diversity{false};

    // HL2-only state (mi0bot networkproto1.c:1162-1176, banks 17-18).
    int     hl2PttHang{0};       // 5-bit
    int     hl2TxLatency{0};     // 7-bit
    bool    hl2ResetOnDisconnect{false};
};

// PureSignal DDC config bytes/words emitted by per-board codec.
// Consumed by ReceiverManager::updateDdcAssignment() (Phase 3M-4 Task 6)
// during MOX+PS-on transitions.
//
// Field semantics: ported verbatim from Thetis console.cs:8186-8538
// UpdateDDCs() [v2.10.3.13] (and mi0bot console.cs:8409-8488
// [v2.10.3.13-beta2] for HL2 deltas).  Each codec subclass returns a
// PsDdcConfig populated to match its per-HpsdrModel branch in the
// upstream switch statement.
//
// This is the wire-byte map; the consumer applies it via:
//   NetworkIO.EnableRxs(ddcEnable)            // console.cs:8527
//   NetworkIO.EnableRxSync(0, syncEnable)     // console.cs:8528
//   NetworkIO.SetDDCRate(i, rate[i]) for i<4  // console.cs:8529-8530
//   NetworkIO.SetADC_cntrl1(cntrl1)           // console.cs:8531
//   NetworkIO.SetADC_cntrl2(cntrl2)           // console.cs:8532
//   NetworkIO.Protocol1DDCConfig(p1DdcConfig, P1_diversity, p1RxCount, nDdc)
//                                             // console.cs:8534
struct PsDdcConfig {
    uint8_t  cntrl1     = 0;     // ADC control register 1
    uint8_t  cntrl2     = 0;     // ADC control register 2
    uint32_t rate[8]    = {};    // Per-DDC sample rates (Hz)
    uint8_t  ddcEnable  = 0;     // Bit mask of enabled DDCs (DDC0=1, DDC1=2, DDC2=4, DDC3=8)
    uint8_t  syncEnable = 0;     // Bit mask of synced DDCs
    int      p1DdcConfig = 0;    // P1-only board-config code (see UpdateDDCs branches)
    int      p1RxCount  = 0;     // P1-only RX count for state-machine
    int      nDdc       = 0;     // Total DDC count for this board family

    // Phase 3M-4 mi0bot audit: per-board PS feedback / TX-monitor DDC indices
    // for PsccPump dispatch.  Different board families place the PS pair on
    // different DDC slots:
    //
    //   nddc=2 (HermesII / ANAN-10E / ANAN-100B):
    //       psFbDdc=0, txMonDdc=1 — networkproto1.c:984 bank-2/3 freq override
    //       forces DDC0+DDC1 to TX freq during PS-MOX
    //   nddc=4 (Hermes / HL2 / ANAN-10 / ANAN-100):
    //       psFbDdc=2, txMonDdc=3 — networkproto1.c MetisRead case 4
    //       `twist(spr, 2, 3, 1)` pairs DDC2+DDC3 (and console.cs
    //       GetDDC():8757-8762 confirms `psrx=2, pstx=3` for HL2 PS-MOX)
    //   nddc=5 (Orion / Saturn / Andromeda / etc.):
    //       psFbDdc=0, txMonDdc=1 — P2 network.c:936-945 unconditional
    //       freq override; P1 case 5 in MetisRead twists DDC0+DDC1
    //
    // From Thetis cmaster.cs:533-534 [v2.10.3.13]:
    //   SetPSRxIdx(0, 0);   // ps_rx_idx points to data[0] in InboundBlock
    //   SetPSTxIdx(0, 1);   // ps_tx_idx points to data[1]
    // Those constants refer to STREAM indices in the InboundBlock data**
    // array (always 0/1 because twist() always pairs into data[0]+data[1]).
    // NereusSDR's PsccPump consumes raw per-DDC streams from
    // RadioConnection::iqDataReceived(ddcIndex, samples) so it needs the
    // actual DDC indices, which depend on the per-board read-loop dispatch.
    //
    // From Thetis console.cs:8579 GetDDC() [v2.10.3.13]:
    //   HL2 P1 PS-MOX (case 5):  rx1=0, rx2=1, psrx=2, pstx=3
    //   HermesII P1 PS-MOX:      psrx=0, pstx=1
    //   Saturn-class P2 PS-MOX:  rx1=2, rx2=3 (DDC0+DDC1 implicit PS pair)
    //
    // Defaults match cmaster.cs:533-534 (Stream0/Stream1) which is correct
    // for nddc=2 boards and Saturn-class P2.  Per-board codec overrides for
    // the nddc=4 family (HL2 / Hermes / ANAN10 / ANAN100).
    int      psFbDdc    = 0;     // PS feedback DDC index (Stream0 default)
    int      txMonDdc   = 1;     // TX monitor DDC index (Stream1 default)
};

} // namespace NereusSDR

// Phase 3M-4 Task 6: PsDdcConfig must round-trip through Qt's signal/slot
// queue (ReceiverManager::ddcConfigChanged → RadioConnection consumer) and
// through QSignalSpy in tests, so it needs a metatype declaration.
Q_DECLARE_METATYPE(NereusSDR::PsDdcConfig)
