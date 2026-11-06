// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 3.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 4: the transmit keys.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 5: CW messages and keys.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 8: setupUnavailableReason, for the Keyboard page.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), first-look fixes: confirmed TUNE, remote keys
//                forgotten with their activity, a local stop's keys, a take not
//                granted, the remote on-air reason. AI-assisted via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/MainWindowHotKeyTargets.h"

#include "core/ControlRanges.h"
#include "core/CwSoftPaddle.h"
#include "core/CwxSender.h"
#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/UnbuiltFeatureList.h"
#include "core/WdspTypes.h"
#include "core/safety/TxRefusal.h"
#include "core/session/IStationLink.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/session/StationClient.h"
#include "gui/DesktopStationController.h"
#include "gui/MainWindow.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/multidevice/TakeTransmitDialog.h"
#include "gui/applets/TxApplet.h"
#include "gui/widgets/StatusToast.h"
#include "gui/widgets/VfoWidget.h"
#include "models/Band.h"
#include "models/BandDefaults.h"
#include "models/BandGrid.h"
#include "models/CwKeyerController.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QApplication>
#include <QList>
#include <QMetaObject>
#include <QPair>
#include <QPointer>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <utility>

namespace NereusSDR {

namespace {

// One press, as the container's AF buttons step (ContainerButtonDispatcher's
// kAfStep). From Thetis console.cs:51556-51557 [v2.10.3.15].
constexpr int kVolumeStep = 5;

// From AetherSDR src/gui/SpectrumWidget.cpp:2168-2169 [@1e0718ad], as the pan
// overlay's zoom buttons use them: emitZoom(1.0 / 1.5) in, emitZoom(1.5) out.
constexpr double kZoomOutFactor = 1.5;
constexpr double kZoomInFactor = 1.0 / 1.5;

// A filter matches a preset within this many Hz per edge, as the RX applet's
// buttons light (RxApplet::updateFilterButtons).
constexpr int kPresetMatchHz = 50;

// The RX applet's mode list and order (RxApplet.cpp, m_modeCombo).
constexpr std::array<DSPMode, 15> kModeWalk = {
    DSPMode::USB, DSPMode::LSB, DSPMode::CWU, DSPMode::CWL, DSPMode::AM,
    DSPMode::SAM, DSPMode::FM, DSPMode::DSB, DSPMode::DIGU, DSPMode::DIGL,
    DSPMode::DRM, DSPMode::RADE_U, DSPMode::RADE_L, DSPMode::RADE2_U, DSPMode::RADE2_L,
};

const QString& bandStackNotReadyReason()
{
    static const QString kReason = QStringLiteral("Band stacking is not ready.");
    return kReason;
}

const QString& voiceNotBuiltReason()
{
    static const QString kReason = QStringLiteral("Voice messages aren't built yet.");
    return kReason;
}

// A Core below remoteCwVersion 3 has no verb for them.
const QString& remotePaddleReason()
{
    static const QString kReason = QStringLiteral("Dot and dash need a newer Core.");
    return kReason;
}

// The MainWindow slot behind a band stack action; null for any other action.
const char* bandStackSlotName(const QString& id)
{
    if (id == QLatin1String("bandstack.next")) {
        return "bandStackNext";
    }
    if (id == QLatin1String("bandstack.previous")) {
        return "bandStackPrevious";
    }
    if (id == QLatin1String("bandstack.add")) {
        return "bandStackAdd";
    }
    if (id == QLatin1String("bandstack.panel")) {
        return "bandStackPanel";
    }
    return nullptr;
}

bool hasBandStackSlot(const QObject* window, const char* slot)
{
    const QByteArray signature = QByteArray(slot) + QByteArrayLiteral("()");
    return window != nullptr && window->metaObject()->indexOfSlot(signature.constData()) != -1;
}

std::optional<OtherButtonItem::ButtonId> containerButtonFor(const QString& id)
{
    using Id = OtherButtonItem::ButtonId;
    if (id == QLatin1String("slice.lock")) {
        return Id::LockA;
    }
    if (id == QLatin1String("nr.toggle")) {
        return Id::Nr;
    }
    if (id == QLatin1String("nb.toggle")) {
        return Id::Nb;
    }
    if (id == QLatin1String("anf.toggle")) {
        return Id::Anf;
    }
    if (id == QLatin1String("rit.toggle")) {
        return Id::Rit;
    }
    if (id == QLatin1String("xit.toggle")) {
        return Id::Xit;
    }
    return std::nullopt;
}

std::optional<OtherButtonItem::ButtonId> transmitButtonFor(const QString& id)
{
    using Id = OtherButtonItem::ButtonId;
    if (id == QLatin1String("tx.mox")) {
        return Id::Mox;
    }
    if (id == QLatin1String("tx.tune")) {
        return Id::Tun;
    }
    if (id == QLatin1String("tx.twotone")) {
        return Id::TwoTon;
    }
    return std::nullopt;
}

QList<QPair<int, int>> filterPresetsFor(RadioModel* model, DSPMode mode)
{
    QList<QPair<int, int>> presets;
    if (FilterPresetStore* const store = model->filterPresetStore()) {
        for (const FilterPreset& preset : store->presetsForMode(mode)) {
            presets.append({preset.low, preset.high});
        }
        return presets;
    }
    // The RX applet's fallback when there is no store.
    return SliceModel::presetsForMode(mode);
}

// The closest preset within the match, so two presets close together can't trap
// the walk on the first. -1 when the filter is off every preset.
int matchingPreset(const QList<QPair<int, int>>& presets, int low, int high)
{
    int best = -1;
    int bestDistance = 0;
    for (int i = 0; i < presets.size(); ++i) {
        const int lowDistance = std::abs(low - presets[i].first);
        const int highDistance = std::abs(high - presets[i].second);
        if (lowDistance > kPresetMatchHz || highDistance > kPresetMatchHz) {
            continue;
        }
        const int distance = lowDistance + highDistance;
        if (best < 0 || distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

// One step around a list of `size`, wrapping. Off the list (`from` is -1), next
// goes to the first and previous to the last.
int steppedIndex(int from, int direction, int size)
{
    if (from < 0) {
        return direction > 0 ? 0 : size - 1;
    }
    return (from + direction + size) % size;
}

} // namespace

MainWindowHotKeyTargets::MainWindowHotKeyTargets(MainWindow* window)
    : m_window(window)
{
    m_clock.start();
}

SliceModel* MainWindowHotKeyTargets::windowSlice() const
{
    return m_window->windowRxSlice(true);
}

SliceModel* MainWindowHotKeyTargets::sliceByLetter(QChar letter) const
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr) {
        return nullptr;
    }
    const QChar wanted = letter.toUpper();
    for (SliceModel* const slice : model->slices()) {
        if (slice == nullptr || slice->sliceLetter() != wanted) {
            continue;
        }
        // Another device's slice this window neither controls nor listens to is
        // not open here, whatever a pinned key says.
        const int id = slice->sliceIndex();
        return m_window->windowControlsSlice(id) || m_window->windowListensTo(id) ? slice : nullptr;
    }
    return nullptr;
}

QChar MainWindowHotKeyTargets::letterOf(const SliceModel* slice) const
{
    return slice != nullptr ? slice->sliceLetter() : QChar();
}

QString MainWindowHotKeyTargets::sliceRefusal(const SliceModel* slice) const
{
    return slice != nullptr ? m_window->sliceChangeRefusal(slice->sliceIndex()) : QString();
}

bool MainWindowHotKeyTargets::listened(const SliceModel* slice) const
{
    return slice != nullptr && m_window->windowListensTo(slice->sliceIndex());
}

bool MainWindowHotKeyTargets::tuneTo(SliceModel* slice, double hz)
{
    if (m_window->m_shuttingDown) {
        return false;
    }
    // Nothing to say with no flag: the slice is on its way in or out.
    VfoWidget* const flag = flagOf(slice);
    return flag != nullptr && flag->tuneFromDrag(hz);
}

void MainWindowHotKeyTargets::runSliceAction(const QString& id, SliceModel* slice, int steps)
{
    if (slice == nullptr || m_window->m_shuttingDown) {
        return;
    }
    if (containerButtonFor(id).has_value()) {
        clickContainerButton(id, slice);
    } else if (id == QLatin1String("step.larger")) {
        slice->changeTuneStepUp();
    } else if (id == QLatin1String("step.smaller")) {
        slice->changeTuneStepDown();
    } else if (id == QLatin1String("tune.goto")) {
        beginFrequencyEdit(slice);
    } else if (id == QLatin1String("volume.up")) {
        changeVolume(slice, kVolumeStep * steps);
    } else if (id == QLatin1String("volume.down")) {
        changeVolume(slice, -kVolumeStep * steps);
    } else if (id == QLatin1String("mute")) {
        toggleMute(slice);
    } else if (id == QLatin1String("filter.next")) {
        walkFilter(slice, 1);
    } else if (id == QLatin1String("filter.previous")) {
        walkFilter(slice, -1);
    } else if (id == QLatin1String("mode.next")) {
        walkMode(slice, 1);
    } else if (id == QLatin1String("mode.previous")) {
        walkMode(slice, -1);
    } else if (id == QLatin1String("band.next")) {
        walkBand(slice, 1);
    } else if (id == QLatin1String("band.previous")) {
        walkBand(slice, -1);
    } else {
        qCWarning(lcHotKeys) << "No slice action for:" << id;
    }
}

void MainWindowHotKeyTargets::runWindowAction(const QString& id)
{
    if (m_window->m_shuttingDown) {
        return;
    }
    if (id == QLatin1String("slice.next")) {
        selectNextSlice();
    } else if (id == QLatin1String("zoom.in") || id == QLatin1String("zoom.out")) {
        if (SpectrumWidget* const spectrum = zoomTarget(QApplication::activeWindow())) {
            spectrum->zoomBy(id == QLatin1String("zoom.in") ? kZoomInFactor : kZoomOutFactor);
        }
    } else if (!runBandStack(m_window, id)) {
        qCWarning(lcHotKeys) << "No window action for:" << id;
    }
}

QString MainWindowHotKeyTargets::unavailableReason(const QString& id) const
{
    if (id.startsWith(QLatin1String("voice.")) && !UnbuiltFeatures::isBuilt(UnbuiltFeature::Voice)) {
        return voiceNotBuiltReason();
    }
    // Only in CW: in a voice mode these keys do nothing and say nothing.
    if (transmitSliceInCw() && remotePaddleMissing(id)) {
        return remotePaddleReason();
    }
    return bandStackReason(m_window, id);
}

QString MainWindowHotKeyTargets::setupUnavailableReason(const QString& id) const
{
    return remotePaddleMissing(id) ? remotePaddleReason() : unavailableReason(id);
}

bool MainWindowHotKeyTargets::remotePaddleMissing(const QString& id) const
{
    return (id == QLatin1String("cw.dot") || id == QLatin1String("cw.dash"))
        && m_window->m_radioModel != nullptr && !m_window->m_radioModel->ownsLocalDsp()
        && (m_window->m_stationClient == nullptr || !m_window->m_stationClient->remoteCwPaddleAvailable());
}

bool MainWindowHotKeyTargets::moxOn() const
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr || !m_window->m_containerButtons) {
        return false;
    }
    // What the button lights, read as its next press would: for a moment after
    // a remote window's release the Core's value is still the old key, and a
    // press then is a new key (RadioModel::moxPressAsksOn).
    const bool lit = m_window->m_containerButtons
                         ->stateOf(OtherButtonItem::ButtonId::Mox, ContainerWidget::kFirstRxSource).on;
    return !model->moxPressAsksOn(!lit);
}

