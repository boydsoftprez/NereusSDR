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
//   - Native audio plan Task 19: an engine on fake engine backends
//     (FakeAudioEngineBackend, no device) fills the speakers menu, which
//     buildSpeakerMenuForTest() builds as a right-click would.
//
// Design spec: docs/architecture/2026-04-19-vax-design.md
//   §5.4 (AppSettings keys: audio/Master/Volume, audio/Master/Muted,
//   audio/Speakers/DeviceName)
//   §6.3 (wiring table for menu-bar MasterOutputWidget)
//   §7.3 (UI layout: speaker icon + 100px slider + inset readout)
// Radio speaker plan Task 6 (R-SPK-17, D1, D5): the button shows the
// pc-on / pc-muted icons and a "PC" word label follows it.
// docs/architecture/2026-10-08-native-audio-engines-design.md (R-AUD-23,
// R-AUD-03, R-AUD-08, R-AUD-11, R-AUD-12, D20, V-UI-2): the speakers menu
// and tooltip, against header-menu-mockup.html option A.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 19 (R-AUD-23, R-AUD-03, R-AUD-08,
//               R-AUD-11, R-AUD-12, D20, V-UI-2). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio fix wave (R-AUD-19): a header pick on a
//               second ASIO driver asks first, as the Setup card does.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QLayout>
#include <QComboBox>
#include <QDir>
#include <QImage>
#include <QMenu>
#include <QToolTip>
#include <QWidgetAction>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/PortAudioBackend.h"
#include "gui/setup/AsioSwitchAllDialog.h"
#include "gui/setup/DeviceCard.h"
#include "gui/widgets/AppIcon.h"
#include "gui/widgets/MasterOutputWidget.h"

#include "fakes/FakeAudioEngineBackend.h"

#include <cmath>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

using namespace NereusSDR;

namespace {

AudioDeviceInfo deviceInfo(AudioBackendId backend, const QString& id, const QString& name,
                           int channels = 2)
{
    AudioDeviceInfo info;
    info.backend = backend;
    info.direction = AudioDeviceDirection::Output;
    info.id = id;
    info.name = name;
    info.channelCount = channels;
    return info;
}

void saveChoice(AudioEngineKind engine, const QString& id, const QString& name,
                int firstChannel = 1)
{
    AudioDeviceConfig cfg;
    cfg.engine = engine;
    cfg.deviceId = id;
    cfg.deviceName = name;
    cfg.firstChannel = firstChannel;
    cfg.saveToSettings(QStringLiteral("audio/Speakers"));
}

// An engine on fake engine backends: the native backend decides the
// system (Core Audio, or Windows audio with AudioBackendId::Wasapi), with
// an older-driver backend and, when set, an ASIO one. The engine reads the
// saved choices when it starts, so start() comes after a test saves them.
struct Rig {
    std::shared_ptr<FakeAudioEngineBackend> native;
    std::shared_ptr<FakeAudioEngineBackend> older =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
    std::shared_ptr<FakeAudioEngineBackend> asio;
    std::vector<std::pair<QString, AsioDriverCaps>> asioCaps;   // set before start()
    std::unique_ptr<AudioEngine> engine;

    explicit Rig(AudioBackendId nativeId = AudioBackendId::CoreAudio)
        : native(std::make_shared<FakeAudioEngineBackend>(nativeId))
    {
        native->setDevices({deviceInfo(nativeId, QStringLiteral("desk-uid"),
                                       QStringLiteral("Desk speakers")),
                            deviceInfo(nativeId, QStringLiteral("built-in-uid"),
                                       QStringLiteral("Built-in speakers"))});
        native->setDefault(AudioDeviceDirection::Output, QStringLiteral("built-in-uid"));
        older->setTakesStereoMix(false);
        AudioDeviceInfo mme = deviceInfo(
            AudioBackendId::PortAudio,
            portAudioDeviceId(QStringLiteral("MME"), QStringLiteral("Desk speakers")),
            QStringLiteral("Desk speakers"));
        mme.hostApi = QStringLiteral("MME");
        older->setDevices({mme});
    }

