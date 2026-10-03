// SPDX-License-Identifier: GPL-2.0-or-later
// NereusSDR-original production-widget tests for WDSP 2.10 NNR controls.

#include <QtTest/QtTest>

#include "core/dsp/NnrSettings.h"
#include "core/AppSettings.h"
#include "gui/widgets/DspParamPopup.h"
#include "gui/widgets/NnrControls.h"
#include "gui/widgets/RxDashboard.h"
#include "gui/widgets/StatusBadge.h"
#include "gui/widgets/VfoWidget.h"
#include "gui/MainWindow.h"
#include "OperatorWording.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QComboBox>
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSignalSpy>
#include <QToolButton>

using namespace NereusSDR;

namespace {

template<typename T>
T* control(NnrControls& controls, const char* name)
{
    auto* result = controls.findChild<T*>(QString::fromLatin1(name));
    Q_ASSERT(result);
    return result;
}

QPushButton* buttonWithText(QWidget& parent, const QString& text)
{
    for (auto* button : parent.findChildren<QPushButton*>()) {
        if (button->text() == text) return button;
    }
    return nullptr;
}

bool captureIfRequested(QWidget& widget, const QString& fileName,
                        const QSize& minimumSize)
{
    const QString directory = qEnvironmentVariable("NEREUS_DSP_UI_CAPTURE_DIR");
    if (directory.isEmpty()) {
        return true;
    }
    if (!QDir().mkpath(directory)) {
        return false;
    }
    widget.adjustSize();
    widget.resize(qMax(widget.width(), minimumSize.width()),
                  qMax(widget.height(), minimumSize.height()));
    widget.show();
    QApplication::processEvents();
    return widget.grab().save(QDir(directory).filePath(fileName), "PNG");
}

} // namespace

class TestNnrControls final : public QObject
{
    Q_OBJECT

private slots:
    void editsAllNineAcceptedPropertiesWithFractionalPrecision();
    void refusedEditSnapsBackToAcceptedReadback();
    void resetPreservesModelAndNrSelection();
    void contextOpeningDoesNotEnableNnr();
    void refusedNrChoiceSnapsBackAndSaysWhy();
    void refusedNrChoiceIsShownOnceWhereItWasAsked();
    void bindingStaysWithOpenerAndInvalidatesOnDestruction();
    void stationSessionChangeInvalidatesBinding();
    void diagnosticActionIsTemporaryAndRequiresReadyRuntime();
    void diagnosticModesAndTuningHelpDescribeWdspBehavior();
    void compactAdvancedExpansionPersistsAsGuiPreference();
    void stepBackNoticeExplainsAndTryAgainAsksForTheSavedChoice();
    void stepBackIndicatorsOnTheVfoFlagAndDashboard();
    void choosingAnotherModelAsksToTryAgainOnce();
    void remoteWindowsNameTheCoreComputer();
};

namespace {
const QString kStandardOnlyText = QStringLiteral(
    "Noise reduction is using the Standard model. This computer could not keep up with Premium.");
const QString kOffText = QStringLiteral(
    "Noise reduction was turned off. This computer could not keep up.");
}

