// =================================================================
// src/gui/applets/TxCfcDialog.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmCFCConfig.{cs,Designer.cs}
//   [v2.10.3.13] — the per-band CFC (Continuous Frequency Compressor)
//   editor.  Original licence reproduced below.
//
// This file replaces the spartan 10-row-grid TxCfcDialog from
// Phase 3M-3a-ii Batch 6 with a full Thetis-faithful 1:1 port that
// embeds two ParametricEqWidget instances (compression curve + post-EQ
// curve) per the Thetis frmCFCConfig layout.
//
// Layout reference (Thetis frmCFCConfig.Designer.cs:30-776 [v2.10.3.13]):
//   - Top edit row (#, f, Pre-Comp, Comp, Q) above ucCFC_comp.
//   - ucCFC_comp (compression curve + bar chart, 12,36 → 521,356).
//   - Middle edit row (Post-EQ, Gain, dB, Q) between widgets.
//   - ucCFC_eq (post-EQ curve, 12,387 → 521,707).
//   - Right column: 5/10/18-band radios, Low/High freq spinboxes,
//     Use Q Factors / Live Update / Log scale checkboxes,
//     Reset Comp / Reset EQ buttons, OG CFC Guide LinkLabel.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-02 — Native matched editor and paired typed-profile/session
//                 history by J.J. Boyd (KG4VCF), with OpenAI Codex.
//   2026-04-30 — Phase 3M-3a-ii Batch 6 (Task A): created by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.  Modeless lazy-singleton
//                 owned by TxApplet (m_cfcDialog).  Bidirectional
//                 binding to TransmitModel for 32 controls (10 freq +
//                 10 comp + 10 post-EQ band gain + 2 globals).
//   2026-04-30 — Phase 3M-3a-ii follow-up sub-PR Batch 8: full
//                 Thetis-verbatim rewrite by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 Drops the spartan 10-row grid + profile combo for two
//                 embedded ParametricEqWidget instances cross-synced per
//                 frmCFCConfig.cs:218-306 [v2.10.3.13].  50ms QTimer-
//                 driven bar chart fed by Task 7
//                 TxChannel::getCfcDisplayCompression wrapper.
//   2026-09-25 - R-R3-49 (parity Task 4): setSettingsPermitted greys the
//                 controls with a reason in a remote window while the Core
//                 cannot take a CFC change. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-27 - R-R3-49 (parity Task 33): in a remote window the bar
//                 chart reads the Core's txCfcCompression stream while the
//                 dialog is shown (setStationBarChart,
//                 applyStationCompression), or says why it cannot. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Setup publication (CFC band editor): a remote window
//                 sends the whole band table as the Core's cfc.setProfile
//                 command (setStationProfileSender, onStationCommandFinished),
//                 one at a time with the newest edit held, and shows a
//                 refusal under the controls. An older Core keeps the
//                 property write. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

//=================================================================
// frmCFCConfig.cs
//=================================================================
//  frmCFCConfig.cs
//
// This file is part of a program that implements a Software-Defined Radio.
//
// This code/file can be found on GitHub : https://github.com/ramdor/Thetis
//
// Copyright (C) 2020-2026 Richard Samphire MW0LGE
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// The author can be reached by email at
//
// mw0lge@grange-lane.co.uk
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#pragma once

#include <QCloseEvent>
#include <QDialog>
#include <functional>
#include <QPointer>
#include <QShowEvent>
#include <QHash>
#include "core/CfcProfile.h"
#include "core/CfcEditProfile.h"

class QButtonGroup;
class QCheckBox;
class QLabel;
class QDoubleSpinBox;
class QHideEvent;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTimer;
class QSlider;
class QLabel;
class QHBoxLayout;
class QAbstractSpinBox;

namespace NereusSDR {

class ParametricEqWidget;
class EqEditHistory;
class TransmitModel;
class TxChannel;

// Modeless native CFC editor. Both graphs share frequency/selection and one
// exact session history; complete typed profiles route through TransmitModel.
// Measured compression bars retain the existing 50 ms show/hide lifecycle.
class TxCfcDialog : public QDialog {
    Q_OBJECT

public:
    explicit TxCfcDialog(TransmitModel* tm,
                         TxChannel* tx,
                         QWidget* parent = nullptr);
    ~TxCfcDialog() override;

    // Inject / replace the TxChannel (e.g. when the connection comes up
    // after the dialog was lazy-created).  Null is allowed — the bar chart
    // simply skips the WDSP poll until a TxChannel is available.
    void setTxChannel(TxChannel* tx);

