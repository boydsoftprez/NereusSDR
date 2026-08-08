#include <QTest>
#include <QVector>
#include <cmath>

#include "gui/DssRenderer.h"

using namespace NereusSDR;

namespace {

QVector<float> flatRow(int n, float dbm)
{
    QVector<float> v(n, dbm);
    return v;
}

}  // namespace

class TestDssRendererRing : public QObject {
    Q_OBJECT

private slots:
    void emptyRenderer_hasNoData() {
        DssRenderer r;
        QVERIFY(!r.hasData());
        QCOMPARE(r.rowCount(), 0);
        QCOMPARE(r.cols(), kDssCols);
        QCOMPARE(r.rows(), kDssRows);
    }

    // A single-bin carrier must survive the squeeze from full FFT width to
    // 768 columns. A mean-based downsample would bury it; the reduction is
    // peak-preserving precisely so it cannot.
    void singleBinCarrier_survivesDownsample() {
        DssRenderer r;
        QVector<float> bins = flatRow(8192, -130.0f);
        bins[4096] = -40.0f;
        // Three identical pushes clear the median-of-3 impulse rejector,
        // which would otherwise treat a one-frame spike as interference.
        for (int i = 0; i < 3; ++i) {
            r.pushRow(bins, 14.2, 0.192);
        }
        const float* row = r.rowDataRing(r.headRing());
        float peak = -1000.0f;
        int peakCol = -1;
        for (int c = 0; c < kDssCols; ++c) {
            if (row[c] > peak) { peak = row[c]; peakCol = c; }
        }
        // Two independent properties, because either alone is weak.
        //
        // The carrier must still be the loudest column, and at the column it
        // belongs in. This is blur-independent and is how upstream's own
        // dss_renderer_test.cpp checks the same thing.
        const int expectedCol = 4096 * kDssCols / 8192;
        QCOMPARE(peakCol, expectedCol);
        //
        // And it must still stand well clear of the floor in absolute terms,
        // which is what actually catches a mean-based downsample. Do not
        // tighten this past -85: the unconditional 1-2-1 spatial blur maps an
        // isolated column to 0.25*floor + 0.5*carrier + 0.25*floor, so a
        // -40 dBm carrier on a -130 dBm floor converges to exactly -85.0 and
        // no number of further pushes moves it. A mean-based downsample would
        // land near -125.8 instead, so -100 discriminates with 15 dB of margin
        // above the true value and 25 dB below the broken one.
        QVERIFY2(peak > -100.0f,
                 qPrintable(QStringLiteral("carrier lost, peak=%1").arg(peak)));
    }

    // Broadband impulse noise lasting one frame is rejected by median-of-3.
    void singleFrameImpulse_isRejected() {
        DssRenderer r;
        const QVector<float> quiet = flatRow(768, -130.0f);
        r.pushRow(quiet, 14.2, 0.192);
        r.pushRow(quiet, 14.2, 0.192);
        r.pushRow(flatRow(768, -20.0f), 14.2, 0.192);   // one-frame burst
        const float* row = r.rowDataRing(r.headRing());
        QVERIFY2(row[384] < -100.0f,
                 qPrintable(QStringLiteral("impulse not rejected: %1")
                                .arg(row[384])));
    }

    void ringWrap_keepsCountBounded() {
        DssRenderer r;
        for (int i = 0; i < kDssRows + 20; ++i) {
            r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        }
        QCOMPARE(r.rowCount(), kDssRows);
        QCOMPARE(r.visibleRowCount(), kDssVisibleRows);
        QVERIFY(r.headRing() >= 0 && r.headRing() < kDssRows);
    }

    void pushRow_stampsFrequencyFrame() {
        DssRenderer r;
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        QCOMPARE(r.rowCenterMhzAtAge(0),    14.2);
        QCOMPARE(r.rowBandwidthMhzAtAge(0), 0.192);
        r.pushRow(flatRow(768, -130.0f), 7.1, 0.096);
        QCOMPARE(r.rowCenterMhzAtAge(0),    7.1);
        QCOMPARE(r.rowCenterMhzAtAge(1),    14.2);
        QCOMPARE(r.rowBandwidthMhzAtAge(1), 0.192);
    }

    // A retune must not blend the new row against the previous one: adjacent
    // bin indices no longer represent the same frequencies, and blending
    // smears every signal in the direction of the pan.
    void frameChange_dropsTemporalBlend() {
        DssRenderer r;
        for (int i = 0; i < 4; ++i) {
            r.pushRow(flatRow(768, -60.0f), 14.2, 0.192);
        }
        r.pushRow(flatRow(768, -130.0f), 21.0, 0.192);   // retune
        const float* row = r.rowDataRing(r.headRing());
        QVERIFY2(row[384] < -125.0f,
                 qPrintable(QStringLiteral("blended across retune: %1")
                                .arg(row[384])));
    }

    void pushRow_withoutWide_marksWideUncovered() {
        DssRenderer r;
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        const quint8* cov = r.rowWideCoverageRing(r.headRing());
        QCOMPARE(cov[0],   quint8(0));
        QCOMPARE(cov[384], quint8(0));
        QCOMPARE(r.newestWideBandwidthMhz(0.192), 0.0);
    }

    void pushRowWithWide_recordsBothChannels() {
        DssRenderer r;
        r.pushRowWithWide(flatRow(768, -130.0f), 14.2, 0.192,
                          flatRow(768, -140.0f), 14.2, 0.500);
        const quint8* exact = r.rowCoverageRing(r.headRing());
        const quint8* wide  = r.rowWideCoverageRing(r.headRing());
        QCOMPARE(exact[384], quint8(1));
        QCOMPARE(wide[384],  quint8(1));
        QCOMPARE(r.rowWideBandwidthMhzAtAge(0), 0.500);
        QCOMPARE(r.newestWideBandwidthMhz(0.192), 0.500);
    }

    // The front row is deliberately not authoritative for the wide channel:
    // a producer that appends without a wide slice must not collapse the
    // overhang to nothing while rows carrying it are still on screen.
    void newestWideBandwidth_looksPastBareFrontRows() {
        DssRenderer r;
        r.pushRowWithWide(flatRow(768, -130.0f), 14.2, 0.192,
                          flatRow(768, -140.0f), 14.2, 0.500);
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);   // no wide slice
        QCOMPARE(r.newestWideBandwidthMhz(0.192), 0.500);
    }

    void clear_resetsEverything() {
        DssRenderer r;
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        QVERIFY(r.hasData());
        r.clear();
        QVERIFY(!r.hasData());
        QCOMPARE(r.rowCount(), 0);
        // hasData() and rowCount() both reduce to m_count == 0, so the two
        // assertions above pass whether or not clear() actually wipes the
        // per-row frame stamps. Query an age accessor as well: ringAtAge()
        // clamps to ring bounds without an m_count guard, so a clear() that
        // skipped the fills would hand back the stale 14.2 MHz stamp here.
        QCOMPARE(r.rowCenterMhzAtAge(0),    0.0);
        QCOMPARE(r.rowBandwidthMhzAtAge(0), 0.0);
        QCOMPARE(r.rowWideCenterMhzAtAge(0),    0.0);
        QCOMPARE(r.rowWideBandwidthMhzAtAge(0), 0.0);
        QCOMPARE(r.rowWideCoverageRing(r.headRing())[0], quint8(0));
    }

    void rowGeneration_advancesOnEveryPush() {
        DssRenderer r;
        const quint64 before = r.rowGeneration();
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        QVERIFY(r.rowGeneration() > before);
    }
};

QTEST_APPLESS_MAIN(TestDssRendererRing)
#include "tst_dss_renderer_ring.moc"
