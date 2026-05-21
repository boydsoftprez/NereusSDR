# 4O3A LAN PTT Chain: Pcap-Canonical vs NereusSDR: Divergence Analysis and Change Spec

**Status:** Research + change spec (approval-gated)
**Date:** 2026-05-21
**Author:** J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
**Scope (locked):** SmartSDR-API handshake + PTT key/unkey chain. Excludes periodic status broadcasts, autotune protocol, slice/transmit pushes (handled elsewhere).
**Verification (locked):** Manual 10-cycle bench matrix with clean amp power cycle between each row.

## 1. Background

The 4O3A LAN PTT chain (NereusSDR's SmartSDR API listener on TCP 4992 acting as a FlexRadio for PowerGenius XL and Tuner Genius XL amplifiers) has been chased through 6+ commits over April-May 2026. The last bench-confirmed-working state is commit `01ca5b82` (the "12-fix checkpoint"). Multiple subsequent attempts to improve it (pcap-faithful frame format, settle delays, meter-create ID returns, UNKEY_REQUESTED insertion) each regressed something else. The 2026-05-21 bench session ended with:

- TGXL display reports **"no PTT in"** intermittently to consistently
- PGXL hardware reports **"high SWR"** on key-down 50% of the time and on un-key 100% of the time
- PGXL `interlock ready` ACK latency is **305 ms** vs the canonical pcap's **26 ms** (~12x slower)
- TGXL **never** sends `interlock ready`: we cover by always-grant on 500 ms timeout

This doc grounds the next change set in pcap evidence and ends the iterate-and-hope cycle. No code is written until the change set is approved.

## 2. Reference Materials

| File | Role | Notes |
|---|---|---|
| `captures/flex-tgxl-direct-CONTROL.pcapng` | Pcap A | 10,407 TCP-4992 frames. Radio-side capture; FLEX-8600 is `192.168.109.85`. Covers a full PGXL+TGXL session including amp connect, several PTT cycles, autotune. **Primary reference for canonical FLEX behavior.** |
| `captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng` | Pcap B | 2,577 TCP-4992 frames. SmartSDR-Win-side capture. Cross-references Pcap A for amp connect ordering. |
| `/tmp/nereus-maxbin-debug/log.txt` | Bench log | 15,511 lines from the 2026-05-21 17:24+ session. NereusSDR's SmartSDR listener is at `192.168.109.52`. Logs every RX/TX frame at info level. |
| `src/core/SmartSdrApiListener.cpp` (1,034 lines) | Code under review | Currently at the 01ca5b82-restored state after this session's reverts. |
| `docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md` | Prior design doc | First-pass pcap analysis. Some of the wire format docs below extend its work. |

## 3. Identity Convention

Throughout this doc:

| Symbol | IP | Role | Wire identity |
|---|---|---|---|
| **FLEX** | `192.168.109.85` (Pcap A) / `192.168.109.6` (Pcap B) | The real radio | banner handle: random 8-hex (e.g. `0x4C70DC9B`) |
| **SmartSDR-PC** | `192.168.109.19` | Windows GUI client | `tx_client_handle` in PTT frames: `0x66B137B7` |
| **TGXL** | `192.168.109.234` | Tuner Genius XL | banner handle: `0x096016A4` (Pcap A: verified via `S0\|amplifier 0x096016A4 ... model=TunerGeniusXL ip=192.168.109.234`) |
| **PGXL** | `192.168.109.235` | Power Genius XL | banner handle: `0x22E8213A` (Pcap A: verified via `S0\|amplifier 0x22E8213A ... model=PowerGeniusXL ip=192.168.109.235`) |
| **NereusSDR** | `192.168.109.52` (bench) | Our listener | banner handle: random per-client, our convention |

Key protocol terms:

- **banner handle**: 8-hex assigned to a client on TCP accept. Sent as `H<hex>\n` to the client. Used by the client as the prefix on its `S<handle>|...` frames.
- **amp handle**: the amp's identity on the wire. In canonical FLEX = the amp's banner handle. The amplifier's state and per-amp messages are addressed by this handle.
- **interlock id**: assigned by FLEX in response to `C<seq>|interlock create`. The amp uses this id in its `C<seq>|interlock ready <id>` ACK.
- **tx_client_handle**: the handle of the GUI client that initiated TX. In Pcap A this is `0x66B137B7` (SmartSDR-Win). In our system we have no separate GUI client and have been using an amp's own handle here. This is a divergence (see §7 row D2).

## 4. Handshake Timeline

The handshake runs from TCP accept through `keepalive enable`. Below: canonical FLEX behavior alongside ours, aligned at the equivalent event.

### 4a. PGXL handshake (canonical, Pcap A T+206.131)

| T+ (ms) | Δ | Direction | Frame |
|---:|---:|---|---|
| 0 |  | PGXL → FLEX | `C0\|amplifier create ip=192.168.109.235 port=9008 model=PowerGeniusXL serial_num=10-200/24-0046 ant=ANT1:PORTA,ANT2:NONE` |
| ~42 | +42 | PGXL → FLEX | `C1\|meter create name=FWD type=AMP min=30.0 max=63.01 units=DBM` |
| ~122 | +80 | PGXL → FLEX | `C2\|meter create name=RL type=AMP min=0.3 max=60.0 units=DB` |
| ~202 | +80 | PGXL → FLEX | `C3\|meter create name=DRV type=AMP min=10.0 max=50.00 units=DBM` |
| ~282 | +80 | PGXL → FLEX | `C4\|meter create name=ID type=AMP min=0.0 max=70.0 units=AMPS` |
| ~363 | +81 | PGXL → FLEX | `C5\|meter create name=TEMP type=AMP min=0.0 max=100.0 units=TEMP_C` |
| ~529 | +166 | PGXL → FLEX | `C7\|sub slice all` |
| ~621 | +92 | PGXL → FLEX | `C8\|interlock create type=AMP valid_antennas=ANT1 name=PG-XL serial=10-200/24-0046` |
| ~622 | +1 | PGXL → FLEX | `C9\|keepalive enable` |
| varies |  | FLEX → PGXL | `R<seq>\|0\|` (empty body) for every C-frame above. Bulk-delivered in ~3.7 s window starting at T+3727. Notably **late**: FLEX doesn't respond inline; it batches responses. |

**Key observations:**

1. PGXL creates **5 meters** before `sub slice all` or `interlock create`.
2. `interlock create` includes `valid_antennas=ANT1` (PGXL specifies which antenna port it's TX-licensed on).
3. FLEX's R-frame body is **empty** for `meter create` (no id returned).
4. PGXL sends `C9|keepalive enable` 1 ms after `interlock create`.

### 4b. TGXL handshake (canonical, Pcap A T+212.147)

| T+ (ms) | Δ | Direction | Frame |
|---:|---:|---|---|
| 0 |  | TGXL → FLEX | `C16\|amplifier create ip=192.168.109.234 port=9010 model=TunerGeniusXL serial_num=241288-1 ant=ANT1` |
| ~497 | +497 | TGXL → FLEX | `C1\|sub slice all` |
| ~497 | +0 | TGXL → FLEX | `C9\|sub amplifier all` |
| ~497 | +0 | TGXL → FLEX | `C4\|interlock create type=AMP name=TG serial=241288-1 valid_antennas=ANT1` |
| ~499 | +2 | TGXL → FLEX | `C13\|meter create name=FWD type=AMP min=30.0 max=63.01 units=DBM` |
| ~1000 | +501 | TGXL → FLEX | `C13\|meter create name=FWD ...` **(retry)** |
| ~3000 | +2000 | TGXL → FLEX | `C13\|meter create name=FWD ...` **(retry)** |

**Key observations:**

5. TGXL pre-batches `sub slice all + sub amplifier all + interlock create + meter create` into a single ~500-byte TCP segment with `\n` separators (verified by the bench log RX line counts).
6. TGXL **only registers 1 meter** (FWD) vs PGXL's 5.
7. TGXL **re-sends `meter create` indefinitely** at ~500 ms cadence even after FLEX responds with empty-body R-frames. Confirmed pcap-canonical normal behavior (see §6 row M1).
8. TGXL includes `sub amplifier all` (PGXL does not). This is how TGXL learns PGXL's amplifier state (band, antenna, operate/standby).

### 4c. Our handshake (bench log 17:24:18 onwards)

| T+ (ms) | Δ | Direction | Frame |
|---:|---:|---|---|
| 0 |  | TGXL → NereusSDR | `C16\|amplifier create ip=192.168.109.234 port=9010 model=TunerGeniusXL serial_num=241288-1 ant=ANT1` |
| 0 | 0 | NereusSDR → TGXL | `R16\|0\|` (empty) |
| 0 | 0 | TGXL → NereusSDR | `C1\|sub slice all` (bench log line 64) |
| 0 | 0 | NereusSDR → TGXL | `R1\|0\|` (empty) |
| 0 | 0 | TGXL → NereusSDR | `C9\|sub amplifier all` |
| 0 | 0 | NereusSDR → TGXL | `R9\|0\|` |
| 0 | 0 | TGXL → NereusSDR | `C4\|interlock create type=AMP name=TG serial=241288-1 valid_antennas=ANT1` |
| 0 | 0 | NereusSDR → TGXL | `R4\|0\|1` ← body carries the interlock id |
| 0 | 0 | TGXL → NereusSDR | `C13\|meter create name=FWD ...` |
| 0 | 0 | NereusSDR → TGXL | `R13\|0\|` (empty: matches pcap) |
| ~177 | +177 | TGXL → NereusSDR | `C13\|meter create ...` **(retry)**: same as pcap pattern |

**Key observations:**

9. Our R-frames are returned **inline** (same millisecond), whereas canonical FLEX batches them with multi-second delay. Functionally this should be equivalent: the client only needs to see R-frames eventually.
10. TGXL DOES send `C1|sub slice all` to us (bench log line 64), and we reply `R1|0|`. Matches canonical handshake order.
11. We return the **interlock id in the R-frame body** (`R4|0|1`). Confirmed canonical: pcap shows FLEX returns `R4|0|5` to TGXL and `R8|0|4` to PGXL for `interlock create` (Appendix A.5).
12. Our meter create response is empty-body, matches canonical (pcap R13|0|).

**Handshake summary:** Our handshake is **structurally pcap-faithful**. No P0 divergence here. All structural differences are inline-vs-batched R-frame delivery, which has no functional impact.

## 5. PTT Key-Down Timeline (TUNE source)

### 5a. Canonical (Pcap A, T+167.678)

| T+ (ms) | Δ | Direction | Frame body |
|---:|---:|---|---|
| 0 |  | FLEX → all subscribers | `S0\|interlock tx_client_handle=0x66B137B7 state=PTT_REQUESTED reason=AMP:TG source=TUNE tx_allowed=1 amplifier=` |
| 17 | +17 | TGXL → FLEX | `C6\|interlock ready 3` |
| 26 | +9 | PGXL → FLEX | `C1128\|interlock ready 4` |
| 56 | +30 | FLEX → all | `S0\|interlock tx_client_handle=0x66B137B7 state=TRANSMITTING reason= source=TUNE tx_allowed=1 amplifier=0x096016A4,0x22E8213A` |
| (post-) |  | FLEX → amp state subscribers | `S0\|amplifier 0x22E8213A state=TRANSMIT_A` (PGXL state flip) |

**Key observations:**

13. **ONE PTT_REQUESTED frame total**, broadcast to every TCP-4992 subscriber. Not per-amp.
14. `reason=AMP:TG` names the **initiating amp** (TGXL here, because TGXL fired the TUNE).
15. `amplifier=` is **empty** in PTT_REQUESTED.
16. `tx_client_handle=0x66B137B7` is the **SmartSDR-Win** client handle. Both amps ACK this with their own interlock id regardless.
17. TGXL ACKs in **17 ms**, PGXL in **26 ms**. FLEX waits **30 ms after the last ACK** before broadcasting TRANSMITTING.
18. **ONE TRANSMITTING frame total**. `reason=` empty. `amplifier=0x<h1>,0x<h2>` comma-separated list of all keyed amp handles.
19. PGXL's amplifier state flips to `TRANSMIT_A` shortly after TRANSMITTING. This is the signal the spectrum/UI consumes to show PGXL is transmitting.

### 5b. Ours (bench log T+17:24:37.799)

| T+ (ms) | Δ | Direction | Frame body |
|---:|---:|---|---|
| 0 |  | NereusSDR → all subs | `S0\|interlock tx_client_handle=0x4788912E state=PTT_REQUESTED reason= source=TUNE tx_allowed=1 amplifier=0x4788912E` (frame 1 of 2) |
| 0 | 0 | NereusSDR → all subs | `S0\|interlock tx_client_handle=0x46B60DEA state=PTT_REQUESTED reason= source=TUNE tx_allowed=1 amplifier=0x46B60DEA` (frame 2 of 2) |
| 305 | +305 | PGXL → NereusSDR | `C26\|interlock ready 2` |
| 479 | +174 | (timeout fires) | `1 of 2 enabled amps acked; laggards= QList("TG")` |
| 479 | +0 | NereusSDR → all subs | `S0\|interlock state=TRANSMITTING ... amplifier=0x4788912E` (frame 1 of 2) |
| 479 | 0 | NereusSDR → all subs | `S0\|interlock state=TRANSMITTING ... amplifier=0x46B60DEA` (frame 2 of 2) |
| 479 | 0 | NereusSDR → each amp | `S<h>\|amplifier 0x<h> pttA=1` per amp |
| 479 | 0 | (local) | RF-flow gate releases, TxChannel starts (RF on-air) |

**Key observations:**

20. We send **two PTT_REQUESTED frames** (one per enabled interlock amp) instead of one.
21. `reason=` is **empty** instead of `AMP:<initiating-amp>`.
22. `amplifier=0x<self>` is the amp's own handle instead of empty.
23. `tx_client_handle=0x<self>` is the amp's own handle instead of a SmartSDR-PC handle. (We have no separate PC client.)
24. PGXL ACKs in **305 ms** vs canonical 26 ms: **~12x slower**.
25. TGXL **never ACKs `interlock ready`**. The 500 ms always-grant timeout fires and we proceed to TRANSMITTING without TGXL's confirmation. TGXL then responds to the `S<h>|amplifier pttA=1` push on the amp path, which makes its "PTT in" display flip, but the interlock state machine on TGXL's side never completed, hence intermittent display lies.
26. We send **two TRANSMITTING frames** instead of one, and our `amplifier=0x<single>` instead of comma-separated list.
27. We send TRANSMITTING + pttA=1 + RF on-air **0 ms after the last ACK** (or after timeout). Canonical: 30 ms settle.
28. We **never push amp state=TRANSMIT_A** ourselves. PGXL flips its own state in its periodic broadcast after we send pttA=1, but the canonical FLEX path is for the **radio** to broadcast the amp state change.

## 6. PTT Key-Up Timeline

### 6a. Canonical (Pcap A, T+168.874)

| T+ (ms) | Δ | Direction | Frame body |
|---:|---:|---|---|
| 0 |  | FLEX → all subs | `S0\|interlock tx_client_handle=0x66B137B7 state=UNKEY_REQUESTED reason=AMP:TG source= tx_allowed=1 amplifier=` |
| 2 | +2 | FLEX → all subs | `S0\|interlock tx_client_handle=0x66B137B7 state=READY reason= source= tx_allowed=1 amplifier=` |
| 2.5 | +0.5 | FLEX → all subs | `S0\|interlock tx_client_handle=0x66B137B7 state=READY reason=AMP:TG source= tx_allowed=1 amplifier=` |
| 6 | +3.5 | FLEX → all subs | `S0\|amplifier 0x22E8213A state=IDLE` |
| 432 | +426 | FLEX → all subs | `S0\|interlock tx_client_handle=0x00000000 state=READY reason= source= tx_allowed=1 amplifier=` |
| 432 | +0 | FLEX → all subs | `S0\|interlock tx_client_handle=0x00000000 state=READY reason=AMP:TG source= tx_allowed=1 amplifier=` |
| ANY |  | (later) | Periodic amp state broadcast includes `pttA=0`, but no dedicated `S<h>\|amplifier 0x<h> pttA=0` ever appears in the pcap. |

**Key observations:**

29. **UNKEY_REQUESTED comes FIRST** (signals amps to start switching back to RX).
30. **READY (empty reason) comes 2 ms after UNKEY_REQUESTED**, then READY (with reason=AMP:<name>) 0.5 ms after.
31. **Amplifier state → IDLE** ~6 ms after UNKEY_REQUESTED.
32. **No explicit pttA=0 push.** PGXL drops carrier based on amplifier state change (which it gets from `S0|amplifier ... state=IDLE` broadcast).
33. **Tx_client_handle resets to 0x00000000** in the second READY pair ~430 ms later (deferred housekeeping; not on critical path).

### 6b. Ours (bench log T+17:24:38.278: current 01ca5b82-restored state)

| T+ (ms) | Δ | Direction | Frame body |
|---:|---:|---|---|
| 0 |  | NereusSDR → each amp | `S<h>\|amplifier 0x<h> pttA=0` per amp |
| 0 | 0 | NereusSDR → all subs | `S0\|interlock tx_client_handle=0x<h1> state=READY reason=AMP:TG source= tx_allowed=1 amplifier=` (per amp) |
| 0 | 0 | NereusSDR → all subs | `S0\|interlock tx_client_handle=0x<h2> state=READY reason=AMP:PG-XL source= tx_allowed=1 amplifier=` (per amp) |
| (no UNKEY_REQUESTED at all) | | | |

**Key observations:**

34. We **skip UNKEY_REQUESTED entirely**.
35. We **explicitly push pttA=0** (NereusSDR-specific divergence; canonical FLEX never does).
36. We send **READY per amp** instead of the canonical 2-frame sequence (empty reason, then with reason).
37. We **never broadcast amplifier state=IDLE**. PGXL infers from periodic broadcasts.
38. The pttA=0 push lands at T+0 (synchronous with the trigger). PGXL un-keys before TGXL has any signal to switch relays back, **producing the brief high-SWR flash JJ observed**.

## 7. Divergence Table

Sorted by hypothesized impact (P0 = canonical-blocking, P1 = high-SWR / no-PTT-in causes, P2 = polish).

| # | Event | Pcap | Ours | Δ | Hypothesized impact | Priority |
|---|---|---|---|---|---|---|
| **D1** | PTT_REQUESTED frame count | 1 frame, broadcast | N frames, one per enabled amp | +N-1 frames | Frame storm; amps may parse N times | P0 |
| **D2** | PTT_REQUESTED `tx_client_handle` | SmartSDR-Win handle | Each amp's own banner handle | Self-referencing | Amps may treat as malformed; could explain TGXL never-ACK | **P0** |
| **D3** | PTT_REQUESTED `reason=` | `AMP:<initiating-amp>` (TG for TUNE) | Empty | Identity loss | Initiating amp not named; possible TGXL state-machine confusion | P1 |
| **D4** | PTT_REQUESTED `amplifier=` | Empty | `0x<self-handle>` | Field mismatch | Combined with D2 and D3, frame format is wrong | P1 |
| **D5** | TGXL ACK latency | 17 ms | Never (timeout) | ∞ | Interlock state machine never completes on TGXL; "no PTT in" intermittent | **P0** |
| **D6** | PGXL ACK latency | 26 ms | 305 ms | 12x slower | Adds latency to every TX; suggests frame format causes slow-path parse | P1 |
| **D7** | TRANSMITTING settle delay (last ACK → broadcast) | 30 ms | 0 ms (synchronous) | -30 ms | RF hits amp path before TGXL relays settle → high-SWR | **P0** |
| **D8** | TRANSMITTING frame count | 1 frame | N frames per amp | +N-1 | Same as D1 | P1 |
| **D9** | TRANSMITTING `amplifier=` | Comma-separated list `0x<h1>,0x<h2>` | `0x<single>` per frame | Format | Format wrong; amps that watch for the multi-amp list miss it | P1 |
| **D10** | Amplifier state push (TRANSMIT_A / IDLE) | FLEX broadcasts `S0\|amplifier 0x<h> state=TRANSMIT_A` and `state=IDLE` | We never broadcast this | Missing | PGXL infers from its own periodic state, but TGXL's view of PGXL state is stale | P1 |
| **D11** | UNKEY_REQUESTED frame | Sent FIRST on un-key | Skipped entirely | Missing | TGXL has no signal to start relay switch-back; we drop pttA=0 immediately and PGXL still keyed → high-SWR flash | **P0** |
| **D12** | READY frame count | 2 frames (empty reason, then with reason) | 1 per amp | Order | Minor; functional impact unclear | P2 |
| **D13** | pttA=0 push on unkey | Canonical FLEX **never sends explicit pttA=0** | We send `S<h>\|amplifier 0x<h> pttA=0` per amp | NereusSDR-specific addition | If timed wrong, contributes to high-SWR flash | P1 |
| **D14** | Settle delay after UNKEY_REQUESTED | ~2 ms (FLEX → READY chain) | Not applicable (we skip UNKEY_REQUESTED) |  | Will need a small settle when we add UNKEY_REQUESTED back | P1 |
| **M1** | TGXL `meter create` retries | Re-sends ad infinitum at 500 ms cadence | Same: already pcap-matching |  | Not a divergence; documented to prevent future misdiagnosis | (Doc-only) |

## 8. Change Set

Ordered by priority. Each change is small enough to test independently. **No code is written until JJ approves this list.**

### Definitions (shared across changes)

- **synthetic local-client handle**: an 8-hex handle generated once at `SmartSdrApiListener::start()` time, stored on the listener as `m_localClientHandle`, distinct from every amp's banner handle. Persists for the lifetime of the listener (re-generated on every `start()`). This is the NereusSDR equivalent of the SmartSDR-Win PC client's `0x66B137B7` in the pcap.
- **initiating-amp name**: the `name=` field from the amp's `interlock create` that triggered TX:
  - For TUNE source = the amp that sent `C<n>|transmit tune on` (typically TGXL: name=`TG`).
  - For MOX source initiated by a remote amp = that amp's name.
  - For **MOX source initiated locally** (operator clicks our MOX button) = the first PGXL-class amp registered with us, or empty string if no PGXL-class amp is registered. (NereusSDR has no separate GUI client; the operator IS the radio. Use empty string in that case; this is acceptable because canonical FLEX uses `reason=` empty in the TRANSMITTING frame anyway and in the PTT_REQUESTED frame the field is informational.)

### P0: canonical-blocking

**C1. Add synthetic local-client handle and use it for `tx_client_handle` in every interlock S-frame.**
- **What:** Generate one synthetic 8-hex handle at `start()`, store as `m_localClientHandle`. Use that value (not any amp's banner handle) in `tx_client_handle=` of every PTT_REQUESTED / TRANSMITTING / UNKEY_REQUESTED / READY frame.
- **Why:** Row D2. Hypothesized root cause of TGXL never ACKing: TGXL may reject interlock frames where `tx_client_handle` equals its own banner handle.
- **File:** `src/core/SmartSdrApiListener.h` (add member `QString m_localClientHandle;`) + `src/core/SmartSdrApiListener.cpp` (generate in `start()`, consume in `setInterlockTransmitting` key + unkey branches and `advanceToTransmittingIfReady`).
- **Bench verification:** After this change alone, bench log should show TGXL sending `C<seq>|interlock ready <id>` within ~30 ms of our PTT_REQUESTED. If TGXL still never ACKs, C1 alone wasn't the root cause; revisit hypothesis before proceeding.

**C2. Refactor PTT_REQUESTED to one frame total, with canonical field values.**
- **What:** Change from N per-amp frames to one `S0|interlock` frame broadcast to all subscribers. Fields: `tx_client_handle=<synthetic from C1>`, `state=PTT_REQUESTED`, `reason=AMP:<initiating-amp-name>` (per Definitions above), `source=TUNE` or `source=MIC`, `tx_allowed=1`, `amplifier=` empty.
- **Why:** Rows D1, D3, D4.
- **File:** `setInterlockTransmitting` key branch.
- **Bench verification:** Wireshark capture of NereusSDR's outbound bytes matches canonical exactly (modulo handle values).

**C3. Refactor TRANSMITTING to one frame with comma-separated amplifier list.**
- **What:** Single `S0|interlock` frame: `tx_client_handle=<synthetic>`, `state=TRANSMITTING`, `reason=` empty, `source=<same as PTT_REQUESTED>`, `tx_allowed=1`, `amplifier=0x<h1>,0x<h2>,...` (comma-separated list of every interlocked amp's handle).
- **Why:** Rows D8, D9.
- **File:** `advanceToTransmittingIfReady`.
- **Bench verification:** Wireshark byte-compare.

**C4. Add 30 ms settle delay after last ACK (or timeout) before TRANSMITTING broadcast.**
- **What:** Wrap the TRANSMITTING broadcast + RF-flow gate release in `QTimer::singleShot(30, ...)`. Applies to BOTH paths: (a) success (all amps ACKed), (b) timeout fallback. Both paths must wait the full 30 ms before proceeding.
- **Why:** Row D7. Direct cause of key-down high-SWR. PGXL has registered carrier presence before TGXL relays have settled to TRANSMIT position.
- **File:** `advanceToTransmittingIfReady` (success path) + `onPttAckTimeout` (timeout path). Both must share the same 30 ms delay sequence.
- **Bench verification:** 10 TUNE cycles, 0/10 high-SWR on key-down (was 5/10). Bench log shows ≥30 ms between "last interlock ready ACK received" and "interlock TRANSMITTING broadcast sent".

**C5. Replace unkey path with canonical UNKEY_REQUESTED → READY (empty) → READY (with reason) sequence.**
- **What:** On unkey, send these frames in this order with no per-amp duplication:
  1. `S0|interlock tx_client_handle=<synthetic> state=UNKEY_REQUESTED reason=AMP:<initiator-name> source= tx_allowed=1 amplifier=`
  2. (~2 ms later) `S0|interlock tx_client_handle=<synthetic> state=READY reason= source= tx_allowed=1 amplifier=`
  3. (~0.5 ms later) `S0|interlock tx_client_handle=<synthetic> state=READY reason=AMP:<initiator-name> source= tx_allowed=1 amplifier=`
- **Why:** Rows D11 + D12. UNKEY_REQUESTED signals TGXL to start relay switch-back BEFORE we tell PGXL we're done. The 2-frame READY pair (empty-reason first, then with reason) is the canonical un-key tail per pcap (T+168.876 → T+168.876735).
- **File:** `setInterlockTransmitting` un-key branch (current 01ca5b82 state has no UNKEY_REQUESTED at all).
- **Bench verification:** 10 unkey cycles, 0/10 high-SWR flash (was 10/10). Wireshark byte-compare against canonical.

### P1: quality

**C6. Stop pushing explicit pttA=1 / pttA=0 frames. Replace with canonical amplifier state broadcasts.**
- **What:**
  - On key-down: replace `S<h>|amplifier 0x<h> pttA=1` with `S0|amplifier 0x<h> state=TRANSMIT_A` for each keyed amp. Emit ~5 ms AFTER the TRANSMITTING broadcast (pcap T+167.734 → T+167.740).
  - On un-key: replace `S<h>|amplifier 0x<h> pttA=0` with `S0|amplifier 0x<h> state=IDLE` for each keyed amp. Emit ~5 ms AFTER the second READY frame in C5 (pcap T+168.876 → T+168.881).
- **Why:** Rows D10 + D13. Canonical FLEX never sends explicit `pttA=` pushes; amps derive PTT-in display state from `S0|interlock state=...` + `S0|amplifier state=...`. Eliminating the pttA pushes also removes the ordering race that contributes to the un-key high-SWR flash.
- **File:** `advanceToTransmittingIfReady` (key-down state push) + `setInterlockTransmitting` un-key branch (un-key state push).
- **Bench verification:** TGXL "PTT in" display still flips correctly on both key-down and un-key. PGXL "TRANSMITTING" display still flips. No regression in display behavior. Wireshark shows `state=TRANSMIT_A` and `state=IDLE` frames ~5 ms after the corresponding interlock-state frame.

### P2: polish (deferred, post-bench)

**C7. Deferred-housekeeping READY pair (~430 ms post-unkey) with `tx_client_handle=0x00000000`.**
- Row D12 tail. After the un-key sequence in C5 completes, schedule a second `S0|interlock state=READY` pair (~430 ms later) with `tx_client_handle=0x00000000` to match the canonical FLEX behavior in pcap (T+169.306). Cosmetic; no observable bench impact. Defer to a follow-up after C1-C6 are bench-confirmed.

**C8. Document why R-frame batching diverges (we are inline, FLEX is deferred).**
- Row 9 (handshake observation). Doc-only; no code change. Add a comment in `dispatchLine` explaining the divergence is intentional and functionally equivalent.

## 9. Ten-Cycle Bench Verification Matrix

Acceptance gate: **all 10 rows must pass** before we declare the LAN PTT chain pcap-aligned. Between each row, **power-cycle both TGXL and PGXL** (10-second off) to reset their state.

| Row | Scenario | Steps | Pass criteria |
|---|---|---|---|
| 1 | **TUNE × OPERATE (clean)** | Tune to 14.200 MHz USB, PGXL OPERATE, no prior TX in session. Press TUNE in TunerApplet. Release. | TGXL "PTT in" persists for full TUNE duration. No PGXL high-SWR. Bench log: TGXL ACKs in <30 ms, PGXL ACKs in <50 ms, 30 ms settle visible, no unkey flash. |
| 2 | **TUNE × STANDBY** | Same as row 1 but PGXL in STANDBY. | No PGXL TX (amp passes through). TGXL still cycles autotune. Bench log: PTT chain still completes (PGXL ACKs via interlock; TGXL relays). |
| 3 | **MOX-LSB × OPERATE** | Tune to 7.200 MHz LSB, PGXL OPERATE. Press MOX in TxApplet. Talk into mic for 3 sec. Release. | Amp keys, voice modulation peaks visible on PGXL screen. No high-SWR. Clean un-key, no flash. |
| 4 | **MOX-LSB × STANDBY** | Row 3 with PGXL STANDBY. | No PGXL TX. TGXL relays still cycle. Clean. |
| 5 | **MOX-CW × OPERATE** | CW mode, PGXL OPERATE. Hold MOX. | Same as row 3 but CW. |
| 6 | **Rapid-fire (5 tunes in 3 sec)** | Press TUNE 5x in 3 sec. | All 5 succeed. No accumulated state errors. No "no PTT in" on TGXL. Last unkey clean. |
| 7 | **Amp power cycle mid-test** | During an active TUNE, power-cycle PGXL. | NereusSDR un-keys gracefully (no orphan PTT_REQUESTED state). Next TUNE after PGXL reboots: clean. |
| 8 | **DDC pan during TX** | During an active MOX, pan the spectrum (move DDC center). | RF stays clean (no high-SWR from misalignment). Un-key clean. |
| 9 | **4O3A toggle during TX** | During an active MOX, toggle the 4O3A master switch OFF then ON. | TX continues (master toggle does not interrupt active TX), or TX cleanly aborts (acceptable). No high-SWR. |
| 10 | **Fresh boot (clean room)** | Cold-boot NereusSDR. Toggle 4O3A on. Wait for both amps to connect. First TUNE after this. | Row 1 behavior on the first try. No "first TUNE is broken" pattern. |

If any row fails, **stop and diagnose** before proceeding. Do not iterate-and-hope.

## 10. Appendix A: Raw Pcap Extracts

### A.1 Handshake (Pcap A, T+206.131 onward: PGXL connect)

```
206.131262  PGXL → FLEX  C0|amplifier create ip=192.168.109.235 port=9008 model=PowerGeniusXL serial_num=10-200/24-0046 ant=ANT1:PORTA,ANT2:NONE
206.173132  PGXL → FLEX  C1|meter create name=FWD type=AMP min=30.0 max=63.01 units=DBM
206.253095  PGXL → FLEX  C2|meter create name=RL type=AMP min=0.3 max=60.0 units=DB
206.333703  PGXL → FLEX  C3|meter create name=DRV type=AMP min=10.0 max=50.00 units=DBM
206.413704  PGXL → FLEX  C4|meter create name=ID type=AMP min=0.0 max=70.0 units=AMPS
206.493987  PGXL → FLEX  C5|meter create name=TEMP type=AMP min=0.0 max=100.0 units=TEMP_C
206.659921  PGXL → FLEX  C7|sub slice all
206.752487  PGXL → FLEX  C8|interlock create type=AMP valid_antennas=ANT1 name=PG-XL serial=10-200/24-0046
206.753539  PGXL → FLEX  C9|keepalive enable
```

### A.2 Handshake (Pcap A, T+212.147 onward: TGXL connect)

```
212.147770  TGXL → FLEX  C16|amplifier create ip=192.168.109.234 port=9010 model=TunerGeniusXL serial_num=241288-1 ant=ANT1
212.644535  TGXL → FLEX  C1|sub slice all
212.644785  TGXL → FLEX  C9|sub amplifier all
212.645333  TGXL → FLEX  C4|interlock create type=AMP name=TG serial=241288-1 valid_antennas=ANT1
212.646416  TGXL → FLEX  C13|meter create name=FWD type=AMP min=30.0 max=63.01 units=DBM
213.147644  TGXL → FLEX  C13|meter create name=FWD ... (retry)
```

### A.3 PTT key-down (Pcap A, T+167.678 onward)

```
167.678679  FLEX → all  S0|interlock tx_client_handle=0x66B137B7 state=PTT_REQUESTED reason=AMP:TG source=TUNE tx_allowed=1 amplifier=
167.695991  TGXL → FLEX C6|interlock ready 3
167.704620  PGXL → FLEX C1128|interlock ready 4
167.734731  FLEX → all  S0|interlock tx_client_handle=0x66B137B7 state=TRANSMITTING reason= source=TUNE tx_allowed=1 amplifier=0x096016A4,0x22E8213A
167.740...  FLEX → all  S0|amplifier 0x22E8213A state=TRANSMIT_A
```

### A.4 PTT un-key (Pcap A, T+168.874 onward)

```
168.874155  FLEX → all  S0|interlock tx_client_handle=0x66B137B7 state=UNKEY_REQUESTED reason=AMP:TG source= tx_allowed=1 amplifier=
168.876205  FLEX → all  S0|interlock tx_client_handle=0x66B137B7 state=READY reason= source= tx_allowed=1 amplifier=
168.876735  FLEX → all  S0|interlock tx_client_handle=0x66B137B7 state=READY reason=AMP:TG source= tx_allowed=1 amplifier=
168.881344  FLEX → all  S0|amplifier 0x22E8213A state=IDLE
169.306275  FLEX → all  S0|interlock tx_client_handle=0x00000000 state=READY reason= source= tx_allowed=1 amplifier=  (deferred housekeeping)
```

### A.5 R-frame bodies for handshake commands (Pcap A: confirms id-in-body for interlock create)

Extracted via `tshark -r captures/flex-tgxl-direct-CONTROL.pcapng -Y "tcp.port==4992 and ip.src==192.168.109.85"` filtered by destination.

FLEX → PGXL (192.168.109.235), handshake window T+206:
```
206.172  R0|0|       (C0|amplifier create: empty body)
206.252  R1|0|98     (C1|meter create FWD    : meter id 98)
206.332  R2|0|99     (C2|meter create RL     : meter id 99)
206.412  R3|0|100    (C3|meter create DRV    : meter id 100)
206.493  R4|0|101    (C4|meter create ID     : meter id 101)
206.573  R5|0|102    (C5|meter create TEMP   : meter id 102)
206.659  R6|0|model="FLEX-8600M",...    (C6|info: full info blob)
206.750  R7|0|       (C7|sub slice all: empty body)
206.752  R8|0|4      (C8|interlock create    : interlock id 4)
206.828  R9|0|       (C9|keepalive enable: empty body)
```

FLEX → TGXL (192.168.109.234), handshake window T+212-332:
```
213.337  R16|0|      (C16|amplifier create: empty body)
213.344  R1|0|       (C1|sub slice all: empty body)
213.344  R9|0|       (C9|sub amplifier all: empty body)
213.344  R13|0|      (C13|meter create FWD: empty body: TGXL re-sends despite this)
332.004  R4|0|5      (C4|interlock create: interlock id 5; deferred ~120 s)
```

**Findings:**
- `meter create` returns the meter id in the R-frame body for PGXL but EMPTY for TGXL. Both are pcap-canonical. Our current behavior (empty body for meter create) matches the TGXL response, NOT the PGXL response. This is **probably** acceptable (PGXL bench-confirmed working at 01ca5b82) but flag as a follow-up if any PGXL meter-display issue surfaces. Not in current change set.
- `interlock create` ALWAYS returns the id in the body (`|4` for PGXL, `|5` for TGXL). Our current behavior matches.
- `amplifier create` returns empty body. Our current behavior matches.

## 11. Appendix B: Raw Bench Log Extracts (2026-05-21 17:24 session)

### B.1 Our handshake response (NereusSDR ← TGXL → NereusSDR)

```
17:24:18.719  RX from .234  C16|amplifier create ip=*.*.*. 234 port=9010 model=TunerGeniusXL serial_num=241288-1 ant=ANT1
              amplifier create -> using client handle "4788912E" as amp handle
              TX R-frame seq= 16 err= 0 body= ""
17:24:18.719  RX from .234  C9|sub amplifier all  →  TX R-frame seq= 9 body= ""
17:24:18.719  RX from .234  C4|interlock create type=AMP name=TG ...
              interlock create -> id= 1 name= "TG"
              TX R-frame seq= 4 err= 0 body= "1"
17:24:18.719  RX from .234  C13|meter create name=FWD ...  →  TX R-frame seq= 13 body= ""
... (TGXL re-sends C13 at ~500 ms cadence; matches canonical)
```

### B.2 Our PTT key-down (T+17:24:37.799)

```
17:24:37.799  TX S0|interlock state=PTT_REQUESTED tx_client_handle=0x4788912E source=TUNE amplifier=0x4788912E  (frame 1 of 2)
17:24:37.799  TX S0|interlock state=PTT_REQUESTED tx_client_handle=0x46B60DEA source=TUNE amplifier=0x46B60DEA  (frame 2 of 2)
17:24:38.104  RX from .235  C26|interlock ready 2
17:24:38.278  WRN interlock ACK timeout (500 ms): 1 of 2 enabled amps acked; laggards=("TG")
17:24:38.278  TX S0|interlock state=TRANSMITTING source=TUNE amplifier=0x4788912E  (frame 1 of 2)
17:24:38.278  TX S0|interlock state=TRANSMITTING source=TUNE amplifier=0x46B60DEA  (frame 2 of 2)
17:24:38.278  TX S<h>|amplifier pttA=1 to all subscribed amps
17:24:38.278  RF-flow gate: interlock TRANSMITTING confirmed: starting TxChannel
```

### B.3 Our PTT un-key (current 01ca5b82-restored state)

```
(timestamp+0)  TX S<h>|amplifier pttA=0 to all subscribed amps
(timestamp+0)  TX S0|interlock state=READY reason=AMP:"TG"   (per amp)
(timestamp+0)  TX S0|interlock state=READY reason=AMP:"PG-XL"   (per amp)
(no UNKEY_REQUESTED at all)
```

## 12. Approval

This doc is the authoritative spec for the next LAN PTT chain change. Approval is JJ's. After approval:

1. Mark this doc Status: **Approved**.
2. Invoke `superpowers:writing-plans` to convert the §8 change set into a step-by-step implementation plan with TDD checkpoints.
3. Implement C1-C5 (P0) first, bench-verify each per §8. Only proceed to P1 (C6) once P0 is green.
4. P2 (C7, C8) deferred to a follow-up doc/PR once P0+P1 are bench-confirmed.
5. Run the §9 10-cycle matrix at the end. Document any failures, **stop**, return to this doc.

Do not iterate-and-hope.
