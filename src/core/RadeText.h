// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-native file. The Phase 3R Task I4 review
// concluded the realistic implementation is a thin Qt6 wrapper around
// the already-vendored third_party/rade callsign-over-EOO API
// (rade_tx_set_eoo_callsign / rade_rx_get_eoo_callsign at
// third_party/rade/src/rade_api.h:120-145 [@b289102]), not a port of
// freedv-gui's rade_text.c (which transitively pulls in ~1500 lines
// of codec2 dependencies absent from NereusSDR's tree). The mentions
// of freedv-gui below are deviation-rationale prose, not source-port
// claims; see docs/attribution/aethersdr-reconciliation.md Phase 3R
// Task I4 for the full discussion.
//
// =================================================================
// src/core/RadeText.h  (NereusSDR)
// =================================================================
//
// NereusSDR - RadeText: thin Qt6 wrapper around the third_party/rade
// library's native callsign-over-EOO channel.
//
// This file is NereusSDR-native code. There is no upstream port: the
// original Phase 3R plan called for a verbatim port of freedv-gui
// `src/pipeline/rade_text.{h,c}`, but that source pulls in roughly
// 1500 lines of codec2 dependencies (gp_interleaver, ldpc_codes,
// mpdecode_core, ofdm_internal, ulog) that are not in the NereusSDR
// tree. The already-vendored RADE library exposes a working
// callsign-over-EOO channel via
// `rade_tx_set_eoo_callsign` / `rade_rx_get_eoo_callsign`
// (third_party/rade/src/rade_api.h:120-145 [@b289102]) with no
// additional dependencies, so RadeText wraps that surface instead.
//
// Trade-offs vs the freedv-gui rade_text.c that the plan originally
// targeted:
//   * No LDPC FEC: the RADE EOO channel is bare 7-bit ASCII bits, so
//     it is less robust at low SNR. The freedv-gui channel runs an
//     LDPC code over the bits.
//   * No CRC: per-message integrity is a future enhancement.
//   * No grid square: the RADE EOO channel is a flat 7-bit ASCII path
//     with at most RADE_EOO_CALLSIGN_MAX (8) characters; a grid square
//     would need a separate format-design pass over the leftover EOO
//     bits and is deferred to a future task.
//
// The underlying RADE library is BSD-2-Clause licensed (see
// third_party/rade/LICENSE; Copyright (C) 2026 Peter B Marks).
// BSD-2-Clause is GPL-compatible; NereusSDR ships under GPLv3.
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
// =================================================================

#pragma once

#include <QObject>
#include <QString>

// Forward declaration so RadeText.h does not need to pull in
// rade_api.h transitively at every callsite. The .cpp resolves
// `struct rade` via `#include "rade_api.h"`.
struct rade;

namespace NereusSDR {

// Thin Qt6 wrapper around third_party/rade's native callsign-over-EOO
// channel (rade_tx_set_eoo_callsign / rade_rx_get_eoo_callsign, declared
// in third_party/rade/src/rade_api.h:120-145 [@b289102]). Owns no rade
// state of its own; instead, callers pass in the active
// `struct rade*` from RadeChannel when calling encode / decode.
//
// The text channel is one-shot per "over": before the TX over starts,
// caller invokes setOurCallsign() to stash the local callsign, then
// pushTxCallsign(rade) to encode it into the EOO bit-buffer that
// `rade_tx_eoo` will emit at end-of-over. On RX, when `rade_rx`
// returns `has_eoo_out=true`, caller hands the eoo_out array to
// processRxEooBits(eooBits, nBits) which decodes and emits
// textDecoded(callsign).
//
// Grid square is NOT supported here. The RADE EOO channel is a flat
// 7-bit ASCII path with ~8 chars; a grid-square channel would need a
// separate format-design pass over the leftover EOO bits, deferred to
// a future task.
class RadeText : public QObject {
    Q_OBJECT

public:
    explicit RadeText(QObject* parent = nullptr);
    ~RadeText() override;

    // Stash the local callsign for subsequent pushTxCallsign() calls.
    // Storage is verbatim (case-preserving); pushTxCallsign upper-cases
    // on the way to the wire to match the conventional callsign form
    // and to be defensive against mixed-case user input.
    void setOurCallsign(const QString& callsign);
    QString ourCallsign() const;

    // Stuff the stashed callsign into the active rade's EOO bit
    // buffer. Call once at TX-start (MOX on) per over. No-op if rade
    // is null or ourCallsign() is empty. The callsign is truncated to
    // RADE_EOO_CALLSIGN_MAX characters and upper-cased before being
    // passed to rade_tx_set_eoo_callsign.
    void pushTxCallsign(struct rade* rade);

    // Decode incoming EOO bits. Call from RadeChannel::processIq when
    // rade_rx returns has_eoo_out=true; pass eoo_out and the value of
    // rade_n_eoo_bits(rade). Emits textDecoded(callsign) if a valid
    // (non-empty after trimming) callsign is recovered.
    void processRxEooBits(const float* eooBits, int nBits);

signals:
    // Fired when processRxEooBits successfully recovers a non-empty
    // callsign. The argument is the decoded callsign with trailing
    // padding spaces removed (the underlying rade_rx_get_eoo_callsign
    // pads to RADE_EOO_CALLSIGN_MAX chars with spaces and trims them
    // on the way out; see rade_api_nopy.c:195-200 [@b289102]).
    void textDecoded(const QString& callsign);

private:
    QString m_ourCallsign;
};

}  // namespace NereusSDR
