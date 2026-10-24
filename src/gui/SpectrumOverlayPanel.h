// src/gui/SpectrumOverlayPanel.h
// Left overlay button strip for SpectrumWidget.
// Ported from AetherSDR SpectrumOverlayMenu — same visual style, adapted
// for NereusSDR's OpenHPSDR/Thetis feature set.
//
// 8 buttons (68×22px, stacked vertically) + 4 flyout sub-panels.
// Positioned via move() as a child of the spectrum widget.

// =================================================================
// src/gui/SpectrumOverlayPanel.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - Approved mode projections and chooser notes. J.J. Boyd,
//                 AI-assisted via OpenAI Codex.
//   2026-10-12 - Display-only workspace geometry refresh and retained
//                 toggle groups. J.J. Boyd, AI-assisted via OpenAI Codex.
//   2026-10-12 - Compact approved Display rows and parent label binding.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Levels tab added; saved Fft and View3D tokens retained.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-13 - Export the panel's Qt signal addresses from the Windows
//                 GUI DLL. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Ported from AetherSDR `src/gui/SpectrumOverlayMenu.{h,cpp}`
//                 (left button strip + 5 flyout panels).
//   2026-04-20 — Phase 3O Sub-Phase 9 Task 9.2c (issue #70 fold-in):
//                 added setRadioModel() so the previously-disabled VAX Ch
//                 combo on the left-edge overlay is now wired bidirectionally
//                 to the resolved pan slice's vaxChannel() with echo prevention. IQ Ch
//                 stays feature-flagged off (design spec §6.7/§11.3 —
//                 audio/SendIqToVax stored-but-not-active). J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-49: the RF Gain slider and WNB button (ANT flyout)
//                 and the IQ Ch combo (VAX flyout) are removed. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-49, R-R3-21: the ANT button is not shown on a board
//                 with no antenna choices, where its flyout would be empty.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-26 - Parity Task 18: the Display flyout's Grid Lines toggle
//                 reports gridVisibleChanged (it changed only its own label)
//                 and takes the pan's state through setGridVisible. J.J.
//                 Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-30 - TX rulings (item 3, JJ): setAttHeldReason, the ATT flyout
//                 held on a pan whose slice this window only listens to.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-10-10 - Display pill plan Task 16: the Display flyout is the
//                 four-tab Display pill (Spectrum, FFT, Waterfall, 3D View),
//                 bound to the pan's SpectrumWidget (setDisplayTarget). The
//                 old display signals and setGridVisible are gone. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-10-10 - Display pill plan Task 17: the Waterfall and FFT tabs.
//                 setClarityState replaces setClarityStatus;
//                 clarityEnabledRequested; DisplayPillFftState, setFftState
//                 and the five FFT request signals;
//                 setStationRowsAvailable. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-10-10 - Follow-up wave F4: addPillSlider takes a value width,
//                 for a value wider than the value column. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-10-10 - Follow-up wave F3: dB Max and dB Min are greyed while
//                 the pan is keyed (updateBandGridNote). J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-10-11 - addPillAvgTimeSlider: the averaging time rows are a 1 ms
//                 slider and a number box. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-10-11 - FFT sizing mode (design D38): fftSizingModeRequested and
//                 the three "Sizing:" buttons; the Hz/bin Off/On pill and
//                 the pill's own remembered target are gone. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-10-22 - Closed-pill geometry refreshes wait for the pill to open;
//                 unchanged remote/station gates and Clarity badge are
//                 skipped; displayPillGeometryRefreshCountForTest. J.J. Boyd
//                 (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#pragma once

#include "core/ControlRanges.h"
#include "gui/NereusGuiExport.h"

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QVector>
#include <functional>
#include <utility>

class QPushButton;
class QComboBox;
class QSlider;
class QLabel;
class QCheckBox;
class QSpinBox;
class QEvent;
class QMouseEvent;
class QGridLayout;
class QHBoxLayout;
class QTimer;

