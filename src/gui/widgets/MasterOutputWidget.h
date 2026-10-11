#pragma once

// =================================================================
// src/gui/widgets/MasterOutputWidget.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original widget. Visual styling draws on AetherSDR
// `src/gui/TitleBar.cpp:172-215` (the master-volume section of
// AetherSDR's title bar): speaker emoji button with mute glyph flip,
// horizontal volume slider with the shared #00b4d8 handle/sub-page
// palette, and an inset percent readout to the right of the slider.
// The structure here is NereusSDR-original because this widget
// isolates JUST the master-output triad; AetherSDR's TitleBar
// combines master + headphones + PC-audio + minimal-mode in one
// monolithic bar. TitleBar-strip host wiring lands in Task 10c.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code. Phase 3O
//                Sub-Phase 10 Task 10b. Widget layout + mute-glyph
//                flip + inset readout styling take cues from
//                AetherSDR TitleBar.cpp:172-215. Two-way bind to
//                AudioEngine setVolume / setMasterMuted with the
//                m_updatingFromModel + QSignalBlocker echo guard
//                pattern documented in CLAUDE.md "GUI↔Model Sync".
//                Persists audio/Master/Volume and audio/Master/Muted
//                per design spec
//                docs/architecture/2026-04-19-vax-design.md §5.4.
//   2026-09-23 - R-R3-23: a picked output device is saved to
//                audio/Speakers/DeviceName before outputDeviceChanged
//                is emitted (selectOutputDevice). J.J. Boyd (KG4VCF),
//                with AI-assisted implementation via Anthropic Claude
//                Code.
//   2026-10-06 - Radio speaker plan Task 6 (R-SPK-17, D1, D5): this is the
//                header's PC group. The speaker button shows the app's
//                own pc-on / pc-muted icons (AppIcon) instead of emoji
//                text, a "PC" word label sits between it and the slider,
//                and the styles the RADIO group shares are exported in
//                HeaderVolumeStyle. Behaviour, keys and the right-click
//                device menu are unchanged. J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-10-06 - Radio speaker plan Task 6, JJ decision 2 (R-SPK-17, D1):
//                setStacked() switches the PC group to the stacked form
//                (thin row, small icon and readout, same handle) and
//                HeaderVolumeStyle::applyForm() sizes either group for
//                either form. J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
//   2026-10-09 - Native audio plan Task 19 (R-AUD-23, R-AUD-03, R-AUD-08,
//                R-AUD-11, R-AUD-12, D20): the right-click menu lists only
//                the speakers' driver, from the engine's device catalogue
//                (option A of header-menu-mockup.html): the "Speakers ·
//                <driver>" heading, "(platform default)", the devices with
//                pairs under their interface, a missing or busy choice on
//                top, and "Sound setup…" (soundSetupRequested). The tick and
//                the tooltip follow the engine's speakers status. A pick
//                saves the choice as the Outputs card does. setAudioEngine()
//                and buildSpeakerMenuForTest(). J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-10-09: Windows test fix (R-AUD-03): exported from the
//               GUI DLL, so a signal of it is found from outside the DLL
//               on Windows. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/AudioDeviceConfig.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/IAudioStreamHost.h"
#include "gui/NereusGuiExport.h"

#include <QPointer>
#include <QString>
#include <QWidget>

class QLabel;
class QMenu;
class QPoint;
class QPushButton;
class QSlider;

