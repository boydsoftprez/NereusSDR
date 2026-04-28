# TUN-Engaged P2 Capture Comparison Notes

Side-by-side OpenHPSDR Protocol-2 packet captures used to diagnose why
NereusSDR's TUN press does not put carrier on the air against an ANAN-G2,
while Thetis's TUN press does.

The plan is to capture both clients keying TUN against the same radio at the
same frequency, then byte-diff the first PC→radio port-1027 packet that flips
the MOX bit (payload byte 4: `0x01` → `0x03`).

## Status

| Side | Status | Captured |
|------|--------|----------|
| Thetis (working reference) | **Captured** | 2026-04-27 15:16-15:18 EDT |
| NereusSDR (pre-fix, `main` @ `d2d50e8`) | Pending — next session | |

---

## Thetis side (this session)

### Capture environment

- **Host:** Windows 11 bench PC, JJ
- **PC IP:** 192.168.109.19/24 on adapter "Ethernet" (Realtek PCIe 2.5GbE Family Controller, dumpcap interface index `8`)
- **Radio:** ANAN-G2 (Saturn / OrionMkII), MAC `2c:cf:67:ab:fc:f4`, IP **192.168.109.45**
- **Frequency:** 7.240 MHz LSB (40 m)
- **Thetis version:** _TBD — record from Help → About on next bench session_
- **Capture tool:** `dumpcap.exe` 4.6.2 with capture filter `host 192.168.109.45`
- **Wall-clock window:** 2026-04-27 15:16:13 → 15:18:20 EDT (~127 s)

### Pcap file (local, not in git)

Pcaps are gitignored in this repo and the file is too large for GitHub's
100 MB hard limit (no LFS configured). Kept on the Windows host:

- **Path:** `C:\Users\boyds\NereusSDR\captures\thetis-tun-7240-20260427.pcapng`
- **Size:** 249,695,352 bytes (249.7 MB)
- **sha256:** `51366ec481e402b8488367f7fc9ff04735db4231f0b42c97ab9862f473fec184`
- **Frames:** 161,141 (161,124 OpenHPSDR P2 UDP "data" frames + 17 background mDNS/DNS/ARP/STUN)

If next session needs the bytes for byte-level diff, transfer via the usual
cross-machine path (scp/shared drive). The findings below are sufficient for
most analysis without moving the file.

### Port set (matches expected OpenHPSDR P2 layout)

- **PC → radio (dst):** 1024, 1025, 1026, 1027, 1029
- **Radio → PC (src):** 1024, 1025, 1026, 1035, 1037
- Note: radio→PC port `1027` was not seen this run — that direction is sparse on the G2 and is sometimes absent. Not a defect.

### MOX / TUN bit transitions on port 1027 PC→radio

UDP payload byte 4 (immediately after the 32-bit BE sequence number) is the
**run / MOX** byte:

| Value | Meaning |
|:-----:|---------|
| `0x00` | Stopped |
| `0x01` | Running, RX only (MOX clear) |
| `0x03` | Running + MOX (TX engaged — the "TUN engaged" marker) |

Six clean transitions observed across the capture:

| Frame    | t_rel (s) | byte 4 | Event                                          |
|---------:|----------:|:------:|------------------------------------------------|
| 13       | 52.934    | `01`   | Thetis RUN start (RX active)                   |
| **92242**    | **76.082**    | **`03`**   | **TUN press #1 — MOX bit asserted**            |
| 113812   | 79.758    | `01`   | TUN release #1                                 |
| **123917**   | **82.220**    | **`03`**   | **TUN press #2 — MOX bit asserted**            |
| 141211   | 85.417    | `01`   | TUN release #2                                 |
| 161138   | 90.348    | `00`   | Thetis disconnect (run cleared)                |

Two TUN press/release pairs were captured (TUN-down ~3.7 s and ~3.2 s
respectively) — bonus second event for cross-validation if needed.

### Byte-diff anchor

For the comparison next session, the canonical byte-diff anchor is:

> **Thetis frame 92242 at t_rel = 76.082 s** — the first PC→radio
> port-1027 packet with payload byte 4 = `0x03` after TUN press #1.

