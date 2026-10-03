// =================================================================
// tests/tst_master_output_widget.cpp  (NereusSDR)
// =================================================================
//
// Exercises MasterOutputWidget — the menu-bar composite widget
// combining a speaker-mute button, a 0–100 master-volume slider,
// an inset value readout, and a right-click output-device picker.
// Phase 3O Sub-Phase 10 Task 10b.
//
// Test seams used:
//   - AudioEngine default constructor is valid without a real device
//     (m_paInitialized may be false on headless CI; rxBlockReady is a
//     no-op without a RadioModel, so bare AudioEngine is safe).
//   - MasterOutputWidget exposes findChild<QSlider*>("masterSlider")
//     and findChild<QPushButton*>("speakerBtn") via objectName so the
//     tests can drive them directly (no fragile eventFilter plumbing).
//
// Design spec: docs/architecture/2026-04-19-vax-design.md
//   §5.4 (AppSettings keys: audio/Master/Volume, audio/Master/Muted,
//   audio/Speakers/DeviceName)
//   §6.3 (wiring table for menu-bar MasterOutputWidget)
//   §7.3 (UI layout: speaker icon + 100px slider + inset readout)
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "gui/widgets/MasterOutputWidget.h"

using namespace NereusSDR;

class TstMasterOutputWidget : public QObject {
    Q_OBJECT

private:
    // Reset the AppSettings keys the widget reads/writes so each test
    // starts from a known-clean slate. The test sandbox (via
    // TestSandboxInit.cpp) guarantees we're not touching the real
    // ~/.config file.
    void clearMasterKeys() {
        auto& s = AppSettings::instance();
        s.remove(QStringLiteral("audio/Master/Volume"));
        s.remove(QStringLiteral("audio/Master/Muted"));
        s.remove(QStringLiteral("audio/Speakers/DeviceName"));
    }

private slots:

    void init() {
        clearMasterKeys();
    }

    // ── 1. Constructs without crashing ─────────────────────────────────────

