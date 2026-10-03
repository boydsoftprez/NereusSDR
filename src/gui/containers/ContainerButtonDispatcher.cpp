// no-port-check: NereusSDR-original. Maps each container button to the
// NereusSDR feature it fronts, on the container's own slice.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/containers/ContainerButtonDispatcher.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30 - Fix round 1 (minor 4): MOX, TUNE and 2-TONE on a remote
//                 window give the link-down words by state. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-49, R-R3-21). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2): MON follows
//                                    the transmit settings gate in a
//                                    remote window (the Core's monEnabled).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Receiver and transmit gaps plan, Task
//                                    7: the MOX button keys through
//                                    RadioModel::setMoxFromButton (a manual
//                                    key). AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan, desktop remote
//                                    transmit (R-IOS-13): a remote window's
//                                    MOX, TUNE and 2-TONE light from the
//                                    Core; 2-TONE goes through
//                                    RadioModel::setTwoTone. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 7): PS-A follows
//                                    the PureSignal arming gate in a remote
//                                    window (the Core arms off the air).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Receiver and transmit gaps plan, Task
//                                    16: TUNE, MOX (outside SPEC and DRM)
//                                    and 2-Tone are unavailable with the
//                                    receive-only reason. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Task 16 fix wave: MOX in every mode
//                                    (I3); both reasons in a remote window
//                                    (M6). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  A11 / R-R3-49 (parity Task 31): DUP
//                                    (display duplex), the window's
//                                    DisplayDuplex setting through its
//                                    hooks. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: TUN, MOX and 2TONE are unavailable
//                with the reason while a TX inhibit holds (the HL2 I/O
//                board's fault code, say), as Thetis's TXInhibit setter
//                does. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 2: sliceFor
//                                    asks SliceAccessPolicy. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 15 fix round
//                                    1: the sliceRefusal hook refuses a
//                                    slice another device controls with
//                                    the RX applet's reason, in remote and
//                                    hosting windows. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  TX rulings (item 1): a remote
//                                    window's MOX and TUNE press toggles
//                                    against its own key. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave, hosting 2-TONE parity: a
//                                    hosting window's 2TONE asks to take
//                                    transmit, as MOX and TUNE do.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave GUI-I7: PS-A greys with the
//                                    facade's own reason (on the air, or
//                                    no radio that supports PureSignal).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  The container Power button is removed
//                                    (maintainer decision): an old Power id
//                                    falls to the unavailable default and a
//                                    click changes nothing. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include "gui/containers/ContainerButtonDispatcher.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/TwoToneController.h"
#include "core/session/IStationLink.h"
#include "core/session/PureSignalSessionFacade.h"
#include "gui/SpectrumWidget.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ButtonBoxItem.h"
#include "models/Band.h"
#include "models/NotchModel.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <utility>

