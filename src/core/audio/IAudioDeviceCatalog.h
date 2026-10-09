// =================================================================
// src/core/audio/IAudioDeviceCatalog.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original interface for the live device
// catalogue (R-AUD-03); no upstream logic.
//
// Every device list in the program reads one catalogue.  The getters are
// main-thread calls that return the last snapshot; the signals arrive on
// the main thread.  devicesChanged() follows a re-list; defaultChanged()
// follows a change of the system default without waiting for a re-list.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 (R-AUD-06): olderDriversRescanned().
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AudioDeviceTypes.h"

#include <QList>
#include <QObject>

#include <optional>

namespace NereusSDR {

class IAudioDeviceCatalog : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~IAudioDeviceCatalog() override = default;

    virtual QList<AudioBackendId> backends() const = 0;                // R-AUD-01 order
    virtual bool backendRunning(AudioBackendId id) const = 0;
    virtual QList<AudioDeviceInfo> devices(AudioBackendId id, AudioDeviceDirection direction) const = 0;
    virtual std::optional<AudioDeviceInfo> defaultDevice(AudioBackendId id,
                                                         AudioDeviceDirection direction) const = 0;
    virtual void rescanOlderDrivers() = 0;

signals:
    void devicesChanged();
    void defaultChanged(NereusSDR::AudioDeviceDirection direction);
    // R-AUD-06: a rescanOlderDrivers() has finished; the list it made has
    // been adopted (devicesChanged, when it differs, came first).
    void olderDriversRescanned();
};

} // namespace NereusSDR
