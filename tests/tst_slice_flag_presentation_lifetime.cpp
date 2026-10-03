// =================================================================
// tests/tst_slice_flag_presentation_lifetime.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native regression for secondary VFO flags removed during a
// multi-pan layout rehome. There is no upstream AetherSDR equivalent for
// this lifecycle boundary (no port check applies).
//
// Modification history (NereusSDR):
//   2026-09-21 -- Added by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via OpenAI Codex.
//   2026-09-23 -- Rehome handler-count regression (R-R3-30) added
//                 by J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

#include <QComboBox>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QTest>

#include "core/ControlRanges.h"
#include "gui/SliceFlagPresentationBinding.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

QPushButton* buttonWithText(VfoWidget* flag, const QString& text)
{
    const QList<QPushButton*> buttons = flag->findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

QSlider* agcThresholdSlider(VfoWidget* flag)
{
    const QList<QSlider*> sliders = flag->findChildren<QSlider*>();
    for (QSlider* slider : sliders) {
        if (slider->minimum() == NereusSDR::ControlRanges::kAgcThresholdMinDb
            && slider->maximum() == NereusSDR::ControlRanges::kAgcThresholdMaxDb) {
            return slider;
        }
    }
    return nullptr;
}

QSlider* sliderWithTooltipPrefix(VfoWidget* flag, const QString& prefix)
{
    const QList<QSlider*> sliders = flag->findChildren<QSlider*>();
    for (QSlider* slider : sliders) {
        if (slider->toolTip().startsWith(prefix)) {
            return slider;
        }
    }
    return nullptr;
}

QComboBox* modeCombo(VfoWidget* flag)
{
    const QList<QComboBox*> combos = flag->findChildren<QComboBox*>();
    for (QComboBox* combo : combos) {
        if (combo->findText(QStringLiteral("USB")) >= 0
            && combo->findText(QStringLiteral("LSB")) >= 0) {
            return combo;
        }
    }
    return nullptr;
}

}  // namespace

class TestSliceFlagPresentationLifetime : public QObject {
    Q_OBJECT

private slots:
    void removingFlagDisconnectsEveryPresentationEdge()
    {
        SliceModel slice(1);
        SpectrumWidget spectrum;
        VfoWidget* flag = spectrum.addVfoWidget(slice.sliceIndex());
        QVERIFY(flag);

        const QList<QMetaObject::Connection> connections =
            wireSliceFlagPresentation(&slice, flag);
        QCOMPARE(connections.size(), 14);

        QPointer<VfoWidget> retiredFlag(flag);
        spectrum.removeVfoWidget(slice.sliceIndex());
        QVERIFY(retiredFlag.isNull());

        // QObject::disconnect(handle) returns false once Qt has already
        // retired the connection with its receiver. Check the operation,
        // not bool(handle): a stored handle can remain truthy after an
        // automatic disconnect. Consume every still-live handle before the
        // assertion so the RED test reports safely instead of dereferencing
        // the deleted flag on teardown or a later model update.
        bool allAlreadyDisconnected = true;
        for (const QMetaObject::Connection& connection : connections) {
            if (QObject::disconnect(connection)) {
                allAlreadyDisconnected = false;
            }
        }
        QVERIFY2(allAlreadyDisconnected,
                 "retired VFO flag left model presentation connections live");

        // Once a flag has gone, later mirrored/model updates must remain safe.
        slice.setRitEnabled(true);
        slice.setRitHz(125);
        slice.setXitEnabled(true);
        slice.setXitHz(-80);
        slice.setSnbEnabled(true);
        slice.setApfEnabled(true);
        slice.setApfTuneHz(950);
        slice.setMuted(true);
        slice.setAudioPan(0.75);
        slice.setSsqlEnabled(true);
        slice.setSsqlThresh(-82.0);
        slice.setAgcThreshold(-67);
        slice.setBinauralEnabled(true);
        slice.setLocked(true);

        // A layout rehome immediately creates a replacement flag for the
        // same live slice. Prove that the lifetime fix did not merely drop
        // the presentation path: representative boolean state and the exact
        // AGC-T slider from the crash still follow subsequent model updates.
        VfoWidget* replacement = spectrum.addVfoWidget(slice.sliceIndex());
        QVERIFY(replacement);
        const QList<QMetaObject::Connection> replacementConnections =
            wireSliceFlagPresentation(&slice, replacement);
        QCOMPARE(replacementConnections.size(), 14);

        slice.setRitEnabled(false);
        slice.setXitEnabled(false);
        slice.setSnbEnabled(false);
        slice.setApfEnabled(false);
        slice.setMuted(false);
        slice.setSsqlEnabled(false);
        slice.setBinauralEnabled(false);
        slice.setAudioPan(0.25);
        slice.setAgcThreshold(-91);

        QPushButton* ritButton = buttonWithText(replacement, QStringLiteral("RIT"));
        QPushButton* xitButton = buttonWithText(replacement, QStringLiteral("XIT"));
        QPushButton* snbButton = buttonWithText(replacement, QStringLiteral("SNB"));
        QPushButton* apfButton = buttonWithText(replacement, QStringLiteral("APF"));
        QPushButton* muteButton = buttonWithText(replacement, QStringLiteral("Mute"));
        QPushButton* sqlButton = buttonWithText(replacement, QStringLiteral("SQL"));
        QPushButton* binButton = buttonWithText(replacement, QStringLiteral("BIN"));
        QSlider* panSlider = sliderWithTooltipPrefix(
            replacement, QStringLiteral("Audio pan:"));
        QSlider* agcSlider = agcThresholdSlider(replacement);
        QVERIFY(ritButton);
        QVERIFY(xitButton);
        QVERIFY(snbButton);
        QVERIFY(apfButton);
        QVERIFY(muteButton);
        QVERIFY(sqlButton);
        QVERIFY(binButton);
        QVERIFY(panSlider);
        QVERIFY(agcSlider);
        QVERIFY(!ritButton->isChecked());
        QVERIFY(!xitButton->isChecked());
        QVERIFY(!snbButton->isChecked());
        QVERIFY(!apfButton->isChecked());
        QVERIFY(!muteButton->isChecked());
        QVERIFY(!sqlButton->isChecked());
        QVERIFY(!binButton->isChecked());
        QCOMPARE(panSlider->value(), 25);
        QCOMPARE(agcSlider->value(), -91);

        slice.setRitEnabled(true);
        slice.setXitEnabled(true);
        slice.setSnbEnabled(true);
        slice.setApfEnabled(true);
        slice.setMuted(true);
        slice.setSsqlEnabled(true);
        slice.setBinauralEnabled(true);
        slice.setAgcThreshold(-73);
        QVERIFY(ritButton->isChecked());
        QVERIFY(xitButton->isChecked());
        QVERIFY(snbButton->isChecked());
        QVERIFY(apfButton->isChecked());
        QVERIFY(muteButton->isChecked());
        QVERIFY(sqlButton->isChecked());
        QVERIFY(binButton->isChecked());
        QCOMPARE(agcSlider->value(), -73);
    }

