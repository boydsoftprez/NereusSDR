// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-original test. The vectors it reads were
// written by freedv-backend's own code (scripts/gen-rade-text-vectors.sh).
//
// tst_rade_text_codec: a NereusSDR station and a FreeDV station read each
// other's RADE end-of-over callsigns.
//
//   encodeMatchesFreeDv   RadeTextCodec::generateTxString writes exactly
//                         the 180 EOO floats freedv-backend's
//                         rade_text_generate_tx_string writes, for plain,
//                         lower-case, slashed, punctuated, out-of-set,
//                         over-long and empty callsigns. A FreeDV station
//                         therefore receives what it would from FreeDV.
//   decodeMatchesFreeDv   RadeTextCodec::decodeRx accepts (with the same
//                         text) or rejects exactly the clean and noisy
//                         symbol sets freedv-backend's rade_text_rx accepts
//                         or rejects: NereusSDR reads a FreeDV station as
//                         FreeDV does.
//   roundTrip             every encode vector decodes to FreeDV's text.
//   shortInputRejected    fewer than 56 symbols is refused, not read past.
//
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  RADE end-of-over callsigns. AI tooling:
//                 Anthropic Claude Code.

#include <QtTest/QtTest>

#include <cstring>
#include <string>
#include <vector>

#include "core/RadeTextCodec.h"
#include "data/rade_text/freedv_backend_vectors.h"

using namespace NereusSDR;

class TestRadeTextCodec : public QObject {
    Q_OBJECT

private slots:
    void encodeMatchesFreeDv();
    void decodeMatchesFreeDv();
    void roundTrip();
    void shortInputRejected();
};

void TestRadeTextCodec::encodeMatchesFreeDv()
{
    for (const auto& v : RadeTextVectors::kEncode) {
        std::vector<float> syms(RadeTextVectors::kEooFloats, -9.0f);
        radetext::generateTxString(v.callsign, static_cast<int>(std::strlen(v.callsign)),
                                   syms.data(), RadeTextVectors::kEooFloats);
        for (int i = 0; i < RadeTextVectors::kEooFloats; ++i) {
            if (syms[static_cast<size_t>(i)] != v.syms[i]) {
                QFAIL(qPrintable(QStringLiteral("callsign \"%1\": float %2 is %3, FreeDV wrote %4")
                                     .arg(QString::fromLatin1(v.callsign))
                                     .arg(i)
                                     .arg(syms[static_cast<size_t>(i)])
                                     .arg(v.syms[i])));
            }
        }
    }
}

void TestRadeTextCodec::decodeMatchesFreeDv()
{
    int accepted = 0;
    int rejected = 0;
    for (const auto& v : RadeTextVectors::kDecode) {
        std::string text;
        const bool ok = radetext::decodeRx(v.syms, RadeTextVectors::kEooFloats / 2, &text);
        QVERIFY2(ok == v.decoded,
                 qPrintable(QStringLiteral("%1: NereusSDR %2, FreeDV %3")
                                .arg(QString::fromLatin1(v.label))
                                .arg(ok ? "accepted" : "rejected")
                                .arg(v.decoded ? "accepted" : "rejected")));
        if (ok) {
            QCOMPARE(QString::fromStdString(text), QString::fromLatin1(v.text));
            ++accepted;
        } else {
            ++rejected;
        }
    }
    // The vector set holds both outcomes, so this checks both directions.
    QVERIFY(accepted > 0);
    QVERIFY(rejected > 0);
}

void TestRadeTextCodec::roundTrip()
{
    for (const auto& v : RadeTextVectors::kEncode) {
        std::vector<float> syms(RadeTextVectors::kEooFloats, 0.0f);
        radetext::generateTxString(v.callsign, static_cast<int>(std::strlen(v.callsign)),
                                   syms.data(), RadeTextVectors::kEooFloats);
        std::string text;
        QVERIFY(radetext::decodeRx(syms.data(), RadeTextVectors::kEooFloats / 2, &text));
        // The same vector's clean decode (FreeDV's text for it).
        bool found = false;
        const std::string label = std::string("clean ") + v.callsign;
        for (const auto& d : RadeTextVectors::kDecode) {
            if (label == d.label) {
                QCOMPARE(QString::fromStdString(text), QString::fromLatin1(d.text));
                found = true;
            }
        }
        QVERIFY(found);
    }
}

void TestRadeTextCodec::shortInputRejected()
{
    std::vector<float> syms(RadeTextVectors::kEooFloats, 0.0f);
    radetext::generateTxString("KG4VCF", 6, syms.data(), RadeTextVectors::kEooFloats);
    std::string text;
    QVERIFY(!radetext::decodeRx(syms.data(), radetext::kLdpcTotalSizeBits / 2 - 1, &text));
    // Exactly 56 symbols (no filler) still decodes: upstream's noise
    // estimate then falls back to its floor.
    QVERIFY(radetext::decodeRx(syms.data(), radetext::kLdpcTotalSizeBits / 2, &text));
    QCOMPARE(QString::fromStdString(text), QStringLiteral("KG4VCF"));
}

QTEST_GUILESS_MAIN(TestRadeTextCodec)
#include "tst_rade_text_codec.moc"
