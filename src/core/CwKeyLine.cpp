// no-port-check: NereusSDR-original. See CwKeyLine.h.
// =================================================================
// src/core/CwKeyLine.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the host key line (R-CW-12, R-CW-15,
//               R-CW-16). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include "core/CwKeyLine.h"

#include <utility>

namespace NereusSDR {

void CwKeyLine::setCwx(bool down)   { setBit(kCwxBit, down); }
void CwKeyLine::setDot(bool down)   { setBit(kDotBit, down); }
void CwKeyLine::setDash(bool down)  { setBit(kDashBit, down); }
void CwKeyLine::setCwxPtt(bool on)  { setBit(kCwxPttBit, on); }

bool CwKeyLine::cwx() const    { return (m_bits.load(std::memory_order_acquire) & kCwxBit) != 0; }
bool CwKeyLine::dot() const    { return (m_bits.load(std::memory_order_acquire) & kDotBit) != 0; }
bool CwKeyLine::dash() const   { return (m_bits.load(std::memory_order_acquire) & kDashBit) != 0; }
bool CwKeyLine::cwxPtt() const { return (m_bits.load(std::memory_order_acquire) & kCwxPttBit) != 0; }

quint8 CwKeyLine::p1Bits(bool hl2) const
{
    return static_cast<quint8>(m_bits.load(std::memory_order_acquire) & (hl2 ? kHl2Mask : kP1Mask));
}

void CwKeyLine::setOnChange(std::function<void()> fn)
{
    m_onChange = std::move(fn);
}

void CwKeyLine::setBit(quint8 bit, bool on)
{
    // fetch_or / fetch_and change one bit without a read-modify-write
    // race, so two writers driving different bits never lose one.
    const quint8 before = on ? m_bits.fetch_or(bit, std::memory_order_acq_rel)
                             : m_bits.fetch_and(static_cast<quint8>(~bit), std::memory_order_acq_rel);
    const bool changed = ((before & bit) != 0) != on;
    if (changed && m_onChange) {
        m_onChange();
    }
}

} // namespace NereusSDR
