#pragma once

// =================================================================
// src/gui/setup/AudioVaxPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original VAX section of Setup → Audio → Digital modes
// (R-SPK-21; it was the Audio → VAX page).
// No Thetis port, no attribution headers required (per memory:
// feedback_source_first_ui_vs_dsp — Qt widgets in Setup pages are
// NereusSDR-native).
//
// Sub-Phase 12 Task 12.3 (2026-04-20): Four VAX channel cards (1–4)
// + TX row + Auto-detect QMenu picker. Full 7-row DeviceCard form on
// all platforms (addendum §2.2). Mac/Linux amber "override — no
// consumer" badge when bound to non-native with consumerCount == 0.
// Auto-detect QMenu (addendum §2.3): free/assigned/no-cables/scroll.
//
// Task 21 (2026-04-24): Rebuilt per spec §9.2. VAX is now a virtual
// source exposed to the system — not a device the user picks. The
// DeviceCard 7-row form is replaced with:
//   • "Exposed to system as:" label (node description, editable)
//   • "Format:" static label (48000 Hz · Stereo · Float32)
//   • "Consumers:" placeholder (live count deferred to Task 24+)
//   • Level: HGauge meter (quiescent; telemetry wiring deferred)
//   • Rename… / Copy node name buttons
// The per-channel On toggle is preserved. DeviceCard backing is kept
// hidden for API compatibility (applyAutoDetectBinding / clearBinding
// / currentDeviceName / isChannelEnabled test contracts).
//
// Design spec: docs/architecture/2026-04-23-linux-audio-pipewire-plan.md
// §9.2, §10.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
//   2026-04-24 — Task 21 rebuild: spec §9.2 layout, NodeDescription
//                persistence, telemetry placeholder. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-23: R-R3-44: works in a remote window (the VAX channels are
//                this computer's; the page reaches the engine through
//                RadioModel::localAudioDevices()), and the "Consumers:" row
//                says whether an app is reading the channel where the
//                platform reports it. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24: R-R3-43 / R-R3-44 / R-R3-21: a plain note, shown in a
//                remote window while the Core's receiver streams are Opus,
//                that the weakest digital-mode signals may not decode and
//                that Lossless avoids it (setReceiverAudioCompressed, pushed
//                by MainWindow through SetupDialog). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24: R-R3-43 / R-R3-44 fix wave: setReceiverAudioNote replaces
//                setReceiverAudioCompressed; with Lossless chosen but not
//                running the note says the connection cannot carry it right
//                now. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                Code.
//   2026-09-24: R-R3-43 / R-R3-44 / R-R3-23: the note says "a few of the
//                weakest" signals, true of the receiver streams' 48 kbit/s
//                Opus and of an older Core's 24 kbit/s. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-06 - R-SPK-21, R-SPK-22, R-SPK-24, D16 (radio speaker plan
//                Task 11): now the VAX section of Audio > Digital modes, a
//                plain widget rather than a page. One sentence and a status
//                line for this system; each card shows On, Device (the name
//                on Mac and Linux, a picker of detected cables on Windows),
//                Format, Used by, Activity, Rename and Copy name, with a
//                card line when it cannot work; "Detected virtual cables"
//                with Rescan moved here from Advanced. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-09 - native audio plan Task 18 (R-AUD-03, R-AUD-06, R-AUD-10,
//                R-AUD-11, D12, D13): the cables and the Windows pickers
//                come from the engine's device catalogue and follow it
//                live; the Windows picker lists Windows audio cables and
//                ASIO pairs (one driver at a time); a card shows its
//                channel's state sentence; Rescan rescans the older
//                drivers. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/IAudioStreamHost.h"
#include "core/audio/VirtualCableDetector.h"
#include "gui/HGauge.h"
#include "gui/RemoteReceiverAudioNote.h"
#include "gui/setup/DeviceCard.h"
#include "gui/setup/SoundSystemLine.h"

#include <QCheckBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVector>

#include <optional>

class QComboBox;
class QTimer;
class QShowEvent;
class QHideEvent;

namespace NereusSDR {

class AudioEngine;
class RadioModel;

// VaxChannelCard — one VAX channel slot (QGroupBox with enable toggle,
// spec §9.2 info rows, and Rename / Copy buttons).
//
// Internal DeviceCard is kept hidden for API compatibility (backing
// applyAutoDetectBinding / clearBinding / currentDeviceName /
// isChannelEnabled). The visible layout shows only the PipeWire-era
// "exposed, not picked" spec §9.2 rows.
class VaxChannelCard : public QGroupBox {
    Q_OBJECT
public:
    // channel — 1..4. prefix — e.g. "audio/Vax1".
    explicit VaxChannelCard(int channel,
                            QWidget* parent = nullptr);

