// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableFrequencyPeek.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableFrequencyPeek.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-03: original implementation for NereusSDR by J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code
//               (overlay redesign).
// =================================================================

#include "RoundtableFrequencyPeek.h"

#include "core/AppSettings.h"
#include "models/RoundtableModel.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>

namespace NereusSDR {

namespace {

constexpr int kEdgeGap = 4;         // overlay.js renderPeek: kept 4 px inside
constexpr int kActionsGap = 4;      // .pk-acts margin-top and padding-top
constexpr int kActionsRule = 1;
constexpr int kActionsBottom = 6;
constexpr int kButtonHeight = 24;
constexpr int kButtonGap = 6;       // .pk-acts
constexpr int kBodySide = 8;        // the card's border and side pad
constexpr int kCloseSize = 18;      // .pk-x
constexpr int kCloseTop = 7;
constexpr int kCloseRight = 5;
const QColor kRuleColor(0x2a, 0x2a, 0x2e);

int actionsHeight()
{
    return kActionsGap + kActionsRule + kActionsGap + kButtonHeight + kActionsBottom;
}

} // namespace

RoundtableFrequencyPeek::RoundtableFrequencyPeek(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("roundtableFrequencyPeek"));
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    // Clicking it, or its buttons, leaves the keyboard with the band.
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setMouseTracking(true);
    hide();

    const QString buttons = QStringLiteral(
        "QPushButton { background: #1b1b1f; border: 1px solid #3d3d41; border-radius: 4px; color: #dcdce1;"
        " font-size: 11px; font-weight: 600; padding: 0 8px; }"
        "QPushButton:hover:enabled { background: #26262b; }"
        "QPushButton:disabled { color: #6c6c72; border-color: #2c2c31; }");
    m_tune = new QPushButton(this);
    m_tune->setObjectName(QStringLiteral("roundtablePeekTune"));
    m_tune->setFocusPolicy(Qt::NoFocus);
    m_tune->setStyleSheet(buttons + QStringLiteral(
        "QPushButton:enabled { background: #2f5f92; border-color: #5e8db8; color: #ffffff; }"
        "QPushButton:hover:enabled { background: #3a6ea6; }"
        "QPushButton:disabled { background: #1f3247; border-color: #34506b; color: #7f8d9b; }"));
    connect(m_tune, &QPushButton::clicked, this, [this]() {
        if (m_tune->isEnabled()) {
            emit tuneRequested(m_view.card.hz);
        }
    });
    m_messages = new QPushButton(QStringLiteral("Messages"), this);
    m_messages->setObjectName(QStringLiteral("roundtablePeekMessages"));
    m_messages->setFocusPolicy(Qt::NoFocus);
    m_messages->setStyleSheet(buttons);
    connect(m_messages, &QPushButton::clicked, this, [this]() { emit messagesRequested(m_view.card.hz); });
    m_close = new QToolButton(this);
    m_close->setObjectName(QStringLiteral("roundtablePeekClose"));
    m_close->setFocusPolicy(Qt::NoFocus);
    m_close->setText(QString(QChar(0x00d7)));
    m_close->setAccessibleName(QStringLiteral("Close"));
    m_close->setToolTip(QStringLiteral("Close"));
    m_close->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; border: none; color: #a6a6ac; font-size: 14px; padding: 0; }"
        "QToolButton:hover { color: #ffffff; }"));
    connect(m_close, &QToolButton::clicked, this, &RoundtableFrequencyPeek::closePeek);

    // Escape needs no focus: only while this is visible, in its own window. A
    // shortcut's context checks the widget's visibility, so it costs nothing
    // while closed.
    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WindowShortcut);
    connect(escape, &QShortcut::activated, this, &RoundtableFrequencyPeek::closePeek);

    if (parent != nullptr) {
        parent->installEventFilter(this);
    }
}

RoundtableFrequencyPeek::~RoundtableFrequencyPeek() = default;

void RoundtableFrequencyPeek::showFor(quint64 hz, const QString& family, const QPoint& under)
{
    m_under = under;
    if (!m_open || m_view.card.hz != hz || m_view.family != family) {
        m_panPeak = RoundtablePeakTracker();
    }
    m_view.card = RoundtableCardModel{};
    m_view.card.hz = hz;
    m_view.family = family;
    m_view.panDbm.reset();
    m_view.format = AppSettings::instance()
                        .value(QStringLiteral("Roundtable/SignalFormat"), QStringLiteral("dBm"))
                        .toString();
    m_open = true;
    updateTuneButton();
    fitToContents();
    show();
    raise();
    update();
}

void RoundtableFrequencyPeek::setSlice(quint64 hz, const QString& family, const QString& name)
{
    m_view.sliceHz = hz;
    m_view.sliceFamily = family;
    m_view.sliceName = name;
    updateTuneButton();
}

