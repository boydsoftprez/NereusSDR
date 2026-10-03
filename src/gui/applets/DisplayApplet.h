// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-native applet, no Thetis or AetherSDR
// equivalent. Neither upstream has a left-panel applet surface for
// display controls (see DisplaySettingsModel.h's own header for why
// no consolidating model exists upstream either). This applet composes
// already-attributed NereusSDR infrastructure -- AppletWidget (AetherSDR
// structural derivative), GuardedSlider/GuardedComboBox (AetherSDR
// structural derivative), ComboStyle -- and copies label/range/tooltip
// TEXT verbatim from SpectrumOverlayMenu.cpp (itself already attributed
// to AetherSDR in its own header) rather than porting new logic from
// either upstream. See the 3D Stacked-Trace Spectrum Plan Task 22 design
// note below for the full rationale.
//
// NereusSDR - DisplayApplet: left-panel display controls.
//
// WHY THIS CLASS EXISTS (3D Stacked-Trace Spectrum Plan, Task 22):
//
// Third of three surfaces that edit the fifteen per-pan display values
// on DisplaySettingsModel (Task 17): the right-click SpectrumOverlayMenu
// popup (Task 20) and Setup -> Display (Task 21) already bind to the
// model; this applet is the operator's decision (design doc addendum 2,
// transcript 2026-08-09) that "the display widget on the left" carries
// "Everything, applet becomes primary" -- every control that makes sense
// in a compact panel, all fifteen values.
//
// Three sections, titled exactly as the popup's own section headers:
// Waterfall (Color Scheme, Color Gain, Black Level), Spectrum (Ref
// Level, Dyn Range, Fill Alpha, Fill trace toggle, Split -- Split has no
// popup counterpart), 3D VIEW (Spectrum mode, 3D Floor, 3D Gain, 3D
// Span, 3D Angle, 3D Speed, Slice Shadow, plus a Reset 3D button with no
// popup counterpart). Labels, ranges and tooltips for the fourteen
// popup-sourced controls are copied verbatim from SpectrumOverlayMenu.cpp;
// Split and the Reset 3D button are NereusSDR-original additions per
// design doc section 6.
//
// BINDING: follows the active panadapter (design doc decision 6). The
// constructor binds to RadioModel::spectrumWidget()->displaySettings()
// when a widget is already set, and always connects
// RadioModel::spectrumWidgetChanged (added by this task) to rebind
// later -- MainWindow constructs this applet from populateDefaultMeter(),
// before RadioModel::setSpectrumWidget() is called later in the same
// constructor, so the initial bind is commonly deferred to that signal.
// bindTo() deletes and recreates m_bindContext on every call: since Qt
// disconnects any connection whose receiver/context object is destroyed,
// this one delete severs every reflect AND push connection made against
// the previous model in one shot, so a rebind can never leave a control
// still driving the old (now inactive) panadapter's model.
//
// Modification history (NereusSDR)
//   Created 2026-09-20 by J.J. Boyd / KG4VCF, 3D Stacked-Trace Spectrum
//     Plan Task 22. AI tooling: Claude Code.

#pragma once

#include "AppletWidget.h"

class QComboBox;
class QSlider;
class QCheckBox;
class QPushButton;

namespace NereusSDR {

class DisplaySettingsModel;

class DisplayApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit DisplayApplet(RadioModel* model, QWidget* parent = nullptr);
    ~DisplayApplet() override = default;

    QString appletId()    const override { return QStringLiteral("Display"); }
    QString appletTitle() const override { return QStringLiteral("Display"); }
    void syncFromModel() override;

    // Test seams (Task 22). Exposed so tst_display_applet can verify
    // wiring/values without depending on widget geometry. Concrete
    // runtime objects are GuardedSlider/GuardedComboBox (constructed in
    // buildUI()); the accessors return the Qt base type, matching the
    // forward-declared-pointer convention every other applet header uses
    // (see RadeApplet.h).
    QComboBox*   colorSchemeComboForTest()  const { return m_colorSchemeCombo; }
    QSlider*     colorGainSliderForTest()   const { return m_colorGainSlider; }
    QSlider*     blackLevelSliderForTest()  const { return m_blackLevelSlider; }
    QSlider*     refLevelSliderForTest()    const { return m_refLevelSlider; }
    QSlider*     dynRangeSliderForTest()    const { return m_dynRangeSlider; }
    QSlider*     fillAlphaSliderForTest()   const { return m_fillAlphaSlider; }
    QCheckBox*   fillTraceCheckForTest()    const { return m_fillTraceCheck; }
    QSlider*     splitSliderForTest()       const { return m_splitSlider; }
    QComboBox*   spectrumModeComboForTest() const { return m_spectrumModeCombo; }
    QSlider*     floorSliderForTest()       const { return m_floorSlider; }
    QSlider*     gainSliderForTest()        const { return m_gainSlider; }
    QSlider*     spanSliderForTest()        const { return m_spanSlider; }
    QSlider*     angleSliderForTest()       const { return m_angleSlider; }
    QSlider*     speedSliderForTest()       const { return m_speedSlider; }
    QLabel*      speedValueLabelForTest()   const { return m_speedValueLabel; }
    QCheckBox*   sliceShadowCheckForTest()  const { return m_sliceShadowCheck; }
    QPushButton* reset3dButtonForTest()     const { return m_reset3dButton; }

private:
    void buildUI();
    void bindTo(DisplaySettingsModel* m);

    DisplaySettingsModel* m_bound{nullptr};
    // Recreated on every bindTo() call; deleting it drops every
    // connection (reflect AND push) made against the previously bound
    // model in one shot. See the class-header comment.
    QObject* m_bindContext{nullptr};

    // ---- Waterfall ----
    QComboBox* m_colorSchemeCombo{nullptr};
    QSlider*   m_colorGainSlider{nullptr};
    QLabel*    m_colorGainValue{nullptr};
    QSlider*   m_blackLevelSlider{nullptr};
    QLabel*    m_blackLevelValue{nullptr};

    // ---- Spectrum ----
    QSlider*   m_refLevelSlider{nullptr};
    QLabel*    m_refLevelValue{nullptr};
    QSlider*   m_dynRangeSlider{nullptr};
    QLabel*    m_dynRangeValue{nullptr};
    QSlider*   m_fillAlphaSlider{nullptr};
    QLabel*    m_fillAlphaValue{nullptr};
    QCheckBox* m_fillTraceCheck{nullptr};
    QSlider*   m_splitSlider{nullptr};
    QLabel*    m_splitValue{nullptr};

    // ---- 3D VIEW ----
    QComboBox*   m_spectrumModeCombo{nullptr};
    QSlider*     m_floorSlider{nullptr};
    QLabel*      m_floorValue{nullptr};
    QSlider*     m_gainSlider{nullptr};
    QLabel*      m_gainValue{nullptr};
    QSlider*     m_spanSlider{nullptr};
    QLabel*      m_spanValue{nullptr};
    QSlider*     m_angleSlider{nullptr};
    QLabel*      m_angleValue{nullptr};
    QSlider*     m_speedSlider{nullptr};
    QLabel*      m_speedValueLabel{nullptr};
    QCheckBox*   m_sliceShadowCheck{nullptr};
    QPushButton* m_reset3dButton{nullptr};
};

} // namespace NereusSDR
