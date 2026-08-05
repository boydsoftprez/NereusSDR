// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-native file. See RadeText.h for the full
// rationale for using third_party/rade's native EOO callsign API
// instead of porting freedv-gui's rade_text.c.
//
// =================================================================
// src/core/RadeText.cpp  (NereusSDR)
// =================================================================
//
// See RadeText.h for the upstream-license posture and the rationale
// for using third_party/rade's native EOO callsign API instead of
// porting freedv-gui's rade_text.c verbatim.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I4. Initial
//                 implementation. NereusSDR-native wrapper around
//                 the third_party/rade callsign-over-EOO API
//                 (rade_tx_set_eoo_callsign / rade_rx_get_eoo_callsign,
//                 declared in third_party/rade/src/rade_api.h:120-145
//                 [@b289102]; implemented in
//                 third_party/rade/src/rade_api_nopy.c:159-201
//                 [@b289102]). AI tooling: Anthropic Claude Code.
// =================================================================

#include "core/RadeText.h"

#include <QByteArray>
#include <QLoggingCategory>

extern "C" {
#include "rade_api.h"
}

Q_LOGGING_CATEGORY(lcRadeText, "nereus.rade.text")

namespace NereusSDR {

RadeText::RadeText(QObject* parent)
    : QObject(parent)
{
}

RadeText::~RadeText() = default;

void RadeText::setOurCallsign(const QString& callsign)
{
    m_ourCallsign = callsign;
}

QString RadeText::ourCallsign() const
{
    return m_ourCallsign;
}

void RadeText::pushTxCallsign(struct rade* rade)
{
    // No-op guards: a null rade pointer (RadeChannel's m_rade is null
    // between stop() and the next start()) and an empty callsign
    // stash. The empty-callsign guard is load-bearing: without it we
    // would feed an empty C string into rade_tx_set_eoo_callsign,
    // which (per rade_api_nopy.c:164-172 [@b289102]) pads with spaces
    // and writes 0x20 = 0100000 across all RADE_EOO_CALLSIGN_MAX*7
    // bits. That would silently clobber any callsign the caller had
    // previously set into the EOO buffer.
    if (!rade || m_ourCallsign.isEmpty()) {
        return;
    }

    // The wire convention is upper-case ASCII; the underlying API at
    // rade_api_nopy.c:159-173 [@b289102] does not transform the input,
    // so we upper-case here. Truncate to RADE_EOO_CALLSIGN_MAX
    // characters (the API at :162 asserts the EOO bit budget is at
    // least RADE_EOO_CALLSIGN_MAX*7 bits but only writes those bits;
    // bytes beyond the limit are discarded by strlen-bounded :164-165).
    const QByteArray ascii = m_ourCallsign.left(RADE_EOO_CALLSIGN_MAX)
                                 .toUpper()
                                 .toLatin1();
    rade_tx_set_eoo_callsign(rade, ascii.constData());
}

void RadeText::processRxEooBits(const float* eooBits, int nBits)
{
    // Same no-op guards. A null bit buffer or a too-short buffer
    // (less than RADE_EOO_CALLSIGN_MAX*7 = 56 bits) cannot encode a
    // callsign; the underlying API at rade_api_nopy.c:180-183
    // [@b289102] returns 0 and writes an empty string in that case,
    // but we short-circuit here to skip the function call entirely.
    if (!eooBits || nBits < RADE_EOO_CALLSIGN_MAX * 7) {
        return;
    }

    char buf[RADE_EOO_CALLSIGN_MAX + 1] = {};
    const int decodedLen = rade_rx_get_eoo_callsign(eooBits, nBits, buf);
    if (decodedLen <= 0) {
        return;
    }

    const QString callsign = QString::fromLatin1(buf, decodedLen);
    if (callsign.isEmpty()) {
        return;
    }

    emit textDecoded(callsign);
}

}  // namespace NereusSDR
