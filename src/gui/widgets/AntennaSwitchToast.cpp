// =================================================================
// src/gui/widgets/AntennaSwitchToast.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for description.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#include "gui/widgets/AntennaSwitchToast.h"

#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

AntennaSwitchToast::AntennaSwitchToast(const QString& message, QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border: 1px solid %2;"
        " border-left: 3px solid #00ff88; border-radius: 3px; }"
    ).arg(Style::kPanelBg, Style::kBorder));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);

    auto* checkLbl = new QLabel(QStringLiteral("\xE2\x9C\x93"), this); // UTF-8 checkmark
    checkLbl->setStyleSheet(QStringLiteral("color: #00ff88; font-size: 14px;"));
    layout->addWidget(checkLbl);

    auto* textWidget = new QWidget(this);
    auto* textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);

    auto* titleLbl = new QLabel(QStringLiteral("Antenna auto-switched"), textWidget);
    titleLbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; font-weight: bold;")
            .arg(Style::kTextPrimary));
    textLayout->addWidget(titleLbl);

    auto* msgLbl = new QLabel(message, textWidget);
    msgLbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: 10px;").arg(Style::kTextTertiary));
    textLayout->addWidget(msgLbl);

    layout->addWidget(textWidget);

    auto* undoBtn = new QPushButton(QStringLiteral("UNDO"), this);
    undoBtn->setStyleSheet(
        Style::buttonBaseStyle()
        + QStringLiteral("QPushButton { height: 18px; padding: 0 8px; font-size: 9px; }"));
    connect(undoBtn, &QPushButton::clicked, this, &AntennaSwitchToast::undoRequested);
    connect(undoBtn, &QPushButton::clicked, this, &QWidget::close);
    layout->addWidget(undoBtn);

    setFixedHeight(50);
    setFixedWidth(380);

    m_autoDismissTimer.setSingleShot(true);
    connect(&m_autoDismissTimer, &QTimer::timeout, this, &QWidget::close);
    m_autoDismissTimer.start(8000);
}

AntennaSwitchToast::~AntennaSwitchToast() = default;

} // namespace NereusSDR
