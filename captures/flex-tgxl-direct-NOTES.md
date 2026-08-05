# FLEX-8600 <-> TGXL (and PGXL) direct conversation — Windows-side notes

Captured 2026-05-20 via the inline MikroTik (RouterOS v7) TZSP stream, AFTER
disabling hardware offload on the FLEX + TGXL bridge ports so the CPU sniffer
could see the device-to-device unicast that never crosses the SmartSDR PC.
This is the piece missing from the earlier captures.

## Files

| File | Size | What |
| ---- | ---- | ---- |
| `flex-tgxl-direct-CONTROL.pcapng` | 20.9 MB | **USE THIS.** Control-plane only (TCP 4992 / 9008 / 9010), I/Q stripped out. TZSP-encapsulated (Wireshark auto-dissects). |
| `flex-tgxl-direct-tzsp_00001_20260520203049.pcapng` | 191 MB | Full raw TZSP stream incl. the VITA-49 I/Q firehose (offload was off). Keep only if you need the I/Q; otherwise use the CONTROL file. |

MikroTik = 192.168.109.85. Sniffer streamed TZSP to 192.168.109.19:37008.

## The big picture: a bidirectional pairing

The FLEX and each Genius device hold **two** simultaneous TCP connections with
**opposite client/server roles**:

```
   (1) amp registers with radio          (2) radio drives tuner/amp
   TGXL ---> FLEX  TCP 4992               FLEX ---> TGXL  TCP 9010
   (SmartSDR text API)                    (Genius native text API)
```

Same pattern for the PGXL (it registers on 4992 and is driven on 9008).

### Connection 1 — TGXL -> FLEX :4992 (amp announces itself via the SmartSDR API)

The tuner/amp acts as a SmartSDR-protocol *client* to the radio. Opening
sequence (streams 64 and 77 in the raw file):

```
C16|amplifier create ip=192.168.109.234 port=9010 model=TunerGeniusXL serial_num=241288-1 ant=ANT1
C1 |sub slice all
C9 |sub amplifier all
C4 |interlock create type=AMP name=TG serial=241288-1 valid_antennas=ANT1
C13|meter create name=FWD type=AMP min=30.0 max=63.01 units=DBM
C14|meter create name=RL  type=AMP min=0.35  max=60.0  units=DB
C3 |ping                          (~1 Hz keepalive)
```

Radio's reply (it assigns the amp a per-connection handle — e.g. `0x4E6F36EF`,
`0x20C8CC0F` across reconnects):

```
M10000001|Client connected from IP 192.168.109.234
S<h>|radio slices=3 ... nickname=FLEX-8600 callsign=KG4VCF ... external_pa_allowed=1
S<h>|interlock acc_txreq_enable=0 ... timeout=0
S<h>|slice 0 in_use=1 RF_frequency=7.169500 mode=LSB ... txant=ANT1 tx=1 active=1 ...
S<h>|amplifier 0x18E5F0AA ip=192.168.109.235 model=PowerGeniusXL serial_num=10-200/24-0046 ant=ANT1:PORTA,ANT2:NONE state=IDLE|STANDBY
S<h>|amplifier 0x<own> serial_num=241288-1 version=1.2.17 nickname=Tuner_Genius_XL model=TunerGeniusXL
      one_by_three=1 dhcp=1 ip=192.168.109.234 ... operate=1 bypass=0 tuning=0 relayC1/relayC2/relayL ant=ANT1 pttA=0 pttB=0
```

Key takeaways:
- The amp self-declares with `amplifier create` (ip, **port=9010** = where the
  radio should call back, model, serial, antenna).
- It registers an **interlock** participant: `interlock create type=AMP name=TG
  serial=... valid_antennas=ANT1`. (name=TG = Tuner Genius. PGXL would be PG.)
- It registers **meters** (`FWD` power dBm, `RL` return-loss dB) so the radio
  can show amp telemetry.
- It subscribes to `slice` (to know freq/mode/TX) and `amplifier` objects.

### Connection 2 — FLEX -> TGXL :9010 (radio drives the tuner, native protocol)

The radio is the *client* here, speaking the TGXL's own text protocol (the
same one TunerGeniusDesktop uses). Streams 48, 65, 78:

