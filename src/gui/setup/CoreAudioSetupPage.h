#pragma once
// no-port-check: NereusSDR-original owning-window Core audio Settings page.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "gui/SetupPage.h"
#include "gui/setup/CoreSettingsContext.h"
namespace NereusSDR {
class RemoteMediaController;
class RemoteTelemetryController;
class RemoteAudioWidget;
class CoreAudioSetupPage final : public SetupPage {
    Q_OBJECT
public:
    CoreAudioSetupPage(RemoteMediaController* media, RemoteTelemetryController* telemetry,
                      QWidget* parent = nullptr);
    void setContext(const CoreSettingsContext& context);
private:
    RemoteAudioWidget* m_audio;
};
}
