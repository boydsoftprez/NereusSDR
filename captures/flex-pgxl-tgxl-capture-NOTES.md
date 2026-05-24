# FLEX-8600 <-> PGXL/TGXL capture — Windows-side notes

Captured 2026-05-19 by Claude Code on JJ's Windows box for handoff to the
macOS NereusSDR session. Goal: figure out why NereusSDR pairs with PGXL
but PGXL does not list it in the FlexRadio dropdown.

## Files

| Path | Purpose |
| ---- | ------- |
| `captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng` | The capture (10 min, 66,905 frames, 9.4 MB data / 11.4 MB file). pcapng / Wireshark 4.6.2 / npcap. |
| `captures/flex-pgxl-tgxl-capture-NOTES.md` | This file. |

## Capture parameters

- **Host:** Windows 11 desktop (Claude Code session, user `boyds`)
- **Interface:** `Ethernet` (Realtek PCIe 2.5GbE, MAC `BC-FC-E7-E3-BC-6C`),
  IPv4 `192.168.109.19/24`, promiscuous on.
- **Tool:** `dumpcap.exe` from Wireshark 4.6.2.
- **BPF filter:**
  ```
  (host 192.168.109.6 and not udp port 4991) or
  host 192.168.109.234 or
  host 192.168.109.235 or
  (udp and (ether broadcast or ether multicast))
  ```
  The `not udp port 4991` excludes the FLEX VITA-49 audio/panadapter
  firehose to keep the file small — PGXL/TGXL integration is purely
  control-plane.
- **Rotation:** `-b filesize:204800` (200 MB per file, keep all). Only one
  file produced (10 MB) — never rotated.
- **Window:** `T+0 = 2026-05-19 17:34:52.529 -04:00` (capture start).

## Hosts in scope

| Role | IP | MAC | Notes |
| ---- | -- | --- | ----- |
| FLEX-8600 (the radio) | `192.168.109.6` | `00:1C:2D:05:39:9C` | Serial **2925-1213-8601-4225** (with dashes per PGXL UI). Max licensed v4. Firmware v4.2.18.41174. USA turf region. |
| PGXL (4O3A Power Genius XL) | `192.168.109.235` | `D8:47:8F:5C:FB:18` | Control plane on TCP 9008. |
| TGXL (4O3A Tuner Genius XL) | `192.168.109.234` | `D8:47:8F:68:22:BA` | Control plane on TCP 9010. |
| Windows capture box | `192.168.109.19` | `BC-FC-E7-E3-BC-6C` | Runs SmartSDR + PowerGeniusDesktop + TunerGeniusDesktop. |
| Second FlexRadio (not in scope) | `192.168.109.246` | `00:1C:2D:05:2A:74` | Different unit on the same LAN. Filter excluded it, but it still appears in broadcast/mDNS traffic. |

## Software running during capture

| Process | PID | Role |
| ------- | --- | ---- |
| `SmartSDR.exe v4.2.18` | 34232 | Started 17:35:21; established TCP `.19:52348` → `.6:4992` at T+89s. |
| `PowerGeniusDesktop.exe` | 31220 | Already running pre-capture. Pre-existing TCP `.19:61398` → `.235:9008`. During Port-A pairing a **second** TCP session appeared (`.19:55709` and `.19:55711` → `.235:9008`). |
| `TunerGeniusDesktop.exe` | 11140 | Already running pre-capture. TCP `.19:63859` → `.234:9010` throughout. |

## Event timeline (relative to T+0)

| T+ | Wall clock | Event |
| --- | ---------- | ----- |
| 0 | 17:34:52.529 | dumpcap started; FLEX broadcast beacons already on wire. |
| 51 s | 17:35:43 | SmartSDR launched, Radio Chooser open, FLEX-8600 visible in chooser. |
| 89 s | 17:36:20 | SmartSDR connected to FLEX-8600 (TCP session `.19:52348` -> `.6:4992` established). |
| 241 s | 17:38:52 | PGXL FlexRadio dropdown verified populated; serial format **dashed** (`2925-1213-8601-4225`). |
| 414 s | 17:41:45 | PGXL Port A paired to FLEX-8600 (JJ unpaired then re-paired to capture the handshake). Second PowerGeniusDesktop TCP session appears here. |
| 484 s | 17:42:55 | Band-change cycle on SmartSDR complete (multiple QSYs across band boundaries — JJ did not log individual frequencies; reconstruct from the FLEX TCP stream). |
| 572 s | 17:44:24 | TUNE cycle completed via TGXL. |
| 580 s | 17:44:32 | Post-tune settling window opened. |
| 602 s | 17:44:54.552 | dumpcap stopped. |

## Top-line protocol stats (from `tshark -z io,phs`)

