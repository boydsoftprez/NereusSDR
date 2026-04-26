// src/gui/SpectrumOverlayPanel.cpp
// Left overlay button strip ported from AetherSDR SpectrumOverlayMenu.
// 10 buttons, 5 flyout sub-panels, auto-close on outside click.
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
//                 to slice 0's vaxChannel() with echo prevention. IQ Ch
//                 stays feature-flagged off (design spec §6.7/§11.3 —
//                 audio/SendIqToVax stored-but-not-active). J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

#include "SpectrumOverlayPanel.h"

#include "core/AntennaLabels.h"
#include "core/BoardCapabilities.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
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

namespace NereusSDR {

// ── Constants (from AetherSDR SpectrumOverlayMenu.cpp) ───────────────────────
static constexpr int kBtnW     = 68;
static constexpr int kBtnH     = 22;
static constexpr int kBandBtnW = 48;
static constexpr int kBandBtnH = 26;
static constexpr int kPad      = 2;
static constexpr int kGap      = 2;

// Stylesheets — verbatim from AetherSDR SpectrumOverlayMenu.cpp
static const QString kPanelStyle =
    "QWidget { background: rgba(15, 15, 26, 220); "
    "border: 1px solid #304050; border-radius: 3px; }";

static const QString kLabelStyle =
    "QLabel { background: transparent; border: none; "
    "color: #8aa8c0; font-size: 10px; font-weight: bold; }";

static const QString kSliderStyle =
    "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; "
    "border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #c8d8e8; width: 10px; "
    "margin: -4px 0; border-radius: 5px; }";

static const QString kMenuBtnNormal =
    "QPushButton { background: rgba(20, 30, 45, 240); "
    "border: 1px solid rgba(255, 255, 255, 40); border-radius: 2px; "
    "color: #c8d8e8; font-size: 11px; font-weight: bold; }"
    "QPushButton:hover { background: rgba(0, 112, 192, 180); "
    "border: 1px solid #0090e0; }";

static const QString kMenuBtnActive =
    "QPushButton { background: rgba(0, 112, 192, 180); "
    "border: 1px solid #0090e0; border-radius: 2px; "
    "color: #ffffff; font-size: 11px; font-weight: bold; }";

static const QString kMenuBtnDisabled =
    "QPushButton { background: rgba(20, 30, 45, 180); "
    "border: 1px solid rgba(255, 255, 255, 15); border-radius: 2px; "
    "color: #556070; font-size: 11px; font-weight: bold; }";

static const QString kDspBtnStyle =
    "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
    "border-radius: 2px; color: #c8d8e8; font-size: 10px; font-weight: bold; "
    "padding: 1px 2px; }"
    "QPushButton:checked { background: #1a6030; color: #ffffff; "
    "border: 1px solid #20a040; }"
    "QPushButton:hover { border: 1px solid #0090e0; }";

static const QString kDisplayToggleStyle =
    "QPushButton { background: #1a2a3a; color: #c8d8e8; border: 1px solid #205070;"
    " border-radius: 3px; font-size: 10px; font-weight: bold; padding: 2px 6px; }"
    "QPushButton:hover { background: #204060; }"
    "QPushButton:checked { background: #006040; color: #00ff88; border-color: #00a060; }";

// Helper: create a standard menu button
static QPushButton* makeMenuBtn(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(kBtnW, kBtnH);
    btn->setStyleSheet(kMenuBtnNormal);
    return btn;
}

// Helper: create a disabled NYI menu button
static QPushButton* makeDisabledBtn(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(kBtnW, kBtnH);
    btn->setStyleSheet(kMenuBtnDisabled);
    btn->setEnabled(false);
    btn->setToolTip("Not yet implemented");
    return btn;
}

// ── Band table (from AetherSDR SpectrumOverlayMenu.cpp, reduced to HF + WWV) ─
struct BandEntry {
    const char* label;
    const char* name;
    double      freqHz;
    const char* mode;
};

// Frequencies in Hz (task spec: 1.8e6, 3.5e6, etc.)
static constexpr BandEntry kBands[] = {
    {"160", "160m",  1.8e6,    "LSB"},
    {"80",  "80m",   3.5e6,    "LSB"},
    {"60",  "60m",   5.3e6,    "USB"},
    {"40",  "40m",   7.0e6,    "LSB"},
    {"30",  "30m",  10.1e6,    "DIGU"},
    {"20",  "20m",  14.0e6,    "USB"},
    {"17",  "17m",  18.068e6,  "USB"},
    {"15",  "15m",  21.0e6,    "USB"},
    {"12",  "12m",  24.89e6,   "USB"},
    {"10",  "10m",  28.0e6,    "USB"},
    {"6",   "6m",   50.0e6,    "USB"},
    {"WWV", "WWV",  10.0e6,    "AM"},
};
static constexpr int kBandCount = static_cast<int>(sizeof(kBands) / sizeof(kBands[0]));

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
        auto* btn = makeDisabledBtn("+RX", this);
        btn->setToolTip("Add RX slice (NYI Phase 3F)");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::addRxClicked);
        m_menuBtns.append(btn);
    }

    // Button 3: +TNF (NYI)
    {
        auto* btn = makeDisabledBtn("+TNF", this);
        btn->setToolTip("Add tracking notch filter (NYI)");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::addTnfClicked);
        m_menuBtns.append(btn);
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
        btn->setToolTip("Open antenna and RF gain controls");
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

    // Button 7: VAX — flyout (NYI Phase 3-VAX)
    {
        auto* btn = makeMenuBtn("VAX", this);
        btn->setToolTip("Open VAX audio routing (NYI Phase 3-VAX)");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleVaxFlyout);
        m_menuBtns.append(btn);  // index 5
    }

    // Button 8: ATT (NYI)
    {
        auto* btn = makeDisabledBtn("ATT", this);
        btn->setToolTip("Attenuator control (NYI)");
        m_menuBtns.append(btn);  // index 6
    }

    // Button 9: MNF (NYI)
    {
        auto* btn = makeDisabledBtn("MNF", this);
        btn->setToolTip("Manual notch filter (NYI)");
        m_menuBtns.append(btn);  // index 7
    }

    buildBandFlyout();
    buildAntFlyout();
    buildDisplayFlyout();
    buildVaxFlyout();
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
        m_activeButton->setStyleSheet(kMenuBtnNormal);
        m_activeButton = nullptr;
    }
}

