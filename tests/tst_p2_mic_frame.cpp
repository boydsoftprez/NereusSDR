// =================================================================
// tests/tst_p2_mic_frame.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file.  Verifies the static
// P2RadioConnection::decodeMicFrame132 helper used by the port-1026
// dispatch path (Phase 3M-1c TX pump v3).
//
// Test cases (3):
//   - normalFrame_extractsAllSixtyFourSamples
//   - zeroFrame_yieldsAllZeroSamples
//   - wrongSize_returnsFalse
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-29 — New test for Phase 3M-1c TX pump v3 by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.  No Thetis logic ported.

#include <QtTest/QtTest>
#include <QObject>

#include <array>

#include "core/P2RadioConnection.h"

using namespace NereusSDR;

namespace {

// Build a 132-byte P2 mic frame:
//   bytes 0..3 = seq (big-endian)
//   bytes 4..131 = 64 * 2 bytes of int16 BE samples
QByteArray buildMicFrame(quint32 seq, const std::array<int16_t, 64>& samples)
{
    QByteArray buf;
    buf.resize(132);
    auto* d = reinterpret_cast<uint8_t*>(buf.data());
    d[0] = static_cast<uint8_t>((seq >> 24) & 0xFF);
    d[1] = static_cast<uint8_t>((seq >> 16) & 0xFF);
    d[2] = static_cast<uint8_t>((seq >> 8) & 0xFF);
    d[3] = static_cast<uint8_t>(seq & 0xFF);
    for (int s = 0; s < 64; ++s) {
        const uint16_t v = static_cast<uint16_t>(samples[static_cast<size_t>(s)]);
        d[4 + s * 2 + 0] = static_cast<uint8_t>((v >> 8) & 0xFF);
        d[4 + s * 2 + 1] = static_cast<uint8_t>(v & 0xFF);
    }
    return buf;
}

} // namespace

class TestP2MicFrame : public QObject {
    Q_OBJECT

private slots:

    // ── 1. Normal frame → all 64 samples decoded in order ──────────────────
    void normalFrame_extractsAllSixtyFourSamples()
    {
        std::array<int16_t, 64> in{};
        for (int s = 0; s < 64; ++s) {
            in[static_cast<size_t>(s)] = static_cast<int16_t>((s + 1) * 17);  // 17, 34, 51, ...
        }
        QByteArray buf = buildMicFrame(/*seq=*/12345, in);

        std::array<float, 64> out{};
        quint32 seq = 0;
        const bool ok = P2RadioConnection::decodeMicFrame132(buf, out, &seq);
        QVERIFY(ok);
        QCOMPARE(seq, 12345u);

        for (int s = 0; s < 64; ++s) {
            const float expected = static_cast<float>(in[static_cast<size_t>(s)]) / 32768.0f;
            QCOMPARE(out[static_cast<size_t>(s)], expected);
        }
    }

    // ── 2. All-zero frame yields all zero float samples ────────────────────
    void zeroFrame_yieldsAllZeroSamples()
    {
        std::array<int16_t, 64> in{};
        QByteArray buf = buildMicFrame(/*seq=*/0, in);

        std::array<float, 64> out{};
        out.fill(1.0f);
        QVERIFY(P2RadioConnection::decodeMicFrame132(buf, out));
        for (float v : out) {
            QCOMPARE(v, 0.0f);
        }
    }

    // ── 3. Wrong-sized buffer → returns false ──────────────────────────────
    void wrongSize_returnsFalse()
    {
        QByteArray buf(131, '\0');  // off-by-one
        std::array<float, 64> out{};
        QVERIFY(!P2RadioConnection::decodeMicFrame132(buf, out));
    }
};

QTEST_MAIN(TestP2MicFrame)
#include "tst_p2_mic_frame.moc"
