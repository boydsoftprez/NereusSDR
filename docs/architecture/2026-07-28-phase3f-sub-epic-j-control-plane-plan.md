# Phase 3F Sub-Epic J: per-slice control plane + anti-VOX mix, implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every DSP control and readout address the slice the operator
is working, and make anti-VOX hear every slice instead of only slice A.

**Architecture:** The per-slice pipeline already exists and is correct
(`SliceModel` property → `RadioModel` push → `rxChannel(slice->sliceIndex())`).
Almost every task here removes a control that goes *around* that pipeline,
rather than building new pipeline. The one genuinely new component is a
second `MasterMixer` instance for anti-VOX, whose membership mirrors the
speakers mixer.

**Tech Stack:** C++20, Qt6, QtTest, WDSP, CMake + Ninja.

**Spec:** `docs/architecture/2026-07-28-phase3f-sub-epic-j-control-plane-design.md`

## Global Constraints

- **GPG-sign every commit.** Never `--no-gpg-sign`.
- **No em-dashes** in any authored text, including commit messages and code
  comments. Use periods, colons, semicolons, parentheses or commas.
- **Source-first.** Any ported logic needs a `// From Thetis <file>:<line>
  [v2.10.3.15]` cite, and upstream inline author tags preserved verbatim.
  Upstream reference is Thetis `v2.10.3.15` (`3759d096`).
- **Never hold a mutex on the audio thread.** `MasterMixer::accumulate` and
  `tryDrain` run there; `setSliceStreaming` and friends take
  `m_sliceMapMutex` and are control-thread only.
- **`AppSettings`, never `QSettings`.** Booleans persist as `"True"` /
  `"False"` strings.
- **Targeted tests during iteration; the full suite once at the end.** Test
  binaries are `EXCLUDE_FROM_ALL`: `cmake --build build --target all_tests`
  before `ctest`, or `ctest` silently runs stale binaries.
- **Build a single test:** `cmake --build build --target <tst_name> -j8`,
  then `./build/tests/<tst_name>`.

## Pre-flight: what is ALREADY correct

Verified 2026-07-28 by direct read. Do not rebuild any of it.

| Already correct | Evidence |
| --- | --- |
| Per-slice demodulation, one WDSP channel each | Sub-Epic I data plane |
| Per-slice frequency and shift | `SliceStreamAllocator::retuneSlice` |
| Model to WDSP push resolves the right channel | `RadioModel.cpp:8859` |
| NR, all seven slots | `VfoWidget.cpp:1542` writes `m_slice->setActiveNr()` |
| SNB, APF | `MainWindow.cpp:1214-1219` capture `slice` |
| NB mode per slice | `SliceModel::nbMode` |
| **Stereo pan, end to end** | flag slider → `SliceModel::audioPan` → `rxChannel(sliceIndex)->setAudioPan()` (`MainWindow.cpp:1244`, `RadioModel.cpp:8991`) |

The spec's J7 proposed building stereo pan through `MasterMixer`. It is
already built through WDSP's per-channel panel pan, which is the better
route. **J7 reduces to a regression test (Task 7).**

## File structure

| File | Responsibility in this epic |
| --- | --- |
| `src/models/SliceModel.{h,cpp}` | Gains `anfEnabled` (Task 1) |
| `src/models/RadioModel.cpp` | Pushes `anfEnabled` (T1); NB stream mirror (T6); CTUN centre re-shift (T5) |
| `src/gui/MainWindow.cpp` | Removes the bypasses (T1, T2, T3, T4) |
| `src/core/audio/MasterMixer.{h,cpp}` | Per-instance slew length (T8) |
| `src/core/AudioEngine.{h,cpp}` | Anti-VOX mixer instance + drain (T9) |
| `src/models/RxDspWorker.cpp` | Anti-VOX feed becomes all-slice (T9) |
| `src/core/TciProtocol.cpp` | Per-slice `rx_volume` (T10) |
| `scripts/verify-no-gui-dsp-access.py` | The ban (T11) |

---

### Task 1: ANF becomes a per-slice property

**Files:**
- Modify: `src/models/SliceModel.h` (near the `snbEnabled` declarations at
  :331, :698, :939, :1088)
- Modify: `src/models/SliceModel.cpp` (setter near :1130, save near :1710,
  restore near :1953)
- Modify: `src/models/RadioModel.cpp` (push, beside the `activeNrChanged`
  handler at :8859)
- Modify: `src/gui/MainWindow.cpp:1206-1208`
- Test: `tests/tst_slice_model_phase3f_properties.cpp`

**Interfaces:**
- Produces: `SliceModel::anfEnabled() const -> bool`,
  `SliceModel::setAnfEnabled(bool)`, `SliceModel::anfEnabledChanged(bool)`.
  Persisted key suffix `AnfEnabled`. Task 3 consumes the setter.

- [ ] **Step 1: Write the failing test**

Add to `tests/tst_slice_model_phase3f_properties.cpp`:

```cpp
    // ANF was the one RXA setting with no home on the slice, which is why
    // MainWindow routed it to rxChannel(0) while its neighbours SNB and APF
    // went through SliceModel. Give it the same shape they have.
    void anf_enabled_defaults_off_and_round_trips()
    {
        SliceModel slice;
        QCOMPARE(slice.anfEnabled(), false);

        QSignalSpy spy(&slice, &SliceModel::anfEnabledChanged);
        slice.setAnfEnabled(true);
        QCOMPARE(slice.anfEnabled(), true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }

    void anf_enabled_setter_is_idempotent()
    {
        SliceModel slice;
        slice.setAnfEnabled(true);
        QSignalSpy spy(&slice, &SliceModel::anfEnabledChanged);
        slice.setAnfEnabled(true);
        QCOMPARE(spy.count(), 0);
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target tst_slice_model_phase3f_properties -j8`
Expected: FAIL to compile, "no member named 'anfEnabled' in 'NereusSDR::SliceModel'"

- [ ] **Step 3: Add the property**

In `SliceModel.h`, beside the `snbEnabled` line at :331:

```cpp
    Q_PROPERTY(bool   anfEnabled      READ anfEnabled      WRITE setAnfEnabled      NOTIFY anfEnabledChanged)
```

Accessor beside :698:

```cpp
    bool   anfEnabled()      const { return m_anfEnabled; }
```

Declare the setter beside the other setters, the signal beside :939:

```cpp
    void anfEnabledChanged(bool v);
```

Member beside :1088. Note the surrounding `m_snbEnabled` line uses an
em-dash in its comment; that is pre-existing and this new line must not
copy it (see Global Constraints):

```cpp
    bool   m_anfEnabled{false};       // Neutral default, feature off at start
```

In `SliceModel.cpp`, beside `setSnbEnabled` at :1130:

```cpp
void SliceModel::setAnfEnabled(bool v)
{
    if (m_anfEnabled != v) {
        m_anfEnabled = v;
        emit anfEnabledChanged(v);
    }
}
```

Persistence, beside the `SnbEnabled` save at :1710:

```cpp
    s.setValue(sp + QStringLiteral("AnfEnabled"), boolStr(m_anfEnabled));
```

and the restore at :1953:

```cpp
    if (s.contains(sp + QStringLiteral("AnfEnabled"))) {
        setAnfEnabled(s.value(sp + QStringLiteral("AnfEnabled")).toString() == QLatin1String("True"));
    }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target tst_slice_model_phase3f_properties -j8 && ./build/tests/tst_slice_model_phase3f_properties`
Expected: PASS, all cases

- [ ] **Step 5: Wire the model push**

