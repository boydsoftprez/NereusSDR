#pragma once

// =================================================================
// src/gui/SpectrumOverlayMenu.h  (NereusSDR)
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
// =================================================================

#include <QWidget>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>

namespace NereusSDR {

// Right-click overlay menu for SpectrumWidget display settings.
// Tier 2 settings: occasional adjustment (per-band or per-mode).
//
// Self-contained QWidget popup — communicates via signals only.
// No awareness of containers or docking. Designed so it can later
// be wrapped in a ContainerWidget (Phase 3F) without changes.
//
// From plan Step 9 / AetherSDR SpectrumOverlayMenu pattern.
class SpectrumOverlayMenu : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumOverlayMenu(QWidget* parent = nullptr);

    // Set current values (called before showing)
    void setValues(int wfColorGain, int wfBlackLevel, bool autoBlack,
                   int wfScheme, float fillAlpha, bool panFill,
                   bool heatMap, float refLevel, float dynRange,
                   bool ctunEnabled = true);

    // Absolute RF Hz under the cursor at popup time.  The caller sets it
    // just before show(); the Notch section's button carries it back out
    // through notchAddRequested.  Kept separate from setValues because it
    // changes on every right-click while the display knobs above do not.
    void setNotchAddFrequency(double freqHz);

signals:
    void wfColorGainChanged(int gain);
    void wfBlackLevelChanged(int level);
    void wfColorSchemeChanged(int scheme);
    void fillAlphaChanged(float alpha);
    void panFillChanged(bool on);
    void refLevelChanged(float dBm);
    void dynRangeChanged(float dB);
    void ctunChanged(bool enabled);

    // Tunable notch filter: "Add notch here" pressed.  freqHz is absolute
    // RF in Hz, the value last handed to setNotchAddFrequency.  Every
    // frequency crossing the TNF signal boundary is Hz; the only MHz
    // quantity in the stack is SpectrumWidget::NotchMarker::freqMhz.
    void notchAddRequested(double freqHz);

private:
    void buildUI();
    void updateNotchAddLabel();

    QSlider*   m_wfGainSlider{nullptr};
    QSlider*   m_wfBlackSlider{nullptr};
    QComboBox* m_schemeCombo{nullptr};
    QSlider*   m_fillAlphaSlider{nullptr};
    QCheckBox* m_panFillCheck{nullptr};
    QSlider*   m_refLevelSlider{nullptr};
    QSlider*   m_dynRangeSlider{nullptr};
    QLabel*    m_wfGainLabel{nullptr};
    QLabel*    m_wfBlackLabel{nullptr};
    QLabel*    m_fillAlphaLabel{nullptr};
    QLabel*    m_refLevelLabel{nullptr};
    QLabel*    m_dynRangeLabel{nullptr};
    QCheckBox* m_ctunCheck{nullptr};

    // ---- Notch section ----
    QPushButton* m_notchAddButton{nullptr};
    QLabel*      m_notchFreqLabel{nullptr};
    double       m_notchAddFreqHz{0.0};
};

} // namespace NereusSDR
