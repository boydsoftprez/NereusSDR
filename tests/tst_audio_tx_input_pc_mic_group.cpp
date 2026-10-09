// tests/tst_audio_tx_input_pc_mic_group.cpp  (NereusSDR)
//
// Phase 3M-1b Task I.2 — AudioTxInputPage PC Mic group box +
// TransmitModel PC Mic session-state properties.
//
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies (R-SPK-21: the PC Mic controls are the PC microphone card, the
// DeviceCard the Devices page had, so 1 to 6 drive the card's own combos):
//   1.  Driver API round-trip via UI: select the card's Driver API →
//       TransmitModel pcMicHostApiIndex updates.
//   2.  Driver API round-trip via model: setPcMicHostApiIndex() → combo
//       selects the matching item.
//   3.  Device list repopulates on Driver API change.
//   4.  Buffer size offers the input sizes 64 to 8192 samples.
//   5.  Buffer size round-trip via UI: combo → model updates.
//   6.  Buffer size round-trip via model: setPcMicBufferSamples() → combo.
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
//  14.  PC microphone card visible and live by default (PC Mic selected).
//  15.  PC microphone card stays in view, greyed, when Radio Mic is
//       selected (R-SPK-21: disabled, never hidden).
//
// R-R3-36 Task 6 (2026-09-22, J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code):
//  16.  The page shows the microphone status and a disabled Retry, once.
//  17.  Test Mic holds a TestMic capture demand: the helper starts, the
//       status reads ready, the meter reads the real level; Stop Test
//       releases it and the helper exits.
//  18.  Hiding the page and destroying it release the demand.
//  19.  Each status the fake helper drives is shown with its exact text;
//       Retry is enabled only in Failed and starts a new attempt.
//  20.  One config: the PC microphone card and the TransmitModel setters
//       edit one audio/TxInput config.
//  21.  Existing persisted audio/TxInput values are loaded, not rewritten.
//
// R-R3-36 (2026-09-23, J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code):
//  22.  In a remote window Test Mic opens this computer's microphone and
//       meters it, without the page reaching the audited local-DSP
//       accessors.
//
// Native audio plan Task 16 fix round (2026-10-09, J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code), on fake engine backends:
//  23.  While the mic captures, the card's Delay readout reads
//       "Now N ms from USB Mic to the radio" (R-AUD-15).
//  24.  A mic another program holds reads so on the status line (amber),
//       in the card's note and in its closed Device field (R-AUD-11).
// Fix round 2 (2026-10-09, same authorship): 23 and 24 run on the Mac,
// Windows, PipeWire and PulseAudio fakes; the status line names the mic
// the field names; in 24 Test Mic stays on and Retry microphone is greyed
// with its reason (R-AUD-24), and when the other program lets the mic go
// the test resumes by itself and the note clears (R-AUD-11).
//  With NEREUS_AUDIO_SETUP_CAPTURE_DIR set, 23 and 24 save captures.
// Every capture demand here uses the scripted fake helper (this binary
// re-executed with --fake-capture-child); no real microphone is opened.

#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTimer>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/CaptureSupervisor.h"
#include "core/audio/IAudioEngineBackend.h"
#include "gui/HGauge.h"
#include "gui/setup/AudioTxInputPage.h"
#include "gui/setup/CaptureStatusText.h"
#include "gui/setup/DeviceCard.h"
#include "gui/styles/AppTheme.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include "fakes/FakeAudioEngineBackend.h"
#include "fakes/FakeCaptureChild.h"

#include <cstring>
#include <memory>
#include <vector>

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

// The system a mic case runs on: the fake backend and the engine saved
// for the mic (fix round 2: every desktop system).
struct MicSystem {
    QString stem;                 // the capture's system part
    AudioBackendId backend = AudioBackendId::CoreAudio;
    AudioEngineKind engine = AudioEngineKind::CoreAudio;
};

void addMicSystemRows()
{
    QTest::addColumn<QString>("stem");
    QTest::addColumn<int>("backend");
    QTest::addColumn<int>("engine");
    const auto row = [](const char* name, AudioBackendId backend, AudioEngineKind engine) {
        QTest::newRow(name) << QString::fromLatin1(name) << static_cast<int>(backend)
                            << static_cast<int>(engine);
    };
    row("mac", AudioBackendId::CoreAudio, AudioEngineKind::CoreAudio);
    row("windows", AudioBackendId::Wasapi, AudioEngineKind::WindowsShared);
    row("linux-pipewire", AudioBackendId::PipeWire, AudioEngineKind::PipeWire);
    row("linux-pulseaudio", AudioBackendId::PulseAudio, AudioEngineKind::PulseAudio);
}

MicSystem fetchMicSystem()
{
    QFETCH(QString, stem);
    QFETCH(int, backend);
    QFETCH(int, engine);
    return {stem, static_cast<AudioBackendId>(backend), static_cast<AudioEngineKind>(engine)};
}

AudioDeviceInfo fakeDevice(AudioDeviceDirection direction, const QString& id, const QString& name,
                           AudioBackendId backend = AudioBackendId::CoreAudio)
{
    AudioDeviceInfo info;
    info.backend = backend;
    info.direction = direction;
    info.id = id;
    info.name = name;
    info.channelCount = 2;
    return info;
}

// The saved mic "USB Mic". Saved before the RadioModel is made, as a
// profile is on disk before the app starts: the engine reads it then.
void saveUsbMic(AudioEngineKind engine = AudioEngineKind::CoreAudio)
{
    AudioDeviceConfig mic;
    mic.engine = engine;
    mic.deviceId = QStringLiteral("usb-mic-uid");
    mic.deviceName = QStringLiteral("USB Mic");
    mic.saveToSettings(QStringLiteral("audio/TxInput"));
}

// The engine on a fake Core Audio backend, started, so the mic role runs
// through the stream supervisor and the (fake) capture helper. No device
// is touched.
std::shared_ptr<FakeAudioEngineBackend> startOnFakeMic(
    RadioModel& model, AudioBackendId backend = AudioBackendId::CoreAudio)
{
    auto native = std::make_shared<FakeAudioEngineBackend>(backend);
    native->setDevices({fakeDevice(AudioDeviceDirection::Output, QStringLiteral("built-in-uid"),
                                   QStringLiteral("Built-in speakers"), backend),
                        fakeDevice(AudioDeviceDirection::Input, QStringLiteral("usb-mic-uid"),
                                   QStringLiteral("USB Mic"), backend)});
    native->setDefault(AudioDeviceDirection::Output, QStringLiteral("built-in-uid"));
    native->setDefault(AudioDeviceDirection::Input, QStringLiteral("usb-mic-uid"));
    std::vector<std::shared_ptr<IAudioEngineBackend>> backends;
    if (backend == AudioBackendId::PulseAudio) {
        // PulseAudio runs only where PipeWire does not.
        auto pipeWire = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PipeWire);
        pipeWire->setRunning(false);
        backends.push_back(pipeWire);
    }
    backends.push_back(native);
    AudioEngine* engine = model.audioEngine();
    engine->setVaxOutputsAllowed(false);
    engine->setAudioBackendsForTest(std::move(backends));
    engine->start();
    return native;
}

