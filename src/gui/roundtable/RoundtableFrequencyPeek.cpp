// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableFrequencyPeek.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableFrequencyPeek.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-04: A fold's list (overlay design D37). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-11-04: The one inspector: your own frequency, ⋯, the Viewing line with
//               Back, the foot anchor and the scrolling body. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-11-04: Tune greys when it would write nothing, and with no slice.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
constexpr int kActionsAbove = 6;    // .rc-insp .rc__body's gap and .pk-acts margin-top
constexpr int kActionsRule = 1;
constexpr int kActionsBelow = 4;    // .pk-acts padding-top
constexpr int kActionsBottom = 8;   // .rc-insp .rc__body padding and the border
constexpr int kButtonHeight = 26;   // .rc-insp .pk-acts button
constexpr int kButtonGap = 6;       // .pk-acts
constexpr int kBodySide = 9;        // the card's border and side pad
constexpr int kHeadButton = 24;     // .ins-b
constexpr int kHeadButtonGap = 5;   // .ins-head
constexpr int kHeadButtonTop = 9;   // the border, rail and pad, centered on the 26 px head
constexpr int kBackHeight = 22;     // .ins-view button
constexpr int kWheelNotch = 120;
constexpr int kWheelPixelsPerNotch = 40;
const QColor kRuleColor(0x2a, 0x2a, 0x2e);

int actionsHeight()
{
    return kActionsAbove + kActionsRule + kActionsBelow + kButtonHeight + kActionsBottom;
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
    const QString headButton = QStringLiteral(
        "QToolButton { background: transparent; border: none; border-radius: 4px; color: #a6a6ac; font-size: 15px; padding: 0; }"
        "QToolButton:hover { background: #26262b; color: #ffffff; }");
    m_close->setStyleSheet(headButton);
    connect(m_close, &QToolButton::clicked, this, &RoundtableFrequencyPeek::closePeek);
    m_more = new QToolButton(this);
    m_more->setObjectName(QStringLiteral("roundtablePeekMore"));
    m_more->setFocusPolicy(Qt::NoFocus);
    m_more->setText(QString(QChar(0x22ef)));
    m_more->setAccessibleName(QStringLiteral("More"));
    m_more->setToolTip(QStringLiteral("More"));
    m_more->setStyleSheet(headButton);
    connect(m_more, &QToolButton::clicked, this, [this]() {
        emit moreRequested(m_more->mapToGlobal(QPoint(0, m_more->height())));
    });
    m_back = new QPushButton(roundtableViewBackText(), this);
    m_back->setObjectName(QStringLiteral("roundtablePeekBack"));
    m_back->setFocusPolicy(Qt::NoFocus);
    m_back->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1e3d5f; border: 1px solid #5e8db8; border-radius: 4px; color: #eef3f8;"
        " font-size: 11px; font-weight: 600; padding: 0 7px; }"
        "QPushButton:hover:enabled { background: #28517b; }"
        "QPushButton:disabled { background: #1b2733; border-color: #34506b; color: #7f8d9b; }"));
    connect(m_back, &QPushButton::clicked, this, &RoundtableFrequencyPeek::backRequested);

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
    m_onFoot = false;
    open(hz, family, false);
}

void RoundtableFrequencyPeek::showFor(quint64 hz, const QString& family)
{
    m_onFoot = true;
    open(hz, family, false);
}

void RoundtableFrequencyPeek::showOwn(quint64 hz, const QString& family)
{
    m_onFoot = true;
    open(hz, family, true);
}

void RoundtableFrequencyPeek::setInspectorSpot(const QRect& pan, int dialX, const QVector<QRect>& flags, bool flagFlipped)
{
    if (pan == m_spotPan && dialX == m_spotDialX && flags == m_spotFlags && flagFlipped == m_spotFlipped) {
        return;
    }
    m_spotPan = pan;
    m_spotDialX = dialX;
    m_spotFlags = flags;
    m_spotFlipped = flagFlipped;
    if (m_open && m_onFoot) {
        if (m_listing) {
            fitList();
        } else {
            fitToContents();
        }
    }
}

void RoundtableFrequencyPeek::showFold(const QVector<RoundtableFoldRow>& rows)
{
    if (!m_listing) {
        m_scroll = 0;
    }
    m_onFoot = true;
    m_listing = true;
    m_rows = rows;
    m_hoverRow = -1;
    m_view.card = RoundtableCardModel{};
    m_open = true;
    m_tune->hide();
    m_messages->hide();
    m_more->hide();
    m_back->hide();
    fitList();
    show();
    raise();
    update();
}

