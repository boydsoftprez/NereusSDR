// =================================================================
// tests/tst_audio_engine_vax_open_signal.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It drives AudioEngine's VAX
// outputs with fake buses and Setup > Audio > VAX's channel cards; no
// upstream logic is ported here.
//
// R3 unfinished controls, fix wave follow-up (R-R3-49, R-R3-21): every
// path that changes what AudioEngine::isVaxBusOpen reports announces it
// with vaxBusOpenChanged, so a container's VAX 1 / VAX 2 buttons and
// Setup > Audio > VAX's channel cards follow at once:
//   - setVaxEnabled (on and off),
//   - setVaxConfig: the native fallback that opens the output, a picked
//     device, and a window where VAX outputs are not allowed (the output
//     closes),
//   - openVaxOutputs (a remote window opening its outputs),
//   - resetAudioSettings and stop().
// Setup > Audio > VAX's cards show the open state and the "On" switch a
// container's VAX toggle leaves.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-49, R-R3-21). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Merge with integration: the picked-
//                                    device note follows the device-layer
//                                    gate (a test run skips PortAudio).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QSignalSpy>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "gui/setup/AudioVaxPage.h"
#include "models/RadioModel.h"

#include "fakes/FakeAudioBus.h"

#include <memory>

using namespace NereusSDR;

namespace {

void useFakeVax(AudioEngine* engine)
{
    engine->setDeviceBusFactoryForTest([](const AudioDeviceConfig&, bool) {
        return std::make_unique<FakeAudioBus>(QStringLiteral("FakeDevice"));
    });
    engine->setVaxBusFactoryForTest([](int channel) -> std::unique_ptr<IAudioBus> {
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeVax%1").arg(channel));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        bus->open(fmt);
        return bus;
    });
}

// The channels announced, in order.
QList<int> channels(const QSignalSpy& spy)
{
    QList<int> list;
    for (const QList<QVariant>& args : spy) { list << args.at(0).toInt(); }
    return list;
}

void clearVaxKeys()
{
    for (int ch = 1; ch <= 4; ++ch) {
        AppSettings::instance().remove(QStringLiteral("audio/Vax%1/Enabled").arg(ch));
        AppSettings::instance().remove(QStringLiteral("audio/Vax%1/DeviceName").arg(ch));
    }
}

} // namespace

