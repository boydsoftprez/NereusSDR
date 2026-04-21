// =================================================================
// src/gui/setup/AudioDevicesPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → Devices page.
// See AudioDevicesPage.h for the full header.
//
// Sub-Phase 12 Task 12.2 (2026-04-20): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// =================================================================

#include "AudioDevicesPage.h"
#include "DeviceCard.h"

#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "models/RadioModel.h"

#include <QSignalBlocker>
#include <QVBoxLayout>

namespace NereusSDR {

AudioDevicesPage::AudioDevicesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Devices"), model, parent)
    , m_engine(model ? model->audioEngine() : nullptr)
{
    // ── Speakers card ────────────────────────────────────────────────────
    m_speakersCard = new DeviceCard(
        QStringLiteral("audio/Speakers"),
        DeviceCard::Role::Output,
        false,         // no enable checkbox
        this);
    m_speakersCard->setTitle(QStringLiteral("Speakers"));
    contentLayout()->insertWidget(0, m_speakersCard);

    // ── Headphones card ──────────────────────────────────────────────────
    m_headphonesCard = new DeviceCard(
        QStringLiteral("audio/Headphones"),
        DeviceCard::Role::Output,
        true,          // enable checkbox in title bar
        this);
    m_headphonesCard->setTitle(QStringLiteral("Headphones"));
    contentLayout()->insertWidget(1, m_headphonesCard);

    // ── TX Input card ─────────────────────────────────────────────────────
    m_txInputCard = new DeviceCard(
        QStringLiteral("audio/TxInput"),
        DeviceCard::Role::Input,
        false,
        this);
    m_txInputCard->setTitle(QStringLiteral("TX Input (Microphone)"));
    contentLayout()->insertWidget(2, m_txInputCard);

    if (m_engine) {
        wireEngineConnections();
    }
}

void AudioDevicesPage::wireEngineConnections()
{
    // ── Speakers card → engine ────────────────────────────────────────────
    connect(m_speakersCard, &DeviceCard::configChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                if (m_updatingFromEngine) { return; }
                m_engine->setSpeakersConfig(cfg);
            });

    // Engine → Speakers pill (QSignalBlocker prevents echo).
    connect(m_engine, &AudioEngine::speakersConfigChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                m_updatingFromEngine = true;
                QSignalBlocker blocker(m_speakersCard);
                m_speakersCard->updateNegotiatedPill(cfg);
                m_updatingFromEngine = false;
            });

    // ── Headphones card → engine ──────────────────────────────────────────
    connect(m_headphonesCard, &DeviceCard::configChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                if (m_updatingFromEngine) { return; }
                m_engine->setHeadphonesConfig(cfg);
            });

    connect(m_engine, &AudioEngine::headphonesConfigChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                m_updatingFromEngine = true;
                QSignalBlocker blocker(m_headphonesCard);
                m_headphonesCard->updateNegotiatedPill(cfg);
                m_updatingFromEngine = false;
            });

    // ── TX Input card → engine ────────────────────────────────────────────
    connect(m_txInputCard, &DeviceCard::configChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                if (m_updatingFromEngine) { return; }
                m_engine->setTxInputConfig(cfg);
            });

    connect(m_engine, &AudioEngine::txInputConfigChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                m_updatingFromEngine = true;
                QSignalBlocker blocker(m_txInputCard);
                m_txInputCard->updateNegotiatedPill(cfg);
                m_updatingFromEngine = false;
            });
}

} // namespace NereusSDR
