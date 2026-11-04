// =================================================================
// src/gui/applets/SsbModMonitorApplet.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.
//
// SSB Modulation Monitor. Envelope level of a USB or LSB transmission,
// as percent of full scale and dBFS, with peak hold, crest factor, an
// over-threshold flasher and an envelope trace. The analyzer is
// SsbModulationAnalyzer. Readings are live only while the TX slice is
// USB or LSB.
//
// Sources match the AM Mod Monitor: the TX I/Q NereusSDR sends, or the
// PureSignal feedback receiver. A remote window shows the Core's
// readings from txSsbModulation / txSsbModulationFeedback.
//
// Modification history (NereusSDR):
//   2026-11-04 — Created. J.J. Boyd (KG4VCF), AI-assisted via xAI Grok.
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
class QWidget;

namespace NereusSDR {

class HGauge;

class SsbModScopeWidget : public QWidget {
    Q_OBJECT
public:
    explicit SsbModScopeWidget(QWidget* parent = nullptr);
    void setTrace(std::vector<float> pct);
    void setOverThresholdPct(double pct);
    QSize sizeHint() const override { return {240, 96}; }
    QSize minimumSizeHint() const override { return {120, 60}; }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    std::vector<float> m_trace;
    double m_overPct{89.0};
};

class SsbModMonitorApplet : public AppletWidget {
    Q_OBJECT
public:
    enum class Source { TxIq = 0, PaFeedback = 1 };

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

    static QString notConnectedReason();

public slots:
    void setSource(Source s);
    void resetPeaks();

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void buildUI();
    void loadSettings();
    void tick();
    void applySnapshot(const SsbModulationAnalyzer::Snapshot& s);
    bool isRemoteWindow() const;
    bool txIsSsbVoice() const;
    void updateRemoteAvailability();
    void watchOnCore(int source);
    void setLamp(QLabel* lamp, const QString& text, const char* bg, const char* fg, const char* border);

    Source m_source{Source::TxIq};
    QTimer m_timer;
    bool   m_overLit{false};

    QPushButton* m_srcTxBtn{nullptr};
    QPushButton* m_srcFbBtn{nullptr};
    QPushButton* m_resetBtn{nullptr};
    QSpinBox*    m_fbStreamSpin{nullptr};
    HGauge*      m_peakGauge{nullptr};
    HGauge*      m_avgGauge{nullptr};
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
