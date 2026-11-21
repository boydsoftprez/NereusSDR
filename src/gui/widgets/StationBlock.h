// src/gui/widgets/StationBlock.h
#pragma once

#include <QWidget>
#include <QString>
#include "gui/NereusGuiExport.h"

class QLabel;

namespace NereusSDR {

// StationBlock — bordered cyan box anchoring the connected radio's name.
// Click → emits clicked() (host opens ConnectionPanel).
// Right-click → emits contextMenuRequested(globalPos) (host shows menu
// with Disconnect / Edit / Forget per design spec §STATION block).
// When disconnected, the block paints a dashed-red border with italic
// gray "Click to connect" placeholder text.
class NEREUS_GUI_EXPORT StationBlock : public QWidget {
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
    // The small Anvelina bit-depth chip beside the radio name: "24-bit" in
    // green when the PRO3 hardware revision says the 24-bit path is active,
    // "16-bit" in grey for the legacy path. An empty text hides it.
    void setBitDepthBadge(const QString& text, bool active, const QString& tooltip = {});
    QString bitDepthText() const { return m_bitDepthText; }
    void setConnectionLines(const QString& controls, const QString& audio);
    QSize sizeHint() const override;

signals:
    void clicked();
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyStyle();
    void updateElision();
    void updateMinimumWidth();

    QString  m_radioName;
    QLabel*  m_label{nullptr};
    QString  m_hardwareLine;
    QLabel*  m_hardwareLabel{nullptr};
    QString  m_bitDepthText;
    QLabel*  m_bitDepthLabel{nullptr};
    bool m_corePresentation = false;
    QString m_audioLine;
    QLabel* m_audioLabel{nullptr};
};

} // namespace NereusSDR