bool MainWindowHotKeyTargets::transmitting() const
{
    // Another device's key is not this window's, as on a hosting window: the
    // hold goes on to keyHold, which says why it does nothing.
    if (!otherDeviceOnAirReason().isEmpty()) {
        return false;
    }
    return moxOn() || tuneOrTwoToneAsked();
}

// What a press has to wait for: a TUNE asked for counts from the moment it goes.
bool MainWindowHotKeyTargets::tuneOrTwoToneAsked() const
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr || !m_window->m_containerButtons) {
        return false;
    }
    using Id = OtherButtonItem::ButtonId;
    const ContainerButtonDispatcher& buttons = *m_window->m_containerButtons;
    const int source = ContainerWidget::kFirstRxSource;
    return !model->tunePressAsksOn(!buttons.stateOf(Id::Tun, source).on)
        || buttons.stateOf(Id::TwoTon, source).on;
}

// What a release may leave the key to. A TUNE only asked for can still be
// refused, and no unkey follows a refusal: the release must go. If the Core takes
// that TUNE after all, the Core ending the key is the safe way round.
bool MainWindowHotKeyTargets::tuneOrTwoToneOn() const
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr || !m_window->m_containerButtons) {
        return false;
    }
    using Id = OtherButtonItem::ButtonId;
    const ContainerButtonDispatcher& buttons = *m_window->m_containerButtons;
    const int source = ContainerWidget::kFirstRxSource;
    // Lit, and not a TUNE this window has already let go of (tunePressAsksOn).
    const bool tuneOn = buttons.stateOf(Id::Tun, source).on && !model->tunePressAsksOn(false);
    return tuneOn || buttons.stateOf(Id::TwoTon, source).on;
}

