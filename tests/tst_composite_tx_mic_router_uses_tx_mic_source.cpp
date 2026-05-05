// =================================================================
// tests/tst_composite_tx_mic_router_uses_tx_mic_source.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Verifies that CompositeTxMicRouter::pullSamples routes the
// MicSource::Radio branch to TxMicSource (the live ring fed by P1
// EP6 + P2 port-1026), not to the dead RadioMicSource path.
//
// Plan: 2026-05-05-radio-mic-input-thetis-parity.md Task 11.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-05 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 radio-mic-input Thetis parity port Task 11, with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>

#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/PcMicSource.h"
#include "core/audio/TxMicSource.h"

#include <array>

using namespace NereusSDR;

class TestCompositeTxMicRouterUsesTxMicSource : public QObject {
    Q_OBJECT

private slots:
    // Radio branch with hasMicJack=true must drain from TxMicSource.
    // Pushes a known constant into TxMicSource via inbound() and verifies
    // that pullSamples() through the router returns those samples.
    void radioBranchPullsFromTxMicSource()
    {
        PcMicSource pc(nullptr);
        TxMicSource tx;
        tx.start();

        constexpr int N = TxMicSource::kBlockFrames;
        std::array<float, N> in{};
        for (int i = 0; i < N; ++i) {
            in[i] = 0.5f;
        }
        tx.inbound(in.data(), N);

        CompositeTxMicRouter router(&pc, &tx, /*hasMicJack=*/true);
        router.setActiveSource(MicSource::Radio);

        std::array<float, N> out{};
        for (int i = 0; i < N; ++i) {
            out[i] = -1.0f;
        }
        const int got = router.pullSamples(out.data(), N);
        QCOMPARE(got, N);
        QCOMPARE(out[0], 0.5f);
        QCOMPARE(out[N / 2], 0.5f);
        QCOMPARE(out[N - 1], 0.5f);

        tx.stop();
    }

    // hasMicJack=false collapses Radio selection to PC. Without a real
    // PcMicSource backing engine, pullSamples falls through to PcMicSource
    // which returns 0 (no AudioEngine), and no Radio data is pulled.
    void radioBranchSilentWhenNoMicJack()
    {
        PcMicSource pc(nullptr);
        TxMicSource tx;
        tx.start();

        constexpr int N = TxMicSource::kBlockFrames;
        std::array<float, N> in{};
        for (int i = 0; i < N; ++i) {
            in[i] = 0.5f;
        }
        tx.inbound(in.data(), N);

        // hasMicJack=false: Radio selection silently dropped, router stays on Pc.
        CompositeTxMicRouter router(&pc, &tx, /*hasMicJack=*/false);
        router.setActiveSource(MicSource::Radio);
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Pc));

        // pullSamples flows to PcMicSource. Without an AudioEngine the
        // PcMicSource returns 0 and leaves dst untouched — the key
        // assertion is that no TxMicSource data leaks through.
        std::array<float, N> out{};
        for (int i = 0; i < N; ++i) {
            out[i] = -1.0f;
        }
        router.pullSamples(out.data(), N);
        // The 0.5f from the TxMicSource ring must NOT have been pulled.
        QVERIFY(out[0] != 0.5f);

        tx.stop();
    }
};

QTEST_APPLESS_MAIN(TestCompositeTxMicRouterUsesTxMicSource)
#include "tst_composite_tx_mic_router_uses_tx_mic_source.moc"
