// src/gui/SpectrumOverlayPanel.cpp
// Left overlay button strip ported from AetherSDR SpectrumOverlayMenu.
// 8 buttons, 4 flyout sub-panels, auto-close on outside click.
//
// Ported from AetherSDR src/gui/SpectrumOverlayMenu.cpp
// Adapted for NereusSDR OpenHPSDR/Thetis feature set.

// =================================================================
// src/gui/SpectrumOverlayPanel.cpp  (NereusSDR)
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
//   2026-10-22 - A closed Display pill no longer lays itself out on every
//                 remote telemetry tick: geometry refreshes wait for the
//                 pill to open, unchanged remote and station gates and an
//                 unchanged Clarity badge do nothing. J.J. Boyd (KG4VCF),
//                 AI-assisted via Claude Code.
//   2026-10-14 - Keep Peak Value as the preferred wide-font overflow and
//                 retain each Display choice's two-pixel text margin after
//                 enabled-state styling. J.J. Boyd (KG4VCF), AI-assisted
//                 via OpenAI Codex.
//   2026-10-13 - Keep runtime level and unchanged Clarity status refreshes
//                 from restyling and reflowing Display rows. J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Keep Display pages reachable inside short docked pans.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Approved Levels scale/mode choosers, View segments and
//                 unlabelled FFT sizing, reusing per-pan setters. J.J. Boyd,
//                 AI-assisted via OpenAI Codex.
//   2026-10-12 - Display workspace island, protected color swatches and
//                 metric fit using the existing approved Longpath-derived
//                 shared theme. J.J. Boyd, AI-assisted via OpenAI Codex.
//   2026-10-12 - Approved compact Trace/Grid/Markers and Drawing/Overlays/Time
//                 rows, normalization and Setup marker link. J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Five-tab live mockup regrouping and on-screen parent reasons.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Ported from AetherSDR `src/gui/SpectrumOverlayMenu.{h,cpp}`
//                 (left button strip + 5 flyout panels).
//   2026-10-10 - Display pill plan Task 4: the zoom buttons' place follows
//                 the pan's frequency scale height (SpectrumWidget::
//                 freqScaleHeight) instead of an assumed 32 px. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-04-20 — Phase 3O Sub-Phase 9 Task 9.2c (issue #70 fold-in):
//                 added setRadioModel() so the previously-disabled VAX Ch
//                 combo on the left-edge overlay is now wired bidirectionally
//                 to the resolved pan slice's vaxChannel() with echo prevention. IQ Ch
//                 stays feature-flagged off (design spec §6.7/§11.3 —
//                 audio/SendIqToVax stored-but-not-active). J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-21: the VAX channel combo stays disabled, with a
//                 plain reason, on a remote-station model. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-44: the VAX channel combo is live in a remote window
//                 again (this computer's VAX, fed from the Core's receiver
//                 streams). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-21: the ATT button opens the step attenuator; the
//                 VAX button's tooltip says what it does. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-49: the RF Gain slider and WNB button (ANT flyout)
//                 and the IQ Ch combo (VAX flyout) are removed. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-49, R-R3-21: the ANT button is not shown on a board
//                 with no antenna choices, where its flyout would be empty.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-21 fix wave: the VAX combo's tooltip before
//                 a radio is set reads "waiting for the radio". J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-IOS-27, R-IOS-06: the band table moved to
//                 models/BandGrid.h, shared with the Core's catalogue; the
//                 grid draws as before. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-26 - Parity Task 18: the Display flyout's Grid Lines toggle
//                 reports gridVisibleChanged (it changed only its own label)
//                 and takes the pan's state through setGridVisible. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX rulings (item 3, JJ): the ATT flyout is held, with the
//                reason, on a pan whose slice this window only listens to.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-10 - Display pill plan Task 16: the Display flyout is rebuilt
//                as the four-tab Display pill (Spectrum, FFT, Waterfall,
//                3D View) to the approved mockup, with the Spectrum and 3D
//                View tabs bound to the pan's SpectrumWidget. The grid and
//                header are AetherSDR's (SpectrumOverlayMenu.cpp:1300-1303,
//                1387-1406 [@1e0718ad]); the Line, Grid, Scale text and
//                Background rows are its additions (:1476-1532, :1727-1856).
//                The wheel over the pill is swallowed. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-10 - Display pill plan Task 17: the Waterfall and FFT tabs are
//                filled. The Waterfall rows are bound to the pan's
//                SpectrumWidget and grey by Clarity, AGC and "Use spectrum
//                min/max"; the NB Blank row is AetherSDR's
//                (SpectrumOverlayMenu.cpp:1598-1612 [@1e0718ad]). The FFT
//                rows show the station's settings (setFftState) and ask for
//                changes by signal. setClarityState replaces
//                setClarityStatus. J.J. Boyd (KG4VCF), AI-assisted
//                transformation via Anthropic Claude Code.
//   2026-10-10 - Follow-up wave F4: the pill is brought into line with the
//                approved mockup. Headers are all small caps; the FFT
//                tab's values end at the right edge and its empty note
//                takes no height; Peak Value shares the line of Gradient,
//                Noise Floor and Bin Width. J.J. Boyd (KG4VCF), AI-assisted
//                transformation via Anthropic Claude Code.
//   2026-10-10 - Follow-up wave F3: dB Max and dB Min are greyed while the
//                pan is keyed, with the reason as their tooltip and in the
//                note under them. J.J. Boyd (KG4VCF), AI-assisted
//                transformation via Anthropic Claude Code.
//   2026-10-10 - Follow-up wave F1: the NB Blank slider is a threshold in
//                dB (1 to 20, reads "6 dB"), a NereusSDR design; AetherSDR's
//                factor is not carried over. J.J. Boyd (KG4VCF), AI-assisted
//                transformation via Anthropic Claude Code.
//   2026-10-11 - raiseOpenFlyout(): the open flyout goes back above the
//                pan's flags whenever the pan raises them, and above the
//                zoom strip when that is placed. The flag covered the open
//                Display pill and took its clicks. J.J. Boyd (KG4VCF),
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-10-11 - An open flyout owns the mouse over its whole rectangle
//                (claimMouseForFlyout), and the outside press that closes
//                it on the same pan is taken with its moves, release and
//                context menu. Study, design: AetherSDR's panel filter
//                read, none of it taken. J.J. Boyd (KG4VCF), AI-assisted
//                implementation via Anthropic Claude Code.
//   2026-10-11 - Avg Time and WF Avg Time: the slider is one position per
//                ms from 1 to 500, and its value is a number box over
//                Thetis's whole range (1 to 9999 ms). NereusSDR design.
//                J.J. Boyd (KG4VCF), AI-assisted implementation via
//                Anthropic Claude Code.
//   2026-10-11 - FFT sizing mode (design D38 to D44): the FFT tab opens
//                with "Sizing: Auto | Hz/bin | Fixed". The FFT Size slider
//                acts in Fixed only and the Hz/bin slider in Hz/bin only;
//                grayed, each shows what runs and says why. The Hz/bin
//                Off/On pill is gone. NereusSDR design; Thetis has the
//                Fixed behavior alone (setup.cs:16183-16207 [v2.10.3.15]).
//                J.J. Boyd (KG4VCF), AI-assisted implementation via
//                Anthropic Claude Code.
//   2026-10-08  J.J. Boyd (KG4VCF): Qt 6.11.0 Cocoa accessibility cache
//               repair on a live item view (QtCocoaAccessibleTableCache).
//               AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace visual review Task 26: on a short pan the zoom
//                 row (S, B, -, +) moves to just right of the open button
//                 column instead of covering its lower buttons; it stays at
//                 the bottom left otherwise. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-10-09 - The ANT flyout's RX and TX rows and the ATT check box no
//                 longer draw the flyout's frame again: kPanelStyle stays
//                 AetherSDR's text, the rows take a scoped rule with no
//                 background or border and the check box one with no
//                 border, so its indicator keeps its square. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15  J.J. Boyd (KG4VCF): rows change after a real model reset
//               (resetItemsBeforeRowChange), so accessibility cannot read a
//               deleted cell. AI-assisted via Anthropic Claude Code.
//   2026-10-16  J.J. Boyd (KG4VCF): the pill's "Setup defaults" link names
//               the Setup Display pages as the tree shows them, and the FFT
//               tab's opens FFT & Engine instead of Spectrum. AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include "SpectrumOverlayPanel.h"
#include "gui/QtCocoaAccessibleTableCache.h"

#include "StyleConstants.h"
#include "gui/styles/WorkspaceComponents.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/styles/WorkspaceStyle.h"
#include "core/AntennaLabels.h"
#include "core/AppSettings.h"
#include "core/settings/PanDisplayKeys.h"
#include "core/BoardCapabilities.h"
#include "core/ControlRanges.h"
#include "core/SkuUiProfile.h"
#include "core/StepAttenuatorFacade.h"
#include "core/spectrum/ClarityLevels.h"
#include "core/spectrum/DisplayFollowers.h"
#include "core/spectrum/WaterfallPalettes.h"
#include "gui/AntennaPopupBuilder.h"
#include "gui/DisplayFftReadout.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/GuardedComboBox.h"
#include "gui/widgets/GuardedSlider.h"
#include "models/BandGrid.h"
#include "models/DisplaySettingsModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QColorDialog>
#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QTextDocument>
#include <QStyleOptionSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <utility>

namespace NereusSDR {

// ── Constants (from AetherSDR SpectrumOverlayMenu.cpp) ───────────────────────
static constexpr int kBtnW     = 68;
static constexpr int kBtnH     = 22;
static constexpr int kBandBtnW = 48;
static constexpr int kBandBtnH = 26;
static constexpr int kPad      = 2;
static constexpr int kGap      = 2;

// Translucent overlay colours — intentionally NOT in StyleConstants.h because
// they are designed to alpha-blend over the changing spectrum background (the
// translucency is inherent to their purpose, not incidental). All opaque
// panel/button colours have been consolidated to Style:: canonical helpers.
// Per docs/architecture/ui-audit-polish-plan.md §A2 — "tightly-scoped local
// blocks for legitimate exceptions."
namespace OverlayColors {
    // Flyout panel backgrounds (verbatim from AetherSDR SpectrumOverlayMenu.cpp)
    constexpr auto kPanelStyle =
        "QWidget { background: rgba(15, 15, 26, 220); "
        "border: 1px solid #304050; border-radius: 3px; }";

    // Label style for rows inside translucent flyout panels.
    // Transparent background is required here because the label must not
    // occlude the panel's own rgba layer. Color (#8aa8c0 = Style::kTitleText)
    // has a canonical equivalent but the full rule (transparent bg, no border,
    // 10 px bold) does not — keeping it co-located with the other overlay rules.
    constexpr auto kLabelStyle =
        "QLabel { background: transparent; border: none; "
        "color: #8aa8c0; font-size: 10px; font-weight: bold; }";

    // Menu strip buttons — fully transparent/semi-transparent fills that
    // blend with the spectrum below.
    constexpr auto kMenuBtnNormal =
        "QPushButton { background: rgba(20, 30, 45, 240); "
        "border: 1px solid rgba(255, 255, 255, 40); border-radius: 2px; "
        "color: #c8d8e8; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #0090e0; }";

    constexpr auto kMenuBtnActive =
        "QPushButton { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #0090e0; border-radius: 2px; "
        "color: #ffffff; font-size: 11px; font-weight: bold; }";
} // namespace OverlayColors

// ── Workspace preview styles ─────────────────────────────────────────────────
// With WorkspacePreview::enabled() the strip and corner buttons keep their
// geometry, translucency and font sizes and take the Glass and Depth roles:
// a neutral raised chip, a quiet hover, the selected blue when active. With
// the switch off each helper returns the OverlayColors string unchanged.
namespace {

QString wsRgba(const char* role, const char* fallback, int alpha)
{
    const QColor c(Workspace::role(role, fallback));
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

// A vertical gradient between two palette roles at one alpha: the
// buttons float over the spectrum, so they keep their translucency.
QString wsGrad(const char* topRole, const char* topFallback,
               const char* bottomRole, const char* bottomFallback, int alpha)
{
    return QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2)")
        .arg(wsRgba(topRole, topFallback, alpha), wsRgba(bottomRole, bottomFallback, alpha));
}

// Raised (light top, dark bottom), radius 4, light top edge and dark
// bottom edge; hover lighter; pressed flipped. Sizes and fonts are the
// legacy ones, so no button moves.
QString wsMenuBtnStyle(int fontPx, int bgAlpha)
{
    return QStringLiteral(
        "QPushButton { background: %1; "
        "border: 1px solid %2; border-top-color: %3; border-bottom-color: %4; "
        "border-radius: %5px; "
        "color: %6; font-size: %7px; font-weight: bold; }"
        "QPushButton:hover { background: %8; }"
        "QPushButton:pressed { background: %9; "
        "border-top-color: %4; border-bottom-color: %3; }")
        .arg(wsGrad("raised-top", "#222227", "raised-bottom", "#141417", bgAlpha),
             Workspace::role("border", "#2c2c31"),
             Workspace::role("edge-light", "#38383e"),
             Workspace::role("edge-dark", "#1f1f23"))
        .arg(Workspace::Components::radiusFor(kBtnH))
        .arg(Workspace::role("text", "#dcdce1"))
        .arg(fontPx)
        .arg(wsGrad("button-alt-hover", "#2f3138", "button", "#1a1a1e", 240),
             wsGrad("raised-bottom", "#141417", "raised-top", "#222227", bgAlpha));
}

QString menuBtnNormalStyle()
{
    if (!WorkspacePreview::enabled()) {
        return QString::fromLatin1(OverlayColors::kMenuBtnNormal);
    }
    return wsMenuBtnStyle(11, 240);
}

QString menuBtnActiveStyle()
{
    if (!WorkspacePreview::enabled()) {
        return QString::fromLatin1(OverlayColors::kMenuBtnActive);
    }
    // The selected state: the glass gradient, its border and a lighter
    // top edge.
    return QStringLiteral(
        "QPushButton { background: %1; "
        "border: 1px solid %2; border-top-color: %3; border-radius: %4px; "
        "color: %5; font-size: 11px; font-weight: bold; }")
        .arg(wsGrad("selected-top", "#2f5f92", "selected-bottom", "#1e3d5f", 230),
             Workspace::role("selected-border", "#3d78b0"),
             Workspace::shiftL(QColor(Workspace::role("selected-border", "#3d78b0")), 16))
        .arg(Workspace::Components::radiusFor(kBtnH))
        .arg(Workspace::role("selected-text", "#dbe9f8"));
}

QString panelStyle()
{
    if (!WorkspacePreview::enabled()) {
        return QString::fromLatin1(OverlayColors::kPanelStyle);
    }
    return QStringLiteral(
        "QWidget { background: %1; "
        "border: 1px solid %2; border-radius: 3px; }")
        .arg(wsRgba("panel", "#0c0c0e", 220), Workspace::role("border", "#2c2c31"));
}

QString labelStyle()
{
    if (!WorkspacePreview::enabled()) {
        return QString::fromLatin1(OverlayColors::kLabelStyle);
    }
    return QStringLiteral(
        "QLabel { background: transparent; border: none; "
        "color: %1; font-size: 10px; font-weight: bold; }")
        .arg(Workspace::role("title-text", "#c4c4ca"));
}

// kPanelStyle (AetherSDR's text, kept byte-exact) is an unnamed QWidget
// rule, so it reaches every widget inside a flyout. AetherSDR builds its
// RX ANT row as a bare layout and gives its one row widget (m_loopRow)
// "QWidget { background: transparent; border: none; }". Ours wraps each
// antenna row in a QWidget (so setBoardCapabilities hides it as a unit) and
// the step attenuator has a check box, so each drew the flyout's frame
// around itself again. They take this rule instead, scoped to the row's
// own object name so its label and combo keep theirs. The check box drops
// only the border: its background is what Fusion fills the indicator from,
// so clearing it would also clear the indicator's square.
QString clearRowStyle(const QString& objectName)
{
    return QStringLiteral("QWidget#%1 { background: transparent; border: none; }").arg(objectName);
}

QString checkBoxStyle()
{
    return labelStyle() + QStringLiteral(" QCheckBox { border: none; }");
}

// The flyout's own controls go through the scoped pass when shown.
void themeFlyout(QWidget* flyout)
{
    if (WorkspacePreview::enabled()) {
        WorkspacePreview::themeSubtree(flyout);
    }
}

} // namespace

// File-local helpers for opaque styles that diverge from the canonical
// Style:: helpers (per §A2 exception pattern — verified byte-by-byte):

// overlayDisplayToggleStyle — diverges from Style::buttonBaseStyle() + Style::greenCheckedStyle():
//   - padding: 2px 6px vs buttonBaseStyle()'s 2px 4px (wider pill shape for display toggles)
// All other tokens match: bg kButtonBg, border kBorder, color kTextPrimary, 10px bold,
// border-radius 3px, hover kButtonAltHover, checked bg/text/border kGreen*.
static inline QString overlayDisplayToggleStyle()
{
    return Style::buttonBaseStyle().replace("padding: 2px 4px", "padding: 2px 6px")
         + Style::greenCheckedStyle();
}

// Helper: create a standard menu button
static QPushButton* makeMenuBtn(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(kBtnW, kBtnH);
    btn->setStyleSheet(menuBtnNormalStyle());
    return btn;
}

// ── Band table ────────────────────────────────────────────────────────────────
// From AetherSDR SpectrumOverlayMenu.cpp, reduced to HF + WWV. It lives in
// models/BandGrid.h (kBandGrid) so the Core's catalogue lists the same bands
// in the same order (R-IOS-27).

// ── Constructor ───────────────────────────────────────────────────────────────

SpectrumOverlayPanel::SpectrumOverlayPanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);

    // Button 1: Collapse toggle
    m_collapseBtn = new QPushButton(this);
    m_collapseBtn->setFixedSize(kBtnW, kBtnH);
    m_collapseBtn->setStyleSheet(
        "QPushButton { background: rgba(20, 30, 45, 240); "
        "border: 1px solid rgba(255, 255, 255, 40); border-radius: 2px; "
        "color: #c8d8e8; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #0090e0; }");
    if (WorkspacePreview::enabled()) {
        m_collapseBtn->setStyleSheet(wsMenuBtnStyle(13, 240));
    }
    connect(m_collapseBtn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggle);

    // Button 2: +RX (NYI Phase 3F)
    {
        // Adds a slice on THIS pan -- the one this strip is drawn on -- not on
        // whichever pan happens to be active. A control rendered on a pan acts
        // on that pan; anything else makes the operator track hidden state to
        // predict what a visible button will do.
        auto* btn = makeMenuBtn("+RX", this);
        btn->setToolTip("Add an RX slice on this panadapter");
        connect(btn, &QPushButton::clicked, this, [this]() {
            emit addRxClicked(m_panId);
        });
        m_menuBtns.append(btn);
    }

    // Button 3: +TNF
    {
        // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:293 [@c6481cbf] --
        // the "+TNF" entry in the strip's button table. Upstream's signal is
        // arg-less; ours carries the pan id, matching the +RX shape at
        // SpectrumOverlayMenu.cpp:315 [@c6481cbf] so a strip drawn on pan-2
        // never adds a notch on pan-0.
        auto* btn = makeMenuBtn("+TNF", this);
        btn->setObjectName(QStringLiteral("tnfAddButton"));
        btn->setToolTip("Add a notch filter at this panadapter's VFO");
        connect(btn, &QPushButton::clicked, this, [this]() {
            emit addTnfClicked(m_panId);
        });
        m_menuBtns.append(btn);  // index 1
    }

    // Button 4: BAND — flyout
    {
        auto* btn = makeMenuBtn("BAND", this);
        btn->setToolTip("Open band selector");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleBandFlyout);
        m_menuBtns.append(btn);  // index 2
    }

    // Button 5: ANT — flyout
    {
        auto* btn = makeMenuBtn("ANT", this);
        btn->setToolTip("Open antenna controls");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleAntFlyout);
        m_menuBtns.append(btn);  // index 3
    }

    // DSP flyout removed — controls duplicated the VFO flag's DSP grid.
    // Use the top menu bar's DSP menu (full parity) or the VFO flag.

    // Button 6: Display — flyout
    {
        auto* btn = makeMenuBtn("Display", this);
        btn->setToolTip("Open display settings");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleDisplayFlyout);
        m_menuBtns.append(btn);  // index 4
    }

    // Button 7: VAX flyout
    {
        auto* btn = makeMenuBtn("VAX", this);
        btn->setToolTip("Choose the VAX channel this panadapter's slice sends its audio to");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleVaxFlyout);
        m_menuBtns.append(btn);  // index 5
    }

    // Button 8: ATT flyout (R-R3-21): the radio's step attenuator.
    {
        auto* btn = makeMenuBtn("ATT", this);
        btn->setObjectName(QStringLiteral("attMenuButton"));
        btn->setToolTip("Open the step attenuator");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleAttFlyout);
        m_menuBtns.append(btn);  // index 6
    }

    // The strip used to end in a disabled "MNF" twin of the +TNF button
    // above. It is gone rather than shipped beside a live control that does
    // the same job.

    buildBandFlyout();
    buildAntFlyout();
    buildDisplayFlyout();
    buildVaxFlyout();
    buildAttFlyout();
    buildZoomButtons();

    // Install event filter for auto-close on outside click and parent resize tracking
    qApp->installEventFilter(this);
    if (parentWidget()) {
        parentWidget()->installEventFilter(this);
    }

    // An open flyout stays above the pan's flags: the pan says when it has
    // just raised them. Same thread, so the flyout is back on top before
    // the pass that raised the flags returns.
    if (auto* pan = qobject_cast<SpectrumWidget*>(parentWidget())) {
        connect(pan, &SpectrumWidget::flagsRaised,
                this, &SpectrumOverlayPanel::raiseOpenFlyout);
    }

    updateLayout();
}

// ── eventFilter — auto-close flyout on outside click ─────────────────────────

bool SpectrumOverlayPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (WorkspacePreview::enabled() && !m_refreshingPillGeometry
        && (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange
            || event->type() == QEvent::Show) && isInDisplayPill(obj)) {
        scheduleDisplayPillGeometryRefresh();
    }
    if ((event->type() == QEvent::Resize || event->type() == QEvent::Move)
        && isInDisplayPill(obj)) {
        positionPillOwnIndicators();
    }
    if ((event->type() == QEvent::Leave || event->type() == QEvent::FocusOut)
        && isInDisplayPill(obj)) {
        if (const auto* widget = qobject_cast<const QWidget*>(obj)) {
            for (QWidget* page : std::as_const(m_pillPages)) {
                if (page && page->isAncestorOf(widget)) {
                    QLabel* reason = page->findChild<QLabel*>(QStringLiteral("pillReason"));
                    if (reason && reason->property("pillHoverReason").value<QObject*>() == obj) {
                        reason->setProperty("pillHoverReason", QVariant());
                        updatePillOwnIndicators();
                    }
                    break;
                }
            }
        }
    }
    // Approved live mockup: reasons are readable on the page, including
    // disabled controls whose clicks cannot open a tooltip.
    if ((event->type() == QEvent::Enter || event->type() == QEvent::FocusIn)
        && isInDisplayPill(obj)) {
        if (const auto* widget = qobject_cast<const QWidget*>(obj)) {
            const QString tip = widget->toolTip();
            if (!tip.isEmpty()) {
                for (QWidget* page : std::as_const(m_pillPages)) {
                    if (page && page->isAncestorOf(widget)) {
                        if (auto* reason = page->findChild<QLabel*>(QStringLiteral("pillReason"))) {
                            reason->setProperty("pillHoverReason", QVariant::fromValue(obj));
                            reason->setText(tip);
                            scheduleDisplayPillGeometryRefresh();
                        }
                        break;
                    }
                }
            }
        }
    }

    // Display pill plan Task 16: the wheel over the Display pill goes
    // nowhere. It does not tune the pan underneath and it does not move a
    // row the pointer happens to be over; a slider takes it only once it
    // has been clicked or tabbed to.
    if (event->type() == QEvent::Wheel && isInDisplayPill(obj)) {
        const auto* slider = qobject_cast<const QSlider*>(obj);
        auto* scroll = m_displayFlyout->findChild<QScrollArea*>(QStringLiteral("displayPillScroll"));
        if ((slider && slider->hasFocus())
            || (scroll && (obj == scroll->viewport() || obj == scroll->verticalScrollBar()))) {
            return false;
        }
        event->accept();
        return true;
    }

    // Auto-close flyout on outside click (from AetherSDR)
    // Display pill plan Task 16: a click in a dialog the pill opened (a
    // colour or picture chooser) or in one of its combos' open lists is not
    // an outside click.
    const bool pillOwnsClick = m_activeFlyout == m_displayFlyout
        && (QApplication::activeModalWidget() != nullptr || isInDisplayPill(obj, true));
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        // The same press is seen several times: at its window, at the
        // widget under the pointer, and at each parent it is handed on to.
        // Only a press that is not the one that just closed a flyout ends
        // what that one started.
        if (!isClosingPress(me)) {
            m_closingPress.valid = false;
            m_swallowPanMouse = false;
            m_swallowPanContextMenu = false;
        }
        if (m_activeFlyout && !pillOwnsClick) {
            QPoint gp = me->globalPosition().toPoint();
            bool inFlyout = m_activeFlyout->rect().contains(
                                m_activeFlyout->mapFromGlobal(gp));
            bool inButton = m_activeButton &&
                            m_activeButton->rect().contains(
                                m_activeButton->mapFromGlobal(gp));
            bool inPanel  = rect().contains(mapFromGlobal(gp));
            if (!inFlyout && !inButton && !inPanel) {
                hideFlyout();
                m_closingPress.valid = true;
                m_closingPress.timestamp = me->timestamp();
                m_closingPress.globalPos = gp;
                m_closingPress.button = me->button();
            }
        }
    }

    // The press that closed a flyout only closes it. Where it lands on this
    // strip's own pan (the spectrum itself, not a flag or a button on it)
    // the pan never sees it, so it starts no drag, tunes nothing and does
    // not make the pan the active one; the moves, the release and the
    // context menu that belong to it go the same way. A press on another
    // pan or anywhere else in the window closes the flyout and carries on.
    if (obj == parentWidget() && swallowAfterClosingPress(event)) {
        return true;
    }

    // Reposition zoom buttons when parent (spectrum widget) resizes
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        repositionZoomButtons();
        if (displayPillVisible()) { layoutDisplayPill(); }
    }
    // Task 26: the row's place depends on where the column is, so follow
    // the column when it moves (MainWindow places it after construction).
    // This filter is installed on the application, so it sees our own moves.
    if (obj == this && event->type() == QEvent::Move) {
        repositionZoomButtons();
    }

    return QWidget::eventFilter(obj, event);
}

// ── A flyout owns the mouse over its whole rectangle ────────────────────────
//
// Study, design. AetherSDR builds its panels the same way (plain children of
// the spectrum: src/gui/SpectrumOverlayMenu.cpp:542, 606, 1378, 1471, 1624,
// 3475 [@1e0718ad]) and guards them with an event filter on each panel root
// that takes a press or release where childAt() is null (:497-501,
// 3664-3678) plus a wheel guard on every descendant
// (src/gui/SpectrumOverlayWheelGuard.cpp:57-176). That leaves labels,
// disabled controls, moves, double-clicks and the context menu going on to
// the spectrum, which is what the bench reported here (a drag over the open
// Display pill panned the spectrum and the first click tuned). NereusSDR
// stops the hand-on at the flyout's root instead, so no code is taken.
//
// Qt hands a mouse press, move, release, double-click, wheel or context menu
// event that a widget ignores to its parent, and stops at a widget with
// WA_NoMousePropagation (QApplication::notify). A press on the flyout's
// background, a gap, a label, a disabled control, or a button a control
// does not use therefore ends at the root and never reaches the pan. The
// hover moves Qt sends along the same chain stop there too. The widget
// pressed keeps the mouse until release, so a drag that leaves the flyout
// stays with it.
void SpectrumOverlayPanel::claimMouseForFlyout(QWidget* flyout)
{
    flyout->setAttribute(Qt::WA_NoMousePropagation, true);
}

bool SpectrumOverlayPanel::isClosingPress(const QMouseEvent* press) const
{
    return m_closingPress.valid
        && m_closingPress.timestamp == press->timestamp()
        && m_closingPress.button == press->button()
        && m_closingPress.globalPos == press->globalPosition().toPoint();
}

