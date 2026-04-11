// src/gui/applets/DvkApplet.cpp
#include "DvkApplet.h"
#include "NyiOverlay.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>

namespace NereusSDR {

DvkApplet::DvkApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);

    root->addWidget(appletTitleBar(QStringLiteral("DVK")));

    // --- Voice keyer slots (4 rows) ---
    const QString slotNames[4] = {
        QStringLiteral("Slot 1"), QStringLiteral("Slot 2"),
        QStringLiteral("Slot 3"), QStringLiteral("Slot 4")
    };

    for (int i = 0; i < 4; ++i) {
        auto* row = new QHBoxLayout;
        row->setSpacing(3);

        m_slotLabel[i] = new QLabel(slotNames[i], this);
        m_slotLabel[i]->setFixedWidth(44);
        m_slotLabel[i]->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        row->addWidget(m_slotLabel[i]);

        m_recBtn[i]  = styledButton(QStringLiteral("\u25CF Rec"));
        m_playBtn[i] = styledButton(QStringLiteral("\u25BA Play"));
        m_stopBtn[i] = styledButton(QStringLiteral("\u25A0 Stop"));

        row->addWidget(m_recBtn[i]);
        row->addWidget(m_playBtn[i]);
        row->addWidget(m_stopBtn[i]);

        root->addLayout(row);

        NyiOverlay::markNyi(m_recBtn[i],  QStringLiteral("3I-1"));
        NyiOverlay::markNyi(m_playBtn[i], QStringLiteral("3I-1"));
        NyiOverlay::markNyi(m_stopBtn[i], QStringLiteral("3I-1"));
    }

    // --- Record level gauge: -40 to 0 dB, red@-3 ---
    m_recLevel = new HGauge(this);
    m_recLevel->setRange(-40.0, 0.0);
    m_recLevel->setRedStart(-3.0);
    m_recLevel->setTitle(QStringLiteral("Rec Level"));
    m_recLevel->setUnit(QStringLiteral("dB"));
    root->addWidget(m_recLevel);

    // --- Repeat toggle + interval ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_repeatBtn = greenToggle(QStringLiteral("Repeat"));
        m_repeatBtn->setCheckable(true);
        row->addWidget(m_repeatBtn);

        m_repeatSpin = new QSpinBox(this);
        m_repeatSpin->setRange(1, 999);
        m_repeatSpin->setValue(5);
        m_repeatSpin->setSuffix(QStringLiteral(" s"));
        m_repeatSpin->setFixedWidth(60);
        m_repeatSpin->setStyleSheet(QStringLiteral(
            "QSpinBox {"
            "  background: %1; border: 1px solid %2;"
            "  border-radius: 3px; color: %3; font-size: 10px;"
            "}").arg(Style::kInsetBg, Style::kInsetBorder, Style::kTextPrimary));
        row->addWidget(m_repeatSpin);
        row->addStretch();

        root->addLayout(row);

        NyiOverlay::markNyi(m_repeatBtn,  QStringLiteral("3I-1"));
        NyiOverlay::markNyi(m_repeatSpin, QStringLiteral("3I-1"));
    }

    // --- Semi break-in toggle ---
    m_semiBkBtn = greenToggle(QStringLiteral("Semi BK"));
    m_semiBkBtn->setCheckable(true);
    root->addWidget(m_semiBkBtn);
    NyiOverlay::markNyi(m_semiBkBtn, QStringLiteral("3I-1"));

    // --- WAV import button ---
    m_importBtn = styledButton(QStringLiteral("Import WAV\u2026"));
    root->addWidget(m_importBtn);
    NyiOverlay::markNyi(m_importBtn, QStringLiteral("3I-1"));

    root->addStretch();
}

void DvkApplet::syncFromModel()
{
    // NYI — Phase 3I-1
}

} // namespace NereusSDR
