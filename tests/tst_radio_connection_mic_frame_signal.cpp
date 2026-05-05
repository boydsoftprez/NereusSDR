// no-port-check: NereusSDR-original test file. No Thetis logic is ported here.
// Tests the RadioConnection::micFrameDecoded Qt signal added in 3M-1b F.4.
//
// micFrameDecoded (F.4) carries a raw const float* pointer and follows the
// DirectConnection-only contract documented in RadioConnection.h.  QSignalSpy
// can capture the signal in the test because the spy connects synchronously
// (no cross-thread dispatch). The const float* type must be registered with
// the Qt metatype system via qRegisterMetaType before constructing the spy.
//
// Test cases (4):
//   micFrameDecoded_signalExists        — QSignalSpy can attach to the signal
//   micFrameDecoded_emit_capturedBySpy  — emitMicFrame → spy.count() == 1
//   micFrameDecoded_signalCarriesFrameCount
//                                       — args.at(1) int frames matches n
//   micFrameDecoded_multipleEmissions   — spy accumulates across 3 emits
//
// TestRadioConnection is a minimal concrete subclass that stubs all pure
// virtuals and exposes emitMicFrame() for test driving.
//
// Plan: 3M-1b F.4. Pre-code review §6.4.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-27 — New test for Phase 3M-1b Task F.4: verify micFrameDecoded
//                 signal exists and fires with correct frame count.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/RadioConnection.h"

using namespace NereusSDR;

// ── TestRadioConnection ──────────────────────────────────────────────────────
//
// Minimal concrete subclass of RadioConnection. Stubs all pure-virtual slots
// with no-ops. Exposes emitMicFrame() so tests can fire micFrameDecoded
// without needing a real P1/P2 connection thread.
class TestRadioConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit TestRadioConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {}

    // Pure-virtual slot stubs — no-ops.
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void sendTxIq(const float*, int) override {}

    // Test helper — fires micFrameDecoded with the supplied buffer and count.
    void emitMicFrame(const float* samples, int frames)
    {
        emit micFrameDecoded(samples, frames);
    }
};

// ── Test fixture ──────────────────────────────────────────────────────────────

class TstRadioConnectionMicFrameSignal : public QObject {
    Q_OBJECT

private slots:

    // ── QSignalSpy can attach to micFrameDecoded ──────────────────────────────
    //
    // Verifies that the signal exists and is connectable via QSignalSpy.
    // If the signal is missing or has the wrong signature, spy.isValid()
    // returns false.

    void micFrameDecoded_signalExists()
    {
        // Register the raw pointer type so QSignalSpy can handle it.
        qRegisterMetaType<const float*>("const float*");

        TestRadioConnection conn;
        QSignalSpy spy(&conn, &RadioConnection::micFrameDecoded);

        QVERIFY(spy.isValid());
    }

    // ── emitMicFrame → spy captures one emission ──────────────────────────────
    //
    // After one emitMicFrame() call, the spy must record exactly 1 emission.

    void micFrameDecoded_emit_capturedBySpy()
    {
        qRegisterMetaType<const float*>("const float*");

        TestRadioConnection conn;
        QSignalSpy spy(&conn, &RadioConnection::micFrameDecoded);

        float buf[16]{};
        conn.emitMicFrame(buf, 16);

        QCOMPARE(spy.count(), 1);
    }

    // ── Second argument (int frames) matches the emitted value ───────────────
    //
    // The signal carries (const float* samples, int frames). The `frames`
    // argument must equal exactly what was passed to emitMicFrame(). This is
    // the load-bearing invariant: RadioMicSource (F.2) uses the frame count
    // to know how many samples to copy into its SPSC ring.

    void micFrameDecoded_signalCarriesFrameCount()
    {
        qRegisterMetaType<const float*>("const float*");

        TestRadioConnection conn;
        QSignalSpy spy(&conn, &RadioConnection::micFrameDecoded);

        constexpr int kExpectedFrames = 48;
        float buf[kExpectedFrames]{};
        conn.emitMicFrame(buf, kExpectedFrames);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        // args.at(1) is the `int frames` argument — always capturable as
        // a standard Qt metatype.
        QCOMPARE(args.at(1).value<int>(), kExpectedFrames);
    }

    // ── Spy accumulates across multiple emissions ─────────────────────────────
    //
    // The signal must be emitted on every call to emitMicFrame(), not only
    // on the first call. After N calls, spy.count() must equal N.

    void micFrameDecoded_multipleEmissions()
    {
        qRegisterMetaType<const float*>("const float*");

        TestRadioConnection conn;
        QSignalSpy spy(&conn, &RadioConnection::micFrameDecoded);

        float buf[8]{};
        for (int i = 1; i <= 3; ++i) {
            conn.emitMicFrame(buf, 8);
            QCOMPARE(spy.count(), i);
        }
    }
};

QTEST_APPLESS_MAIN(TstRadioConnectionMicFrameSignal)
#include "tst_radio_connection_mic_frame_signal.moc"
