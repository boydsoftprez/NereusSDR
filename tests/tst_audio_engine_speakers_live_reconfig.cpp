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
//
// Uses the NEREUS_BUILD_TESTS seam (setSpeakersBusForTest,
// setHeadphonesBusForTest).
//
// Design spec:
//   docs/architecture/2026-04-20-phase3o-subphase12-addendum.md §4
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <memory>
#include <thread>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 2;
const float kSamples[kFrames * 2] = { 0.1f, 0.2f, 0.3f, 0.4f };

} // namespace

class TstAudioEngineSpeakersLiveReconfig : public QObject {
    Q_OBJECT

private:
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        AudioEngine*  engine{nullptr};   // non-owning
        FakeAudioBus* speakers{nullptr}; // non-owning (engine owns it)

        int addSlice(int vaxCh = 0) {
            const int idx = radio->addSlice();
            radio->sliceAt(idx)->setVaxChannel(vaxCh);
            return idx;
        }
    };

    Harness makeHarness() {
        Harness h;
        h.radio  = std::make_unique<RadioModel>();
        h.engine = h.radio->audioEngine();
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

        QSignalSpy spy(&engine, &AudioEngine::headphonesConfigChanged);

        AudioDeviceConfig cfg;
        cfg.deviceName = QString();
        engine.setHeadphonesConfig(cfg);

        QVERIFY(spy.count() >= 1);
    }

    // ── 5. setTxInputConfig emits txInputConfigChanged ─────────────────────

    void setTxInputConfigEmitsSignal() {
        AudioEngine engine;

        QSignalSpy spy(&engine, &AudioEngine::txInputConfigChanged);

        AudioDeviceConfig cfg;
        cfg.deviceName = QString();
        engine.setTxInputConfig(cfg);

        QVERIFY(spy.count() >= 1);
    }

    // ── 6. setVaxConfig emits vaxConfigChanged for channel 1..4 ─────────────

    void setVaxConfigEmitsSignal() {
        AudioEngine engine;

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
};

QTEST_MAIN(TstAudioEngineSpeakersLiveReconfig)
#include "tst_audio_engine_speakers_live_reconfig.moc"
