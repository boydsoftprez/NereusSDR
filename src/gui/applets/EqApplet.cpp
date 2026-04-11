// src/gui/applets/EqApplet.cpp
//
// Layout from AetherSDR EqApplet.cpp (ten9876/AetherSDR).
// All controls NYI — wired in Phase 3I-3 (TX Processing).

#include "EqApplet.h"
#include "NyiOverlay.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <cmath>

namespace NereusSDR {

// ── ResetButton: 22×22 button that paints a 3/4-circle undo arrow ────────────
// Matches AetherSDR EqApplet.cpp ResetButton exactly.

class ResetButton : public QPushButton {
public:
    explicit ResetButton(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
        setFixedSize(22, 22);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("Reset all bands to 0 dB"));
        setStyleSheet(
            "QPushButton { background-color: #1a2a3a; border: 1px solid #205070; "
            "border-radius: 3px; }"
            "QPushButton:hover { background-color: #203040; }"
            "QPushButton:pressed { background-color: #00b4d8; }");
    }

protected:
    void paintEvent(QPaintEvent* e) override {
        QPushButton::paintEvent(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int cx = width() / 2;
        const int cy = height() / 2;
        const int r  = 6;

        // 3/4 arc: gap at the bottom (6 o'clock), mirrored for "undo" look.
        // Qt angles: 0 = 3 o'clock, counter-clockwise positive, units 1/16 deg.
        // Start at 225 deg (7:30 position), sweep -270 deg (CW) to -45 deg (4:30).
        QPen pen(QColor(0xc8, 0xd8, 0xe8), 1.5);
        p.setPen(pen);
        p.drawArc(cx - r, cy - r, r * 2, r * 2, 225 * 16, -270 * 16);

        // Arrowhead at the end of the arc (225 deg = lower-left).
        const double angle = 225.0 * M_PI / 180.0;
        const double ex = cx + r * std::cos(angle);
        const double ey = cy - r * std::sin(angle);
        p.drawLine(QPointF(ex, ey), QPointF(ex - 4, ey - 1));
        p.drawLine(QPointF(ex, ey), QPointF(ex + 1, ey - 4));
    }
};


// ── Style constants (from AetherSDR EqApplet.cpp + StyleConstants.h) ─────────

static const QString kBtnBase =
    "QPushButton { background-color: #1a2a3a; color: #c8d8e8; "
    "border: 1px solid #205070; border-radius: 3px; font-size: 11px; "
    "font-weight: bold; padding: 2px 4px; }"
    "QPushButton:hover { background-color: #203040; }";

static const QString kGreenActive =
    "QPushButton:checked { background-color: #006040; color: #00ff88; "
    "border: 1px solid #00a060; }";

static const QString kBlueActive =
    "QPushButton:checked { background-color: #0070c0; color: #ffffff; "
    "border: 1px solid #0090e0; }";

// Vertical slider: groove width 4px, handle height 10px width 16px margin 0 -6px.
// From AetherSDR EqApplet.cpp kVSliderStyle.
static constexpr const char* kVSliderStyle =
    "QSlider::groove:vertical { width: 4px; background: #203040; border-radius: 2px; }"
    "QSlider::handle:vertical { height: 10px; width: 16px; margin: 0 -6px;"
    " background: #00b4d8; border-radius: 5px; }";

// 10-band frequency labels: 32/63/125/250/500/1k/2k/4k/8k/16k Hz.
static constexpr int kEqBandCount = 10;
static constexpr const char* kBandLabels[kEqBandCount] = {
    "32", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
};


// ── EqApplet ──────────────────────────────────────────────────────────────────

EqApplet::EqApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void EqApplet::buildUI()
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(4);

    // ── Control row: ON | Reset | RX | TX ────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 1: ON — green checkable 36×22
        m_onBtn = new QPushButton(QStringLiteral("ON"), this);
        m_onBtn->setCheckable(true);
        m_onBtn->setFixedSize(36, 22);
        m_onBtn->setStyleSheet(kBtnBase + kGreenActive);
        m_onBtn->setToolTip(QStringLiteral("Enable equalizer"));
        row->addWidget(m_onBtn);
        NyiOverlay::markNyi(m_onBtn, QStringLiteral("Phase 3I-3"));