// Called for events that are about to reach this strip's pan. True when the
// event belongs to the press that closed a flyout and must go no further.
bool SpectrumOverlayPanel::swallowAfterClosingPress(QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (!isClosingPress(me)) {
            return false;
        }
        m_closingPress.valid = false;
        m_swallowPanMouse = true;
        // The context menu event comes with the press on some platforms
        // and after the release on others.
        m_swallowPanContextMenu = me->button() == Qt::RightButton;
        event->accept();
        return true;
    }
    case QEvent::MouseMove:
        if (m_swallowPanMouse) {
            event->accept();
            return true;
        }
        return false;
    case QEvent::MouseButtonRelease:
        if (m_swallowPanMouse) {
            if (static_cast<QMouseEvent*>(event)->buttons() == Qt::NoButton) {
                m_swallowPanMouse = false;
            }
            event->accept();
            return true;
        }
        return false;
    case QEvent::ContextMenu:
        if (m_swallowPanContextMenu) {
            m_swallowPanContextMenu = false;
            event->accept();
            return true;
        }
        return false;
    default:
        return false;
    }
}

// ── raiseAll ──────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::raiseAll()
{
    raise();
    for (QWidget* fw : {m_bandFlyout, m_antFlyout,
                        m_displayFlyout, m_vaxFlyout}) {
        if (fw) { fw->raise(); }
    }
}

// ── raiseOpenFlyout ───────────────────────────────────────────────────────────

void SpectrumOverlayPanel::raiseOpenFlyout()
{
    if (m_activeFlyout && !m_activeFlyout->isHidden()) {
        m_activeFlyout->raise();
    }
}

// ── hideFlyout — hide active flyout and reset button style ───────────────────

void SpectrumOverlayPanel::hideFlyout()
{
    if (m_activeFlyout) {
        // The Display pill follows its pan only while it is open.
        if (m_activeFlyout == m_displayFlyout) {
            unbindDisplayPill();
        }
        m_activeFlyout->hide();
        m_activeFlyout = nullptr;
    }
    if (m_activeButton) {
        m_activeButton->setStyleSheet(menuBtnNormalStyle());
        m_activeButton = nullptr;
    }
}

// ── updateLayout ─────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::updateLayout()
{
    m_collapseBtn->setText(m_expanded ? QStringLiteral("\u25c0")   // ◀
                                      : QStringLiteral("\u25b6")); // ▶
    m_collapseBtn->move(kPad, kPad);

    // R-R3-49: a button whose flyout would be empty is not shown (the ANT
    // button on a board with no antenna choices), as an empty Setup page
    // is not offered.
    int y = kPad + kBtnH + kGap;
    int shownCount = 0;
    for (int i = 0; i < m_menuBtns.size(); ++i) {
        QPushButton* btn = m_menuBtns[i];
        const bool offered = i != 3 || m_antAvailable;  // index 3 is ANT
        btn->setVisible(m_expanded && offered);
        if (m_expanded && offered) {
            btn->move(kPad, y);
            y += kBtnH + kGap;
            ++shownCount;
        }
    }

    int totalH = m_expanded
        ? (kPad + kBtnH + kGap + shownCount * (kBtnH + kGap))
        : (kPad + kBtnH + kPad);
    setFixedSize(kPad + kBtnW + kPad, totalH);
    // Task 26: the column grew or shrank; keep the zoom row clear of it.
    repositionZoomButtons();
}

// ── toggle ────────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::toggle()
{
    m_expanded = !m_expanded;
    if (!m_expanded) {
        hideFlyout();
    }
    updateLayout();
    emit collapsed(!m_expanded);
}

// ── Band flyout ───────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildBandFlyout()
{
    m_bandFlyout = new QWidget(parentWidget());
    claimMouseForFlyout(m_bandFlyout);
    m_bandFlyout->setStyleSheet(panelStyle());
    m_bandFlyout->hide();

    auto* grid = new QGridLayout(m_bandFlyout);
    grid->setContentsMargins(2, 2, 2, 2);
    grid->setSpacing(2);

    const QString bandBtnStyle =
        "QPushButton { background: rgba(30, 40, 55, 220); "
        "border: 1px solid #304050; border-radius: 3px; "
        "color: #c8d8e8; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #0090e0; }";

    // 4-column grid layout:
    // Row 0: 160, 80, 60, 40
    // Row 1: 30, 20, 17, 15
    // Row 2: 12, 10, 6, 2
    // Row 3: WWV
    static constexpr int kCols = 4;
    for (int i = 0; i < kBandGridCount; ++i) {
        int row = i / kCols;
        int col = i % kCols;

        auto* btn = new QPushButton(QString::fromLatin1(kBandGrid[i].label), m_bandFlyout);
        btn->setFixedSize(kBandBtnW, kBandBtnH);
        btn->setStyleSheet(bandBtnStyle);

        QString bandName = QString::fromLatin1(kBandGrid[i].name);
        double  freqHz   = kBandGrid[i].freqHz;
        QString mode     = QString::fromLatin1(kBandGrid[i].mode);

        connect(btn, &QPushButton::clicked, this, [this, bandName, freqHz, mode]() {
            hideFlyout();
            emit bandSelected(bandName, freqHz, mode);
        });
        grid->addWidget(btn, row, col);
    }

    m_bandFlyout->adjustSize();
}

void SpectrumOverlayPanel::toggleBandFlyout()
{
    // Button index 2 (BAND) in m_menuBtns
    QPushButton* bandBtn = m_menuBtns[2];

    if (m_activeFlyout == m_bandFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();

    // Flyout: top-aligned with panel (from AetherSDR toggleBandPanel)
    m_bandFlyout->move(x() + width(), y());
    m_bandFlyout->raise();
    themeFlyout(m_bandFlyout);
    m_bandFlyout->show();
    m_activeFlyout = m_bandFlyout;
    m_activeButton = bandBtn;
    bandBtn->setStyleSheet(menuBtnActiveStyle());
}

// ── ANT flyout ────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildAntFlyout()
{
    m_antFlyout = new QWidget(parentWidget());
    claimMouseForFlyout(m_antFlyout);
    m_antFlyout->setStyleSheet(panelStyle());
    m_antFlyout->hide();

    auto* vbox = new QVBoxLayout(m_antFlyout);
    vbox->setContentsMargins(6, 6, 6, 6);
    vbox->setSpacing(4);

    constexpr int kLabelW = 52;

    // Phase 3P-I-a T18 — RX/TX antenna rows. Prior to this phase the
    // combos existed but had no currentTextChanged connect — they were
    // zombie controls. Now wired through the pan's resolved slice,
    // and both rows are wrapped in a QWidget so setBoardCapabilities
    // can hide them as a unit on HL2/Atlas.
    {
        m_rxAntRow = new QWidget;
        m_rxAntRow->setObjectName(QStringLiteral("rxAntRow"));
        m_rxAntRow->setStyleSheet(clearRowStyle(m_rxAntRow->objectName()));
        auto* row = new QHBoxLayout(m_rxAntRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto* lbl = new QLabel("RX Ant:");
        lbl->setStyleSheet(labelStyle());
        lbl->setFixedWidth(kLabelW);
        row->addWidget(lbl);
        m_rxAntCmb = new QComboBox;
        m_rxAntCmb->setObjectName(QStringLiteral("m_rxAntCmb"));
        // Seed with default labels; setBoardCapabilities replaces once
        // a radio is connected.
        m_rxAntCmb->addItems(QStringList{"ANT1", "ANT2", "ANT3"});
        m_rxAntCmb->setFixedHeight(kBtnH);
        row->addWidget(m_rxAntCmb, 1);
        vbox->addWidget(m_rxAntRow);

        // Widget → Model: user picks an antenna → resolved slice setRxAntenna.
        // T12's RadioModel connect then routes to AlexController and
        // T9's applyAlexAntennaForBand reaches the wire. Echo from the
        // model side is guarded by m_updatingFromModel (shared with VAX
        // — same flag, different combo, doesn't matter because the
        // echoes arrive serially on the GUI thread).
        connect(m_rxAntCmb, &QComboBox::currentTextChanged, this,
                [this](const QString& ant) {
            if (m_updatingFromModel || !m_radioModel || ant.isEmpty()) {
                return;
            }
            if (SliceModel* s = resolvedSlice()) {
                s->setRxAntenna(ant);
            }
        });
    }

    {
        m_txAntRow = new QWidget;
        m_txAntRow->setObjectName(QStringLiteral("txAntRow"));
        m_txAntRow->setStyleSheet(clearRowStyle(m_txAntRow->objectName()));
        auto* row = new QHBoxLayout(m_txAntRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto* lbl = new QLabel("TX Ant:");
        lbl->setStyleSheet(labelStyle());
        lbl->setFixedWidth(kLabelW);
        row->addWidget(lbl);
        m_txAntCmb = new QComboBox;
        m_txAntCmb->setObjectName(QStringLiteral("m_txAntCmb"));
        m_txAntCmb->addItems(QStringList{"ANT1", "ANT2", "ANT3"});
        m_txAntCmb->setFixedHeight(kBtnH);
        row->addWidget(m_txAntCmb, 1);
        vbox->addWidget(m_txAntRow);

        connect(m_txAntCmb, &QComboBox::currentTextChanged, this,
                [this](const QString& ant) {
            if (m_updatingFromModel || !m_radioModel || ant.isEmpty()) {
                return;
            }
            if (SliceModel* s = resolvedSlice()) {
                s->setTxAntenna(ant);
            }
        });
    }

    m_antFlyout->setFixedWidth(180);
    m_antFlyout->adjustSize();
}

void SpectrumOverlayPanel::toggleAntFlyout()
{
    // Button index 3 (ANT)
    QPushButton* antBtn = m_menuBtns[3];

    if (m_activeFlyout == m_antFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();

    // Center flyout vertically on the ANT button (from AetherSDR toggleAntPanel)
    int antBtnCenterY = antBtn->y() + antBtn->height() / 2;
    int panelY = y() + antBtnCenterY - m_antFlyout->sizeHint().height() / 2;
    m_antFlyout->move(x() + width(), std::max(0, panelY));
    m_antFlyout->raise();
    themeFlyout(m_antFlyout);
    m_antFlyout->show();
    m_activeFlyout = m_antFlyout;
    m_activeButton = antBtn;
    antBtn->setStyleSheet(menuBtnActiveStyle());
}

// ── Display pill ──────────────────────────────────────────────────────────────
// Display pill plan Task 16. The Display button's flyout: a tab strip
// (Spectrum | FFT | Waterfall | 3D View) over one page per tab. Each page is
// one 4-column grid (label | button or swatch | slider | value) under an
// accent header, with the same footer. The per-pan rows read and write the
// target SpectrumWidget directly: they are seeded from it when the pill
// opens (writing nothing), follow it while the pill is open, and let go of
// it when the pill closes.
//
// Layout and colours are the approved mockup's
// (docs/architecture/2026-10-09-display-pill-mockups/), which takes the
// grid and the header from AetherSDR's Display panel.

namespace PillStyle {
    constexpr int kWidth       = 268;
    constexpr int kTabH        = 22;
    constexpr int kControlH    = 18;   // pills, combos, buttons, swatches
    constexpr int kSliderH     = 14;
    constexpr int kLabelH      = 14;
    constexpr int kHeaderH     = 12;
    constexpr int kSubHeaderGap = 8;   // above a header that is not the first
    constexpr int kNoteLineH   = 12;
    constexpr int kReasonLines = 3;
    constexpr int kSeparatorH  = 5;    // a 1 px line with 2 px above and below
    constexpr int kValueW      = 50;
    constexpr int kButtonColW  = 22;
    // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:1300-1303 [@1e0718ad]
    // (the Display panel's grid: margins 8, 6, 8, 6 and spacing 4).
    constexpr int kMarginH     = 8;
    constexpr int kMarginV     = 6;
    constexpr int kSpacing     = 4;
    // Between the Spectrum tab's rows when its overlay pills take two
    // lines (buildPillSpectrumPage), so the tab still fits 480 px keyed.
    constexpr int kSpacingWrapped = 3;

    // Scoped to the pill's own frame, so its rows are not each given the
    // frame's background and border.
    constexpr auto kFrame =
        "QWidget#displayPill { background: rgba(15, 15, 26, 220); "
        "border: 1px solid #304050; border-radius: 3px; }";

    // The tab strip: the left strip's menu-button idiom, 10 px.
    constexpr auto kTab =
        "QPushButton { background: rgba(20, 30, 45, 240); "
        "border: 1px solid rgba(255, 255, 255, 40); border-radius: 2px; "
        "color: #c8d8e8; font-size: 10px; font-weight: bold; padding: 0; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #0090e0; }"
        "QPushButton:checked { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #0090e0; color: #ffffff; }";

    constexpr auto kLabel =
        "QLabel { background: transparent; border: none; color: #8090a0; "
        "font-size: 10px; }"
        "QLabel:disabled { color: #4a5a6a; }";
    constexpr auto kValue =
        "QLabel { background: transparent; border: none; color: #c8d8e8; "
        "font-size: 10px; }"
        "QLabel:disabled { color: #4a5a6a; }";
    constexpr auto kNote =
        "QLabel { background: transparent; border: none; color: #708090; "
        "font-size: 9px; }";
    constexpr auto kHeader =
        "QLabel { background: transparent; border: none; color: #00b4d8; }";
    constexpr auto kHeaderScope =
        "QLabel { background: transparent; border: none; color: #708090; "
        "font-size: 9px; font-weight: bold; }";
    constexpr auto kLine =
        "QFrame { background: #304050; border: none; }";

    constexpr auto kSlider =
        "QSlider { background: transparent; border: none; }"
        "QSlider::groove:horizontal { height: 4px; background: #203040; "
        "border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 10px; height: 10px; "
        "margin: -3px 0; background: #00b4d8; border-radius: 5px; }"
        "QSlider::groove:horizontal:disabled { background: #1a2430; }"
        "QSlider::handle:horizontal:disabled { background: #3a4a5a; }";

    // A toggle pill: 36 px at its narrowest (22 + 2 x 6 padding + border).
    constexpr auto kToggle =
        "QPushButton { background: #1a2a3a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; "
        "font-weight: bold; padding: 0 6px; min-width: 22px; }"
        "QPushButton:hover { background: #204060; }"
        "QPushButton:checked { background: #006040; color: #00ff88; "
        "border: 1px solid #00a060; }"
        "QPushButton:disabled { background: #141e28; color: #4a5a6a; "
        "border: 1px solid #243444; }";

    // A toggle pill on a line of four (follow-up wave F4): kToggle with
    // less padding, so Gradient, Noise Floor, Bin Width and Peak Value
    // share one line of the pill. The font is kToggle's.
    constexpr int kTogglePadding      = 6;   // kToggle's
    constexpr int kTogglePaddingTight = 1;
    constexpr int kTightSpacing       = 2;
    inline QString toggleWithPadding(int px)
    {
        return QStringLiteral(
            "QPushButton { background: #1a2a3a; border: 1px solid #205070; "
            "border-radius: 3px; color: #c8d8e8; font-size: 10px; "
            "font-weight: bold; padding: 0 %1px; min-width: 22px; }"
            "QPushButton:hover { background: #204060; }"
            "QPushButton:checked { background: #006040; color: #00ff88; "
            "border: 1px solid #00a060; }"
            "QPushButton:disabled { background: #141e28; color: #4a5a6a; "
            "border: 1px solid #243444; }")
            .arg(px);
    }

    // The narrow Off/On pill of the button column.
    constexpr auto kToggleCompact =
        "QPushButton { background: #1a2a3a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; "
        "font-weight: bold; padding: 0 3px; min-width: 16px; }"
        "QPushButton:hover { background: #204060; }"
        "QPushButton:checked { background: #006040; color: #00ff88; "
        "border: 1px solid #00a060; }"
        "QPushButton:disabled { background: #141e28; color: #4a5a6a; "
        "border: 1px solid #243444; }";

    // One of the three "Sizing:" buttons: kToggleCompact, and the chosen
    // one still reads as chosen while the row is grayed.
    constexpr int kSegmentSpacing = 2;   // the tab strip's
    constexpr auto kSegment =
        "QPushButton { background: #1a2a3a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; "
        "font-weight: bold; padding: 0 3px; min-width: 16px; }"
        "QPushButton:hover { background: #204060; }"
        "QPushButton:checked { background: #006040; color: #00ff88; "
        "border: 1px solid #00a060; }"
        "QPushButton:disabled { background: #141e28; color: #4a5a6a; "
        "border: 1px solid #243444; }"
        "QPushButton:checked:disabled { background: #12322a; color: #4a8a6c; "
        "border: 1px solid #245a48; }";

    // The Clarity badge: green while Clarity sets the levels, amber while
    // it holds, grey otherwise.
    constexpr auto kBadgeActive =
        "QLabel { background: #006040; color: #00ff88; "
        "border: 1px solid #00a060; border-radius: 2px; font-size: 10px; "
        "font-weight: bold; }";
    constexpr auto kBadgePaused =
        "QLabel { background: #4a3800; color: #ffcc44; "
        "border: 1px solid #d4a017; border-radius: 2px; font-size: 10px; "
        "font-weight: bold; }";
    constexpr auto kBadgeOff =
        "QLabel { background: #141e28; color: #4a5a6a; "
        "border: 1px solid #243444; border-radius: 2px; font-size: 10px; "
        "font-weight: bold; }";

    // A note whose text changes, and the readout under FFT Size. Their
    // font is set by setPillNoteText, which fits the text to one line.
    constexpr int kNotePx = 9;
    constexpr auto kNoteFitted =
        "QLabel { background: transparent; border: none; color: #708090; }";
    constexpr auto kReadout =
        "QLabel { background: transparent; border: none; color: #8aa8c0; }"
        "QLabel:disabled { color: #4a5a6a; }";

    constexpr auto kButton =
        "QPushButton { background: #1a3050; color: #c8d8e8; "
        "border: 1px solid #304050; border-radius: 2px; font-size: 10px; "
        "padding: 0 8px; }"
        "QPushButton:hover { background: #254060; }"
        "QPushButton:pressed { background: #0d2040; }"
        "QPushButton:disabled { background: #141e28; color: #4a5a6a; "
        "border: 1px solid #243444; }";

    constexpr auto kLink =
        "QPushButton { background: transparent; border: none; "
        "color: #00b4d8; font-size: 10px; padding: 0; }"
        "QPushButton:hover { color: #40d0f0; }";

    constexpr auto kCombo =
        "QComboBox { background: #1a2a3a; color: #c8d8e8; "
        "border: 1px solid #304050; border-radius: 3px; padding: 0 6px; "
        "font-size: 10px; }"
        "QComboBox:disabled { background: #141e28; color: #4a5a6a; "
        "border: 1px solid #243444; }"
        "QComboBox::drop-down { border: none; width: 16px; }"
        "QComboBox::down-arrow { image: url(:/icons/spin-down.svg); "
        "width: 8px; height: 8px; }"
        "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8; "
        "selection-background-color: #00b4d8; border: 1px solid #304050; }";

    // The number box at the right of the two averaging time rows: the
    // combo's colors and font, with the two step buttons at its right.
    constexpr int kNumberBoxW       = 68;   // "9999 ms" and the step buttons
    constexpr auto kNumberBox =
        "QSpinBox { background: #1a2a3a; color: #c8d8e8; "
        "border: 1px solid #304050; border-radius: 3px; padding: 0 2px; "
        "font-size: 10px; }"
        "QSpinBox:disabled { background: #141e28; color: #4a5a6a; "
        "border: 1px solid #243444; }"
        "QSpinBox::up-button { subcontrol-origin: border; "
        "subcontrol-position: top right; width: 13px; border: none; }"
        "QSpinBox::down-button { subcontrol-origin: border; "
        "subcontrol-position: bottom right; width: 13px; border: none; }"
        "QSpinBox::up-arrow { image: url(:/icons/spin-up.svg); "
        "width: 7px; height: 5px; }"
        "QSpinBox::down-arrow { image: url(:/icons/spin-down.svg); "
        "width: 7px; height: 5px; }";

    inline QString swatch(const QColor& color)
    {
        return QStringLiteral(
            "QPushButton { background: %1; border: 1px solid #506070; "
            "border-radius: 2px; }"
            "QPushButton:disabled { border: 1px solid #243444; }")
            .arg(color.isValid() ? color.name() : QStringLiteral("#000000"));
    }
} // namespace PillStyle

namespace {

void clearPillLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QLayout* nested = item->layout()) { clearPillLayout(nested); }
        const std::unique_ptr<QLayoutItem> owned(item);
    }
}

int pillTextHeight(QLabel* label, const QString& text, int width)
{
    if (label->textFormat() == Qt::RichText
        || (label->textFormat() == Qt::AutoText && Qt::mightBeRichText(text))) {
        QTextDocument document;
        document.setDocumentMargin(0);
        document.setDefaultFont(label->font());
        document.setHtml(text);
        document.setTextWidth(width);
        return qCeil(document.size().height());
    }
    return label->fontMetrics().boundingRect(QRect(0, 0, width, 1000),
        Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text).height();
}

void retainPillButtonTextWidths(QWidget* flyout, const QVector<QPushButton*>& tabButtons)
{
    if (!flyout) { return; }
    for (QPushButton* button : flyout->findChildren<QPushButton*>()) {
        if (button->text().isEmpty() || tabButtons.contains(button)) { continue; }
        button->ensurePolished();
        button->setMinimumWidth(std::max(button->minimumWidth(),
            button->fontMetrics().horizontalAdvance(button->text()) + 2));
    }
}

} // namespace

void SpectrumOverlayPanel::scheduleDisplayPillGeometryRefresh()
{
    if (!WorkspacePreview::enabled() || m_refreshingPillGeometry || m_pillGeometryQueued
        || m_pillPages.size() != 5) { return; }
    // A closed pill is laid out when it opens: showDisplayPill() refreshes
    // it before show(). Laying it out while closed blocked the GUI thread
    // on every remote telemetry tick.
    if (!displayPillVisible()) {
        m_pillGeometryStale = true;
        return;
    }
    m_pillGeometryQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_pillGeometryQueued = false;
        if (!displayPillVisible()) {
            m_pillGeometryStale = true;
            return;
        }
        refreshDisplayPillGeometry();
    });
}

void SpectrumOverlayPanel::fitWorkspacePillGroup(QWidget* holder,
                                                  const QVector<QPushButton*>& pills)
{
    static constexpr int kRoom = PillStyle::kWidth - 2 - 2 * PillStyle::kMarginH;
    // A retained group owns one grid row. Its box rows are rebuilt without
    // creating more widgets, so font/style changes can reflow it repeatedly.
    QVBoxLayout* column = qobject_cast<QVBoxLayout*>(holder->layout());
    if (!column) {
        { const std::unique_ptr<QLayout> old(holder->layout()); }
        column = new QVBoxLayout(holder);
        column->setContentsMargins(0, 0, 0, 0);
    } else {
        clearPillLayout(column);
    }
    column->setSpacing(PillStyle::kSpacing);
    const auto setPadding = [](QPushButton* pill, int padding) {
        QString sheet = pill->property(WorkspacePreview::kOriginalQssProperty).toString();
        if (sheet.isEmpty()) { sheet = pill->styleSheet(); }
        sheet.replace(QRegularExpression(QStringLiteral("padding: 0 [0-9]+px")),
                      QStringLiteral("padding: 0 %1px").arg(padding));
        pill->setMinimumWidth(0);
        pill->setStyleSheet(WorkspacePreview::lookQss(sheet, pill));
        pill->ensurePolished();
        pill->setFixedHeight(std::max(PillStyle::kControlH, pill->fontMetrics().height() + 2));
    };
    int padding = PillStyle::kTogglePadding;
    int total = 0;
    do {
        total = 0;
        for (QPushButton* pill : pills) {
            setPadding(pill, padding);
            total += pill->sizeHint().width();
        }
        if (total + (pills.size() - 1) * PillStyle::kSpacing <= kRoom
            || padding == PillStyle::kTogglePaddingTight) { break; }
        --padding;
    } while (true);
    const int gap = total + (pills.size() - 1) * PillStyle::kSpacing <= kRoom
        ? PillStyle::kSpacing : PillStyle::kTightSpacing;
    QHBoxLayout* row = nullptr;
    int rowWidth = 0;
    int rowHeight = 0;
    int totalHeight = 0;
    for (QPushButton* pill : pills) {
        const int width = std::max(pill->sizeHint().width(),
                                   pill->fontMetrics().horizontalAdvance(pill->text()) + 2);
        if (!row || rowWidth + gap + width > kRoom) {
            if (row) { row->addStretch(1); totalHeight += rowHeight + PillStyle::kSpacing; }
            row = new QHBoxLayout;
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(gap);
            column->addLayout(row);
            rowWidth = 0;
            rowHeight = 0;
        }
        pill->setMinimumWidth(width);
        row->addWidget(pill);
        rowWidth += (rowWidth ? gap : 0) + width;
        rowHeight = std::max(rowHeight, pill->height());
    }
    if (row) { row->addStretch(1); totalHeight += rowHeight; }
    holder->setFixedHeight(totalHeight);
}