// R-R3-40: while the Core holds the receiver below the saved choice, the
// controls say why in plain words and offer "Try again"; choosing the model
// already shown counts as asking again.
void TestNnrControls::stepBackNoticeExplainsAndTryAgainAsksForTheSavedChoice()
{
    SliceModel slice(9);
    slice.setNnrModelSlot(1);
    NnrControls controls(nullptr, &slice, NnrControls::Presentation::Compact);
    auto* row = control<QWidget>(controls, "nnrLimitRow");
    auto* notice = control<QLabel>(controls, "nnrLimitNotice");
    auto* tryAgain = control<QPushButton>(controls, "nnrTryAgainButton");
    auto* model = control<QComboBox>(controls, "nnrModelCombo");
    QVERIFY(!row->isVisibleTo(&controls));
    QCOMPARE(tryAgain->text(), QStringLiteral("Try again"));
    QSignalSpy retry(&slice, &SliceModel::nnrRetryRequested);

    emit model->activated(model->findData(1));   // no limit: nothing to ask
    QCOMPARE(retry.count(), 0);

    slice.setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
    QVERIFY(row->isVisibleTo(&controls));
    QCOMPARE(notice->text(), kStandardOnlyText);
    QCOMPARE(model->currentData().toInt(), 1);   // still the saved choice
    tryAgain->click();
    QCOMPARE(retry.count(), 1);
    emit model->activated(model->findData(1));   // Premium again, same index
    QCOMPARE(retry.count(), 2);
    QCOMPARE(slice.nnrModelSlot(), 1);

    slice.setNnrLimit(static_cast<int>(NnrLimit::Off));
    QCOMPARE(notice->text(), kOffText);
    QVERIFY(captureIfRequested(controls, QStringLiteral("nnr-step-back-compact.png"),
                               QSize(420, 360)));
    slice.setNnrLimit(static_cast<int>(NnrLimit::None));
    QVERIFY(!row->isVisibleTo(&controls));
}

void TestNnrControls::stepBackIndicatorsOnTheVfoFlagAndDashboard()
{
    SliceModel slice(10);
    slice.setActiveNr(NrSlot::NNR);
    VfoWidget vfo;
    vfo.setSlice(&slice);
    auto* indicator = vfo.findChild<QLabel*>(QStringLiteral("vfoNnrLimitIndicator"));
    auto* nnr = buttonWithText(vfo, QStringLiteral("NNR"));
    QVERIFY(indicator);
    QVERIFY(nnr);
    // The flag's DSP tab may not be the page shown, so this checks the
    // indicator's own shown/hidden state.
    const QString normalTip = nnr->toolTip();
    QVERIFY(indicator->isHidden());

    RxDashboard dashboard;
    dashboard.bindSlice(&slice);
    StatusBadge* nrBadge = dashboard.badgeForRung(8);
    QVERIFY(nrBadge);
    QCOMPARE(nrBadge->variant(), StatusBadge::Variant::On);

    slice.setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
    QVERIFY(!indicator->isHidden());
    QCOMPARE(indicator->toolTip(), kStandardOnlyText);
    QCOMPARE(nnr->toolTip(), kStandardOnlyText);
    QCOMPARE(nrBadge->variant(), StatusBadge::Variant::Warn);
    QCOMPARE(nrBadge->toolTip(), kStandardOnlyText);

    slice.setNnrLimit(static_cast<int>(NnrLimit::Off));
    QCOMPARE(nnr->toolTip(), kOffText);
    QCOMPARE(nrBadge->toolTip(), kOffText);

    slice.setNnrLimit(static_cast<int>(NnrLimit::None));
    QVERIFY(indicator->isHidden());
    QCOMPARE(nnr->toolTip(), normalTip);
    QCOMPARE(nrBadge->variant(), StatusBadge::Variant::On);

    // A new slice on the flag brings its own state; the old one is let go.
    SliceModel other(11);
    other.setNnrLimit(static_cast<int>(NnrLimit::Off));
    vfo.setSlice(&other);
    QVERIFY(!indicator->isHidden());
    slice.setNnrLimit(static_cast<int>(NnrLimit::None));
    QVERIFY(!indicator->isHidden());
}

// R-R3-40: picking a different model while limited is one request to try
// again, though the combo signals both a changed index and an activation.
// With no Core to clear the limit at once (a remote GUI waits for the Core)
// the second signal must not send a second request.
void TestNnrControls::choosingAnotherModelAsksToTryAgainOnce()
{
    SliceModel slice(12);
    slice.setNnrModelSlot(1);
    NnrControls controls(nullptr, &slice, NnrControls::Presentation::Compact);
    auto* model = control<QComboBox>(controls, "nnrModelCombo");
    slice.setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
    QSignalSpy retry(&slice, &SliceModel::nnrRetryRequested);

    const int standard = model->findData(0);
    model->setCurrentIndex(standard);   // what the popup does for a pick
    emit model->activated(standard);
    QCOMPARE(retry.count(), 1);
    QCOMPARE(slice.nnrModelSlot(), 0);

    QCoreApplication::processEvents();
    emit model->activated(standard);    // the same model, a later pick
    QCOMPARE(retry.count(), 2);
}

