// =================================================================
// tests/tst_tci_update_gap.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test for TciUpdateGap, the port of
// Thetis TCPIPtciSocketListener VFOChange / CentreChange /
// TXFrequencyChange (TCIServer.cs:6421-6480 [v2.10.3.15]).
//
// Receiver and transmit gaps plan, Task 10 (R-R3-49), 2026-09-24, J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// The gate logic runs on a clock the test passes in, so the timing cases
// are exact. The TciServer cases check the wiring with a real server and a
// real app on loopback; nothing there keys a radio (no RadioModel).
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/TciUpdateGap.h"

#ifdef HAVE_WEBSOCKETS
#include <QSignalSpy>
#include <QUrl>
#include <QWebSocket>

#include "core/TciProtocol.h"
#include "core/TciServer.h"
#endif

using namespace NereusSDR;

namespace {

QString vfoLine(int rx, qint64 hz)
{
    return QStringLiteral("vfo:%1,0,%2;").arg(rx).arg(hz);
}

// Drives one gate the way TciServer does: a 5 ms drain tick, a change on
// some ticks, the waiting lines checked on every tick. Returns each line
// sent with the time it went.
struct Sent {
    qint64 atMs;
    QString line;
};

} // namespace

class TestTciUpdateGap : public QObject {
    Q_OBJECT
private slots:
    void linesGoToTheirThetisGate()
    {
        using G = TciUpdateGap::Gate;
        QCOMPARE(TciUpdateGap::gateOf(QStringLiteral("vfo:0,1,14074000;")), G::Vfo);
        QCOMPARE(TciUpdateGap::gateOf(QStringLiteral("if:0,0,-300;")), G::Vfo);
        QCOMPARE(TciUpdateGap::gateOf(QStringLiteral("dds:1,7074000;")), G::Centre);
        QCOMPARE(TciUpdateGap::gateOf(QStringLiteral("tx_frequency:14074000;")), G::TxFrequency);
        QCOMPARE(TciUpdateGap::gateOf(QStringLiteral("tx_frequency_thetis:14074000,b20m,false,false;")),
                 G::TxFrequency);
        QVERIFY(!TciUpdateGap::gateOf(QStringLiteral("modulation:0,usb;")).has_value());
        QVERIFY(!TciUpdateGap::gateOf(QStringLiteral("trx:0,true;")).has_value());
        QVERIFY(!TciUpdateGap::gateOf(QStringLiteral("vfo_limits:0,61440000;")).has_value());

        QCOMPARE(TciUpdateGap::keyOf(QStringLiteral("vfo:0,1,14074000;")), QStringLiteral("vfo:0,1"));
        QCOMPARE(TciUpdateGap::keyOf(QStringLiteral("dds:1,7074000;")), QStringLiteral("dds:1"));
        QCOMPARE(TciUpdateGap::keyOf(QStringLiteral("tx_frequency:1;")), QStringLiteral("tx_frequency"));
    }

    void thetisRangeAndDefault()
    {
        TciUpdateGap gap;
        QCOMPARE(gap.gapMs(), 100);
        gap.setGapMs(-5);
        QCOMPARE(gap.gapMs(), 0);
        gap.setGapMs(5000);
        QCOMPARE(gap.gapMs(), 1000);
        QCOMPARE(QString::fromLatin1(TciUpdateGap::kSettingKey), QStringLiteral("TciRateLimitMs"));
    }

    // A tuning knob spun for one second (a change every 10 ms) with the
    // default gap: the app's updates are never closer together than the
    // gap, and the last frequency arrives once the spinning stops.
    void updatesToOneAppAreNoCloserThanTheGap()
    {
        for (const int gapMs : {100, 250, 1000}) {
            TciUpdateGap gap;
            gap.setGapMs(gapMs);
            QList<Sent> sent;
            qint64 lastHz = 0;
            for (qint64 t = 0; t <= 3000; t += 5) {
                if (t <= 1000 && t % 10 == 0) {
                    lastHz = 14000000 + t;
                    for (const QString& line : gap.offer({vfoLine(0, lastHz)}, t)) {
                        sent.append({t, line});
                    }
                }
                for (const QString& line : gap.takeDue(t)) {
                    sent.append({t, line});
                }
            }
            QVERIFY2(sent.size() >= 2, qPrintable(QString::number(sent.size())));
            QCOMPARE(sent.first().line, vfoLine(0, 14000000));
            QCOMPARE(sent.first().atMs, qint64(0));
            QCOMPARE(sent.last().line, vfoLine(0, lastHz));
            for (int i = 1; i < sent.size(); ++i) {
                const qint64 apart = sent.at(i).atMs - sent.at(i - 1).atMs;
                QVERIFY2(apart >= gapMs,
                         qPrintable(QStringLiteral("gap %1: sends %2 ms apart at %3")
                                        .arg(gapMs).arg(apart).arg(sent.at(i).atMs)));
            }
            // Thetis sends at once when more than the gap has passed, so a
            // steady spin sends once per (gap + one change interval).
            QVERIFY2(sent.size() <= 1000 / gapMs + 2, qPrintable(QString::number(sent.size())));
            QVERIFY(!gap.hasWaiting());
        }
    }

