# Multi-Panadapter Layout Engine

**Status:** Phase 2B -- Architecture Design

## Overview

NereusSDR supports up to 4 independent panadapters, each with its own
spectrum display, waterfall, and associated slice(s). Unlike AetherSDR where
the radio assigns panadapter IDs and streams FFT/waterfall data per-pan,
NereusSDR manages panadapters entirely client-side. The radio has no concept
of panadapters -- it sends raw I/Q samples, and the client computes FFT data
independently for each panadapter view.

This means:
- Adding a panadapter increases client CPU load (additional FFT computation)
- Pan center frequency and bandwidth are client-side state, not radio state
- Slice-to-pan association is a client-side mapping
- Layout persistence is handled by AppSettings, not the radio

**Important:** Each panadapter's visible bandwidth is limited by the DDC
sample rate of its associated receiver (max ~384 kHz), not the full ADC
bandwidth (61.44 MHz). See
[adc-ddc-panadapter-mapping.md](adc-ddc-panadapter-mapping.md) for the
complete ADC -> DDC -> Receiver -> FFT -> Panadapter signal chain.

---

## 1. PanadapterStack -- QSplitter-Based Layout Manager

### Responsibility

PanadapterStack owns and arranges N PanadapterApplet instances using nested
QSplitters. It tracks which panadapter is active (receives keyboard/wheel
input and determines which slice the applet column controls).

### Layout Configurations

| Layout ID | Name | Description |
|-----------|------|-------------|
| `"1"` | Single | One full-size panadapter |
| `"2v"` | Stacked | Two pans vertically (top/bottom) |
| `"2h"` | Side-by-side | Two pans horizontally (left/right) |
| `"2x2"` | Grid | Four equal-sized pans in a 2x2 grid |
| `"12h"` | Wide + 2 | Wide pan on top, two smaller pans stacked below |

Layout structure uses nested QSplitters:

```
Layout "1":
  QSplitter(V)
    [PanadapterApplet A]

Layout "2v":
  QSplitter(V)
    [PanadapterApplet A]
    [PanadapterApplet B]

Layout "2h":
  QSplitter(H)
    [PanadapterApplet A]
    [PanadapterApplet B]

Layout "2x2":
  QSplitter(V)
    QSplitter(H)
      [PanadapterApplet A]
      [PanadapterApplet B]
    QSplitter(H)
      [PanadapterApplet C]
      [PanadapterApplet D]

Layout "12h":
  QSplitter(V)
    [PanadapterApplet A]           (stretch 2)
    QSplitter(H)
      [PanadapterApplet B]
      [PanadapterApplet C]
```

### Class Interface

```cpp
#pragma once

#include <QWidget>
#include <QMap>
#include <QSplitter>

namespace NereusSDR {

class PanadapterApplet;
class SpectrumWidget;

// Manages N PanadapterApplet instances in configurable QSplitter layouts.
// Tracks the active panadapter (receives tune/filter commands).
class PanadapterStack : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString activePanId READ activePanId NOTIFY activePanChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit PanadapterStack(QWidget* parent = nullptr);

    // --- Pan lifecycle ---

    // Create a new panadapter applet with the given client-assigned ID.
    // Returns the created applet. Caller must wire signals via wirePanadapter().
    PanadapterApplet* addPanadapter(int panId);

    // Remove a panadapter by ID. Caller must disconnect signals BEFORE calling.
    void removePanadapter(int panId);

    // Remove all applets and reset the splitter hierarchy.
    void removeAll();

    // --- Layout ---

    // Rebuild the splitter structure for the given layout ID.
    // panIds specifies which pans fill each slot in order.
    // Pans not in panIds are hidden; missing pans are created.
    void applyLayout(const QString& layoutId, const QList<int>& panIds);

    // Equalize all splitter sizes (reset user drag).
    void equalizeSizes();

    // Save current splitter sizes to AppSettings. Called on resize events.
    void saveSplitterState();

    // Restore splitter sizes from AppSettings. Called after applyLayout().
    void restoreSplitterState();

    // --- Accessors ---

    PanadapterApplet* panadapter(int panId) const;
    SpectrumWidget*   spectrum(int panId) const;
    int               count() const { return m_pans.size(); }
    QList<PanadapterApplet*> allApplets() const;

    // --- Active pan ---

    int               activePanId() const { return m_activePanId; }
    PanadapterApplet* activeApplet() const;
    SpectrumWidget*   activeSpectrum() const;
    void              setActivePan(int panId);

signals:
    void activePanChanged(int panId);
    void countChanged(int count);

private:
    void rebuildSplitters(const QString& layoutId, const QList<int>& panIds);
    void clearSplitters();

    QSplitter*                   m_rootSplitter{nullptr};
    QMap<int, PanadapterApplet*> m_pans;
    int                          m_activePanId{0};
    QString                      m_currentLayoutId{"1"};
};

} // namespace NereusSDR
```

