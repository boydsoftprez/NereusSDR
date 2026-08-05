// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - tst_rade_text: tests for the Phase 3R Task I4 RadeText
// callsign-over-EOO wrapper.
//
// RadeText is a thin Qt6 wrapper around the third_party/rade library's
// native callsign-over-EOO channel (rade_tx_set_eoo_callsign /
// rade_rx_get_eoo_callsign, declared in third_party/rade/src/rade_api.h
// :120-145 [@b289102]). The wrapper does not own any rade state;
// callers pass the active `struct rade*` from RadeChannel when
// encoding, and pass raw EOO soft-decision bits when decoding. Wire-up
// into RadeChannel's processIq / txEncode paths is deferred to
// Phase L per the plan.
//
// Five lifecycle / behaviour contracts:
//
//   1. constructsAndIsEmpty             a fresh RadeText reports an
//                                       empty ourCallsign().
//   2. setOurCallsignStoresValue        setOurCallsign("KG4VCF") then
//                                       ourCallsign() returns the same
//                                       string.
//   3. pushTxNullRadeIsNoOp             pushTxCallsign(nullptr) is
//                                       safe; no crash.
//   4. pushTxEmptyCallsignIsNoOp        pushTxCallsign(rade) when
//                                       ourCallsign() is empty is a
//                                       no-op; the underlying EOO bits
//                                       must remain in their pre-call
//                                       state (rade_tx.eoo_bits cleared
//                                       to a known fingerprint).
//   5. roundTripDecodesEncodedCallsign  setOurCallsign("KG4VCF") then
//                                       pushTxCallsign(rade), read the
//                                       resulting eoo_bits out of the
//                                       rade struct, feed them to
//                                       processRxEooBits, and verify
//                                       the textDecoded signal fires
//                                       once with "KG4VCF". This is
//                                       the headline test; it mirrors
//                                       the upstream
//                                       third_party/rade/src/rade_callsign_test.c
//                                       round-trip but skips the full
//                                       OFDM modulation/demodulation
//                                       (the wrapper only sits on
//                                       set_eoo_callsign / get_eoo_callsign,
//                                       so a bit-level round-trip
//                                       exercises everything the
//                                       wrapper actually does).
//
// See src/core/RadeText.h for the public surface and design notes.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QString>
#include <vector>

#include "core/RadeText.h"

extern "C" {
#include "rade_api.h"
}

using namespace NereusSDR;

class TestRadeText : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void constructsAndIsEmpty();
    void setOurCallsignStoresValue();
    void pushTxNullRadeIsNoOp();
    void pushTxEmptyCallsignIsNoOp();
    void roundTripDecodesEncodedCallsign();

private:
    // One rade instance shared across the tests that need it. opened
    // in initTestCase() with the "dummy" sentinel (built-in weights;
    // see third_party/rade/src/rade_api_nopy.c:58-76 [@b289102]) and
    // closed in cleanupTestCase().
    struct rade* m_rade{nullptr};
};

void TestRadeText::initTestCase()
{
    // Initialise librade once; rade_open("dummy", ...) returns a
    // working rade context whose tx.eoo_bits[] is the only field we
    // actually read in these tests.
    rade_initialize();
    m_rade = rade_open(const_cast<char*>("dummy"), RADE_VERBOSE_0);
    QVERIFY2(m_rade != nullptr, "rade_open(\"dummy\") returned NULL");
}

void TestRadeText::cleanupTestCase()
{
    if (m_rade) {
        rade_close(m_rade);
        m_rade = nullptr;
    }
    rade_finalize();
}

void TestRadeText::constructsAndIsEmpty()
{
    RadeText rt;
    QVERIFY(rt.ourCallsign().isEmpty());
}

void TestRadeText::setOurCallsignStoresValue()
{
    RadeText rt;
    rt.setOurCallsign("KG4VCF");
    QCOMPARE(rt.ourCallsign(), QStringLiteral("KG4VCF"));

    // Lowercase input must also round-trip through the setter
    // unchanged; pushTxCallsign() is the layer that upper-cases on the
    // way to the wire.
    rt.setOurCallsign("kg4vcf");
    QCOMPARE(rt.ourCallsign(), QStringLiteral("kg4vcf"));

    // Empty input clears the stash.
    rt.setOurCallsign(QString());
    QVERIFY(rt.ourCallsign().isEmpty());
}

