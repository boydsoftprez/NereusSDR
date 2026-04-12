#include "FloatingContainer.h"
#include "ContainerWidget.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"

#include <QCloseEvent>
#include <QVBoxLayout>

namespace NereusSDR {

FloatingContainer::FloatingContainer(int rxSource, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_rxSource(rxSource)
{
    setMinimumSize(ContainerWidget::kMinContainerWidth,
                   ContainerWidget::kMinContainerHeight);
    setStyleSheet(QStringLiteral("background: #0f0f1a;"));
    updateTitle();
    qCDebug(lcContainer) << "FloatingContainer created for RX" << rxSource;
}

FloatingContainer::~FloatingContainer()
{
    qCDebug(lcContainer) << "FloatingContainer destroyed:" << m_id;
}

void FloatingContainer::setId(const QString& id)
{
    m_id = id;
    // From Thetis frmMeterDisplay.cs:150-156 — setting ID triggers geometry restore
    restoreGeometry();
    updateTitle();
}

void FloatingContainer::takeOwner(ContainerWidget* container)
{
    // From Thetis frmMeterDisplay.cs:168-179
    m_containerMinimises = container->containerMinimises();
    m_formEnabled = container->isContainerEnabled();

    container->setParent(this);
    if (!layout()) {
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
    }
    // Remove any existing widgets from layout
    QLayoutItem* child = nullptr;
    while ((child = layout()->takeAt(0)) != nullptr) {
        delete child;
    }
    layout()->addWidget(container);
    container->show();
    container->raise();

    // Phase 3G-6 block 2: listen for the runtime minimised flag on
    // the owner so this top-level window can collapse to the title
    // bar and restore on un-minimise. Title-bar visibility is handled
    // entirely inside ContainerWidget and needs no FloatingContainer
    // involvement (the window just shrinks/grows to fit content).
    connect(container, &ContainerWidget::minimisedChanged, this,
            &FloatingContainer::onOwnerMinimisedChanged,
            Qt::UniqueConnection);

    qCDebug(lcContainer) << "FloatingContainer" << m_id
                          << "took ownership of" << container->id();
}

void FloatingContainer::onOwnerMinimisedChanged(bool minimised)
{
    if (minimised) {
        m_unminimisedHeight = height();
        // Collapse to just the title bar (plus frame margins). The
        // minimum is the inner title-bar height; give it a few pixels
        // slack for borders.
        const int collapsed = ContainerWidget::kTitleBarHeight + 4;
        resize(width(), collapsed);
    } else {
        if (m_unminimisedHeight > 0) {
            resize(width(), m_unminimisedHeight);
        }
        m_unminimisedHeight = 0;
    }
}

void FloatingContainer::onConsoleWindowStateChanged(Qt::WindowStates state, bool rx2Enabled)
{
    // From Thetis frmMeterDisplay.cs:114-139
    if (m_formEnabled && m_floating && m_containerMinimises) {
        if (state & Qt::WindowMinimized) {
            hide();
        } else {
            bool shouldShow = false;
            if (m_rxSource == 1) {
                shouldShow = !m_hiddenByMacro;
            } else if (m_rxSource == 2) {
                if (rx2Enabled || !m_containerHidesWhenRxNotUsed) {
                    shouldShow = !m_hiddenByMacro;
                }
            }
            if (shouldShow) {
                show();
            }
        }
    }
}

void FloatingContainer::closeEvent(QCloseEvent* event)
{
    // From Thetis frmMeterDisplay.cs:158-166 — hide instead of close
    if (event->spontaneous()) {
        hide();
        event->ignore();
        return;
    }
    saveGeometry();
    QWidget::closeEvent(event);
}

void FloatingContainer::saveGeometry()
{
    auto& s = AppSettings::instance();
    QRect r = geometry();
    s.setValue(QStringLiteral("MeterDisplay_%1_Geometry").arg(m_id),
              QStringLiteral("%1,%2,%3,%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()));
}

void FloatingContainer::restoreGeometry()
{
    auto& s = AppSettings::instance();
    QString val = s.value(QStringLiteral("MeterDisplay_%1_Geometry").arg(m_id)).toString();
    if (val.isEmpty()) {
        return;
    }
    QStringList parts = val.split(QLatin1Char(','));
    if (parts.size() != 4) {
        return;
    }
    bool ok1, ok2, ok3, ok4;
    int x = parts[0].toInt(&ok1);
    int y = parts[1].toInt(&ok2);
    int w = parts[2].toInt(&ok3);
    int h = parts[3].toInt(&ok4);
    if (ok1 && ok2 && ok3 && ok4) {
        setGeometry(x, y, w, h);
    }
}

void FloatingContainer::updateTitle()
{
    // From Thetis frmMeterDisplay.cs:140-144 — unique title for OBS/streaming
    uint hash = qHash(m_id) % 100000;
    setWindowTitle(QStringLiteral("NereusSDR Meter [%1]").arg(hash, 5, 10, QLatin1Char('0')));
}

void FloatingContainer::setContainerMinimises(bool minimises) { m_containerMinimises = minimises; }
void FloatingContainer::setContainerHidesWhenRxNotUsed(bool hides) { m_containerHidesWhenRxNotUsed = hides; }
void FloatingContainer::setFormEnabled(bool enabled) { m_formEnabled = enabled; }
void FloatingContainer::setHiddenByMacro(bool hidden) { m_hiddenByMacro = hidden; }
void FloatingContainer::setContainerFloating(bool floating) { m_floating = floating; }

} // namespace NereusSDR
