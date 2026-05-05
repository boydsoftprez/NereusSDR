// =================================================================
// src/core/audio/CompositeTxMicRouter.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. No Thetis logic ported here — Thetis bakes
// mic-source selection directly into audio.cs rather than using the
// strategy pattern. CompositeTxMicRouter is a NereusSDR-native selector
// that holds PcMicSource + TxMicSource and dispatches pullSamples
// to the active source at runtime.
//
// Design: docs/architecture/phase3m-1b-thetis-pre-code-review.md §0.3
// (PcMicSource arch lock) + master design §5.2.1.
// Plan: 3M-1b F.3 (initial); radio-mic-input Thetis parity Task 11
// (Radio source switched from RadioMicSource to TxMicSource — the live
//  ring fed by P1 EP6 + P2 port-1026; RadioMicSource was a dead path
//  whose feeder signal was never emitted in production).
//
// This is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-27 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), Phase 3M-1b Task F.3, with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-05-05 — Radio source switched from RadioMicSource to
//                 TxMicSource (radio-mic-input Thetis parity Task 11).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#pragma once

#include "core/TxMicRouter.h"

#include <atomic>

namespace NereusSDR {

class PcMicSource;
class TxMicSource;
class VaxTxMicSource;

/// MicSource — strategy-pattern enum for which mic source is active.
///
/// Pc    — PC microphone (host audio device via PortAudio/PipeWire).
///          Always available. Used by default and forced on HL2.
/// Radio — Radio mic-jack input via P1/P2 mic-frame stream.
///          Requires hasMicJack == true (not available on HL2).
/// Vax   — Audio routed to "NereusSDR TX" virtual device by a 3rd-
///          party app (FreeDV, WSJT-X, etc.).  Pulled from
///          /nereussdr-vax-tx shared memory by the HAL plugin →
///          CoreAudioHalBus → VaxTxMicSource.  Requires the VAX TX
///          mic source to have been registered via setVaxSource()
///          (called by RadioModel after engine start). If the VAX
///          source is null when MicSource::Vax is selected, the
///          router falls through to Pc as a safety default rather
///          than emitting silence.
enum class MicSource : int {
    Pc    = 0,  ///< PC microphone (host audio device)
    Radio = 1,  ///< Radio mic-jack input via P1/P2 mic-frame stream
    Vax   = 2,  ///< VAX TX virtual device (3rd-party app → /nereussdr-vax-tx)
};

/// CompositeTxMicRouter — selector that holds PcMicSource + TxMicSource
/// and dispatches pullSamples based on the user's active selection.
///
/// Special behaviors:
///
/// HL2 force-PC: when hasMicJack is false (HL2 has no radio mic-jack),
/// setActiveSource(Radio) is silently ignored. The effective source
/// is always Pc on HL2 regardless of user selection.
///
/// MOX-locked: source switching while MOX is active is deferred.
/// setActiveSource() during TX records the request as "pending" and
/// applies it on the next setMoxActive(false) transition. This prevents
/// audible clicks and mid-TX mic-buffer state inconsistency.
///
/// Constructor takes non-owning pointers. The router does not own its
/// sources — lifetime is managed by the caller (RadioModel in production).
///
/// hasMicJack is collapsed with txMicSource at construction: if either
/// hasMicJack is false OR txMicSource is nullptr, the router behaves as
/// if Radio is unavailable (force-PC). This means passing a non-null
/// txMicSource with hasMicJack=false is handled identically to null.
///
/// Audio-thread contract: pullSamples() is called from the WDSP audio
/// thread. All state is accessed via std::atomic with acquire/release
/// ordering. No blocking, no allocation.
///
/// Plan: 3M-1b F.3 (initial); radio-mic-input Thetis parity Task 11
/// (RadioMicSource → TxMicSource).
class CompositeTxMicRouter : public TxMicRouter {
public:
    /// Construct with PcMicSource (always present) and optional
    /// TxMicSource (null on HL2 since the radio-mic path doesn't
    /// exist). hasMicJack mirrors BoardCapabilities::hasMicJack.
    ///
    /// If hasMicJack is false OR txMicSource is nullptr, the router
    /// collapses to Pc-only mode (Radio selection silently ignored).
    CompositeTxMicRouter(PcMicSource* pcSource,
                         TxMicSource* txMicSource,
                         bool         hasMicJack);

