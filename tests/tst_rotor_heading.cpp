// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - strict rotor headings (rotor control plan, Task 3a).
//
// RotorHeading is ported from Longpath src/core/RotorPeilung.h [@551576e].
// A heading that gets through here turns a real mast, so every case below
// is one that must be refused rather than turned into something else:
// empty text is not north, -90 is not 270, 361 is not 1.
//
// Modification history (NereusSDR):
//   2026-10-08: Initial version. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.

#include <QtTest>

#include "core/RotorHeading.h"

#include <limits>

using namespace NereusSDR;

class TestRotorHeading : public QObject {
    Q_OBJECT

private slots:
    void acceptsTextInRange_data();
    void acceptsTextInRange();
    void refusesText_data();
    void refusesText();
    void refusedTextLeavesValueAlone();
    void acceptsNumbers();
    void refusesNumbers_data();
    void refusesNumbers();
};

void TestRotorHeading::acceptsTextInRange_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<double>("expected");

    QTest::newRow("zero") << QStringLiteral("0") << 0.0;
    QTest::newRow("padded") << QStringLiteral("010") << 10.0;
    QTest::newRow("fraction") << QStringLiteral("123.5") << 123.5;
    QTest::newRow("spaces around") << QStringLiteral("  275 ") << 275.0;
    QTest::newRow("just under north") << QStringLiteral("359.9") << 359.9;
    // 360 is north and is sent as 0.
    QTest::newRow("360 is north") << QStringLiteral("360") << 0.0;
    QTest::newRow("360.0 is north") << QStringLiteral("360.0") << 0.0;
}

void TestRotorHeading::acceptsTextInRange()
{
    QFETCH(QString, text);
    QFETCH(double, expected);
    double deg = -1.0;
    QVERIFY(RotorHeading::parse(text, &deg));
    QCOMPARE(deg, expected);
}

void TestRotorHeading::refusesText_data()
{
    QTest::addColumn<QString>("text");

    // An empty box is refused, never north.
    QTest::newRow("empty") << QString();
    QTest::newRow("blank") << QStringLiteral("   ");
    QTest::newRow("letters") << QStringLiteral("abc");
    QTest::newRow("trailing letters") << QStringLiteral("90deg");
    QTest::newRow("nan") << QStringLiteral("nan");
    QTest::newRow("NaN") << QStringLiteral("NaN");
    QTest::newRow("inf") << QStringLiteral("inf");
    QTest::newRow("-inf") << QStringLiteral("-inf");
    // Out of range is refused, never wrapped.
    QTest::newRow("negative") << QStringLiteral("-90");
    QTest::newRow("just below zero") << QStringLiteral("-0.1");
    QTest::newRow("361") << QStringLiteral("361");
    QTest::newRow("just over north") << QStringLiteral("360.1");
    QTest::newRow("overlap heading") << QStringLiteral("450");
}

void TestRotorHeading::refusesText()
{
    QFETCH(QString, text);
    double deg = -1.0;
    QVERIFY(!RotorHeading::parse(text, &deg));
}

void TestRotorHeading::refusedTextLeavesValueAlone()
{
    double deg = 123.0;
    QVERIFY(!RotorHeading::parse(QString(), &deg));
    QCOMPARE(deg, 123.0);
    QVERIFY(!RotorHeading::parse(QStringLiteral("-90"), &deg));
    QCOMPARE(deg, 123.0);
    // A null out-pointer is allowed: the check alone.
    QVERIFY(RotorHeading::parse(QStringLiteral("90"), nullptr));
    QVERIFY(!RotorHeading::parse(QStringLiteral("nan"), nullptr));
}

void TestRotorHeading::acceptsNumbers()
{
    double deg = -1.0;
    QVERIFY(RotorHeading::accept(0.0, &deg));
    QCOMPARE(deg, 0.0);
    QVERIFY(RotorHeading::accept(187.25, &deg));
    QCOMPARE(deg, 187.25);
    QVERIFY(RotorHeading::accept(360.0, &deg));
    QCOMPARE(deg, 0.0);
    QCOMPARE(RotorHeading::kMaxDeg, 360.0);
}

void TestRotorHeading::refusesNumbers_data()
{
    QTest::addColumn<double>("value");

    QTest::newRow("quiet nan") << std::numeric_limits<double>::quiet_NaN();
    QTest::newRow("+inf") << std::numeric_limits<double>::infinity();
    QTest::newRow("-inf") << -std::numeric_limits<double>::infinity();
    QTest::newRow("-90") << -90.0;
    QTest::newRow("tiny negative") << -1e-9;
    QTest::newRow("361") << 361.0;
    QTest::newRow("450") << 450.0;
    QTest::newRow("max double") << std::numeric_limits<double>::max();
}

void TestRotorHeading::refusesNumbers()
{
    QFETCH(double, value);
    double deg = 42.0;
    QVERIFY(!RotorHeading::accept(value, &deg));
    QCOMPARE(deg, 42.0);
}

QTEST_APPLESS_MAIN(TestRotorHeading)
#include "tst_rotor_heading.moc"
