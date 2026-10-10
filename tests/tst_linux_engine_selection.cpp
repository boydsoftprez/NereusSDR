// =================================================================
// tests/tst_linux_engine_selection.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.  Which Linux engine runs
// (native audio plan Task 11: R-AUD-01, R-AUD-02, R-AUD-31; settled call
// 23).  Pure: no sound server, runs on every platform.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: final review fix (R-AUD-31): PipeWire's PulseAudio service
//               runs PulseAudio while PipeWire does not answer. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/LinuxEngineSelection.h"

#include <optional>

using namespace NereusSDR;

namespace {

LinuxSoundServerProbe probe(bool pipewire, std::optional<QString> pulseName,
                            const QString& forced = QString())
{
    LinuxSoundServerProbe p;
    p.pipewireAnswers = pipewire;
    p.pulseServerName = std::move(pulseName);
    p.forced = forced;
    return p;
}

void expectChoice(const LinuxSoundServerProbe& p, bool pipewire, bool pulse)
{
    const LinuxEngineChoice choice = chooseLinuxEngines(p);
    QCOMPARE(choice.pipewireRunning, pipewire);
    QCOMPARE(choice.pulseRunning, pulse);
}

} // namespace

class TestLinuxEngineSelection : public QObject {
    Q_OBJECT

private slots:
    // PipeWire answering: PipeWire runs and PulseAudio does not, even with
    // pipewire-pulse answering as well.
    void pipewireAnswers()
    {
        expectChoice(probe(true, std::nullopt), true, false);
        expectChoice(probe(true, QStringLiteral("PulseAudio (on PipeWire 1.0.5)")), true, false);
        expectChoice(probe(true, QStringLiteral("pulseaudio")), true, false);
    }

    // A real PulseAudio server, PipeWire not answering: PulseAudio runs.
    void pulseAudioServer()
    {
        expectChoice(probe(false, QStringLiteral("pulseaudio")), false, true);
    }

    // PipeWire's own PulseAudio service counts as PipeWire only while
    // PipeWire itself answers (pipewireAnswers above).  When it does not
    // (a build without libpipewire, or a native connection that fails
    // while pipewire-pulse answers) the PulseAudio engine runs, so the
    // server that answers is never shown as not running (R-AUD-31).
    void pulseOnPipeWireRunsPulseAudioWhilePipeWireDoesNotAnswer()
    {
        expectChoice(probe(false, QStringLiteral("PulseAudio (on PipeWire 1.0.5)")), false, true);
        expectChoice(probe(false, QStringLiteral("pulseaudio (on pipewire 0.3.65)")), false, true);
    }

    // Neither answering: both are not running, so the older drivers remain
    // and the migration waits (settled call 23).
    void neitherAnswers()
    {
        expectChoice(probe(false, std::nullopt), false, false);
    }

    // Audio/LinuxBackendPreferred forces the answer as detectLinuxBackend's
    // does, whatever answers.
    void forcedValuesDecide()
    {
        for (bool pipewire : {false, true}) {
            for (const std::optional<QString>& name :
                 {std::optional<QString>{}, std::optional<QString>(QStringLiteral("pulseaudio")),
                  std::optional<QString>(QStringLiteral("PulseAudio (on PipeWire 1.0.5)"))}) {
                expectChoice(probe(pipewire, name, QStringLiteral("pipewire")), true, false);
                expectChoice(probe(pipewire, name, QStringLiteral("pactl")), false, true);
                expectChoice(probe(pipewire, name, QStringLiteral("pulse")), false, true);
                expectChoice(probe(pipewire, name, QStringLiteral("none")), false, false);
            }
        }
        QVERIFY(linuxEngineForced(QStringLiteral("pipewire")));
        QVERIFY(linuxEngineForced(QStringLiteral("pactl")));
        QVERIFY(linuxEngineForced(QStringLiteral("pulse")));
        QVERIFY(linuxEngineForced(QStringLiteral("none")));
        QVERIFY(!linuxEngineForced(QString()));
        QVERIFY(!linuxEngineForced(QStringLiteral("garbage")));
    }

    // Any other value falls through to what answers.
    void unknownForcedFallsThrough()
    {
        expectChoice(probe(true, std::nullopt, QStringLiteral("garbage")), true, false);
        expectChoice(probe(false, QStringLiteral("pulseaudio"), QStringLiteral("PipeWire")), false, true);
        expectChoice(probe(false, std::nullopt, QStringLiteral("garbage")), false, false);
    }
};

QTEST_MAIN(TestLinuxEngineSelection)
#include "tst_linux_engine_selection.moc"
