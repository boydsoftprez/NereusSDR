# FilterDisplay mini-spectrum integration

The FilterDisplay is a 1024-pixel spectrum with an independent RF window.
Thetis `MeterManager.cs:43361-43363,43934-43998,44463-44477`
(`3759d096`) uses 30 frames/s, a 20 kHz RX window (twice its default
`MaxFilterWidth=10000` in `console.cs:13221-13227`), and a 40 kHz TX window
(twice `TX_BANDWIDTH=20000`). Nereus currently exposes no adjustable
MaxFilterWidth, so both local and remote paths use the cited default. Thetis
`MeterManager.cs:44118-44143` copies its RX analyzer settings into MiniSpec
even during MOX; `:5252-5265` and `:43659-43675` key the mini only for its
own transmitting receiver. The seven waterfall palette cases are in
`MeterManager.cs:34321-35100`; each incoming waterfall row changes history,
whereas a paint only reads that history.

One `MainWindow::MiniProducer` is keyed by stable slice ID, with a list of
visible `FilterDisplayItem` consumers. Two containers selecting A share its
single producer; A and B on one DDC still have separate reducers, contexts,
and histories. Reconciliation removes a producer and clears its items when
the last visible consumer leaves, including a hidden floating parent, a
hidden meter item, an applet panel header or body meter, removed content, or
a slice rebinding. Local frames crop
the owning stream's full FFT through `SpectrumReducer`; their linear-power
input is calibrated to dBm by the reducer before painting. Pan zoom and the
active pan do not set the mini's crop.

A remote mini uses the same endpoint namespace and budget as pans. It has a
distinct `displayRole=mini` only after `miniDisplayVersion` negotiation, its
own monotonic endpoint and revision, and an allocator intent below pan
priority. The eight-endpoint cap covers both kinds; suspension releases the
Core demand and clears the item. A received context is accepted only for its
current slice, stream epoch, request, TX mode, source, and generation. Core's
accepted RF centre/span are checked against the exact inclusive FFT-bin crop
from `SpectrumReducer::visibleBinRange`; frame presentation requires that
context and its accepted sample count. Retirement and reconnect clear the
display. The Core supplies a per-slice high-resolution FIR response to a
remote mini, with demand serials guarding direct-listener reentrancy.

The TX mini has a separate WDSP analyzer at display ID 6 (RX IDs 0-3, the
primary TX at 5; WDSP maximum 72). This is attached through the existing
`TXASetSipAllocDisps` API only after successful `XCreateAnalyzer` and all
`SetAnalyzer`/plane settings have executed on the TX lane, and only after a
live TX `OpenChannel`. It takes the RX analyzer settings but never loads or
writes the global `DisplayTx*` preferences. Attachment is limited to one
actual transmitting slice; an unopened or retired channel leaves the mini
unavailable. The same lane orders siphon detach before analyzer destruction
and channel closure. Core daemon media routing uses the separate 40 kHz TX
mini trace, leaving the main TX pan viewer's independent crop intact.

Verification is in `tst_mini_display_composition`,
`tst_mini_filter_response_lifecycle`, `tst_filter_display_high_resolution`,
`tst_remote_display_allocator`, `tst_remote_media_controller`,
`tst_remote_tx_display`, `tst_tx_display_feed`, and
`tst_tx_analyzer_settings`. These cover A/B sharing, floating visibility,
source switching, palette/frame behavior, budget and context rejection,
actual in-process WDSP TX siphon output without RF, and reentrant FIR demand
lifecycle.

The production `ContainerFilterDisplay` is enabled for local and remote
containers. A deterministic regression reproduced the loaded startup failure:
two 2052-float packets exceeded the 4096-float pending queue before a worker
drain, causing both to be discarded. The source now retains the newest valid
whole packet and resets FFT overlap across the discarded gap. Queue bounds and
wait deadlines are unchanged. Shared-DDC fixtures feed each DDC once. Diagnostic
queue state and worker sampling remain available for distinct future stalls.
The regression compares the replacement spectrum with a clean reference after
preloading a partial old FFT, so old and new samples cannot silently blend.
The panel-header composition test
repeats a synthetic full-FFT presentation on each existing Qt wait poll after
revealing a newly bound item; a single immediate presentation could precede
the producer's 30 fps cadence, and this change leaves the wait deadline and
frame size unchanged.
