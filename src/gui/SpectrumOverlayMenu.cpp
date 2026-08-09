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
// =================================================================

#include "SpectrumOverlayMenu.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

namespace NereusSDR {

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

    // --- Waterfall section ---
    auto* wfLabel = new QLabel(QStringLiteral("Waterfall"), this);
    wfLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #00b4d8;"));
    layout->addWidget(wfLabel);

    // Color scheme
    auto* schemeRow = new QHBoxLayout;
    schemeRow->addWidget(new QLabel(QStringLiteral("Color Scheme"), this));
    m_schemeCombo = new QComboBox(this);
    m_schemeCombo->addItems({
        QStringLiteral("Default"),
        QStringLiteral("Enhanced"),
        QStringLiteral("Spectran"),
        QStringLiteral("Black & White")
    });
    schemeRow->addWidget(m_schemeCombo);
    layout->addLayout(schemeRow);

    connect(m_schemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrumOverlayMenu::wfColorSchemeChanged);

    // Color gain
    auto* gainRow = new QHBoxLayout;
    gainRow->addWidget(new QLabel(QStringLiteral("Color Gain"), this));
    m_wfGainSlider = new QSlider(Qt::Horizontal, this);
    m_wfGainSlider->setRange(0, 100);
    m_wfGainSlider->setToolTip(QStringLiteral("Waterfall color intensity — higher values increase contrast"));
    m_wfGainLabel = new QLabel(this);
    gainRow->addWidget(m_wfGainSlider);
    gainRow->addWidget(m_wfGainLabel);
    layout->addLayout(gainRow);

    connect(m_wfGainSlider, &QSlider::valueChanged, this, [this](int v) {
        m_wfGainLabel->setText(QString::number(v));
        emit wfColorGainChanged(v);
    });

    // Black level
    auto* blackRow = new QHBoxLayout;
    blackRow->addWidget(new QLabel(QStringLiteral("Black Level"), this));
    m_wfBlackSlider = new QSlider(Qt::Horizontal, this);
    m_wfBlackSlider->setRange(0, 125);
    m_wfBlackSlider->setToolTip(QStringLiteral("Noise floor suppression — higher hides more noise"));
    m_wfBlackLabel = new QLabel(this);
    blackRow->addWidget(m_wfBlackSlider);
    blackRow->addWidget(m_wfBlackLabel);
    layout->addLayout(blackRow);

    connect(m_wfBlackSlider, &QSlider::valueChanged, this, [this](int v) {
        m_wfBlackLabel->setText(QString::number(v));
        emit wfBlackLevelChanged(v);
    });

    // --- Spectrum section ---
    auto* specLabel = new QLabel(QStringLiteral("Spectrum"), this);
    specLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #00b4d8; margin-top: 6px;"));
    layout->addWidget(specLabel);

    // Pan fill
    m_panFillCheck = new QCheckBox(QStringLiteral("Fill spectrum trace"), this);
    m_panFillCheck->setToolTip(QStringLiteral("Fill the area below the spectrum trace"));
    layout->addWidget(m_panFillCheck);

    connect(m_panFillCheck, &QCheckBox::toggled, this, &SpectrumOverlayMenu::panFillChanged);

    // Fill alpha
    auto* alphaRow = new QHBoxLayout;
    alphaRow->addWidget(new QLabel(QStringLiteral("Fill Alpha"), this));
    m_fillAlphaSlider = new QSlider(Qt::Horizontal, this);
    m_fillAlphaSlider->setRange(0, 100);
    m_fillAlphaSlider->setToolTip(QStringLiteral("Transparency of the spectrum fill area"));
    m_fillAlphaLabel = new QLabel(this);
    alphaRow->addWidget(m_fillAlphaSlider);
    alphaRow->addWidget(m_fillAlphaLabel);
    layout->addLayout(alphaRow);

    connect(m_fillAlphaSlider, &QSlider::valueChanged, this, [this](int v) {
        m_fillAlphaLabel->setText(QStringLiteral("%1%").arg(v));
        emit fillAlphaChanged(static_cast<float>(v) / 100.0f);
    });

    // --- Display Range section ---
    auto* rangeLabel = new QLabel(QStringLiteral("Display Range"), this);
    rangeLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #00b4d8; margin-top: 6px;"));
    layout->addWidget(rangeLabel);

    // Ref level
    auto* refRow = new QHBoxLayout;
    refRow->addWidget(new QLabel(QStringLiteral("Ref Level"), this));
    m_refLevelSlider = new QSlider(Qt::Horizontal, this);
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
    tuneLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #00b4d8; margin-top: 6px;"));
    layout->addWidget(tuneLabel);

