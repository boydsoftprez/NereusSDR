// =================================================================
// tests/tst_mic_mode_keeper.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test of the rule that keeps the
// window's stored macOS Mic Mode at Standard (MicModeKeeper).  The
// system's calls are stood in for; nothing here reads or changes a real
// setting.  The window's runner is tested against this binary run as the
// helper (main below), which answers, never answers, or ends without an
// answer as the test asks.
//
// Modification history (NereusSDR):
//   2026-10-16: new. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-10-16: the mode before, the window's log line and its runner,
//               against this binary run as the helper. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/audio/MicModeKeeper.h"

#include <QElapsedTimer>

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr char kBundle[] = "com.example.window";

// What this binary does when run as the helper (main below).
constexpr char kFakeHelperEnv[] = "NEREUS_TEST_FAKE_MIC_HELPER";
// A helper that never answers waits this long, far past any wait here.
constexpr int kFakeHangMs = 60'000;
// An exit code the helper never answers with.
constexpr int kNoAnswerExitCode = 9;

// A stored mode the calls read and write, with every call recorded.
struct FakeStore {
    long mode = kMicModeStandard;
    bool takesWrites = true;
    std::vector<QString> reads;
    std::vector<std::pair<long, QString>> writes;

    MicModeCalls calls()
    {
        MicModeCalls c;
        c.read = [this](const QString& bundleId) {
            reads.push_back(bundleId);
            return mode;
        };
        c.write = [this](long next, const QString& bundleId) {
            writes.emplace_back(next, bundleId);
            if (takesWrites) {
                mode = next;
            }
            return takesWrites;
        };
        return c;
    }
};

} // namespace

class TstMicModeKeeper : public QObject {
    Q_OBJECT

private slots:
    void voiceIsolationBecomesStandard()
    {
        FakeStore store;
        store.mode = kMicModeVoiceIsolation;
        const MicModeResult result = keepMicModeStandard(QString::fromLatin1(kBundle),
                                                         store.calls());
        QCOMPARE(result.outcome, MicModeOutcome::MadeStandard);
        QCOMPARE(result.before, kMicModeVoiceIsolation);
        QCOMPARE(store.writes.size(), std::size_t(1));
        QCOMPARE(store.writes.front().first, kMicModeStandard);
        QCOMPARE(store.writes.front().second, QString::fromLatin1(kBundle));
        QCOMPARE(store.mode, kMicModeStandard);
        // Read before, and read back after, for our bundle only.
        QCOMPARE(store.reads.size(), std::size_t(2));
        for (const QString& read : store.reads) {
            QCOMPARE(read, QString::fromLatin1(kBundle));
        }
    }

    void wideSpectrumBecomesStandard()
    {
        FakeStore store;
        store.mode = kMicModeWideSpectrum;
        const MicModeResult result = keepMicModeStandard(QString::fromLatin1(kBundle),
                                                         store.calls());
        QCOMPARE(result.outcome, MicModeOutcome::MadeStandard);
        QCOMPARE(result.before, kMicModeWideSpectrum);
        QCOMPARE(store.mode, kMicModeStandard);
    }

    void standardIsLeftAlone()
    {
        FakeStore store;
        const MicModeResult result = keepMicModeStandard(QString::fromLatin1(kBundle),
                                                         store.calls());
        QCOMPARE(result.outcome, MicModeOutcome::AlreadyStandard);
        QCOMPARE(result.before, kMicModeStandard);
        QVERIFY(store.writes.empty());
        QCOMPARE(store.reads.size(), std::size_t(1));
    }

    void aRefusedWriteIsReported()
    {
        FakeStore store;
        store.mode = kMicModeVoiceIsolation;
        store.takesWrites = false;
        const MicModeResult result = keepMicModeStandard(QString::fromLatin1(kBundle),
                                                         store.calls());
        QCOMPARE(result.outcome, MicModeOutcome::Refused);
        QCOMPARE(result.before, kMicModeVoiceIsolation);
        QCOMPARE(store.mode, kMicModeVoiceIsolation);
    }

