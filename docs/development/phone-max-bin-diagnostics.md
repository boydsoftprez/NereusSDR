# Capturing an unavailable phone Max Bin meter

This diagnostic records why the phone cannot use a displayed FFT frame for
the selected flag's Max Bin reading. It does not change meter selection,
fallback, calibration, display subscriptions, or eligibility checks, and it
does not by itself fix an unavailable meter. Build 9639 predates this capture.

On a build containing the capture:

1. While the affected flag is visible, open **… → S-meter**. Keep the disabled
   **Max Bin** entry and its explanation visible briefly. Peak can remain
   selected; opening this menu evaluates Max Bin without selecting it.
2. Open **Tools → Support Bundle → Collect → Share** and send the phone log
   with the approximate time and a screenshot of the failure. Keep the app
   running until collection finishes.
3. Inspect `meter.max: Max Bin gate=...` entries immediately before leaving
   the panadapter. Navigating to Tools can alter the display subscription;
   later observations must not be mistaken for the earlier failure.

The `gate` is the first failing check in the existing order. The record
includes numeric displayed-slice, accepted/requested-subscription and
committed-frame identities; whether the pan identities match; revision,
coverage, pause/transmit state; trace count; passband geometry; and whether
a value was available. Raw accepted/requested holdings remain visible in
the record even when the public accepted display is unavailable.

There are no raw pan-key strings, credentials, addresses, connection
identifiers or trace arrays. Nothing is uploaded automatically. The existing
phone support log exports the local OSLog entries when the operator shares it.

Each slice emits at most once per second when its tuple changes. An unchanged
unavailable tuple may emit one additional persistence record, then stops;
changing signal values and frame sequence numbers do not generate repeated
records. No timer or background task is added. A second observation is needed
for the persistence record, so leaving the menu open does not guarantee it.

The offline regression covers both allocation/context orderings through
viewing another receiver, successful take bookkeeping, returning, suspending
display for audio-only use, and reacquiring the display. Those cases pass;
the operator's persistent failure still requires its actual phone capture.
