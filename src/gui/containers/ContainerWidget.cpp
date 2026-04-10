#include "ContainerWidget.h"
#include "FloatingContainer.h"
#include "core/LogCategories.h"

#include <QUuid>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QApplication>
#include <QMap>
#include <QStringList>
#include <algorithm>

namespace NereusSDR {

ContainerWidget::ContainerWidget(QWidget* parent)
    : QWidget(parent)
{
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setMinimumSize(kMinContainerWidth, kMinContainerHeight);
    setMouseTracking(true);
    buildUI();
    updateTitleBar();
    updateTitle();
    setupBorder();
    storeLocation();

    // Default placeholder content — replaced by setContent() in later phases
    auto* placeholder = new QLabel(QStringLiteral("Container"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral(
        "color: #405060; font-size: 14px; background: transparent;"));
    setContent(placeholder);

    qCDebug(lcContainer) << "Container created:" << m_id;
}

ContainerWidget::~ContainerWidget()
{
    qCDebug(lcContainer) << "Container destroyed:" << m_id;
}

void ContainerWidget::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Title bar — hidden by default, shown on hover (Thetis ucMeter.cs:156)
    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(22);
    m_titleBar->setVisible(false);
    m_titleBar->setStyleSheet(QStringLiteral(
        "background: #1a2a3a; border-bottom: 1px solid #203040;"));

    auto* barLayout = new QHBoxLayout(m_titleBar);
    barLayout->setContentsMargins(4, 0, 0, 0);
    barLayout->setSpacing(2);

    // Title label — "RX1" + notes (Thetis ucMeter.cs:625-639)
    m_titleLabel = new QLabel(QStringLiteral("RX1"), m_titleBar);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "color: #c8d8e8; font-size: 11px; font-weight: bold; background: transparent;"));
    m_titleLabel->setCursor(Qt::SizeAllCursor);
    barLayout->addWidget(m_titleLabel, 1);

    const QString btnStyle = QStringLiteral(
        "QPushButton { background: transparent; border: none; color: #8090a0;"
        "  font-size: 11px; padding: 2px 4px; }"
        "QPushButton:hover { background: #2a3a4a; color: #c8d8e8; }");

    // Axis lock button (overlay-docked only)
    m_btnAxis = new QPushButton(QStringLiteral("\u2196"), m_titleBar);
    m_btnAxis->setFixedSize(22, 22);
    m_btnAxis->setToolTip(QStringLiteral("Axis lock (click to cycle, right-click reverse)"));
    m_btnAxis->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnAxis);

    // Pin-on-top button (floating only)
    m_btnPin = new QPushButton(QStringLiteral("\U0001F4CC"), m_titleBar);
    m_btnPin->setFixedSize(22, 22);
    m_btnPin->setToolTip(QStringLiteral("Pin on top"));
    m_btnPin->setStyleSheet(btnStyle);
    m_btnPin->setVisible(false);
    barLayout->addWidget(m_btnPin);

    // Float/Dock toggle
    m_btnFloat = new QPushButton(QStringLiteral("\u2197"), m_titleBar);
    m_btnFloat->setFixedSize(22, 22);
    m_btnFloat->setToolTip(QStringLiteral("Float / Dock"));
    m_btnFloat->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnFloat);

    // Settings gear
    m_btnSettings = new QPushButton(QStringLiteral("\u2699"), m_titleBar);
    m_btnSettings->setFixedSize(22, 22);
    m_btnSettings->setToolTip(QStringLiteral("Container settings"));
    m_btnSettings->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnSettings);

    mainLayout->addWidget(m_titleBar);

    // Content holder — layout slot for setContent()
    m_contentHolder = new QWidget(this);
    m_contentHolder->setMouseTracking(true);
    m_contentHolder->setStyleSheet(QStringLiteral("background: #0f0f1a;"));
    new QVBoxLayout(m_contentHolder);
    m_contentHolder->layout()->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_contentHolder, 1);

    // Resize grip (bottom-right, hidden until hover)
    m_resizeGrip = new QWidget(this);
    m_resizeGrip->setFixedSize(12, 12);
    m_resizeGrip->setCursor(Qt::SizeFDiagCursor);
    m_resizeGrip->setStyleSheet(QStringLiteral(
        "background: #405060; border-radius: 2px;"));
    m_resizeGrip->setVisible(false);

    // Wire button signals
    connect(m_btnFloat, &QPushButton::clicked, this, [this]() {
        if (isFloating()) {
            emit dockRequested();
        } else {
            emit floatRequested();
        }
    });

    connect(m_btnAxis, &QPushButton::clicked, this, [this]() {
        cycleAxisLock(QApplication::keyboardModifiers() & Qt::ShiftModifier);
    });

    connect(m_btnPin, &QPushButton::clicked, this, [this]() {
        setPinOnTop(!m_pinOnTop);
    });

    connect(m_btnSettings, &QPushButton::clicked, this, [this]() {
        emit settingsRequested();
    });

    // Event filters for title bar drag + resize grip
    m_titleBar->installEventFilter(this);
    m_titleLabel->installEventFilter(this);
    m_resizeGrip->installEventFilter(this);
}

