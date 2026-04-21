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
// Design spec: docs/architecture/2026-04-20-phase3o-subphase12-addendum.md
// §§2.2 + 2.3.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
// =================================================================

#include "core/audio/VirtualCableDetector.h"
#include "gui/SetupPage.h"
#include "gui/setup/DeviceCard.h"

#include <QLabel>
#include <QPushButton>
#include <QVector>

namespace NereusSDR {

class AudioEngine;

// VaxChannelCard — one VAX channel slot (QGroupBox with enable checkbox,
// 7-row DeviceCard form, Auto-detect button, amber badge).
//
// Built on top of DeviceCard for the 7-row form. Adds channel identity,
// Auto-detect button (unassigned slots only), and the Mac/Linux amber
// "override — no consumer" badge.
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

    // Returns current device name from the inner card.
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
        QString label = QStringLiteral("\u25ba  ") + cable.deviceName;
        if (!vendor.isEmpty()) {
            label += QStringLiteral(" \u00b7 ") + vendor;
        }
        return label;
    }
#endif

    int channelIndex() const { return m_channel; }

signals:
    void configChanged(int channel, NereusSDR::AudioDeviceConfig cfg);
    void enabledChanged(int channel, bool on);

private slots:
    void onAutoDetectClicked();
    void onInnerConfigChanged(NereusSDR::AudioDeviceConfig cfg);
    void onInnerEnabledChanged(bool on);
    void updateBadge();

private:
    int          m_channel;
    QString      m_prefix;
    DeviceCard*  m_deviceCard{nullptr};
    QPushButton* m_autoDetectBtn{nullptr};
    QLabel*      m_badgeLabel{nullptr};

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
