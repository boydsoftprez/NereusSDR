// =================================================================
// tests/tst_mirror_schema.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2 Task 7: MirrorSchema walks Qt's meta-object system
// over the five mirrored models and assigns each Q_PROPERTY a dense,
// declaration-ordered, session-scoped ordinal for the wire; MirrorPolicy
// is the default-deny direction table that says which of those a remote
// GUI may write back.
//
// Three things this file pins that nothing else can:
//
//   1. The arity spike. StateMirror connects EVERY property's NOTIFY to
//      one zero-argument slot via connect(sender, QMetaMethod, receiver,
//      QMetaMethod).  Only arity 1 was ever verified in this project;
//      MainWindow::wireSliceStatusOverlayTriggers (the existing
//      precedent) connects only the six properties in
//      PanadapterApplet::statusOverlaySliceProperties(), none of which
//      has a two-argument notifier.  SliceModel::filterChanged(int,int)
//      does, and it drives the whole filter surface, so arity 2 is
//      pinned here against the real models.
//
//   2. The enum codec rule. These models' enums live in plain namespaces
//      with no Q_NAMESPACE / Q_ENUM_NS (see src/core/WdspTypes.h and
//      src/models/Band.h), so QMetaProperty::isEnumType() is FALSE for
//      them even though they are enums. Classifying by isEnumType()
//      would silently mis-encode dspMode, agcMode, band and nine others.
//      QMetaType::IsEnumeration is the flag that actually holds.
//
//   3. The golden-list guard, asserted as MEMBERSHIP and never as a
//      count. Counts have already drifted twice inside this plan's own
//      execution window. A count assertion turns a routine feature
//      commit into a mysterious failure; a membership assertion names
//      the property that needs a MirrorPolicy entry.
//
// 2026-09-24: R-R3-47 / R-R3-22: AccessoryDataModel joins the mirrored
// list. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47 / R-R3-22: AccessorySettingsModel joins it. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-25: iPhone app Task 73 (R-IOS-02): SliceMarker joins it. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-29: R-R3-49 / R-IOS-18: PaProfilesFacade joins it. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-28: Slice control plan Task 4: SliceAccess joins it. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QMetaType>
#include <QVariant>

#include "core/session/MirrorEnumDomain.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/PureSignalSessionFacade.h"
#include "models/NotchModel.h"
#include "core/StepAttenuatorFacade.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/IoBoardHl2Facade.h"
#include "models/PureSignalSettings.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "models/Band.h"
#include "models/MeterModel.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "models/TunerModel.h"
#include "models/AmplifierModel.h"
#include "models/RfKitModel.h"
#include "models/StationTciModel.h"
#include "models/AccessoryDataModel.h"
#include "models/AccessorySettingsModel.h"
#include "core/session/StationCatalog.h"
#include "core/setup/SetupDescriptionService.h"
#include "core/SpotSourceHost.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/ConnectedDevicesFacade.h"
#include "core/session/SliceAccessSet.h"
#include "core/session/SliceMarker.h"
#include "core/session/StationVaxFacade.h"
#include "core/session/TransmitStateFacade.h"
#include "core/session/PaProfilesFacade.h"

using namespace NereusSDR;

// Zero-argument receiver for the arity spike. Deliberately shaped exactly
// like StateMirror's own watcher slot: no parameters at all, so one slot
// can absorb notifiers of every arity.
class ArityWatcher : public QObject {
    Q_OBJECT
public:
    int hits = 0;
    int lastSignalIndex = -1;

public slots:
    void onNotified()
    {
        ++hits;
        lastSignalIndex = senderSignalIndex();
    }
};

class TestMirrorSchema : public QObject {
    Q_OBJECT

private:
    // Resolve a property by name off a live object's metaobject.
    static QMetaProperty prop(const QObject* obj, const char* name)
    {
        const QMetaObject* mo = obj->metaObject();
        const int idx = mo->indexOfProperty(name);
        Q_ASSERT(idx >= 0);
        return mo->property(idx);
    }

private slots:

