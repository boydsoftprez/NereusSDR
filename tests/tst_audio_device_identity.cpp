// =================================================================
// tests/tst_audio_device_identity.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// V-SW-2: matching a saved device against a live list (R-AUD-04, spec
// "Saved identity").  ID first, then name within the same engine only, with
// the two allowances (Windows 31-character names, Linux "(hw:C,D)").  Bug 1:
// a Windows audio choice never matches an MME entry of the same name.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 4. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioDeviceConfig.h"
#include "core/audio/AudioDeviceMatching.h"

using namespace NereusSDR;

namespace {

AudioDeviceInfo device(AudioBackendId backend, const QString& id, const QString& name,
                       const QString& hostApi = QString())
{
    AudioDeviceInfo info;
    info.backend = backend;
    info.id = id;
    info.name = name;
    info.hostApi = hostApi;
    return info;
}

AudioDeviceConfig saved(AudioEngineKind engine, const QString& id, const QString& name,
                        const QString& driverApi = QString())
{
    AudioDeviceConfig cfg;
    cfg.engine = engine;
    cfg.deviceId = id;
    cfg.deviceName = name;
    cfg.driverApi = driverApi;
    return cfg;
}

const QString kRealtek = QStringLiteral("Speakers (Realtek(R) Audio)");

} // namespace

class TstAudioDeviceIdentity : public QObject {
    Q_OBJECT

private slots:

    void idMatchWinsOverNameOfAnotherDevice()
    {
        const QList<AudioDeviceInfo> list = {
            device(AudioBackendId::CoreAudio, QStringLiteral("uid-other"), QStringLiteral("USB Audio")),
            device(AudioBackendId::CoreAudio, QStringLiteral("uid-mine"), QStringLiteral("Renamed Interface")),
        };
        const auto m = matchSavedAudioDevice(
            saved(AudioEngineKind::CoreAudio, QStringLiteral("uid-mine"), QStringLiteral("USB Audio")), list);
        QVERIFY(m.has_value());
        QCOMPARE(m->device.id, QStringLiteral("uid-mine"));
        QVERIFY(m->byId);
    }

    void nameNeverCrossesEngines()
    {
        // Bug 1: a saved WindowsShared choice never reopens on MME.
        const QList<AudioDeviceInfo> list = {
            device(AudioBackendId::PortAudio, QStringLiteral("MME:Speakers"), kRealtek, QStringLiteral("MME")),
        };
        QVERIFY(!matchSavedAudioDevice(saved(AudioEngineKind::WindowsShared, QString(), kRealtek), list)
                     .has_value());
        QVERIFY(!matchSavedAudioDevice(
                     saved(AudioEngineKind::WindowsShared, QStringLiteral("{0.0.0.00000000}.{abc}"), kRealtek),
                     list)
                     .has_value());

        // And the endpoint is found once it is in the list, after the MME entry.
        QList<AudioDeviceInfo> both = list;
        both.append(device(AudioBackendId::Wasapi, QStringLiteral("{0.0.0.00000000}.{abc}"), kRealtek));
        const auto m = matchSavedAudioDevice(saved(AudioEngineKind::WindowsShared, QString(), kRealtek), both);
        QVERIFY(m.has_value());
        QCOMPARE(m->device.backend, AudioBackendId::Wasapi);
        QVERIFY(!m->byId);
    }

    void wasapiKindsShareTheEndpointList()
    {
        const QList<AudioDeviceInfo> list = {
            device(AudioBackendId::Wasapi, QStringLiteral("ep-1"), kRealtek),
        };
        const auto m = matchSavedAudioDevice(saved(AudioEngineKind::WindowsExclusive, QStringLiteral("ep-1"), kRealtek), list);
        QVERIFY(m.has_value());
        QVERIFY(m->byId);
    }

    void sameNameDifferentIdsPickedById()
    {
        const QList<AudioDeviceInfo> list = {
            device(AudioBackendId::PipeWire, QStringLiteral("alsa_output.usb-A"), QStringLiteral("USB Audio")),
            device(AudioBackendId::PipeWire, QStringLiteral("alsa_output.usb-B"), QStringLiteral("USB Audio")),
        };
        const auto a = matchSavedAudioDevice(
            saved(AudioEngineKind::PipeWire, QStringLiteral("alsa_output.usb-A"), QStringLiteral("USB Audio")), list);
        const auto b = matchSavedAudioDevice(
            saved(AudioEngineKind::PipeWire, QStringLiteral("alsa_output.usb-B"), QStringLiteral("USB Audio")), list);
        QVERIFY(a.has_value());
        QVERIFY(b.has_value());
        QCOMPARE(a->device.id, QStringLiteral("alsa_output.usb-A"));
        QCOMPARE(b->device.id, QStringLiteral("alsa_output.usb-B"));
        QVERIFY(a->byId);
        QVERIFY(b->byId);
    }