void ContainerWidget::setContent(QWidget* widget)
{
    QLayout* layout = m_contentHolder->layout();
    if (m_content) {
        layout->removeWidget(m_content);
        m_content->deleteLater();
    }
    m_content = widget;
    if (m_content) {
        m_content->setParent(m_contentHolder);
        layout->addWidget(m_content);
    }
}

void ContainerWidget::updateTitleBar()
{
    // Thetis ucMeter.cs:605-624 — adapted for 3 dock modes
    if (isFloating()) {
        m_btnFloat->setText(QStringLiteral("\u2199"));
        m_btnFloat->setToolTip(QStringLiteral("Dock"));
        m_btnAxis->setVisible(false);
        m_btnPin->setVisible(true);
    } else if (isPanelDocked()) {
        m_btnFloat->setText(QStringLiteral("\u2197"));
        m_btnFloat->setToolTip(QStringLiteral("Float"));
        m_btnAxis->setVisible(false);
        m_btnPin->setVisible(false);
    } else {
        m_btnFloat->setText(QStringLiteral("\u2197"));
        m_btnFloat->setToolTip(QStringLiteral("Float"));
        m_btnAxis->setVisible(true);
        m_btnPin->setVisible(false);
    }
}

void ContainerWidget::updateTitle()
{
    // Thetis ucMeter.cs:625-639
    QString prefix = QStringLiteral("RX");
    QString firstLine = m_notes.section(QLatin1Char('\n'), 0, 0);
    QString title = prefix + QString::number(m_rxSource);
    if (!firstLine.isEmpty()) {
        title += QStringLiteral(" ") + firstLine;
    }
    m_titleLabel->setText(title);
}

void ContainerWidget::setupBorder()
{
    // Thetis ucMeter.cs:640-643
    if (m_border) {
        setStyleSheet(QStringLiteral("ContainerWidget { border: 1px solid #203040; }"));
    } else {
        setStyleSheet(QString());
    }
}

// --- Property setters ---

void ContainerWidget::setId(const QString& id) { m_id = id; m_id.remove(QLatin1Char('|')); }
void ContainerWidget::setRxSource(int rx) { m_rxSource = rx; updateTitle(); }

void ContainerWidget::setDockMode(DockMode mode)
{
    if (m_dockMode == mode) {
        return;
    }
    m_dockMode = mode;
    updateTitleBar();
    emit dockModeChanged(mode);
}

void ContainerWidget::setAxisLock(AxisLock lock)
{
    m_axisLock = lock;

    // Update axis button icon — from Thetis ucMeter.cs:936-968
    static const QChar arrows[] = {
        QChar(0x2190),  // LEFT
        QChar(0x2196),  // TOPLEFT
        QChar(0x2191),  // TOP
        QChar(0x2197),  // TOPRIGHT
        QChar(0x2192),  // RIGHT
        QChar(0x2198),  // BOTTOMRIGHT
        QChar(0x2193),  // BOTTOM
        QChar(0x2199),  // BOTTOMLEFT
    };
    int idx = static_cast<int>(lock);
    if (idx >= 0 && idx < 8) {
        m_btnAxis->setText(QString(arrows[idx]));
    }
}

