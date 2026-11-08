// =================================================================
// src/gui/hotkeys/devices/UlanziChordDecoder.cpp  (NereusSDR)
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

#include "gui/hotkeys/devices/UlanziChordDecoder.h"

#include <utility>

namespace NereusSDR {

namespace {

// Signature to pill: AetherSDR src/gui/UlanziDialMapperDialog.cpp:109-118 [@1e0718ad]
QString bareKeyControl(int keycode)
{
    switch (keycode) {
    case UlanziKey::PlayPause:
        return QStringLiteral("topMiddle");
    case UlanziKey::Mute:
        return QStringLiteral("press");
    case UlanziKey::PreviousSong:
        return QStringLiteral("topLeft");
    case UlanziKey::NextSong:
        return QStringLiteral("topRight");
    default:
        return {};
    }
}

QString chordControl(int keycode, bool withPreviousSong)
{
    // Mode Cycle has no control. AetherSDR src/gui/UlanziDialMapperDialog.cpp:88-97 [@1e0718ad]
    if (withPreviousSong) {
        return {};
    }
    switch (keycode) {
    case UlanziKey::V:
        return QStringLiteral("leftTop");
    case UlanziKey::C:
        return QStringLiteral("leftBottom");
    case UlanziKey::Y:
        return QStringLiteral("rightTop");
    case UlanziKey::Z:
        return QStringLiteral("rightBottom");
    default:
        return {};
    }
}

DeviceEvent buttonEvent(const QString& control, bool down)
{
    DeviceEvent event;
    event.kind = down ? DeviceEvent::Kind::ButtonDown : DeviceEvent::Kind::ButtonUp;
    event.control = control;
    return event;
}

} // namespace

std::optional<int> UlanziChordDecoder::keyCodeFromHidUsage(quint8 page, quint16 usage)
{
    // From AetherSDR src/core/UlanziDialMacOSManager.cpp:20-44 [@1e0718ad]
    if (page == 0x0C) {
        switch (usage) {
        case 0xCD:
            return UlanziKey::PlayPause;
        case 0xE2:
            return UlanziKey::Mute;
        case 0xB5:
            return UlanziKey::NextSong;
        case 0xB6:
            return UlanziKey::PreviousSong;
        case 0xE9:
            return UlanziKey::VolumeUp;
        case 0xEA:
            return UlanziKey::VolumeDown;
        default:
            return std::nullopt;
        }
    }
    if (page == 0x07) {
        switch (usage) {
        case 0x06:
            return UlanziKey::C;
        case 0x19:
            return UlanziKey::V;
        case 0x1C:
            return UlanziKey::Y;
        case 0x1D:
            return UlanziKey::Z;
        case 0xE0:
            return UlanziKey::LeftCtrl;
        case 0xE4:
            return UlanziKey::RightCtrl;
        default:
            return std::nullopt;
        }
    }
    return std::nullopt;
}

QList<DeviceEvent> UlanziChordDecoder::reset()
{
    QList<DeviceEvent> released;
    for (const QString& control : std::as_const(m_held)) {
        released.append(buttonEvent(control, false));
    }
    m_held.clear();
    m_ctrlDown = false;
    m_lastNonModKey = -1;
    m_prevsongAlongsideCtrl = false;
    m_suppressReleaseNonModKey = -1;
    m_suppressReleasePrevsong = false;
    m_chordControl.clear();
    return released;
}

void UlanziChordDecoder::button(QList<DeviceEvent>& out, const QString& control, int action)
{
    if (control.isEmpty()) {
        return;
    }
    if (action == 1) {
        // A second press with no release between runs nothing twice.
        if (m_held.contains(control)) {
            return;
        }
        m_held.append(control);
    } else {
        m_held.removeAll(control);
    }
    out.append(buttonEvent(control, action == 1));
}

// A bare press whose key lifts inside a chord window would otherwise never let go.
// Upstream loses this release.
void UlanziChordDecoder::releaseIfBareHeld(QList<DeviceEvent>& out, int linuxKeyCode)
{
    const QString bare = bareKeyControl(linuxKeyCode);
    if (m_held.contains(bare)) {
        button(out, bare, 0);
    }
}

QList<DeviceEvent> UlanziChordDecoder::feed(int linuxKeyCode, int value)
{
    QList<DeviceEvent> out;
    const auto tune = [&out](int steps) {
        DeviceEvent event;
        event.kind = DeviceEvent::Kind::Rotate;
        event.control = QStringLiteral("wheel");
        event.value = steps;
        out.append(event);
    };

    // Rotary: one tick per press or autorepeat.  The ~30 Hz autorepeat rate
    // from the dial firmware gives a natural acceleration feel under
    // continuous rotation.  (Backends that poll HID never see autorepeat and
    // synthesise one press per detent instead — same result.)
    if (linuxKeyCode == UlanziKey::VolumeUp) {
        if (value == 1 || value == 2) {
            tune(+1);
        }
        return out;
    }
    if (linuxKeyCode == UlanziKey::VolumeDown) {
        if (value == 1 || value == 2) {
            tune(-1);
        }
        return out;
    }

    if (linuxKeyCode == UlanziKey::LeftCtrl || linuxKeyCode == UlanziKey::RightCtrl) {
        m_ctrlDown = (value != 0);
        if (!m_ctrlDown) {
            // Ctrl released — chord window closing.  A chord key still held
            // gets its release now, so listeners never miss it when Ctrl comes
            // up before the non-mod key.  Its physical key-up follows as a
            // *bare* event and is swallowed by the latch below.
            if (m_lastNonModKey != -1) {
                button(out, m_chordControl, 0);
            }
            m_lastNonModKey = -1;
            m_prevsongAlongsideCtrl = false;
            m_chordControl.clear();
        }
        return out;
    }

    // PREVIOUSSONG can arrive alongside a Ctrl chord (Mode Cycle emits
    // Ctrl+Y+KEY_PREVIOUSSONG as a compound), so inside a chord window it only
    // decorates the signature rather than acting as a key of its own.
    if (linuxKeyCode == UlanziKey::PreviousSong) {
        if (m_ctrlDown) {
            if (value == 1) {
                m_prevsongAlongsideCtrl = true;
                m_suppressReleasePrevsong = true;
            } else if (value == 0) {
                m_suppressReleasePrevsong = false;
                releaseIfBareHeld(out, linuxKeyCode);
            }
            return out;
        }
        if (m_suppressReleasePrevsong) {
            if (value == 0) {
                m_suppressReleasePrevsong = false;
                return out;   // trailing release from the chord window
            }
            if (value == 2) {
                return out;   // still held; autorepeat is not a fresh press
            }
            // A fresh press means the key-up was lost. This press keeps its release.
            m_suppressReleasePrevsong = false;
        }
        if (value == 1 || value == 0) {
            button(out, bareKeyControl(linuxKeyCode), value);
        }
        return out;
    }

    // Bare key press/release (no Ctrl).
    if (!m_ctrlDown) {
        if (linuxKeyCode == m_suppressReleaseNonModKey) {
            if (value == 0) {
                m_suppressReleaseNonModKey = -1;
                return out;   // trailing release from the chord window
            }
            if (value == 2) {
                return out;   // still held; autorepeat is not a fresh press
            }
            // A fresh press means the key-up was lost. This press keeps its release.
            m_suppressReleaseNonModKey = -1;
        }
        if (value == 1 || value == 0) {
            button(out, bareKeyControl(linuxKeyCode), value);
        }
        return out;
    }

    // Ctrl is down — this is a chord.
    if (value == 1) {
        // A second chord key pressed while the first is still held: close the
        // first one out now, so its press does not go unreleased.
        if (m_lastNonModKey != -1 && m_lastNonModKey != linuxKeyCode) {
            button(out, m_chordControl, 0);
        }
        if (m_lastNonModKey != linuxKeyCode) {
            m_lastNonModKey = linuxKeyCode;
            m_chordControl = chordControl(linuxKeyCode, m_prevsongAlongsideCtrl);
            button(out, m_chordControl, 1);
        }
        m_suppressReleaseNonModKey = linuxKeyCode;
    } else if (value == 0) {
        if (linuxKeyCode == m_lastNonModKey) {
            button(out, m_chordControl, 0);
            m_lastNonModKey = -1;
            m_prevsongAlongsideCtrl = false;
            m_chordControl.clear();
        } else {
            releaseIfBareHeld(out, linuxKeyCode);
        }
        // Disarm the latch whichever branch consumed the key-up.  Without
        // this, a Ctrl bounce (Ctrl up, Ctrl down again, key up) leaves it
        // armed and it swallows the release of an unrelated later bare press.
        if (linuxKeyCode == m_suppressReleaseNonModKey) {
            m_suppressReleaseNonModKey = -1;
        }
    }
    return out;
}

} // namespace NereusSDR