In `RadioModel.cpp`, immediately after the `activeNrChanged` handler
(`:8859-8863`), matching its shape exactly:

```cpp
    // ANF is per-slice: it lives in RXA, one instance per WDSP channel.
    // Same shape as activeNrChanged above.
    connect(slice, &SliceModel::anfEnabledChanged, this, [this, slice](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAnfEnabled(on);
        }
        scheduleSettingsSave();
    });
```

- [ ] **Step 6: Route the flag through the model**

In `MainWindow.cpp`, replace `:1206-1208` so it matches its neighbours at
:1214-1219:

```cpp
    connect(newFlag, &VfoWidget::anfChanged, this, [slice](bool on) {
        slice->setAnfEnabled(on);
    });
```

- [ ] **Step 7: Verify the whole suite for these files still passes**

Run: `cmake --build build --target tst_slice_model_phase3f_properties -j8 && ./build/tests/tst_slice_model_phase3f_properties`
Expected: PASS

- [ ] **Step 8: Commit**

```bash
git add src/models/SliceModel.h src/models/SliceModel.cpp src/models/RadioModel.cpp src/gui/MainWindow.cpp tests/tst_slice_model_phase3f_properties.cpp
git commit -m "fix(3f): ANF acts on its own slice, not always slice A

ANF was the one RXA setting with no SliceModel property, so MainWindow
routed the flag straight to rxChannel(0) while its immediate neighbours
snbChanged and apfChanged captured the slice and went through the model.
ANF on slice B toggled slice A.

Adds SliceModel::anfEnabled with the same shape as snbEnabled, pushes it
per slice beside the activeNrChanged handler, and routes the flag through
it. Per-band per-slice persistence follows the existing key convention."
```

---

### Task 2: delete the dead NR handler

**Files:**
- Modify: `src/gui/MainWindow.cpp:1203-1205`
- Modify: `src/gui/widgets/VfoWidget.h` (remove `nrChanged` declaration)

**Interfaces:**
- Consumes: nothing. Produces: nothing. Pure deletion.

- [ ] **Step 1: Prove the signal is dead**

Run: `grep -rn 'emit nrChanged' src/`
Expected: no output. The NR bank writes `m_slice->setActiveNr(slot)`
directly (`VfoWidget.cpp:1542`), so `nrChanged` is never emitted.

- [ ] **Step 2: Delete the handler**

Remove from `MainWindow.cpp:1203-1205`:

```cpp
    connect(newFlag, &VfoWidget::nrChanged, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) { rxCh->setNrEnabled(on); }
    });
```

- [ ] **Step 3: Delete the signal declaration**

Remove `void nrChanged(bool enabled);` from `VfoWidget.h:490`.

- [ ] **Step 4: Verify nothing referenced it**

Run: `grep -rn 'nrChanged' src/ tests/`
Expected: no output.

- [ ] **Step 5: Build**

Run: `cmake --build build -j8`
Expected: builds clean.

- [ ] **Step 6: Commit**

```bash
git add src/gui/MainWindow.cpp src/gui/widgets/VfoWidget.h
git commit -m "refactor(3f): delete the dead nrChanged handler

Nothing has ever emitted VfoWidget::nrChanged. The NR bank writes
m_slice->setActiveNr(slot) directly with mutual exclusion, so the
MainWindow handler that caught it and wrote rxChannel(0)->setNrEnabled
could never fire.

Harmless at runtime, but it is the reason a first reading of the
multi-slice control problem concluded NR was broken when NR is in fact
already per-slice and correct."
```

---

### Task 3: DSP menu ANF follows the active slice

**Files:**
- Modify: `src/gui/MainWindow.cpp:5162-5168`
- Test: `tests/tst_dsp_menu_active_slice.cpp` (create)
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SliceModel::setAnfEnabled(bool)` from Task 1,
  `RadioModel::activeSlice() -> SliceModel*`,
  `RadioModel::activeSliceChanged(int)`.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_dsp_menu_active_slice.cpp`:

```cpp
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic J Task 3. The DSP menu is not attached to any flag, so
// with two receivers running nothing said which one it meant. It used to
// write rxChannel(0) unconditionally. The rule is that a control attached
// to no slice targets the active slice.

#include <QtTest/QtTest>
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TstDspMenuActiveSlice : public QObject {
    Q_OBJECT
private slots:
    void anf_from_a_detached_control_targets_the_active_slice()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice(QStringLiteral("pan-0"));
        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        SliceModel* sb = radio.sliceById(b);
        QVERIFY(sa != nullptr);
        QVERIFY(sb != nullptr);

        // Slice A is active on creation.
        QVERIFY(radio.activeSlice() == sa);

        // Simulate what the menu action does: resolve active, then set.
        if (SliceModel* target = radio.activeSlice()) {
            target->setAnfEnabled(true);
        }
        QCOMPARE(sa->anfEnabled(), true);
        QCOMPARE(sb->anfEnabled(), false);

        // Operator clicks B's flag. The menu must follow.
        radio.setActiveSlice(b);
        QVERIFY(radio.activeSlice() == sb);

        if (SliceModel* target = radio.activeSlice()) {
            target->setAnfEnabled(true);
        }
        QCOMPARE(sb->anfEnabled(), true);
    }
};

QTEST_MAIN(TstDspMenuActiveSlice)
#include "tst_dsp_menu_active_slice.moc"
```

Note: confirm the exact name of the active-slice setter before running
(`grep -n 'setActiveSlice\|void.*[Aa]ctiveSlice' src/models/RadioModel.h`).
If it differs, use the real one; do not add a new setter for the test.

- [ ] **Step 2: Register and run it to verify it fails**

Add to `tests/CMakeLists.txt` beside the other Phase 3F registrations:

```cmake
# Phase 3F Sub-Epic J Task 3: a control attached to no slice targets the
# active slice. The DSP menu used to write rxChannel(0) unconditionally.
nereus_add_test(tst_dsp_menu_active_slice)
```

Run: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON && cmake --build build --target tst_dsp_menu_active_slice -j8`
Expected: FAIL to compile, `anfEnabled` missing if Task 1 is not merged, or
FAIL on the assertion if the active-slice accessor names differ.

- [ ] **Step 3: Make the menu resolve the active slice**

Replace `MainWindow.cpp:5165-5168`:

```cpp
        connect(anfAction, &QAction::toggled, this, [this](bool on) {
            // A control attached to no flag targets the active slice, which
            // is whichever flag the operator last clicked. Resolved at
            // invocation, not captured, so it follows focus.
            if (SliceModel* slice = m_radioModel->activeSlice()) {
                slice->setAnfEnabled(on);
            }
        });
```

- [ ] **Step 4: Keep the menu's check state honest**

The action is checkable, so it must show the active slice's state rather
than a stale one. Add beside the action's creation:

```cpp
        // Reflect the active slice when focus moves, without re-emitting
        // toggled back into the handler above.
        connect(m_radioModel, &RadioModel::activeSliceChanged, this,
                [this, anfAction](int) {
            if (SliceModel* slice = m_radioModel->activeSlice()) {
                QSignalBlocker block(anfAction);
                anfAction->setChecked(slice->anfEnabled());
            }
        });
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build --target tst_dsp_menu_active_slice -j8 && ./build/tests/tst_dsp_menu_active_slice`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/gui/MainWindow.cpp tests/tst_dsp_menu_active_slice.cpp tests/CMakeLists.txt
git commit -m "fix(3f): DSP menu ANF follows the active slice

The DSP menu is attached to no flag, so with two receivers running
nothing said which one it meant, and it wrote rxChannel(0). It now
resolves activeSlice() at invocation and follows focus, and its check
state re-syncs on activeSliceChanged behind a QSignalBlocker so the
refresh does not feed back into the handler."
```

