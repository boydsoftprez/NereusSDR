#pragma once

// =================================================================
// src/gui/setup/AudioDevicesPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → Devices page. No Thetis port,
// no attribution headers required (per memory: feedback_source_first_
// ui_vs_dsp — Qt widgets in Setup pages are NereusSDR-native).
//
// Sub-Phase 12 Task 12.2 (2026-04-20): implements three DeviceCard
// instances (Speakers / Headphones / TX Input) with live-edit commit
// semantics (§2.1) and Step 0 engine scaffolding wiring.
//
// Design spec: docs/architecture/2026-04-20-phase3o-subphase12-addendum.md
// §§2.1 + 4.
// =================================================================

#include "gui/SetupPage.h"

namespace NereusSDR {

class AudioEngine;
struct AudioDeviceConfig;
class DeviceCard;

// ---------------------------------------------------------------------------
// Audio > Devices
//
// Hosts three DeviceCard instances:
//   - Speakers   (audio/Speakers,  Output)
//   - Headphones (audio/Headphones, Output, with enable checkbox)
//   - TX Input   (audio/TxInput,   Input,  with monitor + tone extras)
//
// Each card emits configChanged(AudioDeviceConfig) → the page calls the
// matching AudioEngine::set<Role>Config(). The engine emits
// <role>ConfigChanged(AudioDeviceConfig) → the page feeds it back into
// the card's updateNegotiatedPill() with QSignalBlocker to avoid
// echo loops.
// ---------------------------------------------------------------------------
class AudioDevicesPage : public SetupPage {
    Q_OBJECT
public:
    explicit AudioDevicesPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void wireEngineConnections();

    AudioEngine* m_engine{nullptr};

    DeviceCard* m_speakersCard{nullptr};
    DeviceCard* m_headphonesCard{nullptr};
    DeviceCard* m_txInputCard{nullptr};

    bool m_updatingFromEngine{false};
};

} // namespace NereusSDR