    m_ctunCheck = new QCheckBox(QStringLiteral("CTUN (independent pan)"), this);
    m_ctunCheck->setToolTip(QStringLiteral(
        "SmartSDR-style: pan stays fixed, VFO moves within it.\n"
        "Off: traditional — pan follows VFO."));
    layout->addWidget(m_ctunCheck);

    connect(m_ctunCheck, &QCheckBox::toggled, this, &SpectrumOverlayMenu::ctunChanged);

    // --- Notch section ---
    // Plan decision D-e: the "add a notch here" convenience gesture goes
    // into this popup rather than converting the popup into a QMenu, so no
    // shipped right-click behaviour changes shape.  The panadapter's own
    // Ctrl + right-click stays the primary add gesture; this row is the
    // discoverable one for an operator who does not know the chord.
    auto* notchLabel = new QLabel(QStringLiteral("Notch"), this);
    notchLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #00b4d8; margin-top: 6px;"));
    layout->addWidget(notchLabel);

    auto* notchRow = new QHBoxLayout;
    m_notchAddButton = new QPushButton(QStringLiteral("Add notch here"), this);
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

    // --- 3D View section ---
    // From AetherSDR SpectrumOverlayMenu.cpp:1874-1943 [@1872028c]. Labels,
    // ranges, defaults and tooltip wording for the five ported rows below
    // (Spectrum combo, 3D Floor, 3D Gain, 3D Span, and setDssRowSpanSupported's
    // own two tooltip variants) are copied verbatim. 3D Angle and 3D Slice
    // Shadow are NereusSDR-original: the angle slider has no upstream
    // counterpart, and the shadow toggle lives here rather than its own
    // QMenu because upstream's raw QMenu is ITS right-click surface while
    // ours is this QWidget popup -- a second competing surface would show
    // the operator two menus in sequence on one right-click.
    auto* dssLabel = new QLabel(QStringLiteral("3D VIEW"), this);
    dssLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #00b4d8; margin-top: 6px;"));
    layout->addWidget(dssLabel);

