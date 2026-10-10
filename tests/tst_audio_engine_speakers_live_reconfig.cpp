// =================================================================
// tests/tst_audio_engine_speakers_live_reconfig.cpp  (NereusSDR)
// =================================================================
//
// Exercises AudioEngine live-reconfig safety for the speakers bus —
// Sub-Phase 12 Task 12.2 Step 0.
//
// Coverage:
//   1. setSpeakersConfig while rxBlockReady "simulates DSP traffic"
//      doesn't crash and doesn't cause a use-after-free.
//   2. speakersConfigChanged emits after setSpeakersConfig (applied
//      synchronously — no debounce in AudioEngine; debounce lives in
//      DeviceCard's buffer-size combo per addendum §2.1).
//   3. rxBlockReady drops block when m_speakersBusMutex is held.
//   4. setHeadphonesConfig emits headphonesConfigChanged.
//   5. setTxInputConfig emits txInputConfigChanged.
//   6. setVaxConfig emits vaxConfigChanged for each channel.
//   8. R-R3-45 (2026-09-23): the headphones output beside the speakers.
//      start() opens it when audio/Headphones/Enabled is set and not
//      otherwise; the Enabled box opens and closes it; it plays the
//      headphones mix. Fake devices only.
//   9. Native audio final review fix (2026-10-09, J.J. Boyd KG4VCF,
//      AI-assisted via Anthropic Claude Code; R-AUD-15): the Core
//      speaker's delay readout, polled on the main thread while the DSP
//      thread plays, costs that thread no block.
//
// Uses the NEREUS_BUILD_TESTS seam (setSpeakersBusForTest,
// setHeadphonesBusForTest). Native audio plan Task 7 fix (2026-10-09,
// J.J. Boyd KG4VCF, AI-assisted via Anthropic Claude Code): the outputs
// open through the device layer the app runs (setAudioBackendsForTest with
// a fake engine of matcher buses); the direct path ships in no build. R3 receiver audio fix wave follow-up
// (2026-09-23, J.J. Boyd KG4VCF, AI-assisted via Anthropic Claude Code):
// every device the engine opens is a fake (setDeviceBusFactoryForTest,
// setVaxBusFactoryForTest) and the run is in test mode, so no case opens
// this computer's real output, microphone or VAX devices.
//
// Design spec:
//   docs/architecture/2026-04-20-phase3o-subphase12-addendum.md §4
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"
#include "fakes/FakeDeviceLayer.h"

#include <QElapsedTimer>
#include <QThread>

#include <atomic>
#include <memory>
#include <thread>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 2;
const float kSamples[kFrames * 2] = { 0.1f, 0.2f, 0.3f, 0.4f };

// R3 receiver audio fix wave follow-up: every device the engine opens is a
// fake, and test mode (initTestCase) stops anything else from reaching this
// computer's real speakers, microphone or VAX devices. `opened` counts the
// fake devices made.
//
// Native audio plan Task 7 fix: the outputs open through the device layer
// the app runs (engine backends, catalogue, stream supervisor), on a fake
// older-drivers engine with matcher buses (FakeDeviceLayer.h).
void useFakeDevices(AudioEngine* engine, int* opened)
{
    Test::useFakeDeviceLayer(engine, [opened](const QString&) {
        if (opened) { ++*opened; }
    });
    engine->setVaxBusFactoryForTest([opened](int channel) -> std::unique_ptr<IAudioBus> {
        if (opened) { ++*opened; }
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeVax%1").arg(channel));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        bus->open(fmt);
        return bus;
    });
}

