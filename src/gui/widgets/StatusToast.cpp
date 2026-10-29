// =================================================================
// src/gui/widgets/StatusToast.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for description.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-30 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
//   2026-10-09 The severity bar draws: the frame rule names the toast
//              itself, which paints it (paintEvent), as a translucent
//              window skips its style sheet background. J.J. Boyd
//              (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 A timeout of kUntilClosed starts no countdown. J.J. Boyd
//              (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#include "gui/widgets/StatusToast.h"

#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

namespace NereusSDR {

namespace {

/// Left accent colour per severity, taken from the same badge palette
/// the bottom bar uses so the two surfaces agree.
QString accentFor(ToastSeverity severity)
{
    switch (severity) {
        case ToastSeverity::Warning: return QString::fromLatin1(Style::kAmberText);
        case ToastSeverity::Error:   return QString::fromLatin1(Style::kRedBorder);
        case ToastSeverity::Info:    break;
    }
    return QString::fromLatin1(Style::kAccent);
}

// Wide enough for the longest notice in the tree (the multi-radio
// auto-connect notice) at two lines, without covering the panadapter.
constexpr int kToastWidth = 380;

} // namespace

StatusToast::StatusToast(const QString& message,
                         ToastSeverity severity,
                         int timeoutMs,
                         QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool),
      m_message(message)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(QStringLiteral("Click to dismiss"));

    // The frame and its severity bar belong to the toast alone, and the
    // toast draws them in paintEvent: a translucent window skips the style
    // sheet background Qt would otherwise paint, so the bar never showed.
    setObjectName(QStringLiteral("statusToast"));
    setStyleSheet(QStringLiteral(
        "QWidget#statusToast { background: %1; border: 1px solid %2;"
        " border-left: 3px solid %3; border-radius: 3px; }"
    ).arg(QString::fromLatin1(Style::kPanelBg),
          QString::fromLatin1(Style::kBorder),
          accentFor(severity)));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);

    auto* msgLbl = new QLabel(message, this);
    msgLbl->setWordWrap(true);
    msgLbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; border: none;")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
    layout->addWidget(msgLbl, 1);

    setFixedWidth(kToastWidth);
    // Height follows the wrapped text rather than a fixed value: the
    // messages range from "TX > Slice B" to a two-line auto-connect
    // explanation, and a fixed height would clip the long ones.
    adjustSize();

    m_dismissTimer.setSingleShot(true);
    connect(&m_dismissTimer, &QTimer::timeout, this, &QWidget::close);
    refresh(timeoutMs);
}

StatusToast::~StatusToast() = default;

void StatusToast::paintEvent(QPaintEvent* /*event*/)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

void StatusToast::refresh(int timeoutMs)
{
    if (timeoutMs <= kUntilClosed) {
        m_dismissTimer.stop();
        return;
    }
    m_dismissTimer.start(timeoutMs);
}

void StatusToast::mousePressEvent(QMouseEvent* event)
{
    // Any button dismisses. A notice the operator has already read
    // should not sit there for its full timeout blocking the view.
    event->accept();
    close();
}

} // namespace NereusSDR
