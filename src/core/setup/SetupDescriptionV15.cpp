// no-port-check: NereusSDR-original Setup description version 15 rules.
// =================================================================
// src/core/setup/SetupDescriptionV15.cpp  (NereusSDR)
// =================================================================
//
// See SetupDescriptionV15.h. The contract is the "Version 15" section of
// docs/architecture/2026-09-23-setup-description-v1.md.
//
// Modification history (NereusSDR):
//   2026-09-29 - Created. J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
//   2026-09-30 - Radio codec lane: availableOn "orionMicPanel" (the Red
//                Pitaya's Orion mic rows, disabled with a reason); the
//                Hermes rows on the Hermes Lite 2 with its audio add-on
//                note. J.J. Boyd (KG4VCF), with AI-assisted implementation
//                via Anthropic Claude Code.
// =================================================================

#include "core/setup/SetupDescriptionV15.h"

#include "core/setup/SetupDescriptionService.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/TransmitStateFacade.h"
#include "core/settings/SettingsScope.h"
#include "core/StepAttenuatorFacade.h"
#include "core/dsp/DspAssetService.h"
#include "models/AccessoryDataModel.h"
#include "models/AccessorySettingsModel.h"
#include "models/AmplifierModel.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "models/TunerModel.h"

#include <QJsonArray>
#include <QSet>

#include <cmath>

