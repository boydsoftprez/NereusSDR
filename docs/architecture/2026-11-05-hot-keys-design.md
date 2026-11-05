# Hot keys: keyboard, MIDI, knobs and deck plugins

Status: approved 2026-11-05 ("yes plan and build with crew and subagents"). Plan:
[2026-11-05-hot-keys-plan.md](2026-11-05-hot-keys-plan.md). Where the plan's "Corrections to the spec" table
differs from this document, the table is right, because it was checked against the code afterward.

Mockup: [2026-11-05-hot-keys-design/hot-keys-mockup.html](2026-11-05-hot-keys-design/hot-keys-mockup.html).
Open it in a browser. The Setup tab has the new pages; the Main window tab answers the keys.

Read at main `dfa9e1c29`, Thetis v2.10.3.15 (`3759d09`), AetherSDR `1e0718ad`, deskHPSDR `f3d857c`.

## 1. Goal

An operator can run NereusSDR from the keyboard, a MIDI controller, a tuning knob or a button deck.
Every one of them calls one list of actions. Keys work from the first launch.

This answers issue 433 (Ray WD4X: tune, next and previous filter, volume, from a mini keypad with an
encoder). It replaces PR 698, the draft arrow key change, which is not merged. Its one good idea
stays: the step follows the slice, and a move goes through the same path as mouse tuning.

## 2. Veto list

These are the calls I settled in this document without asking. Say the word and any of them changes.

| # | Call | Why |
|---|---|---|
| V1 | **M and N walk the band stack inside the band,** as the band stack design says. Moving to another band gets its own new keys: **J next band, H previous band.** | You approved M and N for band stack next and previous. Nothing else moved between bands, and H and J were free. |
| V2 | **A control reached with Tab also keeps Space,** not only the arrows. | A keyboard or VoiceOver user pressing Space on a button they tabbed to means press this button. A clicked button still leaves Space as hold to talk, as you ruled. |
| V3 | **On a Mac the keys say Cmd.** Qt maps Ctrl to Cmd, as every menu shortcut here already does. The plan checks Cmd+F1 to F8 against macOS and reports any clash to you. | Consistent with our menus. Not yet checked on macOS. |
| V4 | **The off switch is on the Keyboard page and in the View menu, and covers keyboard, MIDI, knobs and decks.** | You ruled one switch for all shortcuts. |
| V5 | **Held keys and knob bursts send at most one tune per 20 ms,** adding up the steps between. Toggles ignore key repeat. | AetherSDR's value, and short enough not to feel slow. |
| V6 | **Tuning from a key on a locked slice does nothing and says "Slice A is locked."** | The same state as the lock on the flag, said plainly. |
| V7 | **MIDI profiles and all bindings live in `AppSettings` on the window's computer,** with shortcut import and export as a file. AetherSDR keeps a separate MIDI file; we have one settings rule. | CLAUDE.md: AppSettings, never another store. |
| V8 | **New device threads:** one worker for MIDI and one for USB devices, each talking to the main thread only by queued signals. | Device reads block. Our rule for crossing threads holds. |
| V9 | **Devices come in three parts of one plan, in this order:** keyboard, MIDI, knobs and decks. One plan, one spec, landed in that order because the keyboard is what you can try. | You have no devices to test on. |
| V10 | **The plugins are ours,** written from a study of AetherSDR's. We ship them with the app, and Install hands one to its maker's app. | You ruled study, not copy. |
| V11 | **A new remote verb carries dot and dash,** because a remote window can only send a straight key today. See 7.4. | Without it, comma and period work only on a local radio. |

## 3. What you decided

| Decision | Ruling |
|---|---|
| Devices | Keyboard, MIDI and all eight knobs (RC-28, PowerMate, ShuttleXpress, ShuttlePro v2, FlexControl, TMate 2, Stream Deck+, Ulanzi Dial), with their displays and lights. |
| Which slice | The window's selected slice (flag or RX tab, and the bottom bar follows). Any binding can be pinned to a slice letter. Transmit actions go to the transmit slice. |
| On from the start | Everything is on from first launch, transmit included. One switch turns it all off. No transmit warnings, prompts or opt-in switches. |
| Transmit keys | Bound out of the box. Rebinding refuses plain letters, numbers and arrows for transmit. MOX, TUNE and two-tone need a combination. |
| When the keyboard can't key | While typing in a text field, with a dialog open, or from an inactive window. A held key drops when focus is lost. |
| Arrow keys | Outside a text field, arrows tune. A control reached with Tab keeps them until focus moves on. |
| Live CW | Comma, period and slash, only in CW mode. |
| Knobs on a remote Core | They plug into the computer with the window. No device libraries on the Core. |
| Deck plugins | A private link to the window on the same computer, using the same action list. Approved as a new connection type. |
| Phone | No hot keys on iPhone or iPad in this work. The action list is written so they can be added. |
| Setup | A new "Keys and controllers" group: Keyboard, MIDI, Knobs, Deck Plugins. MIDI moves out of CAT & Network. |
| Untested devices | You own none. Every device ships marked untested until someone tries it. Tests and simulators do not remove the mark. |

