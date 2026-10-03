# Core/radio selection and standalone desktop operation

Requirements: R-R3-10/16/17/21/38. This implements R3 task 4g and the existing
identity/pairing design §6. The operator requested selection of Core/radio pairs
sooner and confirmed that the desktop must still operate local radios itself.
The full roadmap and R5/R4 sequence remain unchanged.

## Operator flow

One modeless **Connections** screen is reachable from the connection title,
Connect/menu actions and the disconnected pan entry point. It contains the
three groups already specified in the identity design:

| Group | Row content | Action |
| --- | --- | --- |
| Radios on this network | Radio name/model, address and available/in-use/offline status | Connect using this computer's Core/DSP |
| Stations on this network | Core name → advertised radio; address and setup/availability state | Use saved trust for that Core or open connection setup |
| Your stations | Saved Core label → last known or authenticated current radio | Connect to the selected saved Core |

Selection alone does not connect. **Connect**, **Disconnect**, **Cancel retry**,
**Details**, **Add Core…**, **Edit…** and **Forget…** act on explicit targets;
show only actions applicable to the row/current session. Local radio details
retain the existing local picker/edit behavior. Credentials never appear in the
list, tooltip, errors or diagnostic URLs. Core connectivity is separate from
its radio's state: an authenticated Core with an offline radio is still a
connected Core. Show cached/advertised identity as such until authenticated.

Illustrative layout, not a runtime capture:

```text
Connections
  Radios on this network
    ANAN G2E                     This computer       Available
  Stations on this network
    Rock 5C → Saturn G2          192.168.109.…        Saved Core
  Your stations
    Shack → Saturn G2            Last known radio    Disconnected

[Add Core…] [Edit…] [Forget…]     [Details] [Disconnect] [Connect]
```

Preserve startup behavior for an explicitly configured/last-selected target;
opening the picker does not disconnect it. An explicit Local choice suppresses
saved remote fallback. Switching reuses the application process but recreates
the main operating window and model after retiring the outgoing connection.
Preserve window placement and operator layout through existing persistence.
A rejected/invalid selection leaves the current connection unchanged.

The current Core owns one configured radio. Choosing “Rock 5C → Saturn G2”
selects that Core; it does not send a command to change the radio Core owns.
A future radio-reassignment control needs a separate Core-owned contract.
The desktop's local mode remains self-contained. Any later loopback-daemon
convergence must bundle/manage that Core without manual service installation.

## Client-local target store

`gui/CoreTargetStore` owns a versioned `ConnectionTargets/V1` JSON document in
`AppSettings`. `ConnectionTargets/` is explicitly OperatorLocal. Records have
an immutable generated ID, label, URL/token/certificate-pin/bench-override tuple,
and optional cached radio name/MAC. The selected ID is `local` or an existing
Core record. Store at most 128 records and reject malformed/oversized documents
without replacing working state. Missing credentials mean “needs setup”; they
do not weaken `StationClient` admission.

When the document is absent, import the existing `RemoteStation*` fields as one
record and preserve their exact trust tuple. Keep the legacy keys for migration
recovery, but once the new document exists it owns selection; forgetting a Core
or choosing Local cannot resurrect it through legacy fallback. Mutations persist
before reporting success and roll back their setting value on save failure.
Other settings remain untouched. Do not log document contents.

Command-line startup must select a coherent target. A new `--station` URL must
not borrow a different saved Core's token, pin or unpinned override. Resolve
credentials only from that selected record or explicit CLI inputs. Explicit
local startup and picker selection bypass remote fallback.

## Ownership and switching

A GUI session coordinator above `MainWindow` owns the active window and optional
`SettingsProxy`. Local and Remote roles remain immutable within a `RadioModel`.
Same-Core reconnect retains its existing session behavior; switching targets
creates a fresh model, mirror, media controller and settings cache.

Before switching, validate the target without mutation and retire the outgoing
UI admission generation. Cancel retries, stop its media/session, close/save the
operating window using the existing disconnect/shutdown ordering, and destroy
its renderers/model. Keep its proxy alive while the old window tears down, then
remove the non-owning `AppSettings` backend before destroying the proxy. Install
a fresh unready proxy before constructing any new remote model. For Local,
leave the backend null. Only the new generation starts its selected connection.

Window replacement must not trigger `QApplication`'s last-window auto-quit.
Ordinary operator close/quit still exits normally. Disconnect/close paths must
remain usable while offline or retrying. Cancel startup auto-connect and stale
queued picker/window callbacks when a generation retires. Cover the current
QRhi/pan retirement ownership; do not retain old windows/widgets across models.

## LAN discovery boundary

Implement the existing one-way, dual-stack multicast design: Core announces
only its reachable listening service; the GUI listens and never advertises.
Manual/saved addresses continue to work when multicast is unavailable. A
bounded announcement carries service/schema, Core certificate fingerprint,
Core display name, actual control port, and advertised radio name/MAC/status.
Use the datagram's source/interface to form candidates, preserving IPv6 scope;
an announcement cannot supply an arbitrary redirect URL or credentials.

Discovery is a hint, not authentication. A saved pin remains authoritative and
is never replaced by a received advertisement. An unknown Core opens setup;
normal selection does not automatically enable the existing bench-only unpinned
option. Full key pairing/revocation remains its own roadmap work. Explicitly
show an empty or failed discovery state instead of implying discovery is working.

The exact discovery datagram/socket interfaces and protocol constants will be
recorded in the implementation plan before transport implementation. Native
multicast, interface changes and LAN radio ownership require hardware evidence;
codec/cache/loopback tests cannot close those gates.

## Acceptance

- Switch local → Core A → Core B → local without manual process restart.
  Prove one owner, no A settings/receivers/media/retry arriving in B, and a null
  remote settings backend before local model initialization.
- Preserve the old configured Core, explicit local selection and independent
  credentials for multiple targets across application restarts.
- Display Core identity and radio identity separately; distinguish unverified
  advertisement/cache from authenticated current state and Core-offline from
  radio-offline. Malformed discovery cannot change a saved record or trust.
- Preserve local radio selection, editing, direct connection and cancellation.
  No automatic direct-radio takeover when a remote Core is unavailable.
- Exercise actual widgets/actions and render the connection screen with local,
  discovered, saved, connected, retrying and error examples. Follow with an
  operator smoke checkpoint on the installed matching Core/GUI.
- Keep full matching GUI/Core build, unfiltered desktop suite, signed commits,
  attribution and the existing hardware acceptance requirements.
