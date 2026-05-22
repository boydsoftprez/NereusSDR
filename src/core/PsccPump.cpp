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

#include <algorithm>
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

void PsccPump::onIqData(int ddcIndex, const QVector<float>& samples)
{
    if (!m_active) {
        return;
    }
    if (samples.isEmpty()) {
        return;
    }

    // Route to the correct ring.  iqDataReceived emits interleaved float
    // I/Q pairs (samples.size() == 2 * sample_count).  Other DDC indices
    // (RX1's audio path) fall through silently.
    QVector<float>* ring = nullptr;
    if (ddcIndex == m_txMonDdc) {
        ring = &m_txMonRing;
    } else if (ddcIndex == m_psFbDdc) {
        ring = &m_psFbRing;
    } else {
        return;
    }

    // Append to ring.  No upper bound — calcc consumes as fast as the
    // network delivers, so the ring should stay shallow.  In a packet-
    // burst scenario the ring might temporarily grow; the next tryPump
    // call drains as many full blocks as possible.
    const int prevSize = ring->size();
    ring->resize(prevSize + samples.size());
    std::copy(samples.constBegin(), samples.constEnd(),
              ring->begin() + prevSize);

    tryPump();
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

        // Diagnostic — once every ~80 blocks (~one second at 192 kHz +
        // 256 block) so the bench can confirm pumping is happening
        // without flooding the log.  Also report TX + RX envelope max
        // so we can see whether calcc has signal energy to work with.
        if ((m_totalBlocksPumped % 80) == 1) {
            double txEnvMax = 0.0, rxEnvMax = 0.0;
            for (int i = 0; i < m_blockSize; ++i) {
                const double txI = tx[2 * i + 0];
                const double txQ = tx[2 * i + 1];
                const double rxI = rx[2 * i + 0];
                const double rxQ = rx[2 * i + 1];
                const double txE = std::sqrt(txI * txI + txQ * txQ);
                const double rxE = std::sqrt(rxI * rxI + rxQ * rxQ);
                if (txE > txEnvMax) txEnvMax = txE;
                if (rxE > rxEnvMax) rxEnvMax = rxE;
            }
            qCInfo(lcDsp).nospace()
                << "PsccPump: pumped " << m_totalBlocksPumped
                << " blocks (channel=" << m_txChannelId
                << " blockSize=" << m_blockSize
                << " txEnvMax=" << txEnvMax
                << " rxEnvMax=" << rxEnvMax << ")";
        }
    }
}

} // namespace NereusSDR
