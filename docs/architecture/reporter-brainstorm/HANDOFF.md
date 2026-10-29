# NereusSDR Reporter — brainstorm handoff

**Status: brainstorming artifacts. NOT a specification. Nothing here is approved
for implementation, and no plan exists.** No code has been written. This document
exists so a more capable model can pick the design work up without re-deriving
the research or re-asking settled questions.

Read `UI-REFERENCE.md` in this directory before designing anything visual. The
UI reference is measured from the only real capture in the repository; designing
from `StyleConstants.h` alone produces something that does not look like the app.

---

## 1. The request, in the maintainer's words

> a feature to rv.nereussdr.com that is a lot like the freedv reporter but for
> all the other modes ie you can chat, with rich chat, see who is on frequency,
> what signal the current transmitter is putting on the s meters of each station.
> the idea is metadata visablity for operators

Refined over the session to: **who is hearing *you*, for every mode, with chat to
the stations getting you.**

---

## 2. What the reporter is not

Not a band activity viewer. That was built and rejected as the centrepiece.
Research into what operators actually use (PSK Reporter, WSPRNet, the Reverse
Beacon Network, Hamclock) found the same question asked every time:

> **"Who is hearing me?"** — not "who is on frequency?"

An SSB or CW operator who never touches a digimode opens these tools for exactly
one reason: to find out whether anyone is getting them. A band map is a DX
cluster with meters bolted on, and the DX cluster already exists. The design
puts **your own station first**, then who is hearing you, then propagation
across bands, then chat.

Every report's direction matters and is not interchangeable: each row is one
station telling you what **your** signal reads on **its** meter. Never the
reverse. That is how all four reference services work, and it is what makes a
loud report from 11 800 km the most meaningful thing on the page.

---

## 3. Verified repository facts

These were checked against the code, not assumed. Cite-check any that get reused.

| Fact | Where |
| --- | --- |
| The FreeDV reporter client is owned by `RadioModel`, a **client-side** model | `src/models/RadioModel.h:3668`, accessor at `:1002` |
| It is reached from `MainWindow` as a modeless singleton dialog | `src/gui/MainWindow.cpp:6216-6224`, menu action `actFreeDVReporter` |
| The existing Spot Hub is the same shape | `src/gui/MainWindow.cpp:6202-6213`, `actSpotHub` |
| The app's right column is a real layout element beside the panadapter | `src/gui/MainWindow.cpp:2582` (`Main QSplitter`) |
| Applets live in `AppletPanelWidget` with a gradient title bar and a `☰` show/hide button | `src/gui/applets/AppletPanelWidget.cpp:52-80`, `:269` |
| Applet panel minimum width 260 px | `src/gui/applets/AppletPanelWidget.cpp:49` |
| Worked-before state already exists (`cty.dat`, ADIF, DXCC colour provider) | `src/core/CtyDatParser.cpp`, `src/core/DxccWorkedStatus.cpp`, `src/core/AdifParser.cpp` |
| The theme constants | `src/gui/StyleConstants.h` |

### 3.1 The client reports; the Core does not

This was asked directly and the answer is forced by the architecture, not a
preference. The Core is an ADC/DAC with a network transport. It has no DSP, no
mode, no decoded callsign and no S-meter of its own. Every field of a presence
report is something only a client knows. The client therefore reports, and this
happens to be exactly the shape `FreeDVReporterClient` already has:

```
RadioModel (client, GUI thread)
  presence   this slice's frequency, mode, callsign, grid
  signal     the S-meter this slice reads right now
  transmit   "I am keying", so others' reports can be attributed
  chat       send and receive
       |
       v  ReporterClient  — same shape as FreeDVReporterClient
       v  reporter server
```

A new `src/core/` class, sibling to `FreeDVReporterClient`, constructed by
`RadioModel`, same `setIdentity(callsign, grid, …)` entry point, same lifetime.

**Consequence to carry:** a Core with no client connected is invisible to the
reporter, while the rendezvous still reports the station as online. That is
honest — nobody is listening — but the two views will disagree and someone will
ask.

---

## 4. Where the server-side sibling lives — important

The rendezvous at `rv.nereussdr.com` is **not on `main`**.

- `git ls-tree -r main` has **zero** files under `rendezvous/`.
- It exists on the iPhone integration branch, currently
  `claude/iphone-trunk-284aa6` (also `claude/iphone-app`), with 273 files under
  `rendezvous/`.
- That branch is **2589 commits ahead of `main`** and 4 behind, and is **not
  merged**.

Anyone designing the reporter's server must work from that branch, not `main`,
and must decide where the reporter source is meant to live before writing it.
This is an open question, not a settled fact.

### 4.1 The reporter must be a separate program

The rendezvous is built so it never learns a callsign. Its own written spec says
so: `docs/architecture/2026-09-23-rendezvous-v1.md` (on the branch above),
section 1 — it "never learns a label, a callsign, a pairing code's words or a
device's name… it matches opaque ids, forwards what it is given untouched, and
keeps everything in memory."

A reporter is the opposite of that rule. Growing the rendezvous into it would
silently invert a documented privacy decision. **So: a second program on the
same host, its own path through the same Caddy, with its own written privacy
rule.** This was flagged explicitly and accepted.

