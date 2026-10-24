// no-port-check: NereusSDR-original test of the I/Q level diagnostics
// (IqLevelMonitor and ReceiverManager's I/Q level log line), 2026-10-22.
#include <QtTest/QtTest>

#include <QRegularExpression>
#include <QVector>

#include <cmath>
#include <limits>

#include "core/IqLevelMonitor.h"
#include "core/ReceiverManager.h"

using NereusSDR::IqLevelMonitor;
using NereusSDR::ReceiverManager;

namespace {

// n pairs of a constant complex sample (i, q).
QVector<float> constantPairs(int n, float i, float q)
{
    QVector<float> samples;
    samples.reserve(2 * n);
    for (int k = 0; k < n; ++k) {
        samples << i << q;
    }
    return samples;
}

constexpr qint64 kSecondNs = 1'000'000'000LL;

} // namespace

class TstIqLevelMonitor : public QObject
{
    Q_OBJECT

private slots:
    void reportsExactRmsAndPeakPerDdc()
    {
        IqLevelMonitor monitor;
        QVERIFY(!monitor.takeIfDue(0).has_value());  // starts the interval

        // DDC2: three pairs of magnitude 0.1 and one of magnitude 0.5.
        // Mean |x|^2 = (3 * 0.01 + 0.25) / 4 = 0.07 -> 10 log10(0.07).
        QVector<float> ddc2 = constantPairs(3, 0.06f, 0.08f);
        ddc2 << 0.3f << 0.4f;
        monitor.accumulate(2, ddc2.constData(), ddc2.size());
        // DDC3: full-scale on I alone.
        const QVector<float> ddc3 = constantPairs(10, 1.0f, 0.0f);
        monitor.accumulate(3, ddc3.constData(), ddc3.size());

        const auto report = monitor.takeIfDue(IqLevelMonitor::kReportIntervalNs);
        QVERIFY(report.has_value());
        QCOMPARE(report->levels.size(), 2);
        const IqLevelMonitor::DdcLevel& a = report->levels.at(0);
        QCOMPARE(a.ddc, 2);
        QCOMPARE(a.pairs, qint64(4));
        QCOMPARE(a.nonFinite, qint64(0));
        QVERIFY(std::abs(a.rmsDbfs - 10.0 * std::log10(0.07)) < 1e-4);
        QVERIFY(std::abs(a.peakDbfs - 20.0 * std::log10(0.5)) < 1e-4);
        const IqLevelMonitor::DdcLevel& b = report->levels.at(1);
        QCOMPARE(b.ddc, 3);
        QVERIFY(std::abs(b.rmsDbfs) < 1e-6);
        QVERIFY(std::abs(b.peakDbfs) < 1e-6);
        QCOMPARE(report->overloadMask, quint32(0));
    }

    void reportsAtMostOncePerInterval()
    {
        IqLevelMonitor monitor;
        const QVector<float> samples = constantPairs(8, 0.01f, 0.0f);
        QVERIFY(!monitor.takeIfDue(5 * kSecondNs).has_value());
        monitor.accumulate(0, samples.constData(), samples.size());
        // 9.999 s after the interval began: not yet.
        QVERIFY(!monitor.takeIfDue(5 * kSecondNs + IqLevelMonitor::kReportIntervalNs - 1)
                     .has_value());
        monitor.accumulate(0, samples.constData(), samples.size());
        const auto first = monitor.takeIfDue(5 * kSecondNs + IqLevelMonitor::kReportIntervalNs);
        QVERIFY(first.has_value());
        QCOMPARE(first->levels.size(), 1);
        QCOMPARE(first->levels.at(0).pairs, qint64(16));
        // The next interval starts at the report and holds new samples only.
        monitor.accumulate(0, samples.constData(), samples.size());
        QVERIFY(!monitor.takeIfDue(5 * kSecondNs + IqLevelMonitor::kReportIntervalNs + kSecondNs)
                     .has_value());
        const auto second =
            monitor.takeIfDue(5 * kSecondNs + 2 * IqLevelMonitor::kReportIntervalNs);
        QVERIFY(second.has_value());
        QCOMPARE(second->levels.at(0).pairs, qint64(8));
        // A DDC that delivered nothing in an interval is left out.
        const auto empty = monitor.takeIfDue(5 * kSecondNs + 3 * IqLevelMonitor::kReportIntervalNs);
        QVERIFY(empty.has_value());
        QVERIFY(empty->levels.isEmpty());
    }

