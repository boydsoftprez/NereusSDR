// no-port-check: NereusSDR-original test file. Thetis behaviour it pins is
// cited in RadioModel.cpp (console.cs:30132-30140 [v2.10.3.15]); no C# is
// translated here.
// =================================================================
// tests/tst_tx_applet_tune_state.cpp  (NereusSDR)
// =================================================================
//
// R-R3-21: the TUNE button shows the true Tune state.
//
// Before the fix the click handler wrote "TUNING..." after
// RadioModel::setTune(true) returned, and a refused key (band plan or TX
// interlock) left the Tune flag latched, so a refused press read as tuning
// with the radio in RX. The button's text and checked state now come only
// from MoxController::manualMoxChanged and RadioModel::tuneRefused, and a
// disconnect mid-Tune drops it back to "TUNE".
//
// Drives a real TxApplet on a RadioModel with a mock connection; runs on
// the offscreen platform.
// =================================================================

#include <QtTest/QtTest>
#include <QPushButton>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

namespace {

void pump()
{
    for (int i = 0; i < 4; ++i) {
        QCoreApplication::processEvents();
    }
}

QPushButton* findTuneButton(TxApplet& applet)
{
    const auto buttons = applet.findChildren<QPushButton*>();
    for (QPushButton* b : buttons) {
        if (b->accessibleName() == QStringLiteral("Tune carrier")) {
            return b;
        }
    }
    return nullptr;
}

}  // namespace

class TestTxAppletTuneState : public QObject
{
    Q_OBJECT

    struct Rig {
        std::unique_ptr<MockConnection> conn;
        std::unique_ptr<RadioModel> model;
        std::unique_ptr<TxApplet> applet;
        QPushButton* tuneBtn = nullptr;

        Rig()
            : conn(std::make_unique<MockConnection>())
            , model(std::make_unique<RadioModel>())
        {
            model->setCapsForTest(/*hasAlex=*/false);
            model->injectConnectionForTest(conn.get());
            model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
            model->setTuneOffSettleMsForTest(0);
            model->addSlice();
            model->activeSlice()->setDspMode(DSPMode::USB);
            applet = std::make_unique<TxApplet>(model.get());
            tuneBtn = findTuneButton(*applet);
        }

        ~Rig()
        {
            if (model->isTune()) {
                model->setTune(false);
                pump();
            }
            applet.reset();
            model->injectConnectionForTest(nullptr);
            model.reset();
        }
    };

private slots:
    // QTEST_MAIN creates the QApplication (this target links QtWidgets).
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // A band-plan refusal leaves the button unchecked and reading "TUNE".
    void refusedPressShowsTune()
    {
        Rig rig;
        QVERIFY(rig.tuneBtn != nullptr);

        rig.model->moxController()->setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{
                false, QStringLiteral("Out of band")};
        });

        rig.tuneBtn->click();
        QVERIFY(!rig.model->moxController()->isMox());
        QVERIFY(!rig.tuneBtn->isChecked());
        QCOMPARE(rig.tuneBtn->text(), QStringLiteral("TUNE"));

        pump();
        QVERIFY(!rig.tuneBtn->isChecked());
        QCOMPARE(rig.tuneBtn->text(), QStringLiteral("TUNE"));
    }

    // A successful Tune is unchanged, and unkey always returns the button.
    void acceptedPressShowsTuningThenUnkeys()
    {
        Rig rig;
        QVERIFY(rig.tuneBtn != nullptr);

        rig.tuneBtn->click();
        pump();
        QVERIFY(rig.model->moxController()->isMox());
        QVERIFY(rig.tuneBtn->isChecked());
        QCOMPARE(rig.tuneBtn->text(), QStringLiteral("TUNING..."));

        rig.tuneBtn->click();
        pump();
        QVERIFY(!rig.model->moxController()->isMox());
        QVERIFY(!rig.tuneBtn->isChecked());
        QCOMPARE(rig.tuneBtn->text(), QStringLiteral("TUNE"));
    }

    // After a refused press the next allowed press tunes normally.
    void refusedThenAcceptedPressTunes()
    {
        Rig rig;
        QVERIFY(rig.tuneBtn != nullptr);

        rig.model->moxController()->setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{
                false, QStringLiteral("Out of band")};
        });
        rig.tuneBtn->click();
        QVERIFY(!rig.tuneBtn->isChecked());

        rig.model->moxController()->setMoxCheck({});
        rig.tuneBtn->click();
        pump();
        QVERIFY(rig.model->moxController()->isMox());
        QVERIFY(rig.tuneBtn->isChecked());
        QCOMPARE(rig.tuneBtn->text(), QStringLiteral("TUNING..."));
    }

    // A disconnect during Tune returns the button to "TUNE": teardown runs
    // the Tune-off path, which releases the manual MOX the button reads.
    void disconnectMidTuneShowsTune()
    {
        Rig rig;
        QVERIFY(rig.tuneBtn != nullptr);

        rig.tuneBtn->click();
        pump();
        QVERIFY(rig.tuneBtn->isChecked());
        QCOMPARE(rig.tuneBtn->text(), QStringLiteral("TUNING..."));

        rig.model->disconnectFromRadio();
        pump();
        QVERIFY(!rig.model->isTune());
        QVERIFY(!rig.model->moxController()->isManualMox());
        QVERIFY(!rig.tuneBtn->isChecked());
        QCOMPARE(rig.tuneBtn->text(), QStringLiteral("TUNE"));

        // Pressing TUNE off afterwards (the case the review found stuck)
        // stays on "TUNE".
        rig.model->setTune(false);
        pump();
        QVERIFY(!rig.tuneBtn->isChecked());
        QCOMPARE(rig.tuneBtn->text(), QStringLiteral("TUNE"));
    }
};

QTEST_MAIN(TestTxAppletTuneState)
#include "tst_tx_applet_tune_state.moc"