### Pan IDs

Unlike AetherSDR where pan IDs are hex stream IDs assigned by the radio
(e.g. `"0x40000000"`), NereusSDR uses simple integer IDs (0, 1, 2, 3)
assigned client-side. The radio has no knowledge of panadapters.

### Layout Persistence

Layout state is saved to AppSettings:

| Key | Example Value | Description |
|-----|--------------|-------------|
| `PanLayoutId` | `"2v"` | Current layout configuration |
| `PanCount` | `"2"` | Number of active panadapters |
| `PanSplitter0Sizes` | `"300,300"` | Root splitter sizes (comma-separated) |
| `PanSplitter1Sizes` | `"400,400"` | Nested splitter sizes (if applicable) |
| `Pan0CenterMhz` | `"14.225"` | Per-pan center frequency |
| `Pan0BandwidthMhz` | `"0.200"` | Per-pan display bandwidth |
| `Pan0FftSize` | `"4096"` | Per-pan FFT size |

---

## 2. PanadapterApplet -- Single Pan Container Widget

### Responsibility

PanadapterApplet is the container for a single panadapter view. It holds the
SpectrumWidget (FFT + waterfall), a title bar, and optional auxiliary panels
(CW decode, digital mode decode).

### Visual Structure

```
+-----------------------------------------------+
| Title Bar (16px gradient)    [Rx1: 14.225 USB] |
+-----------------------------------------------+
|                                                |
|  SpectrumWidget                                |
|  (FFT spectrum ~40% + waterfall ~60%)          |
|                                                |
+-----------------------------------------------+
| CW Decode Panel (optional, collapsible)        |
+-----------------------------------------------+
```

The title bar follows the NereusSDR applet style guide:
- 16px height, gradient background (`#3a4a5a` -> `#2a3a4a` -> `#1a2a38`)
- 10px bold `#8aa8c0` label
- Shows active slice letter (A/B/C/D), frequency, and mode

### Per-Pan State

Each PanadapterApplet maintains independent display state:

| State | Type | Default | Description |
|-------|------|---------|-------------|
| `m_panId` | `int` | 0 | Client-assigned pan index |
| `m_centerMhz` | `double` | 14.225 | Pan center frequency |
| `m_bandwidthMhz` | `double` | 0.200 | Display bandwidth |
| `m_fftSize` | `int` | 4096 | FFT size for this pan |
| `m_averaging` | `int` | 0 | FFT averaging frames (0=off) |
| `m_refLevel` | `float` | -50.0 | Top of spectrum (dBm) |
| `m_dynamicRange` | `float` | 100.0 | dB range displayed |
| `m_associatedSlices` | `QSet<int>` | {} | Slice IDs shown on this pan |
| `m_activeSliceId` | `int` | -1 | Which slice receives tune commands |

### Class Interface