    void countsNonFinitePairsApartFromTheLevel()
    {
        IqLevelMonitor monitor;
        monitor.takeIfDue(0);
        QVector<float> samples = constantPairs(2, 0.1f, 0.0f);
        samples << std::numeric_limits<float>::quiet_NaN() << 0.0f;
        samples << 0.0f << std::numeric_limits<float>::infinity();
        monitor.accumulate(1, samples.constData(), samples.size());
        const auto report = monitor.takeIfDue(IqLevelMonitor::kReportIntervalNs);
        QVERIFY(report.has_value());
        const IqLevelMonitor::DdcLevel& level = report->levels.at(0);
        QCOMPARE(level.pairs, qint64(2));
        QCOMPARE(level.nonFinite, qint64(2));
        QVERIFY(std::abs(level.rmsDbfs - (-20.0)) < 1e-4);
        QVERIFY(std::isfinite(level.peakDbfs));
    }

    void silenceIsReportedAtTheFloorNotMinusInfinity()
    {
        IqLevelMonitor monitor;
        monitor.takeIfDue(0);
        const QVector<float> zeros = constantPairs(4, 0.0f, 0.0f);
        monitor.accumulate(4, zeros.constData(), zeros.size());
        const auto report = monitor.takeIfDue(IqLevelMonitor::kReportIntervalNs);
        QVERIFY(report.has_value());
        QCOMPARE(report->levels.at(0).rmsDbfs, IqLevelMonitor::kSilentDbfs);
        QCOMPARE(report->levels.at(0).peakDbfs, IqLevelMonitor::kSilentDbfs);
    }

    void ignoresDdcIndicesOutsideItsRange()
    {
        IqLevelMonitor monitor;
        monitor.takeIfDue(0);
        const QVector<float> samples = constantPairs(4, 0.5f, 0.0f);
        monitor.accumulate(-1, samples.constData(), samples.size());
        monitor.accumulate(IqLevelMonitor::kMaxDdc, samples.constData(), samples.size());
        const auto report = monitor.takeIfDue(IqLevelMonitor::kReportIntervalNs);
        QVERIFY(report.has_value());
        QVERIFY(report->levels.isEmpty());
    }

    void overloadFlagCoversOneIntervalOnly()
    {
        IqLevelMonitor monitor;
        monitor.takeIfDue(0);
        monitor.noteAdcOverflow(1);
        monitor.noteAdcOverflow(-1);   // ignored
        monitor.noteAdcOverflow(32);   // ignored
        const auto first = monitor.takeIfDue(IqLevelMonitor::kReportIntervalNs);
        QVERIFY(first.has_value());
        QCOMPARE(first->overloadMask, quint32(0x2));
        const auto second = monitor.takeIfDue(2 * IqLevelMonitor::kReportIntervalNs);
        QVERIFY(second.has_value());
        QCOMPARE(second->overloadMask, quint32(0));
    }

    // ReceiverManager logs one line per DDC: receiver, ADC, level, overload
    // flag and the DDC map; an unmapped DDC is measured and named too.
    void receiverManagerLogsLevelsWithTheDdcMap()
    {
        ReceiverManager rm;
        const int rx = rm.createReceiver();
        rm.setDdcMapping(rx, 2);
        rm.activateReceiver(rx);
        rm.setAdcForReceiver(rx, 0);
        rm.setIqLevelReportIntervalNsForTest(0);

        // The first packet starts the interval; nothing is logged yet.
        rm.feedIqData(2, constantPairs(4, 0.001f, 0.0f));
        rm.noteAdcOverflow(0);
        // With a zero interval the next packet (full scale on the unmapped
        // DDC 3) reports both DDCs.
        QTest::ignoreMessage(
            QtInfoMsg,
            QRegularExpression(QStringLiteral(
                "^I/Q level: DDC2 -> stream 0 \\(ADC0\\): RMS -60\\.0 dBFS, peak -60\\.0 dBFS "
                "over 4 pairs, non-finite pairs 0; ADC0 overload yes; DDC map DDC2->stream0$")));
        QTest::ignoreMessage(
            QtInfoMsg,
            QRegularExpression(QStringLiteral(
                "^I/Q level: DDC3 -> no stream: RMS 0\\.0 dBFS, peak 0\\.0 dBFS over 4 pairs, "
                "non-finite pairs 0; ADC overload on ADC0; DDC map DDC2->stream0$")));
        rm.feedIqData(3, constantPairs(4, 1.0f, 0.0f));
    }
};

QTEST_GUILESS_MAIN(TstIqLevelMonitor)
#include "tst_iq_level_monitor.moc"