void SpectrumOverlayPanel::refreshDisplayPillGeometry()
{
    if (!WorkspacePreview::enabled() || !m_displayFlyout || m_refreshingPillGeometry) { return; }
    const QScopedValueRollback<bool> own(m_refreshingPillGeometry, true);
    ++m_pillGeometryRefreshes;
    WorkspacePreview::themeSubtree(m_displayFlyout);
    static constexpr int kRoom = PillStyle::kWidth - 2 - 2 * PillStyle::kMarginH;
    for (const PillToggleGroup& group : std::as_const(m_pillToggleGroups)) {
        fitWorkspacePillGroup(group.holder, group.buttons);
    }
    for (QLabel* label : m_displayFlyout->findChildren<QLabel*>()) {
        label->ensurePolished();
        if (label->property("pillMinimumText").isValid()) {
            const auto inkWidth = [label](const QString& text) {
                return label->fontMetrics().boundingRect(QRect(0, 0, 1000, 1000),
                    Qt::AlignLeft | Qt::AlignTop, text).width();
            };
            const int width = std::max({PillStyle::kValueW,
                label->property("pillRequestedWidth").toInt(),
                label->fontMetrics().horizontalAdvance(label->property("pillMinimumText").toString()),
                label->fontMetrics().horizontalAdvance(label->property("pillMaximumText").toString()),
                label->fontMetrics().horizontalAdvance(label->text()),
                inkWidth(label->property("pillMinimumText").toString()),
                inkWidth(label->property("pillMaximumText").toString()),
                inkWidth(label->text())});
            label->setFixedWidth(width);
            label->setFixedHeight(std::max(PillStyle::kLabelH, label->fontMetrics().height()));
        } else if (label->property("pillNoteLines").isValid()) {
            label->setWordWrap(true);
            int room = kRoom;
            if (label->parentWidget()->property("pillModeStatus").toBool() && m_clarityRetuneBtn) {
                // Release the classic hidden-page width when the shared
                // workspace theme takes over this same Display island.
                label->setMinimumWidth(0);
                label->setMaximumWidth(QWIDGETSIZE_MAX);
                room -= m_clarityBadge->width() + m_clarityRetuneBtn->sizeHint().width()
                        + 2 * PillStyle::kSpacing;
            }
            int height = std::max(label->property("pillNoteLines").toInt() * label->fontMetrics().height(),
                                  pillTextHeight(label, label->text(), room));
            if (label->objectName() == QStringLiteral("pillReason")) {
                for (QWidget* widget : label->parentWidget()->findChildren<QWidget*>()) {
                    height = std::max(height, pillTextHeight(label, widget->toolTip(), kRoom));
                }
            }
            label->setFixedHeight(height);
            if (label->parentWidget()->property("pillModeStatus").toBool()) {
                // This row shares the note's measured height. Replacing a
                // longer mode note must also release its previous height.
                label->parentWidget()->setFixedHeight(std::max(height, PillStyle::kControlH));
            }
        } else {
            const bool header = label->objectName() == QStringLiteral("pillHeader")
                             || label->objectName() == QStringLiteral("pillHeaderScope");
            label->setMinimumHeight(std::max(header ? PillStyle::kHeaderH : PillStyle::kLabelH,
                                             label->fontMetrics().height()));
            if (label->maximumHeight() < label->minimumHeight()) {
                label->setMaximumHeight(label->minimumHeight());
            }
        }
    }
    for (QWidget* widget : m_displayFlyout->findChildren<QWidget*>()) {
        if (widget->property("pillHeaderGap").isValid()) {
            QLabel* header = widget->findChild<QLabel*>(QStringLiteral("pillHeader"));
            const int gap = widget->property("pillHeaderGap").toInt()
                - (widget->property("pillLevelsSectionGap").toBool() ? 2 : 0);
            widget->layout()->setContentsMargins(0, gap, 0, 0);
            widget->setFixedHeight(std::max(PillStyle::kHeaderH, header->fontMetrics().height()) + gap);
        }
        if (QSpinBox* box = qobject_cast<QSpinBox*>(widget)) {
            box->ensurePolished();
            box->setFixedHeight(std::max(PillStyle::kControlH, box->fontMetrics().height() + 4));
            QStyleOptionSpinBox option;
            option.initFrom(box);
            option.rect = box->rect();
            option.subControls = QStyle::SC_All;
            option.buttonSymbols = box->buttonSymbols();
            option.frame = box->hasFrame();
            const QRect field = box->style()->subControlRect(QStyle::CC_SpinBox, &option,
                                                            QStyle::SC_SpinBoxEditField, box);
            QLineEdit* edit = box->findChild<QLineEdit*>();
            const int textWidth = edit ? edit->fontMetrics().horizontalAdvance(QStringLiteral("9999 ms")) + 4
                                      : box->fontMetrics().horizontalAdvance(QStringLiteral("9999 ms")) + 4;
            box->setFixedWidth(std::max(PillStyle::kNumberBoxW, box->width() + textWidth - field.width()));
            box->parentWidget()->setFixedHeight(std::max(box->height(),
                Workspace::Components::kSliderHandleSpanPx));
        }
        if (widget->property("pillFooter").toBool()) {
            auto* box = qobject_cast<QHBoxLayout*>(widget->layout());
            QPushButton* reset = widget->findChild<QPushButton*>(QStringLiteral("pillResetPan"));
            QPushButton* link = widget->findChild<QPushButton*>(QStringLiteral("pillSetupDefaults"));
            clearPillLayout(box);
            reset->setMinimumWidth(0);
            link->setMinimumWidth(0);
            reset->ensurePolished();
            link->ensurePolished();
            const QString maximumText = reset->property("pillMaximumFooterText").toString();
            const int resetPadding = std::max(2, reset->sizeHint().width()
                - reset->fontMetrics().horizontalAdvance(reset->text()));
            reset->setFixedWidth(std::max(reset->sizeHint().width(),
                reset->fontMetrics().horizontalAdvance(maximumText.isEmpty() ? reset->text() : maximumText)
                    + resetPadding));
            link->setMinimumWidth(std::max(link->sizeHint().width(),
                                           link->fontMetrics().horizontalAdvance(link->text()) + 2));
            const bool stacked = reset->minimumWidth() + link->minimumWidth() + PillStyle::kSpacing > kRoom;
            widget->setProperty("pillFooterStacked", stacked);
            box->setDirection(stacked ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            box->addWidget(reset, 0, Qt::AlignLeft);
            if (!stacked) { box->addStretch(1); }
            box->addWidget(link, 0, Qt::AlignRight);
            widget->setFixedHeight((stacked ? 2 : 1) * PillStyle::kControlH
                                   + (stacked ? PillStyle::kSpacing : 0));
        }
    }
    for (QWidget* widget : m_displayFlyout->findChildren<QWidget*>()) {
        if (QHBoxLayout* box = qobject_cast<QHBoxLayout*>(widget->layout())) {
            if (widget->property("pillFooter").toBool()) { continue; }
            int height = widget->minimumHeight();
            for (int index = 0; index < box->count(); ++index) {
                if (QWidget* child = box->itemAt(index)->widget()) {
                    height = std::max(height, child->minimumHeight());
                }
            }
            if (widget->maximumHeight() < height) { widget->setFixedHeight(height); }
        }
    }
    retainPillButtonTextWidths(m_displayFlyout, m_pillTabBtns);
    int tabsWidth = 0;
    for (QPushButton* button : std::as_const(m_pillTabBtns)) {
        button->setMinimumWidth(0);
        button->ensurePolished();
        // The tab's original QSS has zero padding and a 1px border.
        // The generic button hint can contain extra platform padding;
        // use the actual text plus those two border pixels as its floor.
        const int width = button->fontMetrics().horizontalAdvance(button->text()) + 2;
        button->setMinimumWidth(width);
        tabsWidth += width;
    }
    m_displayFlyout->setProperty("pillTabTextBudget", tabsWidth + 8);
    for (QWidget* page : std::as_const(m_pillPages)) {
        if (page && page->layout()) {
            // The workspace handles and readable notes keep their full
            // size; the existing compact gap supplies the extra room.
            if (QGridLayout* grid = qobject_cast<QGridLayout*>(page->layout())) {
                QWidget* footer = page->findChild<QPushButton*>(QStringLiteral("pillResetPan"))->parentWidget();
                const bool levels = page == m_pillPages[static_cast<int>(DisplayPillTab::Levels)];
                const bool compactFooterWrap = (levels || page == m_pillPages[static_cast<int>(DisplayPillTab::Spectrum)])
                    && footer->property("pillFooterStacked").toBool();
                if (levels && compactFooterWrap) {
                    for (QWidget* header : page->findChildren<QWidget*>()) {
                        if (!header->property("pillLevelsSectionGap").toBool()) { continue; }
                        QLabel* text = header->findChild<QLabel*>(QStringLiteral("pillHeader"));
                        header->layout()->setContentsMargins(0, PillStyle::kTightSpacing, 0, 0);
                        header->setFixedHeight(text->fontMetrics().height() + PillStyle::kTightSpacing);
                    }
                }
                // A fixed maximum-count footer can need its approved second
                // line. Use the existing tight row gap to retain every text
                // and workspace handle within the same 480px envelope.
                grid->setVerticalSpacing(compactFooterWrap ? PillStyle::kTightSpacing : PillStyle::kSpacingWrapped);
            }
            page->layout()->invalidate();
            page->layout()->activate();
        }
    }
    m_displayFlyout->layout()->invalidate();
    m_displayFlyout->layout()->activate();
    layoutDisplayPill();
    positionPillOwnIndicators();
}

// The last tab used, one key for this computer.
static constexpr auto kPillLastTabKey = "DisplayPillLastTab";

// Historical separate-switch notes (design 3.5) are superseded by the
// approved chooser notes in updateWaterfallNote; the requested mode now
// owns the receive editors even before a runtime level is available.
// Why dB Max and dB Min are greyed while the pan is keyed (follow-up wave
// F3): what they set is the receive scale, which is not on show.
static constexpr auto kKeyedScaleNote =
    "These set the receive scale. Change them when not transmitting.";

// A note is one line and is never cut off: one a few pixels too long for
// the pill is drawn with its letters a hair closer together.
static void setPillNoteText(QLabel* note, const QString& text)
{
    static constexpr qreal kTightenMaxPx = 0.4;
    static constexpr int kRoom =
        PillStyle::kWidth - 2 - 2 * PillStyle::kMarginH;   // less border and margins
    if (WorkspacePreview::enabled()) {
        note->setWordWrap(true);
        note->setText(text);
        return;
    }
    QFont font = note->font();
    font.setPixelSize(PillStyle::kNotePx);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
    const int over = QFontMetrics(font).horizontalAdvance(text) - kRoom;
    if (over > 0) {
        font.setLetterSpacing(QFont::AbsoluteSpacing,
                              -std::min(kTightenMaxPx, qreal(over + 1) / qreal(text.size())));
    }
    note->setFont(font);
    note->setText(text);
}

// Keyboard steps of the FFT Size slider ask once, this long after the last
// (shared with Setup's slider, DisplayFftReadout.h).
static constexpr int kFftSizeKeyDelayMs = kDisplayFftSizeKeyDelayMs;
// The Hz/bin slider counts in its 0.5 Hz steps.
static constexpr int kHzPerBinStepsPerHz = 2;

static QString pillTabName(SpectrumOverlayPanel::DisplayPillTab tab)
{
    switch (tab) {
    case SpectrumOverlayPanel::DisplayPillTab::Spectrum:  return QStringLiteral("Spectrum");
    case SpectrumOverlayPanel::DisplayPillTab::Levels:    return QStringLiteral("Levels");
    case SpectrumOverlayPanel::DisplayPillTab::Fft:       return QStringLiteral("Fft");
    case SpectrumOverlayPanel::DisplayPillTab::Waterfall: return QStringLiteral("Waterfall");
    case SpectrumOverlayPanel::DisplayPillTab::View3D:    return QStringLiteral("View3D");
    }
    return QStringLiteral("Spectrum");
}

static SpectrumOverlayPanel::DisplayPillTab pillTabFromName(const QString& name)
{
    for (const auto tab : {SpectrumOverlayPanel::DisplayPillTab::Levels,
                           SpectrumOverlayPanel::DisplayPillTab::Fft,
                           SpectrumOverlayPanel::DisplayPillTab::Waterfall,
                           SpectrumOverlayPanel::DisplayPillTab::View3D}) {
        if (name == pillTabName(tab)) {
            return tab;
        }
    }
    return SpectrumOverlayPanel::DisplayPillTab::Spectrum;
}

void SpectrumOverlayPanel::buildDisplayFlyout()
{
    m_displayFlyout = new QWidget(parentWidget());
    claimMouseForFlyout(m_displayFlyout);
    m_displayFlyout->setObjectName(QStringLiteral("displayPill"));
    m_displayFlyout->setProperty(WorkspacePreview::kThemedIslandProperty, true);
    m_displayFlyout->setAttribute(Qt::WA_StyledBackground, true);
    m_displayFlyout->setStyleSheet(PillStyle::kFrame);
    m_displayFlyout->setFixedWidth(PillStyle::kWidth);
    m_displayFlyout->hide();

    // 1 px all round: the frame's own border.
    auto* column = new QVBoxLayout(m_displayFlyout);
    column->setContentsMargins(1, 1, 1, 1);
    column->setSpacing(0);

    // Approved live mockup: five tabs grow from their text (CSS flex-basis: auto).
    auto* tabs = new QHBoxLayout;
    tabs->setContentsMargins(PillStyle::kMarginH, PillStyle::kMarginV,
                             PillStyle::kMarginH, 0);
    tabs->setSpacing(2);
    const struct {
        DisplayPillTab tab;
        const char*    text;
        const char*    objectName;
    } tabTable[] = {
        {DisplayPillTab::Spectrum,  "Spectrum",  "displayTabSpectrum"},
        {DisplayPillTab::Levels,    "Levels",    "displayTabLevels"},
        {DisplayPillTab::Waterfall, "Waterfall", "displayTabWaterfall"},
        {DisplayPillTab::View3D,    "View",      "displayTab3D"},
        {DisplayPillTab::Fft,       "FFT",       "displayTabFft"},
    };
    for (const auto& entry : tabTable) {
        auto* btn = new QPushButton(QString::fromLatin1(entry.text), m_displayFlyout);
        btn->setObjectName(QString::fromLatin1(entry.objectName));
        btn->setCheckable(true);
        btn->setFixedHeight(PillStyle::kTabH);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setStyleSheet(PillStyle::kTab);
        const DisplayPillTab tab = entry.tab;
        // Rulings wave R8: the page follows the button however it is
        // activated. A press sent through macOS accessibility (VoiceOver,
        // automation) changes the checked state without `clicked`, so on
        // `clicked` alone the tab lit, the page stayed, and all four could
        // end up lit. `toggled` hears every activation. Exactly one tab is
        // checked at all times: the tab being checked is shown and
        // remembered, and the open tab's own button cannot be unchecked.
        // showPillTab() sets the buttons itself and is not heard here.
        connect(btn, &QPushButton::toggled, this, [this, tab, btn](bool checked) {
            if (m_showingPillTab) {
                return;
            }
            if (checked) {
                setDisplayPillTab(tab);
            } else if (tab == m_pillTab) {
                const QScopedValueRollback<bool> own(m_showingPillTab, true);
                btn->setChecked(true);
            }
        });
        tabs->addWidget(btn, 1);
        m_pillTabBtns.append(btn);
    }
    column->addLayout(tabs);

    // NereusSDR design based on AetherSDR SpectrumOverlayMenu.cpp:1263-1299,
    // 2533-2557 [@1e0718ad]: keep content reachable in a short host. Our
    // approved tab strip remains outside the scrolling pages.
    auto* scroll = new QScrollArea(m_displayFlyout);
    scroll->setObjectName(QStringLiteral("displayPillScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(false);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumHeight(0);
    scroll->setStyleSheet(QStringLiteral("QScrollArea#displayPillScroll { background: transparent; border: none; }"));
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("displayPillPages"));
    content->setFixedWidth(PillStyle::kWidth - 2);
    auto* pages = new QVBoxLayout(content);
    pages->setContentsMargins(0, 0, 0, 0);
    pages->setSpacing(0);
    scroll->setWidget(content);
    content->setAutoFillBackground(false);
    column->addWidget(scroll);

    m_pillPages.fill(nullptr, static_cast<int>(std::size(tabTable)));
    buildPillSpectrumPage();
    buildPillLevelsPage();
    buildPillWaterfallPage();
    buildPillView3DPage();
    buildPillFftPage();

    showPillTab(pillTabFromName(
        AppSettings::instance().value(QString::fromLatin1(kPillLastTabKey)).toString()));

    // A strip built on a pan edits that pan until it is told otherwise.
    setDisplayTarget(qobject_cast<SpectrumWidget*>(parentWidget()));
    updatePillEnables();
}

// ── Display pill: page frame ─────────────────────────────────────────────────

SpectrumOverlayPanel::PillPage SpectrumOverlayPanel::beginPillPage(
    DisplayPillTab tab, const QString& title, const QString& scope)
{
    static constexpr const char* kPageNames[] = {
        "displayPageSpectrum", "displayPageLevels",
        "displayPageWaterfall", "displayPage3D", "displayPageFft"};

    PillPage page;
    page.tab = tab;
    page.widget = new QWidget(m_displayFlyout);
    page.widget->setObjectName(QString::fromLatin1(kPageNames[static_cast<int>(tab)]));
    // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:1300-1303 [@1e0718ad]
    page.grid = new QGridLayout(page.widget);
    page.grid->setContentsMargins(PillStyle::kMarginH, PillStyle::kMarginV,
                                  PillStyle::kMarginH, PillStyle::kMarginV);
    page.grid->setSpacing(PillStyle::kSpacing);
    // grid columns: 0=label, 1=button (optional), 2=slider, 3=value
    page.grid->setColumnMinimumWidth(1, PillStyle::kButtonColW);
    page.grid->setColumnStretch(2, 1);
    addPillHeader(page, title, scope, true);
    return page;
}

// A divider, then "Reset this pan" (left) and "Setup defaults" (right).
void SpectrumOverlayPanel::endPillPage(PillPage& page)
{
    QLabel* reason = addPillNote(page, QStringLiteral("Point at a row to see what sets it."), PillStyle::kReasonLines);
    reason->setObjectName(QStringLiteral("pillReason"));

    auto* sep = new QWidget(page.widget);
    sep->setFixedHeight(PillStyle::kSeparatorH);
    auto* sepBox = new QVBoxLayout(sep);
    sepBox->setContentsMargins(0, 2, 0, 2);
    sepBox->setSpacing(0);
    auto* sepLine = new QFrame(sep);
    sepLine->setFixedHeight(1);
    sepLine->setStyleSheet(PillStyle::kLine);
    sepBox->addWidget(sepLine);
    page.grid->addWidget(sep, page.row++, 0, 1, 4);

    auto* foot = new QWidget(page.widget);
    foot->setProperty("pillFooter", true);
    foot->setFixedHeight(PillStyle::kControlH);
    auto* footBox = new QHBoxLayout(foot);
    footBox->setContentsMargins(0, 0, 0, 0);
    footBox->setSpacing(PillStyle::kSpacing);

    QPushButton* reset = makePillButton(
        QStringLiteral("Reset this pan"),
        QStringLiteral("Put this panadapter's display settings back to the Setup defaults"));
    reset->setObjectName(QStringLiteral("pillResetPan"));
    connect(reset, &QPushButton::clicked, this, &SpectrumOverlayPanel::resetPanRequested);
    footBox->addWidget(reset);
    footBox->addStretch(1);

    auto* link = new QPushButton(QStringLiteral("Setup defaults →"), foot);
    link->setObjectName(QStringLiteral("pillSetupDefaults"));
    link->setFixedHeight(PillStyle::kControlH);
    link->setCursor(Qt::PointingHandCursor);
    link->setFocusPolicy(Qt::NoFocus);
    link->setStyleSheet(PillStyle::kLink);
    link->setToolTip(QStringLiteral("Open the Setup page that holds the defaults for this tab"));
    connect(link, &QPushButton::clicked, this, [this]() {
        emit openSetupRequested(pillSetupPage());
    });
    // Keep both footer labels whole on fonts whose two buttons cannot
    // share the fixed page width. The names and destinations stay the same.
    reset->ensurePolished();
    link->ensurePolished();
    link->setMinimumWidth(link->fontMetrics().horizontalAdvance(link->text()) + 2);
    footBox->addWidget(link);
    static constexpr int kFooterRoom = PillStyle::kWidth - 2 - 2 * PillStyle::kMarginH;
    if (reset->sizeHint().width() + std::max(link->sizeHint().width(), link->minimumWidth())
        + PillStyle::kSpacing > kFooterRoom) {
        footBox->setDirection(QBoxLayout::TopToBottom);
        const std::unique_ptr<QLayoutItem> spacer(footBox->takeAt(1));
        footBox->setAlignment(reset, Qt::AlignLeft);
        footBox->setAlignment(link, Qt::AlignRight);
        foot->setFixedHeight(2 * PillStyle::kControlH + PillStyle::kSpacing);
    }
    page.grid->addWidget(foot, page.row++, 0, 1, 4);

    m_pillPages[static_cast<int>(page.tab)] = page.widget;
    m_displayFlyout->findChild<QWidget*>(QStringLiteral("displayPillPages"))->layout()->addWidget(page.widget);
}

// From AetherSDR src/gui/SpectrumOverlayMenu.cpp:1387-1406 [@1e0718ad]
// (makeHeader: a 9 px bold accent title and a 1 px divider).
// NereusSDR, based on AetherSDR: the divider shares the title's row, with
// an optional tag at its right end, as the approved mockup draws it. The
// mockup lower-cases the title before the small caps (text-transform:
// lowercase), so every letter is a small cap and none is full height.
void SpectrumOverlayPanel::addPillHeader(PillPage& page, const QString& title,
                                         const QString& scope, bool first)
{
    const int gap = first ? 0 : PillStyle::kSubHeaderGap;
    auto* row = new QWidget(page.widget);
    row->setProperty("pillHeaderGap", gap);
    row->setFixedHeight(PillStyle::kHeaderH + gap);
    auto* box = new QHBoxLayout(row);
    box->setContentsMargins(0, gap, 0, 0);
    box->setSpacing(6);

    auto* text = new QLabel(title.toLower(), row);
    text->setObjectName(QStringLiteral("pillHeader"));
    text->setAccessibleName(title);
    text->setStyleSheet(PillStyle::kHeader);
    QFont font = text->font();
    font.setPixelSize(9);
    font.setBold(true);
    font.setCapitalization(QFont::SmallCaps);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    text->setFont(font);
    box->addWidget(text);

    auto* line = new QFrame(row);
    line->setFixedHeight(1);
    line->setStyleSheet(PillStyle::kLine);
    box->addWidget(line, 1);

    if (!scope.isEmpty()) {
        auto* tag = new QLabel(scope, row);
        tag->setObjectName(QStringLiteral("pillHeaderScope"));
        tag->setStyleSheet(PillStyle::kHeaderScope);
        box->addWidget(tag);
    }
    page.grid->addWidget(row, page.row++, 0, 1, 4);
}

QLabel* SpectrumOverlayPanel::addPillNote(PillPage& page, const QString& text, int lines)
{
    auto* note = new QLabel(text, page.widget);
    note->setProperty("pillNoteLines", lines);
    note->setStyleSheet(PillStyle::kNote);
    note->setWordWrap(lines > 1);
    note->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    note->setFixedHeight(PillStyle::kNoteLineH * std::max(1, lines));
    // The note takes the page's width; its text never widens the pill.
    note->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    page.grid->addWidget(note, page.row++, 0, 1, 4);
    return note;
}

// ── Display pill: rows ───────────────────────────────────────────────────────

QLabel* SpectrumOverlayPanel::makePillLabel(const QString& text)
{
    auto* label = new QLabel(text, m_displayFlyout);
    label->setStyleSheet(PillStyle::kLabel);
    label->setFixedHeight(PillStyle::kLabelH);
    return label;
}

QPushButton* SpectrumOverlayPanel::makePillButton(const QString& text, const QString& tip)
{
    auto* button = new QPushButton(text, m_displayFlyout);
    button->setFixedHeight(PillStyle::kControlH);
    button->setFocusPolicy(Qt::NoFocus);
    button->setStyleSheet(PillStyle::kButton);
    button->setToolTip(tip);
    return button;
}

void SpectrumOverlayPanel::registerPillRow(const QString& keyBase,
                                           const QVector<QWidget*>& widgets,
                                           const QString& tip,
                                           std::function<void()> seed)
{
    for (QWidget* widget : widgets) {
        widget->setToolTip(tip);
    }
    if (!m_pillRows.contains(keyBase)) {
        m_pillRowOrder.append(keyBase);
    }
    m_pillRows.insert(keyBase, PillRow{widgets, tip, std::move(seed)});
}

// One line of toggle pills across the page. Pills are added to it with
// addPillToggle and sit at the left.
QHBoxLayout* SpectrumOverlayPanel::addPillLine(PillPage& page)
{
    auto* holder = new QWidget(page.widget);
    holder->setFixedHeight(PillStyle::kControlH);
    // The line takes the page's width; its pills never widen the pill.
    holder->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto* line = new QHBoxLayout(holder);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(PillStyle::kSpacing);
    line->addStretch(1);
    page.grid->addWidget(holder, page.row++, 0, 1, 4);
    return line;
}

// A toggle pill, before the line's trailing stretch. With `offText` the
// pill reads `text` while on and `offText` while off (an Off/On pill).
QPushButton* SpectrumOverlayPanel::addPillToggle(QHBoxLayout* line, const QString& keyBase,
                                                 const QString& text, const QString& tip,
                                                 PillBoolGet get, PillBoolSet set,
                                                 const QString& offText)
{
    QPushButton* pill = makePillToggle(line->parentWidget(), keyBase, text, tip,
                                       std::move(get), std::move(set), offText, false);
    line->insertWidget(line->count() - 1, pill);
    return pill;
}

QPushButton* SpectrumOverlayPanel::makePillToggle(QWidget* parent, const QString& keyBase,
                                                  const QString& text, const QString& tip,
                                                  PillBoolGet get, PillBoolSet set,
                                                  const QString& offText, bool compact)
{
    auto* pill = new QPushButton(offText.isEmpty() ? text : offText, parent);
    pill->setObjectName(QStringLiteral("pill") + keyBase);
    pill->setCheckable(true);
    pill->setFixedHeight(PillStyle::kControlH);
    pill->setFocusPolicy(Qt::NoFocus);
    pill->setStyleSheet(compact ? PillStyle::kToggleCompact : PillStyle::kToggle);

    connect(pill, &QPushButton::toggled, this, [this, pill, text, offText, set](bool on) {
        if (!offText.isEmpty()) {
            pill->setText(on ? text : offText);
        }
        if (m_displayTarget && set) {
            set(m_displayTarget, on);
        }
    });
    registerPillRow(keyBase, {pill}, tip, [this, pill, text, offText, get]() {
        if (!m_displayTarget || !get) {
            return;
        }
        const bool on = get(m_displayTarget);
        const QSignalBlocker block(pill);
        pill->setChecked(on);
        if (!offText.isEmpty()) {
            pill->setText(on ? text : offText);
        }
    });
    return pill;
}

// A label in the first column, then a line from the slider column to the
// right edge. Widgets are added to the line before its trailing stretch.
QHBoxLayout* SpectrumOverlayPanel::addPillLabelledLine(PillPage& page, const QString& label,
                                                       QLabel** labelOut)
{
    QLabel* name = makePillLabel(label);
    page.grid->addWidget(name, page.row, 0);
    if (labelOut) {
        *labelOut = name;
    }
    auto* holder = new QWidget(page.widget);
    holder->setFixedHeight(PillStyle::kControlH);
    auto* line = new QHBoxLayout(holder);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(PillStyle::kSpacing);
    line->addStretch(1);
    page.grid->addWidget(holder, page.row, 2, 1, 2);
    ++page.row;
    return line;
}

// A colour swatch. It shows the target's colour and opens the colour
// chooser on that colour, never on a remembered one.
QPushButton* SpectrumOverlayPanel::makePillSwatch(const QString& keyBase, const QString& tip,
                                                  const QString& dialogTitle,
                                                  PillColorGet get, PillColorSet set)
{
    auto* swatch = new QPushButton(m_displayFlyout);
    swatch->setObjectName(QStringLiteral("pill") + keyBase);
    swatch->setProperty(WorkspacePreview::kSkipProperty, true);
    swatch->setFixedSize(PillStyle::kControlH, PillStyle::kControlH);
    swatch->setFocusPolicy(Qt::NoFocus);
    swatch->setStyleSheet(PillStyle::swatch(QColor()));

    connect(swatch, &QPushButton::clicked, this, [this, keyBase, dialogTitle, get, set]() {
        if (!m_displayTarget || !get || !set) {
            return;
        }
        const QColor initial = get(m_displayTarget);
        const QColor chosen = m_pillColorChooser
            ? m_pillColorChooser(initial, dialogTitle)
            : QColorDialog::getColor(initial, window(), dialogTitle,
                                     QColorDialog::DontUseNativeDialog);
        if (chosen.isValid() && m_displayTarget) {
            set(m_displayTarget, chosen);
        }
        seedPillRow(keyBase);
    });
    registerPillRow(keyBase, {swatch}, tip, [this, swatch, get]() {
        if (!m_displayTarget || !get) {
            return;
        }
        swatch->setStyleSheet(PillStyle::swatch(get(m_displayTarget)));
    });
    return swatch;
}

// A slider row: label | `leading` (a swatch or a small pill, optional) |
// slider | value. `format` turns the slider's position into the value text.
// A value wider than the value column (`valueWidth`) shares the slider's
// cell and the value column, so the column keeps its width and every
// value on the page ends at the right edge.
QSlider* SpectrumOverlayPanel::addPillSlider(PillPage& page, const QString& keyBase,
                                             const QString& label, int min, int max, int step,
                                             const QString& tip, PillFormat format,
                                             PillIntGet get, PillIntSet set,
                                             QWidget* leading, int valueWidth)
{
    QLabel* name = makePillLabel(label);
    name->setObjectName(QStringLiteral("pill") + keyBase + QStringLiteral("Label"));
    page.grid->addWidget(name, page.row, 0);
    if (leading) {
        page.grid->addWidget(leading, page.row, 1, Qt::AlignCenter);
    }

    auto* value = new QLabel(page.widget);
    value->setObjectName(QStringLiteral("pill") + keyBase + QStringLiteral("Value"));
    value->setStyleSheet(PillStyle::kValue);
    value->ensurePolished();
    // The value column stays the same width. A font needing more room
    // borrows from this row's slider through the existing wide-value host.
    const QString minText = format ? format(min) : QString::number(min);
    const QString maxText = format ? format(max) : QString::number(max);
    const int textWidth = std::max(value->fontMetrics().horizontalAdvance(minText),
                                   value->fontMetrics().horizontalAdvance(maxText));
    value->setProperty("pillMinimumText", minText);
    value->setProperty("pillMaximumText", maxText);
    value->setProperty("pillRequestedWidth", valueWidth);
    valueWidth = std::max(valueWidth, textWidth);

    QHBoxLayout* wide = nullptr;
    QWidget* host = page.widget;
    if (valueWidth > PillStyle::kValueW || WorkspacePreview::enabled()) {
        host = new QWidget(page.widget);
        wide = new QHBoxLayout(host);
        wide->setContentsMargins(0, 0, 0, 0);
        wide->setSpacing(PillStyle::kSpacing);
    }

    auto* slider = new GuardedSlider(Qt::Horizontal, host);
    slider->setObjectName(QStringLiteral("pill") + keyBase);
    slider->setRange(min, max);
    slider->setSingleStep(step);
    slider->setPageStep(step);
    slider->setFixedHeight(PillStyle::kSliderH);
    // The wheel moves a slider only once it has been clicked or tabbed to
    // (see eventFilter), so the wheel itself must not give it the focus.
    slider->setFocusPolicy(Qt::StrongFocus);
    slider->setStyleSheet(PillStyle::kSlider);

    value->setFixedSize(wide ? valueWidth : PillStyle::kValueW, PillStyle::kLabelH);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setText(format ? format(slider->value()) : QString::number(slider->value()));
    if (wide) {
        wide->addWidget(slider, 1);
        wide->addWidget(value);
        page.grid->addWidget(host, page.row, 2, 1, 2);
    } else {
        page.grid->addWidget(slider, page.row, 2);
        page.grid->addWidget(value, page.row, 3);
    }
    ++page.row;

    connect(slider, &QSlider::valueChanged, this, [this, value, format, set](int v) {
        value->setText(format ? format(v) : QString::number(v));
        if (m_displayTarget && set) {
            set(m_displayTarget, v);
        }
    });
    registerPillRow(keyBase, {name, slider, value}, tip, [this, slider, value, format, get]() {
        if (!m_displayTarget || !get) {
            return;
        }
        const QSignalBlocker block(slider);
        slider->setValue(get(m_displayTarget));
        value->setText(format ? format(slider->value())
                              : QString::number(slider->value()));
    });
    registerPillParentLabel(keyBase, name, leading);
    return slider;
}

namespace {

// A pill slider whose Page Up and Page Down keys move `pageKeyStep`. Its
// page step stays its single step, as on every pill slider, so a click on
// the groove does what it does on the others.
class PillPageKeySlider final : public GuardedSlider {
public:
    PillPageKeySlider(Qt::Orientation orientation, QWidget* parent, int pageKeyStep)
        : GuardedSlider(orientation, parent)
        , m_pageKeyStep(pageKeyStep)
    {
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_PageUp) {
            setValue(value() + m_pageKeyStep);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_PageDown) {
            setValue(value() - m_pageKeyStep);
            event->accept();
            return;
        }
        GuardedSlider::keyPressEvent(event);
    }

private:
    int m_pageKeyStep{1};
};

// A pill number box. Enter takes the typed value and goes no further (a
// spin box hands the key on to its parents, which here are the pill and
// the pan under it).
class PillNumberBox final : public QSpinBox {
public:
    using QSpinBox::QSpinBox;

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        QSpinBox::keyPressEvent(event);
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            event->accept();
        }
    }
};

} // namespace