// A remote window's copy of what a hosting window's controller refuses a hold
// with. Empty on a window that owns its radio, and with nobody else on the air.
QString MainWindowHotKeyTargets::otherDeviceOnAirReason() const
{
    RadioModel* const model = m_window->m_radioModel;
    const StationClient* const client = m_window->m_stationClient;
    if (model == nullptr || model->ownsLocalDsp() || client == nullptr) {
        return {};
    }
    const QString holder = client->otherHolderReason();
    return !holder.isEmpty() && (moxOn() || tuneOrTwoToneAsked()) ? holder : QString();
}

bool MainWindowHotKeyTargets::keyHold(bool down)
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr) {
        return false;
    }
    if (!down) {
        const bool throughHost = std::exchange(m_holdThroughHost, false);
        if (throughHost && m_window->desktopHosting()) {
            keyHoldThroughHost(false);
        } else {
            model->setHoldToTalk(false);
        }
        return false;
    }
    if (m_window->m_shuttingDown || !m_window->m_containerButtons) {
        return false;
    }
    // Refused here, with the press never sent to the Core.
    if (const QString onAir = otherDeviceOnAirReason(); !onAir.isEmpty()) {
        toast(onAir);
        return false;
    }
    if (!m_window->transmitControlsPermitted()) {
        toast(m_window->remoteTransmitReason());
        return false;
    }
    // The MOX button's refusals, but for those the controller makes itself with
    // moxRejected's toast: receive only and TX inhibit, on a window that keys
    // its own controller.
    const ContainerButtonDispatcher::State mox = m_window->m_containerButtons->stateOf(
        OtherButtonItem::ButtonId::Mox, ContainerWidget::kFirstRxSource);
    const bool controllerRefuses = model->ownsLocalDsp() && !model->transmitLinkDown()
        && model->transmitButtonsLocked();
    if (!mox.available && !controllerRefuses) {
        toast(mox.reason);
        return false;
    }
    if (m_window->desktopHosting()) {
        m_holdThroughHost = keyHoldThroughHost(true);
        return m_holdThroughHost;
    }
    model->setHoldToTalk(true);
    if (model->ownsLocalDsp()) {
        return model->moxController() != nullptr && model->moxController()->isMox();
    }
    IStationLink* const link = model->stationLink();
    RemoteTransmitClient* const remote = link != nullptr ? link->remoteTransmit() : nullptr;
    return remote != nullptr && remote->screenKeyDown();
}

