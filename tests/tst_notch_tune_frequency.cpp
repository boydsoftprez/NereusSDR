// =================================================================
// tests/tst_notch_tune_frequency.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Tunable Notch Filter, Task 1. Covers
// docs/architecture/2026-07-28-tunable-notch-filter-design.md section 4:
//   4.1 / 4.2  the notch database's tune frequency is the hosting DDC
//              stream's CENTRE, pushed from bindSliceToStream.
//   4.3        the shift reaches the notch database on the way back down
//              to zero, and keeps its sign.
//   4.4        the stream offset and RIT/DIG compose instead of clobbering
//              each other, asserted in both writer orders.
//   4.5        the connect-time seed commands the stream centre, not the
//              slice frequency.
//
// The invariant every slot below is really asserting, from 4.1:
//     tunefreq + shift == the slice's demodulated RF
//                      == stream centre + slice offset + RIT + DIG
// asserted in BOTH halves, never the sum alone: a sum-only assertion
// passes under the double-count bug whenever the shift happens to be zero.
//
// WDSP is live in test binaries (CMakeLists.txt sets HAVE_WDSP PUBLIC) and
// RXANBPSetTuneFrequency dereferences rxa[channel].ndb.p before it compares
// (third_party/wdsp/src/nbp.c:477-479), so the usual kTestChannel = 99
// never-opened-channel hatch is an out-of-bounds read here, not a no-op.
// Every slot uses a really opened channel through the NEREUS_BUILD_TESTS
// friend seam, the pattern at tests/tst_stream_pool_binding.cpp:1018,1033.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/P1RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Stream geometry shared by every slot. 192 kHz gives a +/- 96 kHz window
// (SliceStreamAllocator::windowContains), so 14.200 and 14.210 MHz share a
// stream and 14.200 / 14.400 MHz do not.
constexpr int    kRateHz       = 192000;
constexpr double kSliceAFreqHz = 14200000.0;
constexpr double kSliceBFreqHz = 14210000.0;
constexpr double kFarFreqHz    = 14400000.0;

// Detaches a stack-injected RadioConnection on scope exit, however the
// scope is left. From tests/tst_stream_pool_binding.cpp:57-63: QCOMPARE
// returns from the enclosing slot on failure, so a trailing
// injectConnectionForTest(nullptr) is skipped on exactly the run where it
// matters most.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

} // namespace

class TestNotchTuneFrequency : public QObject {
    Q_OBJECT

private slots:
    // -- 4.6: the RxChannel carries ---------------------------------------

    void notch_tune_frequency_defaults_to_zero()
    {
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        // The construction default, and the whole defect: nothing in the
        // tree pushed this value, so every channel's notchdb.tunefreq sat
        // here and calc_nbp_lightweight mapped notches from the wrong RF
        // origin (third_party/wdsp/src/nbp.c:192).
        QCOMPARE(ch->notchTuneFrequencyHz(), 0.0);
    }

    void notch_tune_frequency_carry_round_trips()
    {
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        ch->setNotchTuneFrequency(kSliceAFreqHz);
        QCOMPARE(ch->notchTuneFrequencyHz(), kSliceAFreqHz);
    }

    void shift_push_keeps_its_sign()
    {
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        // A slice 10 kHz ABOVE its stream centre pushes +10000, not -10000.
        // Thetis's -value at radio.cs:1419-1420 [v2.10.3.15] is not a
        // divergence: rx_osc is already the negated quantity upstream
        // (console.cs:31916-31922), so Thetis's -rx_osc equals the offsetHz
        // handed in here, which equals frequencyHz - centreHz at
        // SliceStreamAllocator.cpp:70. Locked here so it never gets
        // "corrected" into an inversion of every shifted slice.
        ch->setShiftFrequency(10000.0);
        QCOMPARE(ch->shiftOffsetHz(), 10000.0);
    }
};

QTEST_MAIN(TestNotchTuneFrequency)
#include "tst_notch_tune_frequency.moc"
