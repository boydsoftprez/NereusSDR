// =================================================================
// tests/tst_p1_mic_decimation.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. Verifies the mic sample decimation
// helper P1RadioConnection::decimateMicSamples that downsamples
// radio-rate mic (sample-rate Hz) to 48 kHz before feeding TxMicSource.
//
// Source: Thetis ChannelMaster/networkproto1.c:391-410 [v2.10.3.14]
//         Thetis ChannelMaster/netInterface.c:1287-1310 [v2.10.3.14]
//
// Test cases:
//   - factor1_passesAllSamplesThrough             (48 kHz, no decimation)
//   - factor4_keepsEverySampleAtIndex3_7_etc      (192 kHz path)
//   - factor4_partialFrameDoesNotEmitSample       (counter persists across calls)
//   - factor4_counterResumesAcrossCalls           (cross-frame continuity)
//   - factor2_keepsHalfTheSamples                 (96 kHz path)
//   - factor8_keepsEighthOfSamples                (384 kHz path)
//   - emptyInput_emitsNothing                     (degenerate case)
//   - factorChange_resetsCounter                  (sample-rate change parity)
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-01 — New test for HL2 mic decimation by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. No Thetis logic ported.

#include <QtTest/QtTest>
#include <QObject>

#include <vector>

#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1MicDecimation : public QObject {
    Q_OBJECT

