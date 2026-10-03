// no-port-check: test-only — exercises NereusSDR-native BandPlanGuard mode allow-list
// 3M-1b Task K.1: isModeAllowedForTx + checkMoxAllowed parametrized over all 12 DSPModes.
#include <QtTest>
#include "core/safety/BandPlanGuard.h"
#include "core/WdspTypes.h"
#include "models/Band.h"

using namespace NereusSDR;
using namespace NereusSDR::safety;

class TestBandPlanGuardModeAllowList : public QObject
{
    Q_OBJECT
private slots:
    // ── isModeAllowedForTx: allowed modes ──────────────────────────────────
    void lsb_isAllowed();
    void usb_isAllowed();
    void digl_isAllowed();
    void digu_isAllowed();
    void radeU_isAllowed();   // Phase 3R K-bench: ride USB carrier
    void radeL_isAllowed();   // Phase 3R K-bench: ride LSB carrier

    // ── isModeAllowedForTx: rejected modes ────────────────────────────────
    void cwl_isRejected();
    void cwu_isRejected();
    void am_isAllowed();
    void sam_isAllowed();
    void dsb_isAllowed();
    void fm_isRejected();
    void drm_isRejected();
    void spec_isRejected();

    // ── checkMoxAllowed: reason strings ───────────────────────────────────
    void cwl_checkMox_reasonIsCwPhase();
    void cwu_checkMox_reasonIsCwPhase();
    void am_checkMox_isAllowed();
    void sam_checkMox_isAllowed();
    void dsb_checkMox_isAllowed();
    void fm_checkMox_reasonIsAudioModes();
    void drm_checkMox_reasonIsAudioModes();
    void spec_checkMox_reasonIsNotSupported();

    // ── checkMoxAllowed: ok path ───────────────────────────────────────────
    void lsb_validFreq_returnsOk();
    void usb_validFreq_returnsOk();

    // ── checkMoxAllowed: freq/band reject on allowed mode ─────────────────
    void usb_outOfBandFreq_returnsFreqReject();
    void lsb_crossBandTx_returnsBandReject();
    void crossBandTx_isCheckedBeforeTheBandEdges();
    void refusalsSayWhatIsWrong_data();
    void refusalsSayWhatIsWrong();
};

// ---------------------------------------------------------------------------
// isModeAllowedForTx — allowed modes (expect true)
// ---------------------------------------------------------------------------

void TestBandPlanGuardModeAllowList::lsb_isAllowed()
{
    BandPlanGuard guard;
    QVERIFY(guard.isModeAllowedForTx(DSPMode::LSB));
}

void TestBandPlanGuardModeAllowList::usb_isAllowed()
{
    BandPlanGuard guard;
    QVERIFY(guard.isModeAllowedForTx(DSPMode::USB));
}

void TestBandPlanGuardModeAllowList::digl_isAllowed()
{
    BandPlanGuard guard;
    QVERIFY(guard.isModeAllowedForTx(DSPMode::DIGL));
}

void TestBandPlanGuardModeAllowList::digu_isAllowed()
{
    BandPlanGuard guard;
    QVERIFY(guard.isModeAllowedForTx(DSPMode::DIGU));
}

// Phase 3R K-bench: RADE_U / RADE_L ride USB / LSB carriers respectively.
// User bench-reported "MOX + RADE-U produces no RF / TUNE does not work"
// traced to BandPlanGuard rejecting RADE_U/L as "Mode not supported for TX"
// (default-case fallthrough). Band-plan etiquette equivalent to DIGU/DIGL.
void TestBandPlanGuardModeAllowList::radeU_isAllowed()
{
    BandPlanGuard guard;
    QVERIFY(guard.isModeAllowedForTx(DSPMode::RADE_U));
}

void TestBandPlanGuardModeAllowList::radeL_isAllowed()
{
    BandPlanGuard guard;
    QVERIFY(guard.isModeAllowedForTx(DSPMode::RADE_L));
}

// ---------------------------------------------------------------------------
// isModeAllowedForTx — rejected modes (expect false)
// ---------------------------------------------------------------------------

void TestBandPlanGuardModeAllowList::cwl_isRejected()
{
    BandPlanGuard guard;
    QVERIFY(!guard.isModeAllowedForTx(DSPMode::CWL));
}

void TestBandPlanGuardModeAllowList::cwu_isRejected()
{
    BandPlanGuard guard;
    QVERIFY(!guard.isModeAllowedForTx(DSPMode::CWU));
}

void TestBandPlanGuardModeAllowList::am_isAllowed()
{
    BandPlanGuard guard;
    QVERIFY(guard.isModeAllowedForTx(DSPMode::AM));
}

void TestBandPlanGuardModeAllowList::sam_isAllowed()
{
    BandPlanGuard guard;
    QVERIFY(guard.isModeAllowedForTx(DSPMode::SAM));
}

