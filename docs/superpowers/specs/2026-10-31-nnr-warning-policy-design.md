# NNR warning and operator choice

Date: 2026-10-31. Maintainer direction: JJ approved the warning-and-choice recommendation and requested deep inspection and implementation, rather than another approval loop. Routine implementation choices below are controller decisions, not additional statements attributed to JJ.

## Intent
Keep the operator's selected NNR model running. A busy receiver alone must not silently change Premium to Standard or Off. Describe measured trouble honestly and let the operator decide. Apply the same policy when a desktop hosts the Core and when iPhone/desktop operates a headless Core.

## Approaches
Retuning the old 90%/two-second downgrade threshold retains an intrusive quality change and does not distinguish headroom from actual backlog. Removing all overload handling permits stale audio to accumulate. Selected approach: preserve the existing bounded input recovery, replace automatic model reduction with a fresh-measurement warning, and offer existing explicit model/off actions. Do not add a speculative automatic-reduction preference in this scope; it was conditional in the recommendation and is unnecessary to deliver operator control.

## Core contract
- New Cores never automatically call applyNnrLimit because of processing load. Keep old nnrLimit and nnr.tryAgain semantics for old-Core compatibility; never fake a limit to display a warning.
- Keep the existing RxDspWorker skip >500ms / resume <250ms algorithm unchanged, including diversity routing. This refactor may add lock-free observation metadata; no DSP model parameters, audio routing, priorities or queue bounds change.
- Pressure evidence is fresh queued-input delay or newly skipped input. Busy-wall load and lateBlocks are diagnostic only: frame-based NNR normally makes long blocks.
- Use the existing 250ms resume boundary as the warning backlog boundary and existing two-second observation window for sustained trouble. Require fresh evidence across the window; isolated lag spikes do not warn. A fresh positive skipped-input delta is actual missed work and warns immediately; historical cumulative totals never do. Five seconds of measured recovery clears the warning. These are controller policy choices reusing existing system bounds, not measured perceptual thresholds.
- Add an observation timestamp/sequence if needed to prevent stale latest-batch ages and retained snapshots from fabricating sustained overload. Missing, inconsistent, idle, reused-stream, reset-counter, disconnected and removed receiver observations cannot count as fresh trouble or healthy evidence. Explicit NNR off/nonrunning, receiver removal, disconnect and routing/context reset clear its warning/history. Expire unsupported stale warning rather than claim current overload from old measurements; the agreed freshness cutoff is one second (two sampling intervals), and a warning expires after five seconds without fresh supporting observations. Expiry means evidence is stale, not measured recovery. Worker/channel/stream/rate identity, explicit ordinary/diversity source kind, coherent route identity, and stream observation epoch seed new baselines. All receivers consuming a mixed diversity stream use that input source’s observations.
- Publish one distinct read-only text property, nnrLoadWarning (empty means no current measured warning), gated by capability nnrLoadWarningVersion=1 and hello.features.nnrLoadWarning=1 as required by the station-link rules. Keep protocol minor11. Sender filtering, remote mirror whitelist and schemas must be compatible with old clients and old Cores.
- Warning identifies the receiver context and says processing is falling behind while the selected NNR remains running; do not blame a particular machine's aggregate CPU or assert NNR is the sole cause.

## Desktop/iPhone contract
Shared NNR controls and compact RX indicators must distinguish unavailable/runtime error, measured backlog warning, and an actual old-Core applied limit. Show the warning in-place without modal dialogs or repeated notifications. Explicit Use Standard is offered only when Premium is selected and Standard is available; Turn NNR off uses the existing activeNr write and preserves the saved model choice. Leaving the warning alone keeps NNR running. Existing Try again remains for an actual legacy limit and is not offered as a fake recovery action for a warning.

Use existing remote-authority/read-only checks and field write paths. Old Cores retain their existing fallback notice and retry; lack of new capability must not be presented as newly observed healthy behavior. Both clients must clear warning on model/slice/context changes without stale cross-slice state. Accessibility labels, wrapping, popup bounds, settings/Modes surfaces and operator wording are part of acceptance.

## Non-goals and invariants
No changes to transmit/keying, CW gates, NNR coefficients/models/assets, display throttle, new automatic preference, radio configuration or live station restarts. No fabricated physical audio acceptance. Preserve normal profile/settings authority and queue/thread ownership. This is Core receive-path policy work and was flagged to JJ before implementation. Preserve original upstream headers/cites; this policy is NereusSDR-original, not a new upstream port.

## Acceptance
1. Continuous >=90% busy-time without backlog/drop evidence never reduces NNR and never fabricates overload warning.
2. Sustained fresh backlog/repeated loss warns, selected model and activeNr stay unchanged; short spike does not warn.
3. Measured recovery clears warning; stale/torn/idle/routing changes cannot manufacture escalation or recovery.
4. Queue skipping/resume and diversity behavior remain bounded and unchanged.
5. Local/remote desktop and iPhone show truthful distinct warning/legacy-limit/error states and their actions use existing authority paths.
6. Old peers exchange compatible snapshots/deltas at minor11; new capability gates additive fields.
7. Focused meaningful RED/GREEN regressions, then one required full combined Core and applicable iOS validation. One independent integrated review; correct actual findings without unchanged test loops.
8. Update user manual, upgrade/release/bench notes to reflect warning/manual choice, preserving historical documents as historical.