    void start()
    {
        engine = std::make_unique<AudioEngine>();
        engine->setVaxOutputsAllowed(false);
        std::vector<std::shared_ptr<IAudioEngineBackend>> backends{native};
        if (asio) {
            backends.push_back(asio);
        }
        backends.push_back(older);
        engine->setAudioBackendsForTest(backends);
        for (const auto& [driver, c] : asioCaps) {
            engine->setAsioDriverCapsForTest(driver, c);
        }
        engine->start();
        QVERIFY(engine->catalogue() != nullptr);
    }
};

// One line per menu row: "kind|text|ticked|on" (kind from the row's
// speakerMenuEntry property; "-" for a row that cannot be ticked).
QStringList rowsOf(MasterOutputWidget& w)
{
    std::unique_ptr<QMenu> menu(w.buildSpeakerMenuForTest());
    QStringList rows;
    for (QAction* a : menu->actions()) {
        if (a->isSeparator()) {
            rows << QStringLiteral("separator");
            continue;
        }
        const QString tick = !a->isCheckable() ? QStringLiteral("-")
            : a->isChecked()                   ? QStringLiteral("ticked")
                                               : QStringLiteral("unticked");
        rows << QStringLiteral("%1|%2|%3|%4")
                    .arg(a->property("speakerMenuEntry").toString(), a->text(), tick,
                         a->isEnabled() ? QStringLiteral("on") : QStringLiteral("off"));
    }
    return rows;
}

// Triggers the menu row with this text, as a click on it would.
void trigger(MasterOutputWidget& w, const QString& text)
{
    std::unique_ptr<QMenu> menu(w.buildSpeakerMenuForTest());
    for (QAction* a : menu->actions()) {
        if (a->text() == text) {
            QVERIFY2(a->isEnabled(), qPrintable(text));
            a->trigger();
            return;
        }
    }
    QFAIL(qPrintable(QStringLiteral("no menu row ") + text));
}

// Triggers the row with this text under the interface heading `group`.
void triggerInGroup(MasterOutputWidget& w, const QString& group, const QString& text)
{
    std::unique_ptr<QMenu> menu(w.buildSpeakerMenuForTest());
    QString inGroup;
    for (QAction* a : menu->actions()) {
        const QString kind = a->property("speakerMenuEntry").toString();
        if (kind == QLatin1String("group")) {
            inGroup = a->text();
            continue;
        }
        if (inGroup == group && a->text() == text) {
            QVERIFY2(a->isEnabled(), qPrintable(text));
            a->trigger();
            return;
        }
    }
    QFAIL(qPrintable(QStringLiteral("no menu row ") + text + QStringLiteral(" under ") + group));
}

AsioDriverCaps asioCaps(const QString& name, int inputs, int outputs)
{
    AsioDriverCaps c;
    c.name = name;
    c.inputChannels = inputs;
    c.outputChannels = outputs;
    c.sampleType = AsioSampleType::Int32Lsb;
    c.minBufferFrames = 64;
    c.maxBufferFrames = 1024;
    c.preferredBufferFrames = 256;
    c.granularity = -1;
    c.sampleRates = {44100.0, 48000.0, 96000.0};
    c.currentRate = 48000.0;
    return c;
}

QStringList audioKeysAndValues()
{
    QStringList out;
    AppSettings& s = AppSettings::instance();
    for (const QString& k : s.allKeys()) {
        if (k.startsWith(QStringLiteral("audio/"))) {
            out << k + QLatin1Char('=') + s.value(k).toString();
        }
    }
    out.sort();
    return out;
}

// What the one-driver prompt showed, and whether it showed.
struct SwitchAnswer {
    bool seen = false;
    QString text;
    QStringList moves;
};

// Answers the next AsioSwitchAllDialog once it is on screen (its exec()
// is running): records it, then presses Switch all or Cancel. The same
// seam as the Setup card's tests (tst_device_card_pairs_asio). The answer
// is shared with the poll, so a poll that finds no dialog outlives no test.
std::shared_ptr<SwitchAnswer> answerSwitchDialog(bool switchAll)
{
    auto answer = std::make_shared<SwitchAnswer>();
    auto poll = std::make_shared<std::function<void(int)>>();
    *poll = [=](int tries) {
        for (QWidget* top : QApplication::topLevelWidgets()) {
            auto* dialog = qobject_cast<AsioSwitchAllDialog*>(top);
            if (dialog == nullptr || !dialog->isVisible()) {
                continue;
            }
            answer->seen = true;
            const QString dir = qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR");
            if (!dir.isEmpty()) {
                QDir().mkpath(dir);
                const QPixmap shot = dialog->grab();
                shot.save(QStringLiteral("%1/header-asio-switch-all-prompt@%2x.png")
                              .arg(dir)
                              .arg(qRound(shot.devicePixelRatio())));
            }
            if (auto* t = dialog->findChild<QLabel*>(QStringLiteral("asioSwitchAllText"))) {
                answer->text = t->text();
            }
            for (QLabel* line : dialog->findChildren<QLabel*>(QStringLiteral("asioSwitchAllMove"))) {
                answer->moves << line->text();
            }
            auto* press = dialog->findChild<QPushButton*>(
                switchAll ? QStringLiteral("asioSwitchAllOk") : QStringLiteral("asioSwitchAllCancel"));
            if (press != nullptr) {
                press->click();
            } else {
                dialog->reject();
            }
            return;
        }
        if (tries < 200) {
            QTimer::singleShot(10, qApp, [poll, tries]() { (*poll)(tries + 1); });
        }
    };
    QTimer::singleShot(0, qApp, [poll]() { (*poll)(0); });
    return answer;
}

// Windows with two ASIO drivers: the speakers and the mic on the
// Focusrite, the MOTU free.
const QString kFocusrite = QStringLiteral("Focusrite USB ASIO");
const QString kMotu = QStringLiteral("MOTU M Series");

void twoAsioDrivers(Rig& rig)
{
    rig.asio = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Asio);
    AudioDeviceInfo focusriteIn = deviceInfo(AudioBackendId::Asio, kFocusrite, kFocusrite, 2);
    focusriteIn.direction = AudioDeviceDirection::Input;
    AudioDeviceInfo motuIn = deviceInfo(AudioBackendId::Asio, kMotu, kMotu, 4);
    motuIn.direction = AudioDeviceDirection::Input;
    rig.asio->setDevices({deviceInfo(AudioBackendId::Asio, kFocusrite, kFocusrite, 10), focusriteIn,
                          deviceInfo(AudioBackendId::Asio, kMotu, kMotu, 4), motuIn});
    rig.asioCaps = {{kFocusrite, asioCaps(kFocusrite, 2, 10)}, {kMotu, asioCaps(kMotu, 4, 4)}};
    saveChoice(AudioEngineKind::Asio, kFocusrite, kFocusrite, 1);
    AudioDeviceConfig mic;
    mic.engine = AudioEngineKind::Asio;
    mic.deviceId = kFocusrite;
    mic.deviceName = kFocusrite;
    mic.firstChannel = 1;
    mic.saveToSettings(QStringLiteral("audio/TxInput"));
}

