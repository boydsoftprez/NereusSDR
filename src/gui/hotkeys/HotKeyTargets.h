// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 2.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// What the hot key dispatcher calls. The real implementation reaches the same
// model and window entry points as the on-screen controls; tests use a fake.
#pragma once

#include <QChar>
#include <QString>
#include <QtGlobal>

namespace NereusSDR {

class SliceModel;

enum class HotKeySourceKind { Keyboard, Midi, Knob, Deck };

// One physical thing that can hold a key. instance tells apart two of a kind.
struct HotKeySource {
    HotKeySourceKind kind;
    QString instance;

    bool operator==(const HotKeySource& other) const
    {
        return kind == other.kind && instance == other.instance;
    }
};

class HotKeyTargets {
public:
    virtual ~HotKeyTargets() = default;

    virtual SliceModel* windowSlice() const = 0;
    virtual SliceModel* sliceByLetter(QChar letter) const = 0;
    virtual QChar letterOf(const SliceModel* slice) const = 0;
    virtual QString sliceRefusal(const SliceModel* slice) const = 0;   // empty when it may change
    virtual bool listened(const SliceModel* slice) const = 0;
    virtual bool tuneTo(SliceModel* slice, double hz) = 0;             // the flag's own path
    virtual void runSliceAction(const QString& id, SliceModel* slice, int steps) = 0;
    virtual void runWindowAction(const QString& id) = 0;
    virtual QString unavailableReason(const QString& id) const = 0;    // empty when available
    virtual bool transmitting() const = 0;                             // MOX, TUNE or two-tone on, or Core keyed
    virtual bool tuneOrTwoToneOn() const = 0;
    virtual bool keyHold(bool down) = 0;                               // true if the press was admitted
    virtual void runTransmitAction(const QString& id) = 0;             // tx.mox, tx.tune, tx.twotone
    virtual void runCwMessage(const QString& id) = 0;                  // cw.msgN, cw.stop
    virtual void setCwKeyDown(const QString& id, bool down) = 0;       // cw.dot, cw.dash, cw.straight
    virtual void toast(const QString& text) = 0;
    virtual qint64 nowMs() const = 0;
};

} // namespace NereusSDR
