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
// =================================================================

#include "core/AudioDeviceConfig.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>

class QTimer;
class QToolButton;
class QVBoxLayout;

namespace NereusSDR {

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
    // Driver API, Sample rate (with Auto-match), Bit depth, Channels,
    // Buffer size (with milliseconds), Options and Negotiated sit in a
    // "Device details" section (objectName "deviceDetails", toggled by the
    // "deviceDetailsToggle" button), folded by default.
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

    // Re-reads the device list for the card's driver API, keeping the
    // selected device ("Rescan devices").
    void rescanDevices();
    // Devices the list offers, without "(platform default)" or a kept
    // "(not available)" entry.
    int deviceCount() const;

    // ── R-SPK-24: Exclusive / Event-driven / Bypass mixer ───────────────
    // Live only when the card's driver API is WASAPI; otherwise disabled
    // with wasapiOnlyReason() as their tooltip, and a note says so.
    bool wasapiOptionsAvailable() const;
    static bool isWasapiDriverName(const QString& driverApi);
    static QString wasapiOnlyReason();

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
    void buildLayout();
    void populateDeviceCombo();
    void selectDeviceName(const QString& name);
    void updateBufferMsLabel();  // recompute derived ms readout from current combos
    void updateWasapiOptions();
    void updateBodyEnabled();

    QString       m_prefix;
    Role          m_role;
    bool          m_suppressSignals{false};

    // Controls
    QCheckBox*  m_enableChk{nullptr};   // title-bar enable (Headphones only)
    QComboBox*  m_driverApiCombo{nullptr};
    QComboBox*  m_deviceCombo{nullptr};
    QComboBox*  m_sampleRateCombo{nullptr};
    QCheckBox*  m_autoMatchSampleRate{nullptr};
    QComboBox*  m_bitDepthCombo{nullptr};
    QComboBox*  m_channelsCombo{nullptr};
    QComboBox*  m_bufferSizeCombo{nullptr};
    QLabel*     m_bufferMsLabel{nullptr};  // derived milliseconds readout
    QCheckBox*  m_exclusiveChk{nullptr};   // WASAPI exclusive mode
    QCheckBox*  m_eventDrivenChk{nullptr}; // WASAPI event-driven
    QCheckBox*  m_bypassMixerChk{nullptr}; // WASAPI bypass mixer
    // TX-input extras
    QCheckBox*  m_monitorDuringTxChk{nullptr};
    QCheckBox*  m_toneCheckChk{nullptr};

    // Negotiated-format pill
    QLabel*     m_negotiatedPill{nullptr};

    // R-SPK-21 / D14 / R-SPK-24
    QVBoxLayout* m_aboveDeviceLayout{nullptr};
    QVBoxLayout* m_belowDeviceLayout{nullptr};
    QWidget*     m_body{nullptr};          // Device row + details
    QToolButton* m_detailsToggle{nullptr};
    QWidget*     m_details{nullptr};
    QLabel*      m_wasapiNote{nullptr};
    bool         m_greyedUntilEnabled{false};

    // 200 ms intra-control debounce for the buffer-size combo only (per
    // addendum §2.1 — debounce is intra-control, not card-wide).  Other
    // control changes fire onAnyControlChanged immediately.
    QTimer*     m_bufferSizeDebounceTimer{nullptr};
};

} // namespace NereusSDR