namespace NereusSDR {

struct BoardCapabilities;
class RadioModel;
class SliceModel;
class SpectrumWidget;

/// What the Display pill's FFT tab shows (display pill plan Task 17). These
/// are the station's settings, shared by every pan and window on the Core:
/// the pill shows them and asks for changes, and writes none itself.
struct DisplayPillFftState {
    int    baselineSize{0};        ///< the FFT Size slider's size (the stored DisplayFftSize), 4096 to 262144
    int    fps{0};
    int    windowType{0};          ///< ControlRanges::kDisplayFftWindows value
    double hzPerBinTarget{0.0};    ///< above 0 in the Hz/bin sizing mode only
    int    decimation{0};
    int    runningSize{0};         ///< the size this pan's FFT runs at now
    double sampleRateHz{0.0};      ///< this pan's sample rate
    /// Which control sets the running size (design D38): the zoom in Auto,
    /// the Hz/bin target in HzPerBin, baselineSize in Fixed.
    ControlRanges::DisplayFftSizingMode sizingMode{ControlRanges::kDisplayFftSizingModeDefault};
    /// The last Hz/bin target chosen, 0 when there has never been one: what
    /// a grayed Hz/bin control shows. hzPerBinTarget is above 0 in HzPerBin only.
    double lastHzPerBinTarget{0.0};
};

class NEREUS_GUI_EXPORT SpectrumOverlayPanel : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumOverlayPanel(QWidget* parent = nullptr);

    /// TX rulings (JJ, 2026-09-30, item 3): this pan's slice is one this
    /// window only listens to (another device controls it), so the ATT
    /// flyout's controls are disabled with `reason` and write nothing;
    /// empty lets them be. MainWindow sets it with the RX applet's access.
    void setAttHeldReason(const QString& reason);
    QString attHeldReason() const { return m_attHeldReason; }

    /// The panadapter this strip is drawn on. A control rendered on a pan acts
    /// on THAT pan -- the id travels with the signals rather than the consumer
    /// resolving an implicit "active" pan, so what a visible button does never
    /// depends on hidden state. Same shape as AetherSDR's SpectrumOverlayMenu,
    /// which carries m_panId and emits addRxClicked(m_panId)
    /// (SpectrumOverlayMenu.cpp:315 [@c6481cbf]).
    void setPanId(const QString& panId) { m_panId = panId; }
    QString panId() const { return m_panId; }

    // Bind this overlay panel to a RadioModel. Enables the VAX Ch combo
    // and wires it bidirectionally to the resolved pan slice. Safe to
    // call multiple times — each rebind drops prior SliceModel connections.
    void setRadioModel(RadioModel* model);

    using SliceResolver = std::function<SliceModel*()>;
    void setSliceResolver(SliceResolver resolver);
    void bindToPanSlice();

    // Raise panel and all flyouts above siblings.
    void raiseAll();

    /// Put the open flyout (the Display pill, BAND, ANT, VAX or ATT) back
    /// above its siblings on the pan. The pan's flags raise themselves on
    /// every placement pass (SpectrumWidget::flagsRaised), and without this
    /// a flag covered the open flyout and took its clicks. Does nothing
    /// while no flyout is open.
    void raiseOpenFlyout();

    // ── The Display pill (display pill plan Task 16) ─────────────────────
    // The flyout the strip's Display button opens: a tab strip over five
    // pages. Its per-pan rows read and write one SpectrumWidget directly.
    enum class DisplayPillTab { Spectrum, Levels, Waterfall, View3D, Fft };

    /// The pan the pill edits. Per-pan rows read and write this widget (and
    /// its DisplaySettingsModel) directly. A strip built on a SpectrumWidget
    /// starts with that widget as its target.
    void setDisplayTarget(SpectrumWidget* widget);
    SpectrumWidget* displayTarget() const { return m_displayTarget; }

    /// Show the pill if it is hidden. The pill seeds itself from the target
    /// (writing nothing) and follows it while it is open.
    void openDisplayPill();
    bool displayPillVisible() const;

    DisplayPillTab displayPillTab() const { return m_pillTab; }
    /// Show a tab and remember it (DisplayPillLastTab, this computer).
    void setDisplayPillTab(DisplayPillTab tab);

