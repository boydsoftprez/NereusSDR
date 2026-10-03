#pragma once
// =================================================================
// src/core/session/MirrorEnumDomain.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2, whole-branch
// review finding (Important 3).
//
// The declared value set of every enum type on the mirrored surface, so
// MirrorSchema::decode can refuse an inbound enum value that names no
// enumerator instead of rebuilding it at the enum's underlying width and
// handing it to QMetaProperty::write.
//
// ---- Why this is a hand-declared table and not reflection ----
//
// It would be a table of nothing if Qt could answer the question. It
// cannot, for these enums, and tst_mirror_schema's
// noReflectionRouteReachesTheseEnumsEnumerators measures all four routes
// against this tree's Qt rather than assuming any of them:
//
//   QMetaProperty::isEnumType()      false
//   QMetaProperty::enumerator()      invalid, keyCount 0
//   QMetaType::metaObject()          nullptr
//   QMetaEnum::fromType<T>()         does not compile for such a type
//
// All four need a Q_ENUM or Q_ENUM_NS registration. NereusSDR's enums
// live in plain namespaces with no Q_NAMESPACE (src/core/WdspTypes.h,
// src/models/Band.h) -- the same fact MirrorSchema.h already records as
// the reason its kind classification uses QMetaType::IsEnumeration and
// never isEnumType().
//
// Registering them WOULD make this file unnecessary, and was considered
// and declined here rather than left unsaid. Q_NAMESPACE emits a
// staticMetaObject for the namespace, so it can be declared exactly once
// for NereusSDR; the enums are currently spread across at least two
// headers and would have to be gathered, WdspTypes.h is included almost
// everywhere in the tree, and the registration flips
// QMetaProperty::isEnumType() to true, which MirrorSchema.h documents as
// false and tst_mirror_schema pins. That is an architecture change to a
// core header, not a review fix.
//
// ---- Default deny, and what that costs ----
//
// An enum type with no entry here has an EMPTY domain, so every value
// for it is refused. That matches MirrorPolicy's own default-deny shape
// and fails in the safe direction: a newly declared enum property is not
// remotely writable until someone classifies it, rather than accepting
// anything. tst_mirror_schema's
// everyEnumPropertyOnTheMirroredSurfaceHasADeclaredDomain names the
// offending property so the failure is not silent.
//
// The residual risk this cannot close: adding an enumerator to an enum
// that IS listed here, without adding it below. Nothing in C++ can count
// enumerators without reflection, so no compile-time or test-time guard
// catches that. It fails safe (the new value is refused inbound, and on
// a client an inbound delta carrying it is refused too, showing as a
// property that will not move across a version-skewed link) but it is a
// real maintenance burden, and it is why each list below is written out
// enumerator by enumerator rather than as a first/last range: at least
// every name that IS listed is compile-checked.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 3:
//                                    declared enum domains, so an
//                                    inbound enum value that names no
//                                    enumerator is refused rather than
//                                    written. AI-assisted transformation
//                                    via Anthropic Claude Code.
// =================================================================

#include <QList>
#include <QMetaType>

namespace NereusSDR {

class MirrorEnumDomain {
public:
    /// Is `value` a declared enumerator of `metaType`, in the sense this
    /// protocol means by "a value a remote peer may set"? False for any
    /// type with no declared domain, and false for a declared enumerator
    /// that is an iteration sentinel rather than a value (Band::Count).
    static bool contains(QMetaType metaType, qlonglong value);

    /// Does this type have a declared domain at all? Distinguishes "the
    /// value is wrong" from "nobody has classified this type", which is
    /// what lets the coverage guard name a property instead of reporting
    /// a mysterious refusal.
    static bool hasDomain(QMetaType metaType);

    /// Every declared value of `metaType`, in declaration order. Empty
    /// for an unlisted type. For tests and diagnostics.
    static QList<qlonglong> valuesFor(QMetaType metaType);
};

} // namespace NereusSDR
