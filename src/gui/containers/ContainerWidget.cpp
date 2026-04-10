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

// --- Stub implementations for methods declared but not yet implemented ---
// These will be filled in Tasks 3 and 4.

void ContainerWidget::setId(const QString& id) { m_id = id; m_id.remove(QLatin1Char('|')); }
void ContainerWidget::setRxSource(int rx) { m_rxSource = rx; updateTitle(); }
void ContainerWidget::setDockMode(DockMode mode) { if (m_dockMode == mode) return; m_dockMode = mode; updateTitleBar(); emit dockModeChanged(mode); }
void ContainerWidget::setAxisLock(AxisLock) {}
void ContainerWidget::cycleAxisLock(bool) {}
void ContainerWidget::setPinOnTop(bool) {}
void ContainerWidget::setBorder(bool border) { m_border = border; setupBorder(); }
void ContainerWidget::setLocked(bool locked) { m_locked = locked; }
void ContainerWidget::setContainerEnabled(bool enabled) { m_enabled = enabled; }
void ContainerWidget::setShowOnRx(bool show) { m_showOnRx = show; }
void ContainerWidget::setShowOnTx(bool show) { m_showOnTx = show; }
void ContainerWidget::setHiddenByMacro(bool hidden) { m_hiddenByMacro = hidden; }
void ContainerWidget::setContainerMinimises(bool m) { m_containerMinimises = m; }
void ContainerWidget::setContainerHidesWhenRxNotUsed(bool h) { m_containerHidesWhenRxNotUsed = h; }
void ContainerWidget::setNotes(const QString& notes) { m_notes = notes; m_notes.remove(QLatin1Char('|')); updateTitle(); }
void ContainerWidget::setNoControls(bool nc) { m_noControls = nc; }
void ContainerWidget::setAutoHeight(bool ah) { m_autoHeight = ah; }
void ContainerWidget::setDockedLocation(const QPoint& loc) { m_dockedLocation = loc; }
void ContainerWidget::setDockedSize(const QSize& size) { m_dockedSize = size; }
void ContainerWidget::setDelta(const QPoint& delta) { m_delta = delta; }

void ContainerWidget::storeLocation() { m_dockedLocation = pos(); m_dockedSize = size(); }
void ContainerWidget::restoreLocation() {
    if (m_dockedLocation != pos()) { move(m_dockedLocation); }
    if (m_dockedSize != size()) { resize(m_dockedSize); }
}

// Event handlers — stubs for Task 3
void ContainerWidget::mouseMoveEvent(QMouseEvent* event) { QWidget::mouseMoveEvent(event); }
void ContainerWidget::leaveEvent(QEvent* event) { QWidget::leaveEvent(event); }
bool ContainerWidget::eventFilter(QObject* watched, QEvent* event) { return QWidget::eventFilter(watched, event); }
void ContainerWidget::beginDrag(const QPoint&) {}
void ContainerWidget::updateDrag(const QPoint&) {}
void ContainerWidget::endDrag() {}
void ContainerWidget::beginResize(const QPoint&) {}
void ContainerWidget::updateResize(const QPoint&) {}
void ContainerWidget::endResize() {}
void ContainerWidget::doResize(int, int) {}
void ContainerWidget::setTopMost() {}
int ContainerWidget::roundToNearestTen(int value) { return ((value + 5) / 10) * 10; }

// Serialization stubs — Task 4
QString ContainerWidget::axisLockToString(AxisLock) { return QStringLiteral("TOPLEFT"); }
AxisLock ContainerWidget::axisLockFromString(const QString&) { return AxisLock::TopLeft; }
QString ContainerWidget::dockModeToString(DockMode) { return QStringLiteral("OVERLAY"); }
DockMode ContainerWidget::dockModeFromString(const QString&) { return DockMode::OverlayDocked; }
QString ContainerWidget::serialize() const { return QString(); }
bool ContainerWidget::deserialize(const QString&) { return false; }

} // namespace NereusSDR
