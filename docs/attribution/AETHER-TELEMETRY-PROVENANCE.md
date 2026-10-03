# Aether telemetry graph provenance

This record covers the protocol-neutral GUI graph and in-memory history added
for NereusSDR R-R3-32/33. It does not import any FlexRadio, VITA, DAX,
SmartLink, audio-transport, radio-model, or collector behavior.

| Item | Source |
| --- | --- |
| Upstream project | [AetherSDR](https://github.com/ten9876/AetherSDR) |
| Revision inspected and ported from | `0dea0dd7d73e25a40c8c01d46873af5834e23921` (`upstream/main`, 2026-09-21) |
| Project lead / primary author | Jeremy Fielder, KK7GWY (`ten9876`) |
| Licence | AetherSDR project-level [`LICENSE`](https://github.com/ten9876/AetherSDR/blob/0dea0dd7d73e25a40c8c01d46873af5834e23921/LICENSE), GNU GPL version 3 |
| Relevant upstream paths | `src/gui/TimeSeriesGraphWidget.h`; `src/gui/NetworkDiagnosticsDialog.{h,cpp}`; `src/gui/MemoryHistoryRing.h` |

Aether's relevant source files do **not** carry top-of-file copyright or GPL
notices. The project-level GPLv3 licence above applies. `TimeSeriesGraphWidget.h`
states that it was extracted from `NetworkDiagnosticsDialog.cpp` in upstream
issue/PR #2554; its reviewed history includes commit `52c1ced5d0610c8a947cb1b12a873fccbabd934c`.

Nereus copies and adapts the reusable Qt chart presentation: labelled series,
legend selection, linear/log and fixed axes, maximum-gap line splitting, and
pixel-column downsampling. `TelemetryHistory` adapts Aether's one-second
sampling and bucket-centre query approach to Nereus-owned input values. It adds
per-metric optional values, observation weights, bounded raw/minute retention,
and explicit segment/missing breaks. Those changes prevent a reconnect or an
unavailable field from becoming either a zero sample or a continuous line.

Live Nereus Audio graphs use longer `frames/s` and `packets/s` labels. Their
left gutter is measured from the actual tick and last-value text, and the
legend wraps from the resulting plot edge. The upstream formatting, scale
semantics and inline explanatory comments are retained.

The Nereus telemetry collector, authenticated Core session message, receiver
observations, and all remote-control transport behavior are original Nereus
work and remain outside this port.
