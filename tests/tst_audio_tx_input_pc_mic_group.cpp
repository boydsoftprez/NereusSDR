// tests/tst_audio_tx_input_pc_mic_group.cpp  (NereusSDR)
//
// Phase 3M-1b Task I.2 — AudioTxInputPage PC Mic group box +
// TransmitModel PC Mic session-state properties.
//
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies:
//   1.  Backend round-trip via UI: select backend combo → TransmitModel
//       pcMicHostApiIndex updates.
//   2.  Backend round-trip via model: setPcMicHostApiIndex() → combo
//       selects the matching item.
//   3.  Device list repopulates on backend change.
//   4.  Buffer slider value label updates on slider move: 1024 samples →
//       label contains "1024 samples".
//   5.  Buffer slider round-trip via UI: slider move → model updates.
//   6.  Buffer slider round-trip via model: setPcMicBufferSamples() →
//       slider position updates.
//   7.  Mic Gain slider round-trip via UI: slider → setMicGainDb.
//   8.  Mic Gain slider round-trip via model: setMicGainDb → slider.
//   9.  No feedback loop: model setter triggers signal → UI updates →
//       UI setter must NOT re-trigger model (QSignalSpy count).
//  10.  Test Mic button: click → button checked + VU timer running.
//       Click again → unchecked + timer stopped.  HGauge resets to 0.
//  11.  TransmitModel idempotency — setPcMicHostApiIndex same value:
//       no signal emitted.
//  12.  TransmitModel idempotency — setPcMicDeviceName same value:
//       no signal emitted.
//  13.  TransmitModel idempotency — setPcMicBufferSamples same value:
//       no signal emitted.
//  14.  PC Mic group box visible by default (PC Mic radio button selected).
//  15.  PC Mic group box hidden when Radio Mic is selected.
//
// R-R3-36 Task 6 (2026-09-22, J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code):
//  16.  Both pages show the microphone status and a disabled Retry.
//  17.  Test Mic holds a TestMic capture demand: the helper starts, the
//       status reads ready, the meter reads the real level; Stop Test
//       releases it and the helper exits.
//  18.  Hiding the page and destroying it release the demand.
//  19.  Each status the fake helper drives is shown with its exact text on
//       both pages; Retry is enabled only in Failed and starts a new
//       attempt.
//  20.  Cross-page config: the TX Input page, the Devices card and the
//       TransmitModel setters all edit one audio/TxInput config.
//  21.  Existing persisted audio/TxInput values are loaded, not rewritten.
//
// R-R3-36 (2026-09-23, J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code):
//  22.  In a remote window Test Mic opens this computer's microphone and
//       meters it, without the page reaching the audited local-DSP
//       accessors.
// Every capture demand here uses the scripted fake helper (this binary
// re-executed with --fake-capture-child); no real microphone is opened.

#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTimer>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/CaptureSupervisor.h"
#include "gui/HGauge.h"
#include "gui/setup/AudioDevicesPage.h"
#include "gui/setup/AudioTxInputPage.h"
#include "gui/setup/CaptureStatusText.h"
#include "gui/setup/DeviceCard.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include "fakes/FakeCaptureChild.h"

#include <cstring>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#endif

using namespace NereusSDR;
using CaptureState = CaptureSupervisor::Status::State;
using CaptureReason = CaptureSupervisor::Status::Reason;

namespace {

// Replaces the engine's capture supervisor with one that runs this binary
// as the scripted fake helper. Must run before any demand is taken.
void useFakeHelper(RadioModel& model, const QString& scenario, int openTimeoutMs = 10000)
{
    CaptureSupervisor::Options options;
    options.program = QCoreApplication::applicationFilePath();
    options.arguments = {QStringLiteral("--fake-capture-child"), scenario};
    options.openTimeoutMs = openTimeoutMs;
    model.audioEngine()->setCaptureSupervisorOptionsForTest(options);
}

bool processIsGone(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (handle == nullptr) {
        return true;
    }
    const DWORD result = WaitForSingleObject(handle, 0);
    CloseHandle(handle);
    return result == WAIT_OBJECT_0;
#else
    return ::kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH;
#endif
}

QLabel* statusLabelOf(QWidget* page)
{
    return page->findChild<QLabel*>(QStringLiteral("captureStatus"));
}

QPushButton* retryButtonOf(QWidget* page)
{
    return page->findChild<QPushButton*>(QStringLiteral("retryCapture"));
}

// The Devices page TX Input card's buffer-size combo (its items start at
// 64 samples; no other card combo does).
QComboBox* cardBufferCombo(DeviceCard* card)
{
    const auto combos = card->findChildren<QComboBox*>();
    for (QComboBox* combo : combos) {
        if (combo->count() > 0 && combo->itemData(0).toInt() == 64) {
            return combo;
        }
    }
    return nullptr;
}

DeviceCard* txInputCardOf(AudioDevicesPage& page)
{
    const auto cards = page.findChildren<DeviceCard*>();
    for (DeviceCard* card : cards) {
        if (card->title() == QStringLiteral("TX Input (Microphone)")) {
            return card;
        }
    }
    return nullptr;
}

} // namespace

