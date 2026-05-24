// no-port-check: NereusSDR-original driver class.  Thetis pumps pscc()
// from inside ChannelMaster.dll's xrouter → InboundBlock(id=1) chain
// (Project Files/Source/ChannelMaster/router.c:71-108 +
// sync.c:44-67 [v2.10.3.15]).  ChannelMaster receives multi-stream
// packets, deinterleaves all streams into per-stream double-pointer
// arrays in the SAME call, and immediately calls pscc with both
// pointers from that same call.
//
// Phase 3M-4 bench-fix 2026-05-23 (J.J. Boyd KG4VCF) — source-first
// rewrite to honor that invariant:
//
//   * onPsPairedIqData is the new entry point.  It takes both streams
//     as paired buffers FROM THE SAME PACKET and calls pscc once
//     per packet.  Mirrors sync.c:53-58 [v2.10.3.15] InboundBlock
//     (id=1) where `data[ps_tx_idx]` and `data[ps_rx_idx]` both
//     point into per-stream buffers populated by the same xrouter()
//     call (router.c:91-102 case 2 [v2.10.3.15]).
//
//   * onIqData (legacy) buffered per-DDC float-interleaved I/Q in two
//     independent rings and drained when both reached kBlockSize.
//     That architecture introduced cross-stream drift whenever Qt's
//     queued-connection scheduler delivered the two per-DDC signals
//     out-of-step — bench-measured on ANAN-G2E as a 189-sample /
//     985 us lag, which made calcc fit a Lissajous instead of a
//     function and produced the AmpView bow-tie pattern seen on
//     2026-05-23.  Retained as a transitional no-op while the
//     connection layer migrates to the paired signal.
//
// =================================================================
// src/core/PsccPump.h  (NereusSDR)
// =================================================================
//
// PsccPump — the missing pscc() driver for PureSignal.
//
// Background
// ----------
// The WDSP calcc engine (third_party/wdsp/src/calcc.c [v2.10.3.13])
// runs its corrections-being-applied / cal-attempts state machine
// inside `pscc(channel, size, tx, rx)` (calcc.c:617).  pscc reads
// paired TX-monitor + PS-feedback I/Q blocks, evaluates the calcc
// state, and populates info[16] (which downstream code consumes
// via GetPSInfo).
//
// In Thetis, ChannelMaster.dll's xrouter dispatches multi-stream
// packets and demultiplexes into per-stream pointers, then sync.c's
// InboundBlock(id=1) calls pscc with `data[ps_tx_idx]` and
// `data[ps_rx_idx]` selected from the array.  cmaster.cs:533-534
// configures `ps_rx_idx=0, ps_tx_idx=1` for "all current models".
//
// In NereusSDR, the OpenHPSDR P2 network layer delivers each DDC's
// I/Q as a separate stream (RadioConnection::iqDataReceived(ddcIndex,
// samples)) — one UDP port per DDC.  PsccPump subscribes to that
// stream, buffers DDC0 (PS-feedback per cmaster.cs convention) and
// DDC1 (TX-monitor) independently, and calls pscc when both rings
// have a paired block ready.
//
// Without this driver, the WDSP calcc engine never receives any
// data and `GetPSInfo` returns all zeros — every PsForm Calibration
// Information field, the bottom-banner FB number, the IMD overlay
// peak detection, and the GetPk readout are blocked on info[]
// becoming non-zero.  See docs/architecture/phase3m-4-handoff-bench-debug.md
// "Round 2 status".
//
// Source-first cite map
// ---------------------
//   /Users/j.j.boyd/Thetis/Project Files/Source/ChannelMaster/sync.c:44-67
//     [v2.10.3.13] — InboundBlock(id=1) puresignal case
//   /Users/j.j.boyd/Thetis/Project Files/Source/ChannelMaster/router.c:71-108
//     [v2.10.3.13] — xrouter de-interleave + InboundBlock dispatch
//   /Users/j.j.boyd/Thetis/Project Files/Source/Console/cmaster.cs:533-534
//     [v2.10.3.13] — SetPSRxIdx(0,0) + SetPSTxIdx(0,1) for all current models
//   /Users/j.j.boyd/Thetis/Project Files/Source/wdsp/calcc.c:617-837
//     [v2.10.3.13] — pscc() public entry + state machine
//
// Thread placement
// ----------------
// PsccPump runs on the main thread (constructed by RadioModel,
// receives auto-queued iqDataReceived signals from the connection
// thread).  pscc internally takes a critical section
// (txa[channel].calcc.cs_update at calcc.c:621), so thread safety
// is enforced by WDSP.  Per-block work is sample copy + state
// machine — trivially cheap on main thread.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Created by J.J. Boyd (KG4VCF) for Phase 3M-4
//                 Task 17 chunk C — closed the calcc-feed gap that
//                 prevented PureSignal from working end-to-end.  AI-
//                 assisted source-first protocol via Anthropic
//                 Claude Code.
// =================================================================

