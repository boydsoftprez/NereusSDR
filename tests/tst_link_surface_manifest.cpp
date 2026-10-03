// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_link_surface_manifest.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 1 (R-IOS-01): the drift guard for the link's
// surface. tests/data/link/v1/surface.json must equal LinkSurface::
// capture() on the tree as it is; a change to the wire that does not
// regenerate the file fails here, naming what changed.
//
// Beside that comparison, the declared tables the capture reads are held
// to the code that routes and validates:
//
//   - allKinds() against the SessionMessageKind declaration;
//   - each kind's recorded keys against every key SessionMessages::encode
//     can write for it, read from its source, conditional keys included;
//   - verbSpecs() against the literals SessionCommandDispatcher.cpp routes
//     on and the concrete verbs each prefix family's handler accepts, and
//     against a live dispatch of every verb and of verbs it does not list;
//   - the media control operations against the operations each end
//     dispatches on and the exact key sets they check or build;
//   - the limits held in file-scope constants against their source lines.
//
// Regenerate the file with tst_link_surface_manifest_regen
// (NEREUS_LINK_REGEN_OUT=tests/data/link/v1) and read the diff before
// committing it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01): link
//                                    surface drift guard. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    message keys held to the encoder's
//                                    source; each key's JSON type.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <memory>

#include "core/AppSettings.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DisplayBudget.h"
#include "models/RadioModel.h"

#include "LinkSurface.h"
#include "fakes/ConnectableRadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using NereusSDR::Test::LinkSurface;

