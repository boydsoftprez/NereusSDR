# NereusSDR UI reference — measured, not guessed

Sampled from `docs/images/nereussdr-v016-screenshot.jpg` (1280×831), the only real
capture of the shipped app in this repository. **Treat that file as the ground
truth for anything visual.** Crop regions 3–4× before judging detail; at 1× the
app reads as generic dark theme.

## Layout facts the screenshot settles

These are not in `src/gui/StyleConstants.h`, which is why guessing from that file
produces a wrong-looking result.

- **There is no menu bar.** No `Radio · View · Tools · Setup`. Left side is a
  vertical stack of bordered switch buttons (about 46 px wide), each a *full*
  button with amber `NYI` badges overlapping their top-left corner.
- **There is no OS window chrome in the capture.** The app fills its frame. Do
  not draw window dots or a title bar.
- **Spectrum and waterfall are two separate stacked panes**, not one
  continuous display. A bright cyan rule separates them.
- **The frequency display is a floating panel inside the spectrum pane**, not a
  menu-driven applet. Its anatomy top to bottom: a 2 px cyan rule, a tag row
  (`ANT1` blue / `ANT1` red / `2.9K` cyan, then `TX` grey box and `A` cyan box
  right-aligned), the frequency inside its own **bordered rounded box** at ~30 px
  cyan digits, an S-meter scale row (`S1 3 5 7 9 +20 +40`), a filled signal bar
  with the reading at its right, then a row of small mode buttons.
- **The multimeter is an overlay on the waterfall**, not an applet.
- **The right panel** is a single column of applets, each headed by a ~15 px
  gradient bar carrying a **drag grip (dots), a bold light title**, and
  sometimes a count badge. Bodies are near-black; controls inside are chunky
  bordered buttons, not flat cards.
- **The status bar** carries a row of small pill buttons on the left, the station
  name centred in cyan, and telemetry right.

## Measured colours

| Element | Hex | Note |
| --- | --- | --- |
| App / spectrum background | `#0a0a14` | dominant across the whole frame |
| Applet body | `#0f0f1b` | |
| Applet header bar | `#354252` | ≈ gradient `#3a4a5a`→`#1a2a38` in `StyleConstants.h` |
| Button face | `#141d2c`, `#111520` | darker variants for disabled |
| Button border | `#2a3a4a` | |
| Accent / frequency digits | `#00b4d8` | |
| Bright cyan rule | `#3f9bb0` | the spectrum/waterfall separator |
| Green (in-band, keying) | `#00ff88` | |
| Amber `NYI` badge | `#d8b800` | |
| Red (ALU / TX) | `#ff4444` | |

`StyleConstants.h` is a reasonable source for accent and text greys, but its
`kButtonBg` (`#1a2a3a`) is **lighter and less blue** than what actually ships.

## Type

- Labels and values: **bold**, 9–11 px, sans-serif. Heavier than a typical
  web UI. The app does not use light-weight type anywhere visible.
- Callsigns, frequencies and numeric readouts: monospace.
- Letter-spaced small-caps for section labels, roughly `0.1em`.

## Rendering the mockups

```
docs/architecture/reporter-brainstorm/mockups/app.html       # in-app, sampled palette
docs/architecture/reporter-brainstorm/mockups/modern.html    # web, light + dark
docs/architecture/reporter-brainstorm/mockups/archive.html   # private archive view
```

Open with `open -a Safari <path>`. All three are static: hardcoded HTML standing
in for a live data stream. `modern.html` has a working light/dark toggle, which is
the part that carries forward — one set of CSS rules with a `data-theme`
attribute swapping custom properties.