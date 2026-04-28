#pragma once

// =================================================================
// src/gui/setup/AudioTxInputPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → TX Input page.
// No Thetis port, no attribution headers required (per memory:
// feedback_source_first_ui_vs_dsp — Qt widgets in Setup pages are
// NereusSDR-native).
//
// Phase 3M-1b Task I.1 (2026-04-28): Top-level PC Mic / Radio Mic
// radio buttons. Radio Mic disabled with tooltip
// "Radio mic jack not present on Hermes Lite 2" when
// BoardCapabilities::hasMicJack == false. Selection persists through
// TransmitModel::micSource (new property; AppSettings persistence
// deferred to L.2).
//
// Design spec: docs/architecture/phase3m-1b-mic-ssb-voice-plan.md
// §3 Phase I (I.1) + pre-code review §5.4 + master design §5.2.2.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-28 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "gui/SetupPage.h"
#include "core/audio/CompositeTxMicRouter.h"

#include <QButtonGroup>
#include <QRadioButton>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// AudioTxInputPage — Setup → Audio → TX Input
//
// Top-level mic-source selector:
//   • "PC Mic"    — always enabled; default.
//   • "Radio Mic" — disabled with tooltip when hasMicJack == false (HL2).
//
// Selection change calls TransmitModel::setMicSource. Model changes
// (setMicSource from elsewhere) drive the radio-button check state via
// micSourceChanged signal connection. Two-way sync uses QSignalBlocker
// to prevent echo loops.
//
// Capability gating is static at construction time: hasMicJack is read
// from RadioModel::boardCapabilities() once in the constructor and the
// Radio Mic button enabled-state is fixed. Dynamic capability change
// (reconnect to a different board type) is deferred.
// TODO [3M-1b I.x]: dynamic hasMicJack refresh on currentRadioChanged,
// once RadioModel emits a capability-change signal.
// ---------------------------------------------------------------------------
class AudioTxInputPage : public SetupPage {
    Q_OBJECT
public:
    explicit AudioTxInputPage(RadioModel* model, QWidget* parent = nullptr);

private slots:
    void onButtonToggled(int id, bool checked);
    void onModelMicSourceChanged(MicSource source);

private:
    void buildPage(bool hasMicJack);
    void syncButtonsFromModel(MicSource source);

    QButtonGroup*  m_buttonGroup{nullptr};
    QRadioButton*  m_pcMicBtn{nullptr};
    QRadioButton*  m_radioMicBtn{nullptr};

    bool m_updatingFromModel{false};
};

} // namespace NereusSDR