// An averaging time row (Avg Time, WF Avg Time): label | slider | number
// box. NereusSDR design; Thetis has the number box alone
// (udDisplayAVGTime, udDisplayAVTimeWF).
//
// The slider is one position per ms from the floor to
// ControlRanges::kDisplayAvgTimePillMaxMs. An arrow key or a wheel notch
// moves it 1 ms; Page Up and Page Down move it
// ControlRanges::kDisplayAvgTimePillPageMs.
//
// The box holds Thetis's whole range in steps of 1 ms:
// From Thetis Project Files/Source/Console/setup.designer.cs:34996-35023 [v2.10.3.15]
//   (udDisplayAVGTime: Minimum 1, Maximum 9999, Increment 1, Value 30)
// From Thetis Project Files/Source/Console/setup.designer.cs:34588-34616 [v2.10.3.15]
//   (udDisplayAVTimeWF: Minimum 1, Maximum 9999, Increment 1, Value 120)
// It takes the value column and part of the slider's cell, so the pill
// keeps its width.
//
// Each follows the other. A time past the slider's top leaves the handle
// at the right end and is written nowhere: showing it changes nothing, and
// the slider takes over from its top only when it is moved.
QSlider* SpectrumOverlayPanel::addPillAvgTimeSlider(PillPage& page, const QString& keyBase,
                                                    const QString& label, const QString& tip,
                                                    PillIntGet get, PillIntSet set)
{
    QLabel* name = makePillLabel(label);
    page.grid->addWidget(name, page.row, 0);

    auto* host = new QWidget(page.widget);
    host->setFixedHeight(PillStyle::kControlH);
    auto* line = new QHBoxLayout(host);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(PillStyle::kSpacing);

    auto* slider = new PillPageKeySlider(Qt::Horizontal, host,
                                         ControlRanges::kDisplayAvgTimePillPageMs);
    slider->setObjectName(QStringLiteral("pill") + keyBase);
    slider->setRange(ControlRanges::kDisplayAvgTimeMinMs,
                     ControlRanges::kDisplayAvgTimePillMaxMs);
    slider->setSingleStep(ControlRanges::kDisplayAvgTimeStepMs);
    slider->setPageStep(ControlRanges::kDisplayAvgTimeStepMs);
    slider->setFixedHeight(PillStyle::kSliderH);
    // As addPillSlider: the wheel must not give the slider the focus.
    slider->setFocusPolicy(Qt::StrongFocus);
    slider->setStyleSheet(PillStyle::kSlider);

    auto* box = new PillNumberBox(host);
    box->setObjectName(QStringLiteral("pill") + keyBase + QStringLiteral("Box"));
    box->setRange(ControlRanges::kDisplayAvgTimeMinMs, ControlRanges::kDisplayAvgTimeMaxMs);
    box->setSingleStep(ControlRanges::kDisplayAvgTimeStepMs);
    box->setSuffix(QStringLiteral(" ms"));
    box->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    box->setFixedSize(PillStyle::kNumberBoxW, PillStyle::kControlH);
    box->setFocusPolicy(Qt::StrongFocus);
    // A typed time is taken when it is finished (Enter, or leaving the
    // box), not digit by digit.
    box->setKeyboardTracking(false);
    box->setStyleSheet(PillStyle::kNumberBox);

    line->addWidget(slider, 1);
    line->addWidget(box);
    page.grid->addWidget(host, page.row, WorkspacePreview::enabled() ? 1 : 2,
                         1, WorkspacePreview::enabled() ? 3 : 2);
    ++page.row;

    // What the slider shows for a time: the time, or its top.
    const auto onSlider = [](int timeMs) {
        return std::min(timeMs, ControlRanges::kDisplayAvgTimePillMaxMs);
    };

    connect(slider, &QSlider::valueChanged, this, [this, box, set](int v) {
        {
            const QSignalBlocker block(box);
            box->setValue(v);
        }
        if (m_displayTarget && set) {
            set(m_displayTarget, v);
        }
    });
    connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this, slider, set, onSlider](int v) {
        {
            const QSignalBlocker block(slider);
            slider->setValue(onSlider(v));
        }
        if (m_displayTarget && set) {
            set(m_displayTarget, v);
        }
    });
    registerPillRow(keyBase, {name, slider, box}, tip, [this, slider, box, get, onSlider]() {
        if (!m_displayTarget || !get) {
            return;
        }
        const int timeMs = get(m_displayTarget);
        const QSignalBlocker blockSlider(slider);
        const QSignalBlocker blockBox(box);
        slider->setValue(onSlider(timeMs));
        box->setValue(timeMs);
    });
    return slider;
}

// A combo row: label | combo across the other three columns. Each choice
// carries the value the getter and setter speak.
QComboBox* SpectrumOverlayPanel::addPillCombo(PillPage& page, const QString& keyBase,
                                              const QString& label, const PillChoices& items,
                                              const QString& tip, PillIntGet get, PillIntSet set, QWidget* leading)
{
    QLabel* name = makePillLabel(label);
    name->setObjectName(QStringLiteral("pill") + keyBase + QStringLiteral("Label"));
    page.grid->addWidget(name, page.row, 0);

    auto* combo = new GuardedComboBox(page.widget);
    combo->setObjectName(QStringLiteral("pill") + keyBase);
    for (const auto& [text, value] : items) {
        combo->addItem(text, value);
    }
    combo->setFixedHeight(PillStyle::kControlH);
    combo->setFocusPolicy(Qt::StrongFocus);
    combo->setStyleSheet(PillStyle::kCombo);
    if (leading) {
        page.grid->addWidget(leading, page.row, 1, Qt::AlignCenter);
        page.grid->addWidget(combo, page.row, 2, 1, 2);
    } else {
        page.grid->addWidget(combo, page.row, 1, 1, 3);
    }
    ++page.row;

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, combo, set](int index) {
        if (index < 0 || !m_displayTarget || !set) {
            return;
        }
        set(m_displayTarget, combo->itemData(index).toInt());
    });
    registerPillRow(keyBase, {name, combo}, tip, [this, combo, get]() {
        if (!m_displayTarget || !get) {
            return;
        }
        const int index = combo->findData(get(m_displayTarget));
        if (index < 0) {
            return;
        }
        const QSignalBlocker block(combo);
        combo->setCurrentIndex(index);
    });
    registerPillParentLabel(keyBase, name, leading);
    return combo;
}

// The live page labels compact switch/child rows with the parent's name.
// A child being off greys its editor; its parent label and switch stay usable.
void SpectrumOverlayPanel::registerPillParentLabel(const QString& childKey, QLabel* label,
                                                   QWidget* leading)
{
    if (!leading || (childKey != QStringLiteral("DisplayPeakHoldResetMs")
        && childKey != QStringLiteral("DisplayFreqGridSpacing")
        && childKey != QStringLiteral("WaterfallBlankingThresholdDb"))) {
        return;
    }
    const QString parentKey = leading->objectName().mid(4);
    PillRow& child = m_pillRows[childKey];
    child.widgets.removeAll(label);
    const PillRow parent = m_pillRows.value(parentKey);
    QVector<QWidget*> widgets = parent.widgets;
    widgets.prepend(label);
    registerPillRow(parentKey, widgets, parent.tip, parent.seed);
}

// Picture-only inventory exclusions settled in the approved rebuild. These
// legacy keys can still be in the persistence inventory without being a
// pan's picture control (and without changing the existing whole-pan reset).
static bool pillPictureKey(const QString& key)
{
    static const QStringList excluded = {
        QStringLiteral("DisplayBandwidth"), QStringLiteral("DisplayTxViewBandwidth"),
        QStringLiteral("DisplayCtunEnabled"), QStringLiteral("DisplaySpectrumFrac"),
        QStringLiteral("DisplayTxGridRefLevel"), QStringLiteral("DisplayTxGridDynamicRange"),
        QStringLiteral("DisplayGridMax"), QStringLiteral("DisplayGridMin"), QStringLiteral("Display3DFloorDepth"),
        QStringLiteral("DisplayGridColor"), QStringLiteral("DisplayGridFineColor"),
        QStringLiteral("DisplayHGridColor"), QStringLiteral("DisplayGridTextColor"),
        QStringLiteral("DisplayBandEdgeColor"), QStringLiteral("DisplayRxZeroLineColor"),
        QStringLiteral("DisplayTxZeroLineColor"), QStringLiteral("DisplayRxFilterColor"),
        QStringLiteral("DisplayTxFilterColor"), QStringLiteral("DisplayTnfFilterColor"),
        QStringLiteral("DisplaySubRxFilterColor")};
    return !excluded.contains(key) && !key.startsWith(QStringLiteral("DisplayTxWf"));
}

QStringList SpectrumOverlayPanel::pillOwnKeys(const QString& row) const
{
    if (row == QStringLiteral("WaterfallLevelsMode")) {
        return {QStringLiteral("ClarityEnabled"), QStringLiteral("DisplayWfAgc"),
                QStringLiteral("WaterfallNFAGCEnabled"), QStringLiteral("DisplayWfUseSpectrumMinMax")};
    }
    if (row == QStringLiteral("SpectrumScaleMode")) {
        return {QStringLiteral("DisplayAdjustGridMinToNoiseFloor")};
    }
    if (!m_displayTarget || !pillPictureKey(row)
        || !m_displayTarget->perPanDisplayKeyBases().contains(row)) { return {}; }
    return {row};
}

void SpectrumOverlayPanel::positionPillOwnIndicators()
{
    if (m_positioningOwnDots) { return; }
    const QScopedValueRollback<bool> own(m_positioningOwnDots, true);
    for (QPushButton* dot : std::as_const(m_pillOwnDots)) {
        QWidget* anchor = qobject_cast<QWidget*>(dot->property("pillOwnAnchor").value<QObject*>());
        if (!anchor) { continue; }
        if (dot->parentWidget() == anchor) {
            dot->move(std::max(0, anchor->width() - dot->width()), 0);
        } else {
            const QPoint origin = dot->parentWidget()->mapFromGlobal(anchor->mapToGlobal(QPoint(0, 0)));
            dot->move(origin.x() - PillStyle::kMarginH,
                      origin.y() + (anchor->height() - dot->height()) / 2);
        }
        dot->raise();
    }
}

void SpectrumOverlayPanel::updatePillOwnIndicators()
{
    const SpectrumWidget* target = m_displayTarget;
    const QStringList inventory = target ? target->perPanDisplayKeyBases() : QStringList();
    if (!target) {
        for (QPushButton* dot : std::as_const(m_pillOwnDots)) { dot->hide(); }
    }
    const auto isOwn = [target](const QString& key) {
        if (!target) { return false; }
        const QString fallback = target->perPanShipDefault(key);
        QString inherited = AppSettings::instance().value(key, fallback).toString();
        if (inherited.isEmpty()) { inherited = fallback; }
        // Compare the live value, including coalesced pending edits. Reading
        // the pure getter never creates keys or flushes the pending save.
        return !PanDisplayKeys::valuesEqual(target->perPanDisplayValue(key), inherited);
    };
    // Actual Setup controls absent from the pill. Hidden layout/scaffolding
    // inventory entries must never be called rows that Setup shows.
    static const QStringList setupRows = {
        QStringLiteral("DisplayShowNoiseFloorPosition"), QStringLiteral("DisplayNoiseFloorColor"),
        QStringLiteral("DisplayNoiseFloorTextColor"), QStringLiteral("DisplayNoiseFloorFastColor"),
        QStringLiteral("DisplayNoiseFloorLineWidth"), QStringLiteral("DisplayNoiseFloorShiftDb"),
        QStringLiteral("DisplayPeakValuePosition"), QStringLiteral("DisplayPeakTextDelayMs"),
        QStringLiteral("DisplayPeakValueColor"), QStringLiteral("DisplayActivePeakHoldEnabled"),
        QStringLiteral("DisplayActivePeakHoldDurationMs"), QStringLiteral("DisplayActivePeakHoldDropDbPerSec"),
        QStringLiteral("DisplayActivePeakHoldFill"), QStringLiteral("DisplayActivePeakHoldOnTx"),
        QStringLiteral("DisplayActivePeakHoldColor"), QStringLiteral("DisplayPeakBlobsEnabled"),
        QStringLiteral("DisplayPeakBlobsCount"), QStringLiteral("DisplayPeakBlobsInsideFilterOnly"),
        QStringLiteral("DisplayPeakBlobsHoldEnabled"), QStringLiteral("DisplayPeakBlobsHoldMs"),
        QStringLiteral("DisplayPeakBlobsHoldDrop"), QStringLiteral("DisplayPeakBlobsFallDbPerSec"),
        QStringLiteral("DisplayPeakBlobColor"), QStringLiteral("DisplayPeakBlobTextColor"),
        QStringLiteral("DisplayFreqScaleFontPx"), QStringLiteral("DisplayFreqLabelAlign"),
        QStringLiteral("DisplayDbmScaleVisible"), QStringLiteral("DisplayShowFps")};
    const QStringList modeKeys = pillOwnKeys(QStringLiteral("WaterfallLevelsMode"));
    int total = 0;
    int setupOnly = 0;
    int maximum = 1;
    bool modeOwn = false;
    for (const QString& key : inventory) {
        if (!pillPictureKey(key)) { continue; }
        if (modeKeys.contains(key)) { modeOwn = modeOwn || isOwn(key); continue; }
        ++maximum;
        if (isOwn(key)) {
            ++total;
            if (setupRows.contains(key)) { ++setupOnly; }
        }
    }
    total += modeOwn ? 1 : 0;
    for (const QString& rowKey : std::as_const(m_pillRowOrder)) {
        const QStringList keys = pillOwnKeys(rowKey);
        if (keys.isEmpty()) { continue; }
        const PillRow& row = m_pillRows[rowKey];
        if (row.widgets.isEmpty()) { continue; }
        QPushButton* dot = m_pillOwnDots.value(rowKey);
        if (!dot) {
            QWidget* anchor = m_displayFlyout->findChild<QWidget*>(QStringLiteral("pill") + rowKey + QStringLiteral("Label"));
            if (!anchor) { anchor = row.widgets.first(); }
            if (rowKey == QStringLiteral("WaterfallLevelsMode") || rowKey == QStringLiteral("SpectrumScaleMode")
                || rowKey == QStringLiteral("DisplaySpectrumRenderMode")) {
                anchor = m_displayFlyout->findChild<QWidget*>(QStringLiteral("pill") + rowKey);
            }
            QWidget* page = anchor;
            while (page->parentWidget() && !m_pillPages.contains(page)) { page = page->parentWidget(); }
            const bool corner = qobject_cast<QAbstractButton*>(anchor) != nullptr;
            dot = new QPushButton(corner ? anchor : page);
            dot->setObjectName(QStringLiteral("pillOwn") + rowKey);
            dot->setProperty("pillOwnAnchor", QVariant::fromValue(static_cast<QObject*>(anchor)));
            dot->setProperty(WorkspacePreview::kSkipProperty, true);
            dot->setFixedSize(7, 7); // Approved live mockup .dot: 7px amber mark.
            dot->setFocusPolicy(Qt::NoFocus);
            dot->setCursor(Qt::PointingHandCursor);
            dot->setStyleSheet(QStringLiteral("QPushButton { background: #ffb800; border: none; border-radius: 3px; padding: 0; min-width: 0; } QPushButton:disabled { background: #78623a; }"));
            connect(dot, &QPushButton::clicked, this, [this, keys]() {
                if (m_displayTarget) { m_displayTarget->resetPanDisplayOverridesForKeys(keys); }
            });
            m_pillOwnDots.insert(rowKey, dot);
        }
        bool ownValue = false;
        for (const QString& key : keys) { ownValue = ownValue || isOwn(key); }
        dot->setVisible(ownValue);
        dot->setEnabled(row.widgets.last()->isEnabled());
        QString name = QStringLiteral("value");
        if (rowKey == QStringLiteral("DisplayPeakHoldResetMs")) { name = QStringLiteral("Peak Delay"); }
        else if (rowKey == QStringLiteral("WaterfallBlankingThresholdDb")) { name = QStringLiteral("NB threshold"); }
        else if (rowKey == QStringLiteral("DisplayBackgroundImage")) { name = QStringLiteral("Picture"); }
        else if (rowKey.endsWith(QStringLiteral("Mode"))) { name = QStringLiteral("choice"); }
        else if (rowKey.endsWith(QStringLiteral("Color"))) { name = QStringLiteral("color"); }
        dot->setToolTip(QStringLiteral("Pan %1's own %2. Click the dot to follow the Setup default again.")
            .arg(target ? target->panIndex() + 1 : 1).arg(name));
    }
    for (QWidget* page : std::as_const(m_pillPages)) {
        if (!page) { continue; }
        QPushButton* reset = page->findChild<QPushButton*>(QStringLiteral("pillResetPan"));
        if (!reset) { continue; }
        reset->setProperty("ownValueCount", total);
        reset->setProperty("setupOnlyOwnValueCount", setupOnly);
        reset->setText(total ? QStringLiteral("Reset this pan (%1)").arg(total) : QStringLiteral("Reset this pan"));
        reset->ensurePolished();
        const QString longest = QStringLiteral("Reset this pan (%1)").arg(maximum);
        reset->setProperty("pillMaximumFooterText", longest);
        int padding = reset->property("pillFooterTextPadding").toInt();
        if (padding <= 0) {
            padding = std::max(2, reset->sizeHint().width()
                                      - reset->fontMetrics().horizontalAdvance(reset->text()));
            reset->setProperty("pillFooterTextPadding", padding);
        }
        reset->setFixedWidth(reset->fontMetrics().horizontalAdvance(longest) + padding);
        const QString summary = total
            ? QStringLiteral("Pan %1 has %2 own values; %3 are on rows only Setup shows.")
                .arg(target->panIndex() + 1).arg(total).arg(setupOnly)
            : QStringLiteral("This pan follows the Setup defaults in every picture row.");
        reset->setToolTip(summary + QStringLiteral(" Reset this pan restores its Setup defaults."));
        QLabel* reason = page->findChild<QLabel*>(QStringLiteral("pillReason"));
        if (reason && !reason->property("pillHoverReason").isValid()) { reason->setText(summary); }
    }
    positionPillOwnIndicators();
}

// ── Display pill: binding ────────────────────────────────────────────────────

void SpectrumOverlayPanel::seedPillRow(const QString& keyBase)
{
    const auto it = m_pillRows.constFind(keyBase);
    if (it != m_pillRows.constEnd() && it->seed) {
        it->seed();
    }
}

// Every row shows the target's value. Nothing is written: each row's seed
// blocks its control's signals.
void SpectrumOverlayPanel::seedDisplayPill()
{
    for (const QString& keyBase : std::as_const(m_pillRowOrder)) {
        seedPillRow(keyBase);
    }
    updatePillEnables();
}

// Seed, then follow the target for as long as the pill is open. A change
// made anywhere else (Setup, the right-click menu, a reset) is shown, and
// showing it writes nothing back.
void SpectrumOverlayPanel::bindDisplayPill()
{
    unbindDisplayPill();
    seedDisplayPill();
    SpectrumWidget* target = m_displayTarget;
    if (!target) {
        return;
    }

    m_pillConns.append(connect(target, &SpectrumWidget::displaySettingChanged, this,
                               [this](const QString& keyBase) {
        if (keyBase == QStringLiteral("DisplayWfEffectiveLevels")) {
            // Runtime levels change the readouts, not the stored editors,
            // mode choices, row gates or ownership indicators.
            updateWaterfallReadouts();
            return;
        }
        seedPillRow(keyBase);
        seedPillRow(QStringLiteral("WaterfallLevelsMode"));
        seedPillRow(QStringLiteral("SpectrumScaleMode"));
        if (keyBase == QStringLiteral("DisplayWfUpdatePeriodMs")
            || keyBase == QStringLiteral("DisplayWaterfallHistoryMs")) {
            seedPillRow(QStringLiteral("WaterfallRewindNote"));
            if (QWidget* page = m_pillPages[static_cast<int>(DisplayPillTab::Waterfall)]) {
                if (QLayout* layout = page->layout()) {
                    layout->activate();
                }
            }
            layoutDisplayPill();
        }
        // Any picture setting may be a parent of another row in the
        // approved control list. Showing the gates writes no settings.
        updatePillEnables();
    }));
    // Clarity taking or giving up this pan's levels greys the other set.
    m_pillConns.append(connect(target, &SpectrumWidget::clarityActiveChanged, this,
                               [this](bool active) {
        m_clarityActive = active;
        updateClarityBadge();
        updatePillEnables();
    }));
    // Keying or un-keying the pan greys or frees dB Max and dB Min.
    m_pillConns.append(connect(target, &SpectrumWidget::displayKeyedChanged, this,
                               [this](bool) { updatePillEnables(); }));
    // A reset reloads the pan with nothing announced: read every row again.
    m_pillConns.append(connect(target, &SpectrumWidget::panDisplayReset, this,
                               [this]() { seedDisplayPill(); }));
    m_pillConns.append(connect(target, &SpectrumWidget::peakHoldEnabledChanged, this,
                               [this](bool) {
        seedPillRow(QStringLiteral("DisplayPeakHoldEnabled"));
        updatePillEnables();
    }));

    // The 3D rows and the two fill rows have their own signals.
    const auto follow = [this](const char* keyBase) {
        return [this, key = QString::fromLatin1(keyBase)]() {
            seedPillRow(key);
            updatePillEnables();
        };
    };
    m_pillConns.append(connect(target, &SpectrumWidget::spectrumRenderModeChanged, this,
                               follow("DisplaySpectrumRenderMode")));
    m_pillConns.append(connect(target, &SpectrumWidget::dssFloorDepthChanged, this,
                               follow("Display3DFloor")));
    m_pillConns.append(connect(target, &SpectrumWidget::dssGainChanged, this,
                               follow("Display3DGain")));
    m_pillConns.append(connect(target, &SpectrumWidget::dssRowSpanChanged, this,
                               follow("Display3DSpan")));
    m_pillConns.append(connect(target, &SpectrumWidget::dssAngleChanged, this,
                               follow("Display3DAngle")));
    m_pillConns.append(connect(target, &SpectrumWidget::dssRowDividerChanged, this,
                               follow("Display3DSpeed")));
    m_pillConns.append(connect(target, &SpectrumWidget::threeDSliceDepthChanged, this,
                               follow("Display3DSliceShadow")));
    if (DisplaySettingsModel* model = target->displaySettings()) {
        m_pillConns.append(connect(model, &DisplaySettingsModel::panFillChanged, this,
                                   follow("DisplayPanFill")));
        m_pillConns.append(connect(model, &DisplaySettingsModel::fillAlphaChanged, this,
                                   follow("DisplayFftFillAlpha")));
        m_pillConns.append(connect(model, &DisplaySettingsModel::wfColorSchemeChanged, this,
                                   follow("DisplayWfColorScheme")));
        m_pillConns.append(connect(model, &DisplaySettingsModel::wfColorGainChanged, this,
                                   follow("DisplayWfColorGain")));
        m_pillConns.append(connect(model, &DisplaySettingsModel::wfBlackLevelChanged, this,
                                   follow("DisplayWfBlackLevel")));
    }
}

void SpectrumOverlayPanel::unbindDisplayPill()
{
    for (const QMetaObject::Connection& connection : std::as_const(m_pillConns)) {
        disconnect(connection);
    }
    m_pillConns.clear();
}

void SpectrumOverlayPanel::setDisplayTarget(SpectrumWidget* widget)
{
    if (widget == m_displayTarget) {
        return;
    }
    const bool open = displayPillVisible();
    unbindDisplayPill();
    disconnect(m_displayTargetGone);
    m_displayTarget = widget;
    if (widget) {
        m_displayTargetGone = connect(widget, &QObject::destroyed, this, [this]() {
            m_displayTarget = nullptr;
            m_pillConns.clear();
        });
    }
    if (open) {
        bindDisplayPill();
    } else {
        seedDisplayPill();
    }
}

// A row that cannot act stays visible, greyed, with the reason as its
// tooltip.
void SpectrumOverlayPanel::setPillRowEnabled(const QString& keyBase, bool enabled,
                                             const QString& disabledTip)
{
    const auto it = m_pillRows.constFind(keyBase);
    if (it == m_pillRows.constEnd()) {
        return;
    }
    const QString tip = (enabled || disabledTip.isEmpty()) ? it->tip : disabledTip;
    for (QWidget* widget : it->widgets) {
        widget->setEnabled(enabled);
        widget->setToolTip(tip);
    }
}