## 4. How the references do it

| Topic | Thetis | AetherSDR | deskHPSDR | Ours |
|---|---|---|---|---|
| Shape | Keyboard in `Console_KeyDown`, MIDI in Midi2Cat: separate | Keyboard and MIDI tables that call each other (`MainWindow_Controllers.cpp:2078`) | One table marking key, knob or wheel (`actions.h:252-265`) | One list, like deskHPSDR |
| Off switch | Off by default (`console.cs:13191`) | Off by default (`MainWindow_Menus.cpp:1070`) | Not studied | On by default |
| Space | Each press flips MOX (`console.cs:26723-26729`) | Hold to talk with release handling (`MainWindow_Shortcuts.cpp:207-263`) | Not studied | Hold to talk |
| Text fields | Space still keys from a text box (`console.cs:26710-26720`) | Blocks keying (`MainWindow_Shortcuts.cpp:78-92`) | Not studied | Blocks keying |
| Dialogs | Main window only | No check found; Space goes through an application filter | Not studied | Blocks keying |
| Focus lost | Stays keyed (`console.cs:46408`) | Holds drop, window and app (`MainWindow.cpp:3462-3469`, `MainWindow_Shortcuts.cpp:382`) | Not studied | Holds drop, window and app |

**What this is.** A design for NereusSDR, not a port. The action list, routing, slice rules, Setup
pages and plugin link are ours, because our slices, remote Core and listening rules have no equal in
either program. Default keys are a choice informed by both.

**What may be ported.** Device report layouts are hardware facts and carry a cite to the AetherSDR
file they came from (`HidDeviceParser`, `UlanziChordDecoder`, `MidiRelativeCcDecoder`,
`FlexControlManager`). Where the plan ports code and not only facts, that file keeps the upstream
header byte for byte and gets a row in `docs/attribution/AETHERSDR-PROVENANCE.md`, per
`docs/attribution/HOW-TO-PORT.md`. AetherSDR's tuning code is written around Flex's radio echo, so we
keep the ideas (in-flight target, step grid, dropping queued steps on a lock) and write it for our
slices.

## 5. Default keys

`Cmd` on a Mac, `Ctrl` elsewhere (V3).

