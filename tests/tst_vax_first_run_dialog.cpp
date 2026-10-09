// =================================================================
// tests/tst_vax_first_run_dialog.cpp  (NereusSDR)
// =================================================================
//
// Phase 3O Sub-Phase 11 Task 11a — widget-level coverage for the
// first-run VAX setup dialog. The dialog itself is NereusSDR-original
// (no Thetis port); see src/gui/VaxFirstRunDialog.{h,cpp}.
//
// Coverage:
//   1. Construction in all 5 scenarios.
//   2. Scenario A Apply-suggested emits payload + Accepted.
//   3. Scenario A/B Skip / Continue-without-VAX — no payload, Accepted.
//   4. Scenario A Customize: openSetupAudioPage("Digital modes") emitted, Rejected.
//   5. Scenario B Install-URL — openInstallUrl carries the vendor URL.
//   6. Escape key — Rejected, no signals.
//   7. Scenario C/D Got-it — no payload, Accepted.
//   8. Native audio plan Task 18 (R-AUD-03, R-AUD-06): over an engine on
//      fake backends, a cable added while the dialog is open turns "no
//      cables" into the suggested bindings and its removal turns it back;
//      Scenario E lists a cable added live as new; "Rescan now" rescans
//      the older drivers and lists what they found.
//
// Modification history (NereusSDR):
//   2026-10-09 - native audio plan Task 18 (R-AUD-03, R-AUD-06): the
//                render-end payloads and the live and Rescan now cases.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QPair>
#include <QPushButton>
#include <QSignalSpy>
#include <QVector>

#include "core/AudioEngine.h"
#include "core/audio/CaptureProtocol.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/VirtualCableDetector.h"
#include "gui/VaxFirstRunDialog.h"

#include "fakes/FakeAudioEngineBackend.h"

#include <memory>

using namespace NereusSDR;

namespace {

const QString kCableA = QStringLiteral("CABLE-A Input (VB-Audio Virtual Cable)");
const QString kCableC = QStringLiteral("CABLE-C Input (VB-Audio Virtual Cable)");
const QString kCableD = QStringLiteral("CABLE-D Input (VB-Audio Virtual Cable)");

AudioDeviceInfo playback(AudioBackendId backend, const QString& id, const QString& name)
{
    AudioDeviceInfo info;
    info.backend = backend;
    info.direction = AudioDeviceDirection::Output;
    info.id = id;
    info.name = name;
    info.channelCount = 2;
    return info;
}

// An engine on a fake Windows audio backend and fake older drivers, with
// no device opened and no helper started.
struct EngineRig {
    std::shared_ptr<FakeAudioEngineBackend> native =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Wasapi);
    std::shared_ptr<FakeAudioEngineBackend> older =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
    std::unique_ptr<AudioEngine> engine = std::make_unique<AudioEngine>();

    EngineRig()
    {
        older->setTakesStereoMix(false);
        native->setDevices({playback(AudioBackendId::Wasapi, QStringLiteral("spk-uid"),
                                     QStringLiteral("Desk speakers"))});
    }

    void start()
    {
        CaptureSupervisor::Options options;
        options.program = QDir::temp().absoluteFilePath(QStringLiteral("no-such-capture-helper"));
        engine->setCaptureSupervisorOptionsForTest(options);
        engine->setVaxOutputsAllowed(false);
        engine->setAudioBackendsForTest({native, older});
        engine->start();
        QVERIFY(engine->catalogue() != nullptr);
    }
};

QStringList renderEnds(const VaxFirstRunDialog& dlg)
{
    QStringList out;
    for (const DetectedCable& c : dlg.detectedForTest()) {
        if (!c.isInput) {
            out << c.deviceName;
        }
    }
    return out;
}

// Shared detection payload for Scenarios A and E. VB-Audio cables as
// Windows lists them: "CABLE-x Input" is the playback (render) end VAX
// writes to (isInput false), "CABLE-x Output" the recording end apps read
// (isInput true). The dialog only surfaces render-end rows.
QVector<DetectedCable> windowsCablesPayload()
{
    QVector<DetectedCable> v;
    v.push_back({VirtualCableProduct::VbCableA,
                 QStringLiteral("CABLE-A Input (VB-Audio Virtual Cable)"),
                 false, 0});
    v.push_back({VirtualCableProduct::VbCableA,
                 QStringLiteral("CABLE-A Output (VB-Audio Virtual Cable)"),
                 true, 0});
    v.push_back({VirtualCableProduct::VbCableB,
                 QStringLiteral("CABLE-B Input (VB-Audio Virtual Cable)"),
                 false, 0});
    v.push_back({VirtualCableProduct::VbCableC,
                 QStringLiteral("CABLE-C Input (VB-Audio Virtual Cable)"),
                 false, 0});
    return v;
}

