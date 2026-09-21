// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-native model, no Thetis or AetherSDR
// equivalent. AetherSDR is a thin FlexRadio SmartSDR client and has no
// concept of a shared display-settings model; Thetis keeps this state
// scattered across console.cs/display.cs fields with no consolidating
// class either. See the 3D Stacked-Trace Spectrum Plan Task 17 design
// note below for the actual source of this file's shape.
//
// NereusSDR - DisplaySettingsModel.
//
// WHY THIS CLASS EXISTS (3D Stacked-Trace Spectrum Plan, Task 17):
//
// Two UI surfaces edit the same fourteen panadapter display settings:
// the right-click SpectrumOverlayMenu popup and Setup -> Display (spread
// across SpectrumDefaultsPage / WaterfallDefaultsPage / GridScalesPage /
// Display3DSetupPage). Historically each surface wrote straight to
// SpectrumWidget's own members and the two kept in sync point-to-point,
// which needed a hand-written `m_updatingFromModel` guard bool wherever a
// surface also had to follow the other one live (Display3DSetupPage is
// the one example that existed before this class). A third surface (a
// left-panel Display applet) is the next task in this plan; three
// surfaces wired point-to-point is a lot of hand-maintained bindings, and
// nothing stops a future editor from adding a fourth without the guard.
//
// This model exists so every surface has exactly ONE binding: to the
// model, never to another surface or straight to SpectrumWidget. Each of
// the fourteen values gets its own setter and its own `xxxChanged`
// signal, and every setter has the same shape:
//
//     if (m_field == clampedValue) { return; }   // absorbs a re-applied
//     m_field = clampedValue;                    // echo before it can
//     emit fieldChanged(m_field);                // bounce a second time
//
// That equality guard is what makes an echo loop structurally
// impossible rather than something a future editor has to remember: two
// widgets bound to the same model field can drive each other through at
// most one extra hop (new value -> model updates + emits once -> the
// OTHER widget's setValue()-with-the-same-value is a Qt no-op) and the
// chain always terminates on the guard above, not on a bespoke bool a
// human has to thread through every call site.
//
// RELATIONSHIP TO SpectrumWidget: SpectrumWidget still owns the actual
// render-time state (its own m_wfColorGain, m_refLevel, ... members) and
// still does its own loadSettings()/saveSettings() round trip against the
// exact same AppSettings keys this class uses -- that is unchanged and
// deliberately so; ripping out SpectrumWidget's internal rendering state
// is not in scope here and would be a much bigger, riskier change for no
// behavioural benefit. What changed is which object the TWO EXISTING
// SURFACES talk to: they now call through this model, which is wired
// bidirectionally to SpectrumWidget's existing setters/signals so a
// change lands on the live renderer exactly as before. See
// SpectrumWidget.cpp's constructor for the wiring and
// docs/architecture/2026-08-08-3d-stacked-trace-spectrum-plan.md Task 17
// for the full design note.
//
// THE ONE FIELD THAT IS NOT LIKE THE OTHERS -- 3D Floor (dssFloorDepth):
//
// As of Task 18, all fourteen fields on this class have the same SHAPE:
// a plain in-memory value on this instance, a clamped equality-guarded
// setter, a changed signal. What still makes 3D Floor different is
// persistence, not shape. The other thirteen are keyed by panIndex() and
// round-trip through this model's own load()/save(). 3D Floor does not:
// it is anchored to the measured noise floor, which is a property of the
// BAND being listened to, not of which panadapter widget happens to be
// drawing it -- switch bands and the floor should switch with it,
// independent of which pan you are looking at. Its persistence and
// per-band recall live entirely on PanadapterModel
// (dss3DFloorDepthForBand / setDss3DFloorDepthForBand, key
// "Display3DFloorDepth_<bandKeyName>"), reached through the existing
// MainWindow bridge (MainWindow::wireDss3DFloorRecallForTest), which
// keeps the live value honest across a band change by driving it from
// SpectrumWidget's own dssFloorDepthChanged signal. dssFloorDepth() on
// this class is the same kind of runtime mirror SpectrumWidget's own
// m_dssFloorDepth already was before this task -- kept in sync by that
// bridge and, as of Task 18, by the two-way binding in
// SpectrumWidget::bindDisplaySettings() as well, never by this class's
// own load()/save(), which explicitly skip it (see the comment at each).
//
// Task 17's dssFloorDepth(Band)/setDssFloorDepth(Band, int) API, keyed by
// Band with no panIndex involved at all, is removed along with its
// second writer to PanadapterModel's per-band key. It existed as a
// placeholder for "a future band-aware caller" that turned out not to be
// how Task 18 actually needed to bind: SpectrumWidget already has its
// own per-pan dssFloorDepth(int)/setDssFloorDepth(int)/
// dssFloorDepthChanged(int) mirror, and this model binds to THAT, the
// same per-pan shape as the other thirteen fields, not a Band-keyed one.
//
// SCOPE NOTE (Task 18): SpectrumWidget is, as of this task, the only
// surface bound to this model -- one model per widget, created in the
// widget's constructor. The right-click popup and Setup -> Display still
// talk straight to SpectrumWidget for now; the plan stages them onto
// this model starting with Task 20 (popup) and Task 21 (Setup), with a
// new left-panel applet following in Task 22. Persistence itself stays
// owned by SpectrumWidget's loadSettings()/saveSettings() until Task 23
// moves it onto this model last, behind a golden test.
//
// Modification history (NereusSDR)
//   Created 2026-08-09 by J.J. Boyd / KG4VCF, 3D Stacked-Trace Spectrum
//     Plan Task 17. AI tooling: Claude Code.

