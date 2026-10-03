// 2026-09-27: validate transmit-region writes and shared confirmations.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-09-28: parity ruling C4: setRadioSampleRate is asked of the other
// devices as a radio-wide change and answers later, off the air.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-29: a held RX buffer size write rechecks the on-air lock
// before it applies. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code.
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationSharedSettings.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 75 (R-IOS-30; the several-devices design,
// docs/architecture/2026-09-24-several-devices-on-one-core-design.md,
// sections 7.1 to 7.4, rulings 5.11a, 6.1 and 7.1 to 7.8, D53, D60, D61,
// D64): StationServer's part in settings that affect every device.
//
// Some settings belong to the radio, not to one slice. A change to one of
// them from one device reaches the slices of every other device listening
// through what it touches, so the Core asks first (D53):
//
//   - The list (ruling 7.1): classifyShared() says whether a command, a
//     property write or a settings write is on it, what it touches
//     (DisturbanceCheck::Scope), its words ({label, from, to}) and what it
//     acts on (the target, ruling 7.6). A write that sets the value already
//     there is no change and applies as before.
//   - DisturbanceCheck names each other device it would disturb. None: the
//     change applies at once, as today. Some (ruling 7.1a, the operator's
//     ruling of 2026-09-28): a change on a row that asks (kSharedTiers in
//     sharedTierOf) with a connected device disturbed is held: a device
//     with the feature gets the change's own answer ("Waiting for you to
//     confirm.") and a confirm.request of kind sharedSetting naming the
//     connected devices; an older window gets the refusal only, naming
//     them. Any other change applies at once (applySharedNow), and each
//     disturbed device is told once it has applied, an away device on its
//     return.
//   - On proceed (rulings 7.5 and 7.6): an expired question, a target whose
//     value moved since the question was asked, or a set that grew, changes
//     nothing (the last is asked again). Otherwise the change is applied as
//     the original request would have been, the answer carries its readback
//     (ruling 7.4a), and each disturbed device is told (notice
//     settingChanged, no Take it back).
//   - A new write from the requester to the same target cancels its open
//     question.
//   - Ruling 5.11a: when band tracking keeps the receive antenna because
//     another device listens through it (RadioModel::receiveAntennaKept),
//     the person tuning is told (notice antennaKept).
//
// Transmit (Task 34's TransmitHolder) joins at transmitForCheck(): a
// change that touches the transmitter disturbs its holder (asked while the
// holder is another device), a holder on the air is "transmitting", and
// ruling 7.4's on-air refusal (DisturbanceCheck::refusedOnAir) fires while
// it is keyed. The Core settings that touch only the transmitter (External
// TX Inhibit's keys and RxOnly, receive only) are on the list as
// transmitter changes; the transmit settings themselves are a permitted
// session's (StationServer's transmit gate), so a device that does not hold
// transmit is refused them, never asked.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 75 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: checkpoint join (R-IOS-30, R-R3-49): the parity lane's
//               accessory verbs (moveTgxlRelay, setTgxlAddress,
//               setPgxlOperate, setPgxlAddress, setRfKitOperate,
//               setRfKitAntenna, setRfKitTciMode, setRfKitAddress),
//               setAlexTxAntenna and the two-way transmit antennas and
//               relays join the list. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: the Core's own MOX is not a
//               holder on the air for the shared-settings check (the
//               parity rounds' Thetis rule); the saved accessory
//               addresses go ahead on the air. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2: every holder on the air counts,
//               the station device's own keys included (onAirHolder),
//               exempt by change not by holder; ruling 8.11's freeze on
//               every path (XIT, pan moves, a stored change at proceed); a
//               hosting desktop's key named after it. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Trunk merge of remote transmit (R-R3-46, R-IOS-02): the
//               Alex tab's three transmit high-pass switches are the
//               transmitter's settings (SettingsProxyServer::sharedFamilyOf),
//               as the TX antennas are. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-26: parity Task 21 (R-IOS-18): station.selectRadio joins the
//               list (every slice and the transmitter). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: setTgxlAntenna's question counts the tuner's 0-based
//               antennaA from 1, as its port and buttons do; an RF-Kit
//               external antenna in use is never the internal one tapped.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-28: Ruling 7.1a (the operator's ruling): two tiers from one
//               table (sharedTierOf); a small adjustment applies at once and
//               tells, and away devices are never asked about. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-R3-46 / R-R3-11: a stepAtt attenuationDb write reaches
//               slice A's ADC, rx2AttenuationDb the other ADC's (both while
//               diversity links them). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 2: a verb
//               naming a slice the requester may not change
//               (changeRefusal) is left to the dispatcher's refusal.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: slice control plan Task 8, Amendment 8a: a slice of a
//               device that is not here is never named in a question. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: slice control plan Task 10: requesters are read through
//               peerFor, so the station device is one. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/StationServer.h"

#include "core/station/StationRadios.h"

#include <QJsonObject>
#include <QLoggingCategory>
#include <QSet>

#include <algorithm>
#include <cmath>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/DeviceLayoutStore.h"
#include "core/SkuUiProfile.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/TxRefusal.h"
#include "core/SliceOwnership.h"
#include "core/SliceStreamAllocator.h"
#include "core/StepAttenuatorController.h"
#include "core/accessories/AlexController.h"
#include "core/dsp/Notch.h"
#include "core/session/ConfirmStep.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/ReceiverPlanner.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionTransport.h"
#include "core/session/StateMirror.h"
#include "core/settings/SettingsProxyServer.h"
#include "models/Band.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcShared, "nereus.station.shared")

// Ruling 7.6.
constexpr const char* kTargetChangedReason =
    "That setting changed since you asked. Make the change again.";

// The mirrored objects the list names (StationServer.cpp's keys).
constexpr const char* kStepAtt = "stepAtt";
constexpr const char* kAlexAntennas = "alexAntennas";
constexpr const char* kTransmit = "transmit";
constexpr const char* kPureSignalSettings = "pureSignalSettings";
constexpr const char* kNotches = "notches";
constexpr const char* kAmplifier = "amplifier";
constexpr const char* kTuner = "tuner";
constexpr const char* kRfKit = "rfkit";

// Ruling 7.4 (D60), for the holder Task 34 brings.
QString onAirReason(const QString& holderShortName)
{
    return QStringLiteral("%1 is on the air. Try again when they stop.").arg(holderShortName);
}

// Section 7.4's notice words. `name` is the device's numbered name, the
// operator's own words.
QString settingChangedReason(const QString& name, const QString& label, const QString& from,
                             const QString& to)
{
    return QStringLiteral("%1 changed %2 from %3 to %4.").arg(name, label, from, to);
}

QString movedSentence(const QStringList& letters)
{
    return letters.size() == 1
               ? QStringLiteral("Your slice %1 moved to another receiver.")
                     .arg(ReceiverPlanner::joinWords(letters))
               : QStringLiteral("Your slices %1 moved to another receiver.")
                     .arg(ReceiverPlanner::joinWords(letters));
}

QString closedSentence(const QStringList& letters)
{
    return letters.size() == 1
               ? QStringLiteral("Your slice %1 closed: no receiver was free.")
                     .arg(ReceiverPlanner::joinWords(letters))
               : QStringLiteral("Your slices %1 closed: no receiver was free.")
                     .arg(ReceiverPlanner::joinWords(letters));
}

QString pausedSentence(const QStringList& letters)
{
    return letters.size() == 1
               ? QStringLiteral("Your slice %1 pauses while the radio transmits.")
                     .arg(ReceiverPlanner::joinWords(letters))
               : QStringLiteral("Your slices %1 pause while the radio transmits.")
                     .arg(ReceiverPlanner::joinWords(letters));
}

// Ruling 5.11a (D61's words, the other device's short name in place of
// "the iPad").
QString antennaKeptReason(const QString& antenna, const QStringList& names)
{
    return names.size() == 1
               ? QStringLiteral("The antenna stays on %1 while %2 listens on it.")
                     .arg(antenna, names.first())
               : QStringLiteral("The antenna stays on %1 while %2 listen on it.")
                     .arg(antenna, ReceiverPlanner::joinWords(names));
}

// "ADC 1" for ADC0 (section 7.3: a screen shows adc + 1).
QString adcWords(int adc)
{
    const int number = adc + 1;
    return QStringLiteral("ADC %1").arg(number);
}

QString receiverWords(int stream)
{
    const int number = stream + 1;
    return QStringLiteral("Receiver %1").arg(number);
}

QString onOff(bool on)
{
    return on ? QStringLiteral("On") : QStringLiteral("Off");
}

QString numberWords(double value)
{
    return QString::number(value, 'g', 12);
}

// A value as the operator reads it, with an optional unit.
QString valueWords(const QVariant& value, MirrorWireKind kind, const QString& unit = QString())
{
    switch (kind) {
    case MirrorWireKind::Bool:
        return onOff(value.toBool());
    case MirrorWireKind::Float64:
        return numberWords(value.toDouble()) + unit;
    case MirrorWireKind::Int64:
    case MirrorWireKind::Enum:
        return QString::number(value.toLongLong()) + unit;
    case MirrorWireKind::Utf8:
        return value.toString() + unit;
    case MirrorWireKind::Unsupported:
        break;
    }
    return value.toString();
}

QString preampWords(int mode)
{
    switch (static_cast<PreampMode>(mode)) {
    case PreampMode::Off:
        return QStringLiteral("Off");
    case PreampMode::On:
        return QStringLiteral("On");
    case PreampMode::Minus10:
        return QStringLiteral("-10 dB");
    case PreampMode::Minus20:
        return QStringLiteral("-20 dB");
    case PreampMode::Minus30:
        return QStringLiteral("-30 dB");
    case PreampMode::Minus40:
        return QStringLiteral("-40 dB");
    case PreampMode::Minus50:
        return QStringLiteral("-50 dB");
    case PreampMode::SaMinus10:
        return QStringLiteral("-10 dB");
    case PreampMode::SaMinus20:
        return QStringLiteral("-20 dB");
    case PreampMode::SaMinus30:
        return QStringLiteral("-30 dB");
    }
    return QString::number(mode);
}

QString blankerWords(int mode)
{
    switch (static_cast<NbMode>(mode)) {
    case NbMode::Off:
        return QStringLiteral("Off");
    case NbMode::NB:
        return QStringLiteral("NB");
    case NbMode::NB2:
        return QStringLiteral("NB2");
    }
    return QString::number(mode);
}

QString filterPolicyWords(int mode)
{
    switch (static_cast<AlexController::BpfMode>(mode)) {
    case AlexController::BpfMode::Auto:
        return QStringLiteral("Automatic");
    case AlexController::BpfMode::ForceBand:
        return QStringLiteral("Always filter");
    case AlexController::BpfMode::ForceBypass:
        return QStringLiteral("Always bypass");
    }
    return QString::number(mode);
}

QString notchWords(double centreHz, double widthHz)
{
    return QStringLiteral("%1 MHz, %2 Hz wide")
        .arg(QString::number(centreHz / 1e6, 'f', 6), QString::number(std::lround(widthHz)));
}

const MirrorUpdate* argumentNamed(const QList<MirrorUpdate>& args, const char* name)
{
    for (const MirrorUpdate& a : args) {
        if (a.name == name) {
            return &a;
        }
    }
    return nullptr;
}

int intArgument(const QList<MirrorUpdate>& args, const char* name, int fallback = -1)
{
    const MirrorUpdate* a = argumentNamed(args, name);
    bool ok = false;
    const qlonglong v = a != nullptr ? a->value.toLongLong(&ok) : 0;
    return ok ? static_cast<int>(v) : fallback;
}

double doubleArgument(const QList<MirrorUpdate>& args, const char* name, double fallback = 0.0)
{
    const MirrorUpdate* a = argumentNamed(args, name);
    bool ok = false;
    const double v = a != nullptr ? a->value.toDouble(&ok) : 0.0;
    return ok && std::isfinite(v) ? v : fallback;
}

bool boolArgument(const QList<MirrorUpdate>& args, const char* name)
{
    const MirrorUpdate* a = argumentNamed(args, name);
    return a != nullptr && a->value.toBool();
}

// The 14 per-band values of an antenna list ("1,1,2,...").
QList<int> bandList(const QString& text)
{
    QList<int> values;
    for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        values.append(part.trimmed().toInt());
    }
    return values;
}

