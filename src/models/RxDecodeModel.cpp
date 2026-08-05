// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - RxDecodeModel: bounded ring buffer of recent local
// decodes from MY radio's receivers (rade_text + WSJT-X UDP).
//
// NEW NereusSDR-native model. No upstream equivalent. Distinguishes
// "what my radio just heard" from "what spots are flowing in from
// cluster/network sources" (which live in SpotModel).
//
// Modification history (NereusSDR)
//   Created 2026-05-11 by JJ Boyd / KG4VCF
//   AI tooling: Claude Code

#include "RxDecodeModel.h"

namespace NereusSDR {

RxDecodeModel::RxDecodeModel(int maxSize, QObject* parent)
    : QObject(parent)
    , m_maxSize(maxSize > 0 ? maxSize : 1)
{
    m_decodes.reserve(m_maxSize);
}

void RxDecodeModel::addDecode(const RxDecode& decode)
{
    m_decodes.append(decode);
    // FIFO eviction: drop oldest when over capacity.
    while (m_decodes.size() > m_maxSize) {
        m_decodes.removeFirst();
    }
    emit decodeAdded(decode);
}

void RxDecodeModel::clear()
{
    m_decodes.clear();
    emit cleared();
}

}  // namespace NereusSDR
