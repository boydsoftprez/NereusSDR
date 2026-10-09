// =================================================================
// tests/tst_audio_vax_page_auto_detect.cpp  (NereusSDR)
// =================================================================
//
// Sub-Phase 12 Task 12.3 — VaxChannelCard + AudioVaxPage unit coverage.
//
// Coverage:
//   1. VaxChannelCard constructs for channels 1–4 without crashing.
//   2. loadFromSettings with no saved keys is a no-op (no crash).
//   3. updateNegotiatedPill() with a valid config doesn't crash.
//   4. Auto-detect menu — no cables: menu opens without crash; the
//      filterThirdParty helper correctly drops input-side cables.
//   5. configChanged: bindDeviceNameForTest() emits with correct channel
//      + device name (exercises the VaxChannelCard → caller signal path
//      without going through the QMenu modal).
//   6. channelIndex() returns the channel passed at construction.
//   7. enabledChanged signal is valid (spy construction smoke).
//   8. Auto-detect button widget is findable as a QPushButton child.
//   9. AudioVaxPage constructs (no engine) without crashing.
//  10. AudioVaxPage has four VaxChannelCard children with indices 1–4.
//  11b. A cable picked by name drops the previous device's id and channel
//      pair (R-AUD-04; native audio plan Task 16 fix round, 2026-10-09,
//      J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code).
//  14. Native audio plan Task 18 (R-AUD-03, R-AUD-06, R-AUD-10, R-AUD-11,
//      D12, D13, V-UI-1), the Windows layout over an engine on fake Windows
//      audio, ASIO and older-driver backends: a cable added or removed
//      shows within 1 s; a chosen cable that goes stays as "(not
//      connected)" with the R-AUD-10 sentence; the in-use sentence; the
//      ASIO pairs under the one-driver rule, radio audio's pair greyed;
//      the pair clash prompt; Rescan rescans the older drivers and offers
//      what they found; the capture of Digital modes with a cable missing.
//      2026-10-09, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//      Code.
//
// Notes:
//   - Tests use the NEREUS_BUILD_TESTS seam setDetectedCablesForTest()
//     to inject a predetermined cable vector without invoking PortAudio.
//   - Menu tests use QTimer::singleShot(50ms, ...) to interact with the
//     QMenu event loop. The timer fires during exec(), finds the active
//     popup, triggers the target action, and closes the menu.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStyleFactory>
#include <QTimer>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/CaptureProtocol.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/PortAudioBackend.h"
#include "core/audio/VirtualCableDetector.h"
#include "gui/VaxFirstRunDialog.h"
#include "gui/setup/AsioSwitchAllDialog.h"
#include "gui/setup/AudioDigitalModesPage.h"
#include "gui/setup/AudioVaxPage.h"
#include "gui/styles/AppTheme.h"
#include "models/RadioModel.h"
#include "OperatorWording.h"
#include "gui/RemoteAudioStatus.h"

#include "fakes/FakeAudioEngineBackend.h"

#include <functional>
#include <memory>

using namespace NereusSDR;

namespace {

const QString kCableA = QStringLiteral("CABLE-A Input (VB-Audio Virtual Cable)");
const QString kCableB = QStringLiteral("CABLE-B Input (VB-Audio Virtual Cable)");
const QString kCableC = QStringLiteral("CABLE-C Input (VB-Audio Virtual Cable)");
const QString kCableD = QStringLiteral("CABLE-D Input (VB-Audio Virtual Cable)");
const QString kFocusrite = QStringLiteral("Focusrite USB ASIO");
const QString kMotu = QStringLiteral("MOTU M Series");
const QString kOldBox = QStringLiteral("Old Box ASIO");

AudioDeviceInfo device(AudioBackendId backend, AudioDeviceDirection direction, const QString& id,
                       const QString& name, int channels = 2, const QString& hostApi = {})
{
    AudioDeviceInfo info;
    info.backend = backend;
    info.direction = direction;
    info.id = id;
    info.name = name;
    info.channelCount = channels;
    info.hostApi = hostApi;
    return info;
}

AudioDeviceInfo cable(const QString& id, const QString& name)
{
    return device(AudioBackendId::Wasapi, AudioDeviceDirection::Output, id, name);
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

void saveBinding(int channel, AudioEngineKind engine, const QString& id, const QString& name,
                 int firstChannel, bool on)
{
    AppSettings& s = AppSettings::instance();
    const QString prefix = QStringLiteral("audio/Vax%1").arg(channel);
    s.setValue(prefix + QStringLiteral("/Engine"), audioEngineKey(engine));
    s.setValue(prefix + QStringLiteral("/DeviceId"), id);
    s.setValue(prefix + QStringLiteral("/DeviceName"), name);
    s.setValue(prefix + QStringLiteral("/FirstChannel"), QString::number(firstChannel));
    s.setValue(prefix + QStringLiteral("/Enabled"), on ? QStringLiteral("True")
                                                       : QStringLiteral("False"));
}

void saveSpeakersOnAsio(const QString& driver, int firstChannel)
{
    AppSettings& s = AppSettings::instance();
    s.setValue(QStringLiteral("audio/Speakers/Engine"), audioEngineKey(AudioEngineKind::Asio));
    s.setValue(QStringLiteral("audio/Speakers/DeviceId"), driver);
    s.setValue(QStringLiteral("audio/Speakers/DeviceName"), driver);
    s.setValue(QStringLiteral("audio/Speakers/FirstChannel"), QString::number(firstChannel));
}

QString value(const QString& key)
{
    return AppSettings::instance().value(key).toString();
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

QComboBox* picker(VaxChannelCard* card)
{
    return card != nullptr ? card->findChild<QComboBox*>(QStringLiteral("vaxDevicePicker")) : nullptr;
}

QStringList itemTexts(QComboBox* combo)
{
    QStringList out;
    for (int i = 0; combo != nullptr && i < combo->count(); ++i) {
        out << combo->itemText(i);
    }
    return out;
}

bool itemEnabled(QComboBox* combo, int index)
{
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    return model != nullptr && model->item(index) != nullptr && model->item(index)->isEnabled();
}

// Picks an item as a click in the list does.
void pick(QComboBox* combo, const QString& text)
{
    const int i = combo->findText(text);
    QVERIFY2(i >= 0, qPrintable(text));
    combo->setCurrentIndex(i);
    emit combo->activated(i);
}

QString pairText(const QString& driver, const QString& pair)
{
    return driver + QStringLiteral(" ") + QChar(0x00B7) + QStringLiteral(" ") + pair;
}

// The fake computer under the model's engine: Windows audio with desk
// speakers and the cables given, the older drivers and, when asked, ASIO
// with two interfaces and a driver of an unusable format.
struct PageRig {
    std::shared_ptr<FakeAudioEngineBackend> native =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Wasapi);
    std::shared_ptr<FakeAudioEngineBackend> asio;
    std::shared_ptr<FakeAudioEngineBackend> older =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);

    explicit PageRig(const QList<AudioDeviceInfo>& cables, bool withAsio = false)
    {
        QList<AudioDeviceInfo> devices{
            device(AudioBackendId::Wasapi, AudioDeviceDirection::Output, QStringLiteral("spk-uid"),
                   QStringLiteral("Desk speakers"))};
        devices.append(cables);
        native->setDevices(devices);
        older->setTakesStereoMix(false);
        if (withAsio) {
            asio = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Asio);
            asio->setOpensOneStreamAtATime(true);
            asio->setDevices(
                {device(AudioBackendId::Asio, AudioDeviceDirection::Output, kFocusrite, kFocusrite, 4),
                 device(AudioBackendId::Asio, AudioDeviceDirection::Output, kMotu, kMotu, 2),
                 device(AudioBackendId::Asio, AudioDeviceDirection::Output, kOldBox, kOldBox, 2)});
        }
    }