#pragma once

#include <QObject>
#include <QVector>

#include <vector>

#include "codec/CodecContext.h"   // PsDdcConfig

namespace NereusSDR {

class MoxController;

class PsccPump : public QObject {
    Q_OBJECT

public:
    explicit PsccPump(QObject* parent = nullptr);
    ~PsccPump() override;

    // WDSP TX channel id for psccF.  Mirrors Thetis sync.c:54-55
    // [v2.10.3.13]: `chid(inid(1, 0), 0)` — typically 1 for the
    // primary TX channel.  Defaults to 1; setter exists for tests
    // and future multi-TX support.
    void setTxChannelId(int channelId);

    // Bind a MoxController so onIqData can derive the mox + solidmox
    // flags for psccF.  Optional — without it the pump assumes
    // mox=true while active (calcc clamps internally via SetPSMox).
    void setMoxController(MoxController* mox);

    // Block size for psccF calls.  P2 packets carry spp=238 samples;
    // the pump accumulates to kBlockSize and drains in one call.
    // Default 256 — the WDSP calcc temptx/temprx scratch buffers
    // hold 2048 complex samples per calcc.c:185 [v2.10.3.13], so any
    // size <= 2048 is safe.
    void setBlockSize(int n);

    // ── Test seam ────────────────────────────────────────────────────
    int  txChannelId()        const { return m_txChannelId; }
    int  blockSize()          const { return m_blockSize; }
    bool isActive()           const { return m_active; }
    int  txMonDdc()           const { return m_txMonDdc; }
    int  psFbDdc()            const { return m_psFbDdc; }
    qint64 totalBlocksPumped() const { return m_totalBlocksPumped; }

#ifdef NEREUS_BUILD_TESTS
    // ── Paired-call test seam (NEREUS_BUILD_TESTS only) ──────────────
    //
    // Captures the args that the production code WOULD have passed to
    // extern pscc() so tests can verify alignment without dragging in a
    // live WDSP TX channel.  When setSkipPsccForTests(true) is set, the
    // pump skips the actual pscc invocation but still does all sample
    // conversion + arg capture.  callCount lets tests pin the "exactly
    // one pscc call per packet" contract from sync.c InboundBlock(id=1).
    struct LastPsccArgs {
        int channel{-1};
        int size{0};                  // per-stream sample count (sps)
        std::vector<double> tx;       // interleaved I/Q, size = 2 * sps
        std::vector<double> rx;       // interleaved I/Q, size = 2 * sps
        qint64 callCount{0};
    };

    void setSkipPsccForTests(bool skip) { m_skipPsccForTests = skip; }
    const LastPsccArgs& lastPsccArgsForTests() const { return m_lastPsccArgs; }
#endif

public slots:
    // ── Phase 3M-4 bench-fix 2026-05-23 source-first slot ─────────────────
    //
    // Connect RadioConnection::psPairedIqDataReceived here.  Both buffers
    // come from the SAME packet (P2 multi-stream deinterleave at
    // P2RadioConnection.cpp:2469-2484 / P1 EP6 deinterleave at
    // P1RadioConnection.cpp:2899-2910), so cross-stream sample alignment
    // is guaranteed by construction.
    //
    // Mirrors Thetis sync.c:53-58 [v2.10.3.15] InboundBlock(id=1):
    //   pscc (chid (inid (1, 0), 0), nsamples,
    //         data[ps_tx_idx],      // → pscc tx*  = TX-monitor stream
    //         data[ps_rx_idx]);     // → pscc rx*  = PS-feedback stream
    // with cmaster.cs:533-534 [v2.10.3.13]: ps_rx_idx=0 = PS-FB,
    //                                       ps_tx_idx=1 = TX-mon.
    //
    // Drops the call (without buffering) when:
    //   * !m_active                                — PS not engaged
    //   * (psFbDdc, txMonDdc) ≠ (m_psFbDdc, m_txMonDdc)  — DDC mismatch
    //   * psFbSamples.size() ≠ txMonSamples.size()       — same-packet
    //                                                      invariant broken
    //   * psFbSamples.isEmpty()                          — degenerate
    void onPsPairedIqData(int psFbDdc, const QVector<float>& psFbSamples,
                          int txMonDdc, const QVector<float>& txMonSamples);

