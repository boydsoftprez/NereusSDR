// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: This test references real DXCC entity callsigns (W1AW,
// JA1ABC, VK6APH, G3OCA, K1EA) as fixtures for cty.dat lookup. They are
// well-known callsigns used as test inputs, not ported callsigns.
// Precedent: B2-B5.
//
// NereusSDR - CtyDatParser tests
//
// Phase 3J-2 Task C1. Pins the contract that CtyDatParser loads
// cty.dat (AD1C / K1EA Country File) and resolves callsign prefixes
// to DXCC entities (primary prefix + entity name + continent + CQ
// zone + ITU zone) via longest-prefix match. Six tests:
//   - loadsRealCtyDat: real cty.dat at the repo root loads and parses
//     at least one entity.
//   - resolvesUSACallsign: W1AW maps to entity "United States" /
//     primary prefix "K" / continent "NA".
//   - resolvesJapanCallsign: JA1ABC maps to "Japan" / "JA" / "AS".
//   - resolvesAustraliaCallsign: VK6APH maps to "Australia" / "VK" /
//     "OC".
//   - resolvesEnglandCallsign: G3OCA maps to "England" / "G" / "EU".
//   - rejectsEmptyOrInvalidCallsign: empty string returns empty
//     prefix; total-gibberish callsign with no plausible prefix
//     match returns empty as well.
//
// 2026-10-07 (rotor control, bearings): entity positions from the header
// columns with longitude turned to + east, and <lat/long> alias overrides
// winning over the entity position for the alias that matched.
//   - entityPositionsFromHeader: Japan 36.40 N 138.38 E, United States
//     37.60 N 91.87 W, as the repo's cty.dat writes them (36.40 / -138.38
//     and 37.60 / 91.87, longitude + west).
//   - latLongOverrideWinsForItsAlias: a synthetic cty.dat with a prefix
//     override and an exact-call override; each wins only for its alias.
//   - positionForUnknownCallsignIsEmpty.

#include <QtTest>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>

#include "core/CtyDatParser.h"

using namespace NereusSDR;

// Qt resolves fixtures from the configured source directory even when
// compiler caching rewrites __FILE__ to a path relative to the build root.
static QString resolveCtyDatPath()
{
    return QFINDTESTDATA("../cty.dat");
}

class TestCtyDatParser : public QObject {
    Q_OBJECT
private slots:
    void loadsRealCtyDat();
    void resolvesUSACallsign();
    void resolvesJapanCallsign();
    void resolvesAustraliaCallsign();
    void resolvesEnglandCallsign();
    void rejectsEmptyOrInvalidCallsign();
    void entityPositionsFromHeader();
    void latLongOverrideWinsForItsAlias();
    void positionForUnknownCallsignIsEmpty();
};

void TestCtyDatParser::loadsRealCtyDat()
{
    CtyDatParser parser;
    const QString ctyPath = resolveCtyDatPath();
    QVERIFY2(parser.loadFromFile(ctyPath),
             qPrintable(QString("cty.dat path: %1").arg(ctyPath)));
    QVERIFY(parser.entityCount() > 0);
    QVERIFY(parser.isLoaded());
}

void TestCtyDatParser::resolvesUSACallsign()
{
    CtyDatParser parser;
    QVERIFY(parser.loadFromFile(resolveCtyDatPath()));

    const QString prefix = parser.resolvePrimaryPrefix("W1AW");
    QCOMPARE(prefix, QStringLiteral("K"));

    const DxccEntity* e = parser.entityByPrefix(prefix);
    QVERIFY(e != nullptr);
    QVERIFY2(e->name.contains("United States", Qt::CaseInsensitive),
             qPrintable(QString("entity name: %1").arg(e->name)));
    QCOMPARE(e->continent, QStringLiteral("NA"));
    QVERIFY(e->cqZone > 0);
    QVERIFY(e->ituZone > 0);
}

void TestCtyDatParser::resolvesJapanCallsign()
{
    CtyDatParser parser;
    QVERIFY(parser.loadFromFile(resolveCtyDatPath()));

    const QString prefix = parser.resolvePrimaryPrefix("JA1ABC");
    QCOMPARE(prefix, QStringLiteral("JA"));

    const DxccEntity* e = parser.entityByPrefix(prefix);
    QVERIFY(e != nullptr);
    QVERIFY2(e->name.contains("Japan", Qt::CaseInsensitive),
             qPrintable(QString("entity name: %1").arg(e->name)));
    QCOMPARE(e->continent, QStringLiteral("AS"));
}

void TestCtyDatParser::resolvesAustraliaCallsign()
{
    CtyDatParser parser;
    QVERIFY(parser.loadFromFile(resolveCtyDatPath()));

    const QString prefix = parser.resolvePrimaryPrefix("VK6APH");
    QCOMPARE(prefix, QStringLiteral("VK"));

    const DxccEntity* e = parser.entityByPrefix(prefix);
    QVERIFY(e != nullptr);
    QVERIFY2(e->name.contains("Australia", Qt::CaseInsensitive),
             qPrintable(QString("entity name: %1").arg(e->name)));
    QCOMPARE(e->continent, QStringLiteral("OC"));
}

void TestCtyDatParser::resolvesEnglandCallsign()
{
    CtyDatParser parser;
    QVERIFY(parser.loadFromFile(resolveCtyDatPath()));

    const QString prefix = parser.resolvePrimaryPrefix("G3OCA");
    QCOMPARE(prefix, QStringLiteral("G"));

    const DxccEntity* e = parser.entityByPrefix(prefix);
    QVERIFY(e != nullptr);
    QVERIFY2(e->name.contains("England", Qt::CaseInsensitive),
             qPrintable(QString("entity name: %1").arg(e->name)));
    QCOMPARE(e->continent, QStringLiteral("EU"));
}