    // ── Step 1: the go/no-go arity spike, against the real models ──────────
    //
    // If this fails at arity 2, the entire filter surface needs a different
    // mechanism and the R2 plan changes. It is pinned here rather than left
    // as a one-off scratch probe precisely so a Qt upgrade cannot quietly
    // withdraw the guarantee.
    void metaMethodConnectAcceptsEveryNotifyArity()
    {
        TunerModel tuner;
        SliceModel slice(0);
        ArityWatcher watcher;

        const QMetaObject* wMo = watcher.metaObject();
        const int slotIdx = wMo->indexOfSlot("onNotified()");
        QVERIFY2(slotIdx >= 0, "zero-argument receiver slot must resolve");
        const QMetaMethod slot = wMo->method(slotIdx);
        QCOMPARE(slot.parameterCount(), 0);

        // Arity 0: TunerModel::stateChanged()
        const QMetaMethod sig0 = prop(&tuner, "isOperate").notifySignal();
        QCOMPARE(sig0.name(), QByteArray("stateChanged"));
        QCOMPARE(sig0.parameterCount(), 0);

        // Arity 1: SliceModel::frequencyChanged(double)
        const QMetaMethod sig1 = prop(&slice, "frequency").notifySignal();
        QCOMPARE(sig1.name(), QByteArray("frequencyChanged"));
        QCOMPARE(sig1.parameterCount(), 1);

        // Arity 2: SliceModel::filterChanged(int, int)
        const QMetaMethod sig2 = prop(&slice, "filterLow").notifySignal();
        QCOMPARE(sig2.name(), QByteArray("filterChanged"));
        QCOMPARE(sig2.parameterCount(), 2);

        QVERIFY2(connect(&tuner, sig0, &watcher, slot, Qt::UniqueConnection),
                 "ARITY 0: connect(QMetaMethod -> zero-arg slot) must succeed");
        QVERIFY2(connect(&slice, sig1, &watcher, slot, Qt::UniqueConnection),
                 "ARITY 1: connect(QMetaMethod -> zero-arg slot) must succeed");
        QVERIFY2(connect(&slice, sig2, &watcher, slot, Qt::UniqueConnection),
                 "ARITY 2: connect(QMetaMethod -> zero-arg slot) must succeed");

        // Shared notifiers are what make UniqueConnection load-bearing:
        // filterLow and filterHigh both ask for filterChanged, and the
        // second ask must be refused rather than doubling every emission.
        QCOMPARE(prop(&slice, "filterHigh").notifySignal().methodIndex(),
                 sig2.methodIndex());
        QVERIFY2(!connect(&slice, sig2, &watcher, slot, Qt::UniqueConnection),
                 "UniqueConnection must refuse a duplicate arity-2 connect");

        // ...and they must actually fire, with senderSignalIndex() usable
        // from inside a zero-argument slot to identify which one did.
        watcher.hits = 0;
        slice.setFrequency(14200000.0);
        QCOMPARE(watcher.hits, 1);
        QCOMPARE(watcher.lastSignalIndex, sig1.methodIndex());

        watcher.hits = 0;
        slice.setFilterLow(-2700);
        QCOMPARE(watcher.hits, 1);
        QCOMPARE(watcher.lastSignalIndex, sig2.methodIndex());
    }

    // ── Step 2: enum codec regression guard ───────────────────────────────
    //
    // Measured, not assumed. If a future Qt or a future Q_ENUM_NS on
    // NereusSDR's enum namespaces flips either of these, the schema's
    // classification rule has to be revisited rather than silently
    // producing the other kind.
    void enumPropertiesReportIsEnumerationButNotIsEnumType()
    {
        SliceModel slice(0);
        for (const char* name : { "dspMode", "agcMode", "band" }) {
            const QMetaProperty p = prop(&slice, name);
            QVERIFY2(p.metaType().flags().testFlag(QMetaType::IsEnumeration),
                     qPrintable(QStringLiteral("%1: QMetaType::IsEnumeration "
                                               "must be set").arg(name)));
            QVERIFY2(!p.isEnumType(),
                     qPrintable(QStringLiteral("%1: isEnumType() is false for "
                                               "these unregistered enums; "
                                               "classifying by it would "
                                               "mis-encode").arg(name)));
        }
    }

    // Whole-branch review, Important 3. Two headers promised that
    // per-property domain validation happened at the inbound-apply
    // layer; nothing at any layer performed it, and for Enum the codec
    // did not even do the width round trip it does for Int64 -- an
    // arbitrary integer was rebuilt at the enum's underlying width and
    // handed straight to QMetaProperty::write.
    //
    // Before deciding what CAN be checked, this pins what Qt offers for
    // these particular enums. All four routes are measured here rather
    // than assumed, because the answer is what forces the design:
    // MirrorEnumDomain is a hand-declared table precisely because none
    // of these routes yields an enumerator list.
    void noReflectionRouteReachesTheseEnumsEnumerators()
    {
        SliceModel slice(0);
        for (const char* name : { "dspMode", "agcMode", "band", "nbMode" }) {
            const QMetaProperty p = prop(&slice, name);
            const QString label = QString::fromLatin1(name);
            QVERIFY2(p.metaType().flags().testFlag(QMetaType::IsEnumeration),
                     qPrintable(label));
            // The three routes a Q_ENUM / Q_ENUM_NS registration would
            // open. None of these enums has one (plain namespaces, no
            // Q_NAMESPACE -- src/core/WdspTypes.h, src/models/Band.h), so
            // all three are closed. QMetaEnum::fromType<T>() is the
            // fourth and does not even compile for such a type, which is
            // why it cannot appear here.
            QVERIFY2(!p.isEnumType(), qPrintable(label));
            QVERIFY2(p.metaType().metaObject() == nullptr, qPrintable(label));
            QVERIFY2(!p.enumerator().isValid(), qPrintable(label));
            QCOMPARE(p.enumerator().keyCount(), 0);
        }
    }