QVector<DetectedCable> rescanNewCablesPayload()
{
    QVector<DetectedCable> v;
    v.push_back({VirtualCableProduct::VbCableC,
                 QStringLiteral("CABLE-C Input (VB-Audio Virtual Cable)"),
                 false, 0});
    v.push_back({VirtualCableProduct::VbCableD,
                 QStringLiteral("CABLE-D Input (VB-Audio Virtual Cable)"),
                 false, 0});
    return v;
}

} // namespace

class TstVaxFirstRunDialog : public QObject {
    Q_OBJECT

private slots:

    // ── 1. All 5 scenarios construct without crashing ───────────────────
    void constructsInAllScenarios_data()
    {
        QTest::addColumn<int>("scenarioInt");
        QTest::newRow("A-WindowsCablesFound")
            << static_cast<int>(FirstRunScenario::WindowsCablesFound);
        QTest::newRow("B-WindowsNoCables")
            << static_cast<int>(FirstRunScenario::WindowsNoCables);
        QTest::newRow("C-MacNative")
            << static_cast<int>(FirstRunScenario::MacNative);
        QTest::newRow("D-LinuxNative")
            << static_cast<int>(FirstRunScenario::LinuxNative);
        QTest::newRow("E-RescanNewCables")
            << static_cast<int>(FirstRunScenario::RescanNewCables);
    }

    void constructsInAllScenarios()
    {
        QFETCH(int, scenarioInt);
        const auto scenario = static_cast<FirstRunScenario>(scenarioInt);

        QVector<DetectedCable> payload;
        if (scenario == FirstRunScenario::WindowsCablesFound) {
            payload = windowsCablesPayload();
        } else if (scenario == FirstRunScenario::RescanNewCables) {
            payload = rescanNewCablesPayload();
        }

        VaxFirstRunDialog dlg(scenario, payload);
        QCOMPARE(dlg.scenarioForTest(), scenario);
        QVERIFY(dlg.width() == 560);
    }

