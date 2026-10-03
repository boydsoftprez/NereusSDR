#pragma once
// no-port-check: NereusSDR-original remote telemetry presentation adapter.

#include <QDialog>
#include <QPointer>
#include <QTimer>

class QComboBox;
class QLabel;
class QShowEvent;
class QHideEvent;

namespace NereusSDR {

class RemoteTelemetryController;
class TimeSeriesGraphWidget;

// Presents controller-owned remote telemetry history. The dialog never owns
// samples, so closing it only stops rendering and leaves collection intact.
class RemoteDiagnosticsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit RemoteDiagnosticsDialog(RemoteTelemetryController* controller,
                                     QWidget* parent = nullptr);

    int rangeSeconds() const { return m_rangeSeconds; }
    RemoteTelemetryController* controller() const;

private slots:
    void refresh();
    void setRangeFromSelector(int index);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void buildUi();
    QWidget* buildTab(const QString& title);
    TimeSeriesGraphWidget* addGraph(QWidget* tab, const QString& title,
                                    const QString& suffix);
    void refreshGraphs();
    void refreshCoreHostGraphs();
    void refreshCoreReceiverGraph();
    void refreshDetail();

    QPointer<RemoteTelemetryController> m_controller;
    QTimer m_refreshTimer;
    QComboBox* m_rangeSelector{nullptr};
    QLabel* m_detailLabel{nullptr};
    TimeSeriesGraphWidget* m_totalTrafficGraph{nullptr};
    TimeSeriesGraphWidget* m_audioTrafficGraph{nullptr};
    TimeSeriesGraphWidget* m_speakerBufferGraph{nullptr};
    TimeSeriesGraphWidget* m_radioLinkGraph{nullptr};
    TimeSeriesGraphWidget* m_controlPayloadGraph{nullptr};
    TimeSeriesGraphWidget* m_roundTripGraph{nullptr};
    TimeSeriesGraphWidget* m_packetAgeGraph{nullptr};
    TimeSeriesGraphWidget* m_audioDelayGraph{nullptr};
    TimeSeriesGraphWidget* m_audioPacketsGraph{nullptr};
    TimeSeriesGraphWidget* m_sourceFramesGraph{nullptr};
    TimeSeriesGraphWidget* m_audioEventsGraph{nullptr};
    QLabel* m_coreHostUnavailableLabel{nullptr};
    TimeSeriesGraphWidget* m_coreCpuGraph{nullptr};
    TimeSeriesGraphWidget* m_coreMemoryGraph{nullptr};
    TimeSeriesGraphWidget* m_coreTemperatureGraph{nullptr};
    QLabel* m_coreReceiversUnavailableLabel{nullptr};
    TimeSeriesGraphWidget* m_coreReceiverGraph{nullptr};
    int m_rangeSeconds{5 * 60};
};

} // namespace NereusSDR