namespace NereusSDR::SetupDescriptionV15 {

namespace {

bool fail(QString* why, const QString& text)
{
    if (why) {
        *why = text;
    }
    return false;
}

struct ObjectInfo {
    const QMetaObject* meta = nullptr;
    QByteArray policyClass;
};

// The mirrored objects a version 15 row may name, by their object key.
ObjectInfo objectInfo(const QString& object)
{
    if (object == QLatin1String("slice:active")) {
        return {&SliceModel::staticMetaObject, QByteArrayLiteral("SliceModel")};
    }
    if (object == QLatin1String("transmit")) {
        return {&TransmitModel::staticMetaObject, QByteArrayLiteral("TransmitModel")};
    }
    if (object == QLatin1String("stepAtt")) {
        return {&StepAttenuatorFacade::staticMetaObject, QByteArrayLiteral("StepAttenuatorFacade")};
    }
    if (object == QLatin1String("dspAssets")) {
        return {&DspAssetService::staticMetaObject, QByteArrayLiteral("DspAssetService")};
    }
    if (object == QLatin1String("radio")) {
        return {&RadioModel::staticMetaObject, QByteArrayLiteral("RadioModel")};
    }
    if (object == QLatin1String("txState")) {
        return {&TransmitState::staticMetaObject, QByteArrayLiteral("TransmitState")};
    }
    if (object == QLatin1String("amplifier")) {
        return {&AmplifierModel::staticMetaObject, QByteArrayLiteral("AmplifierModel")};
    }
    if (object == QLatin1String("tuner")) {
        return {&TunerModel::staticMetaObject, QByteArrayLiteral("TunerModel")};
    }
    if (object == QLatin1String("rfkit")) {
        return {&RfKitModel::staticMetaObject, QByteArrayLiteral("RfKitModel")};
    }
    if (object == QLatin1String("accessoryData")) {
        return {&AccessoryDataModel::staticMetaObject, QByteArrayLiteral("AccessoryDataModel")};
    }
    if (object == QLatin1String("accessorySettings")) {
        return {&AccessorySettingsModel::staticMetaObject,
                QByteArrayLiteral("AccessorySettingsModel")};
    }
    return {};
}

const MirrorProperty* propertyOf(const QJsonValue& raw, ObjectInfo* info = nullptr)
{
    if (!raw.isObject()) {
        return nullptr;
    }
    const QJsonObject ref = raw.toObject();
    if (ref.size() != 2 || !ref.value(QStringLiteral("object")).isString()
        || !ref.value(QStringLiteral("name")).isString()) {
        return nullptr;
    }
    const ObjectInfo found = objectInfo(ref.value(QStringLiteral("object")).toString());
    if (!found.meta) {
        return nullptr;
    }
    if (info) {
        *info = found;
    }
    return MirrorSchema::forMetaObject(found.meta).byName(
        ref.value(QStringLiteral("name")).toString().toUtf8());
}

// Radio telemetry fields (station.metrics, stationTelemetryVersion) a
// readout may name, with the version that first sends each.
int telemetryVersionFor(const QString& name)
{
    static const QHash<QString, int> kFields{
        {QStringLiteral("paCurrentAmps"), 4}, {QStringLiteral("paTemperatureCelsius"), 4},
        {QStringLiteral("paVolts"), 4}, {QStringLiteral("supplyVolts"), 4},
        {QStringLiteral("hl2RxBytesPerSecond"), 5}, {QStringLiteral("hl2TxBytesPerSecond"), 5},
        {QStringLiteral("hl2Throttled"), 5}, {QStringLiteral("hl2SequenceGaps"), 5},
        {QStringLiteral("connectionAgeMs"), 6}};
    return kFields.value(name, 0);
}

bool isInteger(const QJsonValue& value)
{
    return value.isDouble() && std::floor(value.toDouble()) == value.toDouble();
}

// A choice whose options' values are text sends that text.
bool textOptions(const QJsonObject& control)
{
    const QJsonArray options = control.value(QStringLiteral("options")).toArray();
    return !options.isEmpty()
        && options.first().toObject().value(QStringLiteral("value")).isString();
}

// A control's value kind on the wire, as a verb argument or a property.
MirrorWireKind controlWireKind(const QJsonObject& control)
{
    const QString kind = control.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("toggle")) { return MirrorWireKind::Bool; }
    if (kind == QLatin1String("decimal")) { return MirrorWireKind::Float64; }
    if (kind == QLatin1String("text")) { return MirrorWireKind::Utf8; }
    if (kind == QLatin1String("choice") && (control.contains(QStringLiteral("choicesFrom"))
                                            || textOptions(control))) {
        return MirrorWireKind::Utf8;
    }
    if (kind == QLatin1String("integer") || kind == QLatin1String("slider")
        || kind == QLatin1String("choice")) {
        return MirrorWireKind::Int64;
    }
    return MirrorWireKind::Unsupported;
}

bool propertyKindFits(const QJsonObject& control, MirrorWireKind property)
{
    // A two-way choice between radio buttons that sets a Boolean (Mic In /
    // Line In): its options' values are false and true.
    const QJsonArray options = control.value(QStringLiteral("options")).toArray();
    if (!options.isEmpty() && options.first().toObject().value(QStringLiteral("value")).isBool()) {
        bool allBool = true;
        for (const QJsonValue& option : options) {
            allBool = allBool && option.toObject().value(QStringLiteral("value")).isBool();
        }
        return allBool && options.size() == 2 && property == MirrorWireKind::Bool;
    }
    const MirrorWireKind wanted = controlWireKind(control);
    if (control.value(QStringLiteral("kind")) == QJsonValue(QStringLiteral("choice"))
        && !control.contains(QStringLiteral("choicesFrom")) && !textOptions(control)) {
        return property == MirrorWireKind::Enum || property == MirrorWireKind::Int64;
    }
    return wanted != MirrorWireKind::Unsupported && wanted == property;
}

bool validGate(const QJsonValue& raw, QString* why)
{
    if (raw.isUndefined()) {
        return true;
    }
    if (!raw.isObject()) {
        return fail(why, QStringLiteral("gate must be an object"));
    }
    const QJsonObject gate = raw.toObject();
    static const QSet<QString> kKeys{QStringLiteral("capability"), QStringLiteral("min"),
                                     QStringLiteral("transmit"), QStringLiteral("offAir"),
                                     QStringLiteral("micLine")};
    for (auto it = gate.constBegin(); it != gate.constEnd(); ++it) {
        if (!kKeys.contains(it.key())) {
            return fail(why, QStringLiteral("unknown gate field %1").arg(it.key()));
        }
        if (it.key() != QLatin1String("capability") && it.key() != QLatin1String("min")
            && it.value() != QJsonValue(true)) {
            return fail(why, QStringLiteral("a gate flag is only ever true"));
        }
    }
    if (gate.contains(QStringLiteral("capability")) != gate.contains(QStringLiteral("min"))
        || (gate.contains(QStringLiteral("capability"))
            && (gate.value(QStringLiteral("capability")).toString().isEmpty()
                || !isInteger(gate.value(QStringLiteral("min")))
                || gate.value(QStringLiteral("min")).toInt() < 1))) {
        return fail(why, QStringLiteral("a capability gate needs a name and a minimum"));
    }
    return true;
}

// A `$control` argument source names a staged row of the same section.
bool validStagedSources(const QJsonObject& control, const QJsonArray& section, QString* why)
{
    const QJsonObject args = control.value(QStringLiteral("binding")).toObject()
        .value(QStringLiteral("command")).toObject().value(QStringLiteral("arguments")).toObject();
    const QByteArray verb = control.value(QStringLiteral("binding")).toObject()
        .value(QStringLiteral("command")).toObject().value(QStringLiteral("verb"))
        .toString().toUtf8();
    const CommandVerbSpec* spec = nullptr;
    for (const CommandVerbSpec& candidate : SessionCommandDispatcher::verbSpecs()) {
        if (candidate.verb == verb) {
            spec = &candidate;
            break;
        }
    }
    for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
        const QJsonObject source = it.value().toObject();
        if (!source.contains(QStringLiteral("$control"))) {
            continue;
        }
        const QString target = source.value(QStringLiteral("$control")).toString();
        QJsonObject staged;
        for (const QJsonValue& raw : section) {
            if (raw.toObject().value(QStringLiteral("id")).toString() == target) {
                staged = raw.toObject();
            }
        }
        if (staged.isEmpty()
            || staged.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("staged"))) {
            return fail(why, QStringLiteral("%1 names no staged row of its section").arg(target));
        }
        MirrorWireKind expected = MirrorWireKind::Unsupported;
        if (spec) {
            for (const CommandArgumentSpec& argument : spec->arguments) {
                if (argument.name == it.key().toUtf8()) {
                    expected = argument.kind;
                }
            }
        }
        if (expected == MirrorWireKind::Unsupported || controlWireKind(staged) != expected) {
            return fail(why, QStringLiteral("%1 does not fit argument %2").arg(target, it.key()));
        }
    }
    return true;
}

