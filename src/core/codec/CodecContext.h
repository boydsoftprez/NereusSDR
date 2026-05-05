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

    // ADC-to-DDC routing (bank 4 C1+C2).
    quint16 adcCtrl{0};

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

} // namespace NereusSDR