    // ── Spectrum render mode (2D waterfall vs 3DSS) ─────────────────────
    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(QStringLiteral("Spectrum:"), this));
    m_renderModeCombo = new QComboBox(this);
    m_renderModeCombo->setObjectName(QStringLiteral("spectrumRenderModeCombo"));  // bridge-addressable
    m_renderModeCombo->addItem(QStringLiteral("2D Waterfall"));       // SpectrumRenderMode::Mode2D
    m_renderModeCombo->addItem(QStringLiteral("3D Stacked Trace"));   // SpectrumRenderMode::Mode3D
    m_renderModeCombo->setToolTip(QStringLiteral(
        "2D: FFT trace + waterfall.\n"
        "3D: perspective stacked-trace spectrum stream."));
    modeRow->addWidget(m_renderModeCombo);
    layout->addLayout(modeRow);

    connect(m_renderModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) { emit spectrumRenderModeChanged(idx); });

    // ── 3D floor depth — how far below the noise floor to surface (dB) ──
    auto* dssFloorRow = new QHBoxLayout;
    dssFloorRow->addWidget(new QLabel(QStringLiteral("3D Floor:"), this));
    m_dssFloorSlider = new QSlider(Qt::Horizontal, this);
    m_dssFloorSlider->setObjectName(QStringLiteral("dssFloorDepthSlider"));
    m_dssFloorSlider->setRange(0, 24);
    m_dssFloorSlider->setValue(6);
    m_dssFloorLabel = new QLabel(QString::number(6), this);
    dssFloorRow->addWidget(m_dssFloorSlider);
    dssFloorRow->addWidget(m_dssFloorLabel);
    layout->addLayout(dssFloorRow);

    connect(m_dssFloorSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_dssFloorLabel) { m_dssFloorLabel->setText(QString::number(v)); }
        emit dssFloorDepthChanged(v);
    });

    // ── 3D gain — how far down the strength range the colormap reaches ──
    auto* dssGainRow = new QHBoxLayout;
    dssGainRow->addWidget(new QLabel(QStringLiteral("3D Gain:"), this));
    m_dssGainSlider = new QSlider(Qt::Horizontal, this);
    m_dssGainSlider->setObjectName(QStringLiteral("dssGainSlider"));
    m_dssGainSlider->setRange(0, 100);
    m_dssGainSlider->setValue(70);
    m_dssGainSlider->setToolTip(QStringLiteral(
        "3D surface colour gain: how far down the signal range the colormap "
        "reaches.\nHigher = colour down toward the noise floor; lower = "
        "colour only on the strongest signals."));
    m_dssGainLabel = new QLabel(QString::number(70), this);
    dssGainRow->addWidget(m_dssGainSlider);
    dssGainRow->addWidget(m_dssGainLabel);
    layout->addLayout(dssGainRow);

    connect(m_dssGainSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_dssGainLabel) { m_dssGainLabel->setText(QString::number(v)); }
        emit dssGainChanged(v);
    });

    // ── 3D span — how far the near rows overhang the plot edges ─────────
    // Caps how much of the radio's offscreen spectrum the surface may use to
    // close the empty wedges beside it. 100 spends everything available, so a
    // source that ships no overhang is unaffected at any setting.
    auto* dssSpanRow = new QHBoxLayout;
    m_dssRowSpanTitle = new QLabel(QStringLiteral("3D Span:"), this);
    dssSpanRow->addWidget(m_dssRowSpanTitle);
    m_dssRowSpanSlider = new QSlider(Qt::Horizontal, this);
    m_dssRowSpanSlider->setObjectName(QStringLiteral("dssRowSpanSlider"));
    m_dssRowSpanSlider->setRange(0, 100);
    m_dssRowSpanSlider->setValue(100);
    m_dssRowSpanSlider->setAccessibleName(tr("3D Span"));
    m_dssRowSpanSlider->setAccessibleDescription(tr(
        "How far the nearest 3D traces overhang the plot edges, using "
        "spectrum from outside the panadapter. 0 keeps the classic "
        "narrowing trapezoid."));
    m_dssRowSpanLabel = new QLabel(QString::number(100), this);
    dssSpanRow->addWidget(m_dssRowSpanSlider);
    dssSpanRow->addWidget(m_dssRowSpanLabel);
    layout->addLayout(dssSpanRow);

    // Tooltip text lives in setDssRowSpanSupported() so the enabled and
    // unavailable wordings cannot drift apart.
    m_dssRowSpanSupported = false;
    setDssRowSpanSupported(true);

    connect(m_dssRowSpanSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_dssRowSpanLabel) {
            m_dssRowSpanLabel->setText(QString::number(v));
        }
        emit dssRowSpanChanged(v);
    });

    // ── 3D angle, viewing elevation (NereusSDR-original) ─────────────────
    auto* dssAngleRow = new QHBoxLayout;
    dssAngleRow->addWidget(new QLabel(QStringLiteral("3D Angle:"), this));
    m_dssAngleSlider = new QSlider(Qt::Horizontal, this);
    m_dssAngleSlider->setObjectName(QStringLiteral("dssAngleSlider"));
    m_dssAngleSlider->setRange(0, 100);
    m_dssAngleSlider->setValue(50);
    m_dssAngleSlider->setAccessibleName(tr("3D Angle"));
    m_dssAngleSlider->setToolTip(tr(
        "Viewing angle for the 3D surface: low looks along the traces "
        "edge-on, high looks down on them.\n"
        "50 is the classic fixed angle."));
    m_dssAngleLabel = new QLabel(QString::number(50), this);
    dssAngleRow->addWidget(m_dssAngleSlider);
    dssAngleRow->addWidget(m_dssAngleLabel);
    layout->addLayout(dssAngleRow);

    connect(m_dssAngleSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_dssAngleLabel) { m_dssAngleLabel->setText(QString::number(v)); }
        emit dssAngleChanged(v);
    });

    // ── 3D slice shadow, perspective decal for slice passbands ──────────
    // Task 12 filled the UBO slots the fragment shader's applySliceShadow
    // reads; this is the control that enables them.
    m_dssSliceShadowChk = new QCheckBox(tr("3D Slice Shadow"), this);
    m_dssSliceShadowChk->setObjectName(QStringLiteral("dssSliceShadowCheck"));
    m_dssSliceShadowChk->setToolTip(tr(
        "Darken each slice's passband onto the 3D surface so it leans back\n"
        "with the perspective, instead of drawing flat on top of it."));
    layout->addWidget(m_dssSliceShadowChk);

    connect(m_dssSliceShadowChk, &QCheckBox::toggled,
            this, &SpectrumOverlayMenu::dssSliceShadowChanged);

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