    void constructsWithoutCrash() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        // Slider and speaker button must be reachable for downstream tests.
        QVERIFY(w.findChild<QSlider*>("masterSlider") != nullptr);
        QVERIFY(w.findChild<QPushButton*>("speakerBtn") != nullptr);
        QVERIFY(w.findChild<QLabel*>("dbLabel") != nullptr);
    }

    // ── 2. Slider move writes through to engine volume ─────────────────────

    void sliderWritesEngineVolume() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* slider = w.findChild<QSlider*>("masterSlider");
        QVERIFY(slider);

        slider->setValue(75);

        QVERIFY(std::abs(engine.volume() - 0.75f) < 1e-3f);
    }

    // ── 3. Slider move persists audio/Master/Volume ────────────────────────

    void sliderPersistsVolume() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* slider = w.findChild<QSlider*>("masterSlider");
        QVERIFY(slider);

        slider->setValue(75);

        const QString stored = AppSettings::instance()
            .value(QStringLiteral("audio/Master/Volume")).toString();
        QVERIFY2(!stored.isEmpty(), "audio/Master/Volume not written");
        const float val = stored.toFloat();
        QVERIFY(std::abs(val - 0.75f) < 1e-3f);
    }

    // ── 4. Mute button toggle drives engine masterMuted ────────────────────

    void muteButtonTogglesEngine() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* btn = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(btn);

        btn->click();  // press: muted
        QCOMPARE(engine.masterMuted(), true);

        btn->click();  // press again: unmuted
        QCOMPARE(engine.masterMuted(), false);
    }

    // ── 5. Mute persists audio/Master/Muted as "True"/"False" ──────────────

    void mutePersists() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* btn = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(btn);

        btn->click();
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/Master/Muted")).toString(),
                 QStringLiteral("True"));

        btn->click();
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/Master/Muted")).toString(),
                 QStringLiteral("False"));
    }

    // ── 6. Engine volume echo updates slider without re-firing engine ──────

    void engineVolumeEchoesToSlider() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* slider = w.findChild<QSlider*>("masterSlider");
        QVERIFY(slider);

        // Seed with a distinct starting value so we can detect the echo.
        slider->setValue(10);

        QSignalSpy engineSpy(&engine, &AudioEngine::volumeChanged);

        engine.setVolume(0.30f);

        // Slider reflects new engine value (30 percent).
        QCOMPARE(slider->value(), 30);

        // The echo path must NOT cause the widget to call setVolume again.
        // setVolume(0.30f) itself emitted volumeChanged once; no extras.
        QCOMPARE(engineSpy.count(), 1);
    }

    // ── 7. Engine mute echo updates button without re-emission ─────────────

    void engineMuteEchoesToButton() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* btn = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(btn);

        QSignalSpy engineSpy(&engine, &AudioEngine::masterMutedChanged);

        engine.setMasterMuted(true);

        QCOMPARE(btn->isChecked(), true);
        // 🔇 U+1F507 when muted
        QCOMPARE(btn->text(), QString::fromUtf8("\xF0\x9F\x94\x87"));

        // The engine emitted exactly once (its own setMasterMuted). The
        // button update via the echo slot must not cause a second emission.
        QCOMPARE(engineSpy.count(), 1);
    }

    // ── 8. setCurrentOutputDevice stores device name for the picker ────────
    //
    // The context-menu QActionGroup is awkward to exercise headlessly
    // (popup + action activation fights QTest without show()). Per the
    // task brief we take the lighter smoke: setCurrentOutputDevice()
    // stores the name the picker will use as its check-state anchor.

    void setCurrentOutputDeviceTracksName() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);

        w.setCurrentOutputDevice(QStringLiteral("TestDevice"));
        // No public getter — verify via outputDeviceChangedSignal round-trip
        // is Task 10c's job. Smoke: the call does not crash, does not emit
        // a spurious outputDeviceChanged (setCurrentOutputDevice is a
        // sync-from-elsewhere path, not a user action).
        QSignalSpy spy(&w, &MasterOutputWidget::outputDeviceChanged);
        w.setCurrentOutputDevice(QStringLiteral("AnotherDevice"));
        QCOMPARE(spy.count(), 0);
    }

    // ── 9. Applied saved volume/mute at construction time ──────────────────
    //
    // AppSettings persistence round-trip: a widget built after a prior
    // session's values were saved should come up with the slider/button
    // reflecting those values AND the engine seeded to match.

    void restoresSavedValuesOnConstruction() {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/Master/Volume"),
                   QStringLiteral("0.600"));
        s.setValue(QStringLiteral("audio/Master/Muted"),
                   QStringLiteral("True"));

        AudioEngine engine;
        MasterOutputWidget w(&engine);

        auto* slider = w.findChild<QSlider*>("masterSlider");
        auto* btn    = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(slider && btn);

        QCOMPARE(slider->value(), 60);
        QCOMPARE(btn->isChecked(), true);
        QVERIFY(std::abs(engine.volume() - 0.60f) < 1e-3f);
        QCOMPARE(engine.masterMuted(), true);
    }

    // ── 10. Saved device name applies at construction (smoke) ─────────────
    //
    // Regression for the gap where audio/Speakers/DeviceName was read but
    // never pushed into AudioEngine on startup, leaving the engine on the
    // platform default until the user reopened the picker. AudioEngine
    // exposes no "current device" getter, so this covers the construction
    // path running cleanly with a non-empty saved device and seeding the
    // same value into the picker anchor.

    void restoresSavedDeviceOnConstruction() {
        AppSettings::instance().setValue(
            QStringLiteral("audio/Speakers/DeviceName"),
            QStringLiteral("SomeDevice"));

        AudioEngine engine;
        MasterOutputWidget w(&engine);

        // Picker anchor round-trip: setCurrentOutputDevice re-setting the
        // value the constructor already stored must not re-emit.
        QSignalSpy spy(&w, &MasterOutputWidget::outputDeviceChanged);
        w.setCurrentOutputDevice(QStringLiteral("SomeDevice"));
        QCOMPARE(spy.count(), 0);
    }

    // ── 11. A picked device is saved before it is announced (R-R3-23) ──────
    //
    // In a remote window the announcement leads, synchronously, to remote
    // playback re-reading audio/Speakers (MainWindow -> setSpeakersConfig
    // -> speakersConfigChanged -> RemoteMediaController). When the widget
    // announced first and saved after, that read found the previous device
    // and the remote audio status named it. The picker's per-device action
    // is selectOutputDevice(); it is invoked by name because the menu itself
    // needs a real popup and real output devices.

    void pickedDeviceIsSavedBeforeItIsAnnounced() {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/Speakers/DeviceName"),
                   QStringLiteral("Previous speakers"));

        AudioEngine engine;
        MasterOutputWidget w(&engine);

        QStringList savedWhenAnnounced;
        connect(&w, &MasterOutputWidget::outputDeviceChanged, this,
                [&savedWhenAnnounced](const QString&) {
            savedWhenAnnounced << AppSettings::instance()
                .value(QStringLiteral("audio/Speakers/DeviceName")).toString();
        });

        QVERIFY(QMetaObject::invokeMethod(&w, "selectOutputDevice",
                                          Q_ARG(QString, QStringLiteral("New speakers"))));
        QCOMPARE(savedWhenAnnounced, QStringList{QStringLiteral("New speakers")});

        // Picking the device already in use announces nothing.
        QVERIFY(QMetaObject::invokeMethod(&w, "selectOutputDevice",
                                          Q_ARG(QString, QStringLiteral("New speakers"))));
        QCOMPARE(savedWhenAnnounced.size(), 1);
    }
};

QTEST_MAIN(TstMasterOutputWidget)
#include "tst_master_output_widget.moc"
