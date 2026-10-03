// no-port-check: NereusSDR-original shared Core audio presentation.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "gui/RemoteAudioWidget.h"
#include "gui/RemoteMediaController.h"
#include "gui/RemoteTelemetryController.h"
#include "gui/StyleConstants.h"
#include <QComboBox>
#include <QAbstractItemView>
#include <QWheelEvent>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>
namespace NereusSDR {
namespace {
// Computer-wide audio preference keeps its existing mouse/keyboard behavior,
// independently of radio ControlsLock. Closed wheel gestures scroll its container.
class AudioQualityCombo final : public QComboBox {
public:
    using QComboBox::QComboBox;
protected:
    void wheelEvent(QWheelEvent* event) override {
        if (view() && view()->isVisible()) { QComboBox::wheelEvent(event); }
        else { event->ignore(); }
    }
};
}
RemoteAudioWidget::RemoteAudioWidget(RemoteMediaController* media, RemoteTelemetryController* telemetry,
    QWidget* parent) : QWidget(parent), m_media(media), m_telemetry(telemetry)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(8);
    auto label = [this](const QString& name) {
        auto* result = new QLabel(this); result->setObjectName(name);
        result->setWordWrap(true); result->setTextFormat(Qt::PlainText);
        result->setTextInteractionFlags(Qt::TextSelectableByMouse); return result;
    };
    m_contextLabel = label(QStringLiteral("coreAudioConnectionContext"));
    layout->addWidget(m_contextLabel);
    auto* preference = new QLabel(tr("Saved on this computer. This choice applies to audio received from the Core and this computer’s microphone sent to the Core. Compressed receiver streams for apps keep their own 48 kbps target."), this);
    preference->setWordWrap(true); layout->addWidget(preference);
    auto group = [this, layout](const QString& title) {
        auto* box = new QGroupBox(title, this); auto* rows = new QVBoxLayout(box);
        layout->addWidget(box); return rows;
    };
    auto* quality = group(tr("Audio quality"));
    m_quality = new AudioQualityCombo(this); m_quality->setObjectName(QStringLiteral("remoteAudioQuality"));
    m_quality->setStyleSheet(Style::kComboStyle);
    m_quality->addItem(tr("High — 48 kbps"), int(RemoteAudioQualityChoice::High));
    m_quality->addItem(tr("Save data — 24 kbps"), int(RemoteAudioQualityChoice::SaveData));
    m_quality->addItem(tr("Lossless"), int(RemoteAudioQualityChoice::Lossless));
    m_quality->setToolTip(tr("High and Save data are compressed targets. Lossless needs about 1.6 Mbit/s; a link that cannot carry it falls back to Opus."));
    quality->addWidget(m_quality);
    m_unavailable = label(QStringLiteral("remoteAudioQualityUnavailable")); quality->addWidget(m_unavailable);
    auto* receive = group(tr("Receive from Core"));
    m_receive = label(QStringLiteral("remoteAudioDetails")); receive->addWidget(m_receive);
    m_payload = label(QStringLiteral("coreAudioPayloadRate")); receive->addWidget(m_payload);
    m_retry = new QPushButton(tr("Retry audio"), this); m_retry->setAutoDefault(false);
    m_retry->setObjectName(QStringLiteral("retryRemoteAudio")); receive->addWidget(m_retry);
    auto* microphone = group(tr("Microphone to Core"));
    m_microphone = label(QStringLiteral("coreAudioMicrophoneDetails")); microphone->addWidget(m_microphone);
    auto* apps = group(tr("Receiver audio for apps"));
    m_apps = label(QStringLiteral("coreAudioAppsDetails")); apps->addWidget(m_apps);
    connect(m_quality, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_media && index >= 0) { m_media->setAudioQualityChoice(
            static_cast<RemoteAudioQualityChoice>(m_quality->itemData(index).toInt())); }
    });
    connect(m_quality, &QComboBox::activated, this, [this](int index) {
        if (m_media && index >= 0 && m_quality->itemData(index).toInt() == int(RemoteAudioQualityChoice::Lossless)
            && m_media->audioStatus().qualityReason == RemoteAudioQualityReason::NetworkTooSlow) {
            m_media->setAudioQualityChoice(RemoteAudioQualityChoice::Lossless);
        }
    });
    connect(m_retry, &QPushButton::clicked, this, [this] { if (m_media) { m_media->retryAudio(); } });
    if (media) {
        connect(media, &RemoteMediaController::audioStatusChanged, this, &RemoteAudioWidget::refresh);
        connect(media, &QObject::destroyed, this, &RemoteAudioWidget::refresh);
    }
    if (telemetry) {
        connect(telemetry, &RemoteTelemetryController::changed, this, [this] { if (isVisible()) { refresh(); } });
    }
    m_timer = new QTimer(this); m_timer->setInterval(1000);
    m_timer->setObjectName(QStringLiteral("remoteAudioPanelTimer"));
    connect(m_timer, &QTimer::timeout, this, &RemoteAudioWidget::refresh);
    refresh();
}
void RemoteAudioWidget::setContext(const CoreSettingsContext& context)
{ m_context = context; m_contextBound = true; refresh(); }
void RemoteAudioWidget::refresh()
{
    const bool current = m_media && (!m_contextBound || m_context.authenticated);
    m_contextLabel->setText(m_contextBound ? (m_context.authenticated
        ? tr("This window’s Core: %1").arg(m_context.coreName.isEmpty() ? tr("Name not reported") : m_context.coreName)
        : tr("No authenticated Core connection in this window. Only this computer’s preference is shown.")) : QString());
    m_contextLabel->setVisible(m_contextBound);
    m_quality->setEnabled(bool(m_media));
    if (m_media) {
        const QSignalBlocker blocker(m_quality);
        m_quality->setCurrentIndex(m_quality->findData(int(m_media->audioQualityChoice())));
    }
    QStringList reasons;
    auto* rows = qobject_cast<QStandardItemModel*>(m_quality->model());
    for (int i = 0; i < m_quality->count(); ++i) {
        const auto choice = static_cast<RemoteAudioQualityChoice>(m_quality->itemData(i).toInt());
        const QString reason = m_media ? m_media->audioQualityUnavailableReason(choice)
                                      : tr("Audio is not available in this window.");
        if (rows && rows->item(i)) { rows->item(i)->setEnabled(reason.isEmpty()); rows->item(i)->setToolTip(reason); }
        if (!reason.isEmpty()) { reasons << QStringLiteral("%1: %2").arg(remoteAudioQualityChoiceName(choice), reason); }
    }
    m_unavailable->setText(reasons.join(QLatin1Char('\n'))); m_unavailable->setVisible(!reasons.isEmpty());
    if (current) {
        const auto status = m_media->audioStatus();
        const auto sections = formatRemoteAudioDetailSections(status, m_media->audioTelemetry(),
            m_media->audioDelay(), m_media->receiverAudioTelemetry());
        m_receive->setText(sections.receive); m_microphone->setText(sections.microphone);
        m_apps->setText(sections.apps.isEmpty() ? tr("No receiver streams requested by apps on this computer.") : sections.apps);
        const auto payload = m_telemetry ? m_telemetry->current().audioPayloadRxKbps : std::optional<double>{};
        m_payload->setText(payload ? tr("Received audio payload: %1 kbps (measured)").arg(*payload, 0, 'f', 1)
                                  : tr("Received audio payload: not measured"));
        m_retry->setEnabled(status.retryAvailable);
    } else {
        m_receive->setText(tr("No current receive format or health measurements."));
        m_microphone->setText(tr("No microphone stream from this computer to a current Core."));
        m_apps->setText(tr("No current receiver streams for apps."));
        m_payload->setText(tr("Received audio payload: not measured")); m_retry->setEnabled(false);
    }
    emit contentChanged();
}
void RemoteAudioWidget::showEvent(QShowEvent* event)
{ QWidget::showEvent(event); refresh(); m_timer->start(); }
void RemoteAudioWidget::hideEvent(QHideEvent* event)
{ m_timer->stop(); QWidget::hideEvent(event); }
} // namespace NereusSDR
