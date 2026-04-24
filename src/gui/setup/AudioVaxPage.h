#pragma once

// =================================================================
// src/gui/setup/AudioVaxPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → VAX page.
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
// =================================================================

#include "core/audio/VirtualCableDetector.h"
#include "gui/HGauge.h"
#include "gui/SetupPage.h"
#include "gui/setup/DeviceCard.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVector>

namespace NereusSDR {

class AudioEngine;

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

signals:
    void configChanged(int channel, NereusSDR::AudioDeviceConfig cfg);
    void enabledChanged(int channel, bool on);

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
    QLabel*      m_nodeDescLabel{nullptr};   // "Exposed to system as:" value
    QLabel*      m_formatLabel{nullptr};     // "Format:" static value
    QLabel*      m_consumerLabel{nullptr};   // "Consumers:" placeholder
    HGauge*      m_levelGauge{nullptr};      // Level meter (quiescent)
    QPushButton* m_renameBtn{nullptr};       // Opens QInputDialog
    QPushButton* m_copyNodeBtn{nullptr};     // Copies nereussdr.vax-N

#ifdef NEREUS_BUILD_TESTS
    bool                    m_useTestCables{false};
    QVector<DetectedCable>  m_testCables;
#endif
};

// ---------------------------------------------------------------------------
// AudioVaxPage
// ---------------------------------------------------------------------------
class AudioVaxPage : public SetupPage {
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

private:
    void buildPage();
    void wirePillFeedback();

    AudioEngine*                m_engine{nullptr};
    QVector<VaxChannelCard*>    m_channelCards;  // index 0 = channel 1
};

} // namespace NereusSDR