#pragma once

#include <QObject>

namespace NereusSDR {

// A QObject owning the fourteen panadapter display values described in
// the file header above. One setter + one changed signal per value.
// Every setter clamps to the same range the existing UI sliders already
// enforce and is a no-op (no member write, no signal) when the incoming
// value already matches what is stored -- see the class-header comment
// for why that single property is what makes the echo problem
// structurally impossible.
//
// Thirteen of the fourteen fields are scoped by panIndex() AND persisted
// by load()/save(), mirroring SpectrumWidget's own per-pan settings
// convention (pan 0 -> bare key, pan N -> "<key>_N"). Call setPanIndex()
// before load()/save() to target a specific panadapter's settings; the
// default is pan 0. dssFloorDepth is the exception: it is still an
// ordinary in-memory field on this instance like the other thirteen, but
// load()/save() never touch it -- see "THE ONE FIELD THAT IS NOT LIKE
// THE OTHERS" above for why.
class DisplaySettingsModel : public QObject {
    Q_OBJECT

public:
    explicit DisplaySettingsModel(QObject* parent = nullptr);

    // ---- Panadapter scope (not one of the fourteen; controls which
    //      pan's keys load()/save() and the thirteen per-pan setters
    //      below target). Mirrors SpectrumWidget::setPanIndex()/panIndex().
    void setPanIndex(int idx) { m_panIndex = idx; }
    int  panIndex() const { return m_panIndex; }

    // ---- Waterfall ----

    // WfColorScheme enum value (see SpectrumWidget.h). Plain int, not the
    // strong enum: this class deliberately does not depend on gui/
    // headers, and SpectrumWidget::setSpectrumRenderMode(int) already
    // establishes the "enum interchange as int" convention this class
    // follows for both of its enum-backed fields. Valid range [0,7]
    // (WfColorScheme::Count == 8).
    int  wfColorScheme() const { return m_wfColorScheme; }
    void setWfColorScheme(int scheme);

    int  wfColorGain() const { return m_wfColorGain; }
    void setWfColorGain(int gain);

    int  wfBlackLevel() const { return m_wfBlackLevel; }
    void setWfBlackLevel(int level);

    // ---- Spectrum ----

    // Top of display, dBm. Range [-180, 80] -- widened in Task 18 from
    // the popup slider's [-160, 20]. SpectrumOverlayMenu's m_refLevelSlider
    // is not the widest source: SpectrumWidget's Task 19 Ctrl-drag gesture
    // (mouseMoveEvent's m_draggingDbmRange branch, via
    // clampDbmRangeForBottom) can legitimately drive refLevel up to
    // upstream's own kMaxDisplayDbm = 80.0f (AetherSDR SpectrumWidget.cpp:338
    // [@1872028c]) -- see tst_dbm_range_drag.cpp's
    // ctrlDragRange_clampsAtMaximum, which pins exactly that. Same
    // never-narrower-than-any-widget-write-path rule as Dyn Range above.
    float refLevel() const { return m_refLevel; }
    void  setRefLevel(float dBm);