void TestBandPlanGuardModeAllowList::dsb_isAllowed()
{
    BandPlanGuard guard;
    QVERIFY(guard.isModeAllowedForTx(DSPMode::DSB));
}

void TestBandPlanGuardModeAllowList::fm_isRejected()
{
    BandPlanGuard guard;
    QVERIFY(!guard.isModeAllowedForTx(DSPMode::FM));
}

void TestBandPlanGuardModeAllowList::drm_isRejected()
{
    BandPlanGuard guard;
    QVERIFY(!guard.isModeAllowedForTx(DSPMode::DRM));
}

void TestBandPlanGuardModeAllowList::spec_isRejected()
{
    BandPlanGuard guard;
    QVERIFY(!guard.isModeAllowedForTx(DSPMode::SPEC));
}

// ---------------------------------------------------------------------------
// checkMoxAllowed — reason strings for each rejection class
// Use US 20m (14.200 MHz) as the freq for all mode-rejection cases so the
// freq check is never the blocking factor.
// ---------------------------------------------------------------------------

static constexpr Region   kRegion  = Region::UnitedStates;
static constexpr std::int64_t kValidHz = 14'200'000; // US 20m, well in-band
static constexpr Band     kBand20m = Band::Band20m;

void TestBandPlanGuardModeAllowList::cwl_checkMox_reasonIsCwPhase()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::CWL,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("CW transmit is not available on this Core"));
}

void TestBandPlanGuardModeAllowList::cwu_checkMox_reasonIsCwPhase()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::CWU,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("CW transmit is not available on this Core"));
}

void TestBandPlanGuardModeAllowList::am_checkMox_isAllowed()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::AM,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(r.ok);
    QVERIFY(r.reason.isEmpty());
}

void TestBandPlanGuardModeAllowList::sam_checkMox_isAllowed()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::SAM,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(r.ok);
    QVERIFY(r.reason.isEmpty());
}

void TestBandPlanGuardModeAllowList::dsb_checkMox_isAllowed()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::DSB,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(r.ok);
    QVERIFY(r.reason.isEmpty());
}

void TestBandPlanGuardModeAllowList::fm_checkMox_reasonIsAudioModes()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::FM,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("FM transmit is not available on this Core"));
}

void TestBandPlanGuardModeAllowList::drm_checkMox_reasonIsAudioModes()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::DRM,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("DRM transmit is not available on this Core"));
}

void TestBandPlanGuardModeAllowList::spec_checkMox_reasonIsNotSupported()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::SPEC,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("This mode cannot transmit."));
}

// ---------------------------------------------------------------------------
// checkMoxAllowed — ok path (LSB/USB + valid freq + same band)
// ---------------------------------------------------------------------------

void TestBandPlanGuardModeAllowList::lsb_validFreq_returnsOk()
{
    // LSB on US 20m (14.200 MHz), same RX/TX band, preventDifferentBand=false
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::LSB,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(r.ok);
    QVERIFY(r.reason.isEmpty());
}

void TestBandPlanGuardModeAllowList::usb_validFreq_returnsOk()
{
    // USB on US 20m (14.200 MHz), same RX/TX band, preventDifferentBand=false
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::USB,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(r.ok);
    QVERIFY(r.reason.isEmpty());
}

// ---------------------------------------------------------------------------
// checkMoxAllowed — freq/band reject on an otherwise-allowed mode
// ---------------------------------------------------------------------------

void TestBandPlanGuardModeAllowList::usb_outOfBandFreq_returnsFreqReject()
{
    // 14.500 MHz is above the US 20m band edge (14.350 MHz) — freq check blocks
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, 14'500'000, DSPMode::USB,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral(
        "14.500000 MHz is outside the transmit bands for your region (United States)."));
}

void TestBandPlanGuardModeAllowList::lsb_crossBandTx_returnsBandReject()
{
    // LSB, valid 20m freq, but TX band is 40m with preventDifferentBand=true;
    // rxBand is the band of the device's other slice.
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::LSB,
                                   kBand20m, Band::Band40m, /*preventDifferentBand=*/true, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral(
        "Transmit would be on 40 m while another slice you have open is on 20 m, and "
        "Setup is set to prevent transmitting on a different band."));
}

// Thetis checks the different band before the US 60 m mode rule and
// CheckValidTXFreq (console.cs:29451, :29467, :29486 [v2.10.3.15]).
// General coverage is named in words.
void TestBandPlanGuardModeAllowList::crossBandTx_isCheckedBeforeTheBandEdges()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, 14'500'000, DSPMode::USB,
                                   Band::GEN, kBand20m, /*preventDifferentBand=*/true, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral(
        "Transmit would be on 20 m while another slice you have open is on general "
        "coverage, and Setup is set to prevent transmitting on a different band."));
    QVERIFY(r.refusalCode.isEmpty());
    r = guard.checkMoxAllowed(kRegion, 5'357'000, DSPMode::AM,
                              kBand20m, Band::Band60m, /*preventDifferentBand=*/true, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral(
        "Transmit would be on 60 m while another slice you have open is on 20 m, and "
        "Setup is set to prevent transmitting on a different band."));
    // Off: the band edges refuse as before.
    r = guard.checkMoxAllowed(kRegion, 14'500'000, DSPMode::USB,
                              Band::GEN, kBand20m, /*preventDifferentBand=*/false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral(
        "14.500000 MHz is outside the transmit bands for your region (United States)."));
}