private slots:

    // ── factor=1 (48 kHz): every sample passes through ─────────────────────
    void factor1_passesAllSamplesThrough()
    {
        const std::vector<float> in{ 0.1f, 0.2f, 0.3f, 0.4f, 0.5f };
        std::vector<float> out;
        int counter = 0;
        P1RadioConnection::decimateMicSamples(in.data(), int(in.size()),
                                              /*factor=*/1, counter, out);
        QCOMPARE(static_cast<int>(out.size()), 5);
        for (size_t i = 0; i < in.size(); ++i) {
            QCOMPARE(out[i], in[i]);
        }
    }

    // ── factor=4 (192 kHz): keep every 4th sample ──────────────────────────
    //
    // Thetis pattern (networkproto1.c:391-410 [v2.10.3.14]):
    //   counter starts at 0;
    //   for each sample: counter++; if (counter == factor) { counter=0; keep; }
    //
    // With factor=4 and counter starting at 0, the kept indices are
    // 3, 7, 11, ... (i.e. the 4th, 8th, 12th sample — 0-based 3, 7, 11).
    void factor4_keepsEverySampleAtIndex3_7_etc()
    {
        std::vector<float> in;
        for (int i = 0; i < 12; ++i) {
            in.push_back(static_cast<float>(i));  // 0..11
        }
        std::vector<float> out;
        int counter = 0;
        P1RadioConnection::decimateMicSamples(in.data(), int(in.size()),
                                              /*factor=*/4, counter, out);
        QCOMPARE(static_cast<int>(out.size()), 3);
        QCOMPARE(out[0], 3.0f);   // index 3 (4th sample)
        QCOMPARE(out[1], 7.0f);   // index 7 (8th sample)
        QCOMPARE(out[2], 11.0f);  // index 11 (12th sample)
        QCOMPARE(counter, 0);     // landed exactly on factor boundary
    }

    // ── factor=4: partial frame leaves counter advanced, no emission ───────
    void factor4_partialFrameDoesNotEmitSample()
    {
        const std::vector<float> in{ 1.0f, 2.0f, 3.0f };
        std::vector<float> out;
        int counter = 0;
        P1RadioConnection::decimateMicSamples(in.data(), int(in.size()),
                                              /*factor=*/4, counter, out);
        QCOMPARE(static_cast<int>(out.size()), 0);
        QCOMPARE(counter, 3);     // advanced to 3, not yet at factor=4
    }

    // ── factor=4: counter persistence across two calls (cross-frame) ───────
    //
    // Mirrors Thetis behaviour: mic_decimation_count is a global static reset
    // ONCE in MetisReadThreadMainLoop init (networkproto1.c:275 [v2.10.3.14])
    // and persists across every Inbound() call.
    void factor4_counterResumesAcrossCalls()
    {
        std::vector<float> out;
        int counter = 0;

        // Call 1: 3 samples → 0 emitted, counter=3.
        const std::vector<float> first{ 1.0f, 2.0f, 3.0f };
        P1RadioConnection::decimateMicSamples(first.data(), int(first.size()),
                                              /*factor=*/4, counter, out);
        QCOMPARE(static_cast<int>(out.size()), 0);
        QCOMPARE(counter, 3);

        // Call 2: 1 sample → 1 emitted (counter wraps to 0).
        const std::vector<float> second{ 4.0f };
        P1RadioConnection::decimateMicSamples(second.data(), int(second.size()),
                                              /*factor=*/4, counter, out);
        QCOMPARE(static_cast<int>(out.size()), 1);
        QCOMPARE(out[0], 4.0f);
        QCOMPARE(counter, 0);

        // Call 3: 8 more samples → 2 emitted at the 4th and 8th of this call.
        std::vector<float> third;
        for (int i = 5; i < 13; ++i) {
            third.push_back(static_cast<float>(i));
        }
        P1RadioConnection::decimateMicSamples(third.data(), int(third.size()),
                                              /*factor=*/4, counter, out);
        QCOMPARE(static_cast<int>(out.size()), 3);
        QCOMPARE(out[1], 8.0f);   // index 3 of third = value 8 (5,6,7,8 — keep 8)
        QCOMPARE(out[2], 12.0f);  // index 7 of third = value 12
        QCOMPARE(counter, 0);
    }

    // ── factor=2 (96 kHz): keep half the samples ───────────────────────────
    void factor2_keepsHalfTheSamples()
    {
        std::vector<float> in;
        for (int i = 0; i < 100; ++i) {
            in.push_back(static_cast<float>(i));
        }
        std::vector<float> out;
        int counter = 0;
        P1RadioConnection::decimateMicSamples(in.data(), int(in.size()),
                                              /*factor=*/2, counter, out);
        QCOMPARE(static_cast<int>(out.size()), 50);
        // First kept = index 1, last kept = index 99
        QCOMPARE(out.front(), 1.0f);
        QCOMPARE(out.back(),  99.0f);
    }

    // ── factor=8 (384 kHz): keep one in eight ──────────────────────────────
    void factor8_keepsEighthOfSamples()
    {
        std::vector<float> in;
        for (int i = 0; i < 64; ++i) {
            in.push_back(static_cast<float>(i));
        }
        std::vector<float> out;
        int counter = 0;
        P1RadioConnection::decimateMicSamples(in.data(), int(in.size()),
                                              /*factor=*/8, counter, out);
        QCOMPARE(static_cast<int>(out.size()), 8);
        QCOMPARE(out[0],  7.0f);
        QCOMPARE(out[7], 63.0f);
    }

    // ── empty input is a no-op ─────────────────────────────────────────────
    void emptyInput_emitsNothing()
    {
        std::vector<float> out;
        int counter = 0;
        P1RadioConnection::decimateMicSamples(/*in=*/nullptr, 0,
                                              /*factor=*/4, counter, out);
        QCOMPARE(static_cast<int>(out.size()), 0);
        QCOMPARE(counter, 0);
    }

    // ── sample-rate change parity: setSampleRate updates factor + resets counter
    //
    // Thetis netInterface.c:1287-1310 [v2.10.3.14] sets mic_decimation_factor
    // from sampleRate (1/2/4/8 for 48/96/192/384) and immediately resets
    // mic_decimation_count = 0 (line 1310). NereusSDR mirrors this in
    // P1RadioConnection::setSampleRate.
    void sampleRateChange_updatesFactorAndResetsCounter()
    {
        P1RadioConnection conn;
        conn.setSampleRate(48000);
        QCOMPARE(conn.micDecimationFactor(), 1);
        conn.setSampleRate(96000);
        QCOMPARE(conn.micDecimationFactor(), 2);
        conn.setSampleRate(192000);
        QCOMPARE(conn.micDecimationFactor(), 4);
        conn.setSampleRate(384000);
        QCOMPARE(conn.micDecimationFactor(), 8);
        // Default for unsupported rates: factor=4 (matches netInterface.c:1306-1307).
        conn.setSampleRate(123456);
        QCOMPARE(conn.micDecimationFactor(), 4);
    }
};

QTEST_MAIN(TestP1MicDecimation)
#include "tst_p1_mic_decimation.moc"