// With captures on, the app's own style, palette and baseline QSS
// (main.cpp), as tst_audio_setup_regroup's Microphone captures use.
void useAppLookForCaptures()
{
    if (qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR").isEmpty()) {
        return;
    }
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    applyDarkPalette(*qApp);
    applyAppBaselineQss(*qApp);
}

void saveCapture(QWidget* w, const QString& stem)
{
    const QString dir = qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR");
    if (dir.isEmpty()) {
        return;
    }
    QDir().mkpath(dir);
    QApplication::processEvents();
    const QPixmap shot = w->grab();
    const QString path =
        QStringLiteral("%1/%2@%3x.png").arg(dir, stem).arg(qRound(shot.devicePixelRatio()));
    QVERIFY2(shot.save(path), qPrintable(path));
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

    // ── 1. Driver API round-trip via UI ──────────────────────────────────────
    // Select a different Driver API on the PC microphone card and verify that
    // TransmitModel::pcMicHostApiIndex() reflects the stored index.

    void driverApi_ui_to_model()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QComboBox* combo = page.driverApiCombo();
        QVERIFY2(combo, "driverApiCombo() must not be null");

        // Item 0 is the PortAudio default; the host APIs follow.
        if (combo->count() < 2) {
            // In a headless test environment PortAudio may list no host API.
            QSKIP("No host API available (headless environment)");
        }

        combo->setCurrentIndex(1);
        QApplication::processEvents();

        const int expectedApi = combo->itemData(1).toInt();
        QCOMPARE(model.audioEngine()->txInputConfig().hostApiIndex, expectedApi);
        QCOMPARE(model.transmitModel().pcMicHostApiIndex(), expectedApi);
    }

    // ── 2. Driver API round-trip via model ───────────────────────────────────

    void driverApi_model_to_ui()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QComboBox* combo = page.driverApiCombo();
        QVERIFY2(combo, "driverApiCombo() must not be null");

        if (combo->count() < 2) {
            QSKIP("No host API available (headless environment)");
        }

        const int targetApi = combo->itemData(1).toInt();
        model.transmitModel().setPcMicHostApiIndex(targetApi);
        QApplication::processEvents();

        QCOMPARE(combo->currentData().toInt(), targetApi);
    }

    // ── 3. Device list repopulates on Driver API change ──────────────────────

    void deviceList_repopulates_on_driverApi_change()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QComboBox* api    = page.driverApiCombo();
        QComboBox* device = page.deviceCombo();
        QVERIFY2(api,    "driverApiCombo() must not be null");
        QVERIFY2(device, "deviceCombo() must not be null");

        if (api->count() >= 2) {
            api->setCurrentIndex(api->currentIndex() == 0 ? 1 : 0);
            QApplication::processEvents();
        }
        // At minimum the "(platform default)" entry.
        QVERIFY2(device->count() >= 1, "Device combo must have at least one entry after repopulation");
        QCOMPARE(device->itemData(0).toString(), QString());
    }

    // ── 4. Buffer size offers the input sizes ─────────────────────────────────

    void bufferSize_offersInputSizes()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QComboBox* buffer = page.bufferSizeCombo();
        QVERIFY2(buffer, "bufferSizeCombo() must not be null");
        QList<int> sizes;
        for (int i = 0; i < buffer->count(); ++i) {
            sizes << buffer->itemData(i).toInt();
        }
        QCOMPARE(sizes, (QList<int>{64, 128, 256, 512, 1024, 2048, 4096, 8192}));
        // In Device details, folded by default.
        QVERIFY(!page.pcMicCard()->detailsExpanded());
    }

    // ── 5. Buffer size round-trip via UI ──────────────────────────────────────

    void bufferSize_ui_to_model()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QComboBox* buffer = page.bufferSizeCombo();
        QVERIFY2(buffer, "bufferSizeCombo() must not be null");

        // The card debounces its buffer combo by 200 ms.
        buffer->setCurrentIndex(buffer->findData(1024));
        QTRY_COMPARE(model.transmitModel().pcMicBufferSamples(), 1024);
        QCOMPARE(AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput")).bufferSamples, 1024);
    }

    // ── 6. Buffer size round-trip via model ───────────────────────────────────

    void bufferSize_model_to_ui()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QComboBox* buffer = page.bufferSizeCombo();
        QVERIFY2(buffer, "bufferSizeCombo() must not be null");

        model.transmitModel().setPcMicBufferSamples(2048);
        QApplication::processEvents();

        QCOMPARE(buffer->currentData().toInt(), 2048);
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

    // ── 14. PC microphone card live by default ─────────────────────────────────
    // NOTE: We check !isHidden() rather than isVisible() because in a headless
    // test environment the page widget is never show()n.

    void pcMicGroup_visible_by_default()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QGroupBox* grp = page.pcMicGroupBox();
        QVERIFY2(grp, "pcMicGroupBox() must not be null");
        QCOMPARE(grp->title(), QStringLiteral("PC microphone"));
        QVERIFY2(!grp->isHidden(), "PC microphone must be in view when PC Mic is selected (default)");
        QVERIFY(grp->isEnabled());
    }

    // ── 15. PC microphone card greyed, never hidden, on Radio Mic ─────────────

    void pcMicGroup_greyed_on_radioMic()
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

        QVERIFY2(!grp->isHidden(), "PC microphone must stay in view when Radio Mic is selected");
        QVERIFY2(!grp->isEnabled(), "PC microphone must be greyed when Radio Mic is selected");

        // Switch back to PC Mic: live again.
        QRadioButton* pcBtn = findRadioButton(&page, QStringLiteral("PC Mic"));
        QVERIFY2(pcBtn, "PC Mic button not found");
        pcBtn->setChecked(true);
        QApplication::processEvents();

        QVERIFY(!grp->isHidden());
        QVERIFY2(grp->isEnabled(), "PC microphone must be live again when PC Mic is re-selected");
    }

    // ── 16. Status row, once, idle ────────────────────────────────────────────

    void statusRow_idle_once()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage txPage(&model);

        QCOMPARE(txPage.findChildren<QLabel*>(QStringLiteral("captureStatus")).size(), 1);
        QCOMPARE(txPage.findChildren<QPushButton*>(QStringLiteral("retryCapture")).size(), 1);
        QLabel* label = statusLabelOf(&txPage);
        QPushButton* retry = retryButtonOf(&txPage);
        QVERIFY(label);
        QVERIFY(retry);
        QCOMPARE(label->text(), QStringLiteral("Microphone not in use"));
        QCOMPARE(retry->text(), QStringLiteral("Retry microphone"));
        QVERIFY(!retry->isEnabled());
        // The status sits in the PC microphone card, with Test Mic.
        QVERIFY(txPage.pcMicGroupBox()->isAncestorOf(statusLabelOf(&txPage)));
        QVERIFY(txPage.pcMicGroupBox()->isAncestorOf(retryButtonOf(&txPage)));
        QVERIFY(txPage.pcMicGroupBox()->isAncestorOf(txPage.testMicButton()));
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

    // ── 23. The mic's Delay readout while it captures ─────────────────────────

    void micDelayReadoutWhileCapturing_data() { addMicSystemRows(); }

    void micDelayReadoutWhileCapturing()
    {
        const MicSystem system = fetchMicSystem();
        useAppLookForCaptures();
        saveUsbMic(system.engine);
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        useFakeHelper(model, QStringLiteral("ready"));
        startOnFakeMic(model, system.backend);
        AudioEngine* engine = model.audioEngine();
        AudioTxInputPage page(&model);
        page.resize(760, 720);
        page.pcMicCard()->setDetailsExpanded(true);
        page.show();
        QLabel* delayNow = page.pcMicCard()->findChild<QLabel*>(QStringLiteral("deviceDelayNow"));
        QVERIFY(delayNow != nullptr);
        QCOMPARE(delayNow->text(), QStringLiteral("Now -- ms"));

        page.testMicButton()->setChecked(true);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Ready, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(delayNow->text().endsWith(QStringLiteral(" ms from USB Mic to the radio")),
                                 5000);
        QVERIFY(delayNow->text().startsWith(QStringLiteral("Now ")));
        // R-AUD-15: the Negotiated pill reads the capture's rate.
        const QString rate = QStringLiteral("USB Mic · %1 Hz · ")
                                 .arg(engine->captureStatus().nativeRate);
        QVERIFY(engine->captureStatus().nativeRate > 0);
        bool pill = false;
        for (QLabel* label : page.pcMicCard()->findChildren<QLabel*>()) {
            pill = pill || label->text().startsWith(rate);
        }
        QVERIFY2(pill, qPrintable(rate));
        // Fix round 2: the status line names the mic the Device field does
        // (the helper reports the device it opened).
        QCOMPARE(statusLabelOf(&page)->text(), QStringLiteral("Microphone ready: USB Mic"));
        QCOMPARE(page.pcMicCard()->deviceCombo()->currentText(), QStringLiteral("USB Mic"));
        QCOMPARE(page.pcMicCard()->driverApiCombo()->currentText(),
                 audioEngineLabel(system.engine));
        saveCapture(&page, QStringLiteral("microphone-%1-delay-capturing-unfolded").arg(system.stem));

        page.testMicButton()->setChecked(false);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Closed, 5000);
        engine->stop();
    }

    // ── 24. A mic another program holds ───────────────────────────────────────

    void micInUseReadsSo_data() { addMicSystemRows(); }

    void micInUseReadsSo()
    {
        const MicSystem system = fetchMicSystem();
        // The fake helper answers busy while this file exists (another
        // program holds the mic), and opens once it is gone.
        QTemporaryDir held;
        QVERIFY(held.isValid());
        const QString heldFile = held.filePath(QStringLiteral("held"));
        {
            QFile marker(heldFile);
            QVERIFY(marker.open(QIODevice::WriteOnly));
        }
        qputenv("NEREUS_FAKE_CAPTURE_BUSY_FILE", heldFile.toLocal8Bit());
        const auto unsetHeld = qScopeGuard([] { qunsetenv("NEREUS_FAKE_CAPTURE_BUSY_FILE"); });
        useAppLookForCaptures();
        saveUsbMic(system.engine);
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        useFakeHelper(model, QStringLiteral("busy-while-marked"));
        startOnFakeMic(model, system.backend);
        AudioEngine* engine = model.audioEngine();
        AudioTxInputPage page(&model);
        page.resize(760, 720);
        page.pcMicCard()->setDetailsExpanded(true);
        page.show();

        page.testMicButton()->setChecked(true);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().reason, CaptureReason::DeviceInUse, 5000);
        QLabel* status = statusLabelOf(&page);
        QTRY_COMPARE_WITH_TIMEOUT(status->text(), QStringLiteral("PC mic in use by another program"),
                                  5000);
        QVERIFY(status->styleSheet().contains(QStringLiteral("#e0a030")));
        QLabel* note = page.pcMicCard()->findChild<QLabel*>(QStringLiteral("deviceStateNote"));
        QTRY_VERIFY_WITH_TIMEOUT(note->text().contains(QStringLiteral("in use by another program")),
                                 5000);
        QComboBox* device = nullptr;
        for (QComboBox* combo : page.pcMicCard()->findChildren<QComboBox*>()) {
            if (combo->currentText() == QStringLiteral("USB Mic (in use by another program)")) {
                device = combo;
            }
        }
        QVERIFY(device != nullptr);
        // The field grew to its text, which arrived while the page was shown.
        QTRY_VERIFY(device->width()
                    > device->fontMetrics().horizontalAdvance(device->currentText()));
        // Fix round 2: the test stays on, waiting for the mic (R-AUD-11: it
        // switches back by itself when it frees), and Retry microphone is
        // greyed with its reason (R-AUD-24: Retry stays for other failures).
        QVERIFY(page.testMicButton()->isChecked());
        QCOMPARE(page.testMicButton()->text(), QStringLiteral("Stop Test"));
        QVERIFY(page.hasTestMicDemand());
        QVERIFY(!retryButtonOf(&page)->isEnabled());
        QCOMPARE(retryButtonOf(&page)->toolTip(),
                 QStringLiteral("The mic resumes by itself when it comes back."));
        saveCapture(&page, QStringLiteral("microphone-%1-in-use-unfolded").arg(system.stem));

        // The other program lets the mic go: the test resumes by itself.
        QVERIFY(QFile::remove(heldFile));
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Ready, 8000);
        QTRY_COMPARE_WITH_TIMEOUT(status->text(), QStringLiteral("Microphone ready: USB Mic"), 5000);
        QVERIFY(status->styleSheet().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(note->text().isEmpty() || note->isHidden(), 5000);
        QCOMPARE(device->currentText(), QStringLiteral("USB Mic"));
        QCOMPARE(page.testMicButton()->text(), QStringLiteral("Stop Test"));
        QVERIFY(!retryButtonOf(&page)->isEnabled());
        QVERIFY(retryButtonOf(&page)->toolTip().isEmpty());

        page.testMicButton()->setChecked(false);
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureHelperProcessIdForTest(), qint64(0), 5000);
        engine->stop();
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

    // ── 19. Fake-driven statuses and the Retry rule ───────────────────────────

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
        QLabel* txLabel = statusLabelOf(&txPage);
        QPushButton* txRetry = retryButtonOf(&txPage);

        // Record what the page shows after every status change, and check
        // the Retry rule at every step. Connected after the page, so its
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
                    if (txLabel->text() != captureStatusText(engine->captureStatus())
                        || txRetry->isEnabled() != failed) {
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

        if (endsFailed) {
            // Retry starts a new attempt for the same demand. Every failing
            // scenario fails again, so wait for the retried generation's own
            // Failed before Test Mic is released.
            const quint32 failedGeneration = engine->captureStatus().generation;
            txRetry->click();
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
            QVERIFY(txRetry->isEnabled());

            // An explicit Retry with no demand clears the failure.
            txRetry->click();
        } else {
            txPage.testMicButton()->setChecked(false);
        }
        QTRY_COMPARE_WITH_TIMEOUT(engine->captureStatus().state, CaptureState::Closed, 5000);
        QTRY_COMPARE(txLabel->text(), QStringLiteral("Microphone not in use"));
        QVERIFY(!txRetry->isEnabled());
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

    // ── 20. One config across the card and TransmitModel ──────────────────────

    void crossPage_bufferRoundTrip()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioEngine* engine = model.audioEngine();
        AudioTxInputPage txPage(&model);
        DeviceCard* card = txPage.pcMicCard();
        QVERIFY(card);
        QComboBox* cardBuffer = txPage.bufferSizeCombo();
        QVERIFY(cardBuffer);

        // Card → engine, settings, TransmitModel (the card debounces its
        // buffer combo by 200 ms).
        cardBuffer->setCurrentIndex(cardBuffer->findData(1024));
        QTRY_COMPARE(engine->txInputConfig().bufferSamples, 1024);
        QCOMPARE(AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput")).bufferSamples, 1024);
        QCOMPARE(model.transmitModel().pcMicBufferSamples(), 1024);
        QCOMPARE(card->currentConfig().bufferSamples, 1024);

        // TransmitModel setter → engine, settings, the card.
        model.transmitModel().setPcMicBufferSamples(512);
        QCOMPARE(engine->txInputConfig().bufferSamples, 512);
        QCOMPARE(AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput")).bufferSamples, 512);
        QCOMPARE(cardBuffer->currentData().toInt(), 512);
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

        // An engine-side change reaches the page and TransmitModel.
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

        QCOMPARE(model.audioEngine()->txInputConfig().deviceName, QStringLiteral("Stored Mic"));
        QCOMPARE(model.transmitModel().pcMicDeviceName(), QStringLiteral("Stored Mic"));
        QCOMPARE(model.transmitModel().pcMicBufferSamples(), 256);
        QCOMPARE(txPage.deviceCombo()->currentData().toString(), QStringLiteral("Stored Mic"));
        QCOMPARE(txPage.bufferSizeCombo()->currentData().toInt(), 256);

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
