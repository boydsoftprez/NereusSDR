// =================================================================
// src/models/SliceModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
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
    if (m_dspMode != DSPMode::RADE_U && m_dspMode != DSPMode::RADE_L) {
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
        return m == DSPMode::RADE_U || m == DSPMode::RADE_L;
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
    if (isRade(oldMode) && !m_lastRadeRxCallsign.isEmpty()) {
        m_lastRadeRxCallsign.clear();
        const QString callsign = m_lastRadeRxCallsign;
        emit lastRadeRxCallsignChanged(callsign);
        if (!currentMode()) { return; }
    }
    // The same two cases end the old decoder (it is destroyed below),
    // and the VFO flag drops its sync dot (VfoWidget::setRadeActive):
    // the next decoder reports its own sync.
    if (isRade(oldMode)) {
        setRadeSynced(false);
        if (!currentMode()) { return; }
    }

    // 2026-05-12 bench: stop the idle-clear timer when leaving
    // RADE.  The clear above already happened; letting the timer
    // fire would just re-emit lastRadeRxCallsignChanged("") and
    // snrDbChanged(NaN) needlessly.  Also stop on RADE_U <-> RADE_L
    // swaps for the same reason.
    if (isRade(oldMode) && m_radeIdleClearTimer) {
        m_radeIdleClearTimer->stop();
    }

    auto* radio = qobject_cast<RadioModel*>(parent());
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
            const bool oldIsRade = isRade(oldMode);
            const bool newIsRade = isRade(newMode);

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
                radeCh->setSideband(newMode == DSPMode::RADE_U);
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
}