// DSP > Filter Presets (FilterPresetsSetupPage): the table of the chosen
// mode's presets from the Core's catalogue, and the page's three resets.
// Closed: each row must be exactly this.
bool validFilterPresetsRow(const QJsonObject& control, QString* why)
{
    const QString id = control.value(QStringLiteral("id")).toString();
    const auto base = [&control](const QString& kind, const QString& label) {
        QJsonObject expected{{QStringLiteral("id"), control.value(QStringLiteral("id"))},
                             {QStringLiteral("label"), label},
                             {QStringLiteral("tooltip"), QString()},
                             {QStringLiteral("kind"), kind},
                             {QStringLiteral("applies"), QStringLiteral("live")},
                             {QStringLiteral("requiresDescriptionVersion"), kVersion}};
        return expected;
    };
    QJsonObject expected;
    if (id == QLatin1String("dsp.filterPresets.presets")) {
        expected = base(QStringLiteral("table"), QStringLiteral("Presets"));
        expected.insert(QStringLiteral("binding"), QJsonObject{{QStringLiteral("filterPresets"),
            QJsonObject{{QStringLiteral("modeFrom"), QStringLiteral("dsp.filterPresets.mode")}}}});
        const auto hz = [](const QString& column, const QString& label) {
            return QJsonObject{{QStringLiteral("id"), column}, {QStringLiteral("label"), label},
                               {QStringLiteral("kind"), QStringLiteral("integer")},
                               {QStringLiteral("min"), -10000}, {QStringLiteral("max"), 10000},
                               {QStringLiteral("step"), 1}, {QStringLiteral("unit"), QStringLiteral("Hz")}};
        };
        expected.insert(QStringLiteral("columns"), QJsonArray{
            QJsonObject{{QStringLiteral("id"), QStringLiteral("slot")},
                        {QStringLiteral("label"), QStringLiteral("#")},
                        {QStringLiteral("kind"), QStringLiteral("readout")}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("name")},
                        {QStringLiteral("label"), QStringLiteral("Name")},
                        {QStringLiteral("kind"), QStringLiteral("text")},
                        {QStringLiteral("maxLength"), 32}},
            hz(QStringLiteral("lowHz"), QStringLiteral("Low (Hz)")),
            hz(QStringLiteral("highHz"), QStringLiteral("High (Hz)")),
            QJsonObject{{QStringLiteral("id"), QStringLiteral("width")},
                        {QStringLiteral("label"), QStringLiteral("Width (Hz)")},
                        {QStringLiteral("kind"), QStringLiteral("readout")}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("reorder")},
                        {QStringLiteral("label"), QStringLiteral("Reorder")},
                        {QStringLiteral("kind"), QStringLiteral("reorder")}}});
    } else if (id == QLatin1String("dsp.filterPresets.resetRow")
               || id == QLatin1String("dsp.filterPresets.resetMode")
               || id == QLatin1String("dsp.filterPresets.resetAll")) {
        const QString action = id.section(QLatin1Char('.'), 2);
        expected = base(QStringLiteral("button"),
                        action == QLatin1String("resetRow")
                            ? QStringLiteral("Reset Selected Row")
                            : action == QLatin1String("resetMode")
                              ? QStringLiteral("Reset All Rows for This Mode")
                              : QStringLiteral("Reset Every Mode to Defaults"));
        expected.insert(QStringLiteral("binding"), QJsonObject{{QStringLiteral("filterPresets"),
            QJsonObject{{QStringLiteral("action"), action}}}});
        if (action == QLatin1String("resetMode")) {
            expected.insert(QStringLiteral("confirm"),
                            QStringLiteral("Reset all presets for %1 to the defaults?"));
        } else if (action == QLatin1String("resetAll")) {
            expected.insert(QStringLiteral("confirm"), QStringLiteral(
                "Reset ALL filter presets for ALL modes to the defaults?\n\n"
                "This cannot be undone."));
        }
    } else {
        return fail(why, QStringLiteral("%1: filter presets row").arg(id));
    }
    if (control != expected) {
        return fail(why, QStringLiteral("%1: must be the exact Filter Presets row").arg(id));
    }
    return true;
}

} // namespace