```
C33|status        -> S33|status fwd=21.15 peak=21.29 max=0.00 swr=-60.0000 pttA=0 bandA=0 modeA=0
                       flexA=FLEX-8600 freqA=0.000 bypassA=0 bypassRxA=0 antA=2 ... state=1 active=1
                       tuning=0 bypass=0 ag=0 relayC1=0 relayL=0 relayC2=0
C35|info          -> R35|0|info serial=241288-1 version=1.2.17 nickname=Tuner_Genius_XL
C36|ifconf read
C27|autotune      -> (begins a tune)
S0 |state ... state=1 tuning=1 bypass=0 relayC1=16 relayL=16 relayC2=0     (unsolicited state stream)
S0 |state ... tuning=1 ... relayC1=16 relayL=16 relayC2=16
S0 |state ... tuning=1 ... relayC1=16 relayL=16 relayC2=32
S0 |state ... tuning=1 ... relayC1=16 relayL=16 relayC2=48
S0 |state ... tuning=1 ... relayC1=16 relayL=32 relayC2=16
   ... (steps C2 in 16s, then L, then C1 — the relay search) ...
M  |Tuned SWR: 1.04:1                                                      (tune complete + result)
S0 |state ... tuning=0 ...
```

Native TGXL :9010 protocol summary:
- `C<seq>|status` -> `S<seq>|status fwd= peak= max= swr= pttA/B band mode flexA= freq bypass ant state active tuning bypass ag relayC1/L/C2`
- `C<seq>|info` -> `R<seq>|0|info serial= version= nickname=`
- `C<seq>|ifconf read`
- `C<seq>|autotune` -> starts a tune; progress via unsolicited `S0|state` lines; ends with `M|Tuned SWR: x.xx:1`
- `S0|state ...` — async state pushes (relay positions, tuning flag, bypass, antenna)
- relay banks step in increments of 16: `relayC1` / `relayC2` (capacitors) and
  `relayL` (inductor). This is the autotuner's search grid.

## Interlock / keying note (cross-ref with PTT-MOX capture)

The earlier PTT/MOX capture (`flex-pgxl-tgxl-PTT-MOX-NOTES.md`) showed the
radio-side interlock state machine (`RECEIVE/READY/PTT_REQUESTED/TRANSMITTING/
UNKEY_REQUESTED`, `reason=AMP:TG`, `amplifier=<handles>`). This capture shows
the other half: how the amp **registers** the interlock participant
(`interlock create type=AMP name=TG`) and how the radio **drives** the tuner
(`autotune` over :9010). Together they are the complete amp/tuner integration.

## Devices seen connecting to the FLEX :4992

| IP | Role | Notes |
| -- | ---- | ----- |
| 192.168.109.234 | TGXL | `amplifier create ... model=TunerGeniusXL serial_num=241288-1` |
| 192.168.109.235 | PGXL | `model=PowerGeniusXL serial_num=10-200/24-0046 ant=ANT1:PORTA,ANT2:NONE` |
| 192.168.109.19  | SmartSDR PC | the GUI client |
| **192.168.109.133** | **UNKNOWN** | also opened TCP to FLEX:4992 (port 47200). Identify this host — another client/amp/logger? |

## Relevance to NereusSDR

To be recognized and to drive PGXL/TGXL the way a real FLEX does, NereusSDR's
SmartSDR-compatible API (TCP 4992) must:

1. Accept inbound amp connections and parse:
   - `amplifier create ip= port= model= serial_num= ant=`
   - `interlock create type=AMP name= serial= valid_antennas=`
   - `meter create name= type=AMP min= max= units=`
   - `sub slice all`, `sub amplifier all`, `ping`
2. Assign and report an `amplifier 0x<handle>` object and stream its state
   under `sub amplifier all` (mirror the fields above).
3. Reflect the `interlock` state machine with `reason=AMP:TG`/`AMP:PG`,
   `amplifier=<handle list>`, `source=TUNE|SW`, `tx_client_handle`.
4. As a client, connect OUT to the tuner on :9010 and speak its native
   protocol (`status`, `info`, `ifconf read`, `autotune`; consume `S0|state`
   and `M|Tuned SWR:`), and to the PGXL on :9008.

## Cleanup reminder for JJ

- Revert the MikroTik offload: `/interface bridge port set [find interface=<FLEX-port>] hw=yes`
  (and the TGXL port). Leaving `hw=no` forces the I/Q firehose through the CPU.
- Stop the sniffer: `/tool sniffer stop`.