// ── updateLayout ─────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::updateLayout()
{
    m_collapseBtn->setText(m_expanded ? QStringLiteral("\u25c0")   // ◀
                                      : QStringLiteral("\u25b6")); // ▶
    m_collapseBtn->move(kPad, kPad);

    int y = kPad + kBtnH + kGap;
    for (auto* btn : m_menuBtns) {
        btn->setVisible(m_expanded);
        if (m_expanded) {
            btn->move(kPad, y);
            y += kBtnH + kGap;
        }
    }

    int totalH = m_expanded
        ? (kPad + kBtnH + kGap + static_cast<int>(m_menuBtns.size()) * (kBtnH + kGap))
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
    m_bandFlyout->setStyleSheet(kPanelStyle);
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
    // Row 2: 12, 10, 6, WWV
    static constexpr int kCols = 4;
    for (int i = 0; i < kBandCount; ++i) {
        int row = i / kCols;
        int col = i % kCols;

        auto* btn = new QPushButton(QString::fromLatin1(kBands[i].label), m_bandFlyout);
        btn->setFixedSize(kBandBtnW, kBandBtnH);
        btn->setStyleSheet(bandBtnStyle);

        QString bandName = QString::fromLatin1(kBands[i].name);
        double  freqHz   = kBands[i].freqHz;
        QString mode     = QString::fromLatin1(kBands[i].mode);

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
    bandBtn->setStyleSheet(kMenuBtnActive);
}

// ── ANT flyout ────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildAntFlyout()
{
    m_antFlyout = new QWidget(parentWidget());
    m_antFlyout->setStyleSheet(kPanelStyle);
    m_antFlyout->hide();

    auto* vbox = new QVBoxLayout(m_antFlyout);
    vbox->setContentsMargins(6, 6, 6, 6);
    vbox->setSpacing(4);

    constexpr int kLabelW = 52;

    // Phase 3P-I-a T18 — RX/TX antenna rows. Prior to this phase the
    // combos existed but had no currentTextChanged connect — they were
    // zombie controls. Now wired through slice 0 (matches VAX pattern),
    // and both rows are wrapped in a QWidget so setBoardCapabilities
    // can hide them as a unit on HL2/Atlas.
    {
        m_rxAntRow = new QWidget;
        auto* row = new QHBoxLayout(m_rxAntRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto* lbl = new QLabel("RX Ant:");
        lbl->setStyleSheet(kLabelStyle);
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

        // Widget → Model: user picks an antenna → slice 0 setRxAntenna.
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
            if (SliceModel* s = m_radioModel->sliceAt(0)) {
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
        lbl->setStyleSheet(kLabelStyle);
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
            if (SliceModel* s = m_radioModel->sliceAt(0)) {
                s->setTxAntenna(ant);
            }
        });
    }

    // RF Gain slider (-8 to +32 dB) — from AetherSDR buildAntPanel
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel("RF Gain:");
        lbl->setStyleSheet(kLabelStyle);
        lbl->setFixedWidth(kLabelW);
        row->addWidget(lbl);

        m_rfGainSlider = new QSlider(Qt::Horizontal);
        m_rfGainSlider->setRange(-8, 32);
        m_rfGainSlider->setValue(0);
        m_rfGainSlider->setSingleStep(8);
        m_rfGainSlider->setPageStep(8);
        m_rfGainSlider->setTickInterval(8);
        m_rfGainSlider->setTickPosition(QSlider::TicksBelow);
        m_rfGainSlider->setStyleSheet(kSliderStyle);
        m_rfGainSlider->setToolTip("RF Gain: −8 to +32 dB (8 dB steps)");
        row->addWidget(m_rfGainSlider, 1);

        m_rfGainLabel = new QLabel("0 dB");
        m_rfGainLabel->setStyleSheet(kLabelStyle);
        m_rfGainLabel->setFixedWidth(36);
        m_rfGainLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(m_rfGainLabel);
        vbox->addLayout(row);

        connect(m_rfGainSlider, &QSlider::valueChanged, this, [this](int v) {
            // Snap to nearest 8 dB step (from AetherSDR buildAntPanel)
            constexpr int kStep = 8;
            int snapped = qRound(static_cast<double>(v) / kStep) * kStep;
            if (snapped != v) {
                QSignalBlocker sb(m_rfGainSlider);
                m_rfGainSlider->setValue(snapped);
            }
            m_rfGainLabel->setText(QString("%1 dB").arg(snapped));
        });
    }

    // WNB toggle
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        m_wnbBtn = new QPushButton("WNB");
        m_wnbBtn->setCheckable(true);
        m_wnbBtn->setFixedSize(48, 22);
        m_wnbBtn->setStyleSheet(
            "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
            "border-radius: 2px; color: #c8d8e8; font-size: 11px; font-weight: bold; }"
            "QPushButton:checked { background: #0070c0; color: #ffffff; "
            "border: 1px solid #0090e0; }"
            "QPushButton:hover { border: 1px solid #0090e0; }");
        m_wnbBtn->setToolTip("Wideband noise blanker — suppresses impulse noise across panadapter bandwidth");
        row->addWidget(m_wnbBtn);
        row->addStretch();
        vbox->addLayout(row);
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
    antBtn->setStyleSheet(kMenuBtnActive);
}

