#pragma once
// no-port-check: NereusSDR-original shared Core audio presentation.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QWidget>
#include <QPointer>
#include "gui/setup/CoreSettingsContext.h"
class QLabel;
class QComboBox;
class QPushButton;
class QTimer;
namespace NereusSDR {
class RemoteMediaController;
class RemoteTelemetryController;
/// Observes the owning window's existing audio controller; owns no stream or policy.
class RemoteAudioWidget final : public QWidget {
    Q_OBJECT
public:
    RemoteAudioWidget(RemoteMediaController* media, RemoteTelemetryController* telemetry,
                      QWidget* parent = nullptr);
    void setContext(const CoreSettingsContext& context);
signals:
    void contentChanged();
protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
private:
    void refresh();
    QPointer<RemoteMediaController> m_media;
    QPointer<RemoteTelemetryController> m_telemetry;
    QLabel *m_contextLabel, *m_receive, *m_microphone, *m_apps, *m_payload, *m_unavailable;
    QComboBox* m_quality;
    QPushButton* m_retry;
    QTimer* m_timer;
    CoreSettingsContext m_context;
    bool m_contextBound = false;
};
}