    void noIdMatchTakesFirstByNameInListOrder()
    {
        const QList<AudioDeviceInfo> list = {
            device(AudioBackendId::PipeWire, QStringLiteral("other"), QStringLiteral("HDMI")),
            device(AudioBackendId::PipeWire, QStringLiteral("second"), QStringLiteral("USB Audio")),
            device(AudioBackendId::PipeWire, QStringLiteral("third"), QStringLiteral("USB Audio")),
        };
        // A stale ID and an absent ID both fall back to the first name match.
        for (const QString& id : {QStringLiteral("gone"), QString()}) {
            const auto m = matchSavedAudioDevice(saved(AudioEngineKind::PipeWire, id, QStringLiteral("USB Audio")), list);
            QVERIFY(m.has_value());
            QCOMPARE(m->device.id, QStringLiteral("second"));
            QVERIFY(!m->byId);
        }
    }

    void portAudioNeedsSameHostApiAndName()
    {
        const QList<AudioDeviceInfo> list = {
            device(AudioBackendId::PortAudio, QStringLiteral("ds"), kRealtek, QStringLiteral("Windows DirectSound")),
            device(AudioBackendId::PortAudio, QStringLiteral("mme"), kRealtek, QStringLiteral("MME")),
            device(AudioBackendId::Wasapi, QStringLiteral("ep"), kRealtek),
        };
        const auto m = matchSavedAudioDevice(
            saved(AudioEngineKind::PortAudio, QString(), kRealtek, QStringLiteral("MME")), list);
        QVERIFY(m.has_value());
        QCOMPARE(m->device.id, QStringLiteral("mme"));
        QVERIFY(!m->byId);

        QVERIFY(!matchSavedAudioDevice(
                     saved(AudioEngineKind::PortAudio, QString(), kRealtek, QStringLiteral("Windows WDM-KS")), list)
                     .has_value());
        QVERIFY(!matchSavedAudioDevice(
                     saved(AudioEngineKind::PortAudio, QString(), QStringLiteral("Other"), QStringLiteral("MME")), list)
                     .has_value());
        // An ID of another host API does not match either.
        QVERIFY(!matchSavedAudioDevice(
                     saved(AudioEngineKind::PortAudio, QStringLiteral("ds"), QStringLiteral("Other"), QStringLiteral("MME")), list)
                     .has_value());
    }

    void unmigratedConfigMatchesAsOlderDrivers()
    {
        AudioDeviceConfig cfg;
        cfg.driverApi = QStringLiteral("MME");
        cfg.deviceName = kRealtek;
        const QList<AudioDeviceInfo> list = {
            device(AudioBackendId::Wasapi, QStringLiteral("ep"), kRealtek),
            device(AudioBackendId::PortAudio, QStringLiteral("mme"), kRealtek, QStringLiteral("MME")),
        };
        const auto m = matchSavedAudioDevice(cfg, list);
        QVERIFY(m.has_value());
        QCOMPARE(m->device.id, QStringLiteral("mme"));
    }

