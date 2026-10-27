// =================================================================
// src/gui/SpectrumOverlayMenu.cpp  (NereusSDR)
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
//                 Overlay-menu pattern from AetherSDR
//                 `src/gui/SpectrumOverlayMenu.{h,cpp}`.
//   2026-08-08 — Final-review fix I4 (3D stacked-trace spectrum plan):
//                 added this header, missing since the file's 2026-04-24
//                 creation even though the sibling .h always carried it.
//                 Newly consequential once Task 13 (same plan) added the
//                 3D VIEW control section below -- 214 lines including
//                 labels, ranges, defaults, and tooltip strings ported
//                 verbatim from AetherSDR `src/gui/SpectrumOverlayMenu.cpp`
//                 [@1872028c]. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - R-IOS-18: 3D Floor says what it does (how far below the
//                 noise floor the 3D surface starts; kept per band).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-02: Restored pinned upstream comments during integration.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-10 - Display pill plan Task 19: the Color Scheme, Color Gain,
//                 Black Level, Fill and 3D VIEW rows are removed (the pan's
//                 Display pill holds them) and a "Display settings..." line
//                 opens the pill. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 - The section titles read WorkspacePreview::roleColourName()
//                 (title-text) with the preview on. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-12: the C-Tune switch on a receiver whose setting is another's
//                 to change is shown disabled with the reason. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "SpectrumOverlayMenu.h"

#include "gui/styles/WorkspacePreview.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

namespace NereusSDR {

namespace {

// The section titles are blue only for decoration: the header text
// (title-text, the application sheet's group box titles) with the
// workspace preview on, today's accent with it off.
QString sectionTitleColour()
{
    return WorkspacePreview::roleColourName("#00b4d8", "title-text");
}

} // namespace

SpectrumOverlayMenu::SpectrumOverlayMenu(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    buildUI();
}

void SpectrumOverlayMenu::buildUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    // Dark theme matching NereusSDR STYLEGUIDE
    // QSS Type selectors match QMetaObject::className(), and Qt rewrites
    // namespace `::` as `--`. The bare `SpectrumOverlayMenu` selector
    // never matched `NereusSDR::SpectrumOverlayMenu`, so the popup
    // background fell through to the system Fusion palette (white on
    // Linux/Ubuntu Yaru). With the namespaced form below the panel
    // gets its dark surface back. Same trap applies to any future
    // top-level QWidget popup in this namespace.
    setStyleSheet(QStringLiteral(
        "NereusSDR--SpectrumOverlayMenu {"
        "  background: #1a2a3a;"
        "  border: 1px solid #2e4e6e;"
        "  border-radius: 4px;"
        "}"
        "QLabel { color: #c8d8e8; font-size: 11px; }"
        "QSlider::groove:horizontal {"
        "  height: 4px; background: #203040; border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 12px; height: 12px; margin: -4px 0;"
        "  background: #00b4d8; border-radius: 6px;"
        "}"
        "QComboBox {"
        "  background: #203040; color: #c8d8e8; border: 1px solid #2e4e6e;"
        "  border-radius: 3px; padding: 2px 6px; font-size: 11px;"
        "}"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView {"
        "  background: #1a2a3a; color: #c8d8e8; selection-background-color: #00b4d8;"
        "}"
        "QCheckBox { color: #c8d8e8; font-size: 11px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }"
        "QPushButton {"
        "  background: #203040; color: #c8d8e8; border: 1px solid #2e4e6e;"
        "  border-radius: 3px; padding: 3px 10px; font-size: 11px;"
        "}"
        "QPushButton:hover { background: #2a4055; border-color: #00b4d8; }"
        "QPushButton:pressed { background: #16232f; }"));

    // Display pill plan Task 19 (design section 7): the Waterfall rows
    // (Color Scheme, Color Gain, Black Level), the Spectrum rows (Fill
    // spectrum trace, Fill Alpha) and the 3D VIEW section that stood in
    // this popup are in the pan's Display pill, opened by the last line
    // below.

    // --- Display Range section ---
    auto* rangeLabel = new QLabel(QStringLiteral("Display Range"), this);
    rangeLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: %1;").arg(sectionTitleColour()));
    layout->addWidget(rangeLabel);

    // Ref level
    auto* refRow = new QHBoxLayout;
    refRow->addWidget(new QLabel(QStringLiteral("Ref Level"), this));
    m_refLevelSlider = new QSlider(Qt::Horizontal, this);
    m_refLevelSlider->setObjectName(QStringLiteral("refLevelSlider"));
    m_refLevelSlider->setRange(-160, 20);
    m_refLevelSlider->setToolTip(QStringLiteral("Maximum signal level shown at top of display (dBm)"));
    m_refLevelLabel = new QLabel(this);
    refRow->addWidget(m_refLevelSlider);
    refRow->addWidget(m_refLevelLabel);
    layout->addLayout(refRow);

    connect(m_refLevelSlider, &QSlider::valueChanged, this, [this](int v) {
        m_refLevelLabel->setText(QStringLiteral("%1 dBm").arg(v));
        emit refLevelChanged(static_cast<float>(v));
    });

    // Dynamic range
    auto* dynRow = new QHBoxLayout;
    dynRow->addWidget(new QLabel(QStringLiteral("Dyn Range"), this));
    m_dynRangeSlider = new QSlider(Qt::Horizontal, this);
    m_dynRangeSlider->setObjectName(QStringLiteral("dynRangeSlider"));
    m_dynRangeSlider->setRange(20, 160);
    m_dynRangeSlider->setToolTip(QStringLiteral("Visible dB range from ref level to bottom of display"));
    m_dynRangeLabel = new QLabel(this);
    dynRow->addWidget(m_dynRangeSlider);
    dynRow->addWidget(m_dynRangeLabel);
    layout->addLayout(dynRow);

    connect(m_dynRangeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_dynRangeLabel->setText(QStringLiteral("%1 dB").arg(v));
        emit dynRangeChanged(static_cast<float>(v));
    });