// On a 2-ADC board a receiver's antenna picks its ADC: ANT1 to ANT3 feed
// ADC0, the receive-only inputs (EXT1, EXT2, ...) ADC1 (6.5).
int adcOfAntennaLabel(const QString& label)
{
    return label.startsWith(QLatin1String("ANT")) ? 0 : 1;
}

} // namespace

// ── The topology ─────────────────────────────────────────────────────────

DisturbanceCheck::Transmit StationServer::transmitForCheck() const
{
    // Task 34's join (merge of the trunk into the transmit lane):
    // TransmitHolder's holder (a device id, or the station device for the
    // radio's own PTT and the Core's own keys), whether it is on the air,
    // and the transmit slice.
    DisturbanceCheck::Transmit transmit;
    if (m_transmitHolder) {
        if (const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder()) {
            // Fix wave 2, Important 1: every holder counts, the station
            // device's own keys included (ruling 7.4 names no exception).
            transmit.holder = holder->deviceId;
            transmit.keyed = holder->keyed;
            // The station device has no session to ask or tell; its keys
            // still hold the changes ruling 7.4 names (refusedOnAir).
            transmit.holderAskable = holder->deviceId != KeyerIdentity::kStationDeviceId;
        }
    }
    if (!m_radioModel.isNull()) {
        if (const SliceModel* slice = m_radioModel->txBoundSlice()) {
            transmit.txSliceId = slice->sliceIndex();
        }
    }
    // Ruling 8.11: the slice frozen while the station device is keyed,
    // whoever owns it (the requester's own included).
    transmit.frozenSliceId = stationFrozenSlice();
    return transmit;
}