void RoundtableFrequencyPeek::setPathPreferences(bool miles, bool bearing)
{
    m_view.miles = miles;
    m_view.bearing = bearing;
}

void RoundtableFrequencyPeek::refresh(const RoundtableCardModel& model, std::optional<double> panDbm, bool keyed)
{
    if (!m_open) {
        return;
    }
    // Values redraw in place; only structure takes the walk that sizes the view.
    const bool sameShape = roundtableCardSameStructure(model, m_view.card)
        && panDbm.has_value() == m_view.panDbm.has_value();
    const QString previousFormat = m_view.format;
    m_view.card = model;
    m_view.panDbm = panDbm;
    if (panDbm) {
        m_view.panPeakDbm = m_panPeak.update(QStringLiteral("pan"), QStringLiteral("pan"), 1, *panDbm,
                                             RoundtableModel::nowMs());
    }
    m_view.format = AppSettings::instance()
                        .value(QStringLiteral("Roundtable/SignalFormat"), QStringLiteral("dBm"))
                        .toString();
    m_keyed = keyed;
    updateTuneButton();
    if (!sameShape || previousFormat != m_view.format) {
        fitToContents();
    }
    // The pan raises its mouse overlay on a resize.
    const QWidget* host = parentWidget();
    if (host != nullptr && !host->children().isEmpty() && host->children().constLast() != this) {
        raise();
    }
    update();
}

void RoundtableFrequencyPeek::closePeek()
{
    if (!m_open) {
        return;
    }
    m_open = false;
    hide();
    emit closed();
}

void RoundtableFrequencyPeek::updateTuneButton()
{
    m_tune->setText(QStringLiteral("Tune slice %1 here").arg(m_view.sliceName));
    const bool alreadyThere = m_view.card.hz == m_view.sliceHz && m_view.family == m_view.sliceFamily;
    m_tune->setEnabled(!m_keyed && !alreadyThere);
}

void RoundtableFrequencyPeek::fitToContents()
{
    ++m_fits;
    const QWidget* host = parentWidget();
    const int available = host != nullptr ? std::max(0, host->height() - 2 * kEdgeGap) : QWIDGETSIZE_MAX;
    const int width = host != nullptr ? std::min(kWidth, std::max(0, host->width() - 2 * kEdgeGap)) : kWidth;

    // Rows give way first on a short pan, with "+N more" (D19).
    int rows = static_cast<int>(roundtableViewRows(m_view).size());
    int body = roundtableViewHeight(m_view, width, rows, this);
    while (rows > 0 && body + actionsHeight() > available) {
        --rows;
        body = roundtableViewHeight(m_view, width, rows, this);
    }
    m_rowLimit = rows;
    m_contentHeight = body;
    const int height = body + actionsHeight();

    int x = m_under.x();
    int y = m_under.y();
    if (host != nullptr) {
        x = std::clamp(x, kEdgeGap, std::max(kEdgeGap, host->width() - width - kEdgeGap));
        y = std::clamp(y, kEdgeGap, std::max(kEdgeGap, host->height() - height - kEdgeGap));
    }
    const QRect wanted(x, y, width, height);
    if (geometry() != wanted) {
        setGeometry(wanted);
    }
    const int buttonTop = body + kActionsGap + kActionsRule + kActionsGap;
    const int tuneWidth = (width - 2 * kBodySide - kButtonGap) * 3 / 5;
    m_tune->setGeometry(kBodySide, buttonTop, tuneWidth, kButtonHeight);
    m_messages->setGeometry(kBodySide + tuneWidth + kButtonGap, buttonTop,
                            width - 2 * kBodySide - tuneWidth - kButtonGap, kButtonHeight);
    m_close->setGeometry(width - kCloseRight - kCloseSize, kCloseTop, kCloseSize, kCloseSize);
}

void RoundtableFrequencyPeek::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    paintRoundtableView(painter, m_view, rect(), m_rowLimit);
    const int ruleY = m_contentHeight + kActionsGap;
    painter.fillRect(QRect(kBodySide, ruleY, width() - 2 * kBodySide, kActionsRule), kRuleColor);
}

// Nothing inside the view reaches the band underneath: a click here never tunes.
void RoundtableFrequencyPeek::mousePressEvent(QMouseEvent* event)
{
    event->accept();
}

void RoundtableFrequencyPeek::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();
}

void RoundtableFrequencyPeek::mouseDoubleClickEvent(QMouseEvent* event)
{
    event->accept();
}

void RoundtableFrequencyPeek::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();
}

void RoundtableFrequencyPeek::wheelEvent(QWheelEvent* event)
{
    event->accept();
}

bool RoundtableFrequencyPeek::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && m_open) {
        fitToContents();
        // After the pan's own resize, which raises its mouse overlay.
        QTimer::singleShot(0, this, [this]() {
            if (m_open) {
                raise();
            }
        });
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace NereusSDR
