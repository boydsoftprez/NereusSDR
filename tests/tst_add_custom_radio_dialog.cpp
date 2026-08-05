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
// =================================================================

#include <QComboBox>
#include <QObject>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

#include "core/HpsdrModel.h"
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
};

QTEST_MAIN(TstAddCustomRadioDialog)
#include "tst_add_custom_radio_dialog.moc"