```cpp
#pragma once

#include <QWidget>
#include <QSet>

class QLabel;
class QPushButton;
class QTextEdit;

namespace NereusSDR {

class SpectrumWidget;

// Container for a single panadapter display: title bar + SpectrumWidget +
// optional CW decode panel. Each PanadapterApplet has independent display
// state and can show overlays for one or more slices.
class PanadapterApplet : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int panId READ panId CONSTANT)
    Q_PROPERTY(int activeSliceId READ activeSliceId
               WRITE setActiveSliceId NOTIFY activeSliceChanged)

public:
    explicit PanadapterApplet(int panId, QWidget* parent = nullptr);

    // --- Accessors ---

    SpectrumWidget* spectrumWidget() const { return m_spectrum; }
    int panId() const { return m_panId; }

    // --- Pan display state ---

    double centerMhz() const { return m_centerMhz; }
    void setCenterMhz(double mhz);

    double bandwidthMhz() const { return m_bandwidthMhz; }
    void setBandwidthMhz(double bw);

    int fftSize() const { return m_fftSize; }
    void setFftSize(int size);

    // --- Slice association ---

    // Associate a slice with this panadapter (overlay appears).
    void addSlice(int sliceId);

    // Remove a slice association from this panadapter.
    void removeSlice(int sliceId);

    // Set which slice is the active (tunable) slice on this pan.
    int activeSliceId() const { return m_activeSliceId; }
    void setActiveSliceId(int sliceId);

    QSet<int> associatedSlices() const { return m_associatedSlices; }

    // --- Title bar ---

    void updateTitle(const QString& sliceLetter, double freqMhz,
                     const QString& mode);
    void clearTitle();

    // --- CW decode panel ---

    void setCwPanelVisible(bool visible);
    void appendCwText(const QString& text);
    void clearCwText();

    QSize sizeHint() const override { return {800, 316}; }

signals:
    // Emitted when user clicks on this pan (makes it active).
    void activated(int panId);

    // Emitted when user clicks the close button on the title bar.
    void closeRequested(int panId);

    // Emitted when the user tunes by clicking/scrolling in the spectrum.
    void tuneRequested(int panId, double freqMhz);

    // Emitted when the user drags a filter edge.
    void filterChangeRequested(int panId, int lowHz, int highHz);

    // Emitted when the active slice on this pan changes.
    void activeSliceChanged(int panId, int sliceId);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    int             m_panId;
    double          m_centerMhz{14.225};
    double          m_bandwidthMhz{0.200};
    int             m_fftSize{4096};
    int             m_activeSliceId{-1};
    QSet<int>       m_associatedSlices;

    SpectrumWidget* m_spectrum{nullptr};
    QLabel*         m_titleLabel{nullptr};

    // CW decode panel
    QWidget*        m_cwPanel{nullptr};
    QTextEdit*      m_cwText{nullptr};
};

} // namespace NereusSDR
```

### Activation

Clicking anywhere in a PanadapterApplet emits `activated(panId)`. The
PanadapterStack forwards this to `setActivePan()`, which:

1. Updates `m_activePanId`
2. Emits `activePanChanged(panId)` to MainWindow
3. MainWindow updates the applet column to show controls for the active
   pan's active slice

The eventFilter on the PanadapterApplet detects `QEvent::MouseButtonPress`
on any child widget and emits `activated`.

---

## 3. wirePanadapter() -- Per-Pan Signal Routing

### Responsibility

`wirePanadapter()` is a private method on MainWindow that connects all
signals between a newly created PanadapterApplet and the rest of the
application. It is called once per pan creation (startup + runtime adds).

### Signal Routing Map

