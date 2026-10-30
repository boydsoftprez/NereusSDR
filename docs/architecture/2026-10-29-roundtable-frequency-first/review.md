# Expanded Roundtable board — integrated source/requirements review

Reviewed 2026-10-29. **Result:** the expansion represents most missing live and retained workflows, but the reviewed v2 source has concrete cross-surface state and action bugs. These are local mockup correctness issues; no production, native, network, authentication-service or radio certification is claimed.

## Scope and evidence

One integrated source/requirements review of the lead-owned fragment and `/tmp/nereus-roundtable-complete-board-implementation.md`. Frozen candidate: `/files/roundtable-complete-states-review-v2.html`. The root owned the browser throughout this review; this reviewer performed **no CUA/browser actions**, no artifact/source edits, no builds/tests, no device/radio actions and no delegation. Only this report was written.

Reviewed source: `/Users/j.j.boyd/.codex/visualizations/2026/10/28/01a17ff3-3eca-7220-b09d-838e93f96826/roundtable-board-body.html` (150 lines when read). The canonical board path remains `/Users/j.j.boyd/NereusSDR/.claude/worktrees/agent-a0a49f5c3d5616f36/docs/architecture/2026-10-29-roundtable-frequency-first/board.html`.

References: retained D9/D14/D21/D25/D27/D28, sections 8.5–8.6, 11–13 and message write rules of `docs/architecture/2026-10-23-roundtable-design.md`; original UX review; exact constants and behavior from `ios/NereusApp/Roundtable/RoundtableFirstRunSheet.swift`; prior integrated review. The proposal's individual-frequency behavior supersedes old room architecture. No old workflow approval ceremony was imposed.

All findings below are **source-confirmed** with deterministic proposed UI reproducers for the root's visible-flow check. Later lead corrections are not independently verified by this report.

## Actionable findings

### 1. P2 — public website details expose radio tuning

**Source:** `showPeople(ps,web)` (line 94) accepts `web` but ignores it. Every non-own station receives `data-explicit` / `Tune A here`; the shared click handler sends that value to `tune()`.

**Reproducer:** reset; on the public website open the cochannel stack (or a crowded group); choose another frequency's Tune button in the resulting details. The shared station's selected slice retunes even while the visitor is public.

**Impact:** contradicts public browse-without-tuning and the newer selected-slice authority model. The ordinary map/list path is safe, but its detail path is not.

**Scoped correction:** retain context on the detail dialog; public web details offer Browse only. Desktop/phone details may expose explicitly labeled local Tune. Do not infer authority from a dialog that lost its originating surface.

### 2. P2 — public turn naming bypasses website authentication/follow and current-frequency gates

**Source:** `turnHtml()` and the unknown `talkHtml()` Name button use `canPost()`. That helper checks station hidden/away/link/enable/consent/block state, but no website auth/follow state. `nameTurn()` and the `rf-name-form` submit handler also use only `canPost()`, with no originating surface or current turn-frequency check.

**Reproducer A:** reset, remain a public website visitor, open Unknown signal, click Name in the website pane, enter a callsign and save. Source allows updating the turn suggestion despite the visitor being read only.

**Reproducer B:** select slice B at 3.980 MHz, then name the globally shown earlier unknown turn at 3.965 MHz; eligible=true still permits it. Being present during the turn is represented, but current frequency eligibility is not enforced.

**Scoped correction:** make name eligibility explicit per origin and turn: allowed station state, authenticated/following website when applicable, unknown kind, eligible historical attendance/window, and current selected-frequency proximity. Use the same predicate in button state, dialog open and submit. Preserve every existing name and archive exclusion.

### 3. P2 — browsing another frequency shows the original frequency's live talker and earlier turns

**Source:** `renderPanel(panel)` computes the correct viewed `h` and nearby people/messages, but always calls unparameterized `talkHtml()` and `turnHtml()`. Those render the single global 3.965 MHz talker/report set and unfiltered `turns` array.