    // ── Legacy per-DDC slot (deprecated; transitional no-op) ──────────────
    //
    // Was originally connected to RadioConnection::iqDataReceived and
    // routed each block into m_txMonRing or m_psFbRing for later draining
    // by tryPump().  Replaced by onPsPairedIqData above on 2026-05-23 to
    // honor sync.c InboundBlock(id=1)'s same-packet pairing invariant.
    //
    // Retained as a no-op so existing RadioModel wiring keeps compiling
    // during the migration window; the old rings are no longer drained.
    // Slated for removal once every RadioConnection subclass emits the
    // paired signal.  See PsccPump.h header comment for the architectural
    // narrative.
    void onIqData(int ddcIndex, const QVector<float>& samples);

    // Connect ReceiverManager::ddcConfigChanged here so the pump
    // tracks which DDC is which.  When the codec returns a config
    // with both PS streams enabled (ddcEnable+syncEnable bits both
    // set + ps rate on those DDCs), activate the pump and identify
    // the indices.  When PS gates close, deactivate.
    void onDdcConfigChanged(const NereusSDR::PsDdcConfig& cfg);

    // Manual activation hook for tests + the off-band production
    // case where ReceiverManager isn't available (unit tests).
    // Caller specifies which DDC index carries TX-monitor and
    // which carries PS-feedback (per Thetis cmaster.cs:533-534
    // [v2.10.3.13]: psFbDdc=0, txMonDdc=1 for all current models).
    void setActive(bool active, int txMonDdc, int psFbDdc);

private:
    // Drains kBlockSize samples from each ring (when both have ≥
    // kBlockSize available), de-interleaves into double tx/rx
    // buffers, and calls pscc().  Called after every onIqData ring
    // append.  Mirrors the InboundBlock(id=1) call pattern at
    // sync.c:53-58 [v2.10.3.13].
    void tryPump();

    // From Thetis calcc.c:902-911 [v2.10.3.13] SetPSMox + the solidmox
    // book-keeping: solidmox flips true ~10 packet cycles after MOX
    // becomes true (not used by pscc directly — calcc internally
    // tracks solidmox via the LMOXDELAY → LSETUP transition based on
    // moxsamps count — but the psccF wrapper takes the flag for the
    // disabled-since-2.9 codepath at calcc.c:846-847).
    int m_txChannelId{1};
    bool m_active{false};
    int m_txMonDdc{1};   // Thetis cmaster.cs:534 [v2.10.3.13]: Stream1 = TX
    int m_psFbDdc{0};    // Thetis cmaster.cs:533 [v2.10.3.13]: Stream0 = RX
    int m_blockSize{256};

    // Legacy independent-ring buffers (deprecated 2026-05-23 — see header
    // comment).  Kept declared but no longer written; the old onIqData
    // slot is a transitional no-op and will be removed once every
    // RadioConnection subclass emits the source-first paired signal.
    QVector<float> m_txMonRing;  // interleaved I/Q (size = 2 * samples)
    QVector<float> m_psFbRing;   // interleaved I/Q

    MoxController* m_mox{nullptr};

    qint64 m_totalBlocksPumped{0};

#ifdef NEREUS_BUILD_TESTS
    bool m_skipPsccForTests{false};
    LastPsccArgs m_lastPsccArgs;
#endif
};

} // namespace NereusSDR