    // Load saved settings without emitting configChanged.
    void loadFromSettings();

    // Receive the engine-reported negotiated format for this slot.
    void updateNegotiatedPill(const AudioDeviceConfig& negotiated,
                              const QString& errorString = QString());

    // Returns current device name from the inner card (or AppSettings fallback).
    QString currentDeviceName() const;

    // Returns true if the enable checkbox is checked.
    // Named isChannelEnabled() to avoid shadowing QWidget::isEnabled().
    bool isChannelEnabled() const;

    // Apply an auto-detected cable binding in one atomic operation:
    // persists all 10 AppSettings fields, refreshes DeviceCard UI,
    // emits configChanged to the engine, and updates badge visibility.
    void applyAutoDetectBinding(const QString& deviceName);

    // R-AUD-03 / D12: one picked device in one operation, as
    // applyAutoDetectBinding() does for a name: saves the engine, id,
    // name, first channel and (older drivers) host API under audio/VaxN,
    // then emits configChanged.
    void applyBinding(AudioEngineKind engine, const QString& deviceId,
                      const QString& deviceName, int firstChannel,
                      const QString& hostApi = QString());

    // R-AUD-03, D12, D13: the engine whose device catalogue fills the
    // Windows Device picker (the cables, then each ASIO driver's pairs)
    // and whose one-driver rule an ASIO pick follows. Without an engine,
    // or before it has a catalogue, the picker lists the cables only.
    void setAudioEngine(AudioEngine* engine);

    // R-AUD-10 / R-AUD-11: the engine's state for this channel. While it
    // has a sentence (not connected, in use) that is the card's line.
    void setRoleStatus(const AudioRoleStatus& status);

    // Tear down this channel completely: wipes all 10 AppSettings fields,
    // refreshes DeviceCard UI, emits configChanged({}) + enabledChanged(false)
    // to close the engine bus, and updates badge visibility.
    void clearBinding();

    // Pure label builder for the disabled "native (bound automatically)"
    // info row shown at the top of the Auto-detect menu on Mac/Linux when
    // a platform-native HAL/pipe VAX bridge is live. Public so unit tests
    // can assert the label format without opening a modal QMenu.
    static QString nativeHalLabelForCable(const DetectedCable& cable);

#ifdef NEREUS_BUILD_TESTS
    // Test seam — override the cable vector used by onAutoDetectClicked()
    // so unit tests can exercise menu population without PortAudio.
    // Passing an empty optional clears the override (back to real scan).
    void setDetectedCablesForTest(const QVector<DetectedCable>& cables)
    {
        m_testCables = cables;
        m_useTestCables = true;
    }
    void clearDetectedCablesForTest() { m_useTestCables = false; }

    // Simulate the user binding a device via the Auto-detect menu, without
    // going through QMenu::exec(). Exercises the full applyAutoDetectBinding()
    // path (persist + UI refresh + engine emit). Used by tests that verify
    // persistence and UI-refresh contracts without relying on modal interaction.
    void bindDeviceNameForTest(const QString& deviceName)
    {
        applyAutoDetectBinding(deviceName);
    }

    // Returns the menu label text that would be generated for a free (unassigned)
    // cable entry per addendum §2.3 ("► deviceName · vendor"). Used by tests to
    // verify the label format without opening a modal QMenu.
    static QString menuLabelForCableForTest(const DetectedCable& cable)
    {
        const QString vendor =
            VirtualCableDetector::vendorDisplayName(cable.product);
        QString label = QStringLiteral("►  ") + cable.deviceName;
        if (!vendor.isEmpty()) {
            label += QStringLiteral(" · ") + vendor;
        }
        return label;
    }
#endif

    int channelIndex() const { return m_channel; }

    // Reflect the actual AudioEngine bus-open state into the card banner.
    // AudioVaxPage calls this once per card after buildPage() and again on
    // every AudioEngine::vaxConfigChanged / enabledChanged round-trip so
    // the banner doesn't lie when makeVaxBus() fails (HAL plugin missing)
    // or the user toggles the channel off (setVaxEnabled(false) closes
    // the bus).
    void setBusOpen(bool open);
    bool busOpenForTest() const { return m_busOpen; }
    // R-R3-21: the "On" switch from audio/VaxN/Enabled (a container's VAX
    // toggle writes it too). Emits nothing.
    void syncEnabledFromSettings();

