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
    void swrAtLimit_fourTripsWithWindBack_latchEngages();
    void fourConsecutiveTrips_windbackLatches();
    void perSampleFoldback_oneTrip_windBackDisabled_factorIs0_5();
    void recoveryWithoutMoxOff_doesNotClearLatch();
    void moxOffClearsLatch();
    void openAntennaDetected_swr50_factor0_01();
    void lowPowerClampedToSwr1_0();
    void disableOnTune_bypassesProtection();

    // Codex P2 follow-up to PR #139:
    void setEnabled_false_clearsLatchedState();

    // F.3 TODO ports — console.cs:26067 + 26020-26057 [v2.10.3.13]:
    void alexFwdLimit_default5W_belowFloor_noTrip();
    void alexFwdLimit_default5W_aboveFloor_trips();
    void alexFwdLimit_anan8000d_2xSlider_belowFloor_noTrip();
    void tuneBypass_sliderAt70_bypasses();
    void tuneBypass_sliderAt71_doesNotBypass();
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

// SWR at limit, windBackEnabled=true → 4 trips → windback latch engages at 0.01
// From Thetis console.cs:26083-26090 [v2.10.3.13] — windback latch path
void TestSwrProtectionController::swrAtLimit_fourTripsWithWindBack_latchEngages()
{
    // fwd=10W, rev=5.56W → rho≈0.7454 → swr≈(1.745)/(0.2546)≈6.86 > limit(2.0)
    // windBackEnabled=true (set in init()) — latch fires after 4 trips at 0.01
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(10.0f, 5.56f, false);
    }
    QVERIFY(m_ctrl->highSwr());
    QVERIFY(m_ctrl->windBackLatched());
    // After 4 trips with windback enabled, latch overrides per-sample factor to 0.01
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
// Tag preserved: //[2.10.3.7]MW0LGE (console.cs:29190 — reset comment)
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
// Tags preserved: //K2UE (console.cs:25985 — open-antenna check bypass for ANAN8000D)
//                 //-W2PA (console.cs:25987 — changed fwd threshold to allow 35W for amp tuners)
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
// Tag preserved: //[2.10.3.6]MW0LGE (console.cs:25980 — SWR/tune config modifications)
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

// Per-sample foldback formula in isolation (windBackEnabled=false isolates this path)
// From Thetis console.cs:26071-26075 [v2.10.3.13]:
//   factor = limit / (swr + 1)
// With limit=2.0, fwd=100W, rev=25W → rho=sqrt(0.25)=0.5 → swr=3.0
//   → factor = 2.0 / (3.0 + 1.0) = 0.5
void TestSwrProtectionController::perSampleFoldback_oneTrip_windBackDisabled_factorIs0_5()
{
    // Disabling windback isolates the per-sample foldback path; without windback
    // the controller computes factor = limit/(swr+1) each debounce cycle and
    // never overrides it to kWindBackFactor (0.01).
    m_ctrl->setWindBackEnabled(false);

    // Per-sample foldback only engages once tripCount reaches the debounce
    // threshold (kTripDebounceCount = 4). Feed four identical trips.
    m_ctrl->ingest(/*fwdW=*/100.0f, /*revW=*/25.0f, /*tuneActive=*/false);
    m_ctrl->ingest(100.0f, 25.0f, false);
    m_ctrl->ingest(100.0f, 25.0f, false);
    m_ctrl->ingest(100.0f, 25.0f, false);

    QVERIFY(m_ctrl->highSwr());
    QVERIFY(!m_ctrl->windBackLatched());

    const float factor = m_ctrl->protectFactor();
    QVERIFY2(factor >= 0.49f && factor <= 0.51f,
             qPrintable(QString("expected ~0.5, got %1").arg(factor)));
}

// Codex P2 follow-up to PR #139: when protection is disabled mid-flight while
// the windback latch / open-antenna flag / trip count are non-default, the
// disabled-path early-return MUST clear all of them. Previously only
// m_protectFactor and m_highSwr were reset; the latch and trip-count would
// survive into the re-enabled path, immediately re-asserting phantom fault
// state.
void TestSwrProtectionController::setEnabled_false_clearsLatchedState()
{
    // Drive the controller into latched + open-antenna state.
    // Two-step: 4 trips engage the windback latch (per console.cs:26069);
    // a follow-up open-antenna sample (fwd=15, fwd-rev<1, console.cs:25989)
    // sets m_openAntennaDetected on top.
    m_ctrl->setWindBackEnabled(true);
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(100.0f, 25.0f, false); // SWR=3 > limit=2 → trip
    }
    QVERIFY(m_ctrl->windBackLatched());
    QVERIFY(m_ctrl->highSwr());
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);

    m_ctrl->ingest(15.0f, 14.5f, false); // open-antenna heuristic
    QVERIFY(m_ctrl->openAntennaDetected());
    QVERIFY(m_ctrl->windBackLatched());     // unchanged from earlier latch
    QVERIFY(m_ctrl->highSwr());
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);

    QSignalSpy windSpy(m_ctrl, &SwrProtectionController::windBackLatchedChanged);
    QSignalSpy openSpy(m_ctrl, &SwrProtectionController::openAntennaDetectedChanged);
    QSignalSpy highSpy(m_ctrl, &SwrProtectionController::highSwrChanged);
    QSignalSpy factorSpy(m_ctrl, &SwrProtectionController::protectFactorChanged);

    // Disable protection mid-flight, then ingest (which would normally
    // re-trigger trip / open-antenna in enabled mode).
    m_ctrl->setEnabled(false);
    m_ctrl->ingest(100.0f, 25.0f, false);

    // All four observable flags must be clear.
    QVERIFY(!m_ctrl->openAntennaDetected());
    QVERIFY(!m_ctrl->windBackLatched());
    QVERIFY(!m_ctrl->highSwr());
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);

    // Each transition emitted exactly once on disable.
    QCOMPARE(windSpy.count(), 1);
    QCOMPARE(windSpy.at(0).at(0).toBool(), false);
    QCOMPARE(openSpy.count(), 1);
    QCOMPARE(openSpy.at(0).at(0).toBool(), false);
    QCOMPARE(highSpy.count(), 1);
    QCOMPARE(highSpy.at(0).at(0).toBool(), false);
    QCOMPARE(factorSpy.count(), 1);
    QCOMPARE(factorSpy.at(0).at(0).toFloat(), 1.0f);

    // Re-enable + ingest something safe → still clean (no phantom latch
    // resurrects from stale state).
    m_ctrl->setEnabled(true);
    m_ctrl->ingest(100.0f, 0.0f, false);
    QVERIFY(!m_ctrl->windBackLatched());
    QVERIFY(!m_ctrl->openAntennaDetected());
    QVERIFY(!m_ctrl->highSwr());
}

