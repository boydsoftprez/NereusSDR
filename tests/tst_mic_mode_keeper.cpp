// =================================================================
// tests/tst_mic_mode_keeper.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test of the rule that keeps the
// window's stored macOS Mic Mode at Standard (MicModeKeeper).  The
// system's calls are stood in for; nothing here reads or changes a real
// setting.
//
// Modification history (NereusSDR):
//   2026-10-16: new. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#include <QtTest>

#include "core/audio/MicModeKeeper.h"

#include <vector>

using namespace NereusSDR;

namespace {

constexpr char kBundle[] = "com.example.window";

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
        QCOMPARE(keepMicModeStandard(QString::fromLatin1(kBundle), store.calls()),
                 MicModeOutcome::MadeStandard);
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
        QCOMPARE(keepMicModeStandard(QString::fromLatin1(kBundle), store.calls()),
                 MicModeOutcome::MadeStandard);
        QCOMPARE(store.mode, kMicModeStandard);
    }

    void standardIsLeftAlone()
    {
        FakeStore store;
        QCOMPARE(keepMicModeStandard(QString::fromLatin1(kBundle), store.calls()),
                 MicModeOutcome::AlreadyStandard);
        QVERIFY(store.writes.empty());
        QCOMPARE(store.reads.size(), std::size_t(1));
    }

    void aRefusedWriteIsReported()
    {
        FakeStore store;
        store.mode = kMicModeVoiceIsolation;
        store.takesWrites = false;
        QCOMPARE(keepMicModeStandard(QString::fromLatin1(kBundle), store.calls()),
                 MicModeOutcome::Refused);
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
        QCOMPARE(keepMicModeStandard(QString::fromLatin1(kBundle), calls), MicModeOutcome::Refused);
    }

    void missingCallsAreUnavailable()
    {
        QCOMPARE(keepMicModeStandard(QString::fromLatin1(kBundle), MicModeCalls{}),
                 MicModeOutcome::Unavailable);
        FakeStore store;
        MicModeCalls readOnly = store.calls();
        readOnly.write = nullptr;
        QCOMPARE(keepMicModeStandard(QString::fromLatin1(kBundle), readOnly),
                 MicModeOutcome::Unavailable);
        QVERIFY(store.reads.empty());
    }

    void noBundleTouchesNothing()
    {
        FakeStore store;
        store.mode = kMicModeVoiceIsolation;
        QCOMPARE(keepMicModeStandard(QString(), store.calls()), MicModeOutcome::NoBundle);
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
};

QTEST_GUILESS_MAIN(TstMicModeKeeper)
#include "tst_mic_mode_keeper.moc"
