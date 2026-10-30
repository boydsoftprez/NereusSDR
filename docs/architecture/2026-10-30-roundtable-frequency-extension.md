# Roundtable frequency extension 1

The approved frequency-first board is implemented as an additive negotiation on
Roundtable wire v1, and station-link Roundtable capability 2. Signing transcripts,
existing v1 commands and legacy response shapes keep their existing meanings.
The public UI source is `roundtable/server/static`, not the protocol documentation
under `website/public/roundtable`.

## Negotiation and actions

Station hello and live watchBand may request `frequencyVersion: 1`. Updated servers
advertise it in welcome or rooms. A client sends extension kinds only after that
advertisement. An older server remains a legacy read-only fallback for the new UI;
it cannot promise frequency chat.

- Station `postAt` requires `slice`, `hz`, `family`, `text`, `clientId`. The selected
  slice must match the station's current accepted exact presence. A stale composer
  is refused, never retargeted. Consent, hidden, away, rate, text and block gates
  still apply. RF room membership and dwell do not authorize this operation.
- Station `readAt` / `unreadAt` and live `watchAt` / `unwatchAt` take `hz`, `family`.
  A separate frequency-view registry preserves RF room identity and caps readers.
- Full room and delta envelopes carry `view: "frequency"`; resync includes that
  marker to distinguish a frequency context from an RF room with the same ID.
  Frequency delta `left` entries are `sourceRoom/ref`; RF deltas keep bare refs.
- Current chat is same family and absolute sender/selected frequency difference
  at most 5,000 Hz, inclusively, across RF room boundaries. A/B/C at 0/5,000/10,000
  Hz remain pairwise: A and C do not receive each other's new chat.

Stored messages retain accepted sender Hz and family. Historical rows without
those facts stay unknown; room centres are not substitutes for exact frequency.
Received device history retains sender frequency and time across tuning. Drafts
and retries retain their original slice/family/Hz context.

## Public data and attribution

Band summaries optionally contain `stations` for every public Placement, including
joining slices: transient ref, sourceRoom, slice, exact hz, family, callsign, nth,
grid, keyed, away and joining. No stable station identity is added to public data.
Cochannel stacks group exact Hz; layout overlap folding is a separate operation.

Members add exact hz, sourceRoom, slice and nullable bearingDeg. Their existing
reading.turn identifies the globally unique turn being reported. Member km and
bearingDeg belong to that reading's track; clients join the turn first. Talker
snapshots retain hz/grid/sourceRoom, and turns retain hz/sourceRoom. Unknown and
doubled turns have no guessed path or named archive attribution. Reporter rows
that leave are not invented by clients.

Initial true bearing is clockwise from true north, from the identified talker to
the reporting station. Missing, coincident or antipodal locator centres have no
unique direction. The original spherical-geodesy helper derives coordinates from
the existing Maidenhead definition. Integer displayed bearings wrap modulo 360.
Legacy archive distance can be displayed, while unrecorded historical bearing
stays blank. New endpoint and path snapshots are added without deleting old data.

## Native station link

Peers declaring roundtable 2 receive stationRoundtableVersion 2. Version 1 peers
retain their original capability and record projections.

| Command | Ordered arguments | Result |
| --- | --- | --- |
| roundtable.postAt | sliceId i64, hz i64, family utf8, text utf8, clientId utf8 | Eventual posted messageId or refusal, with existing per-connection correlation |
| roundtable.readAt | hz i64, family utf8 | stream utf8: roundtableFrequency:family-hz, created before answer |
| roundtable.unreadAt | hz i64, family utf8 | Release this connection's reader |
| roundtable.history | ids utf8 containing a bounded JSON integer array | states utf8 containing correlated message state/text results |

The roundtableRooms stream carries a version-2-only `frequency-capability` record
with fields `{kind:"frequency", version:1}` only after actual server negotiation.
This record also establishes support for an empty band; it never appears as a
room or person. Version 1 peers receive neither that upsert nor its removal.
Core version 2 alone does not imply server support.

Frequency records reuse member/reading/turn/name/message types. Members and
readings use sourceRoom/ref identities; turn and message IDs remain global.
Station preferences use AppSettings and the existing settings proxy:
Roundtable/DistanceUnits = mi (default) or km; Roundtable/ShowBearing = True
(default) or False. Convert kilometres by 1.609344 for presentation only. Sort by
canonical distance; changing units does not change bearing or report attribution.

## Website authority

Public browsing never tunes a radio and is read-only. The authenticated owner
follow surface derives current context from accepted Core presence:

- GET /v1/me/follow returns time, frequencyVersion, live, atRadio and actual slices.
- POST /v1/me/postAt requires csrf, slice, hz, family, text and clientId.
- POST /v1/me/nameAt requires csrf, slice, hz, family, turn and name.
- POST /v1/me/delete requires csrf and msg and checks actual station ownership.
- POST /v1/me/mentionsSeen requires csrf and a nonnegative, nonfuture integer t;
  the persisted seen mark is monotonic.
- Authenticated follow additionally supplies bounded ownMessageIds (at most 500),
  mentionsSeenMs and up to 50 mentions. These do not appear on public endpoints.
- GET /v1/api/frequency/family/hz provides an explicit public frequency snapshot.

Existing cookie, Origin and CSRF checks apply. A callsign or browser-picked
frequency does not grant station authority. Distant and earlier browsing stays
read-only. The phone retains its approved no-Archive policy.


## Retained received history and moderation

Frequency extension 1 adds a read-only `history` request on the negotiated station
and live connections. Its fields are `clientId` and `ids`, a unique array of at
most 200 positive message IDs. The correlated `history` reply contains `states`,
with exactly the requested IDs and their current `state` and `text`. A shown
message uses its actual stored text; deleted, removed, expired and unknown IDs
have null text, with unknown IDs represented as expired. This lookup returns no
new IDs, people, locations, station identity or unrelated message text.

The Core validates the same bound on the station-link `roundtable.history`
command. Its UTF8 `ids` argument and UTF8 `states` result encode the respective
JSON arrays. It is a version-2-only asynchronous command, correlated separately
for each device and bounded to sixteen pending requests with the existing
30-second answer deadline. It creates no additional subscriptions or streams.

Clients retain at most 200 actually received messages. They refresh known IDs on
reconnection, tuning and earlier-history exposure, and every ten seconds while
history exists. Existing active-view deltas still apply immediately. Responses
update only existing cached state/text, never nearby message discovery or
mention exposure. Per-message revisions prevent a late response from restoring
text removed by a newer live update. Unvalidated and offline history is visibly
identified. Bounded status refresh also observes expiry performed by a separate
SQLite maintenance process without relying on an in-process mutation listener.