    ~CompositeTxMicRouter() override = default;

    /// Pull `n` mono float samples into `dst`. Dispatches to the
    /// currently active source. Falls back to zero-fill only if both
    /// sources are null (should not occur in normal operation).
    ///
    /// Called from the WDSP audio thread — lock-free, no allocation.
    int pullSamples(float* dst, int n) override;

    /// User-facing source selection.
    ///
    /// The request is applied immediately when:
    ///   - hasMicJack is true (no HL2 force-PC gate), AND
    ///   - moxActive is false (no MOX-locked deferral)
    ///
    /// If the HL2 gate fires (hasMicJack false AND source == Radio),
    /// the request is silently dropped — it will never be applied.
    ///
    /// If the MOX gate fires, the request is recorded as pending and
    /// applied on the next setMoxActive(false) transition.
    void setActiveSource(MicSource source);

    /// Currently dispatching source (the actual effective source,
    /// which may differ from the last setActiveSource() request if
    /// MOX deferral is in effect).
    MicSource activeSource() const
    {
        return m_activeSource.load(std::memory_order_acquire);
    }

    /// Notify the router of MOX state changes.
    ///
    /// On MOX-off transition (wasActive && !active), any pending switch
    /// recorded during TX is applied to m_activeSource. On MOX-on
    /// transition there is no effect on the active source.
    void setMoxActive(bool active);

    /// Register the VAX TX mic source (additive — the existing
    /// constructor signature is preserved so existing tests and
    /// callers compile unchanged). Pass nullptr to clear.
    ///
    /// VAX is always treated as available when registered: there is
    /// no per-board capability gate analogous to hasMicJack — every
    /// supported macOS / Linux build publishes a "NereusSDR TX"
    /// device and any user can opt in. Windows builds will gain a
    /// VAX TX path once the BYO virtual-cable wiring lands; until
    /// then setVaxSource(nullptr) keeps Vax selection a safe
    /// no-op (falls back to Pc).
    ///
    /// Called from the main thread after RadioModel constructs
    /// VaxTxMicSource. Audio-thread reads the pointer via
    /// std::atomic with acquire ordering so a mid-session register
    /// is safe.
    void setVaxSource(VaxTxMicSource* source);

    /// Currently registered VAX TX source pointer (may be null).
    /// Audio-thread accessor for tests.
    VaxTxMicSource* vaxSource() const
    {
        return m_vaxSource.load(std::memory_order_acquire);
    }

#ifdef NEREUS_BUILD_TESTS
    /// True iff a source switch was deferred because MOX was active
    /// and has not yet been applied. For test inspection only.
    bool hasPendingSwitchForTest() const
    {
        return m_hasPendingSwitch.load(std::memory_order_acquire);
    }
#endif

private:
    PcMicSource* m_pcSource;       // non-owning; always present
    TxMicSource* m_txMicSource;    // non-owning; may be null (HL2 or no jack)
    const bool   m_hasMicJack;     // collapsed: false if !hasMicJack || !txMicSource

    // Audio-thread–accessible state. All accessed with acquire/release.
    std::atomic<MicSource> m_activeSource;     // currently dispatching source
    std::atomic<MicSource> m_pendingSource;    // queued switch (when MOX-locked)
    std::atomic<bool>      m_hasPendingSwitch {false};
    std::atomic<bool>      m_moxActive {false};
    std::atomic<VaxTxMicSource*> m_vaxSource {nullptr};  // non-owning; registered post-ctor
};

} // namespace NereusSDR
