// src/gui/applets/CwxApplet.cpp
#include "CwxApplet.h"
#include "NyiOverlay.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>

namespace NereusSDR {

CwxApplet::CwxApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);

    root->addWidget(appletTitleBar(QStringLiteral("CWX")));

    // --- Text input ---
    m_textEdit = new QTextEdit(this);
    m_textEdit->setFixedHeight(60);
    m_textEdit->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 3px; color: %3; font-size: 11px;"
        "}").arg(Style::kInsetBg, Style::kInsetBorder, Style::kTextPrimary));
    root->addWidget(m_textEdit);
    NyiOverlay::markNyi(m_textEdit, QStringLiteral("3I-2"));

    // --- Send button ---
    m_sendBtn = styledButton(QStringLiteral("Send"));
    root->addWidget(m_sendBtn);
    NyiOverlay::markNyi(m_sendBtn, QStringLiteral("3I-2"));

    // --- Speed override slider row ---
    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider->setRange(1, 60);
    m_speedSlider->setValue(20);
    m_speedValue  = insetValue(QStringLiteral("20"));
    root->addLayout(sliderRow(QStringLiteral("Speed"), m_speedSlider, m_speedValue));
    NyiOverlay::markNyi(m_speedSlider, QStringLiteral("3I-2"));

    // --- Memory buttons M1–M6 ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        const QString labels[6] = {
            QStringLiteral("M1"), QStringLiteral("M2"), QStringLiteral("M3"),
            QStringLiteral("M4"), QStringLiteral("M5"), QStringLiteral("M6")
        };

        for (int i = 0; i < 6; ++i) {
            m_memBtn[i] = styledButton(labels[i], 32);
            row->addWidget(m_memBtn[i]);
            NyiOverlay::markNyi(m_memBtn[i], QStringLiteral("3I-2"));
        }
        row->addStretch();
        root->addLayout(row);
    }

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

        NyiOverlay::markNyi(m_repeatBtn,  QStringLiteral("3I-2"));
        NyiOverlay::markNyi(m_repeatSpin, QStringLiteral("3I-2"));
    }

    // --- Keyboard-to-CW toggle ---
    m_kbCwBtn = greenToggle(QStringLiteral("KB\u2192CW"));
    m_kbCwBtn->setCheckable(true);
    root->addWidget(m_kbCwBtn);
    NyiOverlay::markNyi(m_kbCwBtn, QStringLiteral("3I-2"));

    root->addStretch();
}

void CwxApplet::syncFromModel()
{
    // NYI — Phase 3I-2
}

} // namespace NereusSDR
