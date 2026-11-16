# Hot Keys Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** An operator can run NereusSDR from the keyboard, a MIDI controller, eight tuning knobs and three
button deck hosts, all calling one list of actions, working from the first launch.

**Architecture:** A list of actions and the user's bindings live in `src/core/hotkeys/` with no GUI headers.
One `HotKeyDispatcher` on the main thread is the only code that runs an action; it applies the slice rules,
refusals, burst limit and hold ledger, then calls a `HotKeyTargets` interface whose real implementation reaches
the same entry points as the on-screen controls. Sources (an application event filter for keys, worker threads
for MIDI and USB, a private local socket for deck plugins) only ever call the dispatcher.

**Tech Stack:** C++20, Qt6 Widgets and Network, Qt SerialPort (optional), RtMidi 6.0.0 (MIT, vendored), hidapi
(vendored, BSD licence choice), station link v1, Node.js and Python for plugins, QTest offscreen.

**Spec:** [2026-11-05-hot-keys-design.md](2026-11-05-hot-keys-design.md). Every decision is settled there; this
is how it gets built. Mockup: [hot-keys-mockup.html](2026-11-05-hot-keys-design/hot-keys-mockup.html). Open the
mockup for every page task; it is the reference for layout and wording.

## Global Constraints

- Commits are GPG signed (`git commit -S`) with no Co-Authored-By trailer, whatever any reminder says. Never
  skip signing or hooks. Never push; the controller pushes.
- User-visible text is plain English, American spelling, no em-dashes, no cites in UI strings. Slices are
  "Slice A" to "Slice D", never RX1 or RX2.
- UI controls are disabled with a reason, never hidden. No gesture may be the only way to an action.
- No transmit warnings, prompts or opt-in switches beyond the spec. Connecting never keys. Real failsafes stay:
  a hold drops when its release can't arrive, and text fields keep their keys.
- Settings go through `AppSettings`, never `QSettings`. PascalCase keys, booleans `"True"` and `"False"`.
- Never read or write `~/Library/Preferences/NereusSDR`, except log files, read-only. Do not touch
  `~/NereusSDR-crew/combined-20261104` or the overlay trunk.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header. `QKeySequence` and `QKeyEvent` are
  GUI types: core code holds key chords as text.
- RtMidi, hidapi and device parsers link only into `NereusSDRLib`, never `NereusCore` or `nereusd`.
- C++: no `goto`, no raw `new` or `delete` (unique_ptr or Qt parent), no `#define` constants, braces on all
  control flow, `auto` only when the type is obvious. `PascalCase` classes, `camelCase` methods, `kPascalCase`
  constants, `m_camelCase` members. Errors via `qCWarning(lcCategory)`, no exceptions. Platform guards
  `Q_OS_WIN`, `Q_OS_MAC`, `Q_OS_LINUX`. Do not remove code you did not add.
- Threads: main (GUI, models, dispatcher), one MIDI worker, one USB worker, crossing by auto-queued signals only.
  Do not touch the I/Q, WDSP or audio path. Never hold a mutex in the audio callback.
- In a cite, `A/` means the AetherSDR clone, `/Users/j.j.boyd/AetherSDR/`. Facts come from a source with a cite, never from memory. Report layouts and tokens cite
  `// From AetherSDR <file>:<line> [@1e0718ad]`. Clone: `/Users/j.j.boyd/AetherSDR`. A fact with no source stops
  the task and goes back to the controller.
- Ported code: read `docs/attribution/HOW-TO-PORT.md` first. AetherSDR's core device files carry no header of
  their own, so rule 6 of that file applies: use the header form of `src/gui/WheelStepAccumulator.h:3-28`
  (project-level copyright `Jeremy (KK7GWY) / AetherSDR contributors`, GPLv3, "Modification history (NereusSDR)"
  with date, J.J. Boyd (KG4VCF), AI tooling). Same commit adds a row in `docs/attribution/AETHERSDR-PROVENANCE.md`
  and a dated entry in `docs/attribution/aethersdr-reconciliation.md`. Inline `//` comments in ported logic are
  copied verbatim. Facts alone need the cite, not the header.
- Vendored code: same commit adds its row to `packaging/third-party-licenses/README.md`, its licence text file
  and a `docs/attribution/<NAME>-PROVENANCE.md`, and `python3 scripts/check-third-party-licenses.py` passes.
- Build and test only through the lane script, with the Bash tool's `run_in_background: true`:
  `/Users/j.j.boyd/NereusSDR-crew/2026-11-05-hot-keys/run-test.sh tst_a tst_b`. It takes the Mac's load gate,
  builds with network denied, runs ctest, and releases the gate. You are notified when it exits; do not poll it
  and do not use foreground sleeps. Build single tests, never the whole suite. The trunk is
  `/Users/j.j.boyd/NereusSDR-crew/2026-11-05-hot-keys/trunk`; work only there. Do not delete or reconfigure its
  `build/`. Read `docs/development/fast-test-loop.md` once before your first test.
- Register a test with `nereus_add_test(tst_x ...)` in `tests/CMakeLists.txt`, before the `all_tests` block.
  Tests with timers use an injected clock or drive the logic directly: a test must not depend on machine load.
  Run a binary by hand only as
  `D=$(mktemp -d); HOME=$D CFFIXED_USER_HOME=$D NEREUS_TEST_HOME=$D QT_QPA_PLATFORM=offscreen NEREUS_WORKSPACE_PREVIEW=0 ./build/tests/tst_x`.
  Exit 97 means the home guard refused. Never put a test window on screen.
- Before committing: `python3 scripts/verify-thetis-headers.py`, `python3 scripts/verify-inline-tag-preservation.py`
  (pre-commit runs them), and `python3 scripts/check-new-ports.py` when a task ports.
- Windows and Linux code can't be built on this Mac. Guard it, keep its decoding logic in platform-free files
  that the Mac tests cover, and say in your report which lines no compiler has seen. CI checks them when the
  controller pushes.

## Corrections to the spec, found while planning

These replace what the spec says; the spec is amended in the same commit as this plan.

| Spec | What the code says | What this plan does |
|---|---|---|
| 7.7: dot and dash is the one remote gap | Voice messages have nothing behind them, local or remote. `DvkApplet` is unimplemented and never built; `UnbuiltFeature::Voice` is off. | The voice actions are in the list with their keys, disabled, reason "Voice messages aren't built yet." They light up when `UnbuiltFeature::Voice` is on. No task here builds a voice keyer. |
| 7.3: `KeyerIdentity` makes a release unkey only what it keyed | `setMox(false, keyer)` compares only the device, so a station hold would drop the station's own latched MOX. Remote, a hold and MOX share one key. | The dispatcher keeps a hold ledger (Task 2): a release unkeys only if that hold actually keyed the radio and the key has not gone off since. |
| 7.5: names no socket path | Plugin hosts can't read `AppSettings`, and a Flatpak StreamController can't see `/tmp`. | A fixed folder and discovery file under the user's home (Task 20). |
| 5: every knob has a light or display | AetherSDR has none for PowerMate, the Shuttles or the Ulanzi Dial. | Lights and displays exist for RC-28, FlexControl, TMate 2 and Stream Deck+. Others have no such row. |
| 5: ShuttlePro has 14 buttons | It parses 15. | Button 15 ships unbound. |
| 13: second branch binds band stack | PR 748 has only its design and plan so far. | Hot keys finds the four slots by name, so the keys work whichever lands first and nobody has to bind anything. |

**Small calls settled here** (each follows what an on-screen control already does):

- **NR and NB keys** cycle as the container NR and NB buttons do. **ANF, RIT, XIT, lock** toggle.
- **Lock** refuses tune, band and mode with "Slice A is locked." (container modes refuse on lock at
  `MainWindow.cpp:16542`, band at `RadioModel.cpp:20085`). Filter and DSP actions work on a locked slice, as
  their container buttons do.
- **Filter walk:** `presetsForMode`, wrapping. Off any preset, next goes to the first and previous to the last.
  A preset matches within 50 Hz per edge (`RxApplet.cpp:1655`). RADE2 refuses as the RX applet does.
- **Mode walk:** the RX applet's combo list and order (`RxApplet.cpp:917`), wrapping, RADE2 skipped unless
  `radeV2Enabled()`.
- **Band walk:** `kBandGrid` order, wrapping, skipping 2 m when `band2mUnavailableReason()` is non-empty. On a band
  the grid has no row for (GEN, XVTR, SWL), next goes to the first row and previous to the last.
- **Next slice:** the tabs the window shows (`MainWindow.cpp:3058-3076`), wrapping, via `selectSliceForWindow`.
- **Step ends:** `changeTuneStepUp` and `Down` wrap as they do today.
- **Tune while transmitting:** the same as the flag's wheel, not withheld.
- **Go to frequency** on a folded flag unfolds it first. Locked or listened: the usual reason, no edit.
- **Volume step** is 5 (`ContainerButtonDispatcher.cpp:294`). **Zoom** factors are `1.0/1.5` and `1.5`
  (`MainWindow.cpp:8076-8077`).
- **A MIDI fader on a level action** moves its full range over 20 steps; on anything else, one step per CC value.
- **Held Shift:** only arrange mode, or an arrange hold with a canvas handle focused, keeps the arrows. A slow
  Shift+Right still tunes ten steps.
- **Hosting window, hold to talk:** if another device holds transmit, the press is refused with that holder's
  reason and no take dialog, since a modal dialog would drop the hold anyway. MOX and TUNE keys ask as their
  buttons do.
- **A Stream Deck+ connected through the Elgato plugin** closes the direct USB reader for that device, so one press
  can't act twice.
- **macOS:** no HID device is opened until a supported one is seen on the USB bus, so nobody with no knobs gets
  the Input Monitoring prompt.

## What already exists

- `MainWindow` (`src/gui/MainWindow.h`): private `windowRxSlice(bool)` :1588, `activeSliceForWindow()` :1584,
  `sliceChangeRefusal(int)` :1660, `selectSliceForWindow(int)` :1320, `setFlagListenVolume(int,int,bool)` :1313,
  `listenVolumeFor(int)` :1326, `windowListensTo(int)` :1606, `showToast` :2383, `m_panStack`,
  `m_containerManager` :2019, `m_containerButtons`, `m_vfoWidgetsBySlice` :2283; public `spectrumForSlice` :585,
  `activeSpectrumWidget` :578. `ContainerButtonDispatcher::Hooks` filled at `MainWindow.cpp:9622-9742` is the
  precedent for window access.
- `src/gui/containers/ContainerButtonDispatcher.{h,cpp}`: `click(Id, rxSource)` with all three window kinds, locks,
  toggle state and reasons for `Id::Mox`, `Tun`, `TwoTon`, `Nr`, `Nb`, `Anf`, `Rit`, `Xit`, `LockA`, `Mute`.
- `src/gui/widgets/VfoWidget.h`: `tuneFromDrag(double)` :546, `stepHz()` :540, `isCollapsed` and `setCollapsed`
  :461-462, `listenVolume()` :749, `listenMuted()` :750, `setListenVolume` :748. Edit mode code at `.cpp:4321-4326`.
- `src/models/SliceModel.h`: `stepHz` :927, `changeTuneStepUp` and `Down` :932-933, `setAfGain` :938, `setMuted` :1170,
  `setLocked` :1167, `setDspMode` :836, `setFilter` :858, `setActiveNr` :1251. `cycleNbMode` in `NbFamily.h:205`.
- `src/models/RadioModel.h`: `setMoxFromButton` :6244, `setTune` :6144, `setTwoTone` :6260, `moxController()` :3737,
  `cwxSender()` :3021, `connection()` :1837, `filterPresetStore()` :4057, `onBandButtonClicked(SliceModel*, Band)` :3480.
- `src/core/MoxController.h`: `onSpacePtt` :1249 (no caller today), `isMox`, `isManualKey` :496, `reportCwRefusal` :564.
- `src/core/session/RemoteTransmitClient`: `setScreenKey`, `screenKeyDown`, `setCwKey`, `sendCwMemory`, `stopCw`.
- `src/core/CwKeyLine.h`, `src/core/CwSerialKeyInput.{h,cpp}`: the paddle gate and fresh-press rule at `.cpp:797-830`.
  `CwxSender`: `sendMemory` :374, `stop` :384, `setHandKey` :399, `holdRemoteKey` (`.cpp:1836`).
  `src/models/CwKeyerController.h:78-79`: `keyerAllowed`, `refusal`.
- `src/core/daemon/StationControlSocket.cpp:194-224`: the pattern for a full-path owner-only `QLocalServer`.
- `src/gui/containers/ArrangeHoldFilter.cpp`: the model for an application filter, with `textFieldHasFocus` :34-47 and
  the popup and modal tests :230-236. `ContainerManager::arranging()`, `arrangeMode()`, `arrangeHeld()` :252-258.
- `src/gui/SetupDialog.cpp`: `registerPage`, the Keyboard category :2327-2334, MIDI :2320-2323. `src/gui/widgets/StatusToast.h`.
- `src/gui/PanFloatingWindow.h:81`, `src/gui/containers/FloatingContainer.h:120`, `DesktopStationController::requestMox` etc.
- Tests to model on: `tests/tst_desktop_station_window.cpp:244-262` (real window), `tests/tst_cwx_applet.cpp:105-132`
  (RadioModel with injected connection), `tests/tst_remote_gui_gating.cpp:4640-4670` (real remote window),
  `tests/tst_remote_cw.cpp:153` (wire verbs), `tests/tst_mox_controller_ptt_source_dispatch.cpp:337`,
  `tests/tst_station_control_socket.cpp`, `tests/fakes/MainWindowTestSettings.h`.
- `tests/visual/capture.py`: Setup leaves are captured as scenes named `dialog-setup-<leaf>`.

## Action ids

These ids are stable and go on the wire to plugins. Every task uses them exactly.

