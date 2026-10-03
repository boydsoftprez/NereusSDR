// no-port-check: tests the existing Thetis CFC JSON and gzip contract.
// Modification history (NereusSDR): 2026-09-29 J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code: the published band editor
// (transmitSettingsVersion 15).
#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/CfcProfile.h"
#include "core/ParaEqEnvelope.h"

using namespace NereusSDR;

namespace {
QString curve(int count, bool useQ, double global, double gain)
{
    QJsonArray points;
    for (int i = 0; i < count; ++i) {
        points.append(QJsonObject{{QStringLiteral("frequency_hz"), i * 100.0},
                                  {QStringLiteral("gain_db"), gain},
                                  {QStringLiteral("q"), 2.0 + i * 0.1}});
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("band_count"), count},
        {QStringLiteral("parametric_eq"), useQ},
        {QStringLiteral("global_gain_db"), global},
        {QStringLiteral("frequency_min_hz"), 0.0},
        {QStringLiteral("frequency_max_hz"), (count - 1) * 100.0},
        {QStringLiteral("points"), points}}).toJson(QJsonDocument::Compact));
}
QString blob(int count, bool compQ = true, bool eqQ = true)
{
    return ParaEqEnvelope::encode(curve(count, compQ, 6.0, 5.0)
                                  + QStringLiteral("<SEP>")
                                  + curve(count, eqQ, -7.0, -3.0));
}
}

class TstCfcProfile : public QObject {
    Q_OBJECT
private slots:
    void importedPairedThetisShape_data()
    {
        QTest::addColumn<int>("count");
        QTest::newRow("five") << 5;
        QTest::newRow("ten") << 10;
        QTest::newRow("eighteen") << 18;
    }
    void importedPairedThetisShape()
    {
        QFETCH(int, count);
        CfcProfile::Profile p;
        QVERIFY(CfcProfile::decode(blob(count), p));
        QCOMPARE(static_cast<int>(p.f.size()), count);
        QCOMPARE(p.f.back(), (count - 1) * 100.0);
        QCOMPARE(p.g.at(2), 5.0);
        QCOMPARE(p.e.at(2), -3.0);
        QCOMPARE(p.qg.at(2), 2.2);
        QCOMPARE(p.qe.at(2), 2.2);
        QCOMPARE(p.precompDb, 6.0);
        QCOMPARE(p.postEqGainDb, -7.0);
        CfcProfile::Profile again;
        QVERIFY(CfcProfile::decode(CfcProfile::encode(p), again));
        QCOMPARE(again.f, p.f);
        QCOMPARE(again.qe, p.qe);
    }
    void eitherGraphicFlagDisablesBothQVectors()
    {
        CfcProfile::Profile p;
        QVERIFY(CfcProfile::decode(blob(5, true, false), p));
        QVERIFY(!p.usesQ());
        QCOMPARE(static_cast<int>(p.qg.size()), 5);
        QCOMPARE(static_cast<int>(p.qe.size()), 5);
    }
    void independentWidgetResetAxesRoundTrip()
    {
        CfcProfile::Profile p;
        QVERIFY(CfcProfile::decode(blob(10), p));
        const auto originalPostF = p.postF;
        const auto originalE = p.e;
        const auto originalQe = p.qe;
        for (std::size_t i = 1; i + 1 < p.f.size(); ++i) {
            p.f[i] += 1.0;
            p.g[i] = 0.0;
            p.qg[i] = 4.0;
        }
        CfcProfile::Profile restored;
        QVERIFY(CfcProfile::decode(CfcProfile::encode(p), restored));
        QCOMPARE(restored.f, p.f);
        QCOMPARE(restored.postF, originalPostF);
        QCOMPARE(restored.e, originalE);
        QCOMPARE(restored.qe, originalQe);
        // Resetting post-EQ later must not rewrite the compression axis.
        const auto compF = restored.f;
        restored.postF[4] += 2.0;
        restored.e[4] = 0.0;
        CfcProfile::Profile again;
        QVERIFY(CfcProfile::decode(CfcProfile::encode(restored), again));
        QCOMPARE(again.f, compF);
        QCOMPARE(again.postF, restored.postF);
    }
    void rejectsMismatchedAndOversizeWithoutMutation()
    {
        CfcProfile::Profile p;
        QVERIFY(CfcProfile::decode(blob(5), p));
        const auto oldF = p.f;
        QVERIFY(!CfcProfile::decode(ParaEqEnvelope::encode(curve(5, true, 6, 5)
            + QStringLiteral("<SEP>") + curve(10, true, -7, -3)), p));
        QCOMPARE(p.f, oldF);
        QVERIFY(!CfcProfile::decode(ParaEqEnvelope::encode(QString(100000, QLatin1Char('x'))), p));
        QCOMPARE(p.f, oldF);
    }
    void rejectsSingleTxEqJson()
    {
        CfcProfile::Profile p;
        QVERIFY(!CfcProfile::decode(ParaEqEnvelope::encode(curve(10, true, 0, 0)), p));
    }

