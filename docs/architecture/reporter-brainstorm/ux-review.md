# Roundtable: design review and UX walkthrough (2026-10-20)

This covers every screen built so far:

- desktop pan, panel and pop-out (placement.html)
- iPhone (phone.html)
- iPad (ipad.html)
- public page (web.html)
- archive (archive.html)
- review list (review.html)
- settings (settings.html)

It merges an independent read-only review (crew-reviewer) with my own walk through each screen as a user. Where a claim could be checked, I checked it in the mockup source.

## Shape

The core loop holds together. Someone talks, every station's own reading of them appears, people talk about it in messages, and the turn goes into history. Each screen serves one job:

| Screen | Job | Fits? |
| --- | --- | --- |
| Pan tags + flag line | notice who is on the air near you | yes |
| Right-column panel / pop-out | follow your own frequency | yes |
| Phone sheet, iPad applet | same thing, touch first | yes, phone thinner (below) |
| Public page | anyone, no radio, browse everything | yes, after today's dense rework |
| Archive | "who heard me, and for how long" | yes |
| Review list | helpers deal with reports | yes |
| Settings | on/off, hide, away, archive | yes |

Three shape problems:

1. **"Who hears me" gets less room exactly when you transmit.**
   - On the desktop panel the talker block, earlier turns and messages keep their space while you are keyed.
   - The iPad mockup now shrinks the talker block and drops earlier turns while you transmit. Desktop and phone should do the same.
2. **Reading a frequency and tuning to it are mixed up.**
   - Checked in the source: the desktop band links call `setBand` (placement.html:480), which moves your slice. The phone's Busy Elsewhere band links call `tuneTo` (phone.html:436).
   - The pop-out list and the public page read without tuning, so the same-looking link does two different things.
   - Rule to adopt everywhere: **a list or link reads, a pan tag tunes.** Reading always offers "Back to my slice".
3. **The phone does less than the desktop.**
   - The phone has no offset column, no list of earlier turns, and no way to type a name for an unknown turn.
   - JJ's ruling was "like the desktop", so these belong on the phone too.

## Completeness: what no screen shows yet

Most important first:

1. **First run notice.** Turning Roundtable on makes your callsign and readings public. One sheet on first start, with the hide switch right there.
2. **Connection states.**
   - Needed states: connecting, offline, and reconnecting after a drop.
   - Each reading should show its age, and old ones should go dim. A reading 40 seconds old should not look live.
   - Settings has a status line, but the panel, phone and public page do not.
3. **An empty room.** What you see when you are the first one on a frequency: "Nobody else here yet. Your readings of the next talker will appear here."
4. **When you count as in a room.** Tuning across 7.185 for a second should not put you in it. Suggest about 10 seconds of dwell before you count, show it as "joining".
5. **Naming after the turn ends.** Earlier turns now have "Name this turn" (iPad, public page shows the names). The desktop and phone need it too.
6. **A double with nobody to attribute it to.** For two unknowns keyed at once, show "two stations keyed at once" and keep the readings out of both turns, as decided.
7. **Messages.** Still missing:
   - a length limit with a counter;
   - a failed send that keeps your text;
   - deleting your own message;
   - what happens to your draft when you retune.
8. **Mentions.** Highlighting is in settings, the iPad and the phone mockup. Still needed:
   - an unread count for mentions on the flag line, separate from other messages;
   - clearing the count when you read it.
9. **Reporting.**
   - The review list has reasons. The report sheet on the desktop and phone needs a reason picker (optional), plus "Report a wrong name" from a turn.
   - Mute from a reading row, not only from a message.
10. **Pop-out window.** It should remember its size and which monitor it was on, as floated pans do.
11. **iPad without the applet column.** The notes say it falls back to the phone sheet. Not drawn yet.
12. **Phone landscape and the phone keyboard.** The message box must stay above the keyboard, with the PTT out of reach while typing.

## Usability: walking each screen as a user

### Desktop

