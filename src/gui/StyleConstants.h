// src/gui/StyleConstants.h

// =================================================================
// src/gui/StyleConstants.h  (NereusSDR)
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
//                 Theme palette imported from AetherSDR `src/gui/ComboStyle.h` /
//                 `HGauge.h` / `SliceColors.h` and inline QColor calls
//                 in `MainWindow.cpp` / `VfoWidget.cpp`.
// =================================================================

#pragma once
#include <QString>
#include <QWidget>

namespace NereusSDR::Style {

// Core Theme (from AetherSDR source + STYLEGUIDE.md)
constexpr auto kAppBg           = "#0f0f1a";
constexpr auto kPanelBg         = "#0a0a18";
constexpr auto kTextPrimary     = "#c8d8e8";
constexpr auto kTextSecondary   = "#8090a0";
constexpr auto kTextTertiary    = "#708090";
constexpr auto kTextScale       = "#607080";
constexpr auto kTextInactive    = "#405060";
// NereusSDR-original — used in 5+ places for AGC-T / pan / similar labels;
// sits between kTextSecondary (#8090a0) and kTextScale (#607080).
constexpr auto kLabelMid        = "#8899aa";
constexpr auto kAccent          = "#00b4d8";
constexpr auto kTitleText       = "#8aa8c0";

// Borders & Surfaces
constexpr auto kButtonBg        = "#1a2a3a";
constexpr auto kButtonHover     = "#203040";
constexpr auto kButtonAltHover  = "#204060";
constexpr auto kBorder          = "#205070";
constexpr auto kBorderSubtle    = "#203040";
constexpr auto kInsetBg         = "#0a0a18";
constexpr auto kInsetBorder     = "#1e2e3e";
constexpr auto kGroove          = "#203040";

// Title Bar Gradient
constexpr auto kTitleGradTop    = "#3a4a5a";
constexpr auto kTitleGradMid    = "#2a3a4a";
constexpr auto kTitleGradBot    = "#1a2a38";
constexpr auto kTitleBorder     = "#0a1a28";

// Active/Checked Button States
constexpr auto kGreenBg         = "#006040";
constexpr auto kGreenText       = "#00ff88";
constexpr auto kGreenBorder     = "#00a060";
constexpr auto kBlueBg          = "#0070c0";
constexpr auto kBlueText        = "#ffffff";
constexpr auto kBlueBorder      = "#0090e0";
constexpr auto kBlueHover       = "#0088d8";
constexpr auto kAmberBg         = "#604000";
constexpr auto kAmberText       = "#ffb800";
constexpr auto kAmberBorder     = "#906000";
constexpr auto kAmberWarn       = "#ddbb00";
constexpr auto kRedBg           = "#cc2222";
constexpr auto kRedText         = "#ffffff";
constexpr auto kRedBorder       = "#ff4444";

// Gauge Fill Zones
constexpr auto kGaugeNormal     = "#00b4d8";
constexpr auto kGaugeWarning    = "#ddbb00";
constexpr auto kGaugeDanger     = "#ff4444";
constexpr auto kGaugePeak       = "#ffffff";

// Disabled
constexpr auto kDisabledBg      = "#1a1a2a";
constexpr auto kDisabledText    = "#556070";
constexpr auto kDisabledBorder  = "#2a3040";

// Overlay
constexpr auto kOverlayBtnBg    = "rgba(20, 30, 45, 240)";
constexpr auto kOverlayPanelBg  = "rgba(15, 15, 26, 220)";
constexpr auto kOverlayBtnHover = "rgba(0, 112, 192, 180)";
constexpr auto kOverlayBorder   = "#304050";

// Sizes
constexpr int kTitleBarH        = 16;
constexpr int kButtonH          = 22;
constexpr int kOverlayBtnW      = 68;
constexpr int kOverlayBtnH      = 22;
constexpr int kSliderGrooveH    = 4;
constexpr int kSliderHandleW    = 10;
constexpr int kSliderHandleH    = 10;
constexpr int kAppletPanelW     = 260;

// Status Bar
constexpr int kStatusBarH       = 46;
constexpr auto kStatusBarBg     = "#0a0a14";
constexpr auto kStatusBarBorder = "#203040";
constexpr auto kStatusSep       = "#304050";

// Shared Stylesheet Fragments
inline QString buttonBaseStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1; border: 1px solid %2; border-radius: 3px;"
        "  color: %3; font-size: 10px; font-weight: bold; padding: 2px 4px;"
        "}"
        "QPushButton:hover { background: %4; }"
    ).arg(kButtonBg, kBorder, kTextPrimary, kButtonAltHover);
}