    // The consequence: an inbound enum value that names no declared
    // enumerator has to be refused by a declared table, since nothing
    // can derive one. Before this, {"kind":"enum","name":"dspMode",
    // "value":9999} set SliceModel::m_dspMode = 9999, which reaches
    // SetRXAMode(channel, 9999) via RxChannel.cpp. That switch has a
    // default: and indexes nothing by mode (third_party/wdsp/src/RXA.c,
    // SetRXAMode at :848, plus RXAbpsnbaCheck at :934 and RXAbpsnbaSet
    // which likewise only compare), so it is not a memory-safety
    // problem -- the effect is that the daemon's demodulator holds an
    // undefined mode while SliceModel::modeName() still reports "USB" on
    // both ends.
    void inboundEnumValuesThatNameNoEnumeratorAreRefused()
    {
        SliceModel slice(0);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);
        const MirrorProperty* dsp = schema.byName("dspMode");
        QVERIFY(dsp != nullptr);
        QCOMPARE(dsp->kind, MirrorWireKind::Enum);

        slice.setDspMode(DSPMode::USB);
        const DSPMode before = slice.dspMode();

        QVERIFY2(!MirrorSchema::decode(*dsp, QVariant(qlonglong(9999))).isValid(),
                 "9999 is not a declared DSPMode and must be refused");
        QVERIFY2(!schema.write(*dsp, &slice, QVariant(qlonglong(9999))),
                 "the refusal must reach the live-object path");
        QCOMPARE(slice.dspMode(), before);

        // Negative, and one past the last declared enumerator, are the
        // two ways to walk off either end.
        QVERIFY(!MirrorSchema::decode(*dsp, QVariant(qlonglong(-1))).isValid());
        QVERIFY(!MirrorSchema::decode(*dsp, QVariant(qlonglong(14))).isValid());

        // Every declared enumerator still round-trips, including the two
        // NereusSDR-native RADE modes at the top of the range.
        for (const DSPMode mode : { DSPMode::LSB, DSPMode::AM, DSPMode::DRM,
                                    DSPMode::RADE_U, DSPMode::RADE_L }) {
            const QVariant wire(static_cast<qlonglong>(mode));
            QVERIFY2(MirrorSchema::decode(*dsp, wire).isValid(),
                     qPrintable(QString::number(static_cast<int>(mode))));
            QVERIFY(schema.write(*dsp, &slice, wire));
            QCOMPARE(slice.dspMode(), mode);
        }