// R-R3-40: in a remote window (the limit arrived from the Core through the
// station mirror) every step-back text names the Core computer.
void TestNnrControls::remoteWindowsNameTheCoreComputer()
{
    const QString coreStandard = QStringLiteral(
        "Noise reduction is using the Standard model. The Core computer could not keep up with Premium.");
    const QString coreOff = QStringLiteral(
        "Noise reduction was turned off. The Core computer could not keep up.");
    SliceModel slice(13);
    slice.setActiveNr(NrSlot::NNR);
    slice.setNnrModelSlot(1);
    NnrControls controls(nullptr, &slice, NnrControls::Presentation::Compact);
    auto* notice = control<QLabel>(controls, "nnrLimitNotice");
    VfoWidget vfo;
    vfo.setSlice(&slice);
    auto* indicator = vfo.findChild<QLabel*>(QStringLiteral("vfoNnrLimitIndicator"));
    auto* nnr = buttonWithText(vfo, QStringLiteral("NNR"));
    QVERIFY(indicator);
    QVERIFY(nnr);
    RxDashboard dashboard;
    dashboard.bindSlice(&slice);
    StatusBadge* nrBadge = dashboard.badgeForRung(8);
    QVERIFY(nrBadge);

    QVERIFY(slice.applyStationNnrDiagnostic("nnrLimit", 1));
    QCOMPARE(notice->text(), coreStandard);
    QCOMPARE(indicator->toolTip(), coreStandard);
    QCOMPARE(nnr->toolTip(), coreStandard);
    QCOMPARE(nrBadge->toolTip(), coreStandard);
    QCOMPARE(slice.nnrStatus(), coreStandard);

    QVERIFY(slice.applyStationNnrDiagnostic("nnrLimit", 2));
    QCOMPARE(notice->text(), coreOff);
    QCOMPARE(indicator->toolTip(), coreOff);
    QCOMPARE(nnr->toolTip(), coreOff);
    QCOMPARE(nrBadge->toolTip(), coreOff);
}

void TestNnrControls::editsAllNineAcceptedPropertiesWithFractionalPrecision()
{
    SliceModel slice(3);
    NnrControls controls(nullptr, &slice, NnrControls::Presentation::Full);

    auto* model = control<QComboBox>(controls, "nnrModelCombo");
    auto* floor = control<QDoubleSpinBox>(controls, "nnrMaskFloorSpin");
    auto* position = control<QComboBox>(controls, "nnrPositionCombo");
    auto* alpha = control<QDoubleSpinBox>(controls, "nnrAlphaSpin");
    auto* knee = control<QDoubleSpinBox>(controls, "nnrAlphaKneeSpin");
    auto* tau = control<QDoubleSpinBox>(controls, "nnrTauSpin");
    auto* gain = control<QDoubleSpinBox>(controls, "nnrMaxGainSpin");
    auto* attack = control<QDoubleSpinBox>(controls, "nnrAttackSpin");
    auto* release = control<QDoubleSpinBox>(controls, "nnrReleaseSpin");

    model->setCurrentIndex(model->findData(1));
    floor->setValue(-31.25);
    position->setCurrentIndex(position->findData(static_cast<int>(NrPosition::PreAgc)));
    alpha->setValue(1.23);
    knee->setValue(12.3);
    tau->setValue(3.25);
    gain->setValue(14.5);
    attack->setValue(23.4);
    release->setValue(87.6);

    QCOMPARE(slice.nnrModelSlot(), 1);
    QCOMPARE(slice.nnrMaskFloorDb(), -31.25);
    QCOMPARE(slice.nnrPosition(), NrPosition::PreAgc);
    QCOMPARE(slice.nnrAlpha(), 1.23);
    QCOMPARE(slice.nnrAlphaKneeDb(), 12.3);
    QCOMPARE(slice.nnrTauSeconds(), 3.25);
    QCOMPARE(slice.nnrMaxGainDb(), 14.5);
    QCOMPARE(slice.nnrAttackMs(), 23.4);
    QCOMPARE(slice.nnrReleaseMs(), 87.6);
    QCOMPARE(floor->value(), -31.25);
}