void ContainerWidget::cycleAxisLock(bool reverse)
{
    // From Thetis ucMeter.cs:912-935
    int n = static_cast<int>(m_axisLock);
    if (reverse) {
        n--;
    } else {
        n++;
    }
    if (n > static_cast<int>(AxisLock::BottomLeft)) {
        n = static_cast<int>(AxisLock::Left);
    }
    if (n < static_cast<int>(AxisLock::Left)) {
        n = static_cast<int>(AxisLock::BottomLeft);
    }
    setAxisLock(static_cast<AxisLock>(n));
    emit dockedMoved();
}

void ContainerWidget::setPinOnTop(bool pin)
{
    // From Thetis ucMeter.cs:974-978
    m_pinOnTop = pin;
    m_btnPin->setText(pin ? QStringLiteral("\U0001F4CD") : QStringLiteral("\U0001F4CC"));
    setTopMost();
}

void ContainerWidget::setTopMost()
{
    // From Thetis ucMeter.cs:980-993
    if (isFloating() && parentWidget()) {
        FloatingContainer* fc = qobject_cast<FloatingContainer*>(parentWidget());
        if (fc) {
            bool wasVisible = fc->isVisible();
            if (m_pinOnTop) {
                fc->setWindowFlags(fc->windowFlags() | Qt::WindowStaysOnTopHint);
            } else {
                fc->setWindowFlags(fc->windowFlags() & ~Qt::WindowStaysOnTopHint);
            }
            if (wasVisible) {
                fc->show();
            }
        }
    }
}

void ContainerWidget::setBorder(bool border) { m_border = border; setupBorder(); }
void ContainerWidget::setLocked(bool locked) { m_locked = locked; }
void ContainerWidget::setContainerEnabled(bool enabled) { m_enabled = enabled; }
void ContainerWidget::setShowOnRx(bool show) { m_showOnRx = show; }
void ContainerWidget::setShowOnTx(bool show) { m_showOnTx = show; }
void ContainerWidget::setHiddenByMacro(bool hidden) { m_hiddenByMacro = hidden; }
void ContainerWidget::setContainerMinimises(bool minimises) { m_containerMinimises = minimises; }
void ContainerWidget::setContainerHidesWhenRxNotUsed(bool hides) { m_containerHidesWhenRxNotUsed = hides; }

void ContainerWidget::setNotes(const QString& notes)
{
    m_notes = notes;
    m_notes.remove(QLatin1Char('|'));
    updateTitle();
}

void ContainerWidget::setNoControls(bool noControls) { m_noControls = noControls; }
void ContainerWidget::setAutoHeight(bool autoHeight) { m_autoHeight = autoHeight; }
void ContainerWidget::setDockedLocation(const QPoint& loc) { m_dockedLocation = loc; }
void ContainerWidget::setDockedSize(const QSize& size) { m_dockedSize = size; }
void ContainerWidget::setDelta(const QPoint& delta) { m_delta = delta; }

void ContainerWidget::storeLocation()
{
    // From Thetis ucMeter.cs:567-572
    m_dockedLocation = pos();
    m_dockedSize = size();
}

void ContainerWidget::restoreLocation()
{
    // From Thetis ucMeter.cs:574-593
    bool moved = false;
    if (m_dockedLocation != pos()) {
        move(m_dockedLocation);
        moved = true;
    }
    if (m_dockedSize != size()) {
        resize(m_dockedSize);
        moved = true;
    }
    if (moved) {
        update();
    }
}

int ContainerWidget::roundToNearestTen(int value)
{
    return ((value + 5) / 10) * 10;
}

// --- Hover show/hide ---

