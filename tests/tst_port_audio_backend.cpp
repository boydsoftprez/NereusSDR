// =================================================================
// tests/tst_port_audio_backend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  PortAudio as one engine backend
// (R-AUD-01, R-AUD-06, R-AUD-32): which host APIs it offers under "Older
// drivers" on each system, with and without the host APIs a native engine
// replaces, what each entry carries, and its defaults.  The device list is
// injected; no PortAudio call is made.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 (R-AUD-01, R-AUD-06, R-AUD-32).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: Task 7 fix: ids carry the host API; the same name under two
//               host APIs gives two ids. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/PortAudioBackend.h"

#include <set>

using namespace NereusSDR;

namespace {

const QString kMme = QStringLiteral("MME");
const QString kDirectSound = QStringLiteral("Windows DirectSound");
const QString kWdmKs = QStringLiteral("Windows WDM-KS");
const QString kWasapi = QStringLiteral("Windows WASAPI");
const QString kJack = QStringLiteral("JACK Audio Connection Kit");
const QString kAlsa = QStringLiteral("ALSA");
const QString kCoreAudio = QStringLiteral("Core Audio");

PortAudioDeviceRecord outputRecord(const QString& hostApi, const QString& name,
                                   bool isDefault = false)
{
    PortAudioDeviceRecord record;
    record.hostApi = hostApi;
    record.name = name;
    record.outputChannels = 2;
    record.isDefaultOutput = isDefault;
    return record;
}

PortAudioListFn listOf(QList<PortAudioDeviceRecord> records)
{
    return [records]() { return records; };
}

std::set<QString> hostApisOf(const QList<AudioDeviceInfo>& devices)
{
    std::set<QString> apis;
    for (const AudioDeviceInfo& device : devices) {
        apis.insert(device.hostApi);
    }
    return apis;
}

QList<PortAudioDeviceRecord> windowsRecords()
{
    return {outputRecord(kMme, QStringLiteral("Speakers (MME)")),
            outputRecord(kDirectSound, QStringLiteral("Speakers (DirectSound)")),
            outputRecord(kWdmKs, QStringLiteral("Speakers (WDM-KS)")),
            outputRecord(kWasapi, QStringLiteral("Speakers (WASAPI)"))};
}

} // namespace

class TstPortAudioBackend : public QObject {
    Q_OBJECT

private slots:
    void windowsOffersTheOlderHostApis()
    {
        PortAudioBackend older(listOf(windowsRecords()), OlderDriverPlatform::Windows, false);
        const QList<AudioDeviceInfo> withoutWasapi = older.enumerate();
        QCOMPARE(withoutWasapi.size(), 3);
        QCOMPARE(hostApisOf(withoutWasapi), (std::set<QString>{kMme, kDirectSound, kWdmKs}));

        PortAudioBackend all(listOf(windowsRecords()), OlderDriverPlatform::Windows, true);
        const QList<AudioDeviceInfo> withWasapi = all.enumerate();
        QCOMPARE(withWasapi.size(), 4);
        QCOMPARE(hostApisOf(withWasapi),
                 (std::set<QString>{kMme, kDirectSound, kWdmKs, kWasapi}));
    }

    void linuxOffersJackAndAlsa()
    {
        const QList<PortAudioDeviceRecord> records = {
            outputRecord(kJack, QStringLiteral("system")),
            outputRecord(kAlsa, QStringLiteral("hw:0,0")),
            outputRecord(kCoreAudio, QStringLiteral("Not on Linux"))};
        for (bool includeReplaced : {false, true}) {
            PortAudioBackend backend(listOf(records), OlderDriverPlatform::Linux, includeReplaced);
            const QList<AudioDeviceInfo> devices = backend.enumerate();
            QCOMPARE(devices.size(), 2);
            QCOMPARE(hostApisOf(devices), (std::set<QString>{kJack, kAlsa}));
        }
    }