inline QString greenCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kGreenBg, kGreenText, kGreenBorder);
}

inline QString blueCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kBlueBg, kBlueText, kBlueBorder);
}

inline QString amberCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kAmberBg, kAmberText, kAmberBorder);
}

inline QString redCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kRedBg, kRedText, kRedBorder);
}

// DSP toggle — brighter green than the kGreenBg action buttons.
// Used by VfoWidget DSP tab and SpectrumOverlayPanel DSP toggles.
// Source: NereusSDR-original. Distinct semantic from action-button
// "checked" state because DSP toggles communicate "feature on" not
// "action engaged."
constexpr auto kDspToggleBg     = "#1a6030";
constexpr auto kDspToggleBorder = "#20a040";
constexpr auto kDspToggleText   = "#80ff80";

inline QString dspToggleStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kDspToggleBg, kDspToggleText, kDspToggleBorder);
}

inline QString sliderHStyle()
{
    return QStringLiteral(
        "QSlider::groove:horizontal {"
        "  height: 4px; background: %1; border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 10px; height: 10px; margin: -3px 0;"
        "  background: %2; border-radius: 5px;"
        "}"
    ).arg(kGroove, kAccent);
}

inline QString sliderVStyle()
{
    return QStringLiteral(
        "QSlider::groove:vertical {"
        "  width: 4px; background: %1; border-radius: 2px;"
        "}"
        "QSlider::handle:vertical {"
        "  height: 10px; width: 16px; margin: 0 -6px;"
        "  background: %2; border-radius: 5px;"
        "}"
    ).arg(kGroove, kAccent);
}

inline QString insetValueStyle()
{
    return QStringLiteral(
        "QLabel {"
        "  font-size: 10px; background: %1; border: 1px solid %2;"
        "  border-radius: 3px; padding: 1px 2px; color: %3;"
        "}"
    ).arg(kInsetBg, kInsetBorder, kTextPrimary);
}

inline QString titleBarStyle()
{
    return QStringLiteral(
        "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 %1, stop:0.5 %2, stop:1 %3);"
        " border-bottom: 1px solid %4;"
    ).arg(kTitleGradTop, kTitleGradMid, kTitleGradBot, kTitleBorder);
}

// ── Setup Dialog Page Stylesheets ─────────────────────────────────────────────
// Flat string constants used by SetupPage subclasses that build their own
// layouts directly (CatNetwork, Keyboard, Diagnostics pages).

constexpr auto kPageStyle =
    "QWidget { background: #0f0f1a; color: #c8d8e8; }";

constexpr auto kGroupBoxStyle =
    "QGroupBox { border: 1px solid #304050; border-radius: 4px;"
    " margin-top: 8px; padding-top: 12px; font-weight: bold; color: #8aa8c0; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";

constexpr auto kSecondaryLabelStyle =
    "QLabel { color: #8090a0; font-size: 11px; }";

constexpr auto kComboStyle =
    "QComboBox { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
    " selection-background-color: #00b4d8; }";

constexpr auto kCheckBoxStyle =
    "QCheckBox { color: #c8d8e8; font-size: 12px; }"
    "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
    " border: 1px solid #304050; border-radius: 2px; }"
    "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }";

constexpr auto kRadioButtonStyle =
    "QRadioButton { color: #c8d8e8; font-size: 12px; }"
    "QRadioButton::indicator { width: 14px; height: 14px; background: #1a2a3a;"
    " border: 1px solid #304050; border-radius: 7px; }"
    "QRadioButton::indicator:checked { background: #00b4d8; border-color: #00b4d8; }";

constexpr auto kLineEditStyle =
    "QLineEdit { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }";

constexpr auto kSpinBoxStyle =
    "QSpinBox { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }";

constexpr auto kDoubleSpinBoxStyle =
    "QDoubleSpinBox { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }";

// Backwards-compat wrapper for places that prefer a function form.
inline QString doubleSpinBoxStyle() { return QString::fromLatin1(kDoubleSpinBoxStyle); }

constexpr auto kSliderStyle =
    "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #00b4d8; width: 12px;"
    " height: 12px; border-radius: 6px; margin: -4px 0; }";

