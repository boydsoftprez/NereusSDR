#pragma once

#include <QWidget>
#include <QPoint>
#include <QSize>
#include <QColor>
#include <QString>

class QLabel;
class QPushButton;

namespace NereusSDR {

// From Thetis ucMeter.cs:49-59 — axis lock positions for docked containers.
enum class AxisLock {
    Left = 0, TopLeft, Top, TopRight,
    Right, BottomRight, Bottom, BottomLeft
};

// Docking mode for a container within the application layout.
enum class DockMode {
    PanelDocked,    // In QSplitter (Container #0 default)
    OverlayDocked,  // Absolute position over central widget (Thetis style)
    Floating        // Separate window
};

class FloatingContainer;

class ContainerWidget : public QWidget {
    Q_OBJECT

public:
    static constexpr int kMinContainerWidth = 24;
    static constexpr int kMinContainerHeight = 24;

    explicit ContainerWidget(QWidget* parent = nullptr);
    ~ContainerWidget() override;

    // --- Identity ---
    QString id() const { return m_id; }
    void setId(const QString& id);
    int rxSource() const { return m_rxSource; }
    void setRxSource(int rx);

    // --- Dock Mode ---
    DockMode dockMode() const { return m_dockMode; }
    void setDockMode(DockMode mode);
    bool isFloating() const { return m_dockMode == DockMode::Floating; }
    bool isPanelDocked() const { return m_dockMode == DockMode::PanelDocked; }
    bool isOverlayDocked() const { return m_dockMode == DockMode::OverlayDocked; }

    // --- Axis Lock ---
    AxisLock axisLock() const { return m_axisLock; }
    void setAxisLock(AxisLock lock);
    void cycleAxisLock(bool reverse = false);

    // --- Pin on Top (floating only) ---
    bool isPinOnTop() const { return m_pinOnTop; }
    void setPinOnTop(bool pin);

    // --- Container Properties ---
    bool hasBorder() const { return m_border; }
    void setBorder(bool border);
    bool isLocked() const { return m_locked; }
    void setLocked(bool locked);
    bool isContainerEnabled() const { return m_enabled; }
    void setContainerEnabled(bool enabled);
    bool showOnRx() const { return m_showOnRx; }
    void setShowOnRx(bool show);
    bool showOnTx() const { return m_showOnTx; }
    void setShowOnTx(bool show);
    bool isHiddenByMacro() const { return m_hiddenByMacro; }
    void setHiddenByMacro(bool hidden);
    bool containerMinimises() const { return m_containerMinimises; }
    void setContainerMinimises(bool minimises);
    bool containerHidesWhenRxNotUsed() const { return m_containerHidesWhenRxNotUsed; }
    void setContainerHidesWhenRxNotUsed(bool hides);
    QString notes() const { return m_notes; }
    void setNotes(const QString& notes);
    bool noControls() const { return m_noControls; }
    void setNoControls(bool noControls);
    bool autoHeight() const { return m_autoHeight; }
    void setAutoHeight(bool autoHeight);
    int containerHeight() const { return m_containerHeight; }

    // --- Docked Position (overlay mode only) ---
    QPoint dockedLocation() const { return m_dockedLocation; }
    void setDockedLocation(const QPoint& loc);
    QSize dockedSize() const { return m_dockedSize; }
    void setDockedSize(const QSize& size);
    void storeLocation();
    void restoreLocation();

    // --- Delta (console resize offset for axis-lock) ---
    QPoint delta() const { return m_delta; }
    void setDelta(const QPoint& delta);

    // --- Content Slot ---
    QWidget* content() const { return m_content; }
    void setContent(QWidget* widget);

    // --- Serialization ---
    QString serialize() const;
    bool deserialize(const QString& data);

signals:
    void floatRequested();
    void dockRequested();
    void settingsRequested();
    void dockedMoved();
    void dockModeChanged(DockMode mode);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUI();
    void updateTitleBar();
    void updateTitle();
    void setupBorder();
    void setTopMost();

    void beginDrag(const QPoint& pos);
    void updateDrag(const QPoint& globalPos);
    void endDrag();

    void beginResize(const QPoint& globalPos);
    void updateResize(const QPoint& globalPos);
    void endResize();
    void doResize(int w, int h);

    static int roundToNearestTen(int value);
    static QString axisLockToString(AxisLock lock);
    static AxisLock axisLockFromString(const QString& str);
    static QString dockModeToString(DockMode mode);
    static DockMode dockModeFromString(const QString& str);

    // --- Identity ---
    QString m_id;
    int m_rxSource{1};

    // --- State ---
    DockMode m_dockMode{DockMode::OverlayDocked};
    bool m_dragging{false};
    bool m_resizing{false};
    AxisLock m_axisLock{AxisLock::TopLeft};
    bool m_pinOnTop{false};

    // --- Properties (defaults from Thetis ucMeter.cs:95-103) ---
    bool m_border{true};
    bool m_noControls{false};
    bool m_locked{false};
    bool m_enabled{true};
    bool m_showOnRx{true};
    bool m_showOnTx{true};
    bool m_hiddenByMacro{false};
    bool m_containerMinimises{true};
    bool m_containerHidesWhenRxNotUsed{true};
    QString m_notes;
    int m_containerHeight{kMinContainerHeight};
    bool m_autoHeight{false};
    QColor m_backgroundColor{0x0f, 0x0f, 0x1a};

    // --- Drag/Resize state ---
    QPoint m_dragStartPos;
    QPoint m_resizeStartGlobal;
    QSize m_resizeStartSize;

    // --- Docked geometry ---
    QPoint m_dockedLocation;
    QSize m_dockedSize;
    QPoint m_delta;

    // --- UI elements ---
    QWidget* m_titleBar{nullptr};
    QWidget* m_contentHolder{nullptr};
    QWidget* m_content{nullptr};
    QWidget* m_resizeGrip{nullptr};
    QLabel* m_titleLabel{nullptr};
    QPushButton* m_btnFloat{nullptr};
    QPushButton* m_btnAxis{nullptr};
    QPushButton* m_btnPin{nullptr};
    QPushButton* m_btnSettings{nullptr};
};

} // namespace NereusSDR
