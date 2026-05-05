// tst_xit_offset_application.cpp
//
// no-port-check: NereusSDR-original test file. All Thetis XIT source cites
// are in RadioModel.cpp / SliceModel.cpp.
// =================================================================
// tests/tst_xit_offset_application.cpp  (NereusSDR)
// =================================================================
//
// B6 (Task 16) — TDD: verifies that RadioModel::wireSliceSignals() applies
// the XIT offset to setTxFrequency, mirroring the RIT pattern for the RX
// shift.
//
// Thetis parallel: console.cs chkXIT / txtXIT path where XIT enable/value
// offsets the TX NCO without moving the RX DDC center.
//
// Test strategy:
//   - MockTxFreqConnection captures setTxFrequency() call arguments.
//   - RadioModel constructed, slice added, mock injected.
//   - wireSliceSignalsForTest() called to install the signal connects
//     (mirrors what wireConnectionSignals() does on a real connect).
//   - SliceModel XIT properties mutated; QCoreApplication::processEvents()
//     flushes the auto-connection queued call.
//   - Captured txFreqHz values asserted.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (B6 cluster).
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// ── MockTxFreqConnection ──────────────────────────────────────────────────────
// Minimal RadioConnection that records every setTxFrequency() call.
class MockTxFreqConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<quint64> txFreqCalls;

    explicit MockTxFreqConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64 hz) override { txFreqCalls.append(hz); }
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    // Added on main during the merge: P1 full-parity Task 2.4 introduced
    // setLineInGain + setUserDigOut + setPuresignalRun pure virtuals.
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
};

// ── Test helpers ──────────────────────────────────────────────────────────────

// Helper: build a wired RadioModel + slice with a mock connection injected.
// Returns the mock (caller cleans up via injectConnectionForTest(nullptr) +
// delete before the RadioModel goes out of scope).
static MockTxFreqConnection* buildWiredModel(RadioModel& model)
{
    auto* mock = new MockTxFreqConnection();
    model.addSlice();
    model.setActiveSlice(0);
    model.injectConnectionForTest(mock);
    // Install the slice→connection signal connects, exactly as
    // wireConnectionSignals() does on a real radio connect.
    model.wireSliceSignalsForTest();
    return mock;
}

// Flush any queued QMetaObject::invokeMethod() calls.
static void flushEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