- **TCP 61,493 frames / 7.9 MB** — most of the traffic.
  - `tcp.data` 30,193 — raw payload on PGXL/TGXL/FLEX control channels.
  - `h1` 1,009 / 336 KB — HTTP/1 (PGXL or TGXL embedded web UI being
    polled? worth dissecting).
  - 1 frame mis-identified by Wireshark as `dplay` — almost certainly a
    false positive on a SmartSDR API packet whose first bytes resemble
    DirectPlay magic.
- **UDP 5,211 frames / 1.4 MB**
  - `tcp.data`-equivalent `udp.data` 3,270 — the FLEX discovery beacons
    plus any custom UDP from PGXL/TGXL.
  - `mdns` 1,661 (host noise — Bonjour/AirPlay/etc on the LAN).
  - `ssdp` 176, `dhcp` 39, `mndp` 40, smb-browser 1 — LAN background.

## FLEX-8600 discovery beacon

**Confirmed.** The radio broadcasts every ~1.001 s, src `192.168.109.6:4992`,
dst `255.255.255.255:4992`, UDP payload 652 bytes. About 600 of these in
the 10-minute window. **This is the candidate for the "PGXL FlexRadio
dropdown" populator** — PGXL should be sniffing 255.255.255.255:4992 for
these beacons. Dissect one in Wireshark to confirm whether the 16-digit
serial `2925-1213-8601-4225` is encoded byte-aligned (ASCII or BCD) inside
the 652-byte payload; if yes, NereusSDR needs to emit an equivalent beacon
in the same format.

## Top conversation volume (by host)

```
192.168.109.19  (Windows)  61,510 packets   7.9 MB
192.168.109.234 (TGXL)     39,110 packets   5.0 MB
192.168.109.235 (PGXL)     19,351 packets   2.5 MB
192.168.109.6   (FLEX)      4,812 packets   1.0 MB
224.0.0.251     (mDNS mcast) 1,661          218 KB
192.168.109.255 (bcast)      1,513          170 KB
255.255.255.255              1,452          914 KB   <-- FLEX discovery beacons live here
```

Note: the heavy PGXL/TGXL packet counts are dominated by the desktop-app
polling channels (PowerGeniusDesktop/TunerGeniusDesktop pulling status at
high rate). The FLEX side is comparatively quiet because the
high-bandwidth VITA-49 audio stream (UDP 4991) was filtered out.

## Anomalies / things to flag

1. **PowerGeniusDesktop holds two TCP sessions to PGXL:9008 after pairing**
   (LocalPort 55709 and 55711). Pre-pairing it had one (61398). Worth
   checking whether one is the desktop control channel and the other is
   the FLEX-pairing control channel that PGXL opens internally — or
   whether both are PowerGeniusDesktop and Port A pairing spawned a
   second connection.
2. **Multiple band changes during step 5 not individually timestamped.**
   The window 17:41:45 → 17:42:55 contains every QSY. Reconstruct from
   SmartSDR `slice tune` commands (TCP `.19:52348` ↔ `.6:4992`,
   SmartSDR API text protocol).
3. **No firewall popups, no "no devices found" errors observed by JJ.**
4. **Second FlexRadio (`.246`) on the LAN.** Filter excluded its unicast
   traffic. If you see a second beacon source in the pcap (it would also
   show up in `udp and (ether broadcast or ether multicast)`), that's the
   other unit — distinguish by MAC `00:1C:2D:05:2A:74` vs the in-scope
   `:39:9C`.

## Suggested next-step queries for the macOS session

1. **`udp.port == 4992 && ip.src == 192.168.109.6`** — dump one beacon and
   decode the 652-byte payload byte-for-byte. Hunt for the dashed serial
   `2925-1213-8601-4225` (32 ASCII bytes), the model string `FLEX-8600`
   or `8600`, and the IP `192.168.109.6` (4 BE bytes `c0 a8 6d 06` or
   ASCII).
2. **`tcp.port == 9008 && tcp.stream eq <N>`** — reconstruct the PGXL Port
   A pairing handshake. Compare against NereusSDR's pairing exchange.
3. **`tcp.port == 4992 && ip.addr == 192.168.109.6`** — reconstruct the
   SmartSDR <-> FLEX API stream. Decode `slice tune` events to map the
   band-change window.
4. **`tcp.port == 9010`** — TGXL command channel. Look for what
   PGXL/SmartSDR sends to TGXL during the TUNE cycle (T+572s).
5. Look for any **FLEX -> PGXL unicast** traffic on the wire. If a
   normal L2 switch is between them, it may not have been captured at
   `.19`; if PGXL gets band info from FLEX directly (not via SmartSDR
   relay), we may need a SPAN port or hub for a cleaner capture.

## Transfer

JJ: just `scp` / AirDrop / cloud-drop the .pcapng + this .md to the Mac.
Both live in `~/NereusSDR/captures/` (this repo).