```
PanadapterApplet
  |
  +-- activated(panId) -----> PanadapterStack::setActivePan()
  +-- closeRequested(panId) -> MainWindow: remove pan if count > 1
  |
  +-- SpectrumWidget
        |
        +-- frequencyClicked(mhz) --> SliceModel::setFrequency()
        |                             (on active slice of this pan)
        |
        +-- filterChangeRequested(lo, hi) --> SliceModel::setFilterWidth()
        |                                     (on active slice)
        |
        +-- bandwidthChangeRequested(bw) --> PanadapterApplet::setBandwidthMhz()
        |                                    (client-side, no radio command)
        |
        +-- centerChangeRequested(center) --> PanadapterApplet::setCenterMhz()
        |                                     (client-side, no radio command)
        |
        +-- dbmRangeChangeRequested(min, max) --> SpectrumWidget display update
        |                                          (client-side only)
        |
        +-- tnfCreateRequested(mhz) ----> TnfModel::createTnf()
        +-- tnfMoveRequested(id, mhz) --> TnfModel::setTnfFreq()
        +-- tnfRemoveRequested(id) -----> TnfModel::removeTnf()
        |
        +-- sliceClicked(sliceId) ------> MainWindow::setActiveSlice()
        |
        +-- spotTriggered(idx) ---------> MainWindow: tune to spot frequency
```

### Key Difference from AetherSDR

In AetherSDR, `wirePanadapter()` routes many actions to radio commands because
the radio manages pan state (`display pan set <id> bandwidth=...`). In
NereusSDR, pan display state is entirely client-side:

| Action | AetherSDR | NereusSDR |
|--------|-----------|-----------|
| Change bandwidth | Radio command | Client-side `setBandwidthMhz()` |
| Change center freq | Radio command | Client-side `setCenterMhz()` |
| Change dBm range | Radio command | Client-side SpectrumWidget update |
| Change FFT average | Radio command + client | Client-side only |
| Click-to-tune | `slice m <freq> pan=<id>` | `SliceModel::setFrequency()` |
| Filter drag | `SliceModel::setFilterWidth()` | Same (client-side DSP) |

### Implementation Sketch

```cpp
void MainWindow::wirePanadapter(PanadapterApplet* applet)
{
    auto* sw = applet->spectrumWidget();

    // Pan activation
    connect(applet, &PanadapterApplet::activated,
            m_panStack, &PanadapterStack::setActivePan);

    // Close pan (don't close the last one)
    connect(applet, &PanadapterApplet::closeRequested,
            this, [this](int panId) {
        if (m_panStack->count() <= 1) { return; }
        m_panStack->removePanadapter(panId);
        // Re-route FFT data away from removed pan
        m_fftRouter.removePan(panId);
    });

    // Click-to-tune: route to the active slice on this pan
    connect(sw, &SpectrumWidget::frequencyClicked,
            this, [this, applet](double mhz) {
        int sliceId = applet->activeSliceId();
        if (sliceId < 0) { return; }
        if (auto* s = m_radioModel.slice(sliceId)) {
            s->setFrequency(mhz);
        }
    });

    // Filter drag: route to the active slice on this pan
    connect(sw, &SpectrumWidget::filterChangeRequested,
            this, [this, applet](int lo, int hi) {
        int sliceId = applet->activeSliceId();
        if (sliceId < 0) { return; }
        if (auto* s = m_radioModel.slice(sliceId)) {
            s->setFilterWidth(lo, hi);
        }
    });

    // Bandwidth/center changes are client-side only
    connect(sw, &SpectrumWidget::bandwidthChangeRequested,
            applet, &PanadapterApplet::setBandwidthMhz);
    connect(sw, &SpectrumWidget::centerChangeRequested,
            applet, &PanadapterApplet::setCenterMhz);

    // TNF signals to TnfModel
    auto* tnf = &m_radioModel.tnfModel();
    QPointer<SpectrumWidget> swGuard(sw);
    auto rebuildTnfMarkers = [this, swGuard]() {
        if (!swGuard) { return; }
        // ... rebuild TNF markers from TnfModel
    };
    connect(tnf, &TnfModel::tnfChanged, this, rebuildTnfMarkers);
    connect(sw, &SpectrumWidget::tnfCreateRequested, tnf, &TnfModel::createTnf);
    connect(sw, &SpectrumWidget::tnfMoveRequested,   tnf, &TnfModel::setTnfFreq);
    connect(sw, &SpectrumWidget::tnfRemoveRequested,  tnf, &TnfModel::removeTnf);

    // Spot markers
    auto* spots = &m_radioModel.spotModel();
    auto rebuildSpots = [this, swGuard]() {
        if (!swGuard) { return; }
        // ... rebuild spot markers from SpotModel
    };
    connect(spots, &SpotModel::spotAdded,   this, rebuildSpots);
    connect(spots, &SpotModel::spotRemoved, this, rebuildSpots);

    // Tuning step size sync
    connect(m_appletPanel->rxApplet(), &RxApplet::stepSizeChanged,
            sw, &SpectrumWidget::setStepSize);

    // Per-pan display settings from overlay menu
    auto* menu = sw->overlayMenu();
    connect(menu, &SpectrumOverlayMenu::fftAverageChanged,
            sw, &SpectrumWidget::setFftAverage);
    connect(menu, &SpectrumOverlayMenu::fftFpsChanged,
            sw, &SpectrumWidget::setFftFps);
    connect(menu, &SpectrumOverlayMenu::wfColorSchemeChanged,
            sw, &SpectrumWidget::setWfColorScheme);
}
```