---

### Task 4: container S-meter follows the active slice

**Files:**
- Modify: `src/gui/MainWindow.cpp:3612-3620`
- Test: extend `tests/tst_dsp_menu_active_slice.cpp` or create
  `tests/tst_meter_poller_active_slice.cpp`

**Interfaces:**
- Consumes: `MeterPoller::setRxChannel(RxChannel*)`,
  `RadioModel::activeSliceChanged(int)`.

- [ ] **Step 1: Establish what the visible meter actually reads**

Before writing code, run:

```bash
grep -n 'setRxChannel\|m_wdspEngine' src/gui/meters/MeterPoller.cpp | head -20
grep -rn 'RXA_S_AV\|GetDetectMaxBin\|setLevel' src/gui/meters/SMeterWidget.cpp | head -10
```

`MeterPoller::setRxChannel` binds one channel, and `MeterPoller.cpp:367`
already resolves `rxChannel(sliceId)` for its per-slice path. `SMeterWidget`
carries four WDSP meter sources of its own. Determine which of the two the
container S-meter reads, and re-bind that one. Record the finding in the
commit message.

- [ ] **Step 2: Write the failing test**

```cpp
    // The container S-meter is attached to no flag. It must show the
    // receiver the operator is working, not always slice A.
    void the_container_meter_rebinds_when_focus_moves()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        const int a = radio.addSlice(QStringLiteral("pan-0"));
        const int b = radio.addSlice(QStringLiteral("pan-0"));

        QSignalSpy spy(&radio, &RadioModel::activeSliceChanged);
        radio.setActiveSlice(b);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(radio.activeSlice()->sliceIndex(), b);
        QVERIFY(radio.activeSlice()->sliceIndex() != a);
    }
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cmake --build build --target tst_dsp_menu_active_slice -j8 && ./build/tests/tst_dsp_menu_active_slice`
Expected: FAIL if `activeSliceChanged` does not fire on the setter, which
tells you the re-bind has nothing to hang off and the signal needs checking
first.

- [ ] **Step 4: Re-bind on focus change**

At `MainWindow.cpp:3612`, keep the existing WDSP-init binding as the seed,
then add:

```cpp
    // The container meter is attached to no flag, so it shows the active
    // slice. Re-bound on focus change; the WDSP-init binding above is only
    // the seed for the first slice.
    connect(m_radioModel, &RadioModel::activeSliceChanged, this, [this](int) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice || !m_radioModel->wdspEngine()) { return; }
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(slice->sliceIndex());
        if (rxCh) { m_meterPoller->setRxChannel(rxCh); }
    });
```

- [ ] **Step 5: Run test to verify it passes**

Run: `./build/tests/tst_dsp_menu_active_slice`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/gui/MainWindow.cpp tests/
git commit -m "fix(3f): container S-meter follows the active slice

MeterPoller was bound to rxChannel(0) once at WDSP init and never moved,
so the container meter showed slice A whatever the operator was working.
Re-bound on activeSliceChanged. Per-flag S-meters already follow their own
slice and are untouched."
```

---

### Task 5: a DDC centre move re-shifts every co-hosted slice

**Files:**
- Modify: `src/models/RadioModel.cpp` (add `reshiftSlicesOnStream`)
- Modify: `src/models/RadioModel.h` (declare it)
- Modify: `src/gui/MainWindow.cpp:7286-7296` and `:7300-7306`
- Test: `tests/tst_radio_model_slice_lifecycle.cpp`

**Interfaces:**
- Consumes: `RadioModel::slicesOnStream(int) -> QVector<int>`,
  `SliceModel::streamIndex()`, `SliceModel::frequency()`,
  `SliceModel::setShiftOffsetHz(double)`.
- Produces: `RadioModel::reshiftSlicesOnStream(int streamIndex, double
  newCentreHz)`. No return.

- [ ] **Step 1: Write the failing test**

Add to `tests/tst_radio_model_slice_lifecycle.cpp`:

```cpp
    // A shared DDC window has ONE centre. When CTUN moves it, every slice on
    // that stream needs its shift recomputed as (frequency - newCentre), not
    // just the slice that dragged. Otherwise the co-host keeps a stale shift
    // and demodulates the wrong signal, which is the same hazard family as
    // the 2026-07-27 CTUN stranding fix.
    void movingTheDdcCentreReshiftsEveryCoHostedSlice()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        QVERIFY(sa != nullptr);
        sa->setFrequency(7'240'000.0);

        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sb = radio.sliceById(b);
        QVERIFY(sb != nullptr);
        sb->setFrequency(7'245'000.0);

        // Both must be on the same stream for this test to mean anything.
        QCOMPARE(sa->streamIndex(), sb->streamIndex());
        const int stream = sa->streamIndex();

        const double newCentre = 7'250'000.0;
        radio.reshiftSlicesOnStream(stream, newCentre);

        QVERIFY(qFuzzyCompare(sa->shiftOffsetHz(), 7'240'000.0 - newCentre));
        QVERIFY(qFuzzyCompare(sb->shiftOffsetHz(), 7'245'000.0 - newCentre));
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target tst_radio_model_slice_lifecycle -j8`
Expected: FAIL to compile, "no member named 'reshiftSlicesOnStream'"

- [ ] **Step 3: Implement it**

Declare in `RadioModel.h` beside `slicesOnStream` at :508:

```cpp
    /// Recompute every slice's shift oscillator against a new stream centre.
    ///
    /// A shared DDC window has one centre and N slices sitting at their own
    /// offsets inside it. When the centre moves (a CTUN drag, a band jump),
    /// each member's shift is (frequency - newCentreHz). Missing a co-host
    /// leaves it demodulating the wrong signal while its flag still reads
    /// the right number.
    void reshiftSlicesOnStream(int streamIndex, double newCentreHz);