void ContainerWidget::mouseMoveEvent(QMouseEvent* event)
{
    // From Thetis ucMeter.cs:1198-1229
    bool noControls = m_noControls && !(QApplication::keyboardModifiers() & Qt::ShiftModifier);

    if (!m_dragging && !noControls) {
        bool inTitleRegion = event->position().y() < 22;
        if (inTitleRegion && !m_titleBar->isVisible()) {
            m_titleBar->setVisible(true);
            m_titleBar->raise();
        } else if (!inTitleRegion && m_titleBar->isVisible() && !m_dragging) {
            m_titleBar->setVisible(false);
        }
    }

    // Resize grip: only for overlay-docked and floating, not panel-docked
    if (!m_resizing && !noControls && !isPanelDocked()) {
        bool inGripRegion = event->position().x() > (width() - 16)
                         && event->position().y() > (height() - 16);
        if (inGripRegion && !m_resizeGrip->isVisible()) {
            m_resizeGrip->move(width() - 12, height() - 12);
            m_resizeGrip->setVisible(true);
            m_resizeGrip->raise();
        } else if (!inGripRegion && m_resizeGrip->isVisible() && !m_resizing) {
            m_resizeGrip->setVisible(false);
        }
    }

    if (m_dragging) {
        updateDrag(event->globalPosition().toPoint());
    }
    if (m_resizing) {
        updateResize(event->globalPosition().toPoint());
    }

    QWidget::mouseMoveEvent(event);
}

void ContainerWidget::leaveEvent(QEvent* event)
{
    if (!m_dragging && !m_resizing) {
        m_titleBar->setVisible(false);
        m_resizeGrip->setVisible(false);
    }
    QWidget::leaveEvent(event);
}

// --- Event filter for title bar drag + resize grip ---

bool ContainerWidget::eventFilter(QObject* watched, QEvent* event)
{
    // Title bar drag
    if ((watched == m_titleBar || watched == m_titleLabel) && !m_locked) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !isPanelDocked()) {
                beginDrag(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            updateDrag(me->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            endDrag();
            return true;
        }
    }

    // Resize grip (not for panel-docked)
    if (watched == m_resizeGrip && !m_locked && !isPanelDocked()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                beginResize(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_resizing) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            updateResize(me->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_resizing) {
            endResize();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

// --- Drag logic ---

void ContainerWidget::beginDrag(const QPoint& globalPos)
{
    // From Thetis ucMeter.cs:281-294
    m_dragging = true;
    if (isFloating()) {
        m_dragStartPos = globalPos - parentWidget()->pos();
    } else {
        raise();
        m_dragStartPos = globalPos - pos();
    }
}

void ContainerWidget::updateDrag(const QPoint& globalPos)
{
    if (!m_dragging) {
        return;
    }

    if (isFloating()) {
        // From Thetis ucMeter.cs:319-345
        QPoint newPos = globalPos - m_dragStartPos;
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            newPos.setX(roundToNearestTen(newPos.x()));
            newPos.setY(roundToNearestTen(newPos.y()));
        }
        if (parentWidget() && parentWidget()->pos() != newPos) {
            parentWidget()->move(newPos);
        }
    } else {
        // From Thetis ucMeter.cs:346-374 — overlay-docked, clamped
        QPoint newPos = globalPos - m_dragStartPos;
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            newPos.setX(roundToNearestTen(newPos.x()));
            newPos.setY(roundToNearestTen(newPos.y()));
        }
        if (parentWidget()) {
            int maxX = parentWidget()->width() - width();
            int maxY = parentWidget()->height() - height();
            newPos.setX(std::clamp(newPos.x(), 0, std::max(0, maxX)));
            newPos.setY(std::clamp(newPos.y(), 0, std::max(0, maxY)));
        }
        if (pos() != newPos) {
            move(newPos);
            update();
        }
    }
}

void ContainerWidget::endDrag()
{
    m_dragging = false;
    m_dragStartPos = QPoint();
    if (isOverlayDocked()) {
        m_dockedLocation = pos();
        emit dockedMoved();
    }
}

// --- Resize logic ---

void ContainerWidget::beginResize(const QPoint& globalPos)
{
    // From Thetis ucMeter.cs:400-407
    m_resizeStartGlobal = globalPos;
    m_resizeStartSize = isFloating() && parentWidget() ? parentWidget()->size() : size();
    m_resizing = true;
    raise();
}

void ContainerWidget::updateResize(const QPoint& globalPos)
{
    if (!m_resizing) {
        return;
    }

    // From Thetis ucMeter.cs:489-518
    int dX = globalPos.x() - m_resizeStartGlobal.x();
    int dY = globalPos.y() - m_resizeStartGlobal.y();

    int newW = m_resizeStartSize.width() + dX;
    int newH = m_resizeStartSize.height() + dY;

    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        newW = roundToNearestTen(newW);
        newH = roundToNearestTen(newH);
    }

    doResize(newW, newH);
}