void TestNnrControls::refusedEditSnapsBackToAcceptedReadback()
{
    SliceModel slice(4);
    slice.setNnrSettingsApplier([](const NnrSettings& requested, QString* reason)
        -> std::optional<NnrSettings> {
        if (qFuzzyCompare(requested.alpha, 2.5)) {
            if (reason) *reason = QStringLiteral("That live model is unavailable.");
            return std::nullopt;
        }
        return requested;
    });
    NnrControls controls(nullptr, &slice, NnrControls::Presentation::Full);
    auto* alpha = control<QDoubleSpinBox>(controls, "nnrAlphaSpin");

    alpha->setValue(2.5);

    QCOMPARE(slice.nnrAlpha(), 1.0);
    QCOMPARE(alpha->value(), 1.0);
    QVERIFY(control<QLabel>(controls, "nnrStatusReadback")->text().contains(
        QStringLiteral("unavailable"), Qt::CaseInsensitive));
}

void TestNnrControls::resetPreservesModelAndNrSelection()
{
    SliceModel slice(5);
    slice.setNnrModelSlot(1);
    slice.setNnrAlpha(3.2);
    slice.setNnrMaskFloorDb(-42.25);
    slice.setActiveNr(NrSlot::NNR);
    NnrControls controls(nullptr, &slice, NnrControls::Presentation::Full);

    control<QPushButton>(controls, "nnrResetButton")->click();

    const NnrSettings defaults;
    QCOMPARE(slice.nnrModelSlot(), 1);
    QCOMPARE(slice.activeNr(), NrSlot::NNR);
    QCOMPARE(slice.nnrMaskFloorDb(), defaults.maskFloorDb);
    QCOMPARE(slice.nnrPosition(), defaults.position);
    QCOMPARE(slice.nnrAlpha(), defaults.alpha);
    QCOMPARE(slice.nnrAlphaKneeDb(), defaults.alphaKneeDb);
    QCOMPARE(slice.nnrTauSeconds(), defaults.tauSeconds);
    QCOMPARE(slice.nnrMaxGainDb(), defaults.maxGainDb);
    QCOMPARE(slice.nnrAttackMs(), defaults.attackMs);
    QCOMPARE(slice.nnrReleaseMs(), defaults.releaseMs);
}

void TestNnrControls::contextOpeningDoesNotEnableNnr()
{
    SliceModel slice(6);
    VfoWidget vfo;
    vfo.setSlice(&slice);
    auto* nnr = buttonWithText(vfo, QStringLiteral("NNR"));
    QVERIFY(nnr);
    QCOMPARE(slice.activeNr(), NrSlot::Off);
    QSignalSpy selectionSpy(&slice, &SliceModel::activeNrChanged);

    QVERIFY(QMetaObject::invokeMethod(nnr, "customContextMenuRequested",
                                      Qt::DirectConnection, Q_ARG(QPoint, QPoint(1, 1))));

    QCOMPARE(slice.activeNr(), NrSlot::Off);
    QCOMPARE(selectionSpy.count(), 0);
    QVERIFY(vfo.findChild<DspParamPopup*>());
}

