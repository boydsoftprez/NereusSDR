// =================================================================
// tests/tst_tx_latency_dsp.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It cites Thetis enums.cs,
// radio.cs and WDSP dexp.c only for the behaviour it expects; no upstream
// logic is ported here.
//
// R-IOS-13 (2026-09-27): the DSP side of "no latency the path does not
// need", on real WDSP channels (no radio, no audio device):
//
//   The Setup > DSP filter type reaches WDSP as Thetis sends it. Thetis
//   defines DSPFilterType { Linear_Phase = 0, Low_Latency = 1 }
//   (enums.cs:404-408 [v2.10.3.15]) and sends
//   TXASetMP / RXASetMP(id, Convert.ToBoolean(value)) (radio.cs:2659, 571
//   [v2.10.3.15]), so "Low Latency" is minimum phase (MP 1).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original test for NereusSDR by J.J. Boyd (KG4VCF),
//               R-IOS-13, with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "core/wdsp_api.h"

#include <cmath>
#include <numbers>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kRxId = 0;

void setPhoneOptions(const QString& side, const QString& filterType)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("DspOptionsBufferSizePhone") + side, QStringLiteral("64"));
    s.setValue(QStringLiteral("DspOptionsFilterSizePhone") + side, QStringLiteral("4096"));
    s.setValue(QStringLiteral("DspOptionsFilterTypePhone") + side, filterType);
}

#ifdef HAVE_WDSP
// How long a 1 kHz tone's onset takes through an open TX channel, in ms:
// 500 ms of silence, then the tone at 0.1 with a 2 ms raised-cosine edge,
// 64 frames a block at 48 kHz in, 192 kHz out, USB 100-2900 Hz; the delay
// from the input's to the output envelope's half level.
double txOnsetDelayMs(int channel)
{
    // No analyzer here: the siphon only buffers (WdspEngine points it at the
    // TX display analyzer, which this test does not open).
    TXASetSipMode(channel, 0);
    SetTXAMode(channel, 1);   // USB
    SetTXABandpassFreqs(channel, 100.0, 2900.0);
    SetTXALevelerSt(channel, 0);
    SetChannelState(channel, 1, 0);
    constexpr int kIn = 64;
    constexpr qint64 kOnAt = 24000;
    constexpr int kEdge = 96;
    std::vector<double> in(2 * kIn);
    std::vector<double> out(2 * kIn * 4);
    std::vector<double> envelope;
    double inHalf = -1.0;
    for (int b = 0; b < 900; ++b) {
        for (int i = 0; i < kIn; ++i) {
            const qint64 n = static_cast<qint64>(b) * kIn + i;
            double g = 0.0;
            if (n >= kOnAt + kEdge) {
                g = 1.0;
            } else if (n >= kOnAt) {
                g = 0.5 * (1.0 - std::cos(std::numbers::pi * double(n - kOnAt) / kEdge));
            }
            if (inHalf < 0.0 && g >= 0.5) {
                inHalf = double(n) / 48000.0;
            }
            in[static_cast<size_t>(2 * i)] =
                0.1 * g * std::sin(2.0 * std::numbers::pi * 1000.0 * double(n) / 48000.0);
            in[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
        int error = 0;
        fexchange0(channel, in.data(), out.data(), &error);
        for (int k = 0; k < kIn * 4; ++k) {
            envelope.push_back(std::hypot(out[static_cast<size_t>(2 * k)],
                                          out[static_cast<size_t>(2 * k + 1)]));
        }
    }
    SetChannelState(channel, 0, 0);
    double steady = 0.0;
    for (size_t k = envelope.size() - 19200; k < envelope.size(); ++k) {
        steady += envelope[k];
    }
    steady /= 19200.0;
    for (size_t k = 0; k < envelope.size(); ++k) {
        if (envelope[k] >= 0.5 * steady) {
            return (double(k) / 192000.0 - inHalf) * 1000.0;
        }
    }
    return -1.0;
}
#endif

} // namespace

class TestTxLatencyDsp : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void txFilterTypeChoiceSendsThetisMp();
    void rxFilterTypeChoiceSendsThetisMp();
    void lowLatencyIsTheShorterTxDelay();
    void dexpTimingRunsThroughHoldAndDecay();
};

