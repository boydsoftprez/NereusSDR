// src/gui/applets/TxApplet.cpp
//
// TxApplet — complete TX control panel with all 15 controls, all NYI.
// Layout matches AetherSDR TxApplet.cpp. Wiring is deferred to Phase 3I-1.
//
// Control inventory:
//  1.  Fwd Power gauge   — HGauge 0–120 W, redStart 100 W
//  2.  SWR gauge         — HGauge 1.0–3.0, redStart 2.5
//  3.  RF Power slider   + label + value
//  4.  Tune Power slider + label + value
//  5.  MOX button        — checkable, red:checked style
//  6.  TUNE button       — checkable, red:checked + "TUNING..." text change
//  7.  ATU button        — checkable
//  8.  MEM button        — checkable
//  9.  TX Profile combo  — "Default" item
// 10.  Tune mode combo
// 11.  2-Tone test       — checkable
// 12.  PS-A toggle       — checkable, green:checked style
// 13.  DUP               — checkable
// 14.  xPA indicator     — checkable
// 15.  SWR protection LED — QLabel indicator, inactive style

// =================================================================
// src/gui/applets/TxApplet.cpp  (NereusSDR)
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
//                 Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//                 Wiring deferred to Phase 3M.
// =================================================================

#include "TxApplet.h"
#include "NyiOverlay.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/ComboStyle.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace NereusSDR {

