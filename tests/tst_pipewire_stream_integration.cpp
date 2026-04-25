// =================================================================
// tests/tst_pipewire_stream_integration.cpp  (NereusSDR)
// Author: J.J. Boyd (KG4VCF), AI-assisted via Claude Code. 2026-04-24.
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE
#include <QtTest/QtTest>
#include "core/audio/PipeWireThreadLoop.h"
#include "core/audio/PipeWireStream.h"

using namespace NereusSDR;

class TestPipeWireStreamIntegration : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("XDG_RUNTIME_DIR").isEmpty()) { QSKIP("no session"); }
    }
    void connectsAndStreams() {
        PipeWireThreadLoop loop;
        QVERIFY(loop.connect());

        StreamConfig cfg;
        cfg.nodeName        = QStringLiteral("nereussdr.integration-test");
        cfg.nodeDescription = QStringLiteral("Integration test");
        cfg.direction       = StreamConfig::Output;
        cfg.mediaClass      = QStringLiteral("Stream/Output/Audio");
        cfg.mediaRole       = QStringLiteral("Music");

        PipeWireStream s(&loop, cfg);
        QSignalSpy stateSpy(&s, &PipeWireStream::streamStateChanged);
        QVERIFY(s.open());

        // Wait up to 3 s for STREAMING.
        QTRY_VERIFY_WITH_TIMEOUT(
            stateSpy.count() > 0
                && stateSpy.last().at(0).toString() == QStringLiteral("streaming"),
            3000);

        // Push 500 ms of silence (48 kHz × stereo × F32 = 384000 bytes ≈ 500 ms).
        // Note: payload must be sizeof(float)*channels-aligned (forward contract #2)
        // → 8 B for stereo F32. 96000 is divisible by 8 ✓
        std::vector<char> buf(96000, 0);
        s.push(buf.data(), buf.size());
        QTest::qWait(200);

        s.close();
    }
};
QTEST_MAIN(TestPipeWireStreamIntegration)
#include "tst_pipewire_stream_integration.moc"
#else
int main() { return 0; }   // Compiles cleanly on hosts without libpipewire.
#endif