// Each Setup > DSP choice for TX gives WDSP Thetis's MP value.
void TestTxLatencyDsp::txFilterTypeChoiceSendsThetisMp()
{
#ifndef HAVE_WDSP
    QSKIP("needs WDSP");
#else
    WdspEngine engine;
    engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
    TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId, 64,
                                           WdspEngine::kTxDspBufferSize, 48000,
                                           WdspEngine::kTxDspSampleRate, 192000);
    QVERIFY(tx != nullptr);
    tx->setWdspEngine(&engine);
    // WDSP opens the TX bandpass linear phase (TXA.c create_bandpass mp 0).
    QCOMPARE(tx->bandpassMinimumPhaseForTest(), 0);
    const QList<QPair<QString, int>> choices = {
        {QStringLiteral("Low Latency"), 1},
        {QStringLiteral("Linear Phase"), 0},
        {QStringLiteral("Low Latency"), 1},
    };
    for (const auto& [choice, mp] : choices) {
        setPhoneOptions(QStringLiteral("Tx"), choice);
        tx->onModeChanged(DSPMode::USB);
        QVERIFY2(tx->bandpassMinimumPhaseForTest() == mp,
                 qPrintable(QStringLiteral("%1: MP %2, Thetis sends %3")
                                .arg(choice)
                                .arg(tx->bandpassMinimumPhaseForTest())
                                .arg(mp)));
    }
    engine.destroyTxChannel(WdspEngine::kTxChannelId);
#endif
}

// Each Setup > DSP choice for RX gives WDSP Thetis's MP value.
void TestTxLatencyDsp::rxFilterTypeChoiceSendsThetisMp()
{
#ifndef HAVE_WDSP
    QSKIP("needs WDSP");
#else
    WdspEngine engine;
    engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
    RxChannel* rx = engine.createRxChannel(kRxId, 256, 4096, 48000, 48000, 48000);
    QVERIFY(rx != nullptr);
    rx->setWdspEngine(&engine);
    // WDSP opens the RX notched bandpass linear phase (RXA.c create_nbp mp 0).
    QCOMPARE(rx->bandpassMinimumPhaseForTest(), 0);
    const QList<QPair<QString, int>> choices = {
        {QStringLiteral("Low Latency"), 1},
        {QStringLiteral("Linear Phase"), 0},
        {QStringLiteral("Low Latency"), 1},
    };
    for (const auto& [choice, mp] : choices) {
        setPhoneOptions(QStringLiteral("Rx"), choice);
        rx->onModeChanged(DSPMode::USB);
        QVERIFY2(rx->bandpassMinimumPhaseForTest() == mp,
                 qPrintable(QStringLiteral("%1: MP %2, Thetis sends %3")
                                .arg(choice)
                                .arg(rx->bandpassMinimumPhaseForTest())
                                .arg(mp)));
    }
    engine.destroyRxChannel(kRxId);
#endif
}