    // R-R3-49 (parity Task 4): whether this window may change CFC now. A
    // remote window's TxApplet sets it from the Core's
    // transmitSettingsVersion 4 and the Core's on-the-air state; closed,
    // every control greys and the reason shows at the top.
    void setSettingsPermitted(bool permitted, const QString& reason);
    QLabel* settingsReasonLabel() const { return m_settingsReasonLabel; }

    // R-R3-49 (parity Task 33): a remote window's bar chart. With a hook
    // set, showing the dialog asks for the Core's CFC display
    // (setWanted(true)) instead of reading a local TxChannel, and hiding it
    // lets it go (setWanted(false)). applyStationCompression draws one of
    // the Core's readings (TxChannel::kCfcDisplayBinCount values) over the
    // chart's range exactly as a local reading is drawn.
    void setStationBarChart(std::function<void(bool)> setWanted);
    void applyStationCompression(const QList<double>& binsDb);
    // Why the chart has no bars from the Core (a Core that does not send
    // them); empty hides the note.
    void setBarChartUnavailable(const QString& reason);
    QLabel* barChartReasonLabel() const { return m_barChartReasonLabel; }

    // Setup publication (CFC band editor): a remote window's route to the
    // Core's cfc.setProfile command. With both hooks set and available()
    // true, an edit sends the whole table (CfcProfile::publishedJson) with
    // the revision the window last saw, instead of writing cfcParaEqData.
    // One command is out at a time; later edits wait as the newest one.
    // available() false (an older Core) keeps the property write.
    struct StationProfileSend {
        bool sent = false;
        QString reason;
        quint32 commandId = 0;
    };
    using StationProfileAvailable = std::function<bool()>;
    using StationProfileSender =
        std::function<StationProfileSend(const QString& profileJson,
                                         const QString& expectedRevision)>;
    void setStationProfileSender(StationProfileAvailable available,
                                 StationProfileSender send);
    // RadioModel::stationCommandFinished; only this dialog's command counts.
    void onStationCommandFinished(quint32 commandId, bool accepted,
                                  const QString& reason);
    // RadioModel::stationLinkStateChanged. A lost link takes the answer to
    // this dialog's command with it: the change waiting on it and any edit
    // held behind it are dropped, and the dialog follows the Core's values
    // again.
    void onStationLinkChanged(bool ready);
    // Why the Core did not take the last change; hidden when it did.
    QLabel* profileReasonLabel() const { return m_profileReasonLabel; }

    // ── Widget accessors for tests ────────────────────────────────────────

    ParametricEqWidget* compWidget()    const { return m_compWidget; }
    ParametricEqWidget* postEqWidget()  const { return m_postEqWidget; }

    // Selected-band inputs below both graphs; pre-compression is global.
    QSpinBox*       selectedBandSpin() const { return m_selectedBandSpin; }
    QSpinBox*       freqSpin()         const { return m_freqSpin; }
    QDoubleSpinBox* precompSpin()      const { return m_precompSpin; }
    QDoubleSpinBox* compSpin()         const { return m_compSpin; }
    QDoubleSpinBox* compQSpin()        const { return m_compQSpin; }

    // Independent post-EQ band inputs and global gain.
    QDoubleSpinBox* postEqGainSpin()   const { return m_postEqGainSpin; }
    QDoubleSpinBox* gainSpin()         const { return m_gainSpin; }
    QDoubleSpinBox* eqQSpin()          const { return m_eqQSpin; }

    // Visible band-count choices.
    QRadioButton*   bands5Radio()      const { return m_bands5Radio; }
    QRadioButton*   bands10Radio()     const { return m_bands10Radio; }
    QRadioButton*   bands18Radio()     const { return m_bands18Radio; }

    // Advanced curve-range inputs.
    QSpinBox*       lowSpin()          const { return m_lowSpin; }
    QSpinBox*       highSpin()         const { return m_highSpin; }

    // Shared Q switch and Advanced presentation/live-update switches.
    QCheckBox*      useQFactorsChk()   const { return m_useQFactorsChk; }
    QCheckBox*      liveUpdateChk()    const { return m_liveUpdateChk; }
    QCheckBox*      logScaleChk()      const { return m_logScaleChk; }