// Fix wave I3: a noise reducer the receiver refuses (NR3 on a Core with no
// NR3 model) leaves the VFO's buttons as the receiver has them and shows the
// plain reason, whatever was on before.
void TestNnrControls::refusedNrChoiceSnapsBackAndSaysWhy()
{
    SliceModel slice(7);
    const QString why =
        QStringLiteral("No NR3 model file was found on this Core, so NR3 cannot run.");
    slice.setNrSelectionApplier([why](NrSlot requested, QString* reason) {
        if (requested != NrSlot::NR3) { return true; }
        if (reason) { *reason = why; }
        return false;
    });
    VfoWidget vfo;
    vfo.setSlice(&slice);
    auto* nr2 = buttonWithText(vfo, QStringLiteral("NR2"));
    auto* nr3 = buttonWithText(vfo, QStringLiteral("NR3"));
    QVERIFY(nr2 && nr3);
    QSignalSpy refused(&slice, &SliceModel::nrSelectionRefused);

    nr3->click();
    QCOMPARE(slice.activeNr(), NrSlot::Off);
    QVERIFY(!nr3->isChecked());
    QCOMPARE(refused.count(), 1);
    QCOMPARE(refused.constFirst().at(0).toString(), why);
    QCOMPARE(slice.nnrLastError(), why);
    QCOMPARE(vfo.nrRefusalForTest(), why);

    nr2->click();
    QCOMPARE(slice.activeNr(), NrSlot::NR2);
    nr3->click();
    QCOMPARE(slice.activeNr(), NrSlot::NR2);
    QVERIFY(nr2->isChecked());
    QVERIFY(!nr3->isChecked());
    QCOMPARE(refused.count(), 2);
}

// Follow-up item 3 (R-R3-21): one refused click, one message, at the control
// that was clicked. A choice from the DSP > NR menu is answered by the menu
// (the text it returns for its notice) and not by the VFO flag as well; a
// flag click is answered at the flag. Both in plain words.
void TestNnrControls::refusedNrChoiceIsShownOnceWhereItWasAsked()
{
    SliceModel slice(7);
    const QString why =
        QStringLiteral("No NR3 model file was found on this Core, so NR3 cannot run.");
    slice.setNrSelectionApplier([why](NrSlot requested, QString* reason) {
        if (requested == NrSlot::NNR) {
            if (reason) { *reason = QStringLiteral("This station session does not support NNR."); }
            return false;
        }
        if (requested != NrSlot::NR3) { return true; }
        if (reason) { *reason = why; }
        return false;
    });
    VfoWidget vfo;
    vfo.setSlice(&slice);
    auto* nr3 = buttonWithText(vfo, QStringLiteral("NR3"));
    QVERIFY(nr3);

    // From the menu: the menu has the reason; the flag only follows.
    const QString fromMenu = MainWindow::applyNrMenuChoice(&slice, NrSlot::NR3);
    QCOMPARE(fromMenu, why);
    QVERIFY(OperatorWording::isPlain(fromMenu));
    QCOMPARE(slice.activeNr(), NrSlot::Off);
    QVERIFY(!nr3->isChecked());
    QVERIFY(vfo.nrRefusalForTest().isEmpty());

    // A reason naming internal terms is shown in user words.
    const QString nnr = MainWindow::applyNrMenuChoice(&slice, NrSlot::NNR);
    QVERIFY(!nnr.isEmpty());
    QVERIFY2(OperatorWording::isPlain(nnr), qPrintable(nnr));
    QVERIFY(vfo.nrRefusalForTest().isEmpty());

    // An accepted menu choice has nothing to say.
    QVERIFY(MainWindow::applyNrMenuChoice(&slice, NrSlot::NR2).isEmpty());
    QCOMPARE(slice.activeNr(), NrSlot::NR2);

    // From the flag: the flag says why.
    nr3->click();
    QCOMPARE(slice.activeNr(), NrSlot::NR2);
    QVERIFY(!nr3->isChecked());
    QCOMPARE(vfo.nrRefusalForTest(), why);

    // A later menu refusal does not leave the flag's old reason standing
    // as if the flag had been refused again; the next flag click clears it.
    auto* nr2 = buttonWithText(vfo, QStringLiteral("NR2"));
    QVERIFY(nr2);
    nr2->click();
    QCOMPARE(slice.activeNr(), NrSlot::Off);
    QVERIFY(vfo.nrRefusalForTest().isEmpty());
}