**Reproducer:** browse W1ABC at 3.980 MHz on the website (or Read it in the desktop applet/popout). The neighborhood says 3.980 and messages filter there, yet W4KZO's 3.965 live reports and earlier turns remain immediately under that header.

**Impact:** the panel attributes the wrong RF activity to the frequency being browsed and also exposes unrelated naming actions.

**Scoped correction:** render talker, current reports and earlier turns against the viewed frequency/turn identifiers. Show a quiet/no-current-turn state for W1ABC if that fixture has no activity; retain other-frequency message/session history with its explicit retained-history labeling.

### 4. P2 — hidden/unanswered station's own live reports remain on the public surface

**Source:** the cochannel/self-tag path excludes KG4VCF when hidden or `!firstAnswered`, and the local-reading paragraph says NOT SENT, but `talkHtml()` still renders `reports[talker]` including KG4VCF on every panel. It filters only `notCounted`.

**Reproducer:** reset, replay First run without answering, or choose Stay hidden; inspect the public website's live report table. The KG4VCF reporter row remains despite the read-only/hidden state. Own-TX status similarly remains a known global state if previously selected.

**Scoped correction:** distinguish local unsent readings from published live fixtures. Suppress this station's public presence/report/keyed identity before consent and while hidden, while preserving permitted retained history and the explicitly local NOT SENT reading. Other stations can still report/name an unknown signal, as D21 says.

### 5. P2 — mention visibility timer can clear unread while a modal or onboarding overlay covers the message

**Source:** `visibleMessage()` (line 131) checks viewport/scroll rectangles and whether the panel itself is inside a closed dialog. It does not check an open detail/settings modal making underlying panels inert, or the opaque first-run layer over the phone.

**Reproducer:** receive a mention while its message is in view, open Settings/details before two seconds elapse, leave the dialog open for more than two seconds, then close it. Bounding rectangles remain inside the viewport, so the timer can mark seen while covered. This violates continuous actual visibility.

**Scoped correction:** pause/reset eligibility for surfaces occluded by a modal or first-run overlay. At minimum require the panel to belong to the active modal when one is open, and exclude the phone panel while its onboarding layer is visible. Keep cross-device clearing once a genuinely visible occurrence reaches two continuous seconds.

### 6. P2 — helper duration comes from the first report rather than the acted-on report

**Source:** `renderPrivate()` repeats `id="rf-block-duration"` for every open queue item. The review handler reads global `$('#rf-block-duration')`.

**Reproducer:** create a second report, enter demo helper role, select For good on the second row and click that row's Block posting/naming. The first selector's default 1 hour is used and recorded.

**Scoped correction:** use a per-row class or unique ID and resolve duration within the clicked `.rf-review-item`. The static-ID assertion in the implementation report does not cover dynamically rendered repeated IDs.

### 7. P2 — wrong-name review offers actions that report success without changing the target

**Source:** every queue item offers Remove message, Block and Stop counting regardless of target kind. For `turn:u1`, the handler finds no message and no station call; it still changes queue status to removed/blocked/not-counted and writes a success log/undo record.

**Reproducer:** use Wrong name on an unknown earlier turn, submit the report, enter helper review and choose Remove message or Block. The queue reports action taken but the names/station state remain unchanged.

**Scoped correction:** render actions by supported target type. A wrong-name report needs a concrete supported moderation response or an honest Leave/route-to-station path; never claim a message was removed when the report targets a turn. Preserve explicit human moderation and no automatic removal.

### 8. P2 — Stop counting does not suppress unknown/doubled report and keyed-state fixtures

**Source:** `allPeople()` and the normal known-report table filter `notCounted`; unknown reports hardcode K4ABC, doubled reports hardcode N4XYZ, and `stateLabel()`/cochannel talker status can still name a stopped known keyed station.

**Reproducer:** report K4ABC's station and choose helper Stop counting; switch to Unknown signal. K4ABC is removed from presence but still appears as the live unknown-turn reporter. Similarly stop N4XYZ then use doubled state; its live combined reading persists.