// Addendum G-42 item 4: each band plan refusal says what is wrong in the
// operator's words, following Thetis's messages (console.cs:29452-29530
// [v2.10.3.15]): the filter edges, the carrier, the US 60 m mode rule and
// the US 60 m 2.8 kHz filter limit.
void TestBandPlanGuardModeAllowList::refusalsSayWhatIsWrong_data()
{
    QTest::addColumn<qint64>("hz");
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("band");
    QTest::addColumn<int>("low");
    QTest::addColumn<int>("high");
    QTest::addColumn<bool>("tune");
    QTest::addColumn<int>("region");
    QTest::addColumn<QString>("reason");
    QTest::newRow("usb-filter-edge") << qint64(14'349'000) << int(DSPMode::USB)
        << int(Band::Band20m) << 100 << 2900 << false << int(Region::UnitedStates)
        << QStringLiteral("14.349000 MHz with the transmit filter from 100 to 2900 Hz "
                          "reaches outside the transmit bands for your region (United States).");
    QTest::newRow("lsb-filter-edge") << qint64(14'001'000) << int(DSPMode::LSB)
        << int(Band::Band20m) << -2900 << -100 << false << int(Region::UnitedStates)
        << QStringLiteral("14.001000 MHz with the transmit filter from -2900 to -100 Hz "
                          "reaches outside the transmit bands for your region (United States).");
    QTest::newRow("tune-carrier") << qint64(14'360'000) << int(DSPMode::USB)
        << int(Band::Band20m) << 100 << 2900 << true << int(Region::UnitedStates)
        << QStringLiteral("14.360000 MHz is outside the transmit bands for your region "
                          "(United States).");
    QTest::newRow("europe-carrier") << qint64(7'250'000) << int(DSPMode::USB)
        << int(Band::Band40m) << 0 << 0 << false << int(Region::Europe)
        << QStringLiteral("7.250000 MHz is outside the transmit bands for your region (Europe).");
    // The three IARU regions in operator words, not the settings' Region1-3.
    QTest::newRow("iaru-region-1") << qint64(7'250'000) << int(DSPMode::USB)
        << int(Band::Band40m) << 0 << 0 << false << int(Region::Region1)
        << QStringLiteral("7.250000 MHz is outside the transmit bands for your region "
                          "(IARU Region 1).");
    QTest::newRow("iaru-region-3") << qint64(7'350'000) << int(DSPMode::USB)
        << int(Band::Band40m) << 0 << 0 << false << int(Region::Region3)
        << QStringLiteral("7.350000 MHz is outside the transmit bands for your region "
                          "(IARU Region 3).");
    QTest::newRow("us-60m-mode") << qint64(5'357'000) << int(DSPMode::AM)
        << int(Band::Band60m) << -2900 << 2900 << false << int(Region::UnitedStates)
        << QStringLiteral("AM is not allowed on 60 m in the United States.");
    QTest::newRow("us-60m-filter") << qint64(5'499'000) << int(DSPMode::USB)
        << int(Band::Band60m) << 100 << 2900 << false << int(Region::UnitedStates)
        << QStringLiteral("The transmit filter is wider than the 2.8 kHz allowed on 60 m "
                          "in the United States.");
}

void TestBandPlanGuardModeAllowList::refusalsSayWhatIsWrong()
{
    QFETCH(qint64, hz);
    QFETCH(int, mode);
    QFETCH(int, band);
    QFETCH(int, low);
    QFETCH(int, high);
    QFETCH(bool, tune);
    QFETCH(int, region);
    QFETCH(QString, reason);
    BandPlanGuard guard;
    const auto r = guard.checkMoxAllowed(static_cast<Region>(region), hz,
                                         static_cast<DSPMode>(mode), static_cast<Band>(band),
                                         static_cast<Band>(band), false, false, low, high, tune);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, reason);
    // Extended lets each of them through (console.cs:6780 [v2.10.3.15]).
    QVERIFY(guard.checkMoxAllowed(static_cast<Region>(region), hz, static_cast<DSPMode>(mode),
                                  static_cast<Band>(band), static_cast<Band>(band), false,
                                  /*extended=*/true, low, high, tune).ok);
}

QTEST_GUILESS_MAIN(TestBandPlanGuardModeAllowList)
#include "tst_band_plan_guard_mode_allow_list.moc"