- **Tags on a busy band.** With 40 or more rooms on 40 m, tags overlap.
  - Suggest: tags fold into a count ("+6") when they collide, and they hide below a zoom level.
  - The settings choice "Rooms with messages only" helps, but it is not the default.
- **Flag line.** "Roundtable · 9 · K4ABC talking" is good. The unread dot should count mentions only, otherwise it is always on.
- **Panel order.** Talker, readings, earlier turns, messages, box is right. Messages need a "jump to newest" when you have scrolled up.
- **Nested scrolling.** The readings list and messages each scroll inside the panel, inside a scrolling column. Make the panel one scroll with the message box pinned.

### iPhone

- **PTT next to the message box** is the biggest risk on any screen. A thumb reaching for Send can key the radio.
  - Fix: keep a 24 px gap, and make PTT press and hold while the sheet is open.
- **"You" is red on the phone and green on the desktop.** Red already means transmitting and a double. Use the VFO cyan for you everywhere (the iPad and public page now do).
- **AWAY note.** It should say that AWAY stations can't post or name turns, so nobody waits on a reply.

### iPad

- Fixed today:
  - a quieter waterfall;
  - tags that clear the flag;
  - a one-line flag line;
  - readings with full row height;
  - you no longer appear in readings of a frequency you are not on.
- **The S-Meter sits above Roundtable** and pushes messages below the fold. Applets already reorder by their grip, so this is the user's call. Default Roundtable second is fine.

### Public page

- Reworked today for a couple hundred rooms:
  - dense rows;
  - band buttons with counts;
  - a band map;
  - quiet rooms folded away;
  - search across talker, readings and posts.
- **Freshness.** Add an "updated 2 s ago" line to the room pane, and dim a room whose last reading is over a minute old.
- **Sorting.** Frequency (default) and Most stations are in. "Newest activity" would help someone arriving to find where things are happening.
- **Room link.** One link per room is in (Copy link). A net control will want the link to keep working when the room is quiet. It should open the quiet room, not "not found".

### Archive

- **Many stations.** With 30 or more stations a session's timeline gets tall. Cap the timeline at the 12 stations that heard you longest, then "Show all 34".
- **Tone.** "Nobody reported you" is honest. Keep it neutral, never "failed".
- **Getting there.** You reach the archive from Settings and from the public page's station key. The desktop panel could link "My archive" in its menu.

### Review list

- **Two helpers on one report.** If a report was acted on while you were looking, say so and show what the other helper did. Don't let both act.
- **Order.** Oldest first by default, so nothing sits forever. A count of reports older than a day.
- **Undo window.** Undo is everywhere, which is good. It should say how long it stays available.

### Settings

- **Hide my station.** It should say plainly what hiding does:
  - you vanish from rooms, readings and the public page;
  - you can still see everything;
  - your own readings of others are not sent either.
  - Confirm that last rule; it is the honest version.
- **Delete my archive.** It sits behind a confirm panel, which is right. Say that the deletion can't be undone, and that removed signal history is gone for everyone who would have seen your readings.

## Consistency across screens

- **State glyphs.** The public page now uses shape as well as color: filled dot talking, ringed dot unknown, striped diamond doubled, hollow ring quiet. The desktop, phone and iPad still use color alone. Carry the shapes everywhere.
- **Text size and contrast.**
  - Several mockups use 9 to 10 px labels. The program raises any text under 11 px to 11 px (WorkspacePreview), so the mockups should too.
  - Hold 4.5:1 contrast for body text.
- **Look.**
  - The desktop screens and both web pages now use the 2026.10.1 workspace look.
  - The phone and iPad now use the app's workspace look too (the iPhone and iPad restyle, merged 2026-10-23), with Roundtable's talking, unknown and doubled colors on its green, amber and red.
- **Keyboard.** Desktop and public page:
  - "/" for search;
  - up and down arrows through rooms;
  - Enter opens a room;
  - Escape goes back to your slice.
  - Earlier turns as a real list, reachable by keyboard.

## Top UX improvements, most valuable first

