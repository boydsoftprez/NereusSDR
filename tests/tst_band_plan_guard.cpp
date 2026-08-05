// no-port-check: test fixture asserting BandPlanGuard predicates against Thetis source rules
#include <QtTest>
#include "core/safety/BandPlanGuard.h"
#include "core/WdspTypes.h"
#include "models/Band.h"

using namespace NereusSDR;
using namespace NereusSDR::safety;

class TestBandPlanGuard : public QObject
{
    Q_OBJECT
private slots:
    void us60m_validChannelCenter_returnsTrue();
    void us60m_betweenChannels_returnsTrue();
    void us60m_usbDialChannel1_returnsTrue();
    void us60m_usbDialChannel5_returnsTrue();
    void us60m_belowBandEdge_returnsFalse();
    void us60m_aboveBandEdge_returnsFalse();
    void us60m_LSBmode_returnsFalse();
    void us60m_permittedModes_returnTrue();
    void uk60m_channel1_3kHz_returnsTrue();
    void uk60m_channel8_12_5kHz_returnsTrue();
    void japan60m_4_63MHz_returnsTrue();
    void japan60m_5_3MHz_returnsFalse();
    void extended_bypassesAllGuards_returnsTrue();
    void differentBandGuard_blocksMismatch_returnsFalse();
    void band20m_USB_validRange_returnsTrue();
    void band20m_USB_outOfBand_returnsFalse();
    void europe40m_inBand_returnsTrue();
    void europe40m_outOfBand_returnsFalse();
    void australia60m_wideRange_returnsTrue();
    void australia6m_outOfBand_returnsFalse();
    void spain20m_inBand_returnsTrue();
    void spain6m_outOfBand_returnsFalse();
};

void TestBandPlanGuard::us60m_validChannelCenter_returnsTrue()
{
    // US 60m channel 1 CW dial: 5.3320 MHz (channel center, FCC 47 CFR
    // 97.303(h) CW operating frequency).
    BandPlanGuard guard;
    auto result = guard.isValidTxFreq(
        Region::UnitedStates, 5'332'000, DSPMode::USB, /*extended=*/false);
    QVERIFY(result);
}

void TestBandPlanGuard::us60m_betweenChannels_returnsTrue()
{
    // Regression for issue #271: a USB dial between channel centers must still
    // pass once the dial sits inside the broad B60M 5.1-5.5 MHz range.
    // Thetis IsOKToTX (clsBandStackManager.cs:1063-1083 [v2.10.3.13]) accepts
    // the entire range, gated only by the US-specific USB/CWL/CWU/DIGU mode
    // restriction.
    BandPlanGuard guard;
    auto result = guard.isValidTxFreq(
        Region::UnitedStates, 5'340'000, DSPMode::USB, /*extended=*/false);
    QVERIFY(result);
}

void TestBandPlanGuard::us60m_usbDialChannel1_returnsTrue()
{
    // Regression for issue #271 (funsutton, ANAN-10E, macOS, 2026-05-18).
    // FCC 47 CFR 97.303(h) puts the US 60m channel 1 USB suppressed-carrier
    // dial at 5330.5 kHz (= channel center 5332.0 - 1.5 kHz). The previous
    // channelized gate rejected this dial because 5330.5 < 5330.6 (the lower
    // edge of channel_center ± 1.4 kHz). Must now pass.
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(
        Region::UnitedStates, 5'330'500, DSPMode::USB, /*extended=*/false));
}

void TestBandPlanGuard::us60m_usbDialChannel5_returnsTrue()
{
    // FCC 47 CFR 97.303(h) channel 5 USB dial: 5403.5 kHz (channel center
    // 5405.0 - 1.5 kHz). Same regression path as channel 1.
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(
        Region::UnitedStates, 5'403'500, DSPMode::USB, /*extended=*/false));
}

void TestBandPlanGuard::us60m_belowBandEdge_returnsFalse()
{
    // 5.099 MHz is below the US B60M lower edge of 5.1 MHz (kUsBandRanges).
    BandPlanGuard guard;
    QVERIFY(!guard.isValidTxFreq(
        Region::UnitedStates, 5'099'000, DSPMode::USB, /*extended=*/false));
}

void TestBandPlanGuard::us60m_aboveBandEdge_returnsFalse()
{
    // 5.501 MHz is above the US B60M upper edge of 5.5 MHz (kUsBandRanges).
    BandPlanGuard guard;
    QVERIFY(!guard.isValidTxFreq(
        Region::UnitedStates, 5'501'000, DSPMode::USB, /*extended=*/false));
}

void TestBandPlanGuard::us60m_LSBmode_returnsFalse()
{
    // US 60m allows USB / CWL / CWU / DIGU only (console.cs:29416-29432 [v2.10.3.13])
    BandPlanGuard guard;
    auto result = guard.isValidTxFreq(
        Region::UnitedStates, 5'332'000, DSPMode::LSB, /*extended=*/false);
    QVERIFY(!result);
}