bool MainWindowHotKeyTargets::keyHoldThroughHost(bool down)
{
    // As MainWindow::requestDesktopKey, but another device holding transmit is
    // a toast and not the take question: a hold is over too soon to answer it.
    using RequestState = DesktopStationController::RequestState;
    const QPointer<MainWindow> window(m_window);
    if (m_window->m_desktopTakeDialog) {
        m_window->m_desktopTakeDialog->close();
    }
    if (!window || !m_window->desktopHosting()) {
        return false;
    }
    const QPointer<DesktopStationController> controller(m_window->m_desktopStationController);
    m_hostTakeNotGranted = false;
    const DesktopStationController::RequestResult result = controller->requestMox(down);
    if (!window || !controller || controller != m_window->m_desktopStationController) {
        return false;
    }
    bool keyed = false;
    if (result.state == RequestState::Ask) {
        const QString name = result.question && !result.question->holderName.isEmpty()
            ? result.question->holderName
            : QStringLiteral("Another device");
        toast(TxRefusals::otherDeviceHolds(name).text);
    } else if (result.state == RequestState::Refused) {
        m_window->handleDesktopTakeResult(result);
        if (!window) {
            return false;
        }
    } else if (result.state == RequestState::Pending) {
        // Transmit is held already and it did not key: the controller refused,
        // and moxRejected has said why. A take still running keys when it ends.
        const MoxController* const mox = m_window->m_radioModel->moxController();
        keyed = !m_hostTakeNotGranted && (!m_window->desktopOwnsTransmit() || (mox != nullptr && mox->isMox()));
    }
    if (m_window->m_txApplet) {
        m_window->m_txApplet->syncDesktopKeyState();
    }
    m_window->refreshContainerControls();
    return keyed;
}

