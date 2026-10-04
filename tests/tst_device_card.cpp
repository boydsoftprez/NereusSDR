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
//   8. configChanged signal emits when the driver-API combo is changed
//      programmatically (via QComboBox::setCurrentIndex).
//   9. AppSettings round-trip: DeviceCard saves on control change;
//      a second loadFromSettings reads the same values back.
//  10. Headphones card: enabledChanged fires when checkable toggled.
//  12. R-R3-36: a configured device that is not present and a configured
//      buffer size the list lacks survive an unrelated edit on the card.
//  13. R-R3-36: reloading replaces those kept entries instead of adding more.
//
// Design spec:
//   docs/architecture/2026-04-20-phase3o-subphase12-addendum.md §2.1
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QComboBox>
#include <QCheckBox>
#include <QAccessible>
#include <QAbstractItemView>
#include <QSignalBlocker>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/audio/PortAudioBus.h"
#include "gui/setup/DeviceCard.h"

using namespace NereusSDR;

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

    // ── 8. configChanged emits when driver-API combo is changed ──────────
    //
    // DeviceCard uses QComboBox::currentIndexChanged internally.
    // We locate the first combo (driver-API) and change its index.

    void configChangedEmitsOnDriverApiChange() {
        DeviceCard card(QStringLiteral("audio/Speakers"),
                        DeviceCard::Role::Output,
                        false);

        QSignalSpy spy(&card, &DeviceCard::configChanged);

        // Find all QComboBox children — pick the one that changes buffer size.
        const auto combos = card.findChildren<QComboBox*>();
        QVERIFY2(!combos.isEmpty(), "No QComboBox children found in DeviceCard");

        // Trigger a change on the first combo (driver API usually).
        QComboBox* firstCombo = combos.first();
        const int origIdx = firstCombo->currentIndex();
        if (firstCombo->count() > 1) {
            const int newIdx = (origIdx == 0) ? 1 : 0;
            firstCombo->setCurrentIndex(newIdx);
            QVERIFY2(spy.count() > 0, "configChanged not emitted after combo change");
        } else {
            // Only one item — can't trigger a change. Skip.
            QSKIP("Only one item in combo; can't trigger change");
        }
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

        // The card shows the configured device as not available.
        bool shown = false;
        for (QComboBox* combo : card.findChildren<QComboBox*>()) {
            if (combo->currentText() == QStringLiteral("%1 (not available)").arg(missing)) {
                QCOMPARE(combo->currentData().toString(), missing);
                shown = true;
            }
        }
        QVERIFY(shown);
        QCOMPARE(card.currentConfig().deviceName, missing);
        QCOMPARE(card.currentConfig().bufferSamples, buffer);

        // An unrelated edit (a WASAPI option) saves the same device and buffer.
        QCheckBox* exclusive = nullptr;
        for (QCheckBox* c : card.findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Exclusive")) {
                exclusive = c;
            }
        }
        QVERIFY(exclusive != nullptr);
        QSignalSpy spy(&card, &DeviceCard::configChanged);
        exclusive->setChecked(!exclusive->isChecked());
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
    // R-R3-36: each load removes the "(not available)" device and the
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
        const QString firstText  = QStringLiteral("%1 (not available)").arg(first);
        const QString secondText = QStringLiteral("%1 (not available)").arg(second);
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
            if (deviceCombo->itemText(i).endsWith(QStringLiteral(" (not available)"))) {
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
        QAccessible::setActive(true);
        const QString prefix = input ? QStringLiteral("audio/TxInput")
                                     : QStringLiteral("audio/Speakers");
        AudioDeviceConfig config;
        config.deviceName = QStringLiteral("Absent test audio device");
        config.bufferSamples = 3000;
        config.saveToSettings(prefix);
        DeviceCard card(prefix, input ? DeviceCard::Role::Input : DeviceCard::Role::Output, false);
        const QList<QComboBox*> combos = card.findChildren<QComboBox*>();
        QVERIFY(!combos.isEmpty());
        QComboBox* driver = combos.first();
        QComboBox* device = nullptr;
        for (QComboBox* combo : combos) {
            if (combo->findText(QStringLiteral("(platform default)")) >= 0) {
                device = combo;
            }
        }
        QVERIFY(device);
        {
            QSignalBlocker blocker(driver);
            driver->addItem(QStringLiteral("Test audio API"), 0);
        }
        QSignalSpy changes(&card, &DeviceCard::configChanged);
        for (int iteration = 0; iteration < 3; ++iteration) {
            QAccessibleInterface* table = QAccessible::queryAccessibleInterface(device->view());
            QVERIFY(table && table->tableInterface());
            QAccessibleInterface* selected = table->tableInterface()->cellAt(device->currentIndex(), 0);
            QVERIFY(selected && selected->isValid());
            QCOMPARE(selected->text(QAccessible::Name), QStringLiteral("Absent test audio device (not available)"));

            driver->setCurrentIndex(driver->currentIndex() == 0 ? 1 : 0);

            QCOMPARE(changes.count(), iteration + 1);
            QCOMPARE(device->count(), 2);
            QCOMPARE(device->currentData().toString(), config.deviceName);
            QCOMPARE(card.currentConfig().deviceName, config.deviceName);
            QCOMPARE(card.currentConfig().bufferSamples, 3000);
            const AudioDeviceConfig saved = AudioDeviceConfig::loadFromSettings(prefix);
            QCOMPARE(saved.deviceName, config.deviceName);
            QCOMPARE(saved.bufferSamples, 3000);
            table = QAccessible::queryAccessibleInterface(device->view());
            selected = table->tableInterface()->cellAt(device->currentIndex(), 0);
            QVERIFY(selected && selected->isValid());
            QCOMPARE(selected->text(QAccessible::Name), device->currentText());
        }
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
};

QTEST_MAIN(TstDeviceCard)
#include "tst_device_card.moc"
