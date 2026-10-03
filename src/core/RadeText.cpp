// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-native Qt6 wrapper. See RadeText.h; the ported
// format lives in RadeTextCodec / RadeLdpc / RadeHra5656.
//
// =================================================================
// src/core/RadeText.cpp  (NereusSDR)
// =================================================================
//
// See RadeText.h.
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
//   2026-09-28  J.J. Boyd / KG4VCF  RADE end-of-over callsigns: FreeDV's
//                 format through RadeTextCodec. AI tooling: Anthropic
//                 Claude Code.
// =================================================================

#include "core/RadeText.h"

#include <QByteArray>
#include <QLoggingCategory>

#include <string>
#include <vector>

#include "core/RadeTextCodec.h"

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
    if (rade == nullptr) {
        return;
    }
    const int nBits = rade_n_eoo_bits(rade);
    if (nBits <= 0) {
        return;
    }
    std::vector<float> syms(static_cast<size_t>(nBits), 0.0f);
    if (!m_ourCallsign.isEmpty()) {
        // FreeDV passes its reporting callsign, cut to 8 characters, to
        // rade_text_generate_tx_string with rade_n_eoo_bits floats
        // (freedv-gui src/main.cpp:2648-2653 and
        // src/freedv_interface.cpp:703-708 [@a4ae053]).
        const QByteArray ascii = m_ourCallsign.toLatin1();
        radetext::generateTxString(ascii.constData(), static_cast<int>(ascii.size()),
                                   syms.data(), nBits);
    }
    rade_tx_set_eoo_bits(rade, syms.data());
}

void RadeText::processRxEooBits(const float* eooBits, int nBits)
{
    if (eooBits == nullptr || nBits < radetext::kLdpcTotalSizeBits) {
        return;
    }
    // freedv-backend RADEReceiveStep passes rade_n_eoo_bits / 2 symbols
    // (src/pipeline/RADEReceiveStep.cpp:239 [@f02e7e9]).
    std::string text;
    if (!radetext::decodeRx(eooBits, nBits / 2, &text) || text.empty()) {
        return;
    }
    const QString callsign = QString::fromLatin1(text.data(), static_cast<int>(text.size()));
    qCInfo(lcRadeText) << "RADE end-of-over callsign decoded:" << callsign;
    emit textDecoded(callsign);
}

}  // namespace NereusSDR
