// =================================================================
// src/core/audio/AudioDeviceMatching.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AudioDeviceMatching.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 4 (R-AUD-04, R-AUD-05). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 fix (R-AUD-04, bug 1): a saved
//               PortAudio ID's host API limits the match to that host API.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AudioDeviceMatching.h"

#include "core/AppSettings.h"
#include "core/audio/PortAudioBackend.h"

#include <QLatin1String>
#include <QRegularExpression>
#include <QStringList>

namespace NereusSDR {

namespace {

// MME cuts device names to 31 characters (spec R-AUD-05, first allowance).
constexpr int kMmeNameLength = 31;

// PortAudio host API names as DeviceCard saved them in DriverApi, plus the
// short forms AppSettings::migrateVaxSchemaV1ToV2 wrote ("WASAPI",
// "CoreAudio", "Pulse").
const QLatin1String kApiMme("MME");
const QLatin1String kApiDirectSound("Windows DirectSound");
const QLatin1String kApiWdmKs("Windows WDM-KS");
const QLatin1String kApiWasapi("Windows WASAPI");
const QLatin1String kApiWasapiShort("WASAPI");
const QLatin1String kApiCoreAudio("Core Audio");
const QLatin1String kApiCoreAudioShort("CoreAudio");
const QLatin1String kApiAlsa("ALSA");
const QLatin1String kApiJack("JACK Audio Connection Kit");
const QLatin1String kApiOss("OSS");

bool isWindowsEngine(AudioEngineKind kind)
{
    return kind == AudioEngineKind::WindowsShared || kind == AudioEngineKind::WindowsExclusive;
}

bool isLinuxEngine(AudioEngineKind kind)
{
    return kind == AudioEngineKind::PipeWire || kind == AudioEngineKind::PulseAudio
        || kind == AudioEngineKind::AlsaDirect;
}

// "USB Audio Device: - (hw:2,0)" gives card 2, device 0.
bool alsaCardAndDevice(const QString& name, int* card, int* device)
{
    static const QRegularExpression kHw(QStringLiteral("\\(hw:(\\d+),(\\d+)\\)$"));
    const QRegularExpressionMatch m = kHw.match(name);
    if (!m.hasMatch()) {
        return false;
    }
    bool okCard = false;
    bool okDevice = false;
    *card = m.captured(1).toInt(&okCard);
    *device = m.captured(2).toInt(&okDevice);
    return okCard && okDevice;
}

// PortAudio's names for the sound server on Linux ALSA (R-AUD-05 row 5,
// settled call 27).  Empty means PortAudio's default device.
bool isAlsaServerName(const QString& name)
{
    return name.isEmpty() || name == QLatin1String("default") || name == QLatin1String("pulse")
        || name == QLatin1String("pipewire");
}

enum class OldChoice {
    NoDriverApi,   // row 1: no DriverApi (or one that never resolved), same device
    Wasapi,        // rows 2 and 3
    CoreAudio,     // row 4
    AlsaServer,    // row 5: ALSA on the sound server, to "(platform default)"
    OlderDriver    // row 6: stays under Older drivers
};

OldChoice classifyOldChoice(const QString& driverApi, const QString& deviceName,
                            const AudioMigrationContext& context)
{
    if (driverApi.isEmpty()) {
        return OldChoice::NoDriverApi;
    }
    if (driverApi == kApiWasapi || driverApi == kApiWasapiShort) {
        // A WASAPI choice copied to another system never reached WASAPI there;
        // it opened PortAudio's default host API, which is row 1.
        return context.windows ? OldChoice::Wasapi : OldChoice::NoDriverApi;
    }
    if (driverApi == kApiCoreAudio || driverApi == kApiCoreAudioShort) {
        return (!context.windows && !context.onLinux) ? OldChoice::CoreAudio
                                                      : OldChoice::NoDriverApi;
    }
    if (driverApi == kApiAlsa) {
        return isAlsaServerName(deviceName) ? OldChoice::AlsaServer : OldChoice::OlderDriver;
    }
    // An explicit MME, DirectSound, WDM-KS or JACK choice stays under Older
    // drivers (settled call 26).
    if (driverApi == kApiMme || driverApi == kApiDirectSound || driverApi == kApiWdmKs
        || driverApi == kApiJack || driverApi == kApiOss) {
        return OldChoice::OlderDriver;
    }
    // Any other value ("ASIO" with PortAudio's ASIO off, "Pulse") matched no
    // PortAudio host API, so the device opened on PortAudio's default host
    // API: the same as no DriverApi.
    return OldChoice::NoDriverApi;
}

} // namespace

std::optional<AudioDeviceMatch> matchSavedAudioDevice(const AudioDeviceConfig& saved,
                                                      const QList<AudioDeviceInfo>& candidates)
{
    if (saved.isNone() || saved.isPlatformDefault()) {
        return std::nullopt;
    }

    const AudioEngineKind engine = saved.engine.value_or(AudioEngineKind::PortAudio);
    const AudioBackendId backend = audioBackendFor(engine);
    const bool portAudio = engine == AudioEngineKind::PortAudio;

    // The same engine only, never across engines (bug 1).  On older drivers
    // the host API is part of the identity; an empty DriverApi is PortAudio's
    // default host API, so any host API is accepted.  A saved PortAudio ID
    // names its host API too (portAudioDeviceId), so neither the ID nor the
    // name ever reaches the same name under another host API.
    QString hostApi = saved.driverApi;
    if (portAudio && hostApi.isEmpty()) {
        hostApi = portAudioHostApiOfId(saved.deviceId);
    }
    QList<AudioDeviceInfo> pool;
    for (const AudioDeviceInfo& info : candidates) {
        if (info.backend != backend) {
            continue;
        }
        if (portAudio && !hostApi.isEmpty() && info.hostApi != hostApi) {
            continue;
        }
        pool.append(info);
    }

    if (!saved.deviceId.isEmpty()) {
        for (const AudioDeviceInfo& info : pool) {
            if (info.id == saved.deviceId) {
                return AudioDeviceMatch{info, true};
            }
        }
    }

    if (saved.deviceName.isEmpty()) {
        return std::nullopt;
    }

    // Name: the first in the system's order (settled call 5).
    for (const AudioDeviceInfo& info : pool) {
        if (info.name == saved.deviceName) {
            return AudioDeviceMatch{info, false};
        }
    }

    if (isWindowsEngine(engine) && saved.deviceName.size() == kMmeNameLength) {
        for (const AudioDeviceInfo& info : pool) {
            if (info.name.startsWith(saved.deviceName)) {
                return AudioDeviceMatch{info, false};
            }
        }
    }

    if (isLinuxEngine(engine)) {
        int card = -1;
        int device = -1;
        if (alsaCardAndDevice(saved.deviceName, &card, &device)) {
            for (const AudioDeviceInfo& info : pool) {
                if (info.alsaCard == card && info.alsaDevice == device) {
                    return AudioDeviceMatch{info, false};
                }
            }
        }
    }

    return std::nullopt;
}

AudioMigrationResult migrateAudioDeviceKeys(const QString& prefix, const AudioMigrationContext& context)
{
    auto& s = AppSettings::instance();
    const QString base = prefix + QLatin1Char('/');
    const QString engineKey = base + QStringLiteral("Engine");

    // Once only: a profile that has Engine is never touched.
    if (s.contains(engineKey)) {
        return AudioMigrationResult::Unchanged;
    }

    // Nothing saved: R-AUD-02's first native choice applies with no keys.
    const QString driverApiKey = base + QStringLiteral("DriverApi");
    const QString deviceNameKey = base + QStringLiteral("DeviceName");
    if (!s.contains(driverApiKey) && !s.contains(deviceNameKey)) {
        return AudioMigrationResult::Unchanged;
    }

    const QString driverApi = s.value(driverApiKey, QString()).toString();
    const QString deviceName = s.value(deviceNameKey, QString()).toString();

    // The Linux Core: ALSA direct is the only engine (settled call 33).  The
    // name is kept and matched later with the "(hw:C,D)" allowance.
    if (context.alsaDirectOnly) {
        s.setValue(engineKey, audioEngineKey(AudioEngineKind::AlsaDirect));
        return AudioMigrationResult::Migrated;
    }

    const OldChoice choice = classifyOldChoice(driverApi, deviceName, context);
    if (choice == OldChoice::OlderDriver) {
        // Every old key unchanged; Engine marks it done.
        s.setValue(engineKey, audioEngineKey(AudioEngineKind::PortAudio));
        return AudioMigrationResult::Unchanged;
    }

    // Every other row moves to a native engine, which must be registered and
    // answering; otherwise try again next start (settled call 23).
    if (!context.nativeEngine.has_value() || !context.nativeRunning) {
        return AudioMigrationResult::Postponed;
    }

    AudioEngineKind target = *context.nativeEngine;
    if (choice == OldChoice::Wasapi) {
        const bool exclusive =
            s.value(base + QStringLiteral("ExclusiveMode"), QStringLiteral("False")).toString()
            == QStringLiteral("True");
        target = exclusive ? AudioEngineKind::WindowsExclusive : AudioEngineKind::WindowsShared;
    } else if (choice == OldChoice::CoreAudio) {
        target = AudioEngineKind::CoreAudio;
    }

    if (choice == OldChoice::AlsaServer) {
        // PortAudio's way to reach the sound server becomes the server's
        // own "(platform default)".
        s.setValue(deviceNameKey, QString());
    }
    s.setValue(engineKey, audioEngineKey(target));
    return AudioMigrationResult::Migrated;
}

void migrateAllAudioDeviceKeys(const AudioMigrationContext& context)
{
    static const QStringList kPrefixes = {
        QStringLiteral("audio/Speakers"), QStringLiteral("audio/Headphones"),
        QStringLiteral("audio/TxInput"),  QStringLiteral("audio/Vax1"),
        QStringLiteral("audio/Vax2"),     QStringLiteral("audio/Vax3"),
        QStringLiteral("audio/Vax4"),
    };
    for (const QString& prefix : kPrefixes) {
        migrateAudioDeviceKeys(prefix, context);
    }
}

} // namespace NereusSDR