void MainWindowHotKeyTargets::runTransmitAction(const QString& id)
{
    if (m_window->m_shuttingDown || !m_window->m_containerButtons) {
        return;
    }
    const std::optional<OtherButtonItem::ButtonId> button = transmitButtonFor(id);
    if (!button.has_value()) {
        qCWarning(lcHotKeys) << "No transmit action for:" << id;
        return;
    }
    // The transmit buttons take no slice; the source is any valid one.
    const QString reason = m_window->m_containerButtons->click(*button, ContainerWidget::kFirstRxSource);
    if (!reason.isEmpty()) {
        toast(reason);
    }
}

bool MainWindowHotKeyTargets::transmitSliceInCw() const
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr) {
        return false;
    }
    const SliceModel* slice = model->txBoundSlice();
    // A remote window has no transmit slice until the Core marks one for a
    // holder. It goes by its active slice then, as its TX applet does.
    if (slice == nullptr && !model->ownsLocalDsp()) {
        slice = model->activeSlice();
    }
    return slice != nullptr && (slice->dspMode() == DSPMode::CWL || slice->dspMode() == DSPMode::CWU);
}

RemoteTransmitClient* MainWindowHotKeyTargets::remoteCwClient() const
{
    return m_window->m_stationClient != nullptr ? m_window->m_stationClient->remoteTransmit() : nullptr;
}

QString MainWindowHotKeyTargets::remoteCwRefusal() const
{
    // The reasons the CWX applet's Send and Key are disabled with
    // (MainWindow's remote transmit controls), in the same order.
    const StationClient* const client = m_window->m_stationClient;
    if (client == nullptr || !client->remoteCwAvailable()) {
        return MainWindow::cwxRemoteSendReason();
    }
    if (!m_window->transmitControlsPermitted()) {
        return m_window->remoteTransmitReason();
    }
    if (client->knowsTransmitHolder() && !client->holdsTransmitHere()) {
        const QString holder = client->otherHolderReason();
        return holder.isEmpty() ? TxRefusals::notHolder().text : holder;
    }
    return {};
}

void MainWindowHotKeyTargets::runCwMessage(const QString& id)
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr || m_window->m_shuttingDown) {
        return;
    }
    const bool local = model->ownsLocalDsp();
    if (id == QLatin1String("cw.stop")) {
        if (local) {
            // As a remote stop: a key still held comes back only with a fresh press.
            model->cwSoftPaddle()->releaseKeys();
            if (CwxSender* const sender = model->cwxSender()) {
                sender->stop();
            }
            return;
        }
        RemoteTransmitClient* const remote = remoteCwClient();
        if (remote == nullptr || remote->stopCw() == 0) {
            toast(MainWindow::cwxRemoteSendReason());
        }
        return;
    }

    bool isNumber = false;
    const int number = id.startsWith(QLatin1String("cw.msg")) ? id.mid(6).toInt(&isNumber) : 0;
    if (!isNumber) {
        qCWarning(lcHotKeys) << "No CW message for:" << id;
        return;
    }
    if (local) {
        CwxSender* const sender = model->cwxSender();
        if (sender == nullptr) {
            return;
        }
        // Emitted before sendMemory returns: only this press's refusal is ours
        // to say. The applet hears the sender too.
        const QMetaObject::Connection said = QObject::connect(
            sender, &CwxSender::refused, m_window, [this](const QString& words) { toast(words); });
        sender->sendMemory(number);
        QObject::disconnect(said);
        return;
    }
    const QString refusal = remoteCwRefusal();
    if (!refusal.isEmpty()) {
        toast(refusal);
        return;
    }
    RemoteTransmitClient* const remote = remoteCwClient();
    if (remote == nullptr || remote->sendCwMemory(number) == 0) {
        toast(MainWindow::cwxRemoteSendReason());
    }
}

