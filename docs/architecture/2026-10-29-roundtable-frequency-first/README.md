# Roundtable: people on frequency

Open [board.html](board.html) in a browser. The board is self-contained and works offline. It uses the repository’s established workspace board, desktop panel and public-page references. All signals, roles, commands and messages are in-memory examples.

The proposed live interaction shows individual callsigns at actual frequencies; stations sharing a frequency form a compact stack. A separately identified last talker sits above **Heard by / Signal dBm / Age**. Details spell out both endpoints. New chat is evaluated pairwise within an inclusive 5,000 Hz of the selected slice, within the represented mode family. Retained station history keeps sender frequency and time; the public website separates nearby conversation from earlier browsing.

## Working review routes

- Desktop: signal stack, selected slice, explicit tuning versus Read, docked panel and local popout.
- iPhone: portrait/half, full sheet, keyboard and landscape; shared feature controls without a desktop. No Archive control, preserving D28.
- Website: public frequency browsing without radio tuning, simulated owner sign-in/follow authority, current/earlier conversation, private owner archive and simulated helper review.
- State controls: known, quiet/after, unknown, doubled, own simulated TX, first-run read-only/hidden/live, connecting/reconnecting/offline and away.
- Messages: 500 Unicode code points and 10 lines, counter from 400, formatted text and safe full-address links, frequency/slice drafts, failure/retry, own deletion, local mute/report and visible-exposure mention/new tracking.
- Turns and records: endpoint readings and ages, eligible unknown-turn names with all suggestions/counts, archive exclusion for names and combined RF.
- Helper: target-specific removal/block/stop-counting, per-report durations, undo, and stale action refusal. No automatic report-count removal.

Use the board’s **Coverage matrix** to jump to the screens. The [review and final integration evidence](review.md) distinguish independent source review, lead browser checks and product limitations.

## Sources

- `../reporter-brainstorm/mockups/board.html`: board/workspace styling and phone shell.
- `../reporter-brainstorm/mockups/placement.html` and `web.html`: desktop/public-page references.
- `../2026-10-23-roundtable-design.md` and `../reporter-brainstorm/ux-review.md`: retained privacy, state, messaging, archive and moderation requirements.
- `ios/NereusApp/Roundtable/RoundtableFirstRunSheet.swift`: exact native notice words.

`board-source.html` is the editable fragment. The rendered board prepends the established board’s style blocks. Product app/server code is outside this artifact change.

## Captured views

[Desktop](previews/desktop.jpg), [public website](previews/web.jpg), [phone keyboard](previews/phone-keyboard.jpg), [phone landscape](previews/phone-landscape.jpg).