void TestNnrControls::bindingStaysWithOpenerAndInvalidatesOnDestruction()
{
    RadioModel radio;
    const int firstId = radio.addSlice();
    const int secondId = radio.addSlice();
    QVERIFY(firstId >= 0);
    QVERIFY(secondId >= 0);
    SliceModel* opener = radio.sliceById(firstId);
    QVERIFY(opener);
    NnrControls controls(&radio, opener, NnrControls::Presentation::Compact);
    QSignalSpy invalidated(&controls, &NnrControls::bindingInvalidated);

    radio.setActiveSlice(1);
    QCOMPARE(controls.boundSlice(), opener);
    QCOMPARE(controls.boundSliceId(), firstId);

    radio.removeSlice(firstId);
    QTRY_COMPARE(invalidated.count(), 1);
    QVERIFY(!controls.bindingValid());
}

void TestNnrControls::stationSessionChangeInvalidatesBinding()
{
    RadioModel radio;
    SliceModel slice(7);
    NnrControls controls(&radio, &slice, NnrControls::Presentation::Compact);
    QSignalSpy invalidated(&controls, &NnrControls::bindingInvalidated);

    radio.reportStationLinkStateChanged();

    QCOMPARE(invalidated.count(), 1);
    QVERIFY(!controls.bindingValid());
    QVERIFY(!control<QDoubleSpinBox>(controls, "nnrAlphaSpin")->isEnabled());
}

void TestNnrControls::diagnosticActionIsTemporaryAndRequiresReadyRuntime()
{
    SliceModel slice(8);
    slice.setActiveNr(NrSlot::NR2);
    slice.setNnrAlpha(1.75);
    NnrControls controls(nullptr, &slice, NnrControls::Presentation::Full);
    auto* apply = control<QPushButton>(controls, "nnrApplyDiagnosticButton");
    QVERIFY(!apply->isEnabled());

    NnrDiagnostics diagnostics;
    diagnostics.available = true;
    diagnostics.ready = true;
    diagnostics.rateSupported = true;
    diagnostics.modelAvailable = {true, true};
    slice.updateNnrDiagnostics(diagnostics);
    QVERIFY(apply->isEnabled());

    auto* testMode = control<QComboBox>(controls, "nnrTestModeCombo");
    auto* outputMode = control<QComboBox>(controls, "nnrOutputModeCombo");
    testMode->setCurrentIndex(testMode->findData(2));
    outputMode->setCurrentIndex(outputMode->findData(0));
    QSignalSpy request(&slice, &SliceModel::nnrDiagnosticsRequested);
    apply->click();

    QCOMPARE(request.count(), 1);
    QCOMPARE(request.first().at(0).toInt(), 2);
    QCOMPARE(request.first().at(1).toInt(), 0);
    QCOMPARE(slice.activeNr(), NrSlot::NR2);
    QCOMPARE(slice.nnrAlpha(), 1.75);
}

void TestNnrControls::diagnosticModesAndTuningHelpDescribeWdspBehavior()
{
    SliceModel slice(9);
    NnrControls controls(nullptr, &slice, NnrControls::Presentation::Full);
    auto* testMode = control<QComboBox>(controls, "nnrTestModeCombo");
    auto* outputMode = control<QComboBox>(controls, "nnrOutputModeCombo");

    QCOMPARE(testMode->itemText(testMode->findData(0)), QStringLiteral("Network"));
    QCOMPARE(testMode->itemText(testMode->findData(1)), QStringLiteral("Identity"));
    QCOMPARE(testMode->itemText(testMode->findData(2)), QStringLiteral("Low-pass"));
    QCOMPARE(outputMode->itemText(outputMode->findData(0)), QStringLiteral("Duplicate I/Q"));
    QCOMPARE(outputMode->itemText(outputMode->findData(1)), QStringLiteral("Q zero"));
    QVERIFY(testMode->itemData(testMode->findData(1), Qt::ToolTipRole)
                .toString().contains(QStringLiteral("unchanged")));
    QVERIFY(outputMode->itemData(outputMode->findData(1), Qt::ToolTipRole)
                .toString().contains(QStringLiteral("zero")));

    QVERIFY(control<QDoubleSpinBox>(controls, "nnrAlphaSpin")
                ->toolTip().contains(QStringLiteral("deep-filter")));
    QVERIFY(control<QDoubleSpinBox>(controls, "nnrTauSpin")
                ->toolTip().contains(QStringLiteral("input-power")));
    QVERIFY(control<QLabel>(controls, "nnrStatusReadback")
                ->text().contains(QStringLiteral("Profiling unavailable")));

    NnrDiagnostics diagnostics;
    diagnostics.available = true;
    diagnostics.ready = true;
    diagnostics.running = true;
    diagnostics.rateSupported = true;
    diagnostics.actualModelSlot = 0;
    diagnostics.modelAvailable = {true, true};
    diagnostics.modelSources = {NnrModelSource::Bundled, NnrModelSource::File};
    diagnostics.dspRateHz = 48000;
    diagnostics.networkRateHz = 16000;
    diagnostics.delaySamples = 384;
    diagnostics.latencyMs = 8.0;
    diagnostics.explanation = QStringLiteral("Network processing is ready.");
    slice.updateNnrDiagnostics(diagnostics);
    QVERIFY2(captureIfRequested(controls, QStringLiteral("nnr-full-normal.png"),
                                QSize(640, 620)),
             "Could not save opt-in full NNR UI capture");
}