**Scoped correction:** apply live exclusion at the common report/keyed-state fixture layer, for every turn kind. Keep stored earlier-turn/archive rows intact and marked, per retained behavior.

## Coverage that is represented coherently in source

- **Known/quiet/unknown/doubled/own TX:** endpoint sentences are explicit; unknown observations keep an unknown turn ID; doubled samples are expressly excluded from both named turns and archives. Own-TX report map measures KG4VCF from other stations and does not include a self measurement in its report rows. Synthetic keyed gauges/PTT are clearly labeled. Frequency scoping still needs finding 3.
- **Unknown naming/archive exclusion:** suggestions retain every name/count, changing this station decrements only its previous suggestion, ineligible attendance disables Name, and no archive renderer derives sessions from suggested names. Auth/current frequency still need finding 2.
- **First-run wording and shared answer:** title, intro, four items, private archive sentence, Hide title/text, Stay hidden/Go live and footer match native constants. The checkbox changes button emphasis; either explicit choice records one shared in-memory decision. Dedicated renderer now targets `#onboarding .rf-first-content`, separately from phone overlay. No phone Archive control was added (D28).
- **Messages:** normalization is NFC; controls except newline are removed; send enforces 500 Unicode code points/10 lines; counter begins at 400; deterministic failure retains draft and offers retry; own Delete and helper Remove create distinct tombstones. Formatting escapes text and only creates bold/italic/http(s) links. No arbitrary HTML evaluation is present in message rendering.
- **Drafts and scrolling:** drafts key by slice/frequency and are not queued while disabled; input events avoid rebuilding the list; per-panel scroll is retained unless following the bottom; incoming messages add new indicators and mention-specific jump controls. `seen` is global across represented device occurrences, with a two-second timer; occlusion needs finding 5.
- **Phone layouts:** source genuinely changes phone frame/stage dimensions for landscape, moves the sheet/composer above the visible keyboard, hides simulated PTT while typing, and implements the full-sheet handle. This is source evidence only; root must judge actual fit/touch/keyboard geometry.
- **Archive:** private workspace returns a sign-in refusal before owner/helper content, fixed owner sessions only, an empty session, capped 12 reporters/Show all14, known-turn endpoints, reporter tracks, WATCHED/loss/gap explanations, AWAY and retained text placeholders. Unknown suggestions and doubles do not add archive sessions. Demo deletion and restoration are explicitly in memory.
- **Helper role/undo:** helper rendering is gated behind owner sign-in and the explicit demo helper role. Actions log the demo helper; undo restores captured state and refuses after ten minutes. Duration/target behavior and live stop-counting still need findings 6–8.
- **Frequency/collision pattern:** exact-Hz cochannel grouping and shared coordinate ticks remain present; public ordinary map/list browsing and station Read remain distinct from explicit Tune; boundary uses inclusive 5000 Hz. The earlier public detail regression is finding1.

## Remaining coverage limits rather than production defects

- Composers are still single-line `<input>` elements. The 10-line validation branch exists, but multiline message authoring/pasting is not represented by those controls. A textarea would make the claimed multiline path reviewable.
- Helper concurrent-action refusal/refresh is not represented by a conflicting-helper scenario; helper membership changes are likewise absent. These are retained requirements, not reasons to demand a real server for the board.
- Signal rows in the live talker table omit the retained distance/offset columns, and Mute remains available on messages/settings rather than reading rows. The nearby presence list has offset/grid, but it is not the same reporter table.
- Source implements deterministic ages, roles, traces, timestamps, eligible flags and stored fixture sessions. It does not establish real-time expiry, production archive authenticity or network security. No such certification is required for this mockup.

## Handoff

Important findings were sent to the lead during review, before this report was completed. The lead remains responsible for visible reproduction and scoped corrections. This report does not request an audit loop or repeat checks already owned by the root.

---

## Bounded v3 correction check — 2026-10-29