    /// The dB Max and dB Min rows show the band's stored values, not the
    /// pan's live range (which the noise floor may be moving). Moving them
    /// asks with perBandDbMaxRequested / perBandDbMinRequested; the pill
    /// writes nothing itself.
    void setBandGrid(const QString& bandLabel, int dbMax, int dbMin);

    /// What the pan's renderer can draw: 3D Span and Slice Shadow stay
    /// visible and are disabled when it cannot.
    void setGpuCapabilities(bool rowSpanSupported, bool sliceShadowSupported);

    /// A remote window: Cal Offset is the Core's to set. The decimation
    /// flag is kept for the FFT tab.
    void setRemoteWindow(bool remote, bool coreOffersDecimation);
    bool remoteWindow() const { return m_remoteWindow; }
    bool coreOffersDecimation() const { return m_coreOffersDecimation; }

    // ── Waterfall and FFT tabs (display pill plan Task 17) ───────────────
    /// Runtime Clarity status for this pan. The bound target's persistent
    /// clarityEnabled() owns the selected mode; `enabled` is a fallback
    /// only when no target is bound. `active` and `paused` drive the badge.
    /// Showing this status writes no settings and does not change the mode.
    void setClarityState(bool enabled, bool active, bool paused);

    using DisplayPillFftState = NereusSDR::DisplayPillFftState;
    /// Show the station's FFT settings. Nothing is asked for in return.
    void setFftState(const DisplayPillFftState& state);
    const DisplayPillFftState& fftState() const { return m_fftState; }

    /// The station's rows (every FFT row, and dB Max and dB Min) can be
    /// changed, or cannot, with the reason shown ("Connect to the Core to
    /// change these.").
    void setStationRowsAvailable(bool available, const QString& reason);

    /// Test seams: the colour and picture choosers the pill's buttons open.
    /// The colour chooser is given the colour the dialog starts on and its
    /// title, and returns the chosen colour (invalid = cancelled). The
    /// picture chooser is given the folder to start in and returns the
    /// chosen file (empty = cancelled).
    using PillColorChooser =
        std::function<QColor(const QColor& initial, const QString& title)>;
    using PillPictureChooser = std::function<QString(const QString& startDir)>;
    void setPillColorChooser(PillColorChooser chooser) {
        m_pillColorChooser = std::move(chooser);
    }
    void setPillPictureChooser(PillPictureChooser chooser) {
        m_pillPictureChooser = std::move(chooser);
    }
    /// Test seam: how many times the Display pill's geometry has been
    /// refreshed (refreshDisplayPillGeometry ran past its guards).
    int displayPillGeometryRefreshCountForTest() const { return m_pillGeometryRefreshes; }

public slots:
    // Phase 3P-I-a T18 — repopulate antenna combos from caps and hide
    // both RX/TX rows on boards without Alex (HL2/Atlas). Also reseeds
    // the combo's current value from the resolved slice so the label matches the
    // new port list (e.g. a persisted ANT3 preserves after reconnect).
    void setBoardCapabilities(const NereusSDR::BoardCapabilities& caps);

signals:
    // Band flyout
    void bandSelected(const QString& bandName, double freqHz, const QString& mode);

    // Display pill
    /// "Setup defaults" in the pill's footer. The page follows the open
    /// tab: "Spectrum Defaults" (Spectrum and FFT), "Waterfall Defaults",
    /// "3D View".
    void openSetupRequested(const QString& page); // B8 Task 24
    /// "Reset this pan" in the pill's footer.
    void resetPanRequested();
    /// The dB Max / dB Min rows were moved: the band's stored value is
    /// asked to change (see setBandGrid).
    void perBandDbMaxRequested(int db);
    void perBandDbMinRequested(int db);
    /// The Clarity On pill was clicked. The pill shows what setClarityState
    /// says, not what was asked.
    void clarityEnabledRequested(bool on);
    /// The FFT tab's rows were moved. The pill writes nothing itself.
    /// The FFT Size slider asks once per drag (on release) and once per run
    /// of keyboard steps (300 ms after the last).
    void fftBaselineRequested(int size);
    /// One of the three "Sizing:" buttons was chosen (design D38). The
    /// second value is the Hz/bin target to start from when the station
    /// remembers none: the bin width this pan runs now, rounded up to the
    /// control's step, so entering Hz/bin does not change the running size.
    /// 0 when this pan has nothing running.
    void fftSizingModeRequested(NereusSDR::ControlRanges::DisplayFftSizingMode mode,
                                double hzPerBinIfNoneRemembered);
    void fftFpsRequested(int fps);
    void fftWindowRequested(int windowType);
    void hzPerBinTargetRequested(double hz);
    void decimationRequested(int decimation);