    // The system said yes but still reads the old mode: not Standard.
    void aWriteThatDoesNotStickIsRefused()
    {
        FakeStore store;
        store.mode = kMicModeVoiceIsolation;
        MicModeCalls calls = store.calls();
        calls.write = [&store](long next, const QString& bundleId) {
            store.writes.emplace_back(next, bundleId);
            return true;
        };
        QCOMPARE(keepMicModeStandard(QString::fromLatin1(kBundle), calls).outcome,
                 MicModeOutcome::Refused);
    }

    // A read that fails (the system threw) leaves the mode alone.
    void aFailedReadChangesNothing()
    {
        FakeStore store;
        MicModeCalls calls = store.calls();
        calls.read = [&store](const QString& bundleId) {
            store.reads.push_back(bundleId);
            return kMicModeNotRead;
        };
        const MicModeResult result = keepMicModeStandard(QString::fromLatin1(kBundle), calls);
        QCOMPARE(result.outcome, MicModeOutcome::Refused);
        QCOMPARE(result.before, kMicModeNotRead);
        QVERIFY(store.writes.empty());
    }

    void missingCallsAreUnavailable()
    {
        const MicModeResult none = keepMicModeStandard(QString::fromLatin1(kBundle),
                                                       MicModeCalls{});
        QCOMPARE(none.outcome, MicModeOutcome::Unavailable);
        QCOMPARE(none.before, kMicModeNotRead);
        FakeStore store;
        MicModeCalls readOnly = store.calls();
        readOnly.write = nullptr;
        QCOMPARE(keepMicModeStandard(QString::fromLatin1(kBundle), readOnly).outcome,
                 MicModeOutcome::Unavailable);
        QVERIFY(store.reads.empty());
    }

    void noBundleTouchesNothing()
    {
        FakeStore store;
        store.mode = kMicModeVoiceIsolation;
        QCOMPARE(keepMicModeStandard(QString(), store.calls()).outcome, MicModeOutcome::NoBundle);
        QVERIFY(store.reads.empty());
        QVERIFY(store.writes.empty());
    }

    void exitCodesRoundTrip()
    {
        const MicModeOutcome all[] = {MicModeOutcome::AlreadyStandard, MicModeOutcome::MadeStandard,
                                      MicModeOutcome::Refused, MicModeOutcome::Unavailable,
                                      MicModeOutcome::NoBundle};
        for (MicModeOutcome outcome : all) {
            MicModeOutcome back = MicModeOutcome::Refused;
            QVERIFY(micModeOutcomeFromExitCode(micModeExitCode(outcome), &back));
            QCOMPARE(back, outcome);
            QVERIFY(!micModeOutcomeText(outcome).isEmpty());
        }
        MicModeOutcome unused = MicModeOutcome::Refused;
        // A crash or a kill is not an outcome.
        QVERIFY(!micModeOutcomeFromExitCode(9, &unused));
        QVERIFY(!micModeOutcomeFromExitCode(-1, &unused));
    }

    void theModeBeforeRoundTrips()
    {
        for (long mode : {kMicModeStandard, kMicModeWideSpectrum, kMicModeVoiceIsolation,
                          kMicModeNotRead, 7L}) {
            QCOMPARE(micModeBeforeFromOutput(micModeBeforeLine(mode)), mode);
        }
        // Other output around the line is ignored; no line is no mode.
        QCOMPARE(micModeBeforeFromOutput(QByteArray("noise\n") + micModeBeforeLine(2)
                                         + "more noise\n"),
                 kMicModeVoiceIsolation);
        QCOMPARE(micModeBeforeFromOutput(QByteArray("noise\n")), kMicModeNotRead);
        QCOMPARE(micModeName(kMicModeVoiceIsolation), QStringLiteral("Voice Isolation"));
        QCOMPARE(micModeName(kMicModeWideSpectrum), QStringLiteral("Wide Spectrum"));
        QCOMPARE(micModeName(kMicModeStandard), QStringLiteral("Standard"));
        QCOMPARE(micModeName(7), QStringLiteral("mode 7"));
    }

