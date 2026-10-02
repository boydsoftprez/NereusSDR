// no-port-check: NereusSDR-original compatibility tests.
#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include "core/CfcProfile.h"
#include "core/ParaEqEnvelope.h"
#include "models/TransmitModel.h"
using namespace NereusSDR;

static CfcProfile profileForCount(int count)
{
    CfcProfile p;
    p.compression.frequencyMaxHz = p.postEq.frequencyMaxHz = 10000;
    p.compression.useQ = p.postEq.useQ = true;
    p.compression.globalGainDb = 3.5;
    p.postEq.globalGainDb = -2.5;
    for (int i = 0; i < count; ++i) {
        p.compression.frequenciesHz.append(i == 1 ? 125.125 : std::nearbyint(i * 10000000.0 / (count - 1)) / 1000.0);
        p.compression.gainsDb.append(3.5);
        p.compression.q.append(1.25);
        p.postEq.gainsDb.append(-3.5);
        p.postEq.q.append(2.75);
    }
    p.postEq.frequenciesHz = p.compression.frequenciesHz;
    return p;
}
class TstCfcProfile : public QObject {
    Q_OBJECT
private slots:
    void thetisPascalCaseLoads()
    {
        const CfcProfile expected = profileForCount(5);
        const QString blob = encodeCfcProfile(expected);
        const auto payload = ParaEqEnvelope::decode(blob);
        QVERIFY(payload);
        const QStringList parts = payload->split("<SEP>");
        QCOMPARE(parts.size(), 2);
        const QJsonObject graph = QJsonDocument::fromJson(parts[0].toUtf8()).object();
        QVERIFY(graph.contains("BandCount"));
        QVERIFY(graph.contains("Points"));
        QVERIFY(!graph.contains("band_count"));
        const auto actual = decodeCfcProfile(blob);
        QVERIFY(actual);
        QVERIFY(*actual == expected);
    }
    void nereusSnakeCaseLoads()
    {
        const CfcProfile expected = profileForCount(10);
        QString payload = *ParaEqEnvelope::decode(encodeCfcProfile(expected));
        const QList<QPair<QString, QString>> names = {{"BandCount","band_count"},
            {"ParametricEQ","parametric_eq"}, {"GlobalGainDb","global_gain_db"},
            {"FrequencyMinHz","frequency_min_hz"}, {"FrequencyMaxHz","frequency_max_hz"},
            {"Points","points"}, {"FrequencyHz","frequency_hz"}, {"GainDb","gain_db"}, {"Q","q"}};
        for (const auto& name : names) { payload.replace('"' + name.first + '"', '"' + name.second + '"'); }
        const auto actual = decodeCfcProfile(ParaEqEnvelope::encode(payload));
        QVERIFY(actual);
        QVERIFY(*actual == expected);
    }
    void allCountsRoundTrip_data()
    {
        QTest::addColumn<int>("count");
        for (int count : {5, 10, 18}) { QTest::newRow(qPrintable(QString::number(count))) << count; }
    }
    void allCountsRoundTrip()
    {
        QFETCH(int, count);
        const CfcProfile expected = profileForCount(count);
        const auto actual = decodeCfcProfile(encodeCfcProfile(expected));
        QVERIFY(actual);
        QCOMPARE(actual->compression.frequenciesHz.size(), count);
        QVERIFY(*actual == expected);
        QCOMPARE(actual->compression.frequenciesHz[1], 125.125);
        QCOMPARE(actual->compression.gainsDb[1], 3.5);
        QCOMPARE(actual->compression.q[1], 1.25);
        QCOMPARE(actual->postEq.q[1], 2.75);
    }
    void badHalfFallsBackWithoutOverwrite()
    {
        TransmitModel model;
        const QString good = *ParaEqEnvelope::decode(encodeCfcProfile(profileForCount(5)));
        const QString half = good.section("<SEP>", 0, 0);
        for (const QString& blob : {QString("future-format"), ParaEqEnvelope::encode(half + "<SEP>{}"),
                                   ParaEqEnvelope::encode(half + "<SEP>" + half + "<SEP>" + half)}) {
            model.setCfcParaEqData(blob);
            QVERIFY(!decodeCfcProfile(blob));
            QCOMPARE(model.effectiveCfcProfile().compression.frequenciesHz.size(), 10);
            QCOMPARE(model.cfcParaEqData(), blob);
            model.setCfcPrecompDb(2);
            QCOMPARE(model.cfcParaEqData(), blob);
        }
    }
    void mismatchedFrequenciesRejected()
    {
        CfcProfile p = profileForCount(18);
        p.postEq.frequenciesHz[1] += 1;
        QVERIFY(!isValidCfcProfile(p));
        QVERIFY(encodeCfcProfile(p).isEmpty());
        const CfcProfile good = profileForCount(18);
        const QStringList halves = ParaEqEnvelope::decode(encodeCfcProfile(good))->split("<SEP>");
        QJsonObject eq = QJsonDocument::fromJson(halves[1].toUtf8()).object();
        QJsonArray pts = eq["Points"].toArray();
        QJsonObject point = pts[1].toObject(); point["FrequencyHz"] = 126.125; pts[1] = point; eq["Points"] = pts;
        const QString malformed = ParaEqEnvelope::encode(halves[0] + "<SEP>" + QString::fromUtf8(QJsonDocument(eq).toJson()));
        QVERIFY(!decodeCfcProfile(malformed));
    }
    void invalidInputsRejected_data()
    {
        QTest::addColumn<int>("invalid");
        for (int invalid = 0; invalid < 10; ++invalid) { QTest::newRow(qPrintable(QString::number(invalid))) << invalid; }
    }
    void invalidInputsRejected()
    {
        QFETCH(int, invalid);
        CfcProfile p = profileForCount(5);
        switch (invalid) {
        case 0: p.compression.q[1] = std::numeric_limits<double>::quiet_NaN(); break;
        case 1: p.compression.q[1] = 0.1; break;
        case 2: p.postEq.q[1] = 21; break;
        case 3: p.compression.gainsDb[1] = -1; break;
        case 4: p.postEq.gainsDb[1] = 25; break;
        case 5: p.compression.globalGainDb = 17; break;
        case 6: p.postEq.frequencyMaxHz = 9000; break;
        case 7: p.compression.frequenciesHz[2] = 20; break;
        case 8: p.compression.q.removeLast(); break;
        case 9: p.compression.frequenciesHz[1] = 21000; break;
        }
        TransmitModel model;
        model.setCfcParaEqData("opaque");
        QVERIFY(!model.setCfcProfile(p));
        QCOMPARE(model.cfcParaEqData(), QString("opaque"));
    }
};
QTEST_GUILESS_MAIN(TstCfcProfile)
#include "tst_cfc_profile.moc"