bool isCategory(const QString& categoryId)
{
    return categoryId == QLatin1String("dsp") || categoryId == QLatin1String("transmit")
        || categoryId == QLatin1String("audio") || categoryId == QLatin1String("diagnostics")
        || categoryId == QLatin1String("catNetwork");
}

bool validateControl(const QString& categoryId, const QJsonObject& control, QString* why)
{
    static const QSet<QString> kFields{
        QStringLiteral("id"), QStringLiteral("label"), QStringLiteral("tooltip"),
        QStringLiteral("kind"), QStringLiteral("binding"), QStringLiteral("applies"),
        QStringLiteral("requiresDescriptionVersion"), QStringLiteral("gate"),
        QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("step"),
        QStringLiteral("unit"), QStringLiteral("decimals"), QStringLiteral("choices"),
        QStringLiteral("options"), QStringLiteral("default"), QStringLiteral("valueEncoding"),
        QStringLiteral("confirm"), QStringLiteral("prompt"), QStringLiteral("choicesFrom"),
        QStringLiteral("enabledWhen"), QStringLiteral("valueOffset"),
        QStringLiteral("rangeFrom"), QStringLiteral("rangeSource"),
        QStringLiteral("availableOn"), QStringLiteral("unsavedChanges"),
        QStringLiteral("maxLength"), QStringLiteral("format"), QStringLiteral("columns"),
        QStringLiteral("target"), QStringLiteral("telemetryFor")};
    const QString id = control.value(QStringLiteral("id")).toString();
    for (auto it = control.constBegin(); it != control.constEnd(); ++it) {
        if (!kFields.contains(it.key())) {
            return fail(why, QStringLiteral("%1: unknown field %2").arg(id, it.key()));
        }
    }
    if (!id.startsWith(categoryId + QLatin1Char('.'))
        || control.value(QStringLiteral("label")).toString().isEmpty()
        || !control.value(QStringLiteral("tooltip")).isString()
        || control.value(QStringLiteral("requiresDescriptionVersion")) != QJsonValue(kVersion)) {
        return fail(why, QStringLiteral("%1: id, label, tooltip or version").arg(id));
    }
    const QString kind = control.value(QStringLiteral("kind")).toString();
    static const QSet<QString> kKinds{QStringLiteral("toggle"), QStringLiteral("integer"),
                                      QStringLiteral("decimal"), QStringLiteral("slider"),
                                      QStringLiteral("choice"), QStringLiteral("text"),
                                      QStringLiteral("button"), QStringLiteral("readout"),
                                      QStringLiteral("table")};
    const QString applies = control.value(QStringLiteral("applies")).toString();
    if (!kKinds.contains(kind)
        || (applies != QLatin1String("live") && applies != QLatin1String("staged"))
        || (applies == QLatin1String("staged")
            && (kind == QLatin1String("button") || kind == QLatin1String("readout")))) {
        return fail(why, QStringLiteral("%1: kind or applies").arg(id));
    }
    if (!validGate(control.value(QStringLiteral("gate")), why)) {
        return false;
    }
    // Numeric ranges: either literal numbers or filled from the Core's radio.
    const bool numeric = kind == QLatin1String("integer") || kind == QLatin1String("decimal")
        || kind == QLatin1String("slider");
    if (numeric && !control.contains(QStringLiteral("rangeSource"))
        && !control.contains(QStringLiteral("rangeFrom"))
        && (!control.value(QStringLiteral("min")).isDouble()
            || !control.value(QStringLiteral("max")).isDouble()
            || !control.value(QStringLiteral("step")).isDouble()
            || control.value(QStringLiteral("min")).toDouble()
                > control.value(QStringLiteral("max")).toDouble()
            || control.value(QStringLiteral("step")).toDouble() <= 0)) {
        return fail(why, QStringLiteral("%1: range").arg(id));
    }
    if (control.contains(QStringLiteral("rangeSource"))
        && control.value(QStringLiteral("rangeSource")) != QJsonValue(QStringLiteral("attOnTx"))) {
        return fail(why, QStringLiteral("%1: rangeSource").arg(id));
    }
    if (control.contains(QStringLiteral("rangeFrom"))) {
        const QJsonObject from = control.value(QStringLiteral("rangeFrom")).toObject();
        const QString name = from.value(QStringLiteral("catalogueTransmit")).toString();
        if (from.size() != 1
            || (name != QLatin1String("tunePower") && name != QLatin1String("micGainDb"))) {
            return fail(why, QStringLiteral("%1: rangeFrom").arg(id));
        }
    }
    if (control.contains(QStringLiteral("telemetryFor"))
        && (control.value(QStringLiteral("telemetryFor")) != QJsonValue(QStringLiteral("paRowVolts"))
            || control.value(QStringLiteral("binding")).toObject().value(QStringLiteral("telemetry"))
                   .toObject().value(QStringLiteral("name")) != QJsonValue(QStringLiteral("paVolts")))) {
        return fail(why, QStringLiteral("%1: telemetryFor").arg(id));
    }
    if (control.contains(QStringLiteral("availableOn"))
        && control.value(QStringLiteral("availableOn")) != QJsonValue(QStringLiteral("hfPaSwitch"))
        && control.value(QStringLiteral("availableOn"))
            != QJsonValue(QStringLiteral("orionMicPanel"))) {
        return fail(why, QStringLiteral("%1: availableOn").arg(id));
    }
    if (control.contains(QStringLiteral("choices"))) {
        const QJsonArray choices = control.value(QStringLiteral("choices")).toArray();
        if (kind != QLatin1String("choice") || choices.isEmpty()) {
            return fail(why, QStringLiteral("%1: choices").arg(id));
        }
        for (const QJsonValue& choice : choices) {
            if (!choice.isString() || choice.toString().isEmpty()) {
                return fail(why, QStringLiteral("%1: choices").arg(id));
            }
        }
    }
    if (control.contains(QStringLiteral("options"))) {
        const QJsonArray options = control.value(QStringLiteral("options")).toArray();
        if (kind != QLatin1String("choice") || options.isEmpty()) {
            return fail(why, QStringLiteral("%1: options").arg(id));
        }
        for (const QJsonValue& raw : options) {
            const QJsonObject option = raw.toObject();
            if (option.size() != 2 || option.value(QStringLiteral("label")).toString().isEmpty()
                || !(isInteger(option.value(QStringLiteral("value")))
                     || option.value(QStringLiteral("value")).isString()
                     || option.value(QStringLiteral("value")).isBool())) {
                return fail(why, QStringLiteral("%1: options").arg(id));
            }
        }
    }
    if (kind == QLatin1String("choice") && !control.contains(QStringLiteral("choices"))
        && !control.contains(QStringLiteral("options"))
        && !control.contains(QStringLiteral("choicesFrom"))) {
        return fail(why, QStringLiteral("%1: a choice needs its choices").arg(id));
    }
    if (control.contains(QStringLiteral("choicesFrom"))) {
        const QJsonObject from = control.value(QStringLiteral("choicesFrom")).toObject();
        const bool profiles = from == QJsonObject{{QStringLiteral("jsonNames"),
            QJsonObject{{QStringLiteral("object"), QStringLiteral("transmit")},
                        {QStringLiteral("name"), QStringLiteral("txProfilesJson")}}}};
        const bool nr3 = from == QJsonObject{{QStringLiteral("dspAssets"), QStringLiteral("nr3")}};
        if (kind != QLatin1String("choice") || (!profiles && !nr3)) {
            return fail(why, QStringLiteral("%1: choicesFrom").arg(id));
        }
    }
    if (control.contains(QStringLiteral("enabledWhen"))) {
        const QJsonObject when = control.value(QStringLiteral("enabledWhen")).toObject();
        if (when.size() != 2 || !propertyOf(when.value(QStringLiteral("property")))
            || when.value(QStringLiteral("oneOf")).toArray().isEmpty()) {
            return fail(why, QStringLiteral("%1: enabledWhen").arg(id));
        }
    }
    if (control.contains(QStringLiteral("format"))) {
        // How a readout shows its value; the description document defines
        // each (the desktop's own words).
        static const QSet<QString> kFormats{
            QStringLiteral("nnrLimit"), QStringLiteral("nnrModelSlot"),
            QStringLiteral("nnrStatus"), QStringLiteral("positiveOrNone"),
            QStringLiteral("enabledOff"), QStringLiteral("phaseRotator"),
            QStringLiteral("cfcBands"), QStringLiteral("cessb"),
            QStringLiteral("radioName"), QStringLiteral("uptime"), QStringLiteral("txMode"),
            QStringLiteral("paTemperature"), QStringLiteral("wattsWhileKeyed"),
            QStringLiteral("swrWhileKeyed"), QStringLiteral("kilobytesPerSecond"),
            QStringLiteral("throttleActive"), QStringLiteral("throttledOk"),
            QStringLiteral("connectionPhase"), QStringLiteral("bandFollow"),
            QStringLiteral("fourO3AListener"), QStringLiteral("sinceMs"),
            QStringLiteral("rfkitBandFollow"), QStringLiteral("rttAverage"),
            QStringLiteral("clockTime"), QStringLiteral("bytes")};
        if (kind != QLatin1String("readout")
            || !kFormats.contains(control.value(QStringLiteral("format")).toString())) {
            return fail(why, QStringLiteral("%1: format").arg(id));
        }
    }
    if (control.contains(QStringLiteral("valueOffset"))
        && !isInteger(control.value(QStringLiteral("valueOffset")))) {
        return fail(why, QStringLiteral("%1: valueOffset").arg(id));
    }
    if (control.contains(QStringLiteral("decimals"))
        && (!isInteger(control.value(QStringLiteral("decimals")))
            || control.value(QStringLiteral("decimals")).toInt() < 0
            || control.value(QStringLiteral("decimals")).toInt() > 6)) {
        return fail(why, QStringLiteral("%1: decimals").arg(id));
    }
    if (control.contains(QStringLiteral("confirm"))
        && (kind != QLatin1String("button") && kind != QLatin1String("choice")
            && kind != QLatin1String("toggle"))) {
        return fail(why, QStringLiteral("%1: confirm").arg(id));
    }
    if (control.contains(QStringLiteral("confirm"))
        && control.value(QStringLiteral("confirm")).toString().isEmpty()) {
        return fail(why, QStringLiteral("%1: confirm").arg(id));
    }
    if (control.contains(QStringLiteral("prompt"))) {
        // A name the operator types before the verb runs (Save TX Profile):
        // its title and label, the text it starts with, and the question
        // asked when the name is already one of `overwrite.namesFrom`.
        const QJsonObject prompt = control.value(QStringLiteral("prompt")).toObject();
        const QJsonObject overwrite = prompt.value(QStringLiteral("overwrite")).toObject();
        const QJsonObject initial = prompt.value(QStringLiteral("initial")).toObject();
        if (kind != QLatin1String("button") || prompt.size() != 4
            || prompt.value(QStringLiteral("title")).toString().isEmpty()
            || prompt.value(QStringLiteral("label")).toString().isEmpty()
            || initial.size() != 1 || !propertyOf(initial.value(QStringLiteral("$property")))
            || overwrite.size() != 3
            || overwrite.value(QStringLiteral("title")).toString().isEmpty()
            || !overwrite.value(QStringLiteral("question")).toString().contains(QStringLiteral("%1"))
            || !propertyOf(overwrite.value(QStringLiteral("namesFrom")))) {
            return fail(why, QStringLiteral("%1: prompt").arg(id));
        }
    }
    if (control.contains(QStringLiteral("unsavedChanges"))) {
        // The desktop's question before a profile switch drops changes:
        // `watch` names the mirrored values whose change since the profile
        // became active makes the question due; `saveVerb` saves first.
        const QJsonObject unsaved = control.value(QStringLiteral("unsavedChanges")).toObject();
        const QJsonArray watch = unsaved.value(QStringLiteral("watch")).toArray();
        if (kind != QLatin1String("choice") || unsaved.size() != 4
            || unsaved.value(QStringLiteral("title")).toString().isEmpty()
            || !unsaved.value(QStringLiteral("question")).toString().contains(QStringLiteral("%1"))
            || unsaved.value(QStringLiteral("saveVerb")) != QJsonValue(QStringLiteral("txProfile.save"))
            || watch.isEmpty()) {
            return fail(why, QStringLiteral("%1: unsavedChanges").arg(id));
        }
        for (const QJsonValue& name : watch) {
            if (!propertyOf(QJsonObject{{QStringLiteral("object"), QStringLiteral("transmit")},
                                        {QStringLiteral("name"), name.toString()}})) {
                return fail(why, QStringLiteral("%1: unsavedChanges watches %2")
                                     .arg(id, name.toString()));
            }
        }
    }

    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    if (binding.size() != 1) {
        return fail(why, QStringLiteral("%1: one binding").arg(id));
    }
    if (control.contains(QStringLiteral("target")) && !binding.contains(QStringLiteral("phone"))) {
        return fail(why, QStringLiteral("%1: target").arg(id));
    }
    if (kind == QLatin1String("table") && !binding.contains(QStringLiteral("filterPresets"))) {
        return fail(why, QStringLiteral("%1: table binding").arg(id));
    }
    if (binding.contains(QStringLiteral("phone"))) {
        // A value the phone draws or chooses itself: NotchVisualEnabled
        // (the phone draws the notch dent; the value is the Core-wide
        // Station key of that name, read and written through the settings
        // proxy) or the page's own choice (the Filter Presets mode, which
        // the phone keeps). On a button it names a phone action: open the
        // TX equalizer, or open the Setup page `target` names.
        const QString phone = binding.value(QStringLiteral("phone")).toString();
        const QString target = control.value(QStringLiteral("target")).toString();
        const bool action = kind == QLatin1String("button");
        if (phone.isEmpty() || control.contains(QStringLiteral("gate"))
            || applies != QLatin1String("live") || kind == QLatin1String("readout")
            || (action && phone != QLatin1String("openTxEq")
                && phone != QLatin1String("openSetupPage"))
            || (action && phone == QLatin1String("openSetupPage")) != !target.isEmpty()
            || (!target.isEmpty() && target.count(QLatin1Char('.')) != 1)) {
            return fail(why, QStringLiteral("%1: phone binding").arg(id));
        }
        return true;
    }
    if (binding.contains(QStringLiteral("filterPresets"))) {
        return validFilterPresetsRow(control, why);
    }
    if (binding.contains(QStringLiteral("property"))) {
        ObjectInfo info;
        const MirrorProperty* property = propertyOf(binding.value(QStringLiteral("property")), &info);
        if (!property) {
            return fail(why, QStringLiteral("%1: the Core does not send this value").arg(id));
        }
        if (kind == QLatin1String("readout")) {
            return true;
        }
        if (!propertyKindFits(control, property->kind)) {
            return fail(why, QStringLiteral("%1: value kind").arg(id));
        }
        if (applies == QLatin1String("staged")) {
            return true;
        }
        if (!property->isWritable
            || !MirrorPolicy::inboundAllowed(info.policyClass, property->name)) {
            return fail(why, QStringLiteral("%1: the Core does not take this value").arg(id));
        }
        return true;
    }
    if (binding.contains(QStringLiteral("setting"))) {
        const QString key = binding.value(QStringLiteral("setting")).toString();
        if (key.isEmpty() || classifySettingsKey(key) != SettingsScope::Station
            || isModelOwnedDspSettingsKey(key) || kind == QLatin1String("readout")
            || kind == QLatin1String("button")
            || !SetupDescription::validateSettingToggleEncoding(control)) {
            return fail(why, QStringLiteral("%1: setting %2").arg(id, key));
        }
        return true;
    }
    if (binding.contains(QStringLiteral("telemetry"))) {
        const QJsonObject ref = binding.value(QStringLiteral("telemetry")).toObject();
        const int since = telemetryVersionFor(ref.value(QStringLiteral("name")).toString());
        const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
        if (ref.size() != 2 || ref.value(QStringLiteral("object")) != QJsonValue(QStringLiteral("radio"))
            || since == 0 || kind != QLatin1String("readout")
            || gate.value(QStringLiteral("capability"))
                != QJsonValue(QStringLiteral("stationTelemetryVersion"))
            || gate.value(QStringLiteral("min")).toInt() < since) {
            return fail(why, QStringLiteral("%1: telemetry").arg(id));
        }
        return true;
    }
    if (binding.contains(QStringLiteral("command"))) {
        if (kind == QLatin1String("readout") || applies == QLatin1String("staged")) {
            return fail(why, QStringLiteral("%1: command kind").arg(id));
        }
        QString error;
        if (!SetupDescription::validateCommandBinding(control, &error)) {
            return fail(why, QStringLiteral("%1: %2").arg(id, error));
        }
        return true;
    }
    return fail(why, QStringLiteral("%1: binding").arg(id));
}