// ── F.3 ports — console.cs:26067 + 26020-26057 [v2.10.3.13] ─────────────────

// alex_fwd_limit floor (default 5W): fwd <= 5W must NOT trip even if SWR > limit.
// console.cs:26067 [v2.10.3.13]:
//   if (swr > _swrProtectionLimit && alex_fwd > alex_fwd_limit && ...)
void TestSwrProtectionController::alexFwdLimit_default5W_belowFloor_noTrip()
{
    // Default m_alexFwdLimit = 5.0f; do not set it — verify the default.
    // fwd=4.9W, rev=3.5W → rho=sqrt(3.5/4.9)≈0.845 → swr≈(1.845/0.155)≈11.9 > limit(2.0)
    // But fwd(4.9) <= alexFwdLimit(5.0) → must NOT trip.
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(4.9f, 3.5f, false);
    }
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
    QVERIFY(!m_ctrl->highSwr());
}

// alex_fwd_limit floor (default 5W): fwd > 5W with high SWR DOES trip.
// console.cs:26067 [v2.10.3.13]
void TestSwrProtectionController::alexFwdLimit_default5W_aboveFloor_trips()
{
    // fwd=10W, rev=5.56W → swr≈6.86 > limit(2.0); fwd(10) > alexFwdLimit(5.0) → trips.
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(10.0f, 5.56f, false);
    }
    QVERIFY(m_ctrl->highSwr());
    QVERIFY(m_ctrl->windBackLatched());
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);
}

// ANAN-8000D variant: setAlexFwdLimit(2.0 * 50) = 100W floor.
// At fwd=99W with high SWR, no trip should fire.
// console.cs:26067 [v2.10.3.13]:
//   if (HardwareSpecific.Model == HPSDRModel.ANAN8000D)
//       alex_fwd_limit = 2.0f * (float)ptbPWR.Value;
void TestSwrProtectionController::alexFwdLimit_anan8000d_2xSlider_belowFloor_noTrip()
{
    // Simulate ANAN-8000D with power slider at 50 → floor = 100W.
    m_ctrl->setAlexFwdLimit(2.0f * 50.0f); // 100W floor
    // fwd=99W, rev=60W → swr≈7+ > limit(2.0), but fwd(99) <= floor(100) → no trip.
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(99.0f, 60.0f, false);
    }
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
    QVERIFY(!m_ctrl->highSwr());
}

// Tune-bypass with slider <= 70: bypass fires (swrPass=true).
// console.cs:26020-26057 [v2.10.3.13]:
//   if (alex_fwd >= 1.0f && alex_fwd <= _tunePowerSwrIgnore && tunePowerSliderValue <= 70)
//       swr_pass = true;
void TestSwrProtectionController::tuneBypass_sliderAt70_bypasses()
{
    m_ctrl->setDisableOnTune(true);
    m_ctrl->setTunePowerSwrIgnore(30.0f);
    m_ctrl->setTunePowerSliderValue(70); // exactly at the boundary — must bypass

    // fwd=10W (in [1, 30]), rev=5.56W → swr≈6.86 > limit; but tune bypass fires.
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(10.0f, 5.56f, /*tuneActive=*/true);
    }
    QCOMPARE(m_ctrl->protectFactor(), 1.0f);
    QVERIFY(!m_ctrl->highSwr());
}

// Tune-bypass with slider > 70: bypass does NOT fire — normal SWR protection applies.
// console.cs:26020-26057 [v2.10.3.13]
void TestSwrProtectionController::tuneBypass_sliderAt71_doesNotBypass()
{
    m_ctrl->setDisableOnTune(true);
    m_ctrl->setTunePowerSwrIgnore(30.0f);
    m_ctrl->setTunePowerSliderValue(71); // one above boundary — must NOT bypass

    // fwd=10W > m_alexFwdLimit(5.0), rev=5.56W → swr≈6.86 > limit → trips.
    for (int i = 0; i < 4; ++i) {
        m_ctrl->ingest(10.0f, 5.56f, /*tuneActive=*/true);
    }
    QVERIFY(m_ctrl->highSwr());
    QVERIFY(m_ctrl->windBackLatched());
    QCOMPARE(m_ctrl->protectFactor(), 0.01f);
}

QTEST_GUILESS_MAIN(TestSwrProtectionController)
#include "tst_swr_protection_controller.moc"
