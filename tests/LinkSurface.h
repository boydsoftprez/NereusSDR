// no-port-check: NereusSDR-original.
#pragma once
// =================================================================
// tests/LinkSurface.h  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 1 (R-IOS-01): the link's whole surface, read from
// the code that speaks it, as one JSON object. tests/data/link/v1/
// surface.json is this object committed; tst_link_surface_manifest
// compares the two and tst_link_surface_manifest_regen writes the file.
//
// Nine sections, exactly:
//
//   messageKinds  every SessionMessageKind by wire name, with the keys a
//                 well-formed message must carry (probed through
//                 SessionMessages::decode), the keys it may carry and
//                 each key's JSON type (tst_link_surface_manifest holds the
//                 key sets to what SessionMessages::encode can write)
//   capabilities  every entry StationCapabilities::toUpdates() emits for a
//                 fully populated descriptor, in order, with its wire kind
//                 and the value the live capture fixture advertises; the
//                 optional txWatchPathVersion has no value when that fixture
//                 does not support a transmit-watch primary route
//   mirrorClasses every MirrorSchema class: each property's ordinal, wire
//                 kind, MirrorPolicy direction and MirrorEnumDomain values
//   objectKeys    the object keys a station's snapshot creates, as
//                 patterns (pan:<i>, slice:<id>), from a live session
//   commands      SessionCommandDispatcher::verbSpecs() plus StationServer's
//                 settingsExport family (handled before generic dispatch)
//   settingsScope the SettingsScope rule tables
//   telemetry     station.metrics.v1 field paths at each version
//   mediaControl  each media.control operation in each direction, with its
//                 exact field set
//   limits        the numeric limits, each with its source constant
//
// capture() runs a real StationServer over an in-process loopback for
// objectKeys, so it needs a QCoreApplication and a private AppSettings
// profile (RadioModel reads AppSettings::instance()); both test
// executables set one in initTestCase().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04  J.J. Boyd / KG4VCF  Qualify optional watch capture values.
//                                    AI-assisted via OpenAI Codex.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01): link
//                                    surface capture. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 3 (R-IOS-01):
//                                    capability values. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    each message key's JSON type.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
struct QMetaObject;
QT_END_NAMESPACE

namespace NereusSDR::Test {

class LinkSurface {
public:
    /// The nine top-level keys of surface.json, in the order the plan
    /// names them.
    static QStringList sectionNames();

    /// The meta-objects of every class MirrorSchema mirrors, in the order
    /// of its allowlist (MirrorSchema.cpp kMirroredClasses).
    static QList<const QMetaObject*> mirroredMetaObjects();

    /// The whole surface, read from the code as it is now.
    static QJsonObject capture();

    /// Every difference between two surfaces, one line each, naming the
    /// path: "capabilities[fooVersion]: added {...}",
    /// "mirrorClasses.SliceModel.properties[frequency].kind: \"f64\" ->
    /// \"utf8\"". Arrays whose elements are objects carrying a "name",
    /// "verb", "key" or "version" are compared by that identity, so one
    /// insertion names one element rather than shifting every index.
    /// Empty when the two are equal.
    static QStringList differences(const QJsonObject& expected, const QJsonObject& actual);
};

} // namespace NereusSDR::Test
