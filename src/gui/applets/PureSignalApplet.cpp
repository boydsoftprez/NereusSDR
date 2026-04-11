// src/gui/applets/PureSignalApplet.cpp
#include "PureSignalApplet.h"
#include "NyiOverlay.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

namespace NereusSDR {

PureSignalApplet::PureSignalApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);

    root->addWidget(appletTitleBar(QStringLiteral("PureSignal")));

    // --- Calibration row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_calibrateBtn = styledButton(QStringLiteral("Calibrate"));
        row->addWidget(m_calibrateBtn);

        m_autoCalBtn = greenToggle(QStringLiteral("Auto"));
        row->addWidget(m_autoCalBtn);
        row->addStretch();

        root->addLayout(row);

        NyiOverlay::markNyi(m_calibrateBtn, QStringLiteral("3I-4"));
        NyiOverlay::markNyi(m_autoCalBtn,   QStringLiteral("3I-4"));
    }

    // --- Feedback level gauge ---
    m_feedbackGauge = new HGauge(this);
    m_feedbackGauge->setRange(0.0, 100.0);
    m_feedbackGauge->setRedStart(80.0);
    m_feedbackGauge->setTitle(QStringLiteral("Feedback"));
    root->addWidget(m_feedbackGauge);

    // --- Correction magnitude gauge ---
    m_correctionGauge = new HGauge(this);
    m_correctionGauge->setRange(0.0, 100.0);
    m_correctionGauge->setTitle(QStringLiteral("Correction"));
    root->addWidget(m_correctionGauge);

    // --- Coefficient save/restore + two-tone row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_saveBtn = styledButton(QStringLiteral("Save"));
        row->addWidget(m_saveBtn);

        m_restoreBtn = styledButton(QStringLiteral("Restore"));
        row->addWidget(m_restoreBtn);

        m_twoToneBtn = styledButton(QStringLiteral("2-Tone"));
        row->addWidget(m_twoToneBtn);
        row->addStretch();

        root->addLayout(row);

        NyiOverlay::markNyi(m_saveBtn,     QStringLiteral("3I-4"));
        NyiOverlay::markNyi(m_restoreBtn,  QStringLiteral("3I-4"));
        NyiOverlay::markNyi(m_twoToneBtn,  QStringLiteral("3I-4"));
    }

    // --- Status LEDs row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        const QString ledInactive = QStringLiteral(
            "QLabel { background: #405060; border-radius: 5px; }");

        for (int i = 0; i < 3; ++i) {
            m_led[i] = new QLabel(this);
            m_led[i]->setFixedSize(10, 10);
            m_led[i]->setStyleSheet(ledInactive);
            row->addWidget(m_led[i]);
        }
        row->addStretch();
        root->addLayout(row);
    }

    // --- Info readout ---
    m_iterations   = new QLabel(QStringLiteral("Iterations: 0"), this);
    m_feedbackDb   = new QLabel(QStringLiteral("Feedback: \u2014 dB"), this);
    m_correctionDb = new QLabel(QStringLiteral("Correction: \u2014 dB"), this);

    for (QLabel* lbl : {m_iterations, m_feedbackDb, m_correctionDb}) {
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { font-size: 10px; color: %1; }").arg(Style::kTextSecondary));
        root->addWidget(lbl);
    }

    root->addStretch();
}

void PureSignalApplet::syncFromModel()
{
    // NYI — Phase 3I-4
}

} // namespace NereusSDR
