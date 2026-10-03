// =================================================================
// src/core/session/MirrorSchema.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 7.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 7: mirrored
//                                    property schema. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 3:
//                                    decode() refuses an enum value that
//                                    names no declared enumerator, and
//                                    the codec's comments no longer
//                                    attribute domain validation to a
//                                    layer that does not perform it.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: the Core's step
//                                    attenuator and preamp (`stepAtt`).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: AlexAntennaFacade mirrored. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46 fix wave: IoBoardHl2Facade mirrored (`ioBoard`).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-47 / R-R3-22: AmplifierModel and RfKitModel mirrored
//                 (`amplifier`, `rfkit`). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-24 - R-R3-48: StationTciModel mirrored (`stationTci`). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: AccessoryDataModel mirrored
//                 (`accessoryData`). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: AccessorySettingsModel mirrored
//                 (`accessorySettings`). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 13 (R-IOS-08): StationDevicesFacade
//                 mirrored (`devices`). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 - iPhone app Task 71 (R-IOS-02): ConnectedDevicesFacade
//                 mirrored (`connectedDevices`). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 - Parity Task 19 (R-IOS-25): SpotSourceHost mirrored
//                (`spotSources`). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 19 (R-IOS-06): StationCatalog mirrored
//                 (`catalog`). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-25 - iPhone app Task 73 (R-IOS-02): SliceMarker mirrored
//                 (`marker:<sliceId>`). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 39 (D14, R-IOS-13): TransmitState
//                 mirrored (`txState`). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 - iPhone app plan Task 25 (R-IOS-18): StationVax mirrored
//                 (vaxVersion 1). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: PaProfilesFacade mirrored (read-only).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Slice control plan Task 4: SliceAccess mirrored
//                 (`access:<sliceId>`). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include "core/session/MirrorSchema.h"

#include "core/session/MirrorEnumDomain.h"

#include <QLoggingCategory>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QString>

#include <iterator>
#include <memory>

Q_LOGGING_CATEGORY(lcMirrorSchema, "nereus.mirror.schema")

