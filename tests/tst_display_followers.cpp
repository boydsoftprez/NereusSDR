// no-port-check: NereusSDR-original. Tests of the display computations
// moved into src/core/spectrum/DisplayFollowers (iPhone app Task 20,
// R-IOS-27); the logic under test is the Thetis port that file carries.
// =================================================================
// tests/tst_display_followers.cpp  (NereusSDR)
// =================================================================
//
// The noise floor, the waterfall's automatic levels, the normalise shift,
// the averaging constant, the calibration offset's range and the peak-blob
// passband, as the desktop's SpectrumWidget ran them before the move and
// as the Core now runs them for an app. Each expectation is worked from
// the formula by hand, not by calling a second copy of it.
//
// =================================================================

#include <QtTest>

#include "core/spectrum/DisplayFollowers.h"

#include <cmath>

using namespace NereusSDR;

namespace {

QVector<float> flatRow(int width, float dbm)
{
    return QVector<float>(width, dbm);
}

} // namespace

class TstDisplayFollowers : public QObject {
    Q_OBJECT
private slots:
    // ── Averaging constant ──────────────────────────────────────────────

    void averageAlphaIsExpOfMinusOneOverFramesTimesTau()
    {
        // 30 ms at 30 frames a second: exp(-1 / (30 * 0.030)) = exp(-1/0.9).
        QCOMPARE(averageAlphaForTimeMs(30, 30),
                 static_cast<float>(std::exp(-1.0 / 0.9)));
        // 120 ms at 60 fps: exp(-1 / 7.2).
        QCOMPARE(averageAlphaForTimeMs(120, 60),
                 static_cast<float>(std::exp(-1.0 / 7.2)));
        // A time under 1 ms counts as 1 ms.
        QCOMPARE(averageAlphaForTimeMs(0, 30), averageAlphaForTimeMs(1, 30));
        QCOMPARE(averageAlphaForTimeMs(-50, 30), averageAlphaForTimeMs(1, 30));
    }

    void averageTimeClampsToTheSetupRange()
    {
        QCOMPARE(clampAverageTimeMs(5), 10);
        QCOMPARE(clampAverageTimeMs(10), 10);
        QCOMPARE(clampAverageTimeMs(500), 500);
        QCOMPARE(clampAverageTimeMs(20000), 9999);
    }

    // ── Normalise and calibration ───────────────────────────────────────

    void normalizeShiftIsMinusTenLogBinWidth()
    {
        QCOMPARE(normalizeShiftDb(false, 100.0), 0.0f);
        QCOMPARE(normalizeShiftDb(true, 100.0), -20.0f);
        QCOMPARE(normalizeShiftDb(true, 1.0), 0.0f);
        QCOMPARE(normalizeShiftDb(true, 0.0), 0.0f);
        QCOMPARE(normalizeShiftDb(true, -5.0), 0.0f);
        // 192 kHz over 4096 bins: 46.875 Hz a bin.
        QCOMPARE(normalizeShiftDb(true, 192000.0 / 4096.0),
                 -10.0f * std::log10(46.875f));
    }

    void calibrationOffsetClampsToThirtyDb()
    {
        QCOMPARE(clampCalibrationOffsetDb(-45.0f), -30.0f);
        QCOMPARE(clampCalibrationOffsetDb(12.5f), 12.5f);
        QCOMPARE(clampCalibrationOffsetDb(31.0f), 30.0f);
    }

    // ── Passband ────────────────────────────────────────────────────────

    void passbandPixelsFloorTheLowEdgeAndCeilTheHigh()
    {
        // 100 pixels over 0 +/- 50 Hz: 1 Hz a pixel, left edge -50 Hz.
        auto [lo, hi] = passbandPixels(100, 0.0, 100.0, -10.5, 10.5);
        QCOMPARE(lo, 39);   // floor(39.5)
        QCOMPARE(hi, 61);   // ceil(60.5)
        // Clamped to the row.
        std::tie(lo, hi) = passbandPixels(100, 0.0, 100.0, -500.0, 500.0);
        QCOMPARE(lo, 0);
        QCOMPARE(hi, 99);
        // No span: the whole row.
        std::tie(lo, hi) = passbandPixels(100, 0.0, 0.0, -10.0, 10.0);
        QCOMPARE(lo, 0);
        QCOMPARE(hi, 99);
    }