    // R-R3-21: the channel's audio level, linear 0..1, as the VAX applet's
    // meters read it (AudioEngine::vaxRxLevel). Shown in dB, -60..0.
    void setLevel(float linear);
    double levelDbForTest() const;
    // R-R3-44: the "Consumers:" row. true / false where the platform
    // reports whether an app is reading this channel's output (macOS,
    // PipeWire), nullopt where it does not or the output is closed.
    void setReaderState(std::optional<bool> reading);
    QString readerText() const;

    // R-SPK-24 / D16: the cables the Windows Device picker offers (output
    // cables other than NereusSDR's own). Does nothing on Mac and Linux,
    // where the Device row shows the channel's own device by name.
    void setCableChoices(const QVector<DetectedCable>& cables);

    // Whether the engine's output for this channel is open.
    bool busOpen() const { return m_busOpen; }
    // True when the channel is on, uses NereusSDR's own device (no cable
    // picked) and that device did not open: on a Mac the driver is
    // blocked, on Linux the sound system could not make it.
    bool ownDeviceFailed() const;
    // The card's line under Activity: why it cannot work, or empty.
    QString statusLineText() const;

signals:
    void configChanged(int channel, NereusSDR::AudioDeviceConfig cfg);
    void enabledChanged(int channel, bool on);
    // The card's state changed (binding, On, open state); the section
    // refreshes its status line.
    void stateChanged(int channel);

private slots:
    void onAutoDetectClicked();
    void onInnerConfigChanged(NereusSDR::AudioDeviceConfig cfg);
    void onInnerEnabledChanged(bool on);
    void updateBadge();
    void onRenameClicked();
    void onCopyNodeNameClicked();

private:
    void buildSpecLayout(QVBoxLayout* outerLayout);
    void updateNodeDescLabel();
    // R-SPK-24: a cable picked in the Windows Device picker; asks before
    // taking a cable another channel uses.
    void onCablePicked(int index);
    // Fills the Windows picker from m_cableChoices and selects the bound
    // cable. Emits nothing.
    void fillPicker();
    // The picker from the catalogue (Windows layout with an engine): the
    // cables under "Virtual cables", each ASIO driver's pairs under its
    // name, then the saved choice as "<name> (not connected)" when it is
    // missing.
    void fillPickerFromCatalogue(IAudioDeviceCatalog& catalogue);
    // A catalogue pick: asks before taking a cable or pair another
    // channel uses and before a second ASIO driver (R-AUD-19).
    void onCataloguePick(int index);
    // The catalogue while the Windows layout has an engine, else null.
    IAudioDeviceCatalog* pickerCatalogue() const;
    AudioRole vaxRole() const;
    // Tooltips and button states that depend on the system and binding.
    void refreshPlatformTexts();

    int          m_channel;
    QString      m_prefix;

    // Hidden backing DeviceCard — preserves API compat for
    // applyAutoDetectBinding / clearBinding / currentDeviceName /
    // isChannelEnabled (test contracts must not regress).
    DeviceCard*  m_deviceCard{nullptr};

    // Legacy widgets — kept hidden; test probes find them via Qt hierarchy.
    QPushButton* m_autoDetectBtn{nullptr};
    QLabel*      m_badgeLabel{nullptr};
    QLabel*      m_statusLabel{nullptr};

    // Mirror of AudioEngine::isVaxBusOpen(m_channel). Defaults to true so
    // standalone card instances (tests, previews without a RadioModel)
    // render the "bound" happy path rather than a misleading amber
    // "unavailable" banner. AudioVaxPage overrides via setBusOpen() once
    // the engine pointer is in hand.
    bool         m_busOpen{true};

    // Spec §9.2 visible widgets.
    QCheckBox*   m_enableChk{nullptr};      // "On" toggle (visible)
    QLabel*      m_nodeDescLabel{nullptr};   // "Device:" value (Mac, Linux)
    QComboBox*   m_devicePicker{nullptr};    // "Device:" cable picker (Windows)
    QVector<DetectedCable> m_cableChoices;
    QPointer<AudioEngine> m_engine;
    std::optional<AudioRoleStatus> m_roleStatus;   // from the engine
    // The saved choice is missing from the catalogue picker; its label.
    QString      m_missingLabel;
    QLabel*      m_asioNote{nullptr};        // an ASIO pair: how apps reach it
    QLabel*      m_cardStatus{nullptr};      // why the channel cannot work
    QLabel*      m_formatLabel{nullptr};     // "Format:" static value
    QLabel*      m_consumerLabel{nullptr};   // "Used by:" value
    HGauge*      m_levelGauge{nullptr};      // "Activity:" meter
    QPushButton* m_renameBtn{nullptr};       // Opens QInputDialog
    QPushButton* m_copyNodeBtn{nullptr};     // "Copy name"

#ifdef NEREUS_BUILD_TESTS
    bool                    m_useTestCables{false};
    QVector<DetectedCable>  m_testCables;
#endif
};

// ---------------------------------------------------------------------------
// AudioVaxPage: the VAX section of Setup > Audio > Digital modes
// (AudioDigitalModesPage). A plain widget, not a Setup page.
// ---------------------------------------------------------------------------
class AudioVaxPage : public QWidget {
    Q_OBJECT
public:
    explicit AudioVaxPage(RadioModel* model, QWidget* parent = nullptr);

