// no-port-check: NereusSDR-original. See CwSoftPaddle.h.
// =================================================================
// src/core/CwSoftPaddle.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-11-05: Hot keys plan Task 5 (R-HK-13). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/CwSoftPaddle.h"

#include "core/CwKeyLine.h"

namespace NereusSDR {

CwSoftPaddle::CwSoftPaddle(QObject* parent)
    : QObject(parent)
{
}

CwSoftPaddle::~CwSoftPaddle()
{
    releaseKeys();
}

void CwSoftPaddle::setLine(CwKeyLine* line)
{
    if (line == m_line) {
        return;
    }
    releaseKeys();
    m_line = line;
}

void CwSoftPaddle::setGateAllowed(bool allowed, const TxRefusal& refusal)
{
    m_gateRefusal = refusal;
    const bool wasAllowed = m_gateAllowed;
    m_gateAllowed = allowed;
    if (allowed) {
        return;
    }
    const bool wasKeying = isKeying();
    releaseKeys();
    if (wasAllowed && wasKeying && !refusal.isEmpty()) {
        emit keyRefused(refusal);
    }
}

void CwSoftPaddle::setDot(bool down)
{
    setKey(m_dot, &CwKeyLine::setSoftDot, down);
}

void CwSoftPaddle::setDash(bool down)
{
    setKey(m_dash, &CwKeyLine::setSoftDash, down);
}

void CwSoftPaddle::releaseKeys()
{
    releaseKey(m_dot, &CwKeyLine::setSoftDot);
    releaseKey(m_dash, &CwKeyLine::setSoftDash);
}

void CwSoftPaddle::setKey(Key& key, LineSetter setter, bool down)
{
    if (!down) {
        key.blocked = false;
        if (key.on) {
            key.on = false;
            if (m_line != nullptr) {
                (m_line->*setter)(false);
            }
        }
        return;
    }
    if (key.blocked || key.on) {
        return;
    }
    if (!m_gateAllowed) {
        key.blocked = true;
        if (!m_gateRefusal.isEmpty()) {
            emit keyRefused(m_gateRefusal);
        }
        return;
    }
    if (m_line == nullptr) {
        key.blocked = true;
        return;
    }
    key.on = true;
    (m_line->*setter)(true);
}

void CwSoftPaddle::releaseKey(Key& key, LineSetter setter)
{
    if (!key.on) {
        return;
    }
    key.on = false;
    key.blocked = true;
    if (m_line != nullptr) {
        (m_line->*setter)(false);
    }
}

} // namespace NereusSDR