DisturbanceCheck::Topology StationServer::sharedTopology() const
{
    DisturbanceCheck::Topology topology;
    if (m_radioModel.isNull()) {
        return topology;
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    for (const SliceModel* slice : m_radioModel->slices()) {
        if (slice == nullptr) {
            continue;
        }
        // Slice control plan Task 8, Amendment 8a (JJ approved): a slice of
        // a device that is not here (away in its 180 s, or kept for it) is
        // never named in a question; the change still reaches it. It stays
        // in the topology so ruling 7.1a can tell its device on its return
        // (and close it when a rate cannot keep it); connectedAffected()
        // leaves it out of the question.
        DisturbanceCheck::SliceInfo info;
        info.sliceId = slice->sliceIndex();
        info.device = ownership != nullptr ? ownership->mark(info.sliceId).subject() : QByteArray();
        info.stream = slice->streamIndex();
        info.adc = m_radioModel->adcForStream(info.stream);
        info.frequencyHz = slice->frequency();
        info.filterLowHz = slice->filterLow();
        info.filterHighHz = slice->filterHigh();
        topology.slices.append(info);
    }
    topology.transmit = transmitForCheck();
    return topology;
}

// ── The list (ruling 7.1) ────────────────────────────────────────────────

StationServer::SharedChange StationServer::classifyShared(const SessionMessage& message,
                                                          const QByteArray& requester) const
{
    SharedChange c;
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local) {
        return c;
    }
    RadioModel& model = *m_radioModel;
    const bool oneAdc = model.boardCapabilities().adcCount < 2;
    const auto everyReceiver = [&c]() { c.scope.radio = true; };
    const auto adc0OrAll = [&c, oneAdc]() {
        if (oneAdc) {
            c.scope.radio = true;
        } else {
            c.scope.adcs.insert(0);
        }
    };
    // The transmitter, and the transmit path (ruling 7.4's refusal).
    const auto transmitter = [&c]() {
        c.scope.transmitter = true;
        c.scope.transmitPath = true;
    };
    // The tuner and the RF-Kit antenna (ruling 7.2).
    const auto tuner = [&]() {
        adc0OrAll();
        transmitter();
    };
    // PureSignal: on a 1-ADC board every user receiver stops while the
    // holder transmits (6.5); on a 2-ADC board only the transmitter.
    const auto pureSignal = [&]() {
        if (oneAdc) {
            c.scope.radio = true;
            c.scope.effect = DisturbanceCheck::Effect::PausesWhileTransmitting;
        }
        transmitter();
    };
    const auto words = [&c](const QString& label, const QString& from, const QString& to) {
        c.change = QJsonObject{{QStringLiteral("label"), label},
                               {QStringLiteral("from"), from},
                               {QStringLiteral("to"), to}};
    };
    // Ruling 7.1a: which of table 7.1's rows the message touches
    // (kSharedTiers says which of them ask).
    const auto mark = [&c](SharedCategory row) {
        c.categories |= 1u << static_cast<unsigned>(row);
    };

    // ── Property writes ──────────────────────────────────────────────────
    if (message.kind == SessionMessageKind::PropertyWrite) {
        const QByteArray& key = message.objectKey;
        // Most writes (a slice's tuning above all) name nothing on the
        // list: they leave before the object is read.
        const auto sliceListed = [](const QByteArray& n) {
            return n == "rxAntenna" || n == "txAntenna" || n.startsWith("diversity")
                || n == "nbMode" || n.startsWith("nb1") || n == "nb2Mode";
        };
        bool relevant = key == kStepAtt || key == kAlexAntennas || key == kPureSignalSettings
            || key == kNotches || key == kAmplifier || key == kTransmit;
        if (key.startsWith("slice:")) {
            for (const MirrorUpdate& u : message.updates) {
                relevant = relevant || sliceListed(u.name);
            }
        }
        if (!relevant) {
            return c;
        }
        QHash<QByteArray, MirrorUpdate> current;
        for (const MirrorUpdate& u : m_mirror->snapshot(key)) {
            current.insert(u.name, u);
        }
        QStringList names;
        QStringList values;
        bool changes = false;
        bool first = true;
        const auto listed = [&](const MirrorUpdate& update) {
            names.append(QString::fromLatin1(update.name));
            values.append(current.value(update.name).value.toString());
            const bool differs = !current.contains(update.name)
                || current.value(update.name).value != update.value;
            changes = changes || differs;
            const bool wordsNow = first && differs;
            if (differs) {
                first = false;
            }
            return wordsNow;
        };
        const auto finish = [&]() {
            if (names.isEmpty()) {
                return;
            }
            c.target = QString::fromUtf8(key) + QLatin1Char(':') + names.join(QLatin1Char(','));
            c.targetValue = values.join(QLatin1Char('|'));
            c.shared = changes;
        };
        const auto currentWords = [&current](const MirrorUpdate& update, const QString& unit) {
            const MirrorUpdate now = current.value(update.name);
            return valueWords(now.value, update.kind, unit);
        };

        if (key == kStepAtt) {
            // R-R3-46 / R-R3-11: attenuationDb is the attenuator of slice
            // A's ADC, rx2AttenuationDb the other ADC's own; while diversity
            // links them each reaches both.
            const StepAttenuatorController* const att = model.stepAttController();
            const int rx1Adc = att ? att->rx1Adc() : 0;
            const int rx2Adc = att ? att->rx2Adc() : -1;
            const bool attLinked = att && att->adcAttenuatorsLinked();
            for (const MirrorUpdate& u : message.updates) {
                const QByteArray& n = u.name;
                const bool adc1 = n == "rx1Preamp";
                // RX2's value, enable and auto-attenuate settings reach RX2's ADC.
                const bool rx2Att = n == "rx2AttenuationDb" || n == "rx2StepAttEnabled"
                    || n.startsWith("rx2AutoAtt") || n == "rx2PreampMode";
                const bool known = n == "attenuationDb" || n == "enabled" || n == "preampMode"
                    || adc1 || rx2Att || n.startsWith("autoAtt");
                if (!known) {
                    continue;
                }
                mark(SharedCategory::Attenuator);
                int wordsAdc = 0;
                if (adc1) {
                    c.scope.adcs.insert(1);
                    wordsAdc = 1;
                } else if (n == "attenuationDb" || rx2Att) {
                    const int own = rx2Att ? rx2Adc : rx1Adc;
                    if (own >= 0) {
                        c.scope.adcs.insert(own);
                        wordsAdc = own;
                    }
                    if (attLinked && rx2Adc >= 0) {
                        c.scope.adcs.insert(rx1Adc);
                        c.scope.adcs.insert(rx2Adc);
                    }
                } else {
                    c.scope.adcs.insert(0);
                }
                if (!listed(u)) {
                    continue;
                }
                const QString adc = adcWords(wordsAdc);
                if (n == "attenuationDb" || n == "rx2AttenuationDb") {
                    words(QStringLiteral("Attenuator, %1").arg(adc),
                          currentWords(u, QStringLiteral(" dB")),
                          valueWords(u.value, u.kind, QStringLiteral(" dB")));
                } else if (n == "enabled" || n == "rx2StepAttEnabled") {
                    words(QStringLiteral("Attenuator, %1").arg(adc), currentWords(u, {}),
                          valueWords(u.value, u.kind));
                } else if (n == "preampMode" || n == "rx2PreampMode") {
                    words(QStringLiteral("Preamp, %1").arg(adc),
                          preampWords(current.value(n).value.toInt()),
                          preampWords(u.value.toInt()));
                } else if (adc1) {
                    words(QStringLiteral("Preamp, %1").arg(adc), currentWords(u, {}),
                          valueWords(u.value, u.kind));
                } else {
                    const QString unit = n.endsWith("Ms") ? QStringLiteral(" ms") : QString();
                    words(QStringLiteral("Automatic attenuator, %1").arg(adc),
                          currentWords(u, unit), valueWords(u.value, u.kind, unit));
                }
            }
            finish();
            return c;
        }
        if (key == kAlexAntennas) {
            const SkuUiProfile sku = skuUiProfileFor(model.hardwareProfile().model);
            for (const MirrorUpdate& u : message.updates) {
                const QByteArray& n = u.name;
                // Checkpoint join (parity Task 12 with iPhone app Task 75):
                // the transmit antennas and the transmit relays, two-way
                // since radioHardwareVersion 6 (rxOutOnTx since 5), touch the
                // transmitter (the table's "Transmit antenna", ruling 7.8).
                // They disturb the holder of transmit (transmitForCheck).
                if (n == "txAntennas" || n == "blockTxAnt2" || n == "blockTxAnt3"
                    || n == "rxOutOnTx" || n == "ext1OutOnTx" || n == "ext2OutOnTx") {
                    transmitter();
                    mark(SharedCategory::TransmitAntenna);
                    if (!listed(u)) {
                        continue;
                    }
                    if (n != "txAntennas") {
                        const QString label = n == "blockTxAnt2" ? QStringLiteral("Block TX on Ant 2")
                            : n == "blockTxAnt3"                 ? QStringLiteral("Block TX on Ant 3")
                            : n == "rxOutOnTx"                   ? QStringLiteral("RX Bypass on TX")
                            : n == "ext1OutOnTx"                 ? QStringLiteral("Ext 1 on TX")
                                                                 : QStringLiteral("Ext 2 on TX");
                        words(label, currentWords(u, {}), valueWords(u.value, u.kind));
                        continue;
                    }
                    const QList<int> before = bandList(current.value(n).value.toString());
                    const QList<int> after = bandList(u.value.toString());
                    int band = -1;
                    for (int i = 0; i < std::min(before.size(), after.size()) && band < 0; ++i) {
                        if (before.at(i) != after.at(i)) {
                            band = i;
                        }
                    }
                    const QString where = band >= 0
                        ? QStringLiteral(", ") + ReceiverPlanner::bandWords(static_cast<Band>(band))
                        : QString();
                    words(QStringLiteral("Transmit antenna") + where,
                          QStringLiteral("ANT%1").arg(band >= 0 ? before.at(band) : 0),
                          QStringLiteral("ANT%1").arg(band >= 0 ? after.at(band) : 0));
                    continue;
                }
                // Checkpoint join: Disable RX Bypass relay moves the receive
                // side's bypass relay, so it touches what the receive-only
                // antennas touch.
                if (n == "rxOutOverride") {
                    c.scope.transmitPath = true;
                    mark(SharedCategory::ReceiveAntenna);
                    if (oneAdc) {
                        everyReceiver();
                    } else {
                        c.scope.adcs.insert(0);
                        c.scope.adcs.insert(1);
                    }
                    if (listed(u)) {
                        words(QStringLiteral("Disable RX Bypass relay"), currentWords(u, {}),
                              valueWords(u.value, u.kind));
                    }
                    continue;
                }
                if (n != "rxAntennas" && n != "rxOnlyAntennas" && n != "useTxAntennaForRx") {
                    continue;
                }
                // The receive side only; a relay in the transmit path.
                c.scope.transmitPath = true;
                mark(SharedCategory::ReceiveAntenna);
                if (oneAdc) {
                    everyReceiver();
                } else if (n == "rxOnlyAntennas") {
                    c.scope.adcs.insert(0);
                    c.scope.adcs.insert(1);
                } else {
                    c.scope.adcs.insert(0);
                }
                if (!listed(u)) {
                    continue;
                }
                if (n == "useTxAntennaForRx") {
                    words(QStringLiteral("Receive on the transmit antenna"), currentWords(u, {}),
                          valueWords(u.value, u.kind));
                    continue;
                }
                const QList<int> before = bandList(current.value(n).value.toString());
                const QList<int> after = bandList(u.value.toString());
                int band = -1;
                for (int i = 0; i < std::min(before.size(), after.size()) && band < 0; ++i) {
                    if (before.at(i) != after.at(i)) {
                        band = i;
                    }
                }
                const QString where = band >= 0
                    ? QStringLiteral(", ") + ReceiverPlanner::bandWords(static_cast<Band>(band))
                    : QString();
                const auto antenna = [&](int value) {
                    if (n == "rxAntennas") {
                        return QStringLiteral("ANT%1").arg(value);
                    }
                    if (value >= 1 && value <= 3
                        && !sku.rxOnlyLabels[static_cast<size_t>(value - 1)].isEmpty()) {
                        return sku.rxOnlyLabels[static_cast<size_t>(value - 1)];
                    }
                    return value == 0 ? QStringLiteral("None") : QString::number(value);
                };
                const QString label = n == "rxAntennas" ? QStringLiteral("Receive antenna")
                                                        : QStringLiteral("Receive-only antenna");
                words(label + where, antenna(band >= 0 ? before.at(band) : 0),
                      antenna(band >= 0 ? after.at(band) : 0));
            }
            finish();
            return c;
        }
        if (key.startsWith("slice:")) {
            bool ok = false;
            const int sliceId = key.mid(6).toInt(&ok);
            const SliceModel* slice = ok ? model.sliceById(sliceId) : nullptr;
            if (slice == nullptr) {
                return c;
            }
            const int stream = slice->streamIndex();
            for (const MirrorUpdate& u : message.updates) {
                const QByteArray& n = u.name;
                if (n == "rxAntenna") {
                    // The ADC the relay feeds, and the receiver's other
                    // slices.
                    c.scope.transmitPath = true;
                    mark(SharedCategory::ReceiveAntenna);
                    if (stream >= 0) {
                        c.scope.receivers.insert(stream);
                    }
                    if (oneAdc) {
                        everyReceiver();
                    } else {
                        c.scope.adcs.insert(adcOfAntennaLabel(slice->rxAntenna()));
                        c.scope.adcs.insert(adcOfAntennaLabel(u.value.toString()));
                    }
                    if (listed(u)) {
                        words(QStringLiteral("Receive antenna"), currentWords(u, {}),
                              valueWords(u.value, u.kind));
                    }
                } else if (n == "txAntenna") {
                    transmitter();
                    mark(SharedCategory::TransmitAntenna);
                    if (listed(u)) {
                        words(QStringLiteral("Transmit antenna"), currentWords(u, {}),
                              valueWords(u.value, u.kind));
                    }
                } else if (n == "diversityEnabled" || n == "diversityPhaseDeg"
                           || n == "diversityGainDb" || n == "diversityFineNullEnabled") {
                    // Fix wave (the D53 list): the phase, gain and fine null
                    // steer the same combined receiver as diversity itself.
                    mark(SharedCategory::Diversity);
                    if (oneAdc) {
                        everyReceiver();
                    } else {
                        c.scope.receivers.insert(0);
                    }
                    if (listed(u)) {
                        const QString label = n == "diversityPhaseDeg" ? QStringLiteral("Diversity phase")
                            : n == "diversityGainDb"          ? QStringLiteral("Diversity gain")
                            : n == "diversityFineNullEnabled" ? QStringLiteral("Diversity fine null")
                                                              : QStringLiteral("Diversity");
                        const QString unit = n == "diversityPhaseDeg" ? QStringLiteral(" degrees")
                            : n == "diversityGainDb"                  ? QStringLiteral(" dB")
                                                                      : QString();
                        words(label, currentWords(u, unit), valueWords(u.value, u.kind, unit));
                    }
                } else if ((n == "nbMode" || n.startsWith("nb1") || n == "nb2Mode") && stream >= 0) {
                    // Ruling 6.1: a shared receiver's blanker.
                    c.scope.receivers.insert(stream);
                    mark(SharedCategory::NoiseBlanker);
                    if (!listed(u)) {
                        continue;
                    }
                    const QString where = receiverWords(stream);
                    if (n == "nbMode") {
                        words(QStringLiteral("Noise blanker, %1").arg(where),
                              blankerWords(current.value(n).value.toInt()),
                              blankerWords(u.value.toInt()));
                    } else {
                        const QString unit = n.endsWith("Ms") ? QStringLiteral(" ms") : QString();
                        words(QStringLiteral("Noise blanker settings, %1").arg(where),
                              currentWords(u, unit), valueWords(u.value, u.kind, unit));
                    }
                }
            }
            finish();
            return c;
        }
        if (key == kPureSignalSettings
            || (key == kTransmit && argumentNamed(message.updates, "pureSig") != nullptr)) {
            for (const MirrorUpdate& u : message.updates) {
                if (key == kTransmit && u.name != "pureSig") {
                    continue;
                }
                pureSignal();
                mark(SharedCategory::PureSignal);
                if (listed(u)) {
                    words(QStringLiteral("PureSignal"), currentWords(u, {}),
                          valueWords(u.value, u.kind));
                }
            }
            finish();
            return c;
        }
        if (key == kNotches) {
            const NotchModel* notches = model.notchModel();
            for (const MirrorUpdate& u : message.updates) {
                if (u.name != "globalEnabled" && u.name != "autoIncrease") {
                    continue;
                }
                mark(SharedCategory::Notches);
                if (notches != nullptr) {
                    for (const Notch& notch : notches->notches()) {
                        c.scope.ranges.append({notch.centerHz - notch.widthHz / 2.0,
                                               notch.centerHz + notch.widthHz / 2.0});
                    }
                }
                if (listed(u)) {
                    words(u.name == "globalEnabled" ? QStringLiteral("Notches")
                                                    : QStringLiteral("Notch auto-widening"),
                          currentWords(u, {}), valueWords(u.value, u.kind));
                }
            }
            finish();
            return c;
        }
        if (key == kAmplifier) {
            for (const MirrorUpdate& u : message.updates) {
                if (u.name != "operate") {
                    continue;
                }
                transmitter();
                mark(SharedCategory::Amplifier);
                if (listed(u)) {
                    const auto state = [](bool on) {
                        return on ? QStringLiteral("Operate") : QStringLiteral("Standby");
                    };
                    words(QStringLiteral("Amplifier"), state(current.value(u.name).value.toBool()),
                          state(u.value.toBool()));
                }
            }
            finish();
            return c;
        }
        return c;
    }

    // ── Settings writes (link section 8.1) ───────────────────────────────
    // Fix wave I3: a removal returns the key to its default, live, so it is
    // a write of the default: a change whenever the key holds a value.
    const bool removal = message.kind == SessionMessageKind::SettingsRemove;
    if (message.kind == SessionMessageKind::SettingsWrite || removal) {
        const QString key = QString::fromUtf8(message.objectKey);
        const SettingsProxyServer::SharedFamily family = SettingsProxyServer::sharedFamilyOf(key);
        if (family == SettingsProxyServer::SharedFamily::None
            || (!removal && message.updates.isEmpty())) {
            return c;
        }
        const QString now = m_settings.value(key).toString();
        const QString next = removal ? QString() : message.updates.first().value.toString();
        c.target = QStringLiteral("setting:") + key;
        c.targetValue = now;
        c.shared = removal ? m_settings.contains(key) : now != next;
        QString label;
        switch (family) {
        case SettingsProxyServer::SharedFamily::ReceiveOptions: {
            everyReceiver();
            mark(SharedCategory::ReceiveOptions);
            const QString body = key.mid(10, key.size() - 12);
            const QString what = body.startsWith(QLatin1String("BufferSize"))
                ? QStringLiteral("Receive buffer size")
                : body.startsWith(QLatin1String("FilterSize")) ? QStringLiteral("Receive filter size")
                                                               : QStringLiteral("Receive filter type");
            const QString group = body.endsWith(QLatin1String("Phone")) ? QStringLiteral("voice")
                : body.endsWith(QLatin1String("Cw"))                    ? QStringLiteral("CW")
                : body.endsWith(QLatin1String("Dig"))                   ? QStringLiteral("digital")
                                                                        : QStringLiteral("FM");
            label = QStringLiteral("%1, %2 modes").arg(what, group);
            break;
        }
        case SettingsProxyServer::SharedFamily::Amplifier:
            transmitter();
            mark(SharedCategory::Amplifier);
            label = QStringLiteral("Amplifier setting");
            break;
        case SettingsProxyServer::SharedFamily::Tuner:
            tuner();
            mark(SharedCategory::Tuner);
            label = key.startsWith(QLatin1String("RfKit_")) ? QStringLiteral("RF-Kit amplifier setting")
                                                            : QStringLiteral("Tuner setting");
            break;
        case SettingsProxyServer::SharedFamily::Transmitter:
            transmitter();
            mark(SharedCategory::Transmitter);
            if (key == QLatin1String("RxOnly")) {
                label = QStringLiteral("Receive Only");
            } else if (key == QLatin1String("BandPlanRegion")) {
                label = QStringLiteral("Transmit region");
            } else if (SettingsProxyServer::isAlexHpfTransmitSwitchKey(key)) {
                // Trunk merge of remote transmit: the Alex tab's three
                // transmit high-pass switches, in the Alex tab's words.
                // Table 7.1 lists them with the transmit antennas.
                mark(SharedCategory::TransmitAntenna);
                const QString field = key.section(QLatin1Char('/'), -1).toLower();
                label = field == QLatin1String("hpfbypassontx")
                    ? QStringLiteral("HPF Bypass on TX")
                    : field == QLatin1String("hpfbypassonps")
                    ? QStringLiteral("HPF Bypass on PureSignal feedback")
                    : QStringLiteral("Disable 6m LNA on TX");
            } else {
                label = key.endsWith(QLatin1String("Reversed"))
                    ? QStringLiteral("External TX Inhibit, reversed")
                    : QStringLiteral("External TX Inhibit");
            }
            break;
        case SettingsProxyServer::SharedFamily::None:
            break;
        }
        words(label, now.isEmpty() ? QStringLiteral("None") : now,
              removal          ? QStringLiteral("Default")
              : next.isEmpty() ? QStringLiteral("None")
                               : next);
        return c;
    }

    // ── Commands ─────────────────────────────────────────────────────────
    if (message.kind != SessionMessageKind::CommandInvoke) {
        return c;
    }
    const QByteArray& verb = message.commandVerb;
    const QList<MirrorUpdate>& args = message.arguments;
    const QString asItIs = QStringLiteral("As it is");

    if (verb == "requestSliceSampleRate") {
        const int sliceId = intArgument(args, "sliceId");
        const int rateHz = intArgument(args, "rateHz");
        const SliceOwnership* ownership = model.sliceOwnership();
        const RadioModel::SampleRateReach reach = model.planSampleRateReach(
            sliceId, rateHz, [ownership, &requester](int id) {
                const QByteArray who = ownership->mark(id).subject();
                return !who.isEmpty() && who != requester;
            });
        if (reach.stream < 0) {
            return c;
        }
        c.target = QStringLiteral("rate:%1").arg(sliceId);
        c.targetValue = QStringLiteral("%1:%2").arg(reach.stream).arg(reach.fromRateHz);
        if (reach.refused) {
            // Today's plan refuses the requester's own slice (or finds no
            // plan at all): today's path answers, as it always has.
            c.refusedToday = true;
            return c;
        }
        c.shared = rateHz != reach.fromRateHz;
        mark(SharedCategory::SampleRate);
        if (reach.radioWide) {
            c.scope.radio = true;
            c.scope.stopsDataFlow = true;
        } else {
            c.scope.receivers.insert(reach.stream);
        }
        for (int id : reach.changes) {
            c.scope.planned.insert(id, DisturbanceCheck::Effect::Changes);
        }
        for (int id : reach.moves) {
            c.scope.planned.insert(id, DisturbanceCheck::Effect::Moves);
        }
        for (int id : reach.closes) {
            c.scope.planned.insert(id, DisturbanceCheck::Effect::Closes);
        }
        c.closes = reach.closes;
        const auto kHz = [](int hz) { return QStringLiteral("%1 kHz").arg(hz / 1000); };
        words(reach.radioWide ? QStringLiteral("Sample rate")
                              : QStringLiteral("Sample rate, %1").arg(receiverWords(reach.stream)),
              kHz(reach.fromRateHz), kHz(rateHz));
        return c;
    }
    if (verb == "setRadioSampleRate") {
        // Parity ruling C4: the whole radio's rate, every receiver (the
        // several-devices design, 7.1: a radio-wide change that stops the
        // data flow). Every slice's rate changes; none moves or closes.
        const int rateHz = intArgument(args, "rateHz");
        const int fromRateHz = model.connectionSampleRateHz();
        c.target = QStringLiteral("radioRate");
        c.targetValue = QString::number(fromRateHz);
        c.shared = fromRateHz > 0 && rateHz != fromRateHz;
        c.scope.radio = true;
        c.scope.stopsDataFlow = true;
        for (SliceModel* slice : model.slices()) {
            if (slice != nullptr) {
                c.scope.planned.insert(slice->sliceIndex(), DisturbanceCheck::Effect::Changes);
            }
        }
        const auto kHz = [](int hz) { return QStringLiteral("%1 kHz").arg(hz / 1000); };
        words(QStringLiteral("Sample rate"), kHz(fromRateHz), kHz(rateHz));
        return c;
    }
    if (verb == "setAlexRxAntenna" || verb == "setAlexRxAntennaForRadio") {
        const int band = intArgument(args, "band");
        const int antenna = intArgument(args, "antenna");
        const bool rxOnly = boolArgument(args, "rxOnly");
        const AlexController& alex = model.alexController();
        const Band b = static_cast<Band>(band);
        const int now = rxOnly ? alex.rxOnlyAnt(b) : alex.rxAnt(b);
        c.target = QStringLiteral("alex:%1:%2").arg(band).arg(rxOnly ? 1 : 0);
        mark(SharedCategory::ReceiveAntenna);
        c.targetValue = QString::number(now);
        c.shared = now != antenna;
        c.scope.transmitPath = true;
        if (oneAdc) {
            everyReceiver();
        } else {
            c.scope.adcs.insert(0);
            if (rxOnly) {
                c.scope.adcs.insert(1);
            }
        }
        const SkuUiProfile sku = skuUiProfileFor(model.hardwareProfile().model);
        const auto name = [&](int value) {
            if (!rxOnly) {
                return QStringLiteral("ANT%1").arg(value);
            }
            if (value >= 1 && value <= 3
                && !sku.rxOnlyLabels[static_cast<size_t>(value - 1)].isEmpty()) {
                return sku.rxOnlyLabels[static_cast<size_t>(value - 1)];
            }
            return value == 0 ? QStringLiteral("None") : QString::number(value);
        };
        words((rxOnly ? QStringLiteral("Receive-only antenna, %1") : QStringLiteral("Receive antenna, %1"))
                  .arg(ReceiverPlanner::bandWords(b)),
              name(now), name(antenna));
        return c;
    }
    if (verb == "setAlexBpfMode") {
        const int chain = intArgument(args, "chain");
        const int mode = intArgument(args, "mode");
        const int now = static_cast<int>(model.alexController().bpfMode(chain));
        c.target = QStringLiteral("bpf:%1").arg(chain);
        mark(SharedCategory::FilterPolicy);
        c.targetValue = QString::number(now);
        c.shared = now != mode;
        for (int st = 0; st < model.streamAllocator().streamCount(); ++st) {
            if (model.chainForStream(st) == chain) {
                c.scope.receivers.insert(st);
            }
        }
        words(QStringLiteral("Band filters, %1").arg(adcWords(chain)), filterPolicyWords(now),
              filterPolicyWords(mode));
        return c;
    }
    // Checkpoint join (parity mini-round, radioHardwareVersion 6): one
    // band's TX antenna, the table's "Transmit antenna" (ruling 7.8).
    if (verb == "setAlexTxAntenna" || verb == "setAlexTxAntennaForRadio") {
        const int band = intArgument(args, "band");
        const int antenna = intArgument(args, "antenna");
        const Band b = static_cast<Band>(band);
        const int now = model.alexController().txAnt(b);
        transmitter();
        c.target = QStringLiteral("alextx:%1").arg(band);
        mark(SharedCategory::TransmitAntenna);
        c.targetValue = QString::number(now);
        c.shared = now != antenna;
        const QString where = ReceiverPlanner::bandWords(b);
        words(QStringLiteral("Transmit antenna, %1").arg(where),
              QStringLiteral("ANT%1").arg(now), QStringLiteral("ANT%1").arg(antenna));
        return c;
    }
    if (verb.startsWith("ps3.")) {
        // Every PureSignal action but the two-tone test (Task 34's
        // transmitter), the display subscription and saving a correction,
        // which change nothing on the radio.
        static const QHash<QByteArray, QString> kActions{
            {"ps3.off", QStringLiteral("Off")},
            {"ps3.single", QStringLiteral("Calibrate once")},
            {"ps3.automatic", QStringLiteral("Automatic")},
            {"ps3.applyCurrent", QStringLiteral("Apply the current correction")},
            {"ps3.restoreCorrection", QStringLiteral("Restore a saved correction")},
        };
        const auto action = kActions.constFind(verb);
        if (action == kActions.cend()) {
            return c;
        }
        pureSignal();
        mark(SharedCategory::PureSignal);
        c.target = QStringLiteral("ps3");
        c.shared = true;
        words(QStringLiteral("PureSignal"), asItIs, *action);
        return c;
    }
    if (verb.startsWith("notch.")) {
        const NotchModel* notches = model.notchModel();
        if (notches == nullptr) {
            return c;
        }
        mark(SharedCategory::Notches);
        if (verb == "notch.add") {
            const double centre = doubleArgument(args, "centreHz");
            const double width = doubleArgument(args, "widthHz");
            c.scope.ranges.append({centre - width / 2.0, centre + width / 2.0});
            c.target = QStringLiteral("notch:new");
            c.shared = true;
            words(QStringLiteral("Notch"), QStringLiteral("None"), notchWords(centre, width));
            return c;
        }
        const int id = intArgument(args, "id");
        const Notch* notch = notches->notchById(id);
        if (notch == nullptr) {
            return c;
        }
        c.scope.ranges.append({notch->centerHz - notch->widthHz / 2.0,
                               notch->centerHz + notch->widthHz / 2.0});
        c.target = QStringLiteral("notch:%1").arg(id);
        c.targetValue = QStringLiteral("%1:%2:%3")
                            .arg(numberWords(notch->centerHz), numberWords(notch->widthHz))
                            .arg(notch->active ? 1 : 0);
        const QString at = QStringLiteral("Notch at %1 MHz")
                               .arg(QString::number(notch->centerHz / 1e6, 'f', 6));
        if (verb == "notch.move") {
            const double centre = doubleArgument(args, "centreHz", notch->centerHz);
            const double width = doubleArgument(args, "widthHz", notch->widthHz);
            c.scope.ranges.append({centre - width / 2.0, centre + width / 2.0});
            c.shared = centre != notch->centerHz || width != notch->widthHz;
            words(QStringLiteral("Notch"), notchWords(notch->centerHz, notch->widthHz),
                  notchWords(centre, width));
        } else if (verb == "notch.setActive") {
            const bool active = boolArgument(args, "active");
            c.shared = active != notch->active;
            words(at, onOff(notch->active), onOff(active));
        } else if (verb == "notch.delete") {
            c.shared = true;
            words(at, onOff(notch->active), QStringLiteral("Removed"));
        }
        return c;
    }
    // The tuner and the RF-Kit amplifier's antenna (ruling 7.2). Task 42's
    // tuner.* and rfkit.antenna verbs join this list when that task lands.
    if (verb == "setTgxlAntenna" || verb == "setTgxlOperate" || verb == "setTgxlBypass"
        || verb == "tuner.antenna" || verb == "tuner.operate" || verb == "tuner.bypass") {
        QHash<QByteArray, QVariant> now;
        for (const MirrorUpdate& u : m_mirror->snapshot(kTuner)) {
            now.insert(u.name, u.value);
        }
        tuner();
        mark(SharedCategory::Tuner);
        if (verb == "setTgxlAntenna" || verb == "tuner.antenna") {
            // R-IOS-30: the port is the button's number, 1 to 3 (activate
            // ant=N); the tuner reports antA 0-based (0 is ANT 1), and the
            // mirror's antennaA carries it as reported. Compare and name
            // both in the buttons' numbers.
            const int port = intArgument(args, "port");
            const int was = now.value("antennaA").toInt() + 1;
            c.target = QStringLiteral("tuner:antenna");
            c.targetValue = QString::number(was);
            c.shared = port != was;
            words(QStringLiteral("Tuner antenna"), QStringLiteral("ANT%1").arg(was),
                  QStringLiteral("ANT%1").arg(port));
        } else if (verb == "setTgxlOperate" || verb == "tuner.operate") {
            const bool on = boolArgument(args, "on");
            const bool was = now.value("isOperate").toBool();
            const auto state = [](bool operate) {
                return operate ? QStringLiteral("Operate") : QStringLiteral("Standby");
            };
            c.target = QStringLiteral("tuner:operate");
            c.targetValue = onOff(was);
            c.shared = on != was;
            words(QStringLiteral("Tuner"), state(was), state(on));
        } else {
            const bool on = boolArgument(args, "on");
            const bool was = now.value("isBypass").toBool();
            c.target = QStringLiteral("tuner:bypass");
            c.targetValue = onOff(was);
            c.shared = on != was;
            words(QStringLiteral("Tuner bypass"), onOff(was), onOff(on));
        }
        return c;
    }
    // Checkpoint join (parity Tasks 9 and 10): the Power Genius's and the
    // RF-Kit's OPERATE and STANDBY (the amplifier, the transmitter) and the
    // RF-Kit's antenna switch (ruling 7.2, what the tuner touches).
    if (verb == "setPgxlOperate" || verb == "setRfKitOperate" || verb == "setRfKitAntenna"
        || verb == "amp.operate" || verb == "amp.standby" || verb == "rfkit.operate"
        || verb == "rfkit.standby" || verb == "rfkit.antenna") {
        const bool pgxl = verb == "setPgxlOperate" || verb == "amp.operate"
            || verb == "amp.standby";
        QHash<QByteArray, QVariant> now;
        for (const MirrorUpdate& u : m_mirror->snapshot(pgxl ? kAmplifier : kRfKit)) {
            now.insert(u.name, u.value);
        }
        if (verb == "setRfKitAntenna" || verb == "rfkit.antenna") {
            tuner();
            mark(SharedCategory::Tuner);
            // R-IOS-30: the port and activeAntennaNumber both count from 1
            // (0 is none reported), but the amp numbers its external
            // antennas from 1 too, and the port is always an internal one
            // (Rf2ksApplet::setActiveAntenna), so an external antenna in
            // use is never the one tapped.
            const int port = intArgument(args, "port");
            const int number = now.value("activeAntennaNumber").toInt();
            const bool external = now.value("activeAntennaExternal").toBool();
            c.target = QStringLiteral("rfkit:antenna");
            c.targetValue = (external ? QStringLiteral("E") : QString()) + QString::number(number);
            c.shared = external || port != number;
            QString from = QStringLiteral("ANT%1").arg(number);
            if (number <= 0) {
                from = QStringLiteral("None");
            } else if (external) {
                from = QStringLiteral("External antenna %1").arg(number);
            }
            words(QStringLiteral("RF-Kit antenna"), from, QStringLiteral("ANT%1").arg(port));
            return c;
        }
        transmitter();
        mark(SharedCategory::Amplifier);
        const bool on = verb == "amp.operate" || verb == "rfkit.operate" ? true
            : verb == "amp.standby" || verb == "rfkit.standby" ? false
            : boolArgument(args, "on");
        const bool was = now.value("operate").toBool();
        const auto state = [](bool operate) {
            return operate ? QStringLiteral("Operate") : QStringLiteral("Standby");
        };
        // The Power Genius's target is the amplifier property write's, so
        // a later write to either cancels an open question.
        c.target = pgxl ? QStringLiteral("amplifier:operate") : QStringLiteral("rfkit:operate");
        c.targetValue = now.value("operate").toString();
        c.shared = on != was;
        words(pgxl ? QStringLiteral("Amplifier") : QStringLiteral("RF-Kit amplifier"), state(was),
              state(on));
        return c;
    }
    // Fix wave (the D53 list): turning 4O3A on or off connects or drops the
    // amplifier and the tuner together, so it reaches what the tuner does.
    if (verb == "setFourO3AEnabled") {
        const bool on = boolArgument(args, "enabled");
        const bool was = model.fourO3AEnabled();
        tuner();
        mark(SharedCategory::FourO3A);
        c.target = QStringLiteral("fourO3A");
        c.targetValue = onOff(was);
        c.shared = on != was;
        words(QStringLiteral("4O3A amplifier and tuner"), onOff(was), onOff(on));
        return c;
    }
    // Checkpoint join: parity Task 8's relay nudge and saved address.
    // scanTgxlLan only listens, and joins no list.
    static const QSet<QByteArray> kTunerVerbs{"configureTgxl", "disconnectTgxl", "setTgxlName",
                                              "setTgxlNetwork", "saveTgxlSettings",
                                              "moveTgxlRelay", "setTgxlAddress"};
    // The amplifier, its interlock and power limit (the transmitter). Task
    // 42's amp.operate and amp.standby join when that task lands.
    static const QSet<QByteArray> kAmplifierVerbs{
        "configurePgxl",   "disconnectPgxl",   "setPgxlConnectionSettings",
        "setPgxlName",     "setPgxlHardware",  "setPgxlNetwork",
        "savePgxlSettings", "setTxInterlockPolicy", "setPgxlPowerCap",
        "configureRfKit",  "disconnectRfKit",  "setRfKitEnabled",
        // Checkpoint join: parity Tasks 9 and 10's saved addresses and the
        // RF-Kit's TCI mode. scanPgxlLan only listens, and joins no list.
        "setPgxlAddress",  "setRfKitAddress",  "setRfKitTciMode"};
    if (kTunerVerbs.contains(verb) || kAmplifierVerbs.contains(verb)) {
        if (kTunerVerbs.contains(verb)) {
            tuner();
            mark(SharedCategory::Tuner);
        } else {
            transmitter();
            mark(verb == "setTxInterlockPolicy" || verb == "setPgxlPowerCap"
                     ? SharedCategory::Interlock
                     : SharedCategory::Amplifier);
        }
        // The operator's ruling (parity mini-round, rulings a to c): the
        // saved addresses go ahead on the air; they touch nothing on the
        // transmit path. They stay on design table 7.1's list, so another
        // device's change still asks the holder (D53, ruling 7.8).
        if (verb == "setTgxlAddress" || verb == "setPgxlAddress" || verb == "setRfKitAddress") {
            c.scope.transmitPath = false;
        }
        c.target = QStringLiteral("verb:") + QString::fromLatin1(verb);
        c.shared = true;
        const QString label = verb == "setTxInterlockPolicy" ? QStringLiteral("Transmit interlock")
            : verb == "setPgxlPowerCap"                      ? QStringLiteral("Amplifier power limit")
            : verb == "moveTgxlRelay"                        ? QStringLiteral("Tuner relays")
            : kTunerVerbs.contains(verb)                     ? QStringLiteral("Tuner settings")
            : verb.contains("RfKit")                         ? QStringLiteral("RF-Kit amplifier")
                                                             : QStringLiteral("Amplifier settings");
        words(label, asItIs, QStringLiteral("Your change"));
        return c;
    }
    // The radio (design table 7.1's last row; parity Task 21): changing the
    // Core's radio touches every slice and the transmitter.
    if (verb == "station.selectRadio") {
        const MirrorUpdate* macArgument = argumentNamed(args, "mac");
        const QString mac = macArgument != nullptr
            ? macArgument->value.toString().trimmed().toUpper()
            : QString();
        const QString current = m_stationRadios.isNull() ? QString()
                                                         : m_stationRadios->currentMac();
        everyReceiver();
        transmitter();
        mark(SharedCategory::Radio);
        c.target = QStringLiteral("radio");
        c.targetValue = current;
        c.shared = !mac.isEmpty() && mac != current;
        const auto nameOf = [this](const QString& m) {
            if (!m_stationRadios.isNull()) {
                if (const auto radio = m_stationRadios->radioFor(m)) {
                    return radio->displayName();
                }
            }
            return m;
        };
        words(QStringLiteral("Radio"),
              current.isEmpty() ? QStringLiteral("None") : nameOf(current), nameOf(mac));
        return c;
    }
    return c;
}

