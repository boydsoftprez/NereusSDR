// src/gui/StyleConstants.h
#pragma once
#include <QString>

namespace NereusSDR::Style {

// Core Theme (from AetherSDR source + STYLEGUIDE.md)
constexpr auto kAppBg           = "#0f0f1a";
constexpr auto kPanelBg         = "#0a0a18";
constexpr auto kTextPrimary     = "#c8d8e8";
constexpr auto kTextSecondary   = "#8090a0";
constexpr auto kTextTertiary    = "#708090";
constexpr auto kTextScale       = "#607080";
constexpr auto kTextInactive    = "#405060";
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
constexpr auto kAmberBg         = "#604000";
constexpr auto kAmberText       = "#ffb800";
constexpr auto kAmberBorder     = "#906000";
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

} // namespace NereusSDR::Style