// Helper: find the first QRadioButton with the given text.
static QRadioButton* findRadioButton(QWidget* parent, const QString& text)
{
    const auto buttons = parent->findChildren<QRadioButton*>();
    for (QRadioButton* btn : buttons) {
        if (btn->text() == text) { return btn; }
    }
    return nullptr;
}

class TestAudioTxInputPcMicGroup : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        AppSettings::instance().clear();
    }

    // Each case starts from an empty audio/TxInput so config written by one
    // case does not seed the next.
    void init()
    {
        AppSettings::instance().clear();
    }

    // ── 1. Backend round-trip via UI ─────────────────────────────────────────
    // Programmatically select a different item in the backend combo and verify
    // that TransmitModel::pcMicHostApiIndex() reflects the stored index.

    void backendCombo_ui_to_model()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QComboBox* combo = page.backendCombo();
        QVERIFY2(combo, "backendCombo() must not be null");

        if (combo->count() < 2) {
            // In a headless test environment, PortAudio may not have been
            // initialized; only one (placeholder) item or zero items.  Skip
            // rather than fail — the widget logic is correct; it's the host
            // environment that has no audio hardware.
            QSKIP("Fewer than 2 host APIs available (headless environment)");
        }

        // Select index 1 (whatever the second API is).
        combo->setCurrentIndex(1);
        QApplication::processEvents();

        // The stored value must equal the itemData of the selected combo entry.
        const int expectedApi = combo->itemData(1).toInt();
        QCOMPARE(model.transmitModel().pcMicHostApiIndex(), expectedApi);
    }

    // ── 2. Backend round-trip via model ──────────────────────────────────────
    // setPcMicHostApiIndex(N) → the combo should show the matching item.

    void backendCombo_model_to_ui()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QComboBox* combo = page.backendCombo();
        QVERIFY2(combo, "backendCombo() must not be null");

        if (combo->count() == 0) {
            QSKIP("No host APIs available (headless environment)");
        }

        // Pick the itemData of the first combo entry and call the model setter.
        const int targetApi = combo->itemData(0).toInt();
        model.transmitModel().setPcMicHostApiIndex(targetApi);
        QApplication::processEvents();

        // The combo's current item data must match.
        QCOMPARE(combo->currentData().toInt(), targetApi);
    }

    // ── 3. Device list repopulates on backend change ──────────────────────────
    // After changing the backend combo, the device combo must be repopulated
    // (we just verify it is non-empty and differs in count or content, or at
    // minimum still has the "(default)" entry).

    void deviceList_repopulates_on_backend_change()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QComboBox* backend = page.backendCombo();
        QComboBox* device  = page.deviceCombo();
        QVERIFY2(backend, "backendCombo() must not be null");
        QVERIFY2(device,  "deviceCombo() must not be null");

        // Record device count before backend change.
        const int countBefore = device->count();

        // Switch backend (if more than one available).
        if (backend->count() >= 2) {
            backend->setCurrentIndex(backend->currentIndex() == 0 ? 1 : 0);
            QApplication::processEvents();
        }
        // Device combo must be valid after change — at minimum has one entry.
        QVERIFY2(device->count() >= 1, "Device combo must have at least one entry after repopulation");
        (void)countBefore;  // suppress unused-variable warning in single-API envs
    }

    // ── 4. Buffer slider value label updates ──────────────────────────────────
    // Move the slider to the position for 1024 samples and verify the label
    // shows "1024 samples".

    void bufferSlider_label_updates()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QSlider* slider = page.bufferSlider();
        QLabel*  label  = page.bufferLabel();
        QVERIFY2(slider, "bufferSlider() must not be null");
        QVERIFY2(label,  "bufferLabel() must not be null");

        // Find the slider position for 1024 samples.
        const QVector<int>& sizes = AudioTxInputPage::kBufferSizes;
        const int pos1024 = sizes.indexOf(1024);
        QVERIFY2(pos1024 >= 0, "1024 must be in kBufferSizes");

        slider->setValue(pos1024);
        QApplication::processEvents();

        QVERIFY2(label->text().contains(QLatin1String("1024 samples")),
                 qPrintable(QStringLiteral("Expected '1024 samples' in label, got: %1")
                            .arg(label->text())));
    }

    // ── 5. Buffer slider round-trip via UI ────────────────────────────────────

    void bufferSlider_ui_to_model()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QSlider* slider = page.bufferSlider();
        QVERIFY2(slider, "bufferSlider() must not be null");

        const QVector<int>& sizes = AudioTxInputPage::kBufferSizes;
        const int pos1024 = sizes.indexOf(1024);
        QVERIFY2(pos1024 >= 0, "1024 must be in kBufferSizes");

        slider->setValue(pos1024);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().pcMicBufferSamples(), 1024);
    }

    // ── 6. Buffer slider round-trip via model ─────────────────────────────────

    void bufferSlider_model_to_ui()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QSlider* slider = page.bufferSlider();
        QVERIFY2(slider, "bufferSlider() must not be null");

        model.transmitModel().setPcMicBufferSamples(2048);
        QApplication::processEvents();

        const QVector<int>& sizes = AudioTxInputPage::kBufferSizes;
        const int expectedPos = sizes.indexOf(2048);
        QVERIFY2(expectedPos >= 0, "2048 must be in kBufferSizes");
        QCOMPARE(slider->value(), expectedPos);
    }

    // ── 7. Mic Gain slider round-trip via UI ──────────────────────────────────

    void micGainSlider_ui_to_model()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider, "micGainSlider() must not be null");

        slider->setValue(0);  // 0 dB
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().micGainDb(), 0);
    }

    // ── 8. Mic Gain slider round-trip via model ───────────────────────────────

    void micGainSlider_model_to_ui()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider, "micGainSlider() must not be null");

        model.transmitModel().setMicGainDb(10);
        QApplication::processEvents();

        QCOMPARE(slider->value(), 10);
    }

    // ── 9. No feedback loop ───────────────────────────────────────────────────
    // Setting mic gain via the model must update the UI, which must NOT then
    // re-trigger the model.  Use QSignalSpy to count micGainDbChanged emissions.

    void micGain_no_feedback_loop()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QSignalSpy spy(&model.transmitModel(), &TransmitModel::micGainDbChanged);

        // Set from model — this should drive the UI (one emission for the set).
        model.transmitModel().setMicGainDb(5);
        QApplication::processEvents();

        // The slider update must not have bounced a second emission back.
        QCOMPARE(spy.count(), 1);

        // Second set to a different value — still only one more emission.
        model.transmitModel().setMicGainDb(10);
        QApplication::processEvents();
        QCOMPARE(spy.count(), 2);
    }

    // ── 10. Test Mic button state machine ─────────────────────────────────────
    // Click → checked + VU timer running.  Click again → unchecked + stopped.
    // No real audio stream is opened in tests (pcMicInputLevel() returns 0.0f).

    void testMicButton_statesMachine()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        useFakeHelper(model, QStringLiteral("ready"));
        AudioTxInputPage page(&model);

        QPushButton* btn    = page.testMicButton();
        HGauge*      vuBar  = page.vuBar();
        QVERIFY2(btn,   "testMicButton() must not be null");
        QVERIFY2(vuBar, "vuBar() must not be null");

        // Initially unchecked.
        QVERIFY2(!btn->isChecked(), "Test Mic must start unchecked");

        // Click to start.
        btn->setChecked(true);
        QApplication::processEvents();
        QVERIFY2(btn->isChecked(), "Test Mic must be checked after first click");
        QCOMPARE(btn->text(), QStringLiteral("Stop Test"));

        // Click to stop.
        btn->setChecked(false);
        QApplication::processEvents();
        QVERIFY2(!btn->isChecked(), "Test Mic must be unchecked after second click");
        QCOMPARE(btn->text(), QStringLiteral("Test Mic"));
    }

    // ── 11. TransmitModel idempotency — pcMicHostApiIndex ────────────────────

    void transmitModel_setPcMicHostApiIndex_idempotent()
    {
        TransmitModel tx;
        // Default is -1.
        QSignalSpy spy(&tx, &TransmitModel::pcMicHostApiIndexChanged);

        tx.setPcMicHostApiIndex(-1);  // same as default — no signal
        QCOMPARE(spy.count(), 0);

        tx.setPcMicHostApiIndex(3);   // change — signal fires
        QCOMPARE(spy.count(), 1);

        tx.setPcMicHostApiIndex(3);   // same value — no signal
        QCOMPARE(spy.count(), 1);
    }

    // ── 12. TransmitModel idempotency — pcMicDeviceName ──────────────────────

    void transmitModel_setPcMicDeviceName_idempotent()
    {
        TransmitModel tx;
        // Default is empty.
        QSignalSpy spy(&tx, &TransmitModel::pcMicDeviceNameChanged);

        tx.setPcMicDeviceName(QString());       // same as default — no signal
        QCOMPARE(spy.count(), 0);

        tx.setPcMicDeviceName(QStringLiteral("Built-in Mic"));  // change
        QCOMPARE(spy.count(), 1);

        tx.setPcMicDeviceName(QStringLiteral("Built-in Mic"));  // same — no signal
        QCOMPARE(spy.count(), 1);
    }

    // ── 13. TransmitModel idempotency — pcMicBufferSamples ───────────────────

    void transmitModel_setPcMicBufferSamples_idempotent()
    {
        TransmitModel tx;
        // Default is 512.
        QSignalSpy spy(&tx, &TransmitModel::pcMicBufferSamplesChanged);

        tx.setPcMicBufferSamples(512);    // same as default — no signal
        QCOMPARE(spy.count(), 0);

        tx.setPcMicBufferSamples(1024);   // change
        QCOMPARE(spy.count(), 1);

        tx.setPcMicBufferSamples(1024);   // same — no signal
        QCOMPARE(spy.count(), 1);
    }

    // ── 14. PC Mic group visible by default ───────────────────────────────────
    // NOTE: We check !isHidden() rather than isVisible() because in a headless
    // test environment the page widget is never show()n, so isVisible() always
    // returns false for the top-level widget and its children — but isHidden()
    // accurately reflects the explicit setVisible(false) call on the group box.

    void pcMicGroup_visible_by_default()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QGroupBox* grp = page.pcMicGroupBox();
        QVERIFY2(grp, "pcMicGroupBox() must not be null");
        // isHidden() reflects the explicit hide/show state independent of parent show state.
        QVERIFY2(!grp->isHidden(), "PC Mic group must not be hidden when PC Mic is selected (default)");
    }

    // ── 15. PC Mic group hidden when Radio Mic is selected ────────────────────

    void pcMicGroup_hidden_on_radioMic()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);  // hasMicJack=true so Radio Mic is enabled
        AudioTxInputPage page(&model);

        QGroupBox*    grp      = page.pcMicGroupBox();
        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(grp,      "pcMicGroupBox() must not be null");
        QVERIFY2(radioBtn, "Radio Mic button not found");

        radioBtn->setChecked(true);
        QApplication::processEvents();

        QVERIFY2(grp->isHidden(), "PC Mic group must be hidden when Radio Mic is selected");

        // Switch back to PC Mic — group must reappear.
        QRadioButton* pcBtn = findRadioButton(&page, QStringLiteral("PC Mic"));
        QVERIFY2(pcBtn, "PC Mic button not found");
        pcBtn->setChecked(true);
        QApplication::processEvents();

        QVERIFY2(!grp->isHidden(), "PC Mic group must reappear when PC Mic is re-selected");
    }

    // ── 16. Status row on both pages, idle ────────────────────────────────────

    void statusRow_idle_onBothPages()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage txPage(&model);
        AudioDevicesPage devicesPage(&model);

        for (QWidget* page : {static_cast<QWidget*>(&txPage), static_cast<QWidget*>(&devicesPage)}) {
            QLabel* label = statusLabelOf(page);
            QPushButton* retry = retryButtonOf(page);
            QVERIFY(label);
            QVERIFY(retry);
            QCOMPARE(label->text(), QStringLiteral("Microphone not in use"));
            QCOMPARE(retry->text(), QStringLiteral("Retry microphone"));
            QVERIFY(!retry->isEnabled());
        }
        // The TX page's status sits in the PC Mic group, with Test Mic.
        QVERIFY(txPage.pcMicGroupBox()->isAncestorOf(statusLabelOf(&txPage)));
        QVERIFY(txPage.pcMicGroupBox()->isAncestorOf(retryButtonOf(&txPage)));
    }

    // ── 17. Test Mic holds a real capture demand ──────────────────────────────

    void testMic_holdsAndReleasesDemand()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        useFakeHelper(model, QStringLiteral("ready"));
        AudioEngine* engine = model.audioEngine();
        AudioTxInputPage page(&model);

        QCOMPARE(engine->captureHelperProcessIdForTest(), qint64(0));
        QVERIFY(!page.hasTestMicDemand());

        page.testMicButton()->setChecked(true);
        QVERIFY(page.hasTestMicDemand());
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Ready, 5000);
        const qint64 pid = engine->captureHelperProcessIdForTest();
        QVERIFY(pid > 0);

        QLabel* label = statusLabelOf(&page);
        QTRY_COMPARE(label->text(), captureStatusText(engine->captureStatus()));
        QVERIFY(label->text().startsWith(QStringLiteral("Microphone ready: ")));
        QVERIFY(!retryButtonOf(&page)->isEnabled());

        // The meter reads the real capture level (the fake sends a tone).
        QTRY_VERIFY_WITH_TIMEOUT(page.vuBar()->value() > 1.0, 3000);

        page.testMicButton()->setChecked(false);
        QVERIFY(!page.hasTestMicDemand());
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Closed, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(processIsGone(pid), 5000);
        QTRY_COMPARE(label->text(), QStringLiteral("Microphone not in use"));
        QCOMPARE(page.vuBar()->value(), 0.0);
    }

    // ── 22. Test Mic in a remote window (R-R3-36) ─────────────────────────────
    //
    // The microphone is this computer's in a remote window too: the page
    // reaches the engine through RadioModel::localAudioDevices(), so the
    // Setup gate leaves it enabled, and Test Mic takes the same capture
    // demand and meters the same level it does locally.

    void testMic_opensThisComputersMicrophoneInARemoteWindow()
    {
        RadioModel remote(RadioModel::Role::Remote);
        AudioEngine* engine = remote.localAudioDevices();
        AudioTxInputPage page(&remote);
        QCOMPARE(remote.localDspHandOutCount(), 0);

        // Before any demand, as setCaptureSupervisorOptionsForTest needs.
        CaptureSupervisor::Options options;
        options.program = QCoreApplication::applicationFilePath();
        options.arguments = {QStringLiteral("--fake-capture-child"), QStringLiteral("ready")};
        options.openTimeoutMs = 10000;
        engine->setCaptureSupervisorOptionsForTest(options);

        page.testMicButton()->setChecked(true);
        QVERIFY(page.hasTestMicDemand());
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Ready, 5000);
        const qint64 pid = engine->captureHelperProcessIdForTest();
        QVERIFY(pid > 0);
        QTRY_VERIFY(statusLabelOf(&page)->text().startsWith(QStringLiteral("Microphone ready: ")));
        QTRY_VERIFY_WITH_TIMEOUT(page.vuBar()->value() > 1.0, 3000);

        page.testMicButton()->setChecked(false);
        QVERIFY(!page.hasTestMicDemand());
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Closed, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(processIsGone(pid), 5000);
        QCOMPARE(remote.localDspHandOutCount(), 0);
    }

    // ── 18. Hide and destruction release the demand ───────────────────────────

    void testMic_releasedOnHide()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        useFakeHelper(model, QStringLiteral("ready"));
        AudioEngine* engine = model.audioEngine();
        AudioTxInputPage page(&model);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        page.testMicButton()->setChecked(true);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Ready, 5000);
        const qint64 pid = engine->captureHelperProcessIdForTest();
        QVERIFY(pid > 0);

        page.hide();
        QVERIFY(!page.testMicButton()->isChecked());
        QVERIFY(!page.hasTestMicDemand());
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(processIsGone(pid), 5000);
    }

    void testMic_releasedOnDestruction()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        useFakeHelper(model, QStringLiteral("ready"));
        AudioEngine* engine = model.audioEngine();
        auto page = std::make_unique<AudioTxInputPage>(&model);

        page->testMicButton()->setChecked(true);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Ready, 5000);
        const qint64 pid = engine->captureHelperProcessIdForTest();
        QVERIFY(pid > 0);

        page.reset();
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Closed, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(processIsGone(pid), 5000);
    }

    // ── 19. Fake-driven statuses and the Retry rule on both pages ─────────────

    void status_fakeScenarios_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<int>("openTimeoutMs");
        QTest::addColumn<QStringList>("mustShow");
        QTest::addColumn<bool>("endsFailed");

        QTest::newRow("ready") << QStringLiteral("ready") << 10000
            << QStringList{QStringLiteral("Preparing microphone")} << false;
        QTest::newRow("permission") << QStringLiteral("permission-then-ready") << 10000
            << QStringList{QStringLiteral("Waiting for microphone permission")} << false;
        QTest::newRow("timeout") << QStringLiteral("hang-open") << 300
            << QStringList{QStringLiteral("Preparing microphone"),
                           QStringLiteral("The microphone did not respond in time.")} << true;
        QTest::newRow("protocol") << QStringLiteral("malformed") << 10000
            << QStringList{QStringLiteral("Microphone support stopped unexpectedly.")} << true;
        QTest::newRow("exited") << QStringLiteral("crash-after-ready") << 10000
            << QStringList{QStringLiteral("Microphone support stopped unexpectedly.")} << true;
        QTest::newRow("input-lost") << QStringLiteral("input-lost") << 10000
            << QStringList{QStringLiteral("The microphone stopped sending audio.")} << true;
    }

    void status_fakeScenarios()
    {
        QFETCH(QString, scenario);
        QFETCH(int, openTimeoutMs);
        QFETCH(QStringList, mustShow);
        QFETCH(bool, endsFailed);

        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        useFakeHelper(model, scenario, openTimeoutMs);
        AudioEngine* engine = model.audioEngine();
        AudioTxInputPage txPage(&model);
        AudioDevicesPage devicesPage(&model);
        QLabel* txLabel = statusLabelOf(&txPage);
        QLabel* devLabel = statusLabelOf(&devicesPage);
        QPushButton* txRetry = retryButtonOf(&txPage);
        QPushButton* devRetry = retryButtonOf(&devicesPage);

        // Record what each page shows after every status change, and check
        // the Retry rule at every step. Connected after the pages, so their
        // own handlers have already run.
        QStringList shown;
        bool retryRuleHeld = true;
        // Declared last so it is destroyed first: the recorder never
        // outlives the locals it writes, even when a check returns early.
        QObject recorderScope;
        connect(engine, &AudioEngine::captureStatusChanged, &recorderScope,
                [&](const CaptureSupervisor::Status& status) {
                    const bool failed = status.state == CaptureState::Failed;
                    shown << txLabel->text();
                    if (txLabel->text() != devLabel->text()
                        || txLabel->text() != captureStatusText(engine->captureStatus())
                        || txRetry->isEnabled() != failed
                        || devRetry->isEnabled() != failed) {
                        retryRuleHeld = false;
                    }
                });

        txPage.testMicButton()->setChecked(true);
        if (endsFailed) {
            QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Failed, 5000);
        } else {
            QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Ready, 5000);
        }
        for (const QString& text : mustShow) {
            QVERIFY2(shown.contains(text), qPrintable(shown.join(QStringLiteral(" | "))));
        }
        QVERIFY2(retryRuleHeld, qPrintable(shown.join(QStringLiteral(" | "))));
        QCOMPARE(txRetry->isEnabled(), endsFailed);
        QCOMPARE(devRetry->isEnabled(), endsFailed);

        if (endsFailed) {
            // Retry from the Devices page starts a new attempt for the same
            // demand. Every failing scenario fails again, so wait for the
            // retried generation's own Failed before Test Mic is released.
            const quint32 failedGeneration = engine->captureStatus().generation;
            devRetry->click();
            QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().state == CaptureState::Failed
                                         && engine->captureStatus().generation > failedGeneration,
                                     5000);
            const CaptureSupervisor::Status retriedFailure = engine->captureStatus();

            // Failure before release. The capture design (item 8 of
            // docs/architecture/2026-09-22-optional-microphone-capture-design.md)
            // keeps a failure visible until an explicit retry, a device
            // change or a new eligible session; releasing demand is none of
            // those, so Stop Test leaves the failure and Retry on screen.
            txPage.testMicButton()->setChecked(false);
            QVERIFY(!txPage.hasTestMicDemand());
            QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
            QTest::qWait(300);
            QCOMPARE(engine->captureStatus(), retriedFailure);
            QCOMPARE(txLabel->text(), captureStatusText(retriedFailure));
            QVERIFY(txLabel->text() != QStringLiteral("Microphone not in use"));
            QCOMPARE(devLabel->text(), txLabel->text());
            QVERIFY(txRetry->isEnabled());
            QVERIFY(devRetry->isEnabled());

            // An explicit Retry with no demand clears the failure.
            txRetry->click();
        } else {
            txPage.testMicButton()->setChecked(false);
        }
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Closed, 5000);
        QTRY_COMPARE(txLabel->text(), QStringLiteral("Microphone not in use"));
        QTRY_COMPARE(devLabel->text(), QStringLiteral("Microphone not in use"));
        QVERIFY(!txRetry->isEnabled());
        QVERIFY(!devRetry->isEnabled());
        QVERIFY2(retryRuleHeld, qPrintable(shown.join(QStringLiteral(" | "))));
    }

    // Retry is wired on the TX Input page too. Covers both orders of a
    // failure and the release of the last demand. The capture design (item 8
    // of docs/architecture/2026-09-22-optional-microphone-capture-design.md)
    // keeps a failure visible until an explicit retry, a device change or a
    // new eligible session, so releasing after a failure leaves it on screen,
    // while a failure that arrives after the release is ignored.
    void retry_fromTxPage_startsNewAttempt()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        useFakeHelper(model, QStringLiteral("malformed"));
        AudioEngine* engine = model.audioEngine();
        AudioTxInputPage page(&model);
        QLabel* label = statusLabelOf(&page);
        QPushButton* retry = retryButtonOf(&page);

        page.testMicButton()->setChecked(true);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Failed, 5000);
        QTRY_VERIFY(retry->isEnabled());
        const quint32 firstFailure = engine->captureStatus().generation;

        // Failure before release: Retry opens a new generation, which the
        // malformed helper fails too; only then is Test Mic switched off.
        retry->click();
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().state == CaptureState::Failed
                                     && engine->captureStatus().generation > firstFailure,
                                 5000);
        const CaptureSupervisor::Status failedBeforeRelease = engine->captureStatus();
        page.testMicButton()->setChecked(false);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
        QTest::qWait(300);
        QCOMPARE(engine->captureStatus(), failedBeforeRelease);
        QCOMPARE(label->text(), QStringLiteral("Microphone support stopped unexpectedly."));
        QVERIFY(retry->isEnabled());

        // A new Test Mic demand is a new attempt; it fails the same way.
        page.testMicButton()->setChecked(true);
        QTRY_VERIFY_WITH_TIMEOUT(engine->captureStatus().state == CaptureState::Failed
                                     && engine->captureStatus().generation
                                            > failedBeforeRelease.generation,
                                 5000);
        const quint32 lastFailure = engine->captureStatus().generation;

        // Failure after release: Retry and Stop Test in the same turn queue
        // the new attempt and the release to the capture thread back to back,
        // so the release reaches it before the new helper has started and
        // written its malformed record. That late failure must be ignored.
        QList<CaptureSupervisor::Status> seen;
        QObject recorderScope;
        connect(engine, &AudioEngine::captureStatusChanged, &recorderScope,
                [&seen](const CaptureSupervisor::Status& status) { seen << status; });
        retry->click();
        page.testMicButton()->setChecked(false);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Closed, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
        QTest::qWait(300);
        QCOMPARE(engine->captureStatus().state, CaptureState::Closed);
        QVERIFY(engine->captureStatus().generation > lastFailure);
        for (const CaptureSupervisor::Status& status : seen) {
            QVERIFY2(!(status.state == CaptureState::Failed && status.generation > lastFailure),
                     "a failure from the released attempt was published");
        }
        QCOMPARE(label->text(), QStringLiteral("Microphone not in use"));
        QVERIFY(!retry->isEnabled());
    }

    // ── 20. One config across both pages and TransmitModel ────────────────────

    void crossPage_bufferRoundTrip()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioEngine* engine = model.audioEngine();
        AudioTxInputPage txPage(&model);
        AudioDevicesPage devicesPage(&model);
        DeviceCard* card = txInputCardOf(devicesPage);
        QVERIFY(card);
        QComboBox* cardBuffer = cardBufferCombo(card);
        QVERIFY(cardBuffer);

        // TX Input page → engine, settings, TransmitModel, Devices card.
        txPage.bufferSlider()->setValue(AudioTxInputPage::kBufferSizes.indexOf(1024));
        QCOMPARE(engine->txInputConfig().bufferSamples, 1024);
        QCOMPARE(AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput")).bufferSamples, 1024);
        QCOMPARE(model.transmitModel().pcMicBufferSamples(), 1024);
        QCOMPARE(card->currentConfig().bufferSamples, 1024);

        // Devices card → engine, TransmitModel, TX Input page (the card
        // debounces its buffer combo by 200 ms).
        cardBuffer->setCurrentIndex(cardBuffer->findData(256));
        QTRY_COMPARE(engine->txInputConfig().bufferSamples, 256);
        QCOMPARE(model.transmitModel().pcMicBufferSamples(), 256);
        QCOMPARE(txPage.bufferSlider()->value(), AudioTxInputPage::kBufferSizes.indexOf(256));
        QVERIFY(txPage.bufferLabel()->text().contains(QLatin1String("256 samples")));

        // TransmitModel setter → engine, settings, both pages.
        model.transmitModel().setPcMicBufferSamples(512);
        QCOMPARE(engine->txInputConfig().bufferSamples, 512);
        QCOMPARE(AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput")).bufferSamples, 512);
        QCOMPARE(txPage.bufferSlider()->value(), AudioTxInputPage::kBufferSizes.indexOf(512));
        QCOMPARE(card->currentConfig().bufferSamples, 512);
    }

    void crossPage_deviceNameIsOneSelection()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioEngine* engine = model.audioEngine();
        AudioTxInputPage txPage(&model);

        // A named device that is not present stays the selection: it is
        // persisted, mirrored and shown under its own name, never swapped
        // for another microphone.
        const QString missing = QStringLiteral("No Such Microphone");
        model.transmitModel().setPcMicDeviceName(missing);
        QCOMPARE(engine->txInputConfig().deviceName, missing);
        QCOMPARE(AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput")).deviceName, missing);
        QCOMPARE(txPage.deviceCombo()->currentData().toString(), missing);
        QVERIFY(txPage.deviceCombo()->currentText().contains(missing));

        // Picking "(default)" on the page clears it everywhere.
        txPage.deviceCombo()->setCurrentIndex(0);
        QCOMPARE(engine->txInputConfig().deviceName, QString());
        QCOMPARE(model.transmitModel().pcMicDeviceName(), QString());
        QCOMPARE(AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput")).deviceName, QString());

        // An engine-side change (as the Devices card makes) reaches the page
        // and TransmitModel.
        AudioDeviceConfig cfg = engine->txInputConfig();
        cfg.deviceName = missing;
        cfg.saveToSettings(QStringLiteral("audio/TxInput"));
        engine->setTxInputConfig(cfg);
        QCOMPARE(model.transmitModel().pcMicDeviceName(), missing);
        QCOMPARE(txPage.deviceCombo()->currentData().toString(), missing);
    }

    // ── 21. Existing persisted values are loaded, not rewritten ───────────────

    void persistedTxInput_isLoadedUntouched()
    {
        AudioDeviceConfig stored;
        stored.deviceName = QStringLiteral("Stored Mic");
        stored.bufferSamples = 256;
        stored.sampleRate = 44100;
        stored.channels = 1;
        stored.saveToSettings(QStringLiteral("audio/TxInput"));
        const QStringList keysBefore = AppSettings::instance().allKeys();
        QMap<QString, QVariant> before;
        for (const QString& key : keysBefore) {
            before.insert(key, AppSettings::instance().value(key));
        }

        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage txPage(&model);
        AudioDevicesPage devicesPage(&model);

        QCOMPARE(model.audioEngine()->txInputConfig().deviceName, QStringLiteral("Stored Mic"));
        QCOMPARE(model.transmitModel().pcMicDeviceName(), QStringLiteral("Stored Mic"));
        QCOMPARE(model.transmitModel().pcMicBufferSamples(), 256);
        QCOMPARE(txPage.deviceCombo()->currentData().toString(), QStringLiteral("Stored Mic"));
        QCOMPARE(txPage.bufferSlider()->value(), AudioTxInputPage::kBufferSizes.indexOf(256));

        for (auto it = before.cbegin(); it != before.cend(); ++it) {
            if (it.key().startsWith(QStringLiteral("audio/TxInput"))) {
                QCOMPARE(AppSettings::instance().value(it.key()), it.value());
            }
        }
    }
};

int main(int argc, char* argv[])
{
    if (argc > 2 && std::strcmp(argv[1], "--fake-capture-child") == 0) {
        return NereusSDR::Test::runFakeCaptureChild(QString::fromLocal8Bit(argv[2]));
    }
    QApplication app(argc, argv);
    TestAudioTxInputPcMicGroup test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_audio_tx_input_pc_mic_group.moc"