bool validateSection(const QString& categoryId, const QJsonArray& controls, QString* why)
{
    for (const QJsonValue& raw : controls) {
        const QJsonObject control = raw.toObject();
        if (control.value(QStringLiteral("requiresDescriptionVersion")) != QJsonValue(kVersion)) {
            continue;
        }
        if (!validateControl(categoryId, control, why)
            || (control.value(QStringLiteral("binding")).toObject()
                    .contains(QStringLiteral("command"))
                && !validStagedSources(control, controls, why))) {
            return false;
        }
    }
    return true;
}

bool projectForRadio(QJsonObject* control, const BoardCapabilities& caps, HPSDRModel model)
{
    if (control->value(QStringLiteral("rangeSource")) == QJsonValue(QStringLiteral("attOnTx"))) {
        // PowerPage: the step attenuator's minimum to kMaxAttOnTxDb, and the
        // tooltip names that range.
        control->insert(QStringLiteral("min"), caps.attenuator.minDb);
        control->insert(QStringLiteral("max"), StepAttenuatorFacade::kMaxAttOnTxDb);
        control->insert(QStringLiteral("step"), 1);
        control->insert(QStringLiteral("tooltip"),
                        control->value(QStringLiteral("tooltip")).toString()
                            .arg(caps.attenuator.minDb)
                            .arg(StepAttenuatorFacade::kMaxAttOnTxDb));
        control->remove(QStringLiteral("rangeSource"));
    }
    if (control->value(QStringLiteral("telemetryFor")) == QJsonValue(QStringLiteral("paRowVolts"))) {
        // RadioModel::paRowVolts: the supply volts on the ANAN-G2E, the PA
        // volts elsewhere.
        control->remove(QStringLiteral("telemetryFor"));
        if (model == HPSDRModel::ANAN_G2E) {
            control->insert(QStringLiteral("binding"), QJsonObject{{QStringLiteral("telemetry"),
                QJsonObject{{QStringLiteral("object"), QStringLiteral("radio")},
                            {QStringLiteral("name"), QStringLiteral("supplyVolts")}}}});
        }
    }
    if (control->value(QStringLiteral("availableOn")) == QJsonValue(QStringLiteral("hfPaSwitch"))) {
        control->remove(QStringLiteral("availableOn"));
        if (!RadioModel::hfPaSwitchAvailable(model)) {
            control->insert(QStringLiteral("availability"), QJsonObject{
                {QStringLiteral("enabled"), false},
                {QStringLiteral("reason"), RadioModel::hfPaSwitchUnavailableReason()}});
        }
    }
    if (control->value(QStringLiteral("availableOn")) == QJsonValue(QStringLiteral("orionMicPanel"))) {
        // AudioTxInputPage's Orion group: Thetis greys out the ORION mic
        // panel on the Red Pitaya (RadioModel::orionMicPanelAvailable).
        control->remove(QStringLiteral("availableOn"));
        if (!RadioModel::orionMicPanelAvailable(model)) {
            control->insert(QStringLiteral("availability"), QJsonObject{
                {QStringLiteral("enabled"), false},
                {QStringLiteral("reason"), RadioModel::orionMicPanelUnavailableReason()}});
        }
    }
    return true;
}