void ContainerWidget::endResize()
{
    m_resizing = false;
    m_resizeStartGlobal = QPoint();
    if (isOverlayDocked()) {
        m_dockedSize = size();
    }
}

void ContainerWidget::doResize(int w, int h)
{
    // From Thetis ucMeter.cs:520-549
    w = std::max(w, kMinContainerWidth);
    h = std::max(h, kMinContainerHeight);

    if (isFloating()) {
        if (parentWidget()) {
            parentWidget()->resize(w, h);
        }
    } else {
        if (parentWidget()) {
            if (x() + w > parentWidget()->width()) {
                w = parentWidget()->width() - x();
            }
            if (y() + h > parentWidget()->height()) {
                h = parentWidget()->height() - y();
            }
        }
        QSize newSize(w, h);
        if (newSize != size()) {
            resize(newSize);
            update();
        }
    }
}

// --- Serialization ---

QString ContainerWidget::axisLockToString(AxisLock lock)
{
    static const char* names[] = {
        "LEFT", "TOPLEFT", "TOP", "TOPRIGHT",
        "RIGHT", "BOTTOMRIGHT", "BOTTOM", "BOTTOMLEFT"
    };
    int idx = static_cast<int>(lock);
    return (idx >= 0 && idx < 8) ? QString::fromLatin1(names[idx]) : QStringLiteral("TOPLEFT");
}

AxisLock ContainerWidget::axisLockFromString(const QString& str)
{
    static const QMap<QString, AxisLock> map = {
        {QStringLiteral("LEFT"), AxisLock::Left},
        {QStringLiteral("TOPLEFT"), AxisLock::TopLeft},
        {QStringLiteral("TOP"), AxisLock::Top},
        {QStringLiteral("TOPRIGHT"), AxisLock::TopRight},
        {QStringLiteral("RIGHT"), AxisLock::Right},
        {QStringLiteral("BOTTOMRIGHT"), AxisLock::BottomRight},
        {QStringLiteral("BOTTOM"), AxisLock::Bottom},
        {QStringLiteral("BOTTOMLEFT"), AxisLock::BottomLeft},
    };
    return map.value(str.toUpper(), AxisLock::TopLeft);
}

QString ContainerWidget::dockModeToString(DockMode mode)
{
    switch (mode) {
    case DockMode::PanelDocked:   return QStringLiteral("PANEL");
    case DockMode::OverlayDocked: return QStringLiteral("OVERLAY");
    case DockMode::Floating:      return QStringLiteral("FLOATING");
    }
    return QStringLiteral("OVERLAY");
}

DockMode ContainerWidget::dockModeFromString(const QString& str)
{
    if (str == QStringLiteral("PANEL")) {
        return DockMode::PanelDocked;
    }
    if (str == QStringLiteral("FLOATING")) {
        return DockMode::Floating;
    }
    return DockMode::OverlayDocked;
}

QString ContainerWidget::serialize() const
{
    // Based on Thetis ucMeter.cs:1012-1038, extended with DockMode (field 23)
    QStringList p;
    p << m_id;                                                              // 0
    p << QString::number(m_rxSource);                                       // 1
    p << QString::number(m_dockedLocation.x());                             // 2
    p << QString::number(m_dockedLocation.y());                             // 3
    p << QString::number(m_dockedSize.width());                             // 4
    p << QString::number(m_dockedSize.height());                            // 5
    p << (isFloating() ? QStringLiteral("true") : QStringLiteral("false")); // 6 (Thetis compat)
    p << QString::number(m_delta.x());                                      // 7
    p << QString::number(m_delta.y());                                      // 8
    p << axisLockToString(m_axisLock);                                      // 9
    p << (m_pinOnTop ? QStringLiteral("true") : QStringLiteral("false"));   // 10
    p << (m_border ? QStringLiteral("true") : QStringLiteral("false"));     // 11
    p << m_backgroundColor.name(QColor::HexArgb);                           // 12
    p << (m_noControls ? QStringLiteral("true") : QStringLiteral("false")); // 13
    p << (m_enabled ? QStringLiteral("true") : QStringLiteral("false"));    // 14
    p << m_notes;                                                           // 15
    p << (m_containerMinimises ? QStringLiteral("true") : QStringLiteral("false")); // 16
    p << (m_autoHeight ? QStringLiteral("true") : QStringLiteral("false")); // 17
    p << (m_showOnRx ? QStringLiteral("true") : QStringLiteral("false"));   // 18
    p << (m_showOnTx ? QStringLiteral("true") : QStringLiteral("false"));   // 19
    p << (m_locked ? QStringLiteral("true") : QStringLiteral("false"));     // 20
    p << (m_containerHidesWhenRxNotUsed ? QStringLiteral("true") : QStringLiteral("false")); // 21
    p << (m_hiddenByMacro ? QStringLiteral("true") : QStringLiteral("false")); // 22
    p << dockModeToString(m_dockMode);                                      // 23 (NereusSDR extension)
    return p.join(QLatin1Char('|'));
}

