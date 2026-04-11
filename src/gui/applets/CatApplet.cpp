// src/gui/applets/CatApplet.cpp
#include "CatApplet.h"
#include "NyiOverlay.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>

namespace NereusSDR {

static QLabel* makeLed(const QString& name, QWidget* parent)
{
    auto* led = new QLabel(name, parent);
    led->setFixedSize(24, 14);
    led->setAlignment(Qt::AlignCenter);
    led->setStyleSheet(QStringLiteral(
        "QLabel { background: #405060; color: #c8d8e8; border-radius: 2px;"
        " font-size: 8px; font-weight: bold; }"));
    return led;
}

static QLineEdit* makePortEdit(const QString& def, int w, QWidget* parent)
{
    auto* ed = new QLineEdit(def, parent);
    ed->setFixedWidth(w);
    ed->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 3px; color: %3; font-size: 10px;"
        "}").arg(Style::kInsetBg, Style::kInsetBorder, Style::kTextPrimary));
    return ed;
}

CatApplet::CatApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);

    root->addWidget(appletTitleBar(QStringLiteral("CAT")));

    // --- rigctld row: enable + 4 LEDs (A/B/C/D) ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_rigctldBtn = greenToggle(QStringLiteral("rigctld"));
        m_rigctldBtn->setCheckable(true);
        row->addWidget(m_rigctldBtn);

        const QString ledNames[4] = {
            QStringLiteral("A"), QStringLiteral("B"),
            QStringLiteral("C"), QStringLiteral("D")
        };
        for (int i = 0; i < 4; ++i) {
            m_rigctldLed[i] = makeLed(ledNames[i], this);
            row->addWidget(m_rigctldLed[i]);
        }
        row->addStretch();

        root->addLayout(row);
        NyiOverlay::markNyi(m_rigctldBtn, QStringLiteral("3K"));
    }

    // --- TCP CAT row: enable + port ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_tcpCatBtn = greenToggle(QStringLiteral("TCP CAT"));
        m_tcpCatBtn->setCheckable(true);
        row->addWidget(m_tcpCatBtn);

        m_tcpCatPort = makePortEdit(QStringLiteral("4532"), 50, this);
        row->addWidget(m_tcpCatPort);
        row->addStretch();

        root->addLayout(row);

        NyiOverlay::markNyi(m_tcpCatBtn,  QStringLiteral("3K"));
        NyiOverlay::markNyi(m_tcpCatPort, QStringLiteral("3K"));
    }

    // --- TCI row: enable + port + LED ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_tciBtn = greenToggle(QStringLiteral("TCI"));
        m_tciBtn->setCheckable(true);
        row->addWidget(m_tciBtn);

        m_tciPort = makePortEdit(QStringLiteral("40001"), 50, this);
        row->addWidget(m_tciPort);

        m_tciLed = makeLed(QStringLiteral("\u2022"), this);
        row->addWidget(m_tciLed);
        row->addStretch();

        root->addLayout(row);

        NyiOverlay::markNyi(m_tciBtn,  QStringLiteral("3K"));
        NyiOverlay::markNyi(m_tciPort, QStringLiteral("3K"));
    }

    // --- Active connections readout ---
    m_connectLabel = new QLabel(QStringLiteral("Connections: 0"), this);
    m_connectLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
    root->addWidget(m_connectLabel);

    // --- Serial port combo ---
    m_serialCombo = new QComboBox(this);
    m_serialCombo->addItem(QStringLiteral("(none)"));
    applyComboStyle(m_serialCombo);
    root->addWidget(m_serialCombo);
    NyiOverlay::markNyi(m_serialCombo, QStringLiteral("3K"));

    root->addStretch();
}

void CatApplet::syncFromModel()
{
    // NYI — Phase 3K
}

} // namespace NereusSDR
