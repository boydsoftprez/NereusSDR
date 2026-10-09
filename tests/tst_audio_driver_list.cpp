// =================================================================
// tests/tst_audio_driver_list.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The Setup device cards' Driver
// list, Device list, state notes, delay readout, Sound system words and
// Rescan note (native audio plan Task 16; R-AUD-01, R-AUD-03, R-AUD-06,
// R-AUD-08 to R-AUD-11, R-AUD-14, R-AUD-15), as pure functions over a
// fake device catalogue: the fake's backends decide the system, never
// the build's.  No device is touched.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 16. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioDeviceConfig.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/PortAudioBackend.h"
#include "gui/setup/AudioDriverList.h"

#include <utility>

using namespace NereusSDR;

namespace {

// A catalogue whose backends, running flags and devices a test sets.
class FakeCatalogue final : public IAudioDeviceCatalog {
public:
    FakeCatalogue(QList<AudioBackendId> backends, QList<AudioBackendId> stopped = {})
        : m_backends(std::move(backends))
        , m_stopped(std::move(stopped))
    {
    }

    QList<AudioBackendId> backends() const override { return m_backends; }
    bool backendRunning(AudioBackendId id) const override
    {
        return m_backends.contains(id) && !m_stopped.contains(id);
    }
    QList<AudioDeviceInfo> devices(AudioBackendId id, AudioDeviceDirection direction) const override
    {
        QList<AudioDeviceInfo> out;
        for (const AudioDeviceInfo& d : m_devices) {
            if (d.backend == id && d.direction == direction) {
                out.append(d);
            }
        }
        return out;
    }
    std::optional<AudioDeviceInfo> defaultDevice(AudioBackendId id,
                                                 AudioDeviceDirection direction) const override
    {
        for (const AudioDeviceInfo& d : devices(id, direction)) {
            if (d.isDefault) {
                return d;
            }
        }
        return std::nullopt;
    }
    void rescanOlderDrivers() override {}

    void add(const AudioDeviceInfo& info) { m_devices.append(info); }

private:
    QList<AudioBackendId> m_backends;
    QList<AudioBackendId> m_stopped;
    QList<AudioDeviceInfo> m_devices;
};

AudioDeviceInfo device(AudioBackendId backend, AudioDeviceDirection direction, const QString& id,
                       const QString& name, const QString& hostApi = {})
{
    AudioDeviceInfo info;
    info.backend = backend;
    info.direction = direction;
    info.id = id;
    info.name = name;
    info.hostApi = hostApi;
    info.channelCount = 2;
    return info;
}

QStringList labels(const QList<AudioDriverEntry>& entries)
{
    QStringList out;
    for (const AudioDriverEntry& e : entries) {
        out << e.label;
    }
    return out;
}

QStringList labels(const QList<AudioDeviceEntry>& entries)
{
    QStringList out;
    for (const AudioDeviceEntry& e : entries) {
        out << e.label;
    }
    return out;
}

constexpr auto kOut = AudioDeviceDirection::Output;
constexpr auto kIn = AudioDeviceDirection::Input;

AudioRoleStatus status(AudioRoleState state, AudioRoleReason reason, const QString& chosenName,
                       const QString& playingName, bool bluetooth = false)
{
    AudioRoleStatus s;
    s.state = state;
    s.reason = reason;
    s.chosenName = chosenName;
    s.playingName = playingName;
    s.playingBluetooth = bluetooth;
    return s;
}

} // namespace

class TstAudioDriverList : public QObject {
    Q_OBJECT

private slots:
    // ── R-AUD-01: the Driver list per system ─────────────────────────────
    void macListsCoreAudioOnly()
    {
        FakeCatalogue mac({AudioBackendId::CoreAudio, AudioBackendId::PortAudio});
        mac.add(device(AudioBackendId::PortAudio, kOut, QStringLiteral("Core Audio|Desk"),
                       QStringLiteral("Desk"), QStringLiteral("Core Audio")));
        const QList<AudioDriverEntry> entries = audioDriverEntries(mac, false);
        QCOMPARE(labels(entries), QStringList{QStringLiteral("Core Audio")});
        QCOMPARE(entries.first().engine, std::optional<AudioEngineKind>(AudioEngineKind::CoreAudio));
        QVERIFY(!entries.first().enabled);
        QCOMPARE(entries.first().disabledReason, QStringLiteral("The only sound system on the Mac."));
    }

