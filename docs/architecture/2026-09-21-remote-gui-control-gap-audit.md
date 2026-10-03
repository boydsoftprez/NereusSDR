# Remote GUI controls and station accessories: September 21 audit

Execution policy: yonder-cost-aware-execution. Two bounded read-only scouts
examined GUI controls and accessory ownership; the lead checked the source,
saved designs and current runtime. Code checkpoint: `501b2701`; preceding
plan checkpoint: `902ebe3f`. This is a plan audit, not a claim that the newly
identified gaps have been fixed. No product code, live settings, pairing,
network configuration or RF state was changed during this audit.

## Judgment and next implementation order

Keep the existing Core/UI architecture. A working model mirror and settings
cache do not automatically make every menu, imperative method or telemetry
property remotely functional. Several exposed controls still address local
objects that intentionally have no radio or DSP resources in remote mode.
Other controls are deliberately unavailable until remote TX is implemented.
The GUI needs to distinguish these cases visibly.

1. Complete R3 connection entry points, persistent Core/radio/audio status,
   snapshot-safe layout restoration and capability-based control gating.
2. Block receive-only accessory tuning side effects, then complete R3
   station-owned validated connection, disconnect/cancel, configuration
   application and receive status. Discovery must run at the station or be
   clearly unavailable. Resolve the TGXL control-port problem separately
   from GUI wiring.
3. Finish the existing R3 wideband parity and sustained-audio work. Show the
   current audio profile and diagnostics; use the pending 24/48 kbit/s
   comparison to decide the selectable quality profiles.
4. Run the R3 multi-pan, mode/filter, reconnect/boot and two-hour acceptance.
5. Continue receive-only R5 traversal, R4 safeguarded TX, and R6 complete
   selection/pairing/packaging in the previously recorded order.

Basic menu behavior and peripheral connection/status must not be silently
postponed to R6. TX-related controls need honest gating now; enabling their
operation remains subject to R4. Exact new wire contracts are implementation
discovery tasks, not interfaces already present in the current protocol.

## Source-grounded gaps

| ID | Finding and evidence at this checkpoint | Required result / phase |
| --- | --- | --- |
| C01 | Radio menu Connect/Disconnect has a station path (`src/gui/MainWindow.cpp:6386`, `:6456`), but title/status/pan clicks reach `showConnectionPanel`, which returns in remote mode (`:9876`). Chrome reads radio connectivity while menu gating reads the station session (`:9969`). | R3, R-R3-16/17: all entry points act on the configured Core session; show Core identity, retry/cancel/error and separate radio state. Preserve manual disconnect cancellation. |
| C02 | Connected-state handling queues `populateEmptyPans` (`MainWindow.cpp:3218`), which can call `addSliceOnPan` before snapshot hydration finishes (`:10575`). The live launch added a second station slice before handshake completion. | R3, R-R3-24: replay and hydrate without outbound create commands. A saved one-pan GUI attaching to an existing one-slice Core must retain exactly that slice. Explicit post-hydration add/layout actions still obey station capacity. The source path is consistent with the live symptom; an integration regression must pin causality. |
| C03 | The high-resolution FIR graph option binds a local `RxChannel` (`src/gui/setup/DspOptionsPage.cpp:515`); that channel is absent remotely. General remote setup gating is in `src/gui/SetupDialog.cpp:330`. | R3, R-R3-21: disable or clearly label this unavailable visualization. This finding does not establish that ordinary receive DSP setters are broken; no second concrete audio-DSP write failure was established in this bounded audit. |
| C04 | `txPermitted` is false (`src/core/session/StationCapabilities.h:102`) but has no GUI consumer. Remote MOX is refused by RadioModel (`src/models/RadioModel.cpp:797`). | R3 interface gate, R4 functionality: consume negotiated capabilities to disable/explain unavailable TX actions, retaining Core's refusal. Do not turn a refusal-only boundary into a claim of functioning remote TX. |
| C05 | Media errors are transient toasts (`MainWindow.cpp:970`); the active codec is not surfaced persistently. Production starts the default encoder (`src/core/session/media/DaemonMediaController.cpp:931`); the codec accepts 24/48 kbit/s but has no negotiated profile control. | R3, R-R3-23: distinguish local output/mute, station radio state, media failure and reception; display the actual profile and link health. A profile selector follows measured quality and an explicit accepted configuration, not arbitrary raw codec knobs. |
| C06 | All 13 TunerModel properties are outbound (`src/core/session/MirrorPolicy.cpp:243`), but client application lacks a safe TunerModel state adapter (`src/core/session/StationClient.cpp:1659`). Reusing the command hook would send commands instead of applying telemetry. | R3, R-R3-22/25: apply Core tuner telemetry through a client-only state writer that assigns/emits without hardware commands, and gate the applet's reactive tune orchestration at the same time. Presence, connection, relays, antenna and meters must hydrate and recover on reconnect. |
| C07 | Station-scoped accessory settings correctly proxy to Core storage (`src/core/settings/SettingsScope.cpp:230`), but Peripherals buttons still call GUI-local accessory connections and scan locally (`src/gui/setup/CatNetworkSetupPages.cpp:1240`, `:1352`). The 4O3A toggle also invokes a GUI-local listener (`src/gui/setup/FourO3APage.cpp:180`). Saving settings does not live-apply them in Core. | R3, R-R3-22: Core owns discovery and sockets; authenticated commands apply the selected station configuration and return accepted/refused outcomes. Use atomic configuration plus connect, or a settings acknowledgement followed by an ordered command. Status must come from Core. Local direct mode retains its existing path. Full administration/pairing UX remains R6. |
| C08 | `TgxlConnection` accepts any V banner, sends info/status and marks connected before validating device identity (`src/core/TgxlConnection.cpp:234`). The current saved TGXL endpoint is actually the PGXL. | R3, R-R3-22: keep a provisional connection until correlated discovery/info identifies a supported tuner. Use actual observed model aliases and serial identity; a version banner or successful TCP connect is insufficient. Wrong-device responses must not establish tuner presence or enable tuner commands. |
| C09 | Peripheral disable/teardown skips PGXL/TGXL unless already connected (`RadioModel.cpp:2729`, `:3012`). TGXL scheduled reconnect captures an old endpoint in an uncancellable singleShot and ignores manual cancellation (`TgxlConnection.cpp:503`). | R3, R-R3-22: cancel connecting, handshaking and retry states as well as active sockets. Old timers cannot redial after disable, radio switch or endpoint replacement. Cover local and remote callers. |
| C10 | Remote gating tests explicitly avoid constructing MainWindow (`tests/tst_remote_gui_gating.cpp:348`, `:720`), so metadata/slot-existence checks do not prove the visible interactions. | R3, R-R3-21: add a narrow injected session/presentation harness for connection, capability and hydration behavior, plus a receive-only GUI acceptance script. Retain meaningful Core command/mirror tests. |
| C11 | PGXL band updates and SmartSDR listener frequency/mode seeding and changes live in MainWindow (`src/gui/MainWindow.cpp:9463`). Headless Core never executes that wiring. | R3, R-R3-22: move station frequency/mode propagation and initial seeding into Core, following the stable TX-bound slice identity, with no duplicate GUI sender. Verify within-band tuning, band/mode changes and slice rebind/removal using reported state without RF. The inferred stale listener default has not been bench-confirmed. |
| C12 | `RadioModel::onSliceBandChanged` can issue `autotune` if auto-recall is enabled, a stored memory exists and TGXL is connected (`src/models/RadioModel.cpp:15415`). Once projected, tuner `isTuning` telemetry also reaches a GUI handler that starts local carrier orchestration (`src/gui/applets/TunerApplet.cpp:385`). | R3, R-R3-25: receive-only remote operation and inbound telemetry cannot trigger TX-coupled accessory commands. Gate these paths before enabling the repaired TGXL connection/projection. Preserve local-direct behavior. These are source-proven conditional paths, not observed RF or autotune events on this bench; R4 owns their authorized remote operation. |

