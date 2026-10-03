# Remote receive plan review, September 20

Reviewed against integration checkpoint `14124e7c`, including the GUI
correction `ed246176`. J.J. Boyd (KG4VCF), with OpenAI Codex assistance.

## Claude's recovered planning chain

This review expands the existing Claude work. These remain the source documents:

1. [July 28 architecture and brainstorming](2026-07-28-remote-daemon-architecture-design.md):
   DSP split, RTP/Opus, native display codec, FFT ownership, bandwidth,
   transport ladder and phases R1 through R6.
2. [August 2 identity, pairing and network design](2026-08-02-remote-station-identity-and-pairing-design.md):
   machine keys, pairing, dual-stack discovery, private rendezvous, relay
   racing, carrier-network evidence and the required measurement bench.
3. [August 2 R1 implementation plan](2026-08-02-remote-daemon-r1-plan.md)
   and its [verification ledger](2026-08-02-remote-daemon-r1-verification/README.md).
4. [August 3 R2/R3 addendum](2026-08-03-remote-daemon-r2-r3-design-addendum.md):
   corrections to the umbrella, the combined receive release and open R3 choices.
5. [August 3 R2 implementation plan](2026-08-03-remote-daemon-r2-plan.md)
   and its [acceptance ledger](2026-08-03-remote-daemon-r2-verification/README.md).

The original R2 worktree also retains the 968-line execution ledger and task
briefs/reports under `.superpowers/sdd/2026-08-03-remote-daemon-r2-plan/`.
Archived Claude sessions `a9c45d88-8759-4539-87d7-612c959f6929` (architecture)
and `cce211f4-844a-4816-8f4e-893075723aca` (R2 execution) were searched and
relevant passages read. The latter's August 9 final bench instructions
explicitly describe a blank waterfall and silent speakers.

A search of local Git history, worktrees and the conversation index found no
separate prior R3 implementation plan. That is a search result, not a claim
that every historical message or unavailable branch was exhaustively read.
The September 20 R3 plan continues the umbrella's next phase; it does not
replace the R1/R2 plans or reopen settled network choices.

## Judgment

Keep the architecture. It puts radio I/O, WDSP and FFT production beside the
radio, preserves the existing GUI and uses compact media over an encrypted
connection. Replacing it now would discard working control, identity,
settings, lifecycle and display code.

Do not execute the old umbrella as though it were a finished R3 implementation
plan. It has good invariants but missing production wiring and unresolved
interfaces. The next useful milestone is one real remote receive path, then
the multi-pan, capacity and long-session gates. This is implementation
sequencing; the agreed R2-plus-R3 release criteria remain binding.

## Findings that change the work

| Priority | Evidence | Required change |
| --- | --- | --- |
| High | `DaemonApp.h:41-52` explicitly says it does not construct an FFT pool. `mintFftEndpoints()` only fills topology. The reusable pool is not a live daemon producer. | Wire real per-stream I/Q into a daemon-owned FFT pool; prove frame production without a GUI before adding network delivery. |
| High | `SessionTransport.h:62-85` and `SessionTransport.cpp:53-76` implement text-only WebSocket control. Parent sections 7.2 and 10.4 require separate RTP audio and unreliable display media. | R3 must establish encrypted direct media peers using authenticated WSS signalling. R5 adds hosted rendezvous and relay deployment; it cannot own the first media transport. |
| High, R5 boundary | Pinned libdatachannel v0.24.5 `src/impl/icetransport.cpp:159-161` rejects TURN/TCP and TURN/TLS with its libjuice backend. Direct WSS control also presumes a reachable listener. | Preserve the mandatory TLS/443 fallback and all-traffic relay design. Validate a compatible backend or relay path plus authenticated bootstrap before claiming NAT traversal complete. Current LAN transport is only a subset. |
| High | `src/core/Resampler.h:45-56` exposes a fixed conversion ratio. The vendored r8brain interpolator has no public continuously adjustable ratio API. | Treat clock discipline as real implementation work. Do not claim a fixed-rate wrapper supplies adaptive drift correction, or recreate it every packet. |
| High | PR #323's `SpectrumWidget::buildDssWideRow` crops a full-DDC cache, then `DssRenderer::pushRowWithWide` preserves off-screen history. | Carry a bounded optional wider row for remote 3D. Keep the independent exact trace and waterfall planes; never transport full FFT arrays merely to satisfy the renderer. Preserve the local wide-row semantics when extracting them. |
| Medium | Parent sections 9.1 and 17 leave wide/fast and narrow/fine FFTs unresolved. Four pans can share a stream while requesting different resolutions. | Express the tier in the internal source key from the start, share engines by stream and tier, and activate fine resolution on demand within measured capacity. |
| Medium | Parent section 2.5's bandwidth observations came from another client and favorable FFT overlap. The old mono audio measurement does not establish our mixed stereo budget. | Measure both display planes, optional 3D coverage, audio and transport overhead together, including the default 4096-point full-span FFT. Report delivered quality and rate under the same budget. |
| Medium | The R2 acceptance ledger includes incomplete bench rows. Passing control tests does not prove remote receive. | Require real audio playback, advancing 2D and 3D waterfalls, retune/reconfigure recovery, four-pan behavior, and a long audio clock test. |

