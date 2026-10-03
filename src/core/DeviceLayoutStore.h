#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/DeviceLayoutStore.h  (NereusSDR)
// =================================================================
//
// Each device's closed slices, kept for its return (iPhone app plan Task 73,
// R-IOS-02; the several-devices design, docs/architecture/2026-09-24-
// several-devices-on-one-core-design.md, section 5.3, ruling 5.3).
//
// The Core keeps two stores. The restart manifest (ReceiveLayoutStore, per
// radio) holds only the live slices, within its limit of five. This one,
// per device and per radio, holds the slices a device lost while it was
// away or gone: the end of its 180 s, leaving on purpose, or a take (Task
// 74). For each: its id (its letter), pan, frequency, mode, and a copy of
// the slice's own settings, taken when it closed:
//
//   - every AppSettings key under "Slice<id>/" (every band's, every mode's);
//   - every key under "hardware/<mac>/slices/<id>/" (its NNR choices).
//
// The copy is stored relative to the slice ("Slice/<rest>", "radio/<rest>")
// so it can be written back under another letter: a slice restored where
// its old letter is taken gets its settings on the new letter's keys, and
// the old letter's keys are cleared when the copy is taken, so a new slice
// another device makes on that letter never inherits them.
//
// At most the board's maxSlices entries per device (the oldest go first); a
// device's entries are removed when it is revoked. A window signed in with
// the older token is never saved: it cannot be recognised when it comes
// back. The caller decides that; this class only stores.
//
// Like ReceiveLayoutStore it only validates and stages AppSettings values;
// it never writes the file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 73 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/WdspTypes.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>

namespace NereusSDR {

class AppSettings;

/// One closed slice, as a device's store keeps it.
struct SavedSlice {
    int id {0};
    QString panKey;
    double frequencyHz {0.0};
    DSPMode dspMode {DSPMode::USB};
    /// The slice's own settings, relative ("Slice/<rest>", "radio/<rest>").
    QMap<QString, QString> settings;
};

class DeviceLayoutStore final {
public:
    /// The device's saved slices for this radio, oldest first. Entries that
    /// fail validation are left out; an unreadable record reads as none.
    static QList<SavedSlice> load(const AppSettings& settings, const QString& mac,
                                  const QByteArray& deviceId);

    /// Adds a closed slice to the device's record for this radio. An entry
    /// with the same id is replaced; past `maxSlices` the oldest go. False
    /// (and nothing changed) when the slice or an identity is not valid.
    static bool append(AppSettings& settings, const QString& mac, const QByteArray& deviceId,
                       const SavedSlice& slice, int maxSlices);

    /// Replaces the device's record for this radio (an empty list removes
    /// it), within `maxSlices`.
    static bool replace(AppSettings& settings, const QString& mac, const QByteArray& deviceId,
                        const QList<SavedSlice>& slices, int maxSlices);

    /// Forgets the device's record for every radio (a revoke).
    static void forgetDevice(AppSettings& settings, const QByteArray& deviceId);

    /// A copy of slice `sliceId`'s own settings, relative.
    static QMap<QString, QString> captureSliceSettings(const AppSettings& settings,
                                                       const QString& mac, int sliceId);
    /// Removes slice `sliceId`'s own settings, so a new slice on that
    /// letter starts from defaults.
    static void clearSliceSettings(AppSettings& settings, const QString& mac, int sliceId);
    /// Clears slice `sliceId`'s own settings, then writes `copy` to them.
    static void writeSliceSettings(AppSettings& settings, const QString& mac, int sliceId,
                                   const QMap<QString, QString>& copy);

    /// The AppSettings key (under hardware/<mac>/) a device's record lives
    /// at, for tests and the provenance of what is stored.
    static QString recordKey(const QByteArray& deviceId);
};

} // namespace NereusSDR