void MainWindowHotKeyTargets::setCwKeyDown(const QString& id, bool down)
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr) {
        return;
    }
    const bool isDot = id == QLatin1String("cw.dot");
    const bool isDash = id == QLatin1String("cw.dash");
    if (!isDot && !isDash && id != QLatin1String("cw.straight")) {
        qCWarning(lcHotKeys) << "No CW key for:" << id;
        return;
    }
    if (!down) {
        // Whatever has changed since the press, letting go always works.
        if (isDot) {
            model->cwSoftPaddle()->setDot(false);
            setRemotePaddleKey(true, false);
        } else if (isDash) {
            model->cwSoftPaddle()->setDash(false);
            setRemotePaddleKey(false, false);
        } else {
            setStraightKeyDown(false);
        }
        return;
    }
    // A press in a voice mode does nothing and says nothing, and stays that way
    // if the mode changes under the held key: the dispatcher sends no second press.
    if (m_window->m_shuttingDown || !transmitSliceInCw()) {
        return;
    }
    if (!isDot && !isDash) {
        setStraightKeyDown(true);
        return;
    }
    // Below remoteCwVersion 3 the press is refused before this (unavailableReason).
    if (!model->ownsLocalDsp()) {
        setRemotePaddleKey(isDot, true);
        return;
    }
    const QString holder = model->cwxHolderRefusal(QByteArray());
    if (!holder.isEmpty()) {
        toast(holder);
    } else if (isDot) {
        model->cwSoftPaddle()->setDot(true);
    } else {
        model->cwSoftPaddle()->setDash(true);
    }
}

// R-HK-13: the Core is told the state of both keys on each change. Letting go
// always goes out, as the straight key's does; a key it never heard of goes
// nowhere. The Core's own refusal arrives as cwRefused, which the window toasts.
void MainWindowHotKeyTargets::setRemotePaddleKey(bool isDot, bool down)
{
    bool& key = isDot ? m_remoteDot : m_remoteDash;
    if (!down) {
        if (!std::exchange(key, false)) {
            return;
        }
        if (RemoteTransmitClient* const remote = remoteCwClient()) {
            remote->setCwPaddle(m_remoteDot, m_remoteDash);
        }
        return;
    }
    const QString refusal = remoteCwRefusal();
    if (!refusal.isEmpty()) {
        toast(refusal);
        return;
    }
    key = true;
    RemoteTransmitClient* const remote = remoteCwClient();
    if (remote == nullptr || remote->setCwPaddle(m_remoteDot, m_remoteDash) == 0) {
        key = false;
        toast(remotePaddleReason());
    }
}

// The session or the activity is gone, so what this window reported is too. Lifting
// one key later must not tell the Core the other is down. With the session still
// there the Core hears both up, so its record agrees and the next press is a press.
void MainWindowHotKeyTargets::onRemoteCwEnded()
{
    const bool anyDown = std::exchange(m_remoteDot, false);
    const bool dashDown = std::exchange(m_remoteDash, false);
    if (anyDown || dashDown) {
        if (RemoteTransmitClient* const remote = remoteCwClient()) {
            remote->setCwPaddle(false, false);
        }
    }
}

bool MainWindowHotKeyTargets::onHostKeyTakeNotGranted(bool superseded)
{
    // Superseded, this says nothing of the request now being made, a hold's own among them.
    if (!superseded) {
        m_hostTakeNotGranted = true;
    }
    return std::exchange(m_holdThroughHost, false);
}

