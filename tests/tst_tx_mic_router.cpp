// =================================================================
// tests/tst_tx_mic_router.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis port at this layer.
//
// Verifies the TxMicRouter strategy interface and NullMicSource
// concrete stub added in Phase 3M-1a Task D.1:
//   - NullMicSource emits zero-padded samples for typical buffer sizes
//   - pullSamples(nullptr, n) returns 0 (defensive null check)
//   - pullSamples(dst, 0) returns 0 (zero-size request)
//   - pullSamples(dst, -1) returns 0 (negative size — defensive)
//   - pullSamples(dst, 1) writes a single zero (degenerate case)
//   - Destination buffer is fully zeroed (not just touched)
//   - Repeated calls produce consistent output
//   - NullMicSource is usable via TxMicRouter pointer (vtable correct)
//
// Design ref: docs/architecture/phase3m-tx-epic-master-design.md §5.1.1
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-25 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include "core/TxMicRouter.h"

using namespace NereusSDR;

class TestTxMicRouter : public QObject {
    Q_OBJECT

private slots:

    // NullMicSource returns n and fills buffer with zeros for n=64.
    void nullSource_typical64()
    {
        NullMicSource src;
        float buf[64];
        buf[0] = 1.0f; // sentinel — must be overwritten

        const int written = src.pullSamples(buf, 64);
        QCOMPARE(written, 64);
        for (int i = 0; i < 64; ++i) {
            QCOMPARE(buf[i], 0.0f);
        }
    }

    // NullMicSource returns n and fills buffer with zeros for n=128.
    void nullSource_typical128()
    {
        NullMicSource src;
        float buf[128];
        std::fill(std::begin(buf), std::end(buf), 9.9f); // sentinel

        const int written = src.pullSamples(buf, 128);
        QCOMPARE(written, 128);
        for (int i = 0; i < 128; ++i) {
            QCOMPARE(buf[i], 0.0f);
        }
    }

    // NullMicSource returns n and fills buffer with zeros for n=256.
    void nullSource_typical256()
    {
        NullMicSource src;
        float buf[256];
        std::fill(std::begin(buf), std::end(buf), -1.0f); // sentinel

        const int written = src.pullSamples(buf, 256);
        QCOMPARE(written, 256);
        for (int i = 0; i < 256; ++i) {
            QCOMPARE(buf[i], 0.0f);
        }
    }

    // NullMicSource returns n and fills buffer with zeros for n=1024.
    void nullSource_typical1024()
    {
        NullMicSource src;
        float buf[1024];
        std::fill(std::begin(buf), std::end(buf), 42.0f); // sentinel

        const int written = src.pullSamples(buf, 1024);
        QCOMPARE(written, 1024);
        for (int i = 0; i < 1024; ++i) {
            QCOMPARE(buf[i], 0.0f);
        }
    }

    // Defensive: pullSamples(nullptr, 64) returns 0 — no crash.
    void nullSource_nullDst_returnsZero()
    {
        NullMicSource src;
        const int written = src.pullSamples(nullptr, 64);
        QCOMPARE(written, 0);
    }

    // Defensive: pullSamples(dst, 0) returns 0.
    void nullSource_zeroSize_returnsZero()
    {
        NullMicSource src;
        float buf[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        const int written = src.pullSamples(buf, 0);
        QCOMPARE(written, 0);
        // Buffer must be untouched.
        QCOMPARE(buf[0], 1.0f);
        QCOMPARE(buf[1], 2.0f);
    }

    // Defensive: pullSamples(dst, -1) returns 0.
    void nullSource_negativeSize_returnsZero()
    {
        NullMicSource src;
        float buf[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        const int written = src.pullSamples(buf, -1);
        QCOMPARE(written, 0);
        // Buffer must be untouched.
        QCOMPARE(buf[0], 1.0f);
    }

    // Degenerate: pullSamples(dst, 1) writes exactly one zero.
    void nullSource_singleSample()
    {
        NullMicSource src;
        float buf[2] = {7.0f, 7.0f};
        const int written = src.pullSamples(buf, 1);
        QCOMPARE(written, 1);
        QCOMPARE(buf[0], 0.0f);
        // Second element must be untouched.
        QCOMPARE(buf[1], 7.0f);
    }

    // Destination buffer is fully zeroed, not just partially touched.
    // Uses a buffer pre-filled with non-zero sentinel values and checks
    // every byte to confirm fill_n covered the full range.
    void nullSource_fullBufferZeroed()
    {
        NullMicSource src;
        constexpr int kN = 512;
        float buf[kN];
        std::fill(std::begin(buf), std::end(buf), 3.14f); // sentinel

        const int written = src.pullSamples(buf, kN);
        QCOMPARE(written, kN);

        bool allZero = true;
        for (int i = 0; i < kN; ++i) {
            if (buf[i] != 0.0f) {
                allZero = false;
                break;
            }
        }
        QVERIFY2(allZero, "Every element in the output buffer must be 0.0f");
    }

    // Repeated calls produce consistent output — NullMicSource is
    // stateless and returns zeros every time.
    void nullSource_repeatedCallsConsistent()
    {
        NullMicSource src;
        float buf[64];

        for (int call = 0; call < 8; ++call) {
            std::fill(std::begin(buf), std::end(buf), static_cast<float>(call + 1));
            const int written = src.pullSamples(buf, 64);
            QCOMPARE(written, 64);
            for (int i = 0; i < 64; ++i) {
                QCOMPARE(buf[i], 0.0f);
            }
        }
    }

    // Verify interface polymorphism: NullMicSource is usable via
    // TxMicRouter* pointer (virtual dispatch works correctly).
    void nullSource_viaBasePointer()
    {
        NullMicSource concrete;
        TxMicRouter* router = &concrete;

        float buf[256];
        std::fill(std::begin(buf), std::end(buf), -99.0f); // sentinel

        const int written = router->pullSamples(buf, 256);
        QCOMPARE(written, 256);
        for (int i = 0; i < 256; ++i) {
            QCOMPARE(buf[i], 0.0f);
        }
    }

    // Nullptr dst through base pointer also returns 0 (vtable + defensive
    // check both reachable via polymorphic call).
    void nullSource_viaBasePointer_nullDst()
    {
        NullMicSource concrete;
        TxMicRouter* router = &concrete;
        const int written = router->pullSamples(nullptr, 64);
        QCOMPARE(written, 0);
    }
};

QTEST_APPLESS_MAIN(TestTxMicRouter)
#include "tst_tx_mic_router.moc"
