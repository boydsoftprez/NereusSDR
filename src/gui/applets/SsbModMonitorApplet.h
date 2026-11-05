// =================================================================
// src/gui/applets/SsbModMonitorApplet.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.
//
// SSB Modulation Monitor. Envelope level of a USB or LSB transmission,
// as percent of full scale and dBFS, with peak hold, crest factor, an
// over-threshold flasher and an envelope trace. The analyzer is
// SsbModulationAnalyzer. Readings are live only while keyed in USB or LSB.
// The panel starts off; nothing is measured or watched until it is shown.
//
// Sources match the AM Mod Monitor: the TX I/Q NereusSDR sends, or the
// PureSignal feedback, which needs PureSignal running: without it PA FB is
// disabled with the reason. A remote window shows the Core's readings from
// txSsbModulation / txSsbModulationFeedback.
//
// Modification history (NereusSDR):
//   2026-11-04 — Created. Brian Sutton (W4KZO), AI-assisted via xAI Grok.
//   2026-11-05 - Corrections for review findings.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once

#include "AppletWidget.h"
#include "core/SsbModulationAnalyzer.h"

#include <QPaintEvent>
#include <QShowEvent>
#include <QTimer>
#include <vector>

class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QWidget;

namespace NereusSDR {

class HGauge;
class VintageModMeterWidget;

class SsbModScopeWidget : public QWidget {
    Q_OBJECT
public:
    explicit SsbModScopeWidget(QWidget* parent = nullptr);
    void setTrace(std::vector<float> pct);
    void setOverThresholdPct(double pct);
    /// The AM monitor's green screen: 10 columns by 6 rows, with the
    /// envelope drawn across it.
    void setVintage(bool on);
    std::size_t tracePoints() const { return m_trace.size(); }
    QSize sizeHint() const override { return {240, 140}; }
    QSize minimumSizeHint() const override { return {120, 120}; }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    std::vector<float> m_trace;
    double m_overPct{89.0};
    bool m_vintage{true};
};

class SsbModMonitorApplet : public AppletWidget {
    Q_OBJECT
public:
    enum class Source { TxIq = 0, PaFeedback = 1 };
    /// In the window's panels list, for a profile with no saved choice.
    static constexpr bool kShownByDefault = false;

    explicit SsbModMonitorApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("ssb_mod_monitor"); }
    QString appletTitle() const override { return QStringLiteral("SSB Mod Monitor"); }
    void    syncFromModel() override;
    bool    canFloat() const override { return true; }

    Source source() const noexcept { return m_source; }
    double overThresholdDbfs() const;

    void applySnapshotForTest(const SsbModulationAnalyzer::Snapshot& s) { applySnapshot(s); }
    void tickForTest() { tick(); }
    bool monitorAvailableForTest() const { return m_available; }
    QString unavailableReasonForTest() const;
    QString lampTextForTest() const;
    QString crestTextForTest() const;
    bool paFeedbackEnabledForTest() const;
    QString paFeedbackToolTipForTest() const;
    std::size_t scopePointsForTest() const;

    static QString notConnectedReason();
    static QString paFeedbackOffReason();

public slots:
    void setSource(Source s);
    void resetPeaks();
    /// The illuminated analog faces, the same pair the AM monitor uses.
    void setVintageMeters(bool on);

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void buildUI();
    void loadSettings();
    void tick();
    void applySnapshot(const SsbModulationAnalyzer::Snapshot& s);
    bool isRemoteWindow() const;
    void updatePaFeedbackAvailability();
    void syncLocalMeasuring(bool shown);
    void restartLocalWindow();
    void updateRemoteAvailability();
    void watchOnCore(int source);
    void setLamp(QLabel* lamp, const QString& text, const char* bg, const char* fg, const char* border);

    Source m_source{Source::TxIq};
    QTimer m_timer;
    bool   m_overLit{false};
    bool   m_vintage{true};

    QPushButton* m_srcTxBtn{nullptr};
    QPushButton* m_srcFbBtn{nullptr};
    QPushButton* m_resetBtn{nullptr};
    QPushButton* m_vuBtn{nullptr};
    QStackedWidget* m_meterStack{nullptr};
    HGauge*      m_peakGauge{nullptr};
    HGauge*      m_avgGauge{nullptr};
    VintageModMeterWidget* m_peakMeter{nullptr};
    VintageModMeterWidget* m_avgMeter{nullptr};
    QLabel*      m_peakValue{nullptr};
    QLabel*      m_avgValue{nullptr};
    QLabel*      m_crestValue{nullptr};
    QLabel*      m_pctValue{nullptr};
    QLabel*      m_overFlasher{nullptr};
    QSpinBox*    m_overSpin{nullptr};
    QLabel*      m_lamp{nullptr};
    SsbModScopeWidget* m_scope{nullptr};
    QWidget*     m_body{nullptr};
    QLabel*      m_unavailableLabel{nullptr};
    bool         m_available{true};
};

} // namespace NereusSDR
