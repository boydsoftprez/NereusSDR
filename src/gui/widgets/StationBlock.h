// src/gui/widgets/StationBlock.h
#pragma once

#include <QWidget>
#include <QString>

class QLabel;

namespace NereusSDR {

// StationBlock — bordered cyan box anchoring the connected radio's name.
// Click → emits clicked() (host opens ConnectionPanel).
// Right-click → emits contextMenuRequested(globalPos) (host shows menu
// with Disconnect / Edit / Forget per design spec §STATION block).
// When disconnected, the block paints a dashed-red border with italic
// gray "Click to connect" placeholder text.
class StationBlock : public QWidget {
    Q_OBJECT

public:
    explicit StationBlock(QWidget* parent = nullptr);

    // Set the radio name shown in the block. Empty → disconnected mode.
    void setRadioName(const QString& name);
    QString radioName() const noexcept { return m_radioName; }

    bool isConnectedAppearance() const noexcept { return !m_radioName.isEmpty(); }

    // Second row: "ANAN-G2 · v27". Either part may be empty; the middle dot
    // appears only when both are present. Cleared automatically whenever the
    // radio name is cleared, so the disconnected placeholder never shows a
    // stale board name.
    void setHardwareLine(const QString& model, const QString& firmware);
    QString hardwareLine() const noexcept { return m_hardwareLine; }

signals:
    void clicked();
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void applyStyle();

    QString  m_radioName;
    QLabel*  m_label{nullptr};
    QString  m_hardwareLine;
    QLabel*  m_hardwareLabel{nullptr};
};

} // namespace NereusSDR
