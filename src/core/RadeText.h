// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-native Qt6 wrapper. The over-the-air format it
// drives is ported in RadeTextCodec / RadeLdpc / RadeHra5656 (registered in
// docs/attribution/FREEDV-GUI-PROVENANCE.md); the mentions of FreeDV below
// say which upstream calls this wrapper stands in for, not a port claim.
//
// =================================================================
// src/core/RadeText.h  (NereusSDR)
// =================================================================
//
// NereusSDR - RadeText: the callsign in a RADE end-of-over (EOO) frame,
// in FreeDV's format, as a small Qt6 object RadeChannel owns.
//
// TX: pushTxCallsign(rade) encodes ourCallsign() with RadeTextCodec (the
// port of freedv-backend's rade_text) into rade_n_eoo_bits() floats and
// hands them to librade's rade_tx_set_eoo_bits, which rade_tx_eoo then
// sends; FreeDV does the same (freedv-gui src/freedv_interface.cpp:697-711
// [@a4ae053]). RX: processRxEooBits decodes the EOO data rade_rx returns
// and emits textDecoded(callsign) for a non-empty callsign whose CRC
// matched, as freedv-backend's RADEReceiveStep hands them to rade_text_rx.
//
// Until 2026-09-28 this wrapper used librade's raw 7-bit ASCII helpers
// (rade_tx_set_eoo_callsign / rade_rx_get_eoo_callsign), which have no
// FEC or CRC, which FreeDV never used, and which upstream rade_c removed
// on 2026-07-10 (e57958f); a FreeDV station could not read a NereusSDR
// callsign, nor the reverse.
//
// The underlying RADE library is BSD-2-Clause licensed (see
// third_party/rade/LICENSE; Copyright (C) 2026 Peter B Marks).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I4. NereusSDR-native
//                 wrapper around the third_party/rade library's
//                 callsign-over-EOO channel. Replaces the 18-line I1
//                 forward stub. The Phase 3R review (logged at the
//                 commit message and at
//                 docs/attribution/aethersdr-reconciliation.md Phase 3R
//                 Task I4) concluded that the original plan to port
//                 freedv-gui's rade_text.c verbatim was not workable
//                 because that source pulls in roughly 1500 lines of
//                 codec2 dependencies absent from NereusSDR's tree;
//                 the vendored third_party/rade library already
//                 exposes a working callsign-over-EOO surface with no
//                 extra dependencies, so the wrapper sits on that.
//                 Public API (setOurCallsign / ourCallsign /
//                 pushTxCallsign / processRxEooBits / textDecoded
//                 signal) is NereusSDR-native shape with no upstream
//                 counterpart. Wire-up into RadeChannel's processIq /
//                 txEncode paths is deferred to Phase L per the plan.
//                 AI tooling: Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  RADE end-of-over callsigns: FreeDV's
//                 format through RadeTextCodec instead of librade's raw
//                 ASCII helpers, so NereusSDR and FreeDV stations decode
//                 each other. An empty callsign now clears the EOO data
//                 (FreeDV sends zeros when it has no callsign to send);
//                 textDecoded fires only for a non-empty callsign.
//                 AI tooling: Anthropic Claude Code.
// =================================================================

#pragma once

#include <QObject>
#include <QString>

// Forward declaration so RadeText.h does not pull in rade_api.h (and
// librade's internals) at every callsite.
struct rade;

namespace NereusSDR {

class RadeText : public QObject {
    Q_OBJECT

public:
    explicit RadeText(QObject* parent = nullptr);
    ~RadeText() override;

    // The callsign pushTxCallsign sends. Stored as given; the codec
    // upper-cases it and drops characters outside FreeDV's set.
    void setOurCallsign(const QString& callsign);
    QString ourCallsign() const;

    // Write ourCallsign() into the active rade's EOO data, for the next
    // rade_tx_eoo. No-op when rade is null. An empty callsign writes zeros,
    // what librade holds before anything is set and what FreeDV sends with
    // no callsign.
    void pushTxCallsign(struct rade* rade);

    // Decode the EOO data rade_rx returned with has_eoo_out set. nBits is
    // rade_n_eoo_bits(rade) (floats). Emits textDecoded(callsign) when
    // FreeDV's decoder would accept it and the callsign is not empty.
    void processRxEooBits(const float* eooBits, int nBits);

signals:
    void textDecoded(const QString& callsign);

private:
    QString m_ourCallsign;
};

}  // namespace NereusSDR
