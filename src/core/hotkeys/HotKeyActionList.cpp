// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-07 - J.J. Boyd (KG4VCF), hot keys: VOX on or off (vox.toggle).
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "core/hotkeys/HotKeyActionList.h"

#include <QHash>

namespace NereusSDR {

namespace {

QList<HotKeyAction> buildActions()
{
    using G = HotKeyGesture;
    using S = HotKeyScope;

    const QString tuning = QStringLiteral("Tuning");
    const QString filter = QStringLiteral("Filter");
    const QString audio = QStringLiteral("Audio");
    const QString bandMode = QStringLiteral("Band and mode");
    const QString display = QStringLiteral("Display");
    const QString dsp = QStringLiteral("DSP");
    const QString transmit = QStringLiteral("Transmit");
    const QString cw = QStringLiteral("CW");
    const QString voice = QStringLiteral("Voice");

    QList<HotKeyAction> a;
    const auto add = [&a](const QString& id, const QString& name, const QString& group,
                          HotKeyGesture gesture, bool transmits, HotKeyScope scope,
                          const QStringList& keys) {
        a.append(HotKeyAction{id, name, group, gesture, transmits, scope, keys});
    };

    add(QStringLiteral("tune.up"), QStringLiteral("Tune up one step"), tuning, G::Turn, false, S::Slice, {QStringLiteral("Right")});
    add(QStringLiteral("tune.down"), QStringLiteral("Tune down one step"), tuning, G::Turn, false, S::Slice, {QStringLiteral("Left")});
    add(QStringLiteral("tune.up10"), QStringLiteral("Tune up ten steps"), tuning, G::Turn, false, S::Slice, {QStringLiteral("Shift+Right")});
    add(QStringLiteral("tune.down10"), QStringLiteral("Tune down ten steps"), tuning, G::Turn, false, S::Slice, {QStringLiteral("Shift+Left")});
    add(QStringLiteral("step.larger"), QStringLiteral("Tune step larger"), tuning, G::Press, false, S::Slice, {QStringLiteral("]")});
    add(QStringLiteral("step.smaller"), QStringLiteral("Tune step smaller"), tuning, G::Press, false, S::Slice, {QStringLiteral("[")});
    add(QStringLiteral("slice.lock"), QStringLiteral("Lock"), tuning, G::Press, false, S::Slice, {QStringLiteral("L")});
    add(QStringLiteral("tune.goto"), QStringLiteral("Go to frequency"), tuning, G::Press, false, S::Slice, {QStringLiteral("G")});
    add(QStringLiteral("slice.next"), QStringLiteral("Next slice"), tuning, G::Press, false, S::Window, {QStringLiteral("`")});

    add(QStringLiteral("filter.next"), QStringLiteral("Filter next"), filter, G::Press, false, S::Slice, {QStringLiteral("B")});
    add(QStringLiteral("filter.previous"), QStringLiteral("Filter previous"), filter, G::Press, false, S::Slice, {QStringLiteral("V")});

    add(QStringLiteral("volume.up"), QStringLiteral("Volume up"), audio, G::Turn, false, S::Slice, {QStringLiteral("Up"), QStringLiteral("Num++")});
    add(QStringLiteral("volume.down"), QStringLiteral("Volume down"), audio, G::Turn, false, S::Slice, {QStringLiteral("Down"), QStringLiteral("Num+-")});
    add(QStringLiteral("mute"), QStringLiteral("Mute"), audio, G::Press, false, S::Slice, {QStringLiteral("U"), QStringLiteral("Num+*")});

    add(QStringLiteral("bandstack.next"), QStringLiteral("Band stack next"), bandMode, G::Press, false, S::Window, {QStringLiteral("M")});
    add(QStringLiteral("bandstack.previous"), QStringLiteral("Band stack previous"), bandMode, G::Press, false, S::Window, {QStringLiteral("N")});
    add(QStringLiteral("band.next"), QStringLiteral("Next band"), bandMode, G::Press, false, S::Slice, {QStringLiteral("J")});
    add(QStringLiteral("band.previous"), QStringLiteral("Previous band"), bandMode, G::Press, false, S::Slice, {QStringLiteral("H")});
    add(QStringLiteral("bandstack.add"), QStringLiteral("Add band stack entry"), bandMode, G::Press, false, S::Window, {QStringLiteral("Shift+M")});
    add(QStringLiteral("bandstack.panel"), QStringLiteral("Open band stack panel"), bandMode, G::Press, false, S::Window, {QStringLiteral("Shift+N")});
    add(QStringLiteral("mode.next"), QStringLiteral("Mode next"), bandMode, G::Press, false, S::Slice, {QStringLiteral("X")});
    add(QStringLiteral("mode.previous"), QStringLiteral("Mode previous"), bandMode, G::Press, false, S::Slice, {QStringLiteral("Z")});

    add(QStringLiteral("zoom.in"), QStringLiteral("Zoom in"), display, G::Press, false, S::Pan, {QStringLiteral("=")});
    add(QStringLiteral("zoom.out"), QStringLiteral("Zoom out"), display, G::Press, false, S::Pan, {QStringLiteral("-")});

    add(QStringLiteral("nr.toggle"), QStringLiteral("Noise reduction"), dsp, G::Press, false, S::Slice, {QStringLiteral("R")});
    add(QStringLiteral("nb.toggle"), QStringLiteral("Noise blanker"), dsp, G::Press, false, S::Slice, {QStringLiteral("E")});
    add(QStringLiteral("anf.toggle"), QStringLiteral("Auto notch"), dsp, G::Press, false, S::Slice, {QStringLiteral("A")});
    add(QStringLiteral("rit.toggle"), QStringLiteral("RIT on or off"), dsp, G::Press, false, S::Slice, {QStringLiteral("I")});
    add(QStringLiteral("xit.toggle"), QStringLiteral("XIT on or off"), dsp, G::Press, false, S::Slice, {QStringLiteral("O")});

    add(QStringLiteral("tx.hold"), QStringLiteral("Hold to talk"), transmit, G::Hold, true, S::TransmitSlice, {QStringLiteral("Space")});
    add(QStringLiteral("tx.mox"), QStringLiteral("MOX"), transmit, G::Press, true, S::TransmitSlice, {QStringLiteral("Ctrl+Shift+M")});
    add(QStringLiteral("tx.tune"), QStringLiteral("TUNE"), transmit, G::Press, true, S::TransmitSlice, {QStringLiteral("Ctrl+T")});
    add(QStringLiteral("tx.twotone"), QStringLiteral("Two-tone"), transmit, G::Press, true, S::TransmitSlice, {QStringLiteral("Ctrl+Shift+T")});
    // The TX applet's VOX button. It lets the voice key the radio, so it takes the
    // transmit rules. It has no default key.
    add(QStringLiteral("vox.toggle"), QStringLiteral("VOX on or off"), transmit, G::Press, true, S::TransmitSlice, {});

    for (int n = 1; n <= 8; ++n) {
        add(QStringLiteral("cw.msg%1").arg(n), QStringLiteral("CW message %1").arg(n), cw, G::Press, true,
            S::TransmitSlice, {QStringLiteral("Ctrl+F%1").arg(n)});
    }
    add(QStringLiteral("cw.stop"), QStringLiteral("Stop CW message"), cw, G::Press, false, S::TransmitSlice, {QStringLiteral("Ctrl+F10")});
    add(QStringLiteral("cw.dot"), QStringLiteral("CW dot"), cw, G::Hold, true, S::TransmitSlice, {QStringLiteral(",")});
    add(QStringLiteral("cw.dash"), QStringLiteral("CW dash"), cw, G::Hold, true, S::TransmitSlice, {QStringLiteral(".")});
    add(QStringLiteral("cw.straight"), QStringLiteral("CW straight key"), cw, G::Hold, true, S::TransmitSlice, {QStringLiteral("/")});

    for (int n = 1; n <= 4; ++n) {
        add(QStringLiteral("voice.msg%1").arg(n), QStringLiteral("Voice message %1").arg(n), voice, G::Press, true,
            S::TransmitSlice, {QStringLiteral("Ctrl+Shift+F%1").arg(n)});
    }
    add(QStringLiteral("voice.stop"), QStringLiteral("Stop voice message"), voice, G::Press, false, S::TransmitSlice, {QStringLiteral("Ctrl+Shift+F10")});

    return a;
}

} // namespace

const QList<HotKeyAction>& HotKeyActionList::all()
{
    static const QList<HotKeyAction> kActions = buildActions();
    return kActions;
}

const HotKeyAction* HotKeyActionList::find(const QString& id)
{
    static const QHash<QString, const HotKeyAction*> kById = [] {
        QHash<QString, const HotKeyAction*> byId;
        for (const HotKeyAction& action : all()) {
            byId.insert(action.id, &action);
        }
        return byId;
    }();
    return kById.value(id, nullptr);
}

} // namespace NereusSDR
