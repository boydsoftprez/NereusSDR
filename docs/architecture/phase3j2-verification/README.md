# Phase 3J-2 Bench Verification Matrix

**Goal:** Confirm the full spot system (DX cluster, RBN, WSJT-X UDP,
SpotCollector, POTA, FreeDV Reporter, PSK Reporter, DXCC color provider,
panadapter overlay, settings persistence) works end-to-end against live
networks and a real radio.

**Status:** Post-v0.5.0-rc1 bench matrix. All rows must pass before
v0.5.0 final ships. Failed rows produce GitHub issues on the
NereusSDR repo and block the final tag.

**Design authority:** `docs/architecture/phase3j2-3r-spots-and-rade-design.md`
Section 9 (Bench verification matrices).

**Companion matrix:** `docs/architecture/phase3r-verification/README.md`
covers the RADE peer-mode bench rows.

## How to use

1. Build and launch a v0.5.0-rc release-candidate from the
   `claude/elegant-liskov-73ad75` branch (or the merged equivalent).
2. For software-only rows, an ANAN-G2 connection is optional but
   recommended so panadapter overlays can be verified visually.
3. For each row below, follow the reproducer steps in order. Tick the
   matching status box when the expected behaviour is observed; if the
   observed behaviour differs, file a GitHub issue, link it from the
   row, and tick **Failed**.
4. Sign off the file at the bottom once every row is **Passed**.

## Tester sign-off legend

Each row carries one of:

- `[ ] Untested`
- `[x] Passed YYYY-MM-DD by NAME (callsign)`
- `[x] Failed YYYY-MM-DD by NAME (callsign), issue: #N`

When **Failed**, leave the **Passed** box unticked. The row is only
considered closed when **Passed** is ticked or the linked issue is
resolved and the bench is re-run.

---

## Row 1: DX cluster live stream

**Hardware:** ANAN-G2 (panadapter overlay visual) or software only.
**Network:** Internet route to `dxc.k1ttt.net:7300`.

**Reproducer:**
1. Open Setup, navigate to the Spots tab (or use the SpotHubDialog
   DX Cluster tab gear button); set the cluster host to
   `dxc.k1ttt.net`, port `7300`, login callsign to a real callsign.
2. Toggle Connect. Wait up to 10 seconds for the login banner to land
   in the cluster console.
3. Open Tools > Spot Hub (Ctrl+Shift+S). Switch to the DX Cluster
   tab; verify spots flow in (DX de KX0X, etc.).
4. Switch to the Spot List tab; verify the spots show source pill
   `DX-CLUSTER` and the band pill matches the spot frequency.
5. Tune the active slice into the cluster spot frequency range
   (typical: 14 MHz +/- 10 kHz). Watch the panadapter; spot markers
   should render with the cluster colour and a callsign label.
6. Leave the application running long enough for the spot lifetime
   (default `DxClusterSpotLifetimeSec`, typically 600 s) to expire on
   the oldest spot. Verify the marker fades out and is removed.
7. Click a spot marker on the panadapter. The active slice frequency
   should snap to the spot frequency.

**Expected:** Login succeeds; spot stream visibly flows; lifetime
expiry removes stale spots; click-to-tune retunes the slice.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** None.

---

## Row 2: RBN (Reverse Beacon Network) CW spot stream

**Hardware:** Software only (CW activity visualised against any band on
the panadapter once a radio is connected).
**Network:** Internet route to `telnet.reversebeacon.net:7000`.

**Reproducer:**
1. SpotHubDialog > RBN tab gear button; set host
   `telnet.reversebeacon.net`, port `7000`, login callsign to a real
   callsign.
2. Toggle Connect. Wait for the RBN welcome banner.
3. Watch CW spots arrive in the RBN tab. RBN spot rows include an
   SNR (dB) field; verify it is populated and numeric.
4. Switch to the Spot List tab; verify the source pill colour for
   RBN spots differs from the cluster pill colour (the design doc
   specifies two distinct colours so the user can tell sources apart
   at a glance).
5. Tune the panadapter to a busy CW band (40m or 20m); verify RBN
   spot markers render with the RBN colour.

**Expected:** RBN spots arrive; SNR field populated; visible colour
delta between RBN and cluster pills.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** RBN is CW-heavy; FT8/RTTY spots from RBN are
out-of-band variants and not the focus here.

---

## Row 3: WSJT-X UDP decode ingest

**Hardware:** ANAN-G2 in receive (or software-only with a recorded FT8
loop). WSJT-X v2.6 or later installed on the same host.

**Reproducer:**
1. Open WSJT-X. Open File > Settings > Reporting; confirm UDP Server
   is `127.0.0.1`, port `2237`, "Accept UDP requests" enabled.
2. Tune WSJT-X to a known-active FT8 frequency (14.074 MHz dial).
3. In NereusSDR, open SpotHubDialog > WSJT-X tab; confirm it shows
   "Listening on 2237" or equivalent.
4. Wait for WSJT-X to decode at least 5 FT8 stations.
5. Verify the decoded callsigns appear in the WSJT-X tab AND in the
   unified Spot List. Verify panadapter markers render at the
   correct FT8 sub-band offsets.