    // ── transmitSettingsVersion 15: the published band editor ─────────────
    void publishedJsonRoundTripsThroughTheVerbReader_data()
    {
        QTest::addColumn<int>("count");
        QTest::newRow("five") << 5;
        QTest::newRow("ten") << 10;
        QTest::newRow("eighteen") << 18;
    }
    void publishedJsonRoundTripsThroughTheVerbReader()
    {
        QFETCH(int, count);
        CfcProfile::Profile p;
        QVERIFY(CfcProfile::decode(blob(count), p));
        // The dialog keeps its range at least 1000 Hz wide.
        for (int i = 0; i < count; ++i) {
            const auto k = static_cast<std::size_t>(i);
            p.f[k] = p.postF[k] = i * 1000.0;
            // Q as the Core keeps it (two places).
            p.qg[k] = std::nearbyint(p.qg[k] * 100.0) / 100.0;
            p.qe[k] = std::nearbyint(p.qe[k] * 100.0) / 100.0;
        }
        p.maxHz = p.postMaxHz = (count - 1) * 1000.0;
        QVERIFY(CfcProfile::decode(CfcProfile::encode(p), p));
        const QString published = CfcProfile::publishedJson(p, QStringLiteral("saved"));
        const QJsonObject o = QJsonDocument::fromJson(published.toUtf8()).object();
        QCOMPARE(o.value(QStringLiteral("state")).toString(), QStringLiteral("saved"));
        QCOMPARE(o.value(QStringLiteral("bands")).toArray().size(), count);
        QCOMPARE(o.value(QStringLiteral("revision")).toString().size(), 16);
        QCOMPARE(o.value(QStringLiteral("revision")).toString(), CfcProfile::revision(p));
        // Keys are sorted, so the text is the same on every host.
        QVERIFY(published.startsWith(QStringLiteral("{\"bands\":[{\"compressionDb\"")));

        CfcProfile::Profile back;
        QString why;
        QVERIFY2(CfcProfile::fromPublishedJson(published, back, &why), qPrintable(why));
        QCOMPARE(back.f, p.f);
        QCOMPARE(back.postF, p.f);
        QCOMPARE(back.g, p.g);
        QCOMPARE(back.e, p.e);
        QCOMPARE(back.qg, p.qg);
        QCOMPARE(back.qe, p.qe);
        QCOMPARE(back.precompDb, p.precompDb);
        QCOMPARE(back.postEqGainDb, p.postEqGainDb);
        QCOMPARE(CfcProfile::revision(back), CfcProfile::revision(p));
        CfcProfile::Profile decoded;
        QVERIFY(CfcProfile::decode(CfcProfile::encode(back), decoded));
    }
    void revisionFollowsValuesNotState()
    {
        CfcProfile::Profile p;
        QVERIFY(CfcProfile::decode(blob(10), p));
        const QString before = CfcProfile::revision(p);
        QCOMPARE(QJsonDocument::fromJson(CfcProfile::publishedJson(p, QStringLiteral("legacy"))
                     .toUtf8()).object().value(QStringLiteral("revision")).toString(), before);
        p.g[3] = 7.0;
        QVERIFY(CfcProfile::revision(p) != before);
    }
    void verbReaderRoundsAndLocksTheEnds()
    {
        CfcProfile::Profile p;
        QVERIFY(CfcProfile::decode(blob(5), p));
        QJsonObject o = QJsonDocument::fromJson(
            CfcProfile::publishedJson(p, QStringLiteral("saved")).toUtf8()).object();
        o.insert(QStringLiteral("minHz"), 50.0);
        o.insert(QStringLiteral("maxHz"), 3000.0);
        o.insert(QStringLiteral("parametric"), false);
        o.insert(QStringLiteral("precompDb"), 3.04);
        QJsonArray bands = o.value(QStringLiteral("bands")).toArray();
        QJsonObject b1 = bands.at(1).toObject();
        b1.insert(QStringLiteral("compressionQ"), 1.234);
        b1.insert(QStringLiteral("postEqGainDb"), -2.26);
        bands.replace(1, b1);
        o.insert(QStringLiteral("bands"), bands);
        CfcProfile::Profile back;
        QString why;
        QVERIFY2(CfcProfile::fromPublishedJson(
                     QString::fromUtf8(QJsonDocument(o).toJson()), back, &why), qPrintable(why));
        QCOMPARE(back.f.front(), 50.0);
        QCOMPARE(back.f.back(), 3000.0);
        QCOMPARE(back.postF, back.f);
        QCOMPARE(back.qg.at(1), 1.23);
        QCOMPARE(back.e.at(1), -2.3);
        QCOMPARE(back.precompDb, 3.0);
        QVERIFY(!back.compParametric);
        QVERIFY(!back.eqParametric);
    }
    void verbReaderRefusesWhatTheDialogCannotHold_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<int>("band");
        QTest::addColumn<double>("value");
        QTest::addColumn<QString>("reason");
        const QString range = QStringLiteral("Choose a low and a high end from 0 to 20000 Hz, "
                                             "the high end at least 1000 Hz above the low end.");
        QTest::newRow("low below 0") << "minHz" << -1 << -1.0 << range;
        QTest::newRow("high above 20000") << "maxHz" << -1 << 20001.0 << range;
        QTest::newRow("range too narrow") << "maxHz" << -1 << 999.0 << range;
        QTest::newRow("precomp") << "precompDb" << -1 << 16.1
            << QStringLiteral("Choose a pre-compression from 0 to 16 dB.");
        QTest::newRow("post-EQ gain") << "postEqGainDb" << -1 << -24.1
            << QStringLiteral("Choose a post-EQ gain from -24 to 24 dB.");
        QTest::newRow("band compression") << "compressionDb" << 2 << 16.5
            << QStringLiteral("Choose each band's compression from 0 to 16 dB.");
        QTest::newRow("band post-EQ gain") << "postEqGainDb" << 2 << 24.5
            << QStringLiteral("Choose each band's post-EQ gain from -24 to 24 dB.");
        QTest::newRow("compression Q") << "compressionQ" << 2 << 0.1
            << QStringLiteral("Choose each Q from 0.2 to 20.");
        QTest::newRow("post-EQ Q") << "postEqQ" << 2 << 20.5
            << QStringLiteral("Choose each Q from 0.2 to 20.");
        QTest::newRow("band out of order") << "frequencyHz" << 2 << 50.0
            << QStringLiteral("Keep each band's frequency above the one before it.");
        QTest::newRow("band past the high end") << "frequencyHz" << 2 << 9000.0
            << QStringLiteral("Choose each band's frequency between the low and high ends.");
    }
    void verbReaderRefusesWhatTheDialogCannotHold()
    {
        QFETCH(QString, key);
        QFETCH(int, band);
        QFETCH(double, value);
        QFETCH(QString, reason);
        CfcProfile::Profile p;
        QVERIFY(CfcProfile::decode(blob(5), p));   // 0..400 Hz: widen to 0..4000
        p.maxHz = p.postMaxHz = 4000.0;
        for (int i = 0; i < 5; ++i) { p.f[i] = p.postF[i] = i * 1000.0; }
        QJsonObject o = QJsonDocument::fromJson(
            CfcProfile::publishedJson(p, QStringLiteral("saved")).toUtf8()).object();
        if (band < 0) {
            o.insert(key, value);
        } else {
            QJsonArray bands = o.value(QStringLiteral("bands")).toArray();
            QJsonObject b = bands.at(band).toObject();
            b.insert(key, value);
            bands.replace(band, b);
            o.insert(QStringLiteral("bands"), bands);
        }
        CfcProfile::Profile out = p;
        const auto before = out.f;
        QString why;
        QVERIFY(!CfcProfile::fromPublishedJson(QString::fromUtf8(QJsonDocument(o).toJson()),
                                               out, &why));
        QCOMPARE(why, reason);
        QCOMPARE(out.f, before);
    }
    void verbReaderRefusesOtherShapes()
    {
        CfcProfile::Profile out;
        QString why;
        QVERIFY(!CfcProfile::fromPublishedJson(QStringLiteral("[]"), out, &why));
        QCOMPARE(why, QStringLiteral("The CFC settings were not understood."));
        QVERIFY(!CfcProfile::fromPublishedJson(
            QStringLiteral("{\"bands\":[],\"minHz\":0,\"maxHz\":4000,\"parametric\":true,"
                           "\"precompDb\":0,\"postEqGainDb\":0}"), out, &why));
        QCOMPARE(why, QStringLiteral("Choose 5, 10 or 18 bands."));
    }
    void legacyProfileIsWhatTheDialogSeeds()
    {
        const std::array<int, 10> f{0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000};
        const std::array<int, 10> c{5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
        const std::array<int, 10> e{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        const CfcProfile::Profile p = CfcProfile::legacyProfile(f, c, e, 2, -3);
        QCOMPARE(p.minHz, 0.0);
        QCOMPARE(p.maxHz, 10000.0);
        QCOMPARE(p.f.at(4), 1000.0);
        QCOMPARE(p.e.at(9), 9.0);
        QCOMPARE(p.qg.at(0), 4.0);
        QCOMPARE(p.precompDb, 2.0);
        QCOMPARE(p.postEqGainDb, -3.0);
        QVERIFY(p.usesQ());
        CfcProfile::Profile decoded;
        QVERIFY(CfcProfile::decode(CfcProfile::encode(p), decoded));
    }
};

QTEST_GUILESS_MAIN(TstCfcProfile)
#include "tst_cfc_profile.moc"
