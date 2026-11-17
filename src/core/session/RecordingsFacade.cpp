// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original. See RecordingsFacade.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 5.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 10: the five on-air
//                 switches in the settings. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11: the voice
//                 keyer's part. AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 21: the Digital
//                 Spectrum Recorder's part and settings. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 22: `rewound` in
//                 the `dsr` part. AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: `stretch` and
//                 `saving` in the `dsr` part; a whole-pan entry's centre and
//                 span. AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 26: `receivers`
//                 and `bytesPerHour` in the `dsr` part, for Setup's hours.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF): `keptCentreHz` and `keptSpanHz` on
//                 each rewound slice, what was kept at the moment it plays
//                 from. AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/RecordingsFacade.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QTimeZone>

#include <algorithm>

#include "core/recordings/RecordingStore.h"

namespace NereusSDR {

namespace {

QString isoUtc(const QDateTime& when)
{
    return when.isValid() ? when.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QJsonObject noKept()
{
    return QJsonObject{{QStringLiteral("seq"), 0},
                       {QStringLiteral("slice"), -1},
                       {QStringLiteral("id"), QString()},
                       {QStringLiteral("name"), QString()},
                       {QStringLiteral("lengthMs"), 0},
                       {QStringLiteral("reason"), QString()},
                       {QStringLiteral("device"), QString()}};
}

}  // namespace

RecordingsFacade::RecordingsFacade(QObject* parent)
    : QObject(parent)
    , m_kept(noKept())
{
}

void RecordingsFacade::bind(RecordingService* service)
{
    if (m_service) {
        disconnect(m_service, nullptr, this, nullptr);
        if (VoiceKeyer* voice = m_service->voiceKeyer()) {
            disconnect(voice, nullptr, this, nullptr);
        }
    }
    m_service = service;
    if (m_service) {
        connect(m_service, &RecordingService::sliceStateChanged, this, &RecordingsFacade::rebuild);
        connect(m_service, &RecordingService::kept, this, &RecordingsFacade::onKept);
        connect(m_service, &RecordingService::dsrChanged, this, &RecordingsFacade::rebuild);
        if (VoiceKeyer* voice = m_service->voiceKeyer()) {
            connect(voice, &VoiceKeyer::changed, this, &RecordingsFacade::rebuild);
        }
    }
    if (m_published) {
        refresh();
    }
}

void RecordingsFacade::publish()
{
    if (m_published || !m_service) {
        return;
    }
    m_published = true;
    // Opens the recordings folder (RecordingService::store).
    RecordingStore* store = m_service->store();
    if (store) {
        connect(store, &RecordingStore::changed, this, &RecordingsFacade::onIndexChanged);
    }
    refresh();
}

void RecordingsFacade::refresh()
{
    if (!m_published || !m_service) {
        return;
    }
    m_volumes = RecordingDisks::volumes();
    rebuild();
}

void RecordingsFacade::onKept(const KeptEvent& event)
{
    ++m_keptSeq;
    m_kept = QJsonObject{{QStringLiteral("seq"), m_keptSeq},
                         {QStringLiteral("slice"), event.sliceId},
                         {QStringLiteral("id"), event.id},
                         {QStringLiteral("name"), event.name},
                         {QStringLiteral("lengthMs"), event.lengthMs},
                         {QStringLiteral("reason"), keptReasonKey(event.reason)},
                         {QStringLiteral("device"), event.deviceName}};
    rebuild();
}

void RecordingsFacade::onIndexChanged()
{
    ++m_indexRevision;
    rebuild();
}

void RecordingsFacade::rebuild()
{
    if (!m_published || !m_service) {
        return;
    }
    QJsonArray slices;
    for (const RecordingSliceState& st : m_service->sliceStates()) {
        slices.append(QJsonObject{{QStringLiteral("slice"), st.sliceId},
                                  {QStringLiteral("state"), stateKey(st.state)},
                                  {QStringLiteral("startUtc"), isoUtc(st.startUtc)},
                                  {QStringLiteral("startedBy"), st.startedBy},
                                  {QStringLiteral("elapsedMs"), st.elapsedMs},
                                  {QStringLiteral("myVoice"), st.hearingMyVoice}});
    }
    const RecordingStore* store = m_service->store();
    QJsonObject index{{QStringLiteral("revision"), static_cast<qint64>(m_indexRevision)},
                      {QStringLiteral("count"), store ? store->entries().size() : 0},
                      {QStringLiteral("usedBytes"), store ? store->usedBytes() : 0},
                      {QStringLiteral("freeBytes"), store ? store->freeBytes() : 0},
                      {QStringLiteral("folder"), store ? QDir::toNativeSeparators(store->folder())
                                                       : QString()}};
    QJsonObject settings = settingsJson(m_service->settings());
    QJsonArray disks;
    for (const RecordingVolume& volume : m_volumes) {
        disks.append(volumeJson(volume));
    }
    settings.insert(QStringLiteral("disks"), disks);

    const QJsonObject document{{QStringLiteral("version"), kVersion},
                               {QStringLiteral("slices"), slices},
                               {QStringLiteral("kept"), m_kept},
                               {QStringLiteral("index"), index},
                               {QStringLiteral("settings"), settings},
                               {QStringLiteral("voiceKeyer"), m_service->voiceKeyer()
                                                                  ? m_service->voiceKeyer()->documentJson()
                                                                  : QJsonObject()},
                               {QStringLiteral("dsr"), dsrJson(m_service->dsrStatus())}};
    const QString next = QString::fromUtf8(QJsonDocument(document).toJson(QJsonDocument::Compact));
    updateTick();
    if (next == m_json) {
        return;
    }
    m_json = next;
    ++m_revision;
    emit changed();
}

void RecordingsFacade::updateTick()
{
    bool recording = false;
    if (m_service) {
        for (const RecordingSliceState& st : m_service->sliceStates()) {
            recording = recording || st.state == RecordState::Recording;
        }
        // A voice message going out, or waiting to repeat.
        recording = recording || (m_service->voiceKeyer() && m_service->voiceKeyer()->sending().has_value());
    }
    if (recording && !m_tick) {
        m_tick = new QTimer(this);
        m_tick->setInterval(kElapsedRefreshMs);
        connect(m_tick, &QTimer::timeout, this, &RecordingsFacade::rebuild);
    }
    if (m_tick) {
        if (recording && !m_tick->isActive()) {
            m_tick->start();
        } else if (!recording && m_tick->isActive()) {
            m_tick->stop();
        }
    }
}

bool RecordingsFacade::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    if (propertyName == "json") {
        const QString next = value.toString();
        if (!m_service && next != m_json) {
            m_json = next;
            emit changed();
        }
        return true;
    }
    if (propertyName == "revision") {
        const quint32 next = value.toUInt();
        if (!m_service && next != m_revision) {
            m_revision = next;
            emit changed();
        }
        return true;
    }
    return false;
}

void RecordingsFacade::clearStationValues()
{
    if (m_service || (m_json.isEmpty() && m_revision == 0)) {
        return;
    }
    m_json.clear();
    m_revision = 0;
    emit changed();
}

QString RecordingsFacade::stateKey(RecordState state)
{
    switch (state) {
    case RecordState::Idle:
        return QStringLiteral("idle");
    case RecordState::Armed:
        return QStringLiteral("armed");
    case RecordState::Recording:
        return QStringLiteral("recording");
    }
    return QStringLiteral("idle");
}

QString RecordingsFacade::keptReasonKey(KeptReason reason)
{
    switch (reason) {
    case KeptReason::StoppedHere:
        return QStringLiteral("here");
    case KeptReason::StoppedOnDevice:
        return QStringLiteral("device");
    case KeptReason::StoppedQuiet:
        return QStringLiteral("quiet");
    case KeptReason::DiskFull:
        return QStringLiteral("diskFull");
    case KeptReason::KeptAsWav:
        return QStringLiteral("wav");
    }
    return QStringLiteral("here");
}

QJsonObject RecordingsFacade::entryJson(const RecordingEntry& entry)
{
    QJsonObject o{{QStringLiteral("id"), entry.id},
                       {QStringLiteral("kind"), recordingKindKey(entry.kind)},
                       {QStringLiteral("slice"), entry.sliceId},
                       {QStringLiteral("startUtc"), isoUtc(entry.startUtc)},
                       {QStringLiteral("lengthMs"), entry.lengthMs},
                       {QStringLiteral("frequencyHz"), entry.frequencyHz},
                       {QStringLiteral("mode"), entry.mode},
                       {QStringLiteral("call"), entry.call},
                       {QStringLiteral("name"), entry.name},
                       {QStringLiteral("fileName"), entry.fileName},
                       {QStringLiteral("sizeBytes"), entry.sizeBytes}};
    // Recording plan Task 25: a whole-pan stretch's pan, for its range.
    if (entry.spanHz > 0) {
        o.insert(QStringLiteral("centreHz"), entry.centreHz);
        o.insert(QStringLiteral("spanHz"), entry.spanHz);
    }
    return o;
}

std::optional<RecordingEntry> RecordingsFacade::entryFromJson(const QJsonObject& object)
{
    RecordingEntry entry;
    entry.id = object.value(QStringLiteral("id")).toString();
    if (entry.id.isEmpty()
        || !recordingKindFromKey(object.value(QStringLiteral("kind")).toString(), &entry.kind)) {
        return std::nullopt;
    }
    entry.sliceId = object.value(QStringLiteral("slice")).toInt(-1);
    entry.startUtc = QDateTime::fromString(object.value(QStringLiteral("startUtc")).toString(),
                                           Qt::ISODateWithMs)
                         .toUTC();
    entry.lengthMs = object.value(QStringLiteral("lengthMs")).toInteger();
    entry.frequencyHz = object.value(QStringLiteral("frequencyHz")).toInteger();
    entry.mode = object.value(QStringLiteral("mode")).toString();
    entry.call = object.value(QStringLiteral("call")).toString();
    entry.name = object.value(QStringLiteral("name")).toString();
    entry.fileName = object.value(QStringLiteral("fileName")).toString();
    entry.sizeBytes = object.value(QStringLiteral("sizeBytes")).toInteger();
    entry.centreHz = object.value(QStringLiteral("centreHz")).toInteger();
    entry.spanHz = object.value(QStringLiteral("spanHz")).toInteger();
    return entry;
}

QJsonObject RecordingsFacade::volumeJson(const RecordingVolume& volume)
{
    return QJsonObject{{QStringLiteral("root"), QDir::toNativeSeparators(volume.rootPath)},
                       {QStringLiteral("name"), volume.displayName},
                       {QStringLiteral("free"), volume.freeBytes},
                       {QStringLiteral("total"), volume.totalBytes},
                       {QStringLiteral("sd"), volume.isSdCard},
                       {QStringLiteral("own"), volume.isCoreOwnDisk}};
}

QJsonObject RecordingsFacade::settingsJson(const RecordingSettings& settings)
{
    return QJsonObject{
        {QStringLiteral("keepMyVoice"), settings.keepMyVoice},
        {QStringLiteral("sidesApart"), settings.sidesApart},
        {QStringLiteral("autoStart"), settings.autoStartOnTransmit},
        {QStringLiteral("autoStop"), settings.autoStopWhenQuiet},
        {QStringLiteral("quietMinutes"), settings.quietMinutes},
        {QStringLiteral("format"),
         settings.format == AudioFileFormat::Wav ? QStringLiteral("wav") : QStringLiteral("mp3")},
        {QStringLiteral("disk"), QDir::toNativeSeparators(settings.disk)},
        {QStringLiteral("onAirUseTxEq"), settings.onAirUseTxEq},
        {QStringLiteral("onAirUseCompressor"), settings.onAirUseCompressor},
        {QStringLiteral("onAirUseLeveler"), settings.onAirUseLeveler},
        {QStringLiteral("onAirUseCfc"), settings.onAirUseCfc},
        {QStringLiteral("onAirUsePhaseRotator"), settings.onAirUsePhaseRotator}};
}

QJsonObject RecordingsFacade::dsrJson(const DsrStatus& status)
{
    // Recording plan Task 22: the slices playing the past (rewind sound).
    QJsonArray rewound;
    for (const DsrRewoundSlice& r : status.rewound) {
        rewound.append(QJsonObject{{QStringLiteral("slice"), r.slice},
                                   {QStringLiteral("utcNs"), r.utcNs},
                                   {QStringLiteral("frequencyHz"), r.frequencyHz},
                                   {QStringLiteral("liveFrequencyHz"), r.liveFrequencyHz},
                                   {QStringLiteral("keptCentreHz"), r.keptCentreHz},
                                   {QStringLiteral("keptSpanHz"), r.keptSpanHz}});
    }
    QJsonArray receivers;
    for (const DsrReceiverRate& r : status.receivers) {
        receivers.append(QJsonObject{{QStringLiteral("receiver"), r.receiver}, {QStringLiteral("rateHz"), r.rateHz}});
    }
    return QJsonObject{{QStringLiteral("enabled"), status.enabled},
                       {QStringLiteral("disk"), QDir::toNativeSeparators(status.disk)},
                       {QStringLiteral("sizeLimitGb"), status.sizeLimitGb},
                       {QStringLiteral("recording"), status.recording},
                       {QStringLiteral("reason"), status.reason},
                       {QStringLiteral("sizeLimitBytes"), status.sizeLimitBytes},
                       {QStringLiteral("usedBytes"), status.usedBytes},
                       {QStringLiteral("writePosition"), status.writePosition},
                       {QStringLiteral("oldestUtcMs"), status.oldestUtcMs},
                       {QStringLiteral("newestUtcMs"), status.newestUtcMs},
                       {QStringLiteral("hoursHeld"), status.hoursHeld()},
                       {QStringLiteral("overflows"), static_cast<qint64>(status.overflows)},
                       {QStringLiteral("rewound"), rewound},
                       // Recording plan Task 25: a saved stretch opened on
                       // slice A, and the saves still running.
                       {QStringLiteral("stretch"),
                        status.stretchId.isEmpty()
                            ? QJsonValue()
                            : QJsonValue(QJsonObject{{QStringLiteral("id"), status.stretchId},
                                                     {QStringLiteral("fromUtcNs"), status.stretchFromUtcNs},
                                                     {QStringLiteral("toUtcNs"), status.stretchToUtcNs}})},
                       {QStringLiteral("saving"), status.saving},
                       // Recording plan Task 26: the receivers it keeps now
                       // and their disk bytes an hour (Setup's hours).
                       {QStringLiteral("receivers"), receivers},
                       {QStringLiteral("bytesPerHour"), status.bytesPerHour}};
}

bool RecordingsFacade::applySettingsChange(const QJsonObject& changed,
                                           const QList<RecordingVolume>& volumes,
                                           RecordingSettings* settings, QString* refusal)
{
    const auto refuse = [refusal](const QString& why) {
        if (refusal) {
            *refusal = why;
        }
        return false;
    };
    const QString notUnderstood =
        QStringLiteral("The change to the recording settings was not understood.");
    if (changed.isEmpty() || settings == nullptr) {
        return refuse(notUnderstood);
    }
    RecordingSettings next = *settings;
    for (auto it = changed.constBegin(); it != changed.constEnd(); ++it) {
        const QString& name = it.key();
        const QJsonValue& value = it.value();
        bool* flag = nullptr;
        if (name == QLatin1String("keepMyVoice")) {
            flag = &next.keepMyVoice;
        } else if (name == QLatin1String("sidesApart")) {
            flag = &next.sidesApart;
        } else if (name == QLatin1String("autoStart")) {
            flag = &next.autoStartOnTransmit;
        } else if (name == QLatin1String("autoStop")) {
            flag = &next.autoStopWhenQuiet;
        } else if (name == QLatin1String("onAirUseTxEq")) {
            flag = &next.onAirUseTxEq;
        } else if (name == QLatin1String("onAirUseCompressor")) {
            flag = &next.onAirUseCompressor;
        } else if (name == QLatin1String("onAirUseLeveler")) {
            flag = &next.onAirUseLeveler;
        } else if (name == QLatin1String("onAirUseCfc")) {
            flag = &next.onAirUseCfc;
        } else if (name == QLatin1String("onAirUsePhaseRotator")) {
            flag = &next.onAirUsePhaseRotator;
        } else if (name == QLatin1String("dsrEnabled")) {
            flag = &next.dsrEnabled;
        }
        if (flag != nullptr) {
            if (!value.isBool()) {
                return refuse(notUnderstood);
            }
            *flag = value.toBool();
        } else if (name == QLatin1String("quietMinutes")) {
            const double minutes = value.toDouble(-1.0);
            if (!value.isDouble() || minutes != static_cast<double>(static_cast<qint64>(minutes))) {
                return refuse(notUnderstood);
            }
            if (minutes < RecordingSettings::kQuietMinutesMin
                || minutes > RecordingSettings::kQuietMinutesMax) {
                return refuse(QStringLiteral("Quiet can be %1 to %2 minutes.")
                                  .arg(RecordingSettings::kQuietMinutesMin)
                                  .arg(RecordingSettings::kQuietMinutesMax));
            }
            next.quietMinutes = static_cast<int>(minutes);
        } else if (name == QLatin1String("format")) {
            const QString format = value.toString();
            if (format == QLatin1String("mp3")) {
                next.format = AudioFileFormat::Mp3;
            } else if (format == QLatin1String("wav")) {
                next.format = AudioFileFormat::Wav;
            } else {
                return refuse(notUnderstood);
            }
        } else if (name == QLatin1String("disk") || name == QLatin1String("dsrDisk")) {
            if (!value.isString()) {
                return refuse(notUnderstood);
            }
            QString& target = name == QLatin1String("disk") ? next.disk : next.dsrDisk;
            const QString disk = QDir::fromNativeSeparators(value.toString());
            if (disk.isEmpty()) {
                target.clear();
            } else {
                const auto found = std::find_if(volumes.cbegin(), volumes.cend(),
                                                [&disk](const RecordingVolume& volume) {
                                                    return QDir::fromNativeSeparators(volume.rootPath) == disk;
                                                });
                if (found == volumes.cend()) {
                    return refuse(QStringLiteral("That disk is not on the Core now."));
                }
                // An SD card may be chosen for the recorder; it then stays
                // off and says why (RecordingService::DsrOff::SdChosen).
                target = found->isCoreOwnDisk ? QString() : disk;
            }
        } else if (name == QLatin1String("dsrSizeLimitGb")) {
            const double gb = value.toDouble(-1.0);
            if (!value.isDouble() || gb != static_cast<double>(static_cast<qint64>(gb))) {
                return refuse(notUnderstood);
            }
            if (gb < RecordingSettings::kDsrSizeLimitGbMin || gb > RecordingSettings::kDsrSizeLimitGbMax) {
                return refuse(QStringLiteral("The recorder's size can be %1 to %2 GB.")
                                  .arg(RecordingSettings::kDsrSizeLimitGbMin)
                                  .arg(RecordingSettings::kDsrSizeLimitGbMax));
            }
            next.dsrSizeLimitGb = static_cast<int>(gb);
        } else {
            return refuse(notUnderstood);
        }
    }
    *settings = next;
    return true;
}

} // namespace NereusSDR