Scope limited to the eight P2 findings and three retained gaps above. Reviewed the v3 fragment and implementation-report correction appendix, plus `/tmp/check-roundtable-v3.py` and generated `/tmp/check-roundtable-v3.js`. Root retained exclusive browser ownership. No CUA, source/artifact edits, agents, builds or device actions. The supplied direct-function harness was executed unchanged (`node /tmp/check-roundtable-v3.js`, exit0), then a read-only in-memory extension exercised the remaining hidden-TX public-detail case. No harness file was changed.

### Disposition by finding

| Original item | v3 disposition | Current source/function evidence |
| --- | --- | --- |
| 1. Public details Tune | **Corrected in source** | `showPeople(ps,web)` omits `data-explicit` when web=true and records dialog origin. Dispatcher independently refuses explicit/tuneRead from web-origin detail or web panel. Harness confirms public details have Browse and no Tune; local details still have explicit Tune. |
| 2. Public/current-frequency Name gate | **Corrected in source** | `nameAllowed(t,ctx)` is shared by rendered button state, `nameTurn()` and form submit. It requires eligible unknown turn, allowed station, LSB context, current/view proximity and, for web, authenticated+following+notEarlierBrowse+browse=current. Context attributes survive turn detail and naming form. Harness confirms public refusal, followed owner acceptance, local acceptance and distant/mode refusal. |
| 3. Global activity under other-frequency heading | **Corrected in source** | `renderPanel()` passes `viewContext(panel)` to talk/turn renderers. `talkHtml(ctx)` renders no attributed turn outside the fixture's ±5000Hz neighborhood or mode; `turnHtml(ctx)` filters turns likewise. Harness checks the 3.980 view contains neither W4KZO live attribution nor 3.965 earlier fixtures. |
| 4. Hidden/unanswered own public data | **Partially corrected — remaining P2 below** | Main `talkHtml(web)` suppresses own reporter rows by canPublish, omits local reading, and hides named own-TX state. Web cochannel rendering uses unknown state when ownTX cannot publish; self tag inclusion uses canPublish. However public `showPeople()` rows still use context-free `report(p)`, retaining hidden own-TX attribution. Harness's main-panel privacy assertions pass but did not cover this detail path; extended check confirms it. |
| 5. Covered mention visibility | **Corrected in source** | `visibleMessage()` refuses background document, underlying panels when another dialog is open, and the phone beneath its first-run overlay. Interval deletes visibleSince whenever no eligible visible occurrence exists, resetting the continuous two-second requirement. Harness confirms modal and onboarding coverage refusal. Actual browser visibility timing remains root-owned. |
| 6. Wrong helper duration row | **Corrected in source** | Duration is `.rf-block-duration` resolved within `button.closest('.rf-review-item')`; no repeated selector IDs. Harness acts on second report with24hours and verifies that exact scoped card/state, then Undo restores it. |
| 7. Wrong-name false-success actions | **Corrected in source** | `reviewTarget()` distinguishes message/turn/station. Wrong-name report selects a named suggestion; its helper card offers Remove reported suggestion rather than message removal or identity-free block. `applyReview()` decrements the targeted count, captures mine/count state, and Undo restores. Harness checks effect, Undo and type-appropriate card actions. |
| 8. Stop-counting live fixture leaks | **Corrected for reported cases in source** | Unknown K4ABC and doubled N4XYZ report branches now consult notCounted; known stopped talker reports/state are suppressed, and combined keyed labels lose stopped names. Cochannel lists and live known reports also filter. Historical turns/archives remain unchanged. Harness covers unknown reporter, doubled reporter/keyed names and known stopped talker. |

### Disposition of the three retained gaps

| Gap | v3 disposition | Evidence |
| --- | --- | --- |
| Multiline composers | **Represented in source** | Four real textarea composers use `[data-draft]`; draft/counter/focus/selectors were updated from input. Existing send normalization/500-codepoint/10line validator remains. Harness accepts500 astral characters, rejects501 and11lines, retaining invalid draft. Actual textarea geometry/keyboard fit remains root-owned. |
| Helper stale/conflicting action | **Represented and guarded in source** | Per-report revision accompanies actions. `applyReview()` rejects non-open/revision-mismatched reports before mutation. Another helper acted fixture records K4HELP's result and deliberately attempts a stale action. Harness verifies refusal and no block/log mutation. |
| Reading distance/offset and Mute/Report | **Represented in source** | Known report table includes km, Hz offset, signal and age; synthetic distance ordering precedes missing-grid callsign ordering, without strength ranking. Unknown/double return no distance. `readingActions()` supplies Mute/Report on remote reading rows and details. Harness checks ordering, missing distance and action markup. Root owns actual fit at desktop/phone widths. |

