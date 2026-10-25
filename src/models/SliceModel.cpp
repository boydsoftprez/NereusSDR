// =================================================================
// src/models/SliceModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - setRfGain clamps to the AGC-T range -20..120 (Thetis
//                ptbRF, console.designer.cs:3708-3709 [v2.10.3.15]) instead
//                of 0..100, so TCI agc_gain and the AGC-top readback keep
//                both ends. After Longpath f1d0c181 (Martin Fischer,
//                OE5SOS). J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Claude Code.
//   2026-10-22 - rxAntennaLabelFor: refreshAntennasFromAlex's receive-label
//                rule on its own, so the Core can compare the label a slice
//                will have on a new band before it gets there. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22 - refreshAntennasFromAlex names RX-only 4 and 5 by the XVTR
//                label. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// 2026-10-14 - Capture per-band persistence values before deferred saves.
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 — J.J. Boyd (KG4VCF), gate experimental RADE V2 opt-in;
//                AI-assisted implementation via OpenAI Codex.
// 2026-10-11 - Settings restore: AGC-T AUTO and its offset are saved per
//              slice (per receiver), with the older per-band keys read as
//              a fallback. NereusSDR-original. J.J. Boyd (KG4VCF),
//              AI-assisted via Anthropic Claude Code.
// 2026-10-11 - Settings restore: saveToSettings and restoreFromSettings
//              carry the audio pan, BIN, squelch, APF, FM, DIG offset and
//              RTTY values, per slice. They were never saved.
//              NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//              Anthropic Claude Code.
// 2026-10-04 - Guard mode/filter notification continuations against deletion
//              and superseding commits, including a return to the same tuple.
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - Guard frequency notification continuations against deletion/reentry.
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-30 - RADE reason: applyRadeModeChange brackets each RADE
//                 decoder start (RadioModel::beginRadeStart, endRadeStart)
//                 so a create or start that fails gives the slice its
//                 radeReason; setRadeReason. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-30 - setDspMode's RADE decoder start and stop moved, unchanged,
//                 into applyRadeModeChange, which restoreFromSettings now
//                 runs too: a restored or band-changed RADE slice played its
//                 sideband audio with no decoder. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - radeSynced and radeFreqOffsetHz, the RADE decoder's sync
//                 and frequency offset; sync clears on leaving a RADE
//                 sideband. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 11 fix round 1:
//                                    applyMirroredValue's "active" rejection
//                                    now names the reachable
//                                    setActiveSliceById command verb instead
//                                    of the unreachable RadioModel::
//                                    setActiveSlice(). NereusSDR-original
//                                    addition, no Thetis change. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-40: runtime NNR limit
//                                    (setNnrLimit, requestNnrRetry, the
//                                    nnrLimit mirror field); choosing a
//                                    model while limited asks for a retry.
//                                    NereusSDR-original. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-44: setVaxChannelStore(); with a
//                                    store set, setVaxChannel() keeps the
//                                    channel there and writes no setting.
//                                    NereusSDR-original. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-45: setOutputRoute() and
//                                    restoreOutputRoute(), speakers or
//                                    headphones per receiver, persisted as
//                                    Slice<N>/OutputRoute. NereusSDR-original.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-45 Task 2: setOutputRoutePersisted(),
//                                    so a remote window leaves the route to
//                                    the Core. AI-assisted implementation via
//                                    Anthropic Claude Code. NereusSDR-original.
//   2026-09-24  J.J. Boyd / KG4VCF  R-IOS-01: activeWriteReason(), the
//                                    refusal of a write to `active` in plain
//                                    operator words. AI-assisted via
//                                    Anthropic Claude Code. NereusSDR-original.
//   2026-09-24 - iPhone app Task 4b (R-IOS-01, R-R3-21): the reasons this
//                file sends an app are in operator words. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Part A fix wave (R-IOS-01): the saved-settings
//                notice (nnrLastError) is in operator words; the setting
//                names go to the log. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - iPhone app follow-up (R-IOS-06): setAfGain clamps to
//                ControlRanges.h's AF range, which the Core's catalogue
//                reads too. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-26 - Remote-window parity Task 15 (R-R3-13, R-R3-49): the ADC
//                and AGC reading setters, applied from the mirror in a
//                remote window. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code. NereusSDR-original.
//   2026-09-26 - Remote-window parity Task 16 (R-R3-49): minNotchWidthHz,
//                applied from the mirror in a remote window. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//                NereusSDR-original.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13): setTxMarkAllowed
//                                    (ruling 5.4a). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-27 - R-R3-49: savedSampleRateHz, the rate saved for a band,
//                read at connect. NereusSDR-original. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Slice control and shared listening plan Task 5: the
//                read-only listener mark; every setter of a property a
//                remote window writes to the Core (MirrorPolicy
//                Bidirectional) but panKey, setFilter, applyNnrSettings
//                and restoreFromSettings hold a change back while it is
//                set.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-23: Added changeTuneStepUp/Down, ported from Thetis
//                 ChangeTuneStepUp/Down, by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-10-06 - restoreFromSettings: a mode handler that changes the
//                mode or filter mid-restore skips only the saved filter;
//                AGC, step, NB, sample rate and diversity still restore.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-10-15 - Squelch is saved: saveToSettings and restoreFromSettings
//                carry the SSQL, AM and FM squelch enables and thresholds
//                per slice, from Thetis console.cs:2830-2852 and 4164-4193
//                [v2.10.3.15]. A restored threshold is clamped to its
//                ControlRanges range; one that is not a number is skipped.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22 - setMeterReadingNs, the RX meter readings' measurement time
//                (rxMeterStampsVersion 1). NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22 - Filter presets match Thetis InitFilterPresets
//                (console.cs:5188-5582 [v2.10.3.15]): the LSB, USB, DIGL,
//                DIGU, CWL, CWU, AM, SAM and DSB F1-F10 values, the DIGU and
//                DIGL F5 default (+/-500, was +/-600), and the CW slots'
//                image-limit slide from the CWPitch setter
//                (console.cs:18212-18266 [v2.10.3.15]). Longpath PRs 139 and
//                140 (oe5sos/Longpath @383f058) used as a cross-check. J.J.
//                Boyd (KG4VCF), with AI-assisted transformation via Claude
//                Code.
//   2026-10-24 - defaultFilterForMode: DIGU/DIGL start on F1 3.0k (JJ's
//                ruling, for FT8), NereusSDR's own default; Thetis starts
//                them on F5 1.0k. The F1-F10 presets stay as Thetis has
//                them. By J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
//=================================================================
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

#include <QPointer>
#include "SliceModel.h"

#include "Band.h"
#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/DiversityPattern.h"
#include "core/LogCategories.h"
#include "core/RadeChannel.h"
#include "core/RadeV2Feature.h"
#include "core/WdspEngine.h"
#include "core/accessories/AlexController.h"
#include "core/session/MirrorEnumDomain.h"
#include "core/SkuUiProfile.h"  // issue #257 — rxOnlyLabels lookup in refreshAntennasFromAlex
#include "models/RadioModel.h"

#include <QFile>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

SliceModel::SliceModel(QObject* parent)
    : QObject(parent)
{
    setupRadeIdleClearTimer();
}

SliceModel::SliceModel(int sliceId, QObject* parent)
    : QObject(parent)
    , m_sliceIndex(sliceId)
{
    setupRadeIdleClearTimer();
}

// 2026-05-12 bench: RADE idle-clear timer setup.
//
// One-time construction during SliceModel ctor.  Reads
// RadeIdleClearMs from AppSettings (default 20 s, clamp 5..120 s).
// Single-shot QTimer; expiry lambda clears callsign + SNR back to
// the "no decode yet" sentinels.  Timer is parented to `this` so
// destruction is automatic; safe to leave running across mode swaps
// because the expiry only clears state that's already cleared in
// those paths.
void SliceModel::setupRadeIdleClearTimer()
{
    auto& s = AppSettings::instance();
    int ms = s.value(QStringLiteral("RadeIdleClearMs"), 20000).toInt();
    if (ms < 5000)   { ms = 5000;   }
    if (ms > 120000) { ms = 120000; }
    m_radeIdleClearMs = ms;

    m_radeIdleClearTimer = new QTimer(this);
    m_radeIdleClearTimer->setSingleShot(true);
    m_radeIdleClearTimer->setInterval(m_radeIdleClearMs);
    connect(m_radeIdleClearTimer, &QTimer::timeout, this, [this]() {
        // Clear callsign back to empty.  Renderer (VfoWidget) falls
        // back to the "RADE" literal prefix when callsign is empty,
        // so no "NaN" or empty cell on screen.
        if (!m_lastRadeRxCallsign.isEmpty()) {
            m_lastRadeRxCallsign.clear();
            emit lastRadeRxCallsignChanged(m_lastRadeRxCallsign);
        }
        // Clear SNR to NaN.  VfoWidget renders NaN as "---" (not
        // the literal string "NaN") in the SNR column.
        if (!qIsNaN(m_snrDb)) {
            m_snrDb = std::numeric_limits<double>::quiet_NaN();
            emit snrDbChanged(m_snrDb);
        }
    });
}

// Restart the idle timer when fresh activity arrives.  Only runs the
// timer while in a RADE sideband -- SSB / WSJT-X paths don't care
// about RADE idle and would burn cycles on an irrelevant timer.
void SliceModel::restartRadeIdleClearTimer()
{
    if (!m_radeIdleClearTimer) { return; }
    if (!isRadeDspMode(m_dspMode)) {
        return;
    }
    m_radeIdleClearTimer->start();  // restarts even if already running
}

SliceModel::~SliceModel() = default;

// ---------------------------------------------------------------------------
// Frequency
// ---------------------------------------------------------------------------

void SliceModel::setFrequency(double freq)
{
    if (holdsListenerWrite(frequency(), freq)) { return; }
    // 3G-10 S2.9: client-side lock guard. When locked, setFrequency is a
    // no-op — prevents accidental tuning. The hardware VFO is not changed.
    if (m_locked) { return; }
    applyFrequency(freq);
}

bool SliceModel::applyStationFrequency(double freq)
{
    const auto* radio = qobject_cast<const RadioModel*>(parent());
    if (!radio || radio->role() != RadioModel::Role::Remote || !std::isfinite(freq)) {
        return false;
    }
    applyFrequency(freq);
    return true;
}

void SliceModel::setReadOnlyListener(bool readOnly, const QString& reason)
{
    // Slice control plan Task 5: set by the remote window's
    // SliceAccessMirror only; a Local model never listens to a slice.
    const QString words = readOnly ? reason : QString();
    const bool changed = m_readOnlyListener != readOnly;
    m_readOnlyListener = readOnly;
    m_readOnlyListenerReason = words;
    if (changed) {
        emit readOnlyListenerChanged(readOnly);
    }
}

bool SliceModel::holdForListener()
{
    // The Core's own state applied to a listened slice is never held; only
    // a change this window would send is.
    if (!m_readOnlyListener || (m_stationApplyProbe && m_stationApplyProbe())) {
        return false;
    }
    emit listenerWriteHeld(m_readOnlyListenerReason);
    return true;
}

void SliceModel::applyFrequency(double freq)
{
    if (!qFuzzyCompare(m_frequency, freq)) {
        const QPointer<SliceModel> self(this);
        m_frequency = freq;
        emit frequencyChanged(freq);
        // CAT/model callbacks may retire this slice or supersede the frequency.
        if (!self || !qFuzzyCompare(m_frequency, freq)) { return; }
        noteDiversityPatternInputs();
        if (!self || !qFuzzyCompare(m_frequency, freq)) { return; }

        // Phase 3P-II Task 64: emit bandChanged on band boundary cross.
        // Uses Band::bandFromFrequency (IARU Region 2, GEN fallback).
        // Guarded so the signal fires at most once per distinct Band change.
        Band newBand = bandFromFrequency(freq);
        if (newBand != m_currentBand) {
            m_currentBand = newBand;
            emit bandChanged(newBand);
        }
    }
}

// ---------------------------------------------------------------------------
// Demodulation mode
// ---------------------------------------------------------------------------

void SliceModel::applyRadeModeChange(DSPMode oldMode, DSPMode newMode)
{
    const QPointer<SliceModel> lifetime(this);
    const quint64 modeRevision = m_modeRevision;
    const auto currentMode = [lifetime,modeRevision] {
        return lifetime && lifetime->m_modeRevision == modeRevision;
    };
    // ── Phase 3R J3 + K-bench: RADE channel-additive lifecycle ────────────
    //
    // RADE_U / RADE_L are NereusSDR-native DSPModes (J1).  Original J3
    // design destroyed the WDSP RxChannel and replaced it with a
    // RadeChannel on entry into RADE.  K-bench reframed the RX pipeline
    // (RxDspWorker.cpp:160-191) so RADE is now ADDITIVE rather than
    // replacement:
    //   - WDSP RxChannel stays alive in EVERY mode.  WDSP serves as
    //     the SSB demod front-end and produces decoded audio that
    //     feeds the S-meter / spectrum / AGC every tick.
    //   - RadeChannel is created ALONGSIDE RxChannel in RADE_U /
    //     RADE_L, consumes WDSP's decoded audio (downsampled to
    //     24 kHz), and owns the speaker path while active.
    //   - The WDSP-facing mode is mapped at the RxChannel boundary
    //     (RxChannel::wdspModeFor): RADE_U -> USB, RADE_L -> LSB.
    //     Without that mapping, raw enum 12/13 would land in WDSP's
    //     mode enum (review finding 2026-05-12, PR #238).
    //
    // So the swap logic below is now only about RadeChannel
    // create/destroy.  RxChannel is created once at connect time
    // (RadioModel) and stays alive.
    //
    // RADE_U <-> RADE_L is still a destroy-and-recreate of RadeChannel
    // because the sideband flag is set on construction; the RxChannel
    // is untouched (RxChannel::setMode below will retune USB <-> LSB
    // via the wdspModeFor mapping when it fires from the
    // dspModeChanged signal).
    //
    // Reach the WdspEngine via the parent RadioModel rather than holding
    // a direct pointer on SliceModel; this keeps the construction graph
    // unchanged (slices are parented to RadioModel; see RadioModel.cpp:
    // 1374 [Phase 3R J3] new SliceModel(this)).
    const auto isRade = [](DSPMode m) {
        return isRadeDspMode(m);
    };

    // 2026-05-12 bench: clear last RADE-decoded speaker callsign
    // when leaving the *current* RADE sideband.  Two cases now
    // covered (refined from 2026-05-11 design which kept the
    // callsign sticky on U <-> L swap):
    //   1. RADE -> non-RADE: leaving RADE entirely.
    //   2. RADE_U <-> RADE_L: still in RADE, but the channel is
    //      destroyed and recreated below so the decoder state is
    //      no longer associated with the old caller's transmission.
    // Trigger: oldMode was a RADE sideband AND mode actually changed
    // (we're already inside the modeChanged guard).
    // An early callback can commit a non-RADE mode before the old decoder
    // retires. A nested setter must still complete that actual lifecycle.
    auto* radio = qobject_cast<RadioModel*>(parent());
    const bool retainedRade = radio != nullptr && radio->role() != RadioModel::Role::Remote
        && !m_radeStartDeferredToAdmission && radio->wdspEngine() != nullptr
        && radio->wdspEngine()->radeChannel(m_sliceIndex) != nullptr;
    const bool oldRadeState = isRade(oldMode) || retainedRade;
    if (oldRadeState && !m_lastRadeRxCallsign.isEmpty()) {
        m_lastRadeRxCallsign.clear();
        const QString callsign = m_lastRadeRxCallsign;
        emit lastRadeRxCallsignChanged(callsign);
        if (!currentMode()) { return; }
    }
    // The same two cases end the old decoder (it is destroyed below),
    // and the VFO flag drops its sync dot (VfoWidget::setRadeActive):
    // the next decoder reports its own sync.
    if (oldRadeState) {
        setRadeSynced(false);
        if (!currentMode()) { return; }
    }

    // 2026-05-12 bench: stop the idle-clear timer when leaving
    // RADE.  The clear above already happened; letting the timer
    // fire would just re-emit lastRadeRxCallsignChanged("") and
    // snrDbChanged(NaN) needlessly.  Also stop on RADE_U <-> RADE_L
    // swaps for the same reason.
    if (oldRadeState && m_radeIdleClearTimer) {
        m_radeIdleClearTimer->stop();
    }

    // Remote-daemon R2 Task 5: the only model-to-engine reach-through
    // in src/models outside RadioModel itself. Gate BEFORE any
    // channel creation, not merely before the resulting emit --
    // WdspEngine::createRadeChannel carries no isInitialized guard,
    // so a mirrored RADE mode delta on a Role::Remote model (whose
    // WdspEngine is constructed but never initialize()'d) would
    // otherwise construct and start() a live RadeChannel -- a real
    // vocoder -- on a machine with no DSP role at all. See design
    // addendum docs/architecture/2026-08-03-remote-daemon-r2-r3-
    // design-addendum.md section 4.1.
    if (radio != nullptr && radio->role() != RadioModel::Role::Remote) {
        WdspEngine* engine = radio->wdspEngine();
        // restoreReceiveState's offline seam starts its RADE decoder at
        // admission instead (m_radeStartDeferredToAdmission).
        if (engine != nullptr && !m_radeStartDeferredToAdmission) {
            const int channelId = m_sliceIndex;
            // Reconcile the owned decoder, rather than an intermediate logical
            // old mode left by a superseded early notification continuation.
            const bool oldIsRade = engine->radeChannel(channelId) != nullptr;
            const bool newIsRade = isRade(newMode)
                && (!isRade2DspMode(newMode) || radio->radeV2Enabled());

            // RADE reason (2026-09-30): each start below is bracketed
            // (RadioModel::beginRadeStart, before any destroy, and
            // endRadeStart with how it ended), so the slice's radeReason
            // says why when it has no working decoder.
            auto wireAndStartRade = [&](RadeChannel* radeCh,
                                        const char* context) {
                if (radeCh == nullptr) {
                    radio->endRadeStart(channelId,
                                        RadioModel::RadeStartFault::CreateFailed);
                    return;
                }
                radeCh->setVersion(isRade2DspMode(newMode)
                    ? RadeChannel::Version::V2 : RadeChannel::Version::V1);
                radeCh->setSideband(isRadeUpperDspMode(newMode));
                radio->wireRadeChannel(channelId, radeCh, this);
                if (!currentMode()) { return; }
                const QString modelPath = radeModelPath();
                const bool started = radeCh->start(modelPath);
                if (!currentMode()) { return; }
                if (!started) {
                    qCWarning(lcDsp)
                        << "SliceModel" << m_sliceIndex
                        << context
                        << ": RadeChannel.start() failed for"
                        << modelPath
                        << "- channel-swap proceeds but RADE will"
                           " not decode";
                    radio->endRadeStart(channelId,
                                        RadioModel::RadeStartFault::StartFailed);
                    return;
                }
                radio->endRadeStart(channelId, RadioModel::RadeStartFault::None);
            };

            if (oldIsRade && !newIsRade) {
                // RADE -> any WDSP mode: tear down the RadeChannel
                // only.  K-bench: WDSP RxChannel was running the
                // whole time as the demod front-end; leave it
                // alone.  WDSP-facing mode will retune from
                // USB/LSB (the wdspModeFor mapping) to the new
                // mode via the dspModeChanged -> rxCh->setMode
                // path in RadioModel.cpp:5202-5206.
                engine->destroyRadeChannel(channelId);
            } else if (!oldIsRade && newIsRade) {
                // Any WDSP mode -> RADE: create RadeChannel
                // alongside the still-running RxChannel.  Wire
                // its signals into RadioModel's per-slice slot
                // graph and start it with the configured model
                // path.  WDSP-facing mode will map to USB/LSB
                // via the dspModeChanged path.
                radio->beginRadeStart(channelId);
                wireAndStartRade(engine->createRadeChannel(channelId),
                                 "setDspMode(RADE)");
            } else if (oldIsRade && newIsRade) {
                // RADE_U <-> RADE_L: destroy + recreate the
                // RadeChannel so the sideband flag is set fresh
                // on a clean instance.  RxChannel is untouched;
                // the dspModeChanged path retunes it USB <-> LSB
                // through wdspModeFor.
                radio->beginRadeStart(channelId);
                engine->destroyRadeChannel(channelId);
                wireAndStartRade(engine->createRadeChannel(channelId),
                                 "setDspMode(RADE U<->L)");
            }
        }
    }
    if (!currentMode()) { return; }
    if (radio && isRade2DspMode(newMode) && !radio->radeV2Enabled()) {
        radio->beginRadeStart(m_sliceIndex);
        radio->endRadeStart(m_sliceIndex, RadioModel::RadeStartFault::None);
    }
}