```

Implement in `RadioModel.cpp` near `slicesOnStream`:

```cpp
void RadioModel::reshiftSlicesOnStream(int streamIndex, double newCentreHz)
{
    if (streamIndex < 0) {
        return;
    }
    const QVector<int> members = slicesOnStream(streamIndex);
    for (int sliceIdx : members) {
        SliceModel* s = sliceById(sliceIdx);
        if (!s) {
            continue;
        }
        const double shiftHz = s->frequency() - newCentreHz;
        s->setShiftOffsetHz(shiftHz);
        // From Thetis radio.cs:1417 [v2.10.3.15]: SetRXAShiftFreq receives
        // +(freq - center).
        if (m_wdspEngine) {
            if (RxChannel* ch = m_wdspEngine->rxChannel(s->sliceIndex())) {
                ch->setShiftFrequency(shiftHz);
            }
        }
    }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target tst_radio_model_slice_lifecycle -j8 && ./build/tests/tst_radio_model_slice_lifecycle`
Expected: PASS

- [ ] **Step 5: Route the CTUN paths through it**

Replace `MainWindow.cpp:7290-7293`, which currently writes `rxChannel(0)`
after setting the DDC centre:

```cpp
            activeSpectrumWidget()->setDdcCenterFrequency(centerHz);
            // Every slice on this stream sits at its own offset inside the
            // window, so a centre move re-shifts all of them, not only the
            // one that dragged.
            m_radioModel->reshiftSlicesOnStream(slice->streamIndex(), centerHz);
```

And `:7302-7305`, where disabling CTUN returns the shift to zero:

```cpp
        if (!enabled) {
            SliceModel* slice = m_radioModel->activeSlice();
            if (slice) {
                m_radioModel->reshiftSlicesOnStream(
                    slice->streamIndex(),
                    m_radioModel->streamCentreHzForTest(slice->streamIndex()));
            }
        }
```

If no public accessor for a stream's centre exists, use the allocator's
`streamCentreHz(int)` through whatever `RadioModel` already exposes; do not
add a `ForTest` accessor to production code. Check first:
`grep -n 'streamCentreHz' src/models/RadioModel.h src/core/SliceStreamAllocator.h`

- [ ] **Step 6: Build and run the slice tests**

Run: `cmake --build build --target tst_radio_model_slice_lifecycle -j8 && ./build/tests/tst_radio_model_slice_lifecycle`
Expected: PASS, including `secondSliceOnAPanTunesIndependently`

- [ ] **Step 7: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp src/gui/MainWindow.cpp tests/tst_radio_model_slice_lifecycle.cpp
git commit -m "fix(3f): a DDC centre move re-shifts every co-hosted slice

The CTUN path set the DDC centre and then wrote rxChannel(0)'s shift, so
with two slices sharing a window the co-host kept a stale shift and
demodulated the wrong signal while its flag still read the right number.
Same hazard family as the CTUN stranding fix in 1058500a.

reshiftSlicesOnStream walks slicesOnStream and recomputes each member's
offset as (frequency - newCentre), pushing it to that slice's own channel."
```

---

### Task 6: co-hosted slices share NB state at the model level

**Files:**
- Modify: `src/models/RadioModel.cpp` (beside the `nbModeChanged` handler
  at :8882)
- Test: `tests/tst_radio_model_slice_lifecycle.cpp`

**Interfaces:**
- Consumes: `SliceModel::nbMode()`, `setNbMode(NbMode)`,
  `nbModeChanged(NbMode)`, `RadioModel::slicesOnStream(int)`.

- [ ] **Step 1: Write the failing test**

```cpp
    // The noise blanker belongs to the DDC, not the slice: ANB panb / NOB
    // pnob live in struct _rcvr beside double* audio[cmMAXSubRcvr]
    // (Thetis cmaster.h:74-82 [v2.10.3.15]). Sub-Epic I Task 4b's rule is
    // that the first slice to reach processIq blanks the chunk WITH ITS OWN
    // SETTINGS and the co-hosts are bypassed, so linking only the buttons
    // would leave the blanker reading whichever slice happened to own the
    // pass. The state itself has to be mirrored.
    void coHostedSlicesShareNbState()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        sa->setFrequency(7'240'000.0);
        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sb = radio.sliceById(b);
        sb->setFrequency(7'245'000.0);
        QCOMPARE(sa->streamIndex(), sb->streamIndex());

        sa->setNbMode(NereusSDR::NbMode::NB1);
        QCOMPARE(sb->nbMode(), NereusSDR::NbMode::NB1);

        // And the other direction.
        sb->setNbMode(NereusSDR::NbMode::Off);
        QCOMPARE(sa->nbMode(), NereusSDR::NbMode::Off);
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target tst_radio_model_slice_lifecycle -j8 && ./build/tests/tst_radio_model_slice_lifecycle coHostedSlicesShareNbState`
Expected: FAIL, `sb->nbMode()` still Off after setting A's

- [ ] **Step 3: Mirror NB across the stream**

In `RadioModel.cpp`, extend the `nbModeChanged` handler at :8882. Guard
against the mirror re-entering itself:

```cpp
    connect(slice, &SliceModel::nbModeChanged, this, [this, slice](NereusSDR::NbMode m) {
        // existing per-slice push stays exactly as it is

        // The blanker belongs to the DDC, not the slice (Thetis
        // cmaster.h:74-82 [v2.10.3.15]: ANB panb / NOB pnob per receiver,
        // audio per sub-receiver). Sub-Epic I Task 4b runs one blanking pass
        // per chunk using the settings of whichever slice reaches processIq
        // first, so co-hosted slices must agree or the result depends on
        // arrival order. Mirroring the state makes ownership irrelevant.
        if (m_mirroringNbMode) { return; }
        m_mirroringNbMode = true;
        const int stream = slice->streamIndex();
        if (stream >= 0) {
            for (int idx : slicesOnStream(stream)) {
                SliceModel* peer = sliceById(idx);
                if (peer && peer != slice) {
                    peer->setNbMode(m);
                }
            }
        }
        m_mirroringNbMode = false;
    });
```

Add the guard member to `RadioModel.h` beside the other re-entrancy flags
(`m_rollingBackFrequency` is the existing example):

```cpp
    bool m_mirroringNbMode{false};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `./build/tests/tst_radio_model_slice_lifecycle coHostedSlicesShareNbState`
Expected: PASS

- [ ] **Step 5: Add the migration rule test**

```cpp
    // A slice joining an occupied stream adopts that stream's NB state; a
    // slice claiming an empty one keeps its own.
    void aSliceJoiningAStreamAdoptsItsNbState()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        sa->setFrequency(7'240'000.0);
        sa->setNbMode(NereusSDR::NbMode::NB2);

        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sb = radio.sliceById(b);
        QCOMPARE(sa->streamIndex(), sb->streamIndex());
        QCOMPARE(sb->nbMode(), NereusSDR::NbMode::NB2);
    }
```

- [ ] **Step 6: Implement adoption on bind**

In `bindSliceToStream`, after the placement is accepted and
`setStreamIndex` has run, adopt the stream's existing NB state:

```cpp
    // Joining an occupied window adopts its blanker state; claiming an empty
    // one keeps this slice's own. See the nbModeChanged mirror.
    const QVector<int> peers = slicesOnStream(placement.streamIndex);
    for (int idx : peers) {
        SliceModel* peer = sliceById(idx);
        if (peer && peer != slice) {
            slice->setNbMode(peer->nbMode());
            break;
        }
    }
```

- [ ] **Step 7: Run both tests**

Run: `cmake --build build --target tst_radio_model_slice_lifecycle -j8 && ./build/tests/tst_radio_model_slice_lifecycle`
Expected: PASS

- [ ] **Step 8: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp tests/tst_radio_model_slice_lifecycle.cpp
git commit -m "fix(3f): co-hosted slices share noise blanker state

The blanker belongs to the DDC, not the slice: ANB panb and NOB pnob live
in struct _rcvr beside double* audio[cmMAXSubRcvr] (Thetis cmaster.h:74-82
[v2.10.3.15]). Sub-Epic I Task 4b runs one blanking pass per chunk using
the settings of whichever slice reaches processIq first, so co-hosted
slices that disagree produce a result that depends on arrival order.

Mirroring nbMode across slicesOnStream makes pass ownership irrelevant and
makes the flags visibly move together, which is what the design chose over
greying the control. A slice joining an occupied stream adopts its state;
one claiming an empty stream keeps its own. Per-band persistence is still
per slice, so slices that later separate regain independent NB."
```

---

### Task 7: pin stereo pan as already working

**Files:**
- Test: `tests/tst_radio_model_slice_lifecycle.cpp`

**Interfaces:**
- Consumes: `SliceModel::audioPan()`, `setAudioPan(double)`.

Stereo pan is already correct end to end. This task adds the regression
test the spec's J7 assumed would need building, and nothing else.

- [ ] **Step 1: Write the test**

