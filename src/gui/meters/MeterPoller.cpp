// =================================================================
// src/gui/meters/MeterPoller.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04 — Selected RX source identity and RX-only presentation reset by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-01  J.J. Boyd / KG4VCF. Resolve remote Max Bin by the slice
//                 hosted in this window. AI-assisted via OpenAI Codex.
//   2026-10-02 — Draft-only edits and inert cached previews by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Composite reading/replay/cadence contracts by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-26 — Phase 3M-1a H.2: TX meter bindings.  setTxChannel(),
//                 setInTx(bool) slot, pollTxMeters() helper.
//                 Cite: Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter.
//   2026-05-01 — Task 3.1: setIntervalMs/intervalMs + setAverageWindow/averageWindow
//                 accessors for MultimeterPage live wire-up.
//                 Corresponds to Thetis udDisplayMeterDelay + udDisplayMeterAvg
//                 (display.cs) [v2.10.3.13].
//   2026-08-06: Remote Daemon R2 Task 12: pollSliceSMeters() / setSliceChannels()
//                 / sliceSmeterUpdated / m_sliceChannels extracted to the new
//                 core-side src/core/meters/SliceMeterPump.{h,cpp}, so a
//                 headless nereusd (which never links this GUI-only class)
//                 also produces the per-slice S-meter reading. smeterUpdated
//                 removed with its only listener (MainWindow.cpp:8132);
//                 pollSMeter() itself is unchanged. J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-09-22: R-R3-13: pollRemoteRxMeters() feeds the -400 dBm no-reading
//                 sentinel to the S-meter and the flags in every RX mode when
//                 there is no reading (disconnected, snapshot not ready, no
//                 slice). J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-09-23: R-R3-13: the container meter items learned the no-reading
//                 rule (isNoMeterReading), so pollRemoteRxMeters() now feeds
//                 the -400 dBm sentinel to SignalPeak / SignalAvg with no
//                 reading (was the -140 floor), and poll() feeds it to the
//                 RX bindings once the local RX channel is gone instead of
//                 leaving them frozen. J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-09-24: R-R3-21: the Compression reading (TxComp) takes Thetis's
//                 -30 floor, max(-30, TXA_COMP_AV) (console.cs:46979 with
//                 dsp.cs:1056 [v2.10.3.15]); PROC off reads -30, not -400.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-09-24: R-R3-39: the RX readings (RxChannel::getMeter and
//                 WdspEngine::getRxaSignalPeak) come from the cache the
//                 receive lane refreshes, so this GUI-thread poll makes no
//                 RX WDSP call. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-25: R-R3-39 (station Task 32): the TX readings too
//               (TxChannel::txMeter, the transmit lane's last reading).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25: D14 / R-R3-49: the TX readings name a TxMeterType, which
//               TxChannel::txMeter maps to its WDSP index; ALC, ALC gain
//               and COMP had read TXA_COMP_PK, TXA_COMP_AV and TXA_CFC_AV.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25: D14 / R-R3-49: every WDSP-read TX binding (MIC, EQ,
//               Leveler, Leveler gain, CFC, CFC gain, COMP, ALC, ALC gain,
//               ALC group) is polled and shows Thetis's reading
//               (thetisTxReading: CalculateTXMeter's alcgain and sign, then
//               the console.cs floors). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13): a remote window's
//               ALC and MIC bindings from the Core's `txState` while it
//               transmits; the transmit meters the Core does not send are
//               disabled with the reason. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25 - R-R3-32 (remote-window parity Task 6): HwVolts, HwAmps
//                 and HwTemperature fed from RadioModel::paReadings() on
//                 every poll (console.cs:47061-47068 [v2.10.3.15]).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-13 / R-R3-49 (remote-window parity Task 15): a
//                 remote window's ADC Peak, ADC Average, AGC Gain, AGC Peak
//                 and AGC Average bindings read the Core's slice readings
//                 (meterReadingsVersion 1), no reading below it; the AGC
//                 Gain binding shows Thetis's 0 - RXA_AGC_GAIN in both
//                 windows (console.cs:46914 [v2.10.3.15]). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - Trunk merge of remote transmit (R-R3-49, R-IOS-13): on a
//                 connected Core below meterReadingsVersion 1 those five
//                 bindings are shown disabled with the reason
//                 (setBindingUnavailable), as Task 39's transmit meters are.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-R3-49 (remote-window parity Task 33): a remote window's
//                 power, reflected power and SWR reach its RadioStatus as the
//                 Core read them (setPowerReadings, SWR included); the
//                 S-meter's Level and Compression TX modes follow the MIC and
//                 COMP readings handed out (local and remote alike), and a
//                 remote window's S-meter shows each TX mode its Core does
//                 not send with the reason; panMaxBinSource, a remote
//                 window's Max Bin from the slice's own pan. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - A9 (iPhone app plan Task 39): a remote window's EQ,
//                 Leveler, Leveler gain, CFC, CFC gain, ALC gain and ALC
//                 group meters read the Core's stage readings
//                 (txReadingsVersion 3); below it they name the reason.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include "MeterPoller.h"
#include "MeterWidget.h"

#include <algorithm>  // std::clamp (Task 3.1 setIntervalMs / setAverageWindow)
#include "MeterItem.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"
#include "core/RadioStatus.h"
#include "core/LogCategories.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
// Task 41 (Phase 3P-II): SMeterWidget + WdspEngine for the pollSMeter() path.
#include "gui/SMeterWidget.h"
#include "gui/SpectrumWidget.h"
#include "core/WdspEngine.h"
// Parity Task 15: the AGC Gain reading both windows show.
#include "core/meters/SliceMeterPump.h"
#include "core/session/TransmitStateFacade.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <cmath>
#include <QJsonDocument>
#include <QScopeGuard>

// WDSP GetTXAMeter — lock-free TX meter read.
// From Thetis dsp.cs:390-391 [v2.10.3.13]:
//   [DllImport("wdsp.dll")] extern double GetTXAMeter(int channel, txaMeterType meter);
// NereusSDR declaration in src/core/wdsp_api.h.
#ifdef HAVE_WDSP
#include "core/wdsp_api.h"
#endif

