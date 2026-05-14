#pragma once

// =================================================================
// src/gui/setup/AudioTciPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup -> Audio -> TCI page.
// No Thetis port, no attribution headers required (per memory:
// feedback_source_first_ui_vs_dsp -- Qt widgets in Setup pages are
// NereusSDR-native).
//
// Phase 24 Task 24.2 (2026-05-10): Flesh out AudioTciPage.
//   Four group boxes:
//     1. Output Sample Rate per Slice (Slice A / Slice B combos)
//     2. Sample Format (format combo / channels combo / block size)
//     3. TX Direction (TX channel combo / TX buffering spinbox)
//     4. Master Mute Note (read-only info label)
//   AppSettings keys per design doc Section 2.7.
//
// Note: TciAudioStreamSamples / TciTxChannel are also exposed on
// the Setup -> Network -> TCI Server page (CatTciServerPage).
// Both pages read/write the same AppSettings keys -- last writer
// wins. This is intentional: the pages serve different audiences
// (audio engineer vs. network/protocol configurator).
//
// Design spec: docs/architecture/2026-04-19-vax-design.md Section 2.7,
//              Section 10 (AppSettings inventory).
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 -- Phase 24 (Task 24.2): written by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/SetupPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// AudioTciPage
//
// Audio bridge configuration for the TCI server. Wires:
//   - Per-slice output sample rate (Slice A / Slice B)
//   - Audio stream sample format + channel count + block size
//   - TX audio direction + TX buffering
//   - Read-only note explaining master-mute independence
// ---------------------------------------------------------------------------
class AudioTciPage : public SetupPage {
    Q_OBJECT
public:
    explicit AudioTciPage(RadioModel* model, QWidget* parent = nullptr);

private:
    // Group 1: Output Sample Rate per Slice
    QComboBox* m_sliceARateCombo{nullptr};
    QComboBox* m_sliceBRateCombo{nullptr};

    // Group 2: Sample Format
    QComboBox* m_formatCombo{nullptr};
    QComboBox* m_channelsCombo{nullptr};
    QSpinBox*  m_blockSizeSpin{nullptr};

    // Group 3: TX Direction
    QComboBox* m_txChannelCombo{nullptr};
    QSpinBox*  m_txBufferingSpin{nullptr};

    void buildUI();
    void buildSampleRateGroup();
    void buildFormatGroup();
    void buildTxDirectionGroup();
    void buildMasterMuteNoteGroup();
};

} // namespace NereusSDR
