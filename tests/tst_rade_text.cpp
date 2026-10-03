// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-original test.
//
// NereusSDR - tst_rade_text: the RadeText wrapper sends and reads the RADE
// end-of-over callsign in FreeDV's format through librade.
//
//   constructsAndIsEmpty            a fresh RadeText has no callsign.
//   setOurCallsignStoresValue       the setter keeps what it is given.
//   pushTxNullRadeIsNoOp            pushTxCallsign(nullptr) is safe (a
//                                   stopped RadeChannel has no rade).
//   pushTxWritesFreeDvFormat        the EOO data librade will send is,
//                                   float for float, what FreeDV's
//                                   rade_text writes for the callsign
//                                   (freedv-backend vectors).
//   pushTxEmptyCallsignWritesZeros  no callsign: zeros, what FreeDV sends
//                                   when it has none, not a stale call.
//   decodesFreeDvStation            a FreeDV station's EOO data (upstream
//                                   vector, with noise) emits its callsign.
//   emptyCallsignNotEmitted         a valid empty callsign emits nothing.
//   noiseNotEmitted                 data FreeDV rejects emits nothing.
//   endToEndThroughModem            the callsign survives librade: rade_tx
//                                   frames, rade_tx_eoo, the real leg only
//                                   (what goes on air), rade_rx, and
//                                   processRxEooBits emits it once.
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I4: initial tests over
//                 librade's raw ASCII helpers. AI tooling: Anthropic Claude
//                 Code.
//   2026-09-28  J.J. Boyd / KG4VCF  RADE end-of-over callsigns: rewritten
//                 for FreeDV's format. AI tooling: Anthropic Claude Code.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QString>

#include <cstring>
#include <vector>

#include "core/RadeText.h"
#include "data/rade_text/freedv_backend_vectors.h"

extern "C" {
#include "rade_api.h"
}

using namespace NereusSDR;

namespace {

const RadeTextVectors::EncodeVector* encodeVector(const char* callsign)
{
    for (const auto& v : RadeTextVectors::kEncode) {
        if (std::strcmp(v.callsign, callsign) == 0) {
            return &v;
        }
    }
    return nullptr;
}

const RadeTextVectors::DecodeVector* decodeVector(const char* label)
{
    for (const auto& v : RadeTextVectors::kDecode) {
        if (std::strcmp(v.label, label) == 0) {
            return &v;
        }
    }
    return nullptr;
}

}  // namespace

class TestRadeText : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void constructsAndIsEmpty();
    void setOurCallsignStoresValue();
    void pushTxNullRadeIsNoOp();
    void pushTxWritesFreeDvFormat();
    void pushTxEmptyCallsignWritesZeros();
    void decodesFreeDvStation();
    void emptyCallsignNotEmitted();
    void noiseNotEmitted();
    void endToEndThroughModem();

private:
    // Built-in weights ("dummy"; rade_api_nopy.c:58-76 [@b289102]).
    struct rade* m_rade{nullptr};
};

void TestRadeText::initTestCase()
{
    rade_initialize();
    m_rade = rade_open(const_cast<char*>("dummy"),
                       RADE_USE_C_ENCODER | RADE_USE_C_DECODER | RADE_VERBOSE_0);
    QVERIFY2(m_rade != nullptr, "rade_open(\"dummy\") returned NULL");
    QCOMPARE(rade_n_eoo_bits(m_rade), RadeTextVectors::kEooFloats);
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
    rt.setOurCallsign(QStringLiteral("KG4VCF"));
    QCOMPARE(rt.ourCallsign(), QStringLiteral("KG4VCF"));
    rt.setOurCallsign(QStringLiteral("kg4vcf"));
    QCOMPARE(rt.ourCallsign(), QStringLiteral("kg4vcf"));
    rt.setOurCallsign(QString());
    QVERIFY(rt.ourCallsign().isEmpty());
}

void TestRadeText::pushTxNullRadeIsNoOp()
{
    RadeText rt;
    rt.setOurCallsign(QStringLiteral("KG4VCF"));
    rt.pushTxCallsign(nullptr);
    QVERIFY(true);
}

void TestRadeText::pushTxWritesFreeDvFormat()
{
    for (const char* call : {"KG4VCF", "kg4vcf", "W1AW/P", "KG4VCF/QRP"}) {
        const auto* v = encodeVector(call);
        QVERIFY(v != nullptr);
        RadeText rt;
        rt.setOurCallsign(QString::fromLatin1(call));
        rt.pushTxCallsign(m_rade);
        for (int i = 0; i < RadeTextVectors::kEooFloats; ++i) {
            QVERIFY2(m_rade->tx.eoo_bits[i] == v->syms[i],
                     qPrintable(QStringLiteral("%1: EOO float %2").arg(call).arg(i)));
        }
    }
}