namespace NereusSDR {

MeterPoller::MeterPoller(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_timer.setInterval(100);  // 10 fps default (from Thetis UpdateInterval=100ms)
    connect(&m_timer, &QTimer::timeout, this, &MeterPoller::poll);
}

MeterPoller::~MeterPoller() = default;

void MeterPoller::setRxChannel(RxChannel* channel)
{
    if (m_rxChannel == channel) { return; }
    disconnect(m_rxDestroyed);
    m_rxChannel = channel;
    // Adapted contexts track their actual slice, independent of legacy focus.
    if (!m_rxReadingSource || !m_rxSourceIdentitySource) { invalidateReadings(true, false, false); }
    if (channel) {
        m_rxDestroyed = connect(channel, &QObject::destroyed, this, [this] {
            // The legacy focused wrapper is not the source of every adapted
            // context. Slice/stream identity and link readiness govern those.
            if (!m_rxReadingSource || !m_rxSourceIdentitySource) { invalidateReadings(true, false, false); }
        });
    }
    refreshBindingSupport();
    qCDebug(lcMeter) << "MeterPoller: RxChannel set, channelId:"
                      << (channel ? channel->channelId() : -1);
}

void MeterPoller::setLocalRxReadingAvailable(bool available)
{
    if (m_localRxReadingAvailable == available) { return; }
    m_localRxReadingAvailable = available;
    invalidateReadings(true, false, false);
}

// H.2 (Phase 3M-1a): store non-owning pointer to the TX channel.
// WdspEngine owns the object; call setTxChannel(nullptr) on radio disconnect.
void MeterPoller::setTxChannel(TxChannel* channel)
{
    disconnect(m_txDestroyed);
    m_txChannel = channel;
    invalidateTxAudioReadings();
    if (channel) { m_txDestroyed = connect(channel, &QObject::destroyed, this, [this] {
        invalidateTxAudioReadings();
        for (int binding = MeterBinding::TxMic; binding <= MeterBinding::TxCfcPeak; ++binding) { publishAvailability(binding, tr("No transmit source.")); }
    }); }
    for (int binding = MeterBinding::TxMic; binding <= MeterBinding::TxCfcPeak; ++binding) {
        publishAvailability(binding, channel ? QString() : tr("No transmit source."));
    }
    refreshBindingSupport();
    qCDebug(lcMeter) << "MeterPoller: TxChannel set, channelId:"
                      << (channel ? channel->channelId() : -1);
}

// Task 41 (Phase 3P-II): store non-owning pointer to the analog SMeterWidget.
// Call with nullptr on panel destruction (QPointer auto-clears on widget delete).
SMeterWidget* MeterPoller::smeterForTest() const { return m_sMeter; }

void MeterPoller::setSMeter(SMeterWidget* widget)
{
    m_sMeter = widget;
    qCDebug(lcMeter) << "MeterPoller: SMeterWidget set:" << (widget ? "yes" : "nullptr");
    // Parity Task 33: a remote window's S-meter learns which TX modes its
    // Core cannot feed.
    refreshRemoteTxAvailability(/*force=*/true);
}

// Task 41 (Phase 3P-II): store non-owning pointer to WdspEngine for getMaxBinDbm.
// RadioModel owns the engine; call with nullptr on disconnect if needed.
void MeterPoller::setWdspEngine(WdspEngine* engine)
{
    m_wdspEngine = engine;
    invalidateReadings(true, false, false);
    qCDebug(lcMeter) << "MeterPoller: WdspEngine set:" << (engine ? "yes" : "nullptr");
}

void MeterPoller::setRemoteRadioModel(RadioModel* model,
                                     std::function<bool()> snapshotReady,
                                     std::function<double(const SliceModel*)> maxBinSource)
{
    for (const auto& connection : m_remoteConnections) { disconnect(connection); }
    m_remoteConnections.clear();
    invalidateReadings(true, true, true);
    m_remoteRole = model && model->role() == RadioModel::Role::Remote;
    m_remoteModel = m_remoteRole ? model : nullptr;
    m_remoteSnapshotReady = std::move(snapshotReady);
    m_remoteMaxBinSource = std::move(maxBinSource);
    m_supportIdentity.clear();
    for(int binding:{MeterBinding::HwVolts,MeterBinding::HwAmps,MeterBinding::TxAlcGain,MeterBinding::TxAlcGroup}) { publishSupport(binding,MeterItem::BindingSupport::Unknown); }
    if (m_remoteModel) {
        m_remoteConnections.append(connect(model, &QObject::destroyed, this, [this] { invalidateReadings(true, true, true); }));
        m_remoteConnections.append(connect(model, &RadioModel::activeSliceChanged, this, [this] {
            if (!m_rxReadingSource || !m_rxSourceIdentitySource) { invalidateReadings(true, false, false); }
        }));
        m_remoteConnections.append(connect(model, &RadioModel::connectionStateChanged, this, [this] { invalidateReadings(true, true, true); }));
    }
    if (!m_remoteRole) {
        const QList<int> previouslyUnavailable = m_availability.keys();
        for (int binding : previouslyUnavailable) { publishAvailability(binding, {}); }
    }
    refreshRemoteMeterReadingsAvailability(true);
    // Task 39: whichever of the two setters runs last marks the meters.
    refreshRemoteTxAvailability(/*force=*/true);
    refreshBindingSupport();
}

void MeterPoller::setRemoteTransmitState(TransmitState* state,
                                         std::function<QString()> unavailableText)
{
    if (m_remoteTransmitState) {
        disconnect(m_remoteTransmitState, nullptr, this, nullptr);
    }
    invalidateReadings(false, true, false);
    m_remoteTransmitState = state;
    m_remoteTransmitUnavailable = std::move(unavailableText);
    if (state != nullptr) {
        connect(state, &QObject::destroyed, this, [this] {
            invalidateReadings(false, true, false);
            for (int b = MeterBinding::TxPower; b <= MeterBinding::TxCfcPeak; ++b) { publishAvailability(b, tr("No transmit source.")); }
        });
        setInTx(state->keyed());
        if (m_remoteModel) {
            m_remoteModel->radioStatus().setPowerReadings(state->forwardPowerWatts(), state->reflectedPowerWatts(), state->swr());
            publishGlobalReading(MeterBinding::TxPower, state->forwardPowerWatts());
            publishGlobalReading(MeterBinding::TxReversePower, state->reflectedPowerWatts());
            publishGlobalReading(MeterBinding::TxSwr, state->swr());
        }
        // Power, reflected power and SWR go into this window's RadioStatus,
        // which the S-meter's TX needle, the TX applet's power gauge and the
        // container meters already follow (setRadioStatus). The local
        // window's RadioModel fills it from the radio; a remote window's
        // model has no radio, so the Core's readings fill it here.
        connect(state, &TransmitState::metersChanged, this, [this]() {
            TransmitState* s = m_remoteTransmitState.data();
            RadioModel* model = m_remoteModel.data();
            if (s == nullptr || model == nullptr) {
                return;
            }
            // Parity Task 33 (R-R3-49): the three as the Core read them,
            // SWR included, as one sample.
            model->radioStatus().setPowerReadings(s->forwardPowerWatts(),
                                                  s->reflectedPowerWatts(), s->swr());
        });
        // The local window switches on MoxController's walk; a remote
        // window's controller never keys, so the Core's keyed state does.
        connect(state, &TransmitState::stateChanged, this, [this]() {
            if (TransmitState* s = m_remoteTransmitState.data()) {
                setInTx(s->keyed());
            }
        });
    } else {
        for (int b = MeterBinding::TxPower; b <= MeterBinding::TxCfcPeak; ++b) { publishAvailability(b, tr("No transmit source.")); }
    }
    refreshRemoteTxAvailability(/*force=*/true);
}

const QList<int>& MeterPoller::remoteTxBindingsNotSent()
{
    // Task 39: txState v1 carries forward and reflected power, SWR, ALC and
    // MIC; parity Task 33's txReadingsVersion 1 adds COMP (compressionDb).
    // A9: txReadingsVersion 3 adds these seven, a Core below it keeps them.
    static const QList<int> bindings{
        MeterBinding::TxEq,       MeterBinding::TxLeveler, MeterBinding::TxLevelerGain,
        MeterBinding::TxCfc,      MeterBinding::TxCfcGain,
        MeterBinding::TxAlcGain,  MeterBinding::TxAlcGroup,
    };
    return bindings;
}

QString MeterPoller::remoteTxMeterNotSentText()
{
    // A9: only a Core below txReadingsVersion 3 leaves these out now.
    return TransmitState::txReadingNotSentText();
}

void MeterPoller::setRemoteTxStageReadingsAvailable(std::function<bool()> available)
{
    for (int binding : remoteTxBindingsNotSent()) { publishGlobalReading(binding, kNoMeterReadingDbm); }
    m_remoteTxStageReadingsAvailable = std::move(available);
    refreshRemoteTxAvailability(/*force=*/true);
    refreshBindingSupport();
}

bool MeterPoller::remoteTxStageReadingsAvailable() const
{
    return m_remoteTxStageReadingsAvailable && m_remoteTxStageReadingsAvailable();
}

QString MeterPoller::remoteTransmitUnavailableText() const
{
    return m_remoteTransmitUnavailable ? m_remoteTransmitUnavailable() : QString();
}

void MeterPoller::refreshRemoteTxAvailability(bool force)
{
    if (!m_remoteRole || !m_remoteTransmitState) {
        return;
    }
    const QString unavailable = remoteTransmitUnavailableText();
    const bool readings = remoteTxReadingsAvailable();
    const bool stages = remoteTxStageReadingsAvailable();
    if (!force && m_remoteTxAvailabilityShown && unavailable == m_remoteTxUnavailableShown
        && readings == m_remoteTxReadingsShown && stages == m_remoteTxStageReadingsShown) {
        return;
    }
    m_remoteTxAvailabilityShown = true;
    m_remoteTxUnavailableShown = unavailable;
    m_remoteTxReadingsShown = readings;
    m_remoteTxStageReadingsShown = stages;
    // Parity Task 33 follow-up: the COMP reading comes with the Core's
    // transmit readings (txReadingsVersion 1).
    const QString compReason = readings ? QString() : TransmitState::txReadingNotSentText();
    const QList<int>& notSent = remoteTxBindingsNotSent();
    for (int bindingId = MeterBinding::TxPower; bindingId <= MeterBinding::TxCfcPeak; ++bindingId) {
        QString reason = unavailable;
        if (reason.isEmpty() && !stages && notSent.contains(bindingId)) { reason = remoteTxMeterNotSentText(); }
        if (reason.isEmpty() && remoteTxPeakBindingsNotSent().contains(bindingId)) { reason = remoteTxMeterNotSentText(); }
        if (reason.isEmpty() && bindingId == MeterBinding::TxComp) { reason = compReason; }
        publishAvailability(bindingId, reason);
        if (!reason.isEmpty()) { publishGlobalReading(bindingId, kNoMeterReadingDbm); }
    }
    // R-R3-49 (parity Task 33): the S-meter's TX modes from the same
    // readings a local window's read: Power and SWR txState's
    // forwardPowerWatts and swr (through this window's RadioStatus), Level
    // its micLevelDb (the MIC reading, TxMic) and Compression its
    // compressionDb (the COMP reading, TxComp), as handOutTxReading feeds
    // the local widget. Compression waits for txReadingsVersion 1.
    if (SMeterWidget* sm = m_sMeter.data()) {
        for (SMeterWidget::TxMode mode : {SMeterWidget::TxMode::Power, SMeterWidget::TxMode::SWR,
                                          SMeterWidget::TxMode::Level,
                                          SMeterWidget::TxMode::Compression}) {
            QString reason = unavailable;
            if (reason.isEmpty() && mode == SMeterWidget::TxMode::Compression) {
                reason = compReason;
            }
            sm->setTxModeUnavailable(mode, reason);
        }
    }
}

void MeterPoller::pollRemoteTxMeters()
{
    TransmitState* state = m_remoteTransmitState.data();
    const bool ready = m_remoteModel && m_remoteModel->isConnected()
        && m_remoteSnapshotReady && m_remoteSnapshotReady();
    if (!state || !ready || !remoteTransmitUnavailableText().isEmpty()) {
        invalidateReadings(false, true, false);
        return;
    }
    if (!m_inTx) { return; }
    // The Core's readings, already worked as Thetis shows them
    // (TxMeterPump: thetisTxReading ALC and MIC).
    handOutTxReading(MeterBinding::TxAlc, state->alcDb());
    handOutTxReading(MeterBinding::TxMic, state->micLevelDb());
    // Parity Task 33 follow-up: the COMP reading, from a Core that sends it.
    if (remoteTxReadingsAvailable()) {
        handOutTxReading(MeterBinding::TxComp, state->compressionDb());
    }
    // A9: the seven container stage readings, from a Core that sends them
    // (txReadingsVersion 3), already worked as the local poll works its
    // own transmit channel's (thetisTxReading, kTxReadings).
    if (remoteTxStageReadingsAvailable()) {
        handOutTxReading(MeterBinding::TxEq, state->eqDb());
        handOutTxReading(MeterBinding::TxLeveler, state->levelerDb());
        handOutTxReading(MeterBinding::TxLevelerGain, state->levelerGainDb());
        handOutTxReading(MeterBinding::TxCfc, state->cfcDb());
        handOutTxReading(MeterBinding::TxCfcGain, state->cfcGainDb());
        handOutTxReading(MeterBinding::TxAlcGain, state->alcGainDb());
        handOutTxReading(MeterBinding::TxAlcGroup, state->alcGroupDb());
    }
}

void MeterPoller::setRemoteTxReadingsAvailable(std::function<bool()> available)
{
    publishGlobalReading(MeterBinding::TxComp, kNoMeterReadingDbm);
    m_remoteTxReadingsAvailable = std::move(available);
    refreshRemoteTxAvailability(/*force=*/true);
}

bool MeterPoller::remoteTxReadingsAvailable() const
{
    return m_remoteTxReadingsAvailable && m_remoteTxReadingsAvailable();
}

void MeterPoller::setRemoteMeterReadingsAvailable(std::function<bool()> available)
{
    invalidateReadings(true, false, false);
    m_remoteMeterReadingsAvailable = std::move(available);
    refreshRemoteMeterReadingsAvailability(/*force=*/true);
}

const QList<int>& MeterPoller::remoteMeterReadingBindings()
{
    static const QList<int> bindings{
        MeterBinding::AdcPeak, MeterBinding::AdcAvg, MeterBinding::AgcGain,
        MeterBinding::AgcPeak, MeterBinding::AgcAvg,
    };
    return bindings;
}

QString MeterPoller::remoteMeterReadingsNotSentText()
{
    return tr("This Core does not send this meter. Update the Core to see it here.");
}

void MeterPoller::refreshRemoteMeterReadingsAvailability(bool force)
{
    if (!m_remoteRole) {
        return;
    }
    // Trunk merge (R-R3-49): only a connected Core can be below version 1;
    // with no Core the meters show no reading ("--"), as the signal meters do.
    const bool connected = m_remoteModel && m_remoteModel->isConnected()
        && m_remoteSnapshotReady && m_remoteSnapshotReady();
    const bool sends = m_remoteMeterReadingsAvailable && m_remoteMeterReadingsAvailable();
    const QString reason = connected && !sends ? remoteMeterReadingsNotSentText() : QString();
    if (!force && m_remoteReadingsAvailabilityShown && reason == m_remoteReadingsUnavailableShown) {
        return;
    }
    m_remoteReadingsAvailabilityShown = true;
    m_remoteReadingsUnavailableShown = reason;
    for (int bindingId : remoteMeterReadingBindings()) { publishAvailability(bindingId, reason); }
}

// RX meter cal offset source (Thetis-faithful port).
//
// Stores a std::function returning the current dB offset.  Called once per
// pollSMeter() and the SignalPeak/SignalAvg loop in poll() to add the
// cumulative RXOffset(rx) value to WDSP-sourced readings before display.
//
// Thetis equivalent: console.cs:46821 [v2.10.3.13]:
//   float offset = RXOffset(1);
// where RXOffset = RXPreampOffset + RXCalibrationOffset.
//
// Set by MainWindow once during RadioModel wiring; refreshed at every poll
// tick (the callable is cheap; see RadioModel::rxMeterOffsetDb).
void MeterPoller::setRxOffsetSource(std::function<double()> source)
{
    invalidateReadings(true, false, false);
    m_rxOffsetSource = std::move(source);
    qCDebug(lcMeter) << "MeterPoller: rxOffsetSource set:"
                      << (m_rxOffsetSource ? "yes" : "nullptr");
}

// H.2 (Phase 3M-1a): switch poll set on MOX engage/release.
// Porting from Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter dispatch:
//   the switch on MeterType selects TX vs RX meter reads.
// NereusSDR translates the dispatch to a bool flag set at MOX boundary.
void MeterPoller::setInTx(bool isTx)
{
    if (m_inTx == isTx) {
        return;  // idempotent
    }
    m_inTx = isTx;
    qCDebug(lcMeter) << "MeterPoller: TX mode" << (isTx ? "on" : "off");

    for (const auto& guarded : m_targets) {
        if (MeterWidget* target = guarded.data()) { target->resetForTxTransition(isTx); }
    }
    // TX values from the preceding keying must never seed a recreated face.
    for (auto it = m_globalReadings.begin(); it != m_globalReadings.end();) {
        if (it.key() >= MeterBinding::TxPower && it.key() <= MeterBinding::TxCfcPeak) { it = m_globalReadings.erase(it); }
        else { ++it; }
    }
}

namespace {
QByteArray contextKey(const QJsonObject& context) { return QJsonDocument(context).toJson(QJsonDocument::Compact); }
}
void MeterPoller::rememberPresentation(const MeterWidget* widget)
{
    if (!widget) { return; }
    m_unitMode = widget->unitMode();
    if (widget->powerScale() > 0) { m_powerScale = widget->powerScale(); }
}
void MeterPoller::addTarget(MeterWidget* widget)
{
    if (!widget) { return; }
    m_targets.removeAll(QPointer<MeterWidget>(nullptr));
    for (const auto& target : m_targets) {
        if (target == widget) { return; }
        if (target) { rememberPresentation(target); }
    }
    m_targets.append(widget);
    connect(widget, &MeterWidget::itemAdded, this, [this, widget](MeterItem* item) {
        if (item && item->hasMmioBinding()) { replayMmioReading(widget, item); }
    });
    connect(widget, &MeterWidget::aboutToDestroy, this, [this, widget] { removeTarget(widget); });
    connect(widget, &QObject::destroyed, this, [this, widget] {
        m_targetContexts.remove(widget);
        m_targets.removeAll(QPointer<MeterWidget>(nullptr));
    });
    refreshRemoteTxAvailability(true);
    refreshRemoteMeterReadingsAvailability(true);
    replayReadings(widget, m_targetContexts.value(widget));
}
void MeterPoller::removeTarget(MeterWidget* widget)
{
    // aboutToDestroy removes registered widgets before their members die.
    // Later QObject::destroyed callers supply raw identity only; it is safe
    // even when Qt has not cleared a QWidget QPointer yet.
    bool registered = false;
    for (const auto& target : m_targets) {
        if (target && target.data() == widget) { rememberPresentation(target); registered = true; break; }
    }
    m_targets.removeIf([widget](const QPointer<MeterWidget>& target) { return !target || target.data() == widget; });
    m_targets.removeAll(QPointer<MeterWidget>(nullptr));
    m_targetContexts.remove(widget);
    if (registered) { disconnect(widget, nullptr, this, nullptr); }
}
void MeterPoller::setTargetContext(MeterWidget* widget, const QJsonObject& context)
{
    if (!widget) { return; }
    const bool existing = m_targets.contains(widget);
    const bool changed = m_targetContexts.value(widget) != context;
    m_targetContexts.insert(widget, context);
    if (changed) {
        widget->clearReadingCache();
        // Reset source-specific dynamics/history before seeding another slice.
        widget->resetForTxTransition(m_inTx);
        for (int b = MeterBinding::SignalPeak; b <= MeterBinding::PbSnr; ++b) { widget->updateMeterValue(b, kNoMeterReadingDbm); }
    }
    addTarget(widget);
    if (existing && changed) { replayReadings(widget, context); }
}
void MeterPoller::setRxReadingSource(std::function<double(const QJsonObject&, int)> source)
{
    m_rxReadingSource = std::move(source);
    invalidateReadings(true, false, false);
    refreshBindingSupport();
}
void MeterPoller::setRxSourceIdentitySource(std::function<QByteArray(const QJsonObject&)> source)
{
    m_rxSourceIdentitySource = std::move(source);
}
bool MeterPoller::synchronizeRxSource(MeterWidget* widget, const QJsonObject& context) const
{
    if (!widget || !m_rxSourceIdentitySource) { return false; }
    const QByteArray identity = m_rxSourceIdentitySource(context);
    const QVariant previous = widget->property("containerRxSourceIdentity");
    if (previous.isValid() && previous.toByteArray() == identity) { return false; }
    widget->setProperty("containerRxSourceIdentity", identity);
    if (previous.isValid()) { widget->resetRxSource(); }
    return previous.isValid();
}
void MeterPoller::setSessionIdSource(std::function<QString()> source)
{
    m_sessionIdSource = std::move(source);
    m_cachedSessionId = m_sessionIdSource ? m_sessionIdSource() : QString();
    invalidateReadings(false, true, true);
}
void MeterPoller::refreshGlobalSession()
{
    const QString sessionId = m_sessionIdSource ? m_sessionIdSource() : QString();
    if (sessionId == m_cachedSessionId) { return; }
    m_cachedSessionId = sessionId;
    // Publish callbacks can arrive before the next shared poll. Clear the
    // preceding station's samples before accepting the first new sample.
    invalidateReadings(false, true, true);
}
bool MeterPoller::acceptsGlobalReading(const QJsonObject& context) const
{
    const QString requested = context.value("sessionId").toString();
    return requested.isEmpty() || requested == (m_sessionIdSource ? m_sessionIdSource() : QString());
}
QString MeterPoller::globalAvailability(const QJsonObject& context, int binding) const
{
    const bool windowGlobal = (binding >= MeterBinding::TxPower && binding <= MeterBinding::TxCfcPeak)
        || (binding >= MeterBinding::HwVolts && binding <= MeterBinding::HwTemperature);
    return windowGlobal && !acceptsGlobalReading(context)
        ? tr("This meter belongs to another radio session.") : m_availability.value(binding);
}
void MeterPoller::setUnitMode(MeterItem::MeterUnit unit)
{
    m_unitMode = unit;
    for (const auto& target : m_targets) { if (target) { target->setUnitMode(unit); } }
}
void MeterPoller::rescalePowerMeters(int watts)
{
    if (watts <= 0) { return; }
    m_powerScale = watts;
    for (const auto& target : m_targets) { if (target) { target->rescalePowerMeters(watts); } }
}
MeterPoller::MmioReading MeterPoller::mmioReading(const MeterItem* item) const
{
    MmioEndpoint* endpoint = nullptr;
#ifdef NEREUS_BUILD_TESTS
    if (m_mmioEndpointLookup) { endpoint = m_mmioEndpointLookup(item->mmioGuid()); }
    else
#endif
    { endpoint = ExternalVariableEngine::instance().endpoint(item->mmioGuid()); }
    if (!endpoint) { return {kNoMeterReadingDbm, tr("External meter source is unavailable."),MeterItem::BindingSupport::Unknown}; }
    bool ok = false;
    const double value = endpoint->valueForName(item->mmioVariable()).toDouble(&ok);
    if (!ok || !std::isfinite(value)) { return {kNoMeterReadingDbm, tr("External meter variable has no numeric reading."),MeterItem::BindingSupport::Supported}; }
    return {value, {},MeterItem::BindingSupport::Supported};
}
void MeterPoller::replayMmioReading(MeterWidget* widget, MeterItem* item) const
{
    // Existing endpoint cache only: no endpoint factory, transport or timer.
    // Validate at reconstruction so removal/new non-numeric data between
    // shared ticks cannot revive a stale same-identity sample.
    const MmioReading reading = mmioReading(item);
    widget->updateMmioValue(item, reading.value, reading.reason,reading.support);
}
void MeterPoller::replayReadings(MeterWidget* widget, const QJsonObject& context) const
{
    if (!widget) { return; }
    widget->clearReadingCache();
    widget->resetForTxTransition(m_inTx);
    widget->setUnitMode(m_unitMode);
    widget->rescalePowerMeters(m_powerScale);
    copyCachedReadings(widget, context);
}
void MeterPoller::copyCachedReadings(MeterWidget* widget, const QJsonObject& context) const
{
    if (!widget) { return; }
    synchronizeRxSource(widget, context);
    if (widget->unitMode()!=m_unitMode) { widget->setUnitMode(m_unitMode); }
    if (m_powerScale>0 && widget->powerScale()!=m_powerScale) { widget->rescalePowerMeters(m_powerScale); }
    for(auto it=m_bindingSupport.cbegin();it!=m_bindingSupport.cend();++it) {
        const bool global=it.key()>=MeterBinding::TxPower;
        widget->setBindingSupport(it.key(),global && !acceptsGlobalReading(context)?MeterItem::BindingSupport::Unknown:it.value());
    }
    for (auto it = m_availability.cbegin(); it != m_availability.cend(); ++it) { widget->setBindingUnavailable(it.key(), globalAvailability(context,it.key())); }
    const auto readings = m_contextReadings.value(contextKey(context));
    const bool remoteReady = !m_remoteRole || (m_remoteModel && m_remoteModel->isConnected() && m_remoteSnapshotReady && m_remoteSnapshotReady());
    for (int b = MeterBinding::SignalPeak; b <= MeterBinding::PbSnr; ++b) {
        const double value = remoteReady && m_localRxReadingAvailable && !m_inTx
            ? (m_rxReadingSource ? m_rxReadingSource(context,b) : readings.value(b,kNoMeterReadingDbm)) : kNoMeterReadingDbm;
        widget->updateMeterValue(b, std::isfinite(value) ? value : kNoMeterReadingDbm);
    }
    const bool sameCachedSession = m_cachedSessionId == (m_sessionIdSource ? m_sessionIdSource() : QString());
    for (auto it = m_globalReadings.cbegin(); it != m_globalReadings.cend(); ++it) { widget->updateMeterValue(it.key(), remoteReady && sameCachedSession && acceptsGlobalReading(context) ? it.value() : kNoMeterReadingDbm); }
    for (int b = MeterBinding::TxPower; b <= MeterBinding::TxCfcPeak; ++b) {
        widget->setBindingUnavailable(b,globalAvailability(context,b));
        if (!m_globalReadings.contains(b)) { widget->updateMeterValue(b, kNoMeterReadingDbm); }
    }
    for (int b = MeterBinding::HwVolts; b <= MeterBinding::HwTemperature; ++b) {
        widget->setBindingUnavailable(b,globalAvailability(context,b));
        if (!m_globalReadings.contains(b)) { widget->updateMeterValue(b, kNoMeterReadingDbm); }
    }
    for (MeterItem* item : widget->items()) { if (item->hasMmioBinding()) { replayMmioReading(widget, item); } }
}
void MeterPoller::publishContextReading(const QJsonObject& context, int binding, double value)
{
    // A configured adapter owns even the default context. Legacy active-source
    // producers must not overwrite it after this context pass.
    if (m_rxReadingSource && context.isEmpty()) { return; }
    m_knownContexts[contextKey(context)] = context;
    m_contextReadings[contextKey(context)][binding] = value;
    for (const auto& target : m_targets) {
        if (target && m_targetContexts.value(target) == context) { target->updateMeterValue(binding, value); }
    }
    emit readingUpdated(context, binding, value);
}
void MeterPoller::publishGlobalReading(int binding, double value)
{
    refreshGlobalSession();
    m_globalReadings[binding] = value;
    QSet<QByteArray> contexts;
    for (const auto& target : m_targets) {
        if (!target) { continue; }
        const QJsonObject context = m_targetContexts.value(target);
        const double delivered = acceptsGlobalReading(context) ? value : kNoMeterReadingDbm;
        target->setBindingUnavailable(binding,globalAvailability(context,binding));
        target->updateMeterValue(binding, delivered);
        if (!contexts.contains(contextKey(context))) { contexts.insert(contextKey(context)); emit readingUpdated(context, binding, delivered); }
    }
    for (auto it = m_knownContexts.cbegin(); it != m_knownContexts.cend(); ++it) {
        if (!contexts.contains(it.key())) {
            contexts.insert(it.key()); emit readingUpdated(it.value(), binding, acceptsGlobalReading(it.value()) ? value : kNoMeterReadingDbm);
        }
    }
    if (!contexts.contains(contextKey({}))) { emit readingUpdated({}, binding, value); }
}
void MeterPoller::publishAvailability(int binding, const QString& reason)
{
    const bool changed = !m_availability.contains(binding) || m_availability.value(binding) != reason;
    m_availability[binding] = reason;
    if (changed) { emit bindingAvailabilityChanged(binding, reason); }
    for (const auto& target : m_targets) { if (target) { target->setBindingUnavailable(binding, globalAvailability(m_targetContexts.value(target),binding)); } }
}
void MeterPoller::publishSupport(int binding,MeterItem::BindingSupport support)
{
    if(m_bindingSupport.contains(binding) && bindingSupport(binding)==support) { return; }
    const bool changed=bindingSupport(binding)!=support;
    m_bindingSupport[binding]=support;
    if(changed) { m_globalReadings.remove(binding); }
    for(const auto& target:m_targets) {
        if(target) {
            const bool foreign=binding>=MeterBinding::TxPower && !acceptsGlobalReading(m_targetContexts.value(target));
            target->setBindingSupport(binding,foreign?MeterItem::BindingSupport::Unknown:support);
        }
    }
    if(changed) { emit bindingSupportChanged(binding,support); }
}
void MeterPoller::refreshBindingSupport()
{
    using Support=MeterItem::BindingSupport;
    RadioModel* model=m_remoteRole?m_remoteModel.data():m_paReadingsModel.data();
    const bool known=model && model->currentRadioInfo().boardType!=HPSDRHW::Unknown && model->hardwareProfile().caps;
    const QString identity=known?QStringLiteral("%1/%2/%3").arg(model->currentRadioInfo().macAddress).arg(int(model->hardwareProfile().effectiveBoard)).arg(int(model->hardwareProfile().model)):QString();
    if(identity!=m_supportIdentity) {
        m_supportIdentity=identity;
        for(int binding:{MeterBinding::HwVolts,MeterBinding::HwAmps,MeterBinding::TxAlcGain,MeterBinding::TxAlcGroup}) { publishSupport(binding,Support::Unknown); }
        invalidateReadings(true,true,true);
    }
    publishSupport(MeterBinding::HwAmps,known?(model->boardCapabilities().hasPaAmpsTelemetry?Support::Supported:Support::Unsupported):Support::Unknown);
    publishSupport(MeterBinding::HwVolts,known?(model->boardCapabilities().hasPaVoltsTelemetry?Support::Supported:Support::Unsupported):Support::Unknown);
    if(m_remoteRole) {
        const bool ready=known && model->isConnected() && m_remoteSnapshotReady && m_remoteSnapshotReady();
        if(ready) {
            const bool stages=m_remoteTxStageReadingsAvailable?remoteTxStageReadingsAvailable():model->stationTxReadingsVersion()>=3;
            for(int binding:{MeterBinding::TxAlcGain,MeterBinding::TxAlcGroup}) { publishSupport(binding,stages?Support::Supported:Support::Unsupported); }
        }
    } else {
#ifdef HAVE_WDSP
        for(int binding:{MeterBinding::TxAlcGain,MeterBinding::TxAlcGroup}) { publishSupport(binding,Support::Supported); }
#else
        for(int binding:{MeterBinding::TxAlcGain,MeterBinding::TxAlcGroup}) {
            publishSupport(binding,Support::Unsupported); publishAvailability(binding,tr("This build has no WDSP transmit stage readings."));
        }
#endif
    }
    const Support rxSupport=(m_rxReadingSource || m_rxChannel || (m_remoteRole && model && model->isConnected() && m_remoteSnapshotReady && m_remoteSnapshotReady()))?Support::Supported:Support::Unknown;
    for(int binding=MeterBinding::SignalPeak;binding<=MeterBinding::PbSnr;++binding) { publishSupport(binding,rxSupport); }
    for(int binding:{MeterBinding::TxPower,MeterBinding::TxSwr}) { publishSupport(binding,m_radioStatus?Support::Supported:Support::Unknown); }
}
void MeterPoller::invalidateReadings(bool rx, bool tx, bool hardware)
{
    if (rx) {
        QSet<QByteArray> contexts;
        for (auto it = m_contextReadings.cbegin(); it != m_contextReadings.cend(); ++it) { contexts.insert(it.key()); }
        m_contextReadings.clear();
        for (const auto& target : m_targets) {
            if (!target) { continue; }
            const QJsonObject context = m_targetContexts.value(target);
            for (int b = MeterBinding::SignalPeak; b <= MeterBinding::PbSnr; ++b) { target->updateMeterValue(b, kNoMeterReadingDbm); }
            contexts.insert(contextKey(context));
        }
        contexts.insert(contextKey({}));
        for (const QByteArray& key : contexts) {
            const QJsonObject context = QJsonDocument::fromJson(key).object();
            for (int b = MeterBinding::SignalPeak; b <= MeterBinding::PbSnr; ++b) { emit readingUpdated(context, b, kNoMeterReadingDbm); }
        }
    }
    if (tx) { for (int b = MeterBinding::TxPower; b <= MeterBinding::TxCfcPeak; ++b) { publishGlobalReading(b, kNoMeterReadingDbm); } }
    if (hardware) { for (int b = MeterBinding::HwVolts; b <= MeterBinding::HwTemperature; ++b) { publishGlobalReading(b, kNoMeterReadingDbm); } }
}
void MeterPoller::invalidateTxAudioReadings()
{
    for (int binding = MeterBinding::TxMic; binding <= MeterBinding::TxCfcPeak; ++binding) {
        publishGlobalReading(binding, kNoMeterReadingDbm);
    }
}
void MeterPoller::pollContextReadings()
{
    if (!m_rxReadingSource || m_inTx) { return; }
    const bool sourceReady = m_localRxReadingAvailable && (!m_remoteRole
        || (m_remoteModel && m_remoteModel->isConnected() && m_remoteSnapshotReady && m_remoteSnapshotReady()));
    // Reset every recipient before the first context's shared sample fan-out.
    for (const auto& target : m_targets) {
        if (!target) { continue; }
        const QJsonObject context = m_targetContexts.value(target);
        if (synchronizeRxSource(target, context)) { m_contextReadings.remove(contextKey(context)); }
    }
    QSet<QByteArray> visited;
    for (const auto& target : m_targets) {
        if (!target) { continue; }
        const QJsonObject context = m_targetContexts.value(target);
        const QByteArray key = contextKey(context);
        m_knownContexts[key] = context;
        if (visited.contains(key)) { continue; }
        visited.insert(key);
        for (int b = MeterBinding::SignalPeak; b <= MeterBinding::PbSnr; ++b) {
            const double v = sourceReady ? m_rxReadingSource(context, b) : kNoMeterReadingDbm;
            const double value = std::isfinite(v) ? v : kNoMeterReadingDbm;
            m_contextReadings[key][b] = value;
            for (const auto& recipient : m_targets) {
                if (recipient && m_targetContexts.value(recipient) == context) { recipient->updateMeterValue(b, value); }
            }
            emit readingUpdated(context, b, value);
        }
    }
}
const QList<int>& MeterPoller::remoteTxPeakBindingsNotSent()
{
    static const QList<int> bindings{MeterBinding::TxMicPeak, MeterBinding::TxAlcPeak,
        MeterBinding::TxCompPeak, MeterBinding::TxEqPeak, MeterBinding::TxLevelerPeak, MeterBinding::TxCfcPeak};
    return bindings;
}

// ── Task 3.1: MultimeterPage-facing interval API ─────────────────────────────
// setIntervalMs / intervalMs: preferred interface for MultimeterPage.
// Clamps to [10..2000] matching the MultimeterPage spinbox range so a
// spurious 0 from a default-constructed AppSettings value can't stall the
// timer.  Corresponds to Thetis udDisplayMeterDelay (display.cs) [v2.10.3.13].
void MeterPoller::setIntervalMs(int ms)
{
    m_timer.setInterval(std::clamp(ms, 10, 2000));
}

int MeterPoller::intervalMs() const
{
    return m_timer.interval();
}

// setAverageWindow: store the averaging window size; full dispatch in Task 3.2.
// Clamps to [1..32] matching MultimeterPage spinbox range.
// From Thetis udDisplayMeterAvg (display.cs) [v2.10.3.13].
void MeterPoller::setAverageWindow(int n)
{
    m_avgWindow = std::clamp(n, 1, 32);
}

int MeterPoller::averageWindow() const
{
    return m_avgWindow;
}

// ── Legacy interval API (backward-compat, delegates to setIntervalMs) ────────
void MeterPoller::setInterval(int ms)
{
    m_timer.setInterval(ms);
}

int MeterPoller::interval() const
{
    return m_timer.interval();
}

void MeterPoller::start()
{
    // Phase 3G-6 block 5: poll runs regardless of m_rxChannel so
    // MMIO-bound items update even before a radio is connected.
    m_timer.start();
    qCDebug(lcMeter) << "MeterPoller: started at" << m_timer.interval() << "ms";
}

void MeterPoller::stop()
{
    m_timer.stop();
    qCDebug(lcMeter) << "MeterPoller: stopped";
}

void MeterPoller::poll()
{
    refreshGlobalSession();
    refreshBindingSupport();
    const qint64 timestamp = m_monotonicSource ? m_monotonicSource() : m_clock.elapsed();
    const auto frame = qScopeGuard([this, timestamp] {
        for (const auto& target : m_targets) { if (target) { target->advanceMeters(timestamp); } }
        emit frameAdvanced(timestamp);
    });
    pollContextReadings();
    // Phase 3G-6 block 5: MMIO item polling is independent of the
    // RX channel, so this branch runs even when m_rxChannel is
    // unset (e.g. no radio connected). For every target widget,
    // walk its items and push the latest value from the bound
    // endpoint's variable cache into each item with an MMIO
    // binding.
    QSet<QString> sampled;
    for (const auto& guarded : m_targets) {
        MeterWidget* target = guarded.data();
        if (!target) { continue; }
        for (MeterItem* item : target->items()) {
            if (!item || !item->hasMmioBinding()) { continue; }
            const QString key = item->mmioSourceKey();
            if (!sampled.contains(key)) {
                sampled.insert(key);
                const MmioReading reading = mmioReading(item);
                m_mmioReadings[key] = reading;
                emit mmioReadingUpdated(item->mmioGuid(), item->mmioVariable(), reading.value, reading.reason);
            }
            const MmioReading reading = m_mmioReadings.value(key);
            target->updateMmioValue(item, reading.value, reading.reason,reading.support);
        }
    }

    // R-R3-32 (parity Task 6): the PA readings reach their meters in every
    // window and every state, transmitting or not.
    pollHardwareTelemetry();

    // R3: a remote window must never fall through to the inactive local
    // DSP, including after its model has been destroyed or disconnected.
    if (m_remoteRole) {
        // Task 39: the Core's transmit meters, and which it cannot send.
        refreshRemoteTxAvailability();
        // Trunk merge (R-R3-49): the ADC and AGC meters an older Core
        // does not send.
        refreshRemoteMeterReadingsAvailability();
        pollRemoteTxMeters();
        pollRemoteRxMeters();
        return;
    }

    // H.2 (Phase 3M-1a): when MOX is active, switch to TX meter polling.
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter — the switch
    // on MeterType dispatches TX vs RX reads from the same timer tick.
    if (m_inTx) {
        pollTxMeters();
        return;  // don't poll RX meters while transmitting
    }

    if (m_rxReadingSource) { pollAdaptedSMeter(); return; }

    // NereusSDR (R-R3-13): with no RX channel (never created, or destroyed:
    // the QPointer clears) there is no reading, and with the radio link not
    // up (LinkLost keeps the channels alive, but their meters stop and an
    // inactive one reads -140 dBm) there is none either.  Feed the -400 dBm
    // sentinel to the RX bindings this loop drives and to the analog
    // S-meter header, so each shows "--" rather than the last value.
    // MeterWidget::updateMeterValue and SMeterWidget::setLevel drop repeats.
    if (!m_rxChannel || !m_localRxReadingAvailable) {
        for (int bindingId = MeterBinding::SignalPeak;
             bindingId <= MeterBinding::AgcAvg; ++bindingId) {
            publishContextReading({}, bindingId, kNoMeterReadingDbm);
        }
        if (SMeterWidget* sm = m_sMeter.data()) {
            sm->setLevel(static_cast<float>(kNoMeterReadingDbm));
        }
        return;
    }

    // Thetis-faithful RX meter cal offset for the SignalPeak / SignalAvg
    // bindings (RXA_S_PK / RXA_S_AV).  ADC_PK / ADC_AV / AGC_PK / AGC_AV /
    // AGC_GAIN bindings do NOT take the offset (Thetis console.cs:46831-
    // 46835 [v2.10.3.13] omit +offset for those exact rows.
    // Cite: console.cs:46821 -> float offset = RXOffset(1);
    //       console.cs:46824 -> ... = CalculateRXMeter(...) + offset;  // SIGNAL_STRENGTH
    //       console.cs:46828 -> ... = CalculateRXMeter(...) + offset;  // AVG_SIGNAL_STRENGTH
    const double rxOffsetDb = m_rxOffsetSource ? m_rxOffsetSource() : 0.0;

    // Poll all RX meter types. R-R3-39: RxChannel::getMeter reads the cache
    // the receive lane refreshes (GetRXAMeter takes a meter lock, so it
    // never runs on this, the GUI, thread); with no lane it reads WDSP.
    double smeterDbm = -140.0;
    for (int bindingId = MeterBinding::SignalPeak;
         bindingId <= MeterBinding::AgcAvg; ++bindingId) {
        double value = m_rxChannel->getMeter(static_cast<RxMeterType>(bindingId));
        // Apply RXOffset to SignalPeak / SignalAvg only (matches Thetis
        // console.cs:46824 + :46828, NOT :46831-:46835).
        if (bindingId == MeterBinding::SignalPeak
         || bindingId == MeterBinding::SignalAvg) {
            value += rxOffsetDb;
        }
        // R-R3-13 (parity Task 15): the AGC Gain meter shows Thetis's
        // reading, 0 - RXA_AGC_GAIN (console.cs:46914 [v2.10.3.15]), the
        // value the Core sends a remote window (SliceMeterPump).
        if (bindingId == MeterBinding::AgcGain) {
            value = SliceMeterPump::thetisAgcGainReading(value);
        }
        if (bindingId == MeterBinding::SignalAvg) {
            smeterDbm = value;   // post-offset; matches VfoWidget expectation
        }
        publishContextReading({}, bindingId, value);
    }

    // Task 41 (Phase 3P-II): drive the analog SMeterWidget header.
    //
    // Remote Daemon R2 Task 12: the per-slice pass that used to run here
    // (pollSliceSMeters(), emitting sliceSmeterUpdated for every flag) has
    // moved to SliceMeterPump (src/core/meters/), a separate core-side
    // QTimer owned by RadioModel -- GUI-only, this poller could never run
    // for a headless nereusd, so the per-slice S-meter reading a remote
    // GUI's mirror carries had no producer there. Every flag now listens
    // to its own SliceModel::signalStrengthDbmChanged directly
    // (MainWindow.cpp's createSliceFlag) instead of a signal from this
    // class. pollSMeter() below is unchanged: it still drives the analog
    // SMeterWidget header, and MainWindow wires the SAME rxMode()
    // selector into SliceMeterPump so the flags and the analog needle
    // never disagree on source.
    Q_UNUSED(smeterDbm);
    pollSMeter();
}

void MeterPoller::setPaReadingsModel(RadioModel* model)
{
    for(const auto& connection:m_supportConnections) { disconnect(connection); }
    m_supportConnections.clear();
    disconnect(m_paDestroyed);
    m_paReadingsModel = model;
    m_supportIdentity.clear();
    for(int binding:{MeterBinding::HwVolts,MeterBinding::HwAmps}) { publishSupport(binding,MeterItem::BindingSupport::Unknown); }
    invalidateReadings(false, false, true);
    if (model) {
        m_paDestroyed = connect(model, &QObject::destroyed, this, [this] { m_paReadingsModel=nullptr; invalidateReadings(false,false,true); refreshBindingSupport(); });
        m_supportConnections.append(connect(model,&RadioModel::currentRadioChanged,this,[this]{refreshBindingSupport();}));
        m_supportConnections.append(connect(model,&RadioModel::connectionStateChanged,this,[this]{refreshBindingSupport();}));
        m_supportConnections.append(connect(model,&RadioModel::stationTxReadingsVersionChanged,this,[this]{refreshBindingSupport();}));
    }
    refreshBindingSupport();
}

void MeterPoller::pollHardwareTelemetry()
{
    RadioModel* const model = m_paReadingsModel.data();
    const bool remoteReady = !m_remoteRole || (m_remoteModel && m_remoteModel->isConnected()
        && m_remoteSnapshotReady && m_remoteSnapshotReady());
    if (!model || !remoteReady) {
        for (int b = MeterBinding::HwVolts; b <= MeterBinding::HwTemperature; ++b) { publishGlobalReading(b, kNoMeterReadingDbm); }
        return;
    }
    // From Thetis console.cs:47061-47068 [v2.10.3.15]: the VOLTS and AMPS
    // meter readings are _MKIIPAVolts and _MKIIPAAmps, the same PA volts and
    // amps the status bar shows (console.cs:26216-26239):
    //   if (bNeedVolts) _RX1MeterValues[Reading.VOLTS] = _MKIIPAVolts;
    //   if (bNeedAmps) _RX1MeterValues[Reading.AMPS] = _MKIIPAAmps;
    // NereusSDR takes them from RadioModel::paReadings(), which the System
    // tile's PA row reads too (the G2E's supply volts, as its row shows).
    // HwTemperature (NereusSDR's own binding; Thetis has no temperature
    // reading) is the PA temperature. Absent: the no-reading sentinel.
    const RadioModel::PaReadings readings = model->paReadings();
    const RadioModel::PaRowVolts row = model->paRowVolts();
    const auto valueOf = [](std::optional<double> v) {
        return v ? *v : kNoMeterReadingDbm;
    };
    const double volts = valueOf(row.volts);
    const double amps = valueOf(readings.paCurrentAmps);
    const double temperature = valueOf(readings.paTemperatureCelsius);
    publishGlobalReading(MeterBinding::HwVolts, volts);
    publishGlobalReading(MeterBinding::HwAmps, amps);
    publishGlobalReading(MeterBinding::HwTemperature, temperature);
}

void MeterPoller::pollRemoteRxMeters()
{
    // The station owns calibration and RX meter production. The GUI only
    // chooses a reading and applies the existing widget ballistics.
    const bool ready = m_remoteModel && m_remoteModel->isConnected()
        && m_remoteSnapshotReady && m_remoteSnapshotReady();
    SliceModel* slice = ready ? m_remoteModel->activeSlice() : nullptr;
    if (slice && (m_inTx || m_remoteModel->radioStatus().isTransmitting()
                        || m_remoteModel->transmitModel().isMox())) {
        return;
    }
    auto finiteOr = [](double value, double fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    // NereusSDR (R-R3-13): with no reading (disconnected, snapshot not
    // ready, no slice) the S-meter, the flags and the container meter
    // items (SignalPeak / SignalAvg / SignalMaxBin) all get the -400 dBm
    // no-reading sentinel in every RX mode, which each shows as "--".  The
    // -140 floor below applies only while a slice reading exists.
    constexpr double kNoReadingDbm = kNoMeterReadingDbm;
    const double peak = slice ? finiteOr(slice->signalPeakDbm(), -140.0) : kNoReadingDbm;
    const double average = slice ? finiteOr(slice->signalAverageDbm(), -140.0) : kNoReadingDbm;
    const double maxBin = slice && m_remoteMaxBinSource
        ? finiteOr(m_remoteMaxBinSource(slice), -400.0) : -400.0;
    // R-R3-13 / R-R3-49 (parity Task 15): the ADC and AGC bindings read the
    // Core's readings for the active slice. A Core below
    // meterReadingsVersion 1 sends none, so they show no reading, as they
    // do with no slice; never this window's inactive DSP or a frozen value.
    const SliceModel* readings = slice && m_remoteMeterReadingsAvailable
            && m_remoteMeterReadingsAvailable()
        ? slice : nullptr;
    const auto coreReading = [&](double (SliceModel::*reading)() const) {
        return readings ? finiteOr((readings->*reading)(), kNoReadingDbm) : kNoReadingDbm;
    };
    const double adcPeak = coreReading(&SliceModel::adcPeakDbfs);
    const double adcAverage = coreReading(&SliceModel::adcAverageDbfs);
    const double agcGain = coreReading(&SliceModel::agcGainDb);
    const double agcPeak = coreReading(&SliceModel::agcPeakDb);
    const double agcAverage = coreReading(&SliceModel::agcAverageDb);
    publishContextReading({}, MeterBinding::SignalPeak, peak);
    publishContextReading({}, MeterBinding::SignalAvg, average);
    publishContextReading({}, MeterBinding::SignalMaxBin, maxBin);
    publishContextReading({}, MeterBinding::AdcPeak, adcPeak);
    publishContextReading({}, MeterBinding::AdcAvg, adcAverage);
    publishContextReading({}, MeterBinding::AgcGain, agcGain);
    publishContextReading({}, MeterBinding::AgcPeak, agcPeak);
    publishContextReading({}, MeterBinding::AgcAvg, agcAverage);
    if (!m_sMeter) { return; }
    double level = slice ? peak : kNoReadingDbm;
    switch (m_sMeter->rxMode()) {
    case SMeterWidget::RxMode::SMeter:
    case SMeterWidget::RxMode::SMeterPeak:
        break;
    case SMeterWidget::RxMode::SignalAverage:
        level = slice ? average : kNoReadingDbm;
        break;
    case SMeterWidget::RxMode::MaxBin:
        level = maxBin;
        break;
    }
    if (m_rxReadingSource) { pollAdaptedSMeter(); }
    else { m_sMeter->setLevel(static_cast<float>(level)); }

    // Keep each flag on the same selected meter source as the applet,
    // resolving stable slice IDs on every tick rather than caching channels.
    if (!m_remoteModel) { return; }
    for (const SliceModel* flagSlice : m_remoteModel->slices()) {
        if (!flagSlice) { continue; }
        double flagLevel = kNoReadingDbm;
        if (ready) {
            switch (m_sMeter->rxMode()) {
            case SMeterWidget::RxMode::SMeter:
            case SMeterWidget::RxMode::SMeterPeak:
                flagLevel = flagSlice->signalPeakDbm();
                break;
            case SMeterWidget::RxMode::SignalAverage:
                flagLevel = flagSlice->signalAverageDbm();
                break;
            case SMeterWidget::RxMode::MaxBin:
                flagLevel = m_remoteMaxBinSource ? m_remoteMaxBinSource(flagSlice) : -400.0;
                break;
            }
        }
        emit remoteSliceLevelUpdated(flagSlice->sliceIndex(),
                                     ready ? finiteOr(flagLevel, -140.0) : kNoReadingDbm);
    }
}

void MeterPoller::pollAdaptedSMeter()
{
    if (!m_sMeter || !m_rxReadingSource || m_inTx) { return; }
    if (m_rxSourceIdentitySource) {
        const QByteArray identity = m_rxSourceIdentitySource(m_sMeterContext);
        const QVariant previous = m_sMeter->property("containerRxSourceIdentity");
        if (previous.isValid() && previous.toByteArray() != identity) {
            m_sMeter->setLevel(static_cast<float>(kNoMeterReadingDbm));
        }
        m_sMeter->setProperty("containerRxSourceIdentity", identity);
    }
    int binding = MeterBinding::SignalPeak;
    if (m_sMeter->rxMode()==SMeterWidget::RxMode::SignalAverage) { binding=MeterBinding::SignalAvg; }
    else if (m_sMeter->rxMode()==SMeterWidget::RxMode::MaxBin) { binding=MeterBinding::SignalMaxBin; }
    const double value = m_localRxReadingAvailable ? m_rxReadingSource(m_sMeterContext,binding) : kNoMeterReadingDbm;
    m_sMeter->setLevel(static_cast<float>(std::isfinite(value)?value:kNoMeterReadingDbm));
}

void MeterPoller::pollSMeter()
{
    SMeterWidget* sm = m_sMeter.data();
    if (!sm) { return; }
    if (!m_rxChannel) { return; }

    const int ch = m_rxChannel->channelId();

    // Thetis-faithful RX meter cal offset (Thetis-faithful port).
    // Applied to ALL three RxMode branches (SIGNAL_STRENGTH,
    // AVG_SIGNAL_STRENGTH, SIGNAL_MAX_BIN) to match Thetis console.cs:
    //   :46824 (SIGNAL_STRENGTH)      ... + offset
    //   :46828 (AVG_SIGNAL_STRENGTH)  ... + offset
    //   :46881 (SIGNAL_MAX_BIN)       ... + offset
    // RXOffset = RXPreampOffset + RXCalibrationOffset (console.cs:21040).
    const float rxOffsetDb = m_rxOffsetSource
        ? static_cast<float>(m_rxOffsetSource())
        : 0.0f;

    float dbm = -127.0f;
    switch (sm->rxMode()) {
    case SMeterWidget::RxMode::SMeter:
    case SMeterWidget::RxMode::SMeterPeak:
        // From Thetis Console/dsp.cs:954 [@501e3f5] (CalculateRXMeter):
        //   case MeterType.SIGNAL_STRENGTH: val = GetRXAMeter(channel, RXA_S_PK);
        // The adjacent ADC_REAL case at dsp.cs:959 carries //MW0LGE [2.9.0.7]
        // attribution that we preserve verbatim per GPL inline-tag rule.
        // Display-side offset add per console.cs:46824 [v2.10.3.13]:
        //   _RX1MeterValues[Reading.SIGNAL_STRENGTH] = ... + offset;
        if (m_wdspEngine) {
            dbm = static_cast<float>(m_wdspEngine->getRxaSignalPeak(ch)) + rxOffsetDb;
        } else {
            // Fallback via RxChannel wrapper (RXA_S_PK = RxMeterType::SignalPeak = 0).
            dbm = static_cast<float>(m_rxChannel->getMeter(RxMeterType::SignalPeak)) + rxOffsetDb;
        }
        break;
    case SMeterWidget::RxMode::SignalAverage:
        // From Thetis Console/dsp.cs:957 [@501e3f5] (CalculateRXMeter):
        //   case MeterType.AVG_SIGNAL_STRENGTH: val = GetRXAMeter(channel, RXA_S_AV);
        // The adjacent ADC_REAL case at dsp.cs:959 carries //MW0LGE [2.9.0.7]
        // attribution that we preserve verbatim per GPL inline-tag rule.
        // Display-side offset add per console.cs:46828 [v2.10.3.13]:
        //   _RX1MeterValues[Reading.AVG_SIGNAL_STRENGTH] = ... + offset;
        dbm = static_cast<float>(m_rxChannel->getMeter(RxMeterType::SignalAvg)) + rxOffsetDb;
        break;
    case SMeterWidget::RxMode::MaxBin:
        // GetDetectMaxBin(disp=0) -- single-pan display channel 0.
        // From Thetis Console/dsp.cs:849-850 [@501e3f5] (P/Invoke GetDetectMaxBin).
        // Display-side offset add per console.cs:46881 [v2.10.3.13]:
        //   if (max_bin > -400f)
        //       _RX1MeterValues[Reading.SIGNAL_MAX_BIN] = max_bin + offset;
        // NereusSDR sources MaxBin from FFTEngine via WdspEngine::getMaxBinDbm
        // (single-panadapter assumption; see WdspEngine.cpp:1327).  Same
        // logical reading as Thetis GetDetectMaxBin so the same +offset
        // applies (gates on the -400 sentinel matching Thetis).
        if (m_wdspEngine) {
            const float maxBinRaw = static_cast<float>(m_wdspEngine->getMaxBinDbm(/*disp=*/0));
            if (maxBinRaw > -400.0f) {
                dbm = maxBinRaw + rxOffsetDb;
            } else {
                dbm = maxBinRaw;  // pass through sentinel unchanged
            }
        }
        break;
    }
    sm->setLevel(dbm);
}

// Poll the WDSP TX meters and push Thetis's reading of each to the meter
// widget targets.
//
// D14, R-R3-49: every TX binding a WDSP meter feeds, each worked as Thetis
// works it (thetisTxReading, WdspTypes.h): CalculateTXMeter (dsp.cs:992-1053
// [v2.10.3.15]) and the MOX reading step (console.cs:46969-46986
// [v2.10.3.15]). Before this, ALC showed the raw TXA_ALC_AV without the -30
// floor, ALC gain the raw TXA_ALC_GAIN without alcgain's +3, and MIC, EQ,
// Leveler, Leveler gain, CFC, CFC gain and ALC group were not polled.
//
// Hardware PA meters (forward/reflected/SWR) are driven by the existing
// RadioStatus::powerChanged connection (setRadioStatus()), which is active
// regardless of TX/RX state, as Thetis's PWR reading comes from the
// hardware, not CalculateTXMeter.
//
// Without HAVE_WDSP every WDSP reading is -140.0 (no WDSP channel).
namespace {
struct TxReadingEntry { int bindingId; ThetisTxReading reading; };
// Which of Thetis's readings each NereusSDR binding shows (MeterPoller.h
// names each binding's WDSP meter; MeterItem.cpp its label).
constexpr TxReadingEntry kTxReadings[] = {
    { MeterBinding::TxMicPeak,     ThetisTxReading::MicPk },
    { MeterBinding::TxAlcPeak,     ThetisTxReading::AlcPk },
    { MeterBinding::TxCompPeak,    ThetisTxReading::CompPk },
    { MeterBinding::TxEqPeak,      ThetisTxReading::EqPk },
    { MeterBinding::TxLevelerPeak, ThetisTxReading::LevelerPk },
    { MeterBinding::TxCfcPeak,     ThetisTxReading::CfcPk },
    { MeterBinding::TxMic,         ThetisTxReading::Mic      },   // TXA_MIC_AV
    { MeterBinding::TxEq,          ThetisTxReading::Eq       },   // TXA_EQ_AV
    { MeterBinding::TxLeveler,     ThetisTxReading::Leveler  },   // TXA_LVLR_AV
    { MeterBinding::TxLevelerGain, ThetisTxReading::LvlG     },   // TXA_LVLR_GAIN
    { MeterBinding::TxCfc,         ThetisTxReading::CfcAv    },   // TXA_CFC_AV
    { MeterBinding::TxCfcGain,     ThetisTxReading::CfcG     },   // TXA_CFC_GAIN
    { MeterBinding::TxComp,        ThetisTxReading::Comp     },   // TXA_COMP_AV
    { MeterBinding::TxAlc,         ThetisTxReading::Alc      },   // TXA_ALC_AV
    { MeterBinding::TxAlcGain,     ThetisTxReading::AlcG     },   // TXA_ALC_GAIN + 3
    { MeterBinding::TxAlcGroup,    ThetisTxReading::AlcGroup },   // ALC_PK + ALC_G
};
} // namespace

double MeterPoller::txReadingForBinding(int bindingId,
                                        const std::function<double(TxMeterType)>& readRaw)
{
    for (const TxReadingEntry& entry : kTxReadings) {
        if (entry.bindingId == bindingId) {
            return thetisTxReading(entry.reading, readRaw);
        }
    }
    return -400.0;
}

void MeterPoller::pollTxMeters()
{
    if (!m_txChannel) {
        for (const TxReadingEntry& entry : kTxReadings) { publishGlobalReading(entry.bindingId, kNoMeterReadingDbm); }
        return;  // no TX channel yet (WDSP not initialized or disconnected)
    }

    // R-R3-39: TxChannel::txMeter reads GetTXAMeter on the transmit lane and
    // returns the lane's last reading, so this poll never waits on WDSP. It
    // maps each TxMeterType to its WDSP index (wdspTxaMeterIndex).
    TxChannel* channel = m_txChannel;
    const auto readRaw = [channel](TxMeterType meter) -> double {
#ifdef HAVE_WDSP
        return channel->txMeter(meter);
#else
        Q_UNUSED(channel)
        Q_UNUSED(meter)
        return -140.0;
#endif
    };
    for (const TxReadingEntry& entry : kTxReadings) {
        const bool unsupported=(entry.bindingId==MeterBinding::TxAlcGain || entry.bindingId==MeterBinding::TxAlcGroup) && bindingSupport(entry.bindingId)==MeterItem::BindingSupport::Unsupported;
        handOutTxReading(entry.bindingId, unsupported?kNoMeterReadingDbm:thetisTxReading(entry.reading, readRaw));
    }
}

// Hands one transmit reading to the meters and to txMeterReading.
void MeterPoller::handOutTxReading(int bindingId, double value)
{
    publishGlobalReading(bindingId, value);
    // R-R3-49 (parity Task 33): the S-meter's Level and Compression TX
    // modes. AetherSDR feeds them from its meter model's mic and
    // compression peaks:
    // From AetherSDR src/gui/MainWindow_Wiring.cpp:5506-5507 [@1e0718ad]:
    //   connect(&m_radioModel.meterModel(), &MeterModel::micMetersChanged,
    //           m_appletPanel->sMeterWidget(), &SMeterWidget::setMicMeters);
    // NereusSDR's are the MIC and COMP readings handed out here: WDSP's in
    // a local window, the Core's `txState` in a remote one, so both windows
    // show the same value for the same reading.
    if (bindingId == MeterBinding::TxMic || bindingId == MeterBinding::TxComp) {
        if (bindingId == MeterBinding::TxMic) {
            m_sMeterMicDb = static_cast<float>(value);
        } else {
            m_sMeterCompDb = static_cast<float>(value);
        }
        if (SMeterWidget* sm = m_sMeter.data()) {
            sm->setMicMeters(m_sMeterMicDb, m_sMeterCompDb, m_sMeterMicDb, m_sMeterCompDb);
        }
    }
    emit txMeterReading(bindingId, value);
}

std::function<double(const SliceModel*)> MeterPoller::panMaxBinSource(
    std::function<SpectrumWidget*(const QString& panKey)> spectrumFor)
{
    return panMaxBinSourceForSlice(
        [spectrumFor = std::move(spectrumFor)](const SliceModel* slice) -> SpectrumWidget* {
            return slice && spectrumFor ? spectrumFor(slice->panKey()) : nullptr;
        });
}

// 2026-10-02 KG4VCF, Codex: inherited TX slice-host dependency, retaining
// the established QString resolver contract for other window/test callers.
std::function<double(const SliceModel*)> MeterPoller::panMaxBinSourceForSlice(
    std::function<SpectrumWidget*(const SliceModel* slice)> spectrumFor)
{
    return [spectrumFor = std::move(spectrumFor)](const SliceModel* slice) -> double {
        SpectrumWidget* sw = slice && spectrumFor ? spectrumFor(slice) : nullptr;
        if (!sw) {
            return -400.0;
        }
        // The same passband bounds the local reading takes from its
        // widget's VFO and filter (SpectrumWidget::peakDbmInSlicePassband).
        return sw->peakDbmInPassband(slice->frequency() + slice->filterLow(),
                                     slice->frequency() + slice->filterHigh());
    };
}

// From Thetis console.cs:46979 [v2.10.3.15]:
//   updateMetersReading(Reading.COMP, (float)Math.Max(-30.0f, -WDSP.CalculateTXMeter(1, WDSP.MeterType.COMP)), 0);
// with dsp.cs:1013-1014 + :1056 [v2.10.3.15]: CalculateTXMeter reads
// TXA_COMP_AV and returns -(float)val, so the reading is max(-30, raw).
// D14, R-R3-49: now thetisTxReading's COMP reading.
double MeterPoller::compressionReading(double rawTxaCompAv)
{
    return thetisTxReading(ThetisTxReading::Comp,
                           [rawTxaCompAv](TxMeterType) { return rawTxaCompAv; });
}

void MeterPoller::setRadioStatus(RadioStatus* status)
{
    // Disconnect any previous connection before re-wiring.
    if (m_powerConn) {
        QObject::disconnect(m_powerConn);
        m_powerConn = QMetaObject::Connection{};
    }
    disconnect(m_statusDestroyed);
    for (int binding = MeterBinding::TxPower; binding <= MeterBinding::TxSwr; ++binding) { publishGlobalReading(binding, kNoMeterReadingDbm); }
    m_radioStatus = status;
    refreshBindingSupport();
    if (m_radioStatus) {
        m_statusDestroyed = connect(status, &QObject::destroyed, this, [this] { for (int binding = MeterBinding::TxPower; binding <= MeterBinding::TxSwr; ++binding) { publishGlobalReading(binding, kNoMeterReadingDbm); } });
        publishGlobalReading(MeterBinding::TxPower, status->forwardPowerWatts());
        publishGlobalReading(MeterBinding::TxReversePower, status->reflectedPowerWatts());
        publishGlobalReading(MeterBinding::TxSwr, status->swrRatio());
        // From Thetis console.cs PollPAPWR loop [v2.10.3.13]:
        // RadioStatus::powerChanged aggregates forward/reflected/swr from
        // PollPAPWR's alex_fwd / alex_rev / swr locals and emits them
        // together. Fan values out to all registered MeterWidget targets
        // via updateMeterValue() — same pattern as the RX poll() loop.
        m_powerConn = connect(
            m_radioStatus, &RadioStatus::powerChanged,
            this, [this](double fwd, double rev, double swr) {
                publishGlobalReading(MeterBinding::TxPower, fwd);
                publishGlobalReading(MeterBinding::TxReversePower, rev);
                publishGlobalReading(MeterBinding::TxSwr, swr);
            });
    }
}

} // namespace NereusSDR