// ── Which changes ask (ruling 7.1a) ─────────────────────────────────────

StationServer::SharedTier StationServer::sharedTierOf(const SharedChange& change)
{
    // JJ's ruling of 2026-09-28 (the several-devices design, ruling 7.1a):
    // a change that can take another device's reception away, or reaches
    // the transmitter, asks first; a small adjustment applies at once and
    // the disturbed devices are told. The one table both tiers come from.
    struct Row {
        SharedCategory category;
        SharedTier tier;
    };
    static constexpr Row kSharedTiers[] = {
        {SharedCategory::SampleRate, SharedTier::Ask},
        {SharedCategory::Radio, SharedTier::Ask},
        {SharedCategory::ReceiveAntenna, SharedTier::Ask},
        {SharedCategory::TransmitAntenna, SharedTier::Ask},
        {SharedCategory::PureSignal, SharedTier::Ask},
        {SharedCategory::Diversity, SharedTier::Ask},
        {SharedCategory::FourO3A, SharedTier::Ask},
        {SharedCategory::Amplifier, SharedTier::Ask},
        {SharedCategory::Tuner, SharedTier::Ask},
        {SharedCategory::Interlock, SharedTier::Ask},
        // Receive Only, the transmit region and External TX Inhibit: not
        // named by the ruling, so they keep asking as before.
        {SharedCategory::Transmitter, SharedTier::Ask},
        {SharedCategory::Attenuator, SharedTier::Notify},
        {SharedCategory::NoiseBlanker, SharedTier::Notify},
        {SharedCategory::Notches, SharedTier::Notify},
        {SharedCategory::ReceiveOptions, SharedTier::Notify},
        {SharedCategory::FilterPolicy, SharedTier::Notify},
    };
    if (change.categories == 0) {
        return SharedTier::Ask;
    }
    for (const Row& row : kSharedTiers) {
        if ((change.categories & (1u << static_cast<unsigned>(row.category))) != 0
            && row.tier == SharedTier::Ask) {
            return SharedTier::Ask;
        }
    }
    return SharedTier::Notify;
}