    void windowsThirtyOneCharacterNameMatchesPrefix()
    {
        // MME cuts names to 31 characters; the cut matches the endpoint
        // whose name starts with it.
        const QString full = QStringLiteral("Speakers (Focusrite USB Audio 2nd Gen)");
        const QString saved31 = full.left(31);
        QCOMPARE(saved31, QStringLiteral("Speakers (Focusrite USB Audio 2"));

        const QList<AudioDeviceInfo> list = {
            device(AudioBackendId::Wasapi, QStringLiteral("ep-hdmi"), QStringLiteral("HDMI Output")),
            device(AudioBackendId::Wasapi, QStringLiteral("ep-fr"), full),
        };
        for (AudioEngineKind kind : {AudioEngineKind::WindowsShared, AudioEngineKind::WindowsExclusive}) {
            const auto m = matchSavedAudioDevice(saved(kind, QString(), saved31), list);
            QVERIFY(m.has_value());
            QCOMPARE(m->device.id, QStringLiteral("ep-fr"));
            QVERIFY(!m->byId);
        }

        // Shorter than 31: a prefix is not enough.
        QVERIFY(!matchSavedAudioDevice(saved(AudioEngineKind::WindowsShared, QString(), full.left(30)), list)
                     .has_value());
        QVERIFY(!matchSavedAudioDevice(
                     saved(AudioEngineKind::WindowsShared, QString(), QStringLiteral("Speakers (Focusrite USB Aud")), list)
                     .has_value());

        // Not on other engines.
        const QList<AudioDeviceInfo> pw = {
            device(AudioBackendId::PipeWire, QStringLiteral("n"), full),
        };
        QVERIFY(!matchSavedAudioDevice(saved(AudioEngineKind::PipeWire, QString(), saved31), pw).has_value());
    }

    void linuxAlsaHwNameMatchesCardAndDevice()
    {
        AudioDeviceInfo usb = device(AudioBackendId::PipeWire, QStringLiteral("alsa_output.usb"),
                                     QStringLiteral("USB Audio Device Analog Stereo"));
        usb.alsaCard = 2;
        usb.alsaDevice = 0;
        AudioDeviceInfo other = device(AudioBackendId::PipeWire, QStringLiteral("alsa_output.hdmi"),
                                       QStringLiteral("HDMI"));
        other.alsaCard = 2;
        other.alsaDevice = 3;
        AudioDeviceInfo onboard = device(AudioBackendId::PipeWire, QStringLiteral("alsa_output.pci"),
                                         QStringLiteral("Built-in Audio"));
        onboard.alsaCard = 0;
        onboard.alsaDevice = 0;
        const QList<AudioDeviceInfo> list = {onboard, other, usb};

        const QString alsaName = QStringLiteral("USB Audio Device: - (hw:2,0)");
        for (AudioEngineKind kind : {AudioEngineKind::PipeWire, AudioEngineKind::PulseAudio,
                                     AudioEngineKind::AlsaDirect}) {
            QList<AudioDeviceInfo> pool = list;
            for (AudioDeviceInfo& d : pool) {
                d.backend = audioBackendFor(kind);
            }
            const auto m = matchSavedAudioDevice(saved(kind, QString(), alsaName), pool);
            QVERIFY(m.has_value());
            QCOMPARE(m->device.id, QStringLiteral("alsa_output.usb"));
            QVERIFY(!m->byId);
        }

        // No device on that card and device number: no match.
        QVERIFY(!matchSavedAudioDevice(
                     saved(AudioEngineKind::PipeWire, QString(), QStringLiteral("Card: - (hw:5,0)")), list)
                     .has_value());

        // Not on Windows audio.
        QList<AudioDeviceInfo> win = list;
        for (AudioDeviceInfo& d : win) {
            d.backend = AudioBackendId::Wasapi;
        }
        QVERIFY(!matchSavedAudioDevice(saved(AudioEngineKind::WindowsShared, QString(), alsaName), win).has_value());
    }

    void platformDefaultAndNoneAreNotMatched()
    {
        const QList<AudioDeviceInfo> list = {
            device(AudioBackendId::CoreAudio, QStringLiteral("uid"), QStringLiteral("MacBook Pro Speakers")),
        };
        AudioDeviceConfig def;
        def.engine = AudioEngineKind::CoreAudio;
        QVERIFY(def.isPlatformDefault());
        QVERIFY(!def.isNone());
        QVERIFY(!matchSavedAudioDevice(def, list).has_value());

        AudioDeviceConfig none;
        none.engine = AudioEngineKind::CoreAudio;
        none.deviceId = QString::fromLatin1(kAudioDeviceNone);
        QVERIFY(none.isNone());
        QVERIFY(!none.isPlatformDefault());
        QVERIFY(!matchSavedAudioDevice(none, list).has_value());

        AudioDeviceConfig named;
        named.deviceName = QStringLiteral("MacBook Pro Speakers");
        QVERIFY(!named.isPlatformDefault());
        QVERIFY(!named.isNone());
    }
};

QTEST_MAIN(TstAudioDeviceIdentity)
#include "tst_audio_device_identity.moc"
