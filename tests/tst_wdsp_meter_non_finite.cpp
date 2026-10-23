// no-port-check: NereusSDR-original regression; no upstream algorithm is
// duplicated in this test.
// =================================================================
// tests/tst_wdsp_meter_non_finite.cpp  (NereusSDR)
// =================================================================
//
// 2026-10-23, the iPhone S-meter at "3086.5 dBm": slice B's WDSP output
// carried a short non-finite burst (Core log: "Slice B (index 1): 28544
// non-finite samples in WDSP's output"). WDSP's xmeter (meter.c) added those
// samples to its average and peak, which then stayed NaN, and mlog10 reads a
// NaN's bits as about 3084 dB, so the slice's signalPeakDbm and
// signalAverageDbm read about +3086 dBm (with the G2E's offset) from then on.
// A meter must leave a non-finite sample out: its readings stay finite
// through the burst and follow the signal again after it.
//
// Modification history (NereusSDR):
//   2026-10-23  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================
#include <QtTest>

#include <cmath>
#include <limits>
#include <vector>

extern "C" int nereus_meter_probe(const double* blocks, int size, int count, double* average,
                                  double* peak);

namespace {

constexpr int kSize = 256;
// 200 blocks of 256 samples at 48 kHz is about 1.07 s, ten of the meter's
// 0.1 s averaging time constants, so the average (which starts from zero)
// has settled on the carrier by the last block.
constexpr int kBlocks = 200;
constexpr double kAmplitude = 0.1;           // a steady carrier, 20 dB below full scale
constexpr double kCarrierDb = -20.0;         // 10 log10(0.1 * 0.1)
constexpr int kBurstBlock = 10;              // the block that carries the burst
constexpr double kFiniteCeilingDb = 100.0;   // any real reading is far below this
// A stretch of 20 non-finite blocks (about 107 ms, about one 0.1 s time
// constant) from block 100; the average falls about 4.6 dB through it.
constexpr int kStretchFirst = 100;
constexpr int kStretchEnd = 120;
constexpr double kFallDb = 3.0;

} // namespace

class TestWdspMeterNonFinite : public QObject {
    Q_OBJECT

private slots:
    void nonFiniteBurstLeavesTheReadingsFinite()
    {
        std::vector<double> blocks(static_cast<size_t>(2 * kSize * kBlocks));
        for (int i = 0; i < kSize * kBlocks; ++i) {
            blocks[static_cast<size_t>(2 * i)] = kAmplitude;
            blocks[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
        // The burst: NaN and infinite samples through one block, among them
        // lone infinities (I = +inf, Q = 0) that a NaN-only guard would let in.
        const int first = kBurstBlock * kSize;
        for (int i = first; i < first + kSize; i += 3) {
            blocks[static_cast<size_t>(2 * i)] = std::numeric_limits<double>::quiet_NaN();
            blocks[static_cast<size_t>(2 * i + 1)] =
                (i % 2 == 0) ? std::numeric_limits<double>::infinity() : 0.0;
            if (i + 1 < first + kSize) {
                blocks[static_cast<size_t>(2 * (i + 1))] = std::numeric_limits<double>::infinity();
                blocks[static_cast<size_t>(2 * (i + 1) + 1)] = 0.0;
            }
        }

        std::vector<double> average(kBlocks);
        std::vector<double> peak(kBlocks);
        QVERIFY(nereus_meter_probe(blocks.data(), kSize, kBlocks, average.data(), peak.data()) == 1);

        for (int b = 0; b < kBlocks; ++b) {
            QVERIFY2(std::isfinite(average[b]) && average[b] < kFiniteCeilingDb,
                     qPrintable(QStringLiteral("block %1 average %2 dB").arg(b).arg(average[b])));
            QVERIFY2(std::isfinite(peak[b]) && peak[b] < kFiniteCeilingDb,
                     qPrintable(QStringLiteral("block %1 peak %2 dB").arg(b).arg(peak[b])));
        }
        // After the burst the meter reads the carrier again.
        QVERIFY2(std::abs(peak[kBlocks - 1] - kCarrierDb) < 0.5,
                 qPrintable(QStringLiteral("peak %1 dB").arg(peak[kBlocks - 1])));
        QVERIFY2(std::abs(average[kBlocks - 1] - kCarrierDb) < 0.5,
                 qPrintable(QStringLiteral("average %1 dB").arg(average[kBlocks - 1])));
    }

    // Through a stretch with no finite sample the average and the peak fall,
    // as they would on silence, instead of holding the last reading.
    void nonFiniteStretchLetsTheMeterFall()
    {
        std::vector<double> blocks(static_cast<size_t>(2 * kSize * kBlocks));
        for (int i = 0; i < kSize * kBlocks; ++i) {
            const bool inStretch = i >= kStretchFirst * kSize && i < kStretchEnd * kSize;
            blocks[static_cast<size_t>(2 * i)] =
                inStretch ? std::numeric_limits<double>::infinity() : kAmplitude;
            blocks[static_cast<size_t>(2 * i + 1)] = 0.0;
        }

        std::vector<double> average(kBlocks);
        std::vector<double> peak(kBlocks);
        QVERIFY(nereus_meter_probe(blocks.data(), kSize, kBlocks, average.data(), peak.data()) == 1);

        const double before = average[kStretchFirst - 1];
        const double during = average[kStretchEnd - 1];
        QVERIFY2(std::isfinite(during) && during < before - kFallDb,
                 qPrintable(QStringLiteral("average %1 dB before, %2 dB at the end of the stretch")
                                .arg(before).arg(during)));
        QVERIFY2(std::isfinite(peak[kStretchEnd - 1]) && peak[kStretchEnd - 1] < peak[kStretchFirst - 1] - kFallDb,
                 qPrintable(QStringLiteral("peak %1 dB before, %2 dB at the end of the stretch")
                                .arg(peak[kStretchFirst - 1]).arg(peak[kStretchEnd - 1])));
        QVERIFY2(std::abs(average[kBlocks - 1] - kCarrierDb) < 0.5,
                 qPrintable(QStringLiteral("average %1 dB").arg(average[kBlocks - 1])));
    }
};

QTEST_GUILESS_MAIN(TestWdspMeterNonFinite)
#include "tst_wdsp_meter_non_finite.moc"