    // Returns the VaxChannelCard for the given 1-based channel index,
    // or nullptr if out of range. Used by reassign path in VaxChannelCard
    // to reach the source slot for clearBinding().
    VaxChannelCard* channelCard(int channel) const
    {
        const int idx = channel - 1;
        if (idx >= 0 && idx < m_channelCards.size()) {
            return m_channelCards[idx];
        }
        return nullptr;
    }

    // R-R3-43 / R-R3-44: in a remote window whose receiver streams (the
    // ones feeding VAX) are Opus rather than lossless, shows the
    // compressed-audio note: with Opus chosen it points to the Lossless
    // choice; with Lossless chosen but not running it says the connection
    // cannot carry it right now. None (the default, and always in a local
    // window) hides it. SetupDialog forwards MainWindow's live value.
    void setReceiverAudioNote(RemoteReceiverAudioNote note);
    bool compressedAudioNoteShown() const;
    QString compressedAudioNoteText() const;

    // R-SPK-24: the system the section is laid out for (this build's,
    // unless a test overrides it before building the section).
    static SoundSystemLine::System system();
    // The sentence under the VAX heading for `system`.
    static QString introText(SoundSystemLine::System system);

    // What the status line under the sentence is worked out from.
    struct StatusInputs {
        SoundSystemLine::System system{SoundSystemLine::System::Mac};
        LinuxAudioBackend linuxBackend{LinuxAudioBackend::None};
        int cablesFound{0};        // Windows: virtual output cables found
        bool ownDeviceFailed{false};  // a channel on NereusSDR's device did not open
        bool ownDeviceOpen{false};    // a channel on NereusSDR's device is open
        bool anyOn{false};            // any channel is on
    };
    static QString statusText(const StatusInputs& in);
    static bool statusIsProblem(const StatusInputs& in);

    QString statusLineText() const;
    bool statusLineShowsProblem() const { return m_statusProblem; }

    // "Detected virtual cables" text for the cables found.
    QString detectedCablesText() const;

#ifdef NEREUS_BUILD_TESTS
    // Lays every section built after this call out for `system` (nullopt:
    // this build's).
    static void setSystemForTest(std::optional<SoundSystemLine::System> system);
    // Replaces the scanned cables (no PortAudio), as Rescan would find them.
    void setDetectedCablesForTest(const QVector<DetectedCable>& cables);
#endif

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void buildPage();
    void wirePillFeedback();
    // R-R3-44: refreshes each card's "Used by:" row.
    void refreshReaders();
    // R-SPK-24: the status line, from the system and the cards' state.
    void refreshStatus();
    // The cables found: the "Detected virtual cables" line, the Windows
    // pickers and the status line.
    void applyCables(const QVector<DetectedCable>& cables);
    void onRescan();
    // R-AUD-03: the engine's catalogue, once it has one: the cables come
    // from it and follow it live (no Rescan needed).
    void attachCatalogue();
    // After a rescan: the cables found, and any new ones offered through
    // the first-run dialog.
    void afterRescan(const QVector<DetectedCable>& current);
    // The other channels' pickers again, after `channel` changed (0: every
    // channel's, after a speakers or headphones change).
    void refreshOtherPickers(int channel);
    // R-R3-21: 20 Hz level poll while the page is showing, the VAX
    // applet's cadence (VaxApplet::pollLevels).
    void pollLevels();

    AudioEngine*                m_engine{nullptr};
    QVector<VaxChannelCard*>    m_channelCards;  // index 0 = channel 1
    QLabel*                     m_compressedNote{nullptr};
    QLabel*                     m_introLabel{nullptr};
    QLabel*                     m_statusDot{nullptr};
    QLabel*                     m_statusLabel{nullptr};
    bool                        m_statusProblem{false};
    QLabel*                     m_cablesLabel{nullptr};
    QPushButton*                m_rescanButton{nullptr};
    QVector<DetectedCable>      m_cables;
    QPointer<IAudioDeviceCatalog> m_catalogue;
    bool                        m_rescanWaiting{false};  // a Rescan awaits the new list
    QTimer*                     m_levelTimer{nullptr};
};

} // namespace NereusSDR
