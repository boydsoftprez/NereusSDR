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
// =================================================================

#include "SpectrumOverlayPanel.h"

#include "StyleConstants.h"
#include "core/AntennaLabels.h"
#include "core/BoardCapabilities.h"
#include "core/SkuUiProfile.h"
#include "core/StepAttenuatorFacade.h"
#include "gui/AntennaPopupBuilder.h"
#include "models/BandGrid.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

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
#include <QMouseEvent>
#include <QApplication>
#include <QColorDialog>
#include <QColor>
#include <QSignalBlocker>
#include <algorithm>
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
    btn->setStyleSheet(OverlayColors::kMenuBtnNormal);
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

    updateLayout();
}

// ── eventFilter — auto-close flyout on outside click ─────────────────────────

bool SpectrumOverlayPanel::eventFilter(QObject* obj, QEvent* event)
{
    // Auto-close flyout on outside click (from AetherSDR)
    if (event->type() == QEvent::MouseButtonPress && m_activeFlyout) {
        auto* me = static_cast<QMouseEvent*>(event);
        QPoint gp = me->globalPosition().toPoint();
        bool inFlyout = m_activeFlyout->rect().contains(
                            m_activeFlyout->mapFromGlobal(gp));
        bool inButton = m_activeButton &&
                        m_activeButton->rect().contains(
                            m_activeButton->mapFromGlobal(gp));
        bool inPanel  = rect().contains(mapFromGlobal(gp));
        if (!inFlyout && !inButton && !inPanel) {
            hideFlyout();
        }
    }

    // Reposition zoom buttons when parent (spectrum widget) resizes
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        repositionZoomButtons();
    }

    return QWidget::eventFilter(obj, event);
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

// ── hideFlyout — hide active flyout and reset button style ───────────────────

