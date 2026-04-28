// =================================================================
// src/core/TxMicRouter.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. Defines the TxMicRouter strategy interface
// and the NullMicSource concrete stub used by the 3M-1a TUNE path.
//
// This is a NereusSDR-native design with no direct Thetis equivalent.
// Thetis bakes mic-source selection directly into audio.cs rather than
// using the strategy pattern. The split here follows the master design
// §5.1.1 (3M-1a components) and §5.2.1 (3M-1b concrete sources).
//
// Concrete implementations deferred to 3M-1b:
//   PcMicSource   — PortAudioBus / PipeWireBus (reuses 3O VAX infra)
//   RadioMicSource — P2 port 1026 / P1 EP2 mic byte zone
// See: docs/architecture/phase3m-tx-epic-master-design.md §5.2
//   (3M-1b — Mic + SSB Voice mic-source plumbing)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-25 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#pragma once

#include <algorithm>  // std::fill_n

namespace NereusSDR {

/// Strategy-pattern interface for TX mic sources.
///
/// Concrete implementations:
///   - NullMicSource   (3M-1a, this file)     — zero-padded samples
///   - PcMicSource     (3M-1b, deferred)       — PortAudioBus / PipeWireBus
///   - RadioMicSource  (3M-1b, deferred)       — radio mic-jack via P1/P2
///
/// 3M-1a context: TxChannel uses NullMicSource as a placeholder so the
/// WDSP TXA rsmpin stage always has a valid input buffer. The gen1
/// PostGen at TXA stage 22 overwrites the zero-padded samples with the
/// TUNE carrier, so the input is functionally unused during TUNE-only TX.
///
/// 3M-1b context: RadioModel selects the source per user preference
/// (Mic Source dropdown in TxApplet) and per BoardCapabilities (HL2
/// forces PcMicSource since it has no radio-side mic jack).
///
/// Audio-thread contract: pullSamples() is called from the WDSP audio
/// thread. Implementations must not block. RadioMicSource (3M-1b) will
/// use a lock-free ring buffer fed by the connection thread.
class TxMicRouter {
public:
    virtual ~TxMicRouter() = default;

    /// Pull `n` mono float samples into `dst`. Returns the number
    /// of samples actually written. For well-behaved sources this
    /// equals `n`; less is permissible only for transient underrun.
    /// Returns 0 if `dst` is null or `n <= 0` (programming-error guard
    /// only; callers providing valid args will never see this path at
    /// runtime).
    ///
    /// Called from the WDSP audio thread — must not block or allocate.
    /// `dst` is guaranteed non-null and large enough to hold `n`
    /// floats when called by TxChannel.
    virtual int pullSamples(float* dst, int n) = 0;
};

/// 3M-1a stub: emits zero-padded samples.
///
/// The TUNE path (gen1 PostGen) overwrites the rsmpin input buffer at
/// TXA stage 22, so the silence produced here is functionally inert
/// during TUNE-only TX. This stub satisfies the WDSP TX-input contract
/// until 3M-1b wires a real mic source.
///
/// HL2 note: HL2 also uses this stub in 3M-1a; 3M-1b forces HL2 to
/// PcMicSource (no radio-side mic jack).
///
/// TODO [3M-1b]: replace NullMicSource with PcMicSource (PortAudioBus /
///   PipeWireBus, reusing 3O VAX infrastructure) or RadioMicSource
///   (P2 port 1026 / P1 EP2 mic byte zone) per user preference.
///
///   Audio-thread constraint: concrete implementations must be
///   lock-free. RadioMicSource will use a ring buffer fed by the
///   connection thread; PcMicSource taps PortAudioBus's existing
///   ring buffer. NO allocations or blocking inside pullSamples().
///
///   HL2 boardcap forces PcMicSource (no radio-side mic jack).
///   See docs/architecture/phase3m-tx-epic-master-design.md §5.2.
class NullMicSource : public TxMicRouter {
public:
    /// Always writes `n` zero floats to `dst` and returns `n`.
    /// Returns 0 for null `dst` or non-positive `n` (defensive).
    int pullSamples(float* dst, int n) override
    {
        if (dst == nullptr || n <= 0) {
            return 0;
        }
        // std::fill_n: type-safe alternative to memset for float buffers
        // (equivalent on IEEE-754 where 0.0f is all-zero bits).
        std::fill_n(dst, n, 0.0f);
        return n;
    }
};

} // namespace NereusSDR
