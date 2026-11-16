// no-port-check: NereusSDR-original. Keyboard, MIDI and deck dot and dash on
// the host key line's soft bits, with the same gate the serial reader asks.
// =================================================================
// src/core/CwSoftPaddle.h  (NereusSDR)
// =================================================================
//
// R-HK-13. Comma and period (or any source bound to cw.dot and cw.dash) drive
// CwKeyLine's soft dot and dash, so the radio's keyer does the timing. It
// follows CwSerialKeyInput::setPaddleLocked: a press the gate refuses blocks
// that key until it comes up and goes down again, a gate going false releases
// what is down, and a stop releases everything. Main thread only; the line it
// writes is atomic, read by the protocol threads.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05: Hot keys plan Task 5 (R-HK-13). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QObject>

#include "core/NereusCoreExport.h"
#include "core/safety/TxRefusal.h"

namespace NereusSDR {

class CwKeyLine;

class NEREUS_CORE_EXPORT CwSoftPaddle : public QObject {
    Q_OBJECT

public:
    explicit CwSoftPaddle(QObject* parent = nullptr);
    ~CwSoftPaddle() override;   // releases what is down

    // `line` must outlive this, or be replaced first. A new line lets go of
    // the old one; a key held across the change keys nothing until it comes up
    // and goes down again.
    void setLine(CwKeyLine* line);
    void setGateAllowed(bool allowed, const TxRefusal& refusal = {});
    void setDot(bool down);
    void setDash(bool down);
    // Both keys up on the line. A key still held keys nothing until it comes
    // up and goes down again.
    void releaseKeys();
    bool isKeying() const { return m_dot.on || m_dash.on; }

signals:
    void keyRefused(const NereusSDR::TxRefusal& refusal);   // once per press

private:
    struct Key {
        bool on{false};
        bool blocked{false};
    };
    using LineSetter = void (CwKeyLine::*)(bool);

    void setKey(Key& key, LineSetter setter, bool down);
    void releaseKey(Key& key, LineSetter setter);

    CwKeyLine* m_line{nullptr};
    bool m_gateAllowed{false};
    TxRefusal m_gateRefusal;
    Key m_dot;
    Key m_dash;
};

} // namespace NereusSDR
