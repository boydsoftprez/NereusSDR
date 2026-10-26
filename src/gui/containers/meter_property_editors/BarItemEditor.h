#pragma once
// Modification history (NereusSDR):
//   2026-10-25 - Rows for the BAR options: style (adds Line, Segments,
//                 Solid), value and peak text, markers, peak hold, history,
//                 ignore history, power limit and calibration. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "BaseItemEditor.h"

#include <QColor>
#include <QList>
#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace NereusSDR {

class MeterItem;
class BarItem;

class BarItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit BarItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;
    void buildOptionRows();
    // A colour swatch row that reads and writes one BarItem colour.
    QPushButton* makeColourRow(const QString& label,
                               std::function<QColor(const BarItem&)> get,
                               std::function<void(BarItem&, const QColor&)> set);
    static void paintSwatch(QPushButton* button, const QColor& colour);

    // Enum selectors
    QComboBox*      m_comboOrientation{nullptr};
    QComboBox*      m_comboBarStyle{nullptr};

    // Range
    QDoubleSpinBox* m_spinMinVal{nullptr};
    QDoubleSpinBox* m_spinMaxVal{nullptr};

    // Threshold / smoothing
    QDoubleSpinBox* m_spinRedThreshold{nullptr};
    QDoubleSpinBox* m_spinAttack{nullptr};
    QDoubleSpinBox* m_spinDecay{nullptr};

    // Color buttons — Filled mode
    QPushButton*    m_btnBarColor{nullptr};
    QPushButton*    m_btnBarRedColor{nullptr};

    // Color buttons — Edge mode
    QPushButton*    m_btnEdgeBg{nullptr};
    QPushButton*    m_btnEdgeLow{nullptr};
    QPushButton*    m_btnEdgeHigh{nullptr};
    QPushButton*    m_btnEdgeAvg{nullptr};

    // Value and peak text
    QCheckBox*      m_chkShowValue{nullptr};
    QCheckBox*      m_chkShowPeakValue{nullptr};
    QPushButton*    m_btnFontColour{nullptr};
    QPushButton*    m_btnPeakValueColour{nullptr};

    // Markers
    QCheckBox*      m_chkShowMarker{nullptr};
    QPushButton*    m_btnMarkerColour{nullptr};
    QCheckBox*      m_chkPeakHold{nullptr};
    QPushButton*    m_btnPeakHoldColour{nullptr};

    // History
    QCheckBox*      m_chkShowHistory{nullptr};
    QPushButton*    m_btnHistoryColour{nullptr};
    QSpinBox*       m_spinHistoryMs{nullptr};
    QSpinBox*       m_spinIgnoreHistoryMs{nullptr};

    // Scale
    QDoubleSpinBox* m_spinPowerLimit{nullptr};
    QLineEdit*      m_editCalibration{nullptr};

    // Swatch refreshers, run by setItem().
    QList<std::function<void(const BarItem&)>> m_swatchRefreshers;
};

} // namespace NereusSDR