QList<DisturbanceCheck::Affected> StationServer::connectedAffected(
    const QList<DisturbanceCheck::Affected>& affected) const
{
    QList<DisturbanceCheck::Affected> connected;
    const SliceOwnership* ownership =
        m_radioModel.isNull() ? nullptr : m_radioModel->sliceOwnership();
    for (const DisturbanceCheck::Affected& a : affected) {
        if (planDevice(a.device).state == QLatin1String("away")) {
            continue;
        }
        // Slice control plan Task 8, Amendment 8a: a device whose disturbed
        // slices are all away slices (kept for it) is not here to ask.
        bool here = ownership == nullptr || a.slices.isEmpty();
        for (const DisturbanceCheck::AffectedSlice& s : a.slices) {
            if (ownership == nullptr || !ownership->isAwaySlice(s.sliceId)) {
                here = true;
                break;
            }
        }
        if (here) {
            connected.append(a);
        }
    }
    return connected;
}

QHash<int, QJsonObject> StationServer::sharedSliceWords(
    const QList<DisturbanceCheck::Affected>& affected) const
{
    QHash<int, QJsonObject> sliceWords;
    const ReceiverPlanner planner = receiverPlanner();
    for (const DisturbanceCheck::Affected& a : affected) {
        for (const DisturbanceCheck::AffectedSlice& s : a.slices) {
            const QJsonArray one = planner.noticeSlicesJson({s.sliceId});
            if (!one.isEmpty()) {
                sliceWords.insert(s.sliceId, one.first().toObject());
            }
        }
    }
    return sliceWords;
}