1. A list or link reads, a pan tag tunes. Make that true on every screen, with "Back to my slice". **Approved by JJ 2026-10-20 and applied** to the desktop panel, pop-out and phone sheet.
2. Separate PTT from the message box on the phone: a gap, and press and hold while the sheet is open. **Approved by JJ 2026-10-20 and applied** (24 px gap, Hold label, dashed ring).
3. Readings get the space while you transmit, on the desktop and phone as on the iPad. **Approved by JJ 2026-10-20 and applied.** Today's keyed meters stay untouched: the phone's gauges on the waterfall, and the desktop S-Meter's power and SWR face. The phone sheet sits below the gauges while keyed. With a tuning dial on, the sheet stops above a strip that keeps PTT at the left and the dial at the right.
4. Show freshness: the age of each reading, dimming when old, and the connection state on every screen. **Approved by JJ 2026-10-20 and built on board.html:** an Age column, rows dim past 20 seconds, rooms dim in lists past a minute, and one connection line (live, reconnecting, offline, connecting).
5. A first-run notice that you are public, with the hide switch in it. **Approved by JJ 2026-10-21 and built on board.html:** a "Roundtable is public" sheet listing what goes public, the hide switch, Stay hidden or Go live, asked once per station; a hidden station sees a banner, can't post, and its own reading shows NOT SENT. iPad uses a form sheet.
6. Mentions get their own unread count. Plain messages don't light the flag. **Approved by JJ 2026-10-21 and built on board.html:** the flag line and room lists show @1 only for messages that name you; other new messages show inside the room; opening the room keeps the count, a pill jumps to the mention, and it clears once on screen.
7. Shapes as well as colors for talking, unknown, doubled and quiet, everywhere. **Already true on board.html (2026-10-21); carry to the desktop at spec time.**
8. You are cyan everywhere, never red or green. **Already true on board.html; carry to the desktop at spec time.**
9. Name a turn after it ends, on every screen, and show every name given, never one winner. **Already true on board.html; carry to the desktop at spec time.**
10. Pan tags fold into counts when crowded. **Approved by JJ 2026-10-22 and built on board.html:** overlapping tags fold into one +N tag (stacked glyphs show what is inside); a tap lists the rooms, and a row tunes; your slice's room and rooms that name you never fold; zooming in opens the folds.
11. One scroll per panel, message box pinned, "jump to newest". **Approved by JJ 2026-10-23 and built on board.html:** the phone sheet and the iPad applet column are one scroll with the message box pinned; messages that arrive while you read further up wait below and an "N new" pill above the box counts them (a pill naming you comes first); at the newest message the view follows; a draft stays put while one arrives. New messages carry a blue edge and "new" until seen. Desktop panel at spec time.
12. An 11 px minimum and 4.5:1 contrast, as the program already enforces. **Approved by JJ 2026-10-23 and built on board.html and web.html:** an audit of every Roundtable text run on the board finds nothing under 11 px and nothing under 4.5:1, apart from disabled controls. Old readings and quiet rooms now read grey (about 5:1) rather than faded; the mention badge is cyan.
13. Keyboard paths on the desktop and public page. **Approved by JJ 2026-10-23; built on web.html:** / searches, the arrows move through the rooms (each shows as you land on it), Enter goes into the room, whose earlier turns are a list the arrows walk, and Escape steps back (room, then search, then band). The desktop panel at spec time, where Escape goes back to your slice.
14. The phone gets the offset column and the earlier turns list. **Approved by JJ 2026-10-23; already true on the phone** (offset column, earlier turns in the full sheet, Name on unknown turns); the narrow iPad applet now shows earlier turns too.

## My calls, open to your veto

- The review list lives on the website, behind the same station key as the archive.
- Roundtable settings are a Spot Hub tab after PSK Reporter.
- "Delete my archive" exists.
- The away choice: keep reporting marked AWAY, or stop reporting.
- Mention highlighting is on by default.
- The public page lists readings nearest first.
- The public page folds quiet rooms by default.
- The public page's band map appears only once a band is picked.