    // Zoom buttons
    void zoomSegment();
    void zoomBand();
    void zoomOut();
    void zoomIn();

    // Collapse
    void collapsed(bool isCollapsed);

    // Clarity adaptive tuning (Phase 3G-9c)
    void clarityRetuneRequested();

    /// Add an RX slice on the pan this strip belongs to. Carries the pan id so
    /// the consumer never has to guess which pan the operator meant.
    void addRxClicked(const QString& panId);

    /// Add a notch on the pan this strip belongs to. Carries the pan id for
    /// the same reason addRxClicked does: a control rendered on a pan acts on
    /// THAT pan, never on whichever pan is implicitly "active".
    ///
    /// A pure signal. The notch centre is composed by NotchModel and the add
    /// is issued by MainWindow, so no DSP logic lands in this file.
    void addTnfClicked(const QString& panId);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

public:
    // Reposition zoom buttons to bottom-left of parent spectrum widget.
    // Must be called after parent resize.
    void repositionZoomButtons();

private:
    /// Which panadapter this strip is drawn on; see setPanId.
    QString m_panId;

    // Layout
    void updateLayout();
    void toggle();
    void buildZoomButtons();

    // Flyout builders
    void buildBandFlyout();
    void buildAntFlyout();
    void buildDisplayFlyout();
    void buildVaxFlyout();
    void buildAttFlyout();
    // R-R3-21: show the step attenuator (RadioModel::stepAttFacade(), the
    // same object the RX applet's ATT row and Setup use, local or remote).
    void showAttValues();

    // Flyout toggles
    void toggleBandFlyout();
    void toggleAntFlyout();
    void toggleDisplayFlyout();
    void toggleVaxFlyout();
    void toggleAttFlyout();

    // Auto-close helper
    void hideFlyout();

    // A flyout owns the mouse over its whole rectangle: nothing pressed,
    // moved, wheeled or right-clicked inside it reaches the pan under it.
    // Applied where each flyout's root is built.
    static void claimMouseForFlyout(QWidget* flyout);
    // The press that closed a flyout by landing outside it. On this strip's
    // own pan that press, and the moves, release and context menu that
    // follow it, are taken before the pan sees them.
    struct ClosingPress {
        bool            valid{false};
        quint64         timestamp{0};
        QPoint          globalPos;
        Qt::MouseButton button{Qt::NoButton};
    };
    bool isClosingPress(const QMouseEvent* press) const;
    bool swallowAfterClosingPress(QEvent* event);
    ClosingPress m_closingPress;
    bool         m_swallowPanMouse{false};
    bool         m_swallowPanContextMenu{false};

    // ── Main button strip ────────────────────────────────────────────────
    QPushButton*         m_collapseBtn{nullptr};
    QVector<QPushButton*> m_menuBtns;   // indices 0-6 (buttons 2-8)
    // False on a board with no antenna choices: the ANT flyout would be
    // empty, so its button is not shown (R-R3-49).
    bool                 m_antAvailable{true};
    bool                 m_expanded{true};

    // ── Active flyout tracking (one visible at a time) ───────────────────
    QWidget*     m_activeFlyout{nullptr};
    QPushButton* m_activeButton{nullptr};

    // ── Band flyout ──────────────────────────────────────────────────────
    QWidget* m_bandFlyout{nullptr};