    // Reset buttons + OG CFC Guide link.
    QPushButton*    resetCompBtn()     const { return m_resetCompBtn; }
    QPushButton*    resetEqBtn()       const { return m_resetEqBtn; }
    QPushButton*    ogGuideLink()      const { return m_ogGuideLink; }

    // Bar chart timer (test inspection only).
    QTimer*         barChartTimer()    const { return m_barChartTimer; }

    // Currently-loaded band count (5, 10, or 18) — derived from the
    // authoritative widget state. Defaults to 10 (matches Thetis radCFC_10.Checked=true
    // at frmCFCConfig.Designer.cs:474 [v2.10.3.13]).
    int             currentBandCount() const;

protected:
    void showEvent (QShowEvent*  event) override;
    void hideEvent (QHideEvent*  event) override;
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    // ── Band-count radios ────────────────────────────────────────────────
    // From Thetis frmCFCConfig.cs:108-118 [v2.10.3.13].
    void onBandCountChanged();

    // ── Freq-range spinboxes ─────────────────────────────────────────────
    // From Thetis frmCFCConfig.cs:120-140 [v2.10.3.13].
    void onLowFreqChanged(int hz);
    void onHighFreqChanged(int hz);

    // ── Compression / shared selected-band inputs (selected-band) ─────────────────────────────────────
    // From Thetis frmCFCConfig.cs:142-204 [v2.10.3.13].
    void onSelectedBandChanged(int oneBased);
    void onFreqSpinChanged(int hz);
    void onPrecompSpinChanged(double db);
    void onCompSpinChanged(double db);
    void onCompQSpinChanged(double q);

    // ── Post-EQ inputs ──────────────────────────────────────────────────
    // From Thetis frmCFCConfig.cs:169-194 [v2.10.3.13].
    void onPostEqGainSpinChanged(double db);
    void onGainSpinChanged(double db);
    void onEqQSpinChanged(double q);

    // ── Checkbox toggles ─────────────────────────────────────────────────
    // From Thetis frmCFCConfig.cs:484-490 + 603-607 [v2.10.3.13].
    void onUseQFactorsToggled(bool on);
    void onLogScaleToggled(bool on);

    // ── Reset buttons ────────────────────────────────────────────────────
    // From Thetis frmCFCConfig.cs:451-463 [v2.10.3.13].
    void onResetCompClicked();
    void onResetEqClicked();

    // ── OG CFC Guide link ────────────────────────────────────────────────
    // From Thetis frmCFCConfig.cs:598-601 [v2.10.3.13].
    void onOgGuideClicked();

    // ── 50ms bar chart timer slot ────────────────────────────────────────
    // From Thetis frmCFCConfig.cs:394-431 [v2.10.3.13] — timerTick.
    void onBarChartTick();

    // ── Model → UI sync (echo-guarded) ───────────────────────────────────
    void syncFromModel();

private:
    void buildUi();
    void wireSignals();
    void seedWidgetsFromTransmitModel();

    void updateSelectedRowEnable();
    void updateEditRowFromSelection(int index);
    void pushCfcProfileToModel();

    CfcEditProfile captureProfile() const;
    void restoreProfile(const CfcEditProfile& profile);
    QByteArray captureEditState() const;
    void restoreEditState(const QByteArray& state);
    void rebaseEditHistory();
    void beginEdit();
    void finishEdit();
    void changed(bool dragging = false);
    void syncPairedFrequencies(ParametricEqWidget* source, ParametricEqWidget* target);
    void refreshControls();
    void rebuildBandSelectors();
    void applyBandCount();
    void cancelBandCount();
    void resetCurve(ParametricEqWidget* widget);
    void editSelectedPoint(ParametricEqWidget* widget, double frequency, double gain, double q);
    void undoEdit();
    void redoEdit();

    // Selected-index helpers — Thetis-style, returns the index across both
    // widgets (frmCFCConfig.cs:307-315 [v2.10.3.13]).
    int  selectedIndex() const;

    // The bars for one CFC display reading (kCfcDisplayBinCount values):
    // the slice over the chart's frequency range, as Thetis's timerTick
    // takes it. Shared by the local and the remote chart.
    void drawCompressionBins(const double* bins, int count);

    QPointer<TransmitModel> m_tm;        // non-owning
    QPointer<TxChannel>     m_tx;        // non-owning, may be null pre-connect

    // ── Echo guards (mirror frmCFCConfig.cs:64-65 _ignore_udpates / _ignore_unselected) ──
    bool m_ignoreUpdates    = false;
    bool m_ignoreUnselected = false;
    bool m_updatingFromModel = false;