| Group | Action | Key |
|---|---|---|
| Tuning | Tune up, down one step | Right, Left |
| | Tune up, down ten steps | Shift+Right, Shift+Left |
| | Tune step larger, smaller | ], [ |
| | Lock | L |
| | Go to frequency | G |
| | Next slice | Backtick |
| Filter | Filter next, previous | B, V |
| Audio | Volume up, down | Up, Down, and keypad +, - |
| | Mute | U, and keypad * |
| Band and mode | Band stack next, previous | M, N |
| | Next band, previous band | J, H (V1) |
| | Add band stack entry | Shift+M |
| | Open band stack panel | Shift+N |
| | Mode next, previous | X, Z |
| Display | Zoom in, out | =, - |
| DSP | Noise reduction, noise blanker, auto notch | R, E, A |
| | RIT, XIT on or off | I, O |
| Transmit | Hold to talk | Space |
| | MOX | Ctrl+Shift+M |
| | TUNE | Ctrl+T |
| | Two-tone | Ctrl+Shift+T |
| CW | Messages 1 to 8, stop | Ctrl+F1 to F8, Ctrl+F10 |
| | Dot, dash, straight key | Comma, period, slash |
| Voice | Messages 1 to 4, stop | Ctrl+Shift+F1 to F4, Ctrl+Shift+F10 |

There are 8 CW messages (`CwxSender.h:274`) and 4 voice slots (`DvkApplet.cpp:64`).

**Reserved for menus,** shown on the map and not rebindable here: Ctrl+comma, Ctrl+Q, Ctrl+K, Ctrl+L,
Ctrl+R, Ctrl+M, Ctrl+X, Ctrl+1 to 9, Ctrl+Shift+A, S, R, D, K, N and X (`MainWindow.cpp:12460-13970`).

**Knob and deck defaults.**

| Device | Control | Default |
|---|---|---|
| RC-28 | Wheel | Tune |
| | F1 press, hold | Step larger, lock |
| | F2 press, hold | Step smaller, mute |
| | TX bar | Hold to talk, with a stay-on setting |
| PowerMate | Wheel, press | Tune, mute |
| ShuttleXpress | Jog, ring | Tune, tune ten steps |
| | Buttons 1 to 5 | Mute, lock, filter next, filter previous, step larger |
| ShuttlePro v2 | Jog, ring | Tune, tune ten steps |
| | Buttons 1 to 14 | Mute, lock, filter next and previous, step larger and smaller, band stack next and previous, mode next and previous, zoom in and out, volume up and down |
| FlexControl | Wheel | Tune |
| | Button 1 and knob: tap, double tap | Step larger, step smaller |
| | Button 2: tap, double tap | MOX, TUNE |
| | Button 3: tap, double tap | Mute, lock |
| TMate 2 | Main, E1, E2 | Tune, volume, filter |
| | Keys 1 to 6 | Mute, lock, band stack next and previous, mode next and previous |
| Stream Deck+ | Dials 1 to 4 | Tune, volume, filter, band stack |
| | Keys 1 to 8 | Hold to talk, MOX, TUNE, mute, band stack next and previous, filter next and previous |
| Ulanzi Dial | Dial, press | Tune, mute |
| | Top left, middle, right | MOX, RIT, TUNE |
| | Right top | Next slice |

A MIDI controller starts empty, because its controls differ per model. That one is learned, not bound.

## 6. Where keys work

**Windows.** Shortcuts act when the active window is the main window, a floating panadapter
(`PanFloatingWindow`) or a floating container (`FloatingContainer`). Setup, every dialog, the
Roundtable window and the amplifier chart window keep their own keys. No modal dialog may be open.
Knobs, MIDI and decks are not tied to focus and keep working with another app in front.

**Who wins inside those windows.**

| Focused thing | Keeps |
|---|---|
| Text box, multi-line text, number box, editable dropdown | Every key |
| Open dropdown list or menu | Every key, until it closes |
| Container arrange mode (`FreeCanvasSurface`) | Arrows, while arranging |
| Anything reached with Tab or Shift+Tab | Arrows and Space (V2), until focus moves on |
| A clicked slider, button, tab or closed dropdown | Nothing: arrows tune and Space talks. Dragging and the wheel still work. |

Each row of PR 698's faults has an answer here. The filter never takes keys from Setup trees, tables,
tabs or sliders because those windows are excluded. It has an active window and modal test. It uses
one slice rule for both pan and slice. It does not grab focus on a spectrum click, so the CWX box and
Roundtable message box keep theirs.

## 7. Design

### 7.1 Parts

| Part | Where | Job |
|---|---|---|
| `HotKeyAction`, `HotKeyActionList` | `src/core/hotkeys/` | The one list: id, name, group, gesture type (press, hold, turn), transmits flag, what it acts on, default binding as text. No GUI headers (R1). The phone can use this list later. |
| `HotKeyBindings` | `src/core/hotkeys/` | The user's bindings and pins, the rebinding rules (8), defaults, import and export, `AppSettings`. |
| `HotKeyDispatcher` | `src/gui/hotkeys/` | The only code that runs an action. `press(id, source)`, `release(id, source)`, `turn(idUp, idDown, steps, source)`. Finds the slice, applies refusals, coalesces, calls the same model and window entry points as the on-screen control. |
| `KeyboardRouter` | `src/gui/hotkeys/` | An application event filter on the main thread, needed because hold actions must see release (a `QShortcut` has no release signal). Applies section 6, then calls the dispatcher. |
| `MidiInput` | desktop only | RtMidi (MIT, vendored in `third_party/`), learn mode, relative knob decoding, profiles. |
| `DeviceInput` and one parser per device | desktop only | hidapi and Qt SerialPort (FlexControl). Displays and lights. |
| `DeckLink` | `src/gui/hotkeys/` | The plugin connection (7.5). |
| Setup pages | `src/gui/setup/` | Four `SetupScope::ThisComputer` pages in the new group, replacing `KeyboardShortcutsPage` and `CatMidiControlPage`. |

RtMidi and hidapi link only into the desktop app, never `nereusd`. The `Keyboard` and `Midi` entries in
`UnbuiltFeatureList.h` go away, and the View menu item appears.

### 7.2 Which slice

In order, for any action that acts on a slice:

1. A pinned binding uses its letter. A closed letter does nothing and shows "Slice B isn't open."
2. Otherwise, `MainWindow::windowRxSlice(true)`, the window's selected slice. It works for local and
   remote windows and may be a listened slice.
3. Transmit actions use the transmit slice, as the MOX button does. They do not move it.

On a listened slice, tune, filter, mode, band and DSP actions do nothing and show the reason from
`MainWindow::sliceChangeRefusal`, unchanged. They never move to another slice. Volume and mute work
and change only what this device hears, per the slice control design
(`2026-09-28-slice-control-and-listening-design.md`, U5). Zoom acts on the panadapter with focus.

Tune goes through the flag's existing path (`VfoWidget::tuneFromDrag`, which the mouse also uses) and
the slice's own step (`SliceModel`). Toasts use `StatusToast`.

### 7.3 Transmit

Hold to talk, MOX, TUNE and two-tone call what the window's own buttons call: locally
`RadioModel::setMoxFromButton` and `RadioModel::setTune`; on a remote window `RemoteTransmitClient`.
So every existing check applies, and no source bypasses the keying gate or the time-out.

**Hold.** Each source holds its own key, using `MoxController`'s `KeyerIdentity` so that a release
unkeys only what that source keyed: letting go of Space never drops a MOX someone latched. Add
`PttSource` entries only if the plan finds the existing ones report it wrongly. `onSpacePtt` is
connected for the local path with its `PttMode::Space`.

**A hold drops when:** the key or button is released, even with the modifier lifted first; the active
window changes; the app stops being active (both are checked, as macOS can miss the window event); the
device is unplugged; or the plugin link drops. The remote heartbeat already covers a lost connection.
Key repeat never re-presses. MOX and TUNE stay on after you let go and don't drop on focus loss, like
the buttons.

A device button keys regardless of what has focus. Connecting never keys, per CLAUDE.md.

### 7.4 CW

**Messages** use the CWX sender, or `sendCwMemory` and `stopCw` on a remote window. **Keys** act only
in CW mode, and do nothing in other modes.

- **Straight key, slash:** locally `CwxSender::setHandKey`, the serial key's own sink
  (`RadioModel.cpp:33983`); remotely `RemoteTransmitClient::setCwKey`.
- **Dot and dash, comma and period:** locally `CwKeyLine::setDot` and `setDash`, the bits the serial
  paddles drive, so the radio's keyer does the timing, with the same gate the serial reader asks.
- **Remote (V11):** a new `cw.paddle {dot, dash}` verb, sent like `cw.key`, on `remoteCwVersion` 3. A
  Core below that shows the keys disabled with the plain reason. It releases on the same loss
  conditions as `cw.key`, and the verb is documented for the phone.

Keyboard and network timing are not measured. This is for casual sending, not contest speed.

### 7.5 Deck link

`DeckLink` listens on a `QLocalServer` socket, one per settings profile, open only to this user
(`QLocalServer::UserAccessOption`). It has no network listener. Messages are JSON lines:

| Direction | Message | Meaning |
|---|---|---|
| Plugin | `hello {plugin, version}` | Answered with the protocol version and the action list |
| Plugin | `press {id, pin?}`, `release {id, pin?}`, `turn {idUp, idDown, steps, pin?}` | Same calls as every source |
| Window | `state {id, on}` | Lights a button: MOX, TUNE, mute, lock and so on |
| Window | `display {frequency, mode, filter, slice}` | For keys and strips that show text |

When a plugin disconnects, every hold it had is released. "Let deck plugins connect" turns the link off.
Plugins for Elgato's app, StreamController and Ulanzi Studio ship under `plugins/`, each with its own
license note, and call no TCI. A remote Core changes nothing here, since the link stops at the window.

### 7.6 Bursts and repeats

- **Tune:** steps add up and go out at most every 20 ms (V5), snapped to the step grid. Queued steps
  are dropped if the slice locks or stops being changeable.
- **Volume and other held levels:** key repeat, limited the same way.
- **Toggles and one-shots:** repeat ignored.
- **Keypads that send repeated keystrokes** count as held keys, so they get the same limit.
- **Ulanzi Dial:** it acts as a keyboard, so the Linux, Windows and macOS backends decode its chords.
  On Windows its keystrokes also reach the focused window, because the OS gives no exclusive claim
  (per AetherSDR's `UlanziDialWindowsManager.h`). The Knobs page says so. On Linux it needs an access
  rule, which the Knobs page also states.

### 7.7 Remote

Keyboard, knobs, MIDI and decks all belong to the window's computer. Bindings stay on that computer.
Each action reaches the Core by the route its on-screen control uses, so remote and local behave the
same. Any action a Core version can't do is disabled with a plain reason, and the plan names each with
the task that closes it, per the remote parity rule. The one known gap is dot and dash, closed by V11.

## 8. Rebinding rules

| Case | Result |
|---|---|
| Transmit, CW or voice action on a plain letter, number or arrow | Refused, with the reason |
| MOX, TUNE or two-tone on Space | Refused: they stay on after you let go |
| A key a menu reserves | Refused, naming the menu item |
| A key another action has | Offer Replace or Cancel |
| Escape while setting a key | Cancels |

Comma, period and slash are default CW keys and are not in the refused group. No other warnings or
prompts exist around transmit.

## 9. Setup pages

As in the mockup, in the Setup look. Controls a window can't use are disabled with a reason, never
hidden.

- **Keyboard:** Use shortcuts; a keyboard map with Plain, Shift, Ctrl and Ctrl+Shift layers, colored
  by group; a searchable list with Key, Acts on (selected slice or a letter), Set, Clear and Default;
  Reset all, Import and Export.
- **MIDI:** device, profile, learn (pick an action, then move a control), and a control list.
- **Knobs:** the eight devices with connection state and "untested"; a drawing of the device; a row per
  control and gesture; speed, direction, and each device's display or light settings.
- **Deck Plugins:** the connection switch and the three plugins with Install and "untested".

## 10. Errors

| Failure | What happens |
|---|---|
| Device unplugged | Holds drop, the page shows Not plugged in, and it reconnects when it comes back |
| Device can't be opened (permissions, another app has it) | The page says what and how to fix it. Nothing else stops. |
| Settings hold a binding for an action that no longer exists | Dropped with a `qCWarning` |
| Import file with a refused or unknown line | Skipped, then listed afterward. Good lines still apply. |
| Build without hidapi, RtMidi or SerialPort | Those devices stay listed and disabled, with the reason |
| Socket in use | Deck page says so. Keys and knobs are unaffected. |

## 11. Acceptance

Evidence: **U** unit and offscreen tests, **I** integration on this Mac, **H** JJ's hardware.

| ID | Behavior | Evidence |
|---|---|---|
| R-HK-01 | Every default in section 5 runs its action from first launch, with no setup | U, H |
| R-HK-02 | One switch stops keyboard, MIDI, knobs and decks | U |
| R-HK-03 | Each row of section 6 and each excluded window, including no retune from a Setup tree, table, tab or slider | U |
| R-HK-04 | Clicked slider: arrows tune. Tabbed slider: arrows adjust it until focus moves on | U, H |
| R-HK-05 | CWX and Roundtable boxes keep focus and keys after a spectrum click | U, H |
| R-HK-06 | Slice choice per 7.2, local and remote, including pinned, closed, listened and locked | U |
| R-HK-07 | Local and remote windows pick the same slice for pan and action (the PR 698 fault) | U |
| R-HK-08 | Hold drops on each case in 7.3, and a release leaves another keyer's key on | U, H |
| R-HK-09 | No key press in a text field, in a dialog or from an inactive window keys the radio | U, H |
| R-HK-10 | Connecting never keys | U |
| R-HK-11 | Time-out and keying gate apply to every source | U |
| R-HK-12 | Rebinding rules in section 8 | U |
| R-HK-13 | Comma, period and slash key only in CW mode, local and remote | U, H |
| R-HK-14 | One held key or burst sends at most one tune per 20 ms, with no steps lost | U |
| R-HK-15 | Each MIDI and device parser turns known input into the right action; relative and absolute knobs both decode | U (simulated). Device stays marked untested. |
| R-HK-16 | Deck link: action list, press and release, state, drop on disconnect, no network port, other users refused | U, I |
| R-HK-17 | Setup pages match the mockup, in the Setup look | I, H |
| R-HK-18 | Nothing in `src/core` includes a GUI header; `nereusd` links no device library | U |
| R-HK-19 | Every device and plugin shows "untested" until a real-device report changes it | U |

**What the evidence can't show.** Nobody here has any of the devices, so MIDI, knobs, displays, lights
and plugins are checked only against simulated input built from documented layouts. Release notes say
so. The keyboard runs on JJ's hardware before anyone calls this ready.

Tests run through ctest, offscreen with a scratch home.

## 12. Out of scope

- Hot keys on the iPhone and iPad.
- Knobs plugged into a Core with no window.
- A TCI path for plugins.
- Thetis's Q/A through U/J tune pairs as defaults. Any key can be bound to a tune action.

## 13. Depends on, and what follows

- **Band stack.** `bandStackNext`, `bandStackPrevious`, `bandStackAdd` and `bandStackPanel` on
  `MainWindow` (`2026-11-05-band-stack-design.md`, on its own branch). Until that lands, those actions
  show disabled with a plain reason.
- **On JJ's word, not before:** closing PR 698 as replaced, and any reply on issue 433.