```cpp
    // Stereo pan was already per-slice before Sub-Epic J: the flag slider
    // emits panChanged, MainWindow writes SliceModel::audioPan, and
    // RadioModel pushes it to rxChannel(slice->sliceIndex())->setAudioPan.
    // It routes through WDSP's per-channel panel pan rather than
    // MasterMixer, which is the better place for it. Pinned so the epic
    // does not "fix" something that works.
    void audioPanIsIndependentPerSlice()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        const int a = radio.addSlice(QStringLiteral("pan-0"));
        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        SliceModel* sb = radio.sliceById(b);

        QCOMPARE(sa->audioPan(), 0.0);
        QCOMPARE(sb->audioPan(), 0.0);

        sa->setAudioPan(-1.0);
        sb->setAudioPan(1.0);

        QCOMPARE(sa->audioPan(), -1.0);
        QCOMPARE(sb->audioPan(), 1.0);
    }
```

- [ ] **Step 2: Run it**

Run: `cmake --build build --target tst_radio_model_slice_lifecycle -j8 && ./build/tests/tst_radio_model_slice_lifecycle audioPanIsIndependentPerSlice`
Expected: PASS immediately. If it fails, pan is NOT already wired and this
task becomes real work: follow Task 1's shape.

- [ ] **Step 3: Commit**

```bash
git add tests/tst_radio_model_slice_lifecycle.cpp
git commit -m "test(3f): pin stereo pan as already per-slice

The Sub-Epic J design proposed building per-flag stereo pan through
MasterMixer::setSliceGain. Recon found it already built and correct, and
routed through WDSP's per-channel panel pan instead, which is the better
place for it. Pinned so the epic does not rebuild working behaviour."
```

---

### Task 8: per-instance slew length on MasterMixer

**Files:**
- Modify: `src/core/audio/MasterMixer.h`, `src/core/audio/MasterMixer.cpp`
- Test: `tests/tst_master_mixer.cpp`

**Interfaces:**
- Produces: `MasterMixer::setSlewUpFrames(int frames)`. `0` disables the
  slew entirely. Task 9 consumes it.

- [ ] **Step 1: Write the failing test**

```cpp
    // The anti-VOX mixer must NOT slew. Thetis creates the RX mixer with
    // tslewup 0.010 but the anti-VOX mixer with 0.000 on all four slew
    // parameters (cmaster.c:297-313 vs cmaster.c:159-175 [v2.10.3.15]).
    // The DEXP reference has to be amplitude-faithful from the first sample
    // after a transition, so the slew length is per instance.
    void slewCanBeDisabledPerInstance() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSlewUpFrames(0);
        mix.setSliceGain(1, 1.0f, 0.0f);

        std::array<float, 8> in = {1.0f, 1.0f, 1.0f, 1.0f,
                                   1.0f, 1.0f, 1.0f, 1.0f};
        std::array<float, 8> out{};

        // Arming would normally fade the mix in over kSlewUpFrames.
        mix.setSliceStreaming(1, false);
        mix.setSliceStreaming(1, true);

        mix.accumulate(1, in.data(), 4);
        QCOMPARE(mix.tryDrain(out.data(), 4), 4);

        // With slew disabled the first frame is already at full amplitude.
        QVERIFY(std::abs(out[0] - 1.0f) < 0.0001f);
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --target tst_master_mixer -j8`
Expected: FAIL to compile, "no member named 'setSlewUpFrames'"

- [ ] **Step 3: Make the slew length a member**

In `MasterMixer.h`, keep `kSlewUpFrames` as the default and add:

```cpp
    /// Slew length for THIS instance, in frames. 0 disables the fade.
    ///
    /// The speakers mixer wants the 10 ms raised cosine (Thetis
    /// cmaster.c:297-313 [v2.10.3.15], tslewup 0.010). The anti-VOX mixer
    /// wants none: Thetis creates it with 0.000 on all four slew parameters
    /// (cmaster.c:159-175), because the DEXP reference must be
    /// amplitude-faithful from the first sample after a transition.
    void setSlewUpFrames(int frames);
```

and a member beside `m_slewPos`:

```cpp
    int m_slewUpFrames{kSlewUpFrames};
```

In `MasterMixer.cpp`:

```cpp
void MasterMixer::setSlewUpFrames(int frames) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    m_slewUpFrames = std::max(0, frames);
    // Park the position past the end so a shortened window cannot leave a
    // drain mid-fade against a table it has outgrown.
    m_slewPos.store(m_slewUpFrames, std::memory_order_release);
}
```

- [ ] **Step 4: Use the member in the drain**

Replace the slew block at the end of `tryDrain`, which currently reads
`kSlewUpFrames`:

```cpp
    const int slewLen = m_slewUpFrames;
    int pos = m_slewPos.load(std::memory_order_acquire);
    if (slewLen > 0 && pos < slewLen) {
        const float* w = upSlewWindow();
        for (int i = 0; i < n && pos < slewLen; ++i, ++pos) {
            const float g = w[pos];
            out[static_cast<size_t>(i) * 2 + 0] *= g;
            out[static_cast<size_t>(i) * 2 + 1] *= g;
        }
        m_slewPos.store(pos, std::memory_order_release);
    }
```

Note: `upSlewWindow()` is built for `kSlewUpFrames`. Only `0` (disabled) and
`kSlewUpFrames` (default) are supported values; reject anything else in
`setSlewUpFrames` by clamping to those two, or rebuild the table per length.
Clamping is simpler and covers both callers:

```cpp
    m_slewUpFrames = (frames <= 0) ? 0 : kSlewUpFrames;
```

Use that form and delete the `std::max` line above.

- [ ] **Step 5: Also guard the arming path**

In `setSliceStreaming`, do not arm when the slew is disabled:

```cpp
    if (streaming && m_slewUpFrames > 0) {
        m_slewPos.store(0, std::memory_order_release);
    }
```

- [ ] **Step 6: Run to verify it passes**

Run: `cmake --build build --target tst_master_mixer -j8 && ./build/tests/tst_master_mixer`
Expected: PASS, all 21 cases including the existing slew tests

- [ ] **Step 7: Commit**

```bash
git add src/core/audio/MasterMixer.h src/core/audio/MasterMixer.cpp tests/tst_master_mixer.cpp
git commit -m "feat(3f): slew length is per MasterMixer instance

Thetis creates the RX mixer with tslewup 0.010 (cmaster.c:297-313
[v2.10.3.15]) and the anti-VOX mixer with 0.000 on all four slew
parameters (cmaster.c:159-175). The anti-VOX instance landing in the next
commit needs the fade off, because the DEXP reference has to be
amplitude-faithful from the first sample after a transition.

setSlewUpFrames(0) disables it. Values clamp to disabled or the default,
since the cosine table is built for the default length."
```

---

### Task 9: anti-VOX hears every slice

**Files:**
- Modify: `src/core/AudioEngine.h`, `src/core/AudioEngine.cpp`
- Modify: `src/models/RxDspWorker.cpp:590-604`
- Test: `tests/tst_audio_engine_antivox_mix.cpp` (create)
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `MasterMixer::setSlewUpFrames(int)` from Task 8,
  `MasterMixer::{accumulate,tryDrain,setSliceStreaming,setSliceGain}`.
- Produces: `AudioEngine::antiVoxMixForTest() -> MasterMixer&` under
  `NEREUS_BUILD_TESTS`.

This is the largest task. Read spec section J10 in full before starting.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_audio_engine_antivox_mix.cpp`:

```cpp
// no-port-check: NereusSDR-original test infrastructure. Thetis citations
// below are rationale for the topology, not ported code.
//
// Phase 3F Sub-Epic J Task 9. Anti-VOX fed only the stream hosting slice 0,
// so the canceller heard receiver A alone. With a second receiver audible
// the reference no longer matches what reaches the speakers, and
// antivox_level then drives asig = avsig - antivox_gain * antivox_level
// (Thetis dexp.c:313-316 [v2.10.3.15]) either too hard or not hard enough:
// a false VOX trigger, meaning unintended transmit, or a failure to cancel.
//
// Thetis feeds EVERY sub-receiver to a dedicated per-transmitter mixer
// (cmaster.c:371-372) whose membership is explicit, through the same
// SetAAudioMixStates machinery as the RX mixer (cmaster.c:584-588).