    // The window's one line: the old mode when it changed; a line when
    // the system has no call, the change failed, or the helper did not
    // answer; nothing when it was Standard already.
    void theWindowLogsOneLineNamingTheOldMode_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<int>("outcome");
        QTest::addColumn<int>("before");
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("warning");
        const int answered = int(MicModeHelperAnswer::Kind::Answered);
        QTest::newRow("voice isolation set")
            << answered << int(MicModeOutcome::MadeStandard) << int(kMicModeVoiceIsolation)
            << QStringLiteral("Mic Mode: set to Standard for this app; it was Voice Isolation (5 ms)")
            << false;
        QTest::newRow("wide spectrum set")
            << answered << int(MicModeOutcome::MadeStandard) << int(kMicModeWideSpectrum)
            << QStringLiteral("Mic Mode: set to Standard for this app; it was Wide Spectrum (5 ms)")
            << false;
        QTest::newRow("standard already")
            << answered << int(MicModeOutcome::AlreadyStandard) << int(kMicModeStandard)
            << QString() << false;
        QTest::newRow("call missing")
            << answered << int(MicModeOutcome::Unavailable) << int(kMicModeNotRead)
            << QStringLiteral("Mic Mode: this macOS has no Mic Mode call; Mic Mode left as it "
                              "is (5 ms)")
            << false;
        QTest::newRow("call failed")
            << answered << int(MicModeOutcome::Refused) << int(kMicModeVoiceIsolation)
            << QStringLiteral("Mic Mode: the system did not take Standard for this app; it was "
                              "Voice Isolation (5 ms)")
            << true;
        QTest::newRow("read failed")
            << answered << int(MicModeOutcome::Refused) << int(kMicModeNotRead)
            << QStringLiteral("Mic Mode: the system could not read this app's Mic Mode; Mic "
                              "Mode left as it is (5 ms)")
            << true;
        QTest::newRow("no answer")
            << int(MicModeHelperAnswer::Kind::NoAnswer) << 0 << 0
            << QStringLiteral("Mic Mode: no answer from the mic helper within 800 ms; Mic Mode "
                              "left as it is")
            << true;
        QTest::newRow("helper ended")
            << int(MicModeHelperAnswer::Kind::Ended) << 0 << 0
            << QStringLiteral("Mic Mode: the mic helper ended without an answer; Mic Mode left "
                              "as it is (5 ms)")
            << true;
    }
    void theWindowLogsOneLineNamingTheOldMode()
    {
        QFETCH(int, kind);
        QFETCH(int, outcome);
        QFETCH(int, before);
        QFETCH(QString, text);
        QFETCH(bool, warning);
        MicModeHelperAnswer answer;
        answer.kind = MicModeHelperAnswer::Kind(kind);
        answer.result.outcome = MicModeOutcome(outcome);
        answer.result.before = before;
        answer.elapsedMs = 5;
        const MicModeLogLine line = micModeLogLine(answer, 800);
        QCOMPARE(line.text, text);
        QCOMPARE(line.warning, warning);
    }

    // The helper's side, through stand-in calls: the mode before on
    // standard output, the outcome as the exit code.
    void theHelperAnswersWithTheModeBefore()
    {
        FakeStore store;
        store.mode = kMicModeVoiceIsolation;
        const int code = answerMicModeRequest(QString::fromLatin1(kBundle), store.calls());
        MicModeOutcome outcome = MicModeOutcome::Refused;
        QVERIFY(micModeOutcomeFromExitCode(code, &outcome));
        QCOMPARE(outcome, MicModeOutcome::MadeStandard);
        QCOMPARE(store.mode, kMicModeStandard);
    }

    // The window's runner, against this binary run as the helper.
    void theWindowReadsTheHelpersAnswer_data()
    {
        QTest::addColumn<QByteArray>("script");
        QTest::addColumn<int>("outcome");
        QTest::addColumn<int>("before");
        QTest::newRow("voice isolation") << QByteArray("voice-isolation")
                                         << int(MicModeOutcome::MadeStandard)
                                         << int(kMicModeVoiceIsolation);
        QTest::newRow("wide spectrum") << QByteArray("wide-spectrum")
                                       << int(MicModeOutcome::MadeStandard)
                                       << int(kMicModeWideSpectrum);
        QTest::newRow("standard") << QByteArray("standard")
                                  << int(MicModeOutcome::AlreadyStandard)
                                  << int(kMicModeStandard);
        QTest::newRow("call missing") << QByteArray("missing")
                                      << int(MicModeOutcome::Unavailable)
                                      << int(kMicModeNotRead);
        QTest::newRow("call fails") << QByteArray("refuses") << int(MicModeOutcome::Refused)
                                    << int(kMicModeVoiceIsolation);
    }
    void theWindowReadsTheHelpersAnswer()
    {
        QFETCH(QByteArray, script);
        QFETCH(int, outcome);
        QFETCH(int, before);
        qputenv(kFakeHelperEnv, script);
        // A wait no start of this binary comes near, so the answer is read.
        const MicModeHelperAnswer answer =
            runMicModeHelper(QCoreApplication::applicationFilePath(),
                             QString::fromLatin1(kBundle), 30'000);
        qunsetenv(kFakeHelperEnv);
        QCOMPARE(answer.kind, MicModeHelperAnswer::Kind::Answered);
        QCOMPARE(answer.result.outcome, MicModeOutcome(outcome));
        QCOMPARE(answer.result.before, long(before));
    }

    // A helper that never answers is stopped at the wait, and the window
    // goes on: the runner returns long before the helper would.
    void aHelperThatNeverAnswersIsStoppedAtTheWait()
    {
        qputenv(kFakeHelperEnv, "hang");
        QElapsedTimer clock;
        clock.start();
        const MicModeHelperAnswer answer =
            runMicModeHelper(QCoreApplication::applicationFilePath(),
                             QString::fromLatin1(kBundle), 300);
        const qint64 took = clock.elapsed();
        qunsetenv(kFakeHelperEnv);
        QCOMPARE(answer.kind, MicModeHelperAnswer::Kind::NoAnswer);
        QVERIFY2(took >= 300, qPrintable(QString::number(took)));
        QVERIFY2(took < kFakeHangMs / 2, qPrintable(QString::number(took)));
        const MicModeLogLine line = micModeLogLine(answer, 300);
        QVERIFY(line.warning);
        QVERIFY(line.text.contains(QStringLiteral("within 300 ms")));
    }

    // An exit code that is no answer (as a crash or a kill gives) is none.
    void aHelperThatEndsWithoutAnAnswerHasNone()
    {
        qputenv(kFakeHelperEnv, "no-answer");
        const MicModeHelperAnswer answer =
            runMicModeHelper(QCoreApplication::applicationFilePath(),
                             QString::fromLatin1(kBundle), 30'000);
        qunsetenv(kFakeHelperEnv);
        QCOMPARE(answer.kind, MicModeHelperAnswer::Kind::Ended);
        QVERIFY(micModeLogLine(answer, 30'000).warning);
    }

    void aHelperThatIsMissingHasNoAnswer()
    {
        const MicModeHelperAnswer answer =
            runMicModeHelper(QStringLiteral("/nonexistent/nereus-audio-capture"),
                             QString::fromLatin1(kBundle), 30'000);
        QCOMPARE(answer.kind, MicModeHelperAnswer::Kind::Ended);
    }
};

namespace {

// This binary run as the helper: answers through stand-in calls as the
// environment says, never answers, or ends with an exit code that is no
// answer.
int runAsFakeHelper(const char* bundleId)
{
    const QByteArray script = qgetenv(kFakeHelperEnv);
    if (script == "hang") {
        std::this_thread::sleep_for(std::chrono::milliseconds(kFakeHangMs));
        return 0;
    }
    if (script == "no-answer") {
        return kNoAnswerExitCode;
    }
    FakeStore store;
    MicModeCalls calls = store.calls();
    if (script == "voice-isolation") {
        store.mode = kMicModeVoiceIsolation;
    } else if (script == "wide-spectrum") {
        store.mode = kMicModeWideSpectrum;
    } else if (script == "refuses") {
        store.mode = kMicModeVoiceIsolation;
        store.takesWrites = false;
    } else if (script == "missing") {
        calls = MicModeCalls{};
    }
    return answerMicModeRequest(QString::fromLocal8Bit(bundleId), calls);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 3 && std::strcmp(argv[1], kKeepMicModeStandardArg) == 0
        && !qgetenv(kFakeHelperEnv).isEmpty()) {
        return runAsFakeHelper(argv[2]);
    }
    QCoreApplication app(argc, argv);
    TstMicModeKeeper test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_mic_mode_keeper.moc"