    // ── Noise floor ─────────────────────────────────────────────────────

    void noiseFloorShiftClampsToTwelveDb()
    {
        QCOMPARE(NoiseFloorFollower::clampShiftDb(-20.0f), -12.0f);
        QCOMPARE(NoiseFloorFollower::clampShiftDb(3.0f), 3.0f);
        QCOMPARE(NoiseFloorFollower::clampShiftDb(13.0f), 12.0f);
    }

    void firstFrameDriftsUpOneDbAndTheLineLags()
    {
        NoiseFloorFollower nf;
        QCOMPARE(nf.fftBinAverage(), -200.0f);
        QCOMPARE(nf.lerpAverage(), -200.0f);
        // No pixel is below -200, so the estimate drifts up 1 dB.
        QVERIFY(!nf.process(flatRow(100, -120.0f), 30, 0));
        QCOMPARE(nf.fftBinAverage(), -199.0f);
        // framesInAttack = int(30 / 1000 * 2000) + 1 = 61.
        QCOMPARE(nf.lerpAverage(), -200.0f + 1.0f / 61.0f);
    }

    void quietPixelsBlendInTheLinearDomain()
    {
        NoiseFloorFollower nf;
        // Drift the estimate above the row: 81 frames reach -119 dBm.
        for (int frame = 0; frame < 81; ++frame) {
            nf.process(flatRow(100, -120.0f), 30, 0);
        }
        QCOMPARE(nf.fftBinAverage(), -119.0f);
        // Every pixel (-120) is now below it: the average of -120 and -119
        // taken in power, 10*log10((10^-12 + 10^-11.9) / 2).
        nf.process(flatRow(100, -120.0f), 30, 0);
        const double expected = 10.0 * std::log10(
            (std::pow(10.0, -12.0) + static_cast<double>(std::pow(10.0f, -11.9f))) * 0.5);
        QVERIFY(std::abs(nf.fftBinAverage() - static_cast<float>(expected)) < 1.0e-3f);
    }

    void tooFewQuietPixelsKeepsDrifting()
    {
        NoiseFloorFollower nf;
        for (int frame = 0; frame < 81; ++frame) {
            nf.process(flatRow(100, -120.0f), 30, 0);
        }
        // 14 quiet pixels of 100 is under the 15 that sensitivity 3 needs.
        QVector<float> row = flatRow(100, -100.0f);
        for (int i = 0; i < 14; ++i) { row[i] = -130.0f; }
        nf.process(row, 30, 0);
        QCOMPARE(nf.fftBinAverage(), -118.0f);
    }

    void fastAttackDriftsThreeDbAndTheLineFollowsAtOnce()
    {
        NoiseFloorFollower nf;
        QVERIFY(nf.setFastAttack(true, 1000));
        QVERIFY(!nf.setFastAttack(true, 1000));
        QVERIFY(nf.fastAttack());
        nf.process(flatRow(100, -120.0f), 30, 1000);
        QCOMPARE(nf.fftBinAverage(), -197.0f);
        QCOMPARE(nf.lerpAverage(), -197.0f);
    }

