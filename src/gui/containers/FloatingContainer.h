#pragma once

#include <QWidget>
#include <QString>

namespace NereusSDR {

class ContainerWidget;

class FloatingContainer : public QWidget {
    Q_OBJECT

public:
    explicit FloatingContainer(int rxSource, QWidget* parent = nullptr);
    ~FloatingContainer() override;

    QString id() const { return m_id; }
    void setId(const QString& id);

    int rxSource() const { return m_rxSource; }

    // From Thetis frmMeterDisplay.cs:168-179
    void takeOwner(ContainerWidget* container);

    bool containerMinimises() const { return m_containerMinimises; }
    void setContainerMinimises(bool minimises);
    bool containerHidesWhenRxNotUsed() const { return m_containerHidesWhenRxNotUsed; }
    void setContainerHidesWhenRxNotUsed(bool hides);

    bool isFormEnabled() const { return m_formEnabled; }
    void setFormEnabled(bool enabled);

    bool isHiddenByMacro() const { return m_hiddenByMacro; }
    void setHiddenByMacro(bool hidden);

    bool isContainerFloating() const { return m_floating; }
    void setContainerFloating(bool floating);

    // From Thetis frmMeterDisplay.cs:114-139
    void onConsoleWindowStateChanged(Qt::WindowStates state, bool rx2Enabled);

    void saveGeometry();
    void restoreGeometry();

signals:
    void aboutToClose();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // Collapse the top-level window to title-bar height when the owner
    // ContainerWidget enters the minimised runtime state, restore the
    // previous height when it leaves. From Thetis frmMeterDisplay
    // minimise path (MeterManager.cs ContainerMinimised handler).
    void onOwnerMinimisedChanged(bool minimised);

private:
    void updateTitle();

    QString m_id;
    int m_rxSource{1};
    bool m_containerMinimises{true};
    bool m_containerHidesWhenRxNotUsed{true};
    bool m_formEnabled{true};
    bool m_floating{false};
    bool m_hiddenByMacro{false};
    int  m_unminimisedHeight{0};   // cached height before minimise collapse
};

} // namespace NereusSDR
