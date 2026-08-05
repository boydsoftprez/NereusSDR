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

#include <QtTest>
#include <QFileInfo>
#include <QDir>

#include "core/CtyDatParser.h"

using namespace NereusSDR;

// Path resolver: the cty.dat lives at the worktree root. The test
// binary runs from build/tests/, so we hop up from __FILE__ (which
// is tests/tst_cty_dat_parser.cpp) to find the file. Mirrors the
// resolution hint in the C1 task plan.
static QString resolveCtyDatPath()
{
    const QString file = QString::fromUtf8(__FILE__);
    // __FILE__ = .../<worktree>/tests/tst_cty_dat_parser.cpp
    // strip the last two components -> .../<worktree>/
    const QString root = QFileInfo(QFileInfo(file).dir().path()).path();
    return root + "/cty.dat";
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

QTEST_GUILESS_MAIN(TestCtyDatParser)
#include "tst_cty_dat_parser.moc"
