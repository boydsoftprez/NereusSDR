// =================================================================
// src/gui/applets/ModMonitorApplet.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.
//
// AM Modulation Monitor applet.  Display set modelled on a hardware AM
// modulation monitor: separate positive (0..160 %) and negative
// (0..100 %) peak-reading bar meters with peak hold, an asymmetry
// readout, adjustable positive / negative peak flashers, carrier
// low / high / absent lamps, and an oscilloscope-style envelope trace.
//
// Data comes from RadioModel's two AmModulationAnalyzer instances:
//   TX I/Q  — the modulation NereusSDR actually sends to the radio
//   PA FB   — the PureSignal feedback receiver (the PA output), when
//             PureSignal feedback is running on the connected board
//
// Modification history (NereusSDR):
//   2026-09-08 — Created (Lee, AI-assisted via Anthropic Claude Code).
// =================================================================
#pragma once

#include "AppletWidget.h"
#include "core/AmModulationAnalyzer.h"

#include <QTimer>
#include <vector>

class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace NereusSDR {

class HGauge;
class VintageModMeterWidget;
class AsymmetryBarWidget;

/// Envelope trace: % modulation vs time, with 0 / ±100 / +125 gridlines.
class ModScopeWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModScopeWidget(QWidget* parent = nullptr);
    void setTrace(std::vector<float> pct);
    void setFlashThresholds(double posPct, double negPct);
    /// Vintage look: dark-green graticule with a mirrored, filled envelope.
    void setVintage(bool on);
    QSize sizeHint() const override { return {240, 96}; }
    QSize minimumSizeHint() const override { return {120, 60}; }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    std::vector<float> m_trace;
    double m_posFlash{125.0};
    double m_negFlash{95.0};
    bool   m_vintage{false};
};

class ModMonitorApplet : public AppletWidget {
    Q_OBJECT
public:
    enum class Source { TxIq = 0, PaFeedback = 1 };

    explicit ModMonitorApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("mod_monitor"); }
    QString appletTitle() const override { return QStringLiteral("AM Mod Monitor"); }
    void    syncFromModel() override;
    bool    canFloat() const override { return true; }

    Source source() const noexcept { return m_source; }
    bool   vintageMeters() const noexcept { return m_vintage; }
    double posFlashPct() const;
    double negFlashPct() const;

    /// Test seam: push a snapshot through the display path without a model.
    void applySnapshotForTest(const AmModulationAnalyzer::Snapshot& s) { applySnapshot(s); }
    QString carrierLampTextForTest() const;
    bool posFlasherLitForTest() const noexcept { return m_posLit; }
    bool negFlasherLitForTest() const noexcept { return m_negLit; }

public slots:
    void setSource(Source s);
    void resetPeaks();
    /// Swap the bar graphs for the illuminated analog meter pair.
    void setVintageMeters(bool on);

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void buildUI();
    void loadSettings();
    void tick();
    void applySnapshot(const AmModulationAnalyzer::Snapshot& s);
    void setLamp(QLabel* lamp, const QString& text, const char* bg, const char* fg, const char* border);

    Source  m_source{Source::TxIq};
    bool    m_vintage{false};
    QTimer  m_timer;
    bool    m_posLit{false};
    bool    m_negLit{false};

    QPushButton* m_srcTxBtn{nullptr};
    QPushButton* m_srcFbBtn{nullptr};
    QPushButton* m_resetBtn{nullptr};
    QPushButton* m_vuBtn{nullptr};
    QStackedWidget* m_meterStack{nullptr};
    VintageModMeterWidget* m_negMeter{nullptr};
    VintageModMeterWidget* m_posMeter{nullptr};
    AsymmetryBarWidget*    m_asymBar{nullptr};
    QSpinBox*    m_fbStreamSpin{nullptr};
    HGauge*      m_posGauge{nullptr};
    HGauge*      m_negGauge{nullptr};
    QLabel*      m_posValue{nullptr};
    QLabel*      m_negValue{nullptr};
    QLabel*      m_asymValue{nullptr};
    QLabel*      m_carrierValue{nullptr};
    QLabel*      m_posFlasher{nullptr};
    QLabel*      m_negFlasher{nullptr};
    QSpinBox*    m_posFlashSpin{nullptr};
    QSpinBox*    m_negFlashSpin{nullptr};
    QLabel*      m_carrierLamp{nullptr};
    ModScopeWidget* m_scope{nullptr};
};

} // namespace NereusSDR
