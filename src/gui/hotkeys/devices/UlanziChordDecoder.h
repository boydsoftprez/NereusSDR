#pragma once

// =================================================================
// src/gui/hotkeys/devices/UlanziChordDecoder.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - Ulanzi Dial chord decoder ported from AetherSDR
//                 src/core/UlanziChordDecoder.h and .cpp [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 16. Returns DeviceEvents named for the controls, where upstream
//                 returns signatures. Differs where upstream can lose a release:
//                 a chord key lets go of what it pressed, not what the window
//                 now decorates; a bare key that lifts inside a chord window
//                 still lets go; a fresh press disarms the trailing-release
//                 latch; reset() hands back a release for all that is down.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <optional>

namespace NereusSDR {

// Linux keycode numeric values.  Every platform backend translates its native
// event source into this space before decoding, so the signatures the mapper
// dialog and MainWindow's dispatcher see are identical on Linux, macOS and
// Windows.  (Linux gets these from <linux/input.h>; the other two have no such
// header, which is why they are spelled out here rather than included.)
// [original inline comment from src/core/UlanziChordDecoder.h:8-12]
namespace UlanziKey {
// From AetherSDR src/core/UlanziChordDecoder.h:14-25 [@1e0718ad]
constexpr int PlayPause    = 164;
constexpr int Mute         = 113;
constexpr int PreviousSong = 165;
constexpr int NextSong     = 163;
constexpr int VolumeUp     = 115;
constexpr int VolumeDown   = 114;
constexpr int LeftCtrl     = 29;
constexpr int RightCtrl    = 97;
constexpr int V            = 47;
constexpr int C            = 46;
constexpr int Y            = 21;
constexpr int Z            = 44;
}  // namespace UlanziKey

// Pure decoder for the Ulanzi Dial's key stream — no Qt object, no device, no
// event loop, so it is unit-testable on every platform (unlike the three
// backends that drive it, each of which only compiles on its own OS).
//
// The dial encodes its rotary as autorepeating volume keys, its top row and
// dial press as bare media keys, and its side buttons as Ctrl+key chords —
// with Mode Cycle arriving as a Ctrl+Y+KEY_PREVIOUSSONG compound.  feed()
// turns one native key transition into zero or more semantic events.
//
// **Every button press must be matched by a release.**  A momentary binding
// (PTT hold, CW keying) keys the transmitter on press and un-keys on release,
// so a swallowed release strands TX — Constitution Principle VI.  Two rules
// keep that invariant under adversarial event ordering:
//
//   - a chord key still held when the chord window closes (Ctrl up, or a
//     second chord key pressed on top of it) is released right then;
//   - the "trailing bare release" latch, which suppresses the physical key-up
//     that follows a synthesised chord release, is disarmed by whichever
//     branch actually consumes that key-up — otherwise a Ctrl bounce leaves it
//     armed against an unrelated later press.
//
// The opposite error — an occasional *extra* release — is harmless: every
// momentary consumer ignores a release it never pressed.
// [original inline comment from src/core/UlanziChordDecoder.h:28-50]
//
// A key with no control here (a bare V, C, Y or Z, any other key in a chord window,
// and the Ctrl+Y+PreviousSong compound) gives no event at all.
class NEREUS_GUI_EXPORT UlanziChordDecoder {
public:
    // Feed one key transition.  `value` follows the evdev convention:
    // 0 = release, 1 = press, 2 = autorepeat.  Backends that synthesise
    // transitions from polled HID reports only ever pass 0 or 1.
    // [original inline comment from src/core/UlanziChordDecoder.h:61-63]
    QList<DeviceEvent> feed(int linuxKeyCode, int value);

    // Drop all chord state — call on disconnect so a key held across an
    // unplug cannot leak into the next session.
    // [original inline comment from src/core/UlanziChordDecoder.h:66-67]
    // Gives back a release for each control still down. A caller may ignore them.
    QList<DeviceEvent> reset();

    // The Linux keycode for one HID usage, on the consumer or keyboard page.
    static std::optional<int> keyCodeFromHidUsage(quint8 page, quint16 usage);

private:
    void button(QList<DeviceEvent>& out, const QString& control, int action);
    void releaseIfBareHeld(QList<DeviceEvent>& out, int linuxKeyCode);

    bool m_ctrlDown{false};
    int  m_lastNonModKey{-1};             // chord key currently held, if any
    bool m_prevsongAlongsideCtrl{false};  // KEY_PREVIOUSSONG seen in this chord window
    int  m_suppressReleaseNonModKey{-1};  // trailing bare release to swallow
    bool m_suppressReleasePrevsong{false};
    QString m_chordControl;               // what m_lastNonModKey pressed; empty for none
    QStringList m_held;                   // controls pressed and not yet released
};

} // namespace NereusSDR