void SpectrumOverlayPanel::updatePillEnables()
{
    const SpectrumWidget* target = m_displayTarget;
    const bool hasTarget = target != nullptr;
    if (target) { m_clarityEnabled = target->clarityEnabled(); updateClarityBadge(); }
    for (const QString& keyBase : std::as_const(m_pillRowOrder)) {
        setPillRowEnabled(keyBase, hasTarget);
    }
    if (!hasTarget) {
        updateBandGridNote();
        updatePillOwnIndicators();
        retainPillButtonTextWidths(m_displayFlyout, m_pillTabBtns);
        return;
    }
    setPillRowEnabled(QStringLiteral("DisplayDispNormalize"),
                      normalizeAppliesToDetector(static_cast<int>(target->spectrumDetector())),
                      QStringLiteral("Normalize to 1 Hz needs the Average, Sample or RMS detector."));
    setPillRowEnabled(QStringLiteral("DisplayPeakHoldResetMs"), target->peakHoldEnabled(),
                      QStringLiteral("Turn Peak Hold on to set how long a peak is held."));
    setPillRowEnabled(QStringLiteral("DisplayCalOffset"), !m_remoteWindow,
                      QStringLiteral("The Core calibrates the display for its radio."));
    setPillRowEnabled(QStringLiteral("Display3DSpan"), m_gpuRowSpanSupported,
                      QStringLiteral("This computer's graphics cannot widen the 3D view. "
                                     "It keeps its narrowing shape."));
    setPillRowEnabled(QStringLiteral("Display3DSliceShadow"), m_gpuSliceShadowSupported,
                      QStringLiteral("This computer's graphics cannot draw the slice shadow."));

    const bool view3D = target->spectrumRenderMode() == static_cast<int>(SpectrumRenderMode::Mode3D);
    setPillRowEnabled(QStringLiteral("DisplayFreqGridSpacing"), target->gridEnabled(),
                      QStringLiteral("Grid is off, so no grid lines are drawn."));
    setPillRowEnabled(QStringLiteral("DisplaySpectrumAverageTimeMs"),
                      target->spectrumAveraging() != SpectrumAveraging::None,
                      QStringLiteral("Averaging is None, so there is no time to set."));
    setPillRowEnabled(QStringLiteral("DisplayWaterfallAverageTimeMs"),
                      target->waterfallAveraging() != SpectrumAveraging::None,
                      QStringLiteral("WF Avg is None, so there is no time to set."));
    setPillRowEnabled(QStringLiteral("WaterfallBlankingThresholdDb"), target->waterfallBlankingEnabled(),
                      QStringLiteral("NB Blank is off, so there is no threshold to set."));
    setPillRowEnabled(QStringLiteral("DisplayFftFillAlpha"), !view3D && target->panFillEnabled(),
                      view3D ? QStringLiteral("The 3D view does not draw the fill. Pick 2D Waterfall to use it.")
                             : QStringLiteral("Fill is off."));
    const bool fillUsed = target->panFillEnabled() || target->traceColorFollowsFill();
    setPillRowEnabled(QStringLiteral("DisplayFillColor"), !view3D && fillUsed,
                      view3D ? QStringLiteral("The 3D view does not draw the fill. Pick 2D Waterfall to use it.")
                             : QStringLiteral("Fill is off and Trace Color has its own color, so nothing is drawn in Fill Color."));
    setPillRowEnabled(QStringLiteral("DisplayGradientEnabled"), !view3D && fillUsed,
                      view3D ? QStringLiteral("The 3D view does not draw the gradient. Pick 2D Waterfall to use it.")
                             : QStringLiteral("Fill is off and Trace Color has its own color, so there is nothing for Gradient to color."));
    for (const char* key : {"DisplayLineWidth", "DisplayTraceColor"}) {
        setPillRowEnabled(QString::fromLatin1(key), !view3D,
                          QStringLiteral("The 3D view does not draw the trace. Pick 2D Waterfall to use it."));
    }
    setPillRowEnabled(QStringLiteral("DisplayPanFill"), !view3D,
                      QStringLiteral("The 3D view does not draw the fill. Pick 2D Waterfall to use it."));
    setPillRowEnabled(QStringLiteral("DisplayPeakHoldEnabled"), !view3D,
                      QStringLiteral("The 3D view does not draw Peak Hold. Pick 2D Waterfall to use it."));
    if (view3D) {
        setPillRowEnabled(QStringLiteral("DisplayPeakHoldResetMs"), false,
                          QStringLiteral("The 3D view does not draw Peak Hold. Pick 2D Waterfall to use it."));
    }
    for (const char* key : {"DisplayBackgroundImage", "DisplayBackgroundEnabled"}) {
        setPillRowEnabled(QString::fromLatin1(key), !view3D,
                          QStringLiteral("The 3D view has no background. Pick 2D Waterfall to use it."));
    }
    setPillRowEnabled(QStringLiteral("DisplayBackgroundImageClear"),
                      !view3D && !target->backgroundImagePath().isEmpty(),
                      view3D ? QStringLiteral("The 3D view has no background. Pick 2D Waterfall to use it.")
                             : QStringLiteral("No picture is chosen, so there is nothing to clear."));
    setPillRowEnabled(QStringLiteral("DisplayBackgroundColor"), !view3D && target->backgroundEnabled(),
                      view3D ? QStringLiteral("The 3D view has no background. Pick 2D Waterfall to use it.")
                             : QStringLiteral("Background is off."));
    setPillRowEnabled(QStringLiteral("DisplayBackgroundOpacity"),
                      !view3D && target->backgroundEnabled() && !target->backgroundImagePath().isEmpty(),
                      view3D ? QStringLiteral("The 3D view has no background. Pick 2D Waterfall to use it.")
                      : !target->backgroundEnabled() ? QStringLiteral("Background is off.")
                      : QStringLiteral("BG Opacity fades the picture, and no picture is chosen."));
    if (!view3D) {
        for (const char* key : {"Display3DFloor", "Display3DGain", "Display3DSpan", "Display3DAngle",
                                "Display3DSpeed", "Display3DSliceShadow"}) {
            setPillRowEnabled(QString::fromLatin1(key), false,
                              QStringLiteral("The 2D view has no 3D surface. Pick 3D Stacked Trace to use this row."));
        }
    }

    // The station's rows: every FFT row, and dB Max and dB Min.
    static constexpr const char* kFftRows[] = {
        "DisplayFftSizingMode", "DisplayFftSize", "DisplaySpectrumFps", "DisplayFftWindow",
        "DisplayHzPerBinTarget", "DisplayDecimation"};
    for (const char* keyBase : kFftRows) {
        setPillRowEnabled(QString::fromLatin1(keyBase), m_stationRowsAvailable,
                          m_stationRowsReason);
    }
    // One control sets the running size in each sizing mode (design D38):
    // the other of FFT Size and Hz/bin is grayed and says why. The
    // station's reason, above, wins.
    if (m_stationRowsAvailable) {
        using Mode = ControlRanges::DisplayFftSizingMode;
        const Mode mode = m_fftState.sizingMode;
        if (mode != Mode::Fixed) {
            setPillRowEnabled(QStringLiteral("DisplayFftSize"), false,
                              displayFftSizeHeldTip(mode)
                                  .replace(QStringLiteral("Sizing is Auto"), QStringLiteral("Auto is chosen"))
                                  .replace(QStringLiteral("Sizing is Hz/bin"), QStringLiteral("Hz/bin is chosen")));
        }
        if (mode != Mode::HzPerBin) {
            setPillRowEnabled(QStringLiteral("DisplayHzPerBinTarget"), false,
                              displayHzPerBinHeldTip().replace(QStringLiteral("under Sizing"), QStringLiteral("above")));
        }
    }
    if (m_fftReadout) {
        m_fftReadout->setEnabled(m_stationRowsAvailable);
    }
    const QString oldCore = QStringLiteral("This Core is too old to change Decimation.");
    const bool decimationHeld = m_remoteWindow && !m_coreOffersDecimation;
    if (!m_stationRowsAvailable) {
        setPillRowEnabled(QStringLiteral("DisplayGridMax"), false, m_stationRowsReason);
        setPillRowEnabled(QStringLiteral("DisplayGridMin"), false, m_stationRowsReason);
    } else {
        if (decimationHeld) {
            setPillRowEnabled(QStringLiteral("DisplayDecimation"), false, oldCore);
        }
        // While this pan is keyed it shows the transmit scale; dB Max and
        // dB Min set the receive one. The station's reason, above, wins.
        if (target->displayKeyed()) {
            const QString keyedNote = QString::fromUtf8(kKeyedScaleNote);
            setPillRowEnabled(QStringLiteral("DisplayGridMax"), false, keyedNote);
            setPillRowEnabled(QStringLiteral("DisplayGridMin"), false, keyedNote);
        }
    }
    updateBandGridNote();
    if (m_stationNote) {
        const QString reason = !m_stationRowsAvailable ? m_stationRowsReason
                               : decimationHeld        ? oldCore
                                                       : QString();
        setPillNoteText(m_stationNote, reason);
        // An empty note is no row at all, as the mockup has none; with a
        // reason the row is back and the pill grows by it.
        if (m_stationNote->isHidden() != reason.isEmpty()) {
            m_stationNote->setVisible(!reason.isEmpty());
            // The page's layout is told now, so the pill's new height is
            // known before it is resized.
            if (QLayout* layout = m_stationNote->parentWidget()->layout()) {
                layout->activate();
            }
            if (m_displayFlyout) {
                layoutDisplayPill();
            }
        }
    }

    const int mode = waterfallLevelsMode();
    const bool track = target->adjustGridMinToNoiseFloor();
    const bool keepRange = target->maintainNFAdjustDelta();
    setPillRowEnabled(QStringLiteral("DisplayNFOffsetGridFollow"), track,
        QStringLiteral("Floor Offset is used by Track floor. Pick Track floor to use it."));
    setPillRowEnabled(QStringLiteral("DisplayMaintainNFAdjustDelta"), track,
        QStringLiteral("Keep range is used by Track floor. Pick Track floor to use it."));
    if (m_stationRowsAvailable && !target->displayKeyed()) {
        if (track) {
            setPillRowEnabled(QStringLiteral("DisplayGridMin"), false,
                QStringLiteral("Track floor sets dB Min: the noise floor plus Floor Offset. Pick Manual to set it yourself."));
            if (keepRange) {
                setPillRowEnabled(QStringLiteral("DisplayGridMax"), false,
                    QStringLiteral("Keep range is on, so dB Max moves with dB Min. Turn Keep range off to set it yourself."));
            }
        }
    }
    // Requested mode owns the editors even before the first live level.
    // From Thetis setup.cs:16365 [v2.10.3.15]: udDisplayWaterfallLowLevel.Enabled = !chkRX1WaterfallAGC.Checked;
    static constexpr const char* kNames[] = {"Clarity", "Auto", "Floor", "Manual", "Scale"};
    setPillRowEnabled(QStringLiteral("DisplayWfBlackLevel"), true);
    setPillRowEnabled(QStringLiteral("DisplayWfColorGain"), true);
    setPillRowEnabled(QStringLiteral("WaterfallAGCOffsetDb"), mode == 2,
        QStringLiteral("NF Offset is used by Floor. Pick Floor to use it."));
    setPillRowEnabled(QStringLiteral("DisplayWfLowLevel"), mode == 3,
        mode == 4 ? QStringLiteral("Low is the spectrum's dB Min. Pick Manual to set Low here.")
                  : QStringLiteral("%1 is setting Low. Pick Manual to set it yourself.").arg(QLatin1String(kNames[mode])));
    setPillRowEnabled(QStringLiteral("DisplayWfHighLevel"), mode != 0 && mode != 4,
        mode == 4 ? QStringLiteral("High is the spectrum's dB Max. Pick Manual to set High here.")
                  : QStringLiteral("Clarity is setting High. Pick Auto, Floor or Manual to set it yourself."));
    setPillRowEnabled(QStringLiteral("ClarityRetune"), mode == 0,
        QStringLiteral("Re-tune makes Clarity measure the noise floor again. Pick Clarity to use it."));
    if (target->displayKeyed()) {
        for (const char* key : {"SpectrumScaleMode", "WaterfallLevelsMode", "DisplayNFOffsetGridFollow",
            "DisplayMaintainNFAdjustDelta", "DisplayWfBlackLevel", "DisplayWfColorGain", "WaterfallAGCOffsetDb",
            "DisplayWfLowLevel", "DisplayWfHighLevel", "ClarityRetune"}) {
            setPillRowEnabled(QString::fromLatin1(key), false,
                QStringLiteral("This pan is transmitting. These controls set the receive scale and levels, so they cannot be used until it stops."));
        }
    }
    updateWaterfallNote();
    updatePillModeNotes();
    updatePillOwnIndicators();
    retainPillButtonTextWidths(m_displayFlyout, m_pillTabBtns);
}

// One line under Color Gain says who is setting the waterfall's levels.
void SpectrumOverlayPanel::updateWaterfallNote()
{
    if (!m_waterfallNote || !m_displayTarget) { return; }
    static constexpr const char* kNotes[] = {
        "Clarity sets Low and High from the noise floor.",
        "Auto sets Low from the weakest signals. You set High.",
        "Low is the noise floor plus NF Offset. You set High.",
        "You set Low and High.",
        "Low and High are dB Min and dB Max, above."};
    const int mode = waterfallLevelsMode();
    QString text = QString::fromLatin1(kNotes[mode]);
    if (mode == 4 && m_displayTarget->adjustGridMinToNoiseFloor()) {
        text = QStringLiteral("Low and High are dB Min and dB Max. Track floor moves dB Min.");
    }
    if (m_displayTarget->displayKeyed()) {
        text = QStringLiteral("Transmitting: the waterfall uses the transmit levels. Transmit Display →");
    }
    setPillNoteText(m_waterfallNote, text);
}

void SpectrumOverlayPanel::setBandGrid(const QString& bandLabel, int dbMax, int dbMin)
{
    m_bandGridSet = true;
    m_bandLabel = bandLabel;
    m_bandDbMax = dbMax;
    m_bandDbMin = dbMin;
    seedPillRow(QStringLiteral("DisplayGridMax"));
    seedPillRow(QStringLiteral("DisplayGridMin"));
    updateBandGridNote();
}

// The note under dB Max and dB Min: why they are greyed while the pan is
// keyed, else that they are kept per band. A station that cannot be reached
// has its reason on the rows' tooltips and under the FFT rows.
//
// The keyed sentence is too long for one line of the pill at the note's
// size, so it takes two and the pill is one line taller while keyed.
void SpectrumOverlayPanel::updateBandGridNote()
{
    if (!m_bandGridNote) {
        return;
    }
    const SpectrumWidget* target = m_displayTarget;
    const bool keyed = target && target->displayKeyed() && m_stationRowsAvailable;
    m_bandGridNote->setWordWrap(keyed);
    if (keyed) {
        QFont font = m_bandGridNote->font();
        if (!WorkspacePreview::enabled()) { font.setPixelSize(PillStyle::kNotePx); }
        font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
        m_bandGridNote->setFont(font);
        m_bandGridNote->setText(QString::fromUtf8(kKeyedScaleNote));
    } else if (m_bandLabel.isEmpty()) {
        setPillNoteText(m_bandGridNote, QStringLiteral("dB Max and Min are kept per band."));
    } else {
        setPillNoteText(m_bandGridNote,
                        QStringLiteral("dB Max and Min are kept per band (%1).").arg(m_bandLabel));
    }
    if (WorkspacePreview::enabled()) {
        scheduleDisplayPillGeometryRefresh();
        return;
    }
    const int height = PillStyle::kNoteLineH * (keyed ? 2 : 1);
    if (m_bandGridNote->height() != height
        || m_bandGridNote->minimumHeight() != height) {
        m_bandGridNote->setFixedHeight(height);
        // The page's layout is told now, so the pill's new height is known
        // before it is resized.
        if (QLayout* layout = m_bandGridNote->parentWidget()->layout()) {
            layout->activate();
        }
        if (m_displayFlyout) {
            layoutDisplayPill();
        }
    }
}

void SpectrumOverlayPanel::setGpuCapabilities(bool rowSpanSupported, bool sliceShadowSupported)
{
    m_gpuRowSpanSupported = rowSpanSupported;
    m_gpuSliceShadowSupported = sliceShadowSupported;
    updatePillEnables();
}

void SpectrumOverlayPanel::setRemoteWindow(bool remote, bool coreOffersDecimation)
{
    // MainWindow repeats this on every remote telemetry tick.
    if (m_remoteWindowApplied && remote == m_remoteWindow
        && coreOffersDecimation == m_coreOffersDecimation) {
        return;
    }
    m_remoteWindowApplied = true;
    m_remoteWindow = remote;
    m_coreOffersDecimation = coreOffersDecimation;
    updatePillEnables();
}

// ── Display pill: tabs, showing and hiding ───────────────────────────────────

void SpectrumOverlayPanel::showPillTab(DisplayPillTab tab)
{
    m_pillTab = tab;
    const int index = static_cast<int>(tab);
    {
        // The buttons' own `toggled` handler stands aside (R8).
        const QScopedValueRollback<bool> own(m_showingPillTab, true);
        for (int i = 0; i < m_pillTabBtns.size(); ++i) {
            m_pillTabBtns[i]->setChecked(i == index);
        }
    }
    // One page is laid out at a time, so the pill is as tall as the open
    // tab and no taller.
    for (int i = 0; i < m_pillPages.size(); ++i) {
        if (m_pillPages[i]) {
            m_pillPages[i]->setVisible(i == index);
        }
    }
    if (m_displayFlyout) {
        layoutDisplayPill();
    }
}

void SpectrumOverlayPanel::setDisplayPillTab(DisplayPillTab tab)
{
    showPillTab(tab);
    AppSettings::instance().setValue(QString::fromLatin1(kPillLastTabKey), pillTabName(tab));
}

// The Setup Display page that holds this tab's defaults, by the name the
// Setup tree shows. FFT's sizing, size, window, Hz/bin and frame rate are
// on FFT & Engine, not on Spectrum.
QString SpectrumOverlayPanel::pillSetupPage() const
{
    switch (m_pillTab) {
    case DisplayPillTab::Spectrum:
        return QStringLiteral("Spectrum");
    case DisplayPillTab::Fft:
        return QStringLiteral("FFT & Engine");
    case DisplayPillTab::Levels:
        return QStringLiteral("Levels");
    case DisplayPillTab::Waterfall:
        return QStringLiteral("Waterfall");
    case DisplayPillTab::View3D:
        return QStringLiteral("View");
    }
    return QStringLiteral("Spectrum");
}

bool SpectrumOverlayPanel::displayPillVisible() const
{
    return m_displayFlyout && m_activeFlyout == m_displayFlyout
        && !m_displayFlyout->isHidden();
}

// True for the pill and anything inside it. A combo's open list is its own
// window: it counts only when `throughPopups` is set.
bool SpectrumOverlayPanel::isInDisplayPill(const QObject* obj, bool throughPopups) const
{
    if (!m_displayFlyout) {
        return false;
    }
    for (const QObject* o = obj; o; o = o->parent()) {
        if (o == m_displayFlyout) {
            return true;
        }
        const auto* widget = qobject_cast<const QWidget*>(o);
        if (!throughPopups && widget && widget->isWindow()) {
            return false;
        }
    }
    return false;
}

void SpectrumOverlayPanel::layoutDisplayPill()
{
    if (!m_displayFlyout || !parentWidget()) { return; }
    auto* scroll = m_displayFlyout->findChild<QScrollArea*>(QStringLiteral("displayPillScroll"));
    if (!scroll || !scroll->widget()) { return; }
    QWidget* content = scroll->widget();
    content->layout()->invalidate();
    content->layout()->activate();
    QLayout* column = m_displayFlyout->layout();
    const int tabHeight = column->itemAt(0)->layout()->sizeHint().height();
    content->setFixedHeight(content->layout()->sizeHint().height());
    const int naturalHeight = tabHeight + content->height() + 2;
    const int top = std::clamp(y(), 0, std::max(0, parentWidget()->height() - 1));
    const int availableHeight = std::max(1, parentWidget()->height() - top);
    const bool scrolling = naturalHeight > availableHeight;
    // Reserve the scrollbar outside the original page width. Every approved
    // control retains its normal width and font when the host gets shorter.
    const int scrollWidth = scrolling
        ? scroll->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, scroll) : 0;
    m_displayFlyout->setFixedSize(PillStyle::kWidth + scrollWidth,
                                std::min(naturalHeight, availableHeight));
    const int left = std::clamp(x() + width(), 0,
        std::max(0, parentWidget()->width() - m_displayFlyout->width()));
    m_displayFlyout->move(left, top);
    column->activate();
}

void SpectrumOverlayPanel::showDisplayPill()
{
    // Button index 4 (Display)
    QPushButton* dispBtn = m_menuBtns[4];

    bindDisplayPill();
    themeFlyout(m_displayFlyout);
    m_pillGeometryStale = false;
    refreshDisplayPillGeometry();
    // adjustSize then position (from AetherSDR toggleDisplayPanel)
    // The bounded layout now supplies the size before positioning.
    layoutDisplayPill();
    m_displayFlyout->raise();
    m_displayFlyout->show();
    m_activeFlyout = m_displayFlyout;
    m_activeButton = dispBtn;
    dispBtn->setStyleSheet(menuBtnActiveStyle());
    // A refresh asked for while show() ran (its Show and StyleChange
    // events arrive before the pill counts as open) is queued now, as it
    // was before closed pills stopped queueing.
    if (m_pillGeometryStale) {
        m_pillGeometryStale = false;
        scheduleDisplayPillGeometryRefresh();
    }
}

void SpectrumOverlayPanel::openDisplayPill()
{
    if (displayPillVisible()) {
        return;
    }
    hideFlyout();
    showDisplayPill();
}

