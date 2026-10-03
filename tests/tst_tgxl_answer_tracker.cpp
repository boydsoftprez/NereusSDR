// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_tgxl_answer_tracker.cpp  (NereusSDR)
// =================================================================
//
// TgxlAnswerTracker, one table: each row is a sequence of what the Core
// sent the Tuner Genius and what the tuner sent back, with the verdict on
// each of the tuner's `transmit tune on` lines (an answer, or its own
// front-panel press). Timings follow captures/flex-tgxl-direct-CONTROL.pcapng
// (autotune at T+172.199, tuning=1 at +2 ms, tune on at +503 ms).
//
// Script tokens, each `<event>@<ms>`:
//   A<n>   the Core sent `autotune` with sequence n outside a cycle
//   C<n>   the Core sent `autotune` with sequence n for a running cycle
//          (A<e>.<n> / C<e>.<n>: on link epoch e; plain n is epoch 1)
//   X<n>   the tuner refused sequence n (X<e>.<n> for epoch e)
//   B1/B0  the Core sent a frame with tune=1 / a tune=0 change
//   T1/T0  the tuner's tuning went up / down
//   ON=<v> the tuner's tune on; v is what it must answer: p (nothing, its
//          own press), a (an autotune), c (a cycle's autotune), e (a
//          tune=1 echo)
//   OFF    the tuner's tune off
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), TGXL tune lane round 2, with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-10-01: round 3: answers carry their kind; frames counted per
//               frame; epochs; one rise per autotune. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/TgxlAnswerTracker.h"

#include <QStringList>
#include <QTest>

using namespace NereusSDR;

class TgxlAnswerTrackerTest : public QObject {
    Q_OBJECT

private slots:
    void sequences_data()
    {
        QTest::addColumn<QString>("script");
        QTest::addColumn<int>("leftWaiting");

        QTest::newRow("pcap: autotune answered, then a press")
            << "A1@0 T1@2 ON@503=a ON@1000=p" << 0;
        QTest::newRow("nothing sent: a press") << "ON@0=p" << 0;
        QTest::newRow("I-A: a plain tune's echo arrives after it ended")
            << "B1@0 B0@50 ON@200=e OFF@210 ON@1000=p" << 0;
        QTest::newRow("a device's tune: answer and echo both counted")
            << "C1@0 B1@30 ON@503=c ON@520=e ON@1000=p" << 0;
        QTest::newRow("a device's tune, one line only: closed until the window")
            << "C1@0 B1@30 ON@503=c ON@1000=e ON@3100=p" << 0;
        QTest::newRow("m-3: echo per frame, all late")
            << "B1@0 B1@1000 B1@2000 B1@2400 B0@2500 ON@2600=e ON@2610=e ON@2620=e ON@2630=e"
               " OFF@2640 ON@2700=p"
            << 0;
        QTest::newRow("m-3: echo per change, extra frames close until the window")
            << "B1@0 ON@100=e B1@1000 B1@2000 B0@2500 OFF@2600 ON@2700=e ON@4100=e ON@5100=p"
            << 0;
        QTest::newRow("m-3: the unkey resend is a frame like any other")
            << "B1@0 B1@900 B0@901 ON@950=e ON@960=e OFF@970 ON@1000=p" << 0;
        QTest::newRow("m-A1: the second autotune refused, the first still counted")
            << "A1@0 A2@10 X2@0 ON@503=a ON@600=p" << 0;
        QTest::newRow("the only autotune refused: nothing to answer")
            << "A1@0 X1@0 ON@100=p" << 0;
        QTest::newRow("m-4: a refusal from another link epoch removes nothing")
            << "A1.5@0 X2.5@0 ON@503=a" << 0;
        QTest::newRow("m-4: the refusal on its own epoch removes it")
            << "A1.5@0 A2.5@10 X2.5@0 ON@503=a ON@600=p" << 0;
        QTest::newRow("m-4: one rise and fall end only the oldest autotune")
            << "A1@0 A2@10 T1@12 T0@500 ON@600=a ON@700=p" << 0;
        QTest::newRow("m-A2: a fall from an earlier sweep does not clear it")
            << "T1@0 A1@10 T0@20 ON@500=a" << 0;
        QTest::newRow("its own sweep ends without a tune on: cleared")
            << "A1@0 T1@2 T0@1500 ON@1600=p" << 0;
        QTest::newRow("m-A2: an echoed tune off does not clear it")
            << "A1@0 B0@5 OFF@10 ON@503=a" << 0;
        QTest::newRow("a tune off before its sweep starts does not clear it")
            << "A1@0 OFF@3 ON@503=a" << 0;
        QTest::newRow("a tune off after its sweep started clears it")
            << "A1@0 T1@2 OFF@100 ON@200=p" << 0;
        QTest::newRow("the window's last millisecond still counts")
            << "A1@0 ON@3000=a" << 0;
        QTest::newRow("past the window: a press") << "A1@0 ON@3001=p" << 0;
        QTest::newRow("an echoed tune off is not an answer to a tune on")
            << "B0@0 OFF@10 ON@20=p" << 0;
        QTest::newRow("still waiting") << "C1@0 B1@30 ON@503=c" << 1;
    }

