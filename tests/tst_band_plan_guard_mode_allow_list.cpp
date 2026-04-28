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

    // ── isModeAllowedForTx: rejected modes ────────────────────────────────
    void cwl_isRejected();
    void cwu_isRejected();
    void am_isRejected();
    void sam_isRejected();
    void dsb_isRejected();
    void fm_isRejected();
    void drm_isRejected();
    void spec_isRejected();

    // ── checkMoxAllowed: reason strings ───────────────────────────────────
    void cwl_checkMox_reasonIsCwPhase();
    void cwu_checkMox_reasonIsCwPhase();
    void am_checkMox_reasonIsAudioModes();
    void sam_checkMox_reasonIsAudioModes();
    void dsb_checkMox_reasonIsAudioModes();
    void fm_checkMox_reasonIsAudioModes();
    void drm_checkMox_reasonIsAudioModes();
    void spec_checkMox_reasonIsNotSupported();

    // ── checkMoxAllowed: ok path ───────────────────────────────────────────
    void lsb_validFreq_returnsOk();
    void usb_validFreq_returnsOk();

    // ── checkMoxAllowed: freq/band reject on allowed mode ─────────────────
    void usb_outOfBandFreq_returnsFreqReject();
    void lsb_crossBandTx_returnsBandReject();
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

void TestBandPlanGuardModeAllowList::am_isRejected()
{
    BandPlanGuard guard;
    QVERIFY(!guard.isModeAllowedForTx(DSPMode::AM));
}

void TestBandPlanGuardModeAllowList::sam_isRejected()
{
    BandPlanGuard guard;
    QVERIFY(!guard.isModeAllowedForTx(DSPMode::SAM));
}

void TestBandPlanGuardModeAllowList::dsb_isRejected()
{
    BandPlanGuard guard;
    QVERIFY(!guard.isModeAllowedForTx(DSPMode::DSB));
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
    QCOMPARE(r.reason, QStringLiteral("CW TX coming in Phase 3M-2"));
}

void TestBandPlanGuardModeAllowList::cwu_checkMox_reasonIsCwPhase()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::CWU,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("CW TX coming in Phase 3M-2"));
}

void TestBandPlanGuardModeAllowList::am_checkMox_reasonIsAudioModes()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::AM,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
}

void TestBandPlanGuardModeAllowList::sam_checkMox_reasonIsAudioModes()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::SAM,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
}

void TestBandPlanGuardModeAllowList::dsb_checkMox_reasonIsAudioModes()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::DSB,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
}

void TestBandPlanGuardModeAllowList::fm_checkMox_reasonIsAudioModes()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::FM,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
}

void TestBandPlanGuardModeAllowList::drm_checkMox_reasonIsAudioModes()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::DRM,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
}

void TestBandPlanGuardModeAllowList::spec_checkMox_reasonIsNotSupported()
{
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::SPEC,
                                   kBand20m, kBand20m, false, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("Mode not supported for TX"));
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
    QCOMPARE(r.reason, QStringLiteral("Frequency outside TX-allowed range"));
}

void TestBandPlanGuardModeAllowList::lsb_crossBandTx_returnsBandReject()
{
    // LSB, valid 20m freq, but TX band is 40m with preventDifferentBand=true
    BandPlanGuard guard;
    auto r = guard.checkMoxAllowed(kRegion, kValidHz, DSPMode::LSB,
                                   kBand20m, Band::Band40m, /*preventDifferentBand=*/true, false);
    QVERIFY(!r.ok);
    QCOMPARE(r.reason, QStringLiteral("RX/TX band mismatch — cross-band TX disabled"));
}

QTEST_GUILESS_MAIN(TestBandPlanGuardModeAllowList)
#include "tst_band_plan_guard_mode_allow_list.moc"
