// no-port-check: NereusSDR-original unit-test file.  The mi0bot-Thetis
// references below are cite comments documenting which upstream lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_hl2_audio_volume.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::computeAudioVolume — HL2 audio-volume formula.
//
// Spec §5.4.2; Plan Task 5; Issue #175.
//
// Reference: from mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2]:
//
//     Audio.RadioVolume = (double)Math.Min((hl2Power * (gbb / 100)) / 93.75, 1.0);
//
// Branch order matters: HL2 path runs BEFORE the existing gbb >= 99.5
// sentinel-fallback short-circuit, so HL2 HF bands (where gbb=100.0f per
// kHermesliteRow) actually get mi0bot's formula instead of falling into
// the legacy linear path.
//
// Cases:
//   §1 HL2 HF band (40m, gbb=100) at full slider — clamps to 1.0.
//   §2 HL2 HF band (40m, gbb=100) at zero slider — returns 0.0.
//   §3 HL2 HF band (40m, gbb=100) at mid slider — exact formula value.
//   §4 HL2 6m (gbb=38.8) at full slider — formula divided by 93.75.
//   §5 Non-HL2 model on 40m — does NOT use mi0bot formula (legacy path).
// =================================================================

#include <QtTest/QtTest>

#include <cmath>

#include "core/HpsdrModel.h"
#include "core/PaProfile.h"
#include "models/Band.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestTransmitModelHl2AudioVolume : public QObject {
    Q_OBJECT

private slots:

    // §1 HL2 HF band at full slider.
    // HF bands have gbb=100.0f (sentinel) on HL2 per kHermesliteRow.
    // mi0bot: (99 * 100/100) / 93.75 = 99/93.75 = 1.056 -> clamped to 1.0.
    void hl2_hf_band_at_full() {
        TransmitModel m;
        const PaProfile p(QStringLiteral("Default - HERMESLITE"),
                          HPSDRModel::HERMESLITE, true);
        const double v = m.computeAudioVolume(p,
                                              Band::Band40m,
                                              99,
                                              HPSDRModel::HERMESLITE);
        QVERIFY(qFuzzyCompare(v, 1.0));
    }

    // §2 HL2 HF band at zero slider.
    // mi0bot: sliderWatts<=0 short-circuit returns 0.0 (kept in front of
    // the HL2 branch — see implementation in TransmitModel::computeAudioVolume).
    void hl2_hf_band_at_zero() {
        TransmitModel m;
        const PaProfile p(QStringLiteral("Default - HERMESLITE"),
                          HPSDRModel::HERMESLITE, true);
        const double v = m.computeAudioVolume(p,
                                              Band::Band40m,
                                              0,
                                              HPSDRModel::HERMESLITE);
        // qFuzzyCompare-around-zero idiom (see Qt docs).
        QVERIFY(qFuzzyCompare(v + 1.0, 0.0 + 1.0));
    }

    // §3 HL2 HF band at mid slider.
    // mi0bot: (50 * 100/100) / 93.75 = 50/93.75 ~ 0.53333.
    void hl2_hf_band_at_mid() {
        TransmitModel m;
        const PaProfile p(QStringLiteral("Default - HERMESLITE"),
                          HPSDRModel::HERMESLITE, true);
        const double v = m.computeAudioVolume(p,
                                              Band::Band40m,
                                              50,
                                              HPSDRModel::HERMESLITE);
        QVERIFY(std::abs(v - (50.0 / 93.75)) < 1e-9);
    }

    // §4 HL2 6m at full slider.
    // 6m has a real gain entry (gbb=38.8) on HL2.
    // mi0bot: (99 * 38.8/100) / 93.75 ~ 0.4097.
    void hl2_6m_at_full() {
        TransmitModel m;
        const PaProfile p(QStringLiteral("Default - HERMESLITE"),
                          HPSDRModel::HERMESLITE, true);
        const double v = m.computeAudioVolume(p,
                                              Band::Band6m,
                                              99,
                                              HPSDRModel::HERMESLITE);
        QVERIFY(std::abs(v - ((99.0 * 0.388) / 93.75)) < 1e-6);
    }

    // §5 Non-HL2 model — must NOT use the HL2 formula.  ANAN-100 (or any
    // non-HERMESLITE model) should run through the legacy gbb >= 99.5
    // sentinel fallback / dBm-target math.  Concretely: assert the result
    // differs from what the HL2 formula would have returned.
    //
    // Build a profile whose 40m gbb=100 (sentinel) so the legacy path
    // hits the linear fallback (50/100 = 0.5), which differs from the HL2
    // formula value of (50 * 100/100)/93.75 = 0.5333.
    void nonHl2_unchanged_path() {
        TransmitModel m;
        // ANAN100 default profile: kAnan100Row 40m has a real gain entry,
        // not a sentinel.  Use a Bypass-style construction (FIRST inherits
        // HERMES values which are NOT all-100; we override via setGain).
        PaProfile p(QStringLiteral("Bypass"), HPSDRModel::FIRST, true);
        p.setGainForBand(Band::Band40m, 100.0f);
        const double vHl2Formula = (50.0 * 100.0 / 100.0) / 93.75;
        const double vReal = m.computeAudioVolume(p,
                                                  Band::Band40m,
                                                  50,
                                                  HPSDRModel::ANAN100);
        QVERIFY(std::abs(vReal - vHl2Formula) > 1e-6);
        // And the legacy linear fallback returns 50/100 = 0.5 exactly.
        QCOMPARE(vReal, 0.5);
    }
};

QTEST_MAIN(TestTransmitModelHl2AudioVolume)
#include "tst_transmit_model_hl2_audio_volume.moc"