// Pixels of `colour` (within a few steps per channel) in a slider styled
// with `style`, drawn at half way, enabled or not.
int pixelsOf(const char* style, bool enabled, QColor colour)
{
    QSlider slider(Qt::Horizontal);
    slider.setRange(0, 100);
    slider.setValue(50);
    slider.setFixedSize(100, 16);
    slider.setStyleSheet(QLatin1String(style));
    slider.setEnabled(enabled);
    const QImage image = slider.grab().toImage();
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if (std::abs(c.red() - colour.red()) <= 6 && std::abs(c.green() - colour.green()) <= 6
                && std::abs(c.blue() - colour.blue()) <= 6) {
                ++count;
            }
        }
    }
    return count;
}

QString tooltipOf(MasterOutputWidget& w)
{
    auto* btn = w.findChild<QPushButton*>(QStringLiteral("speakerBtn"));
    return btn ? btn->toolTip() : QString();
}

// What MainWindow does with a pick: the speakers open on the saved choice.
void followPicks(MasterOutputWidget& w, AudioEngine& engine)
{
    QObject::connect(&w, &MasterOutputWidget::outputDeviceChanged, &engine,
                     [&engine](const QString& name) {
        AudioDeviceConfig cfg = AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));
        cfg.deviceName = name;
        engine.setSpeakersConfig(cfg);
    });
}

QString captureDir()
{
    return qEnvironmentVariable("NEREUS_HEADER_CAPTURE_DIR");
}

void savePixmap(const QPixmap& shot, const QString& stem)
{
    QDir().mkpath(captureDir());
    const QString path = QStringLiteral("%1/%2@%3x.png")
                             .arg(captureDir(), stem)
                             .arg(qRound(shot.devicePixelRatio()));
    QVERIFY2(shot.save(path), qPrintable(path));
}

// The menu as a right-click shows it, over the header strip's colour.
void captureMenu(MasterOutputWidget& w, const QString& stem)
{
    std::unique_ptr<QMenu> menu(w.buildSpeakerMenuForTest());
    menu->popup(QPoint(40, 40));
    QTest::qWait(50);
    QApplication::processEvents();
    savePixmap(menu->grab(), stem);
    menu->close();
}

// The tooltip as hovering the PC icon shows it.
void captureToolTip(MasterOutputWidget& w, const QString& stem)
{
    auto* btn = w.findChild<QPushButton*>(QStringLiteral("speakerBtn"));
    QVERIFY(btn != nullptr);
    QToolTip::showText(QPoint(40, 40), btn->toolTip(), btn);
    QWidget* tip = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        [&tip]() {
            for (QWidget* top : QApplication::topLevelWidgets()) {
                if (top->inherits("QTipLabel") && top->isVisible()) {
                    tip = top;
                }
            }
            return tip != nullptr;
        }(),
        2000);
    QApplication::processEvents();
    savePixmap(tip->grab(), stem);
    QToolTip::hideText();
}

} // namespace