    void windowsListsItsEnginesThenTheOlderDrivers()
    {
        FakeCatalogue win({AudioBackendId::Wasapi, AudioBackendId::Asio, AudioBackendId::PortAudio});
        const QList<AudioDriverEntry> entries = audioDriverEntries(win, false);
        QCOMPARE(labels(entries),
                 (QStringList{QStringLiteral("Windows audio, shared"),
                              QStringLiteral("Windows audio, exclusive"), QStringLiteral("ASIO"),
                              QStringLiteral("Older drivers"), QStringLiteral("MME"),
                              QStringLiteral("DirectSound"), QStringLiteral("WDM-KS")}));
        QCOMPARE(entries.at(0).engine, std::optional<AudioEngineKind>(AudioEngineKind::WindowsShared));
        QCOMPARE(entries.at(1).engine,
                 std::optional<AudioEngineKind>(AudioEngineKind::WindowsExclusive));
        QCOMPARE(entries.at(2).engine, std::optional<AudioEngineKind>(AudioEngineKind::Asio));
        // The heading is a row, never a choice.
        QVERIFY(!entries.at(3).engine);
        QVERIFY(!entries.at(3).enabled);
        for (int i = 4; i < entries.size(); ++i) {
            QCOMPARE(entries.at(i).engine, std::optional<AudioEngineKind>(AudioEngineKind::PortAudio));
            QVERIFY(entries.at(i).enabled);
        }
        QCOMPARE(entries.at(4).hostApi, QStringLiteral("MME"));
        QCOMPARE(entries.at(5).hostApi, QStringLiteral("Windows DirectSound"));
        QCOMPARE(entries.at(6).hostApi, QStringLiteral("Windows WDM-KS"));
        for (int i = 0; i < 3; ++i) {
            QVERIFY(entries.at(i).enabled);
        }
    }

    // Disabled, never hidden: ASIO without a driver stays in the list.
    void windowsWithoutAsioGreysIt()
    {
        FakeCatalogue win({AudioBackendId::Wasapi, AudioBackendId::PortAudio});
        const QList<AudioDriverEntry> entries = audioDriverEntries(win, false);
        QCOMPARE(entries.at(2).label, QStringLiteral("ASIO"));
        QVERIFY(!entries.at(2).enabled);
        QCOMPARE(entries.at(2).disabledReason,
                 QStringLiteral("No ASIO driver is installed on this computer."));
    }

    void linuxWithPipeWire()
    {
        FakeCatalogue system({AudioBackendId::PipeWire, AudioBackendId::PulseAudio,
                             AudioBackendId::PortAudio});
        const QList<AudioDriverEntry> entries = audioDriverEntries(system, false);
        QCOMPARE(labels(entries),
                 (QStringList{QStringLiteral("PipeWire"), QStringLiteral("Older drivers"),
                              QStringLiteral("JACK"), QStringLiteral("ALSA")}));
        QVERIFY(entries.at(0).enabled);
        QCOMPARE(entries.at(2).hostApi, QStringLiteral("JACK Audio Connection Kit"));
        QCOMPARE(entries.at(3).hostApi, QStringLiteral("ALSA"));
    }

    void linuxWithPulseAudio()
    {
        FakeCatalogue system({AudioBackendId::PipeWire, AudioBackendId::PulseAudio,
                             AudioBackendId::PortAudio},
                            {AudioBackendId::PipeWire});
        const QList<AudioDriverEntry> entries = audioDriverEntries(system, false);
        QCOMPARE(labels(entries),
                 (QStringList{QStringLiteral("PipeWire (not running)"), QStringLiteral("PulseAudio"),
                              QStringLiteral("Older drivers"), QStringLiteral("JACK"),
                              QStringLiteral("ALSA")}));
        QVERIFY(!entries.at(0).enabled);
        QCOMPARE(entries.at(0).disabledReason,
                 QStringLiteral("PipeWire is not running on this computer."));
        QVERIFY(entries.at(1).enabled);
    }

