// =================================================================
// src/gui/setup/AudioDriverList.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AudioDriverList.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 16 (R-AUD-01, R-AUD-03, R-AUD-06,
//               R-AUD-08 to R-AUD-11, R-AUD-14, R-AUD-15, R-AUD-16, D10).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/setup/AudioDriverList.h"

#include "core/audio/AudioDeviceMatching.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/PortAudioBackend.h"

#include <QChar>
#include <QStringList>

#include <cmath>

namespace NereusSDR {

namespace {

bool hasBackend(const IAudioDeviceCatalog& catalogue, AudioBackendId id)
{
    return catalogue.backends().contains(id);
}

bool runningBackend(const IAudioDeviceCatalog& catalogue, AudioBackendId id)
{
    return hasBackend(catalogue, id) && catalogue.backendRunning(id);
}

AudioDriverEntry engineEntry(AudioEngineKind kind)
{
    AudioDriverEntry entry;
    entry.engine = kind;
    entry.label = audioEngineLabel(kind);
    return entry;
}

AudioDriverEntry disabledEntry(AudioDriverEntry entry, const QString& reason)
{
    entry.enabled = false;
    entry.disabledReason = reason;
    return entry;
}

// Which system the catalogue's backends describe (the fake decides in a
// test, never the build).
enum class CatalogueSystem { Mac, Windows, Linux, LinuxCore, Unknown };

CatalogueSystem systemOf(const IAudioDeviceCatalog& catalogue)
{
    if (hasBackend(catalogue, AudioBackendId::CoreAudio)) {
        return CatalogueSystem::Mac;
    }
    if (hasBackend(catalogue, AudioBackendId::Wasapi)) {
        return CatalogueSystem::Windows;
    }
    if (hasBackend(catalogue, AudioBackendId::PipeWire)
        || hasBackend(catalogue, AudioBackendId::PulseAudio)) {
        return CatalogueSystem::Linux;
    }
    if (hasBackend(catalogue, AudioBackendId::AlsaDirect)) {
        return CatalogueSystem::LinuxCore;
    }
    return CatalogueSystem::Unknown;
}

// The older drivers' host APIs, in PortAudioBackend's order for the system,
// or, when the system cannot be told, in the order the devices name them.
QStringList olderHostApis(const IAudioDeviceCatalog& catalogue, CatalogueSystem system)
{
    if (system == CatalogueSystem::Windows) {
        return olderDriverHostApis(OlderDriverPlatform::Windows, false);
    }
    if (system == CatalogueSystem::Linux) {
        return olderDriverHostApis(OlderDriverPlatform::Linux, false);
    }
    QStringList apis;
    for (AudioDeviceDirection direction : {AudioDeviceDirection::Output, AudioDeviceDirection::Input}) {
        for (const AudioDeviceInfo& info : catalogue.devices(AudioBackendId::PortAudio, direction)) {
            if (!info.hostApi.isEmpty() && !apis.contains(info.hostApi)) {
                apis.append(info.hostApi);
            }
        }
    }
    return apis;
}

QString notRunningReason(const QString& engine)
{
    return QStringLiteral("%1 is not running on this computer.").arg(engine);
}

bool isOutputRole(AudioRole role)
{
    return role == AudioRole::Speakers || role == AudioRole::Headphones;
}

int vaxNumber(AudioRole role)
{
    switch (role) {
    case AudioRole::Vax1:
        return 1;
    case AudioRole::Vax2:
        return 2;
    case AudioRole::Vax3:
        return 3;
    case AudioRole::Vax4:
        return 4;
    case AudioRole::Speakers:
    case AudioRole::Headphones:
    case AudioRole::TxInput:
        break;
    }
    return 0;
}

QString troubleWords(AudioRoleReason reason)
{
    return reason == AudioRoleReason::InUse ? QStringLiteral("is in use by another program")
                                            : QStringLiteral("is not connected");
}

QString savedName(const AudioDeviceConfig& saved, AudioDeviceDirection direction)
{
    QString name = saved.deviceName.isEmpty() ? saved.deviceId : saved.deviceName;
    if (saved.firstChannel > 1) {
        name += QLatin1Char(' ') + QChar(0x00B7) + QLatin1Char(' ')
            + audioPairLabel(direction, AudioChannelPair{saved.firstChannel, 2});
    }
    return name;
}

} // namespace

QString olderDriverDisplayName(const QString& hostApi)
{
    if (hostApi == QLatin1String("Windows DirectSound")) {
        return QStringLiteral("DirectSound");
    }
    if (hostApi == QLatin1String("Windows WDM-KS")) {
        return QStringLiteral("WDM-KS");
    }
    if (hostApi == QLatin1String("JACK Audio Connection Kit")) {
        return QStringLiteral("JACK");
    }
    return hostApi;
}

QList<AudioDriverEntry> audioDriverEntries(const IAudioDeviceCatalog& catalogue, bool daemonCore)
{
    QList<AudioDriverEntry> entries;
    const CatalogueSystem system = systemOf(catalogue);

    if (daemonCore || system == CatalogueSystem::LinuxCore) {
        entries.append(disabledEntry(
            engineEntry(AudioEngineKind::AlsaDirect),
            QStringLiteral("The Core runs without a desktop, so it plays straight to the sound card.")));
        return entries;
    }
    if (system == CatalogueSystem::Mac) {
        // Settled call 24: the older drivers add nothing on the Mac.
        entries.append(disabledEntry(engineEntry(AudioEngineKind::CoreAudio),
                                     QStringLiteral("The only sound system on the Mac.")));
        return entries;
    }
    if (system == CatalogueSystem::Windows) {
        const bool wasapi = runningBackend(catalogue, AudioBackendId::Wasapi);
        for (AudioEngineKind kind : {AudioEngineKind::WindowsShared, AudioEngineKind::WindowsExclusive}) {
            entries.append(wasapi ? engineEntry(kind)
                                  : disabledEntry(engineEntry(kind),
                                                  notRunningReason(QStringLiteral("Windows audio"))));
        }
        entries.append(runningBackend(catalogue, AudioBackendId::Asio)
                           ? engineEntry(AudioEngineKind::Asio)
                           : disabledEntry(engineEntry(AudioEngineKind::Asio),
                                           QStringLiteral("No ASIO driver is installed on this computer.")));
    } else if (system == CatalogueSystem::Linux) {
        // R-AUD-01: PipeWire serves programs written for PulseAudio, so
        // PulseAudio is listed only while PipeWire is not running.
        if (runningBackend(catalogue, AudioBackendId::PipeWire)) {
            entries.append(engineEntry(AudioEngineKind::PipeWire));
        } else {
            AudioDriverEntry pipeWire = engineEntry(AudioEngineKind::PipeWire);
            pipeWire.label = QStringLiteral("PipeWire (not running)");
            entries.append(disabledEntry(pipeWire, notRunningReason(QStringLiteral("PipeWire"))));
            if (runningBackend(catalogue, AudioBackendId::PulseAudio)) {
                entries.append(engineEntry(AudioEngineKind::PulseAudio));
            } else {
                AudioDriverEntry pulse = engineEntry(AudioEngineKind::PulseAudio);
                pulse.label = QStringLiteral("PulseAudio (not running)");
                entries.append(disabledEntry(pulse, notRunningReason(QStringLiteral("PulseAudio"))));
            }
        }
    }

    if (hasBackend(catalogue, AudioBackendId::PortAudio)) {
        const QStringList apis = olderHostApis(catalogue, system);
        if (!apis.isEmpty()) {
            AudioDriverEntry heading;
            heading.label = audioEngineLabel(AudioEngineKind::PortAudio);
            heading.enabled = false;
            entries.append(heading);
            for (const QString& api : apis) {
                AudioDriverEntry older = engineEntry(AudioEngineKind::PortAudio);
                older.hostApi = api;
                older.label = olderDriverDisplayName(api);
                entries.append(older);
            }
        }
    }
    return entries;
}

QList<AudioDeviceEntry> audioDeviceEntries(const IAudioDeviceCatalog& catalogue,
                                           AudioEngineKind engine, const QString& hostApi,
                                           AudioDeviceDirection direction,
                                           const AudioDeviceConfig& saved)
{
    QList<AudioDeviceEntry> entries;
    AudioDeviceEntry platformDefault;
    platformDefault.label = QStringLiteral("(platform default)");
    entries.append(platformDefault);

    // Settled call 12: a saved "(none)" shows as "(none)".
    if (saved.isNone()) {
        AudioDeviceEntry none;
        none.deviceId = QString::fromLatin1(kAudioDeviceNone);
        none.label = QString::fromLatin1(kAudioDeviceNone);
        entries.append(none);
    }

    const bool older = engine == AudioEngineKind::PortAudio;
    QList<AudioDeviceInfo> devices;
    for (const AudioDeviceInfo& info : catalogue.devices(audioBackendFor(engine), direction)) {
        if (older && !hostApi.isEmpty() && info.hostApi != hostApi) {
            continue;
        }
        devices.append(info);
    }
    // The saved choice is found as the stream supervisor finds it: by id,
    // else by name ("Saved identity").
    // A choice saved before engines were named is looked for on this list.
    AudioDeviceConfig probe = saved;
    if (!probe.engine) {
        probe.engine = engine;
    }
    const std::optional<AudioDeviceMatch> match = matchSavedAudioDevice(probe, devices);
    bool savedListed = false;
    for (const AudioDeviceInfo& info : devices) {
        const bool pairs = info.channelCount > 2;
        const QList<AudioChannelPair> list = pairs
            ? audioChannelPairs(info.channelCount)
            : QList<AudioChannelPair>{AudioChannelPair{1, info.channelCount < 2 ? 1 : 2}};
        for (const AudioChannelPair& pair : list) {
            AudioDeviceEntry entry;
            entry.deviceId = info.id;
            entry.label = audioDeviceEntryLabel(info, pair);
            entry.group = pairs ? info.name : QString();
            entry.pair = pair;
            entry.state = info.state;
            entry.bluetooth = info.transport == AudioTransport::Bluetooth;
            if (info.state == AudioDeviceState::InUse) {
                entry.label += QStringLiteral(" (in use by another program)");
            } else if (info.state == AudioDeviceState::NotConnected) {
                entry.label += QStringLiteral(" (not connected)");
            }
            if (match && match->device.id == info.id
                && (!pairs || pair.firstChannel == saved.firstChannel)) {
                savedListed = true;
            }
            entries.append(entry);
        }
    }

    // The saved choice, when it belongs to this driver and is missing, stays
    // as "<name> (not connected)" (R-AUD-08 to R-AUD-10).
    const bool savedHere = !saved.engine || *saved.engine == engine;
    const bool sameHostApi = !older || saved.driverApi.isEmpty() || hostApi.isEmpty()
        || saved.driverApi == hostApi;
    if (savedHere && sameHostApi && !saved.isPlatformDefault() && !saved.isNone() && !savedListed) {
        AudioDeviceEntry missing;
        missing.deviceId = saved.deviceId;
        missing.label = savedName(saved, direction) + QStringLiteral(" (not connected)");
        missing.pair = AudioChannelPair{saved.firstChannel, 2};
        missing.state = AudioDeviceState::NotConnected;
        entries.append(missing);
    }
    return entries;
}

QString bluetoothMicNote(const QString& name)
{
    return QStringLiteral("Bluetooth headsets switch to phone-call quality, for listening too, "
                          "while they are your mic. For the best sound, listen on %1 and talk "
                          "on a wired or built-in mic.")
        .arg(name);
}

QString audioRoleNote(AudioRole role, const AudioRoleStatus& status)
{
    const bool trouble = status.reason == AudioRoleReason::NotConnected
        || status.reason == AudioRoleReason::InUse;
    QString name = status.chosenName;
    if (name.isEmpty()) {
        name = status.chosen.deviceName;
    }

    if (isOutputRole(role)) {
        if (!trouble || name.isEmpty()) {
            return {};
        }
        if (status.state == AudioRoleState::PlayingOnDefault && !status.playingName.isEmpty()) {
            return QStringLiteral("%1 %2. Playing on the system default, %3, until it comes back.")
                .arg(name, troubleWords(status.reason), status.playingName);
        }
        if (status.state == AudioRoleState::Silent) {
            return QStringLiteral("%1 %2.").arg(name, troubleWords(status.reason));
        }
        return {};
    }

    if (role == AudioRole::TxInput) {
        if (status.state == AudioRoleState::Silent && trouble && !name.isEmpty()) {
            return QStringLiteral("%1 %2. The mic stays silent until it comes back; NereusSDR "
                                  "never switches to another mic on its own.")
                .arg(name, troubleWords(status.reason));
        }
        if (status.state == AudioRoleState::Playing && status.playingBluetooth
            && !status.playingName.isEmpty()) {
            return bluetoothMicNote(status.playingName);
        }
        return {};
    }

    // VAX channels.
    if (status.state == AudioRoleState::Silent && trouble && !name.isEmpty()) {
        return QStringLiteral("%1 %2. VAX %3 stays silent until it comes back; NereusSDR never "
                              "sends it anywhere else.")
            .arg(name, troubleWords(status.reason))
            .arg(vaxNumber(role));
    }
    return {};
}

QString audioDelayLine(AudioRole role, const AudioDelayParts& parts, const QString& deviceName)
{
    const double total = parts.totalMs();
    if (total < 0.0 || deviceName.isEmpty()) {
        return QStringLiteral("Now -- ms");
    }
    const long ms = std::lround(total);
    if (role == AudioRole::TxInput) {
        return QStringLiteral("Now %1 ms from %2 to the radio").arg(ms).arg(deviceName);
    }
    return QStringLiteral("Now %1 ms from the radio to %2").arg(ms).arg(deviceName);
}

bool rescanHasNothingToDo(const IAudioDeviceCatalog& catalogue)
{
    return systemOf(catalogue) == CatalogueSystem::Mac;
}

QString soundSystemDescription(const IAudioDeviceCatalog& catalogue, const QString& asioDriver)
{
    switch (systemOf(catalogue)) {
    case CatalogueSystem::Mac:
        return QStringLiteral("Core Audio");
    case CatalogueSystem::Windows:
        if (!asioDriver.isEmpty()) {
            return QStringLiteral("Windows audio (WASAPI) and ASIO (%1)").arg(asioDriver);
        }
        return QStringLiteral("Windows audio (WASAPI)");
    case CatalogueSystem::Linux:
        if (runningBackend(catalogue, AudioBackendId::PipeWire)) {
            return QStringLiteral("PipeWire. NereusSDR talks to it directly.");
        }
        if (runningBackend(catalogue, AudioBackendId::PulseAudio)) {
            return QStringLiteral("PulseAudio. PipeWire was not found, so NereusSDR talks to "
                                  "PulseAudio directly.");
        }
        return QStringLiteral("None found. Start PipeWire or PulseAudio, then click Rescan devices.");
    case CatalogueSystem::LinuxCore:
        return audioEngineLabel(AudioEngineKind::AlsaDirect);
    case CatalogueSystem::Unknown:
        break;
    }
    return {};
}

bool soundSystemMissing(const IAudioDeviceCatalog& catalogue)
{
    return systemOf(catalogue) == CatalogueSystem::Linux
        && !runningBackend(catalogue, AudioBackendId::PipeWire)
        && !runningBackend(catalogue, AudioBackendId::PulseAudio);
}

QString withOlderDriversInUse(const QString& description, const QStringList& olderDrivers)
{
    if (olderDrivers.isEmpty()) {
        return description;
    }
    QString text = description;
    if (!text.isEmpty() && !text.endsWith(QLatin1Char('.'))) {
        text += QLatin1Char('.');
    }
    if (!text.isEmpty()) {
        text += QLatin1Char(' ');
    }
    return text + QStringLiteral("Older drivers in use: %1.").arg(olderDrivers.join(QStringLiteral(", ")));
}

QString rescanNote(const IAudioDeviceCatalog& catalogue)
{
    const QString older = QStringLiteral("Only the older drivers need this.");
    switch (systemOf(catalogue)) {
    case CatalogueSystem::Mac:
        return QStringLiteral("Core Audio lists update by themselves, so there is nothing to rescan.");
    case CatalogueSystem::Windows:
        return older + QStringLiteral(" Windows audio and ASIO lists update by themselves.");
    case CatalogueSystem::Linux:
        if (runningBackend(catalogue, AudioBackendId::PipeWire)) {
            return older + QStringLiteral(" PipeWire lists update by themselves.");
        }
        if (runningBackend(catalogue, AudioBackendId::PulseAudio)) {
            return older + QStringLiteral(" PulseAudio lists update by themselves.");
        }
        return older;
    case CatalogueSystem::LinuxCore:
    case CatalogueSystem::Unknown:
        break;
    }
    return older;
}

} // namespace NereusSDR
