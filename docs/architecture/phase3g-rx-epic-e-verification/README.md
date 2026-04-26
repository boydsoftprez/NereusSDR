# Phase 3G — RX Epic Sub-epic E Verification Matrix

**Build under test:** NereusSDR `claude/phase3g-rx-epic-e-waterfall-scrollback`

**Setup:** Connect to a real OpenHPSDR radio (HL2 or ANAN). Defaults at first launch (no `~/.config/NereusSDR/NereusSDR.settings`): 30 ms update period, 20 min depth, cap-reached effective rewind ≈ 8 min (post-merge cap raised from 4 096 to 16 384 rows — see plan §"Post-merge cap raise"). Cap break-even moves from 293 ms to 73 ms.

| # | Action | Expected | Pass? |
|---|--------|----------|-------|
| **Live state** | | | |
| 1 | Fresh launch, connect to radio, watch waterfall scroll for 30 s | Waterfall fills top-down (or bottom-up if reverse-scroll on); right-edge shows narrow (36 px) time-scale strip with grey "LIVE" button at top; tick labels read `5s, 10s, 15s, ...` elapsed | |
| 2 | Hover over the "LIVE" button | Cursor changes to pointing-hand | |
| 3 | Hover over the time-scale strip (away from LIVE button) | Cursor changes to vertical-resize | |
| **Pause / scrub** | | | |
| 4 | Click and drag UP from anywhere on the time-scale strip | Strip widens to ~72 px; tick labels switch to `-HH:mm:ssZ` UTC absolute timestamps; LIVE button turns bright red (#c02020) with white "LIVE" text; waterfall content shifts down to show older rows | |
| 5 | Continue dragging — push past max history | Drag bottoms out at the oldest available row; no further movement; no crash | |
| 6 | Release the drag | Position holds; does NOT auto-resume to live | |
| 7 | Wait 5 seconds while paused | Displayed row stays visually fixed; new live rows continue to be recorded silently (verify by checking that the ring buffer's row-count grows — easiest via temp `qDebug()` in `appendHistoryRow` if needed) | |
| 8 | Click the red LIVE button | Snaps back to live; strip narrows; labels return to elapsed-seconds; LIVE button turns grey | |
| **Pan / zoom while paused** | | | |
| 9 | Drag down to pause + rewind ~30 s. Click-drag the freq scale a *small* amount (within 25% of half-bandwidth, e.g. 5 kHz on a 192 kHz panadapter) | History reprojects horizontally to fit the new center; LIVE stays red; rewind history visible but slightly compressed in the freq axis | |
| 10 | While still paused, click a *different band button* (e.g. 20m → 40m) | History clears; LIVE button turns grey; waterfall starts fresh on the new band; cannot rewind through the band switch | |
| 11 | While still paused, change the bandwidth from 192 kHz to 96 kHz via Ctrl+scroll on the freq scale | `bwChanged` triggers largeShift; history clears; LIVE turns grey | |
| **Narrow bandwidth (largeShift threshold note)** | | | |
| 11a | At a 24 kHz CW panadapter (small bandwidth), pan freq by 5 kHz | History clears + LIVE forced grey — note that 25% of half-BW = 3 kHz at 24 kHz BW, so a 5 kHz pan exceeds the threshold. **This is expected behaviour, not a regression.** Document for QA so the test isn't read as a bug. (Source: Task 9 code review.) | |
| **Period change** | | | |
| 12 | Open Setup → Display → Waterfall. Drag update-period slider from 30 ms to 100 ms | Waterfall scrolls slower; capacity changes from cap-bound (16 384 rows = 8 min) to depth-bound (12 000 rows × 100 ms = 20 min). At the 73 ms break-even the height changes, so history may wipe at that point even within the depth dropdown. Effective-depth hint updates to `100 ms × 12000 rows = 20:00 (uncapped)` | |
| 13 | Drag period slider to 500 ms | Waterfall scrolls much slower; capacity = 2 400 rows (depth-bound); LIVE button forced grey if a wipe occurred; hint reads `500 ms × 2400 rows = 20:00 (uncapped)` | |
| 14 | Drag period slider mid-drag from 30 ms to 500 ms in one motion | No mid-drag flicker; only the final value triggers ensureWaterfallHistory after the 250 ms debounce | |
| **Depth change** | | | |
| 15 | At 500 ms period, change depth dropdown from 20 min to 5 min | Capacity drops to 600 rows; hint updates to `500 ms × 600 rows = 5:00 (uncapped)`; history wiped + forced live | |
| 16 | At 500 ms / 5 min depth, change depth back to 20 min | Capacity climbs to 2400 rows; history wiped + forced live (height changed); hint reads `500 ms × 2400 rows = 20:00 (uncapped)` | |
| **Resize** | | | |
| 17 | Drag the divider between spectrum and waterfall up and down rapidly | No flicker; no allocation lag; once released, viewport rebuilds from preserved history (same rows visible at new height) | |
| 18 | Resize the main window narrow → wide → narrow | History preserved across width changes (verify by pausing with rewind data, then resizing — content stays visually intact, just rescaled horizontally) | |
| **Persistence** | | | |
| 19 | Set depth to 5 minutes. Quit + relaunch | Setup → Display → Waterfall opens with "5 minutes" selected. `~/.config/NereusSDR/NereusSDR.settings` contains `DisplayWaterfallHistoryMs0=300000` | |
| 20 | Set depth to 60 seconds. Quit + relaunch | Persists as `DisplayWaterfallHistoryMs0=60000` | |
| **Disconnect** | | | |
| 21 | While paused with rewind data, click Disconnect | RadioModel::connectionStateChanged emits → MainWindow lambda calls SpectrumWidget::clearWaterfallHistory → ring buffer flushed → LIVE forced grey. Reconnecting starts fresh history. (Wired via MainWindow because NereusSDR has no SpectrumWidget::clearDisplay() equivalent — see plan Task 4 §authoring-time.) | |
| **Multi-pan (Phase 3F is planned, not yet — sanity-check single-pan only)** | | | |
| 22 | Confirm only one panadapter exists | Sub-epic E ring buffer is per-`SpectrumWidget`; multi-pan validation is deferred to Phase 3F | |
| **Memory** | | | |
| 23 | At default settings, run for 10 minutes; check Activity Monitor / `top` | RAM growth from sub-epic-D baseline ≈ 128 MB (= 2 000 × 4 × 16 384 bytes for the QImage history + ~128 KB for timestamps). Cap raised post-spec-merge from 4 096 to 16 384 rows for ~8 min effective rewind at default refresh — see plan §"Post-merge cap raise". | |
| 24 | Slow update period to 1000 ms; check capacity | History capacity = 1200 rows ≈ 9.6 MB at 2000px width | |
| **Visual states (screenshot for PR body)** | | | |
| 25 | Live state, narrow strip, grey LIVE | Take screenshot | |
| 26 | Paused state, wide strip, red LIVE, UTC labels | Take screenshot | |
| 27 | Setup → Display → Waterfall with rewind group visible | Take screenshot | |

## Deferred / known limitations

- **Multi-pan history sharing:** each `SpectrumWidget` owns its own ring buffer (per the design doc). Sharing is YAGNI until Phase 3F lands and a real use case emerges.
- **Stored format is RGB32, not raw dBm:** changing the colour palette / black level while paused continues to show *old* palette in the rewind window until those rows roll off. Faithful to AetherSDR; revisiting would 5× the per-row memory cost.
- **No per-row centerHz metadata:** rewind across a band change is impossible by design (Q4 = B). The history clears on largeShift instead.
- **Drag-to-zero does NOT auto-resume:** only the LIVE button truly returns to live. Drag-to-bottom would auto-bump back to >0 on the next row arrival (faithful to upstream `m_wfLive` semantics).