    // ── ANT flyout ───────────────────────────────────────────────────────
    QWidget*     m_antFlyout{nullptr};
    // Phase 3P-I-a T18 — row wrappers so setBoardCapabilities can
    // setVisible(false) on both the label and the combo together.
    QWidget*     m_rxAntRow{nullptr};
    QWidget*     m_txAntRow{nullptr};
    QComboBox*   m_rxAntCmb{nullptr};
    QComboBox*   m_txAntCmb{nullptr};
    // Stored so the widget→model connection ordering inside setRadioModel
    // can replicate the per-pan rebind pattern used for VAX.
    QMetaObject::Connection m_rxAntConn;
    QMetaObject::Connection m_txAntConn;

    // ── Display pill (display pill plan Task 16) ─────────────────────────
    QWidget*     m_displayFlyout{nullptr};
    // m_heatMapBtn / m_noiseFloorBtn / m_noiseFloorSlider / m_noiseFloorLabel /
    // m_weightedAvgBtn removed B8 Task 23 — unbacked theatre controls.

    // One page of the pill while it is being built: its widget, its
    // 4-column grid (label | button or swatch | slider | value) and the
    // next free row.
    struct PillPage {
        DisplayPillTab tab{DisplayPillTab::Spectrum};
        QWidget*       widget{nullptr};
        QGridLayout*   grid{nullptr};
        int            row{0};
    };
    // One row's control, by the key it shows ("DisplayGridEnabled"; the
    // control's object name is "pill" + that). `widgets` is everything
    // that greys with the row; `tip` is its tooltip while it can act;
    // `seed` shows the target's value without reporting it.
    struct PillRow {
        QVector<QWidget*>     widgets;
        QString               tip;
        std::function<void()> seed;
    };
    using PillBoolGet  = std::function<bool(const SpectrumWidget*)>;
    using PillBoolSet  = std::function<void(SpectrumWidget*, bool)>;
    using PillIntGet   = std::function<int(const SpectrumWidget*)>;
    using PillIntSet   = std::function<void(SpectrumWidget*, int)>;
    using PillColorGet = std::function<QColor(const SpectrumWidget*)>;
    using PillColorSet = std::function<void(SpectrumWidget*, const QColor&)>;
    using PillFormat   = std::function<QString(int)>;
    using PillChoices  = QVector<std::pair<QString, int>>;

    // Page frame.
    PillPage beginPillPage(DisplayPillTab tab, const QString& title,
                           const QString& scope = QString());
    void     endPillPage(PillPage& page);
    void     addPillHeader(PillPage& page, const QString& title,
                           const QString& scope, bool first);
    QLabel*  addPillNote(PillPage& page, const QString& text, int lines = 1);
    // Rows. Each names its control "pill" + keyBase and registers it.
    QHBoxLayout* addPillLine(PillPage& page);
    QPushButton* addPillToggle(QHBoxLayout* line, const QString& keyBase,
                               const QString& text, const QString& tip,
                               PillBoolGet get, PillBoolSet set,
                               const QString& offText = QString());
    // The pill addPillToggle puts on a line, for a row that places it
    // itself. `compact` is the narrow Off/On pill of the button column.
    QPushButton* makePillToggle(QWidget* parent, const QString& keyBase,
                                const QString& text, const QString& tip,
                                PillBoolGet get, PillBoolSet set,
                                const QString& offText, bool compact);
    // A label, then a line of pills from the slider column to the right
    // edge. The label is returned through `label`.
    QHBoxLayout* addPillLabelledLine(PillPage& page, const QString& label,
                                     QLabel** labelOut);
    QPushButton* makePillSwatch(const QString& keyBase, const QString& tip,
                                const QString& dialogTitle,
                                PillColorGet get, PillColorSet set);
    QSlider*     addPillSlider(PillPage& page, const QString& keyBase,
                               const QString& label, int min, int max, int step,
                               const QString& tip, PillFormat format,
                               PillIntGet get, PillIntSet set,
                               QWidget* leading = nullptr, int valueWidth = 0);
    // An averaging time row: a 1 ms slider to half a second and a number
    // box over the whole range.
    QSlider*     addPillAvgTimeSlider(PillPage& page, const QString& keyBase,
                                      const QString& label, const QString& tip,
                                      PillIntGet get, PillIntSet set);
    void registerPillParentLabel(const QString& childKey, QLabel* label, QWidget* leading);
    void fitPillLine(PillPage& page, QHBoxLayout* line, QVector<QPushButton*> pills);
    void scheduleDisplayPillGeometryRefresh();
    void refreshDisplayPillGeometry();
    void updatePillOwnIndicators();
    void positionPillOwnIndicators();
    QStringList pillOwnKeys(const QString& row) const;
    void fitWorkspacePillGroup(QWidget* holder, const QVector<QPushButton*>& pills);
    QComboBox*   addPillCombo(PillPage& page, const QString& keyBase,
                              const QString& label, const PillChoices& items,
                              const QString& tip, PillIntGet get, PillIntSet set,
                              QWidget* leading = nullptr);
    QLabel*      makePillLabel(const QString& text);
    QPushButton* makePillButton(const QString& text, const QString& tip);
    void registerPillRow(const QString& keyBase, const QVector<QWidget*>& widgets,
                         const QString& tip, std::function<void()> seed);