namespace {

QString readSource(const QString& relative)
{
    QFile file(QStringLiteral(NEREUS_SOURCE_DIR "/") + relative);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QSet<QString> matches(const QString& text, const QString& pattern, int group = 1)
{
    QSet<QString> found;
    const QRegularExpression re(pattern);
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        found.insert(it.next().captured(group));
    }
    return found;
}

QString joined(QSet<QString> values)
{
    QStringList list(values.cbegin(), values.cend());
    list.sort();
    return list.join(QStringLiteral(", "));
}

QSet<QString> stringSet(const QJsonArray& array)
{
    QSet<QString> out;
    for (const QJsonValue& v : array) {
        out.insert(v.toString());
    }
    return out;
}

// The dispatcher's three words for a verb it does not know: dispatch()'s
// own fallthrough (and handleNotchAction's), handlePureSignalAction's, and
// DspAssetService::execute's.
bool isUnknownVerbRefusal(const SessionMessage& result)
{
    return !result.accepted
        && (result.reason
                == QStringLiteral("The Core does not know this request. Updating the Core may help.")
            || result.reason == QStringLiteral("The Core does not know this PureSignal action.")
            || result.reason == QStringLiteral("The Core does not know this request."));
}

MirrorUpdate defaultArgument(const CommandArgumentSpec& spec)
{
    switch (spec.kind) {
    case MirrorWireKind::Bool: return {0, spec.name, spec.kind, QVariant(false)};
    case MirrorWireKind::Int64:
    case MirrorWireKind::Enum: return {0, spec.name, spec.kind, QVariant(qlonglong(0))};
    case MirrorWireKind::Float64: return {0, spec.name, spec.kind, QVariant(0.0)};
    case MirrorWireKind::Utf8: return {0, spec.name, spec.kind, QVariant(QString())};
    case MirrorWireKind::Unsupported: break;
    }
    return {0, spec.name, spec.kind, QVariant()};
}


// The keys SessionMessages::encode can write for each kind, read from its
// source (`encodeSource` is SessionMessages.cpp): every
// o.insert(QStringLiteral("<key>"), ...) in the switch case(s) of that kind,
// conditional ones included, plus the keys written before the switch for
// every kind. An insert inside an if whose condition names kinds
// (message.kind == SessionMessageKind::X) counts only for those kinds.
// Kinds are keyed by their wire names, from kKindNames in the same source.
QHash<QString, QSet<QString>> encoderKeys(const QString& encodeSource, QString* error)
{
    QHash<QString, QString> wireNameOf;
    static const QRegularExpression kindName(
        QString::fromLatin1(R"re(\{\s*SessionMessageKind::(\w+),\s*"([^"]+)"\s*\})re"));
    QRegularExpressionMatchIterator names = kindName.globalMatch(encodeSource);
    while (names.hasNext()) {
        const QRegularExpressionMatch m = names.next();
        wireNameOf.insert(m.captured(1), m.captured(2));
    }
    const qsizetype begin =
        encodeSource.indexOf(QStringLiteral("QByteArray SessionMessages::encode("));
    const qsizetype end = begin < 0 ? -1
        : encodeSource.indexOf(QStringLiteral("const QByteArray wire ="), begin);
    if (wireNameOf.isEmpty() || begin < 0 || end < 0) {
        *error = QStringLiteral("SessionMessages::encode or kKindNames not found");
        return {};
    }
    static const QRegularExpression caseLabel(
        QString::fromLatin1(R"re(^\s*case\s+SessionMessageKind::(\w+)\s*:)re"));
    static const QRegularExpression insert(
        QString::fromLatin1(R"re(\bo\.insert\(\s*QStringLiteral\("([^"]+)"\))re"));
    static const QRegularExpression kindTest(
        QString::fromLatin1(R"re(message\.kind\s*==\s*SessionMessageKind::(\w+))re"));
    QHash<QString, QSet<QString>> keys;
    QSet<QString> everyKind;
    QStringList group;          // The kinds of the case being read.
    bool previousWasCase = false;
    bool inSwitch = false;
    int depth = 0;
    struct Restriction {
        int depth;
        QStringList kinds;
    };
    QList<Restriction> restrictions;
    QString pendingCondition;   // An if condition still being read.
    const QStringList lines = encodeSource.mid(begin, end - begin).split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.contains(QStringLiteral("switch (message.kind)"))) {
            inSwitch = true;
        }
        const QRegularExpressionMatch label = caseLabel.match(line);
        if (inSwitch && label.hasMatch()) {
            if (!previousWasCase) {
                group.clear();
            }
            group.append(wireNameOf.value(label.captured(1)));
            previousWasCase = true;
        } else if (!line.trimmed().isEmpty()) {
            previousWasCase = false;
        }
        if (line.contains(QStringLiteral("if (")) || !pendingCondition.isEmpty()) {
            pendingCondition += line;
            if (line.contains(QLatin1Char('{'))) {
                QStringList kinds;
                QRegularExpressionMatchIterator it = kindTest.globalMatch(pendingCondition);
                while (it.hasNext()) {
                    kinds.append(wireNameOf.value(it.next().captured(1)));
                }
                if (!kinds.isEmpty()) {
                    restrictions.append({depth + 1, kinds});
                }
                pendingCondition.clear();
            } else if (line.trimmed().endsWith(QLatin1Char(';'))) {
                pendingCondition.clear();
            }
        }
        QRegularExpressionMatchIterator inserts = insert.globalMatch(line);
        while (inserts.hasNext()) {
            const QString key = inserts.next().captured(1);
            if (!inSwitch) {
                everyKind.insert(key);
                continue;
            }
            QStringList kinds = group;
            for (const Restriction& restriction : std::as_const(restrictions)) {
                QStringList narrowed;
                for (const QString& kind : std::as_const(kinds)) {
                    if (restriction.kinds.contains(kind)) {
                        narrowed.append(kind);
                    }
                }
                kinds = narrowed;
            }
            for (const QString& kind : std::as_const(kinds)) {
                keys[kind].insert(key);
            }
        }
        depth += int(line.count(QLatin1Char('{'))) - int(line.count(QLatin1Char('}')));
        while (!restrictions.isEmpty() && depth < restrictions.last().depth) {
            restrictions.removeLast();
        }
    }
    for (const QString& kind : wireNameOf) {
        keys[kind].unite(everyKind);
    }
    return keys;
}

// Where the keys surface.json records for each kind (required and
// optional) differ from what the encoder can write; empty when they agree.
QStringList encoderKeyDrift(const QString& encodeSource, const QJsonObject& messageKinds)
{
    QString error;
    const QHash<QString, QSet<QString>> written = encoderKeys(encodeSource, &error);
    if (!error.isEmpty()) {
        return {error};
    }
    QStringList drift;
    for (auto it = messageKinds.constBegin(); it != messageKinds.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        QSet<QString> recorded = stringSet(entry.value(QStringLiteral("required")).toArray());
        recorded.unite(stringSet(entry.value(QStringLiteral("optional")).toArray()));
        const QSet<QString> encoder = written.value(it.key());
        for (const QString& key : encoder - recorded) {
            drift.append(QStringLiteral("%1: the encoder can write \"%2\", which surface.json "
                                        "does not record")
                             .arg(it.key(), key));
        }
        for (const QString& key : recorded - encoder) {
            drift.append(QStringLiteral("%1: surface.json records \"%2\", which the encoder "
                                        "never writes")
                             .arg(it.key(), key));
        }
    }
    drift.sort();
    return drift;
}
} // namespace

class TstLinkSurfaceManifest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void surfaceHasExactlyTheNineSections();
    void captureRecordsNoErrors();
    void committedSurfaceMatchesTheCode();
    void driftGuardNamesEachKindOfChange();
    void messageKeysAreWhatTheEncoderWrites();
    void aKeyTheSampleNeverSetsIsStillCaught();

    void allKindsListsEveryEnumeratorInOrder();
    void mirroredClassesAreTheSchemaAllowlist();
    void verbTableMatchesTheRouting();
    void verbCapabilitiesAreAdvertised();
    void everyListedVerbIsRoutedAndNoOtherIs();
    void mediaOperationsMatchBothEnds();
    void limitsMatchTheirSources();

private:
    QJsonObject captured();
    QJsonObject m_captured;
};

void TstLinkSurfaceManifest::initTestCase()
{
    // RadioModel reads AppSettings::instance(); keep this process's copy
    // private, as tst_station_session does.
    const QString profile =
        QStringLiteral("link-surface-%1").arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
    AppSettings::instance().clear();
}

void TstLinkSurfaceManifest::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

QJsonObject TstLinkSurfaceManifest::captured()
{
    if (m_captured.isEmpty()) {
        m_captured = LinkSurface::capture();
    }
    return m_captured;
}

void TstLinkSurfaceManifest::surfaceHasExactlyTheNineSections()
{
    QStringList expected = LinkSurface::sectionNames();
    QCOMPARE(expected.size(), 9);
    QStringList keys = captured().keys();
    keys.sort();
    expected.sort();
    QCOMPARE(keys, expected);
}

void TstLinkSurfaceManifest::captureRecordsNoErrors()
{
    const QJsonObject surface = captured();
    const QJsonObject kinds = surface.value(QStringLiteral("messageKinds")).toObject();
    QCOMPARE(kinds.size(), SessionMessages::allKinds().size());
    for (auto it = kinds.constBegin(); it != kinds.constEnd(); ++it) {
        QVERIFY2(!it.value().toObject().contains(QStringLiteral("error")),
                 qPrintable(it.key() + QStringLiteral(": ")
                            + it.value().toObject().value(QStringLiteral("error")).toString()));
        QVERIFY2(stringSet(it.value().toObject().value(QStringLiteral("required")).toArray())
                     .contains(QStringLiteral("type")),
                 qPrintable(it.key()));
    }
    // The shape the brief gives for hello.
    QCOMPARE(stringSet(kinds.value(QStringLiteral("hello")).toObject()
                           .value(QStringLiteral("required")).toArray()),
             (QSet<QString>{QStringLiteral("type"), QStringLiteral("major"),
                            QStringLiteral("minor"), QStringLiteral("settingsSchema"),
                            QStringLiteral("peer")}));

    const QJsonArray keys = surface.value(QStringLiteral("objectKeys")).toArray();
    QVERIFY(!keys.isEmpty());
    QSet<QString> patterns;
    for (const QJsonValue& v : keys) {
        QVERIFY2(!v.toObject().contains(QStringLiteral("error")),
                 qPrintable(v.toObject().value(QStringLiteral("error")).toString()));
        patterns.insert(v.toObject().value(QStringLiteral("key")).toString());
    }
    for (const char* key : {"radio", "transmit", "tuner", "pan:<i>", "slice:<id>"}) {
        QVERIFY2(patterns.contains(QString::fromLatin1(key)), key);
    }
    // Every mirrored class reaches the wire under some key.
    QSet<QByteArray> classes;
    for (const QJsonValue& v : keys) {
        classes.insert(v.toObject().value(QStringLiteral("class")).toString().toUtf8());
    }
    const QList<QByteArray> mirrored = MirrorSchema::mirroredClassNames();
    QCOMPARE(classes, QSet<QByteArray>(mirrored.cbegin(), mirrored.cend()));
}

void TstLinkSurfaceManifest::committedSurfaceMatchesTheCode()
{
    QFile file(QStringLiteral(NEREUS_LINK_DATA_DIR "/surface.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
    QVERIFY(doc.isObject());
    const QStringList diff = LinkSurface::differences(doc.object(), captured());
    QVERIFY2(diff.isEmpty(),
             qPrintable(QStringLiteral("tests/data/link/v1/surface.json no longer matches the "
                                       "code; regenerate it with "
                                       "tst_link_surface_manifest_regen and review:\n  ")
                        + diff.join(QStringLiteral("\n  "))));
}

void TstLinkSurfaceManifest::driftGuardNamesEachKindOfChange()
{
    const QJsonObject surface = captured();
    QVERIFY(LinkSurface::differences(surface, surface).isEmpty());

    // A capability added.
    {
        QJsonObject changed = surface;
        QJsonArray caps = changed.value(QStringLiteral("capabilities")).toArray();
        caps.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("linkSurfaceProbeVersion")},
                                {QStringLiteral("kind"), QStringLiteral("i64")}});
        changed.insert(QStringLiteral("capabilities"), caps);
        const QStringList diff = LinkSurface::differences(surface, changed);
        QCOMPARE(diff.size(), 1);
        QVERIFY2(diff.first().startsWith(
                     QStringLiteral("capabilities[linkSurfaceProbeVersion]: added")),
                 qPrintable(diff.first()));
    }
    // A mirrored property changes kind.
    {
        QJsonObject changed = surface;
        QJsonObject classes = changed.value(QStringLiteral("mirrorClasses")).toObject();
        QJsonObject slice = classes.value(QStringLiteral("SliceModel")).toObject();
        QJsonArray properties = slice.value(QStringLiteral("properties")).toArray();
        bool found = false;
        for (qsizetype i = 0; i < properties.size(); ++i) {
            QJsonObject prop = properties.at(i).toObject();
            if (prop.value(QStringLiteral("name")).toString() == QStringLiteral("frequency")) {
                QVERIFY(prop.value(QStringLiteral("kind")).toString() != QStringLiteral("utf8"));
                prop.insert(QStringLiteral("kind"), QStringLiteral("utf8"));
                properties.replace(i, prop);
                found = true;
            }
        }
        QVERIFY(found);
        slice.insert(QStringLiteral("properties"), properties);
        classes.insert(QStringLiteral("SliceModel"), slice);
        changed.insert(QStringLiteral("mirrorClasses"), classes);
        const QStringList diff = LinkSurface::differences(surface, changed);
        QCOMPARE(diff.size(), 1);
        QVERIFY2(diff.first().startsWith(
                     QStringLiteral("mirrorClasses.SliceModel.properties[frequency].kind: ")),
                 qPrintable(diff.first()));
        QVERIFY2(diff.first().endsWith(QStringLiteral("-> \"utf8\"")), qPrintable(diff.first()));
    }
    // A verb gains an argument.
    {
        QJsonObject changed = surface;
        QJsonArray commands = changed.value(QStringLiteral("commands")).toArray();
        bool found = false;
        for (qsizetype i = 0; i < commands.size(); ++i) {
            QJsonObject command = commands.at(i).toObject();
            if (command.value(QStringLiteral("verb")).toString() == QStringLiteral("addSlice")) {
                QJsonArray arguments = command.value(QStringLiteral("arguments")).toArray();
                arguments.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("probe")},
                                             {QStringLiteral("kind"), QStringLiteral("bool")},
                                             {QStringLiteral("optional"), false}});
                command.insert(QStringLiteral("arguments"), arguments);
                commands.replace(i, command);
                found = true;
            }
        }
        QVERIFY(found);
        changed.insert(QStringLiteral("commands"), commands);
        const QStringList diff = LinkSurface::differences(surface, changed);
        QCOMPARE(diff.size(), 1);
        QVERIFY2(diff.first().startsWith(
                     QStringLiteral("commands[addSlice].arguments[probe]: added")),
                 qPrintable(diff.first()));
    }
}