// What the TX channel delays the microphone by, per choice, as the Core
// opens it and then applies the Phone TX defaults (64 / 4096 / choice).
// Low Latency is minimum phase and the shorter delay.
void TestTxLatencyDsp::lowLatencyIsTheShorterTxDelay()
{
#ifndef HAVE_WDSP
    QSKIP("needs WDSP");
#else
    double delayMs[2] = {0.0, 0.0};
    const QStringList choices = {QStringLiteral("Low Latency"), QStringLiteral("Linear Phase")};
    for (int c = 0; c < 2; ++c) {
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId, 64,
                                               WdspEngine::kTxDspBufferSize, 48000,
                                               WdspEngine::kTxDspSampleRate, 192000);
        QVERIFY(tx != nullptr);
        tx->setWdspEngine(&engine);
        setPhoneOptions(QStringLiteral("Tx"), choices[c]);
        tx->onModeChanged(DSPMode::USB);
        QCOMPARE(tx->txDspBlockSize(), 64);
        delayMs[c] = txOnsetDelayMs(WdspEngine::kTxChannelId);
        engine.destroyTxChannel(WdspEngine::kTxChannelId);
    }
    qInfo().noquote() << QStringLiteral("TX DSP delay: Low Latency %1 ms, Linear Phase %2 ms")
                             .arg(delayMs[0], 0, 'f', 2).arg(delayMs[1], 0, 'f', 2);
    QVERIFY2(delayMs[0] > 0.0 && delayMs[0] < 20.0, qPrintable(QString::number(delayMs[0])));
    QVERIFY2(delayMs[1] > delayMs[0] + 10.0, qPrintable(QString::number(delayMs[1])));
#endif
}

// R-IOS-13: TxChannel::dexpTimingRunning follows WDSP's DEXP: false while
// the expander is off or closed, true from a word through its hold and
// decay, false again after; the remote microphone buffer splices nothing
// while it is true (Thetis wdsp/dexp.c:312-381 [v2.10.3.15] counts those
// times in the samples it processes).
void TestTxLatencyDsp::dexpTimingRunsThroughHoldAndDecay()
{
#ifndef HAVE_WDSP
    QSKIP("needs WDSP");
#else
    WdspEngine engine;
    engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
    TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId, 64,
                                           WdspEngine::kTxDspBufferSize, 48000,
                                           WdspEngine::kTxDspSampleRate, 192000);
    QVERIFY(tx != nullptr);
    QVERIFY(!tx->dexpTimingRunning());   // DEXP off
    constexpr double kHoldSeconds = 0.25;
    tx->setVoxHangTime(kHoldSeconds);    // SetDEXPHoldTime
    tx->setDexpRun(true);
    std::vector<double> block(2 * 64, 0.0);
    qint64 n = 0;
    const auto pump = [&](float amplitude) {
        for (int i = 0; i < 64; ++i, ++n) {
            block[static_cast<size_t>(2 * i)] = amplitude
                * std::sin(2.0 * std::numbers::pi * 1000.0 * double(n) / 48000.0);
        }
        tx->pumpDexp(block.data());
        return tx->dexpTimingRunning();
    };
    // Closed before the word.
    for (int b = 0; b < 150; ++b) {
        QVERIFY(!pump(0.0f));
    }
    // A word opens it.
    bool opened = false;
    for (int b = 0; b < 150; ++b) {
        opened = pump(0.5f) || opened;
    }
    QVERIFY(opened);
    // Silence: it stays open for at least the hold, then closes.
    int silentBlocksRunning = 0;
    bool closed = false;
    for (int b = 0; b < 1500 && !closed; ++b) {
        if (pump(0.0f)) {
            ++silentBlocksRunning;
        } else {
            closed = true;
        }
    }
    const double runningMs = silentBlocksRunning * 64 / 48.0;
    qInfo().noquote() << QStringLiteral("DEXP timing ran %1 ms into the silence (hold %2 ms)")
                             .arg(runningMs, 0, 'f', 1).arg(kHoldSeconds * 1000.0, 0, 'f', 0);
    QVERIFY(closed);
    QVERIFY2(runningMs >= kHoldSeconds * 1000.0, qPrintable(QString::number(runningMs)));
    QVERIFY2(runningMs <= kHoldSeconds * 1000.0 + 200.0, qPrintable(QString::number(runningMs)));
    // DEXP off again: nothing to hold.
    tx->setDexpRun(false);
    pump(0.0f);
    QVERIFY(!tx->dexpTimingRunning());
    engine.destroyTxChannel(WdspEngine::kTxChannelId);
#endif
}

QTEST_GUILESS_MAIN(TestTxLatencyDsp)
#include "tst_tx_latency_dsp.moc"