void SpectrumOverlayPanel::toggleDisplayFlyout()
{
    if (m_activeFlyout == m_displayFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();
    showDisplayPill();
}

// ── Display pill: the Spectrum tab ───────────────────────────────────────────

void SpectrumOverlayPanel::fitPillLine(PillPage& page, QHBoxLayout* line,
                                        QVector<QPushButton*> tight)
{
    if (WorkspacePreview::enabled()) {
        QWidget* holder = line->parentWidget();
        m_pillToggleGroups.append({holder, tight});
        fitWorkspacePillGroup(holder, tight);
        return;
    }
    // The four are given the most padding at which they share the line:
    // the text is as wide as this computer's font makes it. If they do not
    // fit even at the least, with the pills closer together, Peak Value
    // takes a second line rather than have a label cut off, and the three
    // left are fitted to the first line the same way.
    {
        static constexpr int kRoom =
            PillStyle::kWidth - 2 - 2 * PillStyle::kMarginH;   // less border and margins
        // Gives `pills` the most padding at which they fit the line, then
        // the closer spacing if they still do not. False if nothing fits.
        const auto fitToLine = [line](const QVector<QPushButton*>& pills) {
            const auto widthAt = [&pills](int padding) {
                int width = 0;
                for (QPushButton* pill : pills) {
                    pill->setStyleSheet(PillStyle::toggleWithPadding(padding));
                    width += pill->sizeHint().width();
                }
                return width;
            };
            const int gaps = int(pills.size()) - 1;
            int padding = PillStyle::kTogglePadding;
            while (padding > PillStyle::kTogglePaddingTight
                   && widthAt(padding) + gaps * PillStyle::kSpacing > kRoom) {
                --padding;
            }
            const int width = widthAt(padding);
            if (width + gaps * PillStyle::kSpacing <= kRoom) {
                return true;
            }
            if (width + gaps * PillStyle::kTightSpacing <= kRoom) {
                line->setSpacing(PillStyle::kTightSpacing);
                return true;
            }
            return false;
        };
        QVector<QPushButton*> overflow;
        while (!fitToLine(tight) && tight.size() > 1) {
            QPushButton* last = tight.takeLast();
            line->removeWidget(last);
            last->setStyleSheet(PillStyle::kToggle);
            overflow.prepend(last);
            // The extra line would take the keyed tab past the pill's 480 px
            // (the keyed note is a line taller too), so this tab's rows sit
            // 1 px closer together when it is there.
            page.grid->setVerticalSpacing(PillStyle::kSpacingWrapped);
        }
        if (!overflow.isEmpty()) {
            QHBoxLayout* second = addPillLine(page);
            for (QPushButton* pill : overflow) {
                second->insertWidget(second->count() - 1, pill);
            }
            fitPillLine(page, second, overflow);
        }
    }

}

void SpectrumOverlayPanel::buildPillSpectrumPage()
{
    PillPage page = beginPillPage(DisplayPillTab::Spectrum, QStringLiteral("Trace"));

    const PillFormat percent = [](int v) { return QStringLiteral("%1%").arg(v); };
    const PillFormat ms = [](int v) { return QStringLiteral("%1 ms").arg(v); };
    const PillFormat halves = [](int v) { return QString::number(v * 0.5, 'f', 1); };

    PillChoices detectors;
    for (const ControlRanges::DisplayChoiceItem& item : ControlRanges::kDisplaySpectrumDetectors) {
        detectors.append({QString::fromLatin1(item.label), item.value});
    }
    addPillCombo(page, QStringLiteral("DisplaySpectrumDetector"), QStringLiteral("Detector:"),
                 detectors,
                 QStringLiteral("How each point of the spectrum is taken from the signal"),
                 [](const SpectrumWidget* w) { return static_cast<int>(w->spectrumDetector()); },
                 [](SpectrumWidget* w, int v) {
                     w->setSpectrumDetector(static_cast<SpectrumDetector>(v));
                 });

    QHBoxLayout* line = addPillLine(page);
    QVector<QPushButton*> traceToggles;
    traceToggles << addPillToggle(line, QStringLiteral("DisplayDispNormalize"), QStringLiteral("Normalize to 1 Hz"),
                  QStringLiteral("Show the spectrum level in a 1 Hz reference bandwidth"),
                  [](const SpectrumWidget* w) { return w->dispNormalize(); },
                  [](SpectrumWidget* w, bool on) { w->setDispNormalize(on); });
    traceToggles << addPillToggle(line, QStringLiteral("DisplayPanFill"), QStringLiteral("Fill"),
                  QStringLiteral("Fill the area under the spectrum trace"),
                  [](const SpectrumWidget* w) { return w->panFillEnabled(); },
                  [](SpectrumWidget* w, bool on) { w->setPanFillEnabled(on); });
    traceToggles << addPillToggle(line, QStringLiteral("DisplayGradientEnabled"), QStringLiteral("Gradient"),
                  QStringLiteral("Color the spectrum by signal level"),
                  [](const SpectrumWidget* w) { return w->gradientEnabled(); },
                  [](SpectrumWidget* w, bool on) { w->setGradientEnabled(on); });
    fitPillLine(page, line, traceToggles);

    // Fill: colour swatch + opacity.
    QPushButton* fillSwatch = makePillSwatch(
        QStringLiteral("DisplayFillColor"),
        QStringLiteral("Choose the spectrum fill color"),
        QStringLiteral("Spectrum Fill Color"),
        [](const SpectrumWidget* w) { return w->fillColor(); },
        [](SpectrumWidget* w, const QColor& c) { w->setFillColor(c); });
    addPillSlider(page, QStringLiteral("DisplayFftFillAlpha"), QStringLiteral("Fill Alpha:"),
                  0, 100, 1,
                  QStringLiteral("Opacity of the spectrum fill area below the trace."),
                  percent,
                  [](const SpectrumWidget* w) { return qRound(w->fillAlpha() * 100.0f); },
                  [](SpectrumWidget* w, int v) { w->setFillAlpha(static_cast<float>(v) / 100.0f); },
                  fillSwatch);

    PillChoices averaging;
    for (const ControlRanges::DisplayChoiceItem& item : ControlRanges::kDisplayAveragingModes) {
        averaging.append({QString::fromLatin1(item.label), item.value});
    }
    addPillCombo(page, QStringLiteral("DisplaySpectrumAveraging"), QStringLiteral("Averaging:"),
                 averaging,
                 QStringLiteral("How the spectrum is smoothed over time"),
                 [](const SpectrumWidget* w) { return static_cast<int>(w->spectrumAveraging()); },
                 [](SpectrumWidget* w, int v) {
                     w->setSpectrumAveraging(static_cast<SpectrumAveraging>(v));
                 });

    addPillAvgTimeSlider(page, QStringLiteral("DisplaySpectrumAverageTimeMs"),
                         QStringLiteral("Avg Time:"),
                         QStringLiteral("How long the spectrum averaging looks back"),
                         [](const SpectrumWidget* w) { return w->spectrumAverageTimeMs(); },
                         [](SpectrumWidget* w, int v) { w->setSpectrumAverageTimeMs(v); });

    // Line: trace colour swatch + width.
    // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:1476-1532 [@1e0718ad]
    // (the trace's own colour beside its width).
    QPushButton* traceSwatch = makePillSwatch(
        QStringLiteral("DisplayTraceColor"),
        QStringLiteral("Choose the spectrum trace color"),
        QStringLiteral("Spectrum Trace Color"),
        [](const SpectrumWidget* w) { return w->traceColor(); },
        [](SpectrumWidget* w, const QColor& c) { w->setTraceColor(c); });
    // 0.1 to 5 px in 0.1 px steps: slider positions 1 to 50
    // (ControlRanges::kDisplayLineWidth*, from Thetis udDisplayLineWidth).
    addPillSlider(page, QStringLiteral("DisplayLineWidth"), QStringLiteral("Line Width:"),
                  ControlRanges::kDisplayLineWidthMinTenths,
                  ControlRanges::kDisplayLineWidthMaxTenths, 1,
                  QStringLiteral("Width of the spectrum trace"),
                  [](int v) {
                      return QString::number(ControlRanges::displayLineWidthPx(v), 'f', 1)
                          + QStringLiteral(" px");
                  },
                  [](const SpectrumWidget* w) {
                      return ControlRanges::displayLineWidthTenths(w->lineWidth());
                  },
                  [](SpectrumWidget* w, int v) {
                      w->setLineWidth(static_cast<float>(ControlRanges::displayLineWidthPx(v)));
                  },
                  traceSwatch);

    QPushButton* peakSwitch = makePillToggle(page.widget, QStringLiteral("DisplayPeakHoldEnabled"), QStringLiteral("On"),
                  QStringLiteral("Keep the highest level seen at each frequency on the spectrum"),
                  [](const SpectrumWidget* w) { return w->peakHoldEnabled(); },
                  [](SpectrumWidget* w, bool on) { w->setPeakHoldEnabled(on); }, QStringLiteral("Off"), true);

    static constexpr int kPeakDelayMinMs  = 100;
    static constexpr int kPeakDelayMaxMs  = 10000;
    static constexpr int kPeakDelayStepMs = 100;
    addPillSlider(page, QStringLiteral("DisplayPeakHoldResetMs"), QStringLiteral("Peak Hold:"),
                  kPeakDelayMinMs, kPeakDelayMaxMs, kPeakDelayStepMs,
                  QStringLiteral("How long a held peak stays before it is let go"),
                  ms,
                  [](const SpectrumWidget* w) { return w->peakHoldDelayMs(); },
                  [](SpectrumWidget* w, int v) { w->setPeakHoldDelayMs(v); }, peakSwitch);

    // -30 to 30 dB in 0.5 dB steps: slider positions -60 to 60.
    static constexpr int kCalOffsetHalves = 60;
    addPillSlider(page, QStringLiteral("DisplayCalOffset"), QStringLiteral("Cal Offset:"),
                  -kCalOffsetHalves, kCalOffsetHalves, 1,
                  QStringLiteral("Shift the spectrum's level readings to match a known signal"),
                  [halves](int v) { return halves(v) + QStringLiteral(" dB"); },
                  [](const SpectrumWidget* w) { return qRound(w->dbmCalOffset() * 2.0f); },
                  [](SpectrumWidget* w, int v) { w->setDbmCalOffset(static_cast<float>(v) * 0.5f); });

    addPillHeader(page, QStringLiteral("Grid"), QString(), false);
    QPushButton* gridSwitch = makePillToggle(page.widget, QStringLiteral("DisplayGridEnabled"), QStringLiteral("On"),
                  QStringLiteral("Show or hide frequency and dB grid lines"),
                  [](const SpectrumWidget* w) { return w->gridEnabled(); },
                  [](SpectrumWidget* w, bool on) { w->setGridEnabled(on); }, QStringLiteral("Off"), true);
    // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:1820-1836 [@1e0718ad]
    // (Auto, then 1, 2, 5, 10, 25, 50 and 100 kHz).
    PillChoices spacings;
    spacings.append({QStringLiteral("Auto"), 0});
    for (const int hz : SpectrumWidget::kFreqGridSpacingsHz) {
        spacings.append({QStringLiteral("%1 kHz").arg(hz / 1000), hz});
    }
    addPillCombo(page, QStringLiteral("DisplayFreqGridSpacing"), QStringLiteral("Grid:"),
                 spacings,
                 QStringLiteral("Spacing of the frequency grid lines. "
                                "Auto follows the width shown."),
                 [](const SpectrumWidget* w) { return w->freqGridSpacingHz(); },
                 [](SpectrumWidget* w, int v) { w->setFreqGridSpacingHz(v); }, gridSwitch);

    // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:1840-1856 [@1e0718ad]
    // NereusSDR, based on AetherSDR: pixels, every size from 8 to 14, so
    // any size the pan can hold is one the combo can show.
    static constexpr int kScaleTextMinPx = 8;
    static constexpr int kScaleTextMaxPx = 14;
    PillChoices textSizes;
    for (int px = kScaleTextMinPx; px <= kScaleTextMaxPx; ++px) {
        textSizes.append({QStringLiteral("%1 px").arg(px), px});
    }
    addPillCombo(page, QStringLiteral("DisplayFreqScaleFontPx"), QStringLiteral("Scale Text:"),
                 textSizes,
                 QStringLiteral("Size of the frequency scale's text"),
                 [](const SpectrumWidget* w) { return w->freqScaleFontPx(); },
                 [](SpectrumWidget* w, int v) { w->setFreqScaleFontPx(v); });

    addPillHeader(page, QStringLiteral("Markers"), QString(), false);
    line = addPillLine(page);
    QVector<QPushButton*> tight;
    tight << addPillToggle(line, QStringLiteral("DisplayShowNoiseFloor"), QStringLiteral("Noise Floor"),
                  QStringLiteral("Show the measured noise floor on the spectrum. "
                                 "Every panadapter follows."),
                  [](const SpectrumWidget* w) { return w->showNoiseFloor(); },
                  [](SpectrumWidget* w, bool on) { w->setShowNoiseFloor(on); });
    tight << addPillToggle(line, QStringLiteral("DisplayShowBinWidth"), QStringLiteral("Bin Width"),
                  QStringLiteral("Show how wide each spectrum point is. "
                                 "Every panadapter follows."),
                  [](const SpectrumWidget* w) { return w->showBinWidth(); },
                  [](SpectrumWidget* w, bool on) { w->setShowBinWidth(on); });
    QPushButton* peakValue = addPillToggle(
        line, QStringLiteral("DisplayShowPeakValueOverlay"), QStringLiteral("Peak Value"),
                  QStringLiteral("Show the strongest signal's level and frequency. "
                                 "Every panadapter follows."),
                  [](const SpectrumWidget* w) { return w->showPeakValueOverlay(); },
                  [](SpectrumWidget* w, bool on) { w->setShowPeakValueOverlay(on); });
    tight << addPillToggle(line, QStringLiteral("DisplayShowCursorFreq"), QStringLiteral("Cursor Freq"),
                  QStringLiteral("Show frequency at mouse cursor position"),
                  [](const SpectrumWidget* w) { return w->cursorFreqVisible(); },
                  [](SpectrumWidget* w, bool on) { w->setCursorFreqVisible(on); });
    // Keep the visible order above. Peak Value is the preferred overflow,
    // so it is last only in the fitting order that removes from the end.
    tight << peakValue;
    fitPillLine(page, line, tight);
    QLabel* markersNote = addPillNote(page, QStringLiteral(
        "In Setup, Cursor Freq is on Spectrum; the other three and their details are on "
        "<a href=\"peaks\">Peaks &amp; Markers →</a>"), 3);
    markersNote->setObjectName(QStringLiteral("pillSpectrumMarkersNote"));
    markersNote->setTextFormat(Qt::RichText);
    markersNote->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    connect(markersNote, &QLabel::linkActivated, this, [this]() {
        emit openSetupRequested(QStringLiteral("Spectrum Peaks"));
    });

    endPillPage(page);
}

// Approved live mockup: both receive-level jobs share the Levels tab.
int SpectrumOverlayPanel::waterfallLevelsMode() const
{
    const SpectrumWidget* widget = m_displayTarget;
    if (!widget) { return 0; }
    if (widget->clarityEnabled()) { return 0; }
    if (widget->wfUseSpectrumMinMax()) { return 4; }
    if (widget->wfAgcEnabled()) { return widget->waterfallNFAGCEnabled() ? 2 : 1; }
    return 3;
}

void SpectrumOverlayPanel::addPillChoices(PillPage& page, const QString& key,
    const QString& prefix, const PillChoices& choices, const QStringList& suffixes,
    const QString& tip, PillIntGet get, PillIntSet set)
{
    auto* holder = new QWidget(page.widget);
    holder->setObjectName(QStringLiteral("pill") + key);
    holder->setFixedHeight(PillStyle::kControlH);
    holder->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto* line = new QHBoxLayout(holder);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(PillStyle::kSegmentSpacing);
    QVector<QPushButton*> buttons;
    QVector<QWidget*> widgets;
    for (int index = 0; index < choices.size(); ++index) {
        const auto& choice = choices[index];
        auto* button = new QPushButton(choice.first, holder);
        button->setObjectName(prefix + suffixes[index]);
        button->setCheckable(true);
        button->setFixedHeight(PillStyle::kControlH);
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        button->setFocusPolicy(Qt::NoFocus);
        button->setStyleSheet(PillStyle::kSegment);
        button->ensurePolished();
        button->setMinimumWidth(button->fontMetrics().horizontalAdvance(button->text()) + 2);
        const int value = choice.second;
        connect(button, &QPushButton::toggled, this, [this, key, value, get, set](bool checked) {
            if (m_applyingPillChoice || !m_displayTarget) { return; }
            if (!checked || get(m_displayTarget) == value) {
                seedPillRow(key);
                return;
            }
            {
                const QScopedValueRollback<bool> own(m_applyingPillChoice, true);
                set(m_displayTarget, value);
            }
            seedPillRow(key);
            updatePillEnables();
        });
        buttons.append(button);
        widgets.append(button);
        line->addWidget(button, 1);
    }
    page.grid->addWidget(holder, page.row++, 0, 1, 4);
    registerPillRow(key, widgets, tip, [this, buttons, choices, get]() {
        if (!m_displayTarget) { return; }
        const int selected = get(m_displayTarget);
        for (int index = 0; index < buttons.size(); ++index) {
            const QSignalBlocker block(buttons[index]);
            buttons[index]->setChecked(choices[index].second == selected);
        }
    });
}

void SpectrumOverlayPanel::updatePillModeNotes()
{
    const SpectrumWidget* widget = m_displayTarget;
    if (!widget) { return; }
    const int mode = waterfallLevelsMode();
    const bool track = widget->adjustGridMinToNoiseFloor();
    if (m_spectrumScaleNote) {
        QString text = track ? QStringLiteral("dB Min follows the noise floor plus Floor Offset.")
                             : QStringLiteral("You set dB Max and dB Min.");
        if (mode == 4) { text += QStringLiteral(" The waterfall follows this scale."); }
        if (widget->displayKeyed()) {
            text = QStringLiteral("Transmitting: this pan is drawn on the transmit scale. Transmit Display →");
        }
        setPillNoteText(m_spectrumScaleNote, text);
    }
    if (m_viewNote) {
        setPillNoteText(m_viewNote, widget->spectrumRenderMode() == static_cast<int>(SpectrumRenderMode::Mode3D)
            ? QStringLiteral("The 3D rows are in use. The background, the trace and its fill are not drawn in 3D.")
            : QStringLiteral("The Background rows are used by this view. The 3D rows are used by the 3D view."));
    }
    if (m_fftSizingNote) {
        QString text;
        switch (m_fftState.sizingMode) {
        case ControlRanges::DisplayFftSizingMode::Auto: text = QStringLiteral("The zoom picks the FFT size."); break;
        case ControlRanges::DisplayFftSizingMode::HzPerBin: text = QStringLiteral("The bin width you ask for picks the FFT size."); break;
        case ControlRanges::DisplayFftSizingMode::Fixed: text = QStringLiteral("You pick the FFT size."); break;
        }
        setPillNoteText(m_fftSizingNote, text);
    }
    updateWaterfallReadouts();
    if (!WorkspacePreview::enabled() && m_waterfallNote && m_clarityRetuneBtn) {
        m_waterfallNote->ensurePolished();
        m_clarityRetuneBtn->ensurePolished();
        const int room = PillStyle::kWidth - 2 - 2 * PillStyle::kMarginH
            - m_clarityBadge->width() - m_clarityRetuneBtn->sizeHint().width() - 2 * PillStyle::kSpacing;
        // A hidden Levels page has not necessarily laid out its children.
        // Give the paired note the same real width used for its wrap measure,
        // so a mode/status change cannot leave a stale narrow label rectangle.
        m_waterfallNote->setFixedWidth(room);
        const int height = std::max(2 * PillStyle::kNoteLineH,
                                   pillTextHeight(m_waterfallNote, m_waterfallNote->text(), room));
        m_waterfallNote->setFixedHeight(height);
        m_waterfallNote->parentWidget()->setFixedHeight(height);
        for (QWidget* page : std::as_const(m_pillPages)) {
            if (page && page->layout()) { page->layout()->activate(); }
        }
        layoutDisplayPill();
    }
    scheduleDisplayPillGeometryRefresh();
}

void SpectrumOverlayPanel::updateWaterfallReadouts()
{
    if (!m_displayTarget) { return; }
    const SpectrumWidget* widget = m_displayTarget;
    const int mode = waterfallLevelsMode();
    const bool runKnown = widget->coreWaterfallLevelsInUse()
        ? widget->coreWaterfallLevels().has_value() : widget->waterfallAgcRunLevel().has_value();
    const bool known = mode == 3 || (mode == 0 && widget->clarityActive())
        || ((mode == 1 || mode == 2) && runKnown)
        || (mode == 4 && !widget->adjustGridMinToNoiseFloor());
    const auto effective = widget->effectiveWaterfallLevels();
    for (const bool low : {true, false}) {
        const QString key = low ? QStringLiteral("DisplayWfLowLevel") : QStringLiteral("DisplayWfHighLevel");
        QSlider* slider = m_displayFlyout->findChild<QSlider*>(QStringLiteral("pill") + key);
        QLabel* value = m_displayFlyout->findChild<QLabel*>(QStringLiteral("pill") + key + QStringLiteral("Value"));
        if (!slider || !value) { continue; }
        const bool measured = low ? mode != 3 : mode == 0 || mode == 4;
        if (measured && !known) {
            value->setText(QStringLiteral("no signal"));
        } else {
            const float dbm = measured ? (low ? effective.first : effective.second)
                                       : (low ? widget->wfLowThreshold() : widget->wfHighThreshold());
            const QSignalBlocker block(slider);
            slider->setValue(qRound(dbm));
            value->setText(QStringLiteral("%1 dBm").arg(qRound(dbm)));
        }
    }
}

void SpectrumOverlayPanel::buildPillLevelsPage()
{
    PillPage page = beginPillPage(DisplayPillTab::Levels, QStringLiteral("Spectrum scale"));
    const PillFormat dBm = [](int value) { return QStringLiteral("%1 dBm").arg(value); };
    addPillChoices(page, QStringLiteral("SpectrumScaleMode"), QStringLiteral("pillSpectrumScale"),
        {{QStringLiteral("Manual"), 0}, {QStringLiteral("Track floor"), 1}},
        {QStringLiteral("Manual"), QStringLiteral("Track")},
        QStringLiteral("Choose who sets the receive spectrum scale"),
        [](const SpectrumWidget* widget) { return widget->adjustGridMinToNoiseFloor() ? 1 : 0; },
        [](SpectrumWidget* widget, int mode) { widget->setAdjustGridMinToNoiseFloor(mode == 1); });
    m_spectrumScaleNote = addPillNote(page, QString(), 2);
    m_spectrumScaleNote->setObjectName(QStringLiteral("pillSpectrumScaleNote"));

    // dB Max and dB Min are the band's stored values (setBandGrid), not the
    // pan's live range, which the noise floor may be moving. Moving them
    // asks; the pill writes nothing.
    addPillSlider(page, QStringLiteral("DisplayGridMax"), QStringLiteral("dB Max:"),
                  -200, 0, 1,
                  QStringLiteral("Top of the spectrum's level scale on this band"),
                  dBm,
                  [this](const SpectrumWidget* w) {
                      return m_bandGridSet ? m_bandDbMax : qRound(w->refLevel());
                  },
                  [this](SpectrumWidget*, int v) {
                      m_bandDbMax = v;
                      emit perBandDbMaxRequested(v);
                  });
    addPillSlider(page, QStringLiteral("DisplayGridMin"), QStringLiteral("dB Min:"),
                  -200, 0, 1,
                  QStringLiteral("Bottom of the spectrum's level scale on this band"),
                  dBm,
                  [this](const SpectrumWidget* w) {
                      return m_bandGridSet ? m_bandDbMin
                                           : qRound(w->refLevel() - w->dynamicRange());
                  },
                  [this](SpectrumWidget*, int v) {
                      m_bandDbMin = v;
                      emit perBandDbMinRequested(v);
                  });
    static constexpr int kFloorOffsetMinDb = -60;
    static constexpr int kFloorOffsetMaxDb = 60;
    addPillSlider(page, QStringLiteral("DisplayNFOffsetGridFollow"), QStringLiteral("Floor Offset:"),
        kFloorOffsetMinDb, kFloorOffsetMaxDb, 1, QStringLiteral("Offset added to the noise floor for Track floor"),
        [](int value) { return QStringLiteral("%1 dB").arg(value); },
        [](const SpectrumWidget* widget) { return widget->nfOffsetGridFollow(); },
        [](SpectrumWidget* widget, int value) { widget->setNFOffsetGridFollow(value); });
    QHBoxLayout* keep = addPillLine(page);
    addPillToggle(keep, QStringLiteral("DisplayMaintainNFAdjustDelta"), QStringLiteral("Keep range"),
        QStringLiteral("Move dB Max with dB Min to keep the spectrum range"),
        [](const SpectrumWidget* widget) { return widget->maintainNFAdjustDelta(); },
        [](SpectrumWidget* widget, bool enabled) { widget->setMaintainNFAdjustDelta(enabled); });
    m_bandGridNote = addPillNote(page, QStringLiteral("dB Max and Min are kept per band."));
    m_bandGridNote->setObjectName(QStringLiteral("pillBandGridNote"));
    m_bandGridNote->setStyleSheet(PillStyle::kNoteFitted);

    addPillHeader(page, QStringLiteral("Waterfall levels"), QString(), false);
    // The readable workspace font makes this page two pixels taller than
    // its 480px envelope. Recover them from the section's top gap, keeping
    // every control, note and reason at its measured intrinsic height.
    page.grid->itemAtPosition(page.row - 1, 0)->widget()->setProperty("pillLevelsSectionGap", true);
    // Phase 3G-9c: Clarity adaptive tuning — status badge + Re-tune button
    // NOTE: Heat Map / Noise Floor / Weighted Avg toggles were removed (B8
    // Task 23). They were pure label-update theatre with no signal/model state.
    // Noise floor estimation uses NoiseFloorEstimator + ClarityController (the
    // canonical path). Heat Map gradient control is in Setup → Display → Gradient.
    // The approved chooser supersedes the old Clarity/AGC/NF-AGC/link toggles.
    addPillChoices(page, QStringLiteral("WaterfallLevelsMode"), QStringLiteral("pillWaterfallLevels"),
        {{QStringLiteral("Clarity"), 0}, {QStringLiteral("Auto"), 1},
         {QStringLiteral("Floor"), 2}, {QStringLiteral("Manual"), 3}, {QStringLiteral("Scale"), 4}},
        {QStringLiteral("Clarity"), QStringLiteral("Auto"), QStringLiteral("Floor"),
         QStringLiteral("Manual"), QStringLiteral("Scale")},
        QStringLiteral("Choose who sets the receive waterfall levels"),
        [this](const SpectrumWidget*) { return waterfallLevelsMode(); },
        [](SpectrumWidget* widget, int mode) {
            widget->setClarityEnabled(mode == 0);
            if (mode == 0) { return; } // Preserve inactive fallback flags in Clarity.
            widget->setWfAgcEnabled(mode == 1 || mode == 2);
            widget->setWaterfallNFAGCEnabled(mode == 2);
            widget->setWfUseSpectrumMinMax(mode == 4);
        });
    // The accepted note/C/Re-tune row shares its height, leaving the
    // controls below it room without shortening the explanation.
    QHBoxLayout* status = addPillLine(page);
    status->parentWidget()->setProperty("pillModeStatus", true);
    status->parentWidget()->setFixedHeight(2 * PillStyle::kNoteLineH);
    m_waterfallNote = new QLabel(status->parentWidget());
    m_waterfallNote->setObjectName(QStringLiteral("pillWaterfallNote"));
    m_waterfallNote->setProperty("pillNoteLines", 2);
    m_waterfallNote->setStyleSheet(PillStyle::kNote);
    m_waterfallNote->setWordWrap(true);
    m_waterfallNote->setFixedHeight(2 * PillStyle::kNoteLineH);
    m_waterfallNote->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    status->insertWidget(0, m_waterfallNote, 1);
    // Runtime status remains beside one Re-tune action, not a mode input.
    m_clarityBadge = new QLabel(QStringLiteral("C"), status->parentWidget());
    m_clarityBadge->setObjectName(QStringLiteral("pillClarityBadge"));
    m_clarityBadge->setFixedSize(PillStyle::kControlH, PillStyle::kControlH);
    m_clarityBadge->setAlignment(Qt::AlignCenter);
    m_clarityBadge->setToolTip(QStringLiteral("Clarity is active while this pan has a measured level"));
    status->insertWidget(1, m_clarityBadge);
    m_clarityRetuneBtn = makePillButton(QStringLiteral("Re-tune"),
        QStringLiteral("Force Clarity to re-estimate the noise floor now"));
    m_clarityRetuneBtn->setObjectName(QStringLiteral("pillClarityRetune"));
    status->insertWidget(2, m_clarityRetuneBtn);
    // Unlike toggle lines this note owns all room left by C and Re-tune.
    // The generic line's trailing stretch would take half of that room.
    { const std::unique_ptr<QLayoutItem> spacer(status->takeAt(status->count() - 1)); }
    connect(m_clarityRetuneBtn, &QPushButton::clicked, this, &SpectrumOverlayPanel::clarityRetuneRequested);
    registerPillRow(QStringLiteral("ClarityRetune"), {m_clarityRetuneBtn}, m_clarityRetuneBtn->toolTip(), {});
    updateClarityBadge();

    addPillSlider(page, QStringLiteral("DisplayWfBlackLevel"), QStringLiteral("Black Level:"),
                  ClarityLevels::kBlackLevelMin, ClarityLevels::kBlackLevelMax, 1,
                  QStringLiteral("Where black sits against the noise floor. Up is blacker: "
                                 "more of the noise goes black. N is the noise floor."),
                  [](int v) {
                      // Whole dB above or below the noise; a half dB reads down.
                      const int offsetDb = static_cast<int>(std::floor(
                          double(v - ClarityLevels::kBlackLevelAtNoise)
                          * double(ClarityLevels::kBlackLevelDbPerStep)));
                      return QStringLiteral("N%1%2 dB")
                          .arg(offsetDb < 0 ? QLatin1Char('-') : QLatin1Char('+'))
                          .arg(std::abs(offsetDb));
                  },
                  [](const SpectrumWidget* w) { return w->wfBlackLevel(); },
                  [](SpectrumWidget* w, int v) { w->setWfBlackLevel(v); });
    addPillSlider(page, QStringLiteral("DisplayWfColorGain"), QStringLiteral("Color Gain:"),
                  ClarityLevels::kColorGainMin, ClarityLevels::kColorGainMax, 1,
                  QStringLiteral("How many dB the colors cover, from black to the brightest. "
                                 "Up covers fewer dB, so weak signals stand out more."),
                  [](int v) {
                      return QStringLiteral("%1 dB").arg(qRound(ClarityLevels::colorSpanDb(v)));
                  },
                  [](const SpectrumWidget* w) { return w->wfColorGain(); },
                  [](SpectrumWidget* w, int v) { w->setWfColorGain(v); });

    // Rulings wave R6: whole dB, 1 dB a step, the range of Setup's box
    // (-60 to 60). It was 0.1 dB steps.
    addPillSlider(
            page, QStringLiteral("WaterfallAGCOffsetDb"), QStringLiteral("NF Offset:"),
            WaterfallLevelFollower::kNoiseFloorAgcOffsetMinDb,
            WaterfallLevelFollower::kNoiseFloorAgcOffsetMaxDb, 1,
            QStringLiteral("How far above or below the noise floor NF-AGC holds Low"),
            [](int db) { return QStringLiteral("%1 dB").arg(db); },
            [](const SpectrumWidget* w) { return w->waterfallAGCOffsetDb(); },
            [](SpectrumWidget* w, int v) { w->setWaterfallAGCOffsetDb(v); });
    static constexpr int kLevelMinDbm = -200;
    static constexpr int kLevelMaxDbm = 0;
    addPillSlider(page, QStringLiteral("DisplayWfLowLevel"), QStringLiteral("Low:"),
                  kLevelMinDbm, kLevelMaxDbm, 1,
                  QStringLiteral("The level drawn as the darkest color"),
                  dBm,
                  [](const SpectrumWidget* w) { return qRound(w->wfLowThreshold()); },
                  [](SpectrumWidget* w, int v) { w->setWfLowThreshold(float(v)); });
    addPillSlider(page, QStringLiteral("DisplayWfHighLevel"), QStringLiteral("High:"),
                  kLevelMinDbm, kLevelMaxDbm, 1,
                  QStringLiteral("The level drawn as the brightest color"),
                  dBm,
                  [](const SpectrumWidget* w) { return qRound(w->wfHighThreshold()); },
                  [](SpectrumWidget* w, int v) { w->setWfHighThreshold(float(v)); });

    endPillPage(page);
}

// ── Display pill: the FFT tab ────────────────────────────────────────────────
// The station's FFT settings, shared by every pan and window on the Core
// (the header says ALL PANS). The rows show what setFftState gives and ask
// for changes by signal; the pill writes no setting itself.

void SpectrumOverlayPanel::buildPillFftPage()
{
    PillPage page = beginPillPage(DisplayPillTab::Fft, QStringLiteral("FFT"),
                                  QStringLiteral("CORE"));

    m_fftState.baselineSize = ControlRanges::kDisplayFftSizeDefault;
    m_fftState.fps = ControlRanges::kDisplaySpectrumFpsDefault;
    m_fftState.windowType = ControlRanges::kDisplayFftWindowDefault;
    m_fftState.hzPerBinTarget = ControlRanges::kDisplayHzPerBinTargetDefault;
    m_fftState.decimation = ControlRanges::kDisplayDecimationDefault;

    // A station row is built as a pan row with no getter or setter, then
    // given a seed that reads m_fftState.
    const auto reseed = [this](const char* keyBase, std::function<void()> seed) {
        const QString key = QString::fromLatin1(keyBase);
        const PillRow row = m_pillRows.value(key);
        registerPillRow(key, row.widgets, row.tip, std::move(seed));
    };
    const PillFormat number = [](int v) { return QString::number(v); };

    // Sizing: which control sets the running FFT size (design D38). Three
    // exclusive buttons across the slider and value columns, kept exclusive
    // the tab strip's way (rulings wave R8): `toggled` hears every
    // activation, exactly one is lit, and the lit one cannot be unlit. A
    // choice is a request; the row then shows what setFftState gives.
    {
        auto* holder = new QWidget(page.widget);
        holder->setFixedHeight(PillStyle::kControlH);
        // The buttons take the two columns' width and never widen the pill.
        holder->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* line = new QHBoxLayout(holder);
        line->setContentsMargins(0, 0, 0, 0);
        line->setSpacing(PillStyle::kSegmentSpacing);

        QVector<QWidget*> widgets;
        for (const DisplayFftSizingChoice& choice : kDisplayFftSizingChoices) {
            auto* btn = new QPushButton(QString::fromLatin1(choice.label), holder);
            btn->setObjectName(QStringLiteral("pillDisplayFftSizingMode")
                               + QString::fromLatin1(
                                     ControlRanges::displayFftSizingModeToken(choice.mode)));
            btn->setCheckable(true);
            btn->setFixedHeight(PillStyle::kControlH);
            btn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            btn->setFocusPolicy(Qt::NoFocus);
            btn->setStyleSheet(PillStyle::kSegment);
            const ControlRanges::DisplayFftSizingMode mode = choice.mode;
            connect(btn, &QPushButton::toggled, this, [this, mode, btn](bool checked) {
                if (m_showingFftSizingMode) {
                    return;
                }
                if (!checked) {
                    const QScopedValueRollback<bool> own(m_showingFftSizingMode, true);
                    btn->setChecked(true);
                    return;
                }
                showFftSizingMode(mode);
                // With no target remembered, Hz/bin starts from the bin
                // width running now, rounded up to the control's step, so
                // choosing it does not change the running size.
                const double running = runningBinWidthHz();
                emit fftSizingModeRequested(
                    mode,
                    running > 0.0 ? ControlRanges::hzPerBinTargetForBinWidth(running) : 0.0);
            });
            line->addWidget(btn, 1);
            m_fftSizingBtns.append(btn);
            widgets.append(btn);
        }
        page.grid->addWidget(holder, page.row, 0, 1, 4);
        ++page.row;
        registerPillRow(QStringLiteral("DisplayFftSizingMode"), widgets, displayFftSizingTip(),
                        [this]() { showFftSizingMode(m_fftState.sizingMode); });
        showFftSizingMode(m_fftState.sizingMode);
    }
    m_fftSizingNote = addPillNote(page, QString(), 1);
    m_fftSizingNote->setObjectName(QStringLiteral("pillFftSizingNote"));

    // FFT Size: one request per drag (on release) and one per run of
    // keyboard steps, since each new size restarts every pan's FFT. It sets
    // the size in the Fixed sizing mode only; grayed, it shows the size
    // this pan's FFT runs at.
    {
        const PillFormat size = [](int position) {
            return QString::number(ControlRanges::displayFftSizeAt(position));
        };
        m_fftSizeSlider = addPillSlider(
            page, QStringLiteral("DisplayFftSize"), QStringLiteral("FFT Size:"),
            0, ControlRanges::kDisplayFftSizePositionMax, 1,
            QStringLiteral("How many points each transform has. Larger is finer in "
                           "frequency and slower to follow change. Applies when the "
                           "slider is let go."),
            size, nullptr, nullptr);
        m_fftSizeSlider->setTracking(false);
        QLabel* value = page.widget->findChild<QLabel*>(QStringLiteral("pillDisplayFftSizeValue"));

        m_fftSizeKeyTimer = new QTimer(this);
        m_fftSizeKeyTimer->setObjectName(QStringLiteral("pillFftSizeKeyTimer"));
        m_fftSizeKeyTimer->setSingleShot(true);
        m_fftSizeKeyTimer->setInterval(kFftSizeKeyDelayMs);
        connect(m_fftSizeKeyTimer, &QTimer::timeout, this, [this]() {
            if (m_fftState.sizingMode != ControlRanges::DisplayFftSizingMode::Fixed) {
                return;
            }
            const int size = ControlRanges::displayFftSizeAt(m_fftSizeSlider->value());
            if (size != m_fftState.baselineSize) {
                emit fftBaselineRequested(size);
            }
        });

        // The value follows the handle while it is dragged.
        connect(m_fftSizeSlider, &QSlider::sliderMoved, this, [value, size](int position) {
            value->setText(size(position));
        });
        connect(m_fftSizeSlider, &QSlider::sliderReleased, this, [this]() {
            m_fftSizeReleasePending =
                m_fftSizeSlider->sliderPosition() != m_fftSizeSlider->value();
        });
        connect(m_fftSizeSlider, &QSlider::valueChanged, this,
                &SpectrumOverlayPanel::fftSizeSliderChanged);
        reseed("DisplayFftSize", [this, value, size]() {
            const bool fixed =
                m_fftState.sizingMode == ControlRanges::DisplayFftSizingMode::Fixed;
            // A handle being dragged is the operator's until it is let go.
            if (fixed && m_fftSizeSlider->isSliderDown()) {
                return;
            }
            const QSignalBlocker block(m_fftSizeSlider);
            if (!fixed) {
                // The slider no longer sets the size: a drag or a run of
                // keyboard steps under way when the mode changed ends here
                // and asks nothing.
                m_fftSizeSlider->setSliderDown(false);
                m_fftSizeReleasePending = false;
                m_fftSizeKeyTimer->stop();
            }
            if (!fixed && m_fftState.runningSize > 0) {
                // Grayed: the size that runs, which the zoom or the Hz/bin
                // target chose. The handle goes to the nearest position.
                m_fftSizeSlider->setValue(
                    ControlRanges::displayFftSizePosition(m_fftState.runningSize));
                value->setText(QString::number(m_fftState.runningSize));
                return;
            }
            m_fftSizeSlider->setValue(
                ControlRanges::displayFftSizePosition(m_fftState.baselineSize));
            value->setText(size(m_fftSizeSlider->value()));
        });
    }

    // Hz/bin: the bin width to hold. It sets the size in the Hz/bin sizing
    // mode only; grayed, it shows the remembered target, or with none the
    // bin width running now on the slider's step. The label carries "/bin",
    // so the value reads "12.5 Hz": the slider moves in 0.5 Hz steps, one
    // decimal says all of it, and "200.0 Hz" fits the value column, so the
    // slider is as wide as FFT Size's.
    {
        constexpr int kHzPerBinShownDecimals = 1;
        const PillFormat target = [](int steps) {
            return QStringLiteral("%1 Hz")
                .arg(double(steps) / kHzPerBinStepsPerHz, 0, 'f', kHzPerBinShownDecimals);
        };
        // The slider's lowest position is one step: the mode, not a value
        // of 0, turns the target off.
        QSlider* slider = addPillSlider(
            page, QStringLiteral("DisplayHzPerBinTarget"), QStringLiteral("Hz/bin:"),
            qRound(ControlRanges::kDisplayHzPerBinTargetStep * kHzPerBinStepsPerHz),
            int(ControlRanges::kDisplayHzPerBinTargetMax * kHzPerBinStepsPerHz), 1,
            displayHzPerBinTip(), target, nullptr, nullptr);
        QLabel* value =
            page.widget->findChild<QLabel*>(QStringLiteral("pillDisplayHzPerBinTargetValue"));

        connect(slider, &QSlider::valueChanged, this, [this](int steps) {
            if (m_fftState.sizingMode != ControlRanges::DisplayFftSizingMode::HzPerBin) {
                // Grayed, it sets nothing: back to what it shows.
                seedPillRow(QStringLiteral("DisplayHzPerBinTarget"));
                return;
            }
            emit hzPerBinTargetRequested(double(steps) / kHzPerBinStepsPerHz);
        });
        reseed("DisplayHzPerBinTarget", [this, slider, value, target]() {
            const bool inCharge =
                m_fftState.sizingMode == ControlRanges::DisplayFftSizingMode::HzPerBin;
            const double shown = inCharge
                ? m_fftState.hzPerBinTarget
                : displayHzPerBinShownWhileHeld(m_fftState.lastHzPerBinTarget,
                                                runningBinWidthHz());
            const QSignalBlocker block(slider);
            slider->setValue(qRound(shown * kHzPerBinStepsPerHz));
            value->setText(target(slider->value()));
        });
    }

    // The running numbers go under the pair of controls that can set the
    // size, so the three sizing rows sit together (Sizing, FFT Size, Hz/bin).
    m_fftReadout = addPillNote(page, QString(), 1);
    m_fftReadout->setObjectName(QStringLiteral("pillFftReadout"));
    m_fftReadout->setStyleSheet(PillStyle::kReadout);

    {
        QSlider* fps = addPillSlider(
            page, QStringLiteral("DisplaySpectrumFps"), QStringLiteral("FPS:"),
            ControlRanges::kDisplaySpectrumFpsMin, ControlRanges::kDisplaySpectrumFpsMax,
            ControlRanges::kDisplaySpectrumFpsStep,
            QStringLiteral("How many spectrum frames are drawn each second"),
            number, nullptr, nullptr);
        QLabel* value =
            page.widget->findChild<QLabel*>(QStringLiteral("pillDisplaySpectrumFpsValue"));
        connect(fps, &QSlider::valueChanged, this, [this](int v) { emit fftFpsRequested(v); });
        reseed("DisplaySpectrumFps", [this, fps, value]() {
            const QSignalBlocker block(fps);
            fps->setValue(m_fftState.fps);
            value->setText(QString::number(fps->value()));
        });
    }

    {
        PillChoices windows;
        for (const ControlRanges::DisplayChoiceItem& item : ControlRanges::kDisplayFftWindows) {
            windows.append({QString::fromLatin1(item.label), item.value});
        }
        QComboBox* window = addPillCombo(
            page, QStringLiteral("DisplayFftWindow"), QStringLiteral("Window:"), windows,
            QStringLiteral("The window applied to each transform. It trades how sharp a "
                           "signal looks against how far its skirts spread."),
            nullptr, nullptr);
        connect(window, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, window](int index) {
            if (index >= 0) {
                emit fftWindowRequested(window->itemData(index).toInt());
            }
        });
        reseed("DisplayFftWindow", [this, window]() {
            const int index = window->findData(m_fftState.windowType);
            if (index >= 0) {
                const QSignalBlocker block(window);
                window->setCurrentIndex(index);
            }
        });
    }

    {
        QSlider* decimation = addPillSlider(
            page, QStringLiteral("DisplayDecimation"), QStringLiteral("Decimation:"),
            ControlRanges::kDisplayDecimationMin, ControlRanges::kDisplayDecimationMax,
            ControlRanges::kDisplayDecimationStep,
            QStringLiteral("Divide the sample rate before the transform: finer bins "
                           "over a narrower span"),
            number, nullptr, nullptr);
        QLabel* value =
            page.widget->findChild<QLabel*>(QStringLiteral("pillDisplayDecimationValue"));
        connect(decimation, &QSlider::valueChanged, this,
                [this](int v) { emit decimationRequested(v); });
        reseed("DisplayDecimation", [this, decimation, value]() {
            const QSignalBlocker block(decimation);
            decimation->setValue(m_fftState.decimation);
            value->setText(QString::number(decimation->value()));
        });
    }

    // Why the station's rows cannot be changed, when they cannot.
    m_stationNote = addPillNote(page, QString(), 1);
    m_stationNote->setObjectName(QStringLiteral("pillStationNote"));
    m_stationNote->setStyleSheet(PillStyle::kNoteFitted);
    // It takes no height while it has nothing to say (updatePillEnables).
    m_stationNote->hide();

    endPillPage(page);
    updateFftReadout();
}

