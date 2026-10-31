// no-port-check: NereusSDR-original full Compact NNR popup regression.
// 2026-10-12: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardPaths>
#include <QToolButton>
#include "core/AppSettings.h"
#include "gui/widgets/DspParamPopup.h"
#include "gui/widgets/NnrControls.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestNnrPopupBounds : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        AppSettings::setProfileOverride(QStringLiteral("codex-nnr-popup-bounds-20261012"));
    }
    void expandedCompactControlsRemainReachableWithoutChangingReceiver()
    {
        AppSettings::instance().setValue(QStringLiteral("NnrControls/AdvancedExpanded"), QStringLiteral("True"));
        SliceModel slice(0);
        slice.setNnrModelSlot(1);
        slice.setActiveNr(NrSlot::NNR);
        NnrDiagnostics runtime;
        runtime.available = true;
        runtime.ready = true;
        runtime.running = true;
        runtime.modelAvailable = {true, true};
        slice.updateNnrDiagnostics(runtime);
        QVERIFY(slice.applyStationNnrDiagnostic("nnrLoadWarning", QStringLiteral(
            "Receiver A: receive processing is falling behind. The selected noise reduction remains running. "
            "This long warning must wrap within the popup and leave both explicit actions reachable.")));
        const auto selection = slice.activeNr();
        const double alpha = slice.nnrAlpha();
        const double maskFloor = slice.nnrMaskFloorDb();
        const QRect available = QApplication::primaryScreen()->availableGeometry();
        DspParamPopup popup;
        auto* controls = new NnrControls(nullptr, &slice, NnrControls::Presentation::Compact, &popup);
        // Match the reported popup's oversize condition independently of host DPI.
        controls->setMinimumHeight(available.height() + 200);
        popup.addWidget(controls);
        popup.showAt(available.bottomLeft() + QPoint(30, -20));
        QCoreApplication::processEvents();
        qInfo() << "NNR available" << available << "popup" << popup.geometry() << "content" << controls->size();
        QVERIFY(available.contains(popup.geometry()));
        auto* scroll = popup.findChild<QScrollArea*>();
        QVERIFY(scroll);
        const QStringList names{QStringLiteral("nnrLoadWarningNotice"), QStringLiteral("nnrUseStandardButton"),
            QStringLiteral("nnrTurnOffButton"), QStringLiteral("nnrModelCombo"), QStringLiteral("nnrMaskFloorSpin"),
            QStringLiteral("nnrPositionCombo"), QStringLiteral("nnrAlphaSpin"),
            QStringLiteral("nnrAlphaKneeSpin"), QStringLiteral("nnrTauSpin"),
            QStringLiteral("nnrMaxGainSpin"), QStringLiteral("nnrAttackSpin"),
            QStringLiteral("nnrReleaseSpin"), QStringLiteral("nnrMaskFloorSlider"),
            QStringLiteral("nnrAdvancedToggle"), QStringLiteral("nnrTestModeCombo"),
            QStringLiteral("nnrOutputModeCombo"), QStringLiteral("nnrApplyDiagnosticButton"),
            QStringLiteral("nnrRuntimeReadback"), QStringLiteral("nnrModelsReadback"),
            QStringLiteral("nnrRateLatencyReadback"), QStringLiteral("nnrSourceReadback"),
            QStringLiteral("nnrStatusReadback"), QStringLiteral("nnrModelsButton"),
            QStringLiteral("nnrResetButton"), QStringLiteral("nnrMoreSettingsButton")};
        for (const QString& name : names) {
            QWidget* control = controls->findChild<QWidget*>(name);
            QVERIFY2(control, qPrintable(name));
            QVERIFY2(control->isVisible(), qPrintable(name));
            scroll->ensureWidgetVisible(control);
            QCoreApplication::processEvents();
            QVERIFY2(scroll->viewport()->rect().contains(control->mapTo(scroll->viewport(), control->rect().center())), qPrintable(name));
        }
        auto* more = controls->findChild<QPushButton*>(QStringLiteral("nnrMoreSettingsButton"));
        QSignalSpy moreRequested(controls, &NnrControls::openMoreSettingsRequested);
        QTest::mouseClick(more, Qt::LeftButton);
        QCOMPARE(moreRequested.count(), 1);
        QCOMPARE(slice.activeNr(), selection);
        QCOMPARE(slice.nnrAlpha(), alpha);
        QCOMPARE(slice.nnrMaskFloorDb(), maskFloor);
    }
    void advancedExpansionAfterOpeningRemainsContained()
    {
        AppSettings::instance().setValue(QStringLiteral("NnrControls/AdvancedExpanded"), QStringLiteral("False"));
        SliceModel slice(1);
        DspParamPopup popup;
        auto* controls = new NnrControls(nullptr, &slice, NnrControls::Presentation::Compact, &popup);
        popup.addWidget(controls);
        const QRect available = QApplication::primaryScreen()->availableGeometry();
        popup.showAt(available.bottomLeft() + QPoint(30, -20));
        auto* toggle = controls->findChild<QToolButton*>(QStringLiteral("nnrAdvancedToggle"));
        QVERIFY(toggle);
        QTest::mouseClick(toggle, Qt::LeftButton);
        controls->setMinimumHeight(available.height() + 200);
        QTRY_VERIFY(available.contains(popup.geometry()));
        auto* scroll = popup.findChild<QScrollArea*>();
        QVERIFY(scroll);
        QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 0);
        QVERIFY(available.contains(popup.geometry()));
        QCOMPARE(slice.activeNr(), NrSlot::Off);
    }
};
QTEST_MAIN(TestNnrPopupBounds)
#include "tst_nnr_popup_bounds.moc"