void SliceModel::setDspMode(DSPMode mode)
{
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
    // back to defaultFilterForMode (Thetis F5 presets).  Mirrors
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
    gain = std::clamp(gain, 0, 100);
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
// for ANAN-7000D, EXT2/EXT1/XVTR for ANAN-100D, etc).
void SliceModel::refreshAntennasFromAlex(const AlexController& alex,
                                         Band band,
                                         const SkuUiProfile* sku)
{
    const int rxOnly = alex.rxOnlyAnt(band);  // 0=none, 1/2/3 indexed
    const int rx     = alex.rxAnt(band);      // 1..3
    const int tx     = alex.txAnt(band);      // 1..3
    auto name = [](int n) {
        switch (n) {
            case 2:  return QStringLiteral("ANT2");
            case 3:  return QStringLiteral("ANT3");
            default: return QStringLiteral("ANT1");
        }
    };

    // Issue #257: prefer the SKU-specific RX-only label when the bypass mux
    // is engaged. Fall back to the ANT* label when sku is null, when the
    // mux is disengaged (rxOnly == 0), or when the indexed slot in the SKU
    // is empty (defensive — should never happen because rxOnlyLabels is
    // always-3 in SkuUiProfile.h).
    QString rxLabel;
    if (sku && rxOnly >= 1 && rxOnly <= 3) {
        const QString& slot = sku->rxOnlyLabels[static_cast<size_t>(rxOnly - 1)];
        rxLabel = slot.isEmpty() ? name(rx) : slot;
    } else {
        rxLabel = name(rx);
    }

    // Use the public setters so rxAntennaChanged / txAntennaChanged
    // signals fire — VFO Flag and RxApplet listen. The loop back to
    // AlexController via T12's handler is idempotent: AlexController's
    // setRxAnt/setTxAnt returns early on equal value, so no signal is
    // emitted.
    setRxAntenna(rxLabel);
    setTxAntenna(name(tx));
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

// Porting from Thetis console.cs:5180-5575 — InitFilterPresets, F5 per mode.
//
// Filter low/high are in Hz relative to the carrier frequency.
// LSB: negative offsets (passband below carrier)
// USB: positive offsets (passband above carrier)
// AM/SAM/DSB: symmetric around carrier
// CW: centered on cw_pitch (600 Hz from Thetis display.cs:1023)
// DIGU: centered on digu_click_tune_offset (1500 Hz from Thetis console.cs:14636)
// DIGL: centered on -digl_click_tune_offset (-2210 Hz from Thetis console.cs:14671)
std::pair<int, int> SliceModel::defaultFilterForMode(DSPMode mode)
{
    // Phase 3J-1 closeout Item 6 (2026-05-12): read CW pitch from
    // AppSettings instead of hardcoding 600.  Operator-configurable in
    // Thetis (Setup → Keyboard / DSP → CW pitch slider; default 600 Hz);
    // the dedicated NereusSDR setter lands with Phase 3M-2 CW TX, but the
    // read path needs to be in place now so the filter center moves with
    // the setting once that UI ships.  Range matches Thetis udCWPitch
    // (Setup.designer.cs CW pitch up-down: 100..2000 Hz).
    //
    // From Thetis display.cs:1023 [v2.10.3.13] — cw_pitch default 600.
    auto& s = AppSettings::instance();
    int cwPitch = s.value(QStringLiteral("CWPitch"), 600).toInt();
    if (cwPitch < 100)  { cwPitch = 100;  }
    if (cwPitch > 2000) { cwPitch = 2000; }
    const int kCwPitch = cwPitch;
    // From Thetis console.cs:14636
    static constexpr int kDiguOffset = 1500;
    // From Thetis console.cs:14671
    // Upstream inline attribution preserved verbatim:
    //   :14669  //reset preset filter's center frequency - W4TME
    static constexpr int kDiglOffset = 2210;

    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5207 — F5: -3000 to -100
        return {-3000, -100};
    case DSPMode::USB:
        // From Thetis console.cs:5249 — F5: 100 to 3000
        return {100, 3000};
    case DSPMode::DSB:
        // From Thetis console.cs:5543 — F5: -3300 to 3300
        return {-3300, 3300};
    case DSPMode::CWL:
        // From Thetis console.cs:5375 — F5: -(cw_pitch+200) to -(cw_pitch-200)
        return {-(kCwPitch + 200), -(kCwPitch - 200)};
    case DSPMode::CWU:
        // From Thetis console.cs:5417 — F5: (cw_pitch-200) to (cw_pitch+200)
        return {kCwPitch - 200, kCwPitch + 200};
    case DSPMode::FM:
        // FM filters are dynamic in Thetis (from deviation + high cut).
        // Default deviation=5000, so use ±8000 as reasonable default.
        // From Thetis console.cs:7559-7565
        // Upstream inline attribution preserved verbatim (console.cs:7560):
        //   int halfBw = (int)(radio.GetDSPRX(0, 0).RXFMDeviation + radio.GetDSPRX(0, 0).RXFMHighCut);  //[2.10.3.4]MW0LGE
        return {-8000, 8000};
    case DSPMode::AM:
        // From Thetis console.cs:5459 — F5: -5000 to 5000
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
        // From Thetis console.cs:5328 [v2.10.3.13] — DIGU F5 preset:
        //   preset[m].SetFilter(Filter.F5, digu_click_tune_offset - 600,
        //                       digu_click_tune_offset + 600, "1.2k");
        return {kDiguOffset - 600, kDiguOffset + 600};
    case DSPMode::SPEC:
        // SPEC mode: passthrough, wide filter
        return {-5000, 5000};
    case DSPMode::DIGL:
        // Phase 3J-1 closeout Item 4 (2026-05-12): reverted to Thetis F5
        // default -- see DIGU case above for the full rationale.
        //
        // From Thetis console.cs:5286 [v2.10.3.13] — DIGL F5 preset:
        //   preset[m].SetFilter(Filter.F5, -(digl_click_tune_offset + 600),
        //                       -(digl_click_tune_offset - 600), "1.2k");
        return {-(kDiglOffset + 600), -(kDiglOffset - 600)};
    case DSPMode::SAM:
        // From Thetis console.cs:5501 — F5: -5000 to 5000
        return {-5000, 5000};
    case DSPMode::DRM:
        // DRM: wide filter similar to AM
        return {-5000, 5000};
    case DSPMode::RADE_U:
        // Phase 3R Task J1.  RADE Upper sideband: the modem occupies
        // ~650..2350 Hz (1700 Hz wide, centered at +1500 Hz).  This is
        // the SSB-style passband that the panadapter filter window
        // displays and that the TX I/Q routing confines the RADE
        // baseband energy to.  The earlier +/-5000 Hz AM-class window
        // was a placeholder; the filter IS visible on the panadapter
        // AND defines the IF/baseband passband for the modem energy.
        return {650, 2350};
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

// From Thetis console.cs:5180-5575 [v2.10.3.13] — InitFilterPresets (F1-F10 per mode).
// Returns (low_hz, high_hz) pairs in Thetis F1→F10 order. The full list drives
// RxApplet's 10-button filter grid; VfoWidget uses commonPresetsForMode() (a subset).
QList<std::pair<int, int>> SliceModel::presetsForMode(DSPMode mode)
{
    // Phase 3J-1 closeout Item 6 (2026-05-12): read CW pitch from
    // AppSettings — see defaultFilterForMode() above for the full
    // rationale.  Default 600 Hz from Thetis display.cs:1023.
    auto& s = AppSettings::instance();
    int cwPitch = s.value(QStringLiteral("CWPitch"), 600).toInt();
    if (cwPitch < 100)  { cwPitch = 100;  }
    if (cwPitch > 2000) { cwPitch = 2000; }
    const int kCwPitch = cwPitch;
    // From Thetis console.cs:14636 [v2.10.3.13]
    static constexpr int kDiguOffset = 1500;
    // From Thetis console.cs:14671 [v2.10.3.13]
    // Upstream tags preserved: //W4TME (from cited console.cs:14669) [v2.10.3.15]
    static constexpr int kDiglOffset = 2210;

    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5191-5231 [v2.10.3.13] — LSB F1-F10
        return { {-5100,-100}, {-4500,-100}, {-3900,-100}, {-3300,-100},
                 {-3000,-100}, {-2700,-100}, {-2400,-100}, {-1800,-100},
                 {-1200,-100}, {-600,-100} };
    case DSPMode::USB:
        // From Thetis console.cs:5233-5273 [v2.10.3.13] — USB F1-F10
        return { {100,5100}, {100,4500}, {100,3900}, {100,3300},
                 {100,3000}, {100,2700}, {100,2400}, {100,1800},
                 {100,1200}, {100,600} };
    case DSPMode::DSB:
        // From Thetis console.cs:5527-5575 [v2.10.3.13] — DSB F1-F10, symmetric
        return { {-5100,5100}, {-4500,4500}, {-3900,3900}, {-3300,3300},
                 {-3000,3000}, {-2700,2700}, {-2400,2400}, {-1800,1800},
                 {-1200,1200}, {-600,600} };
    case DSPMode::CWL:
        // From Thetis console.cs:5359-5399 [v2.10.3.13] — CWL F1-F10 (lower sideband CW)
        return { {-(kCwPitch+750), -(kCwPitch-750)}, {-(kCwPitch+500), -(kCwPitch-500)},
                 {-(kCwPitch+400), -(kCwPitch-400)}, {-(kCwPitch+300), -(kCwPitch-300)},
                 {-(kCwPitch+200), -(kCwPitch-200)}, {-(kCwPitch+125), -(kCwPitch-125)},
                 {-(kCwPitch+50),  -(kCwPitch-50)},  {-(kCwPitch+25),  -(kCwPitch-25)},
                 {-(kCwPitch+12),  -(kCwPitch-12)},  {-(kCwPitch+6),   -(kCwPitch-6)} };
    case DSPMode::CWU:
        // From Thetis console.cs:5401-5441 [v2.10.3.13] — CWU F1-F10 (upper sideband CW)
        return { {kCwPitch-750, kCwPitch+750}, {kCwPitch-500, kCwPitch+500},
                 {kCwPitch-400, kCwPitch+400}, {kCwPitch-300, kCwPitch+300},
                 {kCwPitch-200, kCwPitch+200}, {kCwPitch-125, kCwPitch+125},
                 {kCwPitch-50,  kCwPitch+50},  {kCwPitch-25,  kCwPitch+25},
                 {kCwPitch-12,  kCwPitch+12},  {kCwPitch-6,   kCwPitch+6} };
    case DSPMode::FM:
        // From Thetis console.cs:5527 region [v2.10.3.13] — FM uses wide symmetric filters
        return { {-8000,8000}, {-6000,6000}, {-4000,4000} };
    case DSPMode::AM:
        // From Thetis console.cs:5443-5483 [v2.10.3.13] — AM F1-F10, symmetric
        return { {-10000,10000}, {-6000,6000}, {-5000,5000},
                 {-4000,4000},   {-3000,3000}, {-2500,2500},
                 {-2000,2000},   {-1500,1500}, {-1000,1000}, {-500,500} };
    case DSPMode::DIGU:
        // From Thetis console.cs:5317-5357 [v2.10.3.13] — DIGU F1-F10
        return { {kDiguOffset-3000, kDiguOffset+3000}, {kDiguOffset-2000, kDiguOffset+2000},
                 {kDiguOffset-1500, kDiguOffset+1500}, {kDiguOffset-1000, kDiguOffset+1000},
                 {kDiguOffset-500,  kDiguOffset+500},  {kDiguOffset-300,  kDiguOffset+300},
                 {kDiguOffset-150,  kDiguOffset+150},  {kDiguOffset-100,  kDiguOffset+100},
                 {kDiguOffset-50,   kDiguOffset+50},   {kDiguOffset-25,   kDiguOffset+25} };
    case DSPMode::SPEC:
        // Passthrough wideband
        return { {-5000,5000} };
    case DSPMode::DIGL:
        // From Thetis console.cs:5275-5315 [v2.10.3.13] — DIGL F1-F10
        return { {-(kDiglOffset+3000), -(kDiglOffset-3000)}, {-(kDiglOffset+2000), -(kDiglOffset-2000)},
                 {-(kDiglOffset+1500), -(kDiglOffset-1500)}, {-(kDiglOffset+1000), -(kDiglOffset-1000)},
                 {-(kDiglOffset+500),  -(kDiglOffset-500)},  {-(kDiglOffset+300),  -(kDiglOffset-300)},
                 {-(kDiglOffset+150),  -(kDiglOffset-150)},  {-(kDiglOffset+100),  -(kDiglOffset-100)},
                 {-(kDiglOffset+50),   -(kDiglOffset-50)},   {-(kDiglOffset+25),   -(kDiglOffset-25)} };
    case DSPMode::SAM:
        // From Thetis console.cs:5485-5525 [v2.10.3.13] — SAM F1-F10, symmetric
        return { {-10000,10000}, {-6000,6000}, {-5000,5000},
                 {-4000,4000},   {-3000,3000}, {-2500,2500},
                 {-2000,2000},   {-1500,1500}, {-1000,1000}, {-500,500} };
    case DSPMode::DRM:
        // DRM: wide digital AM-like filters
        return { {-10000,10000}, {-5000,5000} };
    case DSPMode::RADE_U:
        // Phase 3R Task J1.  RADE Upper sideband: single fixed-bandwidth
        // preset matching the 1700 Hz modem passband.  No F1-F10
        // variants; RADE has a fixed bandwidth per sideband.
        return { {650, 2350} };
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
        // From Thetis display.cs:1023 [v2.10.3.13] — default 600.
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
    case DSPMode::RADE_L: return QStringLiteral("RADE-L");
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

void SliceModel::saveToSettings(Band band)
{
    saveNnrSettings();
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    s.setValue(bp + QStringLiteral("Frequency"),    m_frequency);
    s.setValue(bp + QStringLiteral("AgcThreshold"), m_agcThreshold);
    s.setValue(bp + QStringLiteral("AgcHang"),      m_agcHang);
    s.setValue(bp + QStringLiteral("AgcSlope"),     m_agcSlope);
    s.setValue(bp + QStringLiteral("AgcAttack"),    m_agcAttack);
    s.setValue(bp + QStringLiteral("AgcDecay"),     m_agcDecay);
    s.setValue(bp + QStringLiteral("AgcAutoEnabled"), m_autoAgcEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(bp + QStringLiteral("AgcAutoOffset"), m_autoAgcOffset);
    s.setValue(bp + QStringLiteral("AgcFixedGain"), m_agcFixedGain);
    s.setValue(bp + QStringLiteral("AgcHangThreshold"), m_agcHangThreshold);
    s.setValue(bp + QStringLiteral("AgcMaxGain"),   m_agcMaxGain);
    s.setValue(bp + QStringLiteral("FilterLow"),    m_filterLow);
    s.setValue(bp + QStringLiteral("FilterHigh"),   m_filterHigh);
    // Phase 3J-1 closeout Item 4 (2026-05-12): ALSO persist filter under
    // (band, currentMode) so a future mode change can restore it.  Legacy
    // (band)/FilterLow stays for backward compat with code that reads it
    // directly without going through restoreFromSettings.
    {
        const QString bmp = bandModePrefix(m_sliceIndex, band, m_dspMode);
        s.setValue(bmp + QStringLiteral("FilterLow"),  m_filterLow);
        s.setValue(bmp + QStringLiteral("FilterHigh"), m_filterHigh);
    }
    s.setValue(bp + QStringLiteral("DspMode"),      static_cast<int>(m_dspMode));
    s.setValue(bp + QStringLiteral("AgcMode"),      static_cast<int>(m_agcMode));
    s.setValue(bp + QStringLiteral("StepHz"),       m_stepHz);

    // Noise-blanker mode (per-band). Tri-state Off/NB/NB2 mirrors Thetis
    // chkNB state per-receiver. NB TUNING (threshold / tau / lag / lead) is
    // NOT per-band in Thetis and lives globally inside NbFamily — see
    // SliceModel.h for the 2026-04-22 removal note.
    s.setValue(bp + QStringLiteral("NbMode"), static_cast<int>(m_nbMode));

    // Phase 3F: per-slice DDC sample rate, persisted per-band so each band
    // can independently remember its preferred rate (e.g. 192 kHz on 40m,
    // 1536 kHz on 10m for a wider pan). NereusSDR-original (no Thetis cite).
    s.setValue(bp + QStringLiteral("SampleRate"), m_sampleRateHz);

    // Phase 3F Sub-Epic G Task 2: per-band diversity tuning. The 8-memory
    // slots (T3) + direction-finding fields (T11) join this block when they
    // ship. NereusSDR-original schema (Thetis persists diversity globally
    // in DSP.console.dsp / Diversity.cs; we scope per-band per-slice so
    // operators can keep distinct DF setups across bands).
    s.setValue(bp + QStringLiteral("DiversityPhaseDeg"), m_diversityPhaseDeg);
    s.setValue(bp + QStringLiteral("DiversityGainDb"), m_diversityGainDb);
    s.setValue(bp + QStringLiteral("DiversityFineNullEnabled"),
               boolStr(m_diversityFineNullEnabled));

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
    if (s.contains(bp + QStringLiteral("AgcAutoEnabled"))) {
        setAutoAgcEnabled(s.value(bp + QStringLiteral("AgcAutoEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(bp + QStringLiteral("AgcAutoOffset"))) {
        setAutoAgcOffset(s.value(bp + QStringLiteral("AgcAutoOffset")).toDouble());
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
            if (!lifetime || revision != m_modeRevision) { return; }
            emit dspModeChanged(mode);
            if (!lifetime || filterRevision != m_modeFilterRevision) { return; }
        }
    }
    // Phase 3J-1 closeout Item 4 (2026-05-12): prefer (band, currentMode)
    // filter when persisted; fall back to legacy (band)/FilterLow/High
    // for pre-Item-4 settings files.  m_dspMode was set above (line ~1491)
    // before reaching this restore block, so it reflects the destination
    // mode for the band restore.
    {
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
