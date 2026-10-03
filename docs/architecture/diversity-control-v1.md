# Diversity control v1

Authoritative movable Diversity contract, agreed 2026-10-01.

## Negotiation and allocation

Keep agreed minor 11 (LinkVersion.h explicitly requires post-11 features to use named hello/capability negotiation). Hello feature `diversityControl: 1`; returned capability `diversityControlVersion: 1`, only when agreed minor >=11, feature is declared, and the client is authenticated. Summary admission needs no control/session holder lease; mutation admission is separate. Version proves coordinated movable Diversity for P1/P2; board identity and diversityPatternVersion never prove it. Pattern summary requires the already defined diversityPattern:1 negotiation too. Unsupported board still gets the version and an authoritative hardware reason, so UI can stay visible and grey. Older Core or omitted capability: refuse/disable movable actions with “Update the Core to move Diversity between slices.”

Append RadioModel `diversityState`, utf8 / Outbound, ordinal **37** (actual tail currently rxFilter0LowPassSlice=36). This property is peer-only behind diversityControl:1; no existing ordinals change. It is station-wide on key `radio`, never joined-SliceModel scoped. Append named capability entry; its table position is not an ordinal.

## One authoritative state value

`diversityState` is compact JSON version 1, replaced as one property value:

```
{ "version":1, "revision":12,
  "requested":true, "running":false, "paused":true,
  "reasonCode":"pureSignalResources", "reason":"Diversity pauses while PureSignal transmits on this radio.",
  "live":{"sliceId":2,"incarnation":123,"controlRevision":4,
          "controllerDeviceId":"...","letter":"C","band":5,
          "frequencyHz":14200000,"phaseDeg":30,"gainDb":-2,
          "fineNullEnabled":false,"pattern":null},
  "targets":[{"sliceId":2,"incarnation":123,"controlRevision":4,
              "controllerDeviceId":"...","eligible":true,
              "reasonCode":"","reason":""}]
}
```

`live` null iff requested=false; owner stays the same while paused. running implies requested and not paused; paused implies requested and not running and a nonempty authoritative reason. Summary is sufficient for a page following an unjoined live slice: blend, band, tuning, controller and Core-generated pattern; no joining or full SliceModel inference. Targets enumerate live slices with Core resource/hardware eligibility and identity; mayChange remains based on current caller's SliceAccess/identity, and is rechecked at execution. Reason codes are open strings, unknown codes use reason text. Minimum codes: hardwareUnavailable, radioDisconnected, resourcesUnavailable, pureSignalResources, sourceChanged, targetChanged, stateChanged, sourceControlRequired, targetControlRequired, onAir, unsupported, invalidRequest. Resources are computed by the assignment planner, never client counts or invented board limits.

`revision` is monotonic for the Core run, changing on owner/requested/running/paused/eligibility changes. Blend/pattern updates republish the summary without invalidating a pending move (identity/controlRevision still checked). Core publishes complete final summary after coordinator commit; intermediate per-slice enabled deltas are not a transaction and must not drive live selection. SliceModel.diversityEnabled remains per-slice **requested** state: true only on live, including a resource pause. DIV identity is live, even paused. Per-slice phase/gain/fine-null remain existing storage; move uses latest target values, never source copies.

## One coordinated action and one result

Command `diversity.setTarget` via existing command.invoke/result, capability diversityControlVersion=1 / minMinor=11. Required typed args: `enabled` bool; `stateRevision` i64; source and target each `SliceId`, `Incarnation`, `ControlRevision` i64, using lower camel names (`sourceSliceId`, etc.). Absent participant is sliceId=-1/incarnation=0/controlRevision=0. Enabling from off: absent source, exact target. Move/reaffirm: exact current source and exact target (may be same). Off: exact source, absent target. No other shapes accepted.

Before any stop/reassignment, on RadioModel's thread: validate exact argument kinds/shapes; current authenticated live session/device identity (existing SessionServer/dispatcher requester, not caller-supplied id); state revision and current source; SliceOwnership.matches(SliceRef) for each participant; exact controlRevision; SliceAccessPolicy.mayChange for **both** nonabsent source and target; freeze/on-air guard for both; then resource planner. Same source/target must pass both participant checks. Source-free enable still needs target control. No automatic takeover or release. All checks happen immediately before mutation, including any deferred dispatch; there is no independently mutating source-off write.

Result uses existing command.result verb/id/accepted/reason/affectedKeys/values. Values include `diversityState` utf8 and `reasonCode` utf8 (empty on acceptance), including current authoritative state on refusal. Accepted move affects `radio`, source slice key and target slice key; refused result has no affectedKeys and no mutation. Success means routing/requested commit accepted; running remains separately authoritative, possibly paused by allowed PS resource conflict. Accepted actual move reason: “Diversity moved from B to C. Both slices paused briefly.” It is a notice, not a measured gap claim.

