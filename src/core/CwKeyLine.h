// no-port-check: NereusSDR-original. The host's CW key state (CWX, dot,
// dash and the HL2 CWX PTT) as one atomic that any thread may write and the
// protocol send threads read. The bit layouts are facts cited at p1Bits; no
// upstream logic is ported here.
// =================================================================
// src/core/CwKeyLine.h  (NereusSDR)
// =================================================================
//
// The key bits the host drives (R-CW-12, R-CW-15, R-CW-16). The CWX
// sender, the TCI key and the serial key reader write them; each
// RadioConnection owns one (RadioConnection::cwKeyLine()). Protocol 1
// reads p1Bits() on its EP2 sender thread for every frame; Protocol 2
// sends its high-priority packet on a change, posted by the change
// callback.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the host key line (R-CW-12, R-CW-15,
//               R-CW-16). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#pragma once

#include <QtGlobal>

#include <atomic>
#include <functional>

#include "core/NereusCoreExport.h"

namespace NereusSDR {

class NEREUS_CORE_EXPORT CwKeyLine {
public:
    void setCwx(bool down);
    void setDot(bool down);
    void setDash(bool down);
    void setCwxPtt(bool on);

    bool cwx() const;
    bool dot() const;
    bool dash() const;
    bool cwxPtt() const;

    // P1 order: dot<<2 | dash<<1 | cwx; HL2 adds cwxPtt<<3.
    // From Thetis networkproto1.c:738-741 [v2.10.3.15]; mi0bot networkproto1.c:1247-1258 [v2.10.3.13-beta2]
    quint8 p1Bits(bool hl2) const;

    // Called on the writer's thread after any change. Must not block.
    // Set once, before any writer starts; a setter that changes nothing
    // does not call it.
    void setOnChange(std::function<void()> fn);

private:
    // The stored layout is the P1 one, so p1Bits is a mask.
    static constexpr quint8 kCwxBit    = 0x01;
    static constexpr quint8 kDashBit   = 0x02;
    static constexpr quint8 kDotBit    = 0x04;
    static constexpr quint8 kCwxPttBit = 0x08;   // HL2 only (MI0BOT)
    // From Thetis networkproto1.c:741 [v2.10.3.15] (& 0b00000111) and
    // mi0bot networkproto1.c:1252 [v2.10.3.13-beta2] (& 0b00001111).
    static constexpr quint8 kP1Mask    = 0x07;
    static constexpr quint8 kHl2Mask   = 0x0F;

    void setBit(quint8 bit, bool on);

    std::atomic<quint8> m_bits{0};
    std::function<void()> m_onChange;   // set once before any writer starts
};

} // namespace NereusSDR