    void zeroSendsEveryChange()
    {
        TciUpdateGap gap;
        gap.setGapMs(0);
        for (qint64 t = 0; t < 100; ++t) {
            const QString line = vfoLine(0, 14000000 + t);
            QCOMPARE(gap.offer({line}, t), QStringList{line});
            QVERIFY(!gap.hasWaiting());
            QVERIFY(gap.takeDue(t).isEmpty());
        }
        // Even two changes in the same millisecond.
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 200), QStringList{vfoLine(0, 1)});
        QCOMPARE(gap.offer({vfoLine(0, 2)}, 200), QStringList{vfoLine(0, 2)});
    }

    // Thetis: a waiting update goes m_nRateLimit ms after it arrived, and a
    // newer arrival cancels that wait and starts it again.
    void theWaitRunsFromTheLatestChange()
    {
        TciUpdateGap gap;  // 100 ms
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 0).size(), 1);
        QVERIFY(gap.offer({vfoLine(0, 2)}, 30).isEmpty());
        QVERIFY(gap.takeDue(129).isEmpty());
        QVERIFY(gap.offer({vfoLine(0, 3)}, 60).isEmpty());
        QVERIFY(gap.takeDue(130).isEmpty());   // restarted at 60
        QVERIFY(gap.takeDue(159).isEmpty());
        QCOMPARE(gap.takeDue(160), QStringList{vfoLine(0, 3)});
        QVERIFY(!gap.hasWaiting());
    }

    // Thetis compares ElapsedMilliseconds > m_nRateLimit: at exactly the
    // gap the update still waits.
    void exactlyTheGapStillWaits()
    {
        TciUpdateGap gap;
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 0).size(), 1);
        QVERIFY(gap.offer({vfoLine(0, 2)}, 100).isEmpty());
        QCOMPARE(gap.offer({vfoLine(0, 3)}, 101), QStringList{vfoLine(0, 3)});
        QVERIFY(!gap.hasWaiting());  // the waiting line was replaced, not sent
    }

    // Thetis's deferred send (VFOcallback) does not restart the stopwatch,
    // so a change just after it goes at once when the last immediate send
    // is older than the gap. Pinned so a change to that is deliberate.
    void theDeferredSendDoesNotRestartTheGap()
    {
        TciUpdateGap gap;
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 0).size(), 1);
        QVERIFY(gap.offer({vfoLine(0, 2)}, 50).isEmpty());
        QCOMPARE(gap.takeDue(150), QStringList{vfoLine(0, 2)});
        QCOMPARE(gap.offer({vfoLine(0, 3)}, 160), QStringList{vfoLine(0, 3)});
    }

    void theThreeGatesAreIndependent()
    {
        TciUpdateGap gap;
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 0).size(), 1);
        QCOMPARE(gap.offer({QStringLiteral("dds:0,1;")}, 10).size(), 1);
        QCOMPARE(gap.offer({QStringLiteral("tx_frequency:1;")}, 20).size(), 1);
        QVERIFY(gap.offer({vfoLine(0, 2)}, 30).isEmpty());
        QCOMPARE(gap.offer({QStringLiteral("dds:0,2;")}, 111), QStringList{QStringLiteral("dds:0,2;")});
    }

    // Every other line goes at once and keeps its place.
    void otherLinesAreNeverHeld()
    {
        TciUpdateGap gap;
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 0).size(), 1);
        const QStringList tick = {QStringLiteral("modulation:0,usb;"), vfoLine(0, 2),
                                  QStringLiteral("rx_enable:1,true;")};
        QCOMPARE(gap.offer(tick, 10),
                 (QStringList{QStringLiteral("modulation:0,usb;"), QStringLiteral("rx_enable:1,true;")}));
        QCOMPARE(gap.takeDue(110), QStringList{vfoLine(0, 2)});
    }

    // One tick's lines of one gate are one Thetis event: they go together.
    void oneTicksLinesGoTogether()
    {
        TciUpdateGap gap;
        const QStringList tick = {QStringLiteral("vfo:0,0,5;"), QStringLiteral("vfo:0,1,5;"),
                                  QStringLiteral("dds:0,5;")};
        QCOMPARE(gap.offer(tick, 0), tick);
    }

    // Recorded difference from Thetis (Task 10 report): its waiting slot
    // holds one update, so a second receiver's change inside the gap would
    // lose the first receiver's last frequency. Here both arrive.
    void everyReceiversLastFrequencyArrives()
    {
        TciUpdateGap gap;
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 0).size(), 1);
        QVERIFY(gap.offer({vfoLine(0, 2)}, 20).isEmpty());
        QVERIFY(gap.offer({vfoLine(1, 7)}, 40).isEmpty());
        QVERIFY(gap.offer({vfoLine(0, 3)}, 60).isEmpty());
        QCOMPARE(gap.takeDue(160), (QStringList{vfoLine(0, 3), vfoLine(1, 7)}));
    }

    // Sending at once also sends what another receiver had waiting, but not
    // an older value of a line this tick replaces.
    void sendingNowFlushesOtherReceiversWaitingLines()
    {
        TciUpdateGap gap;
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 0).size(), 1);
        QVERIFY(gap.offer({vfoLine(0, 2), vfoLine(1, 7)}, 20).isEmpty());
        QCOMPARE(gap.offer({vfoLine(0, 3)}, 101), (QStringList{vfoLine(1, 7), vfoLine(0, 3)}));
        QVERIFY(!gap.hasWaiting());
    }

    // Thetis sends a centre change's dds and its if together under the
    // centre gate (TCIServer.cs:1378-1382 [v2.10.3.15]), and a VFO change's
    // if and vfo under the VFO gate. So while the VFO gate is waiting, a
    // centre change still sends its dds and if at once, and the if a VFO
    // change made still waits with its vfo.
    void aCentreChangeSendsItsDdsAndIfTogether()
    {
        TciUpdateGap gap;  // 100 ms
        const QString vfoIf = QStringLiteral("if:0,0,-300;");
        QCOMPARE(gap.offer({vfoIf, vfoLine(0, 1)}, 0).size(), 2);
        QVERIFY(gap.offer({QStringLiteral("if:0,0,-200;"), vfoLine(0, 2)}, 10).isEmpty());

        const QStringList centre = {QStringLiteral("dds:0,14000000;"),
                                    QStringLiteral("if:0,0,-100;")};
        QCOMPARE(gap.offer(centre, 20), centre);

        QCOMPARE(gap.takeDue(110),
                 (QStringList{QStringLiteral("if:0,0,-200;"), vfoLine(0, 2)}));
        QVERIFY(!gap.hasWaiting());

        const auto gates = TciUpdateGap::gatesOf(
            {QStringLiteral("dds:1,7000000;"), QStringLiteral("if:1,0,5;"),
             QStringLiteral("if:0,0,5;")});
        QCOMPARE(gates.size(), std::size_t(3));
        QCOMPARE(gates[0], std::optional(TciUpdateGap::Gate::Centre));
        QCOMPARE(gates[1], std::optional(TciUpdateGap::Gate::Centre));
        QCOMPARE(gates[2], std::optional(TciUpdateGap::Gate::Vfo));
    }

    // A changed gap reaches lines already waiting: they move to the last
    // send plus the new gap, or go on the next tick if that is past.
    void aShorterGapSendsWaitingLinesSooner()
    {
        TciUpdateGap gap;  // 100 ms
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 0).size(), 1);
        QVERIFY(gap.offer({vfoLine(0, 2)}, 30).isEmpty());   // due at 130
        gap.setGapMs(50);                                      // now due at 50
        QVERIFY(gap.takeDue(49).isEmpty());
        QCOMPARE(gap.takeDue(50), QStringList{vfoLine(0, 2)});

        QCOMPARE(gap.offer({vfoLine(0, 3)}, 60).size(), 1);
        QVERIFY(gap.offer({vfoLine(0, 4)}, 70).isEmpty());    // due at 120
        gap.setGapMs(5);                                       // 65 is already past
        QCOMPARE(gap.takeDue(75), QStringList{vfoLine(0, 4)});
    }

    void aLongerGapHoldsWaitingLinesLonger()
    {
        TciUpdateGap gap;  // 100 ms
        QCOMPARE(gap.offer({vfoLine(0, 1)}, 0).size(), 1);
        QVERIFY(gap.offer({vfoLine(0, 2)}, 30).isEmpty());   // due at 130
        gap.setGapMs(300);                                     // now due at 300
        QVERIFY(gap.takeDue(130).isEmpty());
        QVERIFY(gap.takeDue(299).isEmpty());
        QCOMPARE(gap.takeDue(300), QStringList{vfoLine(0, 2)});
    }

    // Task 12 (R-R3-49), rereview of the fix wave N2: an if line's gate is
    // the one its event named, wherever the line sits in the tick. The
    // string-only rule (an if straight after a dds is a centre if) would put
    // both of these on the wrong gate.
    void anIfLineTakesTheGateItsEventNamed()
    {
        using G = TciUpdateGap::Gate;
        TciUpdateGap gap;  // 100 ms
        // Hold the VFO gate: a VFO event at 0, another at 10 waits.
        QCOMPARE(gap.offer({QStringLiteral("if:0,0,-300;"), vfoLine(0, 1)},
                           {G::Vfo, G::Vfo}, 0).size(), 2);
        QVERIFY(gap.offer({QStringLiteral("if:0,0,-200;"), vfoLine(0, 2)},
                          {G::Vfo, G::Vfo}, 10).isEmpty());

        // A centre event whose if is not after its dds: still the centre
        // gate, so it goes now and the waiting VFO if is untouched.
        const QStringList centre = {QStringLiteral("if:0,0,-100;"),
                                    QStringLiteral("dds:0,14000000;")};
        QCOMPARE(gap.offer(centre, {G::Centre, G::Centre}, 20), centre);
        QCOMPARE(gap.takeDue(110),
                 (QStringList{QStringLiteral("if:0,0,-200;"), vfoLine(0, 2)}));

        // A VFO event's if straight after a centre dds for the same
        // receiver: still the VFO gate, so it waits behind the VFO gap
        // while the dds goes. A VFO send at 140 restarts the VFO gap first.
        QCOMPARE(gap.offer({vfoLine(0, 9)}, {G::Vfo}, 140).size(), 1);
        QCOMPARE(gap.offer({QStringLiteral("dds:0,14100000;"),
                            QStringLiteral("if:0,0,-50;"), vfoLine(0, 3)},
                           {G::Centre, G::Vfo, G::Vfo}, 150),
                 QStringList{QStringLiteral("dds:0,14100000;")});
        QCOMPARE(gap.takeDue(250),
                 (QStringList{QStringLiteral("if:0,0,-50;"), vfoLine(0, 3)}));

        // A line with no gate given is sorted by its command.
        QCOMPARE(gap.offer({vfoLine(1, 7)}, {std::nullopt}, 400),
                 QStringList{vfoLine(1, 7)});
    }

