// no-port-check: NereusSDR-original owning-window Core audio Settings page.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "gui/setup/CoreAudioSetupPage.h"
#include "gui/RemoteAudioWidget.h"
namespace NereusSDR {
CoreAudioSetupPage::CoreAudioSetupPage(RemoteMediaController* media, RemoteTelemetryController* telemetry,
    QWidget* parent) : SetupPage(tr("Audio with the Core"), parent)
{
    m_audio = new RemoteAudioWidget(media, telemetry, this);
    auto* section = addSection(tr("This computer’s Core audio"));
    section->layout()->addWidget(m_audio);
}
void CoreAudioSetupPage::setContext(const CoreSettingsContext& context) { m_audio->setContext(context); }
}