**Expected:** Every WSJT-X decode emits a `spotReceived` signal into
SpotModel; the call appears in Spot List + panadapter overlay within
1 second of the decode landing in WSJT-X.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** WSJT-X v2.6 UDP envelope is the only target;
older variants may need a separate row.

---

## Row 4: SpotCollector (DXLab) UDP feed

**Hardware:** Windows host with DXLab Suite installed (SpotCollector
+ Spotted Now). Software-only otherwise (use a synthetic netcat
datagram fixture if no DXLab box is available).

**Reproducer:**
1. Open SpotCollector. Configure it to broadcast spots to UDP
   `127.0.0.1:8888` (the AetherSDR-faithful default; can be
   overridden in SpotHubDialog > SpotCollector tab).
2. Open Spotted Now to drive incoming spots.
3. In NereusSDR, SpotHubDialog > SpotCollector tab should show
   "Listening on 8888".
4. Verify spot rows arrive matching what Spotted Now is publishing.
5. As a fallback when no DXLab box is reachable, send a synthetic
   UDP datagram via `echo "<test spot envelope>" | nc -u 127.0.0.1
   8888` and verify the row lands.

**Expected:** SpotCollector datagrams parse cleanly and land in the
unified SpotModel.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Linux/macOS testers will likely run the
synthetic-fixture variant. Document the platform in the sign-off.

---

## Row 5: POTA auto-poll + dedup window

**Hardware:** Software only.
**Network:** Internet route to `api.pota.app`.

**Reproducer:**
1. SpotHubDialog > POTA tab. Verify the poll interval reads 60
   seconds (the Thetis/AetherSDR convention).
2. Wait for the first poll. Verify activations appear in the POTA
   tab and the unified Spot List with source pill `POTA`.
3. Wait for the next poll (60 s). Verify activations that were
   already present within the dedup window (default 10 s after the
   previous poll lands) do NOT double-up in the Spot List.
4. Verify that an activation that drops off the POTA API stream
   (operator goes QRT) expires from the local store after the
   configured spot lifetime.

**Expected:** Auto-poll fires every 60 s; dedup window prevents
duplicates within 10 s; expiry sweeps stale activations.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** POTA activity varies by time of day. Run
during US-evening (UTC 23:00 to 03:00) for a busy stream.

---

## Row 6: FreeDV Reporter live view

**Hardware:** Software only.
**Network:** Internet route to `qso.freedv.org` (HTTPS + Socket.IO).

**Reproducer:**
1. Tools > FreeDV Reporter (Ctrl+Shift+R). Verify the dialog opens
   modeless and renders 14 columns (Callsign, Grid, Distance,
   Heading, Version, RX Frequency, RX Mode, Status, Last TX, Last
   RX, Last Update, Country, plus the 2 reporter-internal columns).
2. Verify station rows populate within 5 seconds of connecting.
3. Identify a station currently transmitting; that row should turn
   red. Identify a station currently receiving the TX-er's signal;
   that row should turn green. After approximately 6 seconds with
   no further activity, both highlights should clear.
4. Right-click a remote station and pick "QSY this station". Enter
   a target frequency. Verify the receiving station's RX Frequency
   column updates within a few seconds.
5. Click the Status Message field at the bottom. Enter a message,
   click Save. Click Send. Click Clear. Verify the message
   round-trips through the reporter UI and appears in your own
   row's Status column.
6. Note a station whose Last RX is older than 2 hours. After the
   idle-sweep timer fires (or after manually triggering the sweep
   if a debug control exists), the row should auto-remove.

**Expected:** All 14 columns populate; TX-station rows go red;
RX-station rows go green; highlights clear after about 6 s; QSY
round-trips; status Save/Send/Clear works; idle-longer-than-2h auto-
removes stations.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Status messages are MRU-cached locally; cache
visibility is verified by the unit test fixture, not by this row.

---

## Row 7: PSK Reporter IPFIX datagram

**Hardware:** Software only.

**Reproducer:**
1. SpotHubDialog > PSK Reporter tab. Verify the listener is bound
   and shows "Listening on 4739".
2. From a separate terminal, send a synthetic IPFIX datagram that
   matches the PSK Reporter spot envelope to UDP
   `127.0.0.1:4739`. (Use the fixture in
   `tests/fixtures/pskreporter/sample.bin` piped through `nc -u`.)
3. Verify the spot lands in the PSK Reporter tab AND in the unified
   Spot List with source pill `PSK-REPORTER`.

**Expected:** The listener parses the IPFIX template + data records,
emits `spotReceived`, and the spot appears in both views.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Live PSK Reporter feed-back is a different
ingest direction (we report TO PSK Reporter, not from); this row
covers the ingest path only.

---

## Row 8: DXCC color priority with a real ADIF log

**Hardware:** ANAN-G2 panadapter for the visual.
**Setup:** Place a real ADIF log file at
`~/.config/NereusSDR/wsjtx_log.adi` (or whichever path the design doc
specifies). Log should include at least 6 worked DXCC entities across
3 bands and 2 modes.