// A drag asks once, as the handle is let go. Anything else that moves the
// slider (the keyboard, a click on the groove) asks once, 300 ms after the
// last step.
void SpectrumOverlayPanel::fftSizeSliderChanged(int position)
{
    if (m_fftState.sizingMode != ControlRanges::DisplayFftSizingMode::Fixed) {
        // Grayed, it sets nothing: back to the size that runs.
        seedPillRow(QStringLiteral("DisplayFftSize"));
        return;
    }
    if (m_fftSizeReleasePending) {
        m_fftSizeReleasePending = false;
        m_fftSizeKeyTimer->stop();
        emit fftBaselineRequested(ControlRanges::displayFftSizeAt(position));
        return;
    }
    m_fftSizeKeyTimer->start();
}

double SpectrumOverlayPanel::runningBinWidthHz() const
{
    return DisplayFftReadoutFacts{m_fftState.runningSize, m_fftState.sampleRateHz,
                                  m_fftState.decimation}
        .binWidthHz();
}

void SpectrumOverlayPanel::showFftSizingMode(ControlRanges::DisplayFftSizingMode mode)
{
    // The buttons' own `toggled` handler stands aside.
    const QScopedValueRollback<bool> own(m_showingFftSizingMode, true);
    for (int i = 0; i < m_fftSizingBtns.size(); ++i) {
        m_fftSizingBtns[i]->setChecked(kDisplayFftSizingChoices[i].mode == mode);
    }
}

// The line always states what runs, whichever control chose it (the
// "Sizing:" row says which). The wording is Setup's too
// (DisplayFftReadout.h).
void SpectrumOverlayPanel::updateFftReadout()
{
    if (!m_fftReadout) {
        return;
    }
    setPillNoteText(m_fftReadout,
                    displayFftReadoutText({m_fftState.runningSize, m_fftState.sampleRateHz,
                                           m_fftState.decimation}));
}

void SpectrumOverlayPanel::setFftState(const DisplayPillFftState& state)
{
    const bool modeChanged = state.sizingMode != m_fftState.sizingMode;
    m_fftState = state;
    static constexpr const char* kRows[] = {
        "DisplayFftSizingMode", "DisplayFftSize", "DisplaySpectrumFps", "DisplayFftWindow",
        "DisplayHzPerBinTarget", "DisplayDecimation"};
    for (const char* keyBase : kRows) {
        seedPillRow(QString::fromLatin1(keyBase));
    }
    updateFftReadout();
    if (modeChanged) {
        // FFT Size and Hz/bin change hands.
        updatePillEnables();
    }
}

void SpectrumOverlayPanel::setStationRowsAvailable(bool available, const QString& reason)
{
    const QString shownReason = available ? QString() : reason;
    if (!available && m_fftSizeKeyTimer) {
        // Nothing is asked of a station that cannot be reached.
        m_fftSizeKeyTimer->stop();
    }
    // MainWindow repeats this on every remote telemetry tick. A changed
    // reason alone still updates the rows and the note.
    if (m_stationRowsApplied && available == m_stationRowsAvailable
        && shownReason == m_stationRowsReason) {
        return;
    }
    m_stationRowsApplied = true;
    m_stationRowsAvailable = available;
    m_stationRowsReason = shownReason;
    updatePillEnables();
}

// ── Display pill: the Waterfall tab ──────────────────────────────────────────
// The pan's waterfall, in the order of the design's table 3.5. Clarity sets
// the levels through Black Level and Color Gain; with Clarity off they are
// AGC, NF-AGC, Low and High, or the spectrum's dB Max and Min.

void SpectrumOverlayPanel::buildPillWaterfallPage()
{
    PillPage page = beginPillPage(DisplayPillTab::Waterfall, QStringLiteral("Drawing"));

    const PillFormat percent = [](int v) { return QStringLiteral("%1%").arg(v); };
    const PillFormat ms = [](int v) { return QStringLiteral("%1 ms").arg(v); };
    QHBoxLayout* line = nullptr;

    {
        PillChoices schemes;
        for (int i = 0; i < static_cast<int>(WfColorScheme::Count); ++i) {
            schemes.append({QString::fromUtf8(wfSchemeName(static_cast<WfColorScheme>(i))), i});
        }
        addPillCombo(page, QStringLiteral("DisplayWfColorScheme"), QStringLiteral("Scheme:"),
                     schemes,
                     QStringLiteral("The colors the waterfall is drawn in"),
                     [](const SpectrumWidget* w) { return static_cast<int>(w->wfColorScheme()); },
                     [](SpectrumWidget* w, int v) {
                         w->setWfColorScheme(static_cast<WfColorScheme>(v));
                     });
    }

    static constexpr int kRateMinMs = 10;
    static constexpr int kRateMaxMs = 500;
    static constexpr int kRateStepMs = 10;
    addPillSlider(page, QStringLiteral("DisplayWfUpdatePeriodMs"), QStringLiteral("Rate:"),
                  kRateMinMs, kRateMaxMs, kRateStepMs,
                  QStringLiteral("How often the waterfall adds a row. Lower is faster."),
                  ms,
                  [](const SpectrumWidget* w) { return w->wfUpdatePeriodMs(); },
                  [](SpectrumWidget* w, int v) { w->setWfUpdatePeriodMs(v); });

    addPillSlider(page, QStringLiteral("DisplayWfOpacity"), QStringLiteral("Opacity:"),
                  0, 100, 1,
                  QStringLiteral("How solid the waterfall is drawn"),
                  percent,
                  [](const SpectrumWidget* w) { return w->wfOpacity(); },
                  [](SpectrumWidget* w, int v) { w->setWfOpacity(v); });

    // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:1598-1612 [@1e0718ad]
    // NereusSDR design (follow-up wave F1): the row and its Off/On pill
    // are AetherSDR's; the slider is a threshold in dB, 1 to 20 in 1 dB
    // steps, where AetherSDR's is 5 to 95 read as a factor of tile
    // intensity (1.05 to 1.95), which has no dB meaning.
    // NB Blank + Off/On
    {
        static constexpr int kBlankMin = 1;
        static constexpr int kBlankMax = 20;
        QPushButton* pill = makePillToggle(
            page.widget, QStringLiteral("WaterfallBlankingEnabled"), QStringLiteral("On"),
            QStringLiteral("Suppress impulse noise stripes in waterfall"),
            [](const SpectrumWidget* w) { return w->waterfallBlankingEnabled(); },
            [](SpectrumWidget* w, bool on) { w->setWaterfallBlankingEnabled(on); },
            QStringLiteral("Off"), true);
        addPillSlider(page, QStringLiteral("WaterfallBlankingThresholdDb"),
                      QStringLiteral("NB Blank:"), kBlankMin, kBlankMax, 1,
                      QStringLiteral("How many dB louder than the rows before it a row "
                                     "must be to be blanked. Lower blanks more."),
                      [](int v) { return QStringLiteral("%1 dB").arg(v); },
                      [](const SpectrumWidget* w) { return w->waterfallBlankingThresholdDb(); },
                      [](SpectrumWidget* w, int v) { w->setWaterfallBlankingThresholdDb(v); },
                      pill);
    }

    {
        PillChoices averaging;
        for (const ControlRanges::DisplayChoiceItem& item : ControlRanges::kDisplayAveragingModes) {
            averaging.append({QString::fromLatin1(item.label), item.value});
        }
        addPillCombo(page, QStringLiteral("DisplayWaterfallAveraging"), QStringLiteral("WF Avg:"),
                     averaging,
                     QStringLiteral("How the waterfall is smoothed over time"),
                     [](const SpectrumWidget* w) {
                         return static_cast<int>(w->waterfallAveraging());
                     },
                     [](SpectrumWidget* w, int v) {
                         w->setWaterfallAveraging(static_cast<SpectrumAveraging>(v));
                     });
    }
    addPillAvgTimeSlider(page, QStringLiteral("DisplayWaterfallAverageTimeMs"),
                         QStringLiteral("WF Avg Time:"),
                         QStringLiteral("How long the waterfall's averaging looks back"),
                         [](const SpectrumWidget* w) { return w->waterfallAverageTimeMs(); },
                         [](SpectrumWidget* w, int v) { w->setWaterfallAverageTimeMs(v); });
    addPillHeader(page, QStringLiteral("Overlays"), QString(), false);
    line = addPillLine(page);
    QVector<QPushButton*> filters;
    filters << addPillToggle(line, QStringLiteral("WaterfallStopOnTx"), QStringLiteral("Stop on TX"),
                  QStringLiteral("Hold the waterfall still while transmitting"),
                  [](const SpectrumWidget* w) { return w->waterfallStopOnTx(); },
                  [](SpectrumWidget* w, bool on) { w->setWaterfallStopOnTx(on); });
    filters << addPillToggle(line, QStringLiteral("DisplayShowRxFilterOnWaterfall"),
                  QStringLiteral("RX Filter"),
                  QStringLiteral("Show the receive filter's passband on the waterfall"),
                  [](const SpectrumWidget* w) { return w->showRxFilterOnWaterfall(); },
                  [](SpectrumWidget* w, bool on) { w->setShowRxFilterOnWaterfall(on); });
    filters << addPillToggle(line, QStringLiteral("DisplayShowTxFilterOnRxWaterfall"),
                  QStringLiteral("TX Filter"),
                  QStringLiteral("Show the transmit filter's passband on the waterfall"),
                  [](const SpectrumWidget* w) { return w->showTxFilterOnRxWaterfall(); },
                  [](SpectrumWidget* w, bool on) { w->setShowTxFilterOnRxWaterfall(on); });
    fitPillLine(page, line, filters);
    line = addPillLine(page);
    QVector<QPushButton*> zeros;
    zeros << addPillToggle(line, QStringLiteral("DisplayShowRxZeroLine"), QStringLiteral("RX Zero"),
                  QStringLiteral("Show the receive frequency as a line on the waterfall"),
                  [](const SpectrumWidget* w) { return w->showRxZeroLineOnWaterfall(); },
                  [](SpectrumWidget* w, bool on) { w->setShowRxZeroLineOnWaterfall(on); });
    zeros << addPillToggle(line, QStringLiteral("DisplayShowTxZeroLine"), QStringLiteral("TX Zero"),
                  QStringLiteral("Show the transmit frequency as a line on the waterfall"),
                  [](const SpectrumWidget* w) { return w->showTxZeroLineOnWaterfall(); },
                  [](SpectrumWidget* w, bool on) { w->setShowTxZeroLineOnWaterfall(on); });

    fitPillLine(page, line, zeros);
    addPillHeader(page, QStringLiteral("Time"), QString(), false);
    addPillCombo(page, QStringLiteral("DisplayWfTimestampPos"), QStringLiteral("Timestamp:"),
                 {{QStringLiteral("None"),
                   static_cast<int>(SpectrumWidget::TimestampPosition::None)},
                  {QStringLiteral("Left"),
                   static_cast<int>(SpectrumWidget::TimestampPosition::Left)},
                  {QStringLiteral("Right"),
                   static_cast<int>(SpectrumWidget::TimestampPosition::Right)}},
                 QStringLiteral("Where the time of each row is written on the waterfall"),
                 [](const SpectrumWidget* w) {
                     return static_cast<int>(w->wfTimestampPosition());
                 },
                 [](SpectrumWidget* w, int v) {
                     w->setWfTimestampPosition(static_cast<SpectrumWidget::TimestampPosition>(v));
                 });

    static constexpr int kSecondMs = 1000;
    static constexpr int kMinuteMs = 60 * kSecondMs;
    addPillCombo(page, QStringLiteral("DisplayWaterfallHistoryMs"), QStringLiteral("Rewind:"),
                 {{QStringLiteral("60 seconds"), 60 * kSecondMs},
                  {QStringLiteral("5 minutes"), 5 * kMinuteMs},
                  {QStringLiteral("15 minutes"), 15 * kMinuteMs},
                  {QStringLiteral("20 minutes"), 20 * kMinuteMs}},
                 QStringLiteral("How far back the waterfall can be scrolled"),
                 [](const SpectrumWidget* w) { return static_cast<int>(w->waterfallHistoryMs()); },
                 [](SpectrumWidget* w, int v) { w->setWaterfallHistoryMs(v); });

    QLabel* rewindNote = addPillNote(page, QString(), 2);
    rewindNote->setObjectName(QStringLiteral("pillWaterfallRewindNote"));
    registerPillRow(QStringLiteral("WaterfallRewindNote"), {rewindNote},
                    QStringLiteral("The selected history is bounded by the waterfall row capacity"),
                    [this, rewindNote]() {
        if (!m_displayTarget) { return; }
        const qint64 wantedSeconds = m_displayTarget->waterfallHistoryMs() / 1000;
        const qint64 capacitySeconds = qint64(m_displayTarget->wfUpdatePeriodMs())
                                     * SpectrumWidget::kMaxWaterfallHistoryRows / 1000;
        const qint64 kept = std::min(wantedSeconds, capacitySeconds);
        const QString time = QStringLiteral("%1:%2").arg(kept / 60)
                           .arg(kept % 60, 2, 10, QLatin1Char('0'));
        rewindNote->setText(kept < wantedSeconds
            ? QStringLiteral("At this Rate the waterfall keeps %1 (%2 rows). Slow Rate to keep more.")
                  .arg(time).arg(SpectrumWidget::kMaxWaterfallHistoryRows)
            : QStringLiteral("Keeps %1 at this Rate.").arg(time));
        const int room = PillStyle::kWidth - 2 - 2 * PillStyle::kMarginH;
        const QRect text = QFontMetrics(rewindNote->font()).boundingRect(
            QRect(0, 0, room, 1000), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
            rewindNote->text());
        rewindNote->setFixedHeight(std::max(2 * PillStyle::kNoteLineH, text.height()));
    });

    endPillPage(page);
}

// ── Display pill: Clarity as it stands ───────────────────────────────────────

void SpectrumOverlayPanel::setClarityState(bool enabled, bool active, bool paused)
{
    const bool effectiveEnabled = m_displayTarget ? m_displayTarget->clarityEnabled() : enabled;
    // The registry reports status on each changed floor. Constructor/open
    // seeding already covers a new panel whose flags are unchanged.
    if (m_clarityEnabled == effectiveEnabled && m_clarityActive == active
        && m_clarityPaused == paused) {
        return;
    }
    m_clarityEnabled = effectiveEnabled;
    m_clarityActive = active;
    m_clarityPaused = paused;
    seedPillRow(QStringLiteral("WaterfallLevelsMode"));
    updateClarityBadge();
    updatePillEnables();
}

// Green while Clarity sets this pan's levels, amber while it holds (as it
// does while keyed), grey otherwise. The badge is never hidden.
void SpectrumOverlayPanel::updateClarityBadge()
{
    if (!m_clarityBadge) {
        return;
    }
    const bool paused = m_clarityEnabled && m_clarityPaused;
    const bool active = m_clarityEnabled && m_clarityActive && !paused;
    const QString state = paused   ? QStringLiteral("paused")
                        : active ? QStringLiteral("active")
                                 : QStringLiteral("off");
    // An unchanged badge is not restyled: each setStyleSheet posts a
    // StyleChange that asks for the pill's geometry again. The look's
    // theming keeps the original sheet, so a theme change needs no re-set.
    if (state == m_clarityBadgeState) {
        return;
    }
    m_clarityBadgeState = state;
    m_clarityBadge->setProperty("clarityState", state);
    m_clarityBadge->setStyleSheet(paused   ? PillStyle::kBadgePaused
                                  : active ? PillStyle::kBadgeActive
                                           : PillStyle::kBadgeOff);
}

// ── Display pill: the 3D View tab ────────────────────────────────────────────
// The same seven controls as the right-click menu's 3D section and Setup's
// 3D View page, with the same words.

void SpectrumOverlayPanel::buildPillView3DPage()
{
    PillPage page = beginPillPage(DisplayPillTab::View3D, QStringLiteral("View"));
    const PillFormat percent = [](int v) { return QStringLiteral("%1%").arg(v); };

    const PillFormat number = [](int v) { return QString::number(v); };

    addPillChoices(page, QStringLiteral("DisplaySpectrumRenderMode"), QStringLiteral("pillDisplaySpectrumRenderMode"),
        {{QStringLiteral("2D Waterfall"), static_cast<int>(SpectrumRenderMode::Mode2D)},
         {QStringLiteral("3D Stacked Trace"), static_cast<int>(SpectrumRenderMode::Mode3D)}},
        {QStringLiteral("2D"), QStringLiteral("3D")},
        QStringLiteral("Show the classic spectrum and waterfall, or the 3D view"),
        [](const SpectrumWidget* widget) { return widget->spectrumRenderMode(); },
        [](SpectrumWidget* widget, int mode) { widget->setSpectrumRenderMode(mode); });
    m_viewNote = addPillNote(page, QString(), 3);
    m_viewNote->setObjectName(QStringLiteral("pillViewNote"));

    // 3D Floor is kept per band, not per pan, so it has no pan key; the
    // row is named for what it shows.
    static constexpr int kFloorMaxDb = 24;
    addPillSlider(page, QStringLiteral("Display3DFloor"), QStringLiteral("3D Floor:"),
                  0, kFloorMaxDb, 1,
                  QStringLiteral("How far below the noise floor the surface starts. "
                                 "Each band keeps its own value."),
                  [](int v) { return QStringLiteral("%1 dB").arg(v); },
                  [](const SpectrumWidget* w) { return w->dssFloorDepth(); },
                  [](SpectrumWidget* w, int v) { w->setDssFloorDepth(v); });
    addPillSlider(page, QStringLiteral("Display3DGain"), QStringLiteral("3D Gain:"),
                  0, 100, 1,
                  QStringLiteral("3D surface color gain: how far down the signal range the "
                                 "colormap reaches.\nHigher = color down toward the noise "
                                 "floor; lower = color only on the strongest signals."),
                  number,
                  [](const SpectrumWidget* w) { return w->dssGain(); },
                  [](SpectrumWidget* w, int v) { w->setDssGain(v); });
    addPillSlider(page, QStringLiteral("Display3DSpan"), QStringLiteral("3D Span:"),
                  0, 100, 1,
                  QStringLiteral("3D surface width: how far the nearest traces overhang the "
                                 "plot edges,\nusing spectrum the radio sends from outside the "
                                 "panadapter.\nHigher = the empty wedges beside the surface "
                                 "close from the front;\n0 = the classic narrowing trapezoid."),
                  number,
                  [](const SpectrumWidget* w) { return w->dssRowSpan(); },
                  [](SpectrumWidget* w, int v) { w->setDssRowSpan(v); });
    addPillSlider(page, QStringLiteral("Display3DAngle"), QStringLiteral("3D Angle:"),
                  0, 100, 1,
                  QStringLiteral("Viewing angle for the 3D surface: low looks along the traces "
                                 "edge-on, high looks down on them.\n"
                                 "50 is the classic fixed angle."),
                  number,
                  [](const SpectrumWidget* w) { return w->dssAngle(); },
                  [](SpectrumWidget* w, int v) { w->setDssAngle(v); });
    static constexpr int kSpeedMax = 10;
    addPillSlider(page, QStringLiteral("Display3DSpeed"), QStringLiteral("3D Speed:"),
                  0, kSpeedMax, 1,
                  QStringLiteral("How many waterfall rows each 3D row covers. Match keeps the "
                                 "3D history the same length in time as the waterfall. 1 pushes "
                                 "every row, the fastest look. Higher values fold more rows "
                                 "into each 3D row, keeping peaks, so the surface recedes more "
                                 "slowly."),
                  [](int v) { return v == 0 ? QStringLiteral("Match") : QString::number(v); },
                  [](const SpectrumWidget* w) { return w->dssRowDivider(); },
                  [](SpectrumWidget* w, int v) { w->setDssRowDivider(v); });

    QHBoxLayout* line = addPillLine(page);
    addPillToggle(line, QStringLiteral("Display3DSliceShadow"), QStringLiteral("Slice Shadow"),
                  QStringLiteral("Darken each slice's passband onto the 3D surface so it leans "
                                 "back\nwith the perspective, instead of drawing flat on top of it."),
                  [](const SpectrumWidget* w) { return w->threeDSliceDepth(); },
                  [](SpectrumWidget* w, bool on) { w->setThreeDSliceDepth(on); });

    // Background: a picture behind the spectrum, its opacity and the colour
    // under it.
    // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:1727-1760 [@1e0718ad]
    // (Choose... / Clear / Off), :1764-1783 (opacity), :1786-1814 (colour).
    // NereusSDR, based on AetherSDR: Clear removes the picture; the Off
    // pill hides it and keeps it chosen; all four are saved per pan.
    addPillHeader(page, QStringLiteral("Background"), QString(), false);
    {
        QLabel* name = makePillLabel(QStringLiteral("Background:"));
        if (WorkspacePreview::enabled()) {
            // The approved live View page labels Background above its
            // full-width Choose/Clear/On row; no label is squeezed aside.
            page.grid->addWidget(name, page.row++, 0, 1, 4);
        } else {
            page.grid->addWidget(name, page.row, 0);
        }

        auto* holder = new QWidget(page.widget);
        holder->setFixedHeight(PillStyle::kControlH);
        auto* row = new QHBoxLayout(holder);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(PillStyle::kSpacing);

        const QString chooseTip =
            QStringLiteral("Choose a picture to show behind the spectrum");
        QPushButton* choose = makePillButton(QStringLiteral("Choose..."), chooseTip);
        choose->setObjectName(QStringLiteral("pillDisplayBackgroundImage"));
        row->addWidget(choose);
        const QString clearTip =
            QStringLiteral("Remove the picture from behind the spectrum");
        QPushButton* clear = makePillButton(QStringLiteral("Clear"), clearTip);
        clear->setObjectName(QStringLiteral("pillDisplayBackgroundImageClear"));
        row->addWidget(clear);
        row->addStretch(1);
        page.grid->addWidget(holder, page.row, WorkspacePreview::enabled() ? 0 : 1,
                             1, WorkspacePreview::enabled() ? 4 : 3);
        ++page.row;

        connect(choose, &QPushButton::clicked, this, [this]() {
            if (!m_displayTarget) {
                return;
            }
            const QString current = m_displayTarget->backgroundImagePath();
            const QString startDir = current.isEmpty()
                ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                : QFileInfo(current).absolutePath();
            const QString chosen = m_pillPictureChooser
                ? m_pillPictureChooser(startDir)
                : QFileDialog::getOpenFileName(
                      window(), QStringLiteral("Choose a Background Picture"), startDir,
                      QStringLiteral("Pictures (*.png *.jpg *.jpeg *.bmp)"));
            if (chosen.isEmpty() || !m_displayTarget) {
                return;
            }
            // A picture just chosen is shown.
            m_displayTarget->setBackgroundImagePath(chosen);
            m_displayTarget->setBackgroundEnabled(true);
        });
        connect(clear, &QPushButton::clicked, this, [this]() {
            if (m_displayTarget) {
                m_displayTarget->setBackgroundImagePath(QString());
            }
        });
        // Nothing to seed: the buttons show no value.
        registerPillRow(QStringLiteral("DisplayBackgroundImage"), {name, choose}, chooseTip, {});
        registerPillRow(QStringLiteral("DisplayBackgroundImageClear"), {clear}, clearTip, {});

        addPillToggle(row, QStringLiteral("DisplayBackgroundEnabled"), QStringLiteral("On"),
                      QStringLiteral("Show or hide the picture behind the spectrum. "
                                     "Off keeps the picture chosen."),
                      [](const SpectrumWidget* w) { return w->backgroundEnabled(); },
                      [](SpectrumWidget* w, bool on) { w->setBackgroundEnabled(on); },
                      QStringLiteral("Off"));
    }
    addPillSlider(page, QStringLiteral("DisplayBackgroundOpacity"), QStringLiteral("BG Opacity:"),
                  0, 100, 1,
                  QStringLiteral("How strongly the picture shows behind the spectrum"),
                  percent,
                  [](const SpectrumWidget* w) { return w->backgroundOpacity(); },
                  [](SpectrumWidget* w, int v) { w->setBackgroundOpacity(v); });
    {
        QLabel* name = makePillLabel(QStringLiteral("BG Color:"));
        page.grid->addWidget(name, page.row, 0);
        QPushButton* swatch = makePillSwatch(
            QStringLiteral("DisplayBackgroundColor"),
            QStringLiteral("Choose the color behind the spectrum and its picture"),
            QStringLiteral("Spectrum Background Color"),
            [](const SpectrumWidget* w) { return w->backgroundFillColor(); },
            [](SpectrumWidget* w, const QColor& c) { w->setBackgroundFillColor(c); });
        page.grid->addWidget(swatch, page.row, 1, Qt::AlignCenter);
        ++page.row;
        // The label greys with its swatch.
        const PillRow row = m_pillRows.value(QStringLiteral("DisplayBackgroundColor"));
        registerPillRow(QStringLiteral("DisplayBackgroundColor"), {name, swatch}, row.tip, row.seed);
    }

    endPillPage(page);
}

