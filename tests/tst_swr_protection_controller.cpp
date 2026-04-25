// no-port-check: test fixture asserting SwrProtectionController state machine against Thetis PollPAPWR
#include <QtTest>
#include "core/safety/SwrProtectionController.h"

using namespace NereusSDR::safety;

class TestSwrProtectionController : public QObject
{
    Q_OBJECT
private:
    SwrProtectionController* m_ctrl = nullptr;

private slots:
    void init();
    void cleanup();

    void cleanMatch_factorIs1_highSwrFalse();
    void swrAtLimit_foldbackEngages_factorComputed();
    void fourConsecutiveTrips_windbackLatches();
    void recoveryWithoutMoxOff_doesNotClearLatch();
    void moxOffClearsLatch();
    void openAntennaDetected_swr50_factor0_01();
    void lowPowerClampedToSwr1_0();
    void disableOnTune_bypassesProtection();
};

void TestSwrProtectionController::init()
{
    delete m_ctrl;
    m_ctrl = new SwrProtectionController();
    m_ctrl->setEnabled(true);
    m_ctrl->setLimit(2.0f);
    m_ctrl->setWindBackEnabled(true);
}

void TestSwrProtectionController::cleanup()
{
    delete m_ctrl;
    m_ctrl = nullptr;
}

// SWR well below limit → factor stays 1.0, highSwr stays false
// From Thetis console.cs:26078-26082 [v2.10.3.13]
void TestSwrProtectionController::cleanMatch_factorIs1_highSwrFalse()
{
    // fwd=10W, rev=0.1W → rho=sqrt(0.01)=0.1 → swr≈1.22 < limit(2.0)
    m_ctrl->ingest(10.0f, 0.1f, false);
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
    QVERIFY(!m_ctrl->highSwr());
}

// SWR at limit → per-sample foldback factor = limit / (swr + 1)
// From Thetis console.cs:26073-26075 [v2.10.3.13] — 4th trip triggers foldback
void TestSwrProtectionController::swrAtLimit_foldbackEngages_factorComputed()
{
    // fwd=10W, rev=5.56W → rho≈0.7454 → swr≈(1.745)/(0.2546)≈6.86 > limit(2.0)
    // Trigger 4 consecutive trips to get foldback
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(10.0f, 5.56f, false);
    }
    QVERIFY(m_ctrl->highSwr());
    // After 4 trips, windback latches at 0.01
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);
}

// After exactly 4 consecutive high-SWR readings, windback latches at 0.01
// From Thetis console.cs:26070-26075 [v2.10.3.13]
void TestSwrProtectionController::fourConsecutiveTrips_windbackLatches()
{
    // fwd=10W, rev=5.56W → swr≈6.86 > limit(2.0)
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(10.0f, 5.56f, false);
    }
    QVERIFY(m_ctrl->windBackLatched());
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);
}

// Once latched, good SWR reading does NOT clear the latch — only moxOff() does
// From Thetis console.cs:26083-26091 [v2.10.3.13] (catch-all windback block)
void TestSwrProtectionController::recoveryWithoutMoxOff_doesNotClearLatch()
{
    // Trigger latch
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(10.0f, 5.56f, false);
    }
    QVERIFY(m_ctrl->windBackLatched());

    // Feed clean signal — latch must persist
    m_ctrl->ingest(10.0f, 0.1f, false);
    QVERIFY(m_ctrl->windBackLatched());
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);
}

// moxOff() clears all latches and resets factor to 1.0
// From Thetis console.cs:29191-29195 [v2.10.3.13] (UIMOXChangedFalse)
void TestSwrProtectionController::moxOffClearsLatch()
{
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(10.0f, 5.56f, false);
    }
    QVERIFY(m_ctrl->windBackLatched());

    m_ctrl->onMoxOff();

    QVERIFY(!m_ctrl->windBackLatched());
    QVERIFY(!m_ctrl->highSwr());
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
}

// Open antenna: fwd > 10W and (fwd-rev) < 1W → swr=50, factor=0.01
// From Thetis console.cs:25989-26009 [v2.10.3.6] //[2.10.3.6]MW0LGE (verbatim inline tag)
void TestSwrProtectionController::openAntennaDetected_swr50_factor0_01()
{
    // fwd=15W, rev=14.5W → (fwd-rev)=0.5 < 1.0 and fwd > 10.0
    m_ctrl->ingest(15.0f, 14.5f, false);
    QVERIFY(m_ctrl->openAntennaDetected());
    QCOMPARE(m_ctrl->measuredSwr(), 50.0f);
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);
    QVERIFY(m_ctrl->highSwr());
}

// Both fwd and rev ≤ 2W floor → SWR clamped to 1.0, no trip
// From Thetis console.cs:25978 [v2.10.3.13]
void TestSwrProtectionController::lowPowerClampedToSwr1_0()
{
    m_ctrl->ingest(1.0f, 1.0f, false);
    QCOMPARE(m_ctrl->measuredSwr(), 1.0f);
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
    QVERIFY(!m_ctrl->highSwr());
}

// Tune-time bypass: disableOnTune=true + tuneActive=true + fwd in [1, tunePowerSwrIgnore]
// → factor stays 1.0, highSwr stays false
// From Thetis console.cs:26020-26057 [v2.10.3.13]
void TestSwrProtectionController::disableOnTune_bypassesProtection()
{
    m_ctrl->setDisableOnTune(true);
    m_ctrl->setTunePowerSwrIgnore(30.0f); // ignore up to 30W during tune

    // fwd=10W, rev=5.56W → swr≈6.86 (would normally trip), but tune bypasses
    m_ctrl->ingest(10.0f, 5.56f, /*tuneActive=*/true);

    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
    QVERIFY(!m_ctrl->highSwr());
}

QTEST_GUILESS_MAIN(TestSwrProtectionController)
#include "tst_swr_protection_controller.moc"