    // Binding: seed on show, follow while visible, let go on hide.
    void seedPillRow(const QString& keyBase);
    void seedDisplayPill();
    void bindDisplayPill();
    void unbindDisplayPill();
    void setPillRowEnabled(const QString& keyBase, bool enabled,
                           const QString& disabledTip = QString());
    void updatePillEnables();
    void showPillTab(DisplayPillTab tab);
    void showDisplayPill();
    void layoutDisplayPill();
    QString pillSetupPage() const;
    bool isInDisplayPill(const QObject* obj, bool throughPopups = false) const;

    // The pages.
    void buildPillSpectrumPage();
    void buildPillLevelsPage();
    void addPillChoices(PillPage& page, const QString& key, const QString& prefix,
                        const PillChoices& choices, const QStringList& suffixes,
                        const QString& tip, PillIntGet get, PillIntSet set);
    int waterfallLevelsMode() const;
    void updatePillModeNotes();
    void updateWaterfallReadouts();
    void buildPillFftPage();
    void buildPillWaterfallPage();
    void buildPillView3DPage();

    // Cleared when the widget is destroyed (m_displayTargetGone).
    SpectrumWidget*          m_displayTarget{nullptr};
    QMetaObject::Connection  m_displayTargetGone;
    DisplayPillTab           m_pillTab{DisplayPillTab::Spectrum};
    QVector<QPushButton*>    m_pillTabBtns;     // in DisplayPillTab order
    // True while showPillTab() sets the tab buttons, so their `toggled`
    // handler does not answer the panel's own changes.
    bool                     m_showingPillTab{false};
    struct PillToggleGroup {
        QWidget* holder{nullptr};
        QVector<QPushButton*> buttons;
    };
    QVector<PillToggleGroup> m_pillToggleGroups;
    bool m_refreshingPillGeometry{false};
    bool m_pillGeometryQueued{false};
    // A refresh was asked for while the pill was closed; showDisplayPill()
    // lays it out on open.
    bool m_pillGeometryStale{false};
    int  m_pillGeometryRefreshes{0};
    QVector<QWidget*>        m_pillPages;       // in DisplayPillTab order
    QHash<QString, PillRow>  m_pillRows;        // by keyBase
    QStringList              m_pillRowOrder;
    QHash<QString, QPushButton*> m_pillOwnDots;
    bool m_positioningOwnDots{false};
    QVector<QMetaObject::Connection> m_pillConns;
    PillColorChooser         m_pillColorChooser;
    PillPictureChooser       m_pillPictureChooser;
    // The band's stored grid (setBandGrid). Until it is given, the dB rows
    // show the target's live range.
    bool    m_bandGridSet{false};
    QString m_bandLabel;
    int     m_bandDbMax{0};
    int     m_bandDbMin{0};
    QLabel* m_bandGridNote{nullptr};
    QLabel* m_spectrumScaleNote{nullptr};
    QLabel* m_viewNote{nullptr};
    QLabel* m_fftSizingNote{nullptr};
    bool m_applyingPillChoice{false};
    bool    m_gpuRowSpanSupported{true};
    bool    m_gpuSliceShadowSupported{true};
    bool    m_remoteWindow{false};
    bool    m_coreOffersDecimation{false};
    // False until setRemoteWindow() first applies, so its first call always does.
    bool    m_remoteWindowApplied{false};

