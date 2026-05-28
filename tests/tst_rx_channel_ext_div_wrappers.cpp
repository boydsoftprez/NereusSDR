// =================================================================
// tests/tst_rx_channel_ext_div_wrappers.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic G Task 1: RxChannel wrapper signatures for the
// WDSP External Diversity API (SetEXTDIVRun / SetEXTDIVNr /
// SetEXTDIVOutput / SetEXTDIVRotate). Compile-time smoke check via
// method-pointer-take. Real DSP behavior requires a live WDSP
// session with create_divEXT() called against a valid id in the
// MAX_EXT_DIVS=2 slot range; that path is exercised by the Sub-Epic
// H bench harness.
// =================================================================

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

class TestRxChannelExtDivWrappers : public QObject {
    Q_OBJECT

private slots:
    // Verify the wrapper signatures exist on RxChannel without invoking
    // WDSP. Taking the method pointer is enough to force the compiler
    // to resolve the declaration, which is the actual contract this
    // task delivers (the WDSP-side behavior is exercised in Sub-Epic H).
    //
    // We deliberately do NOT call these via an RxChannel instance,
    // because WDSP's div.c keys SetEXTDIV* off pdiv[id] where pdiv is
    // a 2-slot array; with no create_divEXT() ahead of the call the
    // wrapper would deref garbage and crash.

    void wrapper_setExtDivRun_signature_compiles()
    {
        auto p = &RxChannel::setExtDivRun;
        QVERIFY(p != nullptr);
    }

    void wrapper_setExtDivNr_signature_compiles()
    {
        auto p = &RxChannel::setExtDivNr;
        QVERIFY(p != nullptr);
    }

    void wrapper_setExtDivOutput_signature_compiles()
    {
        auto p = &RxChannel::setExtDivOutput;
        QVERIFY(p != nullptr);
    }

    void wrapper_setExtDivRotate_signature_compiles()
    {
        auto p = &RxChannel::setExtDivRotate;
        QVERIFY(p != nullptr);
    }
};

QTEST_MAIN(TestRxChannelExtDivWrappers)
#include "tst_rx_channel_ext_div_wrappers.moc"