bool ContainerWidget::deserialize(const QString& data)
{
    // Based on Thetis ucMeter.cs:1039-1160, backward-compatible
    if (data.isEmpty()) {
        return false;
    }

    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 13) {
        qCWarning(lcContainer) << "deserialize: too few fields:" << p.size();
        return false;
    }

    if (p[0].isEmpty()) {
        return false;
    }
    setId(p[0]);

    bool ok = false;
    int rx = p[1].toInt(&ok);
    if (!ok) { return false; }
    setRxSource(rx);

    int x = p[2].toInt(&ok); if (!ok) { return false; }
    int y = p[3].toInt(&ok); if (!ok) { return false; }
    int w = p[4].toInt(&ok); if (!ok) { return false; }
    int h = p[5].toInt(&ok); if (!ok) { return false; }
    setDockedLocation(QPoint(x, y));
    setDockedSize(QSize(w, h));

    // Field 6: Thetis floating bool — used as fallback if field 23 absent
    bool thetisFloating = (p[6].toLower() == QStringLiteral("true"));

    x = p[7].toInt(&ok); if (!ok) { return false; }
    y = p[8].toInt(&ok); if (!ok) { return false; }
    setDelta(QPoint(x, y));

    setAxisLock(axisLockFromString(p[9]));
    setPinOnTop(p[10].toLower() == QStringLiteral("true"));
    setBorder(p[11].toLower() == QStringLiteral("true"));

    QColor bgColor(p[12]);
    if (bgColor.isValid()) {
        m_backgroundColor = bgColor;
        m_contentHolder->setStyleSheet(
            QStringLiteral("background: %1;").arg(bgColor.name(QColor::HexArgb)));
    }

    if (p.size() > 13) { setNoControls(p[13].toLower() == QStringLiteral("true")); }
    if (p.size() > 14) { setContainerEnabled(p[14].toLower() == QStringLiteral("true")); }
    if (p.size() > 15) { setNotes(p[15]); }
    if (p.size() > 16) { setContainerMinimises(p[16].toLower() == QStringLiteral("true")); }
    if (p.size() > 17) { setAutoHeight(p[17].toLower() == QStringLiteral("true")); }
    if (p.size() > 19) {
        setShowOnRx(p[18].toLower() == QStringLiteral("true"));
        setShowOnTx(p[19].toLower() == QStringLiteral("true"));
    }
    if (p.size() > 20) { setLocked(p[20].toLower() == QStringLiteral("true")); }
    if (p.size() > 21) { setContainerHidesWhenRxNotUsed(p[21].toLower() == QStringLiteral("true")); }
    if (p.size() > 22) { setHiddenByMacro(p[22].toLower() == QStringLiteral("true")); }

    // Field 23: NereusSDR DockMode extension
    if (p.size() > 23) {
        setDockMode(dockModeFromString(p[23]));
    } else {
        setDockMode(thetisFloating ? DockMode::Floating : DockMode::OverlayDocked);
    }

    qCDebug(lcContainer) << "Deserialized:" << m_id << "rx:" << m_rxSource
                          << "mode:" << dockModeToString(m_dockMode)
                          << "pos:" << m_dockedLocation << "size:" << m_dockedSize;
    return true;
}

} // namespace NereusSDR
