// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_unkey_gate.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-03; remote design section 12.2): the
// unkey-confirmed gate confirms after MoxController's rxReady; with rxReady
// kept from happening it reports TimedOut at 2000 ms, and by then the
// emergency stop has been applied. The 2000 ms timer is the test's own
// (an injected scheduler), never a sleep.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-03), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/MoxController.h"
#include "core/safety/UnkeyGate.h"

using namespace NereusSDR;

namespace {

struct Rig {
    MoxController mox;
    QStringList log;
    bool unkeyReleasesMox{true};
    QList<QPair<int, std::function<void()>>> timers;
    std::unique_ptr<UnkeyGate> gate;

    Rig()
    {
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        gate = std::make_unique<UnkeyGate>(
            &mox,
            [this]() {
                log.append(QStringLiteral("unkey"));
                if (unkeyReleasesMox) {
                    mox.setMox(false);
                }
            },
            [this](const QString& reason) { log.append(QStringLiteral("stop:") + reason); });
        gate->setScheduler([this](int ms, QObject*, std::function<void()> fire) {
            timers.append({ms, std::move(fire)});
        });
        QObject::connect(&mox, &MoxController::rxReady, &mox,
                         [this]() { log.append(QStringLiteral("rxReady")); });
    }

    void keyAndSettle()
    {
        mox.setMox(true);
        QTRY_COMPARE(mox.state(), MoxState::Tx);
    }
};

} // namespace

class TstUnkeyGate : public QObject {
    Q_OBJECT

private slots:
    void theTimeoutIs2000Ms() { QCOMPARE(UnkeyGate::kConfirmTimeoutMs, 2000); }

    void confirmsAfterRxReady()
    {
        Rig rig;
        rig.keyAndSettle();
        QObject context;
        std::optional<UnkeyOutcome> outcome;
        rig.gate->unkey(QStringLiteral("handoff"), &context,
                        [&](UnkeyOutcome o) { outcome = o; });
        QCOMPARE(rig.log.value(0), QStringLiteral("unkey"));
        QCOMPARE(rig.timers.size(), 1);
        QCOMPARE(rig.timers.first().first, 2000);
        QVERIFY(!outcome.has_value());   // not before rxReady
        QTRY_VERIFY(outcome.has_value());
        QCOMPARE(*outcome, UnkeyOutcome::Confirmed);
        QVERIFY(rig.log.indexOf(QStringLiteral("rxReady")) >= 0);
        QVERIFY(!rig.log.join(QLatin1Char(' ')).contains(QStringLiteral("stop:")));
        QCOMPARE(rig.gate->pendingCount(), 0);
        // The timeout firing later changes nothing.
        rig.timers.first().second();
        QCOMPARE(*outcome, UnkeyOutcome::Confirmed);
        QVERIFY(!rig.log.join(QLatin1Char(' ')).contains(QStringLiteral("stop:")));
    }

    void alreadyInReceiveConfirmsAtOnce()
    {
        Rig rig;
        QObject context;
        std::optional<UnkeyOutcome> outcome;
        rig.gate->unkey(QStringLiteral("release"), &context, [&](UnkeyOutcome o) { outcome = o; });
        QVERIFY(outcome.has_value());
        QCOMPARE(*outcome, UnkeyOutcome::Confirmed);
        QVERIFY(rig.log.isEmpty());
    }

    void withRxReadyKeptAwayItTimesOutWithTheStopApplied()
    {
        Rig rig;
        rig.keyAndSettle();
        rig.unkeyReleasesMox = false;   // MOX never reaches receive
        QObject context;
        std::optional<UnkeyOutcome> outcome;
        QString logAtOutcome;
        rig.gate->unkey(QStringLiteral("handoff"), &context, [&](UnkeyOutcome o) {
            outcome = o;
            logAtOutcome = rig.log.join(QLatin1Char(' '));
        });
        QCoreApplication::processEvents();
        QVERIFY(!outcome.has_value());
        QCOMPARE(rig.timers.size(), 1);
        QCOMPARE(rig.timers.first().first, 2000);

        rig.timers.first().second();   // 2000 ms
        QVERIFY(outcome.has_value());
        QCOMPARE(*outcome, UnkeyOutcome::TimedOut);
        // Transmit was already stopped when TimedOut was reported.
        QVERIFY(logAtOutcome.contains(QStringLiteral("stop:handoff")));
        QCOMPARE(rig.gate->pendingCount(), 0);
    }

    void aGoneContextHearsNothing()
    {
        Rig rig;
        rig.keyAndSettle();
        bool called = false;
        {
            QObject context;
            rig.gate->unkey(QStringLiteral("handoff"), &context, [&](UnkeyOutcome) { called = true; });
        }
        QTRY_COMPARE(rig.mox.state(), MoxState::Rx);
        QCoreApplication::processEvents();
        QVERIFY(!called);
    }

    void twoWaitersBothHearTheOneReceive()
    {
        Rig rig;
        rig.keyAndSettle();
        QObject context;
        int confirmed = 0;
        rig.gate->unkey(QStringLiteral("a"), &context, [&](UnkeyOutcome o) {
            confirmed += o == UnkeyOutcome::Confirmed ? 1 : 0;
        });
        rig.gate->unkey(QStringLiteral("b"), &context, [&](UnkeyOutcome o) {
            confirmed += o == UnkeyOutcome::Confirmed ? 1 : 0;
        });
        QTRY_COMPARE(confirmed, 2);
    }
};

QTEST_MAIN(TstUnkeyGate)
#include "tst_unkey_gate.moc"