bool StationServer::applySharedNow(SessionTransport* transport, const SessionMessage& message,
                                   const SharedChange& change,
                                   const QList<DisturbanceCheck::Affected>& affected)
{
    DeferredProceed notices;
    notices.transport = transport;
    notices.affected = affected;
    notices.sliceWords = sharedSliceWords(affected);
    notices.change = change.change;
    notices.requester = m_peers.value(transport).sessionDeviceId;
    notices.answersProceed = false;
    if (message.kind != SessionMessageKind::CommandInvoke) {
        // Today's path applies the write; tellAppliedNow() follows it.
        m_appliedNow = notices;
        return false;
    }
    // A command is run here, as today's path would run it, and its devices
    // are told when its result says it was taken (finishDeferredProceed).
    const quint64 session = m_peers.value(transport).sessionId;
    const ResultKey key{session, message.commandVerb, message.commandId};
    m_deferredProceeds.insert(key, notices);
    const bool rateChange = message.commandVerb == "requestSliceSampleRate";
    if (rateChange && !change.closes.isEmpty()) {
        // The slices of away devices the rate cannot keep close, as a
        // confirmed rate change closes them (proceedSharedSetting), saved
        // for their owners' return.
        QHash<int, SessionCommandDispatcher::ClosingSlice> closingOwners;
        for (int id : change.closes) {
            closingOwners.insert(
                id, SessionCommandDispatcher::ClosingSlice{
                        QPointer<SliceModel>(m_radioModel->sliceById(id)),
                        m_radioModel->sliceOwnership()->mark(id).subject()});
        }
        const QPointer<StationServer> self(this);
        m_dispatcher->setRateClosing(
            closingOwners, [self, key](int id) {
                if (self.isNull() || self->m_radioModel.isNull()) {
                    return;
                }
                const QByteArray who = self->m_radioModel->sliceOwnership()->mark(id).subject();
                if (!self->closeSliceFor(id, self->saveForAbsentSubject(id), nullptr)) {
                    return;
                }
                const auto entry = self->m_deferredProceeds.find(key);
                if (entry != self->m_deferredProceeds.end()
                    && !entry->closedDevices.contains(who)) {
                    entry->closedDevices.append(who);
                }
            },
            QString::fromLatin1(kTargetChangedReason));
    }
    m_dispatcher->dispatch(message);
    if (rateChange) {
        m_dispatcher->setRateClosing({}, {}, {});
    }
    return true;
}

void StationServer::tellAppliedNow(bool applied)
{
    if (!m_appliedNow) {
        return;
    }
    const DeferredProceed notices = *m_appliedNow;
    m_appliedNow.reset();
    if (applied) {
        tellSettingChanged(notices.affected, notices.sliceWords, notices.change,
                           notices.requester);
    }
}

// ── Asking (7.3) ─────────────────────────────────────────────────────────

QJsonArray StationServer::sharedAffectedJson(const QList<DisturbanceCheck::Affected>& affected) const
{
    QJsonArray out;
    for (const DisturbanceCheck::Affected& a : affected) {
        const ReceiverPlanner::DeviceInfo info = planDevice(a.device);
        QJsonArray slices;
        for (const DisturbanceCheck::AffectedSlice& s : a.slices) {
            const SliceModel* slice = m_radioModel->sliceById(s.sliceId);
            if (slice == nullptr) {
                continue;
            }
            slices.append(QJsonObject{
                {QStringLiteral("sliceId"), s.sliceId},
                {QStringLiteral("letter"), ReceiverPlanner::letterOf(s.sliceId)},
                {QStringLiteral("frequencyHz"), slice->frequency()},
                {QStringLiteral("band"), static_cast<int>(slice->band())},
                {QStringLiteral("mode"), static_cast<int>(slice->dspMode())},
                {QStringLiteral("adc"), m_radioModel->adcForStream(slice->streamIndex())},
                {QStringLiteral("streamIndex"), slice->streamIndex()},
                {QStringLiteral("effect"), DisturbanceCheck::effectName(s.effect)},
            });
        }
        out.append(QJsonObject{
            {QStringLiteral("deviceId"), info.wireId},
            {QStringLiteral("deviceName"), info.name},
            {QStringLiteral("deviceShortName"), info.shortName},
            // "transmitting" joins with Task 34 (the holder, keyed).
            {QStringLiteral("state"), info.state},
            {QStringLiteral("holdsTransmit"), a.holdsTransmit},
            {QStringLiteral("slices"), slices},
        });
    }
    return out;
}

QSet<QString> StationServer::sharedShown(const QList<DisturbanceCheck::Affected>& affected)
{
    QSet<QString> shown;
    for (const DisturbanceCheck::Affected& a : affected) {
        const QString device = QString::fromLatin1(a.device.toHex());
        if (a.holdsTransmit) {
            shown.insert(device + QStringLiteral("|transmit"));
        }
        for (const DisturbanceCheck::AffectedSlice& s : a.slices) {
            shown.insert(QStringLiteral("%1|%2|%3")
                             .arg(device)
                             .arg(s.sliceId)
                             .arg(DisturbanceCheck::effectName(s.effect)));
        }
        if (a.slices.isEmpty() && !a.holdsTransmit) {
            shown.insert(device);
        }
    }
    return shown;
}