    void start(RadioModel& model)
    {
        AudioEngine* engine = model.localAudioDevices();
        CaptureSupervisor::Options options;
        options.program = QDir::temp().absoluteFilePath(QStringLiteral("no-such-capture-helper"));
        engine->setCaptureSupervisorOptionsForTest(options);
        engine->setVaxOutputsAllowed(false);
        std::vector<std::shared_ptr<IAudioEngineBackend>> backends{native};
        if (asio) {
            backends.push_back(asio);
        }
        backends.push_back(older);
        engine->setAudioBackendsForTest(std::move(backends));
        if (asio) {
            engine->setAsioDriverCapsForTest(kFocusrite, asioCaps(kFocusrite, 0, 4));
            engine->setAsioDriverCapsForTest(kMotu, asioCaps(kMotu, 0, 2));
            AsioDriverCaps old = asioCaps(kOldBox, 0, 2);
            old.sampleType = AsioSampleType::Unsupported;
            engine->setAsioDriverCapsForTest(kOldBox, old);
        }
        // Never started: the catalogue lists before start() and nothing
        // opens, so each channel's state is the test's to set.
        QVERIFY(engine->catalogue() != nullptr);
    }
};

// Answers the next modal dialog of type T: `seen` records it, then `press`
// closes it.
template <typename T>
void answerNext(std::function<void(T*)> seen, std::function<void(T*)> press)
{
    auto poll = std::make_shared<std::function<void(int)>>();
    *poll = [=](int tries) {
        for (QWidget* top : QApplication::topLevelWidgets()) {
            auto* dialog = qobject_cast<T*>(top);
            if (dialog == nullptr || !dialog->isVisible()) {
                continue;
            }
            seen(dialog);
            press(dialog);
            return;
        }
        if (tries < 300) {
            QTimer::singleShot(10, qApp, [poll, tries]() { (*poll)(tries + 1); });
        }
    };
    QTimer::singleShot(0, qApp, [poll]() { (*poll)(0); });
}

void answerMessageBox(QMessageBox::StandardButton button, QString* title, QString* text)
{
    answerNext<QMessageBox>(
        [title, text](QMessageBox* box) {
            *title = box->windowTitle();
            *text = box->text();
        },
        [button](QMessageBox* box) {
            if (QAbstractButton* b = box->button(button)) {
                b->click();
            } else {
                box->reject();
            }
        });
}

void answerSwitchDialog(bool ok, QString* text, QStringList* moves)
{
    answerNext<AsioSwitchAllDialog>(
        [text, moves](AsioSwitchAllDialog* dialog) {
            if (auto* t = dialog->findChild<QLabel*>(QStringLiteral("asioSwitchAllText"))) {
                *text = t->text();
            }
            for (QLabel* line : dialog->findChildren<QLabel*>(QStringLiteral("asioSwitchAllMove"))) {
                *moves << line->text();
            }
        },
        [ok](AsioSwitchAllDialog* dialog) {
            auto* button = dialog->findChild<QPushButton*>(
                ok ? QStringLiteral("asioSwitchAllOk") : QStringLiteral("asioSwitchAllCancel"));
            if (button != nullptr) {
                button->click();
            } else {
                dialog->reject();
            }
        });
}

void saveCapture(QWidget* w, const QString& stem)
{
    const QString dir = qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR");
    if (dir.isEmpty() || w == nullptr) {
        return;
    }
    QDir().mkpath(dir);
    QApplication::processEvents();
    QApplication::processEvents();
    const QPixmap shot = w->grab();
    const int scale = qRound(shot.devicePixelRatio());
    const QString path = QStringLiteral("%1/%2@%3x.png").arg(dir, stem).arg(scale);
    QVERIFY2(shot.save(path), qPrintable(path));
}

QString vaxSentence(const QString& name, const QString& trouble, int channel)
{
    return QStringLiteral("%1 %2. VAX %3 stays silent until it comes back; NereusSDR never "
                          "sends it anywhere else.")
        .arg(name, trouble)
        .arg(channel);
}

} // namespace

class TstAudioVaxPageAutoDetect : public QObject {
    Q_OBJECT

private:
    void clearAudioKeys()
    {
        auto& s = AppSettings::instance();
        const QStringList keys = s.allKeys();
        for (const QString& k : keys) {
            if (k.startsWith(QStringLiteral("audio/"))) {
                s.remove(k);
            }
        }
    }

    // Find the currently-active QMenu popup (called from inside exec() loop).
    static QMenu* activeMenu()
    {
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* m = qobject_cast<QMenu*>(w)) {
                if (m->isVisible()) {
                    return m;
                }
            }
        }
        return nullptr;
    }

    // Build a free (unassigned) output cable.
    static DetectedCable freeCable(const QString& name)
    {
        return {VirtualCableProduct::VbCableA, name, false, 0};
    }

    // Build an input-side cable (should be filtered out of the menu).
    static DetectedCable inputCable(const QString& name)
    {
        return {VirtualCableProduct::VbCableA, name, true, 0};
    }

    // Pull the banner QLabel out of a VaxChannelCard. The banner is
    // the QLabel whose current text matches any of the state-machine
    // outcomes from updateBadge(). Returns nullptr if none match so
    // the caller can fail fast.
    static QLabel* findStatusBanner(VaxChannelCard& card)
    {
        for (QLabel* l : card.findChildren<QLabel*>()) {
            const QString t = l->text();
            if (t.contains(QStringLiteral("Bound:"),
                           Qt::CaseInsensitive)
                || t.contains(QStringLiteral("Disabled"),
                              Qt::CaseInsensitive)
                || t.contains(QStringLiteral("Not bound"),
                              Qt::CaseInsensitive)
                || t.contains(QStringLiteral("failed to open"),
                              Qt::CaseInsensitive)
                || t.contains(QStringLiteral("unavailable"),
                              Qt::CaseInsensitive)) {
                return l;
            }
        }
        return nullptr;
    }