**Reproducer:**
1. Restart NereusSDR. Watch the log; DxccWorkedStatus should parse
   the ADIF on startup and report the worked-entity count.
2. SpotHubDialog > Spot List tab; verify each spot row receives a
   colour swatch reflecting the 4-tier resolver (worked = grey,
   needed-band = orange, needed-mode = yellow, needed-mode-and-band
   = red, exact colours per AetherSDR DxccColorProvider).
3. Tune the panadapter to a band where your ADIF shows a mix of
   worked + needed entities; verify the on-panadapter callsign
   labels render with the same colour priority.
4. Add a synthetic QSO to the ADIF (file edit), restart the app,
   verify the new entity is now worked and the colour updates.

**Expected:** 4-tier color resolver fires correctly; ADIF parser
recognises 6+ entities; restart re-parses cleanly.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** cty.dat ships in-tree; if the upstream
country database is stale, callsigns from newly-issued prefixes may
land in the "Unknown" bucket. Update cty.dat from
country-files.com on a quarterly cadence.

---

## Row 9: Panadapter overlay collision avoidance

**Hardware:** ANAN-G2 with any band loaded; need approximately 8+ active spots
in the visible panadapter range.

**Reproducer:**
1. Tune to a high-activity band (20m FT8, 14.074 MHz, mid-evening
   on a weekend works well).
2. Confirm cluster + RBN + WSJT-X are all connected and producing
   spots.
3. Watch the panadapter as spots layer up. When more than approximately
   3 spots cluster within 200 Hz of each other, the renderer should
   stack them vertically (multi-level placement, no callsign label
   overlap).
4. When the cluster density exceeds the configurable
   `MaxSpotsPerSpectrum` cap, a `+N` cluster badge should appear
   at the cluster centre indicating how many additional spots were
   merged.
5. Hover over the `+N` badge; a tooltip should list the merged
   callsigns.

**Expected:** No label overlap; stacking is visibly multi-level;
`+N` badges appear above the cap; tooltip lists merged calls.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Stacking only applies inside a single
panadapter; multi-panadapter is Phase 3F future.

---

## Row 10: Auto-connect restore on launch

**Hardware:** Software only.

**Reproducer:**
1. With NereusSDR running, configure the DX Cluster connection
   identity (call, host, port). Toggle Connect; confirm it works.
2. In the gear panel for the DX Cluster tab, set Auto-Connect to
   On (the key persists as `DxCluster/AutoConnect=True` in
   AppSettings).
3. Disconnect. Quit NereusSDR.
4. Relaunch NereusSDR. Wait 3 seconds after the main window appears.
5. Open SpotHubDialog > DX Cluster tab. Verify it shows a green
   "Connected" LED and spots are flowing without manual user
   intervention.
6. Repeat for each spot source that has an Auto-Connect knob (RBN,
   FreeDV Reporter, PSK Reporter).

**Expected:** Every source with `AutoConnect=True` is connected
automatically within a few seconds of app launch.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Auto-connect waits on the network stack; on
slow Wi-Fi or VPN startup, first connect may take up to 15 s.

---

## Row 11: Spot Hub Display knobs round-trip

**Hardware:** ANAN-G2 panadapter for visual feedback.

**Reproducer:**
1. SpotHubDialog > Display tab. Note the default values for
   ShowSpotsOnSpectrum, MaxSpotsPerSpectrum, spot font size, and
   the per-source toggle row.
2. Toggle ShowSpotsOnSpectrum to Off. Verify all spot markers
   disappear from the panadapter within 1 frame.
3. Toggle back to On. Verify markers reappear.
4. Change MaxSpotsPerSpectrum from default to 10. Verify on a busy
   band that no more than 10 markers render per panadapter; the
   excess clusters into the `+N` badge from Row 9.
5. Change spot font size from default (e.g. 10 pt) to 18 pt.
   Verify callsign labels visibly enlarge.
6. Toggle each per-source pill (DX Cluster, RBN, etc.) off; verify
   that source's markers disappear and its tab dims out.
7. Quit and relaunch NereusSDR; verify every value persists across
   the restart.

**Expected:** Every knob round-trips through AppSettings; visible
changes apply within 1 frame of toggling.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Per-source pill colours are not user-
configurable in this release; if a tester requests it, file a feature
request for v0.5.x.

---

## Cross-cutting checks

- [ ] No `lcSpots` warnings in the log during a 30-minute session
      with all sources connected.
- [ ] CPU usage during a 30-minute session with all sources
      connected stays within +2% of the v0.4.0 baseline (no live
      RADE, no spot overlay rendering).
- [ ] No memory growth beyond the first 5 minutes (steady-state
      after the first cty.dat parse + ADIF load).
- [ ] No crash or hang on shutdown with all sources connected.

## Sign-off

Verification owner: J.J. Boyd (KG4VCF). v0.5.0 final is not tagged
until every row above is Passed and every cross-cutting check is
ticked. If any cell is Failed, file an issue against this branch
and resolve before tagging.