void MainWindowHotKeyTargets::setStraightKeyDown(bool down)
{
    RadioModel* const model = m_window->m_radioModel;
    const bool local = model->ownsLocalDsp();
    if (!down) {
        if (!std::exchange(m_straightDown, false)) {
            return;
        }
        if (local) {
            if (CwxSender* const sender = model->cwxSender()) {
                sender->setHandKey(false);
            }
        } else if (RemoteTransmitClient* const remote = remoteCwClient()) {
            remote->setCwKey(false);
        }
        return;
    }
    if (local) {
        const QString holder = model->cwxHolderRefusal(QByteArray());
        if (!holder.isEmpty()) {
            toast(holder);
            return;
        }
        // setHandKey has no gate of its own: the serial reader asks first, on
        // its thread, and so does this key. Blocked until a fresh press, as the
        // dispatcher sends no second press for a key still down.
        const CwKeyerController* const keyer = model->cwKeyerController();
        if (keyer == nullptr || !keyer->keyerAllowed()) {
            if (keyer != nullptr && model->moxController() != nullptr) {
                model->moxController()->reportCwRefusal(keyer->refusal());
            }
            return;
        }
        CwxSender* const sender = model->cwxSender();
        m_straightDown = sender != nullptr && sender->setHandKey(true);
        return;
    }
    const QString refusal = remoteCwRefusal();
    if (!refusal.isEmpty()) {
        toast(refusal);
        return;
    }
    RemoteTransmitClient* const remote = remoteCwClient();
    if (remote == nullptr || remote->setCwKey(true) == 0) {
        toast(MainWindow::cwxRemoteSendReason());
        return;
    }
    m_straightDown = true;
}

void MainWindowHotKeyTargets::toast(const QString& text)
{
    m_window->showContainerButtonReason(text);
}

qint64 MainWindowHotKeyTargets::nowMs() const
{
    return m_clock.elapsed();
}

SpectrumWidget* MainWindowHotKeyTargets::zoomTarget(const QWidget* activeWindow) const
{
    if (const auto* const floating = qobject_cast<const PanFloatingWindow*>(activeWindow)) {
        if (m_window->m_panStack != nullptr) {
            if (SpectrumWidget* const spectrum = m_window->m_panStack->spectrum(floating->panId())) {
                return spectrum;
            }
        }
    }
    return m_window->activeSpectrumWidget();
}

QString MainWindowHotKeyTargets::bandStackReason(const QObject* window, const QString& id)
{
    const char* const slot = bandStackSlotName(id);
    return slot != nullptr && !hasBandStackSlot(window, slot) ? bandStackNotReadyReason() : QString();
}

bool MainWindowHotKeyTargets::runBandStack(QObject* window, const QString& id)
{
    const char* const slot = bandStackSlotName(id);
    if (slot == nullptr || !hasBandStackSlot(window, slot)) {
        return false;
    }
    return QMetaObject::invokeMethod(window, slot, Qt::DirectConnection);
}

VfoWidget* MainWindowHotKeyTargets::flagOf(const SliceModel* slice) const
{
    return slice != nullptr ? m_window->m_vfoWidgetsBySlice.value(slice->sliceIndex(), nullptr) : nullptr;
}

void MainWindowHotKeyTargets::clickContainerButton(const QString& id, SliceModel* slice)
{
    if (!m_window->m_containerButtons) {
        return;
    }
    const int rxSource = slice->sliceIndex() + ContainerWidget::kFirstRxSource;
    const QString reason = m_window->m_containerButtons->click(*containerButtonFor(id), rxSource);
    if (!reason.isEmpty()) {
        toast(reason);
    }
}

void MainWindowHotKeyTargets::changeVolume(SliceModel* slice, int delta)
{
    const int id = slice->sliceIndex();
    if (m_window->windowListensTo(id)) {
        const std::pair<int, bool> volume = m_window->listenVolumeFor(id);
        setListenVolume(id, volume.first + delta, volume.second);
        return;
    }
    slice->setAfGain(std::clamp(slice->afGain() + delta, ControlRanges::kAfGainMin, ControlRanges::kAfGainMax));
}