    // ── 2. Apply-suggested (Scenario A) emits signal + Accepted ─────────
    void applySuggestedEmitsBindings_scenarioA()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsCablesFound,
                              windowsCablesPayload());

        QSignalSpy applySpy(&dlg,
                            &VaxFirstRunDialog::applySuggested);
        QSignalSpy openSetupSpy(&dlg,
                                &VaxFirstRunDialog::openSetupAudioPage);

        auto* applyBtn = dlg.findChild<QPushButton*>(
            QStringLiteral("btnApplySuggested"));
        QVERIFY(applyBtn);

        QTest::mouseClick(applyBtn, Qt::LeftButton);

        QCOMPARE(applySpy.count(), 1);
        QCOMPARE(openSetupSpy.count(), 0);

        const auto args = applySpy.takeFirst();
        const auto bindings =
            args.at(0).value<QVector<QPair<int, QString>>>();
        QCOMPARE(bindings.size(), 3);
        QCOMPARE(bindings.at(0).first, 1);
        QCOMPARE(bindings.at(0).second,
                 QStringLiteral("CABLE-A Input (VB-Audio Virtual Cable)"));
        QCOMPARE(bindings.at(1).first, 2);
        QCOMPARE(bindings.at(1).second,
                 QStringLiteral("CABLE-B Input (VB-Audio Virtual Cable)"));
        QCOMPARE(bindings.at(2).first, 3);
        QCOMPARE(bindings.at(2).second,
                 QStringLiteral("CABLE-C Input (VB-Audio Virtual Cable)"));

        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    // ── 3a. Skip (Scenario A) — no payload, Accepted ────────────────────
    void skipButtonAcceptsWithoutSignal_scenarioA()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsCablesFound,
                              windowsCablesPayload());

        QSignalSpy applySpy(&dlg, &VaxFirstRunDialog::applySuggested);
        QSignalSpy openSetupSpy(&dlg,
                                &VaxFirstRunDialog::openSetupAudioPage);

        auto* skipBtn = dlg.findChild<QPushButton*>(
            QStringLiteral("btnSkip"));
        QVERIFY(skipBtn);
        QTest::mouseClick(skipBtn, Qt::LeftButton);

        QCOMPARE(applySpy.count(), 0);
        QCOMPARE(openSetupSpy.count(), 0);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    // ── 3b. Continue-without-VAX (Scenario B) — no payload, Accepted ───
    void continueWithoutVaxAcceptsWithoutSignal_scenarioB()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsNoCables, {});

        QSignalSpy applySpy(&dlg, &VaxFirstRunDialog::applySuggested);

        auto* skipBtn = dlg.findChild<QPushButton*>(
            QStringLiteral("btnSkip"));
        QVERIFY(skipBtn);
        QTest::mouseClick(skipBtn, Qt::LeftButton);

        QCOMPARE(applySpy.count(), 0);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    // ── 4. Customize (Scenario A): openSetupAudioPage("Digital modes") + Rejected ──
    void customizeEmitsOpenSetupAndRejects()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsCablesFound,
                              windowsCablesPayload());

        QSignalSpy applySpy(&dlg, &VaxFirstRunDialog::applySuggested);
        QSignalSpy openSetupSpy(&dlg,
                                &VaxFirstRunDialog::openSetupAudioPage);

        auto* customizeBtn = dlg.findChild<QPushButton*>(
            QStringLiteral("btnCustomize"));
        QVERIFY(customizeBtn);
        QTest::mouseClick(customizeBtn, Qt::LeftButton);

        QCOMPARE(openSetupSpy.count(), 1);
        QCOMPARE(openSetupSpy.at(0).at(0).toString(), QStringLiteral("Digital modes"));
        QCOMPARE(applySpy.count(), 0);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Rejected));
    }

    // ── 5. Install URL (Scenario B) — openInstallUrl carries the URL ────
    void installCardEmitsInstallUrl_scenarioB()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsNoCables, {});

        QSignalSpy urlSpy(&dlg, &VaxFirstRunDialog::openInstallUrl);

        const QString wantedName = QStringLiteral("btnInstall_%1")
            .arg(static_cast<int>(VirtualCableProduct::VbCable));
        auto* vbCableBtn = dlg.findChild<QPushButton*>(wantedName);
        QVERIFY2(vbCableBtn, qPrintable(QStringLiteral(
            "Could not find install button with objectName %1")
            .arg(wantedName)));

        QTest::mouseClick(vbCableBtn, Qt::LeftButton);

        QCOMPARE(urlSpy.count(), 1);
        const QString got = urlSpy.takeFirst().at(0).toString();
        QCOMPARE(got, VirtualCableDetector::installUrl(
                          VirtualCableProduct::VbCable));
    }

    // ── 6. Escape dismisses as Rejected, no signals ─────────────────────
    void escapeDismissesAsRejected()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsCablesFound,
                              windowsCablesPayload());
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));

        QSignalSpy applySpy(&dlg, &VaxFirstRunDialog::applySuggested);
        QSignalSpy openSetupSpy(&dlg,
                                &VaxFirstRunDialog::openSetupAudioPage);
        QSignalSpy urlSpy(&dlg, &VaxFirstRunDialog::openInstallUrl);

        QTest::keyClick(&dlg, Qt::Key_Escape);

        QCOMPARE(applySpy.count(), 0);
        QCOMPARE(openSetupSpy.count(), 0);
        QCOMPARE(urlSpy.count(), 0);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Rejected));
    }

    // ── 7a. Got-it (Scenario C) — no payload, Accepted ──────────────────
    void gotItAcceptsWithoutSignal_scenarioC()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::MacNative, {});

        QSignalSpy applySpy(&dlg, &VaxFirstRunDialog::applySuggested);

        auto* gotItBtn = dlg.findChild<QPushButton*>(
            QStringLiteral("btnGotIt"));
        QVERIFY(gotItBtn);
        QTest::mouseClick(gotItBtn, Qt::LeftButton);

        QCOMPARE(applySpy.count(), 0);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    // ── 7b. Got-it (Scenario D) — no payload, Accepted ──────────────────
    void gotItAcceptsWithoutSignal_scenarioD()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::LinuxNative, {});

        QSignalSpy applySpy(&dlg, &VaxFirstRunDialog::applySuggested);

        auto* gotItBtn = dlg.findChild<QPushButton*>(
            QStringLiteral("btnGotIt"));
        QVERIFY(gotItBtn);
        QTest::mouseClick(gotItBtn, Qt::LeftButton);

        QCOMPARE(applySpy.count(), 0);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    // ── 8. Rescan (Scenario E) apply emits suggested bindings ──────────
    void rescanApplyEmitsBindings_scenarioE()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::RescanNewCables,
                              rescanNewCablesPayload());

        QSignalSpy applySpy(&dlg,
                            &VaxFirstRunDialog::applySuggested);

        auto* applyBtn = dlg.findChild<QPushButton*>(
            QStringLiteral("btnApplySuggested"));
        QVERIFY(applyBtn);
        QTest::mouseClick(applyBtn, Qt::LeftButton);

        QCOMPARE(applySpy.count(), 1);
        const auto args = applySpy.takeFirst();
        const auto bindings =
            args.at(0).value<QVector<QPair<int, QString>>>();
        QCOMPARE(bindings.size(), 2);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    // ── 9. Device names with HTML special chars are escaped ────────────
    //
    // Regression test for I2: makeDetRow() renders the device-name QLabel
    // as Qt::RichText, so raw `&`, `<`, `>` in DetectedCable::deviceName
    // (realistic — e.g. FlexRadio uses ampersands in "FLEX-6500 DAX RX1
    // IN & OUT") would corrupt rendering. Confirm the label text() has
    // been HTML-escaped before being handed to Qt.
    void deviceNameHtmlIsEscaped()
    {
        QVector<DetectedCable> payload;
        payload.push_back({VirtualCableProduct::VbCable,
                           QStringLiteral("A & B <C>"),
                           /*isInput=*/false, 0});
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsCablesFound, payload);

        const auto labels = dlg.findChildren<QLabel*>();
        bool found = false;
        for (const auto* label : labels) {
            if (label->textFormat() == Qt::RichText
                && label->text().contains(QStringLiteral("&amp;"))
                && label->text().contains(QStringLiteral("&lt;C&gt;"))) {
                found = true;
                break;
            }
        }
        QVERIFY2(found,
                 "Device name with HTML special chars was not escaped"
                 " before rich-text injection");
    }

    // ── 10. MacNative body reflects m_detected (regression for "always
    //       Ready" bug).  When the HAL plugin is NOT detected the dialog
    //       must say so instead of claiming all 4 channels are ready.
    void macNativeBody_showsNotDetectedWhenHalMissing()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::MacNative, {});

        bool sawWarning = false;
        bool sawAllReadyLie = false;
        for (const auto* label : dlg.findChildren<QLabel*>()) {
            const QString t = label->text();
            if (t.contains(QStringLiteral("not detected"),
                           Qt::CaseInsensitive)) {
                sawWarning = true;
            }
            if (t.contains(QStringLiteral("All 4 VAX channels are ready"),
                           Qt::CaseInsensitive)) {
                sawAllReadyLie = true;
            }
        }
        QVERIFY2(sawWarning,
                 "Expected MacNative body to surface a 'not detected' warning"
                 " when m_detected is empty");
        QVERIFY2(!sawAllReadyLie,
                 "MacNative body must not claim all 4 channels are ready"
                 " when the HAL plugin was not detected");
    }

    // ── 11. MacNative body claims "ready" only when HAL is live ────────
    void macNativeBody_showsAllReadyWhenHalFullyDetected()
    {
        QVector<DetectedCable> payload;
        for (int slot = 1; slot <= 4; ++slot) {
            payload.push_back({VirtualCableProduct::NereusSdrVax,
                               QStringLiteral("NereusSDR VAX %1").arg(slot),
                               /*isInput=*/true, 0});
        }

        VaxFirstRunDialog dlg(FirstRunScenario::MacNative, payload);

        bool sawAllReady = false;
        for (const auto* label : dlg.findChildren<QLabel*>()) {
            if (label->text().contains(
                    QStringLiteral("All 4 VAX channels are ready"),
                    Qt::CaseInsensitive)) {
                sawAllReady = true;
                break;
            }
        }
        QVERIFY2(sawAllReady,
                 "Expected MacNative body to claim 'All 4 VAX channels are"
                 " ready to use' when all 4 HAL devices were detected");
    }

    // ── 12. R-AUD-03: a cable added while open shows, no Rescan ───────
    void liveCableAddAndRemove_scenarioB()
    {
        EngineRig rig;
        rig.start();
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsNoCables, {});
        dlg.setAudioEngine(rig.engine.get());
        QCOMPARE(dlg.scenarioForTest(), FirstRunScenario::WindowsNoCables);
        QVERIFY(dlg.findChild<QPushButton*>(QStringLiteral("btnApplySuggested")) == nullptr);

        rig.native->addDevice(playback(AudioBackendId::Wasapi, QStringLiteral("cable-a-uid"), kCableA));
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(dlg.scenarioForTest(), FirstRunScenario::WindowsCablesFound, 1000);
        QCOMPARE(renderEnds(dlg), QStringList{kCableA});
        QCOMPARE(dlg.suggestedBindingsForTest().size(), 1);
        QCOMPARE(dlg.suggestedBindingsForTest().at(0).second, kCableA);
        QTRY_VERIFY_WITH_TIMEOUT(
            dlg.findChild<QPushButton*>(QStringLiteral("btnApplySuggested")) != nullptr, 1000);

        rig.native->removeDevice(QStringLiteral("cable-a-uid"), AudioDeviceDirection::Output);
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(dlg.scenarioForTest(), FirstRunScenario::WindowsNoCables, 1000);
        QVERIFY(dlg.detectedForTest().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(
            dlg.findChild<QPushButton*>(QStringLiteral("btnApplySuggested")) == nullptr, 1000);
        QVERIFY(dlg.findChild<QPushButton*>(QStringLiteral("btnRescanNow")) != nullptr);
    }

    // ── 13. R-AUD-03: Scenario E lists a cable added live as new ──────
    void liveCableAdd_scenarioE()
    {
        EngineRig rig;
        rig.native->addDevice(playback(AudioBackendId::Wasapi, QStringLiteral("cable-a-uid"), kCableA));
        rig.native->addDevice(playback(AudioBackendId::Wasapi, QStringLiteral("cable-c-uid"), kCableC));
        rig.start();
        // CABLE-C is the new one; CABLE-A was there before.
        QVector<DetectedCable> given;
        given.push_back({VirtualCableProduct::VbCableC, kCableC, false, 0});
        VaxFirstRunDialog dlg(FirstRunScenario::RescanNewCables, given);
        dlg.setAudioEngine(rig.engine.get());
        QCOMPARE(renderEnds(dlg), QStringList{kCableC});

        rig.native->addDevice(playback(AudioBackendId::Wasapi, QStringLiteral("cable-d-uid"), kCableD));
        rig.native->postNotice(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(renderEnds(dlg), (QStringList{kCableC, kCableD}), 1000);
        QCOMPARE(dlg.scenarioForTest(), FirstRunScenario::RescanNewCables);
        QCOMPARE(dlg.suggestedBindingsForTest().size(), 2);
    }

    // ── 14. R-AUD-06: "Rescan now" rescans the older drivers ───────────
    void rescanNowRescansOlderDrivers_scenarioB()
    {
        EngineRig rig;
        rig.start();
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsNoCables, {});
        dlg.setAudioEngine(rig.engine.get());

        // The older drivers see a cable only once they list again.
        rig.older->addDevice(playback(AudioBackendId::PortAudio, QStringLiteral("pa-cable-a"), kCableA));
        QSignalSpy rescanned(rig.engine->catalogue(), &IAudioDeviceCatalog::olderDriversRescanned);
        auto* rescan = dlg.findChild<QPushButton*>(QStringLiteral("btnRescanNow"));
        QVERIFY(rescan != nullptr);
        QTest::mouseClick(rescan, Qt::LeftButton);
        // The rescan runs off the GUI thread; the catalogue says when.
        QVERIFY(rescanned.count() == 1 || rescanned.wait(2000));
        QCOMPARE(rig.older->rescanCount(), 1);
        QCOMPARE(rig.native->rescanCount(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(dlg.scenarioForTest(), FirstRunScenario::WindowsCablesFound, 1000);
        QCOMPARE(renderEnds(dlg), QStringList{kCableA});
        QCOMPARE(dlg.suggestedBindingsForTest().at(0).second, kCableA);
    }
};

// The applySuggested signal carries QVector<QPair<int, QString>>; the
// inner QPair needs to be wrapped in a typedef before Q_DECLARE_METATYPE
// can register it (the macro can't parse unparenthesized template commas).
using VaxBindingList = QVector<QPair<int, QString>>;
Q_DECLARE_METATYPE(VaxBindingList)

QTEST_MAIN(TstVaxFirstRunDialog)
#include "tst_vax_first_run_dialog.moc"