void SliceModel::setDspMode(DSPMode mode)
{
    if (isRade2DspMode(mode) && !(m_stationApplyProbe && m_stationApplyProbe())) {
        const auto* radio = qobject_cast<RadioModel*>(parent());
        if (!(radio ? radio->radeV2Enabled() : RadeV2Feature::enabled())) {
            qCWarning(lcDsp) << "Experimental RADE V2 mode is unavailable on the Core";
            return;
        }
    }
    if (holdsListenerWrite(dspMode(), mode)) { return; }
    const bool modeChanged = (m_dspMode != mode);
    const DSPMode oldMode = m_dspMode;
    m_dspMode = mode;
    if (modeChanged) { ++m_modeRevision; ++m_modeFilterRevision; }
    const quint64 modeRevision = m_modeRevision;
    const quint64 filterRevision = m_modeFilterRevision;
    const QPointer<SliceModel> lifetime(this);
    const auto currentMode = [lifetime,modeRevision] {
        return lifetime && lifetime->m_modeRevision == modeRevision;
    };
    const auto notifyMode = [&] { if (modeChanged) { emit dspModeChanged(mode); } };

    if (modeChanged) {
        // RADE threads (2026-09-30): the RADE start and stop below lives in
        // applyRadeModeChange, which restoreFromSettings runs too.
        applyRadeModeChange(oldMode, mode);
        if (!currentMode()) { return; }
        // A newer filter survives, but the committed mode still needs its notification.
        if (filterRevision != m_modeFilterRevision) { notifyMode(); return; }
    }

    // Phase 3J-1 closeout Item 4 (2026-05-12): per-(band, mode) LastFilter.
    //
    // Before the mode swap, save the CURRENT filter under (currentBand,
    // OLD mode) so coming back to that mode in this band restores the
    // operator's last-set cutoffs.  Then look up the saved filter for the
    // (currentBand, NEW mode) tuple; if persisted, use it; if absent, fall
    // back to defaultFilterForMode (Thetis F5 presets; DIGU/DIGL start on
    // F1, NereusSDR's own default).  Mirrors
    // Thetis preset[m].LastFilter (console.cs:14653-14671 [v2.10.3.13]).
    //
    // Band is computed from the slice's current frequency rather than read
    // off PanadapterModel, so SliceModel stays decoupled from the
    // panadapter (no signal subscription needed).  bandFromFrequency
    // returns the same Band PanadapterModel uses, so the keyspace is
    // shared between the panadapter band-crossing save path (RadioModel
    // signal handler) and this mode-change save path.
    // bandModePrefix() lives in the anonymous namespace later in this
    // file; reach it via a forward-declared helper to keep the source
    // order stable.  Same with AppSettings::save -- we don't call it
    // here because writes are flushed on shutdown / band-change /
    // explicit caller save; mode-change writes are best-effort and
    // shouldn't block on disk I/O.
    auto& s = AppSettings::instance();
    int low = 0, high = 0;
    if (modeChanged) {
        const Band currentBand = bandFromFrequency(m_frequency);
        // 1. Save current filter under (currentBand, OLD mode)
        const QString oldPrefix =
            QStringLiteral("Slice%1/Band%2/Mode%3/")
                .arg(m_sliceIndex)
                .arg(bandKeyName(currentBand))
                .arg(SliceModel::modeName(oldMode));
        s.setValue(oldPrefix + QStringLiteral("FilterLow"),  m_filterLow);
        if (!currentMode()) { return; }
        if (filterRevision != m_modeFilterRevision) { notifyMode(); return; }
        s.setValue(oldPrefix + QStringLiteral("FilterHigh"), m_filterHigh);
        if (!currentMode()) { return; }
        if (filterRevision != m_modeFilterRevision) { notifyMode(); return; }

        // 2. Restore filter for (currentBand, NEW mode); fall back to default.
        const QString newPrefix =
            QStringLiteral("Slice%1/Band%2/Mode%3/")
                .arg(m_sliceIndex)
                .arg(bandKeyName(currentBand))
                .arg(SliceModel::modeName(mode));
        if (s.contains(newPrefix + QStringLiteral("FilterLow")) &&
            s.contains(newPrefix + QStringLiteral("FilterHigh"))) {
            low  = s.value(newPrefix + QStringLiteral("FilterLow")).toInt();
            high = s.value(newPrefix + QStringLiteral("FilterHigh")).toInt();
        } else {
            // From Thetis console.cs:5180-5575 — InitFilterPresets, F5 per mode
            auto pair = defaultFilterForMode(mode);
            low  = pair.first;
            high = pair.second;
        }
    } else {
        // Mode didn't actually change -- preserve current cutoffs.
        low  = m_filterLow;
        high = m_filterHigh;
    }
    bool filterChanged = (m_filterLow != low || m_filterHigh != high);
    m_filterLow = low;
    m_filterHigh = high;
    if (filterChanged) { ++m_modeFilterRevision; }
    const quint64 revision = m_modeFilterRevision;

    if (modeChanged) {
        emit dspModeChanged(mode);
        // A callback may destroy the slice or commit newer mode/filter state.
        // A revision also detects a nested change away and back to this tuple.
        if (!lifetime || revision != m_modeFilterRevision) { return; }
    }
    if (filterChanged) {
        emit this->filterChanged(m_filterLow, m_filterHigh);
    }
}

// Phase 3R Task J3 - see SliceModel.h declaration for the design rationale.
QString SliceModel::radeModelPath() const
{
    auto& s = AppSettings::instance();
    const QString configured =
        s.value(QStringLiteral("Rade/ModelPath"), QString()).toString();
    if (!configured.isEmpty() && QFile::exists(configured)) {
        return configured;
    }
    // From AetherSDR RADEEngine.cpp:34 [@0cd4559] - librade convention
    // "use the built-in weights, ignore the model_file argument".
    return QStringLiteral("dummy");
}

// ---------------------------------------------------------------------------
// Bandpass filter
// ---------------------------------------------------------------------------

void SliceModel::setFilterLow(int low)
{
    if (holdsListenerWrite(filterLow(), low)) { return; }
    if (m_filterLow != low) {
        m_filterLow = low;
        ++m_modeFilterRevision;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

void SliceModel::setFilterHigh(int high)
{
    if (holdsListenerWrite(filterHigh(), high)) { return; }
    if (m_filterHigh != high) {
        m_filterHigh = high;
        ++m_modeFilterRevision;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

void SliceModel::setFilter(int low, int high)
{
    if (holdsListenerWrite(std::pair(m_filterLow, m_filterHigh), std::pair(low, high))) { return; }
    if (m_filterLow != low || m_filterHigh != high) {
        m_filterLow = low;
        m_filterHigh = high;
        ++m_modeFilterRevision;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

// ---------------------------------------------------------------------------
// AGC
// ---------------------------------------------------------------------------

void SliceModel::setAgcMode(AGCMode mode)
{
    if (holdsListenerWrite(agcMode(), mode)) { return; }
    if (m_agcMode != mode) {
        m_agcMode = mode;
        emit agcModeChanged(mode);
    }
}

// ---------------------------------------------------------------------------
// Tuning step
// ---------------------------------------------------------------------------

void SliceModel::setStepHz(int hz)
{
    if (holdsListenerWrite(stepHz(), hz)) { return; }
    if (m_stepHz != hz && hz > 0) {
        m_stepHz = hz;
        emit stepHzChanged(hz);
    }
}

// From Thetis console.cs:6124-6128 [v2.10.3.15]: ChangeTuneStepUp.
void SliceModel::changeTuneStepUp()
{
    //MW0LGE_21j
    const int index = tuneStepIndexForHz(m_stepHz);
    if (index >= 0) {
        // TuneStepIndex = (tune_step_index + 1) % tune_step_list.Count;
        setStepHz(kTuneStepList[(index + 1) % kTuneStepListSize].stepHz);
        return;
    }

    // NereusSDR-native: Thetis holds an index, so it has no off-list case.
    // An off-list Hz value (persisted or hand-edited) moves to the smallest
    // entry above it, wrapping to the first entry when none is larger.
    for (int i = 0; i < kTuneStepListSize; ++i) {
        if (kTuneStepList[i].stepHz > m_stepHz) {
            setStepHz(kTuneStepList[i].stepHz);
            return;
        }
    }
    setStepHz(kTuneStepList[0].stepHz);
}

// From Thetis console.cs:6130-6134 [v2.10.3.15]: ChangeTuneStepDown.
void SliceModel::changeTuneStepDown()
{
    //MW0LGE_21j
    const int index = tuneStepIndexForHz(m_stepHz);
    if (index >= 0) {
        // TuneStepIndex = (tune_step_index - 1 + tune_step_list.Count) % tune_step_list.Count;
        setStepHz(kTuneStepList[(index - 1 + kTuneStepListSize) % kTuneStepListSize].stepHz);
        return;
    }

    // NereusSDR-native: Thetis holds an index, so it has no off-list case.
    // An off-list Hz value (persisted or hand-edited) moves to the largest
    // entry below it, wrapping to the last entry when none is smaller.
    for (int i = kTuneStepListSize - 1; i >= 0; --i) {
        if (kTuneStepList[i].stepHz < m_stepHz) {
            setStepHz(kTuneStepList[i].stepHz);
            return;
        }
    }
    setStepHz(kTuneStepList[kTuneStepListSize - 1].stepHz);
}

// ---------------------------------------------------------------------------
// Gains
// ---------------------------------------------------------------------------

void SliceModel::setAfGain(int gain)
{
    if (holdsListenerWrite(afGain(), gain)) { return; }
    gain = std::clamp(gain, ControlRanges::kAfGainMin, ControlRanges::kAfGainMax);
    if (m_afGain != gain) {
        m_afGain = gain;
        emit afGainChanged(gain);
    }
}

void SliceModel::setRfGain(int gain)
{
    if (holdsListenerWrite(rfGain(), gain)) { return; }
    // rfGain is the AGC-T, the WDSP AGC top (RadioModel hands it to
    // RxChannel::setAgcTop). From Thetis console.designer.cs:3708-3709
    // [v2.10.3.15]: ptbRF.Maximum = 120, ptbRF.Minimum = -20, the range
    // TCIServer.cs:5024 [v2.10.3.15] clamps agc_gain to and that
    // RxChannel::readBackAgcTop applies. The 0..100 clamp here before
    // 2026-10-22 was not from Thetis and folded both ends.
    constexpr int kAgcTopMinDb = -20;
    constexpr int kAgcTopMaxDb = 120;
    gain = std::clamp(gain, kAgcTopMinDb, kAgcTopMaxDb);
    if (m_rfGain != gain) {
        m_rfGain = gain;
        emit rfGainChanged(gain);
    }
}

// ---------------------------------------------------------------------------
// Antenna selection
// ---------------------------------------------------------------------------

void SliceModel::setRxAntenna(const QString& ant)
{
    if (holdsListenerWrite(rxAntenna(), ant)) { return; }
    if (m_rxAntenna != ant) {
        m_rxAntenna = ant;
        emit rxAntennaChanged(ant);
    }
}

void SliceModel::setTxAntenna(const QString& ant)
{
    if (holdsListenerWrite(txAntenna(), ant)) { return; }
    if (m_txAntenna != ant) {
        m_txAntenna = ant;
        emit txAntennaChanged(ant);
    }
}

namespace {
// ANT1..ANT3 by number, as the Alex grid names them.
QString antennaName(int n)
{
    switch (n) {
        case 2:  return QStringLiteral("ANT2");
        case 3:  return QStringLiteral("ANT3");
        default: return QStringLiteral("ANT1");
    }
}
} // namespace

// Phase 3P-I-a T13 — reverse sync: AlexController write → slice cache refresh.
// Reads the current per-band RX and TX antenna values from AlexController
// and refreshes the slice's cached m_rxAntenna / m_txAntenna via the public
// setters so rxAntennaChanged / txAntennaChanged signals fire to VFO Flag
// and RxApplet. The write-back to AlexController via T12's RadioModel handler
// is idempotent — AlexController::setRxAnt/setTxAnt returns early (without
// emitting antennaChanged) when the stored value equals the new value
// (AlexController.cpp:95,107), so no signal loop occurs.
//
// Issue #257: when AlexController::rxOnlyAnt(band) != 0 the radio is
// actually routing through the rx-only mux (EXT1 / EXT2 / BYPS / XVTR
// depending on SKU). The cached m_rxAntenna label must reflect that or
// the user sees "ANT1" while the radio is on EXT1 — and the next pick of
// "ANT1" in the popup gets no-op'd by setRxAntenna's equality guard,
// trapping the user on the bypass path. With the SkuUiProfile in hand
// we resolve rxOnlyAnt (1..3) to the per-SKU label trio (BYPS/EXT1/XVTR
// for ANAN-7000D, EXT2/EXT1/XVTR for ANAN-100D, etc). rxAntennaLabelFor is
// the receive half of that rule on its own.
QString SliceModel::rxAntennaLabelFor(const AlexController& alex,
                                      Band band,
                                      const SkuUiProfile* sku)
{
    const int rxOnly = alex.rxOnlyAnt(band);  // 0=none, 1..5 (checkbox sum)
    const int rx     = alex.rxAnt(band);      // 1..3

    // Issue #257: prefer the SKU-specific RX-only label when the bypass mux
    // is engaged. Fall back to the ANT* label when sku is null, when the
    // mux is disengaged (rxOnly == 0), or when the indexed slot in the SKU
    // is empty (defensive — should never happen because rxOnlyLabels is
    // always-3 in SkuUiProfile.h).
    // 4 and 5 (XVTR with an input, Thetis's checkbox sum) are named by the
    // XVTR box, as Thetis names the winning box (rxOnlyLabelSlot).
    const int rxOnlySlot = AlexController::rxOnlyLabelSlot(rxOnly);
    if (sku && rxOnlySlot >= 0) {
        const QString& slot = sku->rxOnlyLabels[static_cast<size_t>(rxOnlySlot)];
        return slot.isEmpty() ? antennaName(rx) : slot;
    }
    return antennaName(rx);
}

void SliceModel::refreshAntennasFromAlex(const AlexController& alex,
                                         Band band,
                                         const SkuUiProfile* sku)
{
    const int tx = alex.txAnt(band);  // 1..3
    const QString rxLabel = rxAntennaLabelFor(alex, band, sku);

    // Use the public setters so rxAntennaChanged / txAntennaChanged
    // signals fire — VFO Flag and RxApplet listen. The loop back to
    // AlexController via T12's handler is idempotent: AlexController's
    // setRxAnt/setTxAnt returns early on equal value, so no signal is
    // emitted.
    setRxAntenna(rxLabel);
    setTxAntenna(antennaName(tx));
}

// ---------------------------------------------------------------------------
// Slice state
// ---------------------------------------------------------------------------

void SliceModel::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        emit activeChanged(active);
    }
}

void SliceModel::setTxSlice(bool tx)
{
    if (m_txSlice != tx) {
        m_txSlice = tx;
        emit txSliceChanged(tx);
    }
}

void SliceModel::setTxMarkAllowed(bool allowed)
{
    if (m_txMarkAllowed == allowed) {
        return;
    }
    const bool before = txSliceMarked();
    m_txMarkAllowed = allowed;
    if (txSliceMarked() != before) {
        // The binding is unchanged; the link's mark moved (ruling 5.4a).
        emit txSliceChanged(m_txSlice);
    }
}

// Remote Daemon R2 Task 12: per-slice S-meter reading. Plain public
// setter (not a Q_PROPERTY WRITE accessor) -- see SliceModel.h's comment
// on setSignalStrengthDbm() for who calls this and why. qFuzzyIsNull on
// the difference rather than qFuzzyCompare: the same "subtraction to
// zero" pattern used elsewhere in this file (setShiftOffsetHz,
// setAudioPan, setSsqlThresh, ...) for a value that can legitimately be
// exactly 0.0 (qFuzzyCompare is documented UB when either argument is
// zero).
void SliceModel::setSignalStrengthDbm(double dbm)
{
    if (qFuzzyIsNull(m_signalStrengthDbm - dbm)) {
        return;
    }
    m_signalStrengthDbm = dbm;
    emit signalStrengthDbmChanged(dbm);
}

void SliceModel::setSignalPeakDbm(double dbm)
{
    if (qFuzzyIsNull(m_signalPeakDbm - dbm)) {
        return;
    }
    m_signalPeakDbm = dbm;
    emit signalPeakDbmChanged(dbm);
}

void SliceModel::setSignalAverageDbm(double dbm)
{
    if (qFuzzyIsNull(m_signalAverageDbm - dbm)) {
        return;
    }
    m_signalAverageDbm = dbm;
    emit signalAverageDbmChanged(dbm);
}

// Parity Task 15: the ADC and AGC readings, the same change-only shape as
// the S-meter readings above.
void SliceModel::setAdcPeakDbfs(double dbfs)
{
    if (qFuzzyIsNull(m_adcPeakDbfs - dbfs)) {
        return;
    }
    m_adcPeakDbfs = dbfs;
    emit adcPeakDbfsChanged(dbfs);
}

void SliceModel::setAdcAverageDbfs(double dbfs)
{
    if (qFuzzyIsNull(m_adcAverageDbfs - dbfs)) {
        return;
    }
    m_adcAverageDbfs = dbfs;
    emit adcAverageDbfsChanged(dbfs);
}

void SliceModel::setAgcGainDb(double db)
{
    if (qFuzzyIsNull(m_agcGainDb - db)) {
        return;
    }
    m_agcGainDb = db;
    emit agcGainDbChanged(db);
}

void SliceModel::setAgcPeakDb(double db)
{
    if (qFuzzyIsNull(m_agcPeakDb - db)) {
        return;
    }
    m_agcPeakDb = db;
    emit agcPeakDbChanged(db);
}

void SliceModel::setAgcAverageDb(double db)
{
    if (qFuzzyIsNull(m_agcAverageDb - db)) {
        return;
    }
    m_agcAverageDb = db;
    emit agcAverageDbChanged(db);
}

// Parity Task 16: the same change-only shape.
void SliceModel::setMinNotchWidthHz(double hz)
{
    if (qFuzzyIsNull(m_minNotchWidthHz - hz)) {
        return;
    }
    m_minNotchWidthHz = hz;
    emit minNotchWidthHzChanged(hz);
}

void SliceModel::setStationAutoAgcNoiseFloor(double dbm, bool valid, quint64 generation)
{
    if (!std::isfinite(dbm)) { return; }
    if (m_stationAutoAgcNoiseFloorDbm == dbm
        && m_stationAutoAgcNoiseFloorValid == valid
        && m_stationAutoAgcNoiseFloorGeneration == generation) {
        return;
    }
    m_stationAutoAgcNoiseFloorDbm = dbm;
    m_stationAutoAgcNoiseFloorValid = valid;
    m_stationAutoAgcNoiseFloorGeneration = generation;
    emit stationAutoAgcNoiseFloorChanged();
}

// ── Remote Daemon R2 Task 8: inbound mirror hook ─────────────────────────────
//
// NereusSDR-original; no Thetis/AetherSDR equivalent. Each of the no-WRITE
// properties that gets REFUSED is refused for a DIFFERENT reason, named
// explicitly rather than sharing one copy-pasted message, because a remote
// peer (or whoever is reading Task 11's relayed error) needs to know WHICH
// owner to go through instead. signalStrengthDbm (Task 12) is the one
// exception: there is no other owner to name, because on a Role::Remote
// model the mirror's inbound apply IS the value's sole legitimate writer.
QString SliceModel::activeWriteReason()
{
    return QStringLiteral("To make a slice active, select it; this cannot be set directly.");
}

QString SliceModel::applyMirroredValue(const QByteArray& propertyName, const QVariant& value)
{
    if (propertyName == "signalStrengthDbm") {
        // The only case here that ACCEPTS and applies the value instead of
        // refusing it. `value` arrives already decoded to a native double
        // by MirrorSchema::decode (see StateMirror::applyInboundToProperty
        // -- the hook receives a properly narrowed, natively typed value,
        // the same as a normal WRITE would). Empty return means accepted.
        setSignalStrengthDbm(value.toDouble());
        return QString();
    }
    if (propertyName == "signalPeakDbm") {
        setSignalPeakDbm(value.toDouble());
        return QString();
    }
    if (propertyName == "signalAverageDbm") {
        setSignalAverageDbm(value.toDouble());
        return QString();
    }
    // Parity Task 15: the Core's ADC and AGC readings, applied as the
    // S-meter readings are (the mirror is their only writer here).
    if (propertyName == "adcPeakDbfs") {
        setAdcPeakDbfs(value.toDouble());
        return QString();
    }
    if (propertyName == "adcAverageDbfs") {
        setAdcAverageDbfs(value.toDouble());
        return QString();
    }
    if (propertyName == "agcGainDb") {
        setAgcGainDb(value.toDouble());
        return QString();
    }
    if (propertyName == "agcPeakDb") {
        setAgcPeakDb(value.toDouble());
        return QString();
    }
    // Parity Task 16: the Core's channel's minimum notch width.
    if (propertyName == "minNotchWidthHz") {
        const double hz = value.toDouble();
        if (!std::isfinite(hz) || hz < 0.0) {
            return QStringLiteral("The narrowest notch width must be a number of hertz.");
        }
        setMinNotchWidthHz(hz);
        return QString();
    }
    if (propertyName == "agcAverageDb") {
        setAgcAverageDb(value.toDouble());
        return QString();
    }
    if (propertyName == "stationAutoAgcNoiseFloorDbm") {
        const double dbm = value.toDouble();
        if (!std::isfinite(dbm)) { return QStringLiteral("The noise floor reading must be a number."); }
        setStationAutoAgcNoiseFloor(dbm, m_stationAutoAgcNoiseFloorValid,
                                   m_stationAutoAgcNoiseFloorGeneration);
        return {};
    }
    if (propertyName == "stationAutoAgcNoiseFloorValid") {
        setStationAutoAgcNoiseFloor(m_stationAutoAgcNoiseFloorDbm, value.toBool(),
                                   m_stationAutoAgcNoiseFloorGeneration);
        return {};
    }
    if (propertyName == "stationAutoAgcNoiseFloorGeneration") {
        setStationAutoAgcNoiseFloor(m_stationAutoAgcNoiseFloorDbm,
                                   m_stationAutoAgcNoiseFloorValid, value.toULongLong());
        return {};
    }
    if (propertyName == "streamCtunPinned") {
        setStreamCtunPinned(value.toBool());
        return {};
    }
    if (propertyName == "streamEpoch") {
        setStreamEpoch(value.toULongLong());
        return {};
    }
    if (propertyName == "active") {
        // Fix round 1 review finding (Important 1): this used to name
        // RadioModel::setActiveSlice(), which a remote peer cannot reach
        // (it is positional, not a session verb, and not even the
        // preferred local entry point -- see its own doc comment).
        // SessionCommandDispatcher's setActiveSliceById verb is the real,
        // reachable path; name that instead, the same way StateMirror.cpp's
        // kVerbHints table names requestSliceSampleRate for sampleRateHz.
        // R-IOS-01: in operator words; the way in is the setActiveSliceById
        // command, which is what selecting a slice sends.
        return activeWriteReason();
    }
    if (propertyName == "txSlice") {
        return QStringLiteral(
            "Choose the transmit slice with its own control; it cannot be set directly.");
    }
    if (propertyName == "band") {
        return QStringLiteral("The band follows the frequency; change the frequency instead.");
    }

    return QStringLiteral("The Core sets this itself; it cannot be changed from here.");
}

// ── Phase 3F Sub-Epic A: multi-panadapter / multi-slice identity ────────────

void SliceModel::setChainIndex(int idx)
{
    if (m_chainIndex != idx) {
        m_chainIndex = idx;
        emit chainIndexChanged(idx);
    }
}

void SliceModel::setDdcIndex(int ddc)
{
    if (m_ddcIndex != ddc) {
        m_ddcIndex = ddc;
        emit ddcIndexChanged(ddc);
    }
}

void SliceModel::setStreamIndex(int idx)
{
    if (m_streamIndex != idx) {
        m_streamIndex = idx;
        emit streamIndexChanged(idx);
    }
}

void SliceModel::setStreamCtunPinned(bool pinned)
{
    if (m_streamCtunPinned == pinned) {
        return;
    }
    m_streamCtunPinned = pinned;
    emit streamCtunPinnedChanged(pinned);
}

void SliceModel::setStreamEpoch(quint64 epoch)
{
    if (m_streamEpoch == epoch) {
        return;
    }
    m_streamEpoch = epoch;
    emit streamEpochChanged(epoch);
}

void SliceModel::setShiftOffsetHz(double hz)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_shiftOffsetHz - hz)) {
        return;
    }
    m_shiftOffsetHz = hz;
    emit shiftOffsetHzChanged(hz);
}

void SliceModel::setPanKey(const QString& key)
{
    // Slice control plan Task 5: not held on a listened slice. Which pan
    // shows it is this window's layout (a layout change rehomes it so it
    // stays visible); the window's StationClient does not send it.
    if (m_panKey != key) {
        m_panKey = key;
        emit panKeyChanged(key);
    }
}

void SliceModel::setSampleRateHz(int hz)
{
    if (m_sampleRateHz != hz) {
        m_sampleRateHz = hz;
        emit sampleRateHzChanged(hz);
    }
}

void SliceModel::setDiversityEnabled(bool on)
{
    if (holdsListenerWrite(diversityEnabled(), on)) { return; }
    if (m_diversityEnabled != on) {
        m_diversityEnabled = on;
        emit diversityEnabledChanged(on);
    }
}

// ── Phase 3F Sub-Epic G Task 2: per-band diversity tuning setters ────────────
//
// Behaviour mirrors the rest of the Sub-Epic A setters: emit-on-change so the
// future DiversityDialog (T8-T10) and the RadioModel signal wire (T13) only
// fire downstream work when the value actually moves. Reject nonfinite scalars and clamp to the
// same 0..360 / -20..+20 domains used by DiversityDialog.

void SliceModel::setDiversityPhaseDeg(double deg)
{
    if (!std::isfinite(deg)) { return; }
    deg = std::clamp(deg, 0.0, 360.0);
    if (holdsListenerWrite(diversityPhaseDeg(), deg)) { return; }
    if (m_diversityPhaseDeg != deg) {
        m_diversityPhaseDeg = deg;
        emit diversityPhaseDegChanged(deg);
        noteDiversityPatternInputs();
    }
}

void SliceModel::setDiversityGainDb(double db)
{
    if (!std::isfinite(db)) { return; }
    db = std::clamp(db, -20.0, 20.0);
    if (holdsListenerWrite(diversityGainDb(), db)) { return; }
    if (m_diversityGainDb != db) {
        m_diversityGainDb = db;
        emit diversityGainDbChanged(db);
        noteDiversityPatternInputs();
    }
}

void SliceModel::setRadeSynced(bool synced)
{
    if (m_radeSynced == synced) {
        return;
    }
    m_radeSynced = synced;
    emit radeSyncedChanged(synced);
}

void SliceModel::setRadeFreqOffsetHz(double hz)
{
    if (m_radeFreqOffsetHz == hz) {
        return;
    }
    m_radeFreqOffsetHz = hz;
    emit radeFreqOffsetHzChanged(hz);
}

void SliceModel::setRadeReason(const QString& reason)
{
    if (m_radeReason == reason) {
        return;
    }
    m_radeReason = reason;
    emit radeReasonChanged(reason);
}

void SliceModel::setMeterReadingNs(qint64 ns)
{
    if (m_meterReadingNs == ns) {
        return;
    }
    m_meterReadingNs = ns;
    emit meterReadingNsChanged(ns);
}

QString SliceModel::diversityPattern() const
{
    m_diversityPatternLast = DiversityPattern::wireJson(m_frequency, m_diversityPhaseDeg,
                                                        m_diversityGainDb);
    return m_diversityPatternLast;
}

void SliceModel::noteDiversityPatternInputs()
{
    const QString previous = m_diversityPatternLast;
    const QString now = diversityPattern();
    if (now != previous) {
        emit diversityPatternChanged(now);
    }
}

void SliceModel::setDiversityFineNullEnabled(bool on)
{
    if (holdsListenerWrite(diversityFineNullEnabled(), on)) { return; }
    if (m_diversityFineNullEnabled != on) {
        m_diversityFineNullEnabled = on;
        emit diversityFineNullEnabledChanged(on);
    }
}

void SliceModel::setWidebandExtensionRequested(bool on)
{
    if (m_widebandExtensionRequested != on) {
        m_widebandExtensionRequested = on;
        emit widebandExtensionRequestedChanged(on);
    }
}

void SliceModel::setPsPaused(bool paused)
{
    if (m_psPaused != paused) {
        m_psPaused = paused;
        emit psPausedChanged(paused);
    }
}

// ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──

void SliceModel::setLocked(bool v)
{
    if (holdsListenerWrite(locked(), v)) { return; }
    if (m_locked != v) {
        m_locked = v;
        emit lockedChanged(v);
    }
}

void SliceModel::setMuted(bool v)
{
    if (holdsListenerWrite(muted(), v)) { return; }
    if (m_muted != v) {
        m_muted = v;
        emit mutedChanged(v);
    }
}

void SliceModel::setAudioPan(double pan)
{
    if (holdsListenerWrite(audioPan(), pan)) { return; }
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_audioPan - pan)) {
        return;
    }
    m_audioPan = pan;
    emit audioPanChanged(pan);
}