    void linuxWithNeither()
    {
        FakeCatalogue system({AudioBackendId::PipeWire, AudioBackendId::PulseAudio,
                             AudioBackendId::PortAudio},
                            {AudioBackendId::PipeWire, AudioBackendId::PulseAudio});
        const QList<AudioDriverEntry> entries = audioDriverEntries(system, false);
        QCOMPARE(labels(entries),
                 (QStringList{QStringLiteral("PipeWire (not running)"),
                              QStringLiteral("PulseAudio (not running)"),
                              QStringLiteral("Older drivers"), QStringLiteral("JACK"),
                              QStringLiteral("ALSA")}));
        QVERIFY(!entries.at(0).enabled);
        QVERIFY(!entries.at(1).enabled);
        QCOMPARE(entries.at(1).disabledReason,
                 QStringLiteral("PulseAudio is not running on this computer."));
    }

    void theCorePlaysStraightToTheCard()
    {
        FakeCatalogue core({AudioBackendId::PipeWire, AudioBackendId::PortAudio});
        const QList<AudioDriverEntry> entries = audioDriverEntries(core, true);
        QCOMPARE(labels(entries), QStringList{QStringLiteral("ALSA, direct")});
        QVERIFY(!entries.first().enabled);
        QCOMPARE(entries.first().disabledReason,
                 QStringLiteral("The Core runs without a desktop, so it plays straight to "
                                "the sound card."));
        // A catalogue of ALSA direct alone is the Core's too.
        FakeCatalogue alsa({AudioBackendId::AlsaDirect});
        QCOMPARE(labels(audioDriverEntries(alsa, false)), QStringList{QStringLiteral("ALSA, direct")});
    }

    // ── R-AUD-03: the Device list ────────────────────────────────────────
    void deviceListStartsWithThePlatformDefault()
    {
        FakeCatalogue mac({AudioBackendId::CoreAudio});
        mac.add(device(AudioBackendId::CoreAudio, kOut, QStringLiteral("desk-uid"),
                       QStringLiteral("Desk speakers")));
        AudioDeviceInfo busy = device(AudioBackendId::CoreAudio, kOut, QStringLiteral("usb-uid"),
                                      QStringLiteral("USB DAC"));
        busy.state = AudioDeviceState::InUse;
        mac.add(busy);
        const QList<AudioDeviceEntry> entries =
            audioDeviceEntries(mac, AudioEngineKind::CoreAudio, QString(), kOut, AudioDeviceConfig{});
        QCOMPARE(labels(entries),
                 (QStringList{QStringLiteral("(platform default)"), QStringLiteral("Desk speakers"),
                              QStringLiteral("USB DAC (in use by another program)")}));
        QVERIFY(entries.first().deviceId.isEmpty());
        QCOMPARE(entries.at(2).state, AudioDeviceState::InUse);
        QCOMPARE(entries.at(2).deviceId, QStringLiteral("usb-uid"));
    }

    void missingSavedDeviceStaysAsNotConnected()
    {
        FakeCatalogue mac({AudioBackendId::CoreAudio});
        mac.add(device(AudioBackendId::CoreAudio, kOut, QStringLiteral("desk-uid"),
                       QStringLiteral("Desk speakers")));
        AudioDeviceConfig saved;
        saved.engine = AudioEngineKind::CoreAudio;
        saved.deviceId = QStringLiteral("airpods-uid");
        saved.deviceName = QStringLiteral("AirPods Pro");
        const QList<AudioDeviceEntry> entries =
            audioDeviceEntries(mac, AudioEngineKind::CoreAudio, QString(), kOut, saved);
        QCOMPARE(entries.last().label, QStringLiteral("AirPods Pro (not connected)"));
        QCOMPARE(entries.last().deviceId, QStringLiteral("airpods-uid"));
        QCOMPARE(entries.last().state, AudioDeviceState::NotConnected);
        QCOMPARE(entries.size(), 3);

        // Listed: no extra entry.
        saved.deviceId = QStringLiteral("desk-uid");
        saved.deviceName = QStringLiteral("Desk speakers");
        QCOMPARE(audioDeviceEntries(mac, AudioEngineKind::CoreAudio, QString(), kOut, saved).size(),
                 2);
        // Listed under another id, by name ("Saved identity"): no extra entry.
        saved.deviceId = QStringLiteral("old-desk-uid");
        QCOMPARE(audioDeviceEntries(mac, AudioEngineKind::CoreAudio, QString(), kOut, saved).size(),
                 2);
    }