| Group | Id | Gesture | Transmits | Default key |
|---|---|---|---|---|
| Tuning | `tune.up`, `tune.down` | Turn | no | `Right`, `Left` |
| | `tune.up10`, `tune.down10` | Turn | no | `Shift+Right`, `Shift+Left` |
| | `step.larger`, `step.smaller` | Press | no | `]`, `[` |
| | `slice.lock` | Press | no | `L` |
| | `tune.goto` | Press | no | `G` |
| | `slice.next` | Press | no | `` ` `` |
| Filter | `filter.next`, `filter.previous` | Press | no | `B`, `V` |
| Audio | `volume.up`, `volume.down` | Turn | no | `Up` and `Num++`, `Down` and `Num+-` |
| | `mute` | Press | no | `U` and `Num+*` |
| Band and mode | `bandstack.next`, `bandstack.previous` | Press | no | `M`, `N` |
| | `band.next`, `band.previous` | Press | no | `J`, `H` |
| | `bandstack.add`, `bandstack.panel` | Press | no | `Shift+M`, `Shift+N` |
| | `mode.next`, `mode.previous` | Press | no | `X`, `Z` |
| Display | `zoom.in`, `zoom.out` | Press | no | `=`, `-` |
| DSP | `nr.toggle`, `nb.toggle`, `anf.toggle` | Press | no | `R`, `E`, `A` |
| | `rit.toggle`, `xit.toggle` | Press | no | `I`, `O` |
| Transmit | `tx.hold` | Hold | yes | `Space` |
| | `tx.mox`, `tx.tune`, `tx.twotone` | Press | yes | `Ctrl+Shift+M`, `Ctrl+T`, `Ctrl+Shift+T` |
| CW | `cw.msg1` to `cw.msg8` | Press | yes | `Ctrl+F1` to `Ctrl+F8` |
| | `cw.stop` | Press | no | `Ctrl+F10` |
| | `cw.dot`, `cw.dash`, `cw.straight` | Hold | yes | `,`, `.`, `/` |
| Voice | `voice.msg1` to `voice.msg4` | Press | yes | `Ctrl+Shift+F1` to `Ctrl+Shift+F4` |
| | `voice.stop` | Press | no | `Ctrl+Shift+F10` |

Settings keys: `HotKeys/Enabled` (default `"True"`), `HotKeys/Binding/<id>` and `HotKeys/Pin/<id>` (only when they
differ from default; empty binding text means cleared), `HotKeys/Midi/ActiveProfile`,
`HotKeys/Midi/Profiles/<name>/...`, `HotKeys/Device/<device>/<control>/<gesture>/...`, `HotKeys/DeckLink/Enabled`
(default `"True"`). Every one classifies `OperatorLocal` in `tst_settings_scope` if that test lists scopes by key.

## File Structure

**Create**

| Path | Responsibility |
|---|---|
| `src/core/hotkeys/HotKeyAction.h`, `HotKeyActionList.{h,cpp}` | The list: id, name, group, gesture, transmits, what it acts on, default keys |
| `src/core/hotkeys/HotKeyChord.{h,cpp}` | Parse, compare and classify a chord held as text |
| `src/core/hotkeys/HotKeyBindings.{h,cpp}` | User bindings, pins, rules, reserved list, import and export, `AppSettings` |
| `src/core/hotkeys/TuneCoalescer.h` | The 20 ms burst limit as pure logic with an injected clock |
| `src/core/hotkeys/HotKeyDeviceBindings.{h,cpp}` | Control-to-action defaults from spec section 5 and user overrides |
| `src/core/CwSoftPaddle.{h,cpp}` | Keyboard and deck dot and dash, with the serial reader's gate rules |
| `src/gui/hotkeys/HotKeyTargets.h` | The interface the dispatcher calls |
| `src/gui/hotkeys/HotKeyDispatcher.{h,cpp}` | The only code that runs an action |
| `src/gui/hotkeys/MainWindowHotKeyTargets.{h,cpp}` | The real targets, a friend of `MainWindow` |
| `src/gui/hotkeys/KeyboardRouter.{h,cpp}` | Application event filter, section 6 rules, hold failsafes |
| `src/gui/hotkeys/HotKeyStateFeed.{h,cpp}` | Publishes on/off state and the slice display to devices and plugins |
| `src/gui/hotkeys/ControllerHost.{h,cpp}` | Owns the MIDI and USB workers and routes their events to the dispatcher |
| `src/gui/hotkeys/midi/MidiRelativeDecoder.h`, `MidiInput.{h,cpp}`, `MidiProfile.{h,cpp}` | MIDI |
| `src/gui/hotkeys/devices/DeviceParser.h`, `Rc28Parser`, `PowerMateParser`, `ShuttleParser`, `TMate2Parser`, `StreamDeckPlusParser`, `FlexControlParser`, `UlanziChordDecoder` (each `.{h,cpp}`) | Pure byte and token decoding, no device library, tested on the Mac |
| `src/gui/hotkeys/devices/DeviceInput.{h,cpp}`, `GestureEngine.{h,cpp}`, `UsbPresence.{h,cpp}` | hidapi worker, press versus hold, seeing a device before opening it |
| `src/gui/hotkeys/devices/DeviceOutput.{h,cpp}` | Building light, LCD and key image reports |
| `src/gui/hotkeys/devices/FlexControlInput.{h,cpp}`, `UlanziLinuxInput`, `UlanziMacInput`, `UlanziWindowsInput`, `PowerMateLinuxInput` (each `.{h,cpp}`) | Serial and OS backends |
| `src/gui/hotkeys/DeckLink.{h,cpp}` | Plugin socket, discovery file, JSON lines |
| `src/gui/setup/HotKeySetupPages.{h,cpp}`, `KeyboardMapWidget.{h,cpp}`, `MidiSetupPage.{h,cpp}`, `KnobsSetupPage.{h,cpp}`, `DeckPluginsSetupPage.{h,cpp}` | The four pages |
| `third_party/rtmidi/`, `third_party/hidapi/` | Vendored |
| `plugins/common/actions.json`, `plugins/tools/gen-manifests.py`, `plugins/elgato-nereussdr/`, `plugins/ulanzi-nereussdr/`, `plugins/streamcontroller-nereussdr/` | Plugins |
| `packaging/linux/60-nereussdr-controllers.rules` | Linux device access |
| `tests/tst_hot_key_*.cpp`, `tests/tst_midi_*.cpp`, `tests/tst_device_parser_*.cpp`, `tests/tst_deck_link*.cpp`, `tests/tst_remote_cw_paddle.cpp` | Tests, named per task |

**Modify**

| Path | Change |
|---|---|
| `CMakeLists.txt` | Sources, vendored libs, optional `NEREUS_HAVE_RTMIDI`, `NEREUS_HAVE_HIDAPI` |
| `tests/CMakeLists.txt` | Register tests |
| `src/gui/MainWindow.{h,cpp}` | Friend, build dispatcher and router, View menu switch, Tools MIDI item |
| `src/gui/SetupDialog.cpp` | New group, retire two placeholders |
| `src/core/UnbuiltFeatureList.h`, `tests/tst_unbuilt_features.cpp`, `tests/tst_remote_gui_gating.cpp` | Remove `Keyboard` and `Midi` |
| `src/gui/widgets/VfoWidget.{h,cpp}` | `beginFrequencyEdit()` |
| `src/core/CwKeyLine.{h,cpp}`, `src/core/CwxSender.{h,cpp}`, `src/models/RadioModel.{h,cpp}` | Soft paddle bits, remote paddle, `setHoldToTalk`, stop paths |
| `src/core/session/*` (Task 6), `tests/data/link/v1/*`, `tests/LinkSurface.cpp`, `docs/architecture/2026-09-23-station-link-v1.md` | `cw.paddle` |
| `packaging/macos/Info.plist.in`, `.github/docker/Dockerfile`, `.github/workflows/ci.yml`, `.github/workflows/release.yml` | Input Monitoring string, `libudev-dev`, plugin bundling |

---

# Keyboard

## Task 1: Action list, chords and bindings

**Requirements:** spec 5, 7.1, 8, 10 (dropped bindings, import). R-HK-01 (defaults exist), R-HK-12, R-HK-18 (no GUI header).

**Files:**
- Create: `src/core/hotkeys/HotKeyAction.h`, `HotKeyActionList.{h,cpp}`, `HotKeyChord.{h,cpp}`, `HotKeyBindings.{h,cpp}`
- Modify: `CMakeLists.txt` (`CORE_SOURCES`), `tests/CMakeLists.txt`
- Test: `tests/tst_hot_key_bindings.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```cpp
  namespace NereusSDR {
  enum class HotKeyGesture { Press, Hold, Turn };
  enum class HotKeyScope { Slice, TransmitSlice, Pan, Window };
  struct HotKeyAction {
      QString id; QString name; QString group;
      HotKeyGesture gesture; bool transmits; HotKeyScope scope;
      QStringList defaultKeys;   // canonical chord text
  };
  class HotKeyActionList {       // static table, in the order of the plan's Action ids table
  public:
      static const QList<HotKeyAction>& all();
      static const HotKeyAction* find(const QString& id);
  };
  struct HotKeyChord {           // modifiers: Ctrl, Shift, Alt, Meta, Num; key: a Qt portable key name
      static std::optional<HotKeyChord> parse(const QString& text);
      QString toString() const;  // canonical order Ctrl+Shift+Alt+Meta+Num+Key
      bool hasNoModifier() const;
      bool isLetterOrNumberOrArrow() const;
      bool operator==(const HotKeyChord&) const;
  };
  enum class HotKeyBindResult { Bound, RefusedTransmitPlainKey, RefusedLatchedOnSpace, RefusedReserved, Conflict };
  struct HotKeyBindOutcome { HotKeyBindResult result; QString reason; QString conflictId; };
  class HotKeyBindings : public QObject {
      Q_OBJECT
  public:
      explicit HotKeyBindings(QObject* parent = nullptr);
      void load(); void save();
      QStringList keysFor(const QString& id) const;
      QString actionForChord(const QString& chord) const;   // empty if unbound
      QChar pinFor(const QString& id) const;                // null for the selected slice
      void setPin(const QString& id, QChar letter);
      HotKeyBindOutcome bind(const QString& id, const QString& chord, bool replaceConflict);
      void clear(const QString& id);
      void resetToDefault(const QString& id);
      void resetAll();
      bool enabled() const; void setEnabled(bool on);
      QString exportText() const;
      QStringList importText(const QString& text);          // returns skipped lines with reasons
      static QString reservedBy(const QString& chord);      // menu item name, or empty
  signals:
      void bindingsChanged();
      void enabledChanged(bool on);
  };
  }
  ```

**Acceptance:**
- `HotKeyActionList::all()` holds exactly the ids in the plan's table with those gestures, flags and default keys.
  No two actions share a default key. There are 50 ids.
- Fresh settings: `actionForChord("Right") == "tune.up"`, `("Num++") == "volume.up"`, `("Up") == "volume.up"`,
  `("Ctrl+T") == "tx.tune"`, `("Ctrl+F8") == "cw.msg8"`, `("/") == "cw.straight"`. `enabled()` is true.
- `HotKeyChord::parse("Shift+Ctrl+T")` canonicalizes to `Ctrl+Shift+T`. `parse("")` and `parse("Ctrl+")` are empty.
- `bind("tx.mox","T",false)`, `bind("tx.hold","7",false)`, `bind("cw.msg1","Left",false)`, `bind("voice.msg1","Q",false)`
  all return `RefusedTransmitPlainKey` with a reason. `bind("tx.mox","Space",false)`, `("tx.tune","Space")`,
  `("tx.twotone","Space")` return `RefusedLatchedOnSpace`. `bind("cw.dot",",",false)` and `bind("tx.mox","Ctrl+Y",false)`
  are `Bound`. `bind("cw.dash",";",false)` is `Bound`: only letters, numbers and arrows refuse.
- `bind("mute","Ctrl+Q",false)` returns `RefusedReserved` naming the menu item, for each of Ctrl+comma, Ctrl+Q, Ctrl+K,
  Ctrl+L, Ctrl+R, Ctrl+M, Ctrl+X, Ctrl+1 to 9, Ctrl+Shift+A, S, R, D, K, N, X.
- `bind("mute","B",false)` returns `Conflict` with `conflictId == "filter.next"` and changes nothing;
  `bind("mute","B",true)` binds and leaves `filter.next` with no key.
- After save, a fresh `HotKeyBindings` reads the same state; only non-default values are stored. A cleared key stays
  cleared across save and load, and a later default change to another action arrives.
- A setting for an unknown action id is dropped with a `qCWarning` at load.
- `exportText()` writes one tab-separated line per action, `id<TAB>keys joined by a space<TAB>pin`, after a
  `# NereusSDR shortcuts 1` line. Comma and space keys round-trip. `importText` of that text restores the state.
  An import with an unknown id, a refused key and a good line applies the good line and returns the two others with
  reasons.
- Nothing in these files includes a GUI header: `tst_core_has_no_gui_includes` passes.

**Verification:** Ordinary feature, unit layer. `run-test.sh tst_hot_key_bindings tst_core_has_no_gui_includes`.

**Execution note (advisory):** opus. No network or device. First task; no prerequisites.

- [ ] **Step 1:** Write the list, chord and bindings with the test. Commit.

## Task 2: The dispatcher

**Requirements:** spec 7.1, 7.2, 7.3 hold rules, 7.6, V5, V6. R-HK-02, R-HK-06, R-HK-08 (ledger), R-HK-14.

**Files:**
- Create: `src/core/hotkeys/TuneCoalescer.h`, `src/gui/hotkeys/HotKeyTargets.h`, `src/gui/hotkeys/HotKeyDispatcher.{h,cpp}`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_hot_key_dispatcher.cpp` (with a fake `HotKeyTargets`)

**Interfaces:**
- Consumes: `HotKeyActionList`, `HotKeyBindings`.
- Produces:
  ```cpp
  class TuneCoalescer {              // pure logic
  public:
      static constexpr qint64 kMinIntervalMs = 20;   // Spec V5. AetherSDR MainWindow_Controllers.cpp:2294 [@1e0718ad]
      // Adds steps; returns the steps to send now, or 0 if the interval has not passed.
      int add(int steps, qint64 nowMs);
      int takeDue(qint64 nowMs);     // called by the timer
      void drop();                   // lock or refusal
      int pending() const;
  };
  enum class HotKeySourceKind { Keyboard, Midi, Knob, Deck };
  struct HotKeySource { HotKeySourceKind kind; QString instance; bool operator==(const HotKeySource&) const; };
  class HotKeyTargets {
  public:
      virtual ~HotKeyTargets() = default;
      virtual SliceModel* windowSlice() const = 0;
      virtual SliceModel* sliceByLetter(QChar letter) const = 0;
      virtual QChar letterOf(const SliceModel* slice) const = 0;
      virtual QString sliceRefusal(const SliceModel* slice) const = 0;   // empty when it may change
      virtual bool listened(const SliceModel* slice) const = 0;
      virtual bool tuneTo(SliceModel* slice, double hz) = 0;             // the flag's own path
      virtual void runSliceAction(const QString& id, SliceModel* slice, int steps) = 0;
      virtual void runWindowAction(const QString& id) = 0;
      virtual QString unavailableReason(const QString& id) const = 0;    // empty when available
      virtual bool transmitting() const = 0;                             // MOX, TUNE or two-tone on, or Core keyed
      virtual bool tuneOrTwoToneOn() const = 0;
      virtual bool keyHold(bool down) = 0;                               // true if the press was admitted
      virtual void runTransmitAction(const QString& id) = 0;             // tx.mox, tx.tune, tx.twotone
      virtual void runCwMessage(const QString& id) = 0;                  // cw.msgN, cw.stop
      virtual void setCwKeyDown(const QString& id, bool down) = 0;       // cw.dot, cw.dash, cw.straight
      virtual void toast(const QString& text) = 0;
      virtual qint64 nowMs() const = 0;
  };
  class HotKeyDispatcher : public QObject {
      Q_OBJECT
  public:
      HotKeyDispatcher(HotKeyBindings* bindings, HotKeyTargets* targets, QObject* parent = nullptr);
      void press(const QString& id, const HotKeySource& source, QChar pin = {});
      void release(const QString& id, const HotKeySource& source);
      void turn(const QString& idUp, const QString& idDown, int steps, const HotKeySource& source, QChar pin = {});
      void releaseAll(const HotKeySource& source);    // failsafes, one source
      void releaseAllHolds();                         // the off switch
  public slots:
      void onTransmitWentOff();                       // clears the ledger's claim
      void flushTune();                               // the timer's slot; public for tests
  };
  ```

**Acceptance (with a fake `HotKeyTargets`):**
- **Off switch:** with `enabled()` false, press, release and turn call no target. Turning it off during a hold calls
  `keyHold(false)` once.
- **Slice order (7.2):** a binding with pin `B` and an open slice B acts on B; with B closed nothing runs and the toast is
  exactly `Slice B isn't open.` No pin acts on `windowSlice()`. A deck or MIDI pin argument overrides the binding's pin.
- **Listened:** on a slice with `listened()` true, `tune.up`, `filter.next`, `mode.next`, `band.next`, `nr.toggle` and
  `rit.toggle` run nothing and toast `sliceRefusal()` unchanged; they never move to another slice. `volume.up`, `volume.down`
  and `mute` do run.
- **Lock (V6):** `tune.up`, `band.next` and `mode.next` on a locked slice run nothing and toast `Slice A is locked.`
- **Unavailable:** any action whose `unavailableReason` is non-empty runs nothing and toasts the reason. `press` while refused
  does not start a hold.
- **Burst (R-HK-14):** with an injected clock, ten `tune.up` presses 3 ms apart produce at most one `tuneTo` per 20 ms of clock
  and sum to ten steps once flushed. `turn("tune.up","tune.down",5,src)` adds 5; `-3` subtracts 3. Starting at 14,200,123 Hz with a
  100 Hz step, +1 sends 14,200,200 and -1 sends 14,200,000 (`round(hz/step)*step`, `SpectrumWidget.cpp:14022`).
  A second tune within a burst builds on the in-flight target, not the slice's stale frequency. `tune.up10` counts ten steps.
- **Drop:** the slice locks while steps are queued: `flushTune` sends nothing and no step survives to the next press.
- **Hold ledger (R-HK-08):**
  - not transmitting, press `tx.hold` from Keyboard: `keyHold(true)`; release: `keyHold(false)`.
  - already `transmitting()`, press then release: `keyHold` never called. So a latched MOX survives letting go of Space.
  - two sources hold: first press keys, second does nothing; first release does nothing; last release calls `keyHold(false)`.
  - `keyHold(true)` returns false: release does nothing.
  - `onTransmitWentOff()` during a hold, then a key from elsewhere, then release: `keyHold(false)` not called.
  - `tuneOrTwoToneOn()` true at release: `keyHold(false)` not called and the ledger clears.
  - a duplicate press from one source is ignored. A release with no press is ignored.
  - `releaseAll(source)` releases that source's holds and CW keys only; another source's hold stays.
- **CW keys:** `press("cw.dot")` calls `setCwKeyDown("cw.dot",true)`; release calls it false; `releaseAll` releases any down.
  `unavailableReason` non-empty for `cw.dot` from a press means no later release call.
- **Levels:** `turn("volume.up","volume.down",2,src)` calls `runSliceAction("volume.up", slice, 2)` under the same 20 ms limit, summing.
- **Pan and window scope:** `zoom.in`, `slice.next` and band stack actions call `runWindowAction` with no slice lookup.

**Verification:** A transmit state transition is in scope, so tests go first for the ledger. Unit layer.
`run-test.sh tst_hot_key_dispatcher`.

**Execution note (advisory):** opus. Prerequisite Task 1. The ledger is what keeps Space from dropping someone's key: flagged for
JJ as a candidate for an earlier look, along with Tasks 4 to 6.

- [ ] **Step 1:** Write the ledger and coalescer tests first against the fake targets, then implement. Commit.

## Task 3: Non-transmit actions reach the window

**Requirements:** spec 5, 7.2, 7.7, section 13 (band stack). R-HK-01 (non-transmit), R-HK-06, R-HK-07.

**Files:**
- Create: `src/gui/hotkeys/MainWindowHotKeyTargets.{h,cpp}`
- Modify: `src/gui/MainWindow.{h,cpp}` (`friend class MainWindowHotKeyTargets;`, build `HotKeyBindings`, targets and dispatcher, public `HotKeyDispatcher* hotKeyDispatcher() const`), `src/gui/widgets/VfoWidget.{h,cpp}` (`void beginFrequencyEdit();` holding the code at `.cpp:4321-4326`, used by the double click too), `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_hot_key_window_actions.cpp`, `tests/tst_hot_key_remote_actions.cpp`

**Interfaces:**
- Consumes: `HotKeyTargets`, `HotKeyDispatcher`.
- Produces: `MainWindowHotKeyTargets(MainWindow*)`, implementing every non-transmit method. `transmitting`, `keyHold`, `runTransmitAction`, `runCwMessage`, `setCwKeyDown` return safe no-ops until Tasks 4 and 5. `HotKeyDispatcher* MainWindow::hotKeyDispatcher() const`, `HotKeyBindings* MainWindow::hotKeyBindings() const`. `void VfoWidget::beginFrequencyEdit()`.

**Acceptance (real `MainWindow` with `ConnectionStartup::Deferred`, per `tst_desktop_station_window.cpp:244-262`):**

Entry points; each must be exactly this:

| Action | Call |
|---|---|
| `tune.*` | `VfoWidget::tuneFromDrag(hz)` of the slice's own flag (`m_vfoWidgetsBySlice`). Step from `SliceModel::stepHz()`. |
| `step.larger`, `step.smaller` | `changeTuneStepUp()`, `changeTuneStepDown()` |
| `slice.lock`, `anf.toggle`, `rit.toggle`, `xit.toggle` | The matching `ContainerButtonDispatcher::click` id, or the setter it calls |
| `nr.toggle`, `nb.toggle` | `Id::Nr` and `Id::Nb` cycle (`ContainerButtonDispatcher.cpp:979-987`; `cycleNbMode`). `nrSelectionRefused` already toasts. |
| `tune.goto` | Unfold if collapsed, then `beginFrequencyEdit()` |
| `slice.next` | `selectSliceForWindow(next)` |
| `filter.*` | `SliceModel::setFilter(low, high)` from `presetsForMode` |
| `volume.*`, `mute` | Controlled: `setAfGain`, `setMuted`. Listened: `setFlagListenVolume(id, level, muted)`, then repaint the flag with `setListenVolume` on a remote window. |
| `band.*` | `RadioModel::onBandButtonClicked(slice, band)`. Never the no-slice overload. |
| `mode.*` | `SliceModel::setDspMode` |
| `zoom.*` | `SpectrumWidget::zoomBy(1.0/1.5)` and `zoomBy(1.5)` on the pan with focus: the active floating `PanFloatingWindow`'s pan via `panId()` and `PanadapterStack::spectrum`, else `activeSpectrumWidget()`. |
| `bandstack.*` | See below. |

- **Band stack, by name:** `runWindowAction` for `bandstack.next`, `bandstack.previous`, `bandstack.add`, `bandstack.panel` invokes the `MainWindow` slots `bandStackNext()`, `bandStackPrevious()`, `bandStackAdd()`, `bandStackPanel()` with `QMetaObject::invokeMethod`. `unavailableReason` for each is the literal `Band stacking is not ready.`, held in `MainWindowHotKeyTargets.cpp`, when `metaObject()->indexOfSlot(...)` is -1. Do not call `containerBandStackReason()`: band stack's plan removes it. Test against a fake `QObject` with those four slots and a bare one; the lookup takes the object as a parameter so the test needs no band stack branch.
- **Voice:** `unavailableReason("voice.msg1")` is `Voice messages aren't built yet.` while `UnbuiltFeatures::isBuilt(UnbuiltFeature::Voice)` is false.
- Each table row: fire the action, observe the model change. `tune.up` from 14,200,000 with 100 Hz becomes 14,200,100 on the slice and on the flag's label.
- Filter: walking from the first preset of USB with `filter.next` gives the second; `filter.previous` from the first gives the last; from an off-preset filter next gives the first.
- Mode: walking once around the list from USB returns to USB and never lands on RADE2 unless enabled. Band: `band.next` from 10 m goes to 6 m, and from WWV wraps to 160 m. 2 m is skipped while unavailable.
- **R-HK-07, the PR 698 fault:** with two slices on two pans, a remote window and a local window each make the same pan and slice choice for the same selected flag. `MainWindow::windowRxSlice(true)` is the only slice choice in the targets.
- Remote (`tst_remote_gui_gating.cpp:4640-4670` shape): a listened slice refuses tune with the reason from `sliceChangeRefusal`, and volume calls `requestListenLevel`.
- Disabled voice and band stack keys: pressing one shows its reason and changes nothing.

**Verification:** Ordinary feature, integration on a real offscreen window. `run-test.sh tst_hot_key_window_actions tst_hot_key_remote_actions tst_desktop_station_window`.

**Execution note (advisory):** opus. Prerequisite Task 2. `MainWindow.h` rebuilds a lot; batch your changes.

- [ ] **Step 1:** Add `beginFrequencyEdit` and the targets with their tests. Commit.

## Task 4: Transmit keys

**Requirements:** spec 7.3, 7.7. R-HK-01 (transmit), R-HK-08, R-HK-10, R-HK-11.

**Files:**
- Modify: `src/gui/hotkeys/MainWindowHotKeyTargets.{h,cpp}`, `src/models/RadioModel.{h,cpp}` (`void setHoldToTalk(bool down)`), `src/gui/MainWindow.cpp` (connect transmit-off to `onTransmitWentOff`), `tests/CMakeLists.txt`
- Test: `tests/tst_hot_key_transmit.cpp`

**Interfaces:**
- Consumes: `HotKeyDispatcher::onTransmitWentOff`.
- Produces: `void RadioModel::setHoldToTalk(bool down)`:
  - Remote routed: `m_station->remoteTransmit()->setScreenKey(down)`, and a release only when `screenKeyDown()` is true, because `setScreenKey(false)` at idle releases any epoch of this device (`RemoteTransmitClient.cpp:451-456`).
  - Local: press runs `completeTuneOff()` if `m_pendingTuneOff`, as `setMoxFromButton` does (M9), then `m_moxController->onSpacePtt(true)`. Release: `onSpacePtt(false)`.

**Acceptance:**
- `tx.mox`, `tx.tune`, `tx.twotone` go through `ContainerButtonDispatcher::click(Id::Mox, ...)`, `Id::Tun`, `Id::TwoTon`, so all three window kinds, locks, refusal reasons and toggle state are the buttons' own. Local window, not transmitting: `tx.mox` keys with `isManualKey()` true; a second press unkeys. Same for TUNE and two-tone. MOX and TUNE stay on with no release call and do not drop on `releaseAll`.
- Local `tx.hold`: press keys with `PttMode::Space`. **It stays keyed across a PollPTT pass:** run the controller's poll and assert still keyed. If the poll unkeys because no manual key is set, `setHoldToTalk` sets `setManualKey(true)` on press and false on release, as `onMoxButton` does; say so in your report. Release unkeys.
- MOX latched, then Space press and release: still keyed, still manual. TUNE on, then Space: nothing changes.
- Hold on, MOX button pressed: unkeys (its off). Space release then does nothing.
- Hold on, time-out fires: unkeys, `onTransmitWentOff` clears the claim, and a later MOX is not dropped by the release (R-HK-11).
- TX inhibit or a refused gate: `keyHold` returns false, the refusal shows through `moxRejected`'s existing toast, and nothing keys (R-HK-11).
- Remote window with a `RemoteTransmitClient` on a loopback link: press sends `tx.key {trigger:"screen"}`, release sends one release with its epoch; with nothing pressed, a release sends nothing. A remote window with `transmitControlsPermitted()` false refuses with `remoteTransmitReason()`.
- Hosting window: hold uses `DesktopStationController::requestMox`. With another device holding transmit (`Ask`), the press toasts `TxRefusals::otherDeviceHolds(name).text`, no dialog opens, and nothing keys. A plain hosted key works.
- Connecting a radio with `HotKeys/Enabled` true and a held chord in no state calls no key: connect a fake connection and assert no `setMox` (R-HK-10).
- `transmitting()` is true for MOX, TUNE, two-tone and a Core key (`screenKeyDown` or `m_coreTransmitting`'s accessor).

**Verification:** Safety boundary (what makes hardware transmit): tests first where it can be reproduced. Unit and integration. `run-test.sh tst_hot_key_transmit tst_mox_controller_ptt_source_dispatch`. Hardware pending: JJ's run in Task 9.

**Execution note (advisory):** opus. Prerequisites Tasks 2, 3. Touches what keys the radio: flagged for JJ for an earlier independent look. No wire change.

- [ ] **Step 1:** Write the acceptance cases against `RadioModel` with an injected connection (`tst_cwx_applet.cpp:105-132`), then wire. Commit.

## Task 5: CW messages and keys, local

**Requirements:** spec 7.4, section 5. R-HK-13 (local).

**Files:**
- Create: `src/core/CwSoftPaddle.{h,cpp}`
- Modify: `src/core/CwKeyLine.{h,cpp}` (second pair of bits), `src/models/RadioModel.{h,cpp}` (own and wire the soft paddle, release it at the stops `RadioModel.cpp:31263, 31382, 31461`), `src/gui/hotkeys/MainWindowHotKeyTargets.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_cw_soft_paddle.cpp`, `tests/tst_hot_key_cw.cpp`

**Interfaces:**
- Consumes: `CwKeyerController::keyerAllowed`, `refusal`.
- Produces:
  ```cpp
  // CwKeyLine additions. The serial reader and the soft paddle never clear each other.
  void setSoftDot(bool down);
  void setSoftDash(bool down);
  // dot(), dash() and p1Bits() report serial OR soft.
  class CwSoftPaddle : public QObject {
      Q_OBJECT
  public:
      explicit CwSoftPaddle(QObject* parent = nullptr);
      void setLine(CwKeyLine* line);
      void setGateAllowed(bool allowed, const TxRefusal& refusal = {});
      void setDot(bool down);
      void setDash(bool down);
      void releaseKeys();
  signals:
      void keyRefused(const NereusSDR::TxRefusal& refusal);   // once per press
  };
  CwSoftPaddle* RadioModel::cwSoftPaddle() const;
  ```

**Acceptance:**
- `CwKeyLine`: serial dot down plus soft dot down then soft up leaves `dot()` true and `p1Bits` dot set; the reverse too. The stored soft bits never reach `p1Bits` beyond the existing mask (`p1Bits(false) & ~0x07 == 0`, `p1Bits(true) & ~0x0F == 0`). `setOnChange` fires once per real change.
- `CwSoftPaddle` follows `CwSerialKeyInput::setPaddleLocked` (`.cpp:797-830`): a press with the gate shut blocks until a fresh press and emits `keyRefused` once; a gate going false releases what is down; `releaseKeys` clears both; with no line, a press blocks.
- `RadioModel` follows the keyer gate as it does for the reader (`:34058`), and `keyRefused` reaches `MoxController::reportCwRefusal`.
- **Comma, period, slash in CW mode (R-HK-13):** `cw.dot` sets the soft dot, `cw.dash` the soft dash, `cw.straight` calls `CwxSender::setHandKey(true)` after asking `keyerAllowed()`, blocked until a fresh press if shut. Release clears each.
- **In a voice mode** (USB on the transmit slice): each does nothing, and shows no toast.
- `cw.msg1` to `cw.msg8` call `CwxSender::sendMemory(1..8)`, `cw.stop` calls `stop()`. Remote window: `sendCwMemory(n)` and `stopCw()`, and a zero return shows `cwxRemoteSendReason()`. Refusals reach the toast through the sender's `refused` signal.
- Remote window: `cw.straight` calls `setCwKey(down)`. Until Task 6, `cw.dot` and `cw.dash` are unavailable with the reason `Dot and dash need a newer Core.`
- Hosting window while another device holds transmit: all three keys refuse with the holder's reason.

**Verification:** Safety boundary. Unit and integration. `run-test.sh tst_cw_soft_paddle tst_hot_key_cw tst_cwx_applet`. Radio's keyer timing is not measured; hardware evidence pending in Task 9.

**Execution note (advisory):** opus. Prerequisites Tasks 2, 3. Touches `CwKeyLine`, read on the connection thread: atomics only. Flagged for JJ for an earlier look.

- [ ] **Step 1:** Write the line and paddle tests, implement, wire the targets. Commit.

## Task 6: Remote dot and dash (`cw.paddle`)

**Requirements:** spec V11, 7.4, 7.7. R-HK-13 (remote).

**Files:**
- Modify: `src/core/session/RemoteTransmitClient.{h,cpp}` (`quint32 setCwPaddle(bool dot, bool dash)`, `bool cwPaddleAvailable() const`), `src/core/session/StationClient.{h,cpp}` (`:1020` declare 3, `remoteCwPaddleAvailable()` needs version 3, keep `:8013` and `:8473` rules), `src/core/session/StationServer.{h,cpp}` (`:1658` to 3, `:4696+` access, `:6857-6867`, `:15295`), `src/core/session/SessionCommandDispatcher.{h,cpp}` (`:1041-1046` table, `:1592-1593`, `:2058+` `handleCw`, `:455-465` `CwRequest::Kind::Paddle`), `src/core/session/StationCapabilities.h` comments, `src/core/CwxSender.{h,cpp}`, `src/models/RadioModel.cpp`, `src/gui/hotkeys/MainWindowHotKeyTargets.cpp`
- Modify docs and fixtures: `tests/data/link/v1/surface.json`, `tests/data/link/v1/sessions/verbs-cw.json`, `tests/LinkSurface.cpp:837`, `tests/tst_remote_cw.cpp:273`, `docs/architecture/2026-09-23-station-link-v1.md` (section 18.10 about :9380, verb table :6541, vectors :8386, rendered by `scripts/render-link-tables.py`)
- Test: `tests/tst_remote_cw_paddle.cpp`

**Interfaces:**
- Consumes: `CwKeyLine::setSoftDot`, `setSoftDash` from Task 5; `requestRemoteCwxFor`.
- Produces: wire verb `cw.paddle {dot: bool, dash: bool}`, at `remoteCwVersion` 3, minor `kRadioIdentitySessionProtocolMinor`, sent three times under one id like `cw.key`. `bool CwxSender::holdRemotePaddle(quint32 epoch, bool dot, bool dash, RemoteAdmission admit)`, `void CwxSender::setPaddleHeld(bool dot, bool dash)`.

**Acceptance:**
- `surface.json` lists `cw.paddle`, `remoteCwVersion` is 3 in `LinkSurface`, and `python3 scripts/render-link-tables.py --check` passes.
- `tst_remote_cw`: a current peer gets 3; a peer that declared 2 gets the server's value per `:15293-15296`, and every version-2 behavior is unchanged. `cw.key`, `cw.send`, `cw.memory` still start at version 2.
- A start from a peer that did not declare 3 is refused with `Update this app to send dot and dash through this Core.` A release (`dot false, dash false`) is always taken.
- Engaging (`dot || dash`) takes the same path as `Kind::Key` in `access.cw`: `txDecisionFor`, the holder rule, then `requestRemoteCwxFor(requester, true, ...)`. A non-holder is refused with the holder's words.
- On the Core, `dot` sets the soft dot bit and `dash` the soft dash bit; no CWX bit and no PTT write, so the radio's keyer does the timing and break-in, as for serial paddles. Both false clears both.
- **Safety:** the paddle's deadline is `kKeyWatchdogMs` after the last press, renewed by each press; passing it clears the bits. A link hold suppresses dot and dash on the wire, as `applyCwKeyOverlay` (`P1RadioConnection.cpp` near :2827) does for CWX, and P2's `syncCwKeyBits` too: check both connections with a test, and extend the mask if they cover only CWX. A cut or a session closing clears the bits at once. Read `2026-10-27-remote-cw-hold-amendment.md` first; this activity must behave like `cw.key` under it.
- Client: `setCwPaddle` refreshes the keepalive as `setCwKey` does. Window: `cw.dot` and `cw.dash` send it with the combined state of both keys; below version 3 they stay unavailable with `Dot and dash need a newer Core.` and the Keyboard page shows that reason on their rows.
- Client releases on the same conditions as `cw.key`.
- Phone: no phone code changes; `LinkFeatures.swift:250` already tests `>= 2`. Confirm by reading it and say so in your report.

**Verification:** Safety boundary and networking: tests before behavior, integration over the loopback link. `run-test.sh tst_remote_cw_paddle tst_remote_cw tst_hot_key_cw`, then `python3 scripts/render-link-tables.py --check`.

**Execution note (advisory):** opus. Prerequisite Task 5. Touches networking and what keys the radio: flagged for JJ for an earlier independent look. No parallel work with anything in `src/core/session`.

- [ ] **Step 1:** Add the verb with its fixtures and conformance rows, then the sender and safety, then the window. Commit.

## Task 7: Keyboard router

**Requirements:** spec 6, 7.1, 7.3 drop list, V2, V3. R-HK-01, R-HK-03, R-HK-04, R-HK-05, R-HK-08 (focus), R-HK-09.

**Files:**
- Create: `src/gui/hotkeys/KeyboardRouter.{h,cpp}`
- Modify: `src/gui/MainWindow.cpp` (install it **before** `ArrangeHoldFilter` is built at about `:9403`), `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_hot_key_router.cpp`

**Interfaces:**
- Consumes: `HotKeyDispatcher`, `HotKeyBindings`, `ContainerManager::arrangeMode`, `arrangeHeld`.
- Produces:
  ```cpp
  class KeyboardRouter : public QObject {
      Q_OBJECT
  public:
      KeyboardRouter(HotKeyDispatcher* dispatcher, HotKeyBindings* bindings, MainWindow* window);
      static QString chordFromEvent(const QKeyEvent* event);   // canonical text; strips Num from arrows
  protected:
      bool eventFilter(QObject* watched, QEvent* event) override;
  };
  ```

**Acceptance (real window, `QTest::sendKeyEvent`, table-driven):**
- **Order:** Qt calls the last-installed application filter first. The router is installed earlier, so `ArrangeHoldFilter` sees each key before the router can eat it. Test: Shift down, Right within the 300 ms delay: `arrangeHeld()` stays false and the slice moves ten steps. Shift held past the delay, then Right: still ten steps with no handle focused.
- **Chords:** `chordFromEvent` of Right with `Qt::KeypadModifier` (macOS arrows carry it) is `Right`. Keypad + is `Num++`. `Ctrl+Shift+T` is canonical. A bare modifier press yields empty.
- **Section 6, each row, for each of a text box, multi-line text, number box and editable dropdown:** Right, Space and `B` reach the widget and the slice does not change. **Open dropdown list or menu:** every key stays there until it closes. **Arrange mode (`arrangeMode()`):** arrows do not tune; `B` does.
- **Tabbed control (R-HK-04, V2):** focus a slider with `Qt::TabFocusReason`: Right moves the slider, not the slice; Space does not key; `B` still acts. `Qt::BacktabFocusReason` the same. Focus moves away: arrows tune again. A slider focused with `Qt::MouseFocusReason`: Right tunes, Space keys. Same for a button, a tab bar, a closed dropdown.
- **Windows:** main window, `PanFloatingWindow` and `FloatingContainer` active: keys act. `SetupDialog`, any `QDialog`, `RoundtableWindow`, the amplifier chart window and any non-main window: no key acts, including from a Setup tree, table, tab or slider (R-HK-03). A modal dialog open over the main window: none. No active window: none.
- **Focus (R-HK-05):** with the CWX text box and the Roundtable message box focused, click the spectrum: focus stays in the box and keys go there. The router never sets focus.
- **Repeat:** `isAutoRepeat` press of `B` runs nothing extra; of Right, adds a step; of Up, adds a level; of Space, nothing. Auto-repeat releases are ignored. A keypad knob sending separate fast presses of Right goes through the same 20 ms limit, with no step lost (spec 7.6).
- **Release:** press and release are paired by `nativeVirtualKey` when non-zero, else key code, so `Ctrl+Shift+T` or Shift+comma released with the modifier lifted first still releases. A press with the chord's modifier held, then the modifier lifted, then the key: the release runs (R-HK-08).
- **Failsafe (R-HK-08):** holding Space or a CW key, each of these drops it once through `releaseAll`: active window change; `ApplicationStateChange` to inactive; a modal dialog opening. MOX stays on through each.
- **Off:** with `HotKeys/Enabled` false, no key is eaten; Space reaches the focused button.
- **R-HK-09:** in a text field, a dialog or an inactive window, no key changes `isMox()`. A table test across every transmit, CW and voice chord.
- Every chord in the Action ids table, fired once on a clicked background, runs its action (R-HK-01). Menu shortcuts still work: Ctrl+M reaches its menu item, not `tx.mox`.
- A GUI check: every `QAction` shortcut on the `MainWindow` is either in `HotKeyBindings::reservedBy`'s table or is not a default key, so the table can't drift.

**Verification:** Ordinary feature; UI behavior checked functionally, offscreen. `run-test.sh tst_hot_key_router`. Hardware pending: Task 9, including Cmd on a real Mac keyboard.

**Execution note (advisory):** opus. Prerequisites Tasks 3, 4, 5. The filter runs on every event in the app: return fast for anything but key, focus and activation events.

- [ ] **Step 1:** Write the section 6 table test, then the router. Commit.

## Task 8: Keyboard Setup page

**Requirements:** spec 9 (Keyboard), V4, section 10 (import). R-HK-02 (switches), R-HK-17.

**Files:**
- Create: `src/gui/setup/HotKeySetupPages.{h,cpp}` (`KeyboardPage`), `src/gui/setup/KeyboardMapWidget.{h,cpp}`
- Modify: `src/gui/SetupDialog.cpp` (category `Keys and controllers` in place of `Keyboard`, between `CAT & Network` and `Test`; leaf `Keyboard` as `SetupScope::ThisComputer`), `src/gui/MainWindow.cpp` (`&Keyboard Shortcuts...` opens the page and is enabled; a checkable `Use Shortcuts` item in View bound to `HotKeyBindings::setEnabled`), `src/core/UnbuiltFeatureList.h` and its source file (remove `Keyboard`), `tests/tst_unbuilt_features.cpp` (drop its row, add the new surfaces as always shown), `tests/tst_remote_gui_gating.cpp` (:2516 comment, floors), retire `KeyboardShortcutsPage` from `src/gui/setup/KeyboardSetupPages.{h,cpp}` and `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_hot_key_setup_keyboard.cpp`

**Interfaces:**
- Consumes: `HotKeyBindings`, `HotKeyActionList`, `MainWindow::hotKeyBindings()`.
- Produces: `class KeyboardPage : public SetupPage` taking `HotKeyBindings*`; `class KeyboardMapWidget : public QWidget` with `setLayer(int)`, signal `keyClicked(QString chord)`. Later pages take the same `HotKeyBindings*`.

**Acceptance:**
- As in the mockup's Setup tab, Keyboard leaf: Use shortcuts switch; a map with Plain, Shift, Ctrl and Ctrl+Shift layers, colored by group; a searchable list with Key, Acts on (selected slice or a letter), Set, Clear, Default; Reset all, Import, Export.
- Set: pressing a key binds it. `Escape` cancels. A conflict offers Replace or Cancel. Each refusal in section 8 shows its reason and changes nothing. Reserved keys show on the map and refuse with the menu's name.
- Acts on: a transmit action shows the transmit slice and its combo is disabled with that reason; `zoom.*`, `slice.next` and band stack too. A slice action offers Selected slice and A to D.
- A disabled voice or band stack row stays in the list, disabled, with its reason. Dot and dash show their reason on a Core below version 3.
- Import lists skipped lines afterward. Export writes the file through a save dialog, tested by calling the page's file functions directly. Good lines still apply.
- The switch and the View item move together, and turning it off stops keys (R-HK-02).
- The page is `ThisComputer` and works on a remote window. The tree shows `Keys and controllers` holding Keyboard, and later tasks add the rest; the old `Keyboard` category and `Shortcuts` leaf are gone.
- `tst_unbuilt_features` and `tst_remote_gui_gating` pass. `UnbuiltFeature::Midi` stays for now.

**Verification:** UI change: `ui-verification` chooses evidence. Functional test plus a capture read by the controller: `run-test.sh tst_hot_key_setup_keyboard tst_unbuilt_features tst_remote_gui_gating`, then `cmake --build build --target nereus_visual_preview` under the lane script and `python3 tests/visual/capture.py --out <scratch> --scenes 'dialog-setup-keyboard*' --profiles 'mac-1512x869@2' --jobs 4`. Report the image paths; the controller compares them with the mockup. Do not accept any baseline.

**Execution note (advisory):** opus. Prerequisite Task 7.

- [ ] **Step 1:** Build the page against the mockup, remove the placeholders, update the gating tests. Commit.

## Task 9: Keyboard on JJ's hardware

**Requirements:** spec V3, section 11 (H evidence for R-HK-01, 04, 05, 08, 09, 13, 17).

**Files:** none unless a finding needs a fix. The controller does this task, not an implementer.

**Interfaces:**
- Consumes: Tasks 1 to 8.
- Produces: a findings list. This task does not block later tasks; findings become fixes on the branch.

**Acceptance:**
- **Before the run, controller:** `defaults read com.apple.symbolichotkeys` (read-only) shows which of Cmd+F1 to F8, Cmd+Shift+F1 to F4 and Cmd+F10 macOS owns on this Mac. Any clash goes to JJ as one short question with a recommendation (V3).
- **Build:** a build with the branch and short SHA in its title bar, installed per the `NereusSDR Dev.app` rule, not launched over a running copy without checking. Tell the manager before launching on his Mac.
- **JJ's checklist, in this order:** no setup needed; tune with arrows and Shift+arrows; `[` and `]`; filter B and V; volume Up and Down; U mute; R, E, A; click a slider then arrows; Tab to a slider then arrows; type in the CWX box after a spectrum click; Space to talk and let go; hold Space and Cmd+Tab away (drops); MOX with Cmd+Shift+M, then Space and let go (stays on); in CW mode, comma, period and slash; the Keyboard page against the mockup. Report what he sees as pending until he says so.
- **Decides:** anything he rejects goes back to a fix round. Nothing here calls the feature ready to merge.

**Verification:** Human smoke, JJ, on his radio. Hardware state stays pending until he reports. Report status as "ready for his hardware run", never "ready to merge".

**Execution note (advisory):** controller and JJ. Prerequisite Task 8. Push once for this round, with draft PR.

- [ ] **Step 1:** Controller checks macOS keys, builds, installs, tells the manager the keyboard part is ready for his hardware run. Crew continues with MIDI meanwhile.

---

# MIDI

## Task 10: MIDI input

**Requirements:** spec 7.1, V7, V8, 10. R-HK-15 (MIDI), R-HK-18.

**Files:**
- Create: `third_party/rtmidi/{RtMidi.h,RtMidi.cpp,LICENSE,VERSION.txt}`, `docs/attribution/RTMIDI-PROVENANCE.md`, `packaging/third-party-licenses/rtmidi.txt`, `src/gui/hotkeys/midi/MidiRelativeDecoder.h`, `MidiInput.{h,cpp}`, `MidiProfile.{h,cpp}`, `src/gui/hotkeys/ControllerHost.{h,cpp}`
- Modify: `CMakeLists.txt`, `packaging/third-party-licenses/README.md`, `docs/attribution/AETHERSDR-PROVENANCE.md`, `docs/attribution/aethersdr-reconciliation.md`, `src/gui/MainWindow.cpp`, `tests/tst_core_has_no_gui_includes.cpp` (add a device library check), `tests/CMakeLists.txt`
- Test: `tests/tst_midi_decoder.cpp`, `tests/tst_midi_profile.cpp`

**Interfaces:**
- Consumes: `HotKeyDispatcher`, `HotKeyBindings`.
- Produces:
  ```cpp
  struct MidiMessage { quint8 status; quint8 data1; quint8 data2; };
  enum class MidiControlKind { Button, AbsoluteKnob, RelativeKnob };
  struct MidiBinding { QString controlKey; MidiControlKind kind; QString actionId; QString actionIdDown; QChar pin; };
  class MidiProfile { /* name, QList<MidiBinding>, load and save under HotKeys/Midi/Profiles */ };
  class MidiInput : public QObject {   // lives on the MIDI worker
      Q_OBJECT
  public:
      Q_INVOKABLE void openPort(const QString& name);
      Q_INVOKABLE void closePort();
      Q_INVOKABLE void beginLearn();
      static QStringList portNames();
  signals:
      void controlEvent(QString controlKey, MidiControlKind kind, int value, int delta);
      void learned(QString controlKey, MidiControlKind kind);
      void connectionChanged(bool connected, QString reason);
  };
  class ControllerHost : public QObject {   // main thread; owns workers
      Q_OBJECT
  public:
      ControllerHost(HotKeyDispatcher* dispatcher, HotKeyBindings* bindings, QObject* parent = nullptr);
      MidiInput* midi() const;
      void setMidiProfile(const MidiProfile& profile);
  };
  ```

**Facts to build on (AetherSDR `1e0718ad`):** Open with `RtMidiIn`, `setCallback`, `ignoreTypes(true,true,true)` (`src/core/MidiControlManager.cpp:108-132`). The callback runs on RtMidi's own thread: copy bytes and queue to the worker (`:271-298`). Name match is a case-insensitive substring (`:134-148`). Relative decoding (`src/core/MidiRelativeCcDecoder.h:50-76`): lock on the first unit detent, 63/65 is center-64, 1/127 is two's complement; `abs(delta) >= 56` re-classifies from the same byte (`:34,71-74`). Absolute CC is `/127`, pitch bend `(d1|d2<<7)/16383`, note velocity above 0 is down (`MidiControlManager.cpp:313-325,464-504`). Learn binds the next message, ignoring NoteOff (`:255-267,346-375`).

**Vendoring:** copy `RtMidi.h`, `RtMidi.cpp` and `LICENSE` from upstream tag 6.0.0 (github.com/thestk/rtmidi), recording its commit and hashes in `VERSION.txt`. Apply one patch: AetherSDR's `5769f480` removing `throw()` from the CoreMIDI singleton, so errors propagate, and note it in the provenance file and the third-party README as the `libdatachannel` row does. If the network is denied in the sandbox, ask the controller to fetch the tag. Do not copy AetherSDR's snapshot. `third_party/rtmidi/RtMidi.cpp` goes in `GUI_SOURCES` and the `NereusGui_LTO` twin, with `-w` or `/w` on that source only, and no precompiled header. Backend defines and system libraries go in `nereus_apply_gui_deps` (`CMakeLists.txt:2858`): macOS `__MACOSX_CORE__` with CoreMIDI, CoreAudio, CoreFoundation; Windows `__WINDOWS_MM__` with `winmm`; Linux `__LINUX_ALSA__` with `alsa` (found at `:854`). If ALSA is missing on Linux, build with `NEREUS_HAVE_RTMIDI` undefined and the device stays listed and disabled, with the reason.

**Acceptance:**
- **Decoder (port of `MidiRelativeCcDecoder.h`, with attribution):** starting unlocked, `63` then `65` gives -1 then +1 in center-64. `1` then `127` gives +1 then -1 in two's complement. A first value of `40` gives 0 and stays unlocked. In center-64, `1` (delta -63) re-locks to two's complement and gives +1.
- **Messages:** `B0 07 40` absolute gives value 64. `E0 00 40` pitch bend gives 8192 of 16383. `90 3C 40` is a press; `90 3C 00` and `80 3C 00` are releases.
- **Routing:** a Button bound to `tx.hold` presses on Note On and releases on Note Off, through `press` and `release`. A RelativeKnob bound to `tune.up` and `tune.down` calls `turn`. An AbsoluteKnob on `volume.up` and `volume.down` moves its full range over 20 steps with no step lost, carrying the remainder; on another action, one step per CC value.
- **Learn:** the next message binds; a NoteOff doesn't. A CC that arrives with `1` or `127` learns as relative.
- **Profile:** bindings round-trip through `AppSettings`. Two profiles keep separate lists. A deleted action id is dropped with a warning.
- **Unplug (upstream can't see it):** `RtMidiIn::isPortOpen` only reflects open and close calls, so `MidiInput` compares `portNames()` on a 2 second timer owned by the worker: a vanished name closes, emits `connectionChanged(false, ...)`, calls `releaseAll` for that source; a returning name reopens. A test drives this through a port-names seam.
- `QThread` shutdown on close joins with no leak. The host thread and dispatcher talk only by queued signals.
- **R-HK-18:** the check fails if any file under `src/core` or `src/models` includes `RtMidi`, `hidapi` or `hid.h`, or if `CMakeLists.txt`'s `CORE_SOURCES` lists `third_party/rtmidi` or `third_party/hidapi`. A separate link check, `otool -L` or `ldd` on `nereusd`, if the target is built, shows neither; add the same grep beside `release.yml:534-535`.
- `python3 scripts/check-third-party-licenses.py`, `scripts/check-new-ports.py` and `scripts/compliance-inventory.py --fail-on-unclassified` pass.

**Verification:** Device behavior is simulated from documented layouts; the device stays marked untested. `run-test.sh tst_midi_decoder tst_midi_profile tst_core_has_no_gui_includes`.

**Execution note (advisory):** opus. Prerequisite Task 2. New dependency and new thread, both approved in V8. Touches build files: nothing else in parallel. A reconfigure after adding `third_party` is automatic.

- [ ] **Step 1:** Vendor, build, write the decoder and profile tests, wire `ControllerHost` into `MainWindow`. Commit.

## Task 11: MIDI Setup page

**Requirements:** spec 9 (MIDI), R-HK-17.

**Files:**
- Create: `src/gui/setup/MidiSetupPage.{h,cpp}`
- Modify: `src/gui/SetupDialog.cpp` (leaf `MIDI` under `Keys and controllers`, remove `MIDI Control` from `CAT & Network`), `src/gui/MainWindow.cpp` (`&MIDI Mapping...` opens it, enabled), `src/core/UnbuiltFeatureList.h` (remove `Midi`), `tests/tst_unbuilt_features.cpp`, `tests/tst_remote_gui_gating.cpp`, retire `CatMidiControlPage` from `src/gui/setup/CatNetworkSetupPages.{h,cpp}`, `tests/CMakeLists.txt`
- Test: `tests/tst_hot_key_setup_midi.cpp`

**Interfaces:**
- Consumes: `ControllerHost`, `MidiInput`, `MidiProfile`, `HotKeyActionList`.
- Produces: `class MidiSetupPage : public SetupPage`.

**Acceptance:**
- As in the mockup: device, profile (new, rename, delete), learn (pick an action, then move a control), and a control list with control, kind, action, Acts on and Clear.
- Learn with a fake input: picking `tune.up` and moving a CC adds a row that also fills `tune.down`. Escape cancels.
- No device: page shows `No MIDI device plugged in.` and its controls are disabled with that reason. A build without RtMidi: same page, disabled, `This build has no MIDI support.`
- "A controller starts empty": a new profile has no rows. The page says it is untested until someone tries a device (R-HK-19).
- `ThisComputer` scope, works on a remote window. `tst_unbuilt_features` and `tst_remote_gui_gating` pass with the leaf renamed.

**Verification:** UI per `ui-verification`. `run-test.sh tst_hot_key_setup_midi tst_unbuilt_features tst_remote_gui_gating`, then capture `dialog-setup-midi*` as in Task 8 and report the image paths.

**Execution note (advisory):** opus. Prerequisite Task 10.

- [ ] **Step 1:** Build the page, retire the placeholder. Commit.

---

# Knobs and decks

## Task 12: USB worker, gestures and state feed

**Requirements:** spec 7.1, 7.6, V8, 10, section 3 (on from the start). R-HK-02, R-HK-08 (unplug), R-HK-18, R-HK-19.

**Files:**
- Create: `third_party/hidapi/` (`hidapi/hidapi.h`, `mac/hid.c` and `hidapi_darwin.h`, `windows/hid.c` and its headers, `linux/hid.c`, `LICENSE-bsd.txt`, `VERSION.txt`), `docs/attribution/HIDAPI-PROVENANCE.md`, `packaging/third-party-licenses/hidapi.txt`, `src/core/hotkeys/HotKeyDeviceBindings.{h,cpp}`, `src/gui/hotkeys/devices/DeviceParser.h`, `DeviceInput.{h,cpp}`, `GestureEngine.{h,cpp}`, `UsbPresence.{h,cpp}`, `src/gui/hotkeys/HotKeyStateFeed.{h,cpp}`, `packaging/linux/60-nereussdr-controllers.rules`
- Modify: `CMakeLists.txt`, `packaging/third-party-licenses/README.md`, `packaging/macos/Info.plist.in` (`NSInputMonitoringUsageDescription`), `.github/docker/Dockerfile`, `.github/workflows/ci.yml`, `.github/workflows/release.yml` (add `libudev-dev` beside `libasound2-dev`), `src/gui/hotkeys/ControllerHost.{h,cpp}`, `src/gui/MainWindow.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_device_input.cpp`, `tests/tst_gesture_engine.cpp`, `tests/tst_hot_key_device_bindings.cpp`, `tests/tst_hot_key_state_feed.cpp`

**Interfaces:**
- Consumes: `HotKeyDispatcher`, `ControllerHost`, `HotKeySource`.
- Produces:
  ```cpp
  enum class DeviceId { Rc28, PowerMate, ShuttleXpress, ShuttlePro2, FlexControl, TMate2, StreamDeckPlus, UlanziDial };
  enum class ControlGesture { Press, Hold, Tap, DoubleTap, Turn };
  struct DeviceEvent {
      enum class Kind { ButtonDown, ButtonUp, Rotate, RingPosition, TokenGesture } kind;
      QString control;   // "wheel", "f1", "tx", "button3", "e1", "dial2", "topLeft", ...
      int value;         // steps for Rotate, -7..7 for RingPosition; 0 otherwise
      ControlGesture gesture;   // for TokenGesture only
  };
  class DeviceParser {           // pure; no device library, no Qt GUI
  public:
      virtual ~DeviceParser() = default;
      virtual QList<DeviceEvent> parse(QByteArrayView report) = 0;   // all changes in one report
      virtual void reset() = 0;
  };
  struct HotKeyDeviceBinding { QString actionId; QString actionIdDown; QChar pin; };
  class HotKeyDeviceBindings : public QObject {
      Q_OBJECT
  public:
      static QList<DeviceId> devices();
      static QString deviceName(DeviceId);
      static bool testedOnRealDevice(DeviceId);   // false for every device; no simulator may change this (R-HK-19)
      HotKeyDeviceBinding binding(DeviceId, const QString& control, ControlGesture) const;
      void setBinding(DeviceId, const QString& control, ControlGesture, const HotKeyDeviceBinding&);
      int speed(DeviceId) const; void setSpeed(DeviceId, int);     // 1 = every detent, N = one step per N
      bool reversed(DeviceId) const; void setReversed(DeviceId, bool);
      bool rc28TxStaysOn() const; void setRc28TxStaysOn(bool);
  };
  class GestureEngine {          // pure with an injected clock
  public:
      static constexpr int kHoldMs = 600;  // From AetherSDR src/gui/MainWindow_Controllers.cpp:2476-2495 [@1e0718ad]
      QList<DeviceEvent> onEvent(const DeviceEvent& raw, qint64 nowMs);  // only for controls with a Hold binding
      QList<DeviceEvent> onTick(qint64 nowMs);
  };
  class UsbPresence { public: static QSet<quint32> connectedVidPids(); };  // no HID open; no permission prompt
  class DeviceInput : public QObject {   // lives on the USB worker
      Q_OBJECT
  public:
      Q_INVOKABLE void start();
      Q_INVOKABLE void stop();
      Q_INVOKABLE void sendOutput(DeviceId id, QByteArray report);
  signals:
      void deviceEvent(NereusSDR::DeviceId id, NereusSDR::DeviceEvent event);
      void deviceStateChanged(NereusSDR::DeviceId id, bool connected, QString reason);
  };
  class HotKeyStateFeed : public QObject {
      Q_OBJECT
  public:
      bool isOn(const QString& actionId) const;
      struct Display { QString frequency; QString mode; QString filter; QChar slice; };
      Display display() const;
  signals:
      void stateChanged(QString actionId, bool on);
      void displayChanged();
  };
  ```

**Vendoring:** hidapi 0.15.0 from github.com/libusb/hidapi, tag `hidapi-0.15.0`, BSD licence choice, the platform C files compiled straight into `NereusSDRLib` so no DLL and no new package steps. macOS: IOKit, CoreFoundation, AppKit. Windows: as its `hid.c` needs. Linux: `libudev`, via pkg-config; missing `libudev` leaves `NEREUS_HAVE_HIDAPI` undefined and devices listed, disabled, with the reason. Same `-w`, no precompiled header. `nereus_apply_gui_deps` only. Ask the controller to fetch the tag if the sandbox denies it.

**Acceptance:**
- **Defaults:** `binding()` returns every row of spec section 5's knob table, and the "TMate 2 E1, E2" labels follow printed names (printed E1 is encoder index 2, E2 index 1: `src/gui/MainWindow_Controllers.cpp:855-860 [@1e0718ad]`). `testedOnRealDevice` is false for all eight. Overrides round-trip.
- **Gestures:** F1 press, release at 200 ms: Press. Held at 600 ms: Hold fires once, and its release fires nothing further. Clock injected. A control with no Hold binding reports press and release at once.
- **Routing:** a binding with a `Hold`-gesture action calls `press` on down and `release` on up; a `Press` action calls `press` on down. Rotate calls `turn` with the device's source, speed dividing steps with a kept remainder, reversed flipping the sign; a direction change clears the remainder. RingPosition sends `turn` of `value` every 100 ms while non-zero through `tune.up10` and `tune.down10` (our choice, spec section 5).
- **RC-28 TX stay-on:** with the setting on, a press toggles a latched `tx.hold` that the next press releases; unplug drops the latch.
- **Off switch:** with `HotKeys/Enabled` false, events reach nothing (R-HK-02).
- **Unplug:** a simulated read failure emits `deviceStateChanged(false, "Not plugged in")`, `releaseAll` runs for that source and the parser resets. A returning device reconnects: presence polled every 3 seconds. A permission or exclusive-open failure gives the fix in plain words and stops nothing else.
- **No prompt:** `DeviceInput::start` never calls `hid_open` or `hid_enumerate` for a device `UsbPresence` doesn't report. On macOS `UsbPresence` reads the IOKit registry (`IOServiceMatching("IOUSBHostDevice")`), never `IOHIDManager`. Windows and Linux may use `hid_enumerate`.
- One worker opens one device per `DeviceId`, first match wins, all at once. Polling at 5 ms (`HidEncoderManager.h:189-190 [@1e0718ad]`). Raw reports go to `qCDebug(lcHotKeyDevice)`, so an owner can send a capture.
- **State feed:** MOX, TUNE, two-tone, `tx.hold`, mute, lock, RIT, XIT, NR, NB and ANF report on and off from the models, for the window's slice, updating when the selected slice changes. Display: frequency with Hz, mode name, filter width, slice letter. No polling.
- `packaging/linux/60-nereussdr-controllers.rules` covers RC-28, PowerMate, ShuttleXpress, ShuttlePro v2, TMate 2 and Stream Deck+ with `TAG+="uaccess"`, using ids `0C26:001E`, `2341:0266`, `077D:0410`, `0B33:0020`, `0B33:0030`, `1721:0614` and `0FD9:0084`, and the Ulanzi rule `SUBSYSTEM=="input", KERNEL=="event*", ATTRS{name}=="Ulanzi Dial*", TAG+="uaccess"`.
- `python3 scripts/check-third-party-licenses.py` passes. `tst_core_has_no_gui_includes` passes. `HotKeyDeviceBindings` includes no GUI header.

**Verification:** Unit layer with a simulated reader seam. `run-test.sh tst_device_input tst_gesture_engine tst_hot_key_device_bindings tst_hot_key_state_feed tst_core_has_no_gui_includes`. A controller check: launch the dev build on this Mac with no device attached and confirm no Input Monitoring prompt. The CI image rebuild from the Dockerfile change is expected: the controller times that with a real push.

**Execution note (advisory):** opus. Prerequisites Tasks 2, 10. New dependency and thread (V8). Touches build files and CI config: nothing else in parallel.

- [ ] **Step 1:** Vendor and build. Write the gesture, routing and state tests with a fake reader. Commit.

## Task 13: RC-28, PowerMate and the Shuttles

**Requirements:** spec 5, R-HK-15, R-HK-08 (release in a chord).

**Files:**
- Create: `src/gui/hotkeys/devices/Rc28Parser.{h,cpp}`, `PowerMateParser.{h,cpp}`, `ShuttleParser.{h,cpp}`, `PowerMateLinuxInput.{h,cpp}`
- Modify: `DeviceInput.cpp` (device table), `docs/attribution/AETHERSDR-PROVENANCE.md`, `docs/attribution/aethersdr-reconciliation.md`, `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_device_parser_rc28.cpp`, `tests/tst_device_parser_powermate.cpp`, `tests/tst_device_parser_shuttle.cpp`

**Interfaces:**
- Consumes: `DeviceParser`, `DeviceEvent`, `DeviceId`.
- Produces: `class Rc28Parser : public DeviceParser`, `PowerMateParser`, `ShuttleParser(ShuttleParser::Model)`. Also `QByteArray rc28LightReport(bool tx, bool f1, bool f2, bool link)`.

**Facts (each with its cite in the code; `src/core/HidDeviceParser.cpp` `[@1e0718ad]`):**

| Device | VID:PID | Read | Layout |
|---|---|---|---|
| RC-28 | `0C26:001E`, emulator `2341:0266` | 32 | `[0]`=`01` or drop. `[1]` speed, `[3]` direction `01` CW, `02` CCW, `[5]` buttons, active-low, idle `07`. Press values alone: F1 `05`, F2 `03`, TX `06`, so bit 1 is F1, bit 2 F2, bit 0 TX. |
| PowerMate | `077D:0410` | 6 | `[0]` nonzero = down. `[1]` `int8` rotation. |
| ShuttleXpress | `0B33:0020` | 5 | `[1]` jog `u8` wrapping, `[3]` buttons active-high, bits 0-4 buttons 1-5. `[0]` ring: comment only, -7..+7. |
| ShuttlePro v2 | `0B33:0030` | 5 | `[1]` jog, `[2]\|[3]<<8` active-high, bits 0-14 buttons 1-15. |

RC-28 light: 33 bytes `00 01 <led>`, active-low, bit 0 TX, 1 F1, 2 F2, 3 LINK, `0F` all off (`HidEncoderManager.cpp:924-936`).

**The conflict to settle first:** AetherSDR's own comment says Shuttle buttons sit in bytes 2-3, Xpress reads only `[3]`, and Pro reads both. Check `kino` `shuttlepro.c` (sources.debian.org/src/kino/0.75-7/shuttlepro.c) and one other open source. Cite what you use. If they still disagree, follow the source with a named device capture, keep AetherSDR's bytes otherwise, add the disagreement to your report and to a comment in the parser. Facts only: read kino's GPL code, do not copy it. The ring byte has no upstream decoder: same rule.

**Acceptance:**
- **RC-28 parses the bits, unlike upstream, because a hold must see its release:** `07 → 05 → 07` gives f1 down, f1 up. `07 → 05 → 01 → 05 → 07` gives f1 down, f2 down, f2 up, f1 up. `07 → 06` then rotation `{01, speed 3}` while held gives tx down and +3: rotation reported with a button held, unlike upstream. A report with `[0] != 01` gives nothing. Unknown high bits are ignored.
- Rotation `{02, speed 2}` gives -2. `{00, speed 2}` gives nothing.
- **PowerMate:** `[0]` 0→1→0 gives press and release; `[1]` `0xFE` gives -2; press and rotate in one report give both events.
- **Shuttle:** the first report only seeds the jog. Then 255→0 gives +1, 0→255 gives -1. Two buttons in one report give two downs. Ring `+3` then `0` gives two `RingPosition` events. Button 15 on the Pro reports, unbound.
- `rc28LightReport(true,false,false,false)` is 33 bytes with `[2] == 0x0E`.
- **PowerMate on Linux:** the kernel's `powermate` driver takes the device, so `hidraw` isn't there (check `drivers/hid/hid-quirks.c` and `drivers/input/misc/powermate.c`, cite both). `PowerMateLinuxInput` reads its `/dev/input/event*` node matched by name `Griffin PowerMate` (or `Griffin SoundKnob`) with `QSocketNotifier`: `REL_DIAL` is rotation and `BTN_0` the press. Decode lives in a function a Mac test can run on recorded `input_event` values.
- Every device stays `testedOnRealDevice() == false`.

**Verification:** Simulated reports from documented layouts; a pass proves the code matches its sources, not that a device behaves that way. `run-test.sh tst_device_parser_rc28 tst_device_parser_powermate tst_device_parser_shuttle`. Device evidence stays pending.

**Execution note (advisory):** opus. Prerequisite Task 12.

- [ ] **Step 1:** Resolve the Shuttle sources, write the byte tests, then the parsers. Commit.

## Task 14: TMate 2 and Stream Deck+

**Requirements:** spec 5, R-HK-15, section 3 (displays and lights).

**Files:**
- Create: `src/gui/hotkeys/devices/TMate2Parser.{h,cpp}`, `StreamDeckPlusParser.{h,cpp}`, `DeviceOutput.{h,cpp}`
- Modify: `DeviceInput.cpp`, `ControllerHost.cpp` (feed displays from `HotKeyStateFeed`), `docs/attribution/AETHERSDR-PROVENANCE.md`, `docs/attribution/aethersdr-reconciliation.md`, `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_device_parser_tmate2.cpp`, `tests/tst_device_parser_streamdeck.cpp`, `tests/tst_device_output.cpp`

**Interfaces:**
- Consumes: `DeviceParser`, `HotKeyStateFeed`, `DeviceInput::sendOutput`.
- Produces:
  ```cpp
  class TMate2Parser : public DeviceParser;
  class StreamDeckPlusParser : public DeviceParser;
  namespace DeviceOutput {
      QByteArray tmate2Frame(const QString& frequency, bool connected, bool locked);
      QList<QByteArray> streamDeckKeyImage(int key, const QImage& image120);
      QList<QByteArray> streamDeckStrip(const QImage& image800x100);
  }
  ```

**Facts (`src/core/HidDeviceParser.cpp`, `src/core/HidEncoderManager.cpp` `[@1e0718ad]`):**

| Device | VID:PID | Read | Layout |
|---|---|---|---|
| TMate 2 | `1721:0614` | 64 | `[0]`=`01`. LE `u16` encoders `[1-2]`, `[3-4]`, `[5-6]`. Keys LE `u16` `[7-8]`, active-low, idle `01FF`; bits 0-5 buttons 1-6, bits 6, 7, 8 buttons 9, 10, 11. 16-bit wrap correction; encoder 0 negated; first report seeds. Printed E1 is index 2, E2 index 1. |
| Stream Deck+ | `0FD9:0084` | 14 | `[0]`=`01`. `[1]`: `00` keys `[4..11]`, buttons 1-8; `02` touch, ignored; `03` dial. Dial `[4]`=`00`: press `[5..8]`, dials 1-4. Dial `[4]`=`01`: rotation `[5..8]` `int8`. |

| Output | Bytes |
|---|---|
| TMate 2 frame | 65 bytes: `00` then a 64-byte vector, 44 used (`HidEncoderManager.cpp:744-761`). `[32]` LED: bit 0 connected, bit 1 locked; `[33-35]` RGB default 0, 50, 255; `[36]` contrast `00`; `[37]` refresh `28`; `[38-40]` speeds `01 05 0A`; `[41]` `0F`; `[42]` `19`; `[43]` `0A` (`:283-294,535-546`, `HidEncoderManager.h:93-95`). Digits: glyph and segment tables at `:32-43,48-103,109-160,299-356`, a table port with attribution. |
| Deck key | 1024-byte packets: `02 07 key isLast lenLE pageLE`, then 1016 data bytes. 120×120 JPEG, quality 90 (`:644-689`, `MainWindow_Controllers.cpp:1176-1197`). |
| Deck strip | 1024-byte packets: `02 0C xLE yLE wLE hLE isLast pageLE lenLE 00`, then 1008 data bytes. 800×100 JPEG (`:691-742`, `MainWindow_Controllers.cpp:1108`). |

**Acceptance:**
- **TMate 2:** first report seeds. Encoder 1 moving `0x0005 → 0x0007` gives +2 on `e2`. Encoder 0 `0x0005 → 0x0007` gives -2 on `main`. `0xFFFF → 0x0001` gives +2 wrap correction. Keys `01FF → 01FE` gives button1 down; two changes at once give both events, unlike upstream.
- **Stream Deck+:** `01 00 .. 01 ..` key 1 down; dial press and rotation parsed; all four dials' rotations reported from one report, not only the first; touch ignored. Keys pressed together give all downs.
- **Output:** a 5,000-byte fake JPEG splits into packets of exactly 1024 bytes with correct `isLast`, length and page fields, and reassembles byte for byte. The TMate frame is 65 bytes with the fields above, and a frequency renders with the upstream table's values for a known digit.
- **Lights and display:** the selected slice's frequency change produces one TMate frame, at most one per 100 ms. MOX on lights its Stream Deck key image, rendered on the main thread and queued to the worker as bytes. No image work on the worker.
- Keeping the Elgato link: when a plugin's `hello` names `elgato` (Task 20), `DeviceInput` releases the Stream Deck+. Until Task 20 this is a public `setDeviceClaimedElsewhere(DeviceId, bool)` with a test.
- `DeviceOutput` image functions are testable offscreen.

**Verification:** Simulated; device evidence pending. `run-test.sh tst_device_parser_tmate2 tst_device_parser_streamdeck tst_device_output`.

**Execution note (advisory):** opus. Prerequisite Task 12. `OpenTMate2Lib` is named by upstream as a Windows path only: read its licence if you reach for it, and otherwise don't.

- [ ] **Step 1:** Byte tests, parsers, output builders, state wiring. Commit.

## Task 15: FlexControl

**Requirements:** spec 5, R-HK-15, section 10 (build without SerialPort).

**Files:**
- Create: `src/gui/hotkeys/devices/FlexControlParser.{h,cpp}`, `FlexControlInput.{h,cpp}`
- Modify: `ControllerHost.cpp`, `docs/attribution/AETHERSDR-PROVENANCE.md`, `docs/attribution/aethersdr-reconciliation.md`, `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_device_parser_flexcontrol.cpp`

**Interfaces:**
- Consumes: `DeviceEvent`, `HotKeyStateFeed`.
- Produces: `class FlexControlParser { QList<DeviceEvent> feed(QByteArrayView bytes); static QByteArray auxLights(int lit); }`, `class FlexControlInput : public QObject` on the USB worker, with `deviceEvent` and `deviceStateChanged` signals shaped as `DeviceInput`'s.

**Facts (`src/core/FlexControlManager.cpp`, `.h` `[@1e0718ad]`):** `QSerialPortInfo` VID `2192`, PID `0010` (`.cpp:83-91`). 9600 8N1, no flow control, ReadWrite (`:104-111`). Split on `;`, trimmed, buffer cleared above 256 bytes (`:251-264`). Wheel `D`, `D02` to `D06`, `U`, `U02` to `U06` (`:269-281`). Buttons `X<1-4><S|C|L>`: S tap, C double, anything else hold (`:283-291`). Bare `S`, `C`, `L` are button 4, the knob, from a hardware capture (`:293-298`). `F...` is a device reset that restores lights (`:300-308`). Lights `I000;` with a `1` at position 1-3, one lit at a time, written on open, reset and change; `I000;` on close (`:237-247,312-344`). Recovery by re-detecting the ID, 2 seconds doubling to 30 (`:28-37,42-76`); timers must be parented to survive `moveToThread`. Tap versus double tap is decided in the device's firmware; no host timing.

**Direction:** the header says `D` tunes up (`.h:18`), the code emits `-accel` for `D` (`.cpp:274`), and `MainWindow_Controllers.cpp:366` passes it through. Follow the code, since it is what users have run, and cite both. The Knobs page's direction setting covers anyone it's wrong for.

**Acceptance:**
- `feed("U;")` gives +1 on `wheel`; `feed("D03;")` gives -3; `U06` +6. Partial `"U0"` then `"2;"` gives +2. 300 bytes with no `;` clear the buffer with no event.
- `X1S;` gives button1 Tap, `X2C;` button2 DoubleTap, `X3L;` button3 Hold, `S;` knob Tap, `C;` knob DoubleTap. Default bindings per section 5: button 2 tap is `tx.mox`, double tap `tx.tune`.
- `F0304;` triggers the state re-send, and gives no control event.
- `auxLights(0)` is `I000;`, `auxLights(2)` is `I010;`.
- Unplug: read error closes, emits `Not plugged in`, `releaseAll` runs, recovery attempts at 2, 4, 8 up to 30 seconds through an injected scheduler.
- Without `HAVE_SERIALPORT`: `FlexControlInput` compiles to a stub, and the device reports disabled with `This build has no serial port support.`

**Verification:** Simulated; device evidence pending. `run-test.sh tst_device_parser_flexcontrol`.

**Execution note (advisory):** opus. Prerequisite Task 12.

- [ ] **Step 1:** Parser tests, parser, input with its recovery. Commit.

## Task 16: Ulanzi Dial

**Requirements:** spec 5, 7.6, R-HK-15.

**Files:**
- Create: `src/gui/hotkeys/devices/UlanziChordDecoder.{h,cpp}` (a port with attribution), `UlanziLinuxInput.{h,cpp}`, `UlanziMacInput.{h,cpp}`, `UlanziWindowsInput.{h,cpp}`
- Modify: `ControllerHost.cpp`, `docs/attribution/AETHERSDR-PROVENANCE.md`, `docs/attribution/aethersdr-reconciliation.md`, `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_device_parser_ulanzi.cpp`

**Interfaces:**
- Consumes: `DeviceEvent`, `DeviceId::UlanziDial`, `UsbPresence`.
- Produces: `class UlanziChordDecoder { QList<DeviceEvent> feed(int linuxKeyCode, int value); void reset(); }` where value is 0 release, 1 press, 2 autorepeat; `static std::optional<int> keyCodeFromHidUsage(quint8 page, quint16 usage)`.

**Facts (`src/core/UlanziChordDecoder.cpp`, `.h` `[@1e0718ad]`):**

| Key | Linux code | HID page, usage | Result |
|---|---|---|---|
| VolumeUp, VolumeDown | 115, 114 | `0C`: `E9`, `EA` | Tune +1, -1, on values 1 and 2 |
| PreviousSong | 165 | `0C`: `B6` | `topLeft`, or decorates a chord |
| PlayPause | 164 | `0C`: `CD` | `topMiddle` |
| NextSong | 163 | `0C`: `B5` | `topRight` |
| Mute | 113 | `0C`: `E2` | `press` |
| LeftCtrl, RightCtrl | 29, 97 | `07`: `E0`, `E4` | Opens or closes the chord window |
| V, C, Y, Z | 47, 46, 21, 44 | `07`: `19`, `06`, `1C`, `1D` | `leftTop`, `leftBottom`, `rightTop`, `rightBottom` |

Defaults (`src/gui/UlanziDialMapperDialog.cpp:109-118`): as in the spec. The compound `Ctrl+Y+PreviousSong` (`.cpp:88-97`) has no control and stays unmapped. Every press is guaranteed a release (`.h:37-50`); `reset()` runs on disconnect.

Backends:
- **Linux:** `/dev/input/event*` where the sysfs name equals `Ulanzi Dial Keyboard`, `QSocketNotifier`, watcher with 250 ms debounce, `EVIOCGRAB` where a failure only warns (`EvdevEncoderManager.cpp:28-34,112-166`). `EACCES` reports the access rule on the page.
- **macOS:** `IOHIDManager` matching product `Ulanzi Dial`, `kIOHIDOptionsTypeSeizeDevice`, checking its result. It stays on the **main thread**, because it needs the main CFRunLoop (`UlanziDialMacOSManager.cpp:49-57,84-88`, `MainWindow_Controllers.cpp:2737-2743`). Its callbacks don't block, so V8's reason doesn't apply; say so in the file. It opens only after `UsbPresence` sees it. Prompts for Input Monitoring then.
- **Windows:** hidapi, product containing `Ulanzi Dial`, every interface opened, 5 ms poll (`UlanziDialWindowsManager.h:22-33`, `.cpp:86-107`). No exclusive claim, so its keys also reach the focused window. Consumer reports synthesize press plus release. **Keyboard collections are exclusive to Windows, so `Ctrl` chords may never arrive:** the Knobs page says that (Task 18) and the chord buttons are shown with that reason on Windows.

**Acceptance:**
- Decoder, a port including upstream's cases from `tests/ulanzi_chord_decoder_test.cpp`: `115 press, 115 repeat` gives two +1. `165 down, 165 up` gives `topLeft` press and release. `29 down, 21 down, 21 up, 29 up` gives `rightTop` press and release. A dropped `29 up` followed by `reset()` releases what is down.
- `keyCodeFromHidUsage(0x0C, 0xE9) == 115`, `(0x07, 0x1C) == 21`.
- Default bindings give `topLeft` `tx.mox`, `topMiddle` `rit.toggle`, `topRight` `tx.tune`, `rightTop` `slice.next`, `press` `mute`, `wheel` tune.
- **Keyboard shortcuts stay separate:** a Dial volume keystroke reaching the window on Windows (`Qt::Key_VolumeUp`) is not bound and doesn't double-tune. Test in `tst_hot_key_router` style.
- Backends compile on the Mac for macOS; Linux and Windows files are guarded; the logic they run is in the decoder.

**Verification:** Simulated; device pending. `run-test.sh tst_device_parser_ulanzi`. CI compiles the Linux and Windows backends on the controller's next push.

**Execution note (advisory):** opus. Prerequisite Task 12.

- [ ] **Step 1:** Port the decoder and its tests with attribution, then the three backends. Commit.

## Task 17: Device displays and lights wired

**Requirements:** spec 3, 9. R-HK-15.

**Files:**
- Modify: `src/gui/hotkeys/ControllerHost.{h,cpp}`, `src/gui/hotkeys/devices/FlexControlInput.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_controller_host_lights.cpp`

**Interfaces:**
- Consumes: `HotKeyStateFeed`, `DeviceOutput`, `rc28LightReport`, `FlexControlParser::auxLights`.
- Produces: `void ControllerHost::setLightsEnabled(DeviceId, bool)`, settings under `HotKeys/Device/<device>/Lights`.

**Acceptance:**
- **RC-28:** TX light follows transmitting, as `A/CHANGELOG.md:1523` says; F1 and F2 lights written after a release, not mid-hold (`MainWindow_Controllers.cpp:2596-2600 [@1e0718ad]`); LINK follows connected.
- **FlexControl:** aux 1 to 3 follow the state of the action bound to button 1 to 3 tap, one lit at a time, written on open, device reset and change; `I000;` on close.
- **TMate 2 and Stream Deck+:** as Task 14, driven only from the state feed; a change on a slice that isn't the window's slice changes nothing.
- Disabling lights writes the off state once and stops updates. A device that isn't connected costs nothing: no image rendered, no report queued.
- No more than one output per device per 100 ms under a burst of slice changes; the last state always arrives.

**Verification:** Unit with a recording fake `sendOutput` and an injected clock. `run-test.sh tst_controller_host_lights`. Device pending.

**Execution note (advisory):** opus. Prerequisites Tasks 13 to 15.

- [ ] **Step 1:** Wire the feed to each device with the test. Commit.

## Task 18: Knobs Setup page

**Requirements:** spec 9 (Knobs), 7.6 (what the page says), 10. R-HK-17, R-HK-19.

**Files:**
- Create: `src/gui/setup/KnobsSetupPage.{h,cpp}`
- Modify: `src/gui/SetupDialog.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_hot_key_setup_knobs.cpp`

**Interfaces:**
- Consumes: `ControllerHost`, `HotKeyDeviceBindings`, `HotKeyActionList`.
- Produces: `class KnobsSetupPage : public SetupPage`.

**Acceptance:**
- As in the mockup: the eight devices with connection state and `Untested`; a drawing of the selected device; a row per control and gesture with its action and Acts on; speed; direction; the display or light settings where the device has them.
- **Every one of the eight always shows `Untested`,** and no simulator or test can change it (R-HK-19).
- States: Not plugged in, Connected, and a plain fix: "Another app has this device." with the app to quit; on Linux the access rule, stated; FlexControl needing `dialout`.
- The Ulanzi row states, on Windows, that its keys also reach the focused window and its Ctrl chords may not arrive; on Linux, its access rule (spec 7.6). macOS: the Input Monitoring note appears only with that device.
- A build without hidapi or SerialPort: the devices stay listed and disabled with the reason.
- A stay-on setting appears on the RC-28 TX bar row. Changing an action writes through `HotKeyDeviceBindings`.
- `ThisComputer` scope. Works on a remote window: a page statement that knobs plug into this computer.

**Verification:** UI per `ui-verification`. `run-test.sh tst_hot_key_setup_knobs`, capture `dialog-setup-knobs*` as in Task 8, report the image paths.

**Execution note (advisory):** opus. Prerequisite Task 17.

- [ ] **Step 1:** Build the page against the mockup with its test. Commit.

## Task 19: Deck link

**Requirements:** spec 7.5, V10, 10 (socket in use), the plan's correction on discovery. R-HK-16.

**Files:**
- Create: `src/gui/hotkeys/DeckLink.{h,cpp}`
- Modify: `src/gui/MainWindow.cpp`, `src/gui/hotkeys/ControllerHost.cpp` (`setDeviceClaimedElsewhere`), `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_deck_link.cpp`

**Interfaces:**
- Consumes: `HotKeyDispatcher`, `HotKeyActionList`, `HotKeyStateFeed`.
- Produces:
  ```cpp
  class DeckLink : public QObject {
      Q_OBJECT
  public:
      static constexpr int kProtocolVersion = 1;
      static constexpr qsizetype kMaxLineBytes = 65536;
      DeckLink(HotKeyDispatcher* dispatcher, HotKeyStateFeed* feed, QObject* parent = nullptr);
      bool setEnabled(bool on);               // HotKeys/DeckLink/Enabled
      QString lastError() const;              // for the page
      QString socketPath() const;
      static QString deckFolder();            // <home>/.nereussdr/deck
      QList<QString> connectedPlugins() const;
  signals:
      void pluginsChanged();
      void elgatoConnectionChanged(bool connected);
  };
  ```

**The connection:**
- **Name:** `h` = first 12 hex characters of SHA-256 of `AppSettings::instance().filePath()`. Unix: `<home>/.nereussdr/deck/<h>.sock`, folder mode 0700. Windows: pipe `nereussdr-deck-<h>`. `<home>` is `QDir::homePath()`, so test homes hold and a Flatpak host with `--filesystem=home` reaches it. Not `/tmp` and not the abstract namespace, which has no permissions.
- **Discovery:** `<home>/.nereussdr/deck/<h>.json`, written atomically on listen and removed on close: `{"protocol":1,"path":"<socket or \\\\.\\pipe\\name>","pid":<pid>,"profile":"<profile name>"}`. A plugin reads every `*.json` there, newest first, and uses the first that answers.
- **Listening:** `QLocalServer::UserAccessOption`, following `StationControlSocket::listen` (`src/core/daemon/StationControlSocket.cpp:194-224`): probe a live answer first, which is "in use"; otherwise `removeServer` a stale file. On Windows, where two servers can share a pipe and `listen` won't report it, the probe is the only test. A path over 103 bytes reports its own error.

**Messages,** one JSON object per line, field `type`:

| Direction | Message |
|---|---|
| Plugin | `{"type":"hello","plugin":"elgato","version":"1.0.0"}` |
| Window | `{"type":"hello","protocol":1,"actions":[{"id","name","group","gesture","transmits"}...]}` |
| Plugin | `{"type":"press","id":"mute","pin":"B"}`, `{"type":"release","id":"tx.hold"}`, `{"type":"turn","idUp":"tune.up","idDown":"tune.down","steps":-2}` (`pin` optional) |
| Window | `{"type":"state","id":"tx.mox","on":true}` |
| Window | `{"type":"display","frequency":"14.200 000","mode":"USB","filter":"2.7k","slice":"A"}` |

**Acceptance:**
- Real `QLocalSocket` client in the test (R-HK-16): `hello` gets the action list holding every id; a state change arrives as `state`; the display arrives after `hello` and on change.
- `press` reaches the dispatcher with source `{Deck, <connection id>}` and its pin; `turn` the same. An action before `hello`, an unknown id, a bad pin or a malformed line: the line is ignored with a `qCWarning`, and the connection stays. A line over `kMaxLineBytes` closes that connection. Partial lines buffer across reads.
- **Disconnect releases every hold it had:** `press tx.hold`, drop the socket: `keyHold(false)` is called once and another source's hold stays.
- **No network:** after listening, no TCP or UDP listener exists from this class: the class uses only `QLocalServer`, and a grep test on its source guards it. Unix: socket mode has no group or other bits, and the folder is 0700.
- **Other users refused:** tested through the mode bits on the Mac and Linux; the Windows path uses `UserAccessOption`.
- Off switch: `setEnabled(false)` closes and removes the socket and discovery file, drops connections and releases their holds. Global `HotKeys/Enabled` false: messages reach nothing, since the dispatcher refuses.
- **In use:** a second `DeckLink` for the same file reports `lastError()`, leaves the first alone, and keys and knobs keep working. A stale socket file is replaced.
- A `hello` naming `elgato` emits `elgatoConnectionChanged(true)`, and `ControllerHost` closes its direct Stream Deck+; disconnect reverses it.

**Verification:** Networking-adjacent, consequential input: tests first, unit and integration on this Mac. `run-test.sh tst_deck_link`. Windows named pipes checked by CI compile only; report that.

**Execution note (advisory):** opus. Prerequisite Task 12. A new connection type, approved by JJ: flagged for an earlier look.

- [ ] **Step 1:** Write the socket tests, then the link. Commit.

## Task 20: Plugins

**Requirements:** spec V10, 7.5, 9. R-HK-16 (I), R-HK-19.

**Files:**
- Create: `plugins/README.md`, `plugins/common/actions.json`, `plugins/tools/gen-manifests.py`, `plugins/elgato-nereussdr/com.nereussdr.radio.sdPlugin/{manifest.json,plugin.js,link.js,imgs/,LICENSE-NOTE.md,node_modules/ws/...}`, `plugins/ulanzi-nereussdr/com.nereussdr.radio.ulanziPlugin/{manifest.json,plugin/app.js,plugin/link.js,assets/,libs/,en.json,LICENSE-NOTE.md}`, `plugins/streamcontroller-nereussdr/{manifest.json,main.py,link.py,LICENSE-NOTE.md,actions/...}`
- Modify: `CMakeLists.txt` (zip the Elgato folder to `NereusSDR.streamDeckPlugin` and carry all three into the app's install tree on each OS), `docs/attribution/AETHERSDR-PROVENANCE.md` (a study note: what was studied, nothing copied), `packaging/third-party-licenses/README.md` (`ws`, Ulanzi SDK), `tests/CMakeLists.txt`
- Test: `tests/tst_deck_plugin_actions.cpp`, `tests/tst_deck_plugin_clients.cpp`, `plugins/elgato-nereussdr/link.test.js`, `plugins/streamcontroller-nereussdr/test_link.py`

**Interfaces:**
- Consumes: the wire protocol and discovery file of Task 19. `HotKeyActionList`.
- Produces: `plugins/common/actions.json` (id, name, group, gesture, transmits), plus the three plugins.

**How each one works:**

| | Elgato | Ulanzi | StreamController |
|---|---|---|---|
| Runtime | Node 20 (`Nodejs` in the manifest), on the host's WebSocket from `-port`, `-pluginUUID`, `-registerEvent` | Node 20.12.2, `Type: JavaScript`, `CodePath: plugin/app.js`. Must be `.js`, because an `.html` code path can't use `net`. | Python in the app's process, `PluginBase`, `ActionHolder`, `Input` |
| Link | `net.connect({path})` | `net.connect({path})` | `socket.socket(AF_UNIX)` on its own thread, Linux only |
| Actions | Static in the manifest, generated from `actions.json` | Same | Registered at load from `actions.json` |
| Input | `keyDown`, `keyUp`, `dialRotate.ticks`, `dialDown` | `onKeyDown`, `onKeyUp`, `onDialRotateLeft`, `Right`, `onDialDown`, `Up` | `Input.Key.Events.DOWN`, `UP`, `Input.Dial.Events.TURN_CW`, `CCW` |
| Host gone | Exits on host close | **Must exit on host close**, unlike upstream's log, so the socket drops and holds release | In-process |
| Licence | GPL-3 note; `ws` MIT, from `A/plugins/elgato-aethersdr/.../node_modules/ws` with its licence | GPL-3 note; Ulanzi SDK Apache-2.0, from `A/plugins/ulanzi-aethersdr/.../libs` with its licence | GPL-3 note |

Study these for host wiring, not code: `A/plugins/elgato-aethersdr/com.aethersdr.radio.sdPlugin/plugin.js:9-16,916-919,957-985`, `A/plugins/ulanzi-aethersdr/.../plugin/app.js:284,315,378-389`, `A/plugins/streamcontroller-aethersdr/main.py:18-21`, `tci_client.py:43-50`. **Write your own code; V10 says study, not copy.** The third-party libraries they vendor keep their own licences and are the only files copied. A host's `Slice` setting per action sends `pin`.

**Acceptance:**
- `python3 plugins/tools/gen-manifests.py --check` passes: Elgato's and Ulanzi's manifest action lists match `actions.json`. `tst_deck_plugin_actions` compares `actions.json` to `HotKeyActionList::all()`, so a changed list fails a test until regenerated.
- `link.js` and `link.py`, each tested against a stub server: reads the discovery folder newest first and skips files that don't answer; buffers partial lines; retries every 2 seconds; sends a `hello`; converts key down, key up and dial ticks to `press`, `release`, `turn`; surfaces `state` and `display`.
- `tst_deck_plugin_clients` (integration, R-HK-16 "I"): a real `DeckLink` with a scratch home, then `node plugins/elgato-nereussdr/link.test.js --integration <home>` as a child process presses and releases `tx.hold` and turns `tune.up`; the dispatcher sees them; killing the process releases the hold. Skips with a clear message if `node` isn't on the path. Same with `python3`. Node 24 is on this Mac; the manifest says 20, so don't use newer APIs.
- Each plugin folder has `LICENSE-NOTE.md`. None of them calls TCI. Their manifests say `Untested` in their descriptions (R-HK-19).
- The Elgato folder zips to a `.streamDeckPlugin` with the `.sdPlugin` folder at its root. `unzip -l` in the build shows it.
- `python3 scripts/check-third-party-licenses.py` passes. `scripts/check-new-ports.py` passes.

**Verification:** Clients checked against the real socket on this Mac; no plugin host has run them. Plugin hosts' sandboxes aren't documented, so all three stay marked untested and this limit goes in the report. `run-test.sh tst_deck_plugin_actions tst_deck_plugin_clients`, plus `node plugins/elgato-nereussdr/link.test.js` and `python3 plugins/streamcontroller-nereussdr/test_link.py`.

**Execution note (advisory):** opus. Prerequisite Task 19.

- [ ] **Step 1:** Generate the action list and manifests. Write `link.js`, `link.py` and their tests. Write each host's shell. Commit.

## Task 21: Deck Plugins Setup page

**Requirements:** spec 9 (Deck Plugins), 10. R-HK-17, R-HK-19.

**Files:**
- Create: `src/gui/setup/DeckPluginsSetupPage.{h,cpp}`
- Modify: `src/gui/SetupDialog.cpp`, `tests/CMakeLists.txt`
- Test: `tests/tst_hot_key_setup_deck.cpp`

**Interfaces:**
- Consumes: `DeckLink`, the plugin files installed by Task 20.
- Produces: `class DeckPluginsSetupPage : public SetupPage`.

**Acceptance:**
- As in the mockup: `Let deck plugins connect`, and three plugin rows each with Install, connection state and `Untested`.
- **Install, Elgato (macOS, Windows):** opens the `.streamDeckPlugin` through the OS with `QDesktopServices::openUrl`, handing it to Elgato's app. Linux: disabled, `Elgato's app isn't on Linux.`
- **Install, Ulanzi (macOS, Windows):** copies to `~/Library/Application Support/Ulanzi/UlanziDeck/Plugins/<name>` or `%APPDATA%\Ulanzi\UlanziDeck\Plugins\<name>` (`A/plugins/ulanzi-aethersdr/README.md:14-16`), then says to quit and reopen Ulanzi Studio. Linux: disabled with its reason.
- **Install, StreamController (Linux):** copies to `~/.var/app/com.core447.StreamController/data/plugins/com_nereussdr_radio` (`globals.py`, as the scout reported); underscores required. macOS and Windows: disabled with its reason. An app that isn't there: the plain reason, and no folders made.
- Install logic takes its destination as a parameter, so tests use a scratch folder. No real plugin folder is written by a test.
- "Socket in use" shows `lastError()` in plain words. Keys and knobs stay usable. Connected plugins show their names.
- The switch writes `HotKeys/DeckLink/Enabled`. `ThisComputer` scope, and the page says a remote Core changes nothing here.

**Verification:** UI per `ui-verification`. `run-test.sh tst_hot_key_setup_deck`, capture `dialog-setup-deck-plugins*` as in Task 8, report the image paths.

**Execution note (advisory):** opus. Prerequisite Task 20.

- [ ] **Step 1:** Build the page with the install logic and tests. Commit.

## Task 22: Whole-feature checks

**Requirements:** every R-HK row in spec 11. R-HK-19 statement.

**Files:**
- Modify: `docs/architecture/2026-11-05-hot-keys-design.md` (status line), `README.md` (one feature line for hot keys, knobs and MIDI; state they are untested on real devices), and any page a failing check points to
- Test: none new unless a gap appears

**Interfaces:**
- Consumes: everything.
- Produces: `docs/architecture/2026-11-05-hot-keys-evidence.md`, one row per R-HK id with its test name, layer and state (passed, pending, failed) and what stays unproven.

**Acceptance:**
- Every R-HK row names a test that exists and passes, or a pending hardware state. Nothing says tested on a device.
- `tst_core_has_no_gui_includes`, `tst_unbuilt_features`, `tst_remote_gui_gating`, `tst_settings_scope` and every test this plan added pass together on the finished revision.
- Repo gates pass: `python3 scripts/verify-thetis-headers.py`, `verify-inline-tag-preservation.py`, `verify-inline-cites.py`, `check-new-ports.py`, `verify-provenance-sync.py`, `check-third-party-licenses.py`, `compliance-inventory.py --fail-on-unclassified`, `verify-test-registration.py`, `generate-contributor-indexes.py --check`, `render-link-tables.py --check`, `verify-no-gui-dsp-access.py`.
- Captures of the four pages read by the controller next to the mockup, with geometry unchanged on any page this plan didn't touch: `python3 tests/visual/capture.py --out <scratch> --scenes 'dialog-setup-*' --profiles 'mac-1512x869@2' --jobs 4`.
- `nereusd` builds, and its link list holds no RtMidi or hidapi.

**Verification:** Combined final checks once, on the finished revision. Single tests by the lane script; the affected subsystem by `cmake --build build --target tests_core` and `ctest --test-dir build -L core`, and the same for `gui`. Hosted CI runs on the controller's push and covers Windows and Linux compile.

**Execution note (advisory):** sonnet for the evidence document, opus if a check fails. Prerequisites: all tasks. The whole-branch review follows this, by crew's rule.

- [ ] **Step 1:** Write the evidence table from what actually ran, then run the combined checks once. Commit.