namespace NereusSDR {

namespace {

using Id = ContainerButtonDispatcher::Id;

// The function buttons connected to a NereusSDR feature. Every other core
// button is hidden through UnbuiltFeatures (OtherButtonItem). Power is not
// one: NereusSDR has no Power button (maintainer decision 2026-09-30), and
// OtherButtonItem never draws it.
constexpr Id kConnected[] = {
    Id::Mon, Id::Tun, Id::Mox, Id::TwoTon, Id::PsA,
    Id::Anf, Id::Snb, Id::Mnf, Id::PeakHold, Id::Ctun,
    Id::Vac1, Id::Vac2, Id::Mute, Id::Bin, Id::Dup,
};

int vaxChannelOf(Id id)
{
    return id == Id::Vac1 ? 1 : 2;
}

QString vaxEnabledKey(int channel)
{
    // The key Setup > Audio > VAX's channel "On" switch saves.
    return QStringLiteral("audio/Vax%1/Enabled").arg(channel);
}

} // namespace

ContainerButtonDispatcher::ContainerButtonDispatcher(RadioModel* model, Hooks hooks)
    : m_model(model)
    , m_hooks(std::move(hooks))
{
}

SliceModel* ContainerButtonDispatcher::sliceFor(int rxSource) const
{
    if (!m_model) { return nullptr; }
    const int sliceId = ContainerWidget::sliceIdForRxSource(rxSource);
    SliceModel* slice = m_model->sliceById(sliceId);
    // Slice control plan Task 15 fix round 1: a slice this window listens
    // to (another device controls it) is refused as the RX applet refuses
    // it, in a remote window and a hosting one alike.
    if (slice && m_hooks.sliceRefusal && !m_hooks.sliceRefusal(sliceId).isEmpty()) {
        return nullptr;
    }
    if (slice && m_hooks.desktopHosting && m_hooks.desktopHosting()) {
        SliceOwnership* ownership = m_model->sliceOwnership();
        if (!ownership) { return nullptr; }
        // Slice control plan Task 2: a slice the station device may change
        // as its own (not one it runs held for an absent device).
        if (!SliceAccessPolicy::mayChange(*ownership, SliceOwnership::stationDevice(), sliceId)
            || ownership->mark(sliceId).isHeld()) {
            return nullptr;
        }
    }
    return slice;
}

QString ContainerButtonDispatcher::noSliceReason(int rxSource)
{
    return QStringLiteral("%1 is not open. Open it, or choose another slice in this "
                          "container's settings.")
        .arg(ContainerWidget::sliceNameForRxSource(rxSource));
}

QString ContainerButtonDispatcher::sliceUnavailableReason(int rxSource) const
{
    if (m_model && m_hooks.sliceRefusal) {
        const int sliceId = ContainerWidget::sliceIdForRxSource(rxSource);
        if (m_model->sliceById(sliceId)) {
            const QString refusal = m_hooks.sliceRefusal(sliceId);
            if (!refusal.isEmpty()) { return refusal; }
        }
    }
    if (m_model && m_hooks.desktopHosting && m_hooks.desktopHosting()) {
        const int sliceId = ContainerWidget::sliceIdForRxSource(rxSource);
        if (m_model->sliceById(sliceId)) {
            return QStringLiteral("%1 belongs to another device. Choose a Core slice in "
                                  "this container's settings.")
                .arg(ContainerWidget::sliceNameForRxSource(rxSource));
        }
    }
    return noSliceReason(rxSource);
}

QString ContainerButtonDispatcher::noRadioTransmitReason()
{
    return QStringLiteral("Connect a radio to transmit.");
}

bool ContainerButtonDispatcher::transmitBlockedRemotely() const
{
    if (!m_model || m_model->ownsLocalDsp()) { return false; }
    return !(m_hooks.transmitPermitted && m_hooks.transmitPermitted());
}

SpectrumWidget* ContainerButtonDispatcher::spectrumOf(int rxSource) const
{
    SliceModel* slice = sliceFor(rxSource);
    if (!slice || !m_hooks.spectrumFor) { return nullptr; }
    return m_hooks.spectrumFor(slice);
}

ContainerButtonDispatcher::State
ContainerButtonDispatcher::stateOf(Id id, int rxSource) const
{
    State st;
    if (!m_model) {
        st.available = false;
        st.reason = noRadioTransmitReason();
        return st;
    }
    const auto unavailable = [&st](const QString& reason) {
        st.available = false;
        st.reason = reason;
    };
    QString remoteReason = m_hooks.remoteTransmitReasonNow ? m_hooks.remoteTransmitReasonNow()
                                                           : QString();
    if (remoteReason.isEmpty()) {
        remoteReason = m_hooks.remoteTransmitReason.isEmpty()
            ? QStringLiteral("Remote transmit controls are not available from this Core.")
            : m_hooks.remoteTransmitReason;
    }

    switch (id) {
    case Id::Mon:
        st.on = m_model->transmitModel().monEnabled();
        // R-R3-49 (parity Task 2): a transmit setting, not a key. A remote
        // window toggles the Core's monEnabled while the Core takes it.
        if (!m_model->ownsLocalDsp()
            && !(m_hooks.transmitSettingsPermitted && m_hooks.transmitSettingsPermitted())) {
            const QString reason = m_hooks.transmitSettingsReason
                ? m_hooks.transmitSettingsReason() : QString();
            unavailable(reason.isEmpty() ? remoteReason : reason);
        }
        break;
    case Id::Tun:
    case Id::Mox:
    case Id::TwoTon:
        if (m_model->role() == RadioModel::Role::Remote) {
            // Desktop remote transmit (R-IOS-13): the Core's state, as the
            // TX applet shows it; this window's own controller never keys.
            const PureSignalSessionFacade* ps = m_model->pureSignalFacade();
            st.on = id == Id::Tun ? m_model->transmitModel().isTune()
                  : id == Id::Mox ? m_model->isTransmitting()
                                  : ps && ps->twoToneOn();
        } else if (m_hooks.desktopHosting && m_hooks.desktopHosting()
                   && (id == Id::Tun || id == Id::Mox)) {
            st.on = id == Id::Tun ? (m_hooks.desktopTuneOn && m_hooks.desktopTuneOn())
                                  : (m_hooks.desktopMoxOn && m_hooks.desktopMoxOn());
        } else if (id == Id::Tun) {
            st.on = m_model->isTune();
        } else if (id == Id::Mox) {
            st.on = m_model->moxController() && m_model->moxController()->isMox();
        } else {
            st.on = m_model->twoToneController() && m_model->twoToneController()->isActive();
        }
        // Task 16: receive only disables TUN, 2TONE and MOX, as Thetis
        // console.RXOnly does (console.cs:15318-15323 [v2.10.3.15]; the
        // 2TONE line carries // MW0LGE_21a). MOX too in SPEC and DRM, where
        // Thetis leaves it alone (RadioModel::receiveOnlyDisablesMoxButton,
        // fix wave I3). First, since it holds however this window reaches
        // the radio; with a remote window's missing transmit as well, both
        // reasons show, so turning receive only off does not leave the
        // button blocked for a reason never named (fix wave M6).
        // HL2 port part 2: a TX inhibit the same way, with its reason (the
        // HL2 I/O board's fault code), as Thetis's TXInhibit setter
        // disables them (console.cs:15341-15363 [v2.10.3.15]):
        //   chkTUN.Enabled = !_tx_inhibit;
        //   chk2TONE.Enabled = !_tx_inhibit; //MW0LGE_21a
        if (m_model->transmitButtonsLocked()
            && (id != Id::Mox || m_model->transmitLockCoversMox())) {
            unavailable(m_model->transmitLockReasonAlongside(
                transmitBlockedRemotely() ? remoteReason : QString()));
        } else if (transmitBlockedRemotely()) {
            unavailable(remoteReason);
        } else if (!m_model->isConnected()) {
            // Fix round 1 (minor 4): a remote window says which link is
            // down (RadioModel::transmitLinkDownReason).
            unavailable(m_model->ownsLocalDsp() ? noRadioTransmitReason()
                                                : m_model->transmitLinkDownReason());
        } else if ((id == Id::Mox && !m_model->moxController())
                   || (id == Id::TwoTon && !m_model->twoToneController())) {
            unavailable(noRadioTransmitReason());
        }
        break;
    case Id::PsA: {
        PureSignalSessionFacade* ps = m_model->pureSignalFacade();
        st.on = ps && ps->settings() && ps->settings()->autoCalEnabled();
        // R-R3-49 (parity Task 7): arming keys nothing. A remote window
        // arms on a Core that takes it (off the air), else says why.
        const bool armingBlockedRemotely = !m_model->ownsLocalDsp()
            && !(m_hooks.pureSignalArmingPermitted && m_hooks.pureSignalArmingPermitted());
        if (armingBlockedRemotely) {
            const QString reason = m_hooks.pureSignalArmingReason
                ? m_hooks.pureSignalArmingReason() : QString();
            unavailable(reason.isEmpty() ? remoteReason : reason);
        } else if (!ps || !ps->available() || !ps->canArm()) {
            // Fix wave GUI-I7: the facade's own reason (on the air, or no
            // connected radio that supports PureSignal).
            const QString refusal = ps ? ps->armingRefusal() : QString();
            unavailable(refusal.isEmpty() ? PureSignalSessionFacade::needsRadioReason()
                                          : refusal);
        }
        break;
    }
    case Id::Anf:
    case Id::Snb:
    case Id::Mute:
    case Id::Bin: {
        SliceModel* slice = sliceFor(rxSource);
        if (!slice) {
            unavailable(sliceUnavailableReason(rxSource));
            break;
        }
        st.on = id == Id::Anf    ? slice->anfEnabled()
              : id == Id::Snb    ? slice->snbEnabled()
              : id == Id::Mute   ? slice->muted()
                                 : slice->binauralEnabled();
        break;
    }
    case Id::Mnf:
        st.on = m_model->notchModel() && m_model->notchModel()->globalEnabled();
        if (!m_model->notchModel()) {
            unavailable(QStringLiteral("Notches are not ready."));
        }
        break;
    case Id::PeakHold:
    case Id::Ctun: {
        if (!sliceFor(rxSource)) {
            unavailable(sliceUnavailableReason(rxSource));
            break;
        }
        SpectrumWidget* sw = spectrumOf(rxSource);
        if (!sw) {
            unavailable(QStringLiteral("No panadapter shows this slice."));
            break;
        }
        if (id == Id::PeakHold) {
            st.on = sw->peakHoldEnabled();
        } else {
            st.on = sw->ctunEnabled();
            if (!sw->ctunAvailable()) {
                unavailable(QStringLiteral("C-Tune needs a connected Core that supports it."));
            }
        }
        break;
    }
    case Id::Dup: {
        // Parity Task 31 (A11): display duplex, the window's DisplayDuplex
        // (View > Display duplex (DUP) is the same setting). Thetis's
        // container DUP button toggles chkRX2SR, "chkRX2SR is the DUPlex
        // button" (console.cs:37555-37575 [v2.10.3.15];
        // ucOtherButtonsOptionsGrid.cs:540 [v2.10.3.15], "Duplex mode, view
        // the tx rx"). Not a transmit setting: it changes what this window
        // shows, so it works on and off the air.
        st.on = m_hooks.displayDuplexOn && m_hooks.displayDuplexOn();
        const QString reason = m_hooks.displayDuplexReason ? m_hooks.displayDuplexReason()
                                                           : QString();
        if (!m_hooks.setDisplayDuplex) {
            unavailable(QStringLiteral("This button does nothing in this version."));
        } else if (!reason.isEmpty()) {
            unavailable(reason);
        }
        break;
    }
    case Id::Vac1:
    case Id::Vac2:
        if (!m_hooks.vaxDevices) {
            unavailable(QStringLiteral("This computer's sound devices are not ready."));
            break;
        }
        st.on = m_hooks.vaxDevices->isVaxBusOpen(vaxChannelOf(id));
        break;
    default:
        // Hidden until its feature is built (OtherButtonItem).
        unavailable(QStringLiteral("This button does nothing in this version."));
        break;
    }
    return st;
}

void ContainerButtonDispatcher::apply(OtherButtonItem* item, int rxSource) const
{
    if (!item) { return; }
    for (Id id : kConnected) {
        const State st = stateOf(id, rxSource);
        item->setButtonState(id, st.on);
        item->setButtonAvailable(id, st.available, st.reason);
    }
}

QString ContainerButtonDispatcher::click(Id id, int rxSource)
{
    const State st = stateOf(id, rxSource);
    if (!st.available) { return st.reason; }
    const bool turnOn = !st.on;

    switch (id) {
    case Id::Mon:
        // TxApplet's MON button (TransmitModel::setMonEnabled).
        m_model->transmitModel().setMonEnabled(turnOn);
        break;
    case Id::Tun:
        // TxApplet's TUNE button (RadioModel::setTune).
        if (m_hooks.desktopHosting && m_hooks.desktopHosting()
            && m_hooks.requestDesktopTune) {
            m_hooks.requestDesktopTune(turnOn);
        } else {
            // TX rulings (item 1): against a remote window's own TUNE.
            m_model->setTune(m_model->tunePressAsksOn(turnOn));
        }
        break;
    case Id::Mox:
        // TxApplet's MOX button: a manual key, with TUN and two-tone turned
        // off on the way off (RadioModel::setMoxFromButton, Task 7).
        if (m_hooks.desktopHosting && m_hooks.desktopHosting()
            && m_hooks.requestDesktopMox) {
            m_hooks.requestDesktopMox(turnOn);
        } else {
            // TX rulings (item 1): against a remote window's own key.
            m_model->setMoxFromButton(m_model->moxPressAsksOn(turnOn));
        }
        break;
    case Id::TwoTon:
        // TxApplet's 2-TONE button (RadioModel::setTwoTone: the
        // TwoToneController here, the Core's in a remote window).
        // Fix wave (hosting 2-TONE parity): a hosting window asks to take
        // transmit first, as for MOX and TUNE.
        if (m_hooks.desktopHosting && m_hooks.desktopHosting()
            && m_hooks.requestDesktopTwoTone) {
            m_hooks.requestDesktopTwoTone(turnOn);
        } else {
            m_model->setTwoTone(turnOn);
        }
        break;
    case Id::PsA:
        // TxApplet's PS-A button: automatic calibration on, or Off/reset.
        m_model->pureSignalFacade()->requestAction(
            turnOn ? Ps3Action::StartAutomatic : Ps3Action::OffReset);
        break;
    case Id::Anf:
        sliceFor(rxSource)->setAnfEnabled(turnOn);
        break;
    case Id::Snb:
        sliceFor(rxSource)->setSnbEnabled(turnOn);
        break;
    case Id::Mute:
        sliceFor(rxSource)->setMuted(turnOn);
        break;
    case Id::Dup:
        m_hooks.setDisplayDuplex(turnOn);
        break;
    case Id::Bin:
        sliceFor(rxSource)->setBinauralEnabled(turnOn);
        break;
    case Id::Mnf:
        m_model->notchModel()->setGlobalEnabled(turnOn);
        break;
    case Id::PeakHold:
        spectrumOf(rxSource)->setPeakHoldEnabled(turnOn);
        break;
    case Id::Ctun:
        // The pan overlay's C-Tune switch; a remote window routes it to
        // the Core (RemoteMediaController follows ctunEnabledChanged).
        spectrumOf(rxSource)->setCtunEnabled(turnOn);
        break;
    case Id::Vac1:
    case Id::Vac2: {
        // Setup > Audio > VAX's channel "On" switch: saved, then the
        // channel opened or closed on this computer.
        const int channel = vaxChannelOf(id);
        AppSettings::instance().setValue(vaxEnabledKey(channel),
            turnOn ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
        m_hooks.vaxDevices->setVaxEnabled(channel, turnOn);
        break;
    }
    default:
        return st.reason;
    }
    return QString();
}

void ContainerButtonDispatcher::applySliceAvailability(ButtonBoxItem* box, int rxSource) const
{
    if (!box) { return; }
    if (sliceFor(rxSource)) {
        box->setAllButtonsAvailable(true);
    } else {
        box->setAllButtonsAvailable(false, sliceUnavailableReason(rxSource));
    }
}

void ContainerButtonDispatcher::applyBand(BandButtonItem* item, int rxSource) const
{
    if (!item) { return; }
    applySliceAvailability(item, rxSource);
    SliceModel* slice = sliceFor(rxSource);
    // R-IOS-26: a remote window's 2 m button, disabled with the reason
    // when its Core does not have the 2 m band.
    if (slice && m_model->stationLink() != nullptr) {
        const QString reason = m_model->stationLink()->band2mUnavailableReason();
        if (!reason.isEmpty()) {
            item->setButtonAvailable(uiIndexFromBand(Band::Band2m), false, reason);
        }
    }
    item->setActiveBand(slice ? uiIndexFromBand(bandFromFrequency(slice->frequency())) : -1);
}

QString ContainerButtonDispatcher::clickBand(int bandUiIndex, int rxSource)
{
    SliceModel* slice = sliceFor(rxSource);
    if (!slice) { return sliceUnavailableReason(rxSource); }
    m_model->onBandButtonClicked(slice, bandFromUiIndex(bandUiIndex));
    return QString();
}

} // namespace NereusSDR
