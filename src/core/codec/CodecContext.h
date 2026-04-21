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

    // TX drive level (0-255).
    int     txDrive{0};

    // PA enable (bank 10 C3 bit 7).
    bool    paEnabled{false};

    // RX VFO frequency words (Hz, raw, no phase-word conversion on P1).
    quint64 rxFreqHz[7]{};
    quint64 txFreqHz{0};

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

    // Duplex / diversity (bank 0 C4 bits 2 + 7).
    bool    duplex{true};
    bool    diversity{false};

    // HL2-only state (mi0bot networkproto1.c:1162-1176, banks 17-18).
    int     hl2PttHang{0};       // 5-bit
    int     hl2TxLatency{0};     // 7-bit
    bool    hl2ResetOnDisconnect{false};
};

} // namespace NereusSDR
