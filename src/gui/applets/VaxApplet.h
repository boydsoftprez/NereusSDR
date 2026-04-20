// =================================================================
// src/gui/applets/VaxApplet.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR source:
//   src/gui/DaxApplet.h
//   src/gui/DaxApplet.cpp
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. NereusSDR is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per AetherSDR convention.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Ported/adapted in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code. Phase 3O Sub-Phase 9 Task 9.2b.
//                 Renamed DaxApplet → VaxApplet, `dax*` identifiers →
//                 `vax*`, and adapted to NereusSDR's `AppletWidget` base
//                 (AetherSDR's DaxApplet inherits QWidget directly and
//                 uses its own parent-applet frame). Wires slider →
//                 AudioEngine::setVaxRxGain / setVaxMuted / setVaxTxGain
//                 (Task 9.2a) and reads AudioEngine::vaxRxLevel /
//                 vaxTxLevel via a 50 ms poll timer. Per-channel device
//                 label is platform-hardcoded on Mac/Linux ("NereusSDR
//                 VAX N") and read from AppSettings on Windows (BYO
//                 cables). Tags label mirrors AetherSDR's slice-letter
//                 convention, listening to SliceModel::vaxChannelChanged.
//                 Settings keys per docs/architecture/2026-04-19-vax-design.md
//                 §5.4 (PascalCase keys, "True"/"False" booleans).
// =================================================================

#pragma once

#include "AppletWidget.h"

#include <QString>

class QPushButton;
class QLabel;
class QTimer;

namespace NereusSDR {

class AudioEngine;
class MeterSlider;

// VAX applet — per-VAX-channel gain + mute + level meters.
//
// Layout (compact, mirroring AetherSDR DaxApplet with VAX rename):
//   Channel strip × 4:
//     [VAX N:] [Slice tag] [MeterSlider (level + gain)] [Mute]
//     [Device: "NereusSDR VAX N"]
//   Divider
//   TX row:
//     [TX:]    [Slice tag] [MeterSlider (level + gain)]
//
// Widget → Model:
//   MeterSlider::gainChanged → AudioEngine::setVaxRxGain / setVaxTxGain
//   Mute button toggle       → AudioEngine::setVaxMuted
// Model → Widget:
//   AudioEngine::vaxRxGainChanged / vaxMutedChanged / vaxTxGainChanged
//     pushes back into widgets via QSignalBlocker-guarded setters.
// Readonly feed:
//   QTimer @ 50 ms reads AudioEngine::vaxRxLevel / vaxTxLevel into the
//   meter portion of MeterSlider. Pattern A per the Task 9.2b handoff
//   (isolated per-applet timer; no shared poller). Stopped on hide for
//   scope discipline.
class VaxApplet : public AppletWidget {
    Q_OBJECT
public:
    static constexpr int kChannels = 4;

    explicit VaxApplet(RadioModel* model, AudioEngine* audio,
                       QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("vax"); }
    QString appletTitle() const override { return QStringLiteral("VAX"); }
    void    syncFromModel() override;

protected:
    // Start/stop the level-poll timer with visibility so a hidden applet
    // doesn't wake the audio thread 20×/s for nothing.
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void buildUi();
    void connectSliceTagsTracking();
    void updateTagsLabels();
    void pollLevels();

    // Compose the platform-specific device label for `channel` (1..4).
    // Mac/Linux → "NereusSDR VAX N"; Windows → AppSettings
    // audio/Vax<N>/DeviceName, defaulting to "(no device)".
    QString deviceLabelFor(int channel) const;

    AudioEngine* m_audio{nullptr};

    // Per-channel widgets (indexed 0..3 for channels 1..4).
    QPushButton* m_muteBtn[kChannels]{};
    MeterSlider* m_rxMeter[kChannels]{};
    QLabel*      m_tagsLbl[kChannels]{};
    QLabel*      m_deviceLbl[kChannels]{};

    // TX row.
    MeterSlider* m_txMeter{nullptr};
    QLabel*      m_txTagsLbl{nullptr};

    // 20 Hz level-meter poller. Reads AudioEngine::vaxRxLevel / vaxTxLevel.
    QTimer* m_levelTimer{nullptr};
};

} // namespace NereusSDR
