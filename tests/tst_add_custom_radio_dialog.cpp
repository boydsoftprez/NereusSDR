// =================================================================
// tests/tst_add_custom_radio_dialog.cpp  (NereusSDR)
// =================================================================
// no-port-check: test file is NereusSDR-original; the frmAddCustomRadio.cs
// reference above is a comment describing what is being tested, not a port.
//
// Phase 3Q Task 4 — widget-level coverage for the rebuilt Add Custom
// Radio dialog. The dialog is a NereusSDR port of Thetis
// frmAddCustomRadio.cs; this test file itself is NereusSDR-original
// (no Thetis attribution markers, no PROVENANCE row needed).
//
// Coverage:
//   1. modelDropdownContainsAutoDetectFirst — index 0 is "Auto-detect"
//   2. modelDropdownContainsAllSeventeenSkus  — all 16 SKUs present
//   3. saveOfflineButtonExists              — objectName "saveOfflineButton"
//   4. probeAndConnectButtonExists          — objectName "probeButton"
//   5. editKeepsTheRadiosReportedFirmwareAndReceivers — an edit of a
//      known radio keeps the firmware numbers and receiver count the
//      radio reported (the form has no field for them)
//   6. editToAnotherMacDropsTheOldRadiosReport — a changed MAC is another
//      radio, so the old radio's report is not carried to it
// =================================================================

#include <QComboBox>
#include <QHostAddress>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "gui/AddCustomRadioDialog.h"

using namespace NereusSDR;

class TstAddCustomRadioDialog : public QObject {
    Q_OBJECT

private slots:

    void modelDropdownContainsAutoDetectFirst()
    {
        AddCustomRadioDialog dlg;
        auto* combo = dlg.findChild<QComboBox*>(QStringLiteral("modelCombo"));
        QVERIFY(combo);
        QVERIFY2(combo->itemText(0).contains(QStringLiteral("Auto-detect"),
                                              Qt::CaseInsensitive),
                 qPrintable(QStringLiteral(
                     "Index 0 should be Auto-detect, got: %1")
                     .arg(combo->itemText(0))));
    }

    void modelDropdownContainsAllSeventeenSkus()
    {
        AddCustomRadioDialog dlg;
        auto* combo = dlg.findChild<QComboBox*>(QStringLiteral("modelCombo"));
        QVERIFY(combo);

        // 16 SKUs: HPSDRModel values FIRST+1 .. LAST-1 (i.e. 0..15).
        // displayName(HPSDRModel) from HpsdrModel.h:160-182 [v2.10.3.13].
        QStringList expected;
        for (int i = static_cast<int>(HPSDRModel::FIRST) + 1;
             i < static_cast<int>(HPSDRModel::LAST); ++i) {
            expected << QString::fromUtf8(
                displayName(static_cast<HPSDRModel>(i)));
        }

        // 17 SKUs: ANAN_G2E added in Phase E of the ANAN-G2E port (bumps LAST to 17).
        // //N1GP G2E added [v2.10.3.15]
        QCOMPARE(expected.size(), 17);  // Self-check: 17 SKUs expected

        for (const auto& name : expected) {
            int idx = combo->findText(name, Qt::MatchContains);
            QVERIFY2(idx >= 0,
                     qPrintable(QStringLiteral("Missing SKU: %1").arg(name)));
        }
    }

    void saveOfflineButtonExists()
    {
        AddCustomRadioDialog dlg;
        auto* btn =
            dlg.findChild<QPushButton*>(QStringLiteral("saveOfflineButton"));
        QVERIFY2(btn, "Expected QPushButton with objectName 'saveOfflineButton'");
    }

    void probeAndConnectButtonExists()
    {
        AddCustomRadioDialog dlg;
        auto* btn = dlg.findChild<QPushButton*>(QStringLiteral("probeButton"));
        QVERIFY2(btn, "Expected QPushButton with objectName 'probeButton'");
    }

    void editKeepsTheRadiosReportedFirmwareAndReceivers()
    {
        AddCustomRadioDialog dlg;
        dlg.setEditTarget(reportedAnvelina(), false, true);
        // Rename it, as an operator does from the radio list.
        QLineEdit* nameEdit = fieldWithPlaceholder(dlg, QStringLiteral("e.g."));
        QVERIFY(nameEdit);
        nameEdit->setText(QStringLiteral("Anvelina Pro3"));

        const RadioInfo edited = dlg.result();
        QCOMPARE(edited.name, QStringLiteral("Anvelina Pro3"));
        QCOMPARE(edited.macAddress, QStringLiteral("68:27:19:99:57:2F"));
        QCOMPARE(edited.firmwareVersion, 22);
        QCOMPARE(edited.firmwareBetaVersion, 15);
        QCOMPARE(edited.firmwareText(), QStringLiteral("22.15"));
        QCOMPARE(edited.reportedReceivers, 5);
        QCOMPARE(edited.maxReceivers, 5);
    }

    void editToAnotherMacDropsTheOldRadiosReport()
    {
        AddCustomRadioDialog dlg;
        dlg.setEditTarget(reportedAnvelina(), false, true);
        QLineEdit* macEdit = fieldWithPlaceholder(dlg, QStringLiteral("AA:BB"));
        QVERIFY(macEdit);
        macEdit->setText(QStringLiteral("40:84:32:B0:B0:8D"));

        const RadioInfo edited = dlg.result();
        QCOMPARE(edited.macAddress, QStringLiteral("40:84:32:B0:B0:8D"));
        QCOMPARE(edited.firmwareVersion, 0);
        QCOMPARE(edited.firmwareBetaVersion, 0);
        QCOMPARE(edited.reportedReceivers, 0);
    }

private:
    // An Anvelina Pro 3 as Protocol 2 discovery reports it: code version
    // 22, second number 15 (release 2.2.15), five receivers.
    static RadioInfo reportedAnvelina()
    {
        RadioInfo info;
        info.name = QStringLiteral("ANAN-7000DLE/8000DLE (OrionMkII)");
        info.address = QHostAddress(QStringLiteral("192.168.109.36"));
        info.port = 1024;
        info.macAddress = QStringLiteral("68:27:19:99:57:2F");
        info.protocol = ProtocolVersion::Protocol2;
        info.boardType = HPSDRHW::OrionMKII;
        info.modelOverride = HPSDRModel::ANVELINAPRO3;
        info.firmwareVersion = 22;
        info.firmwareBetaVersion = 15;
        info.reportedReceivers = 5;
        info.maxReceivers = 5;
        return info;
    }

    static QLineEdit* fieldWithPlaceholder(const AddCustomRadioDialog& dlg,
                                           const QString& prefix)
    {
        for (QLineEdit* edit : dlg.findChildren<QLineEdit*>()) {
            if (edit->placeholderText().startsWith(prefix)) {
                return edit;
            }
        }
        return nullptr;
    }
};

QTEST_MAIN(TstAddCustomRadioDialog)
#include "tst_add_custom_radio_dialog.moc"