bool keepSectionForRadio(QJsonObject* section, const BoardCapabilities& caps)
{
    if (!section->contains(QStringLiteral("boardFamily"))) {
        return true;
    }
    const QString family = section->value(QStringLiteral("boardFamily")).toString();
    section->remove(QStringLiteral("boardFamily"));
    // AudioTxInputPage::updateRadioMicGroupVisibility's board families.
    const HPSDRHW hw = caps.board;
    if (family == QLatin1String("hermes")) {
        // The Hermes Lite 2 with its audio add-on board takes the same rows
        // (P1CodecHl2); the gateware cannot report the board, so each row
        // carries the note the desktop shows beside Radio Mic.
        if (hw == HPSDRHW::HermesLite && caps.radioMicNeedsAddOn) {
            section->insert(QStringLiteral("title"), QStringLiteral("Radio Mic (Hermes Lite 2)"));
            QJsonArray controls = section->value(QStringLiteral("controls")).toArray();
            for (int c = 0; c < controls.size(); ++c) {
                QJsonObject control = controls.at(c).toObject();
                control.insert(QStringLiteral("tooltip"), RadioModel::radioMicAddOnNote());
                controls[c] = control;
            }
            section->insert(QStringLiteral("controls"), controls);
            return true;
        }
        return hw == HPSDRHW::Hermes || hw == HPSDRHW::HermesII || hw == HPSDRHW::Angelia
            || hw == HPSDRHW::Atlas;
    }
    if (family == QLatin1String("orion")) {
        return hw == HPSDRHW::Orion || hw == HPSDRHW::OrionMKII;
    }
    if (family == QLatin1String("saturn")) {
        return hw == HPSDRHW::Saturn || hw == HPSDRHW::SaturnMKII;
    }
    return false;
}

} // namespace NereusSDR::SetupDescriptionV15
