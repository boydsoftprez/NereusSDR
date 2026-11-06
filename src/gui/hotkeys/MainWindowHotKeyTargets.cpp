// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 3.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 4: the transmit keys.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/MainWindowHotKeyTargets.h"

#include "core/ControlRanges.h"
#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/UnbuiltFeatureList.h"
#include "core/WdspTypes.h"
#include "core/safety/TxRefusal.h"
#include "core/session/IStationLink.h"
#include "core/session/RemoteTransmitClient.h"
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
    return bandStackReason(m_window, id);
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
    return moxOn() || tuneOrTwoToneOn();
}

bool MainWindowHotKeyTargets::tuneOrTwoToneOn() const
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
        keyed = !m_window->desktopOwnsTransmit() || (mox != nullptr && mox->isMox());
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

void MainWindowHotKeyTargets::runCwMessage(const QString& /*id*/)
{
}

void MainWindowHotKeyTargets::setCwKeyDown(const QString& /*id*/, bool /*down*/)
{
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
