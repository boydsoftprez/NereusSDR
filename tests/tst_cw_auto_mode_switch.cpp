// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_cw_auto_mode_switch.cpp  (NereusSDR)
// =================================================================
//
// CW transmit (R-CW-37): the auto mode switch on a RadioModel with a fake
// connection. After Thetis update_for_auto_mode_return and cwAutoModeTick
// (console.cs:25654, 25660-25752 [v2.10.3.15], "[2.10.1.0]MW0LGE implements
// #70"): with the switch on, a dot or dash (the radio's status or the
// serial paddles) while the transmit slice is not in CW switches it to CWL
// from LSB or DIGL and to CWU otherwise; with the return on, it goes back
// cwAutoModeReturnMs after the last dot or dash, and a new one restarts the
// wait; with the switch off nothing changes. Nothing here opens a radio
// socket or keys a radio.
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the auto mode switch. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>

#include "core/AppSettings.h"
#include "core/CwSerialKeyInput.h"
#include "core/CwTypes.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

Q_DECLARE_METATYPE(NereusSDR::DSPMode)

namespace {

// The serial lines, driven by the test.
class FakeLineSource : public CwSerialKeyInput::LineSource {
public:
    struct Shared {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::pair<bool, bool>> pending;
        bool stopped{false};
        bool waiting{false};
    };
    explicit FakeLineSource(std::shared_ptr<Shared> s) : m_s(std::move(s)) {}
    bool open(const QString&, QString*) override
    {
        const std::lock_guard<std::mutex> lock(m_s->mutex);
        m_s->stopped = false;
        m_s->pending.clear();
        return true;
    }
    bool waitForChange(bool* dsr, bool* cts) override
    {
        std::unique_lock<std::mutex> lock(m_s->mutex);
        m_s->waiting = true;
        m_s->cv.notify_all();
        m_s->cv.wait(lock, [this] { return m_s->stopped || !m_s->pending.empty(); });
        m_s->waiting = false;
        if (m_s->stopped) {
            return false;
        }
        *dsr = m_s->pending.front().first;
        *cts = m_s->pending.front().second;
        m_s->pending.pop_front();
        return true;
    }
    void stop() override
    {
        {
            const std::lock_guard<std::mutex> lock(m_s->mutex);
            m_s->stopped = true;
        }
        m_s->cv.notify_all();
    }

private:
    std::shared_ptr<Shared> m_s;
};

class CwMockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit CwMockConnection(QObject* parent = nullptr)
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
    void reportKey(bool dot, bool dash, bool keyOut) { emit radioKeyStatus(dot, dash, keyOut); }
};

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

RadioInfo saturn()
{
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    info.boardType = HPSDRHW::Saturn;
    info.firmwareVersion = 10;
    info.macAddress = QStringLiteral("00:1c:c0:a2:13:dd");
    return info;
}

// A local model on a P2 Saturn: one slice on 20 m in `mode`.
struct Rig {
    RadioModel model;
    CwMockConnection conn;
    std::shared_ptr<FakeLineSource::Shared> lines = std::make_shared<FakeLineSource::Shared>();
    SliceModel* slice{nullptr};

    explicit Rig(DSPMode mode)
    {
        AppSettings::instance().clear();
        model.setCapsForTest(/*hasAlex=*/false);
        model.injectConnectionForTest(&conn);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.replaceCwSerialKeyInputForTest(
            std::make_unique<CwSerialKeyInput>(std::make_unique<FakeLineSource>(lines)));
        slice = model.sliceById(model.addSlice());
        slice->setFrequency(14'050'000.0);
        slice->setDspMode(mode);
        model.setLastRadioInfoForTest(saturn());
        model.wireCwKeyerConnectionForTest();
        pump();
    }
    ~Rig()
    {
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    TransmitModel& tm() { return model.transmitModel(); }
    void switchOn(bool returnOn, int returnMs = 300)
    {
        tm().setCwAutoModeSwitch(true);
        tm().setCwAutoModeReturn(returnOn);
        tm().setCwAutoModeReturnMs(returnMs);
    }
    void key(bool dot, bool dash)
    {
        conn.reportKey(dot, dash, false);
        pump();
    }
    void dot()
    {
        key(true, false);
        key(false, false);
    }
    void pushLines(bool dsr, bool cts)
    {
        {
            const std::lock_guard<std::mutex> lock(lines->mutex);
            lines->pending.emplace_back(dsr, cts);
        }
        lines->cv.notify_all();
        std::unique_lock<std::mutex> lock(lines->mutex);
        lines->cv.wait_for(lock, std::chrono::seconds(2),
                           [this] { return lines->pending.empty() && lines->waiting; });
        lock.unlock();
        pump();
    }
};

} // namespace

