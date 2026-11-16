#pragma once

// =================================================================
// src/gui/hotkeys/devices/UlanziWindowsInput.h  (NereusSDR)
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
//   2026-11-06 - Ulanzi Dial on Windows built on AetherSDR
//                 src/core/UlanziDialWindowsManager.h and .cpp [@1e0718ad] for
//                 NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 16. The device sits behind a seam so a test never opens one, and
//                 nothing is enumerated until the application allows it. Report
//                 decoding is a static function a Mac test can run. A refused open
//                 is reported on the Knobs page.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/UlanziDialInput.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

class QTimer;

namespace NereusSDR {

class UlanziWindowsLayer {
public:
    virtual ~UlanziWindowsLayer() = default;
    virtual QStringList findPaths() = 0;    // every interface of the dial; opens nothing
    // On failure, error gets the detail deviceOpenFailureReason reads.
    virtual std::unique_ptr<HidDeviceHandle> open(const QString& path, QString* error) = 0;
};

// Windows backend for the Ulanzi Dial using hidapi.  Mirrors the Linux
// evdev backend's Qt signal contract:
//   tuneSteps(int)               — rotary delta (+1 CW / -1 CCW)
//   buttonEvent(sig, action)     — see EvdevEncoderManager for signature
//   connectionChanged(bool, name)
//
// Implementation notes:
// - The dial enumerates as multiple HID interfaces on Windows: a Keyboard
//   page interface (Ctrl + chord keys), a Consumer Control page interface
//   (PLAYPAUSE / MUTE / NEXTSONG / PREVIOUSSONG), and possibly Mouse.
//   We open every matching device whose product_string contains
//   "Ulanzi Dial" and read reports from each via a poll timer.
// - Each report is diff-compared against the previous to detect press /
//   release transitions, then fed into the same chord-assembly state
//   machine the Linux backend uses.
// - No EVIOCGRAB-equivalent: keystrokes still go to the focused window
//   on Windows.  A follow-up could wire RawInput + RIDEV_NOLEGACY to
//   intercept globally; see #3232 for the design notes.
// [original inline comment from src/core/UlanziDialWindowsManager.h:16-33]
class NEREUS_GUI_EXPORT UlanziWindowsInput : public UlanziDialInput {   // lives on the USB worker
    Q_OBJECT

public:
    using LayerFactory = std::function<std::unique_ptr<UlanziWindowsLayer>()>;

    // From AetherSDR src/core/UlanziDialWindowsManager.h:80-81 [@1e0718ad]
    static constexpr int kPollMs = 5;
    static constexpr int kHotplugMs = 3000;

    explicit UlanziWindowsInput(QObject* parent = nullptr);
    ~UlanziWindowsInput() override;

    void start() override;
    void stop() override;

    // The key transitions one report means, given the one before it from that
    // interface: a Linux key code, then 1 for press or 0 for release.
    static QList<QPair<int, int>> decodeReport(const QByteArray& previous, const QByteArray& report);

    // Null goes back to the real device.
    static void setLayerFactoryForTest(LayerFactory factory);

public slots:
    void poll();            // the 5 ms timer's slot; public for tests
    void hotplugCheck();    // the 3 s timer's slot; public for tests

private:
    struct OpenInterface {
        std::unique_ptr<HidDeviceHandle> handle;
        QByteArray lastReport;
    };

    std::unique_ptr<UlanziWindowsLayer> m_layer;
    QTimer* m_pollTimer;
    QTimer* m_hotplugTimer;
    std::vector<OpenInterface> m_open;
    bool m_wanted{false};
};

} // namespace NereusSDR
