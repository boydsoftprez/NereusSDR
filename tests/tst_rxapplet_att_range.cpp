// no-port-check: test fixture asserts UI behavior, no Thetis port
//
// Phase 3P-A Task 15: verify S-ATT spinbox max reads from
// BoardCapabilities::attenuator.maxDb (single source of truth).
//   HL2    → 0–63 dB  (mi0bot 6-bit LNA range, [@c26a8a4])
//   Hermes → 0–31 dB  (Thetis setup.cs:15765 [v2.10.3.13] base)
//
// RxApplet sets the spinbox range in buildUi() using
// m_model->boardCapabilities().attenuator.maxDb so the correct ceiling
// is in place from widget creation, before any radio connection occurs.
//
// R-R3-46 / R-R3-21 (2026-09-23): in a remote window the range is the one
// the Core reports for its own radio, once the Core offers its attenuator.
//
// Level Cal fix wave (2026-09-30): a slice on the other ADC shows and sets
// RX2's own preamp mode from RX2's list.

#include <QtTest/QtTest>
#include <QApplication>

#include "core/StepAttenuatorController.h"
#include "core/session/IStationLink.h"
#include "core/StepAttenuatorFacade.h"
#include "models/Band.h"
#include "models/SliceModel.h"
#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestRxAppletAttRange : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int   argc = 0;
            static char* argv = nullptr;
            new QApplication(argc, &argv);
        }
    }

    // HL2 S-ATT slider uses signed −28..+31 dB range.  mi0bot widens
    // upper bound to +32 at console.cs:11043 [v2.10.3.13-beta2] but that
    // is an off-by-one upstream bug (wire encoding `31 - userDb` produces
    // wire = -1 at userDb=32 → 6-bit-masks to LNA-gain wraparound region).
    // NereusSDR caps at +31 per maintainer approval (issue #175).
    void hl2_slider_max_is_signed_range()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        RxApplet applet(nullptr, &model);
        QCOMPARE(applet.stepAttMaxForTest(), 31);
    }

    // All standard-attenuator boards stay at 0–31 dB (no Alex present at
    // construction time; Alex extension happens in connectSlice() on connect).
    void hermes_slider_max_is_31()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        RxApplet applet(nullptr, &model);
        QCOMPARE(applet.stepAttMaxForTest(), 31);
    }

    // A remote window: the board table stands until the Core offers its
    // attenuator, then the range is the Core's (Hermes with Alex: 0..61,
    // Thetis setup.cs:15773-15786 [v2.10.3.13] via stepAttMaxDb on the Core).
    void remote_slider_range_is_the_cores()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setBoardForTest(HPSDRHW::Hermes);
        RxApplet applet(nullptr, &remote);
        StepAttenuatorFacade* stepAtt = remote.stepAttFacade();
        stepAtt->applyRemoteProperty("minDb", 0);
        stepAtt->applyRemoteProperty("maxDb", 61);
        QCOMPARE(applet.stepAttMaxForTest(), 31);
        stepAtt->setWindowAvailability(true, QString());
        QCOMPARE(applet.stepAttMinForTest(), 0);
        QCOMPARE(applet.stepAttMaxForTest(), 61);

        // An HL2 Core reports its signed range.
        stepAtt->applyRemoteProperty("minDb", -28);
        stepAtt->applyRemoteProperty("maxDb", 31);
        QCOMPARE(applet.stepAttMinForTest(), -28);
        QCOMPARE(applet.stepAttMaxForTest(), 31);
    }

    // R-R3-46 / R-R3-11: the ATT / S-ATT / A-ATT label and which control
    // shows follow the step attenuator of the slice's own ADC (Thetis RX2's
    // enable and auto-attenuate for a slice on the other ADC). The preamp
    // choice there is RX2's own (Thetis comboRX2Preamp, RX2PreampMode).
    void localLabelFollowsTheSlicesAdc()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        auto* ctrl = new StepAttenuatorController(&model);
        ctrl->setTickTimerEnabled(false);
        model.setStepAttController(ctrl);
        SliceModel* a = model.sliceById(model.addSlice());
        SliceModel* b = model.sliceById(model.addSlice());
        QVERIFY(a && b);
        ctrl->setAdcRouting(0, 1, Band::Band20m, false, 1u << b->sliceIndex());
        ctrl->setStepAttEnabled(true);
        ctrl->setRx2StepAttEnabled(false);
        ctrl->setPreampMode(PreampMode::SaMinus20);
        RxApplet applet(nullptr, &model);
        auto* label = applet.findChild<QLabel*>(QStringLiteral("RxAttLabel"));
        auto* stack = applet.findChild<QStackedWidget*>(QStringLiteral("RxAttenuatorStack"));
        auto* combo = applet.findChild<QComboBox*>(QStringLiteral("RxPreampCombo"));
        QVERIFY(label && stack && combo);

        applet.setSlice(a);
        QCOMPARE(label->text(), QStringLiteral("S-ATT"));
        QCOMPARE(stack->currentIndex(), 1);

        applet.setSlice(b);
        QCOMPARE(label->text(), QStringLiteral("ATT"));
        QCOMPARE(stack->currentIndex(), 0);
        QVERIFY(combo->isEnabled());
        QVERIFY(combo->toolTip().isEmpty());
        // RX2's list and RX2's mode; a choice sets RX2's mode only.
        QStringList labels;
        for (int i = 0; i < combo->count(); ++i) {
            labels.append(combo->itemText(i));
        }
        QCOMPARE(labels, (QStringList{QStringLiteral("0dB"), QStringLiteral("-10dB"),
                                      QStringLiteral("-20dB"), QStringLiteral("-30dB")}));
        const PreampMode rx1Mode = ctrl->preampMode();
        ctrl->setRx2PreampMode(PreampMode::SaMinus30);
        QCOMPARE(combo->currentData().toInt(), static_cast<int>(PreampMode::SaMinus30));
        combo->setCurrentIndex(combo->findData(static_cast<int>(PreampMode::SaMinus10)));
        QCOMPARE(ctrl->rx2PreampMode(), PreampMode::SaMinus10);
        QCOMPARE(ctrl->preampMode(), rx1Mode);

        ctrl->setRx2StepAttEnabled(true);
        QCOMPARE(label->text(), QStringLiteral("S-ATT"));
        QCOMPARE(stack->currentIndex(), 1);
        ctrl->setRx2AutoAttEnabled(true);
        QCOMPARE(label->text(), QStringLiteral("A-ATT"));

        applet.setSlice(a);
        QCOMPARE(label->text(), QStringLiteral("S-ATT"));
        QVERIFY(combo->isEnabled());
        QCOMPARE(combo->currentData().toInt(), static_cast<int>(ctrl->preampMode()));
    }

    void remoteLabelFollowsTheSlicesAdc()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setBoardForTest(HPSDRHW::Saturn);
        RxApplet applet(nullptr, &remote);
        StepAttenuatorFacade* stepAtt = remote.stepAttFacade();
        stepAtt->setWindowAvailability(true, QString());
        stepAtt->applyRemoteProperty("rx2SliceMask", 1 << 1);
        auto* label = applet.findChild<QLabel*>(QStringLiteral("RxAttLabel"));
        auto* stack = applet.findChild<QStackedWidget*>(QStringLiteral("RxAttenuatorStack"));
        QVERIFY(label && stack);
        SliceModel b(1);
        applet.setSlice(&b);
        QCOMPARE(label->text(), QStringLiteral("ATT"));
        // No Core that carries RX2's own preamp mode: disabled with the reason.
        auto* combo = applet.findChild<QComboBox*>(QStringLiteral("RxPreampCombo"));
        QVERIFY(combo);
        QVERIFY(!combo->isEnabled());
        QCOMPARE(combo->toolTip(), IStationLink::rx2PreampModeUnavailableReason());
        stepAtt->setRx2StepAttEnabled(true);
        QCOMPARE(label->text(), QStringLiteral("S-ATT"));
        QCOMPARE(stack->currentIndex(), 1);
        stepAtt->setRx2AutoAttEnabled(true);
        QCOMPARE(label->text(), QStringLiteral("A-ATT"));
        applet.setSlice(nullptr);
    }
};

QTEST_MAIN(TestRxAppletAttRange)
#include "tst_rxapplet_att_range.moc"
