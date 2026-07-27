# ANAN-G2E disconnect lockup — wire capture analysis

**Date:** 2026-07-27
**Radio:** ANAN-G2E (HermesC10), Protocol 2, firmware 110, `192.168.109.198`
**Clients:** NereusSDR on macOS `192.168.109.52`; Thetis on Windows `192.168.109.58`
**Capture method:** MikroTik `/tool sniffer` TZSP mirror (`filter-ip-address=192.168.109.198/32`)
streamed to the Mac on UDP 37008, collected with `tcpdump`.

Captures themselves are not committed (`captures/.gitignore` excludes `*.pcap*`).
This file is the durable record. A previous round of this same investigation
(2026-05-22) lost its `g2e-tzsp.pcap` and left findings only in commit messages;
see "Prior art" below.

---

## Symptom

Power-cycle the G2E and it works. Connect NereusSDR, use it, then disconnect or
close the app. From that moment the radio will not connect again until it is
physically power cycled. It "starts to connect then nothing happens." Thetis on
the same radio, same network, does not cause this.

---

## Reading the captures

Two gotchas cost a round each:

1. **Capture filter must be by host, not by UDP port.** The 1444-byte
   `CmdHighPriority` frames get IP-fragmented inside TZSP, and continuation
   fragments carry no UDP header, so `udp port 37008` silently drops them and
   every large frame arrives as an unreassemblable offset-0 orphan. Use
   `host <router-ip>`.
2. **The mirror duplicates frames.** Measured factor 2.9x-3.4x, arriving within
   a few hundred microseconds with identical payloads. Any rate measurement must
   deduplicate on (dst port, payload) inside a ~2 ms window first.

---

## Established facts

### The disconnect is not the bug

Stop frames are byte-equivalent between the two clients:

```
THETIS  t=1941.852380  port 1027  seq=00000000  byte4=00  rx0=0ecee101
NEREUS  t=1986.382217  port 1027  seq=00000000  byte4=00  rx0=0f1b3333
```

`byte4=00` is `run=0`; the RX0 frequency is preserved rather than zeroed in both.
In all three captured sessions the radio's last packet coincides with the
client's last packet to the microsecond, so `run=0` is received and honoured and
the radio stops streaming cleanly.

Three prior fixes (`a0514cb1`, `f0e30f20`, `a4cda4e3`) all modified this frame.
It was already correct.

### After our disconnect the radio is dead at layer 2

Confirmed independently twice. The Mac ARPs for `192.168.109.198` and gets no
reply at all:

```
12:52:30.961308  ARP  Who has 192.168.109.198 (40:84:32:b0:b0:8d)? Tell 192.168.109.52
12:52:31.512147  ARP  Who has 192.168.109.198? Tell 192.168.109.52
12:52:32.503097  ARP  Who has 192.168.109.198? Tell 192.168.109.52
```

Zero inbound frames of any kind. Because ARP never resolves, macOS never
transmits our UDP, which is exactly why the connect appears to hang: our packets
never reach the wire. No UDP-level change can recover a radio in this state.

Thetis's disconnect leaves the radio healthy — proven in the same capture, since
NereusSDR connected successfully 22 s after Thetis's second disconnect.

---

## Divergences found, and why each was eliminated

Each was checked against the actual G2E gateware, extracted from
`TAPR/OpenHPSDR-Firmware` → `Protocol 2/HermesC10 (ANAN-G2E)/Hermes_Protocol_2_C10_v11.0.5.qar`
(matches the reported firmware 110). Gateware cited as fact only, per CLAUDE.md.

| Divergence | Verdict |
|---|---|
| `CmdGeneral` bytes 33-36 (Envelope PWM): Thetis 800/100, ours 0/0 | **Dead.** `Envelope_PWM_max/min` are commented out in the `Hermes.v:1511-1512` instantiation and `.EER()` is unconnected |
| `CmdRx`: we set 48 kHz on disabled DDC1-3, Thetis sets 0 | **Harmless.** `Hermes.v:717/741` hold a DDC FIFO in reset from `!C122_EnableRx0_7[d]` regardless of rate, so `fifo_ready` never asserts and `sdr_send` skips it |
| `CmdHighPriority` sequence always 0 | **Not a divergence.** Thetis also sends `00000000` on all four command ports (it never writes the field; `memset` leaves it). Ours is 0 by accident — `composeCmdHighPriority` memcpy's a zero-filled codec buffer over the stamped `m_seqHighPri++`. Left alone deliberately: matching the proven-working baseline beats "fixing" it into a new divergence |
| P1 discovery probe disarming the deadman | **Did not fire.** NereusSDR sent no discovery probe at all during the captured session. Still a latent hazard, see below |

### Latent hazard worth knowing (not this bug)