constexpr auto kButtonStyle =
    "QPushButton { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 3px 10px; }"
    "QPushButton:hover { background: #203040; }"
    "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }";

// Apply the canonical "dark page" stylesheet to a Setup page that lays
// itself out manually (i.e. doesn't inherit the SetupPage::addLabeledX
// helper-based widgets). Replaces the 4 byte-for-byte copies of
// applyDarkStyle() that previously lived in TransmitSetupPages,
// DisplaySetupPages, AppearanceSetupPages, GeneralOptionsPage.
//
// Per docs/architecture/ui-audit-polish-plan.md §A3.
//
// Two intentional drift corrections vs the local copies:
// - QGroupBox border uses kBorder (#205070), not the locals' #203040.
// - QGroupBox padding-top is 12 px, not the locals' 4 px (an 8 px
//   header-position shift, matching the rest of the app).
// All other rules match the locals byte-for-byte so callers see no
// other visual change.
//
// Issue #175 follow-up (2026-05-04): added QRadioButton rules.  Without
// these, calling applyDarkPageStyle() on a SetupPage subclass overwrote
// the QRadioButton::indicator style installed in SetupPage::init() via
// kRadioButtonStyle, leaving radio-button text + indicators in system
// default (black-on-black on dark theme).  Caught on PowerPage's new
// "Tune" group radios.  Mirrors kRadioButtonStyle (lines 241-245 above).
inline void applyDarkPageStyle(QWidget* w)
{
    if (!w) { return; }
    w->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; color: %2; }"
        "QGroupBox { color: %7; font-size: 11px;"
        "  border: 1px solid %3; border-radius: 4px;"
        "  margin-top: 8px; padding-top: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: %2; }"
        "QComboBox { background: %4; color: %2; border: 1px solid %3;"
        "  border-radius: 3px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %4; color: %2;"
        "  selection-background-color: %5; }"
        "QSlider::groove:horizontal { background: %4; height: 4px;"
        "  border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %5; width: 12px;"
        "  margin: -4px 0; border-radius: 6px; }"
        "QSlider::sub-page:horizontal { background: %5; border-radius: 2px; }"
        "QSpinBox, QDoubleSpinBox { background: %4; color: %2;"
        "  border: 1px solid %3; border-radius: 3px; padding: 1px 4px; }"
        "QCheckBox { color: %2; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: %4;"
        "  border: 1px solid %3; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: %5; border-color: %5; }"
        "QRadioButton { color: %2; }"
        "QRadioButton::indicator { width: 14px; height: 14px; background: %4;"
        "  border: 1px solid %3; border-radius: 7px; }"
        "QRadioButton::indicator:checked { background: %5; border-color: %5; }"
        "QLineEdit { background: %4; color: %2; border: 1px solid %3;"
        "  border-radius: 3px; padding: 2px 6px; }"
        "QPushButton { background: %4; color: %2; border: 1px solid %3;"
        "  border-radius: 3px; padding: 3px 12px; }"
        "QPushButton:hover { background: %6; }"
        "QPushButton:pressed { background: %5; color: %1; }"
    ).arg(kAppBg, kTextPrimary, kBorder, kButtonBg, kAccent, kButtonHover, kTextSecondary));
}

// ── TX / RX filter overlay palette ────────────────────────────────────────────
// Plan 4 D9 (Cluster E).  Used by SpectrumWidget::drawTxFilterOverlay() and
// SpectrumWidget::drawTxFilterWaterfallColumn().
// Colours are NereusSDR-original; no Thetis upstream equivalent (Thetis uses
// hard-coded GDI+ brushes without named constants).

// TX filter overlay — translucent orange.
// Customisable via ColorSwatchButton on Setup → Display → TX Display per
// docs/architecture/ui-audit-polish-plan.md §E1.D9b.
constexpr auto kTxFilterOverlayFill   = "rgba(255, 120, 60, 46)";
constexpr auto kTxFilterOverlayBorder = "#ff7833";
constexpr auto kTxFilterOverlayLabel  = "#ffaa70";

// RX filter overlay — translucent cyan.
// Border reuses kAccent (#00b4d8); only the fill variant is new.
// Customisable on Setup → Display → Spectrum Defaults.
constexpr auto kRxFilterOverlayFill   = "rgba(0, 180, 216, 80)";

} // namespace NereusSDR::Style
