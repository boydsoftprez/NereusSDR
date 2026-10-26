// =================================================================
// src/gui/instruments/SMeterDialContent.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original container-content registration for
// added copies of the SIGNAL STRENGTH dial (SMeterWidget, ported from
// AetherSDR and attributed in its own files). Workspace preview only.
// See SMeterDialContent.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - Shared accessory display readings and independent legal
//                 marker / display scale by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-08 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Copies follow the external amplifier as the Main Panel's
//                 dial does; a copy with no face key follows Setup's face.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - TX modes ALC Compression, ALC Group, Volts and Amps
//                 (TxAlcGain, TxAlcGroup, HwVolts, HwAmps); the "Power
//                 limit" property (kPowerLimitKey); the VHF S9 rule of the
//                 copy's slice (LongpathContent::followAboveS9Frequency).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "models/MainPowerMeter.h"
#include "gui/instruments/SMeterDialContent.h"

#include "gui/SMeterWidget.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"

#include <QJsonValue>
#include <QPointer>
#include <QSet>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>

namespace NereusSDR::SMeterDialContent {

namespace {

// The right-click menu's names, in enum order (SMeterWidget::TxMode,
// RxMode, DecayRate); the entry config stores these.
const std::array<QString, SMeterWidget::kTxModeCount>& txModeNames()
{
    static const std::array<QString, SMeterWidget::kTxModeCount> names{
        QStringLiteral("Power"), QStringLiteral("SWR"), QStringLiteral("Level"), QStringLiteral("Compression"),
        QStringLiteral("ALC Compression"), QStringLiteral("ALC Group"), QStringLiteral("Volts"),
        QStringLiteral("Amps")};
    return names;
}
const std::array<QString, 4>& rxModeNames()
{
    static const std::array<QString, 4> names{QStringLiteral("Signal"), QStringLiteral("Sig Avg"),
                                              QStringLiteral("Signal Peak"), QStringLiteral("Max Bin")};
    return names;
}
const std::array<QString, 3>& decayNames()
{
    static const std::array<QString, 3> names{QStringLiteral("Fast"), QStringLiteral("Medium"),
                                              QStringLiteral("Slow")};
    return names;
}

// The SMeterWidget TX mode each transmit binding feeds, for the poller's
// reason a binding has no reading (SMeterWidget::setTxModeUnavailable).
// Level is the MIC reading and Compression the COMP reading, as
// MeterPoller hands them to the Main Panel's dial.
std::optional<SMeterWidget::TxMode> txModeForBinding(int binding)
{
    switch (binding) {
    case MeterBinding::TxPower: return SMeterWidget::TxMode::Power;
    case MeterBinding::TxSwr: return SMeterWidget::TxMode::SWR;
    case MeterBinding::TxMic: return SMeterWidget::TxMode::Level;
    case MeterBinding::TxComp: return SMeterWidget::TxMode::Compression;
    case MeterBinding::TxAlcGain: return SMeterWidget::TxMode::Alc;
    case MeterBinding::TxAlcGroup: return SMeterWidget::TxMode::AlcGroup;
    case MeterBinding::HwVolts: return SMeterWidget::TxMode::Volts;
    case MeterBinding::HwAmps: return SMeterWidget::TxMode::Amps;
    default: return std::nullopt;
    }
}

bool isRxBinding(int binding)
{
    return binding == MeterBinding::SignalPeak || binding == MeterBinding::SignalAvg
        || binding == MeterBinding::SignalMaxBin;
}

// The copy's power limit in whole watts (kPowerLimitKey), up to the
// meter items' limit; 0 when absent or not positive (the radio's scale).
int powerLimitW(const QJsonObject& config)
{
    const int limit = config.value(QLatin1String(kPowerLimitKey)).toInt(0);
    return std::clamp(limit, 0, static_cast<int>(kMaxBarPowerLimitW));
}

// Applies the copy's settings; an absent key keeps today's default (the
// Instance constructor's).
void applyConfig(SMeterWidget& dial, const QJsonObject& config)
{
    const QString tx = config.value(QLatin1String(kTxModeKey)).toString();
    if (std::find(txModeNames().cbegin(), txModeNames().cend(), tx) != txModeNames().cend()) {
        dial.setTxMode(tx);
    }
    const QString rx = config.value(QLatin1String(kRxModeKey)).toString();
    if (std::find(rxModeNames().cbegin(), rxModeNames().cend(), rx) != rxModeNames().cend()) {
        dial.setRxMode(rx);
    }
    const QJsonValue peak = config.value(QLatin1String(kPeakHoldKey));
    if (peak.isBool()) { dial.setPeakHoldEnabled(peak.toBool()); }
    const QString decay = config.value(QLatin1String(kPeakDecayKey)).toString();
    if (std::find(decayNames().cbegin(), decayNames().cend(), decay) != decayNames().cend()) {
        dial.setPeakDecayRate(decay);
    }
    // A face of the copy's own, or (no face key) Setup's face, followed.
    const QString face = config.value(QLatin1String(kFaceKey)).toString();
    if (!face.isEmpty() && SMeterWidget::faceStyleKey(SMeterWidget::faceStyleFromKey(face)) == face) {
        dial.setFaceStyle(SMeterWidget::faceStyleFromKey(face));
    } else {
        dial.setFollowsSetupFace(true);
    }
    // A power limit of the copy's own: red from it, full scale at it.
    if (const int limit = powerLimitW(config); limit > 0) {
        dial.setPowerDisplayScale(limit, limit);
    }
}

// The entry config with the dial's five settings as it shows them now.
QJsonObject configFrom(const SMeterWidget& dial, QJsonObject config)
{
    config.insert(QLatin1String(kTxModeKey), txModeNames().at(static_cast<std::size_t>(dial.txMode())));
    config.insert(QLatin1String(kRxModeKey), rxModeNames().at(static_cast<std::size_t>(dial.rxMode())));
    config.insert(QLatin1String(kPeakHoldKey), dial.peakHoldEnabled());
    config.insert(QLatin1String(kPeakDecayKey), decayNames().at(static_cast<std::size_t>(dial.peakDecayRate())));
    // Setup's face is not the copy's own: no face key.
    if (dial.followsSetupFace()) {
        config.remove(QLatin1String(kFaceKey));
    } else {
        config.insert(QLatin1String(kFaceKey), SMeterWidget::faceStyleKey(dial.faceStyle()));
    }
    return config;
}

QVector<ContentProperty> dialProperties()
{
    const auto choices = [](const auto& names) {
        QVector<ContentPropertyChoice> out;
        for (const QString& n : names) { out.append({n, n, true, {}}); }
        return out;
    };
    ContentProperty tx;
    tx.key = QLatin1String(kTxModeKey);
    tx.label = QObject::tr("TX mode");
    tx.kind = ContentProperty::Kind::Choice;
    tx.choices = choices(txModeNames());
    tx.defaultValue = txModeNames().front();
    ContentProperty rx;
    rx.key = QLatin1String(kRxModeKey);
    rx.label = QObject::tr("RX mode");
    rx.kind = ContentProperty::Kind::Choice;
    rx.choices = choices(rxModeNames());
    rx.defaultValue = rxModeNames().front();
    ContentProperty peak;
    peak.key = QLatin1String(kPeakHoldKey);
    peak.label = QObject::tr("Peak hold");
    peak.kind = ContentProperty::Kind::Toggle;
    peak.defaultValue = true;
    ContentProperty decay;
    decay.key = QLatin1String(kPeakDecayKey);
    decay.label = QObject::tr("Peak decay");
    decay.kind = ContentProperty::Kind::Choice;
    decay.choices = choices(decayNames());
    decay.defaultValue = QStringLiteral("Medium");
    ContentProperty face;
    face.key = QLatin1String(kFaceKey);
    face.label = QObject::tr("Meter face");
    face.kind = ContentProperty::Kind::Choice;
    // Null removes the key: the copy follows Setup's face.
    face.choices.append({QObject::tr("Same as Setup"), QJsonValue(), true, {}});
    for (int i = 0; i <= static_cast<int>(SMeterWidget::FaceStyle::Classic); ++i) {
        const auto style = static_cast<SMeterWidget::FaceStyle>(i);
        face.choices.append({SMeterWidget::faceStyleLabel(style), SMeterWidget::faceStyleKey(style), true, {}});
    }
    face.defaultValue = QJsonValue();
    ContentProperty limit;
    limit.key = QLatin1String(kPowerLimitKey);
    limit.label = QObject::tr("Power limit (0 follows the radio)");
    limit.kind = ContentProperty::Kind::Integer;
    limit.minimum = 0;
    limit.maximum = static_cast<int>(kMaxBarPowerLimitW);
    limit.suffix = QObject::tr(" W");
    limit.defaultValue = 0;
    return {tx, rx, peak, decay, face, limit};
}

// The same mirrored source as the main applet dial, including remote
// windows. A copy without a poller still follows its model's readings.
// A copy with a power limit of its own keeps its scale (followScale false).
void followExternalAmp(SMeterWidget* dial, RadioModel* radio, bool directReadings, bool followScale)
{
    MainPowerMeter* meter = radio->mainPowerMeter();
    if (followScale) {
        const auto scale = [dial, meter] { dial->setPowerDisplayScale(meter->visualLimitW(), meter->scaleTopW()); };
        QObject::connect(meter, &MainPowerMeter::scaleChanged, dial, scale);
        scale();
    }
    if (directReadings) {
        const auto reading = [dial, meter] {
            dial->setTxMeters(static_cast<float>(meter->forwardW()), static_cast<float>(meter->swr()));
        };
        QObject::connect(meter, &MainPowerMeter::readingChanged, dial, reading);
        reading();
    }
}

// The last transmit values, which SMeterWidget takes in pairs.
struct TxValues {
    float power = 0.0f;
    float swr = 1.0f;
    float mic = -50.0f;
    float comp = 0.0f;
    // The appended modes, at the reading Thetis shows with none
    // (MeterManager.cs:2077-2086, 2124-2133 [v2.10.3.15]).
    float alcGain = 0.0f;
    float alcGroup = -30.0f;
    float volts = 0.0f;
    float amps = 0.0f;
};

QWidget* makeDial(const ContentEntry& entry, QWidget* parent, const LongpathContent::Sources& sources,
                  ContentRenderMode mode)
{
    auto* dial = new SMeterWidget(SMeterWidget::SettingsStore::Instance, parent);
    applyConfig(*dial, entry.config);
    // Sized as the host sizes the Main Panel's dial (applet:s_meter row).
    dial->setMaximumHeight(QWIDGETSIZE_MAX);
    dial->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    dial->setMinimumHeight(130);
    // No reading until the first one arrives: "--", as a Longpath needle
    // is offline until its first reading.
    dial->setLevel(static_cast<float>(kNoMeterReadingDbm));
    const bool ownScale = powerLimitW(entry.config) > 0;
    if (sources.radio) {
        dial->setTransmitting(sources.radio->isTransmitting());
        // The amplifier's scale now, as ampStateChanged last set the Main
        // Panel's dial. A copy with its own power limit keeps that.
        if (!ownScale) {
            dial->setPowerDisplayScale(sources.radio->mainPowerMeter()->visualLimitW(),
                                       sources.radio->mainPowerMeter()->scaleTopW());
        }
    }

    QPointer<SMeterWidget> guard(dial);
    auto tx = std::make_shared<TxValues>();
    // RX bindings the poller says have no reading (a Core that does not send them).
    auto rxUnavailable = std::make_shared<QSet<int>>();
    QPointer<RadioModel> radio(sources.radio);
    const auto onReading = [guard, tx, rxUnavailable, radio](int binding, double value) {
        if (!guard) { return; }
        const bool none = isNoMeterReading(value);
        if (isRxBinding(binding)) {
            if (binding != rxBindingForMode(rxModeNames().at(static_cast<std::size_t>(guard->rxMode())))) { return; }
            guard->setLevel(static_cast<float>(none || rxUnavailable->contains(binding) ? kNoMeterReadingDbm : value));
            return;
        }
        switch (binding) {
        case MeterBinding::TxPower: tx->power = none ? 0.0f : static_cast<float>(value); break;
        case MeterBinding::TxSwr: tx->swr = none ? 1.0f : static_cast<float>(value); break;
        case MeterBinding::TxMic: tx->mic = none ? -50.0f : static_cast<float>(value); break;
        case MeterBinding::TxComp: tx->comp = none ? 0.0f : static_cast<float>(value); break;
        case MeterBinding::TxAlcGain: tx->alcGain = none ? 0.0f : static_cast<float>(value); break;
        case MeterBinding::TxAlcGroup: tx->alcGroup = none ? -30.0f : static_cast<float>(value); break;
        case MeterBinding::HwVolts: tx->volts = none ? 0.0f : static_cast<float>(value); break;
        case MeterBinding::HwAmps: tx->amps = none ? 0.0f : static_cast<float>(value); break;
        default: return;
        }
        if (binding == MeterBinding::TxPower || binding == MeterBinding::TxSwr) {
            guard->setTxMeters(tx->power, tx->swr);
        } else if (binding == MeterBinding::TxAlcGain || binding == MeterBinding::TxAlcGroup) {
            guard->setAlcMeters(tx->alcGain, tx->alcGroup);
        } else if (binding == MeterBinding::HwVolts || binding == MeterBinding::HwAmps) {
            guard->setSupplyMeters(tx->volts, tx->amps);
        } else {
            // MeterPoller feeds the Main Panel's dial the same way.
            guard->setMicMeters(tx->mic, tx->comp, tx->mic, tx->comp);
        }
    };
    const QJsonObject context = entry.context;
    MeterPoller* poller = sources.poller;
    if (poller) {
        for (int binding : registeredBindings()) {
            const QString reason = poller->bindingUnavailableReason(binding);
            if (const auto txMode = txModeForBinding(binding)) { dial->setTxModeUnavailable(*txMode, reason); }
            if (isRxBinding(binding) && !reason.isEmpty()) { rxUnavailable->insert(binding); }
        }
    }
    // The VHF S9 rule follows the copy's slice, as a Longpath needle's does
    // (an inert preview reads the slice once).
    LongpathContent::followAboveS9Frequency(dial, sources, context, mode == ContentRenderMode::Preview,
        [guard](bool above) {
            if (guard) { guard->setAboveS9Frequency(above); }
        });
    if (mode == ContentRenderMode::Preview) {
        // Inert preview: the cached readings once, nothing registered.
        if (poller) {
            for (int binding : registeredBindings()) {
                if (const std::optional<double> value = poller->cachedReading(context, binding)) {
                    onReading(binding, *value);
                }
            }
        }
        return dial;
    }
    if (poller) {
        QObject::connect(poller, &MeterPoller::readingUpdated, dial,
            [onReading, context](const QJsonObject& from, int binding, double value) {
                if (from == context) { onReading(binding, value); }
            });
        QObject::connect(poller, &MeterPoller::bindingAvailabilityChanged, dial,
            [guard, rxUnavailable](int binding, const QString& reason) {
                if (!guard) { return; }
                if (const auto txMode = txModeForBinding(binding)) { guard->setTxModeUnavailable(*txMode, reason); }
                if (!isRxBinding(binding)) { return; }
                if (reason.isEmpty()) { rxUnavailable->remove(binding); return; }
                rxUnavailable->insert(binding);
                if (binding == rxBindingForMode(rxModeNames().at(static_cast<std::size_t>(guard->rxMode())))) {
                    guard->setLevel(static_cast<float>(kNoMeterReadingDbm));
                }
            });
        // The receiver behind the context changed: the old one's value and
        // peak no longer apply, as a Longpath needle drops them.
        QObject::connect(poller, &MeterPoller::rxSourceChanged, dial, [guard, context](const QJsonObject& from) {
            if (!guard || from != context) { return; }
            guard->setLevel(static_cast<float>(kNoMeterReadingDbm));
            guard->resetPeak();
        });
        poller->addReadingConsumer(dial, context, registeredBindings(), onReading);
    }
    if (sources.radio) {
        QObject::connect(sources.radio, &RadioModel::transmittingChanged, dial, &SMeterWidget::setTransmitting);
        followExternalAmp(dial, sources.radio, !poller, !ownScale);
    }

    // The right-click menu changed a setting: the copy keeps it in its
    // entry (ContentConfigNotifier); a new RX mode reads its own binding
    // from the cache at once.
    auto* notifier = new ContentConfigNotifier(dial);
    auto config = std::make_shared<QJsonObject>(entry.config);
    auto shownRx = std::make_shared<int>(rxBindingForMode(rxModeNames().at(static_cast<std::size_t>(dial->rxMode()))));
    QObject::connect(dial, &SMeterWidget::instanceSettingsChanged, notifier,
        [guard, notifier, config, shownRx, poller, context, onReading]() {
            if (!guard) { return; }
            *config = configFrom(*guard, *config);
            guard->setProperty(ContentConfigNotifier::kAppliedConfigProperty, *config);
            const int rx = rxBindingForMode(rxModeNames().at(static_cast<std::size_t>(guard->rxMode())));
            if (rx != *shownRx) {
                *shownRx = rx;
                const std::optional<double> value = poller ? poller->cachedReading(context, rx) : std::nullopt;
                onReading(rx, value ? *value : kNoMeterReadingDbm);
            }
            emit notifier->configEdited(*config);
        });
    return dial;
}

} // namespace

int rxBindingForMode(const QString& rxMode)
{
    if (rxMode == QLatin1String("Sig Avg")) { return MeterBinding::SignalAvg; }
    if (rxMode == QLatin1String("Max Bin")) { return MeterBinding::SignalMaxBin; }
    return MeterBinding::SignalPeak;
}

QList<int> registeredBindings()
{
    return {MeterBinding::SignalPeak, MeterBinding::SignalAvg, MeterBinding::SignalMaxBin,
            MeterBinding::TxPower, MeterBinding::TxSwr, MeterBinding::TxMic, MeterBinding::TxComp,
            MeterBinding::TxAlcGain, MeterBinding::TxAlcGroup, MeterBinding::HwVolts, MeterBinding::HwAmps};
}

void registerDial(ContainerContentRegistry& registry, const LongpathContent::Sources& sources)
{
    // "S-Meter" is the Main Panel's dial (applet:s_meter) in the same list;
    // "(additional)" as the list names the additional S-meter bar.
    registry.registerWidgetFactory(QLatin1String(kDialType),
        {QLatin1String(kDialType), QStringLiteral("S-Meter dial (additional)"), false, true, {}},
        [sources](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return makeDial(entry, parent, sources, mode);
        },
        dialProperties());
}

} // namespace NereusSDR::SMeterDialContent