void SliceModel::setSsqlEnabled(bool v)
{
    if (holdsListenerWrite(ssqlEnabled(), v)) { return; }
    if (m_ssqlEnabled != v) {
        m_ssqlEnabled = v;
        emit ssqlEnabledChanged(v);
    }
}

void SliceModel::setSsqlThresh(double dB)
{
    if (holdsListenerWrite(ssqlThresh(), dB)) { return; }
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_ssqlThresh - dB)) {
        return;
    }
    m_ssqlThresh = dB;
    emit ssqlThreshChanged(dB);
}

void SliceModel::setAmsqEnabled(bool v)
{
    if (holdsListenerWrite(amsqEnabled(), v)) { return; }
    if (m_amsqEnabled != v) {
        m_amsqEnabled = v;
        emit amsqEnabledChanged(v);
    }
}

void SliceModel::setAmsqThresh(double dB)
{
    if (holdsListenerWrite(amsqThresh(), dB)) { return; }
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_amsqThresh - dB)) {
        return;
    }
    m_amsqThresh = dB;
    emit amsqThreshChanged(dB);
}

void SliceModel::setFmsqEnabled(bool v)
{
    if (holdsListenerWrite(fmsqEnabled(), v)) { return; }
    if (m_fmsqEnabled != v) {
        m_fmsqEnabled = v;
        emit fmsqEnabledChanged(v);
    }
}

void SliceModel::setFmsqThresh(double dB)
{
    if (holdsListenerWrite(fmsqThresh(), dB)) { return; }
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_fmsqThresh - dB)) {
        return;
    }
    m_fmsqThresh = dB;
    emit fmsqThreshChanged(dB);
}

void SliceModel::setAgcThreshold(int dBu)
{
    if (holdsListenerWrite(agcThreshold(), dBu)) { return; }
    if (m_agcThreshold != dBu) {
        m_agcThreshold = dBu;
        emit agcThresholdChanged(dBu);
    }
}

void SliceModel::setAgcHang(int ms)
{
    if (holdsListenerWrite(agcHang(), ms)) { return; }
    if (m_agcHang != ms) {
        m_agcHang = ms;
        emit agcHangChanged(ms);
    }
}

void SliceModel::setAgcSlope(int dB)
{
    if (holdsListenerWrite(agcSlope(), dB)) { return; }
    if (m_agcSlope != dB) {
        m_agcSlope = dB;
        emit agcSlopeChanged(dB);
    }
}

void SliceModel::setAgcAttack(int ms)
{
    if (holdsListenerWrite(agcAttack(), ms)) { return; }
    if (m_agcAttack != ms) {
        m_agcAttack = ms;
        emit agcAttackChanged(ms);
    }
}

void SliceModel::setAgcDecay(int ms)
{
    if (holdsListenerWrite(agcDecay(), ms)) { return; }
    if (m_agcDecay != ms) {
        m_agcDecay = ms;
        emit agcDecayChanged(ms);
    }
}

void SliceModel::setAutoAgcEnabled(bool on)
{
    if (holdsListenerWrite(autoAgcEnabled(), on)) { return; }
    if (m_autoAgcEnabled != on) {
        m_autoAgcEnabled = on;
        emit autoAgcEnabledChanged(on);
    }
}

void SliceModel::setAutoAgcOffset(double dB)
{
    if (holdsListenerWrite(autoAgcOffset(), dB)) { return; }
    if (!qFuzzyCompare(m_autoAgcOffset, dB)) {
        m_autoAgcOffset = dB;
        emit autoAgcOffsetChanged(dB);
    }
}

void SliceModel::setAgcFixedGain(int dB)
{
    if (holdsListenerWrite(agcFixedGain(), dB)) { return; }
    if (m_agcFixedGain != dB) {
        m_agcFixedGain = dB;
        emit agcFixedGainChanged(dB);
    }
}

void SliceModel::setAgcHangThreshold(int val)
{
    if (holdsListenerWrite(agcHangThreshold(), val)) { return; }
    if (m_agcHangThreshold != val) {
        m_agcHangThreshold = val;
        emit agcHangThresholdChanged(val);
    }
}

void SliceModel::setAgcMaxGain(int dB)
{
    if (holdsListenerWrite(agcMaxGain(), dB)) { return; }
    if (m_agcMaxGain != dB) {
        m_agcMaxGain = dB;
        emit agcMaxGainChanged(dB);
    }
}

void SliceModel::setRitEnabled(bool v)
{
    if (holdsListenerWrite(ritEnabled(), v)) { return; }
    if (m_ritEnabled != v) {
        m_ritEnabled = v;
        emit ritEnabledChanged(v);
    }
}

void SliceModel::setRitHz(int hz)
{
    if (holdsListenerWrite(ritHz(), hz)) { return; }
    if (m_ritHz != hz) {
        m_ritHz = hz;
        emit ritHzChanged(hz);
    }
}

void SliceModel::setXitEnabled(bool v)
{
    if (holdsListenerWrite(xitEnabled(), v)) { return; }
    if (m_xitEnabled != v) {
        m_xitEnabled = v;
        emit xitEnabledChanged(v);
    }
}

void SliceModel::setXitHz(int hz)
{
    if (holdsListenerWrite(xitHz(), hz)) { return; }
    if (m_xitHz != hz) {
        m_xitHz = hz;
        emit xitHzChanged(hz);
    }
}

void SliceModel::setNbMode(NereusSDR::NbMode v)
{
    if (holdsListenerWrite(nbMode(), v)) { return; }
    if (v == m_nbMode) { return; }
    m_nbMode = v;
    emit nbModeChanged(v);
}

// setNbTuning / nbTuningChanged removed 2026-04-22 — per-slice NB tuning is
// not a Thetis concept. All NB tuning is global per DSPRX and lives inside
// NbFamily, seeded from Setup → DSP → NB/SNB. See SliceModel.h.

// --- NR setters (Sub-epic C-1) ---
// See Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13].

void SliceModel::setActiveNr(NereusSDR::NrSlot slot)
{
    if (holdsListenerWrite(activeNr(), slot)) { return; }
    setNnrLastError({});
    if (static_cast<int>(slot) < 0 || static_cast<int>(slot) > static_cast<int>(NrSlot::NNR)) {
        setNnrLastError(QStringLiteral("Unsupported noise-reduction selection."));
        emit nnrEditRejected(m_nnrLastError);
        return;
    }
    if (m_activeNr == slot) { return; }
    QString reason;
    if (m_nrSelectionApplier && !m_nrSelectionApplier(slot, &reason)) {
        setNnrLastError(reason.isEmpty() ? QStringLiteral("The requested noise reducer is unavailable.") : reason);
        emit nnrEditRejected(m_nnrLastError);
        emit nrSelectionRefused(m_nnrLastError);
        return;
    }
    m_activeNr = slot;
    emit activeNrChanged(slot);
}


// NereusSDR-original accepted NNR configuration. The coordinator supplies
// the optional appliers; offline objects retain valid preferences without
// pretending a WDSP receiver is running.
void SliceModel::setNnrLastError(const QString& error)
{
    if (m_nnrLastError == error) return;
    m_nnrLastError = error;
    emit nnrLastErrorChanged();
}

bool SliceModel::applyNnrSettings(const NnrSettings& requested)
{
    if (holdsListenerWrite(m_nnrSettings, requested)) { return false; }
    setNnrLastError({});
    if (!requested.isValid()) {
        setNnrLastError(QStringLiteral("NNR values must be finite and within their supported ranges."));
        emit nnrEditRejected(m_nnrLastError);
        return false;
    }
    if (requested == m_nnrSettings) return true;
    auto accepted = std::optional<NnrSettings>{requested};
    QString reason;
    if (m_nnrSettingsApplier)
        accepted = m_nnrSettingsApplier(requested, &reason);
    if (!accepted || !accepted->isValid()) {
        setNnrLastError(reason.isEmpty() ? QStringLiteral("The NNR receiver refused this configuration.") : reason);
        emit nnrEditRejected(m_nnrLastError);
        return false;
    }
    const auto before = m_nnrSettings;
    m_nnrSettings = *accepted;
    if (before.modelSlot != m_nnrSettings.modelSlot) emit nnrModelSlotChanged(m_nnrSettings.modelSlot);
    if (before.maskFloorDb != m_nnrSettings.maskFloorDb) emit nnrMaskFloorDbChanged(m_nnrSettings.maskFloorDb);
    if (before.position != m_nnrSettings.position) emit nnrPositionChanged(m_nnrSettings.position);
    if (before.alpha != m_nnrSettings.alpha) emit nnrAlphaChanged(m_nnrSettings.alpha);
    if (before.alphaKneeDb != m_nnrSettings.alphaKneeDb) emit nnrAlphaKneeDbChanged(m_nnrSettings.alphaKneeDb);
    if (before.tauSeconds != m_nnrSettings.tauSeconds) emit nnrTauSecondsChanged(m_nnrSettings.tauSeconds);
    if (before.maxGainDb != m_nnrSettings.maxGainDb) emit nnrMaxGainDbChanged(m_nnrSettings.maxGainDb);
    if (before.attackMs != m_nnrSettings.attackMs) emit nnrAttackMsChanged(m_nnrSettings.attackMs);
    if (before.releaseMs != m_nnrSettings.releaseMs) emit nnrReleaseMsChanged(m_nnrSettings.releaseMs);
    if (before != m_nnrSettings) emit nnrConfigurationChanged();
    return true;
}

void SliceModel::setNnrModelSlot(int value)
{
    if (holdsListenerWrite(nnrModelSlot(), value)) { return; }
    // R-R3-40: choosing a model while the Core holds this receiver back is
    // the operator asking for it again, even when it is the saved model
    // (which the equality check in applyNnrSettings would otherwise ignore).
    // The new choice is applied first, so clearing the limit runs it and
    // never the choice it replaces.
    const bool limited = m_nnrLimit != 0;
    auto requested = m_nnrSettings;
    requested.modelSlot = value;
    applyNnrSettings(requested);
    if (limited) {
        emit nnrRetryRequested();
    }
}

void SliceModel::setNnrMaskFloorDb(double value)
{
    if (holdsListenerWrite(nnrMaskFloorDb(), value)) { return; }
    auto requested = m_nnrSettings;
    requested.maskFloorDb = value;
    applyNnrSettings(requested);
}

void SliceModel::setNnrPosition(NereusSDR::NrPosition value)
{
    if (holdsListenerWrite(nnrPosition(), value)) { return; }
    auto requested = m_nnrSettings;
    requested.position = value;
    applyNnrSettings(requested);
}

void SliceModel::setNnrAlpha(double value)
{
    if (holdsListenerWrite(nnrAlpha(), value)) { return; }
    auto requested = m_nnrSettings;
    requested.alpha = value;
    applyNnrSettings(requested);
}

void SliceModel::setNnrAlphaKneeDb(double value)
{
    if (holdsListenerWrite(nnrAlphaKneeDb(), value)) { return; }
    auto requested = m_nnrSettings;
    requested.alphaKneeDb = value;
    applyNnrSettings(requested);
}

void SliceModel::setNnrTauSeconds(double value)
{
    if (holdsListenerWrite(nnrTauSeconds(), value)) { return; }
    auto requested = m_nnrSettings;
    requested.tauSeconds = value;
    applyNnrSettings(requested);
}

void SliceModel::setNnrMaxGainDb(double value)
{
    if (holdsListenerWrite(nnrMaxGainDb(), value)) { return; }
    auto requested = m_nnrSettings;
    requested.maxGainDb = value;
    applyNnrSettings(requested);
}

void SliceModel::setNnrAttackMs(double value)
{
    if (holdsListenerWrite(nnrAttackMs(), value)) { return; }
    auto requested = m_nnrSettings;
    requested.attackMs = value;
    applyNnrSettings(requested);
}

void SliceModel::setNnrReleaseMs(double value)
{
    if (holdsListenerWrite(nnrReleaseMs(), value)) { return; }
    auto requested = m_nnrSettings;
    requested.releaseMs = value;
    applyNnrSettings(requested);
}

void SliceModel::resetNnrTuning()
{
    NnrSettings defaults;
    defaults.modelSlot = m_nnrSettings.modelSlot;
    applyNnrSettings(defaults);
}

void SliceModel::setNnrLimit(int limit)
{
    if (!isValidNnrLimit(limit) || m_nnrLimit == limit) {
        return;
    }
    m_nnrLimit = limit;
    emit nnrLimitChanged(limit);
    // nnrStatus reads the limit's reason while one is in force.
    emit nnrDiagnosticsChanged();
}

void SliceModel::requestNnrRetry()
{
    emit nnrRetryRequested();
}

void SliceModel::updateNnrDiagnostics(const NnrDiagnostics& diagnostics)
{
    if (m_nnrDiagnostics == diagnostics) return;
    m_nnrDiagnostics = diagnostics;
    emit nnrDiagnosticsChanged();
}

QString SliceModel::nnrModelSource() const
{
    const int slot = m_nnrDiagnostics.actualModelSlot;
    if (slot < 0 || slot >= 2) return QStringLiteral("Unavailable");
    switch (m_nnrDiagnostics.modelSources[slot]) {
    case NnrModelSource::Bundled: return QStringLiteral("Bundled");
    case NnrModelSource::File: return QStringLiteral("Station asset");
    case NnrModelSource::Unavailable: return QStringLiteral("Unavailable");
    }
    return QStringLiteral("Unavailable");
}

void SliceModel::setSettingsRadioIdentity(const QString& mac)
{
    m_settingsRadioMac = AppSettings::normalizedRadioMac(mac);
}

void SliceModel::requestNnrDiagnostics(int testMode, int outputMode)
{
    if (testMode < 0 || testMode > 2 || outputMode < 0 || outputMode > 1) {
        setNnrLastError(QStringLiteral("Unsupported NNR diagnostic mode."));
        return;
    }
    emit nnrDiagnosticsRequested(testMode, outputMode);
}