    void savedNoneShowsAsNone()
    {
        FakeCatalogue mac({AudioBackendId::CoreAudio});
        AudioDeviceConfig saved;
        saved.engine = AudioEngineKind::CoreAudio;
        saved.deviceId = QString::fromLatin1(kAudioDeviceNone);
        saved.deviceName = QString::fromLatin1(kAudioDeviceNone);
        QCOMPARE(labels(audioDeviceEntries(mac, AudioEngineKind::CoreAudio, QString(), kOut, saved)),
                 (QStringList{QStringLiteral("(platform default)"), QStringLiteral("(none)")}));
    }

    void interfacesListTheirPairsAndBluetoothIsMarked()
    {
        FakeCatalogue mac({AudioBackendId::CoreAudio});
        AudioDeviceInfo interface = device(AudioBackendId::CoreAudio, kOut, QStringLiteral("rme"),
                                           QStringLiteral("Fireface"));
        interface.channelCount = 4;
        mac.add(interface);
        AudioDeviceInfo headset = device(AudioBackendId::CoreAudio, kIn, QStringLiteral("bt"),
                                         QStringLiteral("AirPods Pro"));
        headset.transport = AudioTransport::Bluetooth;
        mac.add(headset);
        const QList<AudioDeviceEntry> outs =
            audioDeviceEntries(mac, AudioEngineKind::CoreAudio, QString(), kOut, AudioDeviceConfig{});
        QCOMPARE(labels(outs),
                 (QStringList{QStringLiteral("(platform default)"),
                              QStringLiteral("Fireface · Outputs 1-2"),
                              QStringLiteral("Fireface · Outputs 3-4")}));
        QCOMPARE(outs.at(2).group, QStringLiteral("Fireface"));
        QCOMPARE(outs.at(2).pair.firstChannel, 3);
        const QList<AudioDeviceEntry> ins =
            audioDeviceEntries(mac, AudioEngineKind::CoreAudio, QString(), kIn, AudioDeviceConfig{});
        QVERIFY(ins.at(1).bluetooth);
        QVERIFY(!outs.at(1).bluetooth);
    }

    void olderDriversListTheirHostApiOnly()
    {
        FakeCatalogue win({AudioBackendId::Wasapi, AudioBackendId::PortAudio});
        win.add(device(AudioBackendId::PortAudio, kOut, portAudioDeviceId(QStringLiteral("MME"),
                                                                          QStringLiteral("Speakers")),
                       QStringLiteral("Speakers"), QStringLiteral("MME")));
        win.add(device(AudioBackendId::PortAudio, kOut,
                       portAudioDeviceId(QStringLiteral("Windows WDM-KS"), QStringLiteral("Speakers")),
                       QStringLiteral("Speakers"), QStringLiteral("Windows WDM-KS")));
        const QList<AudioDeviceEntry> mme = audioDeviceEntries(
            win, AudioEngineKind::PortAudio, QStringLiteral("MME"), kOut, AudioDeviceConfig{});
        QCOMPARE(labels(mme),
                 (QStringList{QStringLiteral("(platform default)"), QStringLiteral("Speakers")}));
        QCOMPARE(mme.at(1).deviceId, QStringLiteral("MME|Speakers"));
    }

