#pragma once
// NereusSDR native mode container widgets; Qt skeleton patterns informed
// by AetherSDR src/gui/VfoWidget.cpp:996-1300 (mode sub-widget blocks).
// DSP behavior bound to S1.6 SliceModel stub API — Thetis-authoritative
// values; no WDSP forwarding yet (Stage 2).

#include <QWidget>
#include <QPointer>
#include <QSpinBox>
#include <QPushButton>

class QLabel;

namespace NereusSDR {

class SliceModel;
class GuardedComboBox;
class ScrollableLabel;
class TriBtn;

// ── FmOptContainer ────────────────────────────────────────────────────────
// Three-row widget for FM-mode options:
//   Row 1: CTCSS tone mode combo + CTCSS tone value combo (Hz)
//   Row 2: Repeater offset spinbox (kHz)
//   Row 3: TX direction (Low / Simplex / High) + Reverse toggle
//
// Binds to SliceModel: fmCtcssMode, fmCtcssValueHz, fmOffsetHz,
//   fmTxMode (FmTxMode::Low/Simplex/High), fmReverse.
class FmOptContainer : public QWidget {
    Q_OBJECT
public:
    explicit FmOptContainer(QWidget* parent = nullptr);
    void setSlice(SliceModel* s);
    void syncFromSlice();  // reads slice state into widgets; safe if m_slice is null

private:
    void buildUi();

    QPointer<SliceModel> m_slice;

    GuardedComboBox* m_toneModeCmb{nullptr};
    GuardedComboBox* m_toneValueCmb{nullptr};
    QSpinBox*        m_offsetKhzSpin{nullptr};  // FM repeater offset in kHz
    QPushButton*     m_txLowBtn{nullptr};       // "Low" repeater direction (TX below RX)
    QPushButton*     m_simplexBtn{nullptr};     // no repeater offset
    QPushButton*     m_txHighBtn{nullptr};      // "High" repeater direction (TX above RX)
    QPushButton*     m_revBtn{nullptr};         // reverse-listen toggle
};

// ── DigOffsetContainer ────────────────────────────────────────────────────
// Single-row widget: TriBtn(−) + ScrollableLabel + TriBtn(+).
// Routes read/write through dspMode(): DIGL → diglOffsetHz,
// DIGU → diguOffsetHz (Thetis uses separate per-sideband offsets).
class DigOffsetContainer : public QWidget {
    Q_OBJECT
public:
    explicit DigOffsetContainer(QWidget* parent = nullptr);
    void setSlice(SliceModel* s);
    void syncFromSlice();  // reads slice state into widget; safe if m_slice is null

private:
    void buildUi();
    int  currentOffsetHz() const;   // routes by dspMode() to digl or digu
    void applyOffset(int hz);       // routes setter the same way

    QPointer<SliceModel> m_slice;

    ScrollableLabel* m_offsetLabel{nullptr};
    TriBtn*          m_minusBtn{nullptr};
    TriBtn*          m_plusBtn{nullptr};
};

// ── RttyMarkShiftContainer ────────────────────────────────────────────────
// Two sub-groups side by side: Mark (frequency) and Shift (spacing).
//   Mark sub-group:  TriBtn(−) + ScrollableLabel + TriBtn(+)  — step 25 Hz
//   Shift sub-group: TriBtn(−) + ScrollableLabel + TriBtn(+)  — step 5 Hz
//
// Binds to SliceModel: rttyMarkHz (default 2295), rttyShiftHz (default 170).
// Step constants from AetherSDR VfoWidget.cpp; these are UX choices, not DSP
// constants, so they are native NereusSDR.
class RttyMarkShiftContainer : public QWidget {
    Q_OBJECT
public:
    explicit RttyMarkShiftContainer(QWidget* parent = nullptr);
    void setSlice(SliceModel* s);
    void syncFromSlice();  // reads slice state into widgets; safe if m_slice is null

private:
    void buildUi();

    QPointer<SliceModel> m_slice;

    ScrollableLabel* m_markLabel{nullptr};
    ScrollableLabel* m_shiftLabel{nullptr};
    TriBtn*          m_markMinus{nullptr};
    TriBtn*          m_markPlus{nullptr};
    TriBtn*          m_shiftMinus{nullptr};
    TriBtn*          m_shiftPlus{nullptr};
};

}  // namespace NereusSDR