bool SliceModel::applyStationNnrDiagnostic(const QByteArray& name, const QVariant& value)
{
    auto status = m_nnrDiagnostics;
    if (name == "nnrAvailable") status.available = value.toBool();
    else if (name == "nnrReady") status.ready = value.toBool();
    else if (name == "nnrRunning") status.running = value.toBool();
    else if (name == "nnrRateSupported") status.rateSupported = value.toBool();
    else if (name == "nnrStandardAvailable") status.modelAvailable[0] = value.toBool();
    else if (name == "nnrPremiumAvailable") status.modelAvailable[1] = value.toBool();
    else if (name == "nnrActualModelSlot") status.actualModelSlot = value.toInt();
    else if (name == "nnrDspRateHz") status.dspRateHz = value.toInt();
    else if (name == "nnrNetworkRateHz") status.networkRateHz = value.toInt();
    else if (name == "nnrDelaySamples") status.delaySamples = value.toInt();
    else if (name == "nnrProfilingAvailable") status.profilingAvailable = value.toBool();
    else if (name == "nnrLatencyMs") status.latencyMs = value.toDouble();
    else if (name == "nnrTestMode") status.testMode = value.toInt();
    else if (name == "nnrOutputMode") status.outputMode = value.toInt();
    else if (name == "nnrStatus") status.explanation = value.toString();
    else if (name == "nnrLastError") { setNnrLastError(value.toString()); return true; }
    else if (name == "nnrLimit") {
        bool ok = false;
        const int limit = value.toInt(&ok);
        if (!ok || !isValidNnrLimit(limit)) {
            return false;
        }
        m_nnrLimitFromCore = true;   // the Core set it: say so in its reason
        setNnrLimit(limit);
        return true;
    }
    else if (name == "nnrModelSource") {
        if (status.actualModelSlot >= 0 && status.actualModelSlot < 2) {
            status.modelSources[status.actualModelSlot] = value.toString() == QStringLiteral("Bundled")
                ? NnrModelSource::Bundled : value.toString() == QStringLiteral("Station asset")
                ? NnrModelSource::File : NnrModelSource::Unavailable;
        }
    } else return false;
    if (status.actualModelSlot < -1 || status.actualModelSlot > 1
        || status.dspRateHz < 0 || status.networkRateHz < 0 || status.delaySamples < 0
        || !std::isfinite(status.latencyMs) || status.latencyMs < 0
        || status.testMode < 0 || status.testMode > 2 || status.outputMode < 0 || status.outputMode > 1) {
        return false;
    }
    updateNnrDiagnostics(status);
    return true;
}

QString SliceModel::nnrSettingsPrefix() const
{
    if (m_settingsRadioMac.isEmpty() || m_sliceIndex < 0) return {};
    return QStringLiteral("hardware/%1/slices/%2/nnr/").arg(m_settingsRadioMac).arg(m_sliceIndex);
}

void SliceModel::saveNnrSettings() const
{
    const QString prefix = nnrSettingsPrefix();
    if (prefix.isEmpty() || !m_nnrSettings.isValid()) return;
    auto& settings = AppSettings::instance();
    settings.setValue(prefix + QStringLiteral("NrActive"), static_cast<int>(m_activeNr));
    settings.setValue(prefix + QStringLiteral("NnrModelSlot"), m_nnrSettings.modelSlot);
    settings.setValue(prefix + QStringLiteral("NnrMaskFloorDb"), m_nnrSettings.maskFloorDb);
    settings.setValue(prefix + QStringLiteral("NnrPosition"), static_cast<int>(m_nnrSettings.position));
    settings.setValue(prefix + QStringLiteral("NnrAlpha"), m_nnrSettings.alpha);
    settings.setValue(prefix + QStringLiteral("NnrAlphaKneeDb"), m_nnrSettings.alphaKneeDb);
    settings.setValue(prefix + QStringLiteral("NnrTauSeconds"), m_nnrSettings.tauSeconds);
    settings.setValue(prefix + QStringLiteral("NnrMaxGainDb"), m_nnrSettings.maxGainDb);
    settings.setValue(prefix + QStringLiteral("NnrAttackMs"), m_nnrSettings.attackMs);
    settings.setValue(prefix + QStringLiteral("NnrReleaseMs"), m_nnrSettings.releaseMs);
}

void SliceModel::restoreNnrSettings()
{
    const QString prefix = nnrSettingsPrefix();
    if (prefix.isEmpty()) return;
    auto& settings = AppSettings::instance();
    NnrSettings restored;
    QStringList rejected;
    // Validate each stored field against a full valid configuration. One
    // damaged field falls back to its documented default without losing the
    // independent valid fields, and the fallback remains visible.
    if (settings.contains(prefix + QStringLiteral("NnrModelSlot"))) {
        bool ok = false;
        const auto value = settings.value(prefix + QStringLiteral("NnrModelSlot")).toInt(&ok);
        auto candidate = restored;
        candidate.modelSlot = value;
        if (ok && candidate.isValid()) restored = candidate;
        else rejected.append(QStringLiteral("NnrModelSlot"));
    }
    if (settings.contains(prefix + QStringLiteral("NnrMaskFloorDb"))) {
        bool ok = false;
        const auto value = settings.value(prefix + QStringLiteral("NnrMaskFloorDb")).toDouble(&ok);
        auto candidate = restored;
        candidate.maskFloorDb = value;
        if (ok && candidate.isValid()) restored = candidate;
        else rejected.append(QStringLiteral("NnrMaskFloorDb"));
    }
    if (settings.contains(prefix + QStringLiteral("NnrPosition"))) {
        bool ok = false;
        const auto value = settings.value(prefix + QStringLiteral("NnrPosition")).toInt(&ok);
        auto candidate = restored;
        candidate.position = static_cast<NrPosition>(value);
        if (ok && candidate.isValid()) restored = candidate;
        else rejected.append(QStringLiteral("NnrPosition"));
    }
    if (settings.contains(prefix + QStringLiteral("NnrAlpha"))) {
        bool ok = false;
        const auto value = settings.value(prefix + QStringLiteral("NnrAlpha")).toDouble(&ok);
        auto candidate = restored;
        candidate.alpha = value;
        if (ok && candidate.isValid()) restored = candidate;
        else rejected.append(QStringLiteral("NnrAlpha"));
    }
    if (settings.contains(prefix + QStringLiteral("NnrAlphaKneeDb"))) {
        bool ok = false;
        const auto value = settings.value(prefix + QStringLiteral("NnrAlphaKneeDb")).toDouble(&ok);
        auto candidate = restored;
        candidate.alphaKneeDb = value;
        if (ok && candidate.isValid()) restored = candidate;
        else rejected.append(QStringLiteral("NnrAlphaKneeDb"));
    }
    if (settings.contains(prefix + QStringLiteral("NnrTauSeconds"))) {
        bool ok = false;
        const auto value = settings.value(prefix + QStringLiteral("NnrTauSeconds")).toDouble(&ok);
        auto candidate = restored;
        candidate.tauSeconds = value;
        if (ok && candidate.isValid()) restored = candidate;
        else rejected.append(QStringLiteral("NnrTauSeconds"));
    }
    if (settings.contains(prefix + QStringLiteral("NnrMaxGainDb"))) {
        bool ok = false;
        const auto value = settings.value(prefix + QStringLiteral("NnrMaxGainDb")).toDouble(&ok);
        auto candidate = restored;
        candidate.maxGainDb = value;
        if (ok && candidate.isValid()) restored = candidate;
        else rejected.append(QStringLiteral("NnrMaxGainDb"));
    }
    if (settings.contains(prefix + QStringLiteral("NnrAttackMs"))) {
        bool ok = false;
        const auto value = settings.value(prefix + QStringLiteral("NnrAttackMs")).toDouble(&ok);
        auto candidate = restored;
        candidate.attackMs = value;
        if (ok && candidate.isValid()) restored = candidate;
        else rejected.append(QStringLiteral("NnrAttackMs"));
    }
    if (settings.contains(prefix + QStringLiteral("NnrReleaseMs"))) {
        bool ok = false;
        const auto value = settings.value(prefix + QStringLiteral("NnrReleaseMs")).toDouble(&ok);
        auto candidate = restored;
        candidate.releaseMs = value;
        if (ok && candidate.isValid()) restored = candidate;
        else rejected.append(QStringLiteral("NnrReleaseMs"));
    }
    if (!applyNnrSettings(restored)) return;
    NrSlot active = NrSlot::Off;
    if (settings.contains(prefix + QStringLiteral("NrActive"))) {
        bool ok = false;
        const int stored = settings.value(prefix + QStringLiteral("NrActive")).toInt(&ok);
        if (ok && stored >= 0 && stored <= static_cast<int>(NrSlot::NNR)) active = static_cast<NrSlot>(stored);
        else rejected.append(QStringLiteral("NrActive"));
    }
    setActiveNr(active);
    if (!rejected.isEmpty()) {
        // nnrLastError reaches a remote app as sent: operator words there,
        // the setting names in the log (iPhone app Part A fix wave, R-IOS-01).
        qCInfo(lcDsp) << "Saved NNR settings not used, defaults applied:" << rejected;
        setNnrLastError(QStringLiteral("Some saved noise reduction settings could not be used, "
                                       "so their defaults are in use."));
    }
}

// NR1
void SliceModel::setNr1Taps(int v)
{
    if (holdsListenerWrite(nr1Taps(), v)) { return; }
    if (m_nr1Taps == v) { return; }
    m_nr1Taps = v;
    emit nr1TapsChanged(v);
}
void SliceModel::setNr1Delay(int v)
{
    if (holdsListenerWrite(nr1Delay(), v)) { return; }
    if (m_nr1Delay == v) { return; }
    m_nr1Delay = v;
    emit nr1DelayChanged(v);
}
void SliceModel::setNr1Gain(double v)
{
    if (holdsListenerWrite(nr1Gain(), v)) { return; }
    if (qFuzzyCompare(m_nr1Gain, v)) { return; }
    m_nr1Gain = v;
    emit nr1GainChanged(v);
}
void SliceModel::setNr1Leakage(double v)
{
    if (holdsListenerWrite(nr1Leakage(), v)) { return; }
    if (qFuzzyCompare(m_nr1Leakage, v)) { return; }
    m_nr1Leakage = v;
    emit nr1LeakageChanged(v);
}
void SliceModel::setNr1Position(NereusSDR::NrPosition p)
{
    if (holdsListenerWrite(nr1Position(), p)) { return; }
    if (m_nr1Position == p) { return; }
    m_nr1Position = p;
    emit nr1PositionChanged(p);
}

// NR2
void SliceModel::setNr2GainMethod(NereusSDR::EmnrGainMethod v)
{
    if (holdsListenerWrite(nr2GainMethod(), v)) { return; }
    if (m_nr2GainMethod == v) { return; }
    m_nr2GainMethod = v;
    emit nr2GainMethodChanged(v);
}
void SliceModel::setNr2NpeMethod(NereusSDR::EmnrNpeMethod v)
{
    if (holdsListenerWrite(nr2NpeMethod(), v)) { return; }
    if (m_nr2NpeMethod == v) { return; }
    m_nr2NpeMethod = v;
    emit nr2NpeMethodChanged(v);
}
void SliceModel::setNr2TrainT1(double v)
{
    if (holdsListenerWrite(nr2TrainT1(), v)) { return; }
    if (qFuzzyCompare(m_nr2TrainT1, v)) { return; }
    m_nr2TrainT1 = v;
    emit nr2TrainT1Changed(v);
}
void SliceModel::setNr2TrainT2(double v)
{
    if (holdsListenerWrite(nr2TrainT2(), v)) { return; }
    if (qFuzzyCompare(m_nr2TrainT2, v)) { return; }
    m_nr2TrainT2 = v;
    emit nr2TrainT2Changed(v);
}
void SliceModel::setNr2AeFilter(bool v)
{
    if (holdsListenerWrite(nr2AeFilter(), v)) { return; }
    if (m_nr2AeFilter == v) { return; }
    m_nr2AeFilter = v;
    emit nr2AeFilterChanged(v);
}
void SliceModel::setNr2Position(NereusSDR::NrPosition p)
{
    if (holdsListenerWrite(nr2Position(), p)) { return; }
    if (m_nr2Position == p) { return; }
    m_nr2Position = p;
    emit nr2PositionChanged(p);
}
void SliceModel::setNr2Post2Run(bool v)
{
    if (holdsListenerWrite(nr2Post2Run(), v)) { return; }
    if (m_nr2Post2Run == v) { return; }
    m_nr2Post2Run = v;
    emit nr2Post2RunChanged(v);
}
void SliceModel::setNr2Post2Level(double v)
{
    if (holdsListenerWrite(nr2Post2Level(), v)) { return; }
    if (qFuzzyCompare(m_nr2Post2Level, v)) { return; }
    m_nr2Post2Level = v;
    emit nr2Post2LevelChanged(v);
}
void SliceModel::setNr2Post2Factor(double v)
{
    if (holdsListenerWrite(nr2Post2Factor(), v)) { return; }
    if (qFuzzyCompare(m_nr2Post2Factor, v)) { return; }
    m_nr2Post2Factor = v;
    emit nr2Post2FactorChanged(v);
}
void SliceModel::setNr2Post2Rate(double v)
{
    if (holdsListenerWrite(nr2Post2Rate(), v)) { return; }
    if (qFuzzyCompare(m_nr2Post2Rate, v)) { return; }
    m_nr2Post2Rate = v;
    emit nr2Post2RateChanged(v);
}
void SliceModel::setNr2Post2Taper(int v)
{
    if (holdsListenerWrite(nr2Post2Taper(), v)) { return; }
    if (m_nr2Post2Taper == v) { return; }
    m_nr2Post2Taper = v;
    emit nr2Post2TaperChanged(v);
}

// NR3
void SliceModel::setNr3Position(NereusSDR::NrPosition p)
{
    if (holdsListenerWrite(nr3Position(), p)) { return; }
    if (m_nr3Position == p) { return; }
    m_nr3Position = p;
    emit nr3PositionChanged(p);
}
void SliceModel::setNr3UseDefaultGain(bool v)
{
    if (holdsListenerWrite(nr3UseDefaultGain(), v)) { return; }
    if (m_nr3UseDefaultGain == v) { return; }
    m_nr3UseDefaultGain = v;
    emit nr3UseDefaultGainChanged(v);
}

// NR4
void SliceModel::setNr4Reduction(double v)
{
    if (holdsListenerWrite(nr4Reduction(), v)) { return; }
    if (qFuzzyCompare(m_nr4Reduction, v)) { return; }
    m_nr4Reduction = v;
    emit nr4ReductionChanged(v);
}
void SliceModel::setNr4Smoothing(double v)
{
    if (holdsListenerWrite(nr4Smoothing(), v)) { return; }
    if (qFuzzyCompare(m_nr4Smoothing, v)) { return; }
    m_nr4Smoothing = v;
    emit nr4SmoothingChanged(v);
}
void SliceModel::setNr4Whitening(double v)
{
    if (holdsListenerWrite(nr4Whitening(), v)) { return; }
    if (qFuzzyCompare(m_nr4Whitening, v)) { return; }
    m_nr4Whitening = v;
    emit nr4WhiteningChanged(v);
}
void SliceModel::setNr4Rescale(double v)
{
    if (holdsListenerWrite(nr4Rescale(), v)) { return; }
    if (qFuzzyCompare(m_nr4Rescale, v)) { return; }
    m_nr4Rescale = v;
    emit nr4RescaleChanged(v);
}
void SliceModel::setNr4PostThresh(double v)
{
    if (holdsListenerWrite(nr4PostThresh(), v)) { return; }
    if (qFuzzyCompare(m_nr4PostThresh, v)) { return; }
    m_nr4PostThresh = v;
    emit nr4PostThreshChanged(v);
}
void SliceModel::setNr4Algo(NereusSDR::SbnrAlgo v)
{
    if (holdsListenerWrite(nr4Algo(), v)) { return; }
    if (m_nr4Algo == v) { return; }
    m_nr4Algo = v;
    emit nr4AlgoChanged(v);
}

// DFNR
void SliceModel::setDfnrAttenLimit(double v)
{
    if (holdsListenerWrite(dfnrAttenLimit(), v)) { return; }
    if (qFuzzyCompare(m_dfnrAttenLimit, v)) { return; }
    m_dfnrAttenLimit = v;
    emit dfnrAttenLimitChanged(v);
}
void SliceModel::setDfnrPostFilterBeta(double v)
{
    if (holdsListenerWrite(dfnrPostFilterBeta(), v)) { return; }
    if (qFuzzyCompare(m_dfnrPostFilterBeta, v)) { return; }
    m_dfnrPostFilterBeta = v;
    emit dfnrPostFilterBetaChanged(v);
}

// BNR + MNR
void SliceModel::setBnrStrength(double v)
{
    if (holdsListenerWrite(bnrStrength(), v)) { return; }
    if (qFuzzyCompare(m_bnrStrength, v)) { return; }
    m_bnrStrength = v;
    emit bnrStrengthChanged(v);
}
void SliceModel::setMnrStrength(double v)
{
    if (holdsListenerWrite(mnrStrength(), v)) { return; }
    if (qFuzzyCompare(m_mnrStrength, v)) { return; }
    m_mnrStrength = v;
    emit mnrStrengthChanged(v);
}
void SliceModel::setMnrOversub(double v)
{
    if (holdsListenerWrite(mnrOversub(), v)) { return; }
    if (qFuzzyCompare(m_mnrOversub, v)) { return; }
    m_mnrOversub = v;
    emit mnrOversubChanged(v);
}
void SliceModel::setMnrFloor(double v)
{
    if (holdsListenerWrite(mnrFloor(), v)) { return; }
    if (qFuzzyCompare(m_mnrFloor, v)) { return; }
    m_mnrFloor = v;
    emit mnrFloorChanged(v);
}
void SliceModel::setMnrAlpha(double v)
{
    if (holdsListenerWrite(mnrAlpha(), v)) { return; }
    if (qFuzzyCompare(m_mnrAlpha, v)) { return; }
    m_mnrAlpha = v;
    emit mnrAlphaChanged(v);
}
void SliceModel::setMnrBias(double v)
{
    if (holdsListenerWrite(mnrBias(), v)) { return; }
    if (qFuzzyCompare(m_mnrBias, v)) { return; }
    m_mnrBias = v;
    emit mnrBiasChanged(v);
}
void SliceModel::setMnrGsmooth(double v)
{
    if (holdsListenerWrite(mnrGsmooth(), v)) { return; }
    if (qFuzzyCompare(m_mnrGsmooth, v)) { return; }
    m_mnrGsmooth = v;
    emit mnrGsmoothChanged(v);
}

void SliceModel::setSnbEnabled(bool v)
{
    if (holdsListenerWrite(snbEnabled(), v)) { return; }
    if (m_snbEnabled != v) {
        m_snbEnabled = v;
        emit snbEnabledChanged(v);
    }
}

void SliceModel::setAnfEnabled(bool v)
{
    if (holdsListenerWrite(anfEnabled(), v)) { return; }
    if (m_anfEnabled != v) {
        m_anfEnabled = v;
        emit anfEnabledChanged(v);
    }
}

// ── NB1 / NB2 / SNB detailed tuning ─────────────────────────────────────────
// Ranges mirror Thetis's NumericUpDown limits byte-for-byte (grpDSPNB
// setup.designer.cs:44399-44604, grpDSPSNB :44280-44398 [v2.10.3.13]).
// Clamping here rather than at the UI edge keeps a TCI client or a corrupt
// settings file from handing WDSP a value the upstream widget could never
// produce; SetEXTANBTau and friends take the number without validating it.
//
// The idempotency guard is load-bearing beyond the usual signal hygiene: the
// NB1 / NB2 setters feed RadioModel's cross-slice mirror, so a setter that
// re-emitted on an unchanged value would bounce between co-hosted slices.
void SliceModel::setNb1Threshold(int v)
{
    if (holdsListenerWrite(nb1Threshold(), v)) { return; }
    const int clamped = qBound(1, v, 1000);
    if (m_nb1Threshold != clamped) {
        m_nb1Threshold = clamped;
        emit nb1ThresholdChanged(clamped);
    }
}

void SliceModel::setNb1TransitionMs(double v)
{
    if (holdsListenerWrite(nb1TransitionMs(), v)) { return; }
    const double clamped = qBound(0.01, v, 2.00);
    if (!qFuzzyCompare(m_nb1TransitionMs, clamped)) {
        m_nb1TransitionMs = clamped;
        emit nb1TransitionMsChanged(clamped);
    }
}

void SliceModel::setNb1LeadMs(double v)
{
    if (holdsListenerWrite(nb1LeadMs(), v)) { return; }
    const double clamped = qBound(0.01, v, 2.00);
    if (!qFuzzyCompare(m_nb1LeadMs, clamped)) {
        m_nb1LeadMs = clamped;
        emit nb1LeadMsChanged(clamped);
    }
}

void SliceModel::setNb1LagMs(double v)
{
    if (holdsListenerWrite(nb1LagMs(), v)) { return; }
    const double clamped = qBound(0.01, v, 2.00);
    if (!qFuzzyCompare(m_nb1LagMs, clamped)) {
        m_nb1LagMs = clamped;
        emit nb1LagMsChanged(clamped);
    }
}

// comboDSPNOBmode has five entries: Zero / Sample and Hold / Mean-Hold /
// Hold and Sample / Linear Interpolate (setup.designer.cs:44434 [v2.10.3.13]).
void SliceModel::setNb2Mode(int v)
{
    if (holdsListenerWrite(nb2Mode(), v)) { return; }
    const int clamped = qBound(0, v, 4);
    if (m_nb2Mode != clamped) {
        m_nb2Mode = clamped;
        emit nb2ModeChanged(clamped);
    }
}

void SliceModel::setSnbK1(double v)
{
    if (holdsListenerWrite(snbK1(), v)) { return; }
    const double clamped = qBound(2.0, v, 20.0);
    if (!qFuzzyCompare(m_snbK1, clamped)) {
        m_snbK1 = clamped;
        emit snbK1Changed(clamped);
    }
}

void SliceModel::setSnbK2(double v)
{
    if (holdsListenerWrite(snbK2(), v)) { return; }
    const double clamped = qBound(4.0, v, 60.0);
    if (!qFuzzyCompare(m_snbK2, clamped)) {
        m_snbK2 = clamped;
        emit snbK2Changed(clamped);
    }
}