#include <QtTest/QtTest>
#include "core/AudioEngine.h"
#include "core/audio/MasterMixer.h"
#include "models/RadioModel.h"

#include <array>

using namespace NereusSDR;

class TstAudioEngineAntiVoxMix : public QObject {
    Q_OBJECT
private slots:
    void theAntiVoxMixSumsEverySlice()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        AudioEngine* engine = radio.audioEngine();
        QVERIFY(engine != nullptr);

        MasterMixer& av = engine->antiVoxMixForTest();
        av.setRampFrames(1);

        std::array<float, 2> a = {0.3f, 0.3f};
        std::array<float, 2> b = {0.4f, 0.4f};
        std::array<float, 2> out{};

        av.setSliceGain(0, 1.0f, 0.0f);
        av.setSliceGain(1, 1.0f, 0.0f);
        av.accumulate(0, a.data(), 1);
        av.accumulate(1, b.data(), 1);

        QCOMPARE(av.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.7f);
    }

    // Zero slew: the DEXP reference must be amplitude-faithful from the
    // first sample. Thetis creates this mixer with 0.000 on all four slew
    // parameters (cmaster.c:159-175 [v2.10.3.15]).
    void theAntiVoxMixDoesNotSlew()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        MasterMixer& av = radio.audioEngine()->antiVoxMixForTest();
        av.setRampFrames(1);
        av.setSliceGain(0, 1.0f, 0.0f);

        std::array<float, 2> in = {1.0f, 1.0f};
        std::array<float, 2> out{};

        av.setSliceStreaming(0, false);
        av.setSliceStreaming(0, true);
        av.accumulate(0, in.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 1);
        QVERIFY(std::abs(out[0] - 1.0f) < 0.0001f);
    }

    // The TX monitor never feeds anti-VOX. Upstream's masks are drawn from
    // RX1 + RX1S + RX2 and never include MON (console.cs:27650-27771
    // [v2.10.3.15]); monitor audio suppressing the operator's own VOX would
    // be feedback by definition.
    void theTxMonitorNeverFeedsAntiVox()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        MasterMixer& av = radio.audioEngine()->antiVoxMixForTest();

        std::array<float, 2> mon = {0.9f, 0.9f};
        std::array<float, 2> out{};
        // kTxMonitorSlotId is never registered with this instance, so an
        // accumulate for it is dropped.
        av.accumulate(/*kTxMonitorSlotId*/ -2, mon.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 0);
    }
};

QTEST_MAIN(TstAudioEngineAntiVoxMix)
#include "tst_audio_engine_antivox_mix.moc"
```

- [ ] **Step 2: Register and run it to verify it fails**

Add to `tests/CMakeLists.txt`:

```cmake
# Phase 3F Sub-Epic J Task 9: anti-VOX hears every slice, not just slice 0.
# Membership mirrors the speakers mixer; TX monitor excluded; zero slew.
nereus_add_test(tst_audio_engine_antivox_mix)
```

Run: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON && cmake --build build --target tst_audio_engine_antivox_mix -j8`
Expected: FAIL to compile, "no member named 'antiVoxMixForTest'"

- [ ] **Step 3: Add the anti-VOX mixer instance**

In `AudioEngine.h`, beside `m_masterMix`:

```cpp
    // Second mixer whose output is the anti-VOX reference, not the speakers.
    //
    // Thetis runs exactly this: a per-transmitter aamix instance
    // (pcm->xmtr[i].pavoxmix, cmaster.c:159-175 [v2.10.3.15]) fed by every
    // sub-receiver (cmaster.c:371-372), with membership managed explicitly
    // through SetAAudioMixStates (cmaster.c:584-588), the same call the RX
    // mixer uses. So this instance is barrier-paced exactly like
    // m_masterMix, and its membership rides the same setSliceStreaming
    // calls. The TX monitor is deliberately never registered: upstream's
    // masks are drawn from RX1 + RX1S + RX2 and never include MON, and
    // monitor audio suppressing the operator's own VOX would be feedback.
    MasterMixer m_antiVoxMix;
```

and the test seam beside `masterMixForTest()`:

```cpp
#ifdef NEREUS_BUILD_TESTS
    MasterMixer& antiVoxMixForTest() { return m_antiVoxMix; }
#endif
```

- [ ] **Step 4: Configure it at construction**

In the `AudioEngine` constructor, beside the existing
`m_masterMix.setSliceGain(kTxMonitorSlotId, ...)` block:

```cpp
    // No fade on the anti-VOX reference. Thetis creates this mixer with
    // 0.000 on all four slew parameters (cmaster.c:159-175 [v2.10.3.15]),
    // unlike the RX mixer's 0.010, because DEXP needs an amplitude-faithful
    // reference from the first sample after a transition.
    m_antiVoxMix.setSlewUpFrames(0);
```

Extend `preregisterSlices` so both mixers get every slice id, and note that
`kTxMonitorSlotId` is registered with `m_masterMix` only:

```cpp
    for (int id = m_preregisteredSlices; id < wanted; ++id) {
        m_masterMix.setSliceGain(id, 1.0f, 0.0f);
        // Same slots on the anti-VOX instance. Registered here for the same
        // reason: accumulate() drops ids it has no entry for, and the map
        // must not be mutated once the DSP thread is reading it.
        m_antiVoxMix.setSliceGain(id, 1.0f, 0.0f);
    }
```

- [ ] **Step 5: Mirror membership**

Everywhere `AudioEngine::setSliceStreaming` forwards to `m_masterMix`,
forward to `m_antiVoxMix` too:

```cpp
void AudioEngine::setSliceStreaming(int sliceId, bool streaming)
{
    m_masterMix.setSliceStreaming(sliceId, streaming);
    // Membership mirrors the speakers mixer. Mandatory, not stylistic: the
    // MOX gate stops feeding the gated slice for the length of a
    // transmission, so a slice left enrolled here would wedge the anti-VOX
    // barrier for the whole over.
    m_antiVoxMix.setSliceStreaming(sliceId, streaming);
}
```

Do the same in `setMoxState`'s withdraw and re-admit, which already calls
`m_masterMix.setSliceStreaming` through this method: confirm it routes
through `AudioEngine::setSliceStreaming` rather than touching `m_masterMix`
directly, and change it if not.

- [ ] **Step 6: Feed and drain it**

In `rxBlockReady`, immediately after the `m_masterMix.accumulate(...)` call,
feed the same block:

```cpp
    // Anti-VOX hears exactly what the speakers hear. From Thetis
    // cmaster.c:371-372 [v2.10.3.15], every sub-receiver's audio is fed to
    // the transmitter's anti-VOX mixer in the same loop that feeds the
    // speakers mix.
    m_antiVoxMix.accumulate(sliceId, samples, frames, slice->muted());
```

After the speakers drain and push, drain the anti-VOX mix in the same call
stack so both are paced identically:

```cpp
    static thread_local std::vector<float> avMix;
    if (static_cast<int>(avMix.size()) < frames * 2) {
        avMix.resize(static_cast<size_t>(frames) * 2);
    }
    const int avFrames = m_antiVoxMix.tryDrain(avMix.data(), frames);
    if (avFrames > 0) {
        emit antiVoxBlockReady(avMix.data(), avFrames);
    }
```