    EqEditHistory* m_history = nullptr;
    QHash<QByteArray, int> m_selectionStates;
    QByteArray m_committedEditState;
    bool m_liveGestureWrote = false;
    bool m_compUseQ = false;
    bool m_eqUseQ = false;
    bool m_gestureActive = false;
    bool m_sliderActive = false;
    QPointer<QAbstractSpinBox> m_numericEditor;
    quint64 m_numericEditGeneration = 0;
    QPushButton* m_undoBtn = nullptr;
    QPushButton* m_redoBtn = nullptr;
    QPushButton* m_applyBandsBtn = nullptr;
    QPushButton* m_cancelBandsBtn = nullptr;
    QWidget* m_countNotice = nullptr;
    QLabel* m_countMessage = nullptr;
    int m_pendingBandCount = 0;
    QSlider* m_compQSlider = nullptr;
    QSlider* m_eqQSlider = nullptr;
    QLabel* m_qGuidance = nullptr;
    QLabel* m_selectedSummary = nullptr;
    QLabel* m_invalidCurveGuidance = nullptr;
    bool m_curveAvailable = true;
    CfcEditProfile m_invalidLegacyProfile;
    QString m_invalidLegacyBlob;
    QPointer<QSlider> m_cancelledSlider;
    QHBoxLayout* m_bandSelectors = nullptr;

    // ── ParametricEqWidget instances ─────────────────────────────────────
    ParametricEqWidget* m_compWidget   = nullptr;  // ucCFC_comp
    ParametricEqWidget* m_postEqWidget = nullptr;  // ucCFC_eq

    // ── Compression / shared selected-band inputs ─────────────────────────────────────────────────────
    QSpinBox*       m_selectedBandSpin = nullptr;
    QSpinBox*       m_freqSpin         = nullptr;
    QDoubleSpinBox* m_precompSpin      = nullptr;
    QDoubleSpinBox* m_compSpin         = nullptr;
    QDoubleSpinBox* m_compQSpin        = nullptr;

    // ── Post-EQ inputs ──────────────────────────────────────────────────
    QDoubleSpinBox* m_postEqGainSpin   = nullptr;
    QDoubleSpinBox* m_gainSpin         = nullptr;
    QDoubleSpinBox* m_eqQSpin          = nullptr;

    // ── Count / Advanced controls ─────────────────────────────────────────────────────
    QButtonGroup*   m_bandCountGroup   = nullptr;
    QRadioButton*   m_bands5Radio      = nullptr;
    QRadioButton*   m_bands10Radio     = nullptr;
    QRadioButton*   m_bands18Radio     = nullptr;
    QSpinBox*       m_lowSpin          = nullptr;
    QSpinBox*       m_highSpin         = nullptr;
    QCheckBox*      m_useQFactorsChk   = nullptr;
    QCheckBox*      m_liveUpdateChk    = nullptr;
    QCheckBox*      m_logScaleChk      = nullptr;
    QPushButton*    m_resetCompBtn     = nullptr;
    QPushButton*    m_resetEqBtn       = nullptr;
    QPushButton*    m_ogGuideLink      = nullptr;

    // ── Bar chart timer + scratch buffer ─────────────────────────────────
    QTimer*         m_barChartTimer    = nullptr;
    QLabel*         m_settingsReasonLabel = nullptr;  // R-R3-49 (parity Task 4)
    bool            m_barChartBusy     = false;  // mirrors Thetis _busy
    // Parity Task 33: a remote window's bar chart source and its note.
    std::function<void(bool)> m_stationBarChart;
    QLabel*         m_barChartReasonLabel = nullptr;
    // Setup publication (CFC band editor): the remote command route.
    void sendPendingStationProfile();
    void clearStationProfileInFlight();
    void showProfileReason(const QString& reason);
    StationProfileAvailable m_stationProfileAvailable;
    StationProfileSender    m_stationProfileSend;
    quint32         m_profileCommandId = 0;   // 0: none out
    bool            m_profilePending   = false;
    QString         m_pendingProfileJson;
    QLabel*         m_profileReasonLabel = nullptr;

    // Scratch storage for a single tick of WDSP CFC display data.
    // Sized to TxChannel::kCfcDisplayBinCount (1025) lazily on first tick.
    // We don't allocate at construction to keep the dialog cheap to build.
};

} // namespace NereusSDR
