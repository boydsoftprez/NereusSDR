#pragma once

// =================================================================
// src/gui/hotkeys/devices/UlanziLinuxInput.h  (NereusSDR)
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
//   2026-11-06 - Ulanzi Dial on Linux built on AetherSDR
//                 src/core/EvdevEncoderManager.cpp [@1e0718ad] for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 16. The device sits behind a seam so a test never opens one,
//                 and nothing is looked at until the application allows it. It
//                 lets go of the grab before it closes. An access refusal is
//                 reported on the Knobs page.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/UlanziDialInput.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <memory>

class QFileSystemWatcher;
class QTimer;

namespace NereusSDR {

// One open input node. Closing lets go of the grab first, so the device's keys go back
// to the system. wentAway must not come from inside closeDevice.
class NEREUS_GUI_EXPORT UlanziEvdevHandle : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void closeDevice() = 0;

signals:
    void inputEvent(quint16 type, quint16 code, qint32 value);
    void wentAway();
};

class UlanziLinuxLayer {
public:
    virtual ~UlanziLinuxLayer() = default;
    virtual QString findNode() = 0;             // reads names only, and opens nothing
    virtual QString watchedDirectory() = 0;     // empty for none
    // On failure, detail gets the words deviceOpenFailureReason reads.
    virtual std::unique_ptr<UlanziEvdevHandle> open(const QString& node, QString* detail) = 0;
};

class NEREUS_GUI_EXPORT UlanziLinuxInput : public UlanziDialInput {   // lives on the USB worker
    Q_OBJECT

public:
    using LayerFactory = std::function<std::unique_ptr<UlanziLinuxLayer>()>;
    using Scheduler = std::function<void(int delayMs)>;

    // From AetherSDR src/core/EvdevEncoderManager.cpp:55 [@1e0718ad]
    static constexpr int kDebounceMs = 250;

    explicit UlanziLinuxInput(QObject* parent = nullptr);
    ~UlanziLinuxInput() override;

    void start() override;
    void stop() override;

    // The eventN entry under sysClassInput named for the dial's keyboard, or empty.
    // Reads names only, so it finds a dial it has no permission to open.
    static QString findEventNode(const QString& sysClassInput);

    // Null goes back to the real device, or the real timer.
    static void setLayerFactoryForTest(LayerFactory factory);
    static void setSchedulerForTest(Scheduler scheduler);

public slots:
    void onDirectoryChanged();  // the watcher's slot; public for tests
    void checkNow();            // the debounce timer's slot; public for tests
    void onInputEvent(quint16 type, quint16 code, qint32 value);    // public for tests

private:
    void scheduleCheck();
    void retireHandle();
    void onWentAway();

    std::unique_ptr<UlanziLinuxLayer> m_layer;
    Scheduler m_scheduler;
    QTimer* m_debounce;
    QFileSystemWatcher* m_watcher{nullptr};
    UlanziEvdevHandle* m_handle{nullptr};   // a Qt child while open
    bool m_wanted{false};
};

} // namespace NereusSDR