void TestBandPlanGuard::us60m_permittedModes_returnTrue()
{
    // All four US 60m permitted modes pass on a valid channel center
    // (console.cs:29420-29423 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 5'332'000, DSPMode::USB,  false));
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 5'332'000, DSPMode::CWL,  false));
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 5'332'000, DSPMode::CWU,  false));
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 5'332'000, DSPMode::DIGU, false));
}

void TestBandPlanGuard::uk60m_channel1_3kHz_returnsTrue()
{
    // UK 60m channel 1: 5.26125 MHz, 5.5 kHz BW (console.cs:2644 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedKingdom, 5'261'250, DSPMode::USB, false));
}

void TestBandPlanGuard::uk60m_channel8_12_5kHz_returnsTrue()
{
    // UK 60m channel 8: 5.36825 MHz, 12.5 kHz BW (console.cs:2651 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedKingdom, 5'368'250, DSPMode::USB, false));
}

void TestBandPlanGuard::japan60m_4_63MHz_returnsTrue()
{
    // Japan 60m allocation 4.629995-4.630005 MHz
    // (clsBandStackManager.cs:1471 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::Japan, 4'630'000, DSPMode::USB, false));
}

void TestBandPlanGuard::japan60m_5_3MHz_returnsFalse()
{
    // Japan does NOT have IARU-style 5.3 MHz 60m allocation.
    // Thetis clsBandStackManager.cs Japan case has no 5.x entry other than 4.63.
    BandPlanGuard guard;
    QVERIFY(!guard.isValidTxFreq(Region::Japan, 5'332'000, DSPMode::USB, false));
}

void TestBandPlanGuard::extended_bypassesAllGuards_returnsTrue()
{
    // chkExtended short-circuits to true for any frequency
    // (console.cs:6772 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 13'500'000, DSPMode::USB, /*extended=*/true));
}

void TestBandPlanGuard::differentBandGuard_blocksMismatch_returnsFalse()
{
    // VFO-A on 20m, VFO-B-TX on 40m, _preventTXonDifferentBandToRXband ON
    // (console.cs:29401-29414 [2.9.0.7]MW0LGE)
    BandPlanGuard guard;
    QVERIFY(!guard.isValidTxBand(
        Band::Band20m, Band::Band40m, /*preventDifferentBand=*/true));
}

void TestBandPlanGuard::band20m_USB_validRange_returnsTrue()
{
    // 14.200 MHz is squarely in the US 20m band (14.000-14.350 MHz)
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::UnitedStates, 14'200'000, DSPMode::USB, false));
}

void TestBandPlanGuard::band20m_USB_outOfBand_returnsFalse()
{
    // 14.500 MHz is above the US 20m band edge of 14.350 MHz
    BandPlanGuard guard;
    QVERIFY(!guard.isValidTxFreq(Region::UnitedStates, 14'500'000, DSPMode::USB, false));
}

void TestBandPlanGuard::europe40m_inBand_returnsTrue()
{
    // 7.100 MHz is squarely in the Europe 40m band (7.000-7.200 MHz)
    // (clsBandStackManager.cs:1109 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::Europe, 7'100'000, DSPMode::USB, false));
}

void TestBandPlanGuard::europe40m_outOfBand_returnsFalse()
{
    // 7.250 MHz is above the Europe 40m band edge of 7.200 MHz
    // (clsBandStackManager.cs:1109 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(!guard.isValidTxFreq(Region::Europe, 7'250'000, DSPMode::USB, false));
}

void TestBandPlanGuard::australia60m_wideRange_returnsTrue()
{
    // Australia has a wide 60m allocation 5.000-7.000 MHz with no channelization
    // (clsBandStackManager.cs:1485 [v2.10.3.13]). With C1 fixed, Region::Australia
    // returns an empty channels span, so 5.500 MHz falls through to the band-range
    // check where the B60M row (not skipped for unchannelized regions) matches.
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::Australia, 5'500'000, DSPMode::USB, false));
}

void TestBandPlanGuard::australia6m_outOfBand_returnsFalse()
{
    // 54.500 MHz is above Australia's 6m band edge of 54.000 MHz
    // (clsBandStackManager.cs:1499 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(!guard.isValidTxFreq(Region::Australia, 54'500'000, DSPMode::USB, false));
}

void TestBandPlanGuard::spain20m_inBand_returnsTrue()
{
    // 14.200 MHz is in the Spain 20m band (14.000-14.350 MHz)
    // (clsBandStackManager.cs:1455 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(guard.isValidTxFreq(Region::Spain, 14'200'000, DSPMode::USB, false));
}

void TestBandPlanGuard::spain6m_outOfBand_returnsFalse()
{
    // 52.500 MHz is above Spain's 6m band edge of 52.000 MHz
    // (clsBandStackManager.cs:1464 [v2.10.3.13])
    BandPlanGuard guard;
    QVERIFY(!guard.isValidTxFreq(Region::Spain, 52'500'000, DSPMode::USB, false));
}

QTEST_GUILESS_MAIN(TestBandPlanGuard)
#include "tst_band_plan_guard.moc"
