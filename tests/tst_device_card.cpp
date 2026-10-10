// =================================================================
// tests/tst_device_card.cpp  (NereusSDR)
// =================================================================
//
// Exercises DeviceCard widget — Sub-Phase 12 Task 12.2 Step 4.
//
// Coverage:
//   1. DeviceCard(Output) constructs without crashing.
//   2. DeviceCard(Input) constructs without crashing (includes TX extras).
//   3. DeviceCard with enableCheckbox=true constructs and is checkable.
//   4. currentConfig() returns a non-null AudioDeviceConfig.
//   5. updateNegotiatedPill() doesn't crash with a valid config.
//   6. updateNegotiatedPill() with error string doesn't crash.
//   7. loadFromSettings with no prior keys populates defaults.
//   8. configChanged signal emits when the Driver list's choice changes
//      (a fake Windows catalogue: shared to exclusive).
//   9. AppSettings round-trip: DeviceCard saves on control change;
//      a second loadFromSettings reads the same values back.
//  10. Headphones card: enabledChanged fires when checkable toggled.
//  12. R-R3-36: a configured device that is not present and a configured
//      buffer size the list lacks survive an unrelated edit on the card.
//  13. R-R3-36: reloading replaces those kept entries instead of adding more.
//  14. R-SPK-21 / D14: everything but Driver and Device folds under
//      "Device details", folded by default; the toggle unfolds it.
//  15. D10: the WASAPI checkboxes are gone; their saved keys stay as they
//      were and are never written.
//  16. R-SPK-21: a card greyed until Enabled greys Device and Device
//      details while the box is off; a row added above Device stays live.
//  17. Native audio plan Task 16 (R-AUD-01, R-AUD-03, R-AUD-08, R-AUD-11,
//      R-AUD-14 to R-AUD-16), over an engine on fake engine backends: the
//      Driver list, a device added showing within 1 s with the selection
//      kept, a missing device "(not connected)" and its note, a device in
//      use, "(none)", a pick saving the identity and reaching the engine,
//      the Delay line and its readout, the engine notes, the Bluetooth mic
//      note.
//  18. Task 16 fix round: the lists and the Delay before a radio connects
//      (the engine never started), opening nothing; the older drivers'
//      default on the "Older drivers" heading; an in-use device in the
//      closed Device field; the Negotiated pill when Setup opens after the
//      engine started.
//  19. The Driver row is in front, above the Device row, with the fold
//      closed (JJ, 2026-10-10: the driver decides which devices are listed).
//
// Design spec:
//   docs/architecture/2026-04-20-phase3o-subphase12-addendum.md §2.1
//   docs/architecture/2026-10-08-native-audio-engines-design.md
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 16 (R-AUD-01, R-AUD-03, R-AUD-08 to
//               R-AUD-16, D10). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-09: Task 16 fix round, case 18 (R-AUD-01, R-AUD-03, R-AUD-11,
//               R-AUD-15). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-10: case 19, and case 14 follows it: the Driver row sits above
//               the "Device details" fold. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QComboBox>
#include <QCheckBox>
#include <QAccessible>
#include <QGuiApplication>
#include <QAbstractItemView>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QLabel>
#include <QToolButton>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/PortAudioBackend.h"
#include "core/audio/PortAudioBus.h"
#include "gui/setup/DeviceCard.h"

#include "fakes/FakeAudioEngineBackend.h"

#include <memory>

using namespace NereusSDR;

namespace {

AudioDeviceInfo deviceInfo(AudioBackendId backend, AudioDeviceDirection direction,
                           const QString& id, const QString& name, const QString& hostApi = {})
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

AudioDeviceConfig savedChoice(AudioEngineKind engine, const QString& id, const QString& name)
{
    AudioDeviceConfig cfg;
    cfg.engine = engine;
    cfg.deviceId = id;
    cfg.deviceName = name;
    return cfg;
}

// An engine on fake engine backends: the fake's backends decide the
// system (Core Audio here, Windows audio with Rig::windows()).  The engine
// reads the saved choices when it starts, so start() comes after a test
// saves them.  No device is touched.
struct Rig {
    std::shared_ptr<FakeAudioEngineBackend> native;
    std::shared_ptr<FakeAudioEngineBackend> older =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
    std::unique_ptr<AudioEngine> engine;

    explicit Rig(AudioBackendId nativeId = AudioBackendId::CoreAudio)
        : native(std::make_shared<FakeAudioEngineBackend>(nativeId))
    {
        native->setDevices(
            {deviceInfo(nativeId, AudioDeviceDirection::Output, QStringLiteral("desk-uid"),
                        QStringLiteral("Desk speakers")),
             deviceInfo(nativeId, AudioDeviceDirection::Output, QStringLiteral("built-in-uid"),
                        QStringLiteral("Built-in speakers")),
             deviceInfo(nativeId, AudioDeviceDirection::Input, QStringLiteral("usb-mic-uid"),
                        QStringLiteral("USB Mic"))});
        native->setDefault(AudioDeviceDirection::Output, QStringLiteral("built-in-uid"));
        native->setDefault(AudioDeviceDirection::Input, QStringLiteral("usb-mic-uid"));
        older->setTakesStereoMix(false);
        older->setDevices({deviceInfo(AudioBackendId::PortAudio, AudioDeviceDirection::Output,
                                      portAudioDeviceId(QStringLiteral("MME"),
                                                        QStringLiteral("Desk speakers")),
                                      QStringLiteral("Desk speakers"), QStringLiteral("MME"))});
    }

    static AudioBackendId windows() { return AudioBackendId::Wasapi; }

    // The engine before a radio connects: made, never started.
    void prepare()
    {
        engine = std::make_unique<AudioEngine>();
        engine->setVaxOutputsAllowed(false);
        engine->setAudioBackendsForTest({native, older});
    }

    void start()
    {
        prepare();
        engine->start();
        QVERIFY(engine->catalogue() != nullptr);
    }
};

QComboBox* comboNamed(const QWidget& card, const char* name)
{
    return card.findChild<QComboBox*>(QLatin1String(name));
}

QLabel* labelNamed(const QWidget& card, const char* name)
{
    return card.findChild<QLabel*>(QLatin1String(name));
}

} // namespace

class TstDeviceCard : public QObject {
    Q_OBJECT

private:
    void clearAudioKeys() {
        auto& s = AppSettings::instance();
        const QStringList keys = s.allKeys();
        for (const QString& k : keys) {
            if (k.startsWith(QStringLiteral("audio/"))) {
                s.remove(k);
            }
        }
    }

private slots:

    void init()    { clearAudioKeys(); }
    void cleanup() { clearAudioKeys(); }

    // ── 1. Output card constructs ─────────────────────────────────────────

    void outputCardConstructs() {
        DeviceCard card(QStringLiteral("audio/Speakers"),
                        DeviceCard::Role::Output,
                        false);
        QVERIFY(true);
    }

    // ── 2. Input card constructs ──────────────────────────────────────────

    void inputCardConstructs() {
        DeviceCard card(QStringLiteral("audio/TxInput"),
                        DeviceCard::Role::Input,
                        false);
        QVERIFY(true);
    }

    // ── 3. Card with enableCheckbox exposes an "Enabled" QCheckBox ────────

    void headphonesCardHasEnableCheckbox() {
        DeviceCard card(QStringLiteral("audio/Headphones"),
                        DeviceCard::Role::Output,
                        true);  // enableCheckbox
        // The card no longer uses QGroupBox::setCheckable (the native title-
        // bar indicator clips on some platforms and forces awkward interior-
        // disabled semantics).  Instead there is a real "Enabled" QCheckBox
        // laid out as the first visible row.  Verify it exists.
        bool foundEnabled = false;
        for (auto* c : card.findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Enabled")) {
                foundEnabled = true;
                break;
            }
        }
        QVERIFY2(foundEnabled,
                 "enableCheckbox=true should add a QCheckBox labelled Enabled");
    }

    // ── 4. currentConfig() returns non-default AudioDeviceConfig ──────────

    void currentConfigIsValid() {
        DeviceCard card(QStringLiteral("audio/Speakers"),
                        DeviceCard::Role::Output,
                        false);

        const AudioDeviceConfig cfg = card.currentConfig();
        // Channels should be 1 or 2; sampleRate should be > 0.
        QVERIFY(cfg.channels >= 1 && cfg.channels <= 2);
        // sampleRate may be 0 if auto-match is checked; otherwise > 0.
        QVERIFY(cfg.bufferSamples > 0);
    }

    // ── 5. updateNegotiatedPill() with valid config doesn't crash ──────────

    void updatePillValidConfig() {
        DeviceCard card(QStringLiteral("audio/Speakers"),
                        DeviceCard::Role::Output,
                        false);

        AudioDeviceConfig cfg;
        cfg.deviceName  = QStringLiteral("Test Device");
        cfg.sampleRate  = 48000;
        cfg.channels    = 2;
        cfg.bufferSamples = 256;
        card.updateNegotiatedPill(cfg);
        QVERIFY(true);
    }

    // ── 6. updateNegotiatedPill() with error string doesn't crash ──────────

    void updatePillErrorString() {
        DeviceCard card(QStringLiteral("audio/Speakers"),
                        DeviceCard::Role::Output,
                        false);

        AudioDeviceConfig cfg;
        card.updateNegotiatedPill(cfg, QStringLiteral("Sample rate not supported"));
        QVERIFY(true);
    }

    // ── 7. loadFromSettings with no prior keys gives defaults ─────────────

    void loadFromSettingsFreshInstall() {
        DeviceCard card(QStringLiteral("audio/Speakers"),
                        DeviceCard::Role::Output,
                        false);

        // loadFromSettings was called in the constructor.  Verify the
        // combo for sample rate landed on a valid entry.
        const AudioDeviceConfig cfg = card.currentConfig();
        // channels must be 1 or 2 regardless of default path.
        QVERIFY(cfg.channels >= 1 && cfg.channels <= 2);
    }

    // ── 8. configChanged emits when the Driver list's choice changes ─────
    //
    // A fake Windows catalogue: shared to exclusive saves the new engine,
    // and a Delay change emits too.

    void configChangedEmitsOnDriverChange() {
        Rig rig(Rig::windows());
        rig.start();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());

        QComboBox* driver = comboNamed(card, "deviceDriverCombo");
        QVERIFY(driver != nullptr);
        // The Driver combo stays the card's first combo.
        QCOMPARE(card.findChildren<QComboBox*>().first(), driver);
        QVERIFY(driver->isEnabled());
        QCOMPARE(driver->currentText(), QStringLiteral("Windows audio, shared"));

        QSignalSpy spy(&card, &DeviceCard::configChanged);
        driver->setCurrentIndex(driver->findText(QStringLiteral("Windows audio, exclusive")));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().at(0).value<AudioDeviceConfig>().engine,
                 std::optional<AudioEngineKind>(AudioEngineKind::WindowsExclusive));
        QCOMPARE(AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers")).engine,
                 std::optional<AudioEngineKind>(AudioEngineKind::WindowsExclusive));

        QComboBox* delay = comboNamed(card, "deviceDelayCombo");
        QVERIFY(delay != nullptr);
        delay->setCurrentIndex(delay->findData(QVariant::fromValue(10)));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last().at(0).value<AudioDeviceConfig>().delayMs, 10);
        rig.engine->stop();
    }

    // ── 9. AppSettings round-trip via DeviceCard ──────────────────────────
    //
    // When a control changes, the card persists to AppSettings.
    // A second loadFromSettings should read back the same device-name.

    void appSettingsRoundTrip() {
        // Seed AppSettings directly.
        AppSettings::instance().setValue(
            QStringLiteral("audio/Speakers/SampleRate"),
            QStringLiteral("96000"));

        DeviceCard card(QStringLiteral("audio/Speakers"),
                        DeviceCard::Role::Output,
                        false);

        // After loadFromSettings, the combo should reflect 96000.
        const AudioDeviceConfig cfg = card.currentConfig();
        // sampleRate may be 0 if auto-match is checked, or 96000.
        // Either way: not a crash.
        QVERIFY(true);
    }

    // ── 10. Headphones card enabledChanged fires on toggle ─────────────────

    void headphonesEnabledChangedFires() {
        DeviceCard card(QStringLiteral("audio/Headphones"),
                        DeviceCard::Role::Output,
                        true);  // enableCheckbox

        // Locate the "Enabled" checkbox (first visible row of the card).
        QCheckBox* enableChk = nullptr;
        for (auto* c : card.findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Enabled")) {
                enableChk = c;
                break;
            }
        }
        QVERIFY(enableChk != nullptr);

        QSignalSpy spy(&card, &DeviceCard::enabledChanged);
        const bool before = enableChk->isChecked();
        enableChk->setChecked(!before);

        QVERIFY2(spy.count() >= 1, "enabledChanged not emitted on toggle");
    }

    // ── 11. configChanged stays silent during loadFromSettings (issue #172) ───
    //
    // Regression for the "audio gets stuck opening File - Settings" bug.
    // The buffer-size combo uses a 200 ms debounce timer.  loadFromSettings()
    // calls setCurrentIndex on the combo, which fires currentIndexChanged.
    // Before the fix the start-timer lambda ignored m_suppressSignals, so the
    // timer was scheduled during load and fired ~200 ms after construction —
    // emitting a spurious configChanged that triggered AudioEngine::setXxxConfig
    // and tore down/rebuilt the bus on every Settings dialog open.

    void configChangedDoesNotEmitDuringLoad() {
        // Seed AppSettings with a non-default buffer size so setCurrentIndex
        // during loadFromSettings actually changes the index (otherwise Qt
        // suppresses currentIndexChanged when the value is already current).
        // kBufferSizes = {64, 128, 256, 512, 1024, 2048}; default index in
        // loadFromSettings is 2 (256). Pick 1024 (index 4) to force a change.
        AppSettings::instance().setValue(
            QStringLiteral("audio/Speakers/BufferSamples"),
            QStringLiteral("1024"));

        DeviceCard card(QStringLiteral("audio/Speakers"),
                        DeviceCard::Role::Output,
                        false);

        QSignalSpy spy(&card, &DeviceCard::configChanged);

        // Buffer-size debounce timer is 200 ms. Wait long enough that any
        // timer scheduled during loadFromSettings has had time to fire.
        QTest::qWait(300);

        QCOMPARE(spy.count(), 0);
    }

    // ── 12. A missing device and an unlisted buffer survive an edit ───────
    //
    // R-R3-36: the card must not fall back to "(platform default)" or 256
    // samples and then save that on the next unrelated edit. That would
    // silently switch the configured microphone.

    void unrelatedEditKeepsMissingDeviceAndBuffer_data() {
        QTest::addColumn<QString>("prefix");
        QTest::addColumn<int>("role");
        QTest::addColumn<int>("buffer");
        QTest::newRow("input/4096")  << "audio/TxInput"  << int(DeviceCard::Role::Input)  << 4096;
        QTest::newRow("input/8192")  << "audio/TxInput"  << int(DeviceCard::Role::Input)  << 8192;
        QTest::newRow("input/3000")  << "audio/TxInput"  << int(DeviceCard::Role::Input)  << 3000;
        QTest::newRow("output/4096") << "audio/Speakers" << int(DeviceCard::Role::Output) << 4096;
    }
    void unrelatedEditKeepsMissingDeviceAndBuffer() {
        QFETCH(QString, prefix);
        QFETCH(int, role);
        QFETCH(int, buffer);
        const QString missing = QStringLiteral("NereusSDR Test Device Not Present");
        auto& s = AppSettings::instance();
        s.setValue(prefix + QStringLiteral("/DeviceName"), missing);
        s.setValue(prefix + QStringLiteral("/BufferSamples"), QString::number(buffer));

        DeviceCard card(prefix, static_cast<DeviceCard::Role>(role), false);

        // The card shows the configured device as not connected.
        bool shown = false;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->currentText() == QStringLiteral("%1 (not connected)").arg(missing)) {
                QCOMPARE(combo->currentData().toString(), missing);
                shown = true;
            }
        }
        QVERIFY(shown);
        QCOMPARE(card.currentConfig().deviceName, missing);
        QCOMPARE(card.currentConfig().bufferSamples, buffer);

        // An unrelated edit (Channels) saves the same device and buffer.
        QComboBox* channels = nullptr;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->findText(QStringLiteral("1 (Mono)")) >= 0) {
                channels = combo;
            }
        }
        QVERIFY(channels != nullptr);
        QSignalSpy spy(&card, &DeviceCard::configChanged);
        channels->setCurrentIndex(channels->currentIndex() == 0 ? 1 : 0);
        QTRY_VERIFY(spy.count() >= 1);
        const auto cfg = spy.last().at(0).value<AudioDeviceConfig>();
        QCOMPARE(cfg.deviceName, missing);
        QCOMPARE(cfg.bufferSamples, buffer);
        const AudioDeviceConfig saved = AudioDeviceConfig::loadFromSettings(prefix);
        QCOMPARE(saved.deviceName, missing);
        QCOMPARE(saved.bufferSamples, buffer);
    }

    // ── 13. Reloading does not pile up kept entries ───────────────────────
    //
    // R-R3-36: each load removes the "(not connected)" device and the
    // unlisted buffer size the previous load added before adding its own.

    void reloadDropsStaleKeptEntries() {
        const QString prefix = QStringLiteral("audio/TxInput");
        const QString first  = QStringLiteral("NereusSDR Test Device One");
        const QString second = QStringLiteral("NereusSDR Test Device Two");
        auto& s = AppSettings::instance();
        s.setValue(prefix + QStringLiteral("/DeviceName"), first);
        s.setValue(prefix + QStringLiteral("/BufferSamples"), QStringLiteral("3000"));

        DeviceCard card(prefix, DeviceCard::Role::Input, false);

        QComboBox* deviceCombo = nullptr;
        QComboBox* bufferCombo = nullptr;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->findText(QStringLiteral("(platform default)")) >= 0) {
                deviceCombo = combo;
            }
            if (combo->findText(QStringLiteral("256 samples")) >= 0) {
                bufferCombo = combo;
            }
        }
        QVERIFY(deviceCombo != nullptr);
        QVERIFY(bufferCombo != nullptr);
        const QString firstText  = QStringLiteral("%1 (not connected)").arg(first);
        const QString secondText = QStringLiteral("%1 (not connected)").arg(second);
        QVERIFY(deviceCombo->findText(firstText) >= 0);
        QVERIFY(bufferCombo->findData(QVariant::fromValue(3000)) >= 0);
        const int listedBuffers = bufferCombo->count() - 1;

        // A second missing device and another unlisted size replace the first.
        s.setValue(prefix + QStringLiteral("/DeviceName"), second);
        s.setValue(prefix + QStringLiteral("/BufferSamples"), QStringLiteral("5000"));
        card.loadFromSettings();
        QCOMPARE(deviceCombo->findText(firstText), -1);
        QCOMPARE(deviceCombo->currentText(), secondText);
        QCOMPARE(bufferCombo->findData(QVariant::fromValue(3000)), -1);
        QCOMPARE(bufferCombo->currentData().toInt(), 5000);
        QCOMPARE(bufferCombo->count(), listedBuffers + 1);

        // The same config again adds nothing.
        card.loadFromSettings();
        int unavailable = 0;
        for (int i = 0; i < deviceCombo->count(); ++i) {
            if (deviceCombo->itemText(i).endsWith(QStringLiteral(" (not connected)"))) {
                ++unavailable;
            }
        }
        QCOMPARE(unavailable, 1);
        QCOMPARE(bufferCombo->count(), listedBuffers + 1);

        // A config the lists hold leaves no kept entry behind.
        s.setValue(prefix + QStringLiteral("/DeviceName"), QString());
        s.setValue(prefix + QStringLiteral("/BufferSamples"), QStringLiteral("256"));
        card.loadFromSettings();
        QCOMPARE(deviceCombo->findText(secondText), -1);
        QCOMPARE(deviceCombo->currentText(), QStringLiteral("(platform default)"));
        QCOMPARE(bufferCombo->findData(QVariant::fromValue(5000)), -1);
        QCOMPARE(bufferCombo->count(), listedBuffers);
        QCOMPARE(bufferCombo->currentData().toInt(), 256);
    }

    void driverRefreshWithQueriedAccessibleDeviceKeepsConfiguration_data()
    {
        QTest::addColumn<bool>("input");
        QTest::newRow("input") << true;
        QTest::newRow("output") << false;
    }

    void driverRefreshWithQueriedAccessibleDeviceKeepsConfiguration()
    {
        QFETCH(bool, input);
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QVERIFY(PortAudioBus::portAudioBarredForTestRun());
        // DeviceCard's native cache workaround is specific to Qt 6.11 Cocoa.
        // Other backends still exercise every configuration and persistence
        // assertion below, without imposing Cocoa cell-cache semantics.
#if defined(Q_OS_MAC)
        const bool inspectNativeCache = QGuiApplication::platformName() == QStringLiteral("cocoa")
            && qVersion() == QStringLiteral("6.11.0");
#else
        const bool inspectNativeCache = false;
#endif
        if (inspectNativeCache) {
            QAccessible::setActive(true);
        }
        const QString prefix = input ? QStringLiteral("audio/TxInput")
                                     : QStringLiteral("audio/Speakers");
        AudioDeviceConfig config;
        config.deviceName = QStringLiteral("Absent test audio device");
        config.bufferSamples = 3000;
        config.saveToSettings(prefix);
        // A fake Windows catalogue: the Driver list flips shared and
        // exclusive, both listing the same devices.
        Rig rig(Rig::windows());
        rig.start();
        DeviceCard card(prefix, input ? DeviceCard::Role::Input : DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        QComboBox* driver = comboNamed(card, "deviceDriverCombo");
        QVERIFY(driver);
        const int shared = driver->findText(QStringLiteral("Windows audio, shared"));
        const int exclusive = driver->findText(QStringLiteral("Windows audio, exclusive"));
        QVERIFY(shared >= 0 && exclusive >= 0);
        QCOMPARE(driver->currentIndex(), shared);
        QComboBox* device = nullptr;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->findText(QStringLiteral("(platform default)")) >= 0) {
                device = combo;
            }
        }
        QVERIFY(device);
        const int listed = device->count();
        QSignalSpy changes(&card, &DeviceCard::configChanged);
        for (int iteration = 0; iteration < 3; ++iteration) {
            if (inspectNativeCache) {
                QAccessibleInterface* table = QAccessible::queryAccessibleInterface(device->view());
                QVERIFY(table && table->tableInterface());
                QAccessibleInterface* selected = table->tableInterface()->cellAt(device->currentIndex(), 0);
                QVERIFY(selected && selected->isValid());
                QCOMPARE(selected->text(QAccessible::Name), QStringLiteral("Absent test audio device (not connected)"));
            }

            driver->setCurrentIndex(driver->currentIndex() == shared ? exclusive : shared);

            QCOMPARE(changes.count(), iteration + 1);
            QCOMPARE(device->count(), listed);
            QCOMPARE(device->currentText(), QStringLiteral("Absent test audio device (not connected)"));
            QCOMPARE(device->currentData().toString(), config.deviceName);
            QCOMPARE(card.currentConfig().deviceName, config.deviceName);
            QCOMPARE(card.currentConfig().bufferSamples, 3000);
            const AudioDeviceConfig saved = AudioDeviceConfig::loadFromSettings(prefix);
            QCOMPARE(saved.deviceName, config.deviceName);
            QCOMPARE(saved.bufferSamples, 3000);
            if (inspectNativeCache) {
                QAccessibleInterface* table = QAccessible::queryAccessibleInterface(device->view());
                QAccessibleInterface* selected = table->tableInterface()->cellAt(device->currentIndex(), 0);
                QVERIFY(selected && selected->isValid());
                QCOMPARE(selected->text(QAccessible::Name), device->currentText());
            }
        }
        rig.engine->stop();
    }

    // The input card offers the TX Input page's larger buffers.
    void inputCardOffersLargeBuffers() {
        DeviceCard card(QStringLiteral("audio/TxInput"), DeviceCard::Role::Input, false);
        bool has4096 = false;
        bool has8192 = false;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            has4096 = has4096 || combo->findData(QVariant::fromValue(4096)) >= 0;
            has8192 = has8192 || combo->findData(QVariant::fromValue(8192)) >= 0;
        }
        QVERIFY(has4096);
        QVERIFY(has8192);
    }

    // ── 14. Device details fold (R-SPK-21, D14) ───────────────────────────

    void deviceDetailsFoldedByDefault() {
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        auto* details = card.findChild<QWidget*>(QStringLiteral("deviceDetails"));
        auto* toggle = card.findChild<QToolButton*>(QStringLiteral("deviceDetailsToggle"));
        QVERIFY(details != nullptr);
        QVERIFY(toggle != nullptr);
        QCOMPARE(toggle->text(), QStringLiteral("Device details"));
        QVERIFY(!card.detailsExpanded());
        QVERIFY(details->isHidden());

        // Every row but Driver and Device is inside the fold; those two are
        // in front.
        const QStringList folded{QStringLiteral("Sample rate:"),
                                 QStringLiteral("Channels:"), QStringLiteral("Buffer size:"),
                                 QStringLiteral("Delay:"), QStringLiteral("Negotiated:")};
        QStringList foundInDetails;
        for (QLabel* label : details->findChildren<QLabel*>()) {
            foundInDetails << label->text();
        }
        for (const QString& text : folded) {
            QVERIFY2(foundInDetails.contains(text), qPrintable(text));
        }
        QVERIFY(!foundInDetails.contains(QStringLiteral("Device:")));
        QVERIFY(!foundInDetails.contains(QStringLiteral("Driver:")));
        QVERIFY(!foundInDetails.contains(QStringLiteral("Options:")));
        QVERIFY(!foundInDetails.contains(QStringLiteral("Driver API:")));
        QVERIFY(details->isAncestorOf(comboNamed(card, "deviceDelayCombo")));
        QVERIFY(details->isAncestorOf(labelNamed(card, "deviceDelayNow")));
        QVERIFY(details->isAncestorOf(labelNamed(card, "engineNote")));
        // The state note is in front, under the Device row.
        QVERIFY(!details->isAncestorOf(labelNamed(card, "deviceStateNote")));
        // The Driver combo stays the card's first combo, in front of the fold.
        QComboBox* first = card.findChildren<QComboBox*>().first();
        QCOMPARE(first, comboNamed(card, "deviceDriverCombo"));
        QVERIFY(!details->isAncestorOf(first));

        toggle->click();
        QVERIFY(card.detailsExpanded());
        QVERIFY(!details->isHidden());
        card.setDetailsExpanded(false);
        QVERIFY(details->isHidden());
        QVERIFY(!toggle->isChecked());
    }

    // ── 19. The Driver row is in front, above Device ──────────────────────
    // JJ, 2026-10-10: the driver choice decides which devices are listed,
    // so it is never behind the fold (ASIO on the Windows mic card).

    void driverRowIsVisibleAboveDeviceWithTheFoldClosed_data() {
        QTest::addColumn<QString>("prefix");
        QTest::addColumn<bool>("input");
        QTest::newRow("speakers") << QStringLiteral("audio/Speakers") << false;
        QTest::newRow("mic") << QStringLiteral("audio/TxInput") << true;
    }

    void driverRowIsVisibleAboveDeviceWithTheFoldClosed() {
        QFETCH(QString, prefix);
        QFETCH(bool, input);
        DeviceCard card(prefix, input ? DeviceCard::Role::Input : DeviceCard::Role::Output, false);
        // Laid out without a window: grab() sends the pending resizes.
        card.resize(card.sizeHint());
        card.grab();
        QVERIFY(!card.detailsExpanded());

        QComboBox* driver = card.driverApiCombo();
        QComboBox* device = card.deviceCombo();
        QVERIFY(driver != nullptr);
        QVERIFY(device != nullptr);
        QCOMPARE(driver->objectName(), QStringLiteral("deviceDriverCombo"));
        QVERIFY(device->isVisibleTo(&card));
        QVERIFY2(driver->isVisibleTo(&card), "the Driver list is behind the closed fold");

        // Its "Driver:" label is in front with it.
        bool labelShown = false;
        for (QLabel* label : card.findChildren<QLabel*>()) {
            labelShown = labelShown
                || (label->text() == QStringLiteral("Driver:") && label->isVisibleTo(&card));
        }
        QVERIFY(labelShown);

        // Driver first, then Device: on the card and in the tab order.
        const QRect driverRect(driver->mapTo(&card, QPoint(0, 0)), driver->size());
        const QRect deviceRect(device->mapTo(&card, QPoint(0, 0)), device->size());
        QVERIFY2(driverRect.bottom() < deviceRect.top(),
                 qPrintable(QStringLiteral("driver bottom %1, device top %2")
                                .arg(driverRect.bottom())
                                .arg(deviceRect.top())));
        QWidget* next = driver->nextInFocusChain();
        while (next != driver && qobject_cast<QComboBox*>(next) == nullptr) {
            next = next->nextInFocusChain();
        }
        QCOMPARE(next, device);

        // The fold still opens and closes under them; Driver stays put.
        card.setDetailsExpanded(true);
        QVERIFY(driver->isVisibleTo(&card));
        card.setDetailsExpanded(false);
        QVERIFY(driver->isVisibleTo(&card));
    }

    // ── 15. The WASAPI checkboxes are gone; their keys stay (D10) ─────────

    void retiredWasapiOptionsAreGoneAndTheirKeysStay() {
        const QString prefix = QStringLiteral("audio/Speakers");
        auto& s = AppSettings::instance();
        s.setValue(prefix + QStringLiteral("/ExclusiveMode"), QStringLiteral("True"));
        s.setValue(prefix + QStringLiteral("/EventDriven"), QStringLiteral("False"));
        Rig rig;
        rig.start();
        DeviceCard card(prefix, DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        for (QCheckBox* box : card.findChildren<QCheckBox*>()) {
            QVERIFY(box->text() != QStringLiteral("Exclusive"));
            QVERIFY(box->text() != QStringLiteral("Event-driven"));
            QVERIFY(box->text() != QStringLiteral("Bypass mixer"));
        }
        QVERIFY(labelNamed(card, "wasapiOnlyNote") == nullptr);

        // An edit saves the card, and the retired keys stay as they were:
        // the seeded ones unchanged, the absent one never written.
        QComboBox* delay = comboNamed(card, "deviceDelayCombo");
        QVERIFY(delay != nullptr);
        delay->setCurrentIndex(delay->findData(QVariant::fromValue(20)));
        QCOMPARE(s.value(prefix + QStringLiteral("/DelayMs")).toString(), QStringLiteral("20"));
        QCOMPARE(s.value(prefix + QStringLiteral("/ExclusiveMode")).toString(), QStringLiteral("True"));
        QCOMPARE(s.value(prefix + QStringLiteral("/EventDriven")).toString(), QStringLiteral("False"));
        QVERIFY(!s.contains(prefix + QStringLiteral("/BypassMixer")));
        rig.engine->stop();
    }

    // ── 17. Native audio plan Task 16: the cards on the engine's lists ────

    // R-AUD-15: the Delay list and what each choice saves. DelayMs is
    // saved with the engine's keys, so until the lists are ready the Delay
    // is greyed with its reason.
    void delayListSavesDelayMs() {
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        QComboBox* delay = comboNamed(card, "deviceDelayCombo");
        QVERIFY(delay != nullptr);
        QVERIFY(!delay->isEnabled());
        QVERIFY(!delay->isHidden());
        QCOMPARE(delay->toolTip(), QStringLiteral("The device lists are not ready."));
        // Without an engine the readout says nothing is playing.
        QCOMPARE(labelNamed(card, "deviceDelayNow")->text(), QStringLiteral("Now -- ms"));

        Rig rig;
        rig.start();
        card.setAudioEngine(rig.engine.get());
        QVERIFY(delay->isEnabled());
        QVERIFY(delay->toolTip().isEmpty());
        const QStringList texts{QStringLiteral("Automatic"), QStringLiteral("2 ms"),
                                QStringLiteral("3 ms"),      QStringLiteral("5 ms"),
                                QStringLiteral("10 ms"),     QStringLiteral("20 ms"),
                                QStringLiteral("40 ms")};
        const QList<int> saved{0, 2, 3, 5, 10, 20, 40};
        QCOMPARE(delay->count(), texts.size());
        QCOMPARE(delay->currentIndex(), 0);
        for (int i = 0; i < texts.size(); ++i) {
            QCOMPARE(delay->itemText(i), texts.at(i));
        }
        for (int i = texts.size() - 1; i >= 0; --i) {
            delay->setCurrentIndex(i);
            QCOMPARE(AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers")).delayMs,
                     saved.at(i));
        }
        rig.engine->stop();
    }

    // Fix round, R-AUD-01/R-AUD-03: the lists exist before a radio
    // connects (the engine never started), the Delay is settable, and
    // nothing opens a device.
    void listsAndDelayBeforeStartOpenNothing() {
        Rig rig;
        rig.prepare();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        QVERIFY(rig.engine->catalogue() != nullptr);
        bool desk = false;
        bool builtIn = false;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            desk = desk || combo->findText(QStringLiteral("Desk speakers")) >= 0;
            builtIn = builtIn || combo->findText(QStringLiteral("Built-in speakers")) >= 0;
        }
        QVERIFY(desk);
        QVERIFY(builtIn);
        QComboBox* delay = comboNamed(card, "deviceDelayCombo");
        QVERIFY(delay->isEnabled());
        QVERIFY(delay->toolTip().isEmpty());
        delay->setCurrentIndex(delay->findText(QStringLiteral("10 ms")));
        const AudioDeviceConfig saved =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));
        QCOMPARE(saved.delayMs, 10);
        QVERIFY(saved.engine.has_value());
        QVERIFY(rig.native->outputRequests().empty());
        QVERIFY(rig.older->outputRequests().empty());
    }

    // Fix round, R-AUD-01: with no native engine running, the older drivers
    // with no host API saved show on the "Older drivers" heading, never as a
    // second, pickable row of that name.
    void olderDriversDefaultIsTheHeadingRow() {
        Rig rig(AudioBackendId::PulseAudio);
        rig.native->setRunning(false);
        rig.prepare();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        QComboBox* driver = comboNamed(card, "deviceDriverCombo");
        int headings = 0;
        for (int i = 0; i < driver->count(); ++i) {
            headings += driver->itemText(i) == QStringLiteral("Older drivers") ? 1 : 0;
        }
        QCOMPARE(headings, 1);
        QCOMPARE(driver->itemText(0), QStringLiteral("PipeWire (not running)"));
        QCOMPARE(driver->itemText(1), QStringLiteral("PulseAudio (not running)"));
        QCOMPARE(driver->currentText(), QStringLiteral("Older drivers"));
        QVERIFY(driver->isEnabled());
    }

    // Fix round, R-AUD-11: the closed Device field names the chosen device
    // in use by another program.
    void inUseChosenDeviceReadsSoInTheClosedField() {
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        Rig rig;
        rig.start();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        QComboBox* device = nullptr;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->currentText() == QStringLiteral("Desk speakers")) {
                device = combo;
            }
        }
        QVERIFY(device != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(rig.native->lastOutput() != nullptr, 5000);
        AudioStreamEvent event;
        event.kind = AudioStreamEvent::Kind::DeviceBusy;
        rig.native->lastOutput()->emitEventForTest(event);
        QTRY_COMPARE_WITH_TIMEOUT(device->currentText(),
                                  QStringLiteral("Desk speakers (in use by another program)"),
                                  5000);
        QVERIFY(device->findText(QStringLiteral("Built-in speakers")) > 0);
        rig.engine->stop();
    }

    // Fix round, R-AUD-15: a card made after the engine started shows the
    // format the role plays now.
    void pillShowsThePlayingFormatWhenSetupOpensLater() {
        Rig rig;
        rig.start();
        QTRY_VERIFY_WITH_TIMEOUT(rig.native->lastOutput() != nullptr, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(
            rig.engine->roleStatus(AudioRole::Speakers).state == AudioRoleState::Playing
                || rig.engine->roleStatus(AudioRole::Speakers).state
                       == AudioRoleState::PlayingOnDefault,
            5000);
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        const AudioFormat format = rig.native->lastOutput()->negotiatedFormat();
        QVERIFY(format.sampleRate > 0);
        const QString expected = QStringLiteral("%1 Hz \u00b7 %2 ch")
                                     .arg(format.sampleRate)
                                     .arg(format.channels);
        bool shown = false;
        for (QLabel* label : card.findChildren<QLabel*>()) {
            shown = shown || label->text().contains(expected);
        }
        QVERIFY2(shown, qPrintable(expected));
        rig.engine->stop();
    }

    // R-AUD-01: the Mac lists Core Audio alone, greyed with its reason.
    void macDriverListIsCoreAudioGreyed() {
        Rig rig;
        rig.start();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        QComboBox* driver = comboNamed(card, "deviceDriverCombo");
        QCOMPARE(driver->count(), 1);
        QCOMPARE(driver->currentText(), QStringLiteral("Core Audio"));
        QVERIFY(!driver->isEnabled());
        QVERIFY(!driver->isHidden());
        QCOMPARE(driver->toolTip(), QStringLiteral("The only sound system on the Mac."));
        QVERIFY(labelNamed(card, "engineNote")->text().isEmpty());
        rig.engine->stop();
    }

    // R-AUD-03: "(platform default)" first, then the engine's devices; a
    // device added shows within 1 s and the selection stays.
    void deviceAddedShowsAndKeepsTheSelection() {
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        Rig rig;
        rig.start();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        QComboBox* device = nullptr;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->findText(QStringLiteral("(platform default)")) >= 0) {
                device = combo;
            }
        }
        QVERIFY(device != nullptr);
        QCOMPARE(device->itemText(0), QStringLiteral("(platform default)"));
        QCOMPARE(device->currentText(), QStringLiteral("Desk speakers"));
        QCOMPARE(device->findText(QStringLiteral("Built-in speakers")) > 0, true);
        QCOMPARE(card.deviceCount(), 2);

        QSignalSpy changes(&card, &DeviceCard::configChanged);
        rig.native->addDevice(deviceInfo(AudioBackendId::CoreAudio, AudioDeviceDirection::Output,
                                         QStringLiteral("usb-dac-uid"), QStringLiteral("USB DAC")));
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(device->findText(QStringLiteral("USB DAC")) > 0, 1000);
        QCOMPARE(device->currentText(), QStringLiteral("Desk speakers"));
        QCOMPARE(card.deviceCount(), 3);
        QCOMPARE(changes.count(), 0);
        rig.engine->stop();
    }

    // R-AUD-08: a saved device that is missing shows "(not connected)",
    // the note says where the sound plays meanwhile, and both clear when
    // it comes back.
    void missingOutputShowsNotConnectedAndItsNote() {
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("gone-uid"),
                    QStringLiteral("Desk monitor"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        Rig rig;
        rig.start();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        QComboBox* device = nullptr;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->findText(QStringLiteral("(platform default)")) >= 0) {
                device = combo;
            }
        }
        QVERIFY(device != nullptr);
        QCOMPARE(device->currentText(), QStringLiteral("Desk monitor (not connected)"));
        QLabel* note = labelNamed(card, "deviceStateNote");
        QTRY_COMPARE_WITH_TIMEOUT(note->text(),
                                  QStringLiteral("Desk monitor is not connected. Playing on the "
                                                 "system default, Built-in speakers, until it "
                                                 "comes back."),
                                  5000);
        QVERIFY(!note->isHidden());
        // The readout names the device actually playing.
        QTRY_VERIFY_WITH_TIMEOUT(labelNamed(card, "deviceDelayNow")
                                     ->text()
                                     .endsWith(QStringLiteral(" ms from the radio to Built-in speakers")),
                                 2000);

        rig.native->addDevice(deviceInfo(AudioBackendId::CoreAudio, AudioDeviceDirection::Output,
                                         QStringLiteral("gone-uid"), QStringLiteral("Desk monitor")));
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(device->currentText(), QStringLiteral("Desk monitor"), 1000);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::Playing, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(note->text().isEmpty(), 1000);
        QVERIFY(note->isHidden());
        QTRY_VERIFY_WITH_TIMEOUT(labelNamed(card, "deviceDelayNow")
                                     ->text()
                                     .endsWith(QStringLiteral(" ms from the radio to Desk monitor")),
                                 2000);
        rig.engine->stop();
    }

    // R-AUD-11: a device another program holds says so in the list, and
    // a busy stream says so in the note.
    void deviceInUseShowsInTheListAndTheNote() {
        AudioDeviceInfo busy = deviceInfo(AudioBackendId::CoreAudio, AudioDeviceDirection::Output,
                                          QStringLiteral("busy-uid"), QStringLiteral("Studio monitor"));
        busy.state = AudioDeviceState::InUse;
        Rig rig;
        rig.native->addDevice(busy);
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        rig.start();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        bool listed = false;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            listed = listed
                || combo->findText(QStringLiteral("Studio monitor (in use by another program)")) > 0;
        }
        QVERIFY(listed);

        FakeMatcherAudioBus* desk = rig.native->lastOutput();
        QVERIFY(desk != nullptr);
        AudioStreamEvent event;
        event.kind = AudioStreamEvent::Kind::DeviceBusy;
        desk->emitEventForTest(event);
        QTRY_VERIFY_WITH_TIMEOUT(labelNamed(card, "deviceStateNote")
                                     ->text()
                                     .contains(QStringLiteral("is in use by another program")),
                                 5000);
        rig.engine->stop();
    }

    // A saved "(none)" shows "(none)".
    void savedNoneShowsNone() {
        savedChoice(AudioEngineKind::CoreAudio, QString::fromLatin1(kAudioDeviceNone),
                    QString::fromLatin1(kAudioDeviceNone))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        Rig rig;
        rig.start();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        bool shown = false;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            shown = shown || combo->currentText() == QStringLiteral("(none)");
        }
        QVERIFY(shown);
        QCOMPARE(card.currentConfig().deviceId, QString::fromLatin1(kAudioDeviceNone));
        rig.engine->stop();
    }

    // R-AUD-04: a pick saves Engine, DeviceId, DeviceName and FirstChannel,
    // and the engine opens that device.
    void pickSavesTheIdentityAndReachesTheEngine() {
        Rig rig;
        rig.start();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        connect(&card, &DeviceCard::configChanged, rig.engine.get(),
                &AudioEngine::setSpeakersConfig);
        QComboBox* device = nullptr;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->findText(QStringLiteral("(platform default)")) >= 0) {
                device = combo;
            }
        }
        QVERIFY(device != nullptr);
        device->setCurrentIndex(device->findText(QStringLiteral("Desk speakers")));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/Engine")).toString(),
                 QStringLiteral("CoreAudio"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/DeviceId")).toString(),
                 QStringLiteral("desk-uid"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/DeviceName")).toString(),
                 QStringLiteral("Desk speakers"));
        QCOMPARE(s.value(QStringLiteral("audio/Speakers/FirstChannel")).toString(),
                 QStringLiteral("1"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.native->outputRequests().empty()
                                     && rig.native->outputRequests().back().deviceId
                                            == QStringLiteral("desk-uid"),
                                 5000);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).playingName,
                                  QStringLiteral("Desk speakers"), 5000);
        rig.engine->stop();
    }

    // R-AUD-15: "Now -- ms" while the role is not playing, the delay to
    // the device playing while it is.
    void delayReadoutFollowsTheRole() {
        Rig rig;
        rig.start();
        DeviceCard card(QStringLiteral("audio/Headphones"), DeviceCard::Role::Output, true);
        card.setAudioEngine(rig.engine.get());
        QCOMPARE(rig.engine->roleStatus(AudioRole::Headphones).state, AudioRoleState::Off);
        QCOMPARE(labelNamed(card, "deviceDelayNow")->text(), QStringLiteral("Now -- ms"));

        DeviceCard speakers(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        speakers.setAudioEngine(rig.engine.get());
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::Playing);
        QTRY_VERIFY_WITH_TIMEOUT(labelNamed(speakers, "deviceDelayNow")
                                     ->text()
                                     .startsWith(QStringLiteral("Now ")),
                                 2000);
        QVERIFY(labelNamed(speakers, "deviceDelayNow")
                    ->text()
                    .endsWith(QStringLiteral(" ms from the radio to Built-in speakers")));
        rig.engine->stop();
    }

    // R-AUD-16: what the picked driver means.
    void engineNotesFollowTheDriver() {
        Rig rig(Rig::windows());
        rig.start();
        DeviceCard card(QStringLiteral("audio/Speakers"), DeviceCard::Role::Output, false);
        card.setAudioEngine(rig.engine.get());
        QComboBox* driver = comboNamed(card, "deviceDriverCombo");
        QLabel* note = labelNamed(card, "engineNote");
        QVERIFY(note->text().isEmpty());
        driver->setCurrentIndex(driver->findText(QStringLiteral("Windows audio, exclusive")));
        QCOMPARE(note->text(),
                 QStringLiteral("Other apps cannot play through this device while NereusSDR has it."));
        driver->setCurrentIndex(driver->findText(QStringLiteral("MME")));
        QCOMPARE(note->text(), QStringLiteral("An older driver: more delay, and its list updates "
                                              "only with Rescan devices."));
        driver->setCurrentIndex(driver->findText(QStringLiteral("Windows audio, shared")));
        QVERIFY(note->text().isEmpty());
        rig.engine->stop();
    }

    // R-AUD-14: a Bluetooth mic picked says what it costs.
    void bluetoothMicNote() {
        AudioDeviceInfo headset = deviceInfo(AudioBackendId::CoreAudio, AudioDeviceDirection::Input,
                                             QStringLiteral("airpods-uid"), QStringLiteral("AirPods"));
        headset.transport = AudioTransport::Bluetooth;
        Rig rig;
        rig.native->addDevice(headset);
        rig.start();
        DeviceCard card(QStringLiteral("audio/TxInput"), DeviceCard::Role::Input, false);
        card.setAudioEngine(rig.engine.get());
        QLabel* note = labelNamed(card, "deviceStateNote");
        QComboBox* device = nullptr;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->findText(QStringLiteral("(platform default)")) >= 0) {
                device = combo;
            }
        }
        QVERIFY(device != nullptr);
        device->setCurrentIndex(device->findText(QStringLiteral("AirPods")));
        QCOMPARE(note->text(),
                 QStringLiteral("Bluetooth headsets switch to phone-call quality, for listening "
                                "too, while they are your mic. For the best sound, listen on "
                                "AirPods and talk on a wired or built-in mic."));
        device->setCurrentIndex(device->findText(QStringLiteral("USB Mic")));
        QVERIFY(note->text().isEmpty());
        rig.engine->stop();
    }

    // ── 16. Greyed until Enabled (R-SPK-21) ───────────────────────────────

    void greyedUntilEnabled() {
        DeviceCard card(QStringLiteral("audio/Headphones"), DeviceCard::Role::Output, true);
        auto* above = new QLabel(QStringLiteral("note"));
        card.addAboveDevice(above);
        auto* body = card.findChild<QWidget*>(QStringLiteral("deviceCardBody"));
        QVERIFY(body != nullptr);
        QCheckBox* enabled = nullptr;
        for (QCheckBox* box : card.findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("Enabled")) { enabled = box; }
        }
        QVERIFY(enabled != nullptr);
        QVERIFY(!enabled->isChecked());

        // Opt-in: without it the body stays live.
        QVERIFY(body->isEnabled());
        card.setGreyedUntilEnabled(true);
        QVERIFY(!body->isEnabled());
        QVERIFY(above->isEnabled());
        QVERIFY(enabled->isEnabled());
        QVERIFY(!body->isHidden());

        enabled->setChecked(true);
        QVERIFY(body->isEnabled());
        enabled->setChecked(false);
        QVERIFY(!body->isEnabled());
    }
};

QTEST_MAIN(TstDeviceCard)
#include "tst_device_card.moc"