TxApplet::TxApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void TxApplet::buildUI()
{
    // Outer layout: zero margins (title bar flush to edges)
    // Body: padded content — matches AetherSDR TxApplet.cpp outer/inner pattern
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(this);
    body->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 2);
    vbox->setSpacing(2);
    outer->addWidget(body);

    // ── 1. Forward Power gauge ── 0–120 W, redStart 100 W ───────────────────
    // Ticks: 0 / 40 / 80 / 100 / 120  (AetherSDR TxApplet.cpp:71)
    auto* fwdGauge = new HGauge(this);
    fwdGauge->setRange(0.0, 120.0);
    fwdGauge->setRedStart(100.0);
    fwdGauge->setYellowStart(100.0); // same as red — no distinct yellow zone
    fwdGauge->setTitle(QStringLiteral("RF Pwr"));
    fwdGauge->setTickLabels({QStringLiteral("0"), QStringLiteral("40"),
                              QStringLiteral("80"), QStringLiteral("100"),
                              QStringLiteral("120")});
    fwdGauge->setAccessibleName(QStringLiteral("Forward power gauge"));
    m_fwdPowerGauge = fwdGauge;
    NyiOverlay::markNyi(fwdGauge, QStringLiteral("Phase 3I-1"));
    vbox->addWidget(fwdGauge);

    // ── 2. SWR gauge ── 1.0–3.0, redStart 2.5 ───────────────────────────────
    // Ticks: 1 / 1.5 / 2.5 / 3  (AetherSDR TxApplet.cpp:77)
    auto* swrGauge = new HGauge(this);
    swrGauge->setRange(1.0, 3.0);
    swrGauge->setRedStart(2.5);
    swrGauge->setYellowStart(2.5);
    swrGauge->setTitle(QStringLiteral("SWR"));
    swrGauge->setTickLabels({QStringLiteral("1"), QStringLiteral("1.5"),
                              QStringLiteral("2.5"), QStringLiteral("3")});
    swrGauge->setAccessibleName(QStringLiteral("SWR gauge"));
    m_swrGauge = swrGauge;
    NyiOverlay::markNyi(swrGauge, QStringLiteral("Phase 3I-1"));
    vbox->addWidget(swrGauge);

    // ── 3. RF Power slider row ───────────────────────────────────────────────
    // Label fixedWidth 62, value fixedWidth 22  (AetherSDR TxApplet.cpp:87–104)
    {
        auto* rfSlider = new QSlider(Qt::Horizontal, this);
        rfSlider->setRange(0, 100);
        rfSlider->setValue(100);
        rfSlider->setAccessibleName(QStringLiteral("RF power"));

        auto* rfValue = new QLabel(QStringLiteral("100"), this);
        rfValue->setFixedWidth(22);
        rfValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rfValue->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));

        m_rfPowerSlider = rfSlider;
        m_rfPowerValue  = rfValue;

        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("RF Power:"), this);
        lbl->setFixedWidth(62);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
        row->addWidget(lbl);

        rfSlider->setFixedHeight(18);
        row->addWidget(rfSlider, 1);
        row->addWidget(rfValue);

        NyiOverlay::markNyi(rfSlider, QStringLiteral("Phase 3I-1"));
        vbox->addLayout(row);
    }

    // ── 4. Tune Power slider row ─────────────────────────────────────────────
    // Label fixedWidth 62, value fixedWidth 22  (AetherSDR TxApplet.cpp:107–128)
    {
        auto* tunSlider = new QSlider(Qt::Horizontal, this);
        tunSlider->setRange(0, 100);
        tunSlider->setValue(10);
        tunSlider->setAccessibleName(QStringLiteral("Tune power"));

        auto* tunValue = new QLabel(QStringLiteral("10"), this);
        tunValue->setFixedWidth(22);
        tunValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tunValue->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));

        m_tunePwrSlider = tunSlider;
        m_tunePwrValue  = tunValue;

        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Tune Pwr:"), this);
        lbl->setFixedWidth(62);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
        row->addWidget(lbl);

        tunSlider->setFixedHeight(18);
        row->addWidget(tunSlider, 1);
        row->addWidget(tunValue);

        NyiOverlay::markNyi(tunSlider, QStringLiteral("Phase 3I-1"));
        vbox->addLayout(row);
    }

    // ── Button row: TUNE + MOX + ATU + MEM (25% each) ─────────────────────
    // Matches AetherSDR TxApplet.cpp:155–203 (single 4-button row)
    // MOX: red active (#cc2222 bg, #ff4444 border, white text)
    // TUNE: red active when tuning, text becomes "TUNING..."
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        const QString btnStyle = Style::buttonBaseStyle()
            + QStringLiteral("QPushButton { padding: 2px; }");
        const QString redChecked = Style::redCheckedStyle();

        m_tuneBtn = new QPushButton(QStringLiteral("TUNE"), this);
        m_tuneBtn->setCheckable(true);
        m_tuneBtn->setFixedHeight(22);
        m_tuneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_tuneBtn->setStyleSheet(btnStyle + redChecked);
        m_tuneBtn->setAccessibleName(QStringLiteral("Tune carrier"));
        NyiOverlay::markNyi(m_tuneBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_tuneBtn, 1);

        m_moxBtn = new QPushButton(QStringLiteral("MOX"), this);
        m_moxBtn->setCheckable(true);
        m_moxBtn->setFixedHeight(22);
        m_moxBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_moxBtn->setStyleSheet(btnStyle + redChecked);
        m_moxBtn->setAccessibleName(QStringLiteral("MOX transmit"));
        NyiOverlay::markNyi(m_moxBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_moxBtn, 1);

        m_atuBtn = new QPushButton(QStringLiteral("ATU"), this);
        m_atuBtn->setCheckable(true);
        m_atuBtn->setFixedHeight(22);
        m_atuBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_atuBtn->setStyleSheet(btnStyle);
        m_atuBtn->setAccessibleName(QStringLiteral("Antenna tuner"));
        NyiOverlay::markNyi(m_atuBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_atuBtn, 1);

        m_memBtn = new QPushButton(QStringLiteral("MEM"), this);
        m_memBtn->setCheckable(true);
        m_memBtn->setFixedHeight(22);
        m_memBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_memBtn->setStyleSheet(btnStyle);
        m_memBtn->setAccessibleName(QStringLiteral("ATU memory"));
        NyiOverlay::markNyi(m_memBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_memBtn, 1);

        vbox->addLayout(row);
    }

    // ── Profile combo row (50%) + tune mode combo (50%) ─────────────────────
    // (AetherSDR TxApplet.cpp:131–153)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        m_profileCombo = new QComboBox(this);
        m_profileCombo->addItem(QStringLiteral("Default"));
        m_profileCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_profileCombo->setFixedHeight(22);
        applyComboStyle(m_profileCombo);
        m_profileCombo->setAccessibleName(QStringLiteral("TX profile"));
        NyiOverlay::markNyi(m_profileCombo, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_profileCombo, 1);

        m_tuneModeCombo = new QComboBox(this);
        m_tuneModeCombo->addItem(QStringLiteral("Auto"));
        m_tuneModeCombo->addItem(QStringLiteral("Manual"));
        m_tuneModeCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_tuneModeCombo->setFixedHeight(22);
        applyComboStyle(m_tuneModeCombo);
        m_tuneModeCombo->setAccessibleName(QStringLiteral("Tune mode"));
        NyiOverlay::markNyi(m_tuneModeCombo, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_tuneModeCombo, 1);

        vbox->addLayout(row);
    }

    // ── Button row 3: 2-Tone + PS-A + DUP ───────────────────────────────────
    // PS-A: green active (#006030 bg) when checked
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        m_twoToneBtn = new QPushButton(QStringLiteral("2-Tone"), this);
        m_twoToneBtn->setCheckable(true);
        m_twoToneBtn->setFixedHeight(22);
        m_twoToneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_twoToneBtn->setAccessibleName(QStringLiteral("2-tone test"));
        NyiOverlay::markNyi(m_twoToneBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_twoToneBtn, 1);

        // PS-A: green when checked — #006030 bg matches AetherSDR APD button
        m_psaBtn = new QPushButton(QStringLiteral("PS-A"), this);
        m_psaBtn->setCheckable(true);
        m_psaBtn->setFixedHeight(22);
        m_psaBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_psaBtn->setStyleSheet(Style::buttonBaseStyle()
            + QStringLiteral("QPushButton:checked {"
                             " background: #006030; border: 1px solid #008040; color: #fff; }"));
        m_psaBtn->setAccessibleName(QStringLiteral("PS-A PureSignal"));
        NyiOverlay::markNyi(m_psaBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_psaBtn, 1);

        m_dupBtn = new QPushButton(QStringLiteral("DUP"), this);
        m_dupBtn->setCheckable(true);
        m_dupBtn->setFixedHeight(22);
        m_dupBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_dupBtn->setAccessibleName(QStringLiteral("Full duplex"));
        NyiOverlay::markNyi(m_dupBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_dupBtn, 1);

        vbox->addLayout(row);
    }

    // ── APD/xPA row: xPA button + inset container ────────────────────────────
    // Inset: fixedHeight 22, bg #0a0a18, border #1e2e3e  (AetherSDR TxApplet.cpp:224–253)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_xpaBtn = new QPushButton(QStringLiteral("xPA"), this);
        m_xpaBtn->setCheckable(true);
        m_xpaBtn->setFixedHeight(22);
        m_xpaBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_xpaBtn->setStyleSheet(Style::buttonBaseStyle() + Style::greenCheckedStyle());
        m_xpaBtn->setAccessibleName(QStringLiteral("External PA indicator"));
        NyiOverlay::markNyi(m_xpaBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_xpaBtn, 2); // ~40%

        // Inset container for SWR protection LED (styled like AetherSDR atuInset)
        auto* inset = new QWidget(this);
        inset->setFixedHeight(22);
        inset->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        inset->setObjectName(QStringLiteral("xpaInset"));
        inset->setStyleSheet(QStringLiteral(
            "#xpaInset { background: %1; border: 1px solid %2; border-radius: 3px; }"
            "#xpaInset QLabel { border: none; background: transparent; }"
        ).arg(Style::kInsetBg, Style::kInsetBorder));

        auto* insetLayout = new QHBoxLayout(inset);
        insetLayout->setContentsMargins(4, 0, 4, 0);
        insetLayout->setSpacing(2);

        // 15. SWR protection LED — inactive: #405060, 9px bold
        // Matches AetherSDR makeIndicator() pattern (TxApplet.cpp:22–27)
        m_swrProtLed = new QLabel(QStringLiteral("SWR Prot"), inset);
        m_swrProtLed->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
        ).arg(Style::kTextInactive));
        m_swrProtLed->setAlignment(Qt::AlignCenter);
        m_swrProtLed->setAccessibleName(QStringLiteral("SWR protection indicator"));
        NyiOverlay::markNyi(m_swrProtLed, QStringLiteral("Phase 3I-1"));
        insetLayout->addWidget(m_swrProtLed);

        row->addWidget(inset, 3); // ~60%
        vbox->addLayout(row);
    }

    vbox->addStretch();
}

void TxApplet::syncFromModel()
{
    // NYI — wired in Phase 3I-1 (Basic SSB TX)
}

} // namespace NereusSDR
