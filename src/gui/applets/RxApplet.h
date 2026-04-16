// src/gui/applets/RxApplet.h
// Per-slice RX controls applet — all 17 controls from spec.
// Tier 1 controls wired to SliceModel. Tier 2/3 controls present but NYI.
//
// Layout adapted from AetherSDR RxApplet.cpp.
#pragma once

#include "AppletWidget.h"
#include "gui/widgets/TriBtn.h"

#include <QPushButton>
#include <QStringList>
#include <QVector>

class QComboBox;
class QPaintEvent;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QSlider;

namespace NereusSDR {

class FilterPassbandWidget;
class SliceModel;

// RxApplet — per-slice RX controls applet.
//
// Controls (17 total):
//  1.  Slice badge (A/B/C/D)
//  2.  Lock button (checkable, NYI)
//  3.  RX antenna button (Tier 1 wired)
//  4.  TX antenna button (Tier 1 wired)
//  5.  Filter width label
//  6.  Mode combo (Tier 1 wired)
//  7.  Filter preset buttons × 10 (Tier 1 wired)
//  8.  FilterPassband widget (ported from AetherSDR, Tier 1 wired)
//  9.  AGC combo (Tier 1 wired)
//  10. AGC threshold slider (NYI — setAgcThreshold not in SliceModel yet)
//  11. AF gain slider (Tier 1 wired)
//  12. Mute button (NYI — setMuted not in SliceModel yet)
//  13. Audio pan slider (NYI)
//  14. Squelch toggle + slider (NYI)
//  15. RIT toggle + offset + zero (NYI)
//  16. XIT toggle + offset + zero (NYI)
//  17. Step size + up/down (NYI)
class RxApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit RxApplet(SliceModel* slice, RadioModel* model,
                      QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("rx"); }
    QString appletTitle() const override { return QStringLiteral("RX"); }
    void    syncFromModel() override;

    // Attach to a different slice (or nullptr to detach).
    void setSlice(SliceModel* slice);

    // Set the slice letter badge (0=A, 1=B, 2=C, 3=D)
    void setSliceIndex(int idx);

    // Set the antenna list shown in the RX/TX antenna menus.
    void setAntennaList(const QStringList& ants);

private:
    void buildUi();
    void connectSlice(SliceModel* s);
    void disconnectSlice(SliceModel* s);
    void updateFilterLabel();
    void rebuildFilterButtons();
    void updateFilterButtons();
    void applyFilterPreset(int widthHz);

    static QString formatFilterWidth(int low, int high);

    // ── Model ──────────────────────────────────────────────────────────────
    SliceModel* m_slice = nullptr;
    QStringList m_antList{QStringLiteral("ANT1"), QStringLiteral("ANT2")};

    // Filter preset widths by mode (USB default)
    QVector<int> m_filterWidths{1800, 2100, 2400, 2700, 2900, 3300,
                                 500,  800, 1200, 1600};

    // ── Row 1: badge | lock | rx ant | tx ant | filter label ──────────────
    QLabel*      m_sliceBadge     = nullptr;   // Control 1
    QPushButton* m_lockBtn        = nullptr;   // Control 2
    QPushButton* m_rxAntBtn       = nullptr;   // Control 3
    QPushButton* m_txAntBtn       = nullptr;   // Control 4
    QLabel*      m_filterWidthLbl = nullptr;   // Control 5

    // ── Mode combo ────────────────────────────────────────────────────────
    QComboBox*   m_modeCombo      = nullptr;   // Control 6

    // ── Left column ───────────────────────────────────────────────────────
    // Control 17: Step size row
    TriBtn*      m_stepDown       = nullptr;
    QLabel*      m_stepLabel      = nullptr;
    TriBtn*      m_stepUp         = nullptr;

    // Control 7: Filter preset grid (10 buttons, 3×4 layout)
    QVector<QPushButton*> m_filterBtns;
    QWidget*     m_filterContainer = nullptr;
    QGridLayout* m_filterGrid      = nullptr;

    // Control 8: FilterPassband (ported from AetherSDR FilterPassbandWidget)
    FilterPassbandWidget* m_filterPassband = nullptr;

    // ── Right column ──────────────────────────────────────────────────────
    // Controls 11 + 12: Mute + AF gain
    QPushButton* m_muteBtn     = nullptr;   // Control 12
    QSlider*     m_afSlider    = nullptr;   // Control 11

    // Control 13: Audio pan
    QSlider*     m_panSlider   = nullptr;

    // Control 14: Squelch
    QPushButton* m_sqlBtn      = nullptr;
    QSlider*     m_sqlSlider   = nullptr;

    // Controls 9 + 10: AGC
    QComboBox*   m_agcCombo    = nullptr;   // Control 9
    QSlider*     m_agcTSlider  = nullptr;   // Control 10

    // Control 15: RIT
    QPushButton* m_ritOnBtn    = nullptr;
    QLabel*      m_ritLabel    = nullptr;
    QPushButton* m_ritZero     = nullptr;
    TriBtn*      m_ritMinus    = nullptr;
    TriBtn*      m_ritPlus     = nullptr;

    // Control 16: XIT
    QPushButton* m_xitOnBtn    = nullptr;
    QLabel*      m_xitLabel    = nullptr;
    QPushButton* m_xitZero     = nullptr;
    TriBtn*      m_xitMinus    = nullptr;
    TriBtn*      m_xitPlus     = nullptr;
};

} // namespace NereusSDR