Its stack mirrors the existing one: Python service, `Caddyfile` in front, its
own systemd unit, its own `deploy.sh`. `rendezvous/README.md` on that branch is
the recipe to copy.

---

## 5. Decisions the maintainer has made

Settled. Do not re-ask, and do not reopen without new evidence.

| # | Decision | Reasoning given |
| --- | --- | --- |
| 1 | Public web page **and** an in-app view | "I would think both" |
| 2 | Real callsign, everything published, one hide-me switch | "call sign and everything is on, no opt in except where you show up at all or not" |
| 3 | **Raw S-meter, as read** | Chosen over SNR-relative and calibrated dBm |
| 4 | Station-linked chat, light markdown | messages carry sender + their reading of you |
| 5 | Desktop **and** iPhone both report and read | one client implementation each, one server |
| 6 | One station row per **radio**, keyed on station id | desktop + phone on one Core is one row, one signal reading, shared chat |
| 7 | Report **only while a key-down was observed** | see §6 — the load-bearing decision |
| 8 | Server keeps everything, including chat | archive is the feature |
| 9 | Live is public; archive requires station identity | "live public, archive is yours" |
| 10 | Reporting on by default, one hide switch | PSK Reporter / WSPR both work this way |
| 11 | Raw S-meters are never averaged or ranked across stations | each number sits beside its own station's callsign; a footnote says why |

### 5.1 Identity

Reports key on the **station id the rendezvous already issues** — signed and
proven — not a new account. So the reporter invents no identity scheme, the
operator registers no second password, and the private archive opens with a key
they already hold.

---

## 6. The hardest problem, and why decision 7 exists

This is the part most likely to sink the feature. State it plainly in any
design.

A listening station reads a strong signal on its meter. **It does not know whose
signal that is.** It knows only "there is something strong at this frequency."

If it reports `{freq, sMeter}`, the server credits it to whoever is transmitting
— and that inference breaks in three ordinary situations:

- **The transmitter is not keying.** Someone found the carrier, or it is a local
  station, or an FM carrier, or sideband splatter. The page would confidently
  say a station is S9 when it is not.
- **Two signals in one filter.** On SSB with a 2.8 kHz filter, the meter reads
  the sum of everything in the passband. That number is attributable to nobody.
- **Two stations key at once.** Someone has to lose, or several must be shown.

The S-meter measures *energy in a passband*, not *a station*. That is true of
every real radio, but on a shared server with no shared reference it stops being
a report and becomes a guess.

Decision 7 makes it honest: a report is published only if the server saw a
key-down on that frequency recently. "The signal appeared while A was
transmitting" is evidence; "the signal is loud" is not. **The cost, which is
accepted: no reports when nobody is transmitting, so the page goes quiet when the
band goes quiet.** That is correct behaviour, not a gap — and any design must not
paper over it.

Two mitigations were discussed and remain open: letting the reader **name** the
station they think it is (a human always knows who is talking to them), and
labelling the column as *attributed to* rather than *is*.

---

## 7. The private archive

Chosen because operators want to know how their signal behaved, and because
sessions are the unit that matters — not rows.

- A **session** is one continuous period on the air: band, frequency range,
  duration, and every station that reported you during it.
- **WATCHED** is how long a station kept reporting you, which is not how long
  you transmitted. A station that loses you in a fade drops off and returns.
  That distinction separates *they stopped hearing me* from *I stopped
  transmitting* — different fixes, one your radio and one the ionosphere.
- **Empty sessions are kept.** A period on the air that nobody received is
  recorded plainly. Archiving only successes builds a flattering picture of your
  own propagation.
- An hourly histogram of how many stations got you.

---

## 8. Sub-systems and build order

Five pieces. The order is a dependency order, not a preference.

1. **Reporter server** — separate program, own Caddy path, own numbered wire
   spec in the style of `2026-09-23-rendezvous-v1.md`, plus a conformance suite.
2. **Presence** — station id, frequency, mode, heartbeat, expiry. Same timer
   shapes as the rendezvous.
3. **Signal reports** — key-down gating, attribution, distance and bearing from
   grid squares, worked-before badges from existing `cty.dat`/ADIF data.
4. **Chat** — frequency-scoped, stored, private archive.
5. **Clients** — `ReporterClient` in Qt, then the Swift equivalent, against one
   server. The same work twice, in two languages.

---

## 9. Where the UI lives

**In the app: the right applet column**, under a gradient title bar with a drag
grip, exactly like the Meters and RX applets. Its toggle is a switch in the left
stack. Not a Tools-menu dialog — that pattern is for tools you consult
(`openSpotHub()`, `openFreeDVReporter()`), and this is a status you keep in view
while operating.

The frequency panel also carries an `RPT` tag and a "N hear you" count, because
that is the fact you want while transmitting, on the display you already watch.
Reporter stations appear as green ticks on the waterfall.

**On the web: you first**, then who is hearing you, then the band matrix, then
chat. Both themes, light and dark, where light is the same design with hues
inverted and tuned (bare-inverted cyan fails contrast on white).