QString StationServer::sharedTargetChangedReason()
{
    return QString::fromLatin1(kTargetChangedReason);
}

bool StationServer::handleSharedSetting(SessionTransport* transport, const SessionMessage& message)
{
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local) {
        return false;
    }
    const QByteArray requester = peerFor(transport).sessionDeviceId;
    if (requester.isEmpty()) {
        return false;
    }
    // A verb naming another device's slice is refused by the dispatcher's
    // own check (ruling 5.9), before anything is asked.
    if (message.kind == SessionMessageKind::CommandInvoke) {
        const int sliceId = intArgument(message.arguments, "sliceId");
        if (sliceId >= 0 && !changeRefusal(requester, sliceId).isEmpty()) {
            return false;
        }
    }
    const SharedChange change = classifyShared(message, requester);
    // Ruling 7.6: a new write from the requester to the same target
    // cancels its open question.
    if (!change.target.isEmpty()) {
        const ConfirmStep::Question* open = m_confirm->openQuestion(requester);
        if (open != nullptr && open->kind == QLatin1String("sharedSetting")
            && open->target == change.target) {
            m_confirm->dropQuestion(requester);
        }
    }
    if (!change.shared || change.refusedToday) {
        return false;
    }
    const DisturbanceCheck::Topology topology = sharedTopology();
    const QList<DisturbanceCheck::Affected> affected =
        DisturbanceCheck::check(change.scope, topology, requester);
    // Fix wave 2: asked before the empty-set shortcut, since the station
    // device (never asked) still holds the changes ruling 7.4 names while
    // it is keyed.
    const bool onAirWaits =
        DisturbanceCheck::refusedOnAir(change.scope, topology, requester, affected);
    if (affected.isEmpty() && !onAirWaits) {
        // Nobody else is disturbed: it applies at once, as today.
        return false;
    }
    // Ruling 7.1a (JJ, 2026-09-28): only a connected device is asked about,
    // and only for a change on an Ask row; anything else applies at once
    // and every disturbed device is told (an away device on its return).
    const QList<DisturbanceCheck::Affected> connected = connectedAffected(affected);
    if (!onAirWaits && (sharedTierOf(change) == SharedTier::Notify || connected.isEmpty())) {
        return applySharedNow(transport, message, change, affected);
    }
    QString refusal;
    // Task 34: a command refused on the air carries the refusal's code and
    // fix in its values, as every transmit refusal does (the link, 18.3).
    QList<MirrorUpdate> refusalValues;
    if (onAirWaits) {
        // Ruling 7.4 (D60). Task 34: "The radio is on the air." when the
        // radio's own PTT holds transmit (onAirRefusal's words).
        const TxRefusal onAir = onAirRefusal(requester);
        refusal = onAir.text;
        if (refusal.isEmpty()) {
            refusal = onAirReason(planDevice(topology.transmit.holder).shortName);
        } else {
            refusalValues = {
                {0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(onAir.code)},
                {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(onAir.fix)}};
        }
    } else if (!peerHasSessionHolderVersion(transport)) {
        // An older window gets the refusal only (section 7.3, D59).
        QStringList names;
        for (const DisturbanceCheck::Affected& a : connected) {
            const QString name = planDevice(a.device).name;
            if (!name.isEmpty() && !names.contains(name)) {
                names.append(name);
            }
        }
        refusal = olderWindowReason(ReceiverPlanner::joinWords(names));
    }
    if (!refusal.isEmpty()) {
        if (message.kind == SessionMessageKind::CommandInvoke) {
            answerHere(transport, SessionMessages::commandResult(message.commandVerb,
                                                                 message.commandId, false,
                                                                 refusal, {}, refusalValues));
        } else if (message.kind == SessionMessageKind::PropertyWrite) {
            answerWrite(transport, message, refusal);
        } else {
            const QString key = QString::fromUtf8(message.objectKey);
            const QVariant kept = m_settings.value(key);
            send(transport, SessionMessages::settingsReject(key, kept.isValid(), kept.toString(),
                                                            refusal));
        }
        return true;
    }
    askSharedSetting(transport, message, change, connected, true);
    return true;
}

void StationServer::askSharedSetting(SessionTransport* transport, const SessionMessage& original,
                                     const SharedChange& change,
                                     const QList<DisturbanceCheck::Affected>& affected,
                                     bool answerOriginal)
{
    if (answerOriginal) {
        refuseWhileAsking(transport, original);
    }
    ConfirmStep::Question question;
    question.kind = QStringLiteral("sharedSetting");
    // A settings removal is held as a settings write (of the default);
    // its original's kind says which to apply.
    question.held = original.kind == SessionMessageKind::PropertyWrite
                        ? ConfirmStep::Held::PropertyWrite
                    : original.kind == SessionMessageKind::SettingsWrite
                            || original.kind == SessionMessageKind::SettingsRemove
                        ? ConfirmStep::Held::SettingsWrite
                        : ConfirmStep::Held::Command;
    question.original = original;
    question.shown = sharedShown(affected);
    question.target = change.target;
    question.targetValue = change.targetValue;
    question.change = change.change;
    SessionPrompt prompt;
    prompt.affected = sharedAffectedJson(affected);
    prompt.change = change.change;
    sendQuestion(transport, question, prompt);
}

// ── Proceed ──────────────────────────────────────────────────────────────