    // Depth of the visible window, dB. Range [10, 200] -- widened in
    // Task 18 from the popup slider's [20, 160] to match
    // SpectrumWidget::wheelEvent's qBound(10.0f, ..., 200.0f), the widest
    // bound any widget write path accepts (model clamps must never be
    // narrower than any widget write path, or a round trip through the
    // model would narrow a value the widget itself allows).
    float dynamicRange() const { return m_dynamicRange; }
    void  setDynamicRange(float dB);

    // 0.0 .. 1.0.
    float fillAlpha() const { return m_fillAlpha; }
    void  setFillAlpha(float alpha);

    bool panFill() const { return m_panFill; }
    void setPanFill(bool on);

    // Waterfall/spectrum vertical split, clamped [0.10, 0.90] -- same
    // bound SpectrumWidget's own drag handler enforces (SpectrumWidget.cpp,
    // search m_spectrumFrac = std::clamp(...)). Not currently bound to
    // either existing UI surface (neither exposes a control for it), but
    // owned here so the model's claim to all fourteen values is honest.
    float spectrumFrac() const { return m_spectrumFrac; }
    void  setSpectrumFrac(float frac);

    // ---- 3D ----

    // SpectrumRenderMode enum value (Mode2D=0, Mode3D=1). Plain int for
    // the same reason as wfColorScheme above.
    int  spectrumRenderMode() const { return m_spectrumRenderMode; }
    void setSpectrumRenderMode(int mode);

    // Per-pan runtime mirror, NOT persisted by load()/save() -- see the
    // file header comment ("THE ONE FIELD THAT IS NOT LIKE THE OTHERS")
    // for why 3D Floor keeps its persistence on PanadapterModel instead.
    int  dssFloorDepth() const { return m_dssFloorDepth; }
    void setDssFloorDepth(int depthDb);

    int  dssGain() const { return m_dssGain; }
    void setDssGain(int pct);

    int  dssRowSpan() const { return m_dssRowSpan; }
    void setDssRowSpan(int pct);

    int  dssAngle() const { return m_dssAngle; }
    void setDssAngle(int pct);

    bool threeDSliceDepth() const { return m_threeDSliceDepth; }
    void setThreeDSliceDepth(bool on);

    // ---- Persistence ----
    // Explicit, like SpectrumWidget::loadSettings()/saveSettings(): the
    // thirteen per-pan setters above do NOT auto-persist on every call.
    // load() populates every field (including seeding, but not writing,
    // ship defaults when a key is absent) from AppSettings for the
    // current panIndex(), following the same pan-0-fallback-inheritance
    // rule SpectrumWidget::loadSettings() uses (an untouched pan N reads
    // pan 0's value until it is given one of its own). save() writes the
    // current in-memory value of every field to AppSettings for the
    // current panIndex(). Neither touches dssFloorDepth: it is a live
    // runtime mirror only (see the file header); its persistence stays
    // entirely on PanadapterModel.
    void load();
    void save();

signals:
    void wfColorSchemeChanged(int scheme);
    void wfColorGainChanged(int gain);
    void wfBlackLevelChanged(int level);
    void refLevelChanged(float dBm);
    void dynamicRangeChanged(float dB);
    void fillAlphaChanged(float alpha);
    void panFillChanged(bool on);
    void spectrumFracChanged(float frac);
    void spectrumRenderModeChanged(int mode);
    void dssFloorDepthChanged(int depthDb);
    void dssGainChanged(int pct);
    void dssRowSpanChanged(int pct);
    void dssAngleChanged(int pct);
    void threeDSliceDepthChanged(bool on);

private:
    int m_panIndex{0};

    int   m_wfColorScheme{0};      // WfColorScheme::Default
    int   m_wfColorGain{45};
    int   m_wfBlackLevel{104};
    float m_refLevel{-48.0f};
    float m_dynamicRange{68.0f};
    float m_fillAlpha{0.70f};
    bool  m_panFill{true};
    float m_spectrumFrac{0.40f};
    int   m_spectrumRenderMode{0}; // SpectrumRenderMode::Mode2D
    int   m_dssFloorDepth{6};      // ship default; matches SpectrumWidget's own m_dssFloorDepth
    int   m_dssGain{70};
    int   m_dssRowSpan{100};
    int   m_dssAngle{50};
    bool  m_threeDSliceDepth{false};
};

} // namespace NereusSDR
