#pragma once

#include <QObject>
#include <QList>
#include <QMap>
#include <QSize>

#include <functional>

class QSplitter;
class QWidget;

namespace NereusSDR {

class ContainerWidget;
class FloatingContainer;
class MeterWidget;
enum class DockMode;

// Factory that materializes the inner content widget for a restored
// container. MainWindow registers one so that the panel container gets
// an AppletPanelWidget while user-created containers get a bare
// MeterWidget. If unset, ContainerManager defaults to a fresh
// MeterWidget — sufficient for unit tests and the common case.
using ContainerContentFactory =
    std::function<QWidget*(const QString& id, int rxSource)>;

class ContainerManager : public QObject {
    Q_OBJECT

public:
    explicit ContainerManager(QWidget* dockParent, QSplitter* splitter,
                              QObject* parent = nullptr);
    ~ContainerManager() override;

    // --- Container lifecycle ---
    ContainerWidget* createContainer(int rxSource, DockMode mode);
    void destroyContainer(const QString& id);

    // Create a new floating container that clones the user-editable
    // state of an existing container. Meter items are NOT copied here;
    // block 3's dialog rewrite wires the Duplicate button and will
    // copy items via MeterWidget::serializeItems round-trip.
    // From Thetis MeterManager.cs duplicate-container path.
    ContainerWidget* duplicateContainer(const QString& id);

    // --- Dock mode transitions ---
    void floatContainer(const QString& id);
    void dockContainer(const QString& id);
    void panelDockContainer(const QString& id);
    void overlayDockContainer(const QString& id);
    void recoverContainer(const QString& id);

    // --- Axis-lock repositioning (overlay-docked only) ---
    void updateDockedPositions(int hDelta, int vDelta);

    // --- Panel width (QSplitter state) ---
    void saveSplitterState();
    void restoreSplitterState();

    // --- Persistence ---
    void saveState();
    void restoreState();

    // Register the content factory used by restoreState() to populate
    // each restored container. Must be set before restoreState().
    void setContentFactory(ContainerContentFactory factory);

    // --- Queries ---
    QList<ContainerWidget*> allContainers() const;
    ContainerWidget* container(const QString& id) const;
    ContainerWidget* panelContainer() const;
    int containerCount() const;

    // --- Visibility ---
    void setContainerVisible(const QString& id, bool visible);

signals:
    void containerAdded(const QString& id);
    void containerRemoved(const QString& id);
    // Emitted when a container's notes change (notes are the source
    // of the display title). MainWindow consumes this in commit 45 to
    // rebuild the Containers → Edit Container submenu.
    void containerTitleChanged(const QString& id, const QString& title);
    // Emitted whenever a MeterWidget becomes (or appears as a
    // descendant of) a container's content. MainWindow connects this
    // to MeterPoller::addTarget so user-created and restored
    // containers both flow into the WDSP/MMIO poll loop. Without this
    // hookup the poller only knew about the panel container's initial
    // m_meterWidget and every other container's items sat at default
    // values forever — symptom: bars frozen at frac=0, needles never
    // moving.
    void meterReadyForPolling(MeterWidget* meter);

private:
    void setMeterFloating(ContainerWidget* container, FloatingContainer* form);
    void returnMeterFromFloating(ContainerWidget* container, FloatingContainer* form);
    void wireContainer(ContainerWidget* container);

    QWidget* m_dockParent{nullptr};
    QSplitter* m_splitter{nullptr};
    QMap<QString, ContainerWidget*> m_containers;
    QMap<QString, FloatingContainer*> m_floatingForms;
    QString m_panelContainerId;
    ContainerContentFactory m_contentFactory;
};

} // namespace NereusSDR
