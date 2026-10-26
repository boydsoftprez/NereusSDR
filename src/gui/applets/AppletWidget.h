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
//   2026-10-24 - createFloatingWindow / dockFloatingWindow and
//                floatingWindowDocked: an applet may pop out into a window of
//                its own (Roundtable plan Task 13). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
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
//   2026-10-16 - setUnavailableReason: an applet whose feature is off is
//                 grayed with its reason, never hidden. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-25 - NEREUS_GUI_EXPORT, as on the applets that derive from it:
//                 without it a test outside the GUI DLL took the address of
//                 floatingWindowDocked through MinGW's auto-import thunk, so
//                 QMetaMethod::fromSignal did not match it and a QSignalSpy
//                 on it was not valid on Windows. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once
#include "gui/NereusGuiExport.h"

#include <QWidget>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

#include <memory>

class QSlider;
class QPushButton;
class QFrame;
class QHBoxLayout;

namespace NereusSDR {

class RadioModel;

class NEREUS_GUI_EXPORT AppletWidget : public QWidget {
    Q_OBJECT
public:
    explicit AppletWidget(RadioModel* model, QWidget* parent = nullptr);
    ~AppletWidget() override = default;

    virtual QString appletId() const = 0;
    virtual QString appletTitle() const = 0;
    virtual QIcon appletIcon() const;
    virtual void syncFromModel() = 0;
    /// True if the panel should offer a pop-out button so the applet can
    /// float in its own window (AppletPanelWidget::floatApplet).
    virtual bool canFloat() const { return false; }
    /// An applet with a window of its own to pop out into returns it here
    /// (shown by AppletPanelWidget::floatApplet, in place of an
    /// AppletFloatingWindow); nullptr, the default, floats the applet
    /// itself. The window stays the applet's: it docks itself
    /// (dockFloatingWindow) and says so with floatingWindowDocked.
    virtual QWidget* createFloatingWindow() { return nullptr; }
    /// Puts back what createFloatingWindow() popped out; emits
    /// floatingWindowDocked when it did.
    virtual void dockFloatingWindow() {}

signals:
    /// The window createFloatingWindow() returned has docked.
    void floatingWindowDocked();

public:

    // Disabled, never hidden: while `reason` is not empty the applet is
    // grayed and shows the reason at its top (as the Rotor applet does);
    // an empty reason gives it back. Set by the container that shows it.
    void setUnavailableReason(const QString& reason);
    QString unavailableReason() const { return m_unavailableReason; }

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

private:
    QString m_unavailableReason;
    QLabel* m_unavailableLabel = nullptr;
    QWidget* m_titleBar = nullptr;   // the last appletTitleBar() made
    bool m_grayedUnavailable = false; // setEnabled(false) came from here
    QString m_ownToolTip;            // the applet's tooltip before the reason
    QString m_reasonToolTip;         // the reason last set as the tooltip
    // Keeps the reason line across the top of an applet with no layout.
    class UnavailableLabelTracker : public QObject {
    public:
        UnavailableLabelTracker(QWidget* applet, QLabel* label);
        bool eventFilter(QObject* watched, QEvent* event) override;
        void fit();
    private:
        QWidget* m_applet;
        QLabel* m_label;
    };
    std::unique_ptr<UnavailableLabelTracker> m_unavailableTracker;
    void placeUnavailableLabel();
};

} // namespace NereusSDR