`General_CC.v:90/106/141` accepts any packet on port 1024 whose byte 4 is `0x00`
as a General command and loads byte 38 bit 0 into `HW_timer_enable`. Our P1
discovery probe (`RadioDiscovery.cpp:601`, 63 zero bytes plus `EF FE 02`) has
byte 4 = 0 and byte 38 = 0, so it would disarm the board's ~2 s deadman
(`Hermes.v:398-414`), which `High_Priority_CC.v:145-147` shows is the only
automatic path that can clear a stuck `run`. Thetis's probes use byte 4 = `0x02`,
which `General_CC` rejects. The ICMP destination-unreachable fallback is detected
at `icmp.v:129` and exported by `network.v:587` but is **never connected** in
`Hermes.v`, so it cannot help either. Not the cause of this bug, but it means a
scan can switch off the radio's last safety net.

---

## The surviving difference: command cadence

Deduplicated for the mirror factor:

| Command | Thetis | NereusSDR (before fix) | ratio |
|---|---|---|---|
| `CmdGeneral` (1024) | 1.03/s, median gap 1001 ms | 3.41/s, median 293 ms | 3x |
| `CmdRx` (1025) | 1.28/s, **bursty** (gaps 2 ms to 5.5 s) | 5.70/s, steady 199 ms | 4.5x |
| `CmdTx` (1026) | **1 per session** | 5.11/s, steady 200 ms | ~110x |
| `CmdHighPriority` (1027) | **1 per session** | 12.47/s, steady 98 ms | ~280x |

Thetis does not poll. In `network.c [v2.10.3.15]`, `CmdHighPriority()` is reached
only from `SendStart():362-369`, `SendStop():372-376`, and per-control state
changes. The only periodic command is `CmdGeneral()` from
`KeepAliveLoop():1428-1437` (`if (prn->run && prn->wdt) CmdGeneral();`), whose
job is feeding the deadman.

Safety check before changing ours: Thetis sends `CmdHighPriority` once per
session yet still receives high-priority status from the radio (src port 1025) at
~2.8/s. The radio's status stream is autonomous, **not** a reply to our polling,
so reducing our cadence costs no status updates.

The capture also shows NereusSDR emitting a `CmdRx` **4.4 ms before** the stop
frame — the last 100 ms heartbeat tick landing between the timer stops and the
send. Each `CmdRx` re-latches `EnableRx0_7` and the per-DDC rates. Thetis's stop
frame arrives with no other command traffic near it.

---

## Change made

`src/core/P2RadioConnection.cpp`:

