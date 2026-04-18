// =================================================================
// src/gui/applets/AppletWidget.h  (NereusSDR)
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
//   2026-04-18 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Shared applet base class. Title-bar gradient, slider-row,
//                 and toggle-button helpers extracted from the AetherSDR
//                 `src/gui/AppletPanel.{h,cpp}` styling pattern (already
//                 registered for AppletPanelWidget in Bucket A); this base
//                 class hoists that styling up so every NereusSDR applet
//                 (Cat/Cwx/Dvk/Tuner/Eq/Fm/Tx/Rx/PhoneCw/Diversity/Digital/
//                 PureSignal) inherits identical visuals without
//                 duplicating the code.
// =================================================================

#pragma once
#include <QWidget>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

class QSlider;
class QPushButton;
class QFrame;
class QHBoxLayout;

namespace NereusSDR {

class RadioModel;

class AppletWidget : public QWidget {
    Q_OBJECT
public:
    explicit AppletWidget(RadioModel* model, QWidget* parent = nullptr);
    ~AppletWidget() override = default;

    virtual QString appletId() const = 0;
    virtual QString appletTitle() const = 0;
    virtual QIcon appletIcon() const;
    virtual void syncFromModel() = 0;

protected:
    // Call from subclass constructor to add the gradient title bar
    QWidget* appletTitleBar(const QString& text);

    // Create standard slider row: [Label(labelWidth) | Slider(stretch) | ValueLabel(36px)]
    QHBoxLayout* sliderRow(const QString& label, QSlider* slider,
                           QLabel* valueLabel, int labelWidth = 62);

    // Button factories with checked-state colors from StyleConstants
    QPushButton* styledButton(const QString& text, int w = -1, int h = 22);
    QPushButton* greenToggle(const QString& text, int w = -1, int h = 22);
    QPushButton* blueToggle(const QString& text, int w = -1, int h = 22);
    QPushButton* amberToggle(const QString& text, int w = -1, int h = 22);

    // Inset value display label (dark background, subtle border)
    QLabel* insetValue(const QString& text = {}, int w = 40);

    // Horizontal divider line
    QFrame* divider();

    RadioModel* m_model = nullptr;
    bool m_updatingFromModel = false;
};

} // namespace NereusSDR