void SpectrumOverlayMenu::setValues(int wfColorGain, int wfBlackLevel, bool autoBlack,
                                     int wfScheme, float fillAlpha, bool panFill,
                                     bool heatMap, float refLevel, float dynRange,
                                     bool ctunEnabled)
{
    Q_UNUSED(autoBlack);
    Q_UNUSED(heatMap);

    // Block signals to avoid feedback loops while setting initial values
    m_wfGainSlider->blockSignals(true);
    m_wfGainSlider->setValue(wfColorGain);
    m_wfGainLabel->setText(QString::number(wfColorGain));
    m_wfGainSlider->blockSignals(false);

    m_wfBlackSlider->blockSignals(true);
    m_wfBlackSlider->setValue(wfBlackLevel);
    m_wfBlackLabel->setText(QString::number(wfBlackLevel));
    m_wfBlackSlider->blockSignals(false);

    m_schemeCombo->blockSignals(true);
    m_schemeCombo->setCurrentIndex(qBound(0, wfScheme, m_schemeCombo->count() - 1));
    m_schemeCombo->blockSignals(false);

    m_fillAlphaSlider->blockSignals(true);
    m_fillAlphaSlider->setValue(static_cast<int>(fillAlpha * 100.0f));
    m_fillAlphaLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(fillAlpha * 100.0f)));
    m_fillAlphaSlider->blockSignals(false);

    m_panFillCheck->blockSignals(true);
    m_panFillCheck->setChecked(panFill);
    m_panFillCheck->blockSignals(false);

    m_refLevelSlider->blockSignals(true);
    m_refLevelSlider->setValue(static_cast<int>(refLevel));
    m_refLevelLabel->setText(QStringLiteral("%1 dBm").arg(static_cast<int>(refLevel)));
    m_refLevelSlider->blockSignals(false);

    m_dynRangeSlider->blockSignals(true);
    m_dynRangeSlider->setValue(static_cast<int>(dynRange));
    m_dynRangeLabel->setText(QStringLiteral("%1 dB").arg(static_cast<int>(dynRange)));
    m_dynRangeSlider->blockSignals(false);

    m_ctunCheck->blockSignals(true);
    m_ctunCheck->setChecked(ctunEnabled);
    m_ctunCheck->blockSignals(false);
}

// Seeds the 3D VIEW section's six widgets without emitting: the same
// blockSignals(true)/blockSignals(false) idiom setValues() above uses, so
// opening the menu cannot echo the seeded values back out through the
// change signals and rewrite the operator's live SpectrumWidget state.
void SpectrumOverlayMenu::setDssValues(int mode, int floor, int gain, int span,
                                        int angle, bool sliceShadow)
{
    if (m_renderModeCombo) {
        m_renderModeCombo->blockSignals(true);
        m_renderModeCombo->setCurrentIndex(
            qBound(0, mode, m_renderModeCombo->count() - 1));
        m_renderModeCombo->blockSignals(false);
    }

    if (m_dssFloorSlider) {
        m_dssFloorSlider->blockSignals(true);
        m_dssFloorSlider->setValue(floor);
        if (m_dssFloorLabel) { m_dssFloorLabel->setText(QString::number(floor)); }
        m_dssFloorSlider->blockSignals(false);
    }

    if (m_dssGainSlider) {
        m_dssGainSlider->blockSignals(true);
        m_dssGainSlider->setValue(gain);
        if (m_dssGainLabel) { m_dssGainLabel->setText(QString::number(gain)); }
        m_dssGainSlider->blockSignals(false);
    }

    if (m_dssRowSpanSlider) {
        m_dssRowSpanSlider->blockSignals(true);
        m_dssRowSpanSlider->setValue(span);
        if (m_dssRowSpanLabel) { m_dssRowSpanLabel->setText(QString::number(span)); }
        m_dssRowSpanSlider->blockSignals(false);
    }

    if (m_dssAngleSlider) {
        m_dssAngleSlider->blockSignals(true);
        m_dssAngleSlider->setValue(angle);
        if (m_dssAngleLabel) { m_dssAngleLabel->setText(QString::number(angle)); }
        m_dssAngleSlider->blockSignals(false);
    }

    if (m_dssSliceShadowChk) {
        m_dssSliceShadowChk->blockSignals(true);
        m_dssSliceShadowChk->setChecked(sliceShadow);
        m_dssSliceShadowChk->blockSignals(false);
    }
}

// From AetherSDR SpectrumOverlayMenu.cpp:2250-2275 [@1872028c] -- both
// tooltip variants (GPU-mesh-supported / CPU-fallback-unavailable) verbatim.
void SpectrumOverlayMenu::setDssRowSpanSupported(bool supported)
{
    if (!m_dssRowSpanSlider || m_dssRowSpanSupported == supported) {
        return;
    }
    m_dssRowSpanSupported = supported;
    m_dssRowSpanSlider->setEnabled(supported);
    if (m_dssRowSpanLabel) {
        m_dssRowSpanLabel->setEnabled(supported);
    }
    if (m_dssRowSpanTitle) {
        m_dssRowSpanTitle->setEnabled(supported);
    }
    m_dssRowSpanSlider->setToolTip(supported
        ? QStringLiteral(
              "3D surface width: how far the nearest traces overhang the plot "
              "edges,\nusing spectrum the radio sends from outside the "
              "panadapter.\nHigher = the empty wedges beside the surface close "
              "from the front;\n0 = the classic narrowing trapezoid. Limited "
              "by how much\noffscreen spectrum the source actually provides.")
        : QStringLiteral(
              "Unavailable: the 3D view is on the CPU fallback, which always "
              "draws\nthe narrowing trapezoid. The GPU mesh path this control "
              "drives\nneeds float (RGBA16F) textures, which this system's "
              "graphics\ndriver does not report."));
}

} // namespace NereusSDR
