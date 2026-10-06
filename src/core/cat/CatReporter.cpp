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
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// Ported from Thetis Project Files/Source/Console/console.cs
// Modification history (NereusSDR):
// 2026-10-04 - Native event-loop CAT adaptation by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.

// 2026-10-04 - Native separate Hamlib dialect and guarded lifecycle integration,
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex; no new Thetis port.
#include "CatReporter.h"
#include "CatService.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/TxSliceArbiter.h"
#include <cmath>
#include <limits>
#include <utility>
namespace NereusSDR {
namespace {
bool sameBinding(const CatBinding& a, const CatBinding& b)
{
    return a.primarySliceId == b.primarySliceId && a.secondarySliceId == b.secondarySliceId
        && a.primaryIncarnation == b.primaryIncarnation && a.secondaryIncarnation == b.secondaryIncarnation;
}
int keyFor(int channel, const QByteArray& code) { return channel * 3 + (code == "FA" ? 0 : code == "FB" ? 1 : 2); }
}
CatReporter::CatReporter(CatService& service, RadioModel& model)
    : m_service(&service), m_model(&model)
{
    m_elapsed.start(); m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &CatReporter::flushPending);
    connect(&model, &RadioModel::sliceAdded, this, [this](int id) { if (m_model) { watchSlice(m_model->sliceById(id)); } });
    connect(&model, &RadioModel::sliceRemoved, this, [this](int) {
        const QList<int> keys = m_states.keys();
        for (int key : keys) {
            const State state = m_states.value(key);
            if (!messageCurrent(key / 3, state.binding, state.pending.isEmpty() ? state.lastMessage : state.pending)) { m_states.remove(key); }
        }
        scheduleTimer();
    });
    connect(model.txSliceArbiter(), &TxSliceArbiter::txBoundSliceChanged, this, [this](int, int id) { markChanged(id, true); });
    for (SliceModel* slice : model.slices()) { watchSlice(slice); }
    connect(&service, &CatService::globalConfigurationChanged, this, &CatReporter::configurationChanged);
    configurationChanged();
}
void CatReporter::watchSlice(SliceModel* slice)
{
    if (!slice) { return; }
    const int id = slice->sliceIndex();
    connect(slice, &SliceModel::frequencyChanged, this, [this, id](double) { markChanged(id); });
}
void CatReporter::configurationChanged()
{
    // Global source enablement remains service-owned; this observer never writes settings.
    if (!m_service) { return; }
    const CatGlobalConfig config = m_service->globalConfig();
    setAiEnabled(config.allowKenwoodAi && config.aiEnabled);
    for (int channel = 1; channel <= 4; ++channel) { sessionsChanged(channel); }
}
void CatReporter::setAiEnabled(bool enabled)
{
    m_aiEnabled = enabled;
    if (!enabled) { reset(); }
}
void CatReporter::reset()
{
    m_timer.stop(); m_states.clear(); m_frequencyChanges.clear(); m_selectionChanged = false;
}
qint64 CatReporter::now() const { return m_clock ? m_clock() : m_elapsed.elapsed(); }
#ifdef NEREUS_BUILD_TESTS
void CatReporter::setClockForTest(std::function<qint64()> clock) { m_timer.stop(); m_clock = std::move(clock); reset(); }
#endif
bool CatReporter::eligible(quint64 id) const
{
    // From Thetis console.cs:51317-51350 [v2.10.3.15]. Configured destination routes.
    //
    // [original inline comment from console.cs:51331]
    if (!m_service || !m_service->isStarted()) { return false; }
    const CatSession* session = m_service->session(id);
    if (!session || session->dialect() != CatWireDialect::Thetis) { return false; }
    const CatGlobalConfig config = m_service->globalConfig();
    if (!config.allowKenwoodAi || !config.aiEnabled || !m_aiEnabled) { return false; }
    if (session->transport() == CatTransportKind::Tcp) { return config.aiTcp; }
    if (session->transport() != CatTransportKind::Serial && session->transport() != CatTransportKind::Pty) { return false; }
    const std::array<bool, 4> serialRoutes{config.aiSerial1, config.aiSerial2, config.aiSerial3, config.aiSerial4};
    return serialRoutes[session->context().channel - 1];
}
bool CatReporter::bindingCurrent(int channel, const CatBinding& binding) const
{
    if (!m_service || !m_service->isStarted() || !sameBinding(binding, m_service->channelConfig(channel).binding)) { return false; }
    return true;
}
bool CatReporter::messageCurrent(int channel, const CatBinding& binding, const QByteArray& message) const
{
    if (!bindingCurrent(channel, binding) || message.isEmpty()) { return false; }
    const CatVfo vfo = message.startsWith("FB") || message == "ZZSW1;" ? CatVfo::Secondary : CatVfo::Primary;
    const SliceModel* slice = m_service->adapter().resolveSlice(binding, vfo);
    if (!slice || !m_service->adapter().mayRead(binding, vfo)) { return false; }
    if (message.startsWith("ZZSW")) { return m_model && slice == m_model->txBoundSlice(); }
    // Reports are actual settled state, including any change during an earlier recipient callback.
    const QByteArray code = vfo == CatVfo::Primary ? "FA" : "FB";
    return std::isfinite(slice->frequency()) && message == code + QByteArray::number(qRound64(slice->frequency())).rightJustified(kFrequencyWidth, '0') + ';';
}
void CatReporter::sessionsChanged(int channel)
{
    if (!m_service) { return; }
    bool any = false;
    for (quint64 id : m_service->sessionIds(channel)) {
        if (eligible(id)) { any = true; break; }
    }
    if (!any) {
        for (int type = 0; type < 3; ++type) { m_states.remove(channel * 3 + type); }
    }
    scheduleTimer();
}
void CatReporter::markChanged(int sliceId, bool selection)
{
    // From Thetis console.cs:45358-45363,45830-45833,45851-45853 [v2.10.3.15].
    // cat broadcast for Kenwood AI
    // [original inline comment from console.cs:45360]
    //cat broadcast for kenwood AI
    // [original inline comment from console.cs:45830]
    //cat broadcast for kenwood AI
    // [original inline comment from console.cs:45851]
    if (!m_aiEnabled || !m_service || !m_service->isStarted()) { return; }
    if (selection) { m_selectionChanged = true; } else { m_frequencyChanges.insert(sliceId); }
    if (m_refreshQueued) { return; }
    m_refreshQueued = true;
    // Read actual settled state after clamp/rollback/reentrant setters have returned.
    QMetaObject::invokeMethod(this, [this] { m_refreshQueued = false; refreshChanges(); flushPending(); }, Qt::QueuedConnection);
}
void CatReporter::refreshChanges()
{
    if (!m_service || !m_model || !m_aiEnabled) { return; }
    const QSet<int> changes = std::exchange(m_frequencyChanges, {});
    const bool selectionChanged = std::exchange(m_selectionChanged, false);
    for (int channel = 1; channel <= 4; ++channel) {
        bool any = false;
        for (quint64 id : m_service->sessionIds(channel)) { if (eligible(id)) { any = true; break; } }
        if (!any) { continue; }
        const CatBinding binding = m_service->channelConfig(channel).binding;
        for (CatVfo vfo : {CatVfo::Primary, CatVfo::Secondary}) {
            SliceModel* slice = m_service->adapter().resolveSlice(binding, vfo);
            if (!slice || !changes.contains(slice->sliceIndex()) || !m_service->adapter().mayRead(binding, vfo)) { continue; }
            const double frequency = slice->frequency();
            if (!std::isfinite(frequency) || frequency < 0 || frequency > SliceModel::kMaxReceiveFrequencyHz) { continue; }
            // From Thetis console.cs:7923-7929 [v2.10.3.15]. Raw event frequency, no CAT getter RTTY offset.
            //MW0LGE_22a
            // [original inline comment from console.cs:7927]
            const QByteArray code = vfo == CatVfo::Primary ? "FA" : "FB";
            queue(channel, code, binding, code + QByteArray::number(qRound64(frequency)).rightJustified(kFrequencyWidth, '0') + ';');
        }
        if (selectionChanged) {
            SliceModel* selected = m_model->txBoundSlice();
            const bool primary = selected && selected == m_service->adapter().resolveSlice(binding, CatVfo::Primary);
            const bool secondary = selected && selected == m_service->adapter().resolveSlice(binding, CatVfo::Secondary);
            if (primary || secondary) {
                // From Thetis console.cs:39744-39753 [v2.10.3.15]. Normalize current TX selection to bound A/B.
                //Dictionary<string, bool> ken = SetupForm.KenwoodAISettings;
                // [original inline comment from console.cs:39748]
                queue(channel, "ZZSW", binding, primary ? "ZZSW0;" : "ZZSW1;");
            }
        }
    }
}
void CatReporter::queue(int channel, const QByteArray& code, const CatBinding& binding, const QByteArray& message)
{
    // From Thetis console.cs:53916-53953 [v2.10.3.15]. Latest message per UID.
    // Adaptation: independent channel/FA/FB/ZZSW keys avoid losing either bound VFO.
    State& state = m_states[keyFor(channel, code)];
    if (!sameBinding(state.binding, binding)) { state = State{}; state.binding = binding; }
    state.pending = message == state.lastMessage ? QByteArray() : message;
}
void CatReporter::broadcast(int channel, const CatBinding& binding, const QByteArray& bytes)
{
    const QPointer<CatReporter> self(this);
    const QPointer<CatService> service(m_service);
    const QList<quint64> ids = service->sessionIds(channel);
    for (quint64 id : ids) {
        if (!service || !messageCurrent(channel, binding, bytes)) { return; }
        const CatSession* session = service->session(id);
        if (session && sameBinding(session->binding(), binding) && eligible(id)) { service->sendToSession(id, bytes); }
        if (!self || !service) { return; }
    }
}
void CatReporter::flushPending()
{
    refreshChanges();
    if (!m_service || !m_service->isStarted() || !m_aiEnabled) { return; }
    const QPointer<CatReporter> self(this);
    const QList<int> keys = m_states.keys();
    const qint64 time = now();
    for (int key : keys) {
        if (!m_states.contains(key)) { continue; }
        State& state = m_states[key];
        if (!bindingCurrent(key / 3, state.binding)) { m_states.remove(key); continue; }
        if (!state.pending.isEmpty() && !messageCurrent(key / 3, state.binding, state.pending)) {
            state.pending.clear(); continue;
        }
        if (state.pending.isEmpty() || (state.lastSentMs && time - *state.lastSentMs < kAiIntervalMs)) { continue; }
        const QByteArray message = state.pending;
        const CatBinding binding = state.binding;
        state.pending.clear(); state.lastMessage = message; state.lastSentMs = time;
        broadcast(key / 3, binding, message);
        if (!self) { return; }
    }
    scheduleTimer();
}
void CatReporter::scheduleTimer()
{
    m_timer.stop();
    if (m_clock || !m_aiEnabled || !m_service || !m_service->isStarted()) { return; }
    qint64 due = std::numeric_limits<int>::max();
    const qint64 time = now();
    for (const State& state : m_states) {
        if (!state.pending.isEmpty()) { due = qMin(due, state.lastSentMs ? qMax(qint64(1), *state.lastSentMs + kAiIntervalMs - time) : qint64(1)); }
    }
    if (due != std::numeric_limits<int>::max()) { m_timer.start(static_cast<int>(due)); }
}
} // namespace NereusSDR
