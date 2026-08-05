# Phase 3R Bench Verification Matrix

**Goal:** Confirm RADE as a true peer mode (RX, TX, mode dispatch,
UI surfaces, TX preset routing, multi-radio gating) works end-to-end
on real hardware.

**Status:** Post-v0.5.0-rc1 bench matrix. All rows must pass before
v0.5.0 final ships, with the explicit exception of rows tagged as
deferred work (HL2 gating, K-bench TX integration). Failed
non-deferred rows produce GitHub issues on the NereusSDR repo and
block the final tag.

**Design authority:** `docs/architecture/phase3j2-3r-spots-and-rade-design.md`
Section 9 (Bench verification matrices) + Section 10 (Risks and
rollout).

**Companion matrix:** `docs/architecture/phase3j2-verification/README.md`
covers the spot system bench rows.

## How to use

1. Build and launch a v0.5.0-rc release-candidate from the
   `claude/elegant-liskov-73ad75` branch (or the merged equivalent).
2. Connect a known-good ANAN-G2 (or Hermes Lite 2 once row 9 is
   unblocked) to a dummy load + power meter, with the audio output
   routed to a speaker the operator can listen to.
3. For each row, follow the reproducer steps in order. Tick the
   matching status box when the expected behaviour is observed; if
   the observed behaviour differs, file a GitHub issue, link it
   from the row, and tick **Failed**.
4. Sign off the file at the bottom once every non-deferred row is
   **Passed**.

## Tester sign-off legend

Each row carries one of:

- `[ ] Untested`
- `[x] Passed YYYY-MM-DD by NAME (callsign)`
- `[x] Failed YYYY-MM-DD by NAME (callsign), issue: #N`
- `[~] Deferred (see row text)` for rows explicitly gated on
  follow-up work

---

## Row 1: RADE RX on ANAN-G2

**Hardware:** ANAN-G2 on a live antenna or a known-good RADE signal
source (transmitting peer station, recording playback over a coupler,
or another known-good RADE station on `qso.freedv.org`).

**Reproducer:**
1. Tune the active slice into a band where RADE activity exists
   (consult FreeDV Reporter for current activity, typical: 14.236
   MHz USB convention).
2. Switch the active slice to RADE mode via the Mode menu or the
   VFO flag mode picker.
3. Observe the VFO flag SNR row. While there is no RADE signal it
   should read N/A or low SNR.
4. Verify the RadeApplet (right-column applet) is visible and shows
   the RADE profile combo defaulting to RADE.
5. When a RADE signal arrives, the sync indicator on the
   RadeApplet should transition to green within 1 to 2 seconds.
   The SNR row on the VFO flag should populate and update
   continuously.
6. Verify decoded speech is intelligible through the speakers (or
   the routed audio output device).