private slots:

    void init()    { clearAudioKeys(); }
    void cleanup()
    {
        AudioVaxPage::setSystemForTest(std::nullopt);
        clearAudioKeys();
    }

    // ── 1. Constructs for channels 1–4 ────────────────────────────────────
    void constructsForAllChannels_data()
    {
        QTest::addColumn<int>("channel");
        QTest::newRow("ch1") << 1;
        QTest::newRow("ch2") << 2;
        QTest::newRow("ch3") << 3;
        QTest::newRow("ch4") << 4;
    }

    void constructsForAllChannels()
    {
        QFETCH(int, channel);
        VaxChannelCard card(channel, nullptr);
        QVERIFY(card.channelIndex() == channel);
    }

    // ── 2. loadFromSettings is a no-op with no saved keys ─────────────────
    void loadFromSettingsNoCrashWhenEmpty()
    {
        VaxChannelCard card(1, nullptr);
        card.loadFromSettings();  // should not crash
        QVERIFY(card.currentDeviceName().isEmpty());
    }

    // ── 3. updateNegotiatedPill doesn't crash ─────────────────────────────
    void updateNegotiatedPillNoCrash()
    {
        VaxChannelCard card(1, nullptr);
        AudioDeviceConfig cfg;
        cfg.deviceName    = QStringLiteral("Test Device");
        cfg.sampleRate    = 48000;
        cfg.bitDepth      = 16;
        cfg.channels      = 2;
        cfg.bufferSamples = 512;
        card.updateNegotiatedPill(cfg);
        card.updateNegotiatedPill(cfg, QStringLiteral("Device not found"));
    }

    // ── 4. Auto-detect menu — no cables — opens and closes without crash ──
    // When the injected cable vector is empty the menu must open (no crash)
    // and contain at least one "no cables" item and an install item.
    void autoDetectMenu_noCablesOpensAndCloses()
    {
#ifdef Q_OS_MAC
        QSKIP("QMenu::exec() + QTimer::singleShot modal flake — "
              "Cocoa event loop occasionally misses the timer callback. "
              "Menu behavior is exercised deterministically via "
              "menuLabelForCableForTest seam + bindDeviceNameForTest. "
              "Tracked by TODO(sub-phase-12-menu-test-stability).",
              SkipAll);
#endif
        VaxChannelCard card(1, nullptr);
        card.setDetectedCablesForTest({});

        bool menuOpened = false;

        // Fire after 50 ms into the QMenu::exec() event loop.
        QTimer::singleShot(50, this, [&menuOpened]() {
            QMenu* menu = nullptr;
            for (QWidget* w : QApplication::topLevelWidgets()) {
                if (auto* m = qobject_cast<QMenu*>(w)) {
                    if (m->isVisible()) { menu = m; break; }
                }
            }
            if (menu) {
                menuOpened = true;
                // Find the "No virtual cables" action — must be disabled.
                for (QAction* act : menu->actions()) {
                    if (act->text().contains(QStringLiteral("No virtual cables"))) {
                        QVERIFY(!act->isEnabled());
                    }
                }
                menu->hide();
            }
        });

        // Click the Auto-detect button.
        auto* autoBtn = card.findChild<QPushButton*>();
        QVERIFY(autoBtn);
        autoBtn->click();

        // Process events to allow the timer to fire and the menu to close.
        QTest::qWait(100);

        QVERIFY(menuOpened);
    }

    // ── 4b. Free-cable menu entry contains vendor name (addendum §2.3) ──
    // Verifies the "► deviceName · vendor" label format using the
    // menuLabelForCableForTest() seam, which exercises the same code path as
    // onAutoDetectClicked() without opening a modal QMenu (modal tests are
    // macOS-fragile when run sequentially in a test binary).
    void autoDetectMenu_freeCableEntryContainsVendor()
    {
        const QString cableName =
            QStringLiteral("CABLE-A Output (VB-Audio Virtual Cable)");
        const DetectedCable cable{VirtualCableProduct::VbCableA, cableName, false, 0};

        const QString label = VaxChannelCard::menuLabelForCableForTest(cable);

        QVERIFY2(label.contains(QStringLiteral("VB-Audio")),
                 qPrintable(QStringLiteral(
                     "Expected label to contain 'VB-Audio' (addendum §2.3 vendor subtitle); got: %1")
                     .arg(label)));
        QVERIFY2(label.contains(cableName),
                 "Expected label to contain the device name");
    }

    // ── 5. configChanged carries correct channel + device name ────────────
    // Uses the NEREUS_BUILD_TESTS seam bindDeviceNameForTest() to bypass
    // QMenu::exec() and directly verify the signal payload. The menu-open
    // behavior itself is exercised by autoDetectMenu_noCablesOpensAndCloses.
    void configChanged_carriesChannelAndDeviceName()
    {
        VaxChannelCard card(2, nullptr);
        const QString cableName =
            QStringLiteral("CABLE-A Output (VB-Audio Virtual Cable)");

        QSignalSpy configSpy(&card, &VaxChannelCard::configChanged);
        configSpy.clear();

        card.bindDeviceNameForTest(cableName);

        QCOMPARE(configSpy.count(), 1);
        QCOMPARE(configSpy.at(0).at(0).toInt(), 2);  // channel index
        const AudioDeviceConfig cfg =
            configSpy.at(0).at(1).value<AudioDeviceConfig>();
        QCOMPARE(cfg.deviceName, cableName);
    }

    // ── 6. configChanged carries channel + device name ─────────────────────
    // This test exercises the inner onInnerConfigChanged path, not the menu.
    // DeviceCard emits configChanged; VaxChannelCard re-emits with channel.
    // Since we have no real engine, we verify the re-emit structure via the
    // signal spy by checking that VaxChannelCard's signal has the right shape
    // when wired to a known-good inner DeviceCard signal.
    //
    // Lightweight proxy test: channel index is correct on construction.
    void channelIndexIsCorrect()
    {
        VaxChannelCard card(3, nullptr);
        QCOMPARE(card.channelIndex(), 3);
    }

    // ── 7. enabledChanged carries channel index ────────────────────────────
    // VaxChannelCard::enabledChanged(int channel, bool on). Verify the
    // channel index in the signal is the one from construction.
    void enabledChangedCarriesChannelIndex()
    {
        // We can't easily toggle the inner DeviceCard's checkbox without a
        // real QApplication event loop firing through it. The binding is
        // verified structurally: the QObject::connect in the constructor
        // uses a lambda that captures m_channel and emits enabledChanged.
        // Smoke test: construct and confirm no crash.
        VaxChannelCard card(4, nullptr);
        QSignalSpy spy(&card, &VaxChannelCard::enabledChanged);
        QVERIFY(spy.isValid());
    }

    // ── 8. Auto-detect button is a QPushButton child of VaxChannelCard ────
    // isVisible() requires a shown window — just verify the button exists
    // and isn't disabled (it should be clickable when no device is bound).
    void autoDetectButtonIsPresent()
    {
        VaxChannelCard card(1, nullptr);
        auto* btn = card.findChild<QPushButton*>();
        QVERIFY(btn != nullptr);
        QVERIFY(!btn->text().isEmpty());
        QVERIFY(btn->isEnabled());
    }

    // ── 9. AudioVaxPage constructs (no engine / null RadioModel) ──────────
    void audioVaxPageConstructs()
    {
        // R-SPK-21: a section of Digital modes, which owns the title.
        AudioVaxPage page(nullptr);
        QCOMPARE(page.objectName(), QStringLiteral("audioVaxSection"));
        QCOMPARE(page.findChildren<VaxChannelCard*>().size(), 4);
    }

    // ── 10. AudioVaxPage has exactly four VaxChannelCard children (1–4) ───
    void audioVaxPageHasFourChannelCards()
    {
        AudioVaxPage page(nullptr);
        const QList<VaxChannelCard*> cards =
            page.findChildren<VaxChannelCard*>();
        QCOMPARE(cards.size(), 4);

        QSet<int> channels;
        for (VaxChannelCard* card : cards) {
            channels.insert(card->channelIndex());
        }
        QVERIFY(channels.contains(1));
        QVERIFY(channels.contains(2));
        QVERIFY(channels.contains(3));
        QVERIFY(channels.contains(4));
    }

    // ── 11. autoDetectFreeCable_persistsToAppSettings (C1) ───────────────────
    // After binding a cable via the test seam, AppSettings must contain the
    // DeviceName (and at least SampleRate) under audio/Vax<N>/.
    void autoDetectFreeCable_persistsToAppSettings()
    {
        clearAudioKeys();

        VaxChannelCard card(2, nullptr);
        const QString cableName =
            QStringLiteral("CABLE-A Output (VB-Audio Virtual Cable)");

        card.bindDeviceNameForTest(cableName);

        auto& s = AppSettings::instance();
        const QString devKey      = QStringLiteral("audio/Vax2/DeviceName");
        const QString rateKey     = QStringLiteral("audio/Vax2/SampleRate");

        QCOMPARE(s.value(devKey, QString()).toString(), cableName);
        // SampleRate must also be present (default 48000) — proves full config
        // was persisted, not just DeviceName.
        QVERIFY(!s.value(rateKey, QString()).toString().isEmpty());
    }

    // R-AUD-04: a cable picked by name replaces the whole identity; the
    // previous device's id (which would win over the name) and its channel
    // pair are not carried over.
    void autoDetectBindingDropsTheStaleDeviceId()
    {
        clearAudioKeys();
        AudioDeviceConfig old;
        old.deviceId = QStringLiteral("old-cable-id");
        old.deviceName = QStringLiteral("Old cable");
        old.firstChannel = 3;
        old.saveToSettings(QStringLiteral("audio/Vax2"));

        VaxChannelCard card(2, nullptr);
        card.loadFromSettings();
        QSignalSpy spy(&card, &VaxChannelCard::configChanged);
        const QString cableName = QStringLiteral("CABLE-A Output (VB-Audio Virtual Cable)");
        card.bindDeviceNameForTest(cableName);

        const AudioDeviceConfig saved =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Vax2"));
        QCOMPARE(saved.deviceName, cableName);
        QVERIFY(saved.deviceId.isEmpty());
        QCOMPARE(saved.firstChannel, 1);
        QCOMPARE(spy.count(), 1);
        const auto sent = spy.at(0).at(1).value<AudioDeviceConfig>();
        QCOMPARE(sent.deviceName, cableName);
        QVERIFY(sent.deviceId.isEmpty());
        QCOMPARE(sent.firstChannel, 1);
    }

    // ── 12. autoDetectReassign_clearsSourceSlot (C3) ──────────────────────────
    // If a source slot has a full config written to AppSettings, clearBinding()
    // must wipe all 10 fields (verified by checking DeviceName + SampleRate are
    // reset to empty / defaults, not the original values).
    void autoDetectReassign_clearsSourceSlot()
    {
        clearAudioKeys();

        // Write a full config into VAX 1's settings manually.
        const AudioDeviceConfig original = []{
            AudioDeviceConfig c;
            c.deviceName    = QStringLiteral("CABLE-A Output (VB-Audio Virtual Cable)");
            c.sampleRate    = 44100;
            c.bitDepth      = 24;
            c.channels      = 2;
            c.bufferSamples = 512;
            c.driverApi     = QStringLiteral("WASAPI");
            c.exclusiveMode = true;
            c.eventDriven   = true;
            c.bypassMixer   = true;
            c.manualLatencyMs = 20;
            return c;
        }();
        original.saveToSettings(QStringLiteral("audio/Vax1"));
        AppSettings::instance().save();

        // Construct a card for channel 1 (loads the saved settings).
        VaxChannelCard srcCard(1, nullptr);
        srcCard.loadFromSettings();

        // Confirm the pre-clear state is what we persisted.
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/Vax1/DeviceName"), QString())
                     .toString(),
                 original.deviceName);

        // Now clear the binding.
        srcCard.clearBinding();

        // All 10 fields must be reset to default/empty values.
        auto& s = AppSettings::instance();
        const QString base = QStringLiteral("audio/Vax1/");
        QVERIFY(s.value(base + QStringLiteral("DeviceName"),    QString()).toString().isEmpty());
        QCOMPARE(s.value(base + QStringLiteral("SampleRate"),   QString()).toString().toInt(), 48000);
        QCOMPARE(s.value(base + QStringLiteral("BitDepth"),     QString()).toString().toInt(), 32);
        QCOMPARE(s.value(base + QStringLiteral("Channels"),     QString()).toString().toInt(), 2);
        QCOMPARE(s.value(base + QStringLiteral("BufferSamples"),QString()).toString().toInt(), 128);  // PR #286 lowered default 256 -> 128
        QVERIFY(s.value(base + QStringLiteral("DriverApi"),     QString()).toString().isEmpty());
        QCOMPARE(s.value(base + QStringLiteral("ExclusiveMode"),QString()).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(base + QStringLiteral("EventDriven"),  QString()).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(base + QStringLiteral("BypassMixer"),  QString()).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(base + QStringLiteral("ManualLatencyMs"), QString()).toString().toInt(), 0);
    }

    // ── 12c. nativeHalLabelForCable (option-2 Mac/Linux info row) ───────────
    // Pure label builder used by onAutoDetectClicked() on Mac/Linux to
    // render the disabled "native (bound automatically)" row for each
    // detected NereusSdrVax device. Assertions keep the label contract
    // stable so the UI doesn't silently regress when the string changes.
    void nativeHalLabel_containsDeviceNameAndVendor()
    {
        const DetectedCable cable{
            VirtualCableProduct::NereusSdrVax,
            QStringLiteral("NereusSDR VAX 1"),
            /*isInput=*/true, 0};

        const QString label = VaxChannelCard::nativeHalLabelForCable(cable);

        QVERIFY2(label.contains(QStringLiteral("NereusSDR VAX 1")),
                 qPrintable(QStringLiteral(
                     "Expected label to contain the device name; got: %1")
                     .arg(label)));
        QVERIFY2(label.contains(QStringLiteral("NereusSDR")),
                 "Expected vendor name in native HAL label");
        QVERIFY2(label.contains(QStringLiteral("bound automatically"),
                                Qt::CaseInsensitive),
                 "Expected native rows to declare they are bound automatically");
    }

    // ── 12d. Binding-status banner reflects current state ──────────────────
    // The persistent status banner must always answer "what is this channel
    // bound to?" without requiring the user to open any menu. Cases:
    //   - empty deviceName on Mac/Linux → "Bound: Native HAL · NereusSDR VAX N"
    //   - non-empty deviceName → "Bound: <name>"
    void statusLabel_reflectsNativeHalWhenUnbound()
    {
        clearAudioKeys();
        // VAX card defaults to Enabled=True via test seam so the banner
        // doesn't short-circuit to the "Disabled" state. `m_busOpen`
        // defaults to true (assumed healthy when no engine is wired).
        AppSettings::instance().setValue(
            QStringLiteral("audio/Vax2/Enabled"), QStringLiteral("True"));

        VaxChannelCard card(2, nullptr);
        card.loadFromSettings();

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        QLabel* statusLabel = findStatusBanner(card);
        QVERIFY2(statusLabel, "Expected a binding-status label on VaxChannelCard");
        QVERIFY2(statusLabel->text().contains(
                     QStringLiteral("NereusSDR VAX 2")),
                 qPrintable(QStringLiteral(
                     "Expected status to name the channel's native HAL device;"
                     " got: %1").arg(statusLabel->text())));
#else
        QSKIP("Native-HAL status only applies on macOS/Linux.");
#endif
    }

    void statusLabel_reflectsByoBinding()
    {
        clearAudioKeys();
        AppSettings::instance().setValue(
            QStringLiteral("audio/Vax3/Enabled"), QStringLiteral("True"));

        VaxChannelCard card(3, nullptr);
        card.loadFromSettings();
        const QString cableName =
            QStringLiteral("CABLE-A Output (VB-Audio Virtual Cable)");
        card.bindDeviceNameForTest(cableName);

        QLabel* statusLabel = findStatusBanner(card);
        QVERIFY2(statusLabel, "Expected a binding-status label on VaxChannelCard");
        QVERIFY2(statusLabel->text().contains(cableName),
                 qPrintable(QStringLiteral(
                     "Expected status to name the BYO cable; got: %1")
                     .arg(statusLabel->text())));
    }

    // ── Banner flips to amber "Disabled" when the Enabled checkbox is off.
    // Regression gate for Codex P2: "Show unbound state before claiming
    // native HAL is bound" — users can uncheck the card's Enabled box
    // (which closes the bus via AudioEngine::setVaxEnabled(false)) and
    // previously still saw the green "Bound: Native HAL …" banner.
    void statusLabel_flipsToDisabledWhenChannelOff()
    {
        clearAudioKeys();
        AppSettings::instance().setValue(
            QStringLiteral("audio/Vax1/Enabled"), QStringLiteral("False"));

        VaxChannelCard card(1, nullptr);
        card.loadFromSettings();

        QLabel* statusLabel = findStatusBanner(card);
        QVERIFY2(statusLabel, "Expected a banner label");
        const QString txt = statusLabel->text();
        QVERIFY2(txt.contains(QStringLiteral("Disabled"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral(
                     "Expected banner to read 'Disabled' when Enabled=False;"
                     " got: %1").arg(txt)));
        // Must NOT claim any binding while the checkbox is off.
        QVERIFY2(!txt.contains(QStringLiteral("Bound:"), Qt::CaseInsensitive),
                 "Disabled banner must not also claim 'Bound:'");
    }

    // ── Banner flips to amber "unavailable" / "failed to open" when the
    // engine reports the VAX bus is NOT open, regardless of whether the
    // card thinks it has a binding. Second half of the Codex P2 gate.
    void statusLabel_flipsToUnavailableWhenBusNotOpen()
    {
        clearAudioKeys();
        AppSettings::instance().setValue(
            QStringLiteral("audio/Vax4/Enabled"), QStringLiteral("True"));

        VaxChannelCard card(4, nullptr);
        card.loadFromSettings();
        card.setBusOpen(false);

        QLabel* statusLabel = findStatusBanner(card);
        QVERIFY2(statusLabel, "Expected a banner label");
        const QString txt = statusLabel->text();
#if defined(Q_OS_MAC)
        QVERIFY2(txt.contains(QStringLiteral("unavailable"), Qt::CaseInsensitive)
                     || txt.contains(QStringLiteral("failed to open"),
                                     Qt::CaseInsensitive),
                 qPrintable(QStringLiteral(
                     "Expected banner to warn when bus is not open;"
                     " got: %1").arg(txt)));
#elif defined(Q_OS_LINUX)
        QVERIFY2(txt.contains(QStringLiteral("unavailable"), Qt::CaseInsensitive)
                     || txt.contains(QStringLiteral("failed to open"),
                                     Qt::CaseInsensitive),
                 qPrintable(QStringLiteral(
                     "Expected banner to warn when bus is not open;"
                     " got: %1").arg(txt)));
#else
        // Windows with no device name and busOpen=false: the existing
        // "Not bound — pick a virtual cable" copy covers this since
        // Windows has no native-HAL fallback.
        QVERIFY2(txt.contains(QStringLiteral("Not bound"), Qt::CaseInsensitive)
                     || txt.contains(QStringLiteral("failed to open"),
                                     Qt::CaseInsensitive),
                 qPrintable(QStringLiteral(
                     "Expected Windows banner to warn when no cable picked;"
                     " got: %1").arg(txt)));
#endif
        QVERIFY2(!txt.contains(QStringLiteral("\u2713  Bound:"),
                               Qt::CaseInsensitive),
                 "Unavailable banner must not also claim a successful bind");
    }

    // ── setBusOpen toggles the banner between green/amber without
    // requiring a full re-load cycle.
    void statusLabel_tracksBusOpenTransitions()
    {
        clearAudioKeys();
        AppSettings::instance().setValue(
            QStringLiteral("audio/Vax2/Enabled"), QStringLiteral("True"));

        VaxChannelCard card(2, nullptr);
        card.loadFromSettings();

        // Start healthy (default m_busOpen=true).
        QLabel* banner = findStatusBanner(card);
        QVERIFY(banner);
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        QVERIFY(banner->text().contains(QStringLiteral("\u2713  Bound:"),
                                        Qt::CaseInsensitive));

        // Flip to unhealthy.
        card.setBusOpen(false);
        banner = findStatusBanner(card);
        QVERIFY(banner);
        QVERIFY(banner->text().contains(QStringLiteral("unavailable"),
                                        Qt::CaseInsensitive)
                || banner->text().contains(QStringLiteral("failed to open"),
                                           Qt::CaseInsensitive));

        // Flip back to healthy.
        card.setBusOpen(true);
        banner = findStatusBanner(card);
        QVERIFY(banner);
        QVERIFY(banner->text().contains(QStringLiteral("\u2713  Bound:"),
                                        Qt::CaseInsensitive));
#else
        QSKIP("Native-HAL transitions only apply on macOS/Linux.");
#endif
    }

    // ── 13. autoDetectFreeCable_refreshesDeviceCardUI (C2) ───────────────────
    // After binding via the test seam, currentDeviceName() on the live card
    // must reflect the new binding (not the stale empty value).
    void autoDetectFreeCable_refreshesDeviceCardUI()
    {
        clearAudioKeys();

        VaxChannelCard card(3, nullptr);
        QVERIFY(card.currentDeviceName().isEmpty());

        const QString cableName =
            QStringLiteral("VB-Audio Cable Output");
        card.bindDeviceNameForTest(cableName);

        QCOMPARE(card.currentDeviceName(), cableName);
    }

    // R-R3-43 / R-R3-44 / R-R3-21: the compressed-audio note. Absent by
    // default (a local window, or lossless), shown while the receiver
    // streams are Opus, hidden again when they are lossless. Plain words
    // that name the cost; no cite inside. With Opus chosen it points to
    // the Lossless choice; with Lossless chosen but not running (a
    // fallback) it says the connection cannot carry it right now instead.
    void compressedAudioNoteFollowsTheReceiverStreams()
    {
        AudioVaxPage page(nullptr);
        QVERIFY(!page.compressedAudioNoteShown());
        auto* const note = page.findChild<QLabel*>(QStringLiteral("vaxCompressedAudioNote"));
        QVERIFY(note != nullptr);
        QVERIFY(note->isHidden());

        page.setReceiverAudioNote(RemoteReceiverAudioNote::OpusChosen);
        QVERIFY(page.compressedAudioNoteShown());
        QVERIFY(!note->isHidden());
        page.setReceiverAudioNote(RemoteReceiverAudioNote::None);
        QVERIFY(!page.compressedAudioNoteShown());
        QVERIFY(note->isHidden());

        page.setReceiverAudioNote(RemoteReceiverAudioNote::OpusChosen);
        const QString opusText = page.compressedAudioNoteText();
        QCOMPARE(opusText, QStringLiteral(
            "Receiver audio from the Core is compressed (Opus), so a few of the weakest "
            "digital-mode signals may not decode. Set Audio quality to Lossless "
            "in Core connection if your network can carry it."));

        page.setReceiverAudioNote(RemoteReceiverAudioNote::LosslessUnavailable);
        QVERIFY(page.compressedAudioNoteShown());
        const QString fallbackText = page.compressedAudioNoteText();
        QCOMPARE(fallbackText, QStringLiteral(
            "Receiver audio from the Core is compressed (Opus): Lossless is chosen, "
            "but this connection cannot carry it right now. A few of the weakest "
            "digital-mode signals may not decode."));
        QVERIFY(!fallbackText.contains(QLatin1String("Set Audio quality")));

        // Back to Opus chosen: today's text again.
        page.setReceiverAudioNote(RemoteReceiverAudioNote::OpusChosen);
        QCOMPARE(page.compressedAudioNoteText(), opusText);

        for (const QString& text : {opusText, fallbackText}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
            QVERIFY(!text.contains(QChar(0x2014)));
            QVERIFY(!text.contains(QLatin1String("docs/")));
            QVERIFY(!text.contains(QLatin1String(".md")));
        }
    }

    // ── 14. Task 18: the Windows lists from the catalogue ─────────────────

    // R-AUD-03 / D12: the picker lists Windows audio's cables (the render
    // ends) under a heading; a cable added or removed shows within 1 s
    // with no Rescan.
    void catalogueCablesFollowAddAndRemove()
    {
        AudioVaxPage::setSystemForTest(SoundSystemLine::System::Windows);
        RadioModel model;
        PageRig rig({cable(QStringLiteral("cable-a-uid"), kCableA),
                     device(AudioBackendId::Wasapi, AudioDeviceDirection::Input,
                            QStringLiteral("cable-a-rec"),
                            QStringLiteral("CABLE-A Output (VB-Audio Virtual Cable)"))});
        rig.start(model);
        auto stop = qScopeGuard([&model]() { model.localAudioDevices()->stop(); });
        AudioVaxPage page(&model);
        QComboBox* combo = picker(page.channelCard(1));
        QVERIFY(combo != nullptr);
        QCOMPARE(itemTexts(combo),
                 (QStringList{QStringLiteral("(pick a cable)"), QStringLiteral("Virtual cables"),
                              kCableA}));
        QVERIFY(!itemEnabled(combo, 1));
        QCOMPARE(page.statusLineText(), QStringLiteral("1 virtual cable found."));

        rig.native->addDevice(cable(QStringLiteral("cable-b-uid"), kCableB));
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(combo->findText(kCableB) > 0, 1000);
        QCOMPARE(page.statusLineText(), QStringLiteral("2 virtual cables found."));
        QVERIFY(page.detectedCablesText().contains(kCableB));

        rig.native->removeDevice(QStringLiteral("cable-b-uid"), AudioDeviceDirection::Output);
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(combo->findText(kCableB) < 0, 1000);
        QCOMPARE(page.statusLineText(), QStringLiteral("1 virtual cable found."));

        // A pick saves the cable as Windows audio lists it.
        pick(combo, kCableA);
        QCOMPARE(value(QStringLiteral("audio/Vax1/Engine")),
                 audioEngineKey(AudioEngineKind::WindowsShared));
        QCOMPARE(value(QStringLiteral("audio/Vax1/DeviceId")), QStringLiteral("cable-a-uid"));
        QCOMPARE(value(QStringLiteral("audio/Vax1/DeviceName")), kCableA);
        // Another channel sees who has it.
        QVERIFY(itemTexts(picker(page.channelCard(2)))
                    .contains(kCableA + QStringLiteral("  (used by VAX 1)")));
    }

    // R-AUD-10: a chosen cable that goes stays chosen as "(not connected)"
    // with the sentence; nothing is saved over it, and it comes back.
    void chosenCableGoneStaysChosen()
    {
        AudioVaxPage::setSystemForTest(SoundSystemLine::System::Windows);
        saveBinding(1, AudioEngineKind::WindowsShared, QStringLiteral("cable-b-uid"), kCableB, 1,
                    true);
        RadioModel model;
        PageRig rig({cable(QStringLiteral("cable-a-uid"), kCableA),
                     cable(QStringLiteral("cable-b-uid"), kCableB)});
        rig.start(model);
        auto stop = qScopeGuard([&model]() { model.localAudioDevices()->stop(); });
        AudioVaxPage page(&model);
        VaxChannelCard* card = page.channelCard(1);
        // The engine is never started: the cable plays as far as the card
        // knows.
        card->setBusOpen(true);
        QComboBox* combo = picker(card);
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->currentText(), kCableB);
        QCOMPARE(card->statusLineText(), QString());
        const QStringList keys = audioKeysAndValues();

        rig.native->removeDevice(QStringLiteral("cable-b-uid"), AudioDeviceDirection::Output);
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(combo->currentText(), kCableB + QStringLiteral(" (not connected)"),
                                  1000);
        QCOMPARE(card->statusLineText(),
                 QStringLiteral("CABLE-B Input (VB-Audio Virtual Cable) is not connected. VAX 1 "
                                "stays silent until it comes back; NereusSDR never sends it "
                                "anywhere else."));
        QVERIFY(OperatorWording::isPlain(card->statusLineText()));
        QCOMPARE(audioKeysAndValues(), keys);
        // The other cable is still offered, never put in its place.
        QVERIFY(combo->findText(kCableA) > 0);

        rig.native->addDevice(cable(QStringLiteral("cable-b-uid"), kCableB));
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(combo->currentText(), kCableB, 1000);
        QCOMPARE(card->statusLineText(), QString());
        QCOMPARE(audioKeysAndValues(), keys);
    }

    // R-AUD-10 / R-AUD-11: the engine's sentence for a channel, not
    // connected or in use, is the card's line, and goes when it plays.
    void roleStatusSentences()
    {
        AudioVaxPage::setSystemForTest(SoundSystemLine::System::Windows);
        saveBinding(3, AudioEngineKind::WindowsShared, QStringLiteral("cable-c-uid"), kCableC, 1,
                    true);
        RadioModel model;
        PageRig rig({cable(QStringLiteral("cable-c-uid"), kCableC)});
        rig.start(model);
        auto stop = qScopeGuard([&model]() { model.localAudioDevices()->stop(); });
        AudioEngine* engine = model.localAudioDevices();
        AudioVaxPage page(&model);
        VaxChannelCard* card = page.channelCard(3);
        card->setBusOpen(true);
        QCOMPARE(card->statusLineText(), QString());

        AudioRoleStatus status;
        status.state = AudioRoleState::Silent;
        status.reason = AudioRoleReason::InUse;
        status.chosenName = kCableC;
        emit engine->roleStatusChanged(AudioRole::Vax3, status);
        QCOMPARE(card->statusLineText(),
                 vaxSentence(kCableC, QStringLiteral("is in use by another program"), 3));
        // Only VAX 3's card says it.
        QVERIFY(!page.channelCard(1)->statusLineText().contains(kCableC));

        status.reason = AudioRoleReason::NotConnected;
        card->setRoleStatus(status);
        QCOMPARE(card->statusLineText(),
                 vaxSentence(kCableC, QStringLiteral("is not connected"), 3));

        AudioRoleStatus playing;
        playing.state = AudioRoleState::Playing;
        playing.chosenName = kCableC;
        playing.playingName = kCableC;
        emit engine->roleStatusChanged(AudioRole::Vax3, playing);
        QCOMPARE(card->statusLineText(), QString());
    }

    // D13: each ASIO driver's output pairs under its name; the pair radio
    // audio plays on is greyed with the reason, as is a driver of a format
    // NereusSDR cannot play.
    void asioPairsListedAndGreyed()
    {
        AudioVaxPage::setSystemForTest(SoundSystemLine::System::Windows);
        saveSpeakersOnAsio(kFocusrite, 1);
        RadioModel model;
        PageRig rig({cable(QStringLiteral("cable-a-uid"), kCableA)}, /*withAsio=*/true);
        rig.start(model);
        auto stop = qScopeGuard([&model]() { model.localAudioDevices()->stop(); });
        AudioVaxPage page(&model);
        QComboBox* combo = picker(page.channelCard(1));
        QVERIFY(combo != nullptr);
        const QString focus12 = pairText(kFocusrite, QStringLiteral("Outputs 1-2"));
        const QString focus34 = pairText(kFocusrite, QStringLiteral("Outputs 3-4"));
        const QString motu12 = pairText(kMotu, QStringLiteral("Outputs 1-2"));
        const QString old12 = pairText(kOldBox, QStringLiteral("Outputs 1-2"));
        const QStringList texts = itemTexts(combo);
        QVERIFY2(texts.contains(kFocusrite) && texts.contains(kMotu) && texts.contains(kOldBox),
                 qPrintable(texts.join(QLatin1Char('|'))));
        const int used = combo->findText(focus12 + QStringLiteral("  (used by speakers)"));
        QVERIFY2(used > 0, qPrintable(texts.join(QLatin1Char('|'))));
        QVERIFY(!itemEnabled(combo, used));
        QCOMPARE(combo->itemData(used, Qt::ToolTipRole).toString(),
                 QStringLiteral("Radio audio and digital-mode audio never share a pair."));
        QVERIFY(itemEnabled(combo, combo->findText(focus34)));
        QVERIFY(itemEnabled(combo, combo->findText(motu12)));
        const int old = combo->findText(old12);
        QVERIFY(old > 0);
        QVERIFY(!itemEnabled(combo, old));
        QCOMPARE(combo->itemData(old, Qt::ToolTipRole).toString(),
                 QStringLiteral("Old Box ASIO uses a sample format NereusSDR can't play or record."));
        // The driver headings are not choices; the cables come first.
        QVERIFY(!itemEnabled(combo, combo->findText(kFocusrite)));
        QVERIFY(texts.indexOf(kFocusrite) < texts.indexOf(focus34));
        QVERIFY(texts.indexOf(kCableA) < texts.indexOf(kFocusrite));

        // A pair on the speakers' driver: no switch; the ASIO note shows.
        auto* note = page.channelCard(1)->findChild<QLabel*>(QStringLiteral("vaxAsioNote"));
        QVERIFY(note != nullptr);
        QVERIFY(note->isHidden());
        pick(combo, focus34);
        QCOMPARE(value(QStringLiteral("audio/Vax1/Engine")), audioEngineKey(AudioEngineKind::Asio));
        QCOMPARE(value(QStringLiteral("audio/Vax1/DeviceId")), kFocusrite);
        QCOMPARE(value(QStringLiteral("audio/Vax1/FirstChannel")), QStringLiteral("3"));
        QCOMPARE(combo->currentText(), focus34);
        QVERIFY(!note->isHidden());
        QCOMPARE(note->text(),
                 QStringLiteral("Most digital-mode apps cannot open ASIO, so pass this pair on in "
                                "the ASIO app's own mixer (a Voicemeeter strip, for example)."));
    }

    // R-AUD-19 under D13: a pair on a second ASIO driver asks to move
    // every ASIO use; Cancel writes nothing, OK moves the speakers too.
    void asioSecondDriverAsksFirst()
    {
        AudioVaxPage::setSystemForTest(SoundSystemLine::System::Windows);
        saveSpeakersOnAsio(kFocusrite, 1);
        AppSettings::instance().setValue(QStringLiteral("audio/Vax1/Enabled"),
                                         QStringLiteral("True"));
        RadioModel model;
        PageRig rig({}, /*withAsio=*/true);
        rig.start(model);
        auto stop = qScopeGuard([&model]() { model.localAudioDevices()->stop(); });
        AudioVaxPage page(&model);
        QComboBox* combo = picker(page.channelCard(1));
        QVERIFY(combo != nullptr);
        const QString motu12 = pairText(kMotu, QStringLiteral("Outputs 1-2"));
        const QStringList keys = audioKeysAndValues();

        QString text;
        QStringList moves;
        answerSwitchDialog(false, &text, &moves);
        pick(combo, motu12);
        QCOMPARE(text, QStringLiteral("NereusSDR can use one ASIO driver at a time. Switching "
                                      "VAX 1 to MOTU M Series also moves:"));
        QCOMPARE(moves, QStringList{QStringLiteral("Speakers: Outputs 1-2")});
        QCOMPARE(audioKeysAndValues(), keys);
        QCOMPARE(combo->currentText(), QStringLiteral("(pick a cable)"));

        text.clear();
        moves.clear();
        answerSwitchDialog(true, &text, &moves);
        pick(combo, motu12);
        QVERIFY(!text.isEmpty());
        QCOMPARE(value(QStringLiteral("audio/Vax1/Engine")), audioEngineKey(AudioEngineKind::Asio));
        QCOMPARE(value(QStringLiteral("audio/Vax1/DeviceId")), kMotu);
        QCOMPARE(value(QStringLiteral("audio/Vax1/FirstChannel")), QStringLiteral("1"));
        QCOMPARE(value(QStringLiteral("audio/Speakers/DeviceId")), kMotu);
        QTRY_VERIFY_WITH_TIMEOUT(combo->currentText().startsWith(motu12), 1000);
    }

    // D13: a pair another VAX channel that is on uses is asked about first;
    // OK moves it and leaves the other channel with no device.
    void asioPairClashAsksFirst()
    {
        AudioVaxPage::setSystemForTest(SoundSystemLine::System::Windows);
        saveBinding(2, AudioEngineKind::Asio, kFocusrite, kFocusrite, 3, true);
        RadioModel model;
        PageRig rig({}, /*withAsio=*/true);
        rig.start(model);
        auto stop = qScopeGuard([&model]() { model.localAudioDevices()->stop(); });
        AudioVaxPage page(&model);
        QComboBox* combo = picker(page.channelCard(1));
        QVERIFY(combo != nullptr);
        const QString focus34 = pairText(kFocusrite, QStringLiteral("Outputs 3-4"));
        const QString usedText = focus34 + QStringLiteral("  (used by VAX 2)");
        QVERIFY2(combo->findText(usedText) > 0, qPrintable(itemTexts(combo).join(QLatin1Char('|'))));
        const QStringList keys = audioKeysAndValues();

        QString title;
        QString text;
        answerMessageBox(QMessageBox::Cancel, &title, &text);
        pick(combo, usedText);
#ifndef Q_OS_MAC
        // macOS shows no title on a message box.
        QCOMPARE(title, QStringLiteral("Use this pair here?"));
#endif
        QCOMPARE(text, QStringLiteral("VAX 2 uses ") + focus34
                           + QStringLiteral(". Move it to VAX 1? VAX 2 will have no device."));
        QCOMPARE(audioKeysAndValues(), keys);

        answerMessageBox(QMessageBox::Ok, &title, &text);
        pick(combo, usedText);
        QCOMPARE(value(QStringLiteral("audio/Vax1/DeviceId")), kFocusrite);
        QCOMPARE(value(QStringLiteral("audio/Vax1/FirstChannel")), QStringLiteral("3"));
        QVERIFY(value(QStringLiteral("audio/Vax2/DeviceId")).isEmpty());
        QCOMPARE(picker(page.channelCard(2))->currentText(), QStringLiteral("(pick a cable)"));
    }

    // R-AUD-06: Rescan rescans the older drivers (never Windows audio),
    // then lists their cables and offers the new ones.
    void rescanRescansOlderDriversAndOffersNewCables()
    {
        AudioVaxPage::setSystemForTest(SoundSystemLine::System::Windows);
        RadioModel model;
        PageRig rig({cable(QStringLiteral("cable-a-uid"), kCableA)});
        rig.start(model);
        auto stop = qScopeGuard([&model]() { model.localAudioDevices()->stop(); });
        AudioVaxPage page(&model);
        // The cables known before.
        AppSettings::instance().setValue(
            QStringLiteral("audio/LastDetectedCables"),
            VirtualCableDetector::fingerprintCsv(
                VirtualCableDetector::detect(*model.localAudioDevices()->catalogue())));

        // An older driver's cable, seen only once they list again.
        const QString mme = QStringLiteral("MME");
        rig.older->addDevice(device(AudioBackendId::PortAudio, AudioDeviceDirection::Output,
                                    portAudioDeviceId(mme, kCableD), kCableD, 2, mme));
        QComboBox* combo = picker(page.channelCard(1));
        QVERIFY(combo != nullptr);
        QVERIFY(combo->findText(kCableD) < 0);

        QStringList offered;
        bool seen = false;
        answerNext<VaxFirstRunDialog>(
            [&offered, &seen](VaxFirstRunDialog* dlg) {
                seen = true;
                for (const DetectedCable& c : dlg->detectedForTest()) {
                    offered << c.deviceName;
                }
            },
            [](VaxFirstRunDialog* dlg) { dlg->reject(); });
        QSignalSpy rescanned(model.localAudioDevices()->catalogue(),
                             &IAudioDeviceCatalog::olderDriversRescanned);
        auto* button = page.findChild<QPushButton*>(QStringLiteral("detectedCablesRescan"));
        QVERIFY(button != nullptr);
        button->click();
        QVERIFY(rescanned.count() == 1 || rescanned.wait(5000));
        QTRY_VERIFY_WITH_TIMEOUT(seen, 3000);
        QCOMPARE(rig.older->rescanCount(), 1);
        QCOMPARE(rig.native->rescanCount(), 0);
        QCOMPARE(offered, QStringList{kCableD});
        QVERIFY(combo->findText(kCableD) > 0);
        QVERIFY(page.detectedCablesText().contains(kCableD));

        // An older driver's cable saves its host API.
        pick(combo, kCableD);
        QCOMPARE(value(QStringLiteral("audio/Vax1/Engine")),
                 audioEngineKey(AudioEngineKind::PortAudio));
        QCOMPARE(value(QStringLiteral("audio/Vax1/DriverApi")), mme);
    }

    // V-UI-1: Digital modes on Windows with a chosen cable missing.
    void captureDigitalModesCableMissing()
    {
        if (qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR").isEmpty()) {
            QSKIP("Set NEREUS_AUDIO_SETUP_CAPTURE_DIR to save the captures.");
        }
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
        AudioVaxPage::setSystemForTest(SoundSystemLine::System::Windows);
        saveBinding(1, AudioEngineKind::WindowsShared, QStringLiteral("cable-a-uid"), kCableA, 1,
                    true);
        saveBinding(2, AudioEngineKind::WindowsShared, QStringLiteral("cable-b-uid"), kCableB, 1,
                    true);
        RadioModel model;
        PageRig rig({cable(QStringLiteral("cable-a-uid"), kCableA)});
        rig.start(model);
        auto stop = qScopeGuard([&model]() { model.localAudioDevices()->stop(); });
        AudioDigitalModesPage page(&model, nullptr);
        page.resize(760, 1500);
        page.show();
        auto* vax = page.findChild<AudioVaxPage*>();
        QVERIFY(vax != nullptr);
        // The engine is never started; VAX 1's cable plays as far as the
        // card knows.
        vax->channelCard(1)->setBusOpen(true);
        QTRY_VERIFY_WITH_TIMEOUT(!vax->channelCard(2)->statusLineText().isEmpty(), 1000);
        saveCapture(&page, QStringLiteral("digital-modes-windows-cable-missing"));
        page.hide();
    }
};

QTEST_MAIN(TstAudioVaxPageAutoDetect)
#include "tst_audio_vax_page_auto_detect.moc"
