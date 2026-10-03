# Session display capacity — R-R3-37

Date: September 22, 2026. Software evidence is separate from hardware acceptance.

## Implemented behavior

The [approved design](../2026-09-22-session-display-budget-design.md) gives the
active pan priority when requested display quality exceeds Core's configured
capacity. Background FPS decreases before pixel count; requested quality returns
when capacity permits. Capacity suspension preserves pane identity and painted
history. The GUI shows accepted target quality, recent received FPS when it can
measure it, and a reduction, refusal, pending or stalled reason.

Core advertises one validated descriptor and independently enforces reservations
and actual send pacing. Replacement, peer restart or production restart cannot
create new burst credit within the authenticated session. PureSignal display
bytes share this capacity; Opus remains on its independent RTP path. These are
application-byte and spectrum-sample counts, not measurements of wire overhead
or CPU consumption. The sample configuration leaves both limits unset.

Revisioned outcomes separate the last requested operation from the retained
accepted allocation. The GUI waits for reductions before growth, reconciles late
replies, and does not assume capacity was released after an acknowledgment
expires. Removed panes retain only bounded bookkeeping until release is known.

## Verification

Focused checks on macOS 27.0 / Qt 6.11:

| Boundary | Result |
| --- | --- |
| Calculator/pacer, pure allocator, wire contract, configuration/app, PS3, status overlay, rendering, station/verbs and encrypted audio | 11 CTest targets passed in 34.12 s |
| Real Core controller admission/pacing/lifecycle | 31 Qt cases passed; CTest target 15.94 s |
| Real GUI/Core controller allocation and recovery | 23 Qt cases passed; CTest target 12.73 s |
| Status overlay render and hit regions | Rendered fixture inspected; second status row does not activate RF badges |
| Consolidated independent source review | Four findings corrected, regression-covered; no remaining blocking source finding |
| Matching GUI/Core/all-tests build and unfiltered full suite | Passed: 730/730 CTest targets, 286.68 s |
| Signed local checkpoint | `fc129a39`, verified GPG signature; mandatory hooks passed |
| Matching post-checkpoint build identity | GUI/Core rebuilt at signed roadmap checkpoint `3031a2f5`; generated header and actual GUI binary carry the matching tag |

Review corrections cover retirement during a pending subscribe, authoritative
zero-charge retirement at the latest operation revision, failed source-update
reservation release/recovery, and exact replay after involuntary retirement.
Additional tests exercise held/missing replies, stale zero outcomes, four-pan
focus changes, quality restoration, PS3 refusal without retry loops, maximum
codec/chunk bounds, multipart completion and audio under exhausted display credit.

The final GUI fixture explicitly requests 30 FPS, applies PS3 byte pressure with
a generous independent sample limit, and awaits queued restoration acknowledgment.
A process-specific settings profile prevents parallel tests from changing its
request inputs. No production cap was selected from those test values.

The commit hook required the per-pan sender's established `sw` identifier.
A local-variable-only rename satisfied it without changing the gesture target
or callback. Matching GUI/Core/all-tests binaries rebuilt and the GUI controller
passed again in 12.80 s. The unfiltered suite above remains the behavioral
verification; the rename introduced no behavioral change.

Full-run load averages were 5.92 / 6.39 / 5.52 before the build,
17.12 / 10.51 / 7.39 before CTest, and 12.23 / 10.88 / 8.40 afterward.
No test executable was excluded or timed out. Twelve existing inner Qt cases
reported skips: two unavailable UI/device alternatives, one opt-in screenshot,
one modal-menu timing case, two obsolete RADE TX-routing cases and six TX WDSP
harness cases. These remain coverage limitations, especially for forthcoming R4;
the full CTest pass is not proof of those skipped TX behaviors.

Final verification command (from the integration checkout):

```sh
NEREUS_THETIS_DIR=/path/to/Thetis cmake --build build-integration --target NereusSDR nereusd all_tests -j6
ctest --test-dir build-integration -j6 --output-on-failure
```

## Bounded display transport and measured frames (R-R3-03, R-R3-04, R-R3-05, R-R3-09)

September 23, 2026. The rule and the new limits are recorded in the
[display codec v1 spec](../2026-09-20-display-codec-v1.md) under "Measured
sizes and fragments" and "Transport limits".

- Each process applies SCTP send buffer 65,536 bytes and receive buffer
  131,072 bytes once, before its first media peer; libdatachannel's defaults
  were 1 MiB each. The daemon and the GUI share the one adapter call site.
- With the receiver stalled, the sender refused new display frames instead of
  queueing them. Before the change 1,376,067 bytes of 9,361-byte frames piled
  up behind the link (the regression test failed); after it, 159,137 bytes,
  against a bound of both buffers plus two messages (215,330 bytes). The
  sender recovered and every delivered frame arrived whole once the receiver
  drained again. A 64 KiB message still crosses the encrypted loopback.
- Core counts each refused display message and each transport error, logs
  each distinct error once per media peer, forces a keyframe after either,
  and logs the largest keyframe and delta actually sent with their fragment
  counts. Counters restart with each media peer.
- The GUI keeps its 8-message / 256 KiB oldest-first drop rule and now counts
  each dropped message apart from received bytes; nine messages queued before
  one drain drop exactly one.

Focused checks on macOS 27.0 / Qt 6.11, load averages 4.00 / 4.03 / 4.83
before CTest and 2.99 / 3.77 / 4.69 after: six CTest targets passed in 44.43 s
(`tst_media_transport` 11 cases, `tst_media_peer` 9, `tst_daemon_media_controller`
42, `tst_display_budget` 11, `tst_display_codec` 11,
`tst_remote_media_controller` 32).

Pending on the Rock (controller and operator): at 4096 points, 60 fps with
3D, the journal's `daemon display diagnostics` line shows a largest frame of
at most 9,361 bytes / 11 fragments, and a packet capture shows every media
datagram at 1000 bytes or less (969 on IPv4).

## Pending acceptance

### September 22 operating-message correction

The installed `c28e1565` Core configuration omits both aggregate limit keys,
as expected while hardware measurements remain open. The GUI nevertheless
painted "Core does not advertise aggregate display capacity" on every pan.
The operator reported that this internal terminology was unhelpful. The
compatibility branch now leaves the normal display clear and logs the missing
descriptor when media becomes ready. Actual allocation failure/limiting states
remain visible. No new limits or network policy were selected for this change.

The authenticated remote-display lifecycle fixture checks an absent descriptor,
an empty overlay and continued rendered frames. The rebuilt controller and TX
widget targets passed together, 2/2 in 13.20 s. Installation of this correction
is pending the combined checkpoint checks.

### Remaining hardware work

- Measure one/four/floating/shared-source pan workloads with both FFT tiers,
  WDSP, mixed stereo Opus and 3D on the Rock 5C; select production limits only
  from those measurements. The independent Pi 4 floor remains unmeasured.
- Install matching signed Core/GUI with a recoverable previous version, then
  check active-pan transfer, tuning, zoom, pane removal/reopening and reconnect
  with the operator. Observe audio continuity, history and quality restoration.
- Run the two-hour audio/thermal/loss/jitter/packet-size checks and distinguish
  application traffic from encrypted/network overhead.

This source increment has not been deployed or manually exercised on the radio.
The installed matching Core/GUI was `55e7d49f` at this source checkpoint; later
recovery is recorded in [selector evidence](station-selection.md). This does not complete R3 or the
entire Core/GUI plan, and makes no NAT-traversal, TX, or production-capacity claim.
