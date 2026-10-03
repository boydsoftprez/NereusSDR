// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/NoticeCard.cpp  (NereusSDR)
// =================================================================
//
// See NoticeCard.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: take-over parity: a Take it back the Core cannot run here
//               is shown off, with the reason as its tooltip. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: take-over fix wave: a disabled button draws in the style
//               guide's disabled colors. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include "gui/multidevice/NoticeCard.h"

#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

QString NoticeCard::text(const RemotePrompt& notice)
{
    // The time of day by this computer's clock (the design's ruling 10.3:
    // the Core sends how long ago, never a clock time).
    const QString when =
        QLocale().toString(notice.happenedAt().time(), QLocale::ShortFormat);
    return QStringLiteral("At %1. %2").arg(when, notice.reason);
}

NoticeCard::NoticeCard(const RemotePrompt& notice, QWidget* parent,
                       const QString& takeBackOff)
    : QFrame(parent)
    , m_id(notice.prompt.id)
{
    setObjectName(QStringLiteral("NoticeCard"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "QFrame#NoticeCard { background: rgba(16, 24, 36, 230); border: 1px solid #406080;"
        " border-radius: 6px; }"
        "QLabel { color: #d8e4f0; background: transparent; }"
        "QPushButton { background: #1e3048; color: #d8e4f0; border: 1px solid #5078a0;"
        " border-radius: 3px; padding: 2px 10px; }"
        // Take-over fix wave: a Take it back that is off reads as off, in
        // the style guide's disabled colors; its reason is its tooltip.
        "QPushButton:disabled { background: %1; color: %2; border-color: %3; }")
                      .arg(Style::kDisabledBg, Style::kDisabledText, Style::kDisabledBorder));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 6, 6);
    m_text = new QLabel(text(notice), this);
    m_text->setObjectName(QStringLiteral("noticeCardText"));
    m_text->setWordWrap(true);
    layout->addWidget(m_text, 1);
    if (notice.prompt.takeBack) {
        m_takeBack = new QPushButton(QStringLiteral("Take it back"), this);
        m_takeBack->setObjectName(QStringLiteral("noticeCardTakeBack"));
        layout->addWidget(m_takeBack);
        connect(m_takeBack, &QPushButton::clicked, this,
                [this]() { emit takeBackRequested(m_id); });
    } else if (!takeBackOff.isEmpty()) {
        // Take-over parity: shown off, with why.
        m_takeBack = new QPushButton(QStringLiteral("Take it back"), this);
        m_takeBack->setObjectName(QStringLiteral("noticeCardTakeBack"));
        m_takeBack->setEnabled(false);
        m_takeBack->setToolTip(takeBackOff);
        layout->addWidget(m_takeBack);
    }
    m_close = new QPushButton(QStringLiteral("Close"), this);
    m_close->setObjectName(QStringLiteral("noticeCardClose"));
    m_close->setToolTip(QStringLiteral("Put this notice away."));
    layout->addWidget(m_close);
    connect(m_close, &QPushButton::clicked, this, [this]() { emit dismissed(m_id); });
}

} // namespace NereusSDR