    // --- Tuning Mode section ---
    auto* tuneLabel = new QLabel(QStringLiteral("Tuning"), this);
    tuneLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: %1; margin-top: 6px;").arg(sectionTitleColour()));
    layout->addWidget(tuneLabel);

    m_ctunCheck = new QCheckBox(QStringLiteral("CTUN (independent pan)"), this);
    m_ctunCheck->setObjectName(QStringLiteral("ctunCheck"));
    m_ctunCheck->setToolTip(QStringLiteral(
        "SmartSDR-style: pan stays fixed, VFO moves within it.\n"
        "Off: traditional (pan follows VFO)."));
    layout->addWidget(m_ctunCheck);

    connect(m_ctunCheck, &QCheckBox::toggled, this, &SpectrumOverlayMenu::ctunChanged);

    // --- Notch section ---
    // Plan decision D-e: the "add a notch here" convenience gesture goes
    // into this popup rather than converting the popup into a QMenu, so no
    // shipped right-click behaviour changes shape.  The panadapter's own
    // Ctrl + right-click stays the primary add gesture; this row is the
    // discoverable one for an operator who does not know the chord.
    auto* notchLabel = new QLabel(QStringLiteral("Notch"), this);
    notchLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: %1; margin-top: 6px;").arg(sectionTitleColour()));
    layout->addWidget(notchLabel);

    auto* notchRow = new QHBoxLayout;
    m_notchAddButton = new QPushButton(QStringLiteral("Add notch here"), this);
    m_notchAddButton->setObjectName(QStringLiteral("notchAddButton"));
    m_notchAddButton->setToolTip(QStringLiteral(
        "Place a notch filter at the frequency you right-clicked.\n"
        "Ctrl + right-click on the panadapter does the same thing;\n"
        "hold Shift as well for a narrow notch."));
    m_notchFreqLabel = new QLabel(this);
    notchRow->addWidget(m_notchAddButton);
    notchRow->addWidget(m_notchFreqLabel);
    notchRow->addStretch();
    layout->addLayout(notchRow);

    connect(m_notchAddButton, &QPushButton::clicked, this, [this]() {
        emit notchAddRequested(m_notchAddFreqHz);
    });
    updateNotchAddLabel();

    // --- Display pill line ---
    // Display pill plan Task 19 (design section 7): one line that opens
    // this pan's Display pill. The popup hides first, so it is not left
    // lying over the pill it opened.
    layout->addSpacing(6);
    auto* displayRow = new QHBoxLayout;
    m_displaySettingsButton = new QPushButton(QStringLiteral("Display settings..."), this);
    m_displaySettingsButton->setObjectName(QStringLiteral("displaySettingsButton"));
    m_displaySettingsButton->setToolTip(QStringLiteral(
        "Open this panadapter's display controls:\n"
        "spectrum, waterfall, 3D view and FFT."));
    displayRow->addWidget(m_displaySettingsButton);
    displayRow->addStretch();
    layout->addLayout(displayRow);

    connect(m_displaySettingsButton, &QPushButton::clicked, this, [this]() {
        hide();
        emit displaySettingsRequested();
    });

    setFixedWidth(280);
}

void SpectrumOverlayMenu::setNotchAddFrequency(double freqHz)
{
    m_notchAddFreqHz = freqHz;
    updateNotchAddLabel();
}

void SpectrumOverlayMenu::updateNotchAddLabel()
{
    if (!m_notchFreqLabel) {
        return;
    }
    m_notchFreqLabel->setText(
        QStringLiteral("%1 MHz").arg(m_notchAddFreqHz / 1.0e6, 0, 'f', 6));
}

void SpectrumOverlayMenu::setValues(float refLevel, float dynRange, bool ctun)
{
    // Block signals to avoid feedback loops while setting initial values
    m_refLevelSlider->blockSignals(true);
    m_refLevelSlider->setValue(static_cast<int>(refLevel));
    m_refLevelLabel->setText(QStringLiteral("%1 dBm").arg(static_cast<int>(refLevel)));
    m_refLevelSlider->blockSignals(false);

    m_dynRangeSlider->blockSignals(true);
    m_dynRangeSlider->setValue(static_cast<int>(dynRange));
    m_dynRangeLabel->setText(QStringLiteral("%1 dB").arg(static_cast<int>(dynRange)));
    m_dynRangeSlider->blockSignals(false);

    m_ctunCheck->blockSignals(true);
    m_ctunCheck->setChecked(ctun);
    m_ctunCheck->blockSignals(false);
}

void SpectrumOverlayMenu::setCtunAvailable(bool available, const QString& heldReason)
{
    const bool held = available && !heldReason.isEmpty();
    m_ctunCheck->setEnabled(available && !held);
    if (held) {
        m_ctunCheck->setToolTip(heldReason);
        return;
    }
    m_ctunCheck->setToolTip(available
        ? QStringLiteral("Keep the receive window fixed while tuning within it")
        : QStringLiteral("C-Tune requires a connected Core with remote C-Tune support"));
}

void SpectrumOverlayMenu::setCtunState(bool pinned)
{
    const bool blocked = m_ctunCheck->blockSignals(true);
    m_ctunCheck->setChecked(pinned);
    m_ctunCheck->blockSignals(blocked);
}

} // namespace NereusSDR