1. **The 100 ms heartbeat wheel is gated on MOX.** It is preserved verbatim for
   transmit, where its original 3M-1a rationale applies ("keep TX state fresh ...
   never engages the PA"), and does not run during receive, where there is no PA
   state to keep fresh. In the captured session `mox=false` throughout, so all
   278 `CmdHighPriority` frames were idle polling. Every state change is already
   pushed immediately by 11 change-driven `sendCmdHighPriority` sites, 4 for
   `CmdRx` and 10 for `CmdTx`, so the wheel was purely additive.
2. **`writeDatagram`'s return is checked** in `sendCmdHighPriority`. Previously
   discarded, so `"SendStop complete"` was logged whether or not the frame ever
   reached the wire. Three rounds of debugging treated that line as evidence.
3. **`kStopQuiesceMs` settle before the stop frame**, so nothing lands on top of
   it if a disconnect is taken during TX.

The 500 ms `CmdGeneral` keepalive is retained — it is Thetis's `network.c:1428`
period and it is what feeds the board's 2 s deadman.

---

## Second change: the P1 discovery probe

Found after the cadence work, and the stronger suspect of the two.

Our P1 probe is broadcast to UDP 1024, which is also the P2 General command
port. `General_CC.v:90/106` claims any port-1024 datagram whose byte 4 is zero
and parses the rest as a General command. Our probe is `EF FE 02` followed by 60
zero bytes, so a P2 radio accepts it and latches the padding as config: byte 38
clears `HW_timer_enable` (freezing the ~2 s deadman), bytes 58/59 clear
`PA_enable` and `Alex_enable`. `RadioDiscovery.cpp:441-445` sends it to both the
directed subnet broadcast and 255.255.255.255 on every scan, and the app log
shows a scan immediately precedes every connect, so it fired **every time** —
matching the 100%-reproducible symptom far better than cadence does.

Fix: byte 4 = `0x02`, so `General_CC` bails at its command check. Verified that
no P1 gateware reads byte 4 (Hermes v3.3, Angelia, Orion, ANAN-10E/100B,
HermesC10 all match only the command byte in `Rx_MAC.v`).

This one was invisible in every capture we took: the mirror filter was
`filter-ip-address=192.168.109.198/32`, which cannot match a broadcast
destination. It was found in the application log instead. **If you are hunting a
discovery-related problem, do not trust an IP-filtered capture.**

---

## Status: NOT FIXED — but the failure window is now pinned

Bench run 2026-07-27 with both changes in. One cycle survived, the next did not,
so the bug is **not deterministic** as originally believed, and neither change
is sufficient.

| session | duration | I/Q packets | outcome |
|---|---|---|---|
| 16:38:44 → 16:38:54 | 10 s | 2087 | reconnect OK |
| 16:39:03 → 16:39:51 | 48 s | 9568 | **locked up** |

### The important new fact

**The radio survives the disconnect and dies roughly one second later, while
idle, during the post-disconnect scan.** It is not dying at the disconnect.

```
16:39:51.448  P2: Disconnected. I/Q packets: 9568
16:39:51.455  Scanning NIC "feth3169"
16:39:51.497  P2 response from .198  ANAN-G2E fw: 110    <- alive, replies x4
16:39:52.325  P2 response from .45   (Saturn only)       <- G2E gone
16:39:53.141  P2 response from .45   (Saturn only)       <- G2E gone
```

The surviving cycle looks identical through the first scan attempt (four G2E
replies) and then keeps answering attempts 2 and 3. The failing one answers
attempt 1 and is dead by attempt 2. So the kill happens in a sub-second window
after `run=0`, with the radio otherwise idle and only discovery traffic on the
wire.

This retires every "the stop frame is malformed" theory for good: the stop frame
is byte-correct, the radio acts on it, answers discovery afterwards, and *then*
dies.

### A regression this run introduced, since fixed

The first version of the probe pad used byte 4 = `0x02`. Byte 4 is also the P2
*command* byte in `sdr_receive.v`, and `2` is the discovery command, so every P1
probe became a second discovery request. Verified in the app log: 2 replies per
scan attempt on the old build, 4 on the new one. Now `0xFF`, which falls through
to `ST_WAIT`.

Note for anyone touching this byte: `0x03` on a broadcast probe reaches
`ST_SETIP`, which writes the radio's IP to EEPROM. The safe range excludes
`0x00` and `0x02`-`0x06`.

### Still unexplained

Why an idle radio, one second after a clean stop, stops answering ARP entirely.
`sdr_send.v:138` (`if (!run && state > SEND) state <= IDLE`) is the only escape
from a stalled send state, and ARP shares a single TX arbiter with UDP
(`network.v:341-344, 416-421`), which would explain total layer-2 silence — but
that is still a hypothesis, and the trigger inside that one-second window is not
identified.

Next evidence needed: a capture filtered on the **router IP** (so broadcast is
visible) spanning disconnect through the following scan, to see the last frames
the radio emits and exactly which probe it fails to answer.

---

## Update, 2026-07-27 evening: post-disconnect quiet period. Bench result GOOD.

The delta analysis (Thetis is totally silent after its stop; NereusSDR fired a
broadcast discovery burst 7-15 ms after run=0, and both deaths landed in that
window) led to `RadioDiscovery::holdOffScans()`: every teardown now defers all
discovery, broadcast and unicast, for 3 s. Probing a radio while its
stop-transition state machines are settling is a race Thetis never enters, and
now neither do we.

Bench, same G2E, same network, one power cycle to clear the prior wedge:

| | cycle 1 | cycle 2 |
|---|---|---|
| session length | ~7 s | 54 s (longer than the 48 s fatal session) |
| scan after run=0 | +2.3 s | +2.3 s |
| discovery after stop | answers | answers |
| reconnect | clean | clean |

Two consecutive clean cycles. The pre-fix build never survived two in a row
(best observed: one). Attribution note: this build also carries the 0xFF probe
pad and the MOX-gated cadence, but both fatal runs already had the cadence fix
and death #2 involved no mid-session scan, so the quiet period is the change
that separates wedge from no-wedge.

Still true and worth remembering:

* The underlying gateware race presumably still exists. Any OTHER host that
  broadcasts discovery at the wrong instant (another PC running an SDR console,
  a scanner) could in principle wedge a stopping radio. Client-side silence
  narrows the trigger; it does not fix the firmware. Candidate upstream report
  to TAPR / N1GP once more cycles confirm.
* The exact wedged state machine inside the C10 gateware was never pinned.
  udp_send / ip_send / mac_send / the TX arbiter all read orphan-tolerant;
  arp.v and the icmp sending_sync handshake remain unread.
* Confidence: two cycles is strong but not proof against a ~25% survival rate
  observed pre-fix (P of 2 clean by luck is roughly 6%). A day of normal use
  without a power cycle closes this for good.

---

## Prior art

- `a0514cb1` (2026-05-22) 3x SendStop retry — superseded
- `f0e30f20` 5x CmdGeneral winddown, no run=0 — wrong, based on a partial pcap; retracted
- `a4cda4e3` reverted to 1x `CmdHighPriority` run=0 — current stop-frame behaviour, confirmed correct by this capture

`a4cda4e3` closed by asking for exactly the byte-diff performed here. Note its
commit message describes Thetis's stop as "ONE CmdHighPriority frame"; this
capture shows four on the wire, but that is the mirror duplicating a single send,
not four sends.
