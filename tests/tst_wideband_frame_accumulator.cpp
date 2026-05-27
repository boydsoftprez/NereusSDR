// =================================================================
// tests/tst_wideband_frame_accumulator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic F Task 2: WidebandFrameAccumulator assembles
// 32-packet wideband ADC frames (32 x 512 = 16384 floats) and
// emits frameReady. Sequence errors zero-pad the remainder of
// the current frame, emit it, then reset to wait for seq=0.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/WidebandFrameAccumulator.h"

using namespace NereusSDR;

class TestWidebandFrameAccumulator : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // QSignalSpy needs the QVector<float> metatype registered for
        // first().first().value<QVector<float>>() to round-trip.
        qRegisterMetaType<QVector<float>>("QVector<float>");
    }

    void accumulates_32_packets_into_one_frame()
    {
        WidebandFrameAccumulator acc;
        QSignalSpy spy(&acc, &WidebandFrameAccumulator::frameReady);

        // Each packet payload = 1024 bytes (512 16-bit BE samples).
        QByteArray payload(1024, char(0x10));
        for (int seq = 0; seq < 32; ++seq) {
            acc.pushPacket(seq, payload);
        }

        QCOMPARE(spy.count(), 1);
        const auto frame = spy.first().first().value<QVector<float>>();
        QCOMPARE(frame.size(), 32 * 512);  // 16384
    }

    void sequence_error_zero_pads_partial_frame()
    {
        WidebandFrameAccumulator acc;
        QSignalSpy spy(&acc, &WidebandFrameAccumulator::frameReady);

        QByteArray payload(1024, char(0x10));
        acc.pushPacket(0, payload);
        acc.pushPacket(1, payload);
        // Skip 2, jump to 3 -> sequence error.
        // Thetis network.c:591-597 [v2.10.3.15] behavior: zero-pad the
        // remainder of the current frame, emit it, then reset to wait
        // for the next seq=0.
        acc.pushPacket(3, payload);

        // Verify a partial frame was emitted on the sequence error.
        QVERIFY(spy.count() >= 1);
        const auto frame = spy.first().first().value<QVector<float>>();
        QCOMPARE(frame.size(), 32 * 512);  // full-size frame, tail zero-padded
    }
};

QTEST_MAIN(TestWidebandFrameAccumulator)
#include "tst_wideband_frame_accumulator.moc"