void TestRadeText::pushTxNullRadeIsNoOp()
{
    // pushTxCallsign(nullptr) must not crash even when a callsign is
    // stashed. RadeChannel calls pushTxCallsign with its m_rade
    // pointer; that pointer is null between stop() and the next
    // start(), so this is the contract that keeps the wire-up safe
    // across MOX transitions in the Phase L follow-up.
    RadeText rt;
    rt.setOurCallsign("KG4VCF");
    rt.pushTxCallsign(nullptr);
    // No assert: the test passes iff it returns without crashing.
    QVERIFY(true);
}

void TestRadeText::pushTxEmptyCallsignIsNoOp()
{
    // pushTxCallsign() when ourCallsign() is empty must NOT clobber
    // the rade_tx eoo_bits[] buffer. We pre-seed the buffer with a
    // recognisable fingerprint (-2.0f everywhere, an out-of-band value
    // that rade_tx_set_eoo_callsign never writes - it writes +1.0f or
    // -1.0f per bit; see rade_api_nopy.c:159-173 [@b289102]) and
    // confirm none of the first RADE_EOO_CALLSIGN_MAX*7 = 56 bits were
    // touched.
    QVERIFY(m_rade != nullptr);

    constexpr float kSentinel = -2.0f;
    constexpr int kBitsTouchedIfActive = RADE_EOO_CALLSIGN_MAX * 7;
    for (int i = 0; i < kBitsTouchedIfActive; ++i) {
        m_rade->tx.eoo_bits[i] = kSentinel;
    }

    RadeText rt;  // ourCallsign empty by construction
    rt.pushTxCallsign(m_rade);

    for (int i = 0; i < kBitsTouchedIfActive; ++i) {
        QCOMPARE(m_rade->tx.eoo_bits[i], kSentinel);
    }
}

void TestRadeText::roundTripDecodesEncodedCallsign()
{
    // Headline test: stash a callsign, encode it into the rade
    // tx.eoo_bits[] buffer via pushTxCallsign, then hand the buffer
    // straight to processRxEooBits as if it had survived the OFDM
    // round-trip (which is exercised separately by
    // third_party/rade/src/rade_callsign_test.c). textDecoded must
    // fire exactly once with the original callsign.
    //
    // We skip the OFDM modulate/demodulate hop because the wrapper
    // only sits on set_eoo_callsign / get_eoo_callsign; a bit-level
    // round-trip is the tightest test of the wrapper's actual
    // surface and avoids the rade_ofdm dependency the upstream test
    // pulls in.
    QVERIFY(m_rade != nullptr);

    RadeText rt;
    QSignalSpy decodedSpy(&rt, &RadeText::textDecoded);

    const QString kCall = QStringLiteral("KG4VCF");
    rt.setOurCallsign(kCall);
    rt.pushTxCallsign(m_rade);

    // rade_n_eoo_bits is the soft-decision bit count for the EOO
    // frame; rade_api.h:114 [@b289102]. At the v1 default this is
    // 180 bits; the callsign occupies the first 56. We pass the
    // whole buffer to mirror what rade_rx populates in production.
    const int nEooBits = rade_n_eoo_bits(m_rade);
    QVERIFY2(nEooBits >= RADE_EOO_CALLSIGN_MAX * 7,
             qPrintable(QString("rade_n_eoo_bits() returned %1, expected >= %2")
                            .arg(nEooBits)
                            .arg(RADE_EOO_CALLSIGN_MAX * 7)));

    rt.processRxEooBits(m_rade->tx.eoo_bits, nEooBits);

    QCOMPARE(decodedSpy.count(), 1);
    QCOMPARE(decodedSpy.first().value(0).toString(), kCall);
}

QTEST_GUILESS_MAIN(TestRadeText)
#include "tst_rade_text.moc"