---

## 10. Open questions

### 10.1 Chat channel model — the live question

The maintainer's own framing:

> the frequency chat would be a quick place to pass a QSO message to other
> operators on frequency

and separately, wanting to see other frequencies' chats, maybe a lobby, "kinda
like channels on Discord."

The tension worth designing around: **the metadata only exists because the room
is scoped to a frequency.** When VK3XYZ's message says "11 804 km, reads you
S9 +18", the server knows it because everyone is on one frequency. A generic
lobby has no such fact — it is people typing at each other, which is what the DX
cluster telnet already is and does not work.

Candidate shapes, none chosen:

- frequency QSO channels + per-band rooms + one lobby (three tiers)
- frequency QSO channels + one lobby only (two tiers)
- a Discord-style channel list you switch between, with unread counts

### 10.2 Also unresolved

- **Two simultaneous transmitters** — show both as transmitting, or one wins?
- **Does the reader name the station?** See §6.
- **Archive retention** — "keep everything" needs a number eventually, or it
  grows without bound.
- **Abuse and moderation** for stored, forever chat.
- **Stored markdown is a stored-XSS surface.** Untrusted text rendered later,
  indefinitely, on a public page, on the same domain as the rendezvous. Needs
  sanitising on write *and* on render.
- **Does the reporter also let third-party software report in?** Deferred — it
  means owning a public wire protocol forever. Currently out of scope.
- **iPhone specifics** — the reporter has never been looked at against the phone
  UI.

---

## 11. The mockups

Three static HTML files in `mockups/`. Open in a browser. See `UI-REFERENCE.md`
to run them.

| File | What it shows |
| --- | --- |
| `modern.html` | Web page, working light/dark toggle |
| `app.html` | In-app: Reporter applet in the right column, sampled palette |
| `archive.html` | Private archive: sessions and stored chat |

**What they get wrong, so nobody mistakes them for a specification:**

- Data is hardcoded HTML standing in for a live stream.
- The waterfall is CSS gradients, not rendered data.
- In `app.html` the spectrum spikes and the waterfall ticks were drawn
  independently and **disagree with each other** — real code derives both from
  one frequency. The same applies to any marker overlaid on two displays.
- The multimeter numbers are invented.
- No interaction, no empty states, no error states, no mobile layout.
- The first two app renders were drawn from `StyleConstants.h` instead of the
  screenshot and looked nothing like the app. Fixed. Treat them as a cautionary
  example, not a starting point.

---

## 12. Repository constraints the next model must honour

From `CLAUDE.md` and `CONTRIBUTING.md`:

- **Do not change without the maintainer:** visual design, UX behaviour,
  architecture (threads, signal routing, dependencies), feature scope,
  user-facing defaults, DSP parameters. This feature touches several of those,
  so each one is a maintainer decision. Flag them; do not assume.
- **Never hold a mutex in the audio callback**; cross-thread DSP parameters are
  `std::atomic`.
- **Rule R1: nothing under `src/core/` or `src/models/` may include a GUI
  header.** Enforced by `tst_core_has_no_gui_includes`. `ReporterClient` lives in
  `src/core/`, so it must expose an interface, as `ISpectrumSink` did.
- **Settings:** `AppSettings`, never `QSettings`. PascalCase keys, booleans as
  `"True"`/`"False"`. The hide-me switch is persisted, client-authoritative.
- C++ style: no `goto`, no raw `new`/`delete`, no `#define` constants, braces on
  all control flow, `Q_OS_*` platform guards, `qCWarning(lcCategory)` and no
  exceptions.
- **Porting rules** apply if anything is ported from Thetis, freedv-gui, AetherSDR,
  piHPSDR or deskHPSDR: byte-for-byte upstream headers, verbatim inline comments
  above developer tags, provenance rows. See `docs/attribution/HOW-TO-PORT.md`.
- GUI↔model sync: model setters emit, the connection sends, echo loops guarded
  with `m_updatingFromModel` or `QSignalBlocker`.
- No exceptions to raise as a design shortcut.

---

## 13. How this maintainer works

Learned over the session; it will save time.

- **Nothing gets built, scaffolded, specified or planned until he approves a
  design.** That holds for a one-line change.
- **The spec is written when he says "write the spec", and not before.** He has
  said this explicitly and repeatedly. "Brainstorm and plan X" names the
  destination, not the moment. Do not write a spec unprompted.
- **Design is settled by looking at rendered pixels, not by reading about it.**
  Build the thing, render it, show it, iterate. He judges as the operator who
  will use it and can tell in one glance whether something reads as gear.
- **Look before presenting.** Three unlooked-at captures in one session cost
  trust. If a render looks wrong, find out why rather than shipping it — two
  bugs in this session (a label collision, and a stray `.me` CSS rule giving a
  chat message `position:absolute; inset:0` and blanking the page) were
  invisible until rendered.
- One question per message, plain English, one option marked recommended with
  the reason in the same breath. No jargon in the option labels.
- When a section is agreed, build the next mockup. When it feels finished and he
  has not asked for the spec, **offer another screen** — do not offer to write it
  up as the natural next step.