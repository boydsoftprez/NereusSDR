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
//   4. Scenario A Customize — openSetupAudioTab emitted, Rejected.
//   5. Scenario B Install-URL — openInstallUrl carries the vendor URL.
//   6. Escape key — Rejected, no signals.
//   7. Scenario C/D Got-it — no payload, Accepted.
// =================================================================

#include <QtTest/QtTest>

#include <QKeyEvent>
#include <QLabel>
#include <QPair>
#include <QPushButton>
#include <QSignalSpy>
#include <QVector>

#include "core/audio/VirtualCableDetector.h"
#include "gui/VaxFirstRunDialog.h"

using namespace NereusSDR;

namespace {

// Shared detection payload for Scenarios A and E. 4 VB-Audio cables,
// input + output sides each. The dialog only surfaces input-side rows.
QVector<DetectedCable> windowsCablesPayload()
{
    QVector<DetectedCable> v;
    v.push_back({VirtualCableProduct::VbCableA,
                 QStringLiteral("CABLE-A Input (VB-Audio Virtual Cable)"),
                 true, 0});
    v.push_back({VirtualCableProduct::VbCableA,
                 QStringLiteral("CABLE-A Output (VB-Audio Virtual Cable)"),
                 false, 0});
    v.push_back({VirtualCableProduct::VbCableB,
                 QStringLiteral("CABLE-B Input (VB-Audio Virtual Cable)"),
                 true, 0});
    v.push_back({VirtualCableProduct::VbCableC,
                 QStringLiteral("CABLE-C Input (VB-Audio Virtual Cable)"),
                 true, 0});
    return v;
}

QVector<DetectedCable> rescanNewCablesPayload()
{
    QVector<DetectedCable> v;
    v.push_back({VirtualCableProduct::VbCableC,
                 QStringLiteral("CABLE-C Input (VB-Audio Virtual Cable)"),
                 true, 0});
    v.push_back({VirtualCableProduct::VbCableD,
                 QStringLiteral("CABLE-D Input (VB-Audio Virtual Cable)"),
                 true, 0});
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
                                &VaxFirstRunDialog::openSetupAudioTab);

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
                                &VaxFirstRunDialog::openSetupAudioTab);

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

    // ── 4. Customize (Scenario A) — openSetupAudioTab + Rejected ────────
    void customizeEmitsOpenSetupAndRejects()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsCablesFound,
                              windowsCablesPayload());

        QSignalSpy applySpy(&dlg, &VaxFirstRunDialog::applySuggested);
        QSignalSpy openSetupSpy(&dlg,
                                &VaxFirstRunDialog::openSetupAudioTab);

        auto* customizeBtn = dlg.findChild<QPushButton*>(
            QStringLiteral("btnCustomize"));
        QVERIFY(customizeBtn);
        QTest::mouseClick(customizeBtn, Qt::LeftButton);

        QCOMPARE(openSetupSpy.count(), 1);
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
                                &VaxFirstRunDialog::openSetupAudioTab);
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
                           /*isInput=*/true, 0});
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
};

// The applySuggested signal carries QVector<QPair<int, QString>>; the
// inner QPair needs to be wrapped in a typedef before Q_DECLARE_METATYPE
// can register it (the macro can't parse unparenthesized template commas).
using VaxBindingList = QVector<QPair<int, QString>>;
Q_DECLARE_METATYPE(VaxBindingList)

QTEST_MAIN(TstVaxFirstRunDialog)
#include "tst_vax_first_run_dialog.moc"