    // ── Waterfall tab: Clarity (display pill plan Task 17) ───────────────
    void updateClarityBadge();
    void updateWaterfallNote();
    void updateBandGridNote();
    bool m_clarityEnabled{false};
    bool m_clarityActive{false};
    bool m_clarityPaused{false};
    QLabel*      m_clarityBadge{nullptr};
    // The state updateClarityBadge() last styled; empty until the first call.
    QString      m_clarityBadgeState;
    QPushButton* m_clarityRetuneBtn{nullptr};
    QLabel*      m_waterfallNote{nullptr};

    // ── FFT tab (display pill plan Task 17) ──────────────────────────────
    void updateFftReadout();
    void fftSizeSliderChanged(int position);
    // The "Sizing:" buttons show `mode` and nothing is asked.
    void showFftSizingMode(ControlRanges::DisplayFftSizingMode mode);
    // The bin width this pan's FFT runs at now; 0 with nothing running.
    double runningBinWidthHz() const;
    DisplayPillFftState m_fftState;
    // The three "Sizing:" buttons, in kDisplayFftSizingChoices order.
    QVector<QPushButton*> m_fftSizingBtns;
    // True while showFftSizingMode() sets the buttons, so their `toggled`
    // handler does not answer the panel's own changes.
    bool     m_showingFftSizingMode{false};
    QSlider* m_fftSizeSlider{nullptr};
    QTimer*  m_fftSizeKeyTimer{nullptr};
    // Set between a drag's release and the value change that follows it.
    bool     m_fftSizeReleasePending{false};
    QLabel*  m_fftReadout{nullptr};
    QLabel*  m_stationNote{nullptr};
    bool     m_stationRowsAvailable{true};
    QString  m_stationRowsReason;
    // False until setStationRowsAvailable() first applies.
    bool     m_stationRowsApplied{false};

    // ── VAX flyout ───────────────────────────────────────────────────────
    QWidget*   m_vaxFlyout{nullptr};
    QComboBox* m_vaxCmb{nullptr};

    // ── VAX model binding (Phase 3O Sub-Phase 9 Task 9.2c) ───────────────
    // m_vaxChannelConn stores the SliceModel::vaxChannelChanged → combo
    // connection so a rebind via setRadioModel() can explicitly drop the
    // prior subscription. m_updatingFromModel guards the echo path
    // (model → widget) from re-triggering the widget → model side.
    RadioModel*              m_radioModel{nullptr};
    SliceResolver            m_sliceResolver;
    QMetaObject::Connection  m_vaxChannelConn;
    bool                     m_updatingFromModel{false};

    SliceModel* resolvedSlice() const;

    // ── ATT flyout (R-R3-21) ─────────────────────────────────────────────
    QWidget*   m_attFlyout{nullptr};
    QCheckBox* m_attEnableChk{nullptr};
    QSpinBox*  m_attSpin{nullptr};
    QLabel*    m_attReason{nullptr};
    QString    m_attHeldReason;   // TX rulings (item 3)

    // ── Waterfall zoom buttons (bottom-left of spectrum widget) ──────────
    QWidget*     m_zoomStrip{nullptr};   // container for the 4 zoom buttons
    QPushButton* m_zoomSegBtn{nullptr};
    QPushButton* m_zoomBandBtn{nullptr};
    QPushButton* m_zoomOutBtn{nullptr};
    QPushButton* m_zoomInBtn{nullptr};
};

} // namespace NereusSDR