namespace NereusSDR {

class AudioEngine;

// Styles the header's PC and RADIO groups share (R-SPK-17). Each group is
// icon button, word label, 100 px slider and inset readout.
namespace HeaderVolumeStyle {
// Transparent, borderless 20 x 20 icon button.
extern const char* const kIconButton;
// The short word label ("PC", "RADIO"), dimmed while disabled.
extern const char* const kWordLabel;
// The PC slider: #1a2a3a groove, #00b4d8 handle and fill, and no fill
// with a dim handle while disabled (as RADIO's).
extern const char* const kPcSlider;
// The RADIO slider: the same groove with the amber #e0a030 handle and
// fill, and no fill with a dim handle while disabled.
extern const char* const kRadioSlider;
// The inset value readout, dimmed while disabled.
extern const char* const kReadout;
// Logical size of the icon drawn in the 20 x 20 button.
inline constexpr int kIconPx = 18;
// The stacked readout (layout C): 9 px text, no vertical padding.
extern const char* const kReadoutStacked;
// Logical size of the icon drawn in the stacked form's 14 x 14 button.
inline constexpr int kStackedIconPx = 12;

// Sizes one group's icon button, word label, slider and readout for the
// side-by-side form (layout A of header-layouts.html: 20 x 20 button,
// 100 x 16 slider, 22 px readout) or the stacked form (layout C, one row of
// two in the 32 px strip: 14 x 14 button, 84 x 12 slider, 20 x 13 readout,
// the word label kept at stackedLabelWidth so both rows' sliders line up,
// or hidden when stackedLabelWidth is 0). The slider handle stays the
// header's 10 px either way. readoutStyle is the side-by-side readout
// style. Returns the icon size to draw.
int applyForm(QPushButton* button, QLabel* word, QSlider* slider, QLabel* readout,
              bool stacked, int stackedLabelWidth, const char* readoutStyle);
} // namespace HeaderVolumeStyle

// MasterOutputWidget — menu-bar master-output composite.
//
// Layout (matches design spec §7.3, ~222 px wide × 22 px tall, plus the
// "PC" word label of the radio speaker design, R-SPK-17):
//
//   [speaker 20] [PC] [slider 100] [label 22]
//
// - Speaker button: left-click toggles mute (icons pc-on / pc-muted,
//   AppIcon; the button's AppIcon::kIconProperty names the icon). Right-click
//   opens the speakers menu (R-AUD-23, D20): the speakers' own driver and
//   its devices from the engine's device catalogue. A pick saves the
//   choice under audio/Speakers and emits outputDeviceChanged(name); the
//   host (MainWindow) calls AudioEngine::setSpeakersConfig to open it.
//   "Sound setup…" emits soundSetupRequested.
// - Slider: 0–100 range mapped linearly to AudioEngine volume [0,1].
// - Label: inset percent readout (0–100, shown as the raw integer
//   slider value — Option A per task brief, matches AetherSDR).
//
// volumeChanged / mutedChanged / outputDeviceChanged signals fire
// ONLY on user action. The m_updatingFromModel guard plus a
// QSignalBlocker on the speaker button prevents the engine→widget
// echo from re-emitting into the engine.
class NEREUS_GUI_EXPORT MasterOutputWidget : public QWidget {
    Q_OBJECT
public:
    explicit MasterOutputWidget(AudioEngine* audio, QWidget* parent = nullptr);

    // R-AUD-23: the engine whose device catalogue fills the speakers menu
    // and whose speakers status sets the tick and the tooltip. The
    // constructor's engine is taken the same way; a later call follows
    // another engine (or none). Does not seed volume, mute or device.
    void setAudioEngine(AudioEngine* engine);

    // The speakers menu as a right-click opens it, built now from the
    // catalogue and the speakers status, for a test to read and trigger.
    // The caller owns it (its parent is this widget).
    QMenu* buildSpeakerMenuForTest();

    // Called by Setup → Audio → Devices when the user picks a
    // speakers device elsewhere in the app. The menu reads the choice
    // afresh from the engine and the settings each time it opens, so
    // this only refreshes the tooltip.
    // Does NOT emit outputDeviceChanged — this is a sync-from-
    // elsewhere path, not a user action.
    void setCurrentOutputDevice(const QString& name);