namespace NereusSDR {

namespace {

// The mirrored surface, by class name. An allowlist rather than a
// denylist: a model class added later is not mirrored until someone puts
// it here and regenerates MirrorPolicy's table for it.
//
// MeterModel's absence is the one that needed arguing. Its four setters
// have zero callers anywhere in src or tests, and the connect meant to
// write it is an empty lambda with four Q_UNUSED, so mirroring it would
// ship four construction defaults forever and nothing else.
const char* const kMirroredClasses[] = {
    "NereusSDR::SliceModel",
    "NereusSDR::PureSignalSettings",
    "NereusSDR::DspAssetService",
    "NereusSDR::PureSignalSessionFacade",
    "NereusSDR::TransmitModel",
    "NereusSDR::TunerModel",
    "NereusSDR::RadioModel",
    "NereusSDR::PanadapterModel",
    // R-R3-21 / R-R3-09 (notchControlVersion 1): the Core's notch list.
    "NereusSDR::NotchModel",
    // R-R3-46 (radioHardwareVersion 1): the Core's step attenuator and
    // preamp.
    "NereusSDR::StepAttenuatorFacade",
    // R-R3-46 (radioHardwareVersion 2): the Core's Alex antenna settings.
    "NereusSDR::AlexAntennaFacade",
    // R-R3-46 (radioHardwareVersion 3): the Core's HL2 I/O board, read-only.
    "NereusSDR::IoBoardHl2Facade",
    // R-R3-47 / R-R3-22 (remotePgxlControlVersion 1 and
    // remoteRfKitControlVersion 1): the Core's Power Genius XL and RF-Kit
    // RF2K-S status, read-only.
    "NereusSDR::AmplifierModel",
    "NereusSDR::RfKitModel",
    // R-R3-48 (stationTciVersion 1): the Core's station TCI server,
    // read-only.
    "NereusSDR::StationTciModel",
    // R-R3-47 / R-R3-22 (accessoryDataVersion 1): the Core's accessory
    // records and settings, read-only.
    "NereusSDR::AccessoryDataModel",
    // R-R3-47 / R-R3-22 (remotePgxlControlVersion 3 and
    // remoteTgxlControlVersion 1): the amp's and tuner's own settings as the
    // Core last heard them, read-only.
    "NereusSDR::AccessorySettingsModel",
    // iPhone app Task 13 (R-IOS-08, deviceAdminVersion 1): the Core's paired
    // devices and its label, read-only, for a device that signs in by key.
    "NereusSDR::StationDevicesFacade",
    // iPhone app Task 19 (R-IOS-06, stationCatalogVersion 1): the values the
    // Core owns and an app draws its controls from, read-only.
    "NereusSDR::StationCatalog",
    "NereusSDR::SetupDescription",
    // R-IOS-25 / R-R3-49 (parity Task 19, recordStreamVersion 1): the
    // Core's spot sources, read-only.
    "NereusSDR::SpotSourceHost",
    // iPhone app Task 71 (R-IOS-02, sessionHolderVersion 1): who is on the
    // Core, read-only, for a view that shares it with other devices.
    "NereusSDR::ConnectedDevicesFacade",
    // iPhone app Task 73 (R-IOS-02, sessionHolderVersion 1): what another
    // device sees of a slice that is not its own, read-only.
    "NereusSDR::SliceMarker",
    // Slice control plan Task 4 (sliceAccessVersion 1): who controls and who
    // listens to each slice, read-only, for a view that shares slices.
    "NereusSDR::SliceAccess",
    // iPhone app plan Task 39 (D14, R-IOS-13, txStateVersion 1): the Core's
    // transmitter, its meters and why it last stopped, read-only.
    "NereusSDR::TransmitState",
    // iPhone app plan Task 25 (R-IOS-18, vaxVersion 1): the VAX channels of
    // the computer the Core runs on.
    "NereusSDR::StationVax",
    // R-R3-49 / R-IOS-18 (paProfileVersion 1): the Core's PA Gain profiles,
    // read-only; the paProfile verbs change them.
    "NereusSDR::PaProfilesFacade",
};

// Per-property exclusions, as (class, property).
//
// sliceLetter is CONSTANT, derived entirely from sliceIndex (which IS
// mirrored), and the only QChar in the whole surface. Dropping it here is
// what keeps QChar off the wire without the codec needing a kind for it.
struct ExcludedProperty {
    const char* className;
    const char* property;
};
constexpr ExcludedProperty kExcludedProperties[] = {
    { "SliceModel", "sliceLetter" },
};

bool isExcludedProperty(const QByteArray& shortName, const char* property)
{
    for (const ExcludedProperty& e : kExcludedProperties) {
        if (shortName == e.className && qstrcmp(property, e.property) == 0) {
            return true;
        }
    }
    return false;
}

// Rebuild an integer of the property's own underlying width so
// QVariant(QMetaType, const void*) can copy-construct the enum. There is no
// other way in: these enums are not registered with Q_ENUM_NS, so QVariant
// has no name-based or converter-based route back from an integer.
QVariant enumVariantFromInteger(QMetaType metaType, qlonglong value)
{
    switch (metaType.sizeOf()) {
    case 1: { const auto v = static_cast<qint8>(value);  return QVariant(metaType, &v); }
    case 2: { const auto v = static_cast<qint16>(value); return QVariant(metaType, &v); }
    case 4: { const auto v = static_cast<qint32>(value); return QVariant(metaType, &v); }
    case 8: { const auto v = static_cast<qint64>(value); return QVariant(metaType, &v); }
    default:
        return QVariant();
    }
}

QMutex& cacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<const QMetaObject*, std::shared_ptr<MirrorSchema>>& cache()
{
    static QHash<const QMetaObject*, std::shared_ptr<MirrorSchema>> map;
    return map;
}

} // namespace

// ── Construction ──────────────────────────────────────────────────────────

MirrorSchema::MirrorSchema(const QMetaObject* mo)
    : m_metaObject(mo)
{
    if (mo == nullptr) {
        return;
    }
    m_className = QByteArray(mo->className());

    if (!isMirrorable(mo)) {
        return;
    }

    const QByteArray shortName = shortClassName(m_className);

    // propertyOffset() rather than 0: only this class's OWN declarations
    // belong on the wire. Starting at 0 would drag QObject::objectName into
    // every mirrored object.
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        const QMetaProperty metaProp = mo->property(i);
        if (!metaProp.isReadable()) {
            continue;
        }
        if (isExcludedProperty(shortName, metaProp.name())) {
            continue;
        }

        MirrorProperty prop;
        prop.ordinal = static_cast<quint16>(m_properties.size());
        prop.name = QByteArray(metaProp.name());
        prop.metaIndex = i;
        prop.metaType = metaProp.metaType();
        prop.kind = kindFor(prop.metaType);
        prop.isWritable = metaProp.isWritable();
        prop.isConstant = !metaProp.hasNotifySignal();
        prop.notifyMethodIndex =
            metaProp.hasNotifySignal() ? metaProp.notifySignal().methodIndex() : -1;

        if (prop.kind == MirrorWireKind::Unsupported) {
            // Kept in the schema on purpose. tst_mirror_schema's membership
            // guard names it; dropping it here would hide it instead.
            qCWarning(lcMirrorSchema)
                << m_className << "::" << prop.name
                << "has no wire representation for type"
                << prop.metaType.name()
                << "-- it will not be mirrored";
        }

        if (prop.isConstant) {
            m_constantOrdinals.append(prop.ordinal);
        } else {
            QList<quint16>& sharing = m_ordinalsByNotify[prop.notifyMethodIndex];
            if (sharing.isEmpty()) {
                m_notifyMethodIndices.append(prop.notifyMethodIndex);
            }
            sharing.append(prop.ordinal);
        }

        m_ordinalByName.insert(prop.name, static_cast<int>(m_properties.size()));
        m_properties.append(prop);
    }
}