void TestNnrControls::compactAdvancedExpansionPersistsAsGuiPreference()
{
    constexpr auto key = "NnrControls/AdvancedExpanded";
    AppSettings& settings = AppSettings::instance();
    const bool existed = settings.contains(QString::fromLatin1(key));
    const QVariant original = settings.value(QString::fromLatin1(key));
    settings.setValue(QString::fromLatin1(key), QStringLiteral("False"));

    SliceModel firstSlice(10);
    firstSlice.setNnrAlpha(1.75);
    firstSlice.setNnrAlphaKneeDb(14.0);
    firstSlice.setNnrTauSeconds(2.5);
    NnrDiagnostics advancedDiagnostics;
    advancedDiagnostics.available = true;
    advancedDiagnostics.ready = true;
    advancedDiagnostics.running = true;
    advancedDiagnostics.rateSupported = true;
    advancedDiagnostics.actualModelSlot = 0;
    advancedDiagnostics.modelAvailable = {true, true};
    advancedDiagnostics.modelSources = {NnrModelSource::Bundled, NnrModelSource::File};
    advancedDiagnostics.dspRateHz = 48000;
    advancedDiagnostics.networkRateHz = 16000;
    advancedDiagnostics.delaySamples = 384;
    advancedDiagnostics.latencyMs = 8.0;
    advancedDiagnostics.explanation = QStringLiteral("Network processing is ready.");
    firstSlice.updateNnrDiagnostics(advancedDiagnostics);
    {
        NnrControls controls(nullptr, &firstSlice, NnrControls::Presentation::Compact);
        auto* toggle = control<QToolButton>(controls, "nnrAdvancedToggle");
        auto* advanced = controls.findChild<QWidget*>(QStringLiteral("nnrAdvancedGroup"));
        QVERIFY(advanced);
        QVERIFY(!toggle->isChecked());
        QVERIFY(advanced->isHidden());
        toggle->setChecked(true);
        QVERIFY(!advanced->isHidden());
        QVERIFY2(captureIfRequested(controls,
                                    QStringLiteral("nnr-compact-advanced.png"),
                                    QSize(600, 640)),
                 "Could not save opt-in compact NNR UI capture");
    }

    SliceModel secondSlice(11);
    {
        NnrControls controls(nullptr, &secondSlice, NnrControls::Presentation::Compact);
        auto* toggle = control<QToolButton>(controls, "nnrAdvancedToggle");
        auto* advanced = controls.findChild<QWidget*>(QStringLiteral("nnrAdvancedGroup"));
        QVERIFY(advanced);
        QVERIFY(toggle->isChecked());
        QVERIFY(!advanced->isHidden());
    }

    if (existed) {
        settings.setValue(QString::fromLatin1(key), original);
    } else {
        settings.remove(QString::fromLatin1(key));
    }
}

QTEST_MAIN(TestNnrControls)
#include "tst_nnr_controls.moc"