        // Control 4: Reset — 22×22, paints 3/4-circle undo icon (pen #c8d8e8, 1.5px)
        auto* resetBtn = new ResetButton(this);
        m_resetBtn = resetBtn;
        row->addWidget(m_resetBtn);
        NyiOverlay::markNyi(m_resetBtn, QStringLiteral("Phase 3I-3"));

        // Control 2: RX — blue checkable 36×22
        m_rxBtn = new QPushButton(QStringLiteral("RX"), this);
        m_rxBtn->setCheckable(true);
        m_rxBtn->setFixedSize(36, 22);
        m_rxBtn->setStyleSheet(kBtnBase + kBlueActive);
        m_rxBtn->setToolTip(QStringLiteral("Show receive equalizer bands"));
        row->addWidget(m_rxBtn);
        NyiOverlay::markNyi(m_rxBtn, QStringLiteral("Phase 3I-3"));

        // Control 3: TX — blue checkable 36×22, starts checked
        m_txBtn = new QPushButton(QStringLiteral("TX"), this);
        m_txBtn->setCheckable(true);
        m_txBtn->setChecked(true);
        m_txBtn->setFixedSize(36, 22);
        m_txBtn->setStyleSheet(kBtnBase + kBlueActive);
        m_txBtn->setToolTip(QStringLiteral("Show transmit equalizer bands"));
        row->addWidget(m_txBtn);
        NyiOverlay::markNyi(m_txBtn, QStringLiteral("Phase 3I-3"));

        row->addStretch();