QByteArray MirrorSchema::shortClassName(const QByteArray& className)
{
    const int sep = className.lastIndexOf("::");
    return sep < 0 ? className : className.mid(sep + 2);
}

QList<QByteArray> MirrorSchema::mirroredClassNames()
{
    QList<QByteArray> names;
    names.reserve(static_cast<int>(std::size(kMirroredClasses)));
    for (const char* candidate : kMirroredClasses) {
        names.append(shortClassName(QByteArray(candidate)));
    }
    return names;
}

bool MirrorSchema::isMirrorable(const QMetaObject* mo)
{
    if (mo == nullptr) {
        return false;
    }
    const QByteArray shortName = shortClassName(QByteArray(mo->className()));
    for (const char* candidate : kMirroredClasses) {
        if (shortName == shortClassName(QByteArray(candidate))) {
            return true;
        }
    }
    return false;
}

const MirrorSchema& MirrorSchema::forMetaObject(const QMetaObject* mo)
{
    QMutexLocker locker(&cacheMutex());
    auto it = cache().constFind(mo);
    if (it != cache().constEnd()) {
        return *it.value();
    }
    // Private constructor, so make_shared is not available here.
    std::shared_ptr<MirrorSchema> schema(new MirrorSchema(mo));
    cache().insert(mo, schema);
    return *schema;
}

const MirrorSchema& MirrorSchema::forObject(const QObject* obj)
{
    return forMetaObject(obj != nullptr ? obj->metaObject() : nullptr);
}

// ── Lookup ────────────────────────────────────────────────────────────────

const MirrorProperty* MirrorSchema::byOrdinal(quint16 ordinal) const
{
    const int index = static_cast<int>(ordinal);
    if (index < 0 || index >= m_properties.size()) {
        return nullptr;
    }
    return &m_properties.at(index);
}

const MirrorProperty* MirrorSchema::byName(const QByteArray& name) const
{
    const auto it = m_ordinalByName.constFind(name);
    if (it == m_ordinalByName.constEnd()) {
        return nullptr;
    }
    return &m_properties.at(it.value());
}

const QList<quint16>& MirrorSchema::ordinalsForNotifySignal(int notifyMethodIndex) const
{
    static const QList<quint16> kEmpty;
    const auto it = m_ordinalsByNotify.constFind(notifyMethodIndex);
    return it == m_ordinalsByNotify.constEnd() ? kEmpty : it.value();
}

// ── Codec ─────────────────────────────────────────────────────────────────

MirrorWireKind MirrorSchema::kindFor(QMetaType metaType)
{
    // IsEnumeration, never QMetaProperty::isEnumType(). NereusSDR's enums
    // are declared in plain namespaces with no Q_NAMESPACE / Q_ENUM_NS, so
    // isEnumType() is false for all of them while this flag is true.
    // Measured against this tree's Qt; tst_mirror_schema pins it.
    if (metaType.flags().testFlag(QMetaType::IsEnumeration)) {
        return MirrorWireKind::Enum;
    }

    switch (metaType.id()) {
    case QMetaType::Bool:
        return MirrorWireKind::Bool;

    case QMetaType::Float:
    case QMetaType::Double:
        // float widens rather than earning its own kind.
        return MirrorWireKind::Float64;

    case QMetaType::QString:
        return MirrorWireKind::Utf8;

    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return MirrorWireKind::Int64;

    default:
        return MirrorWireKind::Unsupported;
    }
}

QVariant MirrorSchema::encode(const MirrorProperty& prop, const QVariant& nativeValue)
{
    if (!nativeValue.isValid()) {
        return QVariant();
    }
    switch (prop.kind) {
    case MirrorWireKind::Bool:
        return QVariant(nativeValue.toBool());
    case MirrorWireKind::Int64:
    case MirrorWireKind::Enum:
        return QVariant(nativeValue.toLongLong());
    case MirrorWireKind::Float64:
        return QVariant(nativeValue.toDouble());
    case MirrorWireKind::Utf8:
        return QVariant(nativeValue.toString());
    case MirrorWireKind::Unsupported:
        break;
    }
    return QVariant();
}