class TstAudioEngineVaxOpenSignal : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The engine opens no real sound device in test mode.
        QStandardPaths::setTestModeEnabled(true);
        AppSettings::setProfileOverride(QStringLiteral("vax-open-signal-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init() { clearVaxKeys(); }
    void cleanup() { clearVaxKeys(); }

    void setVaxEnabledAnnouncesBothWays()
    {
        AudioEngine engine;
        useFakeVax(&engine);
        QSignalSpy spy(&engine, &AudioEngine::vaxBusOpenChanged);
        engine.setVaxEnabled(1, true);
        QVERIFY(engine.isVaxBusOpen(1));
        engine.setVaxEnabled(1, false);
        QVERIFY(!engine.isVaxBusOpen(1));
        QCOMPARE(channels(spy), (QList<int>{1, 1}));
    }

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    // No device picked: the native output opens (the page's device picker
    // cleared back to the default).
    void setVaxConfigNativeFallbackAnnouncesTheOpenOutput()
    {
        AudioEngine engine;
        useFakeVax(&engine);
        QSignalSpy spy(&engine, &AudioEngine::vaxBusOpenChanged);
        engine.setVaxConfig(2, AudioDeviceConfig{});
        QVERIFY(engine.isVaxBusOpen(2));
        QCOMPARE(channels(spy), (QList<int>{2}));
    }
#endif

    // A picked device (the page's device picker): the output is replaced,
    // and announced. The "device layer not ready" return shares the same
    // announcement (a scope guard on every return). A test run never
    // initialises PortAudio but counts the device layer ready (makeBus
    // hands out only the test's fake devices), so a test cannot reach
    // that return; only a failed Pa_Initialize outside a test run can.
    void setVaxConfigWithAPickedDeviceAnnounces()
    {
        AudioEngine engine;
        useFakeVax(&engine);
        engine.setVaxEnabled(3, true);
        QSignalSpy spy(&engine, &AudioEngine::vaxBusOpenChanged);
        AudioDeviceConfig cfg;
        cfg.deviceName = QStringLiteral("Some Cable");
        engine.setVaxConfig(3, cfg);
        QCOMPARE(channels(spy), (QList<int>{3}));
    }

    // A window where VAX outputs are not allowed: the output closes.
    void setVaxConfigNotAllowedAnnouncesTheClosedOutput()
    {
        AudioEngine engine;
        useFakeVax(&engine);
        engine.setVaxEnabled(4, true);
        QVERIFY(engine.isVaxBusOpen(4));
        engine.setVaxOutputsAllowed(false);
        QSignalSpy spy(&engine, &AudioEngine::vaxBusOpenChanged);
        engine.setVaxConfig(4, AudioDeviceConfig{});
        QVERIFY(!engine.isVaxBusOpen(4));
        QCOMPARE(channels(spy), (QList<int>{4}));
    }

    // A remote window opening its outputs.
    void openVaxOutputsAnnouncesEachOpenedOutput()
    {
        AudioEngine engine;
        useFakeVax(&engine);
        engine.setVaxEnabled(2, true);  // already open: not announced again
        QSignalSpy spy(&engine, &AudioEngine::vaxBusOpenChanged);
        engine.openVaxOutputs();
        for (int ch = 1; ch <= 4; ++ch) { QVERIFY(engine.isVaxBusOpen(ch)); }
        QCOMPARE(channels(spy), (QList<int>{1, 3, 4}));
    }

    void resetAudioSettingsAnnouncesEachChannel()
    {
        AudioEngine engine;
        useFakeVax(&engine);
        engine.setVaxEnabled(1, true);
        QSignalSpy spy(&engine, &AudioEngine::vaxBusOpenChanged);
        engine.resetAudioSettings();
        QCOMPARE(channels(spy), (QList<int>{1, 2, 3, 4}));
    }

    void stopAnnouncesEachOutputItCloses()
    {
        AudioEngine engine;
        useFakeVax(&engine);
        engine.setVaxEnabled(1, true);
        engine.setVaxEnabled(3, true);
        QSignalSpy spy(&engine, &AudioEngine::vaxBusOpenChanged);
        engine.stop();
        QVERIFY(!engine.isVaxBusOpen(1));
        QVERIFY(!engine.isVaxBusOpen(3));
        QCOMPARE(channels(spy), (QList<int>{1, 3}));
    }

    // Setup > Audio > VAX follows the engine and a container's toggle: the
    // card's open state and its "On" switch.
    void vaxPageCardsFollowChangesMadeElsewhere()
    {
        RadioModel radio;
        AudioEngine* engine = radio.localAudioDevices();
        QVERIFY(engine != nullptr);
        useFakeVax(engine);
        engine->setVaxEnabled(1, false);

        AudioVaxPage page(&radio);
        VaxChannelCard* card = page.channelCard(1);
        QVERIFY(card != nullptr);
        QVERIFY(!card->busOpenForTest());
        QVERIFY(!card->isChannelEnabled());

        // What a container's VAX 1 button does: save the switch, open the
        // output.
        AppSettings::instance().setValue(QStringLiteral("audio/Vax1/Enabled"),
                                         QStringLiteral("True"));
        engine->setVaxEnabled(1, true);
        QVERIFY(card->busOpenForTest());
        QVERIFY(card->isChannelEnabled());

        AppSettings::instance().setValue(QStringLiteral("audio/Vax1/Enabled"),
                                         QStringLiteral("False"));
        engine->setVaxEnabled(1, false);
        QVERIFY(!card->busOpenForTest());
        QVERIFY(!card->isChannelEnabled());

        // A remote window opening its outputs.
        engine->openVaxOutputs();
        QVERIFY(page.channelCard(2)->busOpenForTest());
    }
};

QTEST_MAIN(TstAudioEngineVaxOpenSignal)
#include "tst_audio_engine_vax_open_signal.moc"