    // ── R-AUD-08 to R-AUD-11, R-AUD-14: the state notes ──────────────────
    void stateNotes()
    {
        const QString missingOut = QStringLiteral(
            "AirPods Pro is not connected. Playing on the system default, MacBook Pro Speakers, "
            "until it comes back.");
        QCOMPARE(audioRoleNote(AudioRole::Speakers,
                               status(AudioRoleState::PlayingOnDefault, AudioRoleReason::NotConnected,
                                      QStringLiteral("AirPods Pro"),
                                      QStringLiteral("MacBook Pro Speakers"))),
                 missingOut);
        QCOMPARE(audioRoleNote(AudioRole::Headphones,
                               status(AudioRoleState::PlayingOnDefault, AudioRoleReason::InUse,
                                      QStringLiteral("USB DAC"),
                                      QStringLiteral("MacBook Pro Speakers"))),
                 QStringLiteral("USB DAC is in use by another program. Playing on the system "
                                "default, MacBook Pro Speakers, until it comes back."));
        QCOMPARE(audioRoleNote(AudioRole::TxInput,
                               status(AudioRoleState::Silent, AudioRoleReason::NotConnected,
                                      QStringLiteral("USB Mic"), QString())),
                 QStringLiteral("USB Mic is not connected. The mic stays silent until it comes "
                                "back; NereusSDR never switches to another mic on its own."));
        QCOMPARE(audioRoleNote(AudioRole::TxInput,
                               status(AudioRoleState::Silent, AudioRoleReason::InUse,
                                      QStringLiteral("USB Mic"), QString())),
                 QStringLiteral("USB Mic is in use by another program. The mic stays silent "
                                "until it comes back; NereusSDR never switches to another mic "
                                "on its own."));
        QCOMPARE(audioRoleNote(AudioRole::TxInput,
                               status(AudioRoleState::Playing, AudioRoleReason::None,
                                      QStringLiteral("AirPods Pro"), QStringLiteral("AirPods Pro"),
                                      true)),
                 QStringLiteral("Bluetooth headsets switch to phone-call quality, for listening "
                                "too, while they are your mic. For the best sound, listen on "
                                "AirPods Pro and talk on a wired or built-in mic."));
        // Playing as chosen: no note.
        QVERIFY(audioRoleNote(AudioRole::Speakers,
                              status(AudioRoleState::Playing, AudioRoleReason::None,
                                     QStringLiteral("AirPods Pro"), QStringLiteral("AirPods Pro")))
                    .isEmpty());
        QVERIFY(audioRoleNote(AudioRole::TxInput,
                              status(AudioRoleState::Playing, AudioRoleReason::None,
                                     QStringLiteral("USB Mic"), QStringLiteral("USB Mic")))
                    .isEmpty());
        // The name comes from the chosen config when the status has none.
        AudioRoleStatus fromConfig = status(AudioRoleState::Silent, AudioRoleReason::NotConnected,
                                            QString(), QString());
        fromConfig.chosen.deviceName = QStringLiteral("USB Mic");
        QVERIFY(audioRoleNote(AudioRole::TxInput, fromConfig).startsWith(
            QStringLiteral("USB Mic is not connected.")));
    }

    // ── R-AUD-15: the delay readout ──────────────────────────────────────
    void delayLine()
    {
        AudioDelayParts parts;
        parts.matcherFillMs = 6.4;
        parts.resamplerMs = 0.5;
        parts.deviceBufferMs = 2.7;
        parts.deviceLatencyMs = 4.0;   // 13.6 in all
        QCOMPARE(audioDelayLine(AudioRole::Speakers, parts, QStringLiteral("Desk speakers")),
                 QStringLiteral("Now 14 ms from the radio to Desk speakers"));
        QCOMPARE(audioDelayLine(AudioRole::TxInput, parts, QStringLiteral("USB Mic")),
                 QStringLiteral("Now 14 ms from USB Mic to the radio"));
        parts.deviceLatencyMs = 3.8;   // 13.4
        QCOMPARE(audioDelayLine(AudioRole::Headphones, parts, QStringLiteral("Phones")),
                 QStringLiteral("Now 13 ms from the radio to Phones"));
        QCOMPARE(audioDelayLine(AudioRole::Speakers, AudioDelayParts{}, QStringLiteral("Desk")),
                 QStringLiteral("Now -- ms"));
        QCOMPARE(audioDelayLine(AudioRole::Speakers, parts, QString()), QStringLiteral("Now -- ms"));
    }

