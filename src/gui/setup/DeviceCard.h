#pragma once

// =================================================================
// src/gui/setup/DeviceCard.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → Devices card widget.
// No Thetis port, no attribution headers required (per memory:
// feedback_source_first_ui_vs_dsp — Qt widgets in Setup pages are
// NereusSDR-native).
//
// Sub-Phase 12 Task 12.2 (2026-04-20): QGroupBox subclass parameterized
// by settings-prefix + role enum (Output/Input). 7-row form per
// addendum §2.1: Driver API / Device / Sample rate / Bit depth /
// Channels / Buffer size / Options. Negotiated-format pill at the
// bottom. 200 ms intra-control debounce on the buffer-size combo only
// (addendum §2.1: "intra-control only"); all other controls fire
// configChanged immediately.
//
// Design spec: docs/architecture/2026-04-20-phase3o-subphase12-addendum.md
// §§2.1 + 4.
//
// Modification history (NereusSDR):
//   2026-10-06: R-SPK-21, R-SPK-24, D14 by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code. The
//               Device row stays in front and the other rows fold under
//               "Device details"; the WASAPI options are greyed unless the
//               driver API is WASAPI; rows above and below Device, greying
//               until Enabled, and a device rescan for the Outputs page.
//   2026-10-06: R-SPK-21 (Microphone) by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code. The
//               Device, Driver API and Buffer size combos are reachable
//               for the Microphone page's PC microphone card.
//   2026-10-09: native audio plan Task 16 (R-AUD-01, R-AUD-03, R-AUD-08 to
//               R-AUD-11, R-AUD-14 to R-AUD-17, D10) by J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code. One Driver list
//               replaces the Driver API list and the three WASAPI
//               checkboxes; devices come from the engine's device catalogue
//               and follow it live; the role's status notes, the engine
//               notes and the Delay line.
// =================================================================

#include "core/AudioDeviceConfig.h"
#include "core/audio/IAudioStreamHost.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QPointer>

#include <optional>

class QTimer;
class QToolButton;
class QVBoxLayout;

namespace NereusSDR {

class AudioEngine;
class IAudioDeviceCatalog;

// DeviceCard — one audio-endpoint group box.
//
// Emits configChanged(AudioDeviceConfig) whenever any control changes
// (commit semantics per addendum §2.1). The caller (AudioDevicesPage)
// connects configChanged to the appropriate AudioEngine::set<Role>Config()
// and feeds the resulting AudioEngine::<role>ConfigChanged back to
// updateNegotiatedPill().
//
// Role::Output → 7 rows.
// Role::Input  → 7 rows + monitor-during-TX + tone-check extras (TX card).
class DeviceCard : public QGroupBox {
    Q_OBJECT
public:
    enum class Role { Output, Input };

    // prefix — "audio/Speakers", "audio/Headphones", or "audio/TxInput".
    // role   — Output or Input (determines capture direction + extras).
    // enableCheckbox — if true, a checkbox in the group box title enables
    //   or disables the card (used for Headphones).
    explicit DeviceCard(const QString& prefix,
                        Role role,
                        bool enableCheckbox = false,
                        QWidget* parent = nullptr);

    // Build the AudioDeviceConfig from the card's current control state.
    AudioDeviceConfig currentConfig() const;

    // Called by AudioDevicesPage when the engine responds with the
    // negotiated format (or an error). Updates the pill at the bottom.
    void updateNegotiatedPill(const AudioDeviceConfig& negotiated,
                              const QString& errorString = QString());

    // Convenience: read initial state from AppSettings at startup.
    // Called once after construction; does NOT emit configChanged.
    void loadFromSettings();

    // Returns the live checked state of the enable checkbox (Headphones /
    // VaxChannelCard only). Returns true if there is no enable checkbox
    // (always-on cards are considered always enabled).
    bool isCheckboxEnabled() const
    {
        return m_enableChk ? m_enableChk->isChecked() : true;
    }

    // Grey out the enable checkbox (e.g. VaxChannelCard on Windows when no
    // BYO device has been picked, so the user can't toggle Enabled into the
    // "open platform default = speakers" footgun). No-op when there is no
    // enable checkbox on this card.
    void setEnableAllowed(bool allowed)
    {
        if (m_enableChk) {
            m_enableChk->setEnabled(allowed);
            m_enableChk->setToolTip(
                allowed
                    ? QString()
                    : QStringLiteral("Pick a device first"));
        }
    }

    // ── R-SPK-21 / D14: Device details ──────────────────────────────────
    // Driver, Sample rate (with Auto-match), Bit depth, Channels, Buffer
    // size (with milliseconds), Delay, Negotiated and the engine note sit in
    // a "Device details" section (objectName "deviceDetails", toggled by
    // the "deviceDetailsToggle" button), folded by default.
    bool detailsExpanded() const;
    void setDetailsExpanded(bool expanded);

    // A page's own rows: above the Device row (outside the part greyed
    // until Enabled), or below it, above Device details. The card takes
    // ownership.
    void addAboveDevice(QWidget* widget);
    void addBelowDevice(QWidget* widget);

    // R-SPK-21: Device and Device details greyed while the Enabled box is
    // off (Headphones). Off by default; no-op without an Enabled box.
    void setGreyedUntilEnabled(bool greyed);

