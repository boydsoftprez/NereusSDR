#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RecordingsFacade.h  (NereusSDR)
// =================================================================
//
// The mirrored `recordings` object (recordingsVersion 4): the Core's
// recordings as one document, so every device shows every slice's Record
// state, the last recording kept, the index and the recording settings.
// Design: docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// section 6.9.
//
//   json      { "version": 4,
//               "slices":   [ { "slice", "state" (idle, armed, recording),
//                               "startUtc", "startedBy", "elapsedMs",
//                               "myVoice" } ],
//               "kept":     { "seq", "slice", "id", "name", "lengthMs",
//                             "reason" (here, device, quiet, diskFull, wav),
//                             "device" },
//               "index":    { "revision", "count", "usedBytes", "freeBytes",
//                             "folder" },
//               "settings": { "keepMyVoice", "sidesApart", "autoStart",
//                             "autoStop", "quietMinutes", "format" (mp3, wav),
//                             "disk", "disks": [ { "root", "name", "free",
//                             "total", "sd", "own" } ],
//                             "onAirUseTxEq", "onAirUseCompressor",
//                             "onAirUseLeveler", "onAirUseCfc",
//                             "onAirUsePhaseRotator" },
//               "voiceKeyer": { "slots": [ { "n" (1 to 8), "name",
//                               "lengthMs", "empty" } x8 ], "repeat",
//                               "repeatDelaySec", "sending": { "n",
//                               "positionMs", "lengthMs", "waitingMs",
//                               "by" } or null },
//               "dsr":      { "enabled", "disk", "sizeLimitGb",
//                             "recording", "reason", "sizeLimitBytes",
//                             "usedBytes", "writePosition", "oldestUtcMs",
//                             "newestUtcMs", "hoursHeld", "overflows",
//                             "rewound": [ { "slice", "utcNs",
//                             "frequencyHz", "liveFrequencyHz" } ] } }
//             `dsr` is the Digital Spectrum Recorder: its settings (disk ""
//             is the Core's own), whether it writes now, why not when it is
//             on (an SD card, a missing disk, a failed write; "" otherwise),
//             and the ring file's figures (times in ms since 1970 UTC, 0 when
//             nothing is kept; hoursHeld is newest less oldest). The figures
//             move at most every RecordingService::kDsrFiguresRefreshMs.
//             `rewound` lists the slices playing the past (rewind sound,
//             the dsr.* verbs): the moment played when the document was
//             built (ns since 1970 UTC), the slice's frequency, and the
//             frequency LIVE returns it to.
//             `sending` is the message going out, or (`waitingMs` above 0)
//             the wait before its next repeat; it is rebuilt every
//             kElapsedRefreshMs while it lasts.
//             `kept.seq` moves by one for each recording kept; 0 means none
//             yet. `index.revision` moves by one each time the list of
//             recordings changes (kept, renamed, deleted). `elapsedMs` is the
//             running time when the document was built; it is rebuilt every
//             kElapsedRefreshMs while a slice records.
//   revision  moves by one each time `json` changes, and at no other time.
//
// On the Core it follows RecordingService, from publish() on: until then
// `json` is empty and the recordings folder is not opened, so building a
// model does not touch the disk. In a remote window it holds what the mirror
// last sent.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 10: version 2, a
//                 recording on the air, and the five on-air switches in the
//                 settings. AI-assisted implementation via Anthropic Claude
//                 Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11: version 3, the
//                 voice keyer. AI-assisted implementation via Anthropic Claude
//                 Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 21: version 4, the
//                 Digital Spectrum Recorder (`dsr`, and its three settings in
//                 recordings.setSettings). AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 22: `rewound` in
//                 `dsr`, and the dsr.rewind, dsr.live and dsr.seek verbs
//                 (still version 4). AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>

#include <optional>

#include "core/NereusCoreExport.h"
#include "core/recordings/RecordingDisks.h"
#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingSettings.h"

class QTimer;

namespace NereusSDR {

class NEREUS_CORE_EXPORT RecordingsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString json READ json NOTIFY changed)
    Q_PROPERTY(quint32 revision READ revision NOTIFY changed)

public:
    /// The recordingsVersion this Core offers. 2: a recording goes on the
    /// air (recordings.playback onAir) and the settings carry the five
    /// on-air switches. 3: the voice keyer (`voiceKeyer` and the
    /// voiceKeyer.* verbs). 4: the Digital Spectrum Recorder (`dsr`, and
    /// dsrEnabled, dsrDisk and dsrSizeLimitGb in recordings.setSettings).
    static constexpr int kVersion = 4;
    /// How often the running times are rebuilt while a slice records.
    static constexpr int kElapsedRefreshMs = 1000;
    /// recordings.list sends at most this many entries at once.
    static constexpr int kListPageMax = 100;
    /// recordings.export sends a recording's file this many bytes at a time
    /// (base64, 256 KiB on the wire, far under the client's 8 MiB message cap).
    static constexpr qint64 kExportChunkBytes = 196608;

    explicit RecordingsFacade(QObject* parent = nullptr);

    /// Follow `service` (null on a model that does not run its own radio).
    /// Builds nothing yet; see publish().
    void bind(RecordingService* service);
    /// Start publishing: opens the recordings folder, builds the document
    /// now and follows the service from here on. Again: nothing.
    void publish();
    bool published() const { return m_published; }
    /// Rebuild `json`, reading the disks again (after a settings change).
    void refresh();

    /// A remote window's mirror update. Ignored once bound to a service.
    /// True when the property is one of this object's.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);
    /// A remote window keeps no copy when the session ends.
    void clearStationValues();

    QString json() const { return m_json; }
    quint32 revision() const { return m_revision; }

    // ---- The document's words, shared with the verbs and the windows.
    static QString stateKey(RecordState state);
    static QString keptReasonKey(KeptReason reason);
    /// One recording as the list verb and the windows carry it.
    static QJsonObject entryJson(const RecordingEntry& entry);
    static std::optional<RecordingEntry> entryFromJson(const QJsonObject& object);
    static QJsonObject volumeJson(const RecordingVolume& volume);
    /// The recording settings as the document carries them, without disks.
    static QJsonObject settingsJson(const RecordingSettings& settings);
    /// The Digital Spectrum Recorder as the document carries it.
    static QJsonObject dsrJson(const DsrStatus& status);
    /// Applies a recordings.setSettings change (the document's settings
    /// names, any of keepMyVoice, sidesApart, autoStart, autoStop,
    /// quietMinutes, format, disk, onAirUseTxEq, onAirUseCompressor,
    /// onAirUseLeveler, onAirUseCfc, onAirUsePhaseRotator, and from version 4
    /// dsrEnabled, dsrDisk, dsrSizeLimitGb) to `settings`. False, with the refusal in
    /// plain words, when a name, a value or its type is not one of them.
    /// `volumes` are the disks a `disk` or `dsrDisk` may name; the Core's own
    /// disk is stored as "".
    static bool applySettingsChange(const QJsonObject& changed, const QList<RecordingVolume>& volumes,
                                    RecordingSettings* settings, QString* refusal);

signals:
    void changed();

private:
    void rebuild();
    void onKept(const KeptEvent& event);
    void onIndexChanged();
    void updateTick();

    QPointer<RecordingService> m_service;
    bool m_published = false;
    QTimer* m_tick = nullptr;
    QJsonObject m_kept;
    qint64 m_keptSeq = 0;
    quint32 m_indexRevision = 0;
    QList<RecordingVolume> m_volumes;
    QString m_json;
    quint32 m_revision = 0;
};

} // namespace NereusSDR