    // ── R-AUD-06: Rescan devices ─────────────────────────────────────────
    void rescanNotes()
    {
        FakeCatalogue mac({AudioBackendId::CoreAudio});
        QCOMPARE(rescanNote(mac),
                 QStringLiteral("Core Audio lists update by themselves, so there is nothing to "
                                "rescan."));
        QVERIFY(rescanHasNothingToDo(mac));
        FakeCatalogue win({AudioBackendId::Wasapi, AudioBackendId::PortAudio});
        QCOMPARE(rescanNote(win), QStringLiteral("Only the older drivers need this. Windows audio "
                                                 "and ASIO lists update by themselves."));
        QVERIFY(!rescanHasNothingToDo(win));
        FakeCatalogue pipeWire({AudioBackendId::PipeWire, AudioBackendId::PortAudio});
        QCOMPARE(rescanNote(pipeWire), QStringLiteral("Only the older drivers need this. PipeWire "
                                                      "lists update by themselves."));
        FakeCatalogue pulse({AudioBackendId::PipeWire, AudioBackendId::PulseAudio,
                             AudioBackendId::PortAudio},
                            {AudioBackendId::PipeWire});
        QCOMPARE(rescanNote(pulse), QStringLiteral("Only the older drivers need this. PulseAudio "
                                                   "lists update by themselves."));
    }

    // ── R-AUD-01: the Sound system line ──────────────────────────────────
    void soundSystemWords()
    {
        FakeCatalogue win({AudioBackendId::Wasapi, AudioBackendId::PortAudio});
        QCOMPARE(soundSystemDescription(win, QString()), QStringLiteral("Windows audio (WASAPI)"));
        QCOMPARE(soundSystemDescription(win, QStringLiteral("Focusrite USB ASIO")),
                 QStringLiteral("Windows audio (WASAPI) and ASIO (Focusrite USB ASIO)"));
        FakeCatalogue mac({AudioBackendId::CoreAudio});
        QCOMPARE(soundSystemDescription(mac, QString()), QStringLiteral("Core Audio"));
        FakeCatalogue pipeWire({AudioBackendId::PipeWire, AudioBackendId::PortAudio});
        QCOMPARE(soundSystemDescription(pipeWire, QString()),
                 QStringLiteral("PipeWire. NereusSDR talks to it directly."));
        QVERIFY(!soundSystemMissing(pipeWire));
        FakeCatalogue pulse({AudioBackendId::PipeWire, AudioBackendId::PulseAudio},
                            {AudioBackendId::PipeWire});
        QCOMPARE(soundSystemDescription(pulse, QString()),
                 QStringLiteral("PulseAudio. PipeWire was not found, so NereusSDR talks to "
                                "PulseAudio directly."));
        FakeCatalogue neither({AudioBackendId::PipeWire, AudioBackendId::PulseAudio},
                              {AudioBackendId::PipeWire, AudioBackendId::PulseAudio});
        QVERIFY(soundSystemMissing(neither));
        FakeCatalogue unknown({AudioBackendId::PortAudio});
        QVERIFY(soundSystemDescription(unknown, QString()).isEmpty());

        QCOMPARE(withOlderDriversInUse(QStringLiteral("Windows audio (WASAPI)"),
                                       {QStringLiteral("MME"), QStringLiteral("WDM-KS")}),
                 QStringLiteral("Windows audio (WASAPI). Older drivers in use: MME, WDM-KS."));
        QCOMPARE(withOlderDriversInUse(QStringLiteral("PipeWire. NereusSDR talks to it directly."),
                                       {QStringLiteral("JACK")}),
                 QStringLiteral("PipeWire. NereusSDR talks to it directly. Older drivers in use: "
                                "JACK."));
        QCOMPARE(withOlderDriversInUse(QStringLiteral("Core Audio"), {}), QStringLiteral("Core Audio"));
        QCOMPARE(olderDriverDisplayName(QStringLiteral("Windows DirectSound")),
                 QStringLiteral("DirectSound"));
        QCOMPARE(olderDriverDisplayName(QStringLiteral("JACK Audio Connection Kit")),
                 QStringLiteral("JACK"));
    }
};

QTEST_GUILESS_MAIN(TstAudioDriverList)
#include "tst_audio_driver_list.moc"