// ── VAX flyout ────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildVaxFlyout()
{
    m_vaxFlyout = new QWidget(parentWidget());
    claimMouseForFlyout(m_vaxFlyout);
    m_vaxFlyout->setStyleSheet(panelStyle());
    m_vaxFlyout->hide();

    auto* vb = new QVBoxLayout(m_vaxFlyout);
    vb->setContentsMargins(6, 6, 6, 6);
    vb->setSpacing(4);

    // VAX Ch combo (from AetherSDR buildDaxPanel)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel("VAX Ch");
        lbl->setStyleSheet(labelStyle());
        row->addWidget(lbl);
        m_vaxCmb = new QComboBox;
        m_vaxCmb->setObjectName(QStringLiteral("vaxCombo"));
        m_vaxCmb->addItems({"Off", "1", "2", "3", "4"});
        // Disabled until setRadioModel() resolves a slice; retains the
        // pre-3O tooltip in that transient state.
        m_vaxCmb->setEnabled(false);
        m_vaxCmb->setToolTip("VAX channel (waiting for the radio)");
        row->addWidget(m_vaxCmb, 1);
        vb->addLayout(row);

        // Widget → Model: user picks a channel → resolved slice setVaxChannel.
        // Combo index 0 = "Off" = vaxChannel 0; indices 1..4 = VAX 1..4
        // (1:1 mapping, matches the header ordering above).
        connect(m_vaxCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (m_updatingFromModel || !m_radioModel) {
                return;
            }
            SliceModel* s = resolvedSlice();
            if (s) {
                s->setVaxChannel(idx);
            }
        });
    }

    m_vaxFlyout->setFixedWidth(140);
    m_vaxFlyout->adjustSize();
}

// Bind VAX and antenna controls to the slice currently owned by this pan.
//
// Widget → Model: the QComboBox::currentIndexChanged lambda installed in
// buildVaxFlyout() calls the resolved slice's setVaxChannel() (and gates on
// m_updatingFromModel to suppress the echo).
// Model → Widget: this function stores the vaxChannelChanged connection in
// m_vaxChannelConn so a rebind can cleanly disconnect before reconnecting
// to a new slice. QObject auto-disconnect handles the model-destroyed case
// for free; a live-to-live rebind needs the explicit disconnect we do here.
void SpectrumOverlayPanel::setRadioModel(RadioModel* model)
{
    if (m_radioModel == model) {
        return;
    }

    // Drop any prior Model→Widget subscription. Safe when the stored
    // connection is default-constructed (QObject::disconnect() no-ops on
    // an invalid Connection handle).
    if (m_vaxChannelConn) {
        QObject::disconnect(m_vaxChannelConn);
        m_vaxChannelConn = {};
    }
    if (m_rxAntConn) {
        QObject::disconnect(m_rxAntConn);
        m_rxAntConn = {};
    }
    if (m_txAntConn) {
        QObject::disconnect(m_txAntConn);
        m_txAntConn = {};
    }

    if (m_radioModel) {
        QObject::disconnect(m_radioModel, nullptr, this, nullptr);
    }
    m_radioModel = model;

    if (!m_vaxCmb) {
        return;  // defensive: flyout builder has not run yet
    }

    if (!m_radioModel) {
        // Unbound — revert to the pre-3O disabled state.
        m_vaxCmb->setEnabled(false);
        m_vaxCmb->setToolTip("VAX channel (waiting for the radio)");
        if (m_rxAntCmb) { m_rxAntCmb->setEnabled(false); }
        if (m_txAntCmb) { m_txAntCmb->setEnabled(false); }
        showAttValues();
        return;
    }

    // R-R3-21: the ATT flyout follows the step attenuator's object.
    if (StepAttenuatorFacade* att = m_radioModel->stepAttFacade()) {
        connect(att, &StepAttenuatorFacade::enabledChanged,
                this, [this](bool) { showAttValues(); });
        for (auto signal : {&StepAttenuatorFacade::attenuationDbChanged,
                            &StepAttenuatorFacade::minDbChanged,
                            &StepAttenuatorFacade::maxDbChanged}) {
            connect(att, signal, this, [this](int) { showAttValues(); });
        }
        connect(att, &StepAttenuatorFacade::windowAvailabilityChanged,
                this, [this](bool) { showAttValues(); });
        connect(att, &StepAttenuatorFacade::editRejected, this,
                [this](const QString& reason) {
            if (m_attReason) {
                m_attReason->setText(reason);
                m_attReason->setVisible(true);
            }
        });
    }
    showAttValues();

    // Any slice topology change can change this pan's resolved slice.
    const auto rebind = [this](int) {
        bindToPanSlice();
    };
    connect(m_radioModel, &RadioModel::sliceAdded,   this, rebind);
    connect(m_radioModel, &RadioModel::sliceRemoved, this, rebind);

    bindToPanSlice();
}

void SpectrumOverlayPanel::setSliceResolver(SliceResolver resolver)
{
    m_sliceResolver = std::move(resolver);
    if (m_radioModel) {
        bindToPanSlice();
    }
}

SliceModel* SpectrumOverlayPanel::resolvedSlice() const
{
    if (!m_radioModel) {
        return nullptr;
    }
    return m_sliceResolver ? m_sliceResolver() : m_radioModel->sliceById(0);
}

void SpectrumOverlayPanel::bindToPanSlice()
{
    if (!m_vaxCmb || !m_radioModel) {
        return;
    }

    // Drop any prior Model→Widget subscription. QObject auto-disconnect
    // already handles the destroyed-slice case, but a live slice that is
    // no longer index 0 (after a sliceRemoved shuffle) needs the explicit
    // disconnect.
    if (m_vaxChannelConn) {
        QObject::disconnect(m_vaxChannelConn);
        m_vaxChannelConn = {};
    }
    // Phase 3P-I-a T18 — antenna Model→Widget subscriptions. Same
    // rebind-on-shuffle pattern as VAX above.
    if (m_rxAntConn) { QObject::disconnect(m_rxAntConn); m_rxAntConn = {}; }
    if (m_txAntConn) { QObject::disconnect(m_txAntConn); m_txAntConn = {}; }

    SliceModel* s = resolvedSlice();
    if (s) {
        // Seed the combo with the current model value before wiring up
        // the listener, using the flag pattern so no spurious setVaxChannel
        // is issued back to the slice.
        m_updatingFromModel = true;
        m_vaxCmb->setCurrentIndex(s->vaxChannel());
        m_updatingFromModel = false;

        m_vaxChannelConn = connect(s, &SliceModel::vaxChannelChanged,
                                   this, [this](int ch) {
            if (!m_vaxCmb) {
                return;
            }
            m_updatingFromModel = true;
            m_vaxCmb->setCurrentIndex(ch);
            m_updatingFromModel = false;
        });

        // R-R3-44: live in a remote window too. There it picks this
        // computer's VAX channel for the Core's slice, which the remote
        // model keeps on this computer and RemoteVaxRouter feeds from the
        // Core's receiver stream.
        m_vaxCmb->setEnabled(true);
        m_vaxCmb->setToolTip(QStringLiteral("Route this slice's RX audio to a VAX channel"));
        if (m_rxAntCmb) { m_rxAntCmb->setEnabled(true); }
        if (m_txAntCmb) { m_txAntCmb->setEnabled(true); }

        // Phase 3P-I-a T18 — seed + subscribe antenna combos.
        // SliceModel::rxAntennaChanged also fires from T13's reverse
        // sync (AlexController → slice), so the combo label stays
        // coherent with the per-band state after a band change.
        if (m_rxAntCmb) {
            m_updatingFromModel = true;
            const int idx = m_rxAntCmb->findText(s->rxAntenna());
            if (idx >= 0) { m_rxAntCmb->setCurrentIndex(idx); }
            m_updatingFromModel = false;

            m_rxAntConn = connect(s, &SliceModel::rxAntennaChanged,
                                  this, [this](const QString& ant) {
                if (!m_rxAntCmb) { return; }
                m_updatingFromModel = true;
                const int i = m_rxAntCmb->findText(ant);
                if (i >= 0) { m_rxAntCmb->setCurrentIndex(i); }
                m_updatingFromModel = false;
            });
        }
        if (m_txAntCmb) {
            m_updatingFromModel = true;
            const int idx = m_txAntCmb->findText(s->txAntenna());
            if (idx >= 0) { m_txAntCmb->setCurrentIndex(idx); }
            m_updatingFromModel = false;

            m_txAntConn = connect(s, &SliceModel::txAntennaChanged,
                                  this, [this](const QString& ant) {
                if (!m_txAntCmb) { return; }
                m_updatingFromModel = true;
                const int i = m_txAntCmb->findText(ant);
                if (i >= 0) { m_txAntCmb->setCurrentIndex(i); }
                m_updatingFromModel = false;
            });
        }
    } else {
        // No slice for this pan — typically the pre-connectToRadio() state, or a
        // transient window during slice teardown. The sliceAdded listener
        // installed by setRadioModel() will call us again when a slice
        // comes (back) online.
        m_updatingFromModel = true;
        m_vaxCmb->setCurrentIndex(0);
        m_updatingFromModel = false;
        m_vaxCmb->setEnabled(false);
        m_vaxCmb->setToolTip("VAX channel (waiting for this pan's slice)");
        if (m_rxAntCmb) { m_rxAntCmb->setEnabled(false); }
        if (m_txAntCmb) { m_txAntCmb->setEnabled(false); }
    }
}

// Phase 3P-I-a T18 — repopulate RX/TX antenna combos from BoardCapabilities.
// Called by MainWindow on connect and on currentRadioChanged. Empty port
// list (HL2/Atlas) hides both rows entirely. After repopulating we reseed
// the current value from the resolved slice so persisted selections survive
// a reconnect.
void SpectrumOverlayPanel::setBoardCapabilities(const BoardCapabilities& caps)
{
    if (!m_rxAntCmb || !m_txAntCmb) { return; }

    // B3: use AntennaPopupBuilder::labels() for the capability-gated list.
    // Derive SkuUiProfile from RadioModel (already accessible) so RX-only
    // labels (EXT1/EXT2/XVTR/BYPS/RX1/RX2) appear when rxOnlyAntennaCount > 0.
    // Falls back to antennaLabels(caps) (ANT1-3 only) when no model is set.
    const SkuUiProfile sku = m_radioModel
        ? skuUiProfileFor(m_radioModel->hardwareProfile().model)
        : SkuUiProfile{};

    const QStringList rxLabels = AntennaPopupBuilder::labels(caps, sku,
        AntennaPopupBuilder::Mode::RX);
    const QStringList txLabels = AntennaPopupBuilder::labels(caps, sku,
        AntennaPopupBuilder::Mode::TX);
    const bool show = !rxLabels.isEmpty();

    // Suppress widget→model echo while we clear + refill the combos.
    // Clearing a combo emits currentTextChanged("") and addItems()
    // emits again; without the guard each setVisible-false path would
    // stomp the resolved slice's antenna setting to empty.
    m_updatingFromModel = true;
    resetItemsBeforeRowChange(m_rxAntCmb);
    resetItemsBeforeRowChange(m_txAntCmb);
    m_rxAntCmb->clear();
    m_txAntCmb->clear();
    m_rxAntCmb->addItems(rxLabels);
    m_txAntCmb->addItems(txLabels);
    m_updatingFromModel = false;

    if (m_rxAntRow) { m_rxAntRow->setVisible(show); }
    if (m_txAntRow) { m_txAntRow->setVisible(show); }

    // R-R3-49: with both rows hidden the ANT flyout would open empty, so
    // the ANT button is not shown on such a board (Hermes Lite 2, Atlas).
    if (m_antAvailable != show) {
        m_antAvailable = show;
        if (!show && m_activeFlyout == m_antFlyout) { hideFlyout(); }
        updateLayout();
    }

    // Reseed from the resolved slice so the combo label matches persisted state.
    if (show && m_radioModel) {
        if (SliceModel* s = resolvedSlice()) {
            m_updatingFromModel = true;
            const int rxIdx = m_rxAntCmb->findText(s->rxAntenna());
            if (rxIdx >= 0) { m_rxAntCmb->setCurrentIndex(rxIdx); }
            const int txIdx = m_txAntCmb->findText(s->txAntenna());
            if (txIdx >= 0) { m_txAntCmb->setCurrentIndex(txIdx); }
            m_updatingFromModel = false;
        }
    }
}

void SpectrumOverlayPanel::toggleVaxFlyout()
{
    // Button index 5 (VAX)
    QPushButton* vaxBtn = m_menuBtns[5];

    if (m_activeFlyout == m_vaxFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();

    // Center vertically on VAX button (from AetherSDR toggleDaxPanel)
    int btnCenterY = vaxBtn->y() + vaxBtn->height() / 2;
    int panelY = y() + btnCenterY - m_vaxFlyout->sizeHint().height() / 2;
    m_vaxFlyout->move(x() + width(), std::max(0, panelY));
    m_vaxFlyout->raise();
    themeFlyout(m_vaxFlyout);
    m_vaxFlyout->show();
    m_activeFlyout = m_vaxFlyout;
    m_activeButton = vaxBtn;
    vaxBtn->setStyleSheet(menuBtnActiveStyle());
}

// ── ATT flyout (R-R3-21) ─────────────────────────────────────────────────────
//
// The ATT button was a disabled placeholder. It now opens the radio's step
// attenuator: its on/off and its level, through RadioModel::stepAttFacade(),
// the object the RX applet's ATT row and Setup > General > Options use. A
// local window reaches the StepAttenuatorController; a remote window
// reaches the Core's, and shows the Core's reason while it cannot.

void SpectrumOverlayPanel::buildAttFlyout()
{
    m_attFlyout = new QWidget(parentWidget());
    claimMouseForFlyout(m_attFlyout);
    m_attFlyout->setObjectName(QStringLiteral("attFlyout"));
    m_attFlyout->setStyleSheet(panelStyle());
    m_attFlyout->hide();

    auto* vb = new QVBoxLayout(m_attFlyout);
    vb->setContentsMargins(6, 6, 6, 6);
    vb->setSpacing(4);

    m_attEnableChk = new QCheckBox(QStringLiteral("Step attenuator"));
    m_attEnableChk->setObjectName(QStringLiteral("attEnableCheck"));
    m_attEnableChk->setStyleSheet(checkBoxStyle());
    m_attEnableChk->setToolTip(QStringLiteral("Use the radio's step attenuator"));
    vb->addWidget(m_attEnableChk);

    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    auto* lbl = new QLabel(QStringLiteral("ATT"));
    lbl->setStyleSheet(labelStyle());
    row->addWidget(lbl);
    m_attSpin = new QSpinBox;
    m_attSpin->setObjectName(QStringLiteral("attSpin"));
    m_attSpin->setSuffix(QStringLiteral(" dB"));
    m_attSpin->setRange(0, 31);
    m_attSpin->setToolTip(QStringLiteral("Step attenuator level"));
    row->addWidget(m_attSpin, 1);
    vb->addLayout(row);

    m_attReason = new QLabel;
    m_attReason->setObjectName(QStringLiteral("attReason"));
    m_attReason->setWordWrap(true);
    m_attReason->setStyleSheet(labelStyle());
    m_attReason->setVisible(false);
    vb->addWidget(m_attReason);

    connect(m_attEnableChk, &QCheckBox::toggled, this, [this](bool on) {
        if (m_updatingFromModel || !m_radioModel) { return; }
        if (!m_attHeldReason.isEmpty()) { return; }  // TX rulings (item 3)
        if (StepAttenuatorFacade* att = m_radioModel->stepAttFacade()) {
            att->setEnabled(on);
        }
        showAttValues();
    });
    connect(m_attSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int dB) {
        if (m_updatingFromModel || !m_radioModel) { return; }
        if (!m_attHeldReason.isEmpty()) { return; }  // TX rulings (item 3)
        if (StepAttenuatorFacade* att = m_radioModel->stepAttFacade()) {
            att->setAttenuationDb(dB);
        }
        showAttValues();
    });

    m_attFlyout->setFixedWidth(160);
    m_attFlyout->adjustSize();
    showAttValues();
}

void SpectrumOverlayPanel::showAttValues()
{
    if (!m_attEnableChk || !m_attSpin || !m_attReason) { return; }
    StepAttenuatorFacade* att = m_radioModel ? m_radioModel->stepAttFacade() : nullptr;
    if (!att) {
        m_attEnableChk->setEnabled(false);
        m_attSpin->setEnabled(false);
        m_attReason->setVisible(false);
        return;
    }
    // A local model's object is bound to the radio's controller; a remote
    // one reaches the Core only while the window says it can.
    const bool available = att->isBound() || att->windowAvailable();
    m_updatingFromModel = true;
    {
        QSignalBlocker blockChk(m_attEnableChk);
        QSignalBlocker blockSpin(m_attSpin);
        m_attEnableChk->setChecked(att->enabled());
        if (att->maxDb() > att->minDb()) {
            m_attSpin->setRange(att->minDb(), att->maxDb());
        }
        m_attSpin->setValue(att->attenuationDb());
    }
    m_updatingFromModel = false;
    // TX rulings (item 3): held on a listened slice, with the reason.
    const bool held = !m_attHeldReason.isEmpty();
    m_attEnableChk->setEnabled(available && !held);
    m_attSpin->setEnabled(available && !held && att->enabled());
    const QString reason = held ? m_attHeldReason
                         : available ? QString() : att->windowUnavailableReason();
    m_attEnableChk->setToolTip(held ? m_attHeldReason
                                    : QStringLiteral("Use the radio's step attenuator"));
    m_attSpin->setToolTip(held ? m_attHeldReason : QStringLiteral("Step attenuator level"));
    m_attReason->setText(reason);
    m_attReason->setVisible(!reason.isEmpty());
}

void SpectrumOverlayPanel::setAttHeldReason(const QString& reason)
{
    if (reason == m_attHeldReason) { return; }
    m_attHeldReason = reason;
    showAttValues();
}

void SpectrumOverlayPanel::toggleAttFlyout()
{
    // Button index 6 (ATT)
    QPushButton* attBtn = m_menuBtns[6];

    if (m_activeFlyout == m_attFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();
    showAttValues();
    m_attFlyout->adjustSize();

    const int btnCenterY = attBtn->y() + attBtn->height() / 2;
    const int panelY = y() + btnCenterY - m_attFlyout->sizeHint().height() / 2;
    m_attFlyout->move(x() + width(), std::max(0, panelY));
    m_attFlyout->raise();
    themeFlyout(m_attFlyout);
    m_attFlyout->show();
    m_activeFlyout = m_attFlyout;
    m_activeButton = attBtn;
    attBtn->setStyleSheet(menuBtnActiveStyle());
}

void SpectrumOverlayPanel::wheelEvent(QWheelEvent* event) { event->accept(); }
void SpectrumOverlayPanel::mousePressEvent(QMouseEvent* event) { event->accept(); }
void SpectrumOverlayPanel::mouseReleaseEvent(QMouseEvent* event) { event->accept(); }

// ── Waterfall zoom buttons ─────────────────────────────────────────────────────
// Four small buttons [S][B][-][+] docked at bottom-left of the spectrum widget.
// [S] = Segment zoom (fit the band plan segment), [B] = Band zoom (fit whole band),
// [-] = zoom out, [+] = zoom in.
// From AetherSDR SpectrumOverlayMenu.cpp zoom controls section.

void SpectrumOverlayPanel::buildZoomButtons()
{
    // Parent: same spectrum widget as the panel (parentWidget())
    m_zoomStrip = new QWidget(parentWidget());
    m_zoomStrip->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_zoomStrip->setAttribute(Qt::WA_NoSystemBackground, true);

    static constexpr int kZBtnW = 24;
    static constexpr int kZBtnH = 20;
    static constexpr int kZGap  = 2;

    const QString zBtnStyle =
        "QPushButton { background: rgba(20, 30, 45, 200); "
        "border: 1px solid rgba(255, 255, 255, 40); border-radius: 2px; "
        "color: #c8d8e8; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #0090e0; }"
        "QPushButton:pressed { background: rgba(0, 144, 224, 200); }";
    const QString zoomStyle = WorkspacePreview::enabled() ? wsMenuBtnStyle(10, 200) : zBtnStyle;

    auto makeZBtn = [&](const QString& text) -> QPushButton* {
        auto* btn = new QPushButton(text, m_zoomStrip);
        btn->setFixedSize(kZBtnW, kZBtnH);
        btn->setStyleSheet(zoomStyle);
        return btn;
    };

    m_zoomSegBtn  = makeZBtn(QStringLiteral("S"));
    m_zoomBandBtn = makeZBtn(QStringLiteral("B"));
    m_zoomOutBtn  = makeZBtn(QStringLiteral("-"));
    m_zoomInBtn   = makeZBtn(QStringLiteral("+"));

    m_zoomSegBtn->setToolTip(QStringLiteral("Segment zoom: fit the band plan segment the slice is in"));
    m_zoomBandBtn->setToolTip(QStringLiteral("Band zoom: fit the whole amateur band"));
    m_zoomOutBtn->setToolTip(QStringLiteral("Zoom out: show more bandwidth"));
    m_zoomInBtn->setToolTip(QStringLiteral("Zoom in: show less bandwidth"));

    connect(m_zoomSegBtn,  &QPushButton::clicked, this, &SpectrumOverlayPanel::zoomSegment);
    connect(m_zoomBandBtn, &QPushButton::clicked, this, &SpectrumOverlayPanel::zoomBand);
    connect(m_zoomOutBtn,  &QPushButton::clicked, this, &SpectrumOverlayPanel::zoomOut);
    connect(m_zoomInBtn,   &QPushButton::clicked, this, &SpectrumOverlayPanel::zoomIn);

    // Lay out horizontally inside the strip
    int xOff = kZGap;
    for (QPushButton* btn : {m_zoomSegBtn, m_zoomBandBtn, m_zoomOutBtn, m_zoomInBtn}) {
        btn->move(xOff, kZGap);
        xOff += kZBtnW + kZGap;
    }
    m_zoomStrip->setFixedSize(xOff, kZBtnH + 2 * kZGap);
    m_zoomStrip->show();
    m_zoomStrip->raise();

    repositionZoomButtons();

    // Display pill plan Task 4: the strip's place follows the pan's scale
    // height, which follows its text size.
    if (auto* pan = qobject_cast<SpectrumWidget*>(parentWidget())) {
        connect(pan, &SpectrumWidget::displaySettingChanged, this,
                [this](const QString& keyBase) {
            if (keyBase == QLatin1String("DisplayFreqScaleFontPx")) {
                repositionZoomButtons();
            }
        });
        connect(pan, &SpectrumWidget::panDisplayReset, this,
                [this]() { repositionZoomButtons(); });
    }
}

void SpectrumOverlayPanel::repositionZoomButtons()
{
    if (!m_zoomStrip || !parentWidget()) { return; }
    QWidget* spectrum = parentWidget();
    // Place at bottom-left of the waterfall area, above the frequency scale
    // (approximate: leave 32px from bottom for frequency scale bar)
    // Display pill plan Task 4: the bar is as tall as the pan's scale
    // text needs (28 px at the default size), plus the 4 px this always
    // left over it.
    static constexpr int kFreqScaleGap     = 4;
    static constexpr int kFreqScaleDefault = 28;
    const auto* pan = qobject_cast<const SpectrumWidget*>(spectrum);
    const int freqScaleH =
        (pan ? pan->freqScaleHeight() : kFreqScaleDefault) + kFreqScaleGap;
    static constexpr int kMargin     = 4;
    int x = kMargin;
    int y = spectrum->height() - freqScaleH - m_zoomStrip->height() - kMargin;
    y = std::max(kMargin, y);
    // Task 26: on a short pan the open button column reaches down to the
    // row and the row covered its lower buttons (ATT). Then the row moves to
    // just right of the column at the same height; on a tall pan, or with
    // the column folded, nothing changes. AetherSDR keeps its zoom grid at
    // the bottom left with no such check (SpectrumWidget.cpp:11542
    // positionZoomButtons [@3a1f59ea]).
    if (!isHidden()) {
        const QRect row(QPoint(x, y), m_zoomStrip->size());
        bool covers = false;
        for (QPushButton* btn : findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (!btn->isHidden() && btn->geometry().translated(pos()).intersects(row)) {
                covers = true;
                break;
            }
        }
        if (covers) {
            x = geometry().right() + 1 + kMargin;
        }
    }
    m_zoomStrip->move(x, y);
    m_zoomStrip->raise();
    // On a short pan the open flyout reaches down to this strip, and the
    // raise above would leave the strip over it.
    raiseOpenFlyout();
}

} // namespace NereusSDR
