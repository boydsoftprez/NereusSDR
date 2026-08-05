// no-port-check: NereusSDR-original driver class.  See PsccPump.h
// header for the architectural narrative.
//
// =================================================================
// src/core/PsccPump.cpp  (NereusSDR)
// =================================================================
//
// Implementation of the pscc() driver.  See PsccPump.h for the
// upstream cite map and threading model.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Created by J.J. Boyd (KG4VCF) for Phase 3M-4
//                 Task 17 chunk C, with AI-assisted source-first
//                 protocol via Anthropic Claude Code.
// =================================================================

#include "PsccPump.h"

#include "LogCategories.h"
#include "MoxController.h"

#include <QLoggingCategory>

#include <vector>

// pscc() is exported from the WDSP static library (calcc.c:617
// [v2.10.3.13]).  No declaration in our wdsp_api.h yet — declare
// inline here via extern "C".  Signature byte-for-byte:
//   void pscc (int channel, int size, double* tx, double* rx)
extern "C" {
    void pscc(int channel, int size, double* tx, double* rx);
}

namespace NereusSDR {

PsccPump::PsccPump(QObject* parent)
    : QObject(parent)
{
}

PsccPump::~PsccPump() = default;

void PsccPump::setTxChannelId(int channelId)
{
    m_txChannelId = channelId;
}

void PsccPump::setMoxController(MoxController* mox)
{
    m_mox = mox;
}

void PsccPump::setBlockSize(int n)
{
    if (n > 0 && n <= 2048) {
        m_blockSize = n;
    }
}

void PsccPump::setActive(bool active, int txMonDdc, int psFbDdc)
{
    m_active = active;
    m_txMonDdc = txMonDdc;
    m_psFbDdc  = psFbDdc;
    if (!active) {
        // Drop in-flight rings on deactivate so a future activate starts
        // from a clean alignment.  Mirrors Thetis sync.c:38-42 [v2.10.3.13]
        // destroy_sync semantic (free divbuff + destroy_divEXT) — a
        // best-effort reset of paired-stream state.
        m_txMonRing.clear();
        m_psFbRing.clear();
    }
    qCInfo(lcDsp) << "PsccPump: setActive(" << active
                  << ") txMonDdc=" << txMonDdc
                  << "psFbDdc=" << psFbDdc;
}

void PsccPump::onDdcConfigChanged(const PsDdcConfig& cfg)
{
    // From Thetis console.cs:8186-8538 UpdateDDCs [v2.10.3.13] +
    // Upstream tags preserved: //N1GP (from cited console.cs:8388) [v2.10.3.15]
    // P2CodecOrionMkII::applyPureSignalDdcConfig:469-488 [v2.10.3.13 port].
    //MW0LGE   [console.cs:8238 + 8268 inline `// [2.10.3.13]MW0LGE p1 !`
    //          attribution on the P1 rate-fixup `if (p1) Rate[0] = rx1_rate`]
    //DH1KLM   [console.cs:8296 `case HPSDRModel.REDPITAYA: //DH1KLM`
    //          attribution on the RedPitaya-specific PS branch]
    //
    // PureSignal MOX state populates the codec config with:
    //   ddcEnable = DDC0 + DDC2     (bits 0 and 2 set)
    //   syncEnable = DDC1           (bit 1 set)
    //   rate[0] = 192000             (PS rate per cmaster.cs:424)
    //   rate[1] = 192000             (synced DDC at PS rate)
    //   rate[2] = rx1Rate            (RX1 normal)
    //
    // The pump activates when BOTH the PS-feedback DDC (Stream0 per
    // cmaster.cs:533 [v2.10.3.13]) and the TX-monitor DDC (Stream1 per
    // cmaster.cs:534) are enabled by the codec config — this is exactly
    // the (psEnabled && mox) state in P2CodecOrionMkII.cpp:469-488.
    constexpr uint8_t kDdc0Bit = 0x01;   // DDC0 = PS feedback
    constexpr uint8_t kDdc1Bit = 0x02;   // DDC1 = TX monitor (synced)

    const bool ddc0OnEnable  = (cfg.ddcEnable & kDdc0Bit) != 0;
    const bool ddc1OnSync    = (cfg.syncEnable & kDdc1Bit) != 0;
    const bool psRatesPresent = (cfg.rate[0] > 0 && cfg.rate[1] > 0
                                  && cfg.rate[0] == cfg.rate[1]);

    const bool wantActive = ddc0OnEnable && ddc1OnSync && psRatesPresent;

    // Phase 3M-4 mi0bot audit: per-board PS DDC pair indices.
    //
    // From mi0bot networkproto1.c:380-392 [v2.10.3.13-beta2]
    // MetisReadThreadMainLoop dispatch by nddc:
    //   case 2: twist(spr, 0, 1, 0)        // HermesII / ANAN-10E / 100B
    //   case 4: twist(spr, 2, 3, 1)        // Hermes / HL2 / ANAN-10 / 100
    //   case 5: twist(spr, 3, 4, 1)        // Orion-class P1 (rare)
    //
    // For P2 boards the network.c:936-945 freq override forces DDC0+DDC1
    // to TX freq — pscc pair is DDC0+DDC1 universally on P2.
    //
    // The per-board codec encodes the correct indices in
    // PsDdcConfig.psFbDdc / .txMonDdc; if neither has been set (e.g.
    // codec hasn't been wired yet, or fallback pre-PS state), we use the
    // cmaster.cs:533-534 [v2.10.3.13] default of (0, 1).
    //
    // Since 2026-08-01 the unset state is the struct's own default: both
    // fields start at -1 rather than at (0, 1), so this fallback is now the
    // only thing that supplies Stream0/Stream1 for a config that satisfies
    // wantActive without naming a pair (the diversity branches do, since
    // they also run DDC0 with DDC1 synced at a matching rate). Keeping it
    // leaves that case behaving exactly as it did before the sentinel. Such
    // a pump is inert either way: the connection layer emits the paired
    // signal only for a codec-configured pair, so nothing ever reaches
    // onPsPairedIqData to be misread as PS feedback.
    const int newPsFbDdc  = (cfg.psFbDdc  >= 0) ? cfg.psFbDdc  : 0;
    const int newTxMonDdc = (cfg.txMonDdc >= 0) ? cfg.txMonDdc : 1;

    // Re-arm the pump if active state changes OR if the DDC indices change
    // mid-session (e.g. a board switches PS modes — unusual but cheap to
    // handle).  setActive(false, ...) drops the rings; setActive(true, ...)
    // resets them.
    if (wantActive != m_active
        || newPsFbDdc != m_psFbDdc
        || newTxMonDdc != m_txMonDdc) {
        setActive(wantActive, newTxMonDdc, newPsFbDdc);
    }
}

void PsccPump::onIqData(int /*ddcIndex*/, const QVector<float>& /*samples*/)
{
    // Phase 3M-4 bench-fix 2026-05-23 (J.J. Boyd KG4VCF) — transitional
    // no-op.  The legacy independent-rings architecture (m_txMonRing +
    // m_psFbRing drained by tryPump) was replaced by onPsPairedIqData
    // to honor Thetis sync.c InboundBlock(id=1)'s same-packet pairing
    // invariant.  Keeping this slot stub-only avoids breaking the
    // RadioModel::iqDataReceived wiring while the connection layer
    // migrates to RadioConnection::psPairedIqDataReceived.  Drop the
    // slot entirely once that migration is complete.
}

void PsccPump::onPsPairedIqData(int psFbDdc, const QVector<float>& psFbSamples,
                                int txMonDdc, const QVector<float>& txMonSamples)
{
    // From Thetis ChannelMaster/sync.c:53-58 InboundBlock(id=1)
    // [v2.10.3.15]:
    //   pscc (chid (inid (1, 0), 0),
    //         nsamples,
    //         data[_InterlockedAnd (&psyn->xmtr[0].ps_tx_idx, 0xffffffff)],
    //         data[_InterlockedAnd (&psyn->xmtr[0].ps_rx_idx, 0xffffffff)]);
    //
    // Both `data[ps_tx_idx]` and `data[ps_rx_idx]` are pointers into
    // per-stream buffers populated by the SAME xrouter() call (see
    // router.c:91-102 case 2 [v2.10.3.15]).  This slot's contract
    // mirrors that: psFbSamples + txMonSamples are de-interleaved
    // from the same packet, so cross-stream sample alignment is
    // guaranteed by construction.  No ring buffers, no drain loop.

    if (!m_active) {
        return;
    }

    // From cmaster.cs:533-534 [v2.10.3.13] SetPSRxIdx(0,0) + SetPSTxIdx(0,1):
    // the codec layer is responsible for emitting on the configured PS
    // pair.  Refuse the call if the connection emitted on a different
    // pair (defensive guard against future connection-layer regressions).
    if (psFbDdc != m_psFbDdc || txMonDdc != m_txMonDdc) {
        return;
    }

    // Same-packet invariant: both per-stream buffers come from the same
    // deinterleave pass and must have identical sample counts.  A
    // mismatch indicates a bug upstream — drop and let the next packet
    // through clean.
    if (psFbSamples.size() != txMonSamples.size()) {
        return;
    }
    if (psFbSamples.isEmpty()) {
        return;
    }

    // QVector<float>::size() counts floats; interleaved I/Q means each
    // sample is two floats.  sps = per-stream complex-sample count.
    const int interleavedFloats = psFbSamples.size();
    const int sps = interleavedFloats / 2;

    // Build interleaved double buffers (pscc takes double*).  The
    // float→double conversion is the same that psccF performs internally
    // (calcc.c:849-855 [v2.10.3.13]); calling pscc directly skips that
    // wrapper layer.
    std::vector<double> tx(interleavedFloats);
    std::vector<double> rx(interleavedFloats);
    for (int j = 0; j < interleavedFloats; ++j) {
        // TX-monitor input → pscc tx* (Thetis ps_tx_idx=1, cmaster.cs:534).
        tx[j] = static_cast<double>(txMonSamples[j]);
        // PS-feedback input → pscc rx* (Thetis ps_rx_idx=0, cmaster.cs:533).
        rx[j] = static_cast<double>(psFbSamples[j]);
    }

#ifdef NEREUS_BUILD_TESTS
    if (m_skipPsccForTests) {
        m_lastPsccArgs.channel   = m_txChannelId;
        m_lastPsccArgs.size      = sps;
        m_lastPsccArgs.tx        = tx;
        m_lastPsccArgs.rx        = rx;
        m_lastPsccArgs.callCount += 1;
    } else
#endif
    {
        // pscc internally locks calcc.cs_update (calcc.c:621 [v2.10.3.13]),
        // so this is safe to invoke from any thread that owns no calcc lock.
        pscc(m_txChannelId, sps, tx.data(), rx.data());
    }

    ++m_totalBlocksPumped;
}

void PsccPump::tryPump()
{
    // From Thetis ChannelMaster/sync.c:53-58 InboundBlock(id=1)
    // [v2.10.3.13]:
    //   pscc(chid(inid(1, 0), 0),
    //        nsamples,
    //        data[ps_tx_idx],     // index 1 per cmaster.cs:534
    //        data[ps_rx_idx]);    // index 0 per cmaster.cs:533
    //
    // ChannelMaster's xrouter pre-builds paired pointers; NereusSDR's
    // independent UDP-per-DDC path means we have to pair them here.
    const int needed = m_blockSize * 2;   // interleaved I/Q

    while (m_txMonRing.size() >= needed && m_psFbRing.size() >= needed) {
        // Build interleaved double buffers (pscc takes double*):
        //   tx[2i+0] = I_tx, tx[2i+1] = Q_tx  for i in [0, blockSize)
        //   rx[2i+0] = I_rx, rx[2i+1] = Q_rx  for i in [0, blockSize)
        // The float→double conversion is the same that psccF performs
        // internally (calcc.c:849-855 [v2.10.3.13]).  Calling pscc
        // directly skips one wrapper layer.
        std::vector<double> tx(needed);
        std::vector<double> rx(needed);
        for (int j = 0; j < needed; ++j) {
            tx[j] = static_cast<double>(m_txMonRing[j]);
            rx[j] = static_cast<double>(m_psFbRing[j]);
        }

        // Drain N samples from each ring.
        m_txMonRing.remove(0, needed);
        m_psFbRing.remove(0, needed);

        // Call pscc.  WDSP locks calcc.cs_update internally, so this
        // is safe to invoke from any thread that owns no calcc lock.
        pscc(m_txChannelId, m_blockSize, tx.data(), rx.data());
        ++m_totalBlocksPumped;

        // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): TX↔RX correlation
        // diagnostic — proves/disproves the time-alignment hypothesis.
        //
        // calcc fits a polynomial through (env_TX[i], env_RX[i]) pairs.
        // The AmpView bow-tie X pattern (vs Thetis's clean curve at same
        // settings) suggests env_TX and env_RX may be misaligned in time,
        // producing a Lissajous instead of a function.  Dump per-block
        // envelope at 4 indices + cross-correlation lag estimate.
        //
        // Two-tone envelope period at f1=700, f2=1900: beat=600 Hz =>
        // 320 samples at 192 kHz.  Sampling at i=0,64,128,192 within a
        // 256-sample block gives 4 points across roughly 80% of one
        // envelope cycle.  Expected (good) behaviour: env_TX and env_RX
        // both rise and fall together; ratio env_RX/env_TX is roughly
        // constant.  Bad behaviour (time skew): env_TX peak at i where
        // env_RX is at trough or vice-versa.
        //
        // Cross-correlation lag: scan offset k in [-8, +8] samples;
        // find k that maximises sum(env_TX[i] * env_RX[i+k]).  Non-zero
        // lag means the rings are out of sync — fix is in deinterleave
        // order or PsccPump ring draining.
        //
        // Fires every 80 blocks (~1 Hz at 192 kHz / 256 block size) to
        // avoid log flood while still giving live cadence.  Remove once
        // PS lock is achieved + verified.
        if ((m_totalBlocksPumped % 80) == 1) {
            // Per-index envelope (4 points across block).
            const int idx[4] = {0, 64, 128, 192};
            double envTx[4], envRx[4];
            for (int n = 0; n < 4; ++n) {
                const int i = idx[n];
                const double txI = tx[2 * i + 0], txQ = tx[2 * i + 1];
                const double rxI = rx[2 * i + 0], rxQ = rx[2 * i + 1];
                envTx[n] = std::sqrt(txI * txI + txQ * txQ);
                envRx[n] = std::sqrt(rxI * rxI + rxQ * rxQ);
            }

            // Block-wide stats.
            double txEnvMax = 0.0, rxEnvMax = 0.0;
            double txEnvSum = 0.0, rxEnvSum = 0.0;
            for (int i = 0; i < m_blockSize; ++i) {
                const double txE = std::sqrt(tx[2*i+0]*tx[2*i+0] + tx[2*i+1]*tx[2*i+1]);
                const double rxE = std::sqrt(rx[2*i+0]*rx[2*i+0] + rx[2*i+1]*rx[2*i+1]);
                if (txE > txEnvMax) txEnvMax = txE;
                if (rxE > rxEnvMax) rxEnvMax = rxE;
                txEnvSum += txE; rxEnvSum += rxE;
            }
            const double txEnvAvg = txEnvSum / m_blockSize;
            const double rxEnvAvg = rxEnvSum / m_blockSize;

            // Cross-correlation lag: find best alignment k for which
            // env_TX[i] correlates with env_RX[i+k].  Build envelope
            // arrays once, then scan lags.
            std::vector<double> eTx(m_blockSize), eRx(m_blockSize);
            for (int i = 0; i < m_blockSize; ++i) {
                eTx[i] = std::sqrt(tx[2*i+0]*tx[2*i+0] + tx[2*i+1]*tx[2*i+1]);
                eRx[i] = std::sqrt(rx[2*i+0]*rx[2*i+0] + rx[2*i+1]*rx[2*i+1]);
            }
            // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): initial ±8 search
            // returned bestLag=+8 on every block (hit max), so widen to
            // ±200 to capture envelope-period-scale lags.  Two-tone beat
            // period at 192kHz with default freqs is 320 samples — a lag
            // in [0, 320] is the most likely range.
            int bestLag = 0;
            double bestCorr = -1e30;
            for (int k = -200; k <= 200; ++k) {
                double s = 0.0;
                int n = 0;
                for (int i = 0; i < m_blockSize; ++i) {
                    const int j = i + k;
                    if (j < 0 || j >= m_blockSize) { continue; }
                    s += eTx[i] * eRx[j];
                    ++n;
                }
                if (n > 0 && s / n > bestCorr) {
                    bestCorr = s / n;
                    bestLag = k;
                }
            }

            qCInfo(lcDsp).nospace()
                << "PsccPump-corr: pumped=" << m_totalBlocksPumped
                << " bs=" << m_blockSize
                << " txAvg=" << txEnvAvg << " txMax=" << txEnvMax
                << " rxAvg=" << rxEnvAvg << " rxMax=" << rxEnvMax
                << " ratio_avg=" << (txEnvAvg > 1e-9 ? rxEnvAvg / txEnvAvg : 0.0)
                << " bestLag=" << bestLag << " samples"
                << "  envTX[0,64,128,192]=" << envTx[0] << "," << envTx[1] << "," << envTx[2] << "," << envTx[3]
                << "  envRX[0,64,128,192]=" << envRx[0] << "," << envRx[1] << "," << envRx[2] << "," << envRx[3];
        }
    }
}

} // namespace NereusSDR