void RoundtableFrequencyPeek::setFoldRows(const QVector<RoundtableFoldRow>& rows)
{
    if (!showsFold() || rows == m_rows) {
        return;
    }
    const bool sameSize = rows.size() == m_rows.size();
    m_rows = rows;
    if (!sameSize) {
        fitList();
    }
    update();
}

void RoundtableFrequencyPeek::open(quint64 hz, const QString& family, bool mine)
{
    if (!m_open || m_listing || m_view.card.hz != hz || m_view.family != family || m_view.mine != mine) {
        m_panPeak = RoundtablePeakTracker();
        m_scroll = 0;
    }
    m_listing = false;
    m_rows.clear();
    m_hoverRow = -1;
    m_tune->show();
    m_messages->show();
    m_more->show();
    m_view.mine = mine;
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
    if (!m_open || m_listing) {
        return;
    }
    // Values redraw in place; only structure takes the walk that sizes the view.
    const bool sameShape = roundtableCardSameStructure(model, m_view.card)
        && (m_view.mine || panDbm.has_value() == m_view.panDbm.has_value());
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
    m_listing = false;
    m_rows.clear();
    hide();
    emit closed();
}

void RoundtableFrequencyPeek::updateTuneButton()
{
    // With no slice this window controls, Tune stays in view and greyed.
    const bool noSlice = m_view.sliceName.isEmpty();
    m_tune->setText(noSlice ? QStringLiteral("Tune slice here") : QStringLiteral("Tune slice %1 here").arg(m_view.sliceName));
    const bool alreadyThere = m_view.mine
        || roundtableTuneWritesNothing(static_cast<qint64>(m_view.card.hz), m_view.family,
                                       static_cast<qint64>(m_view.sliceHz), m_view.sliceFamily);
    m_tune->setEnabled(!m_keyed && !noSlice && !alreadyThere);
    QString reason;
    if (!noSlice && alreadyThere) {
        reason = QStringLiteral("Slice %1 is already here").arg(m_view.sliceName);
    } else if (!noSlice && m_keyed) {
        reason = QStringLiteral("Not while you transmit");
    }
    m_tune->setToolTip(reason);
    // Back has nowhere to go without a slice.
    m_back->setEnabled(!noSlice);
}

int RoundtableFrequencyPeek::maxScrollForTest() const
{
    if (m_listing) {
        return std::max(0, roundtableFoldListHeight(static_cast<int>(m_rows.size())) - roundtableFoldListBodyTop()
                               - m_bodyHeight);
    }
    return std::max(0, m_layout.body() - m_bodyHeight);
}

void RoundtableFrequencyPeek::fitList()
{
    ++m_fits;
    const QWidget* host = parentWidget();
    const QRect spot = m_spotPan.isEmpty() && host != nullptr ? host->rect().adjusted(kEdgeGap, kEdgeGap, -kEdgeGap, -kEdgeGap)
                                                              : m_spotPan;
    const int wantedHeight = roundtableFoldListHeight(static_cast<int>(m_rows.size()));
    const int height = std::min(spot.height(), wantedHeight);
    m_bodyHeight = std::max(0, height - roundtableFoldListBodyTop());
    m_scroll = std::clamp(m_scroll, 0, maxScrollForTest());
    const int dialX = m_spotPan.isEmpty() ? spot.center().x() : m_spotDialX;
    const QRect wanted = placeRoundtableInspector(spot, dialX, m_spotFlags, m_spotFlipped, height);
    if (geometry() != wanted) {
        setGeometry(wanted);
    }
    placeHeadButtons(wanted);
}

void RoundtableFrequencyPeek::placeHeadButtons(const QRect& wanted)
{
    const int closeLeft = wanted.width() - kBodySide - kHeadButton;
    m_close->setGeometry(closeLeft, kHeadButtonTop, kHeadButton, kHeadButton);
    m_more->setGeometry(closeLeft - kHeadButtonGap - kHeadButton, kHeadButtonTop, kHeadButton, kHeadButton);
}

void RoundtableFrequencyPeek::setHoverRow(int row)
{
    if (row != m_hoverRow) {
        m_hoverRow = row;
        update();
    }
}