Compare it byte-for-byte against the equivalent first-`0x03` packet in the
NereusSDR pcap. Any payload-byte differences in the high-priority command
packet (run/MOX, antenna routing bits, sample-rate bits, DDC config, Alex
bits, etc.) are root-cause candidates for "Thetis works, NereusSDR doesn't".

### Re-extracting full payloads on demand

Frame 92242's full payload was not embedded in this doc to keep it readable.
To pull it from the local pcap:

```
"C:\Program Files\Wireshark\tshark.exe" \
  -r "C:\Users\boyds\NereusSDR\captures\thetis-tun-7240-20260427.pcapng" \
  -Y "frame.number == 92242" -T fields -e udp.payload
```

For reference, **frame 13** (RUN start, byte 4 = `0x01`) payload begins:

```
00000000  01  000000  00  0f141da0 0f141da0 0f141da0 08060ebe ...
^seq=0    ^run/MOX        ^DDC0     ^DDC1     ^DDC2     ^...
```

(structure to be reverse-engineered against the existing
[`thetis-3865-lsb-pcap-analysis.md`](thetis-3865-lsb-pcap-analysis.md) which
documents the P2 high-priority command layout in detail)

### Bench observations (Thetis side) — _TBD_

JJ to fill in on next bench session:

- TX/MOX front-panel LED behavior on the two TUN presses?
- Carrier audible on test receiver?
- Anything weird with RX during MOX (squelch, splatter, fan kick)?
- Thetis version string (Help → About)?

---

## NereusSDR side (next session)

To be filled in by the agent doing the capture.

### Expected fields (template)

- Capture environment (host, PC IP, adapter, NereusSDR HEAD commit + build date)
- Pcap file path / size / sha256 / frame count
- Port set verification
- Byte-4 transition table on PC→radio port 1027
- Frame number of first `0x03` after TUN press
- Bench observations (TX LED, carrier audible, RX behavior during MOX)

---

## Next-session prompt (copy-paste verbatim)

