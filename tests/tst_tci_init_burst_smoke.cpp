// no-port-check: NereusSDR-original smoke test for the init-burst wrapper.
// Phase 3J-1 Task 4.1 — three asserts only:
//   1. buildInitBurst() returns a non-empty list.
//   2. Last entry equals "ready;".
//   3. First entry starts with "protocol:".
// Strict-protocol byte-for-byte assertions live in Phase 4 Task 4.3 golden file.

#include <QtTest>
#include "core/TciProtocol.h"
#include "TestMockRadioModel.h"

using namespace NereusSDR;

class TestTciInitBurstSmoke : public QObject {
    Q_OBJECT
private slots:
    void burst_is_non_empty();
    void last_entry_is_ready();
    void first_entry_is_protocol();
};

void TestTciInitBurstSmoke::burst_is_non_empty()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    QVERIFY(!p.buildInitBurst().isEmpty());
}

void TestTciInitBurstSmoke::last_entry_is_ready()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    const QStringList lines = p.buildInitBurst();
    QVERIFY(!lines.isEmpty());
    QCOMPARE(lines.last(), QStringLiteral("ready;"));
}

void TestTciInitBurstSmoke::first_entry_is_protocol()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    const QStringList lines = p.buildInitBurst();
    QVERIFY(!lines.isEmpty());
    QVERIFY(lines.first().startsWith(QStringLiteral("protocol:")));
}

QTEST_GUILESS_MAIN(TestTciInitBurstSmoke)
#include "tst_tci_init_burst_smoke.moc"
