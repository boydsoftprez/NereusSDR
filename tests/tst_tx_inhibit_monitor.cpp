// no-port-check: test fixture asserting TxInhibitMonitor state machine against Thetis PollTXInhibit
#include <QtTest>
#include "core/safety/TxInhibitMonitor.h"

using namespace NereusSDR::safety;

class TestTxInhibitMonitor : public QObject
{
    Q_OBJECT
private:
    TxInhibitMonitor* m_mon    = nullptr;
    bool              m_pinAsserted = false;

private slots:
    void init();
    void cleanup();

    // ── 8 required test slots ──────────────────────────────────────────────
    void noInhibit_atStartup_signalsFalse();
    void userIo01_assertedActiveLow_emitsWithSourceUserIo01();
    void userIo01_reverseLogic_invertsActiveLow();
    void rxOnly_assertedTrue_emitsWithSourceRx2OnlyRadio();
    void outOfBand_assertedTrue_emitsWithSourceOutOfBand();
    void blockTxAntenna_assertedTrue_emitsWithSourceBlockTxAntenna();
    void multipleSourcesActive_inhibitedRemainsTrueUntilAllClear();
    void pollCadence100ms_doesNotEmitDuplicates();
};

void TestTxInhibitMonitor::init()
{
    delete m_mon;
    m_mon = nullptr;
    m_pinAsserted = false;

    m_mon = new TxInhibitMonitor();
    m_mon->setUserIoReader([this]() -> bool { return m_pinAsserted; });
    m_mon->setEnabled(true);
}

void TestTxInhibitMonitor::cleanup()
{
    delete m_mon;
    m_mon = nullptr;
}

// After startup with pin LOW (not asserted), no inhibit signal fires and
// inhibited() returns false after the first poll tick at 100 ms.
// From Thetis console.cs:25801-25839 [v2.10.3.13] (PollTXInhibit baseline state)
// Tag preserved: //DH1KLM (console.cs:25814 — REDPITAYA/ANAN7000D/8000D use getUserI02 in P1)
void TestTxInhibitMonitor::noInhibit_atStartup_signalsFalse()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_pinAsserted = false;
    QTest::qWait(150); // wait for at least one 100 ms poll tick
    QVERIFY(spy.isEmpty());
    QVERIFY(!m_mon->inhibited());
}

// When the UserIO pin is asserted (active-low → logical true from reader),
// the monitor emits txInhibitedChanged(true, UserIo01).
// From Thetis console.cs:25814-25820 [v2.10.3.13] (inhibit_input = !getUserI01())
// Tag preserved: //DH1KLM (console.cs:25814 — per-board model check)
void TestTxInhibitMonitor::userIo01_assertedActiveLow_emitsWithSourceUserIo01()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_pinAsserted = true;
    QTest::qWait(150);
    QVERIFY(!spy.isEmpty());
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::UserIo01);
    auto args = spy.first();
    QCOMPARE(args.at(0).toBool(), true);
    QCOMPARE(args.at(1).value<TxInhibitMonitor::Source>(), TxInhibitMonitor::Source::UserIo01);
}

// setReverseLogic(true) inverts the pin's sense: pin LOW (reader returns false)
// should now assert inhibit.
// From Thetis console.cs:25830 [v2.10.3.13] (if (_reverseTxInhibit) inhibit_input = !inhibit_input)
void TestTxInhibitMonitor::userIo01_reverseLogic_invertsActiveLow()
{
    // Spy must be attached before setReverseLogic() because that call
    // triggers an immediate recompute() which may emit.
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_pinAsserted = false; // pin LOW; with reverseLogic this should assert inhibit
    m_mon->setReverseLogic(true); // fires recompute() → emits txInhibitedChanged
    QVERIFY(!spy.isEmpty());
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::UserIo01);
}

// notifyRxOnly(true) immediately causes inhibit with source Rx2OnlyRadio.
// From Thetis console.cs:15283-15307 [v2.10.3.13] (RXOnly property setter)
void TestTxInhibitMonitor::rxOnly_assertedTrue_emitsWithSourceRx2OnlyRadio()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_mon->notifyRxOnly(true);
    QVERIFY(!spy.isEmpty());
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::Rx2OnlyRadio);
}

// notifyOutOfBand(true) immediately causes inhibit with source OutOfBand.
// From Thetis console.cs:6770-6806 [v2.10.3.13] (CheckValidTXFreq)
void TestTxInhibitMonitor::outOfBand_assertedTrue_emitsWithSourceOutOfBand()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_mon->notifyOutOfBand(true);
    QVERIFY(!spy.isEmpty());
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::OutOfBand);
}

// notifyBlockTxAntenna(true) immediately causes inhibit with source BlockTxAntenna.
// From Thetis Andromeda.cs:285-306 [v2.10.3.13] (AlexANT[2,3]RXOnly)
void TestTxInhibitMonitor::blockTxAntenna_assertedTrue_emitsWithSourceBlockTxAntenna()
{
    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    m_mon->notifyBlockTxAntenna(true);
    QVERIFY(!spy.isEmpty());
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::BlockTxAntenna);
}

// When two sources are active, clearing one still leaves the monitor inhibited
// (highest-priority source takes over). Only when all sources are cleared does
// inhibited() return false.
// From Thetis console.cs:25801-25839 [v2.10.3.13] — priority ordering.
// Tag preserved: //DH1KLM (console.cs:25814 — per-board model check for P1)
void TestTxInhibitMonitor::multipleSourcesActive_inhibitedRemainsTrueUntilAllClear()
{
    m_mon->notifyRxOnly(true);
    m_mon->notifyOutOfBand(true);
    QVERIFY(m_mon->inhibited());
    // Highest-priority source of the two is Rx2OnlyRadio (priority order: UserIo01 > Rx2OnlyRadio > OutOfBand > BlockTxAntenna)
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::Rx2OnlyRadio);

    // Clear one — still inhibited because OutOfBand is still active
    m_mon->notifyRxOnly(false);
    QVERIFY(m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::OutOfBand);

    // Clear the other — now clear
    m_mon->notifyOutOfBand(false);
    QVERIFY(!m_mon->inhibited());
    QCOMPARE(m_mon->lastSource(), TxInhibitMonitor::Source::None);
}

// The poll timer fires every 100 ms but must NOT emit duplicate signals when
// the pin state has not changed between ticks.
// From Thetis console.cs:25832 [v2.10.3.13] (if (TXInhibit != inhibit_input) fire)
void TestTxInhibitMonitor::pollCadence100ms_doesNotEmitDuplicates()
{
    m_pinAsserted = true;
    QTest::qWait(350); // ~3 poll ticks

    QSignalSpy spy(m_mon, &TxInhibitMonitor::txInhibitedChanged);
    // The transition already happened before the spy was attached; now pin stays
    // asserted for another 300 ms — no new signals should fire.
    QTest::qWait(300);
    QVERIFY(spy.isEmpty()); // no further emissions for an unchanged pin state
    QVERIFY(m_mon->inhibited());
}

QTEST_MAIN(TestTxInhibitMonitor)
#include "tst_tx_inhibit_monitor.moc"