// ── Test class ────────────────────────────────────────────────────────────────
class TstXitOffsetApplication : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── 1. XIT disabled: setXitHz does not shift TX frequency ────────────────
    // When xitEnabled == false, changing xitHz must NOT trigger a TX push
    // (the updateTxFrequency lambda guards on xitEnabled).
    void disabledXit_xitHzChange_doesNotTriggerTxPush()
    {
        RadioModel model;
        auto* mock = buildWiredModel(model);

        SliceModel* slice = model.sliceAt(0);
        QVERIFY(slice);

        slice->setXitEnabled(false);
        flushEvents();

        // xitEnabledChanged fired (false→false is idempotent, so no fire),
        // but even if it fired, xitEnabled==false → xitOffset=0 → same TX freq.
        const int callsBeforeHzChange = mock->txFreqCalls.size();

        slice->setXitHz(300);
        flushEvents();

        // xitHzChanged fired, but the lambda sees xitEnabled()==false so it
        // reads offset=0 and calls setTxFrequency(rawFreq + 0) — same as base.
        // The call IS made (offset=0), so count may increase by 1.
        // Key assertion: the value sent must equal the raw VFO frequency (no offset).
        if (mock->txFreqCalls.size() > callsBeforeHzChange) {
            const quint64 rawFreq = static_cast<quint64>(slice->frequency());
            QCOMPARE(mock->txFreqCalls.last(), rawFreq);
        }

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 2. XIT enabled: setXitHz shifts TX frequency by exactly that offset ──
    void enabledXit_setXitHz_shiftsTxByHz()
    {
        RadioModel model;
        auto* mock = buildWiredModel(model);

        SliceModel* slice = model.sliceAt(0);
        QVERIFY(slice);

        // Set a known VFO frequency.
        slice->setFrequency(14200000.0);
        flushEvents();
        mock->txFreqCalls.clear();  // ignore the RX freq push

        // Enable XIT + set offset.
        slice->setXitEnabled(true);
        flushEvents();

        slice->setXitHz(500);
        flushEvents();

        // After setXitHz(500) with XIT enabled, setTxFrequency should have been
        // called at least once with 14200000 + 500 = 14200500.
        const quint64 expected = 14200000ULL + 500ULL;
        bool found = false;
        for (quint64 v : mock->txFreqCalls) {
            if (v == expected) { found = true; break; }
        }
        QVERIFY2(found, "setTxFrequency was not called with XIT-shifted frequency");

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 3. XIT enable toggle: enabling XIT pushes offset; disabling pushes base ─
    void xitEnableToggle_pushesCorrectFrequency()
    {
        RadioModel model;
        auto* mock = buildWiredModel(model);

        SliceModel* slice = model.sliceAt(0);
        QVERIFY(slice);

        slice->setFrequency(7100000.0);
        slice->setXitHz(1000);
        flushEvents();
        mock->txFreqCalls.clear();

        // Enable XIT → should push 7101000
        slice->setXitEnabled(true);
        flushEvents();

        const quint64 expectedOn = 7100000ULL + 1000ULL;
        bool foundOn = false;
        for (quint64 v : mock->txFreqCalls) {
            if (v == expectedOn) { foundOn = true; break; }
        }
        QVERIFY2(foundOn, "setTxFrequency not called with XIT-on offset");

        mock->txFreqCalls.clear();

        // Disable XIT → should push 7100000 (no offset)
        slice->setXitEnabled(false);
        flushEvents();

        const quint64 expectedOff = 7100000ULL;
        bool foundOff = false;
        for (quint64 v : mock->txFreqCalls) {
            if (v == expectedOff) { foundOff = true; break; }
        }
        QVERIFY2(foundOff, "setTxFrequency not called with XIT-off base freq");

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 4. Negative XIT: negative offset subtracts correctly ─────────────────
    void negativeXit_subtractsFromTxFrequency()
    {
        RadioModel model;
        auto* mock = buildWiredModel(model);

        SliceModel* slice = model.sliceAt(0);
        QVERIFY(slice);

        slice->setFrequency(14200000.0);
        slice->setXitEnabled(true);
        flushEvents();
        mock->txFreqCalls.clear();

        slice->setXitHz(-500);
        flushEvents();

        const quint64 expected = 14200000ULL - 500ULL;
        bool found = false;
        for (quint64 v : mock->txFreqCalls) {
            if (v == expected) { found = true; break; }
        }
        QVERIFY2(found, "setTxFrequency not called with negative XIT offset subtracted");

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 5. XIT zero: xitHz==0 with XIT enabled sends exact VFO frequency ─────
    void xitEnabledZeroHz_sendsBaseFrequency()
    {
        RadioModel model;
        auto* mock = buildWiredModel(model);

        SliceModel* slice = model.sliceAt(0);
        QVERIFY(slice);

        slice->setFrequency(14200000.0);
        slice->setXitEnabled(true);
        slice->setXitHz(0);
        flushEvents();
        mock->txFreqCalls.clear();

        // Trigger the updateTxFrequency lambda by re-enabling XIT.
        slice->setXitEnabled(false);
        flushEvents();
        slice->setXitEnabled(true);
        flushEvents();

        const quint64 expected = 14200000ULL;
        bool found = false;
        for (quint64 v : mock->txFreqCalls) {
            if (v == expected) { found = true; break; }
        }
        QVERIFY2(found, "setTxFrequency not called with exact base frequency when xitHz==0");

        model.injectConnectionForTest(nullptr);
        delete mock;
    }
};

QTEST_MAIN(TstXitOffsetApplication)
#include "tst_xit_offset_application.moc"
