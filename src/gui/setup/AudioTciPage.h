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
//     1. Output Sample Rate per Slice (Slice A combo)
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
//   2026-10-06 -- R-SPK-21, R-SPK-22: J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. The TCI section of Audio > Digital
//                 modes: the Master Mute box becomes one sentence; the
//                 settings sit in "Audio stream" and "Transmit" groups.
// =================================================================

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QWidget>

class QFormLayout;
class QGroupBox;
class QVBoxLayout;

namespace NereusSDR {

class RadioModel;

// ---------------------------------------------------------------------------
// AudioTciPage
//
// Audio bridge configuration for the TCI server. Wires:
//   - Per-slice output sample rate (Slice A)
//   - Audio stream sample format + channel count + block size
//   - TX audio direction + TX buffering
//   - A sentence that TCI audio is separate from the speaker volumes
//
// R-SPK-21: the TCI section of Setup > Audio > Digital modes, a plain
// widget that AudioDigitalModesPage hosts (it owns the title and scroll).
// ---------------------------------------------------------------------------
class AudioTciPage : public QWidget {
    Q_OBJECT
public:
    explicit AudioTciPage(RadioModel* model, QWidget* parent = nullptr);

private:
    // Group 1: Output Sample Rate per Slice
    QComboBox* m_sliceARateCombo{nullptr};

    // Group 2: Sample Format
    QComboBox* m_formatCombo{nullptr};
    QComboBox* m_channelsCombo{nullptr};
    QSpinBox*  m_blockSizeSpin{nullptr};

    // Group 3: TX Direction
    QComboBox* m_txChannelCombo{nullptr};
    QSpinBox*  m_txBufferingSpin{nullptr};

    QVBoxLayout* m_layout{nullptr};

    void buildUI();
    // "Audio stream" group rows.
    void buildSampleRateGroup(QGroupBox* group, QFormLayout* form);
    void buildFormatGroup(QGroupBox* group, QFormLayout* form);
    // "Transmit" group rows.
    void buildTxDirectionGroup(QGroupBox* group, QFormLayout* form);
};

} // namespace NereusSDR