void TestRadeText::pushTxEmptyCallsignWritesZeros()
{
    RadeText rt;
    rt.setOurCallsign(QStringLiteral("KG4VCF"));
    rt.pushTxCallsign(m_rade);
    rt.setOurCallsign(QString());
    rt.pushTxCallsign(m_rade);
    for (int i = 0; i < RadeTextVectors::kEooFloats; ++i) {
        QCOMPARE(m_rade->tx.eoo_bits[i], 0.0f);
    }
}

void TestRadeText::decodesFreeDvStation()
{
    const auto* v = decodeVector("KG4VCF sigma 0.6 seed 5");
    QVERIFY(v != nullptr);
    QVERIFY(v->decoded);
    RadeText rt;
    QSignalSpy spy(&rt, &RadeText::textDecoded);
    rt.processRxEooBits(v->syms, RadeTextVectors::kEooFloats);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().value(0).toString(), QStringLiteral("KG4VCF"));
}

void TestRadeText::emptyCallsignNotEmitted()
{
    const auto* v = decodeVector("clean ");
    QVERIFY(v != nullptr);
    QVERIFY(v->decoded);  // FreeDV accepts it (an empty text)...
    RadeText rt;
    QSignalSpy spy(&rt, &RadeText::textDecoded);
    rt.processRxEooBits(v->syms, RadeTextVectors::kEooFloats);
    QCOMPARE(spy.count(), 0);  // ...and there is no callsign to show.
}

void TestRadeText::noiseNotEmitted()
{
    const auto* v = decodeVector("VK5DGR sigma 2.0 seed 8");
    QVERIFY(v != nullptr);
    QVERIFY(!v->decoded);
    RadeText rt;
    QSignalSpy spy(&rt, &RadeText::textDecoded);
    rt.processRxEooBits(v->syms, RadeTextVectors::kEooFloats);
    QCOMPARE(spy.count(), 0);
}

void TestRadeText::endToEndThroughModem()
{
    // A fresh pair so the other cases' state does not matter.
    struct rade* tx = rade_open(const_cast<char*>("dummy"),
                                RADE_USE_C_ENCODER | RADE_USE_C_DECODER | RADE_VERBOSE_0);
    struct rade* rx = rade_open(const_cast<char*>("dummy"),
                                RADE_USE_C_ENCODER | RADE_USE_C_DECODER | RADE_VERBOSE_0);
    QVERIFY(tx != nullptr && rx != nullptr);

    RadeText sender;
    sender.setOurCallsign(QStringLiteral("KG4VCF"));
    sender.pushTxCallsign(tx);

    // About 1.5 s of modem frames (zero features), the EOO, then silence.
    std::vector<float> air;
    const int nFeat = rade_n_features_in_out(tx);
    const int nOut = rade_n_tx_out(tx);
    std::vector<float> features(static_cast<size_t>(nFeat), 0.0f);
    std::vector<RADE_COMP> out(static_cast<size_t>(std::max(nOut, rade_n_tx_eoo_out(tx))));
    int frames = 0;
    while (static_cast<int>(air.size()) < RADE_MODEM_SAMPLE_RATE * 3 / 2) {
        const int n = rade_tx(tx, out.data(), features.data());
        for (int i = 0; i < n; ++i) {
            air.push_back(out[static_cast<size_t>(i)].real);  // the real leg goes on air
        }
        ++frames;
    }
    const int nEoo = rade_tx_eoo(tx, out.data());
    for (int i = 0; i < nEoo; ++i) {
        air.push_back(out[static_cast<size_t>(i)].real);
    }
    air.insert(air.end(), RADE_MODEM_SAMPLE_RATE / 2, 0.0f);
    QVERIFY(frames > 0);

    RadeText receiver;
    QSignalSpy spy(&receiver, &RadeText::textDecoded);
    std::vector<float> feat(static_cast<size_t>(rade_n_features_in_out(rx)));
    std::vector<float> eoo(static_cast<size_t>(rade_n_eoo_bits(rx)));
    std::vector<RADE_COMP> in(static_cast<size_t>(rade_nin_max(rx)));
    size_t pos = 0;
    int eooFrames = 0;
    for (;;) {
        const int nin = rade_nin(rx);
        if (pos + static_cast<size_t>(nin) > air.size()) {
            break;
        }
        for (int i = 0; i < nin; ++i) {
            in[static_cast<size_t>(i)].real = air[pos + static_cast<size_t>(i)];
            in[static_cast<size_t>(i)].imag = 0.0f;
        }
        pos += static_cast<size_t>(nin);
        int hasEoo = 0;
        rade_rx(rx, feat.data(), &hasEoo, eoo.data(), in.data());
        if (hasEoo) {
            ++eooFrames;
            receiver.processRxEooBits(eoo.data(), static_cast<int>(eoo.size()));
        }
    }
    rade_close(tx);
    rade_close(rx);

    QVERIFY2(eooFrames >= 1, "rade_rx never reported the end-of-over frame");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().value(0).toString(), QStringLiteral("KG4VCF"));
}

QTEST_GUILESS_MAIN(TestRadeText)
#include "tst_rade_text.moc"