class TestCwAutoModeSwitch : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // From Thetis console.cs:25712-25730 [v2.10.3.15]: LSB and DIGL go to
    // CWL, everything else to CWU.
    void aDot_switchesTheTxSliceToCw_data()
    {
        QTest::addColumn<DSPMode>("from");
        QTest::addColumn<DSPMode>("to");
        QTest::newRow("LSB to CWL") << DSPMode::LSB << DSPMode::CWL;
        QTest::newRow("DIGL to CWL") << DSPMode::DIGL << DSPMode::CWL;
        QTest::newRow("USB to CWU") << DSPMode::USB << DSPMode::CWU;
        QTest::newRow("DIGU to CWU") << DSPMode::DIGU << DSPMode::CWU;
        QTest::newRow("AM to CWU") << DSPMode::AM << DSPMode::CWU;
    }
    void aDot_switchesTheTxSliceToCw()
    {
        QFETCH(DSPMode, from);
        QFETCH(DSPMode, to);
        Rig rig(from);
        rig.switchOn(/*returnOn=*/false);
        rig.dot();
        QCOMPARE(rig.slice->dspMode(), to);
        // The return is off: nothing to go back to.
        QVERIFY(!rig.model.cwAutoModeReturnModeForTest().has_value());
    }

    void aDash_switchesToo()
    {
        Rig rig(DSPMode::LSB);
        rig.switchOn(false);
        rig.key(false, true);
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWL);
        rig.key(false, false);
    }

    void switchOff_doesNothing()
    {
        Rig rig(DSPMode::LSB);
        rig.tm().setCwAutoModeSwitch(false);
        rig.tm().setCwAutoModeReturn(true);
        rig.dot();
        QCOMPARE(rig.slice->dspMode(), DSPMode::LSB);
        QVERIFY(!rig.model.cwAutoModeReturnModeForTest().has_value());
    }

    void returnOn_goesBackAfterTheWaitFromTheLastDot()
    {
        Rig rig(DSPMode::LSB);
        rig.switchOn(/*returnOn=*/true, 300);
        QElapsedTimer t;
        t.start();
        rig.dot();
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWL);
        QCOMPARE(rig.model.cwAutoModeReturnModeForTest(), std::optional<DSPMode>(DSPMode::LSB));
        QTRY_COMPARE_WITH_TIMEOUT(rig.slice->dspMode(), DSPMode::LSB, 3000);
        QVERIFY2(t.elapsed() >= 300, qPrintable(QString::number(t.elapsed())));
        QVERIFY(!rig.model.cwAutoModeReturnModeForTest().has_value());
    }

    void aNewDot_restartsTheWait()
    {
        Rig rig(DSPMode::USB);
        rig.switchOn(true, 400);
        rig.dot();
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWU);
        QTest::qWait(250);
        QElapsedTimer sinceSecond;
        sinceSecond.start();
        rig.dot();                        // in CW now: the wait restarts
        QTest::qWait(250);                // 500 ms after the first dot
        if (sinceSecond.elapsed() < 400) {
            QCOMPARE(rig.slice->dspMode(), DSPMode::CWU);
        }
        QTRY_COMPARE_WITH_TIMEOUT(rig.slice->dspMode(), DSPMode::USB, 3000);
        QVERIFY2(sinceSecond.elapsed() >= 400, qPrintable(QString::number(sinceSecond.elapsed())));
    }

    void aKeyHeldPastTheWait_holdsCwUntilReleased()
    {
        Rig rig(DSPMode::LSB);
        rig.switchOn(true, 100);
        rig.key(true, false);
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWL);
        QTest::qWait(350);
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWL);
        QElapsedTimer sinceRelease;
        sinceRelease.start();
        rig.key(false, false);
        QTRY_COMPARE_WITH_TIMEOUT(rig.slice->dspMode(), DSPMode::LSB, 3000);
        QVERIFY2(sinceRelease.elapsed() >= 100, qPrintable(QString::number(sinceRelease.elapsed())));
    }

    // [2.10.3.12]MW0LGE: keying while in CW with nothing to return to
    // leaves the mode alone.
    void keyingInCw_withNothingSaved_staysInCw()
    {
        Rig rig(DSPMode::CWU);
        rig.switchOn(true, 100);
        rig.dot();
        QTest::qWait(300);
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWU);
    }

    void serialPaddles_switchTheModeToo()
    {
        Rig rig(DSPMode::USB);
        rig.switchOn(false);
        rig.tm().setCwSerialPaddles(true);
        rig.tm().setCwSerialPort(QStringLiteral("fake0"));
        pump();
        QVERIFY(rig.model.cwSerialKeyInput()->isReading());
        rig.pushLines(true, false);
        QTRY_COMPARE_WITH_TIMEOUT(rig.slice->dspMode(), DSPMode::CWU, 2000);
        // The paddle was pressed in USB (the keyer gate refused it): it keys
        // nothing until it is pressed again.
        QVERIFY(!rig.conn.cwKeyLine()->dot());
        rig.pushLines(false, false);
        rig.pushLines(true, false);
        QVERIFY(rig.conn.cwKeyLine()->dot());
        rig.pushLines(false, false);
    }
};

QTEST_MAIN(TestCwAutoModeSwitch)
#include "tst_cw_auto_mode_switch.moc"