    // R-R3-30: a layout rehome removes the slice's flag from every pan and
    // wires a fresh one on the destination pan, as MainWindow's panKeyChanged
    // handler does. The frequency, mode and filter handlers (and the other
    // state presentation handlers) must retire with the flag they paint, so
    // five rehomes leave exactly one live copy of each.
    void rehomingRetiresStatePresentationHandlers()
    {
        SliceModel slice(1);
        SpectrumWidget panA;
        SpectrumWidget panB;

        int frequencyRuns = 0;
        int modeRuns = 0;
        int filterRuns = 0;
        const auto hooks = [&] {
            SliceFlagHostHooks h;
            h.frequencyChanged = [&frequencyRuns](double) { ++frequencyRuns; };
            h.modeChanged = [&modeRuns](DSPMode) { ++modeRuns; };
            h.filterChanged = [&filterRuns](int, int) { ++filterRuns; };
            return h;
        };

        QList<QMetaObject::Connection> retiredConnections;
        SpectrumWidget* host = &panA;
        VfoWidget* flag = host->addVfoWidget(slice.sliceIndex());
        QVERIFY(flag);
        QList<QMetaObject::Connection> live =
            wireSliceFlagStatePresentation(&slice, flag, hooks());
        QCOMPARE(live.size(), 9);

        for (int rehome = 0; rehome < 5; ++rehome) {
            QPointer<VfoWidget> retired(flag);
            panA.removeVfoWidget(slice.sliceIndex());
            panB.removeVfoWidget(slice.sliceIndex());
            QVERIFY(retired.isNull());
            retiredConnections.append(live);

            host = (host == &panA) ? &panB : &panA;
            flag = host->addVfoWidget(slice.sliceIndex());
            QVERIFY(flag);
            live = wireSliceFlagStatePresentation(&slice, flag, hooks());
            QCOMPARE(live.size(), 9);
        }

        // Every retired handler went with its flag. QObject::disconnect
        // returns false only when Qt already retired the connection; any
        // still-live handle is consumed here so the RED run stays safe.
        int stillLive = 0;
        for (const QMetaObject::Connection& connection : retiredConnections) {
            if (QObject::disconnect(connection)) {
                ++stillLive;
            }
        }
        QCOMPARE(stillLive, 0);

        // One change runs each presentation handler exactly once, and the
        // surviving flag shows the new value (no suppression).
        slice.setFrequency(14'074'000.0);
        QCOMPARE(frequencyRuns, 1);
        QCOMPARE(flag->frequency(), 14'074'000.0);

        modeRuns = 0;
        filterRuns = 0;
        slice.setDspMode(DSPMode::CWU);
        QCOMPARE(modeRuns, 1);
        QComboBox* mode = modeCombo(flag);
        QVERIFY(mode);
        QCOMPARE(mode->currentText(), SliceModel::modeName(DSPMode::CWU));

        filterRuns = 0;
        slice.setFilter(-321, 654);
        QCOMPARE(filterRuns, 1);
        QCOMPARE(flag->filterLow(), -321);
        QCOMPARE(flag->filterHigh(), 654);

        // Removing the last flag retires the last set too; later model
        // updates reach no handler at all.
        host->removeVfoWidget(slice.sliceIndex());
        for (const QMetaObject::Connection& connection : live) {
            QVERIFY(!QObject::disconnect(connection));
        }
        frequencyRuns = 0;
        modeRuns = 0;
        filterRuns = 0;
        slice.setFrequency(7'074'000.0);
        slice.setDspMode(DSPMode::LSB);
        slice.setFilter(-2850, -150);
        slice.setAgcMode(AGCMode::Fast);
        slice.setAfGain(12);
        slice.setRfGain(34);
        slice.setStepHz(500);
        slice.setRxAntenna(QStringLiteral("ANT2"));
        slice.setTxAntenna(QStringLiteral("ANT3"));
        QCOMPARE(frequencyRuns, 0);
        QCOMPARE(modeRuns, 0);
        QCOMPARE(filterRuns, 0);
    }
};

QTEST_MAIN(TestSliceFlagPresentationLifetime)
#include "tst_slice_flag_presentation_lifetime.moc"