QVariant MirrorSchema::decode(const MirrorProperty& prop, const QVariant& wireValue)
{
    if (!wireValue.isValid()) {
        return QVariant();
    }
    switch (prop.kind) {
    case MirrorWireKind::Bool:
        return QVariant(wireValue.toBool());
    case MirrorWireKind::Float64:
        return QVariant(wireValue.toDouble());
    case MirrorWireKind::Utf8:
        return QVariant(wireValue.toString());
    case MirrorWireKind::Int64: {
        // Narrow back to the property's own declared type so
        // QMetaProperty::write does not have to convert.
        //
        // QVariant::convert alone is NOT a range check. Measured against
        // this tree's Qt: converting LongLong(2^40) to int returns TRUE and
        // yields 0; LongLong(300) to qint8 returns TRUE and yields 44;
        // LongLong(-5) to uint returns TRUE and yields 4294967291. So the
        // round trip is compared explicitly, and a value the property
        // cannot hold is rejected rather than silently truncated into a
        // plausible-looking one. filterLow/filterHigh, stepHz, ritHz/xitHz,
        // nr1Taps and snbOutputBandwidthHz all land here.
        //
        // This is a WIDTH check only. It says nothing about a value being
        // semantically sensible for the property: a 40 kHz filterLow fits
        // in an int perfectly well and nothing anywhere refuses it.
        // Earlier revisions of this comment named Task 8's inbound apply
        // as where per-property domain validation happens; it does not
        // happen there or anywhere else (StateMirror::
        // applyInboundToProperty checks isConstant, then
        // MirrorPolicy::inboundAllowed, then writes), and the R2
        // whole-branch review recorded that as an open gap rather than
        // leave the claim standing. The Enum case below is the one
        // membership check that DOES exist, and it is representability
        // rather than domain: 9999 is not a DSPMode at all.
        const qlonglong wire = wireValue.toLongLong();
        QVariant typed{ QVariant(wire) };
        if (!typed.convert(prop.metaType)) {
            return QVariant();
        }
        if (typed.toLongLong() != wire) {
            return QVariant();
        }
        return typed;
    }
    case MirrorWireKind::Enum: {
        // Whole-branch review, Important 3. Before this, an arbitrary
        // integer was rebuilt at the enum's underlying width and handed
        // to QMetaProperty::write with nothing examining it -- not even
        // the round-trip check the Int64 case above performs. A remote
        // peer sending {"kind":"enum","name":"dspMode","value":9999} set
        // SliceModel::m_dspMode = 9999, which reaches SetRXAMode(channel,
        // 9999) via RxChannel.cpp. That is not a memory-safety problem:
        // WDSP's own switch has a default: and indexes nothing by mode
        // (third_party/wdsp/src/RXA.c, SetRXAMode at :848, and likewise
        // RXAbpsnbaCheck at :934 / RXAbpsnbaSet, which only compare).
        // The effect was that the daemon's demodulator held an undefined
        // mode while SliceModel::modeName() reported "USB" on both ends.
        //
        // Membership in a DECLARED table, because no reflection route
        // reaches these enums' enumerators -- see MirrorEnumDomain.h for
        // the four routes, all measured closed, and for why registering
        // them with Q_ENUM_NS is an architecture change rather than a
        // fix. Refusing here rather than in a caller keeps it beside the
        // Int64 width check, which is the same kind of question:
        // representability, not whether the value is sensible.
        const qlonglong wire = wireValue.toLongLong();
        if (!MirrorEnumDomain::contains(prop.metaType, wire)) {
            qCWarning(lcMirrorSchema)
                << "refusing" << prop.name << "value" << wire
                << "-- not a declared value of" << prop.metaType.name();
            return QVariant();
        }
        return enumVariantFromInteger(prop.metaType, wire);
    }
    case MirrorWireKind::Unsupported:
        break;
    }
    return QVariant();
}

// ── Live-object access ────────────────────────────────────────────────────

QVariant MirrorSchema::read(const MirrorProperty& prop, const QObject* obj) const
{
    if (obj == nullptr || m_metaObject == nullptr) {
        return QVariant();
    }
    if (!obj->metaObject()->inherits(m_metaObject)) {
        return QVariant();
    }
    if (prop.metaIndex < 0 || prop.metaIndex >= m_metaObject->propertyCount()) {
        return QVariant();
    }
    return encode(prop, m_metaObject->property(prop.metaIndex).read(obj));
}

bool MirrorSchema::write(const MirrorProperty& prop, QObject* obj,
                         const QVariant& wireValue) const
{
    if (obj == nullptr || m_metaObject == nullptr || !prop.isWritable) {
        return false;
    }
    if (!obj->metaObject()->inherits(m_metaObject)) {
        return false;
    }
    if (prop.metaIndex < 0 || prop.metaIndex >= m_metaObject->propertyCount()) {
        return false;
    }
    const QVariant native = decode(prop, wireValue);
    if (!native.isValid()) {
        return false;
    }
    return m_metaObject->property(prop.metaIndex).write(obj, native);
}

} // namespace NereusSDR
