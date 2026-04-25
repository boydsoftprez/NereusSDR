// =================================================================
// tests/tst_linux_backend_detection.cpp  (NereusSDR)
// =================================================================
// Author: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-04-23
// =================================================================
#include <QtTest/QtTest>
#include "core/audio/LinuxAudioBackend.h"

using namespace NereusSDR;

// Helper — QCOMPARE on scoped enum LinuxAudioBackend can't use QTest's
// Internal::toString path because ADL picks up NereusSDR::toString(...)
// (QString return type) and collides with QTest's char* contract. Comparing
// the underlying int is equivalent for equality and gives a readable failure.
static int asInt(LinuxAudioBackend b) { return static_cast<int>(b); }

class TestLinuxBackendDetection : public QObject {
    Q_OBJECT

private:
    LinuxAudioBackendProbes makeProbes(bool pw, bool pactl,
                                       QString forced = QString())
    {
        LinuxAudioBackendProbes p;
        p.pipewireSocketReachable = [pw](int) { return pw; };
        p.pactlBinaryRunnable     = [pactl](int) { return pactl; };
        p.forcedBackendOverride   = [forced]() { return forced; };
        return p;
    }

private slots:
    void bothAvailable_prefersPipewire() {
        QCOMPARE(asInt(detectLinuxBackend(makeProbes(true, true))),
                 asInt(LinuxAudioBackend::PipeWire));
    }

    void pipewireOnly_returnsPipewire() {
        QCOMPARE(asInt(detectLinuxBackend(makeProbes(true, false))),
                 asInt(LinuxAudioBackend::PipeWire));
    }

    void pactlOnly_returnsPactl() {
        QCOMPARE(asInt(detectLinuxBackend(makeProbes(false, true))),
                 asInt(LinuxAudioBackend::Pactl));
    }

    void neither_returnsNone() {
        QCOMPARE(asInt(detectLinuxBackend(makeProbes(false, false))),
                 asInt(LinuxAudioBackend::None));
    }

    void forcedPipewire_honoured() {
        QCOMPARE(asInt(detectLinuxBackend(makeProbes(false, true, "pipewire"))),
                 asInt(LinuxAudioBackend::PipeWire));
    }

    void forcedPactl_honoured() {
        QCOMPARE(asInt(detectLinuxBackend(makeProbes(true, true, "pactl"))),
                 asInt(LinuxAudioBackend::Pactl));
    }

    void forcedNone_honoured() {
        QCOMPARE(asInt(detectLinuxBackend(makeProbes(true, true, "none"))),
                 asInt(LinuxAudioBackend::None));
    }

    void forcedUnknown_fallsThrough() {
        QCOMPARE(asInt(detectLinuxBackend(makeProbes(true, false, "garbage"))),
                 asInt(LinuxAudioBackend::PipeWire));
    }

    void toStringMapping() {
        QCOMPARE(toString(LinuxAudioBackend::PipeWire), QStringLiteral("PipeWire"));
        QCOMPARE(toString(LinuxAudioBackend::Pactl),    QStringLiteral("Pactl"));
        QCOMPARE(toString(LinuxAudioBackend::None),     QStringLiteral("None"));
    }
};

QTEST_MAIN(TestLinuxBackendDetection)
#include "tst_linux_backend_detection.moc"