    // R-SPK-17: the side-by-side (false) or stacked (true) form of the
    // group, chosen by TitleBar from the room the header has. The same
    // children, with the same objectNames, serve both forms.
    void setStacked(bool stacked, int stackedLabelWidth);
    bool isStacked() const { return m_stacked; }

signals:
    // User moved the slider. Value is the 0.0–1.0 linear volume.
    void volumeChanged(float v);
    // User clicked the speaker button.
    void mutedChanged(bool muted);
    // User picked an output device from the right-click context menu.
    // The name is empty for "(platform default)".
    void outputDeviceChanged(QString deviceName);
    // R-AUD-23: "Sound setup…" in the speakers menu; the host opens Setup
    // at Audio, Outputs.
    void soundSetupRequested();

private slots:
    // Build and pop the right-click speakers menu at `pos`
    // (widget-local coordinates, as delivered by
    // QWidget::customContextMenuRequested).
    void onSpeakerContextMenu(const QPoint& pos);

    // AudioEngine → widget echo handlers. Both use the
    // m_updatingFromModel / QSignalBlocker guard so a setValue /
    // setChecked driven by these slots does not reenter the
    // widget→engine path.
    void onAudioEngineVolumeChanged(float v);
    void onAudioEngineMasterMutedChanged(bool m);

    // Sub-Phase 12 Task 12.2: live sync with Setup → Audio → Devices edits.
    // Receives the negotiated AudioDeviceConfig from the engine after any
    // speakers bus reconfig. It names the device that opened, a fall-back
    // to the default included, so it never moves the menu's tick (the
    // choice comes from the speakers status); it refreshes the tooltip.
    void onSpeakersConfigChanged(const NereusSDR::AudioDeviceConfig& cfg);

private:
    // One device the speakers menu offers: the identity a pick saves.
    struct SpeakerPick {
        AudioEngineKind engine = AudioEngineKind::CoreAudio;
        QString hostApi;      // older drivers: the PortAudio host API
        QString deviceId;     // empty: "(platform default)"
        QString deviceName;
        int firstChannel = 1;
        int channelCount = 2;   // the pair's width (ASIO: for the one-driver plan)
    };

    // R-R3-23: the menu's action for one device. Saves the choice under
    // audio/Speakers as the Outputs card does (Engine, DeviceId,
    // DeviceName, FirstChannel) FIRST, then emits outputDeviceChanged.
    // The order matters in a remote window: remote playback re-reads
    // audio/Speakers when the engine reports the new speakers, which the
    // emit leads to synchronously, so announcing before saving made the
    // remote audio status name the previous device. Picking the current
    // choice does nothing. A pair on a second ASIO driver asks first, as
    // the Setup card does (R-AUD-19): "Switch all" moves the other roles
    // and the pick goes on; Cancel writes and announces nothing.
    void selectOutputDevice(const SpeakerPick& pick);

    QMenu* buildSpeakerMenu();
    // Connects the engine's volume, mute, speakers and status signals and
    // takes up its catalogue.
    void connectEngine();
    // The speakers' choice: the engine's status while its device layer
    // runs (the device chosen, never the fall-back that plays), else the
    // saved choice.
    AudioDeviceConfig speakersChoice() const;
    AudioRoleStatus speakersStatus() const;
    // The driver the choice is on.
    AudioEngineKind choiceEngine(const AudioDeviceConfig& choice) const;
    // Takes up the engine's catalogue once it has one.
    IAudioDeviceCatalog* attachCatalogue();
    // R-AUD-23 / R-AUD-12: the tooltip from the speakers status.
    void refreshSpeakerToolTip();

    AudioEngine* m_audio{nullptr};
    QPointer<IAudioDeviceCatalog> m_catalogue;
    // Shows pc-muted or pc-on on the speaker button (R-SPK-19, D5).
    void applySpeakerIcon(bool muted);

    QPushButton* m_speakerBtn{nullptr};
    QLabel*      m_pcLabel{nullptr};
    bool         m_stacked{false};
    int          m_iconPx{HeaderVolumeStyle::kIconPx};
    QSlider*     m_slider{nullptr};
    QLabel*      m_dbLabel{nullptr};
    bool         m_updatingFromModel{false};
};

} // namespace NereusSDR