Legacy property.write may safely enable fixed A (id0) from off/reaffirm A or switch off its own live slice, after current admission/controller/resource checks. Enabling non-A, transferring from another live owner, or switching off another owner is refused with “Use the coordinated Diversity control to move Diversity.” No legacy write can transfer. It never silently invokes a move lacking participant revisions. Existing phase/gain writes remain caller-controlled. All local/model requests share one coordinator/owner invariant; station window routing uses the existing station requester policy, with GUI work outside this lane.

## Lifetime and routing

Close live slice => stop/off, live=null, no replacement; blend dies with removed slice, saved memory banks unchanged. Restart => persist requested owner id alongside existing per-slice blend, restore only after that same slice is restored; absent means off. Incarnation is fresh after restart and never persisted/reused for wire requests.

P2: target **stream** occupies coherent DDC0/1 pair with target stream rate/centre and partner frequency; displaced streams retain usable assignments. P1 Orion: target stream occupies slots0/1; other streams use remaining slots; fixed rx frame count/global rate, ADC map, phase-lock bit and bank repush preserved. Resource arbitration keeps Diversity during PS where planner can supply both; otherwise requested owner pauses with a reason. Preserve mixed output for all target-stream cohosts and existing generic RxDspWorker route.

## Implementation boundaries

RadioModel.h/.cpp: movable owner/coordinator, state JSON/signal, close/restart and per-slice changes; CodecContext.h + P1CodecStandard.cpp/P2CodecOrionMkII.cpp: target stream assignment/resource planner; P1RadioConnection.cpp/P2RadioConnection.cpp: assignment/partner frequency and remap banks. RX path touched and will be flagged.

StationCapabilities.h/.cpp, StationServer.h/.cpp, MirrorPolicy.cpp, SessionCommandDispatcher.cpp: negotiation, peer-only radio property, command metadata/dispatch/result, participant authorization. Reuse SliceOwnership::matches/controlRevision, SliceAccessPolicy::mayChange, current requester/session guards and existing command.result values. tests/data/link/v1/surface.json plus a focused session fixture and necessary Core contract docs; no protected phone docs or ios edits.

ReceiverManager publishes the complete receiver map and active-stream mask under its routing lock, once per changed plan. Frequency replay observes only the complete final map and retains each receiver's stored centre (including CTUN), and unchanged plans do not replay frequencies. ReceiveLayoutStore persists the requested owner alongside the stable receive roster.

## Exact decoding shape

All keys shown above are required. Top level: version:i64 (=1), revision:i64 (0..2^53-1), requested/running/paused:bool, reasonCode/reason:string, live:object|null, targets:array of objects. live fields: sliceId:i64 (0..4 at this baseline), incarnation/controlRevision:i64 (0..2^53-1), controllerDeviceId/letter:string, band:i64 (0..27, excluding Count28), frequencyHz:f64 (finite, nonnegative, no receive-layout admission ceiling), phaseDeg:f64 (finite 0..360), gainDb:f64 (finite -20..20), fineNullEnabled:bool, pattern:string|null. pattern is the **existing unmodified diversityPattern JSON string**, present only for a peer that negotiated diversityPattern:1; absent pattern negotiation means null. Off means live:null; targets remain available. Target object fields exactly sliceId,incarnation,controlRevision,controllerDeviceId,eligible,reasonCode,reason with the same types; no nullable target fields. Targets bounded by WdspEngine::kMaxSliceChannels (currently 5; never exceed negotiated/Core roster bounds), deterministic ascending sliceId order; no unbounded client-supplied strings/pattern/roster input. Core summarizes only its known SliceModels/ownership and existing bounded pattern samples. Existing control-frame/JSON/value limits still apply.

Command args are exactly enabled:bool, stateRevision:i64, sourceSliceId:i64, sourceIncarnation:i64, sourceControlRevision:i64, targetSliceId:i64, targetIncarnation:i64, targetControlRevision:i64. No optional fields or caller identity input. Result values exactly diversityState:utf8 and reasonCode:utf8. The summary value in command results follows the requesting peer's pattern negotiation. Refusal uses current Core owner/freeze formatters where applicable. No global transmitting ban is added; approved resource pause preserves requested owner and normal blend writes. A candidate assignment must keep every unrelated live stream supplied at its current rate with its tuning/ADC identity and ownership intact, or be refused; no collateral operator receiver takeover. Physical DDC renumbering is allowed only with complete safe reassignment preserving the receiver.