## TGXL runtime evidence, distinct from the code gaps

Read-only checks from the Rock 5C and Mac established:

- LAN announcements identify `192.168.109.235` as **PowerGeniusXL 3.8.9**
  and `192.168.109.234` as **TunerGenius 1.2.17**.
- The station's saved TGXL endpoint is `.235:9008`. Core has an established
  socket there, and passive capture shows amplifier status fields such as
  `state=STANDBY`, `vac`, `vdd`, `biasA` and `fanmode`. This is not evidence
  of a healthy TGXL connection.
- The real tuner responds to ping and advertises on the LAN, but TCP 9010
  connection attempts time out from both the Rock and Mac. Neither probe
  sent an application command. The cause is not established by ping or by
  the GUI source audit. Whether another application is using the device is
  an open operator question, not a conclusion about exclusivity.
- The remote GUI logs that all 13 tuner fields cannot be applied. Correcting
  the endpoint alone cannot repair that telemetry path, and repairing the
  mirror alone cannot make the physical control port accept a connection.

Captures remain private. No tuner tune, operate, bypass, relay, antenna or
pairing command was issued in this investigation. Do not change the saved
endpoint and call the issue resolved without checking the real tuner path.
The receive-only guards in C12 are a prerequisite to live acceptance of the
repaired connection and telemetry, not evidence that tuning occurred today.

## Minimum remote-audio interface

The existing ownership decision remains: Core mixes slice gain/mute/pan into
one stereo feed; the GUI owns its output device, master trim and playback mute.
Current production codec: 48 kHz stereo, 24 kbit/s constrained VBR, 40 ms,
Opus AUDIO/MUSIC, explicit wideband, complexity 10.

R3 should expose persistent receiving/muted/output-error/media-error status,
the current accepted profile, and useful loss/jitter/buffer diagnostics in
the remote connection/audio surface. Label the counters by what they measure;
packet arrival gaps are not automatically packet loss.

Recommended follow-through, pending the existing listening comparison: a
small quality selector for the profiles that pass measurement, with Core
acceptance/refusal and clean reconfiguration. Do not claim automatic bitrate
adaptation or expose frame size, complexity, FEC and bandwidth as working
controls when the protocol has no such contract. The older addendum's
three-tier example was an option, not an approved requirement. The original
lossless/high-rate PCM requirement for digital-mode work remains open; Opus
listening success does not close that requirement.

## Verification boundary

This read-only audit adds no product behavior. The last software baseline
remains 681/681 tests at code `501b2701`; those tests did not verify the new
findings. Each implementation checkpoint needs its applicable regressions,
unchanged local-mode behavior, and explicit live observations. A complete
inventory of every Setup page, spot/record stream and third-party integration
has not been performed by these two bounded scouts.
