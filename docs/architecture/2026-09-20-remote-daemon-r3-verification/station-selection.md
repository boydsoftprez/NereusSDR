# Core/radio selection — R-R3-38

September 22, 2026. Implements the promoted R3 task 4g without changing the
saved R5-before-R4 roadmap. See the [design](../2026-09-22-station-selection-design.md)
and [implementation plan](../2026-09-22-station-selection-plan.md).

## Implemented behavior

Connections groups local radios, LAN Core announcements and saved stations.
Local radios use the desktop's embedded Core/DSP. Saved stations retain separate
address, token and certificate-pin tuples in operator-local settings. Existing
single-station settings migrate once; explicit local selection survives restart.
Discovery can supply an address for a saved identity, but cannot change its pin,
grant trust or overwrite its saved address. New Cores require connection setup.

Explicit Connect switches the whole operating window/model/settings proxy after
retiring the previous session. Highlighting a row, editing or cancelling does
not switch. Disconnect and Cancel retry always address the current session.
Reconnecting the same selected endpoint preserves its existing session model.
Core connectivity and radio availability are distinct, and LAN/cached identity
remains labelled until authenticated. The Core still owns one configured radio;
the picker does not reassign that radio.

The daemon advertises its actual listening address/port and Core/radio identity
through bounded IPv4/IPv6 multicast announcements. Advertisements contain no
token. Invalid packets, oversized input, stale endpoints and old queued drains
are rejected or retired. Manual addresses remain available without multicast.

## Software verification

| Boundary | Evidence |
| --- | --- |
| Target persistence, one-time migration, CLI tuple isolation, settings scope | Three focused targets passed |
| Whole-window lifetime, retry retirement, local/remote proxy ownership | Real session tests passed; AddressSanitizer regression covers retry-toast destruction |
| Actual Core A→B, local→remote→local, edit/cancel, retry cancellation | Controller tests with two loopback Cores passed |
| LAN trust | Actual TLS server tests accept the saved pin, reject a spoofed identity before token authentication, and leave unknown credentials blank |
| Codec/cache/socket/daemon | Bounded parsing, expiry, UDP ingestion and actual listener close/relisten tests passed |
| Widget presentation | Three rendered QWidget fixtures inspected; actions visible, no overlap, token masked. Illustrative data, not hardware captures |
| Integrated AddressSanitizer | Coordinator, controller and TLS selection: 3/3 targets passed, 50.48 s before final review corrections |
| Consolidated source review | Five findings corrected; no remaining blocking source finding. Includes interface recovery, credential-free errors, applicable connection actions and config identity validation |
| Matching GUI/Core/all-tests build and unfiltered suite | Passed: 738/738 CTest executables, 245.15 s |
| Signed checkpoint and matching installed build | c28e1565, verified GPG signature; matching GUI tag/private-library UUIDs and strict/deep bundle signature passed. Fresh native source manifest/build/install passed |

The session test exposed and corrected a toast-destruction use-after-free:
QWidget child destruction called back into a MainWindow member list after that
list was destroyed. Retirement now destroys those toasts while their owner state
is alive. A separate guard prevents whole-session replacement inside synchronous
WDSP initialization, whose nested event loop could otherwise retire the current
local model before its connection call returned.

The first full run found two failures: the existing TLS assertion expected the
word “fingerprint” (retained without either pin), and a restart/persistence test
used the shared default test settings file. That persistence test passed alone;
a process-specific profile now prevents parallel tests overwriting its saved
restart data. The matching rebuild and corrected full run pass without exclusions.

Load averages were 2.97 / 5.15 / 3.88 before the final build,
9.49 / 6.47 / 4.40 before CTest, and 6.36 / 6.50 / 4.92 afterward.
Twelve existing inner Qt cases skip: two unavailable device/UI alternatives,
one optional screenshot, one modal-menu timing case, two obsolete RADE TX-route
cases and six WDSP TX harness cases. These are coverage gaps, not passed TX
behavior. No executable timed out or was excluded.

## Installed checkpoint and native evidence

Matching Core and GUI `c28e1565` were installed September 22. Core was active
with zero automatic restarts at readback, and actual Saturn I/Q arrived. The
GUI authenticated at 13:18:30, opened 48 kHz stereo speakers and received Opus
at 13:18:32, followed by encrypted spectrum at 13:18:33. This confirms the
backend paths, not sustained smooth playback. Later audio-context recoveries
and source queue drops leave the audio/soak gate open.

The Rock emitted 146-byte IPv4 discovery datagrams from `.106` through `end1`
to `239.255.42.99:47910`, TTL 1, at the expected five-second interval. A passive
Mac receiver received `rock-5c`, `ANAN-G2 (Saturn)`, connected=true and control
port 50055. This proves actual IPv4 multicast emission and client reachability.
The Mac was subsequently unlocked. Native inspection confirmed the running
`c28e1565` GUI, the Rock `.106:50055`/Saturn identity, received spectrum and
waterfall, and live receiver/meter values. Selector interaction and switching
acceptance remain pending; this operating-screen observation does not prove them.

Native inspection then exposed a crash while opening Radio → Connections and
reading its accessibility tree. The macOS report identifies
`QMacAccessibilityElement::accessibilitySelectedChildren`; Core remained active
with zero automatic restarts. The chooser was clearing and rebuilding every
row on each discovery refresh, including unchanged rows. It now reconciles
rows by key, deselects before structural changes and preserves surviving rows
when obsolete siblings are removed. A regression first reproduced the model
reset, then passed with a visible widget using Qt's actual accessibility
selection API. It covers status-only updates, removal of a preceding row and
deletion of the selected row. The rebuilt selector target passed in 0.47 s.
Actual AppKit accessibility inspection and the final combined suite remain
pending for this correction; the source test alone does not close the crash.

## Native acceptance still open

- Actual GUI population from native IPv4 announcements, IPv6 multicast
  reception, interface/address changes and discovery expiry on a real LAN.
- Native renderer/audio retirement while switching between a local radio and
  the Rock/Saturn station; second real Core if available. Offscreen/ASAN tests
  do not prove native GPU or physical-radio ownership behavior.
- Operator use of title/menu/Connect, add/edit/forget, disconnect/reconnect,
  restart selection and separate Core-online/radio-offline presentation.

During deployment preparation the Rock at `.106` was reachable, but systemd
reported Core inactive/masked: `/usr/lib/systemd/system/nereusd.service` and
the previous `55e7d49f` staging copy were empty. The installed executable,
Core library and RADE library were also empty, as was that installation's
rollback archive. The generated service template
and older staging copy were nonempty. The cause of the empty files is not yet
established. Installation preparation now checks a nonempty unit with
`systemd-analyze verify` before stopping the previous service and flushes the
installed binary, libraries and service file before startup. The retained `3402d171` executable/Core/RADE hashes matched the earlier
successful installation log. Those files and its verified unit were restored
with the current configuration/credentials preserved and backed up. systemd
reports active and enabled again. A subsequent reboot was observed without a
root-issued reboot command; the recovered service autostarted. Its cause is
awaiting operator confirmation. The fresh c28e1565 native installation then
completed with a nonempty, flushed rollback of verified 3402d171. Post-install
boot/switch/soak acceptance remains open; recovery alone is not a reconnect pass.