void TestCtyDatParser::rejectsEmptyOrInvalidCallsign()
{
    CtyDatParser parser;
    QVERIFY(parser.loadFromFile(resolveCtyDatPath()));

    // Empty callsign returns empty prefix.
    QVERIFY(parser.resolvePrimaryPrefix(QString()).isEmpty());

    // entityByPrefix on a nonsense primary prefix returns nullptr.
    QVERIFY(parser.entityByPrefix(QStringLiteral("ZZZZ")) == nullptr);
}

void TestCtyDatParser::entityPositionsFromHeader()
{
    CtyDatParser parser;
    QVERIFY(parser.loadFromFile(resolveCtyDatPath()));

    // cty.dat: "Japan: 25: 45: AS: 36.40: -138.38: -9.0: JA:"
    const DxccEntity* ja = parser.entityByPrefix(QStringLiteral("JA"));
    QVERIFY(ja != nullptr);
    QCOMPARE(ja->latitude, 36.40);
    QCOMPARE(ja->longitude, 138.38);

    // cty.dat: "United States: 05: 08: NA: 37.60: 91.87: 5.0: K:"
    const DxccEntity* k = parser.entityByPrefix(QStringLiteral("K"));
    QVERIFY(k != nullptr);
    QCOMPARE(k->latitude, 37.60);
    QCOMPARE(k->longitude, -91.87);

    const auto jaPos = parser.positionForCallsign(QStringLiteral("JA1ABC"));
    QVERIFY(jaPos.has_value());
    QCOMPARE(jaPos->latitudeDeg, 36.40);
    QCOMPARE(jaPos->longitudeDeg, 138.38);

    const auto kPos = parser.positionForCallsign(QStringLiteral("w1aw"));
    QVERIFY(kPos.has_value());
    QCOMPARE(kPos->latitudeDeg, 37.60);
    QCOMPARE(kPos->longitudeDeg, -91.87);
}

void TestCtyDatParser::latLongOverrideWinsForItsAlias()
{
    // Override syntax <lat/long> from the cty.dat format description
    // (https://www.country-files.com/cty-dat-format/), written like the
    // header columns: latitude + north, longitude + west.
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(
        "United States:            05:  08:  NA:   37.60:    91.87:     5.0:  K:\n"
        "    K,W,KL7(1)[1]<61.20/149.90>,\n"
        "    =W1XYZ(5)[8]<42.50/71.25>,=W1ABC;\n"
        "Japan:                    25:  45:  AS:   36.40:  -138.38:    -9.0:  JA:\n"
        "    JA,JR<35.00/-139.50>;\n");
    file.close();

    CtyDatParser parser;
    QVERIFY(parser.loadFromFile(file.fileName()));

    // Prefix aliases still resolve to their entity, overrides stripped.
    QCOMPARE(parser.resolvePrimaryPrefix(QStringLiteral("KL7AB")), QStringLiteral("K"));
    QCOMPARE(parser.resolvePrimaryPrefix(QStringLiteral("W1XYZ")), QStringLiteral("K"));
    QCOMPARE(parser.resolvePrimaryPrefix(QStringLiteral("JR1ABC")), QStringLiteral("JA"));

    // Prefix override wins for its alias.
    const auto kl7 = parser.positionForCallsign(QStringLiteral("KL7AB"));
    QVERIFY(kl7.has_value());
    QCOMPARE(kl7->latitudeDeg, 61.20);
    QCOMPARE(kl7->longitudeDeg, -149.90);

    // Exact-call override wins for that call.
    const auto exact = parser.positionForCallsign(QStringLiteral("W1XYZ"));
    QVERIFY(exact.has_value());
    QCOMPARE(exact->latitudeDeg, 42.50);
    QCOMPARE(exact->longitudeDeg, -71.25);

    // Negative (east) longitude in an override.
    const auto jr = parser.positionForCallsign(QStringLiteral("JR1ABC"));
    QVERIFY(jr.has_value());
    QCOMPARE(jr->latitudeDeg, 35.00);
    QCOMPARE(jr->longitudeDeg, 139.50);

    // Aliases without an override keep the entity position.
    for (const QString& call : {QStringLiteral("W1AW"), QStringLiteral("W1ABC"),
                                QStringLiteral("K1ABC")}) {
        const auto pos = parser.positionForCallsign(call);
        QVERIFY2(pos.has_value(), qPrintable(call));
        QCOMPARE(pos->latitudeDeg, 37.60);
        QCOMPARE(pos->longitudeDeg, -91.87);
    }
    const auto ja = parser.positionForCallsign(QStringLiteral("JA1ABC"));
    QVERIFY(ja.has_value());
    QCOMPARE(ja->latitudeDeg, 36.40);
    QCOMPARE(ja->longitudeDeg, 138.38);
}

void TestCtyDatParser::positionForUnknownCallsignIsEmpty()
{
    CtyDatParser parser;
    QVERIFY(parser.loadFromFile(resolveCtyDatPath()));
    QVERIFY(!parser.positionForCallsign(QString()).has_value());

    CtyDatParser empty;
    QVERIFY(!empty.positionForCallsign(QStringLiteral("W1AW")).has_value());
}

QTEST_GUILESS_MAIN(TestCtyDatParser)
#include "tst_cty_dat_parser.moc"