// R-R3-45: fake VAX devices only; the caller supplies the device factory.
void useFakeVaxOnly(AudioEngine* engine)
{
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

// A speakers bus whose delayParts() takes 2 ms on the main thread, as a
// slow read under the speakers lock would; pushes are counted for the
// DSP thread that makes them.
class SlowDelayBus final : public FakeAudioBus {
public:
    explicit SlowDelayBus(QThread* mainThread)
        : FakeAudioBus(QStringLiteral("SlowDelaySpeakers"))
        , m_mainThread(mainThread)
    {
    }

    qint64 push(const char* data, qint64 bytes) override
    {
        m_pushes.fetch_add(1);
        return bytes > 0 && data != nullptr ? bytes : 0;
    }

    AudioDelayParts delayParts() const override
    {
        if (QThread::currentThread() == m_mainThread) {
            QThread::msleep(2);
        }
        AudioDelayParts parts;
        parts.matcherFillMs = 10.0;
        return parts;
    }

    int pushes() const { return m_pushes.load(); }

private:
    QThread* m_mainThread;
    std::atomic<int> m_pushes{0};
};

} // namespace

class TstAudioEngineSpeakersLiveReconfig : public QObject {
    Q_OBJECT

private:
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        AudioEngine*  engine{nullptr};   // non-owning
        FakeAudioBus* speakers{nullptr}; // non-owning (engine owns it)
        int opened{0};                   // fake devices the engine made

        int addSlice(int vaxCh = 0) {
            const int idx = radio->addSlice();
            radio->sliceById(idx)->setVaxChannel(vaxCh);
            return idx;
        }
    };

    Harness makeHarness() {
        Harness h;
        h.radio  = std::make_unique<RadioModel>();
        h.engine = h.radio->audioEngine();
        useFakeDevices(h.engine, &h.opened);
        // No debounce in AudioEngine — setSpeakersConfig applies synchronously.

        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels   = 2;
        fmt.sample     = AudioFormat::Sample::Float32;
        bus->open(fmt);
        h.speakers = bus.get();
        h.engine->setSpeakersBusForTest(std::move(bus));
        return h;
    }

