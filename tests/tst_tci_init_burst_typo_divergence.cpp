// no-port-check: NereusSDR-original test for the design-doc §7 row 1 typo divergence.
// Phase 3J-1 Task 4.2: verify our init burst contains BOTH `if:1,0,...;` AND
// `if:1,1,...;` (the intended cross-product), where Thetis TCIServer.cs:2374-2375
// [v2.10.3.13] mistakenly calls sendIF(1,1) twice (copy-paste bug). We emit the
// intended (1,0)+(1,1) cross-product instead, matching the sendVFO enumeration.

#include <QtTest>
#include "core/TciProtocol.h"
#include "TestMockRadioModel.h"

using namespace NereusSDR;

class TestTciInitBurstTypoDivergence : public QObject {
    Q_OBJECT
private slots:
    void burst_contains_if_1_0_and_if_1_1();
    void burst_contains_no_duplicate_if_1_1();
};

// Asserts that both halves of the intended cross-product are present.
// Thetis TCIServer.cs:2374-2375 [v2.10.3.13] emits if:1,1 twice;
// NereusSDR emits if:1,0 + if:1,1 per design doc §7 row 1.
void TestTciInitBurstTypoDivergence::burst_contains_if_1_0_and_if_1_1()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    const QStringList lines = p.buildInitBurst();

    bool hasIf10 = false;
    bool hasIf11 = false;
    for (const auto& line : lines) {
        if (line.startsWith(QStringLiteral("if:1,0,"))) { hasIf10 = true; }
        if (line.startsWith(QStringLiteral("if:1,1,"))) { hasIf11 = true; }
    }
    QVERIFY2(hasIf10, "Burst missing if:1,0,...; (typo fix asserts both halves of cross-product)");
    QVERIFY2(hasIf11, "Burst missing if:1,1,...;");
}

// Asserts exactly one if:1,1 line — Thetis sends 2 (the typo), we send 1.
void TestTciInitBurstTypoDivergence::burst_contains_no_duplicate_if_1_1()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    const QStringList lines = p.buildInitBurst();

    int countIf11 = 0;
    for (const auto& line : lines) {
        if (line.startsWith(QStringLiteral("if:1,1,"))) { ++countIf11; }
    }
    QCOMPARE(countIf11, 1);  // exactly one — Thetis sends 2 (the typo)
}

QTEST_GUILESS_MAIN(TestTciInitBurstTypoDivergence)
#include "tst_tci_init_burst_typo_divergence.moc"