class TstMasterOutputWidget : public QObject {
    Q_OBJECT

private:
    // Reset the AppSettings keys the widget reads/writes so each test
    // starts from a known-clean slate. The test sandbox (via
    // TestSandboxInit.cpp) guarantees we're not touching the real
    // ~/.config file.
    void clearMasterKeys() {
        AppSettings::instance().clear();
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

    // ── 1b. The PC group: icon and "PC" word between icon and slider ───────
    // (R-SPK-17, D1). The button shows pc-on, unmuted, as an icon.

    void pcGroupShowsIconAndWord() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* btn = w.findChild<QPushButton*>("speakerBtn");
        auto* label = w.findChild<QLabel*>("pcLabel");
        auto* slider = w.findChild<QSlider*>("masterSlider");
        QVERIFY(btn && label && slider);
        QCOMPARE(label->text(), QStringLiteral("PC"));
        QCOMPARE(btn->property(AppIcon::kIconProperty).toString(),
                 QStringLiteral("pc-on"));
        QVERIFY(btn->text().isEmpty());
        auto* layout = w.layout();
        QVERIFY(layout);
        QCOMPARE(layout->indexOf(label), layout->indexOf(btn) + 1);
        QCOMPARE(layout->indexOf(slider), layout->indexOf(label) + 1);
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
        // The app's own muted speaker icon (R-SPK-19, D5), no emoji text.
        QCOMPARE(btn->property(AppIcon::kIconProperty).toString(),
                 QStringLiteral("pc-muted"));
        QVERIFY(btn->text().isEmpty());
        QVERIFY(!btn->icon().isNull());

        // The engine emitted exactly once (its own setMasterMuted). The
        // button update via the echo slot must not cause a second emission.
        QCOMPARE(engineSpy.count(), 1);

        engine.setMasterMuted(false);
        QCOMPARE(btn->property(AppIcon::kIconProperty).toString(),
                 QStringLiteral("pc-on"));
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
    // and the remote audio status named it. The pick goes through the
    // speakers menu, over an engine on fake engine backends.

    void pickedDeviceIsSavedBeforeItIsAnnounced() {
        saveChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                   QStringLiteral("Desk speakers"));
        Rig rig;
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        followPicks(w, *rig.engine);

        QStringList savedWhenAnnounced;
        connect(&w, &MasterOutputWidget::outputDeviceChanged, this,
                [&savedWhenAnnounced](const QString&) {
            savedWhenAnnounced << AppSettings::instance()
                .value(QStringLiteral("audio/Speakers/DeviceName")).toString();
        });

        trigger(w, QStringLiteral("Built-in speakers"));
        QCOMPARE(savedWhenAnnounced, QStringList{QStringLiteral("Built-in speakers")});
        QTRY_COMPARE_WITH_TIMEOUT(
            rig.engine->roleStatus(AudioRole::Speakers).chosen.deviceId,
            QStringLiteral("built-in-uid"), 2000);

        // Picking the device already chosen announces nothing.
        trigger(w, QStringLiteral("Built-in speakers"));
        QCOMPARE(savedWhenAnnounced.size(), 1);
        rig.engine->stop();
    }

    // ── 12. Option A: the speakers' own driver and its devices (D20) ───────
    // The heading names the driver; "(platform default)" comes first; the
    // older drivers' devices are not listed; the choice is ticked; "Sound
    // setup…" is last, after a separator.