SessionMessage StationServer::proceedSharedSetting(SessionTransport* transport,
                                                   const ConfirmStep::Question& question,
                                                   const SessionMessage& invoke)
{
    const QByteArray requester = question.device;
    const QString antennaRefusal = radioAntennaRowRefusal(transport, question.original);
    if (!antennaRefusal.isEmpty()) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId,
                                              false, antennaRefusal, {});
    }
    // The radio may have keyed after the question was shown. Recheck
    // before applying a region change or removal and before any side effect.
    // Parity ruling C4: and before a radio-wide sample rate change.
    if (question.held == ConfirmStep::Held::Command
        && question.original.commandVerb == "setRadioSampleRate" && !m_radioModel.isNull()) {
        QString onAir;
        if (m_radioModel->stationOnAirRefusal(&onAir)) {
            return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId,
                                                  false, onAir, {});
        }
    }
    // Setup description version 22: and before a DSP > Options RX buffer
    // size write or removal, which waits while the radio is on the air
    // (DisturbanceCheck::refusedOnAir does not cover receive options).
    const QString heldKey = QString::fromUtf8(question.original.objectKey);
    if (question.held == ConfirmStep::Held::SettingsWrite
        && (heldKey == QLatin1String("BandPlanRegion")
            || RadioModel::isRxDspBufferSizeKey(heldKey))) {
        const QString onAir = transmitSettingOnAirRefusal(heldKey);
        if (!onAir.isEmpty()) {
            return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId,
                                                  false, onAir, {});
        }
    }
    const SharedChange now = classifyShared(question.original, requester);
    // Ruling 7.6: the target moved since the question was asked, whoever
    // moved it (a write that already set the asked-for value is a move).
    if (!now.shared || now.refusedToday || now.targetValue != question.targetValue) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kTargetChangedReason), {});
    }
    const DisturbanceCheck::Topology topology = sharedTopology();
    const QList<DisturbanceCheck::Affected> affected =
        DisturbanceCheck::check(now.scope, topology, requester);
    if (DisturbanceCheck::refusedOnAir(now.scope, topology, requester, affected)) {
        // Task 34: the holder's on-air words ("The radio is on the air."
        // for the radio's own PTT).
        const TxRefusal refused = onAirRefusal(requester);
        QString onAir = refused.text;
        QList<MirrorUpdate> values;
        if (onAir.isEmpty()) {
            onAir = onAirReason(planDevice(topology.transmit.holder).shortName);
        } else {
            values = {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(refused.code)},
                      {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(refused.fix)}};
        }
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false, onAir,
                                              {}, values);
    }
    // Step 5: a device or an effect the operator was not shown is asked
    // again, and nothing is applied. Ruling 7.1a: only connected devices
    // are asked about; an away one is told on its return.
    const QList<DisturbanceCheck::Affected> connected = connectedAffected(affected);
    for (const QString& entry : sharedShown(connected)) {
        if (!question.shown.contains(entry)) {
            askSharedSetting(transport, question.original, now, connected, false);
            return askAgain(invoke);
        }
    }

    // A held accessory switch still belongs to the session that asked.
    // Recheck permission at proceed: receive-only may have been enabled
    // while another device was considering the question.
    const QByteArray& heldVerb = question.original.commandVerb;
    const bool newAccessory = heldVerb == "amp.operate" || heldVerb == "amp.standby"
        || heldVerb == "tuner.tune" || heldVerb == "tuner.operate"
        || heldVerb == "tuner.bypass" || heldVerb == "tuner.antenna"
        || heldVerb == "rfkit.operate" || heldVerb == "rfkit.standby"
        || heldVerb == "rfkit.antenna";
    const bool legacyAccessory = heldVerb == "setPgxlOperate"
        || heldVerb == "setTgxlOperate" || heldVerb == "setTgxlBypass"
        || heldVerb == "setTgxlAntenna" || heldVerb == "setRfKitOperate"
        || heldVerb == "setRfKitAntenna";
    if (newAccessory || legacyAccessory) {
        StationTxGate sessionOnly;
        sessionOnly.setRemoteTransmitAllowed(m_txGate.remoteTransmitAllowed());
        const TxDecision decision = sessionOnly.decide(peerInfoFor(transport));
        if (!decision.permitted) {
            const QList<MirrorUpdate> values = legacyAccessory ? QList<MirrorUpdate>{}
                : QList<MirrorUpdate>{{0, "refusalCode", MirrorWireKind::Utf8,
                                       QString::fromUtf8(decision.refusal.code)},
                                      {0, "refusalFix", MirrorWireKind::Utf8,
                                       QString::fromUtf8(decision.refusal.fix)}};
            return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                                  decision.refusal.text, {}, values);
        }
    }

    // What each disturbed slice was, for the notices (a closing slice is
    // gone once applied).
    const QHash<int, QJsonObject> sliceWords = sharedSliceWords(affected);
    // Parity ruling C4: the radio-wide rate answers later the same way.
    const bool rateChange = question.held == ConfirmStep::Held::Command
        && (question.original.commandVerb == "requestSliceSampleRate"
            || question.original.commandVerb == "setRadioSampleRate");
    // The other devices' slices the plan cannot place close. For a sample
    // rate (the only change with closes), fix wave: the rate change closes
    // them itself, and only once it is certain, so a refused change closes
    // nothing (RadioModel::setStreamSampleRateClosing).
    QList<QByteArray> closedDevices;
    if (!rateChange) {
        for (int id : now.closes) {
            const QByteArray who = m_radioModel->sliceOwnership()->mark(id).subject();
            if (closeSliceFor(id, saveForAbsentSubject(id), nullptr)
                && !closedDevices.contains(who)) {
                closedDevices.append(who);
            }
        }
    }

    if (rateChange) {
        // The dispatcher runs a rate change on a later turn (it can take
        // the radio's data flow down for tens of milliseconds): the
        // proceed is answered, with the change's own result as its
        // readback, when that result arrives (finishDeferredProceed).
        DeferredProceed later;
        later.transport = transport;
        later.proceedVerb = invoke.commandVerb;
        later.proceedId = invoke.commandId;
        later.affected = affected;
        later.sliceWords = sliceWords;
        later.change = question.change;
        later.requester = requester;
        later.closedDevices = closedDevices;
        // Fix wave I1: keyed by the asking session, so another device's
        // rate change with the same command id never finishes this one.
        const quint64 session = peerFor(transport).sessionId;
        m_deferredProceeds.insert(
            ResultKey{session, question.original.commandVerb, question.original.commandId},
            later);
        m_proceedAnsweredLater = ResultKey{session, invoke.commandVerb, invoke.commandId};
        const ResultKey deferred{session, question.original.commandVerb,
                                 question.original.commandId};
        const QPointer<StationServer> self(this);
        // Fix wave 2 (Important 4): each closing slice with its owner now,
        // so a slice closed and its id reused before the change runs is
        // never closed in its place. Fix wave 3 (Important 1): and the
        // slice itself, so a reuse by the same owner (or by nobody) is
        // caught too.
        QHash<int, SessionCommandDispatcher::ClosingSlice> closingOwners;
        for (int id : now.closes) {
            closingOwners.insert(
                id, SessionCommandDispatcher::ClosingSlice{
                        QPointer<SliceModel>(m_radioModel->sliceById(id)),
                        m_radioModel->sliceOwnership()->mark(id).subject()});
        }
        m_dispatcher->setRateClosing(
            closingOwners, [self, deferred](int id) {
                if (self.isNull() || self->m_radioModel.isNull()) {
                    return;
                }
                const QByteArray who = self->m_radioModel->sliceOwnership()->mark(id).subject();
                if (!self->closeSliceFor(id, self->saveForAbsentSubject(id), nullptr)) {
                    return;
                }
                const auto entry = self->m_deferredProceeds.find(deferred);
                if (entry != self->m_deferredProceeds.end()
                    && !entry->closedDevices.contains(who)) {
                    entry->closedDevices.append(who);
                }
            },
            QString::fromLatin1(kTargetChangedReason));
        m_dispatcher->dispatch(question.original);
        m_dispatcher->setRateClosing({}, {}, {});
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, true,
                                              QString(), {});
    }

    SessionMessage result;
    if (question.held == ConfirmStep::Held::SettingsWrite
        && question.original.kind == SessionMessageKind::SettingsRemove) {
        // Fix wave I3: a removal's readback is the key alone (it is gone).
        applySettingsRemove(question.original);
        const QString key = QString::fromUtf8(question.original.objectKey);
        result = SessionMessages::commandResult(
            invoke.commandVerb, invoke.commandId, true, QString(), {},
            {MirrorUpdate{0, QByteArrayLiteral("settingsKey"), MirrorWireKind::Utf8, key}});
    } else if (question.held == ConfirmStep::Held::SettingsWrite) {
        QString refusal;
        const bool applied = applySettingsWrite(transport, question.original, &refusal);
        const QString key = QString::fromUtf8(question.original.objectKey);
        // Ruling 7.4a: settingsKey and the stored value.
        result = SessionMessages::commandResult(
            invoke.commandVerb, invoke.commandId, applied, applied ? QString() : refusal, {},
            {MirrorUpdate{0, QByteArrayLiteral("settingsKey"), MirrorWireKind::Utf8, key},
             MirrorUpdate{1, QByteArrayLiteral("value"), MirrorWireKind::Utf8,
                          m_settings.value(key).toString()}});
    } else {
        result = applyHeld(transport, question, -1, invoke);
    }
    // Follow-up N3: a radio change answers, and tells the others, on the
    // Core's restart turn (finishRadioChange), as a rate change's proceed
    // does when its result arrives; one dropped there tells nobody.
    if (result.accepted && m_holdingRadioChange && !m_heldRadioChange
        && question.target == QLatin1String("radio")) {
        HeldRadioChange held;
        held.key = ResultKey{peerFor(transport).sessionId, invoke.commandVerb,
                             invoke.commandId};
        held.result = result;
        held.proceed = true;
        held.later.transport = transport;
        held.later.proceedVerb = invoke.commandVerb;
        held.later.proceedId = invoke.commandId;
        held.later.affected = affected;
        held.later.sliceWords = sliceWords;
        held.later.change = question.change;
        held.later.requester = requester;
        held.later.closedDevices = closedDevices;
        m_heldRadioChange = held;
        m_proceedAnsweredLater = held.key;
        return result;
    }
    if (result.accepted) {
        tellSettingChanged(affected, sliceWords, question.change, requester);
    }
    endOlderWindowsWithoutSlices(closedDevices, requester);
    return result;
}

bool StationServer::finishDeferredProceed(const ResultKey& key, const SessionMessage& result)
{
    const auto it = m_deferredProceeds.find(key);
    if (it == m_deferredProceeds.end()) {
        return false;
    }
    const DeferredProceed later = *it;
    m_deferredProceeds.erase(it);
    if (!later.answersProceed) {
        // Ruling 7.1a: a change applied at once. Its own result goes to its
        // requester as today (false: not consumed here); its devices are
        // told once it was taken. A radio change answers on the Core's
        // restart turn, and tells then (finishRadioChange).
        if (result.accepted && m_holdingRadioChange && !m_heldRadioChange
            && result.commandVerb == "station.selectRadio") {
            m_appliedNowRadio = later;
        } else if (result.accepted) {
            tellSettingChanged(later.affected, later.sliceWords, later.change, later.requester);
        }
        endOlderWindowsWithoutSlices(later.closedDevices, later.requester);
        return false;
    }
    // The proceed's own route (recorded because its answer came later) is
    // used here, and goes.
    m_resultRoutes.remove(ResultKey{key.sessionId, later.proceedVerb, later.proceedId});
    if (!later.transport.isNull() && hasPeer(later.transport.data())) {
        sendToPeer(later.transport.data(),
                   SessionMessages::commandResult(later.proceedVerb, later.proceedId,
                                                  result.accepted, result.reason,
                                                  result.affectedKeys, result.updates));
    }
    if (result.accepted) {
        tellSettingChanged(later.affected, later.sliceWords, later.change, later.requester);
    }
    endOlderWindowsWithoutSlices(later.closedDevices, later.requester);
    return true;
}

void StationServer::tellSettingChanged(const QList<DisturbanceCheck::Affected>& affected,
                                       const QHash<int, QJsonObject>& sliceWords,
                                       const QJsonObject& change, const QByteArray& by)
{
    const QString byName = planDevice(by).name;
    for (const DisturbanceCheck::Affected& a : affected) {
        QJsonArray slices;
        QStringList moved;
        QStringList closed;
        QStringList paused;
        for (const DisturbanceCheck::AffectedSlice& s : a.slices) {
            if (sliceWords.contains(s.sliceId)) {
                slices.append(sliceWords.value(s.sliceId));
            }
            const QString letter = ReceiverPlanner::letterOf(s.sliceId);
            if (s.effect == DisturbanceCheck::Effect::Moves) {
                moved.append(letter);
            } else if (s.effect == DisturbanceCheck::Effect::Closes) {
                closed.append(letter);
            } else if (s.effect == DisturbanceCheck::Effect::PausesWhileTransmitting) {
                paused.append(letter);
            }
        }
        QString reason = settingChangedReason(byName,
                                              change.value(QStringLiteral("label")).toString(),
                                              change.value(QStringLiteral("from")).toString(),
                                              change.value(QStringLiteral("to")).toString());
        if (!moved.isEmpty()) {
            reason += QLatin1Char(' ') + movedSentence(moved);
        }
        if (!closed.isEmpty()) {
            reason += QLatin1Char(' ') + closedSentence(closed);
        }
        if (!paused.isEmpty()) {
            reason += QLatin1Char(' ') + pausedSentence(paused);
        }
        ConfirmStep::Notice notice;
        notice.device = a.device;
        notice.reason = reason;
        notice.prompt.kind = QStringLiteral("settingChanged");
        notice.prompt.change = change;
        notice.prompt.slices = slices;
        tellDevice(notice, by);
    }
}

// ── Ruling 5.11a: the receive antenna kept ──────────────────────────────

void StationServer::onReceiveAntennaKept(int sliceId, const QString& antenna,
                                         const QList<QByteArray>& listeners)
{
    if (m_radioModel.isNull()) {
        return;
    }
    const QByteArray device = m_radioModel->sliceOwnership()->mark(sliceId).subject();
    if (device.isEmpty()) {
        return;  // nobody to tell
    }
    QStringList names;
    for (const QByteArray& listener : listeners) {
        const ReceiverPlanner::DeviceInfo info = planDevice(listener);
        const QString name = info.shortName.isEmpty() ? info.name : info.shortName;
        if (!name.isEmpty() && !names.contains(name)) {
            names.append(name);
        }
    }
    if (names.isEmpty()) {
        return;
    }
    qCInfo(lcShared) << "Receive antenna kept on" << antenna << "for slice" << sliceId;
    ConfirmStep::Notice notice;
    notice.device = device;
    notice.reason = antennaKeptReason(antenna, names);
    notice.prompt.kind = QStringLiteral("antennaKept");
    // Sent from inside the retune, before the slice's band follows its new
    // frequency: the band is the frequency's.
    QJsonArray slices = receiverPlanner().noticeSlicesJson({sliceId});
    if (!slices.isEmpty()) {
        QJsonObject slice = slices.first().toObject();
        slice.insert(QStringLiteral("band"),
                     static_cast<int>(bandFromFrequency(
                         slice.value(QStringLiteral("frequencyHz")).toDouble())));
        slices.replace(0, slice);
    }
    notice.prompt.slices = slices;
    // About the device's own tuning: no `by` keys (7.4).
    tellDevice(notice, QByteArray());
}

} // namespace NereusSDR