    void macOffersCoreAudioOnlyWhileNothingReplacesIt()
    {
        const QList<PortAudioDeviceRecord> records = {
            outputRecord(kCoreAudio, QStringLiteral("MacBook Pro Speakers"), true)};
        PortAudioBackend replaced(listOf(records), OlderDriverPlatform::Mac, false);
        QVERIFY(replaced.enumerate().isEmpty());
        QCOMPARE(replaced.defaultDeviceId(AudioDeviceDirection::Output), std::optional<QString>());

        PortAudioBackend kept(listOf(records), OlderDriverPlatform::Mac, true);
        const QList<AudioDeviceInfo> devices = kept.enumerate();
        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices.first().hostApi, kCoreAudio);
    }

    void entriesCarryTheirHostApiInTheirId()
    {
        PortAudioDeviceRecord duplex;
        duplex.hostApi = kMme;
        duplex.name = QStringLiteral("USB Audio CODEC");
        duplex.outputChannels = 2;
        duplex.inputChannels = 1;
        PortAudioBackend backend(listOf({duplex}), OlderDriverPlatform::Windows, false);
        const QList<AudioDeviceInfo> devices = backend.enumerate();
        QCOMPARE(devices.size(), 2);   // one output entry, one input entry
        for (const AudioDeviceInfo& device : devices) {
            QCOMPARE(device.backend, AudioBackendId::PortAudio);
            QCOMPARE(device.id, QStringLiteral("MME|USB Audio CODEC"));
            QCOMPARE(device.name, QStringLiteral("USB Audio CODEC"));
            QCOMPARE(device.hostApi, kMme);
        }
        QCOMPARE(devices.at(0).direction, AudioDeviceDirection::Output);
        QCOMPARE(devices.at(0).channelCount, 2);
        QCOMPARE(devices.at(1).direction, AudioDeviceDirection::Input);
        QCOMPARE(devices.at(1).channelCount, 1);
    }

    void sameNameUnderTwoHostApisGivesTwoIds()
    {
        const QString realtek = QStringLiteral("Speakers (Realtek(R) Audio)");
        PortAudioBackend backend(listOf({outputRecord(kMme, realtek), outputRecord(kDirectSound, realtek)}),
                                 OlderDriverPlatform::Windows, false);
        const QList<AudioDeviceInfo> devices = backend.enumerate();
        QCOMPARE(devices.size(), 2);
        QVERIFY(devices.at(0).id != devices.at(1).id);
        QCOMPARE(devices.at(0).id, portAudioDeviceId(kMme, realtek));
        QCOMPARE(devices.at(1).id, portAudioDeviceId(kDirectSound, realtek));
        for (const AudioDeviceInfo& device : devices) {
            QCOMPARE(device.name, realtek);
            QCOMPARE(portAudioHostApiOfId(device.id), device.hostApi);
            QCOMPARE(portAudioNameOfId(device.id), realtek);
        }
    }

    void defaultsAreTheFlaggedRecords()
    {
        PortAudioDeviceRecord mic;
        mic.hostApi = kAlsa;
        mic.name = QStringLiteral("USB Mic");
        mic.inputChannels = 1;
        mic.isDefaultInput = true;
        const QList<PortAudioDeviceRecord> records = {
            outputRecord(kAlsa, QStringLiteral("hw:0,0")),
            outputRecord(kAlsa, QStringLiteral("default"), true), mic};
        PortAudioBackend backend(listOf(records), OlderDriverPlatform::Linux, false);
        const QList<AudioDeviceInfo> devices = backend.enumerate();
        QCOMPARE(backend.defaultDeviceId(AudioDeviceDirection::Output),
                 std::optional<QString>(QStringLiteral("ALSA|default")));
        QCOMPARE(backend.defaultDeviceId(AudioDeviceDirection::Input),
                 std::optional<QString>(QStringLiteral("ALSA|USB Mic")));
        int defaults = 0;
        for (const AudioDeviceInfo& device : devices) {
            if (device.isDefault) {
                ++defaults;
            }
        }
        QCOMPARE(defaults, 2);

        // A default on a host API that is not offered is no default here.
        PortAudioBackend mac(listOf({outputRecord(kCoreAudio, QStringLiteral("Speakers"), true)}),
                             OlderDriverPlatform::Linux, true);
        QVERIFY(mac.enumerate().isEmpty());
        QCOMPARE(mac.defaultDeviceId(AudioDeviceDirection::Output), std::optional<QString>());
    }

    void isTheOlderDriversEngine()
    {
        PortAudioBackend backend(listOf({}), OlderDriverPlatform::Windows, false);
        QCOMPARE(backend.id(), AudioBackendId::PortAudio);
        QVERIFY(backend.running());
        QVERIFY(backend.enumerate().isEmpty());
        // The PC microphone is captured by the helper process.
        QVERIFY(backend.createInput(AudioStreamRequest{}, MicChannelPick::Left, nullptr) == nullptr);
        QCOMPARE(olderDriverHostApis(OlderDriverPlatform::Windows, false),
                 (QStringList{kMme, kDirectSound, kWdmKs}));
        QCOMPARE(olderDriverHostApis(OlderDriverPlatform::Linux, false), (QStringList{kJack, kAlsa}));
        QVERIFY(olderDriverHostApis(OlderDriverPlatform::Mac, false).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstPortAudioBackend)
#include "tst_port_audio_backend.moc"