void MainWindowHotKeyTargets::toggleMute(SliceModel* slice)
{
    const int id = slice->sliceIndex();
    if (m_window->windowListensTo(id)) {
        const std::pair<int, bool> volume = m_window->listenVolumeFor(id);
        setListenVolume(id, volume.first, !volume.second);
        return;
    }
    slice->setMuted(!slice->muted());
}

void MainWindowHotKeyTargets::setListenVolume(int sliceId, int level, bool muted)
{
    m_window->setFlagListenVolume(sliceId, level, muted);
    // A hosting window's flag follows listenLevelChanged. A remote window's has
    // no such echo, and its own slider paints itself, so a key must paint it.
    if (!m_window->desktopHosting()) {
        if (VfoWidget* const flag = m_window->m_vfoWidgetsBySlice.value(sliceId, nullptr)) {
            const std::pair<int, bool> volume = m_window->listenVolumeFor(sliceId);
            flag->setListenVolume(volume.first, volume.second);
        }
    }
}

void MainWindowHotKeyTargets::beginFrequencyEdit(const SliceModel* slice)
{
    VfoWidget* const flag = flagOf(slice);
    if (flag == nullptr) {
        return;
    }
    if (flag->isCollapsed()) {
        flag->setCollapsed(false);
    }
    flag->beginFrequencyEdit();
}

void MainWindowHotKeyTargets::walkFilter(SliceModel* slice, int direction)
{
    RadioModel* const model = m_window->m_radioModel;
    // The RX applet offers a disabled RADE2 mode no presets.
    if (model == nullptr || (isRade2DspMode(slice->dspMode()) && !model->radeV2Enabled())) {
        return;
    }
    const QList<QPair<int, int>> presets = filterPresetsFor(model, slice->dspMode());
    if (presets.isEmpty()) {
        return;
    }
    const int from = matchingPreset(presets, slice->filterLow(), slice->filterHigh());
    const QPair<int, int>& preset = presets[steppedIndex(from, direction, static_cast<int>(presets.size()))];
    slice->setFilter(preset.first, preset.second);
}

void MainWindowHotKeyTargets::walkMode(SliceModel* slice, int direction)
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr) {
        return;
    }
    const int count = static_cast<int>(kModeWalk.size());
    const auto* const found = std::find(kModeWalk.begin(), kModeWalk.end(), slice->dspMode());
    int at = found != kModeWalk.end() ? static_cast<int>(found - kModeWalk.begin()) : -1;
    for (int tried = 0; tried < count; ++tried) {
        at = steppedIndex(at, direction, count);
        if (!isRade2DspMode(kModeWalk[at]) || model->radeV2Enabled()) {
            slice->setDspMode(kModeWalk[at]);
            return;
        }
    }
}

void MainWindowHotKeyTargets::walkBand(SliceModel* slice, int direction)
{
    RadioModel* const model = m_window->m_radioModel;
    if (model == nullptr) {
        return;
    }
    IStationLink* const link = model->stationLink();
    const bool has2m = link == nullptr || link->band2mUnavailableReason().isEmpty();

    QList<Band> bands;
    for (const BandGridEntry& entry : kBandGrid) {
        if (entry.band != Band::Band2m || has2m) {
            bands.append(entry.band);
        }
    }
    // The band the grid lights for this slice. GEN, XVTR and SWL have no row.
    const Band current = BandDefaults::bandGridBandFromFrequency(slice->frequency());
    const int from = static_cast<int>(bands.indexOf(current));
    model->onBandButtonClicked(slice, bands[steppedIndex(from, direction, static_cast<int>(bands.size()))]);
}

void MainWindowHotKeyTargets::selectNextSlice()
{
    const QVector<SliceModel*> tabs = m_window->windowSliceTabs();
    if (tabs.size() < 2) {
        return;
    }
    const int from = static_cast<int>(tabs.indexOf(windowSlice()));
    const int to = (from + 1) % static_cast<int>(tabs.size());
    m_window->selectSliceForWindow(tabs[to]->sliceIndex());
}

} // namespace NereusSDR