#ifdef HAVE_WEBSOCKETS
    // Task 12 (R-R3-49), N2's two orderings: a VFO event and a centre event
    // for the same receiver in one drain keep two separate if lines, each
    // carrying its own event's gate, in either order.
    void vfoAndCentreIfLinesKeepTheirOwnGateInEitherOrder()
    {
        using G = TciUpdateGap::Gate;
        const auto drain = [](TciProtocol& p) {
            p.drainCoalescedNotifications();
            QList<TciProtocol::PendingLine> out;
            while (p.hasPendingNotification()) {
                out << p.takePendingLine();
            }
            return out;
        };
        const auto gateOfIf = [](const QList<TciProtocol::PendingLine>& lines, int nth) {
            int seen = 0;
            for (const auto& l : lines) {
                if (l.frame.startsWith(QLatin1String("if:0,0,")) && seen++ == nth) {
                    return l.gate;
                }
            }
            return std::optional<G>{};
        };

        {   // VFO first.
            TciProtocol p;
            p.enqueueLocalBroadcastVfo(0, 14000000, false);
            p.enqueueLocalBroadcastCentre(0);
            const auto lines = drain(p);
            QCOMPARE(lines.size(), 6);
            QCOMPARE(lines.at(0).frame.left(7), QStringLiteral("if:0,0,"));
            QCOMPARE(lines.at(4).frame.left(6), QStringLiteral("dds:0,"));
            QCOMPARE(gateOfIf(lines, 0), std::optional(G::Vfo));
            QCOMPARE(gateOfIf(lines, 1), std::optional(G::Centre));
            QCOMPARE(lines.at(4).gate, std::optional(G::Centre));
        }
        {   // Centre first.
            TciProtocol p;
            p.enqueueLocalBroadcastCentre(0);
            p.enqueueLocalBroadcastVfo(0, 14000000, false);
            const auto lines = drain(p);
            QCOMPARE(lines.size(), 6);
            QCOMPARE(lines.at(0).frame.left(6), QStringLiteral("dds:0,"));
            QCOMPARE(gateOfIf(lines, 0), std::optional(G::Centre));
            QCOMPARE(gateOfIf(lines, 1), std::optional(G::Vfo));
            QCOMPARE(lines.at(3).gate, std::optional(G::Vfo));  // vfo:0,0
        }
    }

    void serverReadsTheGapAtStartAndChangesItLive()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(QString::fromLatin1(TciUpdateGap::kSettingKey), 300);
        TciServer server(nullptr);
        QVERIFY(server.start(0));
        QCOMPARE(server.updateGapMs(), 300);

        QWebSocket app;
        QSignalSpy connected(&app, &QWebSocket::connected);
        app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(connected.wait(2000));
        QTRY_COMPARE(server.clientCount(), 1);
        QCOMPARE(server.clients().constBegin().value()->updateGap.gapMs(), 300);

        server.setUpdateGapMs(40);
        QCOMPARE(server.updateGapMs(), 40);
        QCOMPARE(server.clients().constBegin().value()->updateGap.gapMs(), 40);
        server.setUpdateGapMs(5000);
        QCOMPARE(server.updateGapMs(), 1000);

        app.close();
        server.stop();
        settings.remove(QString::fromLatin1(TciUpdateGap::kSettingKey));
    }

    // A real app on a real server: with the longest gap, a burst of
    // changes reaches the app as the first and the last frequency only;
    // with no gap, every change reaches it.
    void anAppGetsTheFirstAndLastOfABurst()
    {
        TciServer server(nullptr);
        QVERIFY(server.start(0));
        server.setUpdateGapMs(TciUpdateGap::kMaxGapMs);
        QWebSocket app;
        QSignalSpy connected(&app, &QWebSocket::connected);
        QStringList vfoLines;
        connect(&app, &QWebSocket::textMessageReceived, this, [&vfoLines](const QString& msg) {
            for (const QString& part : msg.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                if (part.trimmed().startsWith(QStringLiteral("vfo:0,0,"))) {
                    vfoLines << part.trimmed() + QLatin1Char(';');
                }
            }
        });
        app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(connected.wait(2000));
        QTRY_COMPARE(server.clientCount(), 1);
        QTest::qWait(50);
        vfoLines.clear();

        TciProtocol* protocol = server.protocolForTest();
        protocol->enqueueLocalBroadcast(vfoLine(0, 14000000));
        QTRY_COMPARE(vfoLines, QStringList{vfoLine(0, 14000000)});
        for (int i = 1; i <= 9; ++i) {
            protocol->enqueueLocalBroadcast(vfoLine(0, 14000000 + i * 100));
            QTest::qWait(10);
        }
        QTRY_COMPARE_WITH_TIMEOUT(vfoLines.size(), 2, 5000);
        QCOMPARE(vfoLines.last(), vfoLine(0, 14000900));
        QTest::qWait(100);
        QCOMPARE(vfoLines.size(), 2);

        server.setUpdateGapMs(0);
        vfoLines.clear();
        QStringList expected;
        for (int i = 0; i < 10; ++i) {
            expected << vfoLine(0, 7000000 + i);
            protocol->enqueueLocalBroadcast(expected.last());
            QTest::qWait(10);
        }
        QTRY_COMPARE(vfoLines, expected);

        app.close();
        server.stop();
    }
#endif
};

QTEST_MAIN(TestTciUpdateGap)
#include "tst_tci_update_gap.moc"