        // Band carries a Count sentinel that is NOT a band. It must not
        // be accepted as one even though it is a declared enumerator of
        // the C++ type.
        const MirrorProperty* band = schema.byName("band");
        QVERIFY(band != nullptr);
        QVERIFY2(!MirrorSchema::decode(*band, QVariant(qlonglong(Band::Count))).isValid(),
                 "Band::Count is an iteration sentinel, not a band");
        QVERIFY(MirrorSchema::decode(*band, QVariant(qlonglong(Band::Band20m))).isValid());
        QVERIFY(MirrorSchema::decode(*band, QVariant(qlonglong(Band::Band11m))).isValid());
    }

    // Coverage guard, the same shape as everyMirroredPropertyHasAnExplicit
    // PolicyEntry below: an enum property whose type has no declared
    // domain would silently be refused outright (default deny), so the
    // guard names it rather than letting a future property go quietly
    // unwritable.
    void everyEnumPropertyOnTheMirroredSurfaceHasADeclaredDomain()
    {
        QStringList offenders;
        for (const QMetaObject* mo : mirroredMetaObjects()) {
            const MirrorSchema& schema = MirrorSchema::forMetaObject(mo);
            for (const MirrorProperty& p : schema.properties()) {
                if (p.kind != MirrorWireKind::Enum) {
                    continue;
                }
                if (!MirrorEnumDomain::hasDomain(p.metaType)) {
                    offenders << QStringLiteral("%1::%2 (%3) has no MirrorEnumDomain "
                                                "entry, so no remote peer can write it")
                                     .arg(QString::fromLatin1(mo->className()),
                                          QString::fromUtf8(p.name),
                                          QString::fromLatin1(p.metaType.name()));
                }
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1String("\n"))));

        // Not vacuous: the twelve known enum properties must be present.
        SliceModel slice(0);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);
        int enumProperties = 0;
        for (const MirrorProperty& p : schema.properties()) {
            if (p.kind == MirrorWireKind::Enum) {
                ++enumProperties;
            }
        }
        QVERIFY2(enumProperties >= 12, "the enum surface must not have vanished");
    }

    // The write half of the enum codec: an integer off the wire has to get
    // back into a property whose declared type is a scoped enum.
    void enumDecodeWritesBackThroughQMetaProperty()
    {
        SliceModel slice(0);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);

        const MirrorProperty* dsp = schema.byName("dspMode");
        QVERIFY(dsp != nullptr);
        QCOMPARE(dsp->kind, MirrorWireKind::Enum);

        const qlonglong wire = static_cast<qlonglong>(DSPMode::CWU);
        QVERIFY2(schema.write(*dsp, &slice, QVariant(wire)),
                 "enum decode + QMetaProperty::write must succeed");
        QCOMPARE(slice.dspMode(), DSPMode::CWU);

        // ...and round-trips back out as the underlying integer.
        QCOMPARE(schema.read(*dsp, &slice).toLongLong(), wire);
    }

    // QVariant::convert is NOT a range check on its own. Measured against
    // this tree's Qt: LongLong(2^40) -> int returns TRUE and yields 0;
    // LongLong(300) -> qint8 returns TRUE and yields 44; LongLong(-5) ->
    // uint returns TRUE and yields 4294967291. So decode compares the round
    // trip explicitly, and this pins that it does. Without it a remote peer
    // could set filterLow to a truncated value that looks entirely
    // plausible on the wire.
    void integerDecodeRejectsValuesThePropertyCannotHold()
    {
        SliceModel slice(0);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);

        const MirrorProperty* low = schema.byName("filterLow");
        QVERIFY(low != nullptr);
        QCOMPARE(low->kind, MirrorWireKind::Int64);
        QCOMPARE(low->metaType.id(), int(QMetaType::Int));

        // In range: accepted, exactly.
        QVERIFY(MirrorSchema::decode(*low, QVariant(qlonglong(-2700))).isValid());
        QCOMPARE(MirrorSchema::decode(*low, QVariant(qlonglong(-2700))).toLongLong(),
                 -2700LL);

        // Out of range for int: rejected, NOT truncated to 0.
        const qlonglong tooBig = qlonglong(1) << 40;
        QVERIFY2(!MirrorSchema::decode(*low, QVariant(tooBig)).isValid(),
                 "an out-of-range integer must be rejected, not truncated");

        // ...and the rejection reaches the live-object path, so nothing is
        // written.
        const int before = slice.filterLow();
        QVERIFY(!schema.write(*low, &slice, QVariant(tooBig)));
        QCOMPARE(slice.filterLow(), before);
        QVERIFY(schema.write(*low, &slice, QVariant(qlonglong(-2800))));
        QCOMPARE(slice.filterLow(), -2800);
    }

    // ── Step 3/4: the schema walk ─────────────────────────────────────────

    void ordinalsAreDenseAndDeclarationOrdered()
    {
        SliceModel slice(0);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);
        QVERIFY(schema.size() > 0);

        int lastMetaIndex = -1;
        for (int i = 0; i < schema.size(); ++i) {
            const MirrorProperty& p = schema.properties().at(i);
            QCOMPARE(static_cast<int>(p.ordinal), i);
            // Declaration order == increasing metaobject property index.
            QVERIFY2(p.metaIndex > lastMetaIndex,
                     "ordinals must follow declaration order");
            lastMetaIndex = p.metaIndex;
            QCOMPARE(schema.byOrdinal(p.ordinal), &p);
            QCOMPARE(schema.byName(p.name), &p);
        }
    }

    // Five slices must pay for one walk, so the schema is cached per class
    // and two instances must hand back the very same object.
    void schemaIsCachedOncePerClass()
    {
        SliceModel a(0);
        SliceModel b(1);
        QCOMPARE(&MirrorSchema::forObject(&a), &MirrorSchema::forObject(&b));

        PanadapterModel pan;
        QVERIFY(&MirrorSchema::forObject(&pan) != &MirrorSchema::forObject(&a));
    }

    // The schema walks propertyOffset()..propertyCount(), so QObject's own
    // objectName must never reach the wire.
    void inheritedQObjectPropertiesAreNotMirrored()
    {
        SliceModel slice(0);
        QVERIFY(MirrorSchema::forObject(&slice).byName("objectName") == nullptr);
    }

    // ── Membership guard: every enum-typed property is kind Enum ──────────
    //
    // Derived from the metaobject, never from a hardcoded list, so a new
    // enum property is covered the moment it is declared.
    void everyEnumTypedPropertyIsClassifiedEnum()
    {
        QStringList offenders;
        for (const QMetaObject* mo : mirroredMetaObjects()) {
            const MirrorSchema& schema = MirrorSchema::forMetaObject(mo);
            for (const MirrorProperty& p : schema.properties()) {
                const bool isEnum =
                    p.metaType.flags().testFlag(QMetaType::IsEnumeration);
                if (isEnum && p.kind != MirrorWireKind::Enum) {
                    offenders << QStringLiteral("%1::%2 is an enum but kind is "
                                                "not Enum")
                                     .arg(QString::fromLatin1(mo->className()),
                                          QString::fromUtf8(p.name));
                }
                if (!isEnum && p.kind == MirrorWireKind::Enum) {
                    offenders << QStringLiteral("%1::%2 is kind Enum but is not "
                                                "an enum type")
                                     .arg(QString::fromLatin1(mo->className()),
                                          QString::fromUtf8(p.name));
                }
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1String("\n"))));

        // At least the twelve known ones must be present, so a walk that
        // silently produced nothing cannot pass the loop above vacuously.
        SliceModel slice(0);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);
        for (const char* name : { "dspMode", "agcMode", "band", "nbMode",
                                  "activeNr", "nr1Position", "nr2GainMethod",
                                  "nr2NpeMethod", "nr2Position", "nr3Position",
                                  "nr4Algo", "fmTxMode" }) {
            const MirrorProperty* p = schema.byName(name);
            QVERIFY2(p != nullptr, name);
            QVERIFY2(p->kind == MirrorWireKind::Enum, name);
        }
    }

    // Wire kinds for the non-enum surface. float must widen to f64 rather
    // than getting its own wire kind.
    void wireKindsCoverTheNonEnumSurface()
    {
        SliceModel slice(0);
        const MirrorSchema& s = MirrorSchema::forObject(&slice);
        QCOMPARE(s.byName("frequency")->kind, MirrorWireKind::Float64);
        QCOMPARE(s.byName("filterLow")->kind, MirrorWireKind::Int64);
        QCOMPARE(s.byName("locked")->kind, MirrorWireKind::Bool);
        QCOMPARE(s.byName("panKey")->kind, MirrorWireKind::Utf8);

        // float widens: TransmitModel::micGain and TunerModel::fwdPower/swr
        // are the only float-declared properties in the surface.
        TransmitModel tx;
        QCOMPARE(MirrorSchema::forObject(&tx).byName("micGain")->kind,
                 MirrorWireKind::Float64);
        TunerModel tuner;
        QCOMPARE(MirrorSchema::forObject(&tuner).byName("swr")->kind,
                 MirrorWireKind::Float64);
    }

    // No property may reach the wire with a kind the codec cannot carry.
    //
    // Tests the KIND, not any particular type. An earlier version of this
    // guard checked only for QChar and could never fire: sliceLetter, the
    // one QChar in the surface, is excluded by MirrorSchema before it
    // reaches this loop. Meanwhile a property of any OTHER uncarryable type
    // (QStringList, QDateTime, a struct) gets kind Unsupported, is skipped
    // by MirrorSchema::encode, and vanishes from every delta AND every
    // snapshot with nothing but a qCWarning to show for it. The policy-entry
    // guard does fire in that case, but its remediation is "add a line to
    // kEntries[]", after which the property is silently absent forever.
    //
    // Kind is strictly stronger than the old check, because QChar maps to
    // Unsupported too.
    void everyMirroredPropertyHasACarryableKind()
    {
        QStringList offenders;
        for (const QMetaObject* mo : mirroredMetaObjects()) {
            for (const MirrorProperty& p :
                 MirrorSchema::forMetaObject(mo).properties()) {
                if (p.kind == MirrorWireKind::Unsupported) {
                    offenders << QStringLiteral("%1::%2 has type %3, which the "
                                                "wire cannot carry, so it would "
                                                "be silently absent from every "
                                                "delta and snapshot. Give it a "
                                                "MirrorWireKind in "
                                                "MirrorSchema::kindFor, or "
                                                "exclude it deliberately via "
                                                "kExcludedProperties")
                                     .arg(QString::fromLatin1(mo->className()),
                                          QString::fromUtf8(p.name),
                                          QString::fromLatin1(p.metaType.name()));
                }
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1String("\n"))));
    }

    // ── Exclusions ────────────────────────────────────────────────────────

    void sliceLetterIsExcludedFromTheSurface()
    {
        SliceModel slice(0);
        // It really is declared, CONSTANT and QChar...
        const QMetaProperty declared = prop(&slice, "sliceLetter");
        QVERIFY(declared.isValid());
        QVERIFY(!declared.hasNotifySignal());
        QCOMPARE(declared.metaType().id(), int(QMetaType::QChar));
        // ...and the mirror does not carry it.
        QVERIFY(MirrorSchema::forObject(&slice).byName("sliceLetter") == nullptr);
    }

    // Every membership guard below iterates mirroredMetaObjects(), so a
    // model class added to production but forgotten here would be covered
    // by NOTHING. Cross-check the two lists against each other rather than
    // trusting that a human kept them in step.
    void testAndProductionAgreeOnTheMirroredClassList()
    {
        QStringList fromProduction;
        for (const QByteArray& name : MirrorSchema::mirroredClassNames()) {
            fromProduction << QString::fromUtf8(name);
        }
        QStringList fromTest;
        for (const QMetaObject* mo : mirroredMetaObjects()) {
            fromTest << QString::fromUtf8(
                MirrorSchema::shortClassName(QByteArray(mo->className())));
        }
        fromProduction.sort();
        fromTest.sort();

        QVERIFY2(fromProduction == fromTest,
                 qPrintable(QStringLiteral("kMirroredClasses in "
                                           "MirrorSchema.cpp and "
                                           "mirroredMetaObjects() in this test "
                                           "have diverged.\n  production: %1\n"
                                           "  test:       %2")
                                .arg(fromProduction.join(QLatin1String(", ")),
                                     fromTest.join(QLatin1String(", ")))));
    }

    // A CONSTANT property produces no NOTIFY, so it can never generate an
    // outbound delta. Classifying one Bidirectional would let a remote peer
    // write it while every OTHER connected client saw nothing change,
    // desyncing them all with no way to notice.
    void constantPropertiesAreClassifiedConstantSnapshotAndNothingElse()
    {
        QStringList offenders;
        for (const QMetaObject* mo : mirroredMetaObjects()) {
            const QByteArray cls(mo->className());
            for (const MirrorProperty& p :
                 MirrorSchema::forMetaObject(mo).properties()) {
                const MirrorDirection dir = MirrorPolicy::directionFor(cls, p.name);
                if (p.isConstant && dir != MirrorDirection::ConstantSnapshot) {
                    offenders << QStringLiteral("%1::%2 has no NOTIFY but is "
                                                "not ConstantSnapshot")
                                     .arg(QString::fromLatin1(cls),
                                          QString::fromUtf8(p.name));
                }
                if (!p.isConstant && dir == MirrorDirection::ConstantSnapshot) {
                    offenders << QStringLiteral("%1::%2 is ConstantSnapshot but "
                                                "declares a NOTIFY")
                                     .arg(QString::fromLatin1(cls),
                                          QString::fromUtf8(p.name));
                }
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1String("\n"))));
    }

    void meterModelIsNotAMirroredClass()
    {
        MeterModel meter;
        QVERIFY2(!MirrorSchema::isMirrorable(meter.metaObject()),
                 "MeterModel's setters have zero callers; mirroring it would "
                 "ship four construction defaults forever");
        QCOMPARE(MirrorSchema::forObject(&meter).size(), 0);

        // The five that ARE mirrored must all say so.
        for (const QMetaObject* mo : mirroredMetaObjects()) {
            QVERIFY2(MirrorSchema::isMirrorable(mo), mo->className());
        }
    }

    // ── CONSTANT properties ───────────────────────────────────────────────
    //
    // sliceIndex is the mirror's object identity. It carries no NOTIFY, so a
    // watcher that only enumerates notifiers skips it silently and every
    // object.create on the wire comes out anonymous.
    void constantPropertiesAreReachableOnlyThroughTheSnapshot()
    {
        SliceModel slice(3);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);

        const MirrorProperty* p = schema.byName("sliceIndex");
        QVERIFY2(p != nullptr, "sliceIndex must be in the schema");
        QVERIFY(p->isConstant);
        QCOMPARE(p->notifyMethodIndex, -1);
        QVERIFY(schema.constantOrdinals().contains(p->ordinal));

        // Not reachable through any notifier, by construction.
        for (int sigIdx : schema.notifyMethodIndices()) {
            QVERIFY(!schema.ordinalsForNotifySignal(sigIdx).contains(p->ordinal));
        }

        QCOMPARE(schema.read(*p, &slice).toLongLong(), 3LL);
    }

    // ── Shared notifiers ──────────────────────────────────────────────────

    void sharedNotifiersMapToEveryPropertyTheyName()
    {
        SliceModel slice(0);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);

        const MirrorProperty* low = schema.byName("filterLow");
        const MirrorProperty* high = schema.byName("filterHigh");
        QVERIFY(low && high);
        QCOMPARE(low->notifyMethodIndex, high->notifyMethodIndex);

        const QList<quint16> ords =
            schema.ordinalsForNotifySignal(low->notifyMethodIndex);
        QCOMPARE(ords.size(), 2);
        QVERIFY(ords.contains(low->ordinal));
        QVERIFY(ords.contains(high->ordinal));

        // Every distinct notifier appears exactly once in the connect list,
        // which is what keeps UniqueConnection from doing real work.
        const QList<int> distinct = schema.notifyMethodIndices();
        QSet<int> seen;
        for (int i : distinct) {
            QVERIFY2(!seen.contains(i), "notifyMethodIndices() must be distinct");
            seen.insert(i);
        }
        QVERIFY(seen.contains(low->notifyMethodIndex));
    }

    // ── Step 5: MirrorPolicy, the default-deny direction table ────────────

    void unlistedPropertiesDefaultToOutbound()
    {
        QCOMPARE(MirrorPolicy::directionFor("SliceModel", "noSuchPropertyEver"),
                 MirrorDirection::Outbound);
        QCOMPARE(MirrorPolicy::directionFor("NoSuchModel", "frequency"),
                 MirrorDirection::Outbound);
        QVERIFY(!MirrorPolicy::inboundAllowed("SliceModel", "noSuchPropertyEver"));
        QVERIFY(!MirrorPolicy::hasExplicitEntry("SliceModel", "noSuchPropertyEver"));
    }

    // QMetaObject::className() reports "NereusSDR::SliceModel"; the policy
    // table and hand-written call sites use the bare name. Both must land on
    // the same entry, or the golden-list guard passes vacuously while the
    // real lookups all fall through to the Outbound default.
    void qualifiedAndBareClassNamesResolveToTheSameEntry()
    {
        QCOMPARE(MirrorSchema::shortClassName("NereusSDR::SliceModel"),
                 QByteArray("SliceModel"));
        QCOMPARE(MirrorSchema::shortClassName("SliceModel"),
                 QByteArray("SliceModel"));

        SliceModel slice(0);
        const QByteArray qualified(slice.metaObject()->className());
        QVERIFY(qualified.contains("::"));
        QCOMPARE(MirrorPolicy::directionFor(qualified, "panKey"),
                 MirrorPolicy::directionFor("SliceModel", "panKey"));
        QVERIFY(MirrorPolicy::hasExplicitEntry(qualified, "frequency"));
    }

    // MEMBERSHIP, not counts. When a task lands a Q_PROPERTY without adding a
    // policy entry, this names it.
    void everyMirroredPropertyHasAnExplicitPolicyEntry()
    {
        QStringList missing;
        for (const QMetaObject* mo : mirroredMetaObjects()) {
            const QByteArray cls(mo->className());
            for (const MirrorProperty& p :
                 MirrorSchema::forMetaObject(mo).properties()) {
                if (!MirrorPolicy::hasExplicitEntry(cls, p.name)) {
                    // Name the SAFE default explicitly. The large
                    // majority of entries are Bidirectional, so copying
                    // the nearest neighbour is both the path of least
                    // resistance and the wrong answer for anything that
                    // is not an operator control. (Whole-branch review,
                    // Minor 1: the exact fraction used to be quoted here
                    // and had rotted. A count that goes stale on every
                    // added property does not belong in a comment.)
                    missing << QStringLiteral("%1::%2 has no MirrorPolicy "
                                              "entry; add one to "
                                              "MirrorPolicy.cpp. Default to "
                                              "MirrorDirection::Outbound "
                                              "unless this is an operator "
                                              "control a remote GUI should "
                                              "be able to write")
                                   .arg(QString::fromLatin1(cls),
                                        QString::fromUtf8(p.name));
                }
            }
        }
        QVERIFY2(missing.isEmpty(), qPrintable(missing.join(QLatin1String("\n"))));
    }

    // A property with no WRITE physically cannot be applied inbound, so the
    // policy must never call one Bidirectional.
    void everyReadOnlyPropertyIsDeniedInbound()
    {
        QStringList offenders;
        for (const QMetaObject* mo : mirroredMetaObjects()) {
            const QByteArray cls(mo->className());
            for (const MirrorProperty& p :
                 MirrorSchema::forMetaObject(mo).properties()) {
                if (p.isWritable) { continue; }
                if (MirrorPolicy::inboundAllowed(cls, p.name)) {
                    offenders << QStringLiteral("%1::%2 has no WRITE but the "
                                                "policy allows inbound")
                                     .arg(QString::fromLatin1(cls),
                                          QString::fromUtf8(p.name));
                }
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1String("\n"))));
    }

    // The reverse guard: an entry naming a property that no longer exists is
    // dead weight that silently stops protecting anything.
    void everyPolicyEntryNamesALivePropertyOfAMirroredClass()
    {
        QStringList stale;
        for (const MirrorPolicy::Entry& e : MirrorPolicy::entries()) {
            bool found = false;
            for (const QMetaObject* mo : mirroredMetaObjects()) {
                if (MirrorSchema::shortClassName(QByteArray(mo->className()))
                    != MirrorSchema::shortClassName(QByteArray(e.className))) {
                    continue;
                }
                found = MirrorSchema::forMetaObject(mo).byName(e.property) != nullptr;
                break;
            }
            if (!found) {
                stale << QStringLiteral("MirrorPolicy entry %1::%2 names no "
                                        "mirrored property")
                             .arg(QString::fromLatin1(e.className),
                                  QString::fromLatin1(e.property));
            }
        }
        QVERIFY2(stale.isEmpty(), qPrintable(stale.join(QLatin1String("\n"))));
    }

    // The seven design decisions the R2 plan makes by name. These ARE
    // hardcoded, because they are choices rather than derivable facts: all
    // seven carry WRITE, so nothing in the metaobject would deny them.
    void theSevenWritableOutboundOnlyPropertiesAreDeniedInbound()
    {
        SliceModel slice(0);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);
        for (const char* name : { "chainIndex", "ddcIndex", "streamIndex",
                                  "shiftOffsetHz", "sampleRateHz",
                                  "widebandExtensionRequested", "psPaused" }) {
            const MirrorProperty* p = schema.byName(name);
            QVERIFY2(p != nullptr, name);
            QVERIFY2(p->isWritable,
                     qPrintable(QStringLiteral("%1 is expected to carry WRITE; "
                                               "if it lost it, this entry is "
                                               "now redundant").arg(name)));
            QVERIFY2(MirrorPolicy::directionFor("SliceModel", name)
                         == MirrorDirection::Outbound, name);
            QVERIFY2(!MirrorPolicy::inboundAllowed("SliceModel", name), name);
        }
    }

    // The R2 plan's step 5 deliberately corrects the design addendum's 6.1
    // here: panKey is mirrored outbound so a reconnecting client restores its
    // layout AND applied inbound under the mirror's guard, with pan-affecting
    // creation routed through the addSliceOnPan verb instead.
    void panKeyIsBidirectional()
    {
        QCOMPARE(MirrorPolicy::directionFor("SliceModel", "panKey"),
                 MirrorDirection::Bidirectional);
        QVERIFY(MirrorPolicy::inboundAllowed("SliceModel", "panKey"));
    }

    void sliceIndexIsConstantSnapshotAndNeverInbound()
    {
        QCOMPARE(MirrorPolicy::directionFor("SliceModel", "sliceIndex"),
                 MirrorDirection::ConstantSnapshot);
        QVERIFY(!MirrorPolicy::inboundAllowed("SliceModel", "sliceIndex"));
    }

    // Daemon-produced RADE telemetry that carries WRITE only so the decoder
    // can set it. Outbound because writing either has a real effect on
    // station behaviour, not because it is read-only in spirit: both
    // setters restart the RADE idle-clear timer on any live write, so a
    // client could pin a stale callsign and SNR on the operator's own VFO
    // flag indefinitely, and lastRadeRxCallsign has no periodic writer to
    // overwrite a fabricated value with.
    void radeTelemetryIsOutbound()
    {
        SliceModel slice(0);
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);
        for (const char* name : { "snrDb", "lastRadeRxCallsign" }) {
            const MirrorProperty* p = schema.byName(name);
            QVERIFY2(p != nullptr, name);
            QVERIFY2(p->isWritable,
                     qPrintable(QStringLiteral("%1 is expected to carry WRITE, "
                                               "which is why an explicit "
                                               "Outbound entry is needed")
                                    .arg(name)));
            QVERIFY2(MirrorPolicy::directionFor("SliceModel", name)
                         == MirrorDirection::Outbound, name);
            QVERIFY2(!MirrorPolicy::inboundAllowed("SliceModel", name), name);
        }
    }

    void bandIsOutbound()
    {
        QCOMPARE(MirrorPolicy::directionFor("SliceModel", "band"),
                 MirrorDirection::Outbound);
        QVERIFY(!MirrorPolicy::inboundAllowed("SliceModel", "band"));
    }

    // A representative operator control has to actually be writable back, or
    // the whole default-deny table is a very elaborate way of denying
    // everything.
    void ordinaryOperatorControlsAreBidirectional()
    {
        for (const char* name : { "frequency", "dspMode", "filterLow",
                                  "agcMode", "afGain", "muted" }) {
            QVERIFY2(MirrorPolicy::inboundAllowed("SliceModel", name), name);
        }
        QVERIFY(MirrorPolicy::inboundAllowed("TransmitModel", "power"));
        QVERIFY(MirrorPolicy::inboundAllowed("PanadapterModel", "centerFrequency"));
    }

    void policyEntriesAreUnique()
    {
        QSet<QByteArray> seen;
        QStringList dupes;
        for (const MirrorPolicy::Entry& e : MirrorPolicy::entries()) {
            const QByteArray key = QByteArray(e.className) + "::" + e.property;
            if (seen.contains(key)) {
                dupes << QString::fromUtf8(key);
            }
            seen.insert(key);
        }
        QVERIFY2(dupes.isEmpty(), qPrintable(dupes.join(QLatin1String(", "))));
    }