    // Re-reads the device list for the card's driver, keeping the selected
    // device ("Rescan devices").
    void rescanDevices();
    // Devices the list offers, without "(platform default)", "(none)" or a
    // kept "(not connected)" entry.
    int deviceCount() const;

    // R-SPK-21: the Microphone page and its tests reach the card's own
    // Device, Driver and Buffer size controls.  driverApiCombo() holds the
    // Driver list (R-AUD-01, D10).
    QComboBox* deviceCombo() const { return m_deviceCombo; }
    QComboBox* driverApiCombo() const { return m_driverApiCombo; }
    QComboBox* bufferSizeCombo() const { return m_bufferSizeCombo; }

    // ── Native audio engines (R-AUD-01, R-AUD-03, R-AUD-08 to R-AUD-17) ──
    // The card reads the engine's device catalogue and its role's status
    // and follows their signals: the Driver and Device lists, the state
    // note ("deviceStateNote"), the Delay line ("deviceDelayCombo",
    // "deviceDelayNow") and the engine note ("engineNote").  The catalogue
    // is picked up once the engine has one (it builds it on first use).
    // Without an engine the card lists "(platform default)" and the saved
    // device only.
    void setAudioEngine(AudioEngine* engine);
    // The role the card's prefix names (audio/Speakers, audio/Headphones,
    // audio/TxInput, audio/Vax1 to audio/Vax4).
    static std::optional<AudioRole> roleForPrefix(const QString& prefix);

signals:
    // Emitted on any control edit (excluding loadFromSettings).
    // Carries the card's current AudioDeviceConfig.
    void configChanged(NereusSDR::AudioDeviceConfig cfg);

    // Emitted when the enable checkbox changes (Headphones card only).
    void enabledChanged(bool enabled);

private slots:
    void onAnyControlChanged();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    // The device the card has selected: its saved identity and name.
    struct Selection {
        QString deviceId;
        QString deviceName;
        int firstChannel = 1;
    };

    void buildLayout();
    void populateDriverCombo();
    void populateDeviceCombo();
    void selectDevice();
    void updateBufferMsLabel();  // recompute derived ms readout from current combos
    void updateBodyEnabled();
    void onDriverPicked();
    void onDevicePicked();
    void attachCatalogue();
    void takeSavedChoice(const AudioDeviceConfig& saved);
    void refreshStatus();
    void refreshDelayNow();
    void renderPill();
    void updateEngineNote();
    QString deviceNameForId(const QString& deviceId) const;
    AudioEngineKind selectedEngine() const;

    QString       m_prefix;
    Role          m_role;
    bool          m_suppressSignals{false};

    // Controls
    QCheckBox*  m_enableChk{nullptr};   // title-bar enable (Headphones only)
    QComboBox*  m_driverApiCombo{nullptr};   // the Driver list (R-AUD-01)
    QComboBox*  m_deviceCombo{nullptr};
    QComboBox*  m_sampleRateCombo{nullptr};
    QCheckBox*  m_autoMatchSampleRate{nullptr};
    QComboBox*  m_bitDepthCombo{nullptr};
    QComboBox*  m_channelsCombo{nullptr};
    QComboBox*  m_bufferSizeCombo{nullptr};
    QLabel*     m_bufferMsLabel{nullptr};  // derived milliseconds readout
    QComboBox*  m_delayCombo{nullptr};     // R-AUD-15: DelayMs
    QLabel*     m_delayNow{nullptr};       // R-AUD-15: "Now X ms ..."
    QLabel*     m_stateNote{nullptr};      // R-AUD-08 to R-AUD-11, R-AUD-14
    QLabel*     m_engineNote{nullptr};     // R-AUD-16
    // TX-input extras
    QCheckBox*  m_monitorDuringTxChk{nullptr};
    QCheckBox*  m_toneCheckChk{nullptr};

    // Negotiated-format pill
    QLabel*     m_negotiatedPill{nullptr};
    std::optional<AudioDeviceConfig> m_negotiated;
    QString     m_negotiatedError;
    bool        m_applying{false};

    // R-SPK-21 / D14
    QVBoxLayout* m_aboveDeviceLayout{nullptr};
    QVBoxLayout* m_belowDeviceLayout{nullptr};
    QWidget*     m_body{nullptr};          // Device row + details
    QToolButton* m_detailsToggle{nullptr};
    QWidget*     m_details{nullptr};
    bool         m_greyedUntilEnabled{false};

    // Native audio engines
    std::optional<AudioRole>        m_audioRole;
    QPointer<AudioEngine>           m_engine;
    QPointer<IAudioDeviceCatalog>   m_catalogue;
    QTimer*                         m_refreshTimer{nullptr};   // 1 s: delay readout, catalogue pick-up
    AudioDeviceConfig               m_loaded;                  // the saved config, as last loaded
    std::optional<AudioEngineKind>  m_driverEngine;            // the Driver list's choice
    QString                         m_driverHostApi;           // older drivers: the host API
    Selection                       m_selection;
    AudioRoleStatus                 m_status;

    // 200 ms intra-control debounce for the buffer-size combo only (per
    // addendum §2.1 — debounce is intra-control, not card-wide).  Other
    // control changes fire onAnyControlChanged immediately.
    QTimer*     m_bufferSizeDebounceTimer{nullptr};
};

} // namespace NereusSDR
