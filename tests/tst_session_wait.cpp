// =================================================================
// tests/tst_session_wait.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test of the session tests' waits
// (tests/SessionWait.h).
//
// NEREUS_TRY_COMPARE waits for the same answer QCOMPARE then asserts: for
// floating point that is a fuzzy compare, so a value QCOMPARE accepts ends
// the wait at once instead of after the whole timeout.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-29  J.J. Boyd / KG4VCF  Original test. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "SessionWait.h"

class TstSessionWait : public QObject {
    Q_OBJECT
private slots:
    void doublesWaitForTheCompareQCompareMakes();
    void floatsWaitForTheCompareQCompareMakes();
    void otherTypesStillCompareExactly();
};

namespace {
// 0.1 + 0.2 is not exactly 0.3 in double, nor 1.1f + 2.2f exactly 3.3f in
// float; QCOMPARE accepts both (qFuzzyCompare).
double nearlyThreeTenths()
{
    volatile double a = 0.1;
    volatile double b = 0.2;
    return a + b;
}

float nearlyThreeAndAThird()
{
    volatile float a = 1.1f;
    volatile float b = 2.2f;
    return a + b;
}
} // namespace

void TstSessionWait::doublesWaitForTheCompareQCompareMakes()
{
    QVERIFY(nearlyThreeTenths() != 0.3);
    int asks = 0;
    const auto probe = [&asks]() {
        ++asks;
        return nearlyThreeTenths();
    };
    NEREUS_TRY_COMPARE(probe(), 0.3);
    // Asked once by the wait and once by QCOMPARE: the wait ended on the
    // first answer.
    QCOMPARE(asks, 2);
}

void TstSessionWait::floatsWaitForTheCompareQCompareMakes()
{
    QVERIFY(nearlyThreeAndAThird() != 3.3f);
    int asks = 0;
    const auto probe = [&asks]() {
        ++asks;
        return nearlyThreeAndAThird();
    };
    NEREUS_TRY_COMPARE(probe(), 3.3f);
    QCOMPARE(asks, 2);
}

void TstSessionWait::otherTypesStillCompareExactly()
{
    int asks = 0;
    const auto probe = [&asks]() {
        ++asks;
        return QStringLiteral("keyed");
    };
    NEREUS_TRY_COMPARE(probe(), QStringLiteral("keyed"));
    QCOMPARE(asks, 2);
}

QTEST_GUILESS_MAIN(TstSessionWait)
#include "tst_session_wait.moc"