The missing waterfall is expected in the installed R2 checkpoint. The
[R2/R3 addendum, section 1](2026-08-03-remote-daemon-r2-r3-design-addendum.md)
explicitly describes a blank display and silent speakers. It is not evidence
that the computer upgrade broke an implemented media path.

## Decisions preserved and resolved

- On September 20 the maintainer selected **one daemon-mixed stereo feed**.
  Slice volume, mute and pan remain daemon-side, before the mix. Client
  playback trim and mute remain local. Opus is the receive codec.
- Preserve RTP for Opus, encrypted media, separate audio and display paths,
  direct connections first, relay fallback, first-class IPv6 and no required
  third-party VPN. The old TCI-header and temporary raw-media proposals stay
  rejected. Native spectrum framing remains the recorded choice.
- Preserve the more specific networking addendum too: direct is preferred,
  but direct and relay attempts are raced, with seamless upgrade after initial
  connection. Relay has both UDP 443 and TLS/TCP 443 paths, bounded stale-frame
  queues, short-lived credentials and dual-stack infrastructure. Its carrier
  MTU, NAT lifetime and uplink-jitter experiments remain pending; LAN media
  evidence must not be presented as having answered them.
- `libdatachannel` remains the selected dependency, subject to the existing
  pinned-source and packaging checks. Its current project documentation
  describes WebRTC data channels and media transport; it does not supply an
  audio playback/jitter pipeline. See the [upstream project](https://github.com/paullouisageneau/libdatachannel).
- The display codec and endpoint orchestration are NereusSDR-original.
  Existing WDSP reducers and Aether-derived renderer logic retain their
  source-first, license and inline-comment obligations.
- Full remote TX remains R4. Hosted rendezvous, the two-CGNAT acceptance case,
  and relay operations remain R5. On September 21 the operator requested
  the existing full zoom range: wideband ADC display transport is now an
  explicit R3 parity requirement, R-R3-20/task 4b. Wider coverage inside a
  selected DDC for 3D does not satisfy it.

## September 21 control and accessory audit

The [bounded two-scout audit](2026-09-21-remote-gui-control-gap-audit.md)
found gaps beyond media delivery: dead connection entry points, missing
persistent session/audio feedback, snapshot-time slice creation, unsupported
controls without capability gating, and station accessory operations still
owned by GUI code. These are explicit R3 deliverables in tasks 4c/4d/5a,
not work silently deferred to R6. R3 must also prevent receive-only frequency
changes or inbound tuner telemetry from initiating accessory tuning; R4
retains actual TX and tuner-operation workflows.

Current runtime evidence separates three TGXL problems: the saved endpoint
addresses the amplifier, the actual tuner's TCP control port times out from
both hosts, and the client cannot apply all 13 tuner telemetry properties.
The audit records the evidence and remaining uncertainty. No live settings
or product behavior were changed during the investigation.

The recommended execution order is working R3 LAN receive, then receive-only
R5 internet access without waiting for the entire R4 TX implementation.
Preparing and running the carrier bench can overlap R3 where the required
network and server are available. This advances the remote-use vision while
preserving every TX acceptance requirement before transmit is enabled.

## Opus question checked

The existing build pins Opus
`940d4e5af64351ca8ba8390df3f555484c567fbb`. Its `opus_encoder.c:1658-1659`
promotes mediumband to wideband in CELT mode. A compiled synthetic probe using
the saved AUDIO/MUSIC, 40 ms, constrained VBR, complexity 10 settings produced
wideband packets for mono and stereo inputs at 24 and 48 kbit/s. The stereo
fixture retained two encoded channels. This supports the old warning that
requesting mediumband does not guarantee a 6 kHz result; it does not establish
subjective quality or Rock 5C encode cost. Use explicit supported bandwidths
and inspect actual packets in the codec tests.

## Execution policy

Use [the R3 execution plan](2026-09-20-remote-daemon-r3-plan.md) with the
maintainer-requested Yonder cost-aware workflow. Keep a single durable ledger,
delegate bounded independent implementation or investigation, test according
to risk, and review the integrated boundaries. Do not repeat the old
per-task specification/reviewer/whole-branch ceremony.

The recovered combined GUI builds. The stale RX-cache contribution to TX 3D
rows is fixed in `ed246176`, with a regression that failed before the fix.
At `14124e7c` the full unfiltered desktop suite passes 662/662 tests, after
correcting nine TX-analyzer settings' station scope and a GUI test's unnecessary
window-exposure race. Three data-file tests required local build-directory
fixture symlinks; that environment workaround is separate from source fixes.
The ARM Release daemon and its shared dependencies also build and stage
successfully. These results do not establish a working remote media path.
