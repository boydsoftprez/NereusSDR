# Roundtable strip (desktop)

Date: 2026-11-16. Decided by JJ in the 2026-11-14 brainstorm ("Strip under
spectrum", then "While Roundtable is on"). Reference picture: the brainstorm
board's option 2 (`placement/board.html`, capture `strip.png`), built on the
2026-11-03 overlay board in `docs/architecture/2026-11-03-roundtable-overlay/`.

## Why

The Roundtable card stood on the foot of the spectrum and hid the trace beside
the dial, almost the full height on a short panadapter. The small tags for other
frequencies sat on the trace too.

## What the operator sees

1. **A 30 px strip** sits between the frequency scale and the waterfall. The
   spectrum keeps its full height. The waterfall is 30 px shorter.
2. **When it shows.** The strip is there whenever Roundtable is on in that
   window, on a quiet band too, so the waterfall never jumps. It is gone when
   Roundtable is off. Each panadapter has its own strip.
3. **Your frequency's line.** On the panadapter that holds the slice you are
   working, one line sits in the strip beside the dial: who is talking, how long,
   then each station's reading of them side by side. Each reading has a thin
   meter under it with its peak mark. When the readings do not fit, the last
   place says "+N more ›".
4. **Other frequencies' tags.** Every other frequency with stations on it has a
   small tag in the strip at its own spot. A double's tag is one line with both
   names. Crowded tags still fold into "+N".
5. **Details.** Click the line, a tag or the flag's Roundtable line and the
   details open below the strip, over the waterfall, not the spectrum. The line
   stays in the strip, outlined, while its details are open.
6. Everything else keeps its meaning from the 2026-11-03 overlay design: the
   colors and shapes for talking, double, unknown, you and quiet; frozen grey
   when the link drops; dimmed while you are hidden; the right-click menus; the
   flag's Roundtable line.
7. **iPhone, iPad and web are unchanged.**
8. **Remote windows** (a window connected to a Core on another machine) show the
   same strip. It is drawn by the same code from the same Roundtable feed.

## Decisions settled here, for JJ's veto

| # | Call | Why |
| --- | --- | --- |
| V1 | The strip also shows while Roundtable is still connecting or reconnecting, empty or grey. | "Steady, no jumping": the waterfall should not move a second after you switch Roundtable on. |
| V2 | The line goes on the side of the dial away from the flag when it fits there, else on the side with more room. Readings drop off the end into "+N more ›" until it fits. | Same rule as the mockup. |
| V3 | A second slice on the same panadapter gets a tag; the slice you are working gets the line. A panadapter that does not hold the slice you are working shows tags only. | JJ's brainstorm, Claude's call there. |
| V4 | The waterfall cannot be turned off today (it is always at least a tenth of the panadapter), so "strip at the foot, details open upward" has one live case: when the waterfall is too short for the details (under 140 px of room), they open upward over the spectrum, standing on the frequency scale's top edge. | Keeps the details usable on a very short waterfall. |
| V5 | Details line up with the left edge of your line when your line is showing, else beside the dial, and never cover the time strip on the waterfall's right edge. | Same as the mockup. |
| V6 | The dial's vertical line still runs through the strip from spectrum to waterfall. | It ties the line to the dial, and the old tick from card to dial is gone. |
| V7 | Clicks in the strip never tune or grab a filter edge. Empty strip does nothing. | The strip is its own row. |
| V8 | A double's tag in the strip is one line: "Double AA5ABC K0ABC". | A 30 px strip has no room for three lines. |
| V9 | No new setting. The strip follows Roundtable on or off. | JJ: "While Roundtable is on". |

## Sizes and looks

- Strip: 30 px high, background `#0c0c0e`, a 1 px `#2c2c31` line on its top and
  bottom edges (workspace look roles where the code has them).
- Line: 24 px high, 3 px inside the strip top and bottom. A 3 px state-colored
  rail on its left edge, then who (12 px bold), the clock (10 px mono), then the
  readings 12 px apart. Each reading is the card row as today (call 11 px, value
  11 px mono, age 10 px mono, 3 px meter under the text with the peak tick), at
  least 78 px wide.
- Tags: 22 px high as today, centered in the strip.
- Details: 300 px wide as today, top edge 4 px below the strip.

## What goes away

The card standing on the spectrum, the tags on the spectrum's foot, the tick
from the card to the band plan, and the code that kept tags clear of flags (tags
no longer share space with flags). The phone's and web's own drawing code is not
touched.

## How it is built

`SpectrumWidget::spectrumLayout()` gains a `roundtableStrip` rect and takes its
height from the waterfall when the strip is shown. `MainWindow` tells each pan
whether the strip is shown from the Roundtable model's link state. The line is a
new pure placement function plus a painter in `RoundtableTagLayout`, reusing the
card model and the card row painter. The existing tag layer draws the strip's
contents on GPU builds; the CPU path draws them in `paintEvent`.
`placeRoundtableInspector` gains the hang-below placement.