Declare `antiVoxBlockReady(const float*, int)` in `AudioEngine.h` beside the
other audio signals.

- [ ] **Step 7: Retire the slice-0 gate**

In `RxDspWorker.cpp`, the `hostsSliceZero` block at :595-604 emits
`antiVoxSampleReady`. The mixer now supplies the reference, so delete the
emit and the gate, and leave the explanatory comment block in place with a
note that the cadence argument now lives in `AudioEngine`. Re-point
`TxWorkerThread`'s consumer at `AudioEngine::antiVoxBlockReady`.

Verify the consumer chain first:
`grep -rn 'antiVoxSampleReady\|onAntiVoxSamplesReady' src/`

- [ ] **Step 8: Pin the cadence**

Add to the new test file:

```cpp
    // The cadence constraint. The retired slice-0 gate guaranteed exactly
    // one block per outSize/outRate seconds, which is what DEXP was
    // configured for, and it held because inSize = 64 * rate / 48000 makes
    // inSize/inputRate == outSize/outRate per stream. Barrier pacing now
    // supplies it: one drained block per period, whatever the stream width.
    void theAntiVoxMixDrainsOneBlockPerPeriod()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        MasterMixer& av = radio.audioEngine()->antiVoxMixForTest();
        av.setRampFrames(1);
        av.setSliceGain(0, 1.0f, 0.0f);
        av.setSliceGain(1, 1.0f, 0.0f);

        std::array<float, 2> blk = {0.5f, 0.5f};
        std::array<float, 2> out{};

        // Both members deliver once: exactly one block leaves.
        av.accumulate(0, blk.data(), 1);
        av.accumulate(1, blk.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 0);

        // One member alone delivers: nothing leaves until the other does.
        av.accumulate(0, blk.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 0);
        av.accumulate(1, blk.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 1);
    }
```

- [ ] **Step 9: Run the tests**

Run: `cmake --build build --target tst_audio_engine_antivox_mix -j8 && ./build/tests/tst_audio_engine_antivox_mix`
Expected: PASS, all four cases

- [ ] **Step 10: Run the neighbouring audio suites**

Run:
```bash
cmake --build build --target tst_master_mixer tst_audio_engine_multi_slice_mix tst_audio_engine_rx_leak_during_mox -j8
for t in tst_master_mixer tst_audio_engine_multi_slice_mix tst_audio_engine_rx_leak_during_mox; do ./build/tests/$t; done
```
Expected: PASS

- [ ] **Step 11: Commit**

```bash
git add src/core/AudioEngine.h src/core/AudioEngine.cpp src/models/RxDspWorker.cpp tests/tst_audio_engine_antivox_mix.cpp tests/CMakeLists.txt
git commit -m "fix(3f): anti-VOX hears every slice, not only slice A

RxDspWorker fed anti-VOX from the stream hosting slice 0 alone, so the
canceller heard receiver A while the operator might be listening to B or
to both. antivox_level then drives asig = avsig - antivox_gain *
antivox_level (Thetis dexp.c:313-316 [v2.10.3.15]) either too hard or not
hard enough: a false VOX trigger, meaning unintended transmit, or a
failure to cancel.

Thetis feeds every sub-receiver to a dedicated per-transmitter mixer
(cmaster.c:371-372) whose membership is explicit through the same
SetAAudioMixStates machinery as the RX mixer (cmaster.c:584-588), so it is
barrier-paced exactly like the speakers mix. This adds a second
MasterMixer whose membership rides the same setSliceStreaming calls,
mandatory rather than stylistic because the MOX gate stops feeding the
gated slice and a still-enrolled member would wedge the anti-VOX barrier
for the whole over.

The TX monitor is never registered: upstream masks never include MON, and
monitor audio suppressing the operator's own VOX would be feedback. Slew
is disabled to match upstream's 0.000, so the DEXP reference is
amplitude-faithful from the first sample after a transition.

Cadence, which the retired slice-0 gate owned, is now supplied by barrier
pacing and pinned by test."
```

---

### Task 10: TCI rx_volume addresses the right slice

**Files:**
- Modify: `src/core/TciProtocol.cpp` (the `afLinear` block around :542)
- Test: `tests/tst_tci_dispatch_seam.cpp` (extend)

**Interfaces:**
- Consumes: `RadioModel::sliceById(int)`, `SliceModel::afGain()`.

- [ ] **Step 1: Fix the spec's citation, then decide the mapping**

The spec cites `TciProtocol.cpp:339` and `:542` as per-slice audio
deferrals. Only `:542` is: `:339` is a different divergence, about VFOBTX /
VFOASubFreq / split modelling. Note that in the commit message and leave
`:339` alone; it belongs to a future split-modelling task, not this one.

Then fix the mapping, which is a protocol-facing decision and must be
explicit. TCI addresses receivers as `receiver:channel` pairs. Establish
and document the rule:

```bash
grep -n 'rx_volume\|readIntGlobal\|receiver' src/core/TciProtocol.cpp | head -20
```

Document the chosen mapping in a comment at the handler, in the form
`TCI receiver N, channel M -> slice id X`.

- [ ] **Step 2: Write the failing test**

```cpp
    // TCI rx_volume returned one global afLinear for every slot, so a client
    // asking for receiver 1's volume got receiver 0's.
    void rx_volume_reports_per_slice_gain()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        const int a = radio.addSlice(QStringLiteral("pan-0"));
        const int b = radio.addSlice(QStringLiteral("pan-0"));
        radio.sliceById(a)->setAfGain(30);
        radio.sliceById(b)->setAfGain(70);

        QCOMPARE(radio.sliceById(a)->afGain(), 30);
        QCOMPARE(radio.sliceById(b)->afGain(), 70);
        QVERIFY(radio.sliceById(a)->afGain() != radio.sliceById(b)->afGain());
    }
```

- [ ] **Step 3: Run to verify it fails or passes**

Run: `cmake --build build --target tst_tci_dispatch_seam -j8 && ./build/tests/tst_tci_dispatch_seam`
Expected: this model-level assertion likely PASSES already. If so, the gap
is purely in `TciProtocol`'s read path: extend the test to call the actual
dispatch entry point for `rx_volume` with two different receiver indices and
assert they differ. Do not ship a test that cannot fail.

- [ ] **Step 4: Route the handler per slice**

Replace the `afLinearVal` read so each `rx_volume` slot resolves its own
slice through the documented mapping, falling back to the active slice when
the index does not resolve.

- [ ] **Step 5: Run to verify it passes**

Run: `./build/tests/tst_tci_dispatch_seam`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/core/TciProtocol.cpp tests/tst_tci_dispatch_seam.cpp
git commit -m "fix(3f): TCI rx_volume addresses its own slice

All three rx_volume slots reflected one global afLinear, so a client
asking receiver 1 got receiver 0's gain. Each slot now resolves its own
slice through an explicit receiver:channel to slice-id mapping, documented
at the handler.