    void sequences()
    {
        QFETCH(QString, script);
        QFETCH(int, leftWaiting);
        TgxlAnswerTracker tracker;
        qint64 now = 0;
        const auto epochSeq = [](const QString& rest, quint64& epoch, quint32& seq) {
            const int dot = rest.indexOf(QLatin1Char('.'));
            epoch = dot < 0 ? 1 : rest.left(dot).toULongLong();
            seq = (dot < 0 ? rest : rest.mid(dot + 1)).toUInt();
        };
        for (const QString& token : script.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            const int at = token.indexOf(QLatin1Char('@'));
            QVERIFY2(at > 0, qPrintable(token));
            const QString event = token.left(at);
            QString time = token.mid(at + 1);
            QString verdict;
            const int eq = time.indexOf(QLatin1Char('='));
            if (eq >= 0) {
                verdict = time.mid(eq + 1);
                time = time.left(eq);
            }
            if (!event.startsWith(QLatin1Char('X'))) {
                now = time.toLongLong();
            }
            quint64 epoch = 0;
            quint32 seq = 0;
            if (event == QLatin1String("ON")) {
                const TgxlAnswerTracker::Kind kind = tracker.tuneOnAnswer(now).kind;
                TgxlAnswerTracker::Kind expected = TgxlAnswerTracker::Kind::None;
                if (verdict == QLatin1String("a")) {
                    expected = TgxlAnswerTracker::Kind::Autotune;
                } else if (verdict == QLatin1String("c")) {
                    expected = TgxlAnswerTracker::Kind::CycleAutotune;
                } else if (verdict == QLatin1String("e")) {
                    expected = TgxlAnswerTracker::Kind::TuneOnEcho;
                } else {
                    QCOMPARE(verdict, QStringLiteral("p"));
                }
                QVERIFY2(kind == expected,
                         qPrintable(token + QStringLiteral(" answered ")
                                    + QString::fromLatin1(TgxlAnswerTracker::kindName(kind))));
            } else if (event == QLatin1String("OFF")) {
                tracker.tuneOff(now);
            } else if (event == QLatin1String("B1") || event == QLatin1String("B0")) {
                tracker.tuneSent(event == QLatin1String("B1"), now);
            } else if (event == QLatin1String("T1") || event == QLatin1String("T0")) {
                tracker.tuningChanged(event == QLatin1String("T1"), now);
            } else if (event.startsWith(QLatin1Char('A')) || event.startsWith(QLatin1Char('C'))) {
                epochSeq(event.mid(1), epoch, seq);
                tracker.autotuneSent(epoch, seq, event.startsWith(QLatin1Char('C')), now);
            } else if (event.startsWith(QLatin1Char('X'))) {
                epochSeq(event.mid(1), epoch, seq);
                tracker.autotuneRejected(epoch, seq);
            } else {
                QFAIL(qPrintable(QStringLiteral("unknown token ") + token));
            }
        }
        QCOMPARE(tracker.awaitingTuneOn(now), leftWaiting);
    }
};

QTEST_GUILESS_MAIN(TgxlAnswerTrackerTest)
#include "tst_tgxl_answer_tracker.moc"