    void fastAttackClearsOnlyOnceSettledAndASecondHasPassed()
    {
        NoiseFloorFollower nf;
        nf.setFastAttack(true, 10'000);
        // Converged at once (the line follows in one frame), but only
        // 1000 ms have passed: the flag stays.
        QVERIFY(!nf.process(flatRow(100, -120.0f), 30, 11'000));
        QVERIFY(nf.fastAttack());
        // Past a second: it clears, and says so once.
        QVERIFY(nf.process(flatRow(100, -120.0f), 30, 11'001));
        QVERIFY(!nf.fastAttack());
        QVERIFY(!nf.process(flatRow(100, -120.0f), 30, 11'002));
    }

    void retriggeringFastAttackRestartsItsSecond()
    {
        NoiseFloorFollower nf;
        nf.setFastAttack(true, 0);
        nf.setFastAttack(true, 5'000);
        QVERIFY(!nf.process(flatRow(100, -120.0f), 30, 5'500));
        QVERIFY(nf.fastAttack());
    }

    // ── Waterfall levels ────────────────────────────────────────────────

    void manualLevelsAreTheOperatorsOwn()
    {
        WaterfallLevelFollower follower;
        WaterfallLevelSettings settings;
        settings.lowDbm = -130.0f;
        settings.highDbm = -70.0f;
        float low = 0.0f;
        float high = 0.0f;
        follower.compose(flatRow(10, -100.0f), settings, low, high);
        QCOMPARE(low, -130.0f);
        QCOMPARE(high, -70.0f);
    }

    void agcPrimesOnTheRowThenFollowsAtFivePercent()
    {
        WaterfallLevelFollower follower;
        WaterfallLevelSettings settings;
        settings.agc = true;
        float low = 0.0f;
        float high = 0.0f;
        QVector<float> row = flatRow(10, -110.0f);
        row[3] = -60.0f;
        follower.compose(row, settings, low, high);
        QVERIFY(follower.agcPrimed());
        QCOMPARE(low, -110.0f - 12.0f);
        QCOMPARE(high, -60.0f + 12.0f);
        QVector<float> next = flatRow(10, -100.0f);
        next[5] = -40.0f;
        follower.compose(next, settings, low, high);
        QCOMPARE(low, 0.05f * -100.0f + 0.95f * -110.0f - 12.0f);
        QCOMPARE(high, 0.05f * -40.0f + 0.95f * -60.0f + 12.0f);
        follower.resetAgc();
        QVERIFY(!follower.agcPrimed());
        follower.compose(next, settings, low, high);
        QCOMPARE(low, -100.0f - 12.0f);
        QCOMPARE(high, -40.0f + 12.0f);
    }

    void noiseFloorAgcTakesTheTenthPercentileAndWinsOverAgc()
    {
        WaterfallLevelFollower follower;
        WaterfallLevelSettings settings;
        settings.agc = true;
        settings.noiseFloorAgc = true;
        settings.noiseFloorAgcOffsetDb = -5;
        QVector<float> row;
        for (int i = 0; i < 20; ++i) { row.append(-140.0f + static_cast<float>(i)); }
        float low = 0.0f;
        float high = 0.0f;
        follower.compose(row, settings, low, high);
        // Sorted, index 20 / 10 = 2 holds -138; plus -5.
        QCOMPARE(low, -143.0f);
        QCOMPARE(high, -143.0f + 60.0f);
        QCOMPARE(WaterfallLevelFollower::clampNoiseFloorAgcOffsetDb(-90), -60);
        QCOMPARE(WaterfallLevelFollower::clampNoiseFloorAgcOffsetDb(70), 60);
    }

    void clarityLeavesTheLevelsItSet()
    {
        WaterfallLevelFollower follower;
        WaterfallLevelSettings settings;
        settings.agc = true;
        settings.noiseFloorAgc = true;
        settings.clarityActive = true;
        float low = -125.0f;
        float high = -70.0f;
        follower.compose(flatRow(10, -100.0f), settings, low, high);
        QCOMPARE(low, -125.0f);
        QCOMPARE(high, -70.0f);
        QVERIFY(!follower.agcPrimed());
    }

    void anEmptyRowChangesNothing()
    {
        WaterfallLevelFollower follower;
        WaterfallLevelSettings settings;
        settings.agc = true;
        float low = -1.0f;
        float high = -2.0f;
        follower.compose({}, settings, low, high);
        QCOMPARE(low, -1.0f);
        QCOMPARE(high, -2.0f);
        NoiseFloorFollower nf;
        QVERIFY(!nf.process({}, 30, 0));
        QCOMPARE(nf.fftBinAverage(), -200.0f);
    }
};

QTEST_APPLESS_MAIN(TstDisplayFollowers)
#include "tst_display_followers.moc"