// ── Display flyout ────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildDisplayFlyout()
{
    m_displayFlyout = new QWidget(parentWidget());
    m_displayFlyout->setStyleSheet(kPanelStyle);
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
        m_showGridBtn->setStyleSheet(kDisplayToggleStyle);
        m_showGridBtn->setToolTip("Show or hide frequency and dB grid lines");
        grid->addWidget(m_showGridBtn, row, 2, 1, 2);
        connect(m_showGridBtn, &QPushButton::toggled, this, [this](bool on) {
            m_showGridBtn->setText(on ? "On" : "Off");
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
        m_cursorFreqBtn->setChecked(false);
        m_cursorFreqBtn->setFixedSize(36, 18);
        m_cursorFreqBtn->setStyleSheet(kDisplayToggleStyle);
        m_cursorFreqBtn->setToolTip("Show frequency at mouse cursor position");
        grid->addWidget(m_cursorFreqBtn, row, 3, Qt::AlignRight);
        connect(m_cursorFreqBtn, &QPushButton::toggled, this, [this](bool on) {
            m_cursorFreqBtn->setText(on ? "On" : "Off");
        });
        ++row;
    }

    // Heat Map toggle
    {
        auto* lbl = new QLabel("Heat Map:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0, 1, 2);

        m_heatMapBtn = new QPushButton("Off");
        m_heatMapBtn->setCheckable(true);
        m_heatMapBtn->setChecked(false);
        m_heatMapBtn->setFixedSize(36, 18);
        m_heatMapBtn->setStyleSheet(
            "QPushButton { background: #1a2a3a; color: #8090a0; border: 1px solid #304050;"
            " border-radius: 3px; font-size: 10px; font-weight: bold; }"
            "QPushButton:checked { background: #006040; color: #00ff88; border: 1px solid #00a060; }");
        m_heatMapBtn->setToolTip("Colors spectrum trace by signal strength");
        grid->addWidget(m_heatMapBtn, row, 2, 1, 2);
        connect(m_heatMapBtn, &QPushButton::toggled, this, [this](bool on) {
            m_heatMapBtn->setText(on ? "On" : "Off");
        });
        ++row;
    }

    // Noise Floor toggle + position slider
    {
        auto* lbl = new QLabel("Noise Floor:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_noiseFloorBtn = new QPushButton("Off");
        m_noiseFloorBtn->setCheckable(true);
        m_noiseFloorBtn->setChecked(false);
        m_noiseFloorBtn->setFixedSize(36, 18);
        m_noiseFloorBtn->setStyleSheet(kDisplayToggleStyle);
        m_noiseFloorBtn->setToolTip("Show noise floor reference line on spectrum");
        grid->addWidget(m_noiseFloorBtn, row, 1);

        m_noiseFloorSlider = new QSlider(Qt::Horizontal);
        m_noiseFloorSlider->setRange(0, 100);
        m_noiseFloorSlider->setValue(75);
        m_noiseFloorSlider->setStyleSheet(sliderStyle);
        m_noiseFloorSlider->setToolTip("Vertical position of noise floor reference line");
        grid->addWidget(m_noiseFloorSlider, row, 2);

        m_noiseFloorLabel = new QLabel("75");
        m_noiseFloorLabel->setStyleSheet(valStyle);
        m_noiseFloorLabel->setFixedWidth(28);
        m_noiseFloorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(m_noiseFloorLabel, row, 3);

        connect(m_noiseFloorBtn, &QPushButton::toggled, this, [this](bool on) {
            m_noiseFloorBtn->setText(on ? "On" : "Off");
            m_noiseFloorSlider->setEnabled(on);
        });
        connect(m_noiseFloorSlider, &QSlider::valueChanged, this, [this](int v) {
            m_noiseFloorLabel->setText(QString::number(v));
        });
        ++row;
    }

    // Weighted Average toggle
    {
        auto* lbl = new QLabel("Weighted Avg:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0, 1, 2);

        m_weightedAvgBtn = new QPushButton("Off");
        m_weightedAvgBtn->setCheckable(true);
        m_weightedAvgBtn->setChecked(false);
        m_weightedAvgBtn->setFixedSize(36, 18);
        m_weightedAvgBtn->setStyleSheet(kDisplayToggleStyle);
        m_weightedAvgBtn->setToolTip("Weight recent FFT frames more heavily for faster signal response");
        grid->addWidget(m_weightedAvgBtn, row, 3, Qt::AlignRight);
        connect(m_weightedAvgBtn, &QPushButton::toggled, this, [this](bool on) {
            m_weightedAvgBtn->setText(on ? "On" : "Off");
        });
        ++row;
    }

    // Phase 3G-9c: Clarity adaptive tuning — status badge + Re-tune button
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

    // "More Display Options →" footer link
    {
        auto* moreLbl = new QLabel("<a href=\"#more\" style=\"color: #00b4d8; "
                                   "text-decoration: none; font-size: 10px;\">"
                                   "More Display Options &#x2192;</a>");
        moreLbl->setTextFormat(Qt::RichText);
        moreLbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
        moreLbl->setToolTip("Open full display settings (NYI)");
        grid->addWidget(moreLbl, row, 0, 1, 4);
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
    dispBtn->setStyleSheet(kMenuBtnActive);
}

// ── VAX flyout ────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildVaxFlyout()
{
    m_vaxFlyout = new QWidget(parentWidget());
    m_vaxFlyout->setStyleSheet(kPanelStyle);
    m_vaxFlyout->hide();

    auto* vb = new QVBoxLayout(m_vaxFlyout);
    vb->setContentsMargins(6, 6, 6, 6);
    vb->setSpacing(4);

    // VAX Ch combo (from AetherSDR buildDaxPanel)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel("VAX Ch");
        lbl->setStyleSheet(kLabelStyle);
        row->addWidget(lbl);
        m_vaxCmb = new QComboBox;
        m_vaxCmb->setObjectName(QStringLiteral("vaxCombo"));
        m_vaxCmb->addItems({"Off", "1", "2", "3", "4"});
        // Disabled until setRadioModel() binds slice 0; retains the
        // pre-3O tooltip in that transient state.
        m_vaxCmb->setEnabled(false);
        m_vaxCmb->setToolTip("VAX channel (not yet bound to a radio model)");
        row->addWidget(m_vaxCmb, 1);
        vb->addLayout(row);

        // Widget → Model: user picks a channel → slice 0 setVaxChannel.
        // Combo index 0 = "Off" = vaxChannel 0; indices 1..4 = VAX 1..4
        // (1:1 mapping, matches the header ordering above).
        connect(m_vaxCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (m_updatingFromModel || !m_radioModel) {
                return;
            }
            SliceModel* s = m_radioModel->sliceAt(0);
            if (s) {
                s->setVaxChannel(idx);
            }
        });
    }

    // IQ Ch combo — reserved for a future phase (design spec §11.3).
    // audio/SendIqToVax is stored-but-not-active per spec §6.7; the combo
    // stays disabled until a consumer of the I/Q-to-VAX path exists.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel("IQ Ch");
        lbl->setStyleSheet(kLabelStyle);
        row->addWidget(lbl);
        m_vaxIqCmb = new QComboBox;
        m_vaxIqCmb->setObjectName(QStringLiteral("vaxIqCombo"));
        m_vaxIqCmb->addItems({"None", "1", "2", "3", "4"});
        m_vaxIqCmb->setEnabled(false);
        m_vaxIqCmb->setToolTip("IQ-stream to VAX \u2014 reserved for future phase (design spec \u00a711.3)");
        row->addWidget(m_vaxIqCmb, 1);
        vb->addLayout(row);
    }

    m_vaxFlyout->setFixedWidth(140);
    m_vaxFlyout->adjustSize();
}

// Phase 3O Sub-Phase 9 Task 9.2c — bind the VAX Ch combo to slice 0.
//
// Widget → Model: the QComboBox::currentIndexChanged lambda installed in
// buildVaxFlyout() calls slice 0's setVaxChannel() (and gates on
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

    m_radioModel = model;

    if (!m_vaxCmb) {
        return;  // defensive: flyout builder has not run yet
    }

    if (!m_radioModel) {
        // Unbound — revert to the pre-3O disabled state.
        m_vaxCmb->setEnabled(false);
        m_vaxCmb->setToolTip("VAX channel (not yet bound to a radio model)");
        return;
    }

    // Always listen for slice 0 add/remove so a later lifecycle event —
    // slice 0 created after bind, or slice 0 destroyed and another slice
    // promoted into index 0 — can re-seat the Model→Widget connection.
    // The lambdas call bindToSliceZero(), which is the same bind logic
    // used below for the initial seating.
    const auto rebindOnSliceZero = [this](int index) {
        if (index != 0) {
            return;
        }
        bindToSliceZero();
    };
    connect(m_radioModel, &RadioModel::sliceAdded,   this, rebindOnSliceZero);
    connect(m_radioModel, &RadioModel::sliceRemoved, this, rebindOnSliceZero);

    bindToSliceZero();
}

void SpectrumOverlayPanel::bindToSliceZero()
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

    SliceModel* s = m_radioModel->sliceAt(0);
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

        m_vaxCmb->setEnabled(true);
        m_vaxCmb->setToolTip("Route this slice's RX audio to a VAX channel");

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
        // No slice 0 — typically the pre-connectToRadio() state, or a
        // transient window during slice teardown. The sliceAdded listener
        // installed by setRadioModel() will call us again when slice 0
        // comes (back) online.
        m_updatingFromModel = true;
        m_vaxCmb->setCurrentIndex(0);
        m_updatingFromModel = false;
        m_vaxCmb->setEnabled(false);
        m_vaxCmb->setToolTip("VAX channel (waiting for slice 0)");
    }
}

