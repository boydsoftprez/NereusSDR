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
//   2026-09-23: R-R3-44: setTransmitPermitted() for the TX row, so the
//                 applet works in a remote window. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 25 (R-IOS-18): the "Station computer"
//                 section, the Core computer's VAX channels through the
//                 Core's `vax` object, below this computer's own. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30: the section is titled "Core computer" and, where it does
//                 not apply, is disabled with a plain reason instead of
//                 hidden. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-30: JJ's ruling: hidden in a window that runs the radio
//                 directly (it can never have the section); in a remote
//                 window disabled with its reason while the Core shares no
//                 VAX, its labels greyed. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include "AppletWidget.h"

#include <QPointer>
#include <QString>

class QPushButton;
class QVBoxLayout;
class QLabel;
class QTimer;

namespace NereusSDR {

class AudioEngine;
class MeterSlider;
class StationVax;

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

    // R-R3-44: the TX row sets the level of VAX used as the microphone,
    // so in a remote window it follows the negotiated transmit permission
    // (MainWindow::applyRemoteRoleGating). The receive rows are unaffected.
    void setTransmitPermitted(bool permitted, const QString& reason);

    // iPhone app plan Task 25 (R-IOS-18): in a remote window, below this
    // computer's own VAX channels (R-R3-44), the "Core computer" section
    // shows the Core computer's VAX channels from the Core's `vax` object
    // (StationClient::stationVax): its slices, levels, mutes, device names
    // and transmit row, each control changing the Core's through the
    // object. Usable only while `shown` (the Core sends the object);
    // otherwise, and with a null `vax`, it stays in place disabled, each
    // control showing the plain reason. A window that runs the radio
    // directly can never have the section (its own rows are the Core
    // computer's), so there it is hidden.
    void setStationVax(StationVax* vax, bool shown);
    /// The section's TX row follows this device's transmit permission, as
    /// the Core takes the transmit level only from a device that may
    /// transmit: disabled with the gate's reason otherwise.
    void setStationTransmitPermitted(bool permitted, const QString& reason);
    /// Whether the section's meters are wanted: the section is shown and
    /// the applet is visible.
    bool stationLevelsWanted() const { return m_stationLevelsWanted; }

    // Test accessors for the section.
    QWidget* stationSectionForTest() const { return m_stationSection; }
    MeterSlider* stationRxMeterForTest(int channel) const
    {
        return channel >= 1 && channel <= kChannels ? m_stationRxMeter[channel - 1] : nullptr;
    }
    QPushButton* stationMuteButtonForTest(int channel) const
    {
        return channel >= 1 && channel <= kChannels ? m_stationMuteBtn[channel - 1] : nullptr;
    }
    QLabel* stationTagsLabelForTest(int channel) const
    {
        return channel >= 1 && channel <= kChannels ? m_stationTagsLbl[channel - 1] : nullptr;
    }
    QLabel* stationDeviceLabelForTest(int channel) const
    {
        return channel >= 1 && channel <= kChannels ? m_stationDeviceLbl[channel - 1] : nullptr;
    }
    MeterSlider* stationTxMeterForTest() const { return m_stationTxMeter; }
    QLabel* stationTxTagsLabelForTest() const { return m_stationTxTagsLbl; }

signals:
    /// stationLevelsWanted() changed: MainWindow subscribes to the Core's
    /// vaxLevels stream while it is true.
    void stationLevelsWantedChanged(bool wanted);

protected:
    // Start/stop the level-poll timer with visibility so a hidden applet
    // doesn't wake the audio thread 20×/s for nothing.
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void buildUi();
    void buildStationSection(QWidget* body, QVBoxLayout* vbox);
    void refreshStationValues();
    void refreshStationLevels();
    void updateStationLevelsWanted();
    void applyStationAvailability();
    void updateStationTxRow();
    QString stationUnavailableReason() const;
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

    // The "Core computer" section (iPhone app plan Task 25).
    QPointer<StationVax> m_stationVax;
    bool m_stationShown{false};
    bool m_stationTxPermitted{false};
    QString m_stationTxReason;
    QLabel*      m_stationTitle{nullptr};
    bool m_stationLevelsWanted{false};
    QWidget*     m_stationSection{nullptr};
    QPushButton* m_stationMuteBtn[kChannels]{};
    MeterSlider* m_stationRxMeter[kChannels]{};
    QLabel*      m_stationTagsLbl[kChannels]{};
    QLabel*      m_stationDeviceLbl[kChannels]{};
    MeterSlider* m_stationTxMeter{nullptr};
    QLabel*      m_stationTxTagsLbl{nullptr};
};

} // namespace NereusSDR
