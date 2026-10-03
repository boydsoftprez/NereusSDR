// =================================================================
// tests/tst_diversity_pattern.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. Phone wire batch: the Diversity
// dialog's sensitivity pattern (core/DiversityPattern, which the radar
// draws) and the slice's diversityPattern the Core sends.
//
// The reference values below were computed outside the code from Thetis
// DiversityForm.CalcVrms [v2.10.3.15] with the radar's inputs (5.5 m
// spacing, the gain term, cross fire off), 120 bearings, each divided by
// the peak and rounded to 3 places.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include <algorithm>
#include <cmath>

#include "core/DiversityPattern.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestDiversityPattern : public QObject {
    Q_OBJECT

private:
    static QJsonObject parse(const QString& json)
    {
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            return {};
        }
        return doc.object();
    }

private slots:
    void inputsAreTheDialogs()
    {
        // As DiversityDialog::refreshFromSlice feeds its radar.
        const DiversityPattern::Inputs in =
            DiversityPattern::inputsForSlice(7100000.0, 45.0, 6.0);
        QCOMPARE(in.vfoMhz, 7.1);
        QCOMPARE(in.phaseRad, 45.0 * M_PI / 180.0);
        QCOMPARE(in.gainLinear, std::pow(10.0, 6.0 / 20.0));
        QCOMPARE(in.crossFire, false);
        QCOMPARE(in.spacingMeters, 5.5);
    }

    void samplesPeakAtOne()
    {
        const QList<double> samples = DiversityPattern::normalizedSamples(
            DiversityPattern::inputsForSlice(14200000.0, 0.0, 0.0));
        QCOMPARE(samples.size(), 120);
        QCOMPARE(*std::max_element(samples.cbegin(), samples.cend()), 1.0);
        QVERIFY(*std::min_element(samples.cbegin(), samples.cend()) >= 0.0);
    }

    void wireValueMatchesCalcVrms_data()
    {
        QTest::addColumn<double>("frequencyHz");
        QTest::addColumn<double>("phaseDeg");
        QTest::addColumn<double>("gainDb");
        QTest::addColumn<QList<double>>("expected"); // bearings 0, 90, 180, 270
        QTest::addColumn<int>("peakIndex");
        QTest::newRow("20 m, 0 degrees, 0 dB") << 14200000.0 << 0.0 << 0.0
                                               << QList<double>{1.0, 0.518, 0.069, 0.518} << 0;
        QTest::newRow("40 m, 45 degrees, +6 dB") << 7100000.0 << 45.0 << 6.0
                                                 << QList<double>{0.997, 0.575, 0.575, 0.997}
                                                 << 93;
        QTest::newRow("20 m, 90 degrees, -3 dB") << 14225000.0 << 90.0 << -3.0
                                                 << QList<double>{0.532, 0.099, 0.532, 1.0}
                                                 << 90;
    }

    void wireValueMatchesCalcVrms()
    {
        QFETCH(double, frequencyHz);
        QFETCH(double, phaseDeg);
        QFETCH(double, gainDb);
        QFETCH(QList<double>, expected);
        QFETCH(int, peakIndex);

        const QJsonObject wire =
            parse(DiversityPattern::wireJson(frequencyHz, phaseDeg, gainDb));
        QCOMPARE(wire.keys(), (QStringList{QStringLiteral("crossFire"), QStringLiteral("points"),
                                           QStringLiteral("spacingMeters"),
                                           QStringLiteral("stepDeg")}));
        QCOMPARE(wire.value(QStringLiteral("spacingMeters")).toDouble(), 5.5);
        QCOMPARE(wire.value(QStringLiteral("crossFire")).toBool(true), false);
        QCOMPARE(wire.value(QStringLiteral("stepDeg")).toDouble(), 3.0);
        const QJsonArray points = wire.value(QStringLiteral("points")).toArray();
        QCOMPARE(points.size(), 120);
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(points.at(i * 30).toDouble(), expected.at(i));
        }
        // The peak is exactly 1, where the radar draws its longest lobe.
        QCOMPARE(points.at(peakIndex).toDouble(), 1.0);
        // Each sample is the radar's own, rounded to a thousandth.
        const QList<double> samples = DiversityPattern::normalizedSamples(
            DiversityPattern::inputsForSlice(frequencyHz, phaseDeg, gainDb));
        for (int i = 0; i < 120; ++i) {
            QCOMPARE(points.at(i).toDouble(), std::round(samples.at(i) * 1000.0) / 1000.0);
        }
    }

    void sliceSendsItsOwnPattern()
    {
        SliceModel slice;
        slice.setFrequency(14225000.0);
        slice.setDiversityPhaseDeg(90.0);
        slice.setDiversityGainDb(-3.0);
        QCOMPARE(slice.diversityPattern(),
                 DiversityPattern::wireJson(14225000.0, 90.0, -3.0));
        QCOMPARE(slice.property("diversityPattern").toString(), slice.diversityPattern());
    }

    void patternAnnouncedOnlyWhenItMoves()
    {
        SliceModel slice;
        slice.setFrequency(14225000.0);
        (void)slice.diversityPattern();
        QSignalSpy spy(&slice, &SliceModel::diversityPatternChanged);

        slice.setDiversityPhaseDeg(45.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().at(0).toString(), DiversityPattern::wireJson(14225000.0, 45.0, 0.0));

        slice.setDiversityPhaseDeg(45.0);  // no change
        QCOMPARE(spy.count(), 1);

        slice.setDiversityGainDb(6.0);
        QCOMPARE(spy.count(), 2);

        // A 10 Hz step leaves every rounded sample where it was.
        slice.setFrequency(14225010.0);
        QCOMPARE(spy.count(), 2);

        // A band change moves the lobe.
        slice.setFrequency(7100000.0);
        QCOMPARE(spy.count(), 3);
        QCOMPARE(spy.last().at(0).toString(), DiversityPattern::wireJson(7100000.0, 45.0, 6.0));
    }
};

QTEST_MAIN(TestDiversityPattern)
#include "tst_diversity_pattern.moc"