void RoundtableFrequencyPeek::fitToContents()
{
    ++m_fits;
    const QWidget* host = parentWidget();
    const int hostWidth = host != nullptr ? std::max(0, host->width() - 2 * kEdgeGap) : QWIDGETSIZE_MAX;
    const int hostHeight = host != nullptr ? std::max(0, host->height() - 2 * kEdgeGap) : QWIDGETSIZE_MAX;
    // Until told the spot, the foot is the bottom of the pan.
    const QRect spot = m_spotPan.isEmpty() && host != nullptr ? host->rect().adjusted(kEdgeGap, kEdgeGap, -kEdgeGap, -kEdgeGap)
                                                              : m_spotPan;
    const int room = m_onFoot ? spot.height() : hostHeight;
    const int measureWidth = m_onFoot ? kWidth : std::min(kWidth, hostWidth);

    // Everyone not keyed folds first; what still does not fit scrolls (D19).
    m_fold = false;
    m_layout = roundtableViewLayout(m_view, measureWidth, false, this);
    if (m_layout.total + actionsHeight() > room) {
        m_fold = true;
        m_layout = roundtableViewLayout(m_view, measureWidth, true, this);
    }
    const int height = std::min(room, m_layout.total + actionsHeight());
    m_bodyHeight = std::max(0, height - m_layout.head - actionsHeight());
    m_scroll = std::clamp(m_scroll, 0, maxScrollForTest());

    QRect wanted;
    if (m_onFoot) {
        const int dialX = m_spotPan.isEmpty() ? spot.center().x() : m_spotDialX;
        wanted = placeRoundtableInspector(spot, dialX, m_spotFlags, m_spotFlipped, height);
    } else {
        int x = m_under.x();
        int y = m_under.y();
        if (host != nullptr) {
            x = std::clamp(x, kEdgeGap, std::max(kEdgeGap, host->width() - measureWidth - kEdgeGap));
            y = std::clamp(y, kEdgeGap, std::max(kEdgeGap, host->height() - height - kEdgeGap));
        }
        wanted = QRect(x, y, measureWidth, height);
    }
    if (geometry() != wanted) {
        setGeometry(wanted);
    }

    // Tune and Messages stand at the bottom whatever the body does, side by side.
    const int inner = wanted.width() - 2 * kBodySide;
    const int buttonTop = wanted.height() - kActionsBottom - kButtonHeight;
    const int tuneWidth = std::max(0, inner - kButtonGap) / 2;
    m_tune->setGeometry(kBodySide, buttonTop, tuneWidth, kButtonHeight);
    m_messages->setGeometry(kBodySide + tuneWidth + kButtonGap, buttonTop, inner - tuneWidth - kButtonGap, kButtonHeight);
    placeHeadButtons(wanted);
    m_back->setVisible(!m_view.mine);
    if (m_layout.viewingTop >= 0) {
        m_back->setGeometry(wanted.width() - kBodySide - m_layout.backWidth, m_layout.viewingTop, m_layout.backWidth, kBackHeight);
    }
}

void RoundtableFrequencyPeek::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    if (m_listing) {
        paintRoundtableFoldList(painter, m_rows, rect(), m_hoverRow, m_scroll);
        return;
    }
    paintRoundtableView(painter, m_view, rect(), m_fold, m_scroll, m_bodyHeight);
    const int ruleY = height() - actionsHeight() + kActionsAbove;
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
    if (m_listing && event->button() == Qt::LeftButton) {
        const int row = roundtableFoldListRowAt(event->position().toPoint(), rect(), static_cast<int>(m_rows.size()), m_scroll);
        if (row >= 0) {
            emit foldRowClicked(m_rows.at(row).roomId);
        }
    }
}

void RoundtableFrequencyPeek::mouseDoubleClickEvent(QMouseEvent* event)
{
    event->accept();
}

void RoundtableFrequencyPeek::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();
    if (m_listing) {
        setHoverRow(roundtableFoldListRowAt(event->position().toPoint(), rect(), static_cast<int>(m_rows.size()), m_scroll));
    }
}

void RoundtableFrequencyPeek::leaveEvent(QEvent* event)
{
    setHoverRow(-1);
    QWidget::leaveEvent(event);
}

// The wheel moves the body and stops here, at either end and over the buttons too.
void RoundtableFrequencyPeek::wheelEvent(QWheelEvent* event)
{
    const int pixels = event->pixelDelta().y();
    const int delta = pixels != 0 ? pixels : event->angleDelta().y() * kWheelPixelsPerNotch / kWheelNotch;
    const int scrolled = std::clamp(m_scroll - delta, 0, maxScrollForTest());
    if (scrolled != m_scroll) {
        m_scroll = scrolled;
        update();
    }
    event->accept();
}

bool RoundtableFrequencyPeek::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && m_open) {
        if (m_listing) {
            fitList();
        } else {
            fitToContents();
        }
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