    void menuListsTheSpeakersDriverOnly() {
        saveChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                   QStringLiteral("Desk speakers"));
        Rig rig;
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QCOMPARE(rowsOf(w), (QStringList{
            QStringLiteral("heading|Speakers · Core Audio|-|off"),
            QStringLiteral("device|(platform default)|unticked|on"),
            QStringLiteral("device|Desk speakers|ticked|on"),
            QStringLiteral("device|Built-in speakers|unticked|on"),
            QStringLiteral("separator"),
            QStringLiteral("setup|Sound setup…|-|on"),
        }));
        rig.engine->stop();
    }

    void menuHeadingNamesWindowsSharedAndOlderDrivers() {
        saveChoice(AudioEngineKind::WindowsShared, QStringLiteral("desk-uid"),
                   QStringLiteral("Desk speakers"));
        {
            Rig rig(AudioBackendId::Wasapi);
            rig.start();
            MasterOutputWidget w(rig.engine.get());
            QCOMPARE(rowsOf(w).first(),
                     QStringLiteral("heading|Speakers · Windows audio, shared|-|off"));
            rig.engine->stop();
        }
        AudioDeviceConfig older;
        older.engine = AudioEngineKind::PortAudio;
        older.driverApi = QStringLiteral("MME");
        older.deviceId = portAudioDeviceId(QStringLiteral("MME"), QStringLiteral("Desk speakers"));
        older.deviceName = QStringLiteral("Desk speakers");
        older.saveToSettings(QStringLiteral("audio/Speakers"));
        Rig rig(AudioBackendId::Wasapi);
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QCOMPARE(rowsOf(w), (QStringList{
            QStringLiteral("heading|Speakers · MME|-|off"),
            QStringLiteral("device|(platform default)|unticked|on"),
            QStringLiteral("device|Desk speakers|ticked|on"),
            QStringLiteral("separator"),
            QStringLiteral("setup|Sound setup…|-|on"),
        }));
        rig.engine->stop();
    }

    // ── 13. Pairs indented under their interface's name (R-AUD-07, D20) ────

    void pairsAreIndentedUnderTheirInterface() {
        saveChoice(AudioEngineKind::CoreAudio, QStringLiteral("foc-uid"),
                   QStringLiteral("Focusrite USB Audio"), 3);
        Rig rig;
        rig.native->addDevice(deviceInfo(AudioBackendId::CoreAudio, QStringLiteral("foc-uid"),
                                         QStringLiteral("Focusrite USB Audio"), 4));
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QCOMPARE(rowsOf(w), (QStringList{
            QStringLiteral("heading|Speakers · Core Audio|-|off"),
            QStringLiteral("device|(platform default)|unticked|on"),
            QStringLiteral("device|Desk speakers|unticked|on"),
            QStringLiteral("device|Built-in speakers|unticked|on"),
            QStringLiteral("group|Focusrite USB Audio|-|off"),
            QStringLiteral("device|    Outputs 1-2|unticked|on"),
            QStringLiteral("device|    Outputs 3-4|ticked|on"),
            QStringLiteral("separator"),
            QStringLiteral("setup|Sound setup…|-|on"),
        }));

        // A pair pick saves its first channel, as the Outputs card does.
        followPicks(w, *rig.engine);
        trigger(w, QStringLiteral("    Outputs 1-2"));
        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/DeviceId")).toString(),
                 QStringLiteral("foc-uid"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/FirstChannel")).toString(),
                 QStringLiteral("1"));
        rig.engine->stop();
    }

    // ── 14. A missing choice: on top, ticked, amber, and where it plays ────
    // (R-AUD-08), and the tooltip says so (R-AUD-23).

    void missingChoiceIsOnTopWithWhereItPlays() {
        saveChoice(AudioEngineKind::CoreAudio, QStringLiteral("gone-uid"),
                   QStringLiteral("Desk monitor"));
        Rig rig;
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::PlayingOnDefault, 5000);
        QCOMPARE(rowsOf(w), (QStringList{
            QStringLiteral("heading|Speakers · Core Audio|-|off"),
            QStringLiteral("missing|Desk monitor (not connected)|ticked|off"),
            QStringLiteral("missingNote|Playing on Built-in speakers until it comes back.|-|off"),
            QStringLiteral("separator"),
            QStringLiteral("device|(platform default)|unticked|on"),
            QStringLiteral("device|Desk speakers|unticked|on"),
            QStringLiteral("device|Built-in speakers|unticked|on"),
            QStringLiteral("separator"),
            QStringLiteral("setup|Sound setup…|-|on"),
        }));
        // Amber, with the tick drawn in its row.
        std::unique_ptr<QMenu> menu(w.buildSpeakerMenuForTest());
        QLabel* label = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->property("speakerMenuEntry").toString() == QStringLiteral("missing")) {
                label = qobject_cast<QLabel*>(qobject_cast<QWidgetAction*>(a)->defaultWidget());
            }
        }
        QVERIFY(label != nullptr);
        QVERIFY(label->styleSheet().contains(QStringLiteral("#e0a030")));
        QCOMPARE(label->text(), QStringLiteral("✓  Desk monitor (not connected)"));

        QTRY_COMPARE_WITH_TIMEOUT(
            tooltipOf(w),
            QStringLiteral("PC volume. Click to mute, right-click for speakers. Desk monitor is "
                           "not connected; playing on Built-in speakers meanwhile."),
            2000);
        rig.engine->stop();
    }

    // ── 15. A busy choice reads "in use by another program" (R-AUD-11) ─────

    void busyChoiceSaysInUse() {
        saveChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                   QStringLiteral("Desk speakers"));
        Rig rig;
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        FakeMatcherAudioBus* desk = rig.native->lastOutput();
        QVERIFY(desk != nullptr);
        AudioStreamEvent event;
        event.kind = AudioStreamEvent::Kind::DeviceBusy;
        desk->emitEventForTest(event);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).reason,
                                  AudioRoleReason::InUse, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::PlayingOnDefault, 5000);
        const QStringList rows = rowsOf(w);
        QCOMPARE(rows.mid(0, 4), (QStringList{
            QStringLiteral("heading|Speakers · Core Audio|-|off"),
            QStringLiteral("missing|Desk speakers (in use by another program)|ticked|off"),
            QStringLiteral("missingNote|Playing on Built-in speakers until it comes back.|-|off"),
            QStringLiteral("separator"),
        }));
        // Only the top row is ticked.
        QCOMPARE(rows.filter(QStringLiteral("|ticked|")).size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(
            tooltipOf(w),
            QStringLiteral("PC volume. Click to mute, right-click for speakers. Desk speakers is "
                           "in use by another program; playing on Built-in speakers meanwhile."),
            2000);
        rig.engine->stop();
    }

    // ── 16. ASIO with no device present says so, disabled ──────────────────

    void asioWithNoDeviceSaysSo() {
        AudioDeviceConfig asio;
        asio.engine = AudioEngineKind::Asio;
        asio.saveToSettings(QStringLiteral("audio/Speakers"));
        Rig rig(AudioBackendId::Wasapi);
        rig.asio = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Asio);
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QCOMPARE(rowsOf(w), (QStringList{
            QStringLiteral("heading|Speakers · ASIO|-|off"),
            QStringLiteral("device|(platform default)|ticked|on"),
            QStringLiteral("noDevices|No ASIO devices present|-|off"),
            QStringLiteral("separator"),
            QStringLiteral("setup|Sound setup…|-|on"),
        }));
        rig.engine->stop();
    }

    // ── 17. "Sound setup…" asks for Setup at Audio, Outputs ────────────────

    void soundSetupIsRequested() {
        Rig rig;
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QSignalSpy setup(&w, &MasterOutputWidget::soundSetupRequested);
        QSignalSpy device(&w, &MasterOutputWidget::outputDeviceChanged);
        trigger(w, QStringLiteral("Sound setup…"));
        QCOMPARE(setup.count(), 1);
        QCOMPARE(device.count(), 0);
        rig.engine->stop();
    }

    // ── 18. A pick saves what the Outputs card saves; the card follows ─────
    // (R-AUD-04, R-AUD-23). MainWindow reloads an open Outputs card after a
    // pick (DeviceCard::loadFromSettings), as done here.

    void pickSavesAsTheOutputsCardAndTheCardFollows() {
        auto& s = AppSettings::instance();
        saveChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                   QStringLiteral("Desk speakers"));
        s.setValue(QStringLiteral("audio/Speakers/BufferSamples"), QStringLiteral("512"));
        s.setValue(QStringLiteral("audio/Speakers/ExclusiveMode"), QStringLiteral("True"));
        Rig rig;
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        followPicks(w, *rig.engine);
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());

        trigger(w, QStringLiteral("Built-in speakers"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/Engine")).toString(),
                 QStringLiteral("CoreAudio"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/DeviceId")).toString(),
                 QStringLiteral("built-in-uid"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/DeviceName")).toString(),
                 QStringLiteral("Built-in speakers"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/FirstChannel")).toString(),
                 QStringLiteral("1"));
        // The rest kept, the retired WASAPI key untouched (D10).
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/BufferSamples")).toString(),
                 QStringLiteral("512"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/ExclusiveMode")).toString(),
                 QStringLiteral("True"));

        card.loadFromSettings();
        const AudioDeviceConfig shown = card.currentConfig();
        QCOMPARE(shown.deviceId, QStringLiteral("built-in-uid"));
        QCOMPARE(shown.deviceName, QStringLiteral("Built-in speakers"));
        QCOMPARE(shown.engine, std::optional<AudioEngineKind>(AudioEngineKind::CoreAudio));
        bool selected = false;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            selected = selected || combo->currentText() == QStringLiteral("Built-in speakers");
        }
        QVERIFY(selected);

        // "(platform default)" saves an empty identity.
        QTRY_COMPARE_WITH_TIMEOUT(
            rig.engine->roleStatus(AudioRole::Speakers).chosen.deviceId,
            QStringLiteral("built-in-uid"), 2000);
        trigger(w, QStringLiteral("(platform default)"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/DeviceId")).toString(), QString());
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/DeviceName")).toString(), QString());
        rig.engine->stop();
    }

    // ── 19. A device added while the menu is closed is in the next menu ────
    // with no Rescan (R-AUD-03).

    void deviceAddedIsInTheNextMenu() {
        Rig rig;
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QVERIFY(!rowsOf(w).contains(QStringLiteral("device|USB DAC|unticked|on")));
        rig.native->addDevice(deviceInfo(AudioBackendId::CoreAudio, QStringLiteral("dac-uid"),
                                         QStringLiteral("USB DAC")));
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(rowsOf(w).contains(QStringLiteral("device|USB DAC|unticked|on")),
                                 1000);
        rig.engine->stop();
    }

    // ── 20. On "(platform default)" the tooltip follows the default ────────
    // at once (R-AUD-12).

    void tooltipFollowsTheDefault() {
        Rig rig;
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QTRY_COMPARE_WITH_TIMEOUT(
            tooltipOf(w),
            QStringLiteral("PC volume. Click to mute, right-click for speakers. Playing on "
                           "Built-in speakers."),
            5000);
        QVERIFY(rowsOf(w).contains(QStringLiteral("device|(platform default)|ticked|on")));

        rig.native->setDefault(AudioDeviceDirection::Output, QStringLiteral("desk-uid"));
        rig.native->postNotice(AudioNotice::DefaultOutputChanged);
        QTRY_COMPARE_WITH_TIMEOUT(
            tooltipOf(w),
            QStringLiteral("PC volume. Click to mute, right-click for speakers. Playing on "
                           "Desk speakers."),
            1000);
        QVERIFY(rowsOf(w).contains(QStringLiteral("device|(platform default)|ticked|on")));
        rig.engine->stop();
    }

    // ── 21. Before the engine has its lists: shown, greyed, with why ───────
    // (disabled, never hidden).

    void withoutListsTheChoiceIsGreyedWithItsReason() {
        AppSettings::instance().setValue(QStringLiteral("audio/Speakers/DeviceName"),
                                         QStringLiteral("SomeDevice"));
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        if (engine.catalogue() != nullptr) {
            QSKIP("this engine lists devices before it starts");
        }
#if defined(Q_OS_MAC)
        const QString driver = QStringLiteral("Core Audio");
#elif defined(Q_OS_WIN)
        const QString driver = QStringLiteral("Windows audio, shared");
#else
        const QString driver = QStringLiteral("PipeWire");
#endif
        QCOMPARE(rowsOf(w), (QStringList{
            QStringLiteral("heading|Speakers · ") + driver + QStringLiteral("|-|off"),
            QStringLiteral("device|(platform default)|unticked|off"),
            QStringLiteral("device|SomeDevice|ticked|off"),
            QStringLiteral("notReady|The device lists are not ready.|-|off"),
            QStringLiteral("separator"),
            QStringLiteral("setup|Sound setup…|-|on"),
        }));
        std::unique_ptr<QMenu> menu(w.buildSpeakerMenuForTest());
        QVERIFY(menu->toolTipsVisible());
        QCOMPARE(menu->actions().at(1)->toolTip(), QStringLiteral("The device lists are not ready."));
        QCOMPARE(tooltipOf(w),
                 QStringLiteral("PC volume. Click to mute, right-click for speakers."));
    }

    // ── 22. V-UI-2 captures: the menu (normal, missing device, ASIO pairs) ─
    // and the tooltip, saved when NEREUS_HEADER_CAPTURE_DIR is set, for the
    // comparison with header-menu-mockup.html option A.

    void speakerMenuCaptures() {
        if (qEnvironmentVariable("NEREUS_HEADER_CAPTURE_DIR").isEmpty()) {
            QSKIP("NEREUS_HEADER_CAPTURE_DIR not set");
        }
        auto mockupDevices = [](Rig& rig) {
            rig.native->setDevices(
                {deviceInfo(AudioBackendId::CoreAudio, QStringLiteral("mbp-uid"),
                            QStringLiteral("MacBook Pro Speakers")),
                 deviceInfo(AudioBackendId::CoreAudio, QStringLiteral("pods-uid"),
                            QStringLiteral("AirPods Pro")),
                 deviceInfo(AudioBackendId::CoreAudio, QStringLiteral("foc-uid"),
                            QStringLiteral("Focusrite USB Audio"), 10)});
            rig.native->setDefault(AudioDeviceDirection::Output, QStringLiteral("mbp-uid"));
        };

        // Normal: the Mac, on a Focusrite pair.
        {
            saveChoice(AudioEngineKind::CoreAudio, QStringLiteral("foc-uid"),
                       QStringLiteral("Focusrite USB Audio"), 1);
            Rig rig;
            mockupDevices(rig);
            rig.start();
            MasterOutputWidget w(rig.engine.get());
            QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).state,
                                      AudioRoleState::Playing, 5000);
            captureMenu(w, QStringLiteral("header-menu-normal"));
            captureToolTip(w, QStringLiteral("header-tooltip-normal"));
            rig.engine->stop();
        }
        // Missing: the Focusrite unplugged.
        {
            saveChoice(AudioEngineKind::CoreAudio, QStringLiteral("foc-uid"),
                       QStringLiteral("Focusrite USB Audio"), 1);
            Rig rig;
            mockupDevices(rig);
            rig.native->removeDevice(QStringLiteral("foc-uid"), AudioDeviceDirection::Output);
            rig.start();
            MasterOutputWidget w(rig.engine.get());
            QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).state,
                                      AudioRoleState::PlayingOnDefault, 5000);
            captureMenu(w, QStringLiteral("header-menu-missing"));
            captureToolTip(w, QStringLiteral("header-tooltip-missing"));
            rig.engine->stop();
        }
        // ASIO pairs: Windows, the speakers on the Focusrite's ASIO driver.
        {
            saveChoice(AudioEngineKind::Asio, QStringLiteral("foc-asio"),
                       QStringLiteral("Focusrite USB ASIO"), 1);
            Rig rig(AudioBackendId::Wasapi);
            rig.asio = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Asio);
            rig.asio->setDevices({deviceInfo(AudioBackendId::Asio, QStringLiteral("foc-asio"),
                                             QStringLiteral("Focusrite USB ASIO"), 10)});
            rig.start();
            MasterOutputWidget w(rig.engine.get());
            QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).state,
                                      AudioRoleState::Playing, 5000);
            captureMenu(w, QStringLiteral("header-menu-asio-pairs"));
            rig.engine->stop();
        }
    }
    // ── 23. A pick on a second ASIO driver asks first (R-AUD-19) ───────────
    // The header runs the Setup card's one-driver prompt: Cancel keeps
    // everything as it was; Switch all moves the mic with the speakers.

    void secondAsioDriverCancelKeepsEverything() {
        Rig rig(AudioBackendId::Wasapi);
        twoAsioDrivers(rig);
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QSignalSpy picked(&w, &MasterOutputWidget::outputDeviceChanged);
        const QStringList before = audioKeysAndValues();

        const auto answer = answerSwitchDialog(false);
        triggerInGroup(w, kMotu, QStringLiteral("    Outputs 1-2"));
        QVERIFY(answer->seen);
        QCOMPARE(answer->text,
                 QStringLiteral("NereusSDR can use one ASIO driver at a time. Switching Speakers "
                                "to MOTU M Series also moves:"));
        QCOMPARE(answer->moves, (QStringList{QStringLiteral("Microphone: Inputs 1-2")}));
        QCOMPARE(picked.count(), 0);
        QCOMPARE(audioKeysAndValues(), before);
        rig.engine->stop();
    }

    void secondAsioDriverSwitchAllMovesEveryRole() {
        Rig rig(AudioBackendId::Wasapi);
        twoAsioDrivers(rig);
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QSignalSpy picked(&w, &MasterOutputWidget::outputDeviceChanged);

        const auto answer = answerSwitchDialog(true);
        triggerInGroup(w, kMotu, QStringLiteral("    Outputs 3-4"));
        QVERIFY(answer->seen);
        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/DeviceId")).toString(), kMotu);
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/FirstChannel")).toString(),
                 QStringLiteral("3"));
        QCOMPARE(s.value(QStringLiteral("audio/TxInput/DeviceId")).toString(), kMotu);
        QCOMPARE(s.value(QStringLiteral("audio/TxInput/FirstChannel")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(picked.count(), 1);
        QCOMPARE(picked.at(0).at(0).toString(), kMotu);
        rig.engine->stop();
    }

    void sameAsioDriverDoesNotAsk() {
        Rig rig(AudioBackendId::Wasapi);
        twoAsioDrivers(rig);
        rig.start();
        MasterOutputWidget w(rig.engine.get());
        QSignalSpy picked(&w, &MasterOutputWidget::outputDeviceChanged);

        const auto answer = answerSwitchDialog(false);
        triggerInGroup(w, kFocusrite, QStringLiteral("    Outputs 3-4"));
        QVERIFY(!answer->seen);
        QCOMPARE(picked.count(), 1);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Speakers/FirstChannel")).toString(),
                 QStringLiteral("3"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/TxInput/DeviceId")).toString(),
                 kFocusrite);
        rig.engine->stop();
    }
    // ── 24. The PC slider style greys while disabled, as RADIO's does ──────
    // (R-SPK-17): no cyan handle or fill, the dim handle instead.

    void pcSliderStyleGreysWhileDisabled() {
        const QColor cyan(0x00, 0xb4, 0xd8);
        const QColor dim(0x4a, 0x5a, 0x6a);
        QVERIFY(pixelsOf(HeaderVolumeStyle::kPcSlider, true, cyan) > 20);
        QCOMPARE(pixelsOf(HeaderVolumeStyle::kPcSlider, false, cyan), 0);
        QVERIFY(pixelsOf(HeaderVolumeStyle::kPcSlider, false, dim) > 20);
    }
};

QTEST_MAIN(TstMasterOutputWidget)
#include "tst_master_output_widget.moc"