        vbox->addLayout(row);
    }

    // ── Horizontal divider ────────────────────────────────────────────────────
    {
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setStyleSheet(QStringLiteral("QFrame { color: #203040; }"));
        vbox->addWidget(line);
    }

    // ── Band label row (controls 5): 32/63/125/250/500/1k/2k/4k/8k/16k ──────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(0);

        // Left spacer aligns with dB scale (fixedWidth 20)
        auto* spacerL = new QWidget(this);
        spacerL->setFixedWidth(20);
        row->addWidget(spacerL);

        for (int i = 0; i < kEqBandCount; ++i) {
            auto* lbl = new QLabel(QString::fromLatin1(kBandLabels[i]), this);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet(QStringLiteral("QLabel { color: #8090a0; font-size: 10px; }"));
            row->addWidget(lbl, 1);
        }

        // Right spacer aligns with right dB scale (fixedWidth 20)
        auto* spacerR = new QWidget(this);
        spacerR->setFixedWidth(20);
        row->addWidget(spacerR);

        vbox->addLayout(row);
    }

    // ── Slider area: left dB scale (control 6) | 10 sliders (controls 7-16) | right dB scale ──
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(0);

        // Left dB scale: +10 (#607080, 9px) / 0 (#8090a0, 9px) / -10 (#607080, 9px)
        {
            auto* col = new QVBoxLayout;
            col->setSpacing(0);

            auto* topLbl = new QLabel(QStringLiteral("+10"), this);
            topLbl->setStyleSheet(QStringLiteral("QLabel { color: #607080; font-size: 9px; }"));
            topLbl->setAlignment(Qt::AlignRight | Qt::AlignTop);
            topLbl->setFixedWidth(20);
            col->addWidget(topLbl);

            col->addStretch();

            auto* midLbl = new QLabel(QStringLiteral("0"), this);
            midLbl->setStyleSheet(QStringLiteral("QLabel { color: #8090a0; font-size: 9px; }"));
            midLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            midLbl->setFixedWidth(20);
            col->addWidget(midLbl);

            col->addStretch();

            auto* botLbl = new QLabel(QStringLiteral("-10"), this);
            botLbl->setStyleSheet(QStringLiteral("QLabel { color: #607080; font-size: 9px; }"));
            botLbl->setAlignment(Qt::AlignRight | Qt::AlignBottom);
            botLbl->setFixedWidth(20);
            col->addWidget(botLbl);

            row->addLayout(col);
        }

        // Controls 7–16: 10 vertical band sliders
        for (int i = 0; i < kEqBandCount; ++i) {
            auto* col = new QVBoxLayout;
            col->setSpacing(1);
            col->setContentsMargins(0, 0, 0, 0);

            auto* slider = new QSlider(Qt::Vertical, this);
            slider->setRange(-10, 10);
            slider->setValue(0);
            slider->setTickPosition(QSlider::NoTicks);
            slider->setStyleSheet(QString::fromLatin1(kVSliderStyle));
            slider->setFixedHeight(100);
            slider->setToolTip(
                QStringLiteral("%1 Hz band, \u221210 to +10 dB").arg(
                    QString::fromLatin1(kBandLabels[i])));
            m_sliders[i] = slider;
            col->addWidget(slider, 0, Qt::AlignHCenter);
            NyiOverlay::markNyi(slider, QStringLiteral("Phase 3I-3"));

            // Value label under each slider (#c8d8e8, 9px)
            auto* valLbl = new QLabel(QStringLiteral("0"), this);
            valLbl->setAlignment(Qt::AlignCenter);
            valLbl->setStyleSheet(
                QStringLiteral("QLabel { color: #c8d8e8; font-size: 9px; }"));
            m_valueLabels[i] = valLbl;
            col->addWidget(valLbl);

            row->addLayout(col, 1);
        }

        // Right dB scale: mirror of left
        {
            auto* col = new QVBoxLayout;
            col->setSpacing(0);

            auto* topLbl = new QLabel(QStringLiteral("+10"), this);
            topLbl->setStyleSheet(QStringLiteral("QLabel { color: #607080; font-size: 9px; }"));
            topLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            topLbl->setFixedWidth(20);
            col->addWidget(topLbl);

            col->addStretch();

            auto* midLbl = new QLabel(QStringLiteral("0"), this);
            midLbl->setStyleSheet(QStringLiteral("QLabel { color: #8090a0; font-size: 9px; }"));
            midLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            midLbl->setFixedWidth(20);
            col->addWidget(midLbl);

            col->addStretch();

            auto* botLbl = new QLabel(QStringLiteral("-10"), this);
            botLbl->setStyleSheet(QStringLiteral("QLabel { color: #607080; font-size: 9px; }"));
            botLbl->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
            botLbl->setFixedWidth(20);
            col->addWidget(botLbl);

            row->addLayout(col);
        }

        vbox->addLayout(row);
    }

    // ── Horizontal divider ────────────────────────────────────────────────────
    {
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setStyleSheet(QStringLiteral("QFrame { color: #203040; }"));
        vbox->addWidget(line);
    }

    // ── Preset row: "Preset" label + combo (control 17) ─────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* presetLbl = new QLabel(QStringLiteral("Preset"), this);
        presetLbl->setStyleSheet(
            QStringLiteral("QLabel { color: #8090a0; font-size: 10px; }"));
        row->addWidget(presetLbl);

        // Control 17: Preset combo — Flat / Voice / Music / Custom
        m_presetCombo = new QComboBox(this);
        m_presetCombo->addItem(QStringLiteral("Flat"));
        m_presetCombo->addItem(QStringLiteral("Voice"));
        m_presetCombo->addItem(QStringLiteral("Music"));
        m_presetCombo->addItem(QStringLiteral("Custom"));
        m_presetCombo->setToolTip(QStringLiteral("Equalizer preset"));
        row->addWidget(m_presetCombo, 1);
        NyiOverlay::markNyi(m_presetCombo, QStringLiteral("Phase 3I-3"));

        vbox->addLayout(row);
    }
}

void EqApplet::syncFromModel()
{
    // NYI — Phase 3I-3 will sync band values, enabled state, and preset from model.
}

} // namespace NereusSDR