### Remaining important issue in checked v3

**P2 — public hidden own-TX details still disclose the known talker in each report sentence.**

Reproducer: choose simulated own TX; hide the station or replay unanswered First run; open the public website cochannel stack's details. The dialog header correctly says no published known talker, but each station row calls `report(p)` with the global `turnState==='tx'`. Its report sentences still resolve the talker to KG4VCF, yielding `W4KZO heard KG4VCF at …` and `K4ABC heard KG4VCF at …`.

Direct source-function evidence: after the unchanged supplied harness passed, a read-only extension set `hidden=true; firstAnswered=true; turnState='tx'`, called `showPeople([{call:'W4KZO',hz:3965000,age:2},{call:'K4ABC',hz:3965000,age:4}],true)`, and printed `Hidden TX public detail still names own station: true` from `detail.includes('heard KG4VCF')`.

Scoped correction: carry public publication context into detail report rendering as well as its header. Use unknown/no-published attribution while this station cannot publish keyed state; preserve local unsent readings and allowed retained history. This is the remaining branch of finding4, not a newly expanded feature request. The lead was notified early. No later correction is independently verified in this bounded check.


## Lead final integration evidence

The independent reviewer checked v3 source. The lead then closed its remaining public hidden-own-TX detail branch in the shared `report(p, web)` function, passed publication context from both directory and detail renderers, excluded unpublished own rows, and kept an own cochannel detail row at its actual matching frequency. The actual-function regression `/tmp/check-roundtable-public-attribution.js` passes: public hidden-TX attribution contains no KG4VCF identity while local attribution remains. A Chrome walkthrough also confirmed no KG4VCF disclosure in public hidden-TX details/directory.

Browser evidence on the integrated board (local fixtures only):

- Public station details contain zero Tune controls; browsing W1ABC changes website to3.980MHz while slice A stays3.965MHz. Its current pane shows no attributed3.965 turn. Public naming is disabled.
- Unknown and doubled samples remain explicit, with combined RF excluded from both named archives.
- Failed multiline send retains the two-line draft; retry posts once and clears it.
- Mention count remains@3 while Settings covers it for2.2s. After jump and2.2s of visible exposure, all3 new markers and the shared flag count clear.
- First-run Stay hidden applies across station views, disables posting and shows the answered-on-another-device note. Both notice buttons fit the dedicated full-height view.
- Phone keyboard composer is above the keyboard with PTT hidden. Landscape composer fits its874x402 screen; screenshot corrections separate status/header and keep stack/scale/PTT clear.
- Public archive access refuses sign-in; demo owner access opens session and known-turn endpoint tracks. Helper Remove updates actual message rendering; Undo restores it. Another-helper action refuses the stale action and identifiesK4HELP.
- Final browser console has no errors and the board has no horizontal page overflow at its default1512px viewport.

The final syntax check passes. The implemented state board includes first-run, turn, message, phone-layout, popout, archive, settings and moderation routes. These are interactive design examples, not evidence of native apps, real authentication, network delivery or radio transmission. Root layout/publication corrections after the v3 check are lead verification, not a claim of another independent full review.


### Miles default and true bearing update

Applied JJ’s request: Miles default, Kilometres option under Roundtable settings, true bearing shown by default. Direction is explicitly talker to reporter. Verified in Chrome that 128 km renders as 80 mi, the 082°T fixture stays unchanged when units switch, every represented panel updates, and missing-grid values stay blank. JavaScript syntax passes. This is a focused display-preference change; no repeated application test suite was run.