// No Thetis Setup control for this one: Thetis picks SNB output bandwidth per
// mode at rxa.cs:112-124. The range is NereusSDR's own native override,
// unchanged from the slider it replaces.
void SliceModel::setSnbOutputBandwidthHz(int v)
{
    if (holdsListenerWrite(snbOutputBandwidthHz(), v)) { return; }
    const int clamped = qBound(100, v, 96000);
    if (m_snbOutputBandwidthHz != clamped) {
        m_snbOutputBandwidthHz = clamped;
        emit snbOutputBandwidthHzChanged(clamped);
    }
}

void SliceModel::setApfEnabled(bool v)
{
    if (holdsListenerWrite(apfEnabled(), v)) { return; }
    if (m_apfEnabled != v) {
        m_apfEnabled = v;
        emit apfEnabledChanged(v);
    }
}

void SliceModel::setApfTuneHz(int hz)
{
    if (holdsListenerWrite(apfTuneHz(), hz)) { return; }
    if (m_apfTuneHz != hz) {
        m_apfTuneHz = hz;
        emit apfTuneHzChanged(hz);
    }
}

void SliceModel::setBinauralEnabled(bool v)
{
    if (holdsListenerWrite(binauralEnabled(), v)) { return; }
    if (m_binauralEnabled != v) {
        m_binauralEnabled = v;
        emit binauralEnabledChanged(v);
    }
}

void SliceModel::setFmCtcssMode(int mode)
{
    if (holdsListenerWrite(fmCtcssMode(), mode)) { return; }
    if (m_fmCtcssMode != mode) {
        m_fmCtcssMode = mode;
        emit fmCtcssModeChanged(mode);
    }
}

void SliceModel::setFmCtcssValueHz(double hz)
{
    if (holdsListenerWrite(fmCtcssValueHz(), hz)) { return; }
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_fmCtcssValueHz - hz)) {
        return;
    }
    m_fmCtcssValueHz = hz;
    emit fmCtcssValueHzChanged(hz);
}

void SliceModel::setFmOffsetHz(int hz)
{
    if (holdsListenerWrite(fmOffsetHz(), hz)) { return; }
    if (m_fmOffsetHz != hz) {
        m_fmOffsetHz = hz;
        emit fmOffsetHzChanged(hz);
    }
}

void SliceModel::setFmTxMode(FmTxMode mode)
{
    if (holdsListenerWrite(fmTxMode(), mode)) { return; }
    if (m_fmTxMode == mode) { return; }
    m_fmTxMode = mode;
    emit fmTxModeChanged(mode);
}

void SliceModel::setFmReverse(bool v)
{
    if (holdsListenerWrite(fmReverse(), v)) { return; }
    if (m_fmReverse != v) {
        m_fmReverse = v;
        emit fmReverseChanged(v);
    }
}

void SliceModel::setDiglOffsetHz(int hz)
{
    if (holdsListenerWrite(diglOffsetHz(), hz)) { return; }
    if (m_diglOffsetHz == hz) { return; }
    m_diglOffsetHz = hz;
    emit diglOffsetHzChanged(hz);
}

void SliceModel::setDiguOffsetHz(int hz)
{
    if (holdsListenerWrite(diguOffsetHz(), hz)) { return; }
    if (m_diguOffsetHz == hz) { return; }
    m_diguOffsetHz = hz;
    emit diguOffsetHzChanged(hz);
}

void SliceModel::setRttyMarkHz(int hz)
{
    if (holdsListenerWrite(rttyMarkHz(), hz)) { return; }
    if (m_rttyMarkHz != hz) {
        m_rttyMarkHz = hz;
        emit rttyMarkHzChanged(hz);
    }
}

void SliceModel::setRttyShiftHz(int hz)
{
    if (holdsListenerWrite(rttyShiftHz(), hz)) { return; }
    if (m_rttyShiftHz != hz) {
        m_rttyShiftHz = hz;
        emit rttyShiftHzChanged(hz);
    }
}

// ---------------------------------------------------------------------------
// Per-mode default filter presets
// ---------------------------------------------------------------------------

namespace {

// Porting from Thetis console.cs:18238-18245 and 18256-18263 [v2.10.3.15] — the CWPitch
// setter's loop body, original C# logic: each CWL/CWU preset keeps its
// width and is recentered on the pitch; when that would put an edge across
// zero, the passband slides so the edge sits at zero instead.
// NereusSDR builds the CW slots from the pitch on every call, so the
// recentering is the table itself and this helper is the slide.
//   //-W2PA June 2017
//   //      This centers the passband of the CW filters on the pitch frequency, but if CWPitch setter is called by mode buttons,
//   //      it prevents filter setting from persisting when the mode changes or band changes, since band changes trigger mode changes.
//   //      This happened because of a line:  CWPitch = cw_pitch;  in SetRX1Mode and SetRX2Mode.
//   //      Those are now commented out. This should only be called by the CW Pitch control in the UI and Setup, or by a CAT command.
//   [original inline comments from console.cs:18225-18229]
void slideCwPresetAtImageLimit(DSPMode mode, int& low, int& high)
{
    if (mode == DSPMode::CWL) {
        // Adjust CWL filters  [original inline comment from console.cs:18232]
        if (high > 0) { // stop shifting the passband when it hits the image limit, while allowing pitch to continue to decrease
            low -= high;  // slide the passband down to put its edge at zero
            high = 0;
        }
    } else if (mode == DSPMode::CWU) {
        // Adjust CWU filters  [original inline comment from console.cs:18250]
        if (low < 0) { // stop adjusting the passband when it hits the image limit, while allowing pitch to continue to decrease
            high -= low;  // slide the passband up to put its edge at zero
            low = 0;
        }
    }
}

} // namespace

// Porting from Thetis console.cs:5188-5582 [v2.10.3.15] — InitFilterPresets, F5 per mode.
//
// Filter low/high are in Hz relative to the carrier frequency.
// LSB: negative offsets (passband below carrier)
// USB: positive offsets (passband above carrier)
// AM/SAM/DSB: symmetric around carrier
// CW: centered on cw_pitch (600 Hz from Thetis display.cs:1029 [v2.10.3.15])
// DIGU: centered on digu_click_tune_offset (1500 Hz from Thetis console.cs:14658 [v2.10.3.15])
// DIGL: centered on -digl_click_tune_offset (-2210 Hz from Thetis console.cs:14693 [v2.10.3.15])
std::pair<int, int> SliceModel::defaultFilterForMode(DSPMode mode)
{
    // Phase 3J-1 closeout Item 6 (2026-05-12): read CW pitch from
    // AppSettings instead of hardcoding 600.  Operator-configurable in
    // Thetis (Setup → Keyboard / DSP → CW pitch slider; default 600 Hz);
    // the dedicated NereusSDR setter lands with Phase 3M-2 CW TX, but the
    // read path needs to be in place now so the filter center moves with
    // the setting once that UI ships.  Thetis's CW pitch range is 200..2250 Hz
    // (setup.designer.cs:38264-38273 udDSPCWPitch, console.Designer.cs:1378-1388
    // udCWPitch [v2.10.3.15]); the clamp below still uses 100..2000 and is a
    // later change.
    //
    // From Thetis display.cs:1029 [v2.10.3.15] — cw_pitch default 600.
    auto& s = AppSettings::instance();
    int cwPitch = s.value(QStringLiteral("CWPitch"), 600).toInt();
    if (cwPitch < 100)  { cwPitch = 100;  }
    if (cwPitch > 2000) { cwPitch = 2000; }
    const int kCwPitch = cwPitch;
    // From Thetis console.cs:14658 [v2.10.3.15]
    static constexpr int kDiguOffset = 1500;
    // From Thetis console.cs:14693 [v2.10.3.15]
    // Upstream inline attribution preserved verbatim:
    //   :14669  //reset preset filter's center frequency - W4TME
    static constexpr int kDiglOffset = 2210;

    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5215 [v2.10.3.15] — F5: -3000 to -100, "2.9k"
        return {-3000, -100};
    case DSPMode::USB:
        // From Thetis console.cs:5257 [v2.10.3.15] — F5: 100 to 3000, "2.9k"
        return {100, 3000};
    case DSPMode::DSB:
        // From Thetis console.cs:5551 [v2.10.3.15] — F5: -3300 to 3300, "6.6k"
        return {-3300, 3300};
    case DSPMode::CWL: {
        // From Thetis console.cs:5383 [v2.10.3.15] — F5: -cw_pitch - 200 to -cw_pitch + 200, "400"
        int low  = -kCwPitch - 200;
        int high = -kCwPitch + 200;
        slideCwPresetAtImageLimit(mode, low, high);
        return {low, high};
    }
    case DSPMode::CWU: {
        // From Thetis console.cs:5425 [v2.10.3.15] — F5: cw_pitch - 200 to cw_pitch + 200, "400"
        int low  = kCwPitch - 200;
        int high = kCwPitch + 200;
        slideCwPresetAtImageLimit(mode, low, high);
        return {low, high};
    }
    case DSPMode::FM:
        // FM filters are dynamic in Thetis (from deviation + high cut).
        // Default deviation=5000, so use ±8000 as reasonable default.
        // From Thetis console.cs:7559-7565
        // Upstream inline attribution preserved verbatim (console.cs:7560):
        //   int halfBw = (int)(radio.GetDSPRX(0, 0).RXFMDeviation + radio.GetDSPRX(0, 0).RXFMHighCut);  //[2.10.3.4]MW0LGE
        return {-8000, 8000};
    case DSPMode::AM:
        // From Thetis console.cs:5467 [v2.10.3.15] — F5: -5000 to 5000, "10k"
        return {-5000, 5000};
    case DSPMode::DIGU:
        // Phase 3J-1 closeout Item 4 (2026-05-12): reverted to Thetis F5
        // default (kDiguOffset ± 600 = 900..2100 Hz).  The Phase 3J-1
        // bench fix (commit 624b51c6) widened this to F1 (3 kHz) because
        // setDspMode slammed the default on EVERY mode change, which
        // chopped FT8/FT4 audio when WSJT-X drove band switches via
        // TCI.  With Item 4's per-(band, mode) LastFilter persistence
        // in place, the operator's first widening sticks -- F5 (1 kHz)
        // is now the right Thetis-faithful first-touch default, matching
        // upstream behavior.
        //
        // 2026-10-22: was kDiguOffset ± 600 under a quoted "1.2k" F5 line
        // that no Thetis release carries (Longpath PR 139 found the same).
        //
        // 2026-10-24: NereusSDR's own default, differing from Thetis, which
        // starts DIGU on F5 1.0k. JJ's ruling: most people run FT8 in DIGU,
        // which needs about 3 kHz, so DIGU starts on the F1 3.0k slot. The
        // value is the same expression as the DIGU F1 row of presetsForMode
        // below (kDiguOffset ± 1500 = 0..3000 Hz), so it follows the offset
        // exactly as F1 does. The F1-F10 presets are unchanged.
        return {kDiguOffset - 1500, kDiguOffset + 1500};
    case DSPMode::SPEC:
        // SPEC mode: passthrough, wide filter
        return {-5000, 5000};
    case DSPMode::DIGL:
        // Phase 3J-1 closeout Item 4 (2026-05-12): reverted to Thetis F5
        // default -- see DIGU case above for the full rationale.
        //
        // 2026-10-24: NereusSDR's own default, differing from Thetis, which
        // starts DIGL on F5 1.0k. DIGL starts on the F1 3.0k slot (JJ's
        // ruling, for FT8; see the DIGU case above). The value is the same
        // expression as the DIGL F1 row of presetsForMode below
        // (-kDiglOffset ± 1500 = -3710..-710 Hz at offset 2210).
        return {-kDiglOffset - 1500, -kDiglOffset + 1500};
    case DSPMode::SAM:
        // From Thetis console.cs:5509 [v2.10.3.15] — F5: -5000 to 5000, "10k"
        return {-5000, 5000};
    case DSPMode::DRM:
        // DRM: wide filter similar to AM
        return {-5000, 5000};
    case DSPMode::RADE2_U:
    case DSPMode::RADE_U:
        // Phase 3R Task J1.  RADE Upper sideband: the modem occupies
        // ~650..2350 Hz (1700 Hz wide, centered at +1500 Hz).  This is
        // the SSB-style passband that the panadapter filter window
        // displays and that the TX I/Q routing confines the RADE
        // baseband energy to.  The earlier +/-5000 Hz AM-class window
        // was a placeholder; the filter IS visible on the panadapter
        // AND defines the IF/baseband passband for the modem energy.
        return {650, 2350};
    case DSPMode::RADE2_L:
    case DSPMode::RADE_L:
        // Phase 3R Task J1.  RADE Lower sideband: mirror of RADE-U.
        return {-2350, -650};
    }
    // Fallback
    return {100, 3000};
}

// ---------------------------------------------------------------------------
// Full per-mode filter preset table
// ---------------------------------------------------------------------------

// From Thetis console.cs:5188-5582 [v2.10.3.15] — InitFilterPresets (F1-F10 per mode).
//   // used to initialize all the filter variables
//   [original inline comment from console.cs:5190]
// 2026-10-22: the LSB, USB, DIGL, DIGU, CWL, CWU, AM, SAM and DSB rows below
// were replaced. The earlier rows cited this function but carried values no
// Thetis release has (CW +/-750..+/-6, SSB 3300/2700/600, DIGU +/-3000, AM
// +/-1000/+/-500, DSB 5100..600); Longpath PR 139 (oe5sos/Longpath
// d5eb90b4) reached the same finding. Thetis's button names ("1.0k", "800")
// are not carried here; FilterPresetStore names the slots F1..F10.
// Returns (low_hz, high_hz) pairs in Thetis F1→F10 order. The full list drives
// RxApplet's 10-button filter grid.
QList<std::pair<int, int>> SliceModel::presetsForMode(DSPMode mode)
{
    // Phase 3J-1 closeout Item 6 (2026-05-12): read CW pitch from
    // AppSettings — see defaultFilterForMode() above for the full
    // rationale.  Default 600 Hz from Thetis display.cs:1029 [v2.10.3.15].
    auto& s = AppSettings::instance();
    int cwPitch = s.value(QStringLiteral("CWPitch"), 600).toInt();
    if (cwPitch < 100)  { cwPitch = 100;  }
    if (cwPitch > 2000) { cwPitch = 2000; }
    const int kCwPitch = cwPitch;
    // From Thetis console.cs:14658 [v2.10.3.15]
    static constexpr int kDiguOffset = 1500;
    // From Thetis console.cs:14693 [v2.10.3.15]
    // Upstream tags preserved: //W4TME (from cited console.cs:14669) [v2.10.3.15]
    static constexpr int kDiglOffset = 2210;

    // The CW slots sit on the pitch, as InitFilterPresets puts them and the
    // CWPitch setter recenters them, and slide at the image limit.
    const auto slidCw = [mode](QList<std::pair<int, int>> table) {
        for (auto& p : table) {
            slideCwPresetAtImageLimit(mode, p.first, p.second);
        }
        return table;
    };

    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5199-5240 [v2.10.3.15] — LSB F1-F10
        // ("5.0k" "4.4k" "3.8k" "3.3k" "2.9k" "2.7k" "2.4k" "2.1k" "1.8k" "1.0k")
        return { {-5100,-100}, {-4500,-100}, {-3900,-100}, {-3400,-100},
                 {-3000,-100}, {-2800,-100}, {-2500,-100}, {-2200,-100},
                 {-1900,-100}, {-1100,-100} };
    case DSPMode::USB:
        // From Thetis console.cs:5241-5282 [v2.10.3.15] — USB F1-F10
        // ("5.0k" "4.4k" "3.8k" "3.3k" "2.9k" "2.7k" "2.4k" "2.1k" "1.8k" "1.0k")
        return { {100,5100}, {100,4500}, {100,3900}, {100,3400},
                 {100,3000}, {100,2800}, {100,2500}, {100,2200},
                 {100,1900}, {100,1100} };
    case DSPMode::DSB:
        // From Thetis console.cs:5535-5576 [v2.10.3.15] — DSB F1-F10, symmetric
        // ("16k" "12k" "10k" "8.0k" "6.6k" "5.2k" "4.0k" "3.1k" "2.9k" "2.4k")
        return { {-8000,8000}, {-6000,6000}, {-5000,5000}, {-4000,4000},
                 {-3300,3300}, {-2600,2600}, {-2000,2000}, {-1550,1550},
                 {-1450,1450}, {-1200,1200} };
    case DSPMode::CWL:
        // From Thetis console.cs:5367-5408 [v2.10.3.15] — CWL F1-F10
        // ("1.0k" "800" "600" "500" "400" "250" "150" "100" "50" "25")
        return slidCw({
                 {-kCwPitch - 500, -kCwPitch + 500}, {-kCwPitch - 400, -kCwPitch + 400},
                 {-kCwPitch - 300, -kCwPitch + 300}, {-kCwPitch - 250, -kCwPitch + 250},
                 {-kCwPitch - 200, -kCwPitch + 200}, {-kCwPitch - 125, -kCwPitch + 125},
                 {-kCwPitch - 75,  -kCwPitch + 75},  {-kCwPitch - 50,  -kCwPitch + 50},
                 {-kCwPitch - 25,  -kCwPitch + 25},  {-kCwPitch - 13,  -kCwPitch + 13} });
    case DSPMode::CWU:
        // From Thetis console.cs:5409-5450 [v2.10.3.15] — CWU F1-F10
        // ("1.0k" "800" "600" "500" "400" "250" "150" "100" "50" "25")
        return slidCw({
                 {kCwPitch - 500, kCwPitch + 500}, {kCwPitch - 400, kCwPitch + 400},
                 {kCwPitch - 300, kCwPitch + 300}, {kCwPitch - 250, kCwPitch + 250},
                 {kCwPitch - 200, kCwPitch + 200}, {kCwPitch - 125, kCwPitch + 125},
                 {kCwPitch - 75,  kCwPitch + 75},  {kCwPitch - 50,  kCwPitch + 50},
                 {kCwPitch - 25,  kCwPitch + 25},  {kCwPitch - 13,  kCwPitch + 13} });
    case DSPMode::FM:
        // NereusSDR's own: Thetis has no FM presets. FM falls to the default
        // branch of InitFilterPresets (LastFilter = Filter.NONE,
        // console.cs:5577-5578 [v2.10.3.15]) and SetRX1Mode disables the
        // filter buttons in FM (DisableAllFilters, console.cs:34355
        // [v2.10.3.15]). The earlier "console.cs:5527 region" cite here
        // pointed into the DSB block.
        return { {-8000,8000}, {-6000,6000}, {-4000,4000} };
    case DSPMode::AM:
        // From Thetis console.cs:5451-5492 [v2.10.3.15] — AM F1-F10, symmetric
        // ("20k" "18k" "16k" "12k" "10k" "9.0k" "8.0k" "7.0k" "6.0k" "5.0k")
        return { {-10000,10000}, {-9000,9000}, {-8000,8000}, {-6000,6000},
                 {-5000,5000},   {-4500,4500}, {-4000,4000}, {-3500,3500},
                 {-3000,3000},   {-2500,2500} };
    case DSPMode::DIGU:
        // From Thetis console.cs:5325-5366 [v2.10.3.15] — DIGU F1-F10
        // ("3.0k" "2.5k" "2.0k" "1.5k" "1.0k" "800" "600" "300" "150" "75")
        return { {kDiguOffset - 1500, kDiguOffset + 1500}, {kDiguOffset - 1250, kDiguOffset + 1250},
                 {kDiguOffset - 1000, kDiguOffset + 1000}, {kDiguOffset - 750,  kDiguOffset + 750},
                 {kDiguOffset - 500,  kDiguOffset + 500},  {kDiguOffset - 400,  kDiguOffset + 400},
                 {kDiguOffset - 300,  kDiguOffset + 300},  {kDiguOffset - 150,  kDiguOffset + 150},
                 {kDiguOffset - 75,   kDiguOffset + 75},   {kDiguOffset - 38,   kDiguOffset + 38} };
    case DSPMode::SPEC:
        // Passthrough wideband
        return { {-5000,5000} };
    case DSPMode::DIGL:
        // From Thetis console.cs:5283-5324 [v2.10.3.15] — DIGL F1-F10
        // ("3.0k" "2.5k" "2.0k" "1.5k" "1.0k" "800" "600" "300" "150" "75")
        return { {-kDiglOffset - 1500, -kDiglOffset + 1500}, {-kDiglOffset - 1250, -kDiglOffset + 1250},
                 {-kDiglOffset - 1000, -kDiglOffset + 1000}, {-kDiglOffset - 750,  -kDiglOffset + 750},
                 {-kDiglOffset - 500,  -kDiglOffset + 500},  {-kDiglOffset - 400,  -kDiglOffset + 400},
                 {-kDiglOffset - 300,  -kDiglOffset + 300},  {-kDiglOffset - 150,  -kDiglOffset + 150},
                 {-kDiglOffset - 75,   -kDiglOffset + 75},   {-kDiglOffset - 38,   -kDiglOffset + 38} };
    case DSPMode::SAM:
        // From Thetis console.cs:5493-5534 [v2.10.3.15] — SAM F1-F10, symmetric
        // ("20k" "18k" "16k" "12k" "10k" "9.0k" "8.0k" "7.0k" "6.0k" "5.0k")
        return { {-10000,10000}, {-9000,9000}, {-8000,8000}, {-6000,6000},
                 {-5000,5000},   {-4500,4500}, {-4000,4000}, {-3500,3500},
                 {-3000,3000},   {-2500,2500} };
    case DSPMode::DRM:
        // DRM: wide digital AM-like filters
        return { {-10000,10000}, {-5000,5000} };
    case DSPMode::RADE2_U:
    case DSPMode::RADE_U:
        // Phase 3R Task J1.  RADE Upper sideband: single fixed-bandwidth
        // preset matching the 1700 Hz modem passband.  No F1-F10
        // variants; RADE has a fixed bandwidth per sideband.
        return { {650, 2350} };
    case DSPMode::RADE2_L:
    case DSPMode::RADE_L:
        // Phase 3R Task J1.  RADE Lower sideband: mirror of RADE-U.
        return { {-2350, -650} };
    }
    // Fallback
    return { {100, 3000} };
}