void TstLinkSurfaceManifest::messageKeysAreWhatTheEncoderWrites()
{
    // The capture learns a kind's keys from one sample message, so an
    // optional key its sample never sets would not be recorded. The
    // encoder's own source is the other half of the check: every key it can
    // write for a kind is recorded for that kind, and nothing else is.
    const QString source = readSource(QStringLiteral("src/core/session/SessionMessages.cpp"));
    QVERIFY(!source.isEmpty());
    const QJsonObject kinds = captured().value(QStringLiteral("messageKinds")).toObject();
    const QStringList drift = encoderKeyDrift(source, kinds);
    QVERIFY2(drift.isEmpty(), qPrintable(drift.join(QLatin1Char('\n'))));
    // And the reading itself found the conditional keys it must.
    QString error;
    const QHash<QString, QSet<QString>> keys = encoderKeys(source, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(keys.value(QStringLiteral("hello")).contains(QStringLiteral("majors")));
    QVERIFY(keys.value(QStringLiteral("property.write")).contains(QStringLiteral("writeId")));
    QVERIFY(keys.value(QStringLiteral("settings.reject")).contains(QStringLiteral("reason")));
    QVERIFY(!keys.value(QStringLiteral("settings.write")).contains(QStringLiteral("reason")));
    QVERIFY(keys.value(QStringLiteral("settings.value")).contains(QStringLiteral("origin")));
    QVERIFY(!keys.value(QStringLiteral("settings.remove")).contains(QStringLiteral("origin")));
}

void TstLinkSurfaceManifest::aKeyTheSampleNeverSetsIsStillCaught()
{
    // A planted optional key, written only when a field no sample sets is
    // present: the capture would not see it; the encoder's source does.
    QString source = readSource(QStringLiteral("src/core/session/SessionMessages.cpp"));
    const QString anchor = QStringLiteral("        if (message.featuresOnWire) {");
    QVERIFY(source.contains(anchor));
    source.replace(anchor,
                   QStringLiteral("        if (!message.token.isEmpty()) {\n"
                                  "            o.insert(QStringLiteral(\"plantedKey\"), 1);\n"
                                  "        }\n")
                       + anchor);
    const QJsonObject kinds = captured().value(QStringLiteral("messageKinds")).toObject();
    const QStringList drift = encoderKeyDrift(source, kinds);
    QCOMPARE(drift, QStringList{QStringLiteral(
                        "hello: the encoder can write \"plantedKey\", which surface.json does "
                        "not record")});
}

void TstLinkSurfaceManifest::allKindsListsEveryEnumeratorInOrder()
{
    const QString header = readSource(QStringLiteral("src/core/session/SessionMessages.h"));
    QVERIFY(!header.isEmpty());
    const QRegularExpression body(QStringLiteral(
        R"re(enum class SessionMessageKind \{(.*?)\};)re"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch m = body.match(header);
    QVERIFY(m.hasMatch());
    QString text = m.captured(1);
    text.remove(QRegularExpression(QStringLiteral("//[^\n]*")));
    QStringList enumerators;
    for (const QString& part : text.split(QLatin1Char(','))) {
        const QString name = part.trimmed();
        if (!name.isEmpty()) {
            enumerators.append(name);
        }
    }
    const QList<SessionMessageKind> kinds = SessionMessages::allKinds();
    QCOMPARE(kinds.size(), enumerators.size());
    QSet<QByteArray> names;
    for (qsizetype i = 0; i < kinds.size(); ++i) {
        // No enumerator carries an explicit value, so declaration order is
        // value order.
        QCOMPARE(static_cast<int>(kinds.at(i)), static_cast<int>(i));
        const QByteArray name = SessionMessages::kindName(kinds.at(i));
        QVERIFY2(!name.isEmpty(), qPrintable(enumerators.at(i)));
        names.insert(name);
    }
    QCOMPARE(names.size(), kinds.size());
}

void TstLinkSurfaceManifest::mirroredClassesAreTheSchemaAllowlist()
{
    QList<QByteArray> names;
    for (const QMetaObject* mo : LinkSurface::mirroredMetaObjects()) {
        names.append(MirrorSchema::shortClassName(QByteArray(mo->className())));
    }
    QCOMPARE(names, MirrorSchema::mirroredClassNames());
}

void TstLinkSurfaceManifest::verbTableMatchesTheRouting()
{
    const QString dispatcher =
        readSource(QStringLiteral("src/core/session/SessionCommandDispatcher.cpp"));
    QVERIFY(!dispatcher.isEmpty());
    // Only the routing code, not the declared table: scan from dispatch().
    const qsizetype start = dispatcher.indexOf(
        QStringLiteral("void SessionCommandDispatcher::dispatch("));
    QVERIFY(start > 0);
    const QString routing = dispatcher.mid(start);

    const QSet<QString> exact =
        matches(routing, QStringLiteral(R"re(commandVerb\s*==\s*"([^"]+)")re"));
    const QSet<QString> prefixes =
        matches(routing, QStringLiteral(R"re(commandVerb\.startsWith\("([^"]+)"\))re"));
    QVERIFY(!exact.isEmpty());
    QVERIFY(!prefixes.isEmpty());

    QSet<QString> listed;
    for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
        const QString verb = QString::fromUtf8(spec.verb);
        QVERIFY2(!listed.contains(verb), qPrintable(QStringLiteral("listed twice: ") + verb));
        listed.insert(verb);
    }

    // A routed verb missing from the table.
    for (const QString& verb : exact) {
        QVERIFY2(listed.contains(verb),
                 qPrintable(QStringLiteral("routed but not in verbSpecs(): ") + verb));
    }
    // A listed verb that is not routed: neither an exact literal nor under
    // a routed prefix.
    for (const QString& verb : listed) {
        bool routed = exact.contains(verb);
        for (const QString& prefix : prefixes) {
            routed = routed || verb.startsWith(prefix);
        }
        QVERIFY2(routed, qPrintable(QStringLiteral("in verbSpecs() but not routed: ") + verb));
    }

    // Each prefix family: the concrete verbs its handler accepts are the
    // ones the table lists under that prefix.
    const auto listedUnder = [&listed](const QString& prefix) {
        QSet<QString> out;
        for (const QString& verb : listed) {
            if (verb.startsWith(prefix)) {
                out.insert(verb);
            }
        }
        return out;
    };
    QCOMPARE(prefixes, (QSet<QString>{QStringLiteral("ps3."), QStringLiteral("dspAssets."),
                                      QStringLiteral("notch.")}));

    const QSet<QString> notch =
        matches(routing, QStringLiteral(R"re(\bverb\s*==\s*"(notch\.[A-Za-z]+)")re"));
    QVERIFY2(notch == listedUnder(QStringLiteral("notch.")),
             qPrintable(joined(notch) + QStringLiteral(" vs ")
                        + joined(listedUnder(QStringLiteral("notch.")))));

    QSet<QString> ps3 = matches(readSource(QStringLiteral(
                                    "src/core/session/PureSignalSessionFacade.cpp")),
                                QStringLiteral(R"re(return\s+"(ps3\.[A-Za-z]+)")re"));
    ps3.unite(matches(routing, QStringLiteral(R"re(commandVerb\s*==\s*"(ps3\.[A-Za-z]+)")re")));
    QVERIFY2(ps3 == listedUnder(QStringLiteral("ps3.")),
             qPrintable(joined(ps3) + QStringLiteral(" vs ")
                        + joined(listedUnder(QStringLiteral("ps3.")))));

    const QSet<QString> dspAssets =
        matches(readSource(QStringLiteral("src/core/dsp/DspAssetService.cpp")),
                QStringLiteral(R"re(\bverb\s*==\s*"(dspAssets\.[A-Za-z0-9]+)")re"));
    QVERIFY2(dspAssets == listedUnder(QStringLiteral("dspAssets.")),
             qPrintable(joined(dspAssets) + QStringLiteral(" vs ")
                        + joined(listedUnder(QStringLiteral("dspAssets.")))));
}

void TstLinkSurfaceManifest::verbCapabilitiesAreAdvertised()
{
    QSet<QString> capabilities;
    for (const QJsonValue& v : captured().value(QStringLiteral("capabilities")).toArray()) {
        capabilities.insert(v.toObject().value(QStringLiteral("name")).toString());
    }
    for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
        for (const CommandArgumentSpec& argument : spec.arguments) {
            QVERIFY2(argument.kind != MirrorWireKind::Unsupported, spec.verb.constData());
        }
        QVERIFY2(spec.minMinor <= kSessionProtocolMinor, spec.verb.constData());
        if (spec.capability.isEmpty()) {
            QCOMPARE(spec.capabilityVersion, 0);
            continue;
        }
        QVERIFY2(capabilities.contains(QString::fromUtf8(spec.capability)),
                 spec.verb.constData());
        QVERIFY2(spec.capabilityVersion >= 1, spec.verb.constData());
    }
}

void TstLinkSurfaceManifest::everyListedVerbIsRoutedAndNoOtherIs()
{
    std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
    QVERIFY(harness != nullptr);
    SessionCommandDispatcher dispatcher(&harness->model());
    QHash<quint32, SessionMessage> results;
    connect(&dispatcher, &SessionCommandDispatcher::commandResultReady, this,
            [&results](const SessionMessage& result) {
        if (!results.contains(result.commandId)) {
            results.insert(result.commandId, result);
        }
    });

    const QList<CommandVerbSpec>& specs = SessionCommandDispatcher::verbSpecs();
    quint32 id = 0;
    QHash<quint32, QByteArray> verbById;
    for (const CommandVerbSpec& spec : specs) {
        QList<MirrorUpdate> arguments;
        for (const CommandArgumentSpec& argument : spec.arguments) {
            arguments.append(defaultArgument(argument));
        }
        ++id;
        verbById.insert(id, spec.verb);
        dispatcher.dispatch(SessionMessages::commandInvoke(spec.verb, id, arguments));
    }
    const QList<QByteArray> unlisted{"linkSurfaceUnlisted", "nnr.linkSurfaceUnlisted",
                                     "notch.linkSurfaceUnlisted", "ps3.linkSurfaceUnlisted",
                                     "dspAssets.linkSurfaceUnlisted"};
    for (const QByteArray& verb : unlisted) {
        ++id;
        verbById.insert(id, verb);
        dispatcher.dispatch(SessionMessages::commandInvoke(verb, id, {}));
    }

    // Only "routed or not" is asked here: with default arguments on this
    // one harness many verbs are refused for reasons of their own. That a
    // verb reads its arguments (a renamed one gets a different answer) is
    // held per verb, on a station set up for it, by the session fixtures'
    // rightAndWrongLegsGetDifferentAnswers (tst_link_conformance_session).

    // requestSliceSampleRate answers on a later turn of the model's loop.
    QTRY_COMPARE_WITH_TIMEOUT(results.size(), static_cast<int>(id), 10000);
    for (quint32 i = 1; i <= id; ++i) {
        const QByteArray verb = verbById.value(i);
        const SessionMessage result = results.value(i);
        QCOMPARE(result.commandVerb, verb);
        const bool listed = i <= static_cast<quint32>(specs.size());
        if (listed) {
            QVERIFY2(!isUnknownVerbRefusal(result),
                     qPrintable(QString::fromUtf8(verb) + QStringLiteral(": ") + result.reason));
        } else {
            QVERIFY2(isUnknownVerbRefusal(result),
                     qPrintable(QString::fromUtf8(verb) + QStringLiteral(": ") + result.reason));
        }
    }
}

void TstLinkSurfaceManifest::mediaOperationsMatchBothEnds()
{
    const QJsonObject media = captured().value(QStringLiteral("mediaControl")).toObject();
    const QJsonObject guiToCore = media.value(QStringLiteral("guiToCore")).toObject();
    const QJsonObject coreToGui = media.value(QStringLiteral("coreToGui")).toObject();

    const QString daemon =
        readSource(QStringLiteral("src/core/session/media/DaemonMediaController.cpp"));
    const QString peer = readSource(QStringLiteral("src/core/session/media/MediaPeer.cpp"));
    const QString gui = readSource(QStringLiteral("src/gui/RemoteMediaController.cpp"));
    QVERIFY(!daemon.isEmpty() && !peer.isEmpty() && !gui.isEmpty());

    // The operations each end dispatches on.
    const QSet<QString> peerOps =
        matches(peer, QStringLiteral(R"re(constexpr char k[A-Za-z]+Op\[\]\s*=\s*"([a-z-]+)")re"));
    QSet<QString> coreReceives =
        matches(daemon, QStringLiteral(R"re(op\s*==\s*QLatin1String\("([a-z-]+)"\))re"));
    coreReceives.unite(peerOps);
    const QSet<QString> guiReceives =
        matches(gui, QStringLiteral(R"re(op\s*[=!]=\s*QLatin1String\("([a-z-]+)"\))re"));
    const QStringList guiToCoreKeys = guiToCore.keys();
    const QStringList coreToGuiKeys = coreToGui.keys();
    QVERIFY2(QSet<QString>(guiToCoreKeys.cbegin(), guiToCoreKeys.cend()) == coreReceives,
             qPrintable(joined(coreReceives)));
    QVERIFY2(QSet<QString>(coreToGuiKeys.cbegin(), coreToGuiKeys.cend()) == guiReceives,
             qPrintable(joined(guiReceives)));

    // Every exact key set the Core checks on arrival is one declared shape
    // of a GUI-to-Core operation: its fields, or its fields plus one
    // conditional field.
    QSet<QSet<QString>> declaredShapes;
    for (auto it = guiToCore.constBegin(); it != guiToCore.constEnd(); ++it) {
        const QJsonObject op = it.value().toObject();
        const QSet<QString> base = stringSet(op.value(QStringLiteral("fields")).toArray());
        declaredShapes.insert(base);
        const QJsonObject conditional = op.value(QStringLiteral("conditional")).toObject();
        for (auto c = conditional.constBegin(); c != conditional.constEnd(); ++c) {
            declaredShapes.insert(base + QSet<QString>{c.key()});
        }
    }
    const QRegularExpression exactKeysCall(QStringLiteral(
        R"re((?:hasExactKeys|exactKeys)\(\s*\w+\s*,\s*\{([^}]*)\})re"));
    int checked = 0;
    for (const QString& source : {daemon, peer}) {
        auto it = exactKeysCall.globalMatch(source);
        while (it.hasNext()) {
            const QSet<QString> keys = matches(it.next().captured(1),
                                               QStringLiteral(R"re("([A-Za-z0-9]+)")re"));
            if (!keys.contains(QStringLiteral("op"))) {
                continue; // a nested object's keys (parsePlane)
            }
            ++checked;
            QVERIFY2(declaredShapes.contains(keys),
                     qPrintable(QStringLiteral("the Core checks an undeclared shape: ")
                                + joined(keys)));
        }
    }
    QVERIFY(checked >= 10);

    // Every Core-to-GUI operation built inline by the Core carries exactly
    // its declared fields.
    const auto builtKeys = [](const QString& source, const QString& anchor) {
        const qsizetype at = source.indexOf(anchor);
        if (at < 0) {
            return QSet<QString>();
        }
        const QRegularExpression end(QStringLiteral(R"re(\}\)?;)re"));
        const QRegularExpressionMatch stop = end.match(source, at);
        const QString body = source.mid(at, stop.capturedStart() - at);
        QSet<QString> keys = matches(body, QStringLiteral(R"re(\{QStringLiteral\("([A-Za-z0-9]+)"\),)re"));
        keys.insert(QStringLiteral("op"));
        return keys;
    };
    const QList<QPair<QString, QString>> built{
        {QStringLiteral("rejected"), daemon},
        {QStringLiteral("allocation-result"), daemon},
        {QStringLiteral("noise-floor"), daemon},
        {QStringLiteral("clock-echo"), daemon},
    };
    for (const auto& [op, source] : built) {
        const QSet<QString> keys = builtKeys(
            source, QStringLiteral("{QStringLiteral(\"op\"), QStringLiteral(\"%1\")}").arg(op));
        QVERIFY2(!keys.isEmpty(), qPrintable(op));
        QCOMPARE(keys, stringSet(coreToGui.value(op).toObject()
                                     .value(QStringLiteral("fields")).toArray()));
    }
    for (const auto& [op, constant] : {qMakePair(QStringLiteral("description"),
                                                 QStringLiteral("kDescriptionOp")),
                                       qMakePair(QStringLiteral("candidate"),
                                                 QStringLiteral("kCandidateOp"))}) {
        const QSet<QString> keys = builtKeys(
            peer, QStringLiteral("{QStringLiteral(\"op\"), QLatin1String(%1)}").arg(constant));
        QVERIFY2(!keys.isEmpty(), qPrintable(op));
        QCOMPARE(keys, stringSet(coreToGui.value(op).toObject()
                                     .value(QStringLiteral("fields")).toArray()));
        QCOMPARE(keys, stringSet(guiToCore.value(op).toObject()
                                     .value(QStringLiteral("fields")).toArray()));
    }
}

void TstLinkSurfaceManifest::limitsMatchTheirSources()
{
    const QJsonObject limits = captured().value(QStringLiteral("limits")).toObject();
    const auto value = [&limits](const char* name) {
        return limits.value(QLatin1String(name)).toObject().value(QStringLiteral("value"));
    };

    // Both ends keep the same heartbeat.
    QCOMPARE(StationClient::kDefaultHeartbeatIntervalMs, StationServer::kDefaultHeartbeatIntervalMs);
    QCOMPARE(StationClient::kDefaultMaxMissedPongs, StationServer::kDefaultMaxMissedPongs);

    // The values held in file-scope constants, read back from their lines.
    const QString daemon =
        readSource(QStringLiteral("src/core/session/media/DaemonMediaController.cpp"));
    const QRegularExpressionMatch endpoints =
        QRegularExpression(QStringLiteral(R"re(constexpr int kMaxEndpoints = (\d+);)re")).match(daemon);
    QVERIFY(endpoints.hasMatch());
    QCOMPARE(value("maxDisplayEndpoints").toInt(), endpoints.captured(1).toInt());

    const QRegularExpressionMatch pixels = QRegularExpression(QStringLiteral(
        R"re(exactInt\(control\.value\(QStringLiteral\("pixels"\)\), (\d+), SpectrumEndpoint::kMaxPixels)re"))
        .match(daemon);
    QVERIFY(pixels.hasMatch());
    QCOMPARE(value("endpointPixels").toObject().value(QStringLiteral("min")).toInt(),
             pixels.captured(1).toInt());

    const QRegularExpressionMatch fps = QRegularExpression(QStringLiteral(
        R"re(exactInt\(control\.value\(QStringLiteral\("fps"\)\), (\d+), (\d+),)re")).match(daemon);
    QVERIFY(fps.hasMatch());
    QCOMPARE(value("endpointFps").toObject().value(QStringLiteral("min")).toInt(),
             fps.captured(1).toInt());
    QCOMPARE(value("endpointFps").toObject().value(QStringLiteral("max")).toInt(),
             fps.captured(2).toInt());
    QCOMPARE(fps.captured(2).toInt(), static_cast<int>(kMaximumSpectrumDisplayFramesPerSecond));

    const QString client = readSource(QStringLiteral("src/core/session/StationClient.cpp"));
    const QRegularExpressionMatch steps = QRegularExpression(QStringLiteral(
        R"re(constexpr int kReconnectBackoffSteps\[\] = \{([^}]*)\};)re")).match(client);
    QVERIFY(steps.hasMatch());
    QJsonArray expected;
    for (const QString& step : steps.captured(1).split(QLatin1Char(','))) {
        expected.append(step.trimmed().toInt() * StationClient::kDefaultReconnectBackoffUnitMs);
    }
    QCOMPARE(value("clientReconnectBackoffMs").toArray(), expected);
}

QTEST_MAIN(TstLinkSurfaceManifest)
#include "tst_link_surface_manifest.moc"
