#pragma once
// =================================================================
// src/core/session/MirrorSchema.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 7.
//
// The meta-object walk behind StateMirror. For each mirrored model class
// it enumerates that class's own Q_PROPERTY declarations, assigns each a
// dense declaration-ordered ordinal for the wire, records the NOTIFY
// signal that announces it, and picks the wire kind its value travels as.
// Built once per class and cached, so five slices pay for one walk.
//
// Two mechanics here are measured facts about this tree's Qt, not
// assumptions, and tst_mirror_schema pins both:
//
//   1. The encoding kind comes from QMetaType::IsEnumeration and NEVER
//      from QMetaProperty::isEnumType(). NereusSDR's enums live in plain
//      namespaces with no Q_NAMESPACE / Q_ENUM_NS (src/core/WdspTypes.h,
//      src/models/Band.h), so isEnumType() reports FALSE for every one of
//      them while the QMetaType flag reports true. Classifying by
//      isEnumType() would silently mis-encode twelve SliceModel
//      properties including dspMode, agcMode and band.
//
//   2. connect(sender, QMetaMethod, receiver, QMetaMethod) into a
//      ZERO-ARGUMENT slot works at notify arity 0, 1 and 2. That is what
//      lets one slot absorb the whole surface. Arity 2 matters most:
//      SliceModel::filterChanged(int, int) drives the entire filter
//      surface, and the pre-existing precedent for this technique
//      (MainWindow::wireSliceStatusOverlayTriggers) only ever exercised
//      arity 0 and 1.
//
// Exclusions are deliberate and narrow:
//   - SliceModel::sliceLetter, CONSTANT, derived from sliceIndex, and the
//     only QChar in the surface. Dropped so QChar never reaches the wire.
//   - MeterModel entirely. Its four setters have zero callers anywhere in
//     src or tests and the connect meant to write it is an empty lambda
//     with four Q_UNUSED, so mirroring it would ship four construction
//     defaults forever. Live TX and PA telemetry lives on RadioStatus,
//     which declares no Q_PROPERTY at all and belongs to R4.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 7: mirrored
//                                    property schema. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 3:
//                                    write()/decode() now refuse an enum
//                                    value that names no declared
//                                    enumerator, and this header no
//                                    longer promises per-property domain
//                                    validation that nothing performs.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 4:
//                                    MirrorProperty::ordinal no longer
//                                    claims to be the wire identity. It
//                                    is the in-process coalescing key;
//                                    the wire routes on the name. AI-
//                                    assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QMetaObject;
class QObject;
QT_END_NAMESPACE

namespace NereusSDR {

/// How a mirrored property's value travels on the wire.
///
/// Deliberately narrow. float widens into Float64 rather than earning its
/// own kind, and enums travel as their underlying integer, so the decoder
/// needs four numeric shapes rather than one per C++ type.
enum class MirrorWireKind {
    Bool,
    Int64,   ///< every integral type, widened
    Float64, ///< double, and float widened into it
    Utf8,    ///< QString
    Enum,    ///< carried as the underlying integer

    /// No wire representation. Kept in the schema rather than dropped so
    /// tst_mirror_schema's membership guard can name the offending
    /// property instead of it silently vanishing from the mirror.
    Unsupported,
};

/// One mirrored Q_PROPERTY.
struct MirrorProperty {
    /// Dense, declaration-ordered, stable for the life of the process.
    ///
    /// The IN-PROCESS coalescing key, and nothing more. Whole-branch
    /// review, Important 4: this used to claim to be "the wire identity"
    /// with "the name carried for logging and tests only", and that was
    /// false as shipped. Every cross-process consumer routes by NAME --
    /// StationServer::handlePropertyWrite applies by name,
    /// StationClient::applyUpdates and onWriteFlushTick both resolve
    /// schema.byName(update.name), StationCapabilities::fromUpdates
    /// dispatches on name with a hardcoded ordinal of 0, and
    /// StationClient::handleSchema builds a name set and discards every
    /// ordinal the station sent it. StateMirror's ordinal-keyed
    /// applyInbound overload has no production caller.
    ///
    /// Where the ordinal IS load-bearing: MirrorCoalescer keys its
    /// per-object pending map on it, and flushCoalescedDeltas() re-reads
    /// through byOrdinal(). Both ends of that are inside one process, so
    /// ordinal skew between two builds cannot desynchronise anything --
    /// no consumer assumes the two sides agree.
    ///
    /// The consequence for anyone tempted to shrink a 146-property
    /// snapshot by dropping `name` from updateToJson/updateFromJson:
    /// don't. It would make every property.write refuse with "no such
    /// mirrored property" and every inbound delta land in the
    /// schema-only-on-station bucket, both log-only and both silent to
    /// the operator. tst_mirror_inbound's
    /// thePropertyNameIsWhatTheWireRoutesOnNotTheOrdinal pins it.
    quint16 ordinal = 0;

    QByteArray name;
    MirrorWireKind kind = MirrorWireKind::Unsupported;

    /// Index into the owning QMetaObject's property table.
    int metaIndex = -1;

    /// QMetaMethod::methodIndex() of the declared NOTIFY signal, or -1
    /// when the property is CONSTANT. This is the value senderSignalIndex()
    /// returns from inside a slot, so the two are directly comparable.
    int notifyMethodIndex = -1;

    bool isWritable = false;

    /// True when the property declares no NOTIFY. Such a property is
    /// unreachable through the forwarder and only ever travels in a
    /// snapshot; SliceModel::sliceIndex, the mirror's object identity, is
    /// the case that matters.
    bool isConstant = false;