// ---------------------------------------------------------------------------
// Common (compact) per-mode filter preset subset
// ---------------------------------------------------------------------------

// Per Thetis main-panel filter buttons. Subset of presetsForMode() — 5-6 entries per mode
// for VfoWidget's compact flag context. Values match Thetis console.cs F-button layout.
QList<std::pair<int, int>> SliceModel::commonPresetsForMode(DSPMode mode)
{
    switch (mode) {
    case DSPMode::USB:
        return {{100,2400}, {100,2700}, {100,2900}, {100,3000}, {100,3200}};
    case DSPMode::LSB:
        return {{-2400,-100}, {-2700,-100}, {-2900,-100}, {-3000,-100}, {-3200,-100}};
    case DSPMode::CWU:
    case DSPMode::CWL: {
        // Phase 3J-1 closeout Item 6 (2026-05-12): read CW pitch from
        // AppSettings.  See defaultFilterForMode() for the full rationale.
        // From Thetis display.cs:1029 [v2.10.3.15] — default 600.
        auto& s = AppSettings::instance();
        int cwPitch = s.value(QStringLiteral("CWPitch"), 600).toInt();
        if (cwPitch < 100)  { cwPitch = 100;  }
        if (cwPitch > 2000) { cwPitch = 2000; }
        const int kCwPitch = cwPitch;
        const int sign = (mode == DSPMode::CWL) ? -1 : 1;
        return { {sign*(kCwPitch-50),  sign*(kCwPitch+50)},
                 {sign*(kCwPitch-100), sign*(kCwPitch+100)},
                 {sign*(kCwPitch-150), sign*(kCwPitch+150)},
                 {sign*(kCwPitch-250), sign*(kCwPitch+250)},
                 {sign*(kCwPitch-500), sign*(kCwPitch+500)} };
    }
    case DSPMode::AM:
    case DSPMode::SAM:
        return {{-2900,2900}, {-3500,3500}, {-5000,5000}};
    case DSPMode::FM:
        return {{-3000,3000}, {-5000,5000}, {-8000,8000}};
    case DSPMode::DIGU:
    case DSPMode::DIGL: {
        // From Thetis console.cs:14636,14671 [v2.10.3.13]
        //reset preset filter's center frequency - W4TME  [original inline comment from console.cs:14647,14682]
        const int o = (mode == DSPMode::DIGU) ? 1500 : 2210;
        const int sign = (mode == DSPMode::DIGL) ? -1 : 1;
        return { {sign*(o-1350), sign*(o+1350)},
                 {sign*(o-1450), sign*(o+1450)},
                 {sign*(o-1500), sign*(o+1500)},
                 {sign*(o-1650), sign*(o+1650)},
                 {sign*(o-1750), sign*(o+1750)} };
    }
    case DSPMode::DSB:
        return {{-2900,2900}, {-3500,3500}};
    case DSPMode::DRM:
        return {{-5000,5000}};
    default:
        return {{100, 3000}};
    }
}

// ---------------------------------------------------------------------------
// Mode name utilities
// ---------------------------------------------------------------------------

QString SliceModel::modeName(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:  return QStringLiteral("LSB");
    case DSPMode::USB:  return QStringLiteral("USB");
    case DSPMode::DSB:  return QStringLiteral("DSB");
    case DSPMode::CWL:  return QStringLiteral("CWL");
    case DSPMode::CWU:  return QStringLiteral("CWU");
    case DSPMode::FM:   return QStringLiteral("FM");
    case DSPMode::AM:   return QStringLiteral("AM");
    case DSPMode::DIGU: return QStringLiteral("DIGU");
    case DSPMode::SPEC: return QStringLiteral("SPEC");
    case DSPMode::DIGL: return QStringLiteral("DIGL");
    case DSPMode::SAM:  return QStringLiteral("SAM");
    case DSPMode::DRM:  return QStringLiteral("DRM");
    // Phase 3R Task J1.  NereusSDR-native; not WDSP modes.  Split
    // into upper/lower sidebands like USB/LSB.
    case DSPMode::RADE_U: return QStringLiteral("RADE-U");
    case DSPMode::RADE2_U: return QStringLiteral("RADE2-U");
    case DSPMode::RADE_L: return QStringLiteral("RADE-L");
    case DSPMode::RADE2_L: return QStringLiteral("RADE2-L");
    }
    return QStringLiteral("USB");
}

DSPMode SliceModel::modeFromName(const QString& name)
{
    if (name == QLatin1String("LSB"))  return DSPMode::LSB;
    if (name == QLatin1String("USB"))  return DSPMode::USB;
    if (name == QLatin1String("DSB"))  return DSPMode::DSB;
    if (name == QLatin1String("CWL"))  return DSPMode::CWL;
    if (name == QLatin1String("CWU"))  return DSPMode::CWU;
    if (name == QLatin1String("FM"))   return DSPMode::FM;
    if (name == QLatin1String("AM"))   return DSPMode::AM;
    if (name == QLatin1String("DIGU")) return DSPMode::DIGU;
    if (name == QLatin1String("SPEC")) return DSPMode::SPEC;
    if (name == QLatin1String("DIGL")) return DSPMode::DIGL;
    if (name == QLatin1String("SAM"))  return DSPMode::SAM;
    if (name == QLatin1String("DRM"))  return DSPMode::DRM;
    // Phase 3R Task J1.  NereusSDR-native; not WDSP modes.
    if (name == QLatin1String("RADE2-U")) { return DSPMode::RADE2_U; }
    if (name == QLatin1String("RADE2-L")) { return DSPMode::RADE2_L; }
    if (name == QLatin1String("RADE-U")) return DSPMode::RADE_U;
    if (name == QLatin1String("RADE-L")) return DSPMode::RADE_L;
    // Legacy migration: pre-fix builds persisted the singular "RADE"
    // string before the sideband split landed.  Map it to RADE_U so
    // existing per-MAC persisted slice modes keep working on upgrade.
    if (name == QLatin1String("RADE")) return DSPMode::RADE_U;
    return DSPMode::USB;
}

// ---------------------------------------------------------------------------
// Per-slice-per-band persistence (Phase 3G-10 Stage 2 — S2.P)
// ---------------------------------------------------------------------------
//
// Key layout:
//   Per-band DSP: Slice<N>/Band<key>/<Field>   (varies by band)
//   Session state: Slice<N>/<Field>             (band-agnostic)
//
// <N> comes from m_sliceIndex. <key> comes from bandKeyName(band).

namespace {

// Build the per-band prefix string, e.g. "Slice0/Band20m/".
QString bandPrefix(int sliceIndex, Band band)
{
    return QStringLiteral("Slice%1/Band%2/")
               .arg(sliceIndex)
               .arg(bandKeyName(band));
}

// Phase 3J-1 closeout Item 4 (2026-05-12): build the per-(band, mode)
// prefix, e.g. "Slice0/Band20m/ModeUSB/".  Used by setDspMode + saveTo
// Settings + restoreFromSettings to persist the filter cutoffs under
// (slice, band, mode) instead of the legacy (slice, band) tuple, so a
// mode change inside a band restores the operator's previously-set
// filter for THAT mode rather than slamming to defaultFilterForMode.
//
// Mirrors Thetis's preset[m].LastFilter machinery (console.cs:14653-
// 14671 [v2.10.3.13]) where each (band, mode) pair has its own remembered
// filter slot.
QString bandModePrefix(int sliceIndex, Band band, DSPMode mode)
{
    return QStringLiteral("Slice%1/Band%2/Mode%3/")
               .arg(sliceIndex)
               .arg(bandKeyName(band))
               .arg(SliceModel::modeName(mode));
}

// Build the session-state prefix string, e.g. "Slice0/".
QString slicePrefix(int sliceIndex)
{
    return QStringLiteral("Slice%1/").arg(sliceIndex);
}

// Boolean → AppSettings canonical string.
QString boolStr(bool v) { return v ? QStringLiteral("True") : QStringLiteral("False"); }

} // namespace

bool SliceModel::restoreReceiveState(double frequencyHz, DSPMode mode)
{
    // ReceiveLayoutStore validates descriptors before they reach this seam,
    // but preserve the admission boundary here because this is public API.
    if (!std::isfinite(frequencyHz)
        || frequencyHz < kMinReceiveFrequencyHz
        || frequencyHz > kMaxReceiveFrequencyHz
        || !MirrorEnumDomain::contains(QMetaType::fromType<DSPMode>(),
                                       static_cast<int>(mode))) {
        return false;
    }

    // A SliceModel alone cannot establish Local ownership or prove that no
    // DSP resources exist.  The restore seam is deliberately Local-only,
    // including for tests.
    auto* radio = qobject_cast<RadioModel*>(parent());
    if (radio == nullptr || radio->role() != RadioModel::Role::Local
        || radio->connection() != nullptr) {
        return false;
    }

    WdspEngine* const engine = radio->wdspEngine();
    if (engine == nullptr || engine->isInitialized()) {
        return false;
    }
    for (int channelId = WdspEngine::kFirstSliceChannelId;
         channelId < WdspEngine::kMaxSliceChannels; ++channelId) {
        if (engine->rxChannel(channelId) != nullptr
            || engine->radeChannel(channelId) != nullptr) {
            return false;
        }
    }

    const Band manifestBand = bandFromFrequency(frequencyHz);
    auto& settings = AppSettings::instance();
    const QString legacyPrefix = bandPrefix(m_sliceIndex, manifestBand);
    bool legacyModeMatchesManifest = false;
    if (settings.contains(legacyPrefix + QStringLiteral("DspMode"))) {
        bool validInteger = false;
        const int legacyModeValue = settings.value(
            legacyPrefix + QStringLiteral("DspMode")).toInt(&validInteger);
        legacyModeMatchesManifest = validInteger
            && MirrorEnumDomain::contains(QMetaType::fromType<DSPMode>(), legacyModeValue)
            && legacyModeValue == static_cast<int>(mode);
    }
    const QSignalBlocker blockSignals(this);

    // restoreFromSettings assigns the legacy mode directly, so it is safe
    // before DSP admission.  It also restores per-MAC NNR through its stable
    // slice identity.  The offline gate above is required because an NNR
    // applier may otherwise reach a live RxChannel.
    {
        // No radio and no DSP yet: the restored layout starts its RADE
        // decoder at admission (RadioModel::activateRestoredRadeReceiveOwner).
        const QScopedValueRollback<bool> deferRade(m_radeStartDeferredToAdmission, true);
        restoreFromSettings(manifestBand);
    }

    // The layout manifest is authoritative for tuning.  Do not call the
    // setters: a saved Locked flag must not veto the seed, and setDspMode()
    // can create a RADE channel.
    m_frequency = frequencyHz;
    m_currentBand = manifestBand;
    m_dspMode = mode;
    if (isRade2DspMode(mode) && !radio->radeV2Enabled()) {
        radio->beginRadeStart(m_sliceIndex);
        radio->endRadeStart(m_sliceIndex, RadioModel::RadeStartFault::None);
    }

    // restoreFromSettings already made the full legacy (mode-specific, then
    // band-level) filter selection when its valid stored mode matches this
    // manifest mode.  Otherwise the legacy filter could belong to another
    // mode, or have no compatible mode sentinel at all, so use only the
    // manifest-mode half of setDspMode's read path and its default fallback.
    if (!legacyModeMatchesManifest) {
        const QString modePrefix = bandModePrefix(m_sliceIndex, manifestBand, mode);
        if (settings.contains(modePrefix + QStringLiteral("FilterLow"))
            && settings.contains(modePrefix + QStringLiteral("FilterHigh"))) {
            m_filterLow = settings.value(modePrefix + QStringLiteral("FilterLow")).toInt();
            m_filterHigh = settings.value(modePrefix + QStringLiteral("FilterHigh")).toInt();
        } else {
            const auto filter = defaultFilterForMode(mode);
            m_filterLow = filter.first;
            m_filterHigh = filter.second;
        }
    }
    return true;
}

// DspMode is the sentinel: if present under the per-band namespace,
// the band is treated as visited. Alternatives (Frequency, FilterLow)
// are written by migrateLegacyKeys() even when the upstream VfoDspMode
// key was absent, so they'd report "visited" for a band the user has
// never intentionally configured on the mode side. Using DspMode matches
// the semantic we want for the #118 band-click handler: "has this band
// been configured, not just visited."
bool SliceModel::hasSettingsFor(Band band) const
{
    auto& s = AppSettings::instance();
    return s.contains(bandPrefix(m_sliceIndex, band) + QStringLiteral("DspMode"));
}

int SliceModel::savedSampleRateHz(Band band) const
{
    auto& s = AppSettings::instance();
    const QString key = bandPrefix(m_sliceIndex, band) + QStringLiteral("SampleRate");
    if (!s.contains(key)) {
        return 0;
    }
    bool ok = false;
    const int rate = s.value(key).toInt(&ok);
    return ok && rate > 0 ? rate : 0;
}

QList<QPair<QString, QVariant>> SliceModel::captureBandSettings(Band band) const
{
    QList<QPair<QString, QVariant>> values;
    const QString bp = bandPrefix(m_sliceIndex, band);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    values.append({bp + QStringLiteral("Frequency"),    m_frequency});
    values.append({bp + QStringLiteral("AgcThreshold"), m_agcThreshold});
    values.append({bp + QStringLiteral("AgcHang"),      m_agcHang});
    values.append({bp + QStringLiteral("AgcSlope"),     m_agcSlope});
    values.append({bp + QStringLiteral("AgcAttack"),    m_agcAttack});
    values.append({bp + QStringLiteral("AgcDecay"),     m_agcDecay});
    values.append({bp + QStringLiteral("AgcFixedGain"), m_agcFixedGain});
    values.append({bp + QStringLiteral("AgcHangThreshold"), m_agcHangThreshold});
    values.append({bp + QStringLiteral("AgcMaxGain"),   m_agcMaxGain});
    values.append({bp + QStringLiteral("FilterLow"),    m_filterLow});
    values.append({bp + QStringLiteral("FilterHigh"),   m_filterHigh});
    // Phase 3J-1 closeout Item 4 (2026-05-12): ALSO persist filter under
    // (band, currentMode) so a future mode change can restore it.  Legacy
    // (band)/FilterLow stays for backward compat with code that reads it
    // directly without going through restoreFromSettings.
    {
        const QString bmp = bandModePrefix(m_sliceIndex, band, m_dspMode);
        values.append({bmp + QStringLiteral("FilterLow"),  m_filterLow});
        values.append({bmp + QStringLiteral("FilterHigh"), m_filterHigh});
    }
    values.append({bp + QStringLiteral("DspMode"),      static_cast<int>(m_dspMode)});
    values.append({bp + QStringLiteral("AgcMode"),      static_cast<int>(m_agcMode)});
    values.append({bp + QStringLiteral("StepHz"),       m_stepHz});

    // Noise-blanker mode (per-band). Tri-state Off/NB/NB2 mirrors Thetis
    // chkNB state per-receiver. NB TUNING (threshold / tau / lag / lead) is
    // NOT per-band in Thetis and lives globally inside NbFamily — see
    // SliceModel.h for the 2026-04-22 removal note.
    values.append({bp + QStringLiteral("NbMode"), static_cast<int>(m_nbMode)});

    // Phase 3F: per-slice DDC sample rate, persisted per-band so each band
    // can independently remember its preferred rate (e.g. 192 kHz on 40m,
    // 1536 kHz on 10m for a wider pan). NereusSDR-original (no Thetis cite).
    values.append({bp + QStringLiteral("SampleRate"), m_sampleRateHz});

    // Phase 3F Sub-Epic G Task 2: per-band diversity tuning. The 8-memory
    // slots (T3) + direction-finding fields (T11) join this block when they
    // ship. NereusSDR-original schema (Thetis persists diversity globally
    // in DSP.console.dsp / Diversity.cs; we scope per-band per-slice so
    // operators can keep distinct DF setups across bands).
    values.append({bp + QStringLiteral("DiversityPhaseDeg"), m_diversityPhaseDeg});
    values.append({bp + QStringLiteral("DiversityGainDb"), m_diversityGainDb});
    values.append({bp + QStringLiteral("DiversityFineNullEnabled"),
               boolStr(m_diversityFineNullEnabled)});

    return values;
}