// Phase 3P-I-a T18 — repopulate RX/TX antenna combos from BoardCapabilities.
// Called by MainWindow on connect and on currentRadioChanged. Empty port
// list (HL2/Atlas) hides both rows entirely. After repopulating we reseed
// the current value from slice 0 so persisted per-band selections survive
// a reconnect.
void SpectrumOverlayPanel::setBoardCapabilities(const BoardCapabilities& caps)
{
    if (!m_rxAntCmb || !m_txAntCmb) { return; }

    const QStringList labels = antennaLabels(caps);
    const bool show = !labels.isEmpty();

    // Suppress widget→model echo while we clear + refill the combos.
    // Clearing a combo emits currentTextChanged("") and addItems()
    // emits again; without the guard each setVisible-false path would
    // stomp slice 0's antenna setting to empty.
    m_updatingFromModel = true;
    m_rxAntCmb->clear();
    m_txAntCmb->clear();
    m_rxAntCmb->addItems(labels);
    m_txAntCmb->addItems(labels);
    m_updatingFromModel = false;

    if (m_rxAntRow) { m_rxAntRow->setVisible(show); }
    if (m_txAntRow) { m_txAntRow->setVisible(show); }

    // Reseed from slice 0 so the combo label matches the persisted state.
    if (show && m_radioModel) {
        if (SliceModel* s = m_radioModel->sliceAt(0)) {
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
    vaxBtn->setStyleSheet(kMenuBtnActive);
}

void SpectrumOverlayPanel::wheelEvent(QWheelEvent* event) { event->accept(); }
void SpectrumOverlayPanel::mousePressEvent(QMouseEvent* event) { event->accept(); }
void SpectrumOverlayPanel::mouseReleaseEvent(QMouseEvent* event) { event->accept(); }

// ── Waterfall zoom buttons ─────────────────────────────────────────────────────
// Four small buttons [S][B][-][+] docked at bottom-left of the spectrum widget.
// [S] = Segment zoom (fit visible slice passband), [B] = Band zoom (fit whole band),
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

    m_zoomSegBtn->setToolTip(QStringLiteral("Segment zoom — fit visible slice passband"));
    m_zoomBandBtn->setToolTip(QStringLiteral("Band zoom — fit entire amateur band"));
    m_zoomOutBtn->setToolTip(QStringLiteral("Zoom out — increase visible bandwidth"));
    m_zoomInBtn->setToolTip(QStringLiteral("Zoom in — decrease visible bandwidth"));

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
