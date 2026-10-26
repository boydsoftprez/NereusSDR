# Remote transmit stop: hold first, cut later

Status: proposed, waiting for JJ's sign-off (2026-10-26). Nothing here is
built. Changing transmit safety behavior needs his explicit yes.

Author: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

## 1. Why

JJ, 2026-10-26: "ours is cutting tx too quickly and creating frustrating user
experience we want to protect stuck tx but this is too aggressive."

Today a remote device's transmission is unkeyed the moment its keepalives go
quiet for 400 ms. A carrier, a two-tone or a voice over goes off the air and
the operator has to press again. On a link that drops a few hundred
milliseconds now and then (a web relay, RF getting into the shack Ethernet on
2026-10-23/24), that is a cut every few minutes.

This design keeps the fast protection against a stuck carrier and moves the
unkey later. When the link goes quiet the Core first holds the radio silent
while staying keyed. It only unkeys if the silence lasts.

## 2. What cuts JJ today

These are the watchdog stops found in the logs that still exist (the session
archives' copies of Core logs, 2026-10-11 to 2026-10-23). The Core's own log
files on the STRIX keep only the last few runs (since 2026-10-25 22:29), the
Rock's since 2026-10-16. None of those has a stop, because there was no remote
transmitting in them.

| Date | Stops | Device | What it was keying |
| --- | --- | --- | --- |
| 2026-10-15 | 1 link-quiet, 1 no-mic-audio | phone (5b83cc96), iPhone 18 | voice |
| 2026-10-16 | 2 | Mac remote window (85c8ce85) | voice, TUNE |
| 2026-10-17 | 4 | Mac remote window | voice, TUNE |
| 2026-10-23 | 18 in 18 minutes (14:43 to 15:01) | Mac remote window | TUNE, two-tone, PS two-tone |

What the 25 link-quiet stops have in common:

- Every one was judged at 401 to 403 ms of silence, right at the deadline.
  The log cannot say how long each silence would have lasted.
- 16 had their last keepalive on the media "tx" data channel, 4 on the
  transmit-watch link, and 5 had no keepalive at all in that key ("no
  keepalive heard in this watch"): the key started and its first keepalive
  never arrived within 400 ms.
- On 2026-10-23 JJ ruled the cause RF on the STRIX-to-router Ethernet, not
  code (pings showed only that cable dropping). The watchdog worked as
  designed. The design was the problem: a 400 ms network hiccup cost the
  whole transmission.

Gaps that recovered by themselves are logged too ("Keepalive gap from", 250 ms
or more between two keepalives on one path, 22 logged): 252 to 402 ms for 19
of them, then 426, 839 and 910 ms. A deadline of about 1 s would have kept
every one of them on the air.

## 3. Today's shape

Every way the Core stops or refuses remote transmit, with where it lives.

| Layer | Rule | Number | Code |
| --- | --- | --- | --- |
| Client keepalive | while keyed, TUNE, two-tone or VOX armed: one keepalive per tick, on the "tx" data channel if open, else the session; the transmit-watch link gets its own copy | every 100 ms | `RemoteTransmitClient.cpp:203-249`, `RemoteTxWatchdog.h kKeepaliveIntervalMs` |
| Release fence | no keepalive on any path while an unkey is being sent or waits for its accepted result | | `RemoteTransmitClient.cpp:213, 226, 240` |
| Core watchdog | no counted keepalive for more than the deadline: StopAllTx, VOX that device armed turned off | 400 ms | `RemoteTxWatchdog.h kLinkLossDeadlineMs` |
| Link closed | the device's session ends while keyed: StopAllTx at once | 0 ms | `StationServer.cpp:5385-5405` |
| Cut guard | the same 400 ms judged on its own thread from "tx" channel receipts, so a stalled main thread still cuts (RF gate closed, then the connection's cut latch) | 400 ms | `RemoteTxCutGuard.h`, `TransmitCutLine.h` |
| Mic starvation | no audio on the device's mic line while keyed: AM, SAM, FM, DRM, RADE unkey; SSB, CW, DIGI, SPEC stay keyed; TUNE and two-tone never | 250 ms | `StarvationPolicy.h`, `RemoteMicReceiver.h kStarvationMs` |
| Time-out (TOT) | Thetis's MOX time-out. Station and computers: off, 180 s. Phones and tablets: on, 180 s | 180 s | `RadioModel.cpp:30202-30239` |
| Ordering | jitter buffer 120 < starvation 250 < link loss 400 < client's first reconnect 1000 | | `RemoteTxWatchdog.h` static_asserts |
| Radio, P2 | byte 38 (hardware watchdog) forced on: the radio drops to standby when the Core stops sending control packets | about 2 s | gateware (section 4) |
| Radio, P1 ANAN | no host-silence watchdog | none | gateware (section 4) |
| Radio, HL2 | each TX sample carries PTT, so an empty TX buffer unkeys; a slower watchdog backs it | about 20 to 35 ms; about 11 to 13 s | gateware (section 4) |

How they interact:

- Paths. A keepalive is counted once per sequence, whichever path brings it
  (`RemoteTxWatchdog::keepalive`, sequence newer than the last). Today the
  client sends each sequence on one of channel or session, plus the
  transmit-watch copy. Losing the "tx" channel alone, with the session fine,
  is a quiet link. PR 652 (open, held on this design) sends the session copy
  too.
- Starvation before link loss. In AM and FM a dead link starves the mic line
  at 250 ms, before the 400 ms watchdog, so the starvation cut fires first.
  Both are cuts.
- The cut guard only helps when the Core's main thread stalls; it judges the
  same 400 ms, so it adds no tolerance.
- The radio's own watchdog guards the Core-to-radio link, not the
  client-to-Core link. It is the only guard if the Core process or host dies.
- The release fence means an unkey that is slow to be accepted is followed
  by a watchdog stop. That is harmless: the operator asked to stop.

## 4. What others do

Every fact is from source or official docs unless marked unconfirmed. Full
notes and citations: the research report "Remote transmit stop timeouts" in
the session notes folder (`tx-stop-research/`).

| System | Liveness signal | Gives up on a quiet client | On a closed connection | On missing TX audio | TX time-out |
| --- | --- | --- | --- | --- | --- |
| Flex radio (SmartSDR API) | client `ping` once a second, after `keepalive enable` | radio drops the client after 15 s (API wiki). Whether that unkeys is unconfirmed | | no unkey found | `interlock timeout`, minutes, set by the operator |
| AetherSDR client | ping every 1 s (`RadioModel.cpp:5511`) | client disconnects after 5 missed pings, 15 on a Poor link (`RadioModel.h:1846-1851`); leaves unkeying to the radio | TCI/CAT client gone: unkey | nothing | none (20 s cap for scripts only) |
| FlexLib (Flex's client library) | ping every 1 s | after 17 s with nothing heard (`Radio.cs:2587, 2627`) | | | |
| piHPSDR client/server | ping every 1.1 s (`client_thread.c:532-563`) | 30 s socket receive timeout, then MOX off (`server_thread.c:924-934, 1145-1148`) | MOX off at once | feeds zeros, stays keyed (`server_thread.c:384-404`) | none |
| deskHPSDR | no client/server mode | | TCI owner's connection closes: unkey at once (`tci.c:5538-5576`) | | none |
| Thetis v2.10.3.15 | | no remote client concept; radio data missing for 3000 ms: the radio is called lost and the console powers off, dropping MOX (`network.c:655-667`, `console.cs:21330-21348`) | CAT/TCI client gone: stays keyed (`console.cs:2357, 2506`); TCI CW key released after 3000 ms | | MOX TOT off by default, 30 to 1800 s, default 180 (`TimeOutTimerManager.cs:136-202`); ping TOT off by default |
| P2 radios, byte 38 set | control packet at least once a second | Angelia and Anvelina gateware: 2.0 s (250,000,000 counts at 125 MHz); Saturn/G2: about 1 to 2 s, FPGA guard 1.97 s | | no underrun unkey | |
| P1 ANAN radios | | none: MOX follows the last control frame, last I/Q sample held | | | |
| Hermes-Lite 2 | | TX buffer drains: out of TX in about 20 to 35 ms; watchdog about 11 to 13 s (computed from gateware, unconfirmed) | | | |

What the earlier quick pass got right and wrong:

- Flex/AetherSDR ping about every 1 s: right. Client give-up 5 to 15 s:
  right for AetherSDR (FlexLib is 17 s). Radio deadline unconfirmed: wrong,
  the API docs give 15 s; what is unconfirmed is whether it unkeys.
- piHPSDR 1.1 s ping, 30 s, never cuts on mic starvation: right, but the
  30 s is a socket receive timeout, not a ping watchdog.

The pattern elsewhere: nobody unkeys in under a second. The fastest automatic
stop in any of them is about 2 s (the P2 radio itself) or 3 s (Thetis), and
the client-loss deadlines are 5 to 30 s. NereusSDR's 400 ms is 7 to 75 times
stricter, and it is the only one that also unkeys on 250 ms of missing audio.

## 5. The proposed shape

Two stages for a quiet link and for a starved mic line: hold, then cut. The
hold takes RF off the air at once and keeps everything else keyed, so the
stuck-carrier protection is as fast as today. The cut, the unkey the operator
has to recover from, moves from 400 ms to 3 s.

### 5.1 The numbers

| Name | Today | Proposed | Why this number |
| --- | --- | --- | --- |
| Keepalive interval | 100 ms | 100 ms (same) | cheap; recovery from a hold is noticed within one tick |
| Keepalive paths | channel or session, plus watch link | every open path: channel, session and watch link (PR 652) | a sequence is counted once, so copies cost nothing; losing one path is no longer a quiet link |
| Link hold (new) | | 400 ms without a counted keepalive | the old cut point: RF stops exactly as fast as today |
| Link cut | 400 ms | 3000 ms without a counted keepalive | Thetis's own lost-radio time (3000 ms, `network.c:655-667`); above every recovered gap logged (910 ms max); below AetherSDR's 5 s |
| Link closed | at once | at once (same) | a closed session is certain, not a guess |
| Mic hold (new) | | 250 ms with no audio, AM, SAM, FM, DRM, RADE | the old starvation point: no bare carrier past 250 ms |
| Mic starvation cut | 250 ms in AM/FM/RADE; never in SSB/CW/DIGI | 10 s with no audio packets in every mode while keyed on a mic line; TUNE and two-tone exempt as today | closes the gap PR 652 opens: a live session with a dead audio path no longer holds a key forever |
| Cut guard (off main thread) | cuts at 400 ms | holds at 400 ms (RF gate only), cuts at 3000 ms | same stages when the main thread stalls |
| TX time-out | off for computers, 180 s for phones | no change | a stuck press on a live link is what it is for; changing the default is not needed for this |
| Radio watchdogs | P2 byte 38 forced on | no change | they guard the Core-to-radio link |

New compile-time ordering, replacing today's chain:

    jitter buffer 120 < mic hold 250 < link hold 400 < first reconnect 1000 < link cut 3000

The old rule "the Core stops before a client's first reconnect" becomes "RF is
off before a client's first reconnect". A reconnect still finds transmit
stopped: the old session ends when the device signs in again, and a closed
session cuts at once (section 7 checks this).

### 5.2 What a hold is

- The TX channel's RF gate closes (`TxChannel::closeRfGate`, the same call the
  cut line already makes first). The radio gets zero I/Q, so nothing is
  radiated: no carrier in TUNE, AM or FM, no tone in two-tone, no voice.
- MOX, PTT out, the T/R relays, the amplifier's key line and the transmit
  holder all stay as they are. Nothing switches under power, the PGXL and
  TGXL are not walked through standby, and the key's epoch stays valid.
- The TX time-out keeps counting from key-down. A hold is not a new key.
- When a counted keepalive arrives (or audio returns, for a mic hold) and the
  other hold, if any, has also ended, the gate reopens on the next TX block.
  The operator does not press anything.
- If the silence reaches the cut, the Core does exactly what it does today at
  400 ms: StopAllTx with "The link to <device> went quiet, so the Core
  stopped transmitting.", VOX that device armed turned off, stop reason
  linkLost.

### 5.3 What the operator sees

| Event | On the air | Desktop remote window and phone | Core's own window | Recoverable? |
| --- | --- | --- | --- | --- |
| Gap under 400 ms | no change | nothing | nothing | nothing to recover |
| Hold, 400 ms to 3 s | silent: a dropout in the voice or carrier | the transmit button stays lit; once the link is back the TX indicator shows "Held" for the hold's length (txState gains the hold, published when it starts and ends) | TX indicator shows "Held" in amber while held | yes, by itself, the moment the link is back |
| Cut at 3 s | off | as today: transmit off, the stop reason shown in plain words | as today | press again |
| Session closed | off at once | as today | as today | press again after reconnecting |
| Mic hold (AM/FM/RADE) | silent | same as a link hold | same | yes, by itself |
| No audio for 10 s | off | transmit off, "No microphone audio arrived from <device>, so the Core stopped transmitting." | as today | press again |

The "Held" wording and its color are the visual design call; this design only
says the state is published and shown. The Core logs every hold with its
length and how it ended, so the numbers can be checked against real links.

### 5.4 What this protects against, and what it does not

- A client that crashes, loses Wi-Fi or a relay: RF off in 400 ms (as today),
  unkeyed in 3 s.
- A client whose audio path dies while the session lives (the PR 652 case):
  RF off in 250 ms in carrier modes, unkeyed in 10 s in every mode.
- A Core whose main thread stalls: the cut guard holds at 400 ms and cuts at
  3 s from its own thread.
- Not covered by this design: the Core process or host dying. Then only the
  radio's watchdog is left. P2 radios stop in about 2 s and HL2 in tens of
  milliseconds, but P1 ANAN radios have no watchdog at all and would hold
  their last sample. That is a separate follow-up.

## 6. PR 652, folded in

PR 652 sends the session copy of each keepalive next to the "tx" channel copy.
The review (`pr-review-20261021/review-652.md`) found it safe for the release
fence and the copies, and raised one design point: it removes a cut (a dead
"tx" channel with a live session no longer stops transmit), and in SSB a key
with no audio could then stay up as long as the session lives.

In this shape:

- Sending on every open path is the keepalive rule, so PR 652's change is
  part of the design and can merge with it (its two test fixes from the
  review applied).
- The cut it removes comes back where it belongs: the 10 s no-audio cut
  covers a dead audio path in every mode.
- `tst_tx_link_loss_each_path::dataChannelSeveredStopsWithin500ms` becomes
  "the data channel severed with the session alive keeps transmitting", and
  a new case severs every path and expects a hold at 400 ms and a cut at 3 s.

## 7. Test plan

Unit (fake clocks, no radio):

- `tst_remote_tx_watchdog`: hold at 401 ms, not at 399; a keepalive at 1500 ms
  ends the hold and the device stays watched; cut at 3001 ms, not at 2999;
  the cut sentence and VOX disarm as today; a copy or an old sequence never
  ends a hold; an old epoch never ends a hold; the key's own release during
  a hold ends the watch with no cut.
- `tst_remote_tx_cut_guard`: holds at 400 ms and cuts at 3000 ms off the main
  thread; a heard during the hold ends it; the cut runs once per arm.
- `StarvationPolicy` / `RemoteMicReceiver` tests: mic hold at 250 ms in AM,
  FM, RADE, released by audio; no hold in SSB, CW, DIGI; the 10 s cut in
  every mode; TUNE and two-tone never held or cut by the mic.
- `tst_remote_transmit_client`: every open path gets each sequence (PR 652's
  cases with the review's two fixes); the release fence still blocks all of
  them.
- Holds and the TX time-out: a hold does not restart or pause the time-out.
- Two holds at once (link and mic): the gate opens only when both end.

Integration:

- `tst_tx_link_loss_each_path`: each path severed alone keeps transmitting;
  every path severed holds at 400 ms and cuts at 3 s; the session closing
  cuts at once; the device signing in again while held ends the old session
  and finds transmit stopped.
- `tst_tgxl_core_tune`: a link hold during a Tuner Genius tune holds the
  carrier and the cycle survives a short hold; a cut ends the cycle with its
  usual reason.
- The amplifier: a hold leaves the PGXL and TGXL states alone.

On JJ's hardware, his call each time (nobody else keys the radio):

- Mac remote window over the web relay: VOX, a voice over, TUNE and two-tone;
  look for holds in the Core log and no cuts.
- During a TUNE, pull the client's network: RF gone within half a second,
  transmit off at 3 s, the reason shown, a new press works after reconnect.
- The 2026-10-23 setup (PGXL in operate, the STRIX cable): count holds and
  their lengths for an evening, with the ping loop from the RF memory note
  running, to check the 3 s against real drops.

## 8. Decisions this asks for

One decision: approve the hold-then-cut shape with the numbers in 5.1 (hold at
400 ms link / 250 ms mic, cut at 3 s link / 10 s no audio, keepalives on every
path). Recommended: yes.

Settled in this design, unless JJ says otherwise:

- No change to the TX time-out defaults.
- No change to the radio watchdogs.
- The hold is shown as a state; its words and color follow the existing TX
  indicator's style.

## 9. Follow-ups outside this design

- P1 ANAN radios have no host-silence watchdog: if the Core dies while keyed,
  the radio keeps transmitting its last sample. Worth a design of its own.
- Angelia v12 P2 gateware clears run but leaves PTT latched after its 2 s
  timeout, so a reconnect should send MOX off before (or with) run. Needs a
  check of what NereusSDR sends today.
- The protocol research flagged `P1RadioConnection.cpp:5521-5522`:
  `sendMetisStart` sends 0x02 when `iqAndMic` is true, which the Metis spec
  reads as wideband only. Unverified; needs a check.
- Logs: the STRIX keeps only a few runs, so cut history is lost within a
  day. Keeping a week of Core logs would make the next review easier.
