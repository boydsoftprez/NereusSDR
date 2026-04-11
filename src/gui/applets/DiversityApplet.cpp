// src/gui/applets/DiversityApplet.cpp
#include "DiversityApplet.h"
#include "NyiOverlay.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>

namespace NereusSDR {

DiversityApplet::DiversityApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(appletTitleBar(QStringLiteral("Diversity")));

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1+2: Enable toggle + source selector ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 1: diversity enable (green toggle)
        m_enableBtn = greenToggle(QStringLiteral("DIV"));
        m_enableBtn->setCheckable(true);
        row->addWidget(m_enableBtn);

        // Control 2: source selector — RX1 / RX2
        m_sourceCombo = new QComboBox(this);
        m_sourceCombo->addItems({QStringLiteral("RX1"), QStringLiteral("RX2")});
        applyComboStyle(m_sourceCombo);
        row->addWidget(m_sourceCombo);
        row->addStretch();

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_enableBtn,   QStringLiteral("3F"));
        NyiOverlay::markNyi(m_sourceCombo, QStringLiteral("3F"));
    }

    // --- Control 3: Gain slider (-20..+20 dB) ---
    m_gainSlider = new QSlider(Qt::Horizontal, this);
    m_gainSlider->setRange(-20, 20);
    m_gainSlider->setValue(0);
    m_gainValue  = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("Gain"), m_gainSlider, m_gainValue));
    NyiOverlay::markNyi(m_gainSlider, QStringLiteral("3F"));

    // --- Control 4: Phase slider (0..360°) ---
    m_phaseSlider = new QSlider(Qt::Horizontal, this);
    m_phaseSlider->setRange(0, 360);
    m_phaseSlider->setValue(0);
    m_phaseValue  = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("Phase"), m_phaseSlider, m_phaseValue));
    NyiOverlay::markNyi(m_phaseSlider, QStringLiteral("3F"));

    // --- Control 5: ESC mode selector — Off / Auto / Manual ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("ESC"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(62);
        row->addWidget(lbl);

        m_escCombo = new QComboBox(this);
        m_escCombo->addItems({QStringLiteral("Off"),
                              QStringLiteral("Auto"),
                              QStringLiteral("Manual")});
        applyComboStyle(m_escCombo);
        row->addWidget(m_escCombo, 1);

        vbox->addLayout(row);
        NyiOverlay::markNyi(m_escCombo, QStringLiteral("3F"));
    }

    // --- Control 6: R2 Gain + Phase sliders ---
    m_r2GainSlider = new QSlider(Qt::Horizontal, this);
    m_r2GainSlider->setRange(-20, 20);
    m_r2GainSlider->setValue(0);
    m_r2GainValue  = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("R2 Gain"), m_r2GainSlider, m_r2GainValue));
    NyiOverlay::markNyi(m_r2GainSlider, QStringLiteral("3F"));

    m_r2PhaseSlider = new QSlider(Qt::Horizontal, this);
    m_r2PhaseSlider->setRange(0, 360);
    m_r2PhaseSlider->setValue(0);
    m_r2PhaseValue  = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("R2 Phase"), m_r2PhaseSlider, m_r2PhaseValue));
    NyiOverlay::markNyi(m_r2PhaseSlider, QStringLiteral("3F"));

    vbox->addStretch();
    root->addWidget(body);
}

void DiversityApplet::syncFromModel()
{
    // NYI — Phase 3F
}

} // namespace NereusSDR
