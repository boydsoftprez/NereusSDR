// =================================================================
// tests/tst_pipewire_output_frames.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. R3 receiver audio fix wave
// (R-R3-44): a PipeWire output stream fills and counts the frames the graph
// asked for (pw_buffer::requested), not the whole buffer. Pure; runs on
// every platform.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio fix wave.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/PipeWireOutputFrames.h"

using namespace NereusSDR;

class TstPipeWireOutputFrames : public QObject {
    Q_OBJECT

private slots:
    void fillsWhatTheGraphAskedFor_data()
    {
        QTest::addColumn<quint64>("requested");
        QTest::addColumn<quint32>("maxsize");
        QTest::addColumn<quint32>("stride");
        QTest::addColumn<quint32>("frames");
        // 8192-byte buffer of float stereo (1024 frames), 256 asked for.
        QTest::newRow("a quantum of 256 in a 1024-frame buffer") << quint64(256) << 8192u << 8u << 256u;
        QTest::newRow("a quantum of 1024, the whole buffer") << quint64(1024) << 8192u << 8u << 1024u;
        QTest::newRow("mono, 480 of 2048") << quint64(480) << 8192u << 4u << 480u;
        // No number from the graph: the whole buffer, as before.
        QTest::newRow("nothing asked for") << quint64(0) << 8192u << 8u << 1024u;
        // More than fits: what fits.
        QTest::newRow("more than the buffer holds") << quint64(5000) << 8192u << 8u << 1024u;
        QTest::newRow("no stride") << quint64(256) << 8192u << 0u << 0u;
        // A partial trailing frame is never filled.
        QTest::newRow("a buffer of 1023.5 frames") << quint64(0) << 8188u << 8u << 1023u;
    }

    void fillsWhatTheGraphAskedFor()
    {
        QFETCH(quint64, requested);
        QFETCH(quint32, maxsize);
        QFETCH(quint32, stride);
        QFETCH(quint32, frames);
        QCOMPARE(pipeWireOutputFrames(requested, maxsize, stride), frames);
    }

    // The VAX feeder paces by the consumed count: over a second at a
    // 480-frame quantum the stream must count 48000 frames, not 102400
    // because its buffer holds 1024.
    void aSecondOfCyclesCountsASecond()
    {
        quint64 consumed = 0;
        const int cycles = 48000 / 480;
        for (int i = 0; i < cycles; ++i) {
            consumed += pipeWireOutputFrames(480, 8192, 8);
        }
        QCOMPARE(consumed, quint64(48000));
    }
};

QTEST_MAIN(TstPipeWireOutputFrames)
#include "tst_pipewire_output_frames.moc"