### Disconnect-Before-Removal Pattern

When removing a panadapter, all signals MUST be disconnected before the
widget is destroyed. Failure to do this causes crashes in lambda captures
that reference the deleted SpectrumWidget.

```cpp
// In panadapterRemoved handler:
if (auto* applet = m_panStack->panadapter(panId)) {
    if (auto* sw = applet->spectrumWidget()) {
        sw->disconnect(this);          // disconnect all signals to MainWindow
        sw->disconnect(m_panStack);    // disconnect all signals to PanadapterStack
    }
    applet->disconnect(this);
    applet->disconnect(m_panStack);
}
m_panStack->removePanadapter(panId);
```

This follows AetherSDR's proven pattern from issue #242.

---

## 4. Slice-to-Pan Association

### Design

The OpenHPSDR radio has no concept of panadapters or slices. These are
entirely client-side abstractions:

- **Slice:** A virtual receiver with a frequency, mode, filter, and DSP
  settings. Maps to a WDSP channel.
- **Panadapter:** A visual display showing a frequency range with spectrum
  and waterfall. One or more slices can be overlaid on a panadapter.

### Association Rules

1. Each slice can appear on **one or more** panadapters (its VFO marker and
   filter passband are drawn as overlays).
2. Each panadapter has exactly **one active slice** -- this is the slice that
   receives tune, filter, and mode commands from user interaction on that pan.
3. When a new slice is created, it is automatically associated with the
   currently active panadapter.
4. A slice can be moved to a different pan via drag-and-drop or a context
   menu action.

### Automatic Center Tracking

When the active slice on a panadapter is tuned (frequency changes), the pan
center frequency tracks the slice if the VFO marker would move off-screen:

```cpp
void PanadapterApplet::onActiveSliceFrequencyChanged(double freqMhz)
{
    double halfBw = m_bandwidthMhz / 2.0;
    double leftEdge = m_centerMhz - halfBw;
    double rightEdge = m_centerMhz + halfBw;

    // Margin: keep VFO at least 10% from edge
    double margin = m_bandwidthMhz * 0.10;

    if (freqMhz < leftEdge + margin || freqMhz > rightEdge - margin) {
        setCenterMhz(freqMhz);  // re-center on the VFO
    }
}
```

### SliceModel Integration

SliceModel emits signals when its state changes. MainWindow routes these
to every PanadapterApplet that has the slice associated:

```cpp
// In MainWindow, when a slice's frequency changes:
void MainWindow::onSliceFrequencyChanged(int sliceId, double freqMhz)
{
    for (auto* applet : m_panStack->allApplets()) {
        if (applet->associatedSlices().contains(sliceId)) {
            applet->spectrumWidget()->setSliceOverlayFreq(sliceId, freqMhz);
        }
        // Auto-center if this is the active slice on this pan
        if (applet->activeSliceId() == sliceId) {
            applet->onActiveSliceFrequencyChanged(freqMhz);
        }
    }
}
```

### Multi-Slice Overlay

Each panadapter can display overlays for multiple slices simultaneously.
The overlay system draws:

- **VFO marker:** Vertical line at the slice frequency (orange for active,
  gray for inactive slices)
- **Filter passband:** Semi-transparent rectangle from filterLow to filterHigh
  relative to VFO frequency
- **Slice label:** Letter (A/B/C/D) near the top of the VFO marker

Clicking on an inactive slice's VFO marker makes that slice active on the
clicked pan (emits `sliceClicked(sliceId)`).

---

## 5. FFT Data Routing

### FFTRouter

Since the client computes FFT data from raw I/Q, each panadapter needs its
own FFT output routed to it. The FFTRouter manages this mapping:

```cpp
// Routes FFT output frames to the correct PanadapterApplet(s).
// Each receiver channel produces FFT data; each panadapter subscribes
// to one receiver channel for its spectrum/waterfall feed.
class FFTRouter : public QObject {
    Q_OBJECT

public:
    // Associate a panadapter with a receiver's FFT output.
    void mapPanToReceiver(int panId, int receiverId);

    // Remove a pan from routing.
    void removePan(int panId);

    // Remove a receiver from routing (disconnects all pans watching it).
    void removeReceiver(int receiverId);

public slots:
    // Called by FFTEngine when a new frame is ready for a receiver.
    void onFftFrame(int receiverId, const QVector<float>& binsDbm);

private:
    // receiverId -> list of panIds that display this receiver's FFT
    QMap<int, QList<int>> m_receiverToPans;

    // panId -> PanadapterApplet* (set by MainWindow)
    QMap<int, PanadapterApplet*> m_panWidgets;
};
```

### Multiple Pans, Same Receiver

Two panadapters can show the same receiver's FFT data at different zoom
levels. The FFT is computed once per receiver; each pan applies its own
center/bandwidth windowing to extract and display the relevant portion.

### Multiple Pans, Different Receivers

In multi-receiver configurations (Protocol 1 supports up to 7 receivers),
each pan can be associated with a different receiver. Each receiver has its
own WDSP channel and FFT computation.

---

## 6. Startup and Layout Restore Sequence

```
1. MainWindow::init()
   |
   2. Read AppSettings: PanLayoutId, PanCount, per-pan state
   |
   3. Create PanadapterStack
   |
   4. For each saved pan (0..PanCount-1):
   |   a. PanadapterStack::addPanadapter(panId)
   |   b. wirePanadapter(applet)
   |   c. Restore per-pan settings (center, bandwidth, FFT size, etc.)
   |   d. Restore slice associations from AppSettings
   |
   5. PanadapterStack::applyLayout(layoutId, panIds)
   |
   6. PanadapterStack::restoreSplitterState()
   |
   7. FFTRouter: map each pan to its receiver
   |
   8. If no saved state: create single pan with slice 0, layout "1"
```

### Runtime Layout Change

When the user selects a different layout from the toolbar:

```
1. Save current splitter state
2. If new layout needs more pans: create additional PanadapterApplets
3. If new layout needs fewer pans: remove excess (disconnect first!)
4. PanadapterStack::applyLayout(newLayoutId, panIds)
5. Save new layout ID to AppSettings
```

---

## 7. Key Differences from AetherSDR Summary

| Aspect | AetherSDR | NereusSDR |
|--------|-----------|-----------|
| Pan IDs | Radio-assigned hex (`0x40000000`) | Client-assigned int (0, 1, 2, 3) |
| Pan creation | Radio command `display pan create` | Client-side only |
| Pan removal | Radio command `display pan remove` | Client-side only |
| FFT data source | Radio streams VITA-49 FFT packets | Client computes from raw I/Q |
| Waterfall source | Radio streams waterfall tiles | Client generates from FFT frames |
| Center/BW changes | Radio command `display pan set` | Client-side state update |
| Slice-to-pan | Radio assigns via `pan=` field | Client manages association |
| Multiple pans | Radio duplicates DSP effort | Client duplicates FFT effort |
| Pan settings | Radio-authoritative | Client-authoritative (AppSettings) |
