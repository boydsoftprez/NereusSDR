// tests/tst_tci_dispatch_seam.cpp  (NereusSDR)
// NereusSDR-original — no Thetis upstream port in this file.
//
// Phase 3J-1 Task 3.1: TciProtocol dispatch seam.
// Verifies:
//   1. Unknown command -> empty response, 0 notifications (silent-error invariant
//      per design doc §4.1). The command still routes through one dispatch branch.
//   2. `vfo;` (no args) routes to query path (queryDispatchCount increments).
//   3. `vfo:0,0,14250000;` (with args) routes to set path (setDispatchCount increments).
//   4. Empty / semicolon-only input returns empty and increments no counter.
//   5. Trailing semicolon is stripped: `vfo;` and `vfo` route identically.

#include <QtTest>
#include "core/TciProtocol.h"
#include "TestMockRadioModel.h"

using namespace NereusSDR;

class TestTciDispatchSeam : public QObject {
    Q_OBJECT
private slots:
    void unknown_command_silent_error_invariant();
    void vfo_no_args_routes_to_query();
    void vfo_with_args_routes_to_set();
    void empty_command_returns_empty();
    void trailing_semicolon_stripped();
};

void TestTciDispatchSeam::unknown_command_silent_error_invariant()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    const QString r = p.handleCommand(QStringLiteral("nonexistent_command_xyz;"));
    QCOMPARE(r, QString());
    QVERIFY(!p.hasPendingNotification());
    // Silent error invariant: even an unknown command DOES route through
    // handleSetCommand or handleQueryCommand (the case-default-empty path).
    // The "silence" is in the wire output, not the dispatch routing.
    QCOMPARE(p.setDispatchCount() + p.queryDispatchCount(), 1);
}

void TestTciDispatchSeam::vfo_no_args_routes_to_query()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    p.handleCommand(QStringLiteral("vfo;"));
    QCOMPARE(p.queryDispatchCount(), 1);
    QCOMPARE(p.setDispatchCount(), 0);
}

void TestTciDispatchSeam::vfo_with_args_routes_to_set()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    p.handleCommand(QStringLiteral("vfo:0,0,14250000;"));
    QCOMPARE(p.setDispatchCount(), 1);
    QCOMPARE(p.queryDispatchCount(), 0);
}

void TestTciDispatchSeam::empty_command_returns_empty()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    QCOMPARE(p.handleCommand(QString()), QString());
    QCOMPARE(p.handleCommand(QStringLiteral(";")), QString());
    QCOMPARE(p.setDispatchCount(), 0);
    QCOMPARE(p.queryDispatchCount(), 0);
}

void TestTciDispatchSeam::trailing_semicolon_stripped()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    // Both forms should route identically (trailing ';' is stripped per Thetis).
    p.handleCommand(QStringLiteral("vfo;"));
    p.resetDispatchCounters();
    p.handleCommand(QStringLiteral("vfo"));
    QCOMPARE(p.queryDispatchCount(), 1);
}

QTEST_GUILESS_MAIN(TestTciDispatchSeam)
#include "tst_tci_dispatch_seam.moc"
