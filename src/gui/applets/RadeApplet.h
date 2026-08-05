// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - RadeApplet: RADE-mode control + status applet.
//
// NEW NereusSDR-native applet, no upstream Thetis equivalent.  RADE is
// the FreeDV RADE neural voice codec port (Phase 3R); the channel
// itself lives in src/core/RadeChannel.{h,cpp} (Tasks I1-I3).  This
// applet surfaces the RadeChannel control + status to the user when
// the active slice's mode is DSPMode::RADE_U or DSPMode::RADE_L.
//
// Modification history (NereusSDR):
//   2026-05-11 - Created for Phase 3R Task L2 by J.J. Boyd (KG4VCF),
//                with AI-assisted implementation via Anthropic Claude
//                Code.  Structural pattern follows PhoneCwApplet
//                (NereusSDR PhoneCwApplet, GPLv2).

#pragma once

#include "AppletWidget.h"

#include <limits>

class QComboBox;
class QLabel;
class QPushButton;

namespace NereusSDR {

class SliceModel;
struct RxDecode;

// RadeApplet - applet surface for the RADE neural voice codec.
//
// Controls:
//   * TX mic profile combo, defaults to "RADE" preset (K1 factory entry).
//   * Sync indicator: dim-grey / yellow / green LED based on the active
//     slice's RADE sync state and SNR estimate.
//   * SNR readout: mirrors the VfoWidget L1 SNR row format
//     ("+N dB" / " -   - " / colour by threshold).
//   * Freq offset readout (Hz, drives off the new
//     RadioModel::radeFreqOffsetChanged signal).
//   * Last decoded callsign + grid label, subscribes to
//     RxDecodeModel::decodeAdded filtered to source == "rade_text".
//   * "Reset vocoder" button - calls RadeChannel::resetTx on the
//     active slice's RadeChannel.  Disabled if the slice has no
//     RadeChannel (mode != RADE) at click time.
//
// Visibility is gated on the active slice's DSPMode being either RADE
// sideband (RADE_U or RADE_L): when the user switches the active slice
// out of a RADE mode the applet should be hidden by the parent panel.
// MainWindow tracks the mode-change in the same lambda that flips
// PhoneCwApplet between Phone/CW/FM pages.
class RadeApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit RadeApplet(RadioModel* model, QWidget* parent = nullptr);
    ~RadeApplet() override = default;

    QString appletId()    const override { return QStringLiteral("RADE"); }
    QString appletTitle() const override { return QStringLiteral("RADE"); }
    void syncFromModel() override;

    // Test seams (Phase 3R L2).  Exposed so tst_rade_applet can verify
    // text / colour / wiring without depending on widget geometry.
    QComboBox*   profileComboForTest()       const { return m_profileCombo; }
    QLabel*      snrLabelForTest()           const { return m_snrLabel; }
    QLabel*      freqOffsetLabelForTest()    const { return m_freqOffsetLabel; }
    QLabel*      lastDecodedLabelForTest()   const { return m_lastDecodedLabel; }
    QLabel*      syncIndicatorForTest()      const { return m_syncIndicator; }
    QPushButton* resetVocoderButtonForTest() const { return m_resetButton; }

private slots:
    // Subscribed to RadioModel::radeSyncChanged.  Filters to the
    // active-slice ID then updates m_syncIndicator colour.
    void onSyncChanged(int sliceId, bool synced);

    // Subscribed to RadioModel::radeSnrChanged.  Filters to the
    // active-slice ID then updates m_snrLabel text + colour.
    void onSnrChanged(int sliceId, float snrDb);

    // Subscribed to RadioModel::radeFreqOffsetChanged (added in L2
    // alongside this applet).  Filters to the active-slice ID then
    // updates m_freqOffsetLabel.
    void onFreqOffsetChanged(int sliceId, float hz);

    // Subscribed to RxDecodeModel::decodeAdded.  Filters to
    // source == "rade_text" then updates m_lastDecodedLabel.
    void onDecodeAdded(const RxDecode& decode);

    // Reset vocoder button click.  Looks up the active slice's
    // RadeChannel and calls resetTx() if present.
    void onResetVocoderClicked();

    // Active-slice or active-profile changed -> resync the profile
    // combo display (without re-invoking setActiveProfile).
    void onActiveProfileChanged(const QString& name);

    // User changed the profile combo selection -> push to
    // MicProfileManager::setActiveProfile.
    void onProfileComboActivated(const QString& name);

private:
    void buildUI();
    void wireSignals();

    // Update m_syncIndicator stylesheet for the current sync + SNR pair.
    void repaintSyncIndicator();

    // Active sync + SNR state cached for the indicator colour logic
    // (sync false -> grey, sync true & snr < 5 -> yellow,
    // sync true & snr >= 5 -> green).
    bool   m_synced{false};
    double m_lastSnrDb{std::numeric_limits<double>::quiet_NaN()};

    QComboBox*   m_profileCombo{nullptr};
    QLabel*      m_syncIndicator{nullptr};
    QLabel*      m_snrLabel{nullptr};
    QLabel*      m_freqOffsetLabel{nullptr};
    QLabel*      m_lastDecodedLabel{nullptr};
    QPushButton* m_resetButton{nullptr};
};

}  // namespace NereusSDR