private:
    static QList<const QMetaObject*> mirroredMetaObjects()
    {
        return { &SliceModel::staticMetaObject,
                 &TransmitModel::staticMetaObject,
                 &TunerModel::staticMetaObject,
                 &RadioModel::staticMetaObject,
                 &PanadapterModel::staticMetaObject,
                 &PureSignalSettings::staticMetaObject,
                 &PureSignalSessionFacade::staticMetaObject,
                 &DspAssetService::staticMetaObject,
                 &NotchModel::staticMetaObject,
                 &StepAttenuatorFacade::staticMetaObject,
                 &AlexAntennaFacade::staticMetaObject,
                 &IoBoardHl2Facade::staticMetaObject,
                 &AmplifierModel::staticMetaObject,
                 &RfKitModel::staticMetaObject,
                 &StationTciModel::staticMetaObject,
                 // R-R3-47 / R-R3-22: the Core's accessory records.
                 &AccessoryDataModel::staticMetaObject,
                 // R-R3-47 / R-R3-22: the amp's and tuner's own settings.
                 &AccessorySettingsModel::staticMetaObject,
                 // iPhone app Task 13 (R-IOS-08): the Core's paired devices.
                 &StationDevicesFacade::staticMetaObject,
                 // iPhone app Task 19 (R-IOS-06): the Core's catalogue.
                 &StationCatalog::staticMetaObject,
                 // The Core's read-only Setup descriptions and revision.
                 &SetupDescription::staticMetaObject,
                 // Parity Task 19 (recordStreamVersion 1): the Core's spot
                 // sources, read-only.
                 &SpotSourceHost::staticMetaObject,
                 // iPhone app Task 71 (R-IOS-02): who is on the Core.
                 &ConnectedDevicesFacade::staticMetaObject,
                 // iPhone app Task 73 (R-IOS-02): another device's slice.
                 &SliceMarker::staticMetaObject,
                 // Slice control plan Task 4: who controls and listens.
                 &SliceAccess::staticMetaObject,
                 // iPhone app plan Task 39 (D14, R-IOS-13): the Core's transmitter.
                 &TransmitState::staticMetaObject,
                 // iPhone app plan Task 25 (R-IOS-18): the Core's computer's VAX.
                 &StationVax::staticMetaObject,
                 // R-R3-49 / R-IOS-18: the Core's PA Gain profiles, read-only.
                 &PaProfilesFacade::staticMetaObject };
    }
};

QTEST_MAIN(TestMirrorSchema)
#include "tst_mirror_schema.moc"