```
NereusSDR Phase 3M-1a — TUN-engaged pcap capture (Windows side, NereusSDR run)

GOAL
Capture a Wireshark pcap of NereusSDR keying TUN against the same ANAN-G2
that the prior session captured Thetis against. Output is a self-contained
findings entry appended to captures/tun-pcap-comparison-notes.md, then a PR.

CONTEXT FROM PRIOR SESSION
- Thetis side already captured 2026-04-27. Notes + sha256 of the local
  pcap live in captures/tun-pcap-comparison-notes.md (this file you're
  reading). Read it before starting.
- Byte-diff anchor on the Thetis side: frame 92242, port 1027 PC→radio,
  payload byte 4 = 0x03. Your job is to produce the equivalent NereusSDR
  capture for next-step byte-diff.
- The full Thetis pcap is on the Windows bench machine at
  C:\Users\boyds\NereusSDR\captures\thetis-tun-7240-20260427.pcapng
  (gitignored, 249.7 MB, sha256 51366ec4...184). Don't move it.

ENVIRONMENT (must match prior session)
- Same Windows 11 host. Same PC NIC (192.168.109.19, adapter "Ethernet",
  dumpcap interface index 8).
- Same radio (G2 at 192.168.109.45). Verify still reachable: ping it.
- Same frequency: 7.240 MHz LSB (40 m).

REPO STATE
- Build/run NereusSDR from origin/main HEAD (whatever main is at the moment
  the session starts — pull it). Note the SHA in the findings doc.
- The feature branch feature/phase3m-1a-tune-only-first-rf with the 5
  wire-protocol fixes is NOT what we want here — this capture is the
  pre-fix baseline. Use main.
- Note: BENCH-DIAG instrumentation is NOT on main. Stderr log will be
  quiet. That's expected.

PROCEDURE
1. Confirm no NereusSDR / Thetis is running:
     tasklist | grep -iE "thetis|nereus"
2. Verify G2 reachable:
     ping -n 2 192.168.109.45
3. Cd into the main checkout C:\Users\boyds\NereusSDR. Check it's on main
   and synced:
     git status --short --branch
     git pull --ff-only
     git log --oneline -1
4. Build (or use existing build\NereusSDR.exe if dated after the current
   main HEAD):
     cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
     cmake --build build -j
5. Start dumpcap (background):
     "C:\Program Files\Wireshark\dumpcap.exe" \
        -i 8 -f "host 192.168.109.45" \
        -w "C:\Users\boyds\NereusSDR\captures\nereussdr-tun-7240-<YYYYMMDD>.pcapng"
   Wait ~2 s, verify the .pcapng file exists and grew past the header.
6. Launch NereusSDR with stderr → log:
     build\NereusSDR.exe 2> captures\nereussdr-bench-diag-7240-<YYYYMMDD>.log
7. JJ at the bench: connect to G2 in NereusSDR's discovery UI → tune to
   7.240 LSB → wait ~3 s for steady-state RX (waterfall confirms real
   signals) → press TUNE, hold ~3 s → release → wait ~2 s.
8. Stop dumpcap (taskkill //F //IM dumpcap.exe). Quit NereusSDR.

VERIFICATION
1. File exists, non-empty.
2. Port set: PC→radio 1024/1025/1026/1027/1029, radio→PC 1025/1026/1037
   (1024 and 1035 may or may not appear).
3. Find the byte-4 transitions on PC→radio port 1027:
     "C:\Program Files\Wireshark\tshark.exe" \
        -r captures\nereussdr-tun-7240-<YYYYMMDD>.pcapng \
        -Y "udp.dstport == 1027" \
        -T fields -e frame.number -e frame.time_relative -e udp.payload \
        | awk '{ b=substr($3,9,2); print $1, $2, b }' \
        | awk 'NR==1 || $3 != prev { print; prev=$3 }'
   You expect a sequence ending with 01 → 03 → 01 (or 01 → 03 → 00 if you
   killed NereusSDR mid-press). The first 03 after RUN is the TUN-engaged
   marker. Record that frame number.
4. If byte 4 never reaches 0x03 — that's the bug itself; the MOX bit isn't
   being set on the wire. Document this clearly in the findings; it's
   actionable on its own.

BENCH OBSERVATIONS (record these — they matter)
- Did the G2's front-panel TX/MOX LED light up on TUN press? (Thetis side
  baseline TBD too; the comparison is between the two clients.)
- Carrier audible on test receiver?
- Anything weird with RX during MOX?

DELIVERABLE
1. Append a populated "NereusSDR side" section to
   captures/tun-pcap-comparison-notes.md (template at the bottom of the
   "NereusSDR side" section in this file).
2. Compute sha256 of the full pcap and add it to the doc.
3. Do NOT commit the pcap to git — it'll exceed GitHub's 100 MB limit and
   the repo has no LFS. Same as the Thetis side: stays local.
4. Commit the doc update + bench-diag log (if non-empty and small) on a
   feature branch. Force-add the log if .gitignore catches it.
5. Push and open a PR titled like
   "captures(p2): NereusSDR TUN pcap findings @ 7.240 LSB".

OUT OF SCOPE
- Do NOT modify any NereusSDR source files.
- Do NOT propose fixes — analysis comes after the byte-diff in a separate
  macOS-side session.
- Do NOT commit the pcap binary.

WHEN DONE
Report:
- Findings doc path with section heading
- Local pcap path + size + sha256
- Bench-diag log path + line count
- First-0x03 frame number on port 1027 (or "never reached 0x03" if that's
  what happened)
- PR URL
- Anything weird at the bench
```

---

## References

- Existing related analysis: [`thetis-3865-lsb-pcap-analysis.md`](thetis-3865-lsb-pcap-analysis.md) — RX-only Thetis 3.865 MHz LSB capture, documents P2 packet structures
- Protocol reference: [`docs/protocols/openhpsdr-protocol2.md`](../docs/protocols/openhpsdr-protocol2.md)
- WDSP / TX-path source map: [`CLAUDE.md`](../CLAUDE.md) → "Thetis Source Layout Quick Reference"