7. If the transmitting station includes a callsign in the embedded
   rade_text channel, that callsign should appear in the
   RxDecodeModel ring buffer (visible via the FreeDV Reporter
   dialog's Local Decodes panel, if exposed, or via debug log).

**Expected:** Green sync indicator within 2 seconds; SNR populates;
decoded audio is intelligible; embedded callsign visible.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Sync acquisition time depends on signal SNR;
on very weak signals, expect up to 10 seconds before green.

---

## Row 2: RADE TX on ANAN-G2

**Hardware:** ANAN-G2 into a dummy load with a power meter. Peer
RADE receiver (a second NereusSDR install, freedv-gui, or a known-
good RADE-decoding station) on the same band.

**Reproducer:**
1. Set the operator callsign in the appropriate identity field
   (whichever field the design doc routes into the rade_text
   embedded channel).
2. Switch the active slice to RADE mode.
3. Open the RadeApplet; verify the MicProfileManager combo
   auto-selected RADE.
4. Key MOX. Transmit a 30-second over speaking normal voice into
   the mic.
5. The peer RADE receiver should decode the callsign and produce
   intelligible audio.
6. Capture a measurement: PA forward power, reflected power, audio
   quality rating from the peer.

**Expected:** Peer station decodes the embedded callsign; audio
quality is rated equivalent to a freedv-gui transmission on the
same hardware.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** The TX path through TxWorkerThread is now
end-to-end wired (TxPath enum + HPF + 48-to-16 resampler helpers
landed in commits 34a9f14c / 181d3ee5 / 7beacdc5; the K-bench
follow-up filled in the dispatchOneBlock RADE pump body, the
TxWorkerThread::radeMicBlockReady signal, the queued connection
into RadeChannel::txEncode, and the txModemReady -> 24 -> hwRate
upsampler -> RadioConnection::sendTxIq lambda on RadioModel).
**The TX path produces a DSB-style modulation** (I = real-valued
modem baseband, Q = 0). RADE's correlator syncs on its kernel
regardless of sideband presentation so the link decodes either
way, but a proper analytic (Hilbert-transformed) baseband to get
true USB/LSB matching the RADE_U / RADE_L mode selection is a
follow-up DSP refinement and is NOT a blocker for this row.

---

## Row 3: RADE TX preset routing

**Hardware:** ANAN-G2 (or any TX-capable radio); only the routing
matters here, not radiated TX.

**Reproducer:**
1. Switch the active slice to RADE mode.
2. Open the MicProfileManager dialog (Setup > Transmit > Mic
   Profiles, or wherever the dialog lives in the v0.5.0 UI).
3. Verify the active profile is RADE.
4. Inspect the profile body. Verify:
   - Leveler: enabled.
   - ALC: bypassed.
   - CFC: bypassed.
   - CESSB Compressor: bypassed.
   - Phase Rotator: bypassed.
   - TX EQ: typically flat / off (per design doc Section 9
     Row 3 sketch).
5. Switch back to SSB mode; verify the profile combo reverts to
   whatever profile was active before RADE.

**Expected:** RADE preset auto-selects on mode entry; bypass routing
matches the design-doc spec; profile restores on mode exit.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** The "RADE" profile is non-editable by design;
a tester wanting to tweak it should clone it first (standard
MicProfileManager workflow).

---

## Row 4: Mode dispatch round-trip

**Hardware:** ANAN-G2 with RX active on a quiet band.

**Reproducer (sub-row 4a: USB <-> RADE-U):**
1. Active slice in SSB (USB) mode; tune to a quiet frequency.
2. Switch to RADE-U via the Mode menu. Confirm:
   - WdspEngine logs `RxChannel destroy` + `RadeChannel create`.
   - Audio path stays alive (no extended silence beyond a single
     frame).
   - VFO flag SNR row appears.
3. Switch back to SSB. Confirm:
   - WdspEngine logs `RadeChannel destroy` + `RxChannel create`.
   - Audio resumes on the SSB demodulator.
   - VFO flag SNR row disappears.
4. Repeat the round-trip 10 times back to back. Verify no crashes,
   no Qt warnings about deleted objects, no audio dropouts longer
   than a single frame (approximately 21 ms at 48 kHz with the
   default buffer size).

**Reproducer (sub-row 4b: LSB <-> RADE-L):**
1. Active slice in SSB (LSB) mode; tune to a quiet frequency.
2. Switch to RADE-L via the Mode menu. Confirm:
   - WdspEngine logs `RxChannel destroy` + `RadeChannel create`.
   - VFO flag SNR row appears; tab #2 paints with the RADE purple
     accent (#a78bfa).
   - Panadapter filter window shows the -2350 to -650 Hz passband
     (mirrored from RADE-U).
3. Switch back to LSB. Confirm the standard SSB filter band
   returns; VFO flag SNR row disappears.

**Reproducer (sub-row 4c: RADE-U <-> RADE-L):**
1. Active slice in RADE-U.
2. Switch to RADE-L via the Mode menu. Confirm:
   - WdspEngine logs `RadeChannel destroy` + `RadeChannel create`
     (the U <-> L flip is also a destroy-and-recreate per the J3
     standing pattern; the channel pointer should differ before
     and after).
   - Panadapter filter window mirrors from +650..+2350 Hz to
     -2350..-650 Hz.
   - VFO flag SNR row stays visible; tab #2 still purple.
3. Switch back to RADE-U. Confirm the mirror flips again.

**Expected:** Channel swap is clean across all three sub-rows; no
resource leaks; no audio glitches beyond a frame; 10x round-trip is
stable. Filter band mirrors correctly between U and L.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Mode swap is intentionally a destroy-and-
recreate (per design doc Section 4 Flow 4); steady-state operation
does not pay this cost. Repeated mode-flipping during a QSO is
expected to be rare.

---

## Row 5: Mode dispatch across band change

**Hardware:** ANAN-G2 on a quiet band.

**Reproducer:**
1. Active slice in RADE mode, tuned to 14.236 MHz.
2. Switch the band selector to 40m (or any other band). Note the
   slice frequency should update to the band default (typically
   7.052 MHz for the RADE convention on 40m).
3. Verify the RadeChannel is NOT destroyed and recreated; the log
   should show only the band/frequency change, not a channel
   swap. Audio path stays alive.
4. The sync indicator on RadeApplet will likely drop to red as the
   new band lacks signal; this is expected.

**Expected:** Band change inside RADE mode does NOT trigger a
channel swap; only the frequency/band metadata updates.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** None.

---

## Row 6: VfoWidget SNR row visibility and colour

**Hardware:** ANAN-G2 or software only (RADE RX optional but
recommended for live SNR colour transitions).

**Reproducer:**
1. Active slice in SSB mode. Confirm the VFO flag has NO SNR row.
2. Switch to RADE-U mode. Confirm the SNR row appears, even when no
   signal is present (shows N/A or 0 dB).
3. Switch to RADE-L mode. Confirm the SNR row stays visible (both
   RADE sidebands surface the same SNR estimate from the neural
   codec).
4. When RADE signal is acquired, the SNR text should colour-code
   per the L1 spec in the design doc:
   - Grey when below the green threshold (typical sub 0 dB SNR).
   - Yellow at the mid-band (e.g. 0 to 6 dB).
   - Green above the high threshold (e.g. > 6 dB).
5. Switch back to SSB; confirm the SNR row disappears.

**Expected:** SNR row is mode-aware (visible in either RADE
sideband, hidden everywhere else); colour transitions follow the
L1 spec.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Colour threshold numbers are tunable in
SliceModel; if the tester finds the L1 defaults uncomfortable,
file a feedback issue instead of a Failed row.

---

## Row 7: RadeApplet behaviour

**Hardware:** Software only (best with a live RADE signal source).

**Reproducer:**
1. Active slice in SSB mode. Confirm the RadeApplet is NOT visible
   (the right-column applet stack shows the standard set).
2. Switch to RADE-U mode. Confirm the RadeApplet appears in the
   right column.
3. Switch to RADE-L mode. Confirm the RadeApplet stays visible
   (both RADE sidebands share the same applet surface; the applet
   gates on either sideband).
4. Verify the profile combo defaults to RADE; verify changing the
   combo to a non-RADE profile is allowed (operator override) but
   triggers a tooltip warning that bypass logic is no longer
   matching the recommended preset.
5. Verify the sync indicator colour tracks the RadeChannel state
   (green = sync, red = no sync).
6. Click the Reset Vocoder button. Verify the RadeChannel emits
   `resetTx` (visible in debug log) and audio continues without
   crashing.
7. Switch back to SSB. Confirm the RadeApplet disappears.

**Expected:** Mode-aware visibility; profile combo round-trip;
sync indicator live-tracks; Reset Vocoder fires resetTx.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** None.

---

## Row 8: Mode menu RADE entries

**Hardware:** Software only.

**Reproducer:**
1. Open the Mode menu in the menu bar.
2. Confirm the menu lists 14 entries total: 12 Thetis-faithful
   modes (LSB, USB, DSB, CWL, CWU, AM, SAM, FM, DIGL, DIGU, DRM,
   SPEC) plus RADE-U and RADE-L as the two trailing
   NereusSDR-native entries.
3. Click Mode > RADE-U. Confirm the active slice transitions to
   `DSPMode::RADE_U` (verify via the VFO flag mode label which
   should read "RADE-U").
4. Click Mode > RADE-L. Confirm the active slice transitions to
   `DSPMode::RADE_L` (label reads "RADE-L"); the mode-action
   exclusivity should uncheck RADE-U and check RADE-L.
5. Click any other mode; confirm both RADE entries clear.

**Expected:** Mode menu surfaces both RADE entries; selecting
RADE-U sets `DSPMode::RADE_U`; selecting RADE-L sets
`DSPMode::RADE_L`; QActionGroup exclusivity ensures only one mode
is checked at a time.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** RADE has a fixed 1700 Hz bandwidth per
sideband (650..2350 Hz for RADE-U, mirrored for RADE-L). Selecting
RADE on a band without a RADE-convention slot is allowed but
unlikely to yield decodes.

---

## Row 9: HL2 RADE bench

**Hardware:** Hermes Lite 2 with a clean ATT/filter chain.

**Reproducer:** Same procedure as Rows 1 + 2 + 7 + 8, repeated on
the HL2.

**Expected:** Equivalent behaviour to ANAN-G2 across RX, TX, UI,
and mode dispatch.

**Status:** [~] Deferred. HL2 ATT/filter audit must close first.

**Known limitations:** This row is **gated** on the HL2 ATT/filter
safety audit, which has not closed as of v0.5.0-rc1. Initial v0.5.0
release may ship without this row's green tick; document as a Known
Limitation in the release notes and close in a fast-follow rc once
the HL2 audit is signed off. See design doc Section 10 Rollout
Risks.

---

## Row 10: TX-on-RADE PA safety

**Hardware:** ANAN-G2 into a dummy load with a power meter and a
temperature readout.

**Reproducer:**
1. Set drive to a typical RADE TX level (matching the SSB voice TX
   level on the same band).
2. Switch to RADE mode. Key MOX.
3. Transmit continuously for 5 minutes; speak into the mic
   throughout (RADE's average power profile differs from SSB,
   carrying more constant duty cycle than peak-to-average voice).
4. Monitor PA temperature; verify it stays within the radio
   manufacturer's safe range (consult Apache Labs spec sheets).
5. Stop TX. Verify the radio returns to RX cleanly.

**Expected:** No thermal warnings; no auto-shutdown; no protection
fault during a 5-minute continuous RADE TX.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** Test is hardware-stress; only run on a radio
with a known-good cooling setup. If thermal protection fires, treat
it as a Failed row and file a PA-protection issue.

---

## Row 11: DEXP/VOX with RADE selected

**Hardware:** ANAN-G2 with mic input.

**Reproducer:**
1. Active slice in RADE mode.
2. Open Setup > Transmit > DEXP/VOX. Toggle DEXP enable.
3. Document the observed behaviour:
   - Does VOX trigger cleanly on speech with RADE selected?
   - Does anti-VOX cancellation work?
   - Does the RadeApplet surface a user-visible warning that DEXP
     + RADE is an untested combination?
4. Toggle DEXP off; verify any audio-bus reconfiguration unwinds
   cleanly.

**Expected:** DEXP + RADE either works, or surfaces the user-
visible warning per design doc Section 10 Technical Risks. No
crashes; no stuck-TX state.

**Status:** [ ] Untested  [ ] Passed YYYY-MM-DD by NAME  [ ] Failed YYYY-MM-DD by NAME (issue: #N)

**Known limitations:** DEXP and RADE were not co-developed; if
audio-routing conflicts surface, log them and file as a v0.5.x
follow-up (do not block v0.5.0 final unless the conflict is
crash-class).

---

## Row 12: Single-RX multi-slice (RADE + SSB)

**Hardware:** ANAN-G2; this row exercises a configuration that
v0.5.0 does NOT officially support.

**Reproducer:**
1. Active slice A in RADE mode.
2. (If the UI permits) place slice B in SSB mode on the same band.
3. Document any state-sharing bugs:
   - Mode menu updating both slices?
   - VFO flag SNR row appearing on the SSB slice?
   - RadeApplet docking confusion?
   - WdspEngine refusing to instantiate the second channel?

**Expected:** This row is exploratory. Document anything that
surprises a maintainer; file follow-up issues for any state-
sharing bugs that would block Phase 3F.

**Status:** [~] Deferred (multi-slice). Phase 3F future.

**Known limitations:** Multi-pan and multi-slice are Phase 3F
features; RADE was designed against the single-RX assumption.
Document but do not block v0.5.0 final.

---

## Cross-cutting checks

- [ ] No `lcRade` warnings in the log during a 15-minute RX-only
      session on a quiet band.
- [ ] No `lcRade` warnings during 5 mode-swap round-trips
      (SSB <-> RADE <-> SSB).
- [ ] CPU usage during a steady-state RADE RX session stays within
      +5% of the SSB baseline on the same band (the RADE neural
      vocoder is the budget here; if CPU jumps higher, file a perf
      issue).
- [ ] No memory growth beyond the first 5 minutes of steady-state
      RADE RX.
- [ ] No crash or hang on shutdown with RADE active.

## Sign-off

Verification owner: J.J. Boyd (KG4VCF). v0.5.0 final is not tagged
until every non-deferred row above is Passed and every cross-cutting
check is ticked. Deferred rows (Row 9 HL2, Row 12 multi-slice) must
each carry a tracking issue or follow-up plan documented in the
v0.5.0 release notes. Row 2 was deferred at the v0.5.0-rc1 cut and
flipped back to Untested after the K-bench follow-up landed (full
real-time integration into TxWorkerThread's semaphore-wake TX pump).