    QMetaType metaType;
};

/// One property's value, encoded for the wire.
struct MirrorUpdate {
    quint16 ordinal = 0;
    QByteArray name;
    MirrorWireKind kind = MirrorWireKind::Unsupported;
    QVariant value; ///< already encoded to `kind`
};

class MirrorSchema {
public:
    /// Is this class part of the mirrored surface at all? An allowlist, so
    /// a model class added later is not mirrored until someone says so.
    ///
    /// Keys on the LEAF class name, so a subclass of a mirrored model is
    /// NOT mirrorable. Deliberate (the ordinals are per-class and a
    /// subclass would extend the property table, shifting nothing but
    /// adding ordinals the peer's schema does not know), but it means a
    /// test double written as `class FakeSlice : public SliceModel` cannot
    /// be watched. It fails loudly rather than silently: watch() returns
    /// false and logs. Tests 9, 10 and 16 use model doubles; use a real
    /// SliceModel, or add the double's own name to kMirroredClasses.
    static bool isMirrorable(const QMetaObject* mo);

    /// Short names of every mirrored class, so a guard test can check the
    /// production allowlist and its own list against each other rather
    /// than duplicating one into the other and hoping.
    static QList<QByteArray> mirroredClassNames();

    /// The cached schema for a class. Non-mirrorable classes get a valid
    /// but empty schema rather than a null reference, so callers need no
    /// special case. Thread-safe; the walk happens at most once per class.
    static const MirrorSchema& forMetaObject(const QMetaObject* mo);
    static const MirrorSchema& forObject(const QObject* obj);

    const QMetaObject* metaObject() const { return m_metaObject; }
    QByteArray className() const { return m_className; }

    const QList<MirrorProperty>& properties() const { return m_properties; }
    int size() const { return static_cast<int>(m_properties.size()); }

    const MirrorProperty* byOrdinal(quint16 ordinal) const;
    const MirrorProperty* byName(const QByteArray& name) const;

    /// Every ordinal announced by this notify signal. A notifier is
    /// one-to-many by design: filterChanged names filterLow and
    /// filterHigh, TunerModel::stateChanged names four properties,
    /// PanadapterModel::levelChanged names two. Returns an empty list for
    /// an unknown index.
    const QList<quint16>& ordinalsForNotifySignal(int notifyMethodIndex) const;

    /// Ordinals with no NOTIFY. Reachable only through the snapshot path.
    const QList<quint16>& constantOrdinals() const { return m_constantOrdinals; }

    /// Distinct notify method indices, in first-declaration order. The
    /// forwarder connects each exactly once.
    const QList<int>& notifyMethodIndices() const { return m_notifyMethodIndices; }

    /// Read a property off a live object and encode it for the wire.
    /// Returns an invalid QVariant if the object is not of this class or
    /// the property has no wire representation.
    QVariant read(const MirrorProperty& prop, const QObject* obj) const;

    /// Decode a wire value and write it into a live object. Returns false
    /// without touching the object if it is not of this class, the
    /// property carries no WRITE, or the value cannot be decoded.
    ///
    /// "Cannot be decoded" covers REPRESENTABILITY, in two forms. For an
    /// integer, WIDTH: a value that does not survive a round trip into
    /// the property's declared type is rejected rather than truncated.
    /// For an enum, MEMBERSHIP: a value that names no declared
    /// enumerator (MirrorEnumDomain) is rejected rather than written, so
    /// a property whose C++ type is DSPMode cannot come to hold 9999.
    ///
    /// It does NOT cover the value being SENSIBLE for the property, and
    /// as of the R2 whole-branch review nothing at any layer does.
    /// Nothing stops a remote peer setting a 40 kHz filterLow or a
    /// negative ritHz: StateMirror::applyInboundToProperty checks
    /// isConstant, then MirrorPolicy::inboundAllowed, then calls write().
    /// Earlier revisions of this comment attributed per-property domain
    /// validation to the inbound apply; that was a promise nobody kept,
    /// and it is recorded here as an open gap rather than assigned to a
    /// layer that does not perform it.
    ///
    /// This is the codec only. Whether a remote peer is ALLOWED to write a
    /// given property is MirrorPolicy's question, and gating on it is the
    /// caller's job (Task 8's inbound apply).
    bool write(const MirrorProperty& prop, QObject* obj, const QVariant& wireValue) const;

    /// Value codec, exposed separately so a caller holding a value already
    /// (rather than a live object) can convert without a second read.
    static QVariant encode(const MirrorProperty& prop, const QVariant& nativeValue);
    static QVariant decode(const MirrorProperty& prop, const QVariant& wireValue);

    /// The wire kind a given property type maps to.
    static MirrorWireKind kindFor(QMetaType metaType);

    /// Trailing component of a possibly namespace-qualified class name, so
    /// "NereusSDR::SliceModel" and "SliceModel" name the same class.
    /// QMetaObject::className() reports the qualified form for these
    /// models; hand-written call sites and MirrorPolicy's table use the
    /// bare one. Shared so both agree on exactly one rule.
    static QByteArray shortClassName(const QByteArray& className);

private:
    explicit MirrorSchema(const QMetaObject* mo);

    const QMetaObject* m_metaObject = nullptr;
    QByteArray m_className;
    QList<MirrorProperty> m_properties;
    QHash<QByteArray, int> m_ordinalByName;
    QHash<int, QList<quint16>> m_ordinalsByNotify;
    QList<int> m_notifyMethodIndices;
    QList<quint16> m_constantOrdinals;
};

} // namespace NereusSDR

// Queued delivery across the session's threads (Task 18) needs these
// registered; declaring them here keeps every consumer covered.
Q_DECLARE_METATYPE(NereusSDR::MirrorUpdate)
Q_DECLARE_METATYPE(QList<NereusSDR::MirrorUpdate>)
