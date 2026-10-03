// =================================================================
// src/gui/RemoteDiagnosticsDialog.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original remote telemetry presentation adapter.
// It consumes the separately attributed, protocol-neutral
// TimeSeriesGraphWidget and TelemetryHistory port without adding protocol or
// collection behavior here.
// =================================================================

#include "gui/RemoteDiagnosticsDialog.h"

#include "gui/RemoteTelemetryController.h"
#include "gui/TelemetryHistory.h"
#include "gui/TimeSeriesGraphWidget.h"

#include <QColor>
#include <QComboBox>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>

namespace NereusSDR {
namespace {

using Metric = TelemetryHistory::Metric;

struct GraphSeriesSpec {
    Metric metric;
    const char* label;
    const char* color;
    const char* unit;
};

TimeSeriesGraphWidget::Series toGraphSeries(const TelemetryHistory& history,
                                             Metric metric,
                                             const GraphSeriesSpec& spec,
                                             qint64 nowMs, int rangeSeconds, double scale = 1.0)
{
    const TelemetryHistory::Series source = history.series(metric, nowMs, rangeSeconds);
    TimeSeriesGraphWidget::Series result;
    result.label = QObject::tr(spec.label);
    result.color = QColor(QLatin1String(spec.color));
    result.unitSuffix = QString::fromUtf8(spec.unit);
    result.maxConnectGapSeconds = source.maxConnectGapSeconds;
    result.points.reserve(source.points.size());
    result.breakBefore.reserve(source.points.size());
    for (const TelemetryHistory::Point& point : source.points) {
        result.points.append({point.seconds, point.value * scale});
        result.breakBefore.append(point.breakBefore);
    }
    return result;
}

void setGraph(TimeSeriesGraphWidget* graph, const TelemetryHistory& history,
              qint64 nowMs, int rangeSeconds,
              std::initializer_list<GraphSeriesSpec> specs, double scale = 1.0)
{
    QVector<TimeSeriesGraphWidget::Series> series;
    series.reserve(static_cast<qsizetype>(specs.size()));
    for (const GraphSeriesSpec& spec : specs) {
        series.append(toGraphSeries(history, spec.metric, spec, nowMs, rangeSeconds, scale));
    }
    graph->setSeries(std::move(series), rangeSeconds);
}

} // namespace

RemoteDiagnosticsDialog::RemoteDiagnosticsDialog(RemoteTelemetryController* controller,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_controller(controller)
{
    setWindowTitle(tr("Remote Network Diagnostics"));
    setMinimumSize(760, 620);
    resize(980, 760);

    buildUi();
    m_refreshTimer.setInterval(1000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &RemoteDiagnosticsDialog::refresh);
    if (controller) {
        // Controller sampling can notify at a higher cadence. It remains the
        // owner of data; this dialog deliberately renders only on its 1 Hz
        // visible timer.
        connect(controller, &QObject::destroyed, this, [this] {
            m_controller = nullptr;
            if (isVisible()) {
                refreshDetail();
            }
        });
    }
}

RemoteTelemetryController* RemoteDiagnosticsDialog::controller() const
{
    return m_controller.data();
}

void RemoteDiagnosticsDialog::buildUi()
{
    setStyleSheet(QStringLiteral(
        "QDialog { background: #0f0f1a; }"
        "QTabWidget::pane { border: 1px solid #203040; }"
        "QTabBar::tab { background: #172535; color: #8aa8c0; padding: 6px 12px; }"
        "QTabBar::tab:selected { background: #203b52; color: #c8d8e8; }"
        "QLabel { color: #c8d8e8; }"
        "QComboBox { background: #172535; color: #c8d8e8; border: 1px solid #203040; padding: 3px 6px; }"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);

    auto* controls = new QHBoxLayout;
    auto* rangeLabel = new QLabel(tr("History:"), this);
    controls->addWidget(rangeLabel);
    m_rangeSelector = new QComboBox(this);
    m_rangeSelector->setObjectName(QStringLiteral("remoteDiagnosticsRange"));
    const std::initializer_list<std::pair<const char*, int>> ranges{
        {"1 min", 60}, {"5 min", 5 * 60}, {"15 min", 15 * 60},
        {"1 h", 60 * 60}, {"24 h", 24 * 60 * 60}, {"7 d", 7 * 24 * 60 * 60},
    };
    for (const auto& [label, seconds] : ranges) {
        m_rangeSelector->addItem(tr(label), seconds);
    }
    m_rangeSelector->setCurrentIndex(m_rangeSelector->findData(m_rangeSeconds));
    controls->addWidget(m_rangeSelector);
    controls->addStretch();
    root->addLayout(controls);
    connect(m_rangeSelector, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &RemoteDiagnosticsDialog::setRangeFromSelector);

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("remoteDiagnosticsTabs"));
    QWidget* connection = buildTab(tr("Connection"));
    m_totalTrafficGraph = addGraph(connection, tr("Total traffic between the Core and this app"), tr(" kbps"));
    m_totalTrafficGraph->setObjectName(QStringLiteral("remoteTotalTrafficGraph"));
    m_totalTrafficGraph->setToolTip(tr("Control, display and audio traffic seen at this computer, audio included whether Opus or lossless. Excludes encryption, VPN and network overhead; outgoing audio and display count what this app handed to the network, not confirmed delivery."));
    m_radioLinkGraph = addGraph(connection, tr("Radio link throughput"), tr(" Mbps"));
    m_radioLinkGraph->setObjectName(QStringLiteral("remoteRadioLinkGraph"));
    m_controlPayloadGraph = addGraph(connection, tr("Control traffic"), tr(" kbit/s"));
    m_controlPayloadGraph->setObjectName(QStringLiteral("remoteControlPayloadGraph"));
    tabs->addTab(connection, tr("Connection"));

    QWidget* roundTrip = buildTab(tr("Round trip"));
    m_roundTripGraph = addGraph(roundTrip, tr("Round-trip time"), tr(" ms"));
    m_roundTripGraph->setObjectName(QStringLiteral("remoteRoundTripGraph"));
    m_roundTripGraph->setToolTip(tr("RTT graphs hold the last measurement between pings; age advances independently and stale values disappear."));
    m_speakerBufferGraph = addGraph(roundTrip, tr("Speaker buffering on this computer (not total delay)"), tr(" ms"));
    m_speakerBufferGraph->setObjectName(QStringLiteral("remoteSpeakerBufferGraph"));
    m_speakerBufferGraph->setToolTip(tr("Audio waiting for this computer's speaker only. Excludes network, encoder, arrival smoothing and audio-device delay."));
    m_packetAgeGraph = addGraph(roundTrip, tr("Time since the last audio packet"), tr(" ms"));
    m_packetAgeGraph->setObjectName(QStringLiteral("remotePacketAgeGraph"));
    // R-R3-35: measured, not half a round trip. A Core that does not answer
    // clock probes leaves it empty.
    m_audioDelayGraph = addGraph(roundTrip, tr("Audio delay"), tr(" ms"));
    m_audioDelayGraph->setObjectName(QStringLiteral("remoteAudioDelayGraph"));
    m_audioDelayGraph->setToolTip(tr("How far behind the Core's audio this computer plays it. Delivery is the part up to this computer's player, before the speaker queue. Accuracy is how well this computer knows the Core's clock, not a delay. When the speaker device does not report its own delay, it is not counted."));
    tabs->addTab(roundTrip, tr("Round trip / buffering"));

    QWidget* audio = buildTab(tr("Audio"));
    m_audioTrafficGraph = addGraph(audio, tr("Audio traffic received at this computer"), tr(" kbps"));
    m_audioTrafficGraph->setObjectName(QStringLiteral("remoteAudioTrafficGraph"));
    m_audioTrafficGraph->setToolTip(tr("Audio packets counts every audio packet received, with its packet header. Audio content is the sound alone, Opus or lossless, whichever the Core is sending. Both are part of the total traffic. Network overhead is excluded. No audio is sent to the Core in receive-only mode."));
    m_audioPacketsGraph = addGraph(audio, tr("Audio packet activity"), tr(" packets/s"));
    m_audioPacketsGraph->setObjectName(QStringLiteral("remoteAudioPacketsGraph"));
    m_sourceFramesGraph = addGraph(audio, tr("Core source frames"), tr(" frames/s"));
    m_sourceFramesGraph->setObjectName(QStringLiteral("remoteSourceFramesGraph"));
    m_audioEventsGraph = addGraph(audio, tr("Audio interruption events"), tr(" events/s"));
    m_audioEventsGraph->setObjectName(QStringLiteral("remoteAudioEventsGraph"));
    tabs->addTab(audio, tr("Audio"));

    // R-R3-32/33: the Core computer's own load. An older or non-Linux Core
    // does not send it; the tab then says so instead of drawing empty graphs.
    QWidget* core = buildTab(tr("Core"));
    auto* coreLayout = qobject_cast<QVBoxLayout*>(qobject_cast<QScrollArea*>(core)->widget()->layout());
    m_coreHostUnavailableLabel = new QLabel(tr("This Core does not report computer load."),
                                            qobject_cast<QScrollArea*>(core)->widget());
    m_coreHostUnavailableLabel->setObjectName(QStringLiteral("remoteCoreHostUnavailable"));
    m_coreHostUnavailableLabel->setWordWrap(true);
    coreLayout->insertWidget(coreLayout->count() - 1, m_coreHostUnavailableLabel);
    m_coreCpuGraph = addGraph(core, tr("Core computer CPU"), tr("\u00A0%"));
    m_coreCpuGraph->setObjectName(QStringLiteral("remoteCoreCpuGraph"));
    m_coreCpuGraph->setToolTip(tr("System is the whole Core computer. NereusSDR Core is its share of all processors together, so 100\u00A0% means every processor is busy."));
    m_coreMemoryGraph = addGraph(core, tr("Core computer memory"), tr("\u00A0MiB"));
    m_coreMemoryGraph->setObjectName(QStringLiteral("remoteCoreMemoryGraph"));
    m_coreMemoryGraph->setToolTip(tr("Available is the memory the Core computer can still give to programs. Used by NereusSDR Core is the memory it holds."));
    m_coreTemperatureGraph = addGraph(core, tr("Core computer temperature"), tr("\u00A0°C"));
    m_coreTemperatureGraph->setObjectName(QStringLiteral("remoteCoreTemperatureGraph"));
    m_coreTemperatureGraph->setToolTip(tr("The hottest temperature sensor on the Core computer."));
    m_coreCpuGraph->setVisible(false);
    m_coreMemoryGraph->setVisible(false);
    m_coreTemperatureGraph->setVisible(false);
    // R-R3-40: how hard each receiver works on the Core. An older Core does
    // not send it; the tab then says so, as it does for computer load.
    m_coreReceiversUnavailableLabel = new QLabel(
        tr("This Core does not report receiver processing."),
        qobject_cast<QScrollArea*>(core)->widget());
    m_coreReceiversUnavailableLabel->setObjectName(QStringLiteral("remoteCoreReceiversUnavailable"));
    m_coreReceiversUnavailableLabel->setWordWrap(true);
    coreLayout->insertWidget(coreLayout->count() - 1, m_coreReceiversUnavailableLabel);
    m_coreReceiverGraph = addGraph(core, tr("Receiver processing"), tr("\u00A0%"));
    m_coreReceiverGraph->setObjectName(QStringLiteral("remoteCoreReceiverGraph"));
    m_coreReceiverGraph->setToolTip(tr("How long the Core computer takes to process each receiver's signal, as a share of real time. Below 100\u00A0% the receiver keeps up. At 100\u00A0% or more it cannot keep up, and the Core skips some of that receiver's signal to catch up, which you may hear as gaps."));
    m_coreReceiverGraph->setReferenceLine(100.0, tr("Cannot keep up"));
    m_coreReceiverGraph->setVisible(false);
    tabs->addTab(core, tr("Core"));
    root->addWidget(tabs, 1);

    auto* detailScroll = new QScrollArea(this);
    detailScroll->setWidgetResizable(true);
    detailScroll->setMinimumHeight(110);
    detailScroll->setMaximumHeight(170);
    detailScroll->setStyleSheet(QStringLiteral("QScrollArea { border: 1px solid #203040; background: #101a26; }"));
    m_detailLabel = new QLabel(detailScroll);
    m_detailLabel->setObjectName(QStringLiteral("remoteDiagnosticsDetail"));
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_detailLabel->setContentsMargins(8, 6, 8, 6);
    detailScroll->setWidget(m_detailLabel);
    root->addWidget(detailScroll);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    for (QAbstractButton* button : buttons->buttons()) {
        if (auto* pushButton = qobject_cast<QPushButton*>(button)) {
            pushButton->setAutoDefault(false);
        }
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    root->addWidget(buttons);
}

QWidget* RemoteDiagnosticsDialog::buildTab(const QString& title)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setObjectName(title);
    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

TimeSeriesGraphWidget* RemoteDiagnosticsDialog::addGraph(QWidget* tab, const QString& title,
                                                          const QString& suffix)
{
    auto* scroll = qobject_cast<QScrollArea*>(tab);
    auto* graph = new TimeSeriesGraphWidget(title, suffix, scroll->widget());
    auto* layout = qobject_cast<QVBoxLayout*>(scroll->widget()->layout());
    layout->insertWidget(layout->count() - 1, graph);
    return graph;
}

void RemoteDiagnosticsDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    refresh(); // Show current data immediately; later renders are limited to 1 Hz.
    m_refreshTimer.start();
}

void RemoteDiagnosticsDialog::hideEvent(QHideEvent* event)
{
    m_refreshTimer.stop();
    QDialog::hideEvent(event);
}

void RemoteDiagnosticsDialog::refresh()
{
    if (!isVisible()) {
        return;
    }
    refreshGraphs();
    refreshDetail();
}

void RemoteDiagnosticsDialog::setRangeFromSelector(int index)
{
    if (index < 0 || !m_rangeSelector) {
        return;
    }
    const int range = m_rangeSelector->itemData(index).toInt();
    if (range <= 0 || range == m_rangeSeconds) {
        return;
    }
    m_rangeSeconds = range;
    if (isVisible()) {
        refresh();
    }
}

void RemoteDiagnosticsDialog::refreshGraphs()
{
    if (!m_controller) {
        return;
    }
    const auto& history = m_controller->history();
    const qint64 nowMs = m_controller->nowMs();
    // All directions share one unit over the selected history range.
    double trafficMaximumKbps = 0;
    for (const auto metric : {Metric::CoreGuiRxKbps, Metric::CoreGuiTxKbps, Metric::CoreGuiTotalKbps}) {
        for (const auto& point : history.series(metric, nowMs, m_rangeSeconds).points) {
            trafficMaximumKbps = std::max(trafficMaximumKbps, point.value);
        }
    }
    const bool megabits = trafficMaximumKbps >= 1000.0;
    const char* trafficUnit = megabits ? " Mbps" : " kbps";
    setGraph(m_totalTrafficGraph, history, nowMs, m_rangeSeconds, {
        {Metric::CoreGuiRxKbps, "Core → app received", "#00b4d8", trafficUnit},
        {Metric::CoreGuiTxKbps, "App → Core outgoing", "#5fff8a", trafficUnit},
        {Metric::CoreGuiTotalKbps, "Total", "#ffd700", trafficUnit},
    }, megabits ? 0.001 : 1.0);
    setGraph(m_audioTrafficGraph, history, nowMs, m_rangeSeconds, {
        {Metric::AudioRtpRxKbps, "Audio packets", "#00b4d8", " kbps"},
        {Metric::AudioPayloadRxKbps, "Audio content", "#5fa8ff", " kbps"},
    });
    setGraph(m_speakerBufferGraph, history, nowMs, m_rangeSeconds, {
        {Metric::SpeakerBufferMs, "Speaker buffer", "#5fff8a", " ms"},
    });
    setGraph(m_radioLinkGraph, history, nowMs, m_rangeSeconds, {
        {Metric::RadioRxMbps, "Radio RX", "#00b4d8", " Mbps"},
        {Metric::RadioTxMbps, "Radio TX", "#5fff8a", " Mbps"},
    });
    setGraph(m_controlPayloadGraph, history, nowMs, m_rangeSeconds, {
        {Metric::SessionPayloadRxKbps, "Control received", "#5fa8ff", " kbit/s"},
        {Metric::SessionPayloadTxKbps, "Control sent", "#ffd700", " kbit/s"},
    });
    setGraph(m_roundTripGraph, history, nowMs, m_rangeSeconds, {
        {Metric::RadioRttMs, "Last radio RTT", "#00b4d8", " ms"},
        {Metric::SessionRttMs, "Last Core RTT", "#ffb86c", " ms"},
    });
    setGraph(m_packetAgeGraph, history, nowMs, m_rangeSeconds, {
        {Metric::PlaybackPacketAgeMs, "Packet age", "#c792ea", " ms"},
    });
    setGraph(m_audioDelayGraph, history, nowMs, m_rangeSeconds, {
        {Metric::AudioDelayMs, "Audio delay", "#5fff8a", " ms"},
        {Metric::AudioDeliveryDelayMs, "Delivery", "#00b4d8", " ms"},
        {Metric::AudioDelayAccuracyMs, "Accuracy (\u00B1)", "#ffd700", " ms"},
    });
    setGraph(m_audioPacketsGraph, history, nowMs, m_rangeSeconds, {
        {Metric::AudioEncodedPacketsPerSecond, "Core encoded", "#00b4d8", " packets/s"},
        {Metric::AudioSendAcceptedPerSecond, "Core accepted", "#5fff8a", " packets/s"},
        {Metric::AudioSendRejectedPerSecond, "Core refused", "#ff6060", " packets/s"},
        {Metric::PlaybackDecodedPacketsPerSecond, "Decoded here", "#5fa8ff", " packets/s"},
        {Metric::PlaybackConcealedPacketsPerSecond, "Filled in here", "#ffd700", " packets/s"},
        {Metric::PlaybackLatePacketsPerSecond, "Late here", "#ff8c00", " packets/s"},
    });
    setGraph(m_sourceFramesGraph, history, nowMs, m_rangeSeconds, {
        {Metric::AudioSourceFramesPerSecond, "Core source", "#00b4d8", " frames/s"},
    });
    setGraph(m_audioEventsGraph, history, nowMs, m_rangeSeconds, {
        {Metric::AudioSourceDropsPerSecond, "Core source drops", "#ff6060", " events/s"},
        {Metric::PlaybackUnderflowsPerSecond, "Underflows here", "#ffd700", " events/s"},
        {Metric::PlaybackOverflowsPerSecond, "Overflows here", "#ff8c00", " events/s"},
    });
    refreshCoreHostGraphs();
    refreshCoreReceiverGraph();
}

void RemoteDiagnosticsDialog::refreshCoreHostGraphs()
{
    const auto& history = m_controller->history();
    const qint64 nowMs = m_controller->nowMs();
    const RemoteTelemetryView& view = m_controller->current();
    const std::array<Metric, 5> kHostMetrics{
        Metric::CoreSystemCpuPercent, Metric::CoreProcessCpuPercent,
        Metric::CoreMemoryAvailableMiB, Metric::CoreProcessResidentMiB,
        Metric::CoreHottestZoneCelsius};

    // While the Core's measurements are arriving, whether it reports its load
    // decides. Between sessions the retained history still shows.
    bool reports = false;
    switch (view.state) {
    case RemoteTelemetryView::State::Current:
    case RemoteTelemetryView::State::Stale:
        reports = view.coreHostReported;
        break;
    case RemoteTelemetryView::State::Unsupported:
        reports = false;
        break;
    case RemoteTelemetryView::State::Disconnected:
    case RemoteTelemetryView::State::Waiting:
        for (const Metric metric : kHostMetrics) {
            if (!history.series(metric, nowMs, m_rangeSeconds).points.isEmpty()) {
                reports = true;
                break;
            }
        }
        break;
    }
    m_coreHostUnavailableLabel->setVisible(!reports);
    m_coreCpuGraph->setVisible(reports);
    m_coreMemoryGraph->setVisible(reports);
    m_coreTemperatureGraph->setVisible(reports);
    if (!reports) {
        return;
    }

    setGraph(m_coreCpuGraph, history, nowMs, m_rangeSeconds, {
        {Metric::CoreSystemCpuPercent, "System", "#00b4d8", "\u00A0%"},
        {Metric::CoreProcessCpuPercent, "NereusSDR Core", "#5fff8a", "\u00A0%"},
    });
    // Both series share one unit over the selected range, chosen the way the
    // traffic graph chooses kbps or Mbps.
    double memoryMaximumMiB = 0;
    for (const auto metric : {Metric::CoreMemoryAvailableMiB, Metric::CoreProcessResidentMiB}) {
        for (const auto& point : history.series(metric, nowMs, m_rangeSeconds).points) {
            memoryMaximumMiB = std::max(memoryMaximumMiB, point.value);
        }
    }
    const bool gibibytes = memoryMaximumMiB >= 1024.0;
    const char* memoryUnit = gibibytes ? "\u00A0GiB" : "\u00A0MiB";
    setGraph(m_coreMemoryGraph, history, nowMs, m_rangeSeconds, {
        {Metric::CoreMemoryAvailableMiB, "Available", "#00b4d8", memoryUnit},
        {Metric::CoreProcessResidentMiB, "Used by NereusSDR Core", "#ffd700", memoryUnit},
    }, gibibytes ? 1.0 / 1024.0 : 1.0);
    setGraph(m_coreTemperatureGraph, history, nowMs, m_rangeSeconds, {
        {Metric::CoreHottestZoneCelsius, "Hottest sensor", "#ff8c00", "\u00A0°C"},
    });
    // Only the current view names the sensor; without a name the tooltip is
    // the generic one, so a previous Core's sensor name never lingers.
    m_coreTemperatureGraph->setToolTip(view.coreHost.hottestZoneName.isEmpty()
        ? tr("The hottest temperature sensor on the Core computer.")
        : tr("The hottest temperature sensor on the Core computer: %1.")
              .arg(view.coreHost.hottestZoneName));
}

void RemoteDiagnosticsDialog::refreshCoreReceiverGraph()
{
    const auto& history = m_controller->history();
    const qint64 nowMs = m_controller->nowMs();
    const RemoteTelemetryView& view = m_controller->current();
    constexpr int kSlots = TelemetryHistory::kCoreReceiverLoadSlots;

    // The same rule as the computer-load graphs: while measurements arrive,
    // whether this Core sends receiver processing decides; between sessions
    // the retained history still shows.
    bool hasHistory = false;
    for (int slot = 0; slot < kSlots; ++slot) {
        if (!history.series(TelemetryHistory::coreReceiverLoadMetric(slot), nowMs,
                            m_rangeSeconds).points.isEmpty()) {
            hasHistory = true;
            break;
        }
    }
    bool reports = false;
    switch (view.state) {
    case RemoteTelemetryView::State::Current:
    case RemoteTelemetryView::State::Stale:
        reports = view.coreReceiversReported;
        break;
    case RemoteTelemetryView::State::Unsupported:
        reports = false;
        break;
    case RemoteTelemetryView::State::Disconnected:
    case RemoteTelemetryView::State::Waiting:
        reports = hasHistory;
        break;
    }
    m_coreReceiversUnavailableLabel->setVisible(!reports);
    m_coreReceiverGraph->setVisible(reports);
    if (!reports) {
        return;
    }

    // One series per receiver the range holds, named by its slice letter.
    static constexpr std::array<const char*, kSlots> kColors{
        "#00b4d8", "#5fff8a", "#ffd700", "#c792ea", "#ff8c00"};
    QVector<TimeSeriesGraphWidget::Series> series;
    for (int slot = 0; slot < kSlots; ++slot) {
        const Metric metric = TelemetryHistory::coreReceiverLoadMetric(slot);
        if (history.series(metric, nowMs, m_rangeSeconds).points.isEmpty()) {
            continue;
        }
        TimeSeriesGraphWidget::Series graph = toGraphSeries(
            history, metric, {metric, "", kColors[slot], "\u00A0%"}, nowMs, m_rangeSeconds);
        graph.label = tr("Slice %1").arg(QChar(QLatin1Char('A').unicode() + slot));
        series.append(std::move(graph));
    }
    m_coreReceiverGraph->setSeries(std::move(series), m_rangeSeconds);
}

void RemoteDiagnosticsDialog::refreshDetail()
{
    if (!m_detailLabel) {
        return;
    }
    m_detailLabel->setText(m_controller
        ? m_controller->detailText()
        : tr("Measurements are not available in this window."));
}

} // namespace NereusSDR