private slots:
    void initTestCase()
    {
        // With no fake device supplied, the engine opens nothing real.
        QStandardPaths::setTestModeEnabled(true);
    }

    // ── 1. setSpeakersConfig + concurrent rxBlockReady doesn't crash ────────
    //
    // This is a smoke/no-crash test: we call setSpeakersConfig from the
    // main thread while simulating a burst of rxBlockReady calls. The
    // try_lock in rxBlockReady drops blocks when the mutex is held —
    // the test verifies we don't crash or use-after-free. On macOS,
    // the TSan / ASan run in CI will catch any actual race. Here we
    // just confirm the test path runs without assertion failures.

    void setSpeakersConfigDoesNotCrash() {
        Harness h = makeHarness();
        const int s = h.addSlice();

        // 10 rapid calls — should not crash even without a real audio thread.
        for (int i = 0; i < 10; ++i) {
            AudioDeviceConfig cfg;
            cfg.deviceName = QString();  // platform default
            h.engine->setSpeakersConfig(cfg);
            h.engine->rxBlockReady(s, kSamples, kFrames);
        }
        QVERIFY(true);  // if we get here, no crash
    }

    // ── 2. speakersConfigChanged emits after setSpeakersConfig ─────────────

    void setSpeakersConfigEmitsSignal() {
        Harness h = makeHarness();

        QSignalSpy spy(h.engine, &AudioEngine::speakersConfigChanged);

        AudioDeviceConfig cfg;
        cfg.deviceName = QString();
        h.engine->setSpeakersConfig(cfg);

        // Signal fires synchronously (setSpeakersConfig applies synchronously).
        QVERIFY(spy.count() >= 1);
        // The device it opened was the fake.
        QCOMPARE(h.opened, 1);
    }

    // ── 3. rxBlockReady drops block when mutex is held ─────────────────────
    //
    // We can't hold the mutex externally in production (it's private), but
    // we can verify the drop semantics by checking that when the engine
    // rebuilds the bus (setSpeakersConfig resets m_speakersBus to nullptr
    // during the lock), and then immediately rxBlockReady runs, no push
    // happens on the old (now-deleted) bus.
    //
    // Implementation note: FakeAudioBus is heap-allocated and owned by
    // the engine's unique_ptr. After setSpeakersConfig resets the old bus
    // and tries to open a new one (which fails in test mode since
    // m_paInitialized is likely false), m_speakersBus is null → the push
    // is safely skipped. The pushCount on the FakeAudioBus we injected
    // can't be checked after the engine reset it, but the key safety
    // property is that no crash occurs.

    void rxBlockReadySafeAfterReset() {
        Harness h = makeHarness();
        const int s = h.addSlice();

        // Inject a fake bus, call setSpeakersConfig (which resets it),
        // then rxBlockReady — must not crash or access freed memory.
        h.engine->setSpeakersConfig(AudioDeviceConfig{});
        h.engine->rxBlockReady(s, kSamples, kFrames);

        QVERIFY(true);
    }

    // ── 4. setHeadphonesConfig emits headphonesConfigChanged ───────────────

    void setHeadphonesConfigEmitsSignal() {
        AudioEngine engine;
        useFakeDevices(&engine, nullptr);

        QSignalSpy spy(&engine, &AudioEngine::headphonesConfigChanged);

        AudioDeviceConfig cfg;
        cfg.deviceName = QString();
        engine.setHeadphonesConfig(cfg);

        QVERIFY(spy.count() >= 1);
    }

    // ── 5. setTxInputConfig emits txInputConfigChanged ─────────────────────

    void setTxInputConfigEmitsSignal() {
        AudioEngine engine;
        useFakeDevices(&engine, nullptr);

        QSignalSpy spy(&engine, &AudioEngine::txInputConfigChanged);

        AudioDeviceConfig cfg;
        cfg.deviceName = QString();
        engine.setTxInputConfig(cfg);

        QVERIFY(spy.count() >= 1);
    }

    // ── 6. setVaxConfig emits vaxConfigChanged for channel 1..4 ─────────────

    void setVaxConfigEmitsSignal() {
        AudioEngine engine;
        useFakeDevices(&engine, nullptr);

        for (int ch = 1; ch <= 4; ++ch) {
            QSignalSpy spy(&engine, &AudioEngine::vaxConfigChanged);

            AudioDeviceConfig cfg;
            cfg.deviceName = QString();
            engine.setVaxConfig(ch, cfg);

            QVERIFY2(spy.count() >= 1,
                     qPrintable(QStringLiteral("vaxConfigChanged not emitted for channel %1").arg(ch)));
            if (spy.count() > 0) {
                QCOMPARE(spy.first().at(0).toInt(), ch);
            }
        }
    }

    // ── 7. speakersConfigChanged signal carries the config ─────────────────

    void speakersConfigChangedCarriesConfig() {
        Harness h = makeHarness();

        QSignalSpy spy(h.engine, &AudioEngine::speakersConfigChanged);

        AudioDeviceConfig cfg;
        cfg.deviceName = QStringLiteral("TestDevice");
        cfg.sampleRate = 96000;
        h.engine->setSpeakersConfig(cfg);

        QVERIFY(spy.count() >= 1);
        // The signal carries the config that was passed in (bus may fail
        // to open on headless CI, but the config is still emitted).
        const AudioDeviceConfig emitted =
            spy.first().at(0).value<AudioDeviceConfig>();
        QCOMPARE(emitted.deviceName, QStringLiteral("TestDevice"));
        QCOMPARE(emitted.sampleRate, 96000);
    }

    // ── 8. R-R3-45: the headphones output beside the speakers ─────────────

    void startOpensHeadphonesWhenEnabled() {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("audio/Headphones/Enabled"),
                                         QStringLiteral("True"));
        AppSettings::instance().setValue(QStringLiteral("audio/Headphones/DeviceName"),
                                         QStringLiteral("Desk headphones"));

        AudioEngine engine;
        QStringList openedNames;
        Test::useFakeDeviceLayer(&engine, [&openedNames](const QString& name) {
            openedNames << name;
        });
        useFakeVaxOnly(&engine);
        QSignalSpy spy(&engine, &AudioEngine::headphonesAvailableChanged);

        QVERIFY(engine.headphonesEnabled());
        QVERIFY(!engine.headphonesAvailable());
        engine.start();

        QVERIFY(openedNames.contains(QStringLiteral("Desk headphones")));
        QVERIFY(engine.headphonesAvailable());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), true);

        // The Enabled box turned off closes it.
        engine.setHeadphonesEnabled(false);
        QVERIFY(!engine.headphonesAvailable());
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last().at(0).toBool(), false);

        // A device change while disabled opens nothing.
        openedNames.clear();
        AudioDeviceConfig cfg;
        cfg.deviceName = QStringLiteral("Other headphones");
        engine.setHeadphonesConfig(cfg);
        QVERIFY(!openedNames.contains(QStringLiteral("Other headphones")));
        QVERIFY(!engine.headphonesAvailable());

        // Turned back on, it opens the stored device.
        engine.setHeadphonesEnabled(true);
        QVERIFY(openedNames.contains(QStringLiteral("Other headphones")));
        QVERIFY(engine.headphonesAvailable());
        engine.stop();
        QVERIFY(!engine.headphonesAvailable());
        AppSettings::instance().clear();
    }

    void startLeavesHeadphonesClosedWhenNotEnabled() {
        AppSettings::instance().clear();
        AudioEngine engine;
        int opened = 0;
        useFakeDevices(&engine, &opened);
        engine.start();
        QVERIFY(!engine.headphonesEnabled());
        QVERIFY(!engine.headphonesAvailable());
        engine.stop();
    }

    // The headphones output plays the headphones mix, the speakers the
    // speakers mix, side by side from the same period.
    void headphonesPlayBesideSpeakers() {
        AppSettings::instance().clear();
        Harness h = makeHarness();
        h.radio->configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                     /*defaultRateHz*/ 192000);
        auto hp = std::make_unique<FakeAudioBus>(QStringLiteral("FakeHeadphones"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        hp->open(fmt);
        FakeAudioBus* headphones = hp.get();
        h.engine->setHeadphonesBusForTest(std::move(hp));

        const int s = h.addSlice();
        for (int i = 0; i < 10; ++i) {
            h.engine->rxBlockReady(s, kSamples, kFrames);
        }
        QCOMPARE(h.speakers->pushCount(), 10);
        QCOMPARE(headphones->pushCount(), 10);
        AppSettings::instance().clear();
    }

    // ── 9. R-AUD-15: the Core speaker's delay readout ──────────────────────
    //
    // RadioModel's 1 s Core speaker refresh reads speakersDelayNowMs().
    // Read under the speakers lock it would make the DSP thread's try-lock
    // fail and drop a block; while that thread plays it is read from what
    // the thread published.  Every block sent is pushed.
    void coreSpeakerDelayReadCostsTheDspNoBlock() {
        AppSettings::instance().clear();
        auto radio = std::make_unique<RadioModel>();
        AudioEngine* engine = radio->audioEngine();
        useFakeDevices(engine, nullptr);
        radio->configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                   /*defaultRateHz*/ 192000);
        auto owned = std::make_unique<SlowDelayBus>(QThread::currentThread());
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        owned->open(fmt);
        SlowDelayBus* bus = owned.get();
        engine->setSpeakersBusForTest(std::move(owned));
        const int s = radio->addSlice();

        constexpr int kBlocks = 400;
        std::atomic<bool> done{false};
        std::thread dsp([&] {
            for (int i = 0; i < kBlocks; ++i) {
                engine->rxBlockReady(s, kSamples, kFrames);
                QThread::usleep(250);
            }
            done.store(true);
        });
        QElapsedTimer started;
        started.start();
        while (bus->pushes() == 0 && started.elapsed() < 5000) {
            QThread::usleep(100);
        }
        int polls = 0;
        double last = -1.0;
        while (!done.load()) {
            last = engine->speakersDelayNowMs();
            ++polls;
            QThread::usleep(100);
        }
        dsp.join();

        QVERIFY(polls > 10);
        QCOMPARE(last, 10.0);
        QCOMPARE(bus->pushes(), kBlocks);
        AppSettings::instance().clear();
    }
};

QTEST_MAIN(TstAudioEngineSpeakersLiveReconfig)
#include "tst_audio_engine_speakers_live_reconfig.moc"