Note for the record: the Sub-Epic J design also cited TciProtocol.cpp:339
as a per-slice audio deferral. It is not. That comment is about VFOBTX /
VFOASubFreq / split modelling and belongs to a future task."
```

---

### Task 11: audit, migrate, then ban GUI access to WDSP

**Files:**
- Create: `scripts/verify-no-gui-dsp-access.py`
- Modify: `scripts/install-hooks.sh`, `.github/workflows/ci.yml`
- Modify: whatever the audit convicts

**Interfaces:**
- Consumes: nothing. Produces: a CI gate.

Order matters. The check lands last or CI goes red against files this plan
never listed.

- [ ] **Step 1: Audit all thirteen sites**

Run:

```bash
grep -rn 'rxChannel(' src/gui/ --include='*.cpp'
```

Expected: 13 hits across `MainWindow.cpp`, `setup/DspSetupPages.cpp`,
`setup/DspOptionsPage.cpp`, `meters/MeterPoller.cpp`.

For each, record in a scratch list: does it hardcode `0`, or resolve a
slice? Is it a control write (must migrate) or engine-internal plumbing
(`MeterPoller` is the likely allowlist case, since it is a meter driver that
happens to live under `src/gui/`)?

- [ ] **Step 2: Migrate every convicted site**

For each site that hardcodes `rxChannel(0)` for a control write, follow
Task 1's shape: `SliceModel` property if one is missing, push in
`RadioModel`, GUI writes the property. Commit each file separately so a
reviewer can reject one without the others.

- [ ] **Step 3: Write the checker**

Create `scripts/verify-no-gui-dsp-access.py`:

```python
#!/usr/bin/env python3
"""Fail if src/gui/ reaches into WdspEngine for an RX channel.

Phase 3F Sub-Epic J. The per-slice pipeline is SliceModel property ->
RadioModel push -> rxChannel(slice->sliceIndex()). Controls that call
rxChannel() from the GUI bypass it and, historically, hardcode channel 0,
which is how ANF on slice B ended up toggling slice A.

Allowlist entries are files whose rxChannel() use is engine-internal rather
than a control write.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUI = ROOT / "src" / "gui"
PATTERN = re.compile(r"rxChannel\s*\(")
ALLOWLIST = {
    # Meter driver: resolves per-slice channels for polling, not a control
    # write. Lives under src/gui/ for packaging reasons.
    "src/gui/meters/MeterPoller.cpp",
}

def main() -> int:
    failures = []
    for path in sorted(GUI.rglob("*.cpp")):
        rel = path.relative_to(ROOT).as_posix()
        if rel in ALLOWLIST:
            continue
        for num, line in enumerate(path.read_text().splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            if PATTERN.search(line):
                failures.append(f"{rel}:{num}: {stripped}")
    if failures:
        print("[gui-dsp-access] GUI code must not call rxChannel() directly.")
        print("Route through SliceModel; RadioModel pushes to the right channel.")
        for f in failures:
            print(f"  {f}")
        return 1
    print(f"[gui-dsp-access] OK: no direct rxChannel() use in src/gui/")
    return 0

if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Verify it catches a violation**

```bash
python3 scripts/verify-no-gui-dsp-access.py && echo "clean tree passes"
printf '\n// probe\nvoid probe() { auto* c = eng->rxChannel(0); (void)c; }\n' >> src/gui/MainWindow.cpp
python3 scripts/verify-no-gui-dsp-access.py; echo "exit=$? (expect 1)"
git checkout src/gui/MainWindow.cpp
```

Expected: passes clean, exits 1 with the probe, passes again after revert.

- [ ] **Step 5: Wire it into the hook chain and CI**

Add to `scripts/install-hooks.sh` beside the other verifiers, and to
`.github/workflows/ci.yml` beside the existing attribution checks. Match
their invocation style exactly.

- [ ] **Step 6: Commit**

```bash
git add scripts/verify-no-gui-dsp-access.py scripts/install-hooks.sh .github/workflows/ci.yml
git commit -m "build(3f): fail the build when GUI code reaches into WDSP

The per-slice pipeline is SliceModel property -> RadioModel push ->
rxChannel(slice->sliceIndex()). Controls that call rxChannel() from the
GUI bypass it and historically hardcoded channel 0, which is how ANF on
slice B came to toggle slice A and how the CTUN path came to re-shift only
one of two co-hosted slices.

Landed after the migration rather than before it, so it never went red
against files the sub-epic had not touched yet. MeterPoller is allowlisted:
it resolves per-slice channels for polling and is not a control write."
```

---

### Task 12: full suite and bench matrix

- [ ] **Step 1: Build everything**

Run: `cmake --build build -j8 && cmake --build build --target all_tests -j8`
Expected: no errors

- [ ] **Step 2: Run the full suite**

Run: `cd build && ctest -j8`
Expected: 100% pass. The count will exceed 561 by the tests added here.

- [ ] **Step 3: Relaunch and verify the binary**

```bash
pkill -f 'NereusSDR.app/Contents/MacOS/NereusSDR'; sleep 3
nohup ./build/NereusSDR.app/Contents/MacOS/NereusSDR > /private/tmp/nereus-subepic-j.log 2>&1 &
sleep 12; PID=$(pgrep -f 'NereusSDR.app/Contents/MacOS/NereusSDR' | head -1)
lsof -p "$PID" | grep -E 'txt.*MacOS/NereusSDR'
```

Expected: the running image resolves inside this worktree.

- [ ] **Step 4: Bench matrix, needs the operator and a radio**

Two receivers on one pan, on an ANAN-G2 or G2E:

1. ANF on B's flag changes B only; A's ANF unaffected.
2. DSP menu ANF follows the last flag clicked.
3. Container S-meter follows the last flag clicked; each flag's own meter
   tracks its own slice.
4. Drag CTUN: both receivers stay on their signals, neither drifts.
5. NB on either flag: both move together, and both still blank.
6. Pan A hard left and B hard right: they separate in the stereo field.
7. Key up with two receivers: the non-TX receiver keeps playing, no wedge,
   no click on release.
8. With a second receiver audible, anti-VOX does not false-trigger on
   receiver B's audio, and still cancels.

Record results in `docs/architecture/2026-05-26-phase3f-verification/`
alongside the existing G2 results.

- [ ] **Step 5: Commit the verification record**

```bash
git add docs/architecture/2026-05-26-phase3f-verification/
git commit -m "docs(3f): Sub-Epic J bench verification record"
```

---

## Self-review

**Spec coverage:**

| Spec item | Task |
| --- | --- |
| J1 ANF wrong slice | Task 1 |
| J2 dead NR handler | Task 2 |
| J3 DSP menu | Task 3 |
| J4 container S-meter | Task 4 |
| J5 CTUN centre re-shift | Task 5 |
| J6 shared NB + migration rule | Task 6 |
| J7 stereo pan | Task 7 (already built; regression test only) |
| J8 TCI per-slice | Task 10 |
| J9 audit, migrate, ban | Task 11 |
| J10 anti-VOX mix | Tasks 8 and 9 |
| Bench + full suite | Task 12 |

Every spec item maps to a task. Task 8 exists because J10's zero-slew
requirement needs a per-instance slew length that does not exist yet; the
spec calls for it but does not name it as separate work.

**Type consistency:** `setAnfEnabled(bool)` / `anfEnabled()` /
`anfEnabledChanged(bool)` used identically in Tasks 1, 3 and 11.
`reshiftSlicesOnStream(int, double)` defined in Task 5 and used only there.
`setSlewUpFrames(int)` defined in Task 8, consumed in Task 9.
`antiVoxMixForTest()` defined in Task 9, used only in its own test.

**Known soft spots, flagged rather than hidden:**
- Task 3's test assumes an active-slice setter exists on `RadioModel`; the
  step says to grep for the real name first and not to add one.
- Task 5 step 5 needs a public accessor for a stream's centre; the step says
  to check and not to add a `ForTest` accessor to production code.
- Task 10's step 3 explicitly warns that the model-level assertion may pass
  on arrival, in which case the test must move to the dispatch entry point.
  A test that cannot fail is not a test.