void SliceModel::saveToSettings(Band band)
{
    saveNnrSettings();
    auto& s = AppSettings::instance();
    const QString sp = slicePrefix(m_sliceIndex);
    for (const QPair<QString, QVariant>& value : captureBandSettings(band)) {
        s.setValue(value.first, value.second);
    }

    // ── Session state (band-agnostic) ─────────────────────────────────────────
    // NR active slot + tuning — session-level only, no per-band suffix.
    // Per user directive Q10: no band suffix on NR keys.
    s.setValue(sp + QStringLiteral("NrActive"),        static_cast<int>(m_activeNr));
    // NR1
    s.setValue(sp + QStringLiteral("Nr1Taps"),         m_nr1Taps);
    s.setValue(sp + QStringLiteral("Nr1Delay"),        m_nr1Delay);
    s.setValue(sp + QStringLiteral("Nr1Gain"),         m_nr1Gain);
    s.setValue(sp + QStringLiteral("Nr1Leakage"),      m_nr1Leakage);
    s.setValue(sp + QStringLiteral("Nr1Position"),     static_cast<int>(m_nr1Position));
    // NR2
    s.setValue(sp + QStringLiteral("Nr2GainMethod"),   static_cast<int>(m_nr2GainMethod));
    s.setValue(sp + QStringLiteral("Nr2NpeMethod"),    static_cast<int>(m_nr2NpeMethod));
    s.setValue(sp + QStringLiteral("Nr2TrainT1"),      m_nr2TrainT1);
    s.setValue(sp + QStringLiteral("Nr2TrainT2"),      m_nr2TrainT2);
    s.setValue(sp + QStringLiteral("Nr2AeFilter"),     boolStr(m_nr2AeFilter));
    s.setValue(sp + QStringLiteral("Nr2Position"),     static_cast<int>(m_nr2Position));
    s.setValue(sp + QStringLiteral("Nr2Post2Run"),     boolStr(m_nr2Post2Run));
    s.setValue(sp + QStringLiteral("Nr2Post2Level"),   m_nr2Post2Level);
    s.setValue(sp + QStringLiteral("Nr2Post2Factor"),  m_nr2Post2Factor);
    s.setValue(sp + QStringLiteral("Nr2Post2Rate"),    m_nr2Post2Rate);
    s.setValue(sp + QStringLiteral("Nr2Post2Taper"),   m_nr2Post2Taper);
    // NR3
    s.setValue(sp + QStringLiteral("Nr3Position"),     static_cast<int>(m_nr3Position));
    s.setValue(sp + QStringLiteral("Nr3UseDefaultGain"), boolStr(m_nr3UseDefaultGain));
    // NR4
    s.setValue(sp + QStringLiteral("Nr4Reduction"),    m_nr4Reduction);
    s.setValue(sp + QStringLiteral("Nr4Smoothing"),    m_nr4Smoothing);
    s.setValue(sp + QStringLiteral("Nr4Whitening"),    m_nr4Whitening);
    s.setValue(sp + QStringLiteral("Nr4Rescale"),      m_nr4Rescale);
    s.setValue(sp + QStringLiteral("Nr4PostThresh"),   m_nr4PostThresh);
    s.setValue(sp + QStringLiteral("Nr4Algo"),         static_cast<int>(m_nr4Algo));
    // DFNR
    s.setValue(sp + QStringLiteral("DfnrAttenLimit"),     m_dfnrAttenLimit);
    s.setValue(sp + QStringLiteral("DfnrPostFilterBeta"), m_dfnrPostFilterBeta);
    // BNR + MNR
    s.setValue(sp + QStringLiteral("BnrStrength"),     m_bnrStrength);
    s.setValue(sp + QStringLiteral("MnrStrength"),     m_mnrStrength);
    s.setValue(sp + QStringLiteral("MnrOversub"),      m_mnrOversub);
    s.setValue(sp + QStringLiteral("MnrFloor"),        m_mnrFloor);
    s.setValue(sp + QStringLiteral("MnrAlpha"),        m_mnrAlpha);
    s.setValue(sp + QStringLiteral("MnrBias"),         m_mnrBias);
    s.setValue(sp + QStringLiteral("MnrGsmooth"),      m_mnrGsmooth);

    s.setValue(sp + QStringLiteral("SnbEnabled"), boolStr(m_snbEnabled));
    s.setValue(sp + QStringLiteral("AnfEnabled"), boolStr(m_anfEnabled));
    // Squelch, saved per receiver as Thetis does.
    // From Thetis console.cs:2830-2852 [v2.10.3.15]:
    //   a.Add("chkSquelch_checkstate/" + chkSquelch.CheckState.ToString());
    //   a.Add("rx1_squelch_state/" + _rx1_squelch_state.ToString());
    //   a.Add("rx1_fm_squelch_state/" + _rx1_fm_squelch_state.ToString());
    //   a.Add("rx1_squelch_threshold_scroll/" + rx1_squelch_threshold_scroll);
    //   a.Add("rx1_fm_squelch_threshold_scroll/" + rx1_fm_squelch_threshold_scroll);
    //   a.Add("rx1_voice_squelch_threshold_scroll/" + rx1_voice_squelch_threshold_scroll);
    //   (and the rx2 equivalents)
    // Thetis keeps one squelch state per receiver (OFF/SQL/VSQL) plus a
    // separate FM state, and three thresholds. NereusSDR keeps one enable
    // and one threshold per squelch kind on each slice, so the mapping is:
    // the VSQL state and voice threshold are SsqlEnabled / SsqlThresh, the
    // SQL state and squelch threshold are AmsqEnabled / AmsqThresh, and the
    // FM state and FM threshold are FmsqEnabled / FmsqThresh. Like Thetis
    // they are per receiver (slice), not per band.
    s.setValue(sp + QStringLiteral("SsqlEnabled"), boolStr(m_ssqlEnabled));
    s.setValue(sp + QStringLiteral("SsqlThresh"),  m_ssqlThresh);
    s.setValue(sp + QStringLiteral("AmsqEnabled"), boolStr(m_amsqEnabled));
    s.setValue(sp + QStringLiteral("AmsqThresh"),  m_amsqThresh);
    s.setValue(sp + QStringLiteral("FmsqEnabled"), boolStr(m_fmsqEnabled));
    s.setValue(sp + QStringLiteral("FmsqThresh"),  m_fmsqThresh);
    // NB1 / NB2 / SNB detailed tuning. Per slice per band like everything
    // else here, even though NB1 and NB2 behave as stream-shared while two
    // slices are co-hosted: the mirror lives in RadioModel and only applies
    // while they share a DDC, so slices that later separate must still have
    // their own stored values to go back to.
    s.setValue(sp + QStringLiteral("Nb1Threshold"),    m_nb1Threshold);
    s.setValue(sp + QStringLiteral("Nb1TransitionMs"), m_nb1TransitionMs);
    s.setValue(sp + QStringLiteral("Nb1LeadMs"),       m_nb1LeadMs);
    s.setValue(sp + QStringLiteral("Nb1LagMs"),        m_nb1LagMs);
    s.setValue(sp + QStringLiteral("Nb2Mode"),         m_nb2Mode);
    s.setValue(sp + QStringLiteral("SnbK1"),           m_snbK1);
    s.setValue(sp + QStringLiteral("SnbK2"),           m_snbK2);
    s.setValue(sp + QStringLiteral("SnbOutputBandwidthHz"), m_snbOutputBandwidthHz);
    s.setValue(sp + QStringLiteral("Locked"),     boolStr(m_locked));
    s.setValue(sp + QStringLiteral("Muted"),      boolStr(m_muted));
    s.setValue(sp + QStringLiteral("RitEnabled"), boolStr(m_ritEnabled));
    s.setValue(sp + QStringLiteral("RitHz"),      m_ritHz);
    s.setValue(sp + QStringLiteral("XitEnabled"), boolStr(m_xitEnabled));
    s.setValue(sp + QStringLiteral("XitHz"),      m_xitHz);
    s.setValue(sp + QStringLiteral("AfGain"),     m_afGain);
    s.setValue(sp + QStringLiteral("RfGain"),     m_rfGain);
    s.setValue(sp + QStringLiteral("RxAntenna"),  m_rxAntenna);
    s.setValue(sp + QStringLiteral("TxAntenna"),  m_txAntenna);

    // Audio panel, squelch, APF and the mode-specific offsets. Per slice
    // (no band suffix), like Muted and AfGain beside them: each is a
    // receiver's setting, not a band's. NereusSDR-original keys; they were
    // not saved at all before 2026-10-11, so every one came back at its
    // default whenever the slice was made again.
    s.setValue(sp + QStringLiteral("AudioPan"),        m_audioPan);
    s.setValue(sp + QStringLiteral("BinauralEnabled"), boolStr(m_binauralEnabled));
    s.setValue(sp + QStringLiteral("SsqlEnabled"),     boolStr(m_ssqlEnabled));
    s.setValue(sp + QStringLiteral("SsqlThresh"),      m_ssqlThresh);
    s.setValue(sp + QStringLiteral("AmsqEnabled"),     boolStr(m_amsqEnabled));
    s.setValue(sp + QStringLiteral("AmsqThresh"),      m_amsqThresh);
    s.setValue(sp + QStringLiteral("FmsqEnabled"),     boolStr(m_fmsqEnabled));
    s.setValue(sp + QStringLiteral("FmsqThresh"),      m_fmsqThresh);
    s.setValue(sp + QStringLiteral("ApfEnabled"),      boolStr(m_apfEnabled));
    s.setValue(sp + QStringLiteral("ApfTuneHz"),       m_apfTuneHz);
    s.setValue(sp + QStringLiteral("FmCtcssMode"),     m_fmCtcssMode);
    s.setValue(sp + QStringLiteral("FmCtcssValueHz"),  m_fmCtcssValueHz);
    s.setValue(sp + QStringLiteral("FmOffsetHz"),      m_fmOffsetHz);
    s.setValue(sp + QStringLiteral("FmTxMode"),        static_cast<int>(m_fmTxMode));
    s.setValue(sp + QStringLiteral("FmReverse"),       boolStr(m_fmReverse));
    s.setValue(sp + QStringLiteral("DiglOffsetHz"),    m_diglOffsetHz);
    s.setValue(sp + QStringLiteral("DiguOffsetHz"),    m_diguOffsetHz);
    s.setValue(sp + QStringLiteral("RttyMarkHz"),      m_rttyMarkHz);
    s.setValue(sp + QStringLiteral("RttyShiftHz"),     m_rttyShiftHz);
    // AGC-T AUTO and its offset are the receiver's, not the band's (the
    // maintainer's ruling 2026-10-11): Slice<N>/AgcAutoEnabled and
    // Slice<N>/AgcAutoOffset, no band part. Thetis keeps one switch and one
    // offset per receiver as well.
    // From Thetis console.cs:45992-46033 [v2.10.3.15] (AutoAGCRX1/RX2) and
    // setup.cs:21995-22026 [v2.10.3.15] (chkAutoAGCRX1/RX2, udRX1/RX2AutoAGCOffset).
    // The per-band keys an older build wrote are read once as a fallback
    // (restoreFromSettings) and no longer written. Written here, not in
    // captureBandSettings, since they carry no band part.
    s.setValue(sp + QStringLiteral("AgcAutoEnabled"), m_autoAgcEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(sp + QStringLiteral("AgcAutoOffset"), m_autoAgcOffset);

    // Track the most recently saved band so RadioModel::loadSliceState() on
    // the next launch can land on the user's actual last-used frequency,
    // not the panadapter's 14.225 MHz default. Per-band Frequency keys
    // already store the per-band freq; this just records "which band was
    // active last." Read via SliceModel::loadLastBandFromSettings().
    s.setValue(sp + QStringLiteral("LastBand"), bandKeyName(band));
}

void SliceModel::restoreFromSettings(Band band)
{
    // Slice control plan Task 5: a band's saved state is a change a
    // listened slice holds back (it would go to the Core as writes).
    if (holdForListener()) { return; }
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    // Each key: if absent, leave the current SliceModel default unchanged.

    if (s.contains(bp + QStringLiteral("Frequency"))) {
        setFrequency(s.value(bp + QStringLiteral("Frequency")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("AgcThreshold"))) {
        setAgcThreshold(s.value(bp + QStringLiteral("AgcThreshold")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcHang"))) {
        setAgcHang(s.value(bp + QStringLiteral("AgcHang")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcSlope"))) {
        setAgcSlope(s.value(bp + QStringLiteral("AgcSlope")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcAttack"))) {
        setAgcAttack(s.value(bp + QStringLiteral("AgcAttack")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcDecay"))) {
        setAgcDecay(s.value(bp + QStringLiteral("AgcDecay")).toInt());
    }
    // AGC-T AUTO and its offset: the slice's own keys (per receiver, the
    // maintainer's ruling 2026-10-11). A file from an older build has them
    // under the band instead; that value is read when the slice has none,
    // and the next save writes the slice's key, so nobody's saved state is
    // lost. Still after AgcThreshold above: RadioModel turns AUTO off on a
    // manual threshold change, and the restored threshold is not one.
    if (s.contains(sp + QStringLiteral("AgcAutoEnabled"))) {
        setAutoAgcEnabled(s.value(sp + QStringLiteral("AgcAutoEnabled")).toString() == QLatin1String("True"));
    } else if (s.contains(bp + QStringLiteral("AgcAutoEnabled"))) {
        setAutoAgcEnabled(s.value(bp + QStringLiteral("AgcAutoEnabled")).toString() == QLatin1String("True"));
    }
    // The offset goes into the threshold the AUTO tick pushes to WDSP, so a
    // value that is not a finite number leaves the default, and the rest is
    // held to what the control allows: -60 to +60 dB
    // (src/gui/setup/DspSetupPages.cpp:445, m_autoAgcOffset->setRange).
    const auto restoreAutoAgcOffset = [this, &s](const QString& key) {
        constexpr double kAutoAgcOffsetMinDb = -60.0;
        constexpr double kAutoAgcOffsetMaxDb = 60.0;
        bool isNumber = false;
        const double offset = s.value(key).toString().toDouble(&isNumber);
        if (isNumber && std::isfinite(offset)) {
            setAutoAgcOffset(std::clamp(offset, kAutoAgcOffsetMinDb, kAutoAgcOffsetMaxDb));
        }
    };
    if (s.contains(sp + QStringLiteral("AgcAutoOffset"))) {
        restoreAutoAgcOffset(sp + QStringLiteral("AgcAutoOffset"));
    } else if (s.contains(bp + QStringLiteral("AgcAutoOffset"))) {
        restoreAutoAgcOffset(bp + QStringLiteral("AgcAutoOffset"));
    }
    if (s.contains(bp + QStringLiteral("AgcFixedGain"))) {
        setAgcFixedGain(s.value(bp + QStringLiteral("AgcFixedGain")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcHangThreshold"))) {
        setAgcHangThreshold(s.value(bp + QStringLiteral("AgcHangThreshold")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcMaxGain"))) {
        setAgcMaxGain(s.value(bp + QStringLiteral("AgcMaxGain")).toInt());
    }
    bool restoreFilter = true;
    if (s.contains(bp + QStringLiteral("DspMode"))) {
        // Set mode WITHOUT applying the default filter — filter follows below.
        // We must update m_dspMode before reading FilterLow/FilterHigh so
        // the final setFilter call is not superseded by setDspMode's default.
        DSPMode mode = static_cast<DSPMode>(
            s.value(bp + QStringLiteral("DspMode")).toInt());
        // Directly assign mode without calling setDspMode() (which also
        // resets the filter). Emit the signal manually to keep observers in sync.
        if (m_dspMode != mode) {
            const DSPMode oldMode = m_dspMode;
            m_dspMode = mode;
            ++m_modeRevision;
            ++m_modeFilterRevision;
            // RADE threads (2026-09-30): the same RADE decoder start and
            // stop as setDspMode. A band saved in RADE, or a device's slice
            // made again from its saved band, otherwise read RADE and played
            // the sideband's audio with no decoder (and a band saved in SSB
            // kept a RADE slice's decoder).
            const QPointer<SliceModel> lifetime(this);
            const quint64 revision = m_modeRevision;
            const quint64 filterRevision = m_modeFilterRevision;
            applyRadeModeChange(oldMode, mode);
            if (!lifetime) { return; }
            // A newer mode or filter change owns the filter; the rest of
            // the band still restores.
            if (revision != m_modeRevision) {
                restoreFilter = false;
            } else {
                emit dspModeChanged(mode);
                if (!lifetime) { return; }
                restoreFilter = filterRevision == m_modeFilterRevision;
            }
        }
    }
    // Phase 3J-1 closeout Item 4 (2026-05-12): prefer (band, currentMode)
    // filter when persisted; fall back to legacy (band)/FilterLow/High
    // for pre-Item-4 settings files.  m_dspMode was set above (line ~1491)
    // before reaching this restore block, so it reflects the destination
    // mode for the band restore.
    if (restoreFilter) {
        const QString bmp = bandModePrefix(m_sliceIndex, band, m_dspMode);
        if (s.contains(bmp + QStringLiteral("FilterLow")) &&
            s.contains(bmp + QStringLiteral("FilterHigh"))) {
            setFilter(s.value(bmp + QStringLiteral("FilterLow")).toInt(),
                      s.value(bmp + QStringLiteral("FilterHigh")).toInt());
        } else if (s.contains(bp + QStringLiteral("FilterLow")) &&
                   s.contains(bp + QStringLiteral("FilterHigh"))) {
            setFilter(s.value(bp + QStringLiteral("FilterLow")).toInt(),
                      s.value(bp + QStringLiteral("FilterHigh")).toInt());
        }
    }
    if (s.contains(bp + QStringLiteral("AgcMode"))) {
        setAgcMode(static_cast<AGCMode>(
            s.value(bp + QStringLiteral("AgcMode")).toInt()));
    }
    if (s.contains(bp + QStringLiteral("StepHz"))) {
        setStepHz(s.value(bp + QStringLiteral("StepHz")).toInt());
    }

    // Noise blanker mode (per-band). Tuning keys (NbThreshold/NbTauMs/
    // NbLeadMs/NbLagMs) from an earlier pre-2026-04-22 schema are ignored
    // if present — they'll be overwritten on next save. Per-band NB tuning
    // is not a Thetis concept.
    if (s.contains(bp + QStringLiteral("NbMode"))) {
        setNbMode(static_cast<NereusSDR::NbMode>(
            s.value(bp + QStringLiteral("NbMode")).toInt()));
    }

    // Phase 3F: per-slice DDC sample rate (per-band). Default 192000 when key
    // absent (new install or pre-3F settings file).
    if (s.contains(bp + QStringLiteral("SampleRate"))) {
        setSampleRateHz(s.value(bp + QStringLiteral("SampleRate")).toInt());
    }

    // Phase 3F Sub-Epic G Task 2: per-band diversity tuning. Defaults match
    // the SliceModel member-init values (0.0 deg, 0.0 dB, fine-null off) so
    // absent keys leave the slice in the passive reference-receiver state.
    if (s.contains(bp + QStringLiteral("DiversityPhaseDeg"))) {
        setDiversityPhaseDeg(
            s.value(bp + QStringLiteral("DiversityPhaseDeg")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("DiversityGainDb"))) {
        setDiversityGainDb(
            s.value(bp + QStringLiteral("DiversityGainDb")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("DiversityFineNullEnabled"))) {
        setDiversityFineNullEnabled(
            s.value(bp + QStringLiteral("DiversityFineNullEnabled")).toString()
            == QLatin1String("True"));
    }

    // ── Session state (band-agnostic) ─────────────────────────────────────────
    // NR active slot + tuning (no per-band suffix, per user directive Q10).
    if (m_settingsRadioMac.isEmpty() && s.contains(sp + QStringLiteral("NrActive"))) {
        setActiveNr(static_cast<NereusSDR::NrSlot>(s.value(sp + QStringLiteral("NrActive")).toInt()));
    }
    // NR1
    if (s.contains(sp + QStringLiteral("Nr1Taps"))) {
        setNr1Taps(s.value(sp + QStringLiteral("Nr1Taps")).toInt());
    }
    if (s.contains(sp + QStringLiteral("Nr1Delay"))) {
        setNr1Delay(s.value(sp + QStringLiteral("Nr1Delay")).toInt());
    }
    if (s.contains(sp + QStringLiteral("Nr1Gain"))) {
        setNr1Gain(s.value(sp + QStringLiteral("Nr1Gain")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr1Leakage"))) {
        setNr1Leakage(s.value(sp + QStringLiteral("Nr1Leakage")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr1Position"))) {
        setNr1Position(static_cast<NereusSDR::NrPosition>(s.value(sp + QStringLiteral("Nr1Position")).toInt()));
    }
    // NR2
    if (s.contains(sp + QStringLiteral("Nr2GainMethod"))) {
        setNr2GainMethod(static_cast<NereusSDR::EmnrGainMethod>(s.value(sp + QStringLiteral("Nr2GainMethod")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr2NpeMethod"))) {
        setNr2NpeMethod(static_cast<NereusSDR::EmnrNpeMethod>(s.value(sp + QStringLiteral("Nr2NpeMethod")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr2TrainT1"))) {
        setNr2TrainT1(s.value(sp + QStringLiteral("Nr2TrainT1")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2TrainT2"))) {
        setNr2TrainT2(s.value(sp + QStringLiteral("Nr2TrainT2")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2AeFilter"))) {
        setNr2AeFilter(s.value(sp + QStringLiteral("Nr2AeFilter")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Nr2Position"))) {
        setNr2Position(static_cast<NereusSDR::NrPosition>(s.value(sp + QStringLiteral("Nr2Position")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Run"))) {
        setNr2Post2Run(s.value(sp + QStringLiteral("Nr2Post2Run")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Level"))) {
        setNr2Post2Level(s.value(sp + QStringLiteral("Nr2Post2Level")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Factor"))) {
        setNr2Post2Factor(s.value(sp + QStringLiteral("Nr2Post2Factor")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Rate"))) {
        setNr2Post2Rate(s.value(sp + QStringLiteral("Nr2Post2Rate")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Taper"))) {
        setNr2Post2Taper(s.value(sp + QStringLiteral("Nr2Post2Taper")).toInt());
    }
    // NR3
    if (s.contains(sp + QStringLiteral("Nr3Position"))) {
        setNr3Position(static_cast<NereusSDR::NrPosition>(s.value(sp + QStringLiteral("Nr3Position")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr3UseDefaultGain"))) {
        setNr3UseDefaultGain(s.value(sp + QStringLiteral("Nr3UseDefaultGain")).toString() == QLatin1String("True"));
    }
    // NR4
    if (s.contains(sp + QStringLiteral("Nr4Reduction"))) {
        setNr4Reduction(s.value(sp + QStringLiteral("Nr4Reduction")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Smoothing"))) {
        setNr4Smoothing(s.value(sp + QStringLiteral("Nr4Smoothing")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Whitening"))) {
        setNr4Whitening(s.value(sp + QStringLiteral("Nr4Whitening")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Rescale"))) {
        setNr4Rescale(s.value(sp + QStringLiteral("Nr4Rescale")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4PostThresh"))) {
        setNr4PostThresh(s.value(sp + QStringLiteral("Nr4PostThresh")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Algo"))) {
        setNr4Algo(static_cast<NereusSDR::SbnrAlgo>(s.value(sp + QStringLiteral("Nr4Algo")).toInt()));
    }
    // DFNR
    if (s.contains(sp + QStringLiteral("DfnrAttenLimit"))) {
        setDfnrAttenLimit(s.value(sp + QStringLiteral("DfnrAttenLimit")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("DfnrPostFilterBeta"))) {
        setDfnrPostFilterBeta(s.value(sp + QStringLiteral("DfnrPostFilterBeta")).toDouble());
    }
    // BNR + MNR
    if (s.contains(sp + QStringLiteral("BnrStrength"))) {
        setBnrStrength(s.value(sp + QStringLiteral("BnrStrength")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrStrength"))) {
        setMnrStrength(s.value(sp + QStringLiteral("MnrStrength")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrOversub"))) {
        setMnrOversub(s.value(sp + QStringLiteral("MnrOversub")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrFloor"))) {
        setMnrFloor(s.value(sp + QStringLiteral("MnrFloor")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrAlpha"))) {
        setMnrAlpha(s.value(sp + QStringLiteral("MnrAlpha")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrBias"))) {
        setMnrBias(s.value(sp + QStringLiteral("MnrBias")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrGsmooth"))) {
        setMnrGsmooth(s.value(sp + QStringLiteral("MnrGsmooth")).toDouble());
    }

    if (s.contains(sp + QStringLiteral("SnbEnabled"))) {
        setSnbEnabled(s.value(sp + QStringLiteral("SnbEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("AnfEnabled"))) {
        setAnfEnabled(s.value(sp + QStringLiteral("AnfEnabled")).toString() == QLatin1String("True"));
    }

    // Squelch, per receiver. From Thetis console.cs:4164-4193 [v2.10.3.15]
    // (case "rx1_squelch_state" through case
    // "rx2_voice_squelch_threshold_scroll"); see saveToSettings for how
    // Thetis's per-receiver states and thresholds map onto these keys.
    // Through the public setters, so a restored value reaches WDSP the same
    // way an edit does. Each threshold is set before its enable, so the
    // model's changed signals (and the per-slice pushes wired to them) come
    // threshold first. Thetis applies its squelch the other way round, run
    // before threshold (console.cs:47361 then 47437, 47303-47315
    // [v2.10.3.15]). A restore with signals blocked reaches WDSP another
    // way. For slices B to D it is RadioModel::activateSliceChannel's seed,
    // which also pushes thresholds first. For slice A it is the initial
    // push in RadioModel::connectToRadio, which sets each enable before its
    // threshold; that order is inaudible, because both land before the
    // channel is switched on.
    // A threshold that is not a number keeps the current value; one out of
    // range is clamped to its ControlRanges.h range, so a hand-edited file
    // cannot push a value the sliders cannot make.
    const auto restoreThresh = [&](const QString& key, double lo, double hi,
                                   auto&& setter) {
        if (!s.contains(key)) {
            return;
        }
        bool ok = false;
        const double v = s.value(key).toString().toDouble(&ok);
        if (!ok || !std::isfinite(v)) {
            return;
        }
        setter(std::clamp(v, lo, hi));
    };
    restoreThresh(sp + QStringLiteral("SsqlThresh"),
                  ControlRanges::kSsqlThreshMin, ControlRanges::kSsqlThreshMax,
                  [this](double v) { setSsqlThresh(v); });
    if (s.contains(sp + QStringLiteral("SsqlEnabled"))) {
        setSsqlEnabled(s.value(sp + QStringLiteral("SsqlEnabled")).toString() == QLatin1String("True"));
    }
    restoreThresh(sp + QStringLiteral("AmsqThresh"),
                  ControlRanges::kAmsqThreshMinDb, ControlRanges::kAmsqThreshMaxDb,
                  [this](double v) { setAmsqThresh(v); });
    if (s.contains(sp + QStringLiteral("AmsqEnabled"))) {
        setAmsqEnabled(s.value(sp + QStringLiteral("AmsqEnabled")).toString() == QLatin1String("True"));
    }
    restoreThresh(sp + QStringLiteral("FmsqThresh"),
                  ControlRanges::kFmsqThreshMinDb, ControlRanges::kFmsqThreshMaxDb,
                  [this](double v) { setFmsqThresh(v); });
    if (s.contains(sp + QStringLiteral("FmsqEnabled"))) {
        setFmsqEnabled(s.value(sp + QStringLiteral("FmsqEnabled")).toString() == QLatin1String("True"));
    }

    // NB1 / NB2 / SNB detailed tuning, with a one-way migration off the old
    // radio-global keys. Before these became per-slice properties the NB/SNB
    // setup page wrote one global value per knob and pushed it to channel 0;
    // an operator who had tuned the blanker would otherwise silently lose
    // that tuning on upgrade. So when a slice has no stored value of its own,
    // fall back to the legacy global before falling back to the default.
    // Nothing writes the legacy keys any more, so this decays naturally: once
    // a slice has been saved, its own key wins for good.
    auto restoreInt = [&](const char* perSlice, const char* legacyGlobal,
                          int fallback, auto&& setter) {
        const QString key = sp + QLatin1String(perSlice);
        if (s.contains(key)) {
            setter(s.value(key).toInt());
        } else if (s.contains(QLatin1String(legacyGlobal))) {
            setter(s.value(QLatin1String(legacyGlobal)).toInt());
        } else {
            setter(fallback);
        }
    };
    auto restoreDouble = [&](const char* perSlice, const char* legacyGlobal,
                             double legacyScale, double fallback, auto&& setter) {
        const QString key = sp + QLatin1String(perSlice);
        if (s.contains(key)) {
            setter(s.value(key).toDouble());
        } else if (s.contains(QLatin1String(legacyGlobal))) {
            setter(s.value(QLatin1String(legacyGlobal)).toDouble() * legacyScale);
        } else {
            setter(fallback);
        }
    };

    restoreInt("Nb1Threshold", "NbDefaultThresholdSlider", 30,
               [this](int v) { setNb1Threshold(v); });
    // The legacy transition / lead / lag keys stored the SLIDER integer at
    // x100 scale (1 == 0.01 ms), so migrating them needs the divide the
    // setup page used to apply on the way to WDSP.
    restoreDouble("Nb1TransitionMs", "NbDefaultTransition", 0.01, 0.01,
                  [this](double v) { setNb1TransitionMs(v); });
    restoreDouble("Nb1LeadMs", "NbDefaultLead", 0.01, 0.01,
                  [this](double v) { setNb1LeadMs(v); });
    restoreDouble("Nb1LagMs", "NbDefaultLag", 0.01, 0.01,
                  [this](double v) { setNb1LagMs(v); });
    restoreInt("Nb2Mode", "Nb2DefaultMode", 0,
               [this](int v) { setNb2Mode(v); });
    // The legacy SNB keys already stored real units, so scale is 1.0.
    restoreDouble("SnbK1", "SnbDefaultK1", 1.0, 8.0,
                  [this](double v) { setSnbK1(v); });
    restoreDouble("SnbK2", "SnbDefaultK2", 1.0, 20.0,
                  [this](double v) { setSnbK2(v); });
    restoreInt("SnbOutputBandwidthHz", "SnbDefaultOutputBW", 6000,
               [this](int v) { setSnbOutputBandwidthHz(v); });
    if (s.contains(sp + QStringLiteral("Locked"))) {
        setLocked(s.value(sp + QStringLiteral("Locked")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Muted"))) {
        setMuted(s.value(sp + QStringLiteral("Muted")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("RitEnabled"))) {
        setRitEnabled(s.value(sp + QStringLiteral("RitEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("RitHz"))) {
        setRitHz(s.value(sp + QStringLiteral("RitHz")).toInt());
    }
    if (s.contains(sp + QStringLiteral("XitEnabled"))) {
        setXitEnabled(s.value(sp + QStringLiteral("XitEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("XitHz"))) {
        setXitHz(s.value(sp + QStringLiteral("XitHz")).toInt());
    }
    if (s.contains(sp + QStringLiteral("AfGain"))) {
        setAfGain(s.value(sp + QStringLiteral("AfGain")).toInt());
    }
    if (s.contains(sp + QStringLiteral("RfGain"))) {
        setRfGain(s.value(sp + QStringLiteral("RfGain")).toInt());
    }
    if (s.contains(sp + QStringLiteral("RxAntenna"))) {
        setRxAntenna(s.value(sp + QStringLiteral("RxAntenna")).toString());
    }
    if (s.contains(sp + QStringLiteral("TxAntenna"))) {
        setTxAntenna(s.value(sp + QStringLiteral("TxAntenna")).toString());
    }

    // Audio panel, squelch, APF and the mode-specific offsets (per slice).
    // An absent key leaves the SliceModel default as it is.
    const auto sliceBool = [&s, &sp](const char* key, auto apply) {
        const QString k = sp + QLatin1String(key);
        if (s.contains(k)) {
            apply(s.value(k).toString() == QLatin1String("True"));
        }
    };
    // The setters do not bound these, and several go straight to WDSP when
    // the slice's channel is made (pan to SetRXAPanelPan, the APF tune to
    // SetRXASPCWFreq, the squelch levels), so the file is not trusted: a
    // value that is not a finite number leaves the default, and a number
    // outside what the control allows is held to the nearest end of it.
    const auto sliceInt = [&s, &sp](const char* key, int low, int high, auto apply) {
        const QString k = sp + QLatin1String(key);
        if (s.contains(k)) {
            bool isNumber = false;
            const int v = s.value(k).toString().toInt(&isNumber);
            if (isNumber) {
                apply(std::clamp(v, low, high));
            }
        }
    };
    const auto sliceDouble = [&s, &sp](const char* key, double low, double high, auto apply) {
        const QString k = sp + QLatin1String(key);
        if (s.contains(k)) {
            bool isNumber = false;
            const double v = s.value(k).toString().toDouble(&isNumber);
            if (isNumber && std::isfinite(v)) {
                apply(std::clamp(v, low, high));
            }
        }
    };
    // Each range is the one its control allows.
    // Pan: src/gui/widgets/VfoWidget.cpp:1468 (slider -100..100, over 100).
    constexpr double kAudioPanMin = -1.0;
    constexpr double kAudioPanMax = 1.0;
    // APF tune: src/gui/widgets/VfoWidget.cpp:1884 (slider -500..500 Hz).
    constexpr int kApfTuneMinHz = -500;
    constexpr int kApfTuneMaxHz = 500;
    // CTCSS mode: src/gui/widgets/VfoModeContainers.cpp:188-191 (Off,
    // Encode, Decode, Enc+Dec = 0..3).
    constexpr int kFmCtcssModeMin = 0;
    constexpr int kFmCtcssModeMax = 3;
    // CTCSS tone: src/gui/widgets/VfoModeContainers.cpp:145-155, the first
    // and last entries of the tone table.
    constexpr double kFmCtcssMinHz = 67.0;
    constexpr double kFmCtcssMaxHz = 254.1;
    // FM offset: src/gui/widgets/VfoModeContainers.cpp:222 (spin 0..10000
    // kHz, stored in Hz).
    constexpr int kFmOffsetMinHz = 0;
    constexpr int kFmOffsetMaxHz = 10000 * 1000;
    // DIGL / DIGU offset: src/gui/widgets/VfoModeContainers.cpp:414.
    constexpr int kDigOffsetMinHz = -10000;
    constexpr int kDigOffsetMaxHz = 10000;
    // RTTY mark and shift: src/gui/widgets/VfoModeContainers.cpp:522, 544.
    constexpr int kRttyMarkMinHz = 1000;
    constexpr int kRttyMarkMaxHz = 3500;
    constexpr int kRttyShiftMinHz = 50;
    constexpr int kRttyShiftMaxHz = 1000;
    // Squelch levels: src/core/ControlRanges.h:117-118, 126-127, 133-134.
    sliceDouble("AudioPan", kAudioPanMin, kAudioPanMax, [this](double v) { setAudioPan(v); });
    sliceBool("BinauralEnabled",  [this](bool v) { setBinauralEnabled(v); });
    sliceBool("SsqlEnabled",      [this](bool v) { setSsqlEnabled(v); });
    sliceDouble("SsqlThresh", ControlRanges::kSsqlThreshMin, ControlRanges::kSsqlThreshMax,
                [this](double v) { setSsqlThresh(v); });
    sliceBool("AmsqEnabled",      [this](bool v) { setAmsqEnabled(v); });
    sliceDouble("AmsqThresh", ControlRanges::kAmsqThreshMinDb, ControlRanges::kAmsqThreshMaxDb,
                [this](double v) { setAmsqThresh(v); });
    sliceBool("FmsqEnabled",      [this](bool v) { setFmsqEnabled(v); });
    sliceDouble("FmsqThresh", ControlRanges::kFmsqThreshMinDb, ControlRanges::kFmsqThreshMaxDb,
                [this](double v) { setFmsqThresh(v); });
    sliceBool("ApfEnabled",       [this](bool v) { setApfEnabled(v); });
    sliceInt("ApfTuneHz", kApfTuneMinHz, kApfTuneMaxHz, [this](int v) { setApfTuneHz(v); });
    sliceInt("FmCtcssMode", kFmCtcssModeMin, kFmCtcssModeMax, [this](int v) { setFmCtcssMode(v); });
    sliceDouble("FmCtcssValueHz", kFmCtcssMinHz, kFmCtcssMaxHz,
                [this](double v) { setFmCtcssValueHz(v); });
    sliceInt("FmOffsetHz", kFmOffsetMinHz, kFmOffsetMaxHz, [this](int v) { setFmOffsetHz(v); });
    const auto sliceEnumInt = [&s, &sp](const char* key, auto apply) {
        const QString k = sp + QLatin1String(key);
        if (s.contains(k)) {
            bool isNumber = false;
            const int v = s.value(k).toString().toInt(&isNumber);
            if (isNumber) {
                apply(v);
            }
        }
    };
    sliceEnumInt("FmTxMode", [this](int v) {
        // Only the three values FmTxMode has; anything else in the file
        // leaves the default.
        if (v >= static_cast<int>(FmTxMode::High) && v <= static_cast<int>(FmTxMode::Low)) {
            setFmTxMode(static_cast<FmTxMode>(v));
        }
    });
    sliceBool("FmReverse",        [this](bool v) { setFmReverse(v); });
    sliceInt("DiglOffsetHz", kDigOffsetMinHz, kDigOffsetMaxHz, [this](int v) { setDiglOffsetHz(v); });
    sliceInt("DiguOffsetHz", kDigOffsetMinHz, kDigOffsetMaxHz, [this](int v) { setDiguOffsetHz(v); });
    sliceInt("RttyMarkHz", kRttyMarkMinHz, kRttyMarkMaxHz, [this](int v) { setRttyMarkHz(v); });
    sliceInt("RttyShiftHz", kRttyShiftMinHz, kRttyShiftMaxHz, [this](int v) { setRttyShiftHz(v); });
    restoreNnrSettings();
}

// One-shot migration of the legacy flat key format (VfoFrequency, VfoDspMode,
// etc.) to the new per-slice-per-band namespace. Called once at startup before
// restoreFromSettings(). If the legacy key is absent the function is a no-op.
void SliceModel::migrateLegacyKeys()
{
    auto& s = AppSettings::instance();

    if (!s.contains(QStringLiteral("VfoFrequency"))) {
        return; // Nothing to migrate.
    }

    // Derive the band from the persisted frequency.
    double freq = s.value(QStringLiteral("VfoFrequency"), 14225000.0).toDouble();
    Band band = bandFromFrequency(freq);

    // Slice 0 — the only slice that can have legacy data.
    const QString bp = bandPrefix(0, band);
    const QString sp = slicePrefix(0);

    // Per-band DSP — migrate each key that exists.
    s.setValue(bp + QStringLiteral("Frequency"), freq);

    if (s.contains(QStringLiteral("VfoDspMode"))) {
        s.setValue(bp + QStringLiteral("DspMode"),
                   s.value(QStringLiteral("VfoDspMode")));
    }
    if (s.contains(QStringLiteral("VfoFilterLow"))) {
        s.setValue(bp + QStringLiteral("FilterLow"),
                   s.value(QStringLiteral("VfoFilterLow")));
    }
    if (s.contains(QStringLiteral("VfoFilterHigh"))) {
        s.setValue(bp + QStringLiteral("FilterHigh"),
                   s.value(QStringLiteral("VfoFilterHigh")));
    }
    if (s.contains(QStringLiteral("VfoAgcMode"))) {
        s.setValue(bp + QStringLiteral("AgcMode"),
                   s.value(QStringLiteral("VfoAgcMode")));
    }
    if (s.contains(QStringLiteral("VfoStepHz"))) {
        s.setValue(bp + QStringLiteral("StepHz"),
                   s.value(QStringLiteral("VfoStepHz")));
    }

    // Session state — migrate each key that exists.
    if (s.contains(QStringLiteral("VfoAfGain"))) {
        s.setValue(sp + QStringLiteral("AfGain"),
                   s.value(QStringLiteral("VfoAfGain")));
    }
    if (s.contains(QStringLiteral("VfoRfGain"))) {
        s.setValue(sp + QStringLiteral("RfGain"),
                   s.value(QStringLiteral("VfoRfGain")));
    }
    if (s.contains(QStringLiteral("VfoRxAntenna"))) {
        s.setValue(sp + QStringLiteral("RxAntenna"),
                   s.value(QStringLiteral("VfoRxAntenna")));
    }
    if (s.contains(QStringLiteral("VfoTxAntenna"))) {
        s.setValue(sp + QStringLiteral("TxAntenna"),
                   s.value(QStringLiteral("VfoTxAntenna")));
    }

    // Remove all legacy flat keys.
    s.remove(QStringLiteral("VfoFrequency"));
    s.remove(QStringLiteral("VfoDspMode"));
    s.remove(QStringLiteral("VfoFilterLow"));
    s.remove(QStringLiteral("VfoFilterHigh"));
    s.remove(QStringLiteral("VfoAgcMode"));
    s.remove(QStringLiteral("VfoStepHz"));
    s.remove(QStringLiteral("VfoAfGain"));
    s.remove(QStringLiteral("VfoRfGain"));
    s.remove(QStringLiteral("VfoRxAntenna"));
    s.remove(QStringLiteral("VfoTxAntenna"));
}

// Reads Slice<N>/LastBand and parses it back to a Band. Returns
// std::nullopt on missing key (fresh install / pre-LastBand settings)
// or unparseable values. Static so RadioModel can call this before
// constructing any slice.
std::optional<Band> SliceModel::loadLastBandFromSettings(int sliceIndex)
{
    auto& s = AppSettings::instance();
    const QString key = slicePrefix(sliceIndex) + QStringLiteral("LastBand");
    if (!s.contains(key)) {
        return std::nullopt;
    }
    const QString name = s.value(key).toString();
    if (name.isEmpty()) {
        return std::nullopt;
    }
    // bandFromName handles both label form ("20m") and short form ("20"),
    // plus GEN/WWV/XVTR. Falls back to Band::GEN on unknown input — guard
    // against that so a corrupted key doesn't silently land on GEN.
    const Band parsed = bandFromName(name);
    if (parsed == Band::GEN && name != QLatin1String("GEN")) {
        return std::nullopt;
    }
    return parsed;
}

// ---------------------------------------------------------------------------
// Phase 3O — VAX routing
// ---------------------------------------------------------------------------

void SliceModel::setVaxChannel(int ch)
{
    // Clamp to valid range.
    if (ch < 0 || ch > 4) { ch = 0; }

    const int prev = m_vaxChannel.exchange(ch, std::memory_order_acq_rel);
    if (prev == ch) { return; }

    if (m_vaxChannelStore) {
        // R-R3-44: a remote window keeps it on this computer.
        m_vaxChannelStore(m_sliceIndex, ch);
    } else {
        AppSettings::instance().setValue(
            slicePrefix(m_sliceIndex) + QStringLiteral("VaxChannel"),
            QString::number(ch));
    }

    emit vaxChannelChanged(ch);
}

void SliceModel::setVaxChannelStore(VaxChannelStore store)
{
    m_vaxChannelStore = std::move(store);
}

// ---------------------------------------------------------------------------
// R-R3-45: speakers or headphones (VAX design 6.2)
// ---------------------------------------------------------------------------

QString SliceModel::outputRouteSettingValue(OutputRoute route)
{
    return route == OutputRoute::Headphones ? QStringLiteral("Headphones")
                                            : QStringLiteral("Speakers");
}

void SliceModel::setOutputRoute(OutputRoute route)
{
    if (holdsListenerWrite(outputRoute(), route)) { return; }
    if (route != OutputRoute::Headphones) {
        route = OutputRoute::Speakers;
    }
    const int prev = m_outputRoute.exchange(static_cast<int>(route),
                                            std::memory_order_acq_rel);
    if (prev == static_cast<int>(route)) { return; }

    if (m_outputRoutePersisted) {
        AppSettings::instance().setValue(
            slicePrefix(m_sliceIndex) + QStringLiteral("OutputRoute"),
            outputRouteSettingValue(route));
    }

    emit outputRouteChanged(route);
}

void SliceModel::restoreOutputRoute()
{
    const QString stored = AppSettings::instance()
        .value(slicePrefix(m_sliceIndex) + QStringLiteral("OutputRoute"),
               QStringLiteral("Speakers"))
        .toString();
    const OutputRoute route = stored == QLatin1String("Headphones")
        ? OutputRoute::Headphones : OutputRoute::Speakers;
    const int prev = m_outputRoute.exchange(static_cast<int>(route),
                                            std::memory_order_acq_rel);
    if (prev != static_cast<int>(route)) {
        emit outputRouteChanged(route);
    }
}

// ── Phase 3J-2 Task D5: per-slice live SNR (NereusSDR-native) ──
//
// Emits snrDbChanged only on actual value change:
//   NaN    -> NaN              : no emission (signal stays absent)
//   x      -> identical x      : no emission (no change)
//   NaN    -> numeric          : emission (signal-acquired event)
//   numeric -> NaN             : emission (signal-lost event)
//   x      -> y (x != y)       : emission (normal update)
//
// NaN-aware comparison is required because IEEE NaN != NaN at the
// hardware level, so a naive equality check would treat NaN -> NaN as
// a change and spam emissions on every block when no signal is present.
void SliceModel::setSnrDb(double db)
{
    const bool dbNan   = qIsNaN(db);
    const bool prevNan = qIsNaN(m_snrDb);

    if (dbNan && prevNan) { return; }                                // both NaN: no change
    if (!dbNan && !prevNan && qFuzzyCompare(db, m_snrDb)) { return; }// both numeric and equal

    m_snrDb = db;
    emit snrDbChanged(db);

    // 2026-05-12 bench: heard fresh activity -> restart the idle
    // timer.  Only counts when the new SNR is numeric (NaN -> NaN was
    // already filtered above; numeric -> NaN means "we just cleared",
    // which shouldn't extend the activity window).
    if (!dbNan) {
        restartRadeIdleClearTimer();
    }
}

// ── 2026-05-11 bench: last RADE-decoded speaker callsign ────────────────────
//
// Sticky-while-in-RADE / clears-on-mode-off-RADE semantics per the
// bench design discussion (option A + D).  setDspMode in this file
// also clears the field when transitioning out of RADE_U/RADE_L; this
// setter is the write side for incoming decodes.
void SliceModel::setLastRadeRxCallsign(const QString& callsign)
{
    if (m_lastRadeRxCallsign == callsign) {
        return;
    }
    m_lastRadeRxCallsign = callsign;
    emit lastRadeRxCallsignChanged(callsign);

    // 2026-05-12 bench: heard fresh EOO callsign decode -> restart
    // the idle timer.  Empty -> non-empty is real activity; clears
    // back to empty (from timer expiry or mode-off-RADE) don't
    // restart -- letting the timer keep counting from the last
    // genuine activity.
    if (!callsign.isEmpty()) {
        restartRadeIdleClearTimer();
    }
}

void SliceModel::loadFromSettings()
{
    restoreNnrSettings();
    auto& s = AppSettings::instance();

    // ── VAX channel (Phase 3O) ────────────────────────────────────────────────
    int vaxCh = s.value(
        slicePrefix(m_sliceIndex) + QStringLiteral("VaxChannel"), "0")
        .toString().toInt();
    if (vaxCh < 0 || vaxCh > 4) { vaxCh = 0; }  // spec §5.1: invalid values clamp to 0
    if (vaxCh != m_vaxChannel.load(std::memory_order_acquire)) {
        m_vaxChannel.store(vaxCh, std::memory_order_release);
        emit vaxChannelChanged(vaxCh);
    }

    // ── Output route (R-R3-45) ────────────────────────────────────────────────
    restoreOutputRoute();

}

} // namespace NereusSDR