void SpectrumOverlayPanel::hideFlyout()
{
    if (m_activeFlyout) {
        m_activeFlyout->hide();
        m_activeFlyout = nullptr;
    }
    if (m_activeButton) {
        m_activeButton->setStyleSheet(OverlayColors::kMenuBtnNormal);
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
    m_bandFlyout->setStyleSheet(OverlayColors::kPanelStyle);
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
    m_bandFlyout->show();
    m_activeFlyout = m_bandFlyout;
    m_activeButton = bandBtn;
    bandBtn->setStyleSheet(OverlayColors::kMenuBtnActive);
}

// ── ANT flyout ────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildAntFlyout()
{
    m_antFlyout = new QWidget(parentWidget());
    m_antFlyout->setStyleSheet(OverlayColors::kPanelStyle);
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
        auto* row = new QHBoxLayout(m_rxAntRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto* lbl = new QLabel("RX Ant:");
        lbl->setStyleSheet(OverlayColors::kLabelStyle);
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
        auto* row = new QHBoxLayout(m_txAntRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto* lbl = new QLabel("TX Ant:");
        lbl->setStyleSheet(OverlayColors::kLabelStyle);
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
    m_antFlyout->show();
    m_activeFlyout = m_antFlyout;
    m_activeButton = antBtn;
    antBtn->setStyleSheet(OverlayColors::kMenuBtnActive);
}

// ── Display flyout ────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildDisplayFlyout()
{
    m_displayFlyout = new QWidget(parentWidget());
    m_displayFlyout->setStyleSheet(OverlayColors::kPanelStyle);
    m_displayFlyout->hide();

    auto* grid = new QGridLayout(m_displayFlyout);
    grid->setContentsMargins(8, 6, 8, 6);
    grid->setSpacing(4);
    grid->setColumnStretch(2, 1);

    const QString labelStyle =
        "QLabel { color: #8090a0; font-size: 10px; border: none; }";
    const QString valStyle =
        "QLabel { color: #c8d8e8; font-size: 10px; border: none; min-width: 24px; }";
    const QString sliderStyle =
        "QSlider { border: none; }"
        "QSlider::groove:horizontal { height: 4px; background: #203040; border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 10px; height: 10px; margin: -3px 0;"
        " background: #00b4d8; border-radius: 5px; }";

    // grid columns: 0=label, 1=button (optional), 2=slider, 3=value
    grid->setColumnMinimumWidth(1, 22);

    int row = 0;

    // Color Scheme combo
    {
        auto* lbl = new QLabel("Scheme:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0, 1, 2);

        m_colorSchemeCmb = new QComboBox;
        m_colorSchemeCmb->setObjectName(QStringLiteral("colorSchemeCmb"));
        m_colorSchemeCmb->setFixedHeight(18);
        m_colorSchemeCmb->addItems({"Classic", "Phosphor", "Sunrise", "Inverted"});
        grid->addWidget(m_colorSchemeCmb, row, 2, 1, 2);
        connect(m_colorSchemeCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SpectrumOverlayPanel::colorSchemeChanged);
        ++row;
    }

    // WF Gain slider
    {
        auto* lbl = new QLabel("WF Gain:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_wfGainSlider = new QSlider(Qt::Horizontal);
        m_wfGainSlider->setObjectName(QStringLiteral("wfGainSlider"));
        m_wfGainSlider->setRange(0, 100);
        m_wfGainSlider->setValue(50);
        m_wfGainSlider->setStyleSheet(sliderStyle);
        m_wfGainSlider->setToolTip("Waterfall color gain. Higher values brighten weak signals.");
        grid->addWidget(m_wfGainSlider, row, 2);

        m_wfGainLabel = new QLabel("50");
        m_wfGainLabel->setStyleSheet(valStyle);
        m_wfGainLabel->setFixedWidth(28);
        m_wfGainLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(m_wfGainLabel, row, 3);

        connect(m_wfGainSlider, &QSlider::valueChanged, this, [this](int v) {
            m_wfGainLabel->setText(QString::number(v));
            emit wfColorGainChanged(v);
        });
        ++row;
    }

    // Black Level slider
    {
        auto* lbl = new QLabel("Black Lvl:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_wfBlackSlider = new QSlider(Qt::Horizontal);
        m_wfBlackSlider->setObjectName(QStringLiteral("wfBlackSlider"));
        m_wfBlackSlider->setRange(0, 100);
        m_wfBlackSlider->setValue(15);
        m_wfBlackSlider->setStyleSheet(sliderStyle);
        m_wfBlackSlider->setToolTip("Waterfall black level. Increase to darken the noise floor.");
        grid->addWidget(m_wfBlackSlider, row, 2);

        m_wfBlackLabel = new QLabel("15");
        m_wfBlackLabel->setStyleSheet(valStyle);
        m_wfBlackLabel->setFixedWidth(28);
        m_wfBlackLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(m_wfBlackLabel, row, 3);

        connect(m_wfBlackSlider, &QSlider::valueChanged, this, [this](int v) {
            m_wfBlackLabel->setText(QString::number(v));
            emit wfBlackLevelChanged(v);
        });
        ++row;
    }

    // Fill Alpha slider
    {
        auto* lbl = new QLabel("Fill Alpha:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_fillAlphaSlider = new QSlider(Qt::Horizontal);
        m_fillAlphaSlider->setRange(0, 100);
        m_fillAlphaSlider->setValue(70);
        m_fillAlphaSlider->setStyleSheet(sliderStyle);
        m_fillAlphaSlider->setToolTip("Opacity of the spectrum fill area below the trace.");
        grid->addWidget(m_fillAlphaSlider, row, 2);

        m_fillAlphaLabel = new QLabel("70");
        m_fillAlphaLabel->setStyleSheet(valStyle);
        m_fillAlphaLabel->setFixedWidth(28);
        m_fillAlphaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(m_fillAlphaLabel, row, 3);

        connect(m_fillAlphaSlider, &QSlider::valueChanged, this, [this](int v) {
            m_fillAlphaLabel->setText(QString::number(v));
            // B8 fix-up: emit so MainWindow can forward to SpectrumWidget::setFillAlpha.
            // int 0..100 → float 0.0..1.0.
            emit fillAlphaChanged(static_cast<float>(v) / 100.0f);
        });
        ++row;
    }

    // Fill Color button
    {
        auto* lbl = new QLabel("Fill Color:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_fillColorBtn = new QPushButton;
        m_fillColorBtn->setFixedSize(18, 18);
        m_fillColorBtn->setStyleSheet(
            "QPushButton { background: #00e5ff; border: 1px solid #506070; border-radius: 2px; }");
        m_fillColorBtn->setToolTip("Choose spectrum fill color");
        grid->addWidget(m_fillColorBtn, row, 1);

        connect(m_fillColorBtn, &QPushButton::clicked, this, [this]() {
            QColor c = QColorDialog::getColor(QColor("#00e5ff"), this, "FFT Fill Color",
                                              QColorDialog::DontUseNativeDialog);
            if (c.isValid()) {
                m_fillColorBtn->setStyleSheet(
                    QString("QPushButton { background: %1; border: 1px solid #506070;"
                            " border-radius: 2px; }").arg(c.name()));
                emit fillColorChanged(c);  // B8 Task 22
            }
        });
        ++row;
    }

    // Show Grid toggle
    {
        auto* lbl = new QLabel("Grid Lines:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0, 1, 2);

        m_showGridBtn = new QPushButton("On");
        m_showGridBtn->setCheckable(true);
        m_showGridBtn->setChecked(true);
        m_showGridBtn->setFixedSize(36, 18);
        m_showGridBtn->setStyleSheet(overlayDisplayToggleStyle());
        m_showGridBtn->setToolTip("Show or hide frequency and dB grid lines");
        grid->addWidget(m_showGridBtn, row, 2, 1, 2);
        connect(m_showGridBtn, &QPushButton::toggled, this, [this](bool on) {
            m_showGridBtn->setText(on ? "On" : "Off");
            // Parity Task 18: the pan's grid follows (it changed only this
            // label).
            emit gridVisibleChanged(on);
        });
        ++row;
    }

    // Cursor Freq toggle
    {
        auto* lbl = new QLabel("Cursor Freq:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0, 1, 2);

        m_cursorFreqBtn = new QPushButton("Off");
        m_cursorFreqBtn->setCheckable(true);
        m_cursorFreqBtn->setChecked(true);   // default on — matches SpectrumWidget default
        m_cursorFreqBtn->setText("On");
        m_cursorFreqBtn->setFixedSize(36, 18);
        m_cursorFreqBtn->setStyleSheet(overlayDisplayToggleStyle());
        m_cursorFreqBtn->setToolTip("Show frequency at mouse cursor position");
        grid->addWidget(m_cursorFreqBtn, row, 3, Qt::AlignRight);
        connect(m_cursorFreqBtn, &QPushButton::toggled, this, [this](bool on) {
            m_cursorFreqBtn->setText(on ? "On" : "Off");
            emit cursorFreqVisibleChanged(on);  // B8 Task 21
        });
        ++row;
    }

    // Phase 3G-9c: Clarity adaptive tuning — status badge + Re-tune button
    // NOTE: Heat Map / Noise Floor / Weighted Avg toggles were removed (B8
    // Task 23). They were pure label-update theatre with no signal/model state.
    // Noise floor estimation uses NoiseFloorEstimator + ClarityController (the
    // canonical path). Heat Map gradient control is in Setup → Display → Gradient.
    // Weighted Average is configurable in Setup → Display → Average Mode.
    {
        auto* lbl = new QLabel("Clarity:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_clarityBadge = new QLabel("C");
        m_clarityBadge->setFixedSize(18, 18);
        m_clarityBadge->setAlignment(Qt::AlignCenter);
        m_clarityBadge->setToolTip("Clarity status: green = active, amber = paused");
        m_clarityBadge->hide();
        grid->addWidget(m_clarityBadge, row, 1);

        m_clarityRetuneBtn = new QPushButton("Re-tune");
        m_clarityRetuneBtn->setFixedSize(52, 18);
        m_clarityRetuneBtn->setStyleSheet(
            "QPushButton { background: #1a3050; color: #c8d8e8; border: 1px solid #304050;"
            "  border-radius: 2px; font-size: 10px; padding: 0; }"
            "QPushButton:hover { background: #254060; }"
            "QPushButton:pressed { background: #0d2040; }");
        m_clarityRetuneBtn->setToolTip("Force Clarity to re-estimate the noise floor now");
        grid->addWidget(m_clarityRetuneBtn, row, 2, 1, 2, Qt::AlignRight);
        connect(m_clarityRetuneBtn, &QPushButton::clicked,
                this, &SpectrumOverlayPanel::clarityRetuneRequested);
        ++row;
    }

    // Separator
    {
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("QFrame { color: #304050; border: none; }");
        sep->setFixedHeight(2);
        grid->addWidget(sep, row, 0, 1, 4);
        ++row;
    }

    // "More Display Options →" footer link — B8 Task 24: wired to Setup → Display.
    {
        auto* moreLbl = new QLabel("<a href=\"#more\" style=\"color: #00b4d8; "
                                   "text-decoration: none; font-size: 10px;\">"
                                   "More Display Options &#x2192;</a>");
        moreLbl->setTextFormat(Qt::RichText);
        moreLbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
        moreLbl->setToolTip("Open full display settings in Setup → Display");
        grid->addWidget(moreLbl, row, 0, 1, 4);
        // linkActivated fires when the anchor is clicked; ignore the href value
        // and always navigate to the Display page.
        connect(moreLbl, &QLabel::linkActivated, this, [this](const QString&) {
            emit openSetupRequested(QStringLiteral("Display"));
        });
        ++row;
    }

    m_displayFlyout->adjustSize();
}

void SpectrumOverlayPanel::toggleDisplayFlyout()
{
    // Button index 4 (Display)
    QPushButton* dispBtn = m_menuBtns[4];

    if (m_activeFlyout == m_displayFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();

    // adjustSize then position (from AetherSDR toggleDisplayPanel)
    m_displayFlyout->adjustSize();
    m_displayFlyout->move(x() + width(), y());
    m_displayFlyout->raise();
    m_displayFlyout->show();
    m_activeFlyout = m_displayFlyout;
    m_activeButton = dispBtn;
    dispBtn->setStyleSheet(OverlayColors::kMenuBtnActive);
}

// ── VAX flyout ────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildVaxFlyout()
{
    m_vaxFlyout = new QWidget(parentWidget());
    m_vaxFlyout->setStyleSheet(OverlayColors::kPanelStyle);
    m_vaxFlyout->hide();

    auto* vb = new QVBoxLayout(m_vaxFlyout);
    vb->setContentsMargins(6, 6, 6, 6);
    vb->setSpacing(4);

    // VAX Ch combo (from AetherSDR buildDaxPanel)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel("VAX Ch");
        lbl->setStyleSheet(OverlayColors::kLabelStyle);
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
    m_vaxFlyout->show();
    m_activeFlyout = m_vaxFlyout;
    m_activeButton = vaxBtn;
    vaxBtn->setStyleSheet(OverlayColors::kMenuBtnActive);
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
    m_attFlyout->setObjectName(QStringLiteral("attFlyout"));
    m_attFlyout->setStyleSheet(OverlayColors::kPanelStyle);
    m_attFlyout->hide();

    auto* vb = new QVBoxLayout(m_attFlyout);
    vb->setContentsMargins(6, 6, 6, 6);
    vb->setSpacing(4);

    m_attEnableChk = new QCheckBox(QStringLiteral("Step attenuator"));
    m_attEnableChk->setObjectName(QStringLiteral("attEnableCheck"));
    m_attEnableChk->setStyleSheet(OverlayColors::kLabelStyle);
    m_attEnableChk->setToolTip(QStringLiteral("Use the radio's step attenuator"));
    vb->addWidget(m_attEnableChk);

    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    auto* lbl = new QLabel(QStringLiteral("ATT"));
    lbl->setStyleSheet(OverlayColors::kLabelStyle);
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
    m_attReason->setStyleSheet(OverlayColors::kLabelStyle);
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
    m_attFlyout->show();
    m_activeFlyout = m_attFlyout;
    m_activeButton = attBtn;
    attBtn->setStyleSheet(OverlayColors::kMenuBtnActive);
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

    auto makeZBtn = [&](const QString& text) -> QPushButton* {
        auto* btn = new QPushButton(text, m_zoomStrip);
        btn->setFixedSize(kZBtnW, kZBtnH);
        btn->setStyleSheet(zBtnStyle);
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
}

void SpectrumOverlayPanel::repositionZoomButtons()
{
    if (!m_zoomStrip || !parentWidget()) { return; }
    QWidget* spectrum = parentWidget();
    // Place at bottom-left of the waterfall area, above the frequency scale
    // (approximate: leave 32px from bottom for frequency scale bar)
    static constexpr int kFreqScaleH = 32;
    static constexpr int kMargin     = 4;
    int x = kMargin;
    int y = spectrum->height() - kFreqScaleH - m_zoomStrip->height() - kMargin;
    y = std::max(kMargin, y);
    m_zoomStrip->move(x, y);
    m_zoomStrip->raise();
}

// Parity Task 18: show the pan's grid state without reporting it back.
void SpectrumOverlayPanel::setGridVisible(bool on)
{
    if (!m_showGridBtn) { return; }
    QSignalBlocker block(m_showGridBtn);
    m_showGridBtn->setChecked(on);
    m_showGridBtn->setText(on ? "On" : "Off");
}

// ── Clarity status badge (Phase 3G-9c) ───────────────────────────────────────

void SpectrumOverlayPanel::setClarityStatus(bool active, bool paused)
{
    if (!m_clarityBadge) { return; }
    if (!active && !paused) {
        m_clarityBadge->hide();
        return;
    }
    const QString color = paused
        ? QStringLiteral("#d4a017")   // amber
        : QStringLiteral("#22b14c");  // green
    m_clarityBadge->setStyleSheet(
        QStringLiteral("QLabel { background: %1; color: #0f0f1a; "
                        "border-radius: 9px; font-size: 11px; "
                        "font-weight: bold; }").arg(color));
    m_clarityBadge->show();
}

} // namespace NereusSDR
