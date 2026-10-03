// no-port-check: NereusSDR-original.
// =================================================================
// tests/LinkFixtures.cpp  (NereusSDR)
// =================================================================
//
// See LinkFixtures.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 3 (R-IOS-01): fixture
//                                    loader, matcher and script player.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): linkMajors().
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    linkMajors read against the
//                                    station's supported majors.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    fixtures say which ends run them and
//                                    each client step's role; placeholders
//                                    in client messages.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 12 (R-IOS-08):
//                                    "$device:<case>" and the runner's own
//                                    device key, made at run time.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 13 (R-IOS-08):
//                                    stationSetup "otherPairedDevices" and
//                                    the device ids it records, with the
//                                    runner's own, for "$ref:device:<n>"
//                                    and "$ref:device:self".
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): the several-client player
//               (stationSetup.otherClients, "client" and "to", the connect
//               and close steps, a named expectClosed); preemptingClient
//               withdrawn. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 73 (R-IOS-02): the session registry counts
//               on the virtual clock, so advanceMs reaches the end of a
//               device's 180 s. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app (R-IOS-01, R-IOS-02): {"$json": <expectation>}
//               matches a station string holding JSON; a client message
//               holding one is refused. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13): the
//               radioPtt step. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 28 (R-R3-49, A11): the
//                                    transmit display's NSDC vector
//                                    (nsdc1-transmit). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): framing fixtures
//               (runFraming) and the session player's data-channel mode
//               (setSettleCheck, setConnectClient). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 28 tail (R-IOS-16): the virtual clock
//               moved to LinkVirtualClock.h, where it judges a restart on
//               the real remaining time and swallows real expiries. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-28: stationSetup.deferOwnConnection and the openOwnConnection
//               step (addendum G-53: the placeFreed fixture). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: The phone's direct addresses: stationSetup's coreListener
//               and coreInterfaces, and the coreInterfaces step. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: The connect hook returns why a client's connection did not
//               open, and the connect and openOwnConnection steps report it
//               before waiting for the station. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-01: The phone's monitor-audio fixtures: "$uuid:<name>", filled
//               with a new canonical UUID, refused in a station message.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "LinkFixtures.h"
#include "LinkVirtualClock.h"

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "core/MoxController.h"
#include "core/dsp/Ps3Snapshot.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/session/LinkVersion.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationLanAnnouncement.h"
#include "models/RadioModel.h"
#include "core/session/DnsSdAdvertiser.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/CoreAddresses.h"
#include "core/daemon/DaemonConfig.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "fakes/LoopbackTransport.h"

namespace NereusSDR::Test {

namespace {

constexpr int kMaxShownChars = 400;
// How long the player waits, in real time, for a station message the event
// queue has not produced yet (work on another thread of the fake radio). A
// passing fixture never waits this long; it bounds a failing one.
constexpr int kStationReplyWaitMs = 5000;

QString shown(const QJsonValue& value)
{
    QByteArray text;
    if (value.isObject()) {
        text = QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
    } else if (value.isArray()) {
        text = QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact);
    } else if (value.isString()) {
        text = '"' + value.toString().toUtf8() + '"';
    } else if (value.isDouble()) {
        text = QByteArray::number(value.toDouble(), 'g', 17);
    } else if (value.isBool()) {
        text = value.toBool() ? "true" : "false";
    } else if (value.isNull()) {
        text = "null";
    } else {
        text = "(absent)";
    }
    QString out = QString::fromUtf8(text);
    if (out.size() > kMaxShownChars) {
        out = out.left(kMaxShownChars) + QStringLiteral("...");
    }
    return out;
}

QString placeholderArgument(const QString& text, const QString& prefix)
{
    return text.startsWith(prefix) ? text.mid(prefix.size()) : QString();
}

// The placeholders of the link document's section 16.3, by their text.
//   $any                 any value (the key must be present)
//   $string[:<name>]     any string; with a name, also recorded
//   $int[:<name>]        any whole number; with a name, also recorded
//   $object              any JSON object
//   $capture:<name>      any value, recorded
//   $ref:<name>          equal to the value recorded under <name>
//   $within:<t>:<v>      a number no further than <t> from <v>
//   $majors              only as a hello's "majors": whole numbers from 0 to
//                        65535, ascending, no repeats, naming its "major"
//   $uuid:<name>         only in a client message: a canonical UUID the
//                        client chose (a media connection id), recorded
// and one object form, {"$json": <expectation>} (isJsonForm below).
bool isPlaceholder(const QJsonValue& value)
{
    if (!value.isString()) {
        return false;
    }
    const QString text = value.toString();
    return text == QStringLiteral("$any") || text == QStringLiteral("$string")
        || text == QStringLiteral("$int") || text == QStringLiteral("$object")
        || text == QStringLiteral("$majors")
        || (text.startsWith(QStringLiteral("$string:")) && text.size() > 8)
        || (text.startsWith(QStringLiteral("$int:")) && text.size() > 5)
        || (text.startsWith(QStringLiteral("$capture:")) && text.size() > 9)
        || (text.startsWith(QStringLiteral("$ref:")) && text.size() > 5)
        || (text.startsWith(QStringLiteral("$uuid:")) && text.size() > 6)
        || text.startsWith(QStringLiteral("$within:"));
}

// Whether `value` is the form {"$json": <expectation>} (section 16.1): an
// object holding the key "$json". It stands where the station sends a
// string holding JSON; a second key beside "$json" is a malformed fixture,
// which the matcher reports.
bool isJsonForm(const QJsonValue& value)
{
    return value.isObject() && value.toObject().contains(QStringLiteral("$json"));
}

// `text` parsed as one JSON value of any kind (an object, an array, a
// string, a number, true, false or null); `why` set, and undefined
// returned, when it is not exactly one. QJsonDocument parses only an
// object or an array at the top, so the text is read as the one element
// of an array.
QJsonValue parseJsonText(const QString& text, QString* why)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArrayLiteral("[") + text.toUtf8() + QByteArrayLiteral("]"), &error);
    if (error.error != QJsonParseError::NoError) {
        *why = error.errorString();
        return QJsonValue(QJsonValue::Undefined);
    }
    if (document.array().size() != 1) {
        *why = document.array().isEmpty() ? QStringLiteral("empty")
                                           : QStringLiteral("more than one value");
        return QJsonValue(QJsonValue::Undefined);
    }
    return document.array().at(0);
}

// Why `actual` is not a list of majors naming `major`, or an empty
// string ("$majors", section 16.1).
QString majorsProblem(const QJsonValue& actual, const QJsonValue& major, const QString& path)
{
    if (!actual.isArray() || actual.toArray().isEmpty()) {
        return QStringLiteral("%1: expected a list of majors, got %2").arg(path, shown(actual));
    }
    double previous = -1.0;
    bool named = false;
    for (const QJsonValue& value : actual.toArray()) {
        const double m = value.toDouble(-1.0);
        if (!value.isDouble() || m < 0.0 || m > 65535.0 || m <= previous
            || m != static_cast<double>(static_cast<qint64>(m))) {
            return QStringLiteral("%1: majors must be whole numbers from 0 to 65535, ascending, "
                                  "without repeats; got %2")
                .arg(path, shown(actual));
        }
        named = named || (major.isDouble() && m == major.toDouble());
        previous = m;
    }
    return named ? QString()
                 : QStringLiteral("%1: majors %2 do not name the hello's major %3")
                       .arg(path, shown(actual), shown(major));
}

struct NamedInt {
    QString name;
    bool ranged = false;
    double min = 0.0;
    double max = 0.0;
};

// "$int:<name>" or "$int:<name>:<min>:<max>" (min and max whole numbers in
// JSON syntax, min not above max); false when the text is neither.
bool parseNamedInt(const QString& text, NamedInt* out)
{
    const QStringList parts = text.mid(5).split(QLatin1Char(':'));
    if (parts.isEmpty() || parts.first().isEmpty() || (parts.size() != 1 && parts.size() != 3)) {
        return false;
    }
    out->name = parts.first();
    out->ranged = parts.size() == 3;
    if (!out->ranged) {
        return true;
    }
    static const QRegularExpression whole(QStringLiteral("^-?(?:0|[1-9][0-9]*)$"));
    if (!whole.match(parts.at(1)).hasMatch() || !whole.match(parts.at(2)).hasMatch()) {
        return false;
    }
    out->min = parts.at(1).toDouble();
    out->max = parts.at(2).toDouble();
    return out->min <= out->max;
}

bool isWholeNumber(const QJsonValue& value)
{
    const double d = value.toDouble();
    return value.isDouble() && std::isfinite(d)
        && d == static_cast<double>(static_cast<qint64>(d));
}

// "$within:<tolerance>:<value>", both in JSON number syntax; false when
// the text is not that.
bool parseWithin(const QString& text, double* tolerance, double* centre)
{
    const QStringList parts = text.mid(8).split(QLatin1Char(':'));
    if (parts.size() != 2) {
        return false;
    }
    // Each part in JSON number syntax exactly (RFC 8259 section 6), not
    // whatever a number parser would take ("+1", "inf", ".5", "0x10").
    static const QRegularExpression jsonNumber(QStringLiteral(
        "^-?(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?$"));
    if (!jsonNumber.match(parts.at(0)).hasMatch() || !jsonNumber.match(parts.at(1)).hasMatch()) {
        return false;
    }
    bool okTolerance = false;
    bool okCentre = false;
    *tolerance = parts.at(0).toDouble(&okTolerance);
    *centre = parts.at(1).toDouble(&okCentre);
    return okTolerance && okCentre && std::isfinite(*tolerance) && *tolerance >= 0.0
        && std::isfinite(*centre);
}

QString expectKeys(const QJsonObject& object, const QStringList& required,
                   const QStringList& optional, const QString& where)
{
    for (const QString& key : required) {
        if (!object.contains(key)) {
            return QStringLiteral("%1: missing \"%2\"").arg(where, key);
        }
    }
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!required.contains(it.key()) && !optional.contains(it.key())) {
            return QStringLiteral("%1: unknown field \"%2\"").arg(where, it.key());
        }
    }
    return QString();
}

// Task 28: the data-channel mode's settle check and client joiner.
std::function<bool()>& settleCheck()
{
    static std::function<bool()> check;
    return check;
}

std::function<QString(LoopbackTransport*, StationServer&)>& connectClientHook()
{
    static std::function<QString(LoopbackTransport*, StationServer&)> hook;
    return hook;
}

// How long drain() waits, in real time, for the control channel to settle
// in the data-channel mode. A passing fixture settles in milliseconds.
constexpr int kSettleWaitMs = 5000;

void drainQueue()
{
    QAbstractEventDispatcher* dispatcher = QAbstractEventDispatcher::instance();
    int idle = 0;
    for (int i = 0; i < 1000 && idle < 2; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        const bool worked = dispatcher != nullptr
            && dispatcher->processEvents(QEventLoop::AllEvents);
        idle = worked ? 0 : idle + 1;
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

// Runs the queued work of every object on this thread until the queue is
// idle, including deleteLater(). No real time passes on purpose, except in
// the data-channel mode, where it also waits until the control channel has
// carried everything either end sent (work on the library's threads).
void drain()
{
    drainQueue();
    if (!settleCheck()) {
        return;
    }
    const QDeadlineTimer deadline(kSettleWaitMs);
    // Settled twice in a row with the queue drained between: nothing
    // handled in between sent anything more.
    int settled = 0;
    while (settled < 2 && !deadline.hasExpired()) {
        if (settleCheck()()) {
            ++settled;
        } else {
            settled = 0;
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
        drainQueue();
    }
}

} // namespace

QString LinkFixtures::dataDirectory()
{
    return QStringLiteral(NEREUS_LINK_DATA_DIR);
}

QJsonObject LinkFixtures::readObject(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot read %1").arg(path);
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QStringLiteral("%1 is not one JSON object: %2")
                     .arg(path, parseError.errorString());
        return {};
    }
    error->clear();
    return doc.object();
}

QString LinkFixtures::checkManifest(const QJsonObject& manifest, const QString& directory)
{
    QString problem = expectKeys(manifest, {QStringLiteral("linkMajors"), QStringLiteral("fixtures")},
                                 {}, QStringLiteral("manifest"));
    if (!problem.isEmpty()) {
        return problem;
    }
    // The link majors the suite covers: whole numbers from 1 to 65535,
    // oldest first without repeats, each one this station supports
    // (LinkVersion::supportedMajors(), not a list written here).
    const QJsonArray majors = manifest.value(QStringLiteral("linkMajors")).toArray();
    if (majors.isEmpty()) {
        return QStringLiteral("manifest: linkMajors must name at least one link major");
    }
    const QList<quint16> supported = LinkVersion::supportedMajors();
    double previous = 0.0;
    for (const QJsonValue& value : majors) {
        const double major = value.toDouble(-1.0);
        if (!value.isDouble() || major < 1.0 || major > 65535.0
            || major != static_cast<double>(static_cast<qint64>(major)) || major <= previous) {
            return QStringLiteral("manifest: linkMajors must be whole numbers from 1 to 65535, "
                                  "oldest first, without repeats");
        }
        if (!supported.contains(static_cast<quint16>(major))) {
            return QStringLiteral("manifest: linkMajors names %1, which this station does not "
                                  "support")
                .arg(static_cast<qint64>(major));
        }
        previous = major;
    }
    const QDir root(directory);
    QSet<QString> ids;
    QSet<QString> listed;
    const QJsonArray fixtures = manifest.value(QStringLiteral("fixtures")).toArray();
    if (fixtures.isEmpty()) {
        return QStringLiteral("manifest: no fixtures");
    }
    for (int i = 0; i < fixtures.size(); ++i) {
        const QString where = QStringLiteral("manifest fixtures[%1]").arg(i);
        if (!fixtures.at(i).isObject()) {
            return where + QStringLiteral(": not an object");
        }
        const QJsonObject entry = fixtures.at(i).toObject();
        problem = expectKeys(entry,
                             {QStringLiteral("id"), QStringLiteral("file"), QStringLiteral("kind"),
                              QStringLiteral("requires")},
                             {}, where);
        if (!problem.isEmpty()) {
            return problem;
        }
        const QString id = entry.value(QStringLiteral("id")).toString();
        const QString file = entry.value(QStringLiteral("file")).toString();
        const QString kind = entry.value(QStringLiteral("kind")).toString();
        if (id.isEmpty() || ids.contains(id)) {
            return QStringLiteral("%1: id \"%2\" is empty or used twice").arg(where, id);
        }
        ids.insert(id);
        if (!entry.value(QStringLiteral("requires")).isObject()) {
            return where + QStringLiteral(": requires must be an object");
        }
        const QJsonObject requirements = entry.value(QStringLiteral("requires")).toObject();
        for (auto it = requirements.constBegin(); it != requirements.constEnd(); ++it) {
            const double version = it.value().toDouble(-1.0);
            if (!it.value().isDouble() || version < 1.0 || version != static_cast<qint64>(version)) {
                return QStringLiteral("%1: requires.%2 must be a whole version of at least 1")
                    .arg(where, it.key());
            }
        }
        const QString folder = kind == QStringLiteral("control")   ? QStringLiteral("control/")
                               : kind == QStringLiteral("session") ? QStringLiteral("sessions/")
                               : kind == QStringLiteral("media")   ? QStringLiteral("media/")
                               : kind == QStringLiteral("framing") ? QStringLiteral("framing/")
                                                                   : QString();
        if (folder.isEmpty()) {
            return QStringLiteral("%1: unknown kind \"%2\"").arg(where, kind);
        }
        const bool media = kind == QStringLiteral("media");
        if (!file.startsWith(folder) || !file.endsWith(media ? QStringLiteral(".bin")
                                                             : QStringLiteral(".json"))) {
            return QStringLiteral("%1: %2 fixture \"%3\" must be %4*%5")
                .arg(where, kind, file, folder,
                     media ? QStringLiteral(".bin") : QStringLiteral(".json"));
        }
        if (!QFileInfo::exists(root.filePath(file))) {
            return QStringLiteral("%1: %2 does not exist").arg(where, file);
        }
        listed.insert(file);
        if (media) {
            const QString expect = file.chopped(4) + QStringLiteral(".expect.json");
            if (!QFileInfo::exists(root.filePath(expect))) {
                return QStringLiteral("%1: %2 does not exist").arg(where, expect);
            }
            listed.insert(expect);
        }
    }
    for (const QString& folder : {QStringLiteral("control"), QStringLiteral("sessions"),
                                  QStringLiteral("media"), QStringLiteral("framing")}) {
        QDirIterator it(root.filePath(folder), QDir::Files);
        while (it.hasNext()) {
            const QString relative = root.relativeFilePath(it.next());
            if (!listed.contains(relative)) {
                return QStringLiteral("manifest: %1 is not listed").arg(relative);
            }
        }
    }
    return QString();
}

QList<quint16> LinkFixtures::linkMajors(const QJsonObject& manifest)
{
    QList<quint16> majors;
    for (const QJsonValue& value : manifest.value(QStringLiteral("linkMajors")).toArray()) {
        majors.append(static_cast<quint16>(value.toInt()));
    }
    return majors;
}

QList<LinkFixtures::Entry> LinkFixtures::entries(const QJsonObject& manifest, const QString& kind)
{
    QList<Entry> out;
    for (const QJsonValue& value : manifest.value(QStringLiteral("fixtures")).toArray()) {
        const QJsonObject o = value.toObject();
        if (o.value(QStringLiteral("kind")).toString() != kind) {
            continue;
        }
        out.append(Entry{o.value(QStringLiteral("id")).toString(),
                         o.value(QStringLiteral("file")).toString(), kind,
                         o.value(QStringLiteral("requires")).toObject()});
    }
    return out;
}

QString LinkFixtures::match(const QJsonValue& expected, const QJsonValue& actual,
                            Captures* captures, const QString& path)
{
    if (isPlaceholder(expected)) {
        const QString text = expected.toString();
        if (actual.isUndefined()) {
            return QStringLiteral("%1: expected %2, the key is absent").arg(path, text);
        }
        if (text == QStringLiteral("$any")) {
            return QString();
        }
        if (text == QStringLiteral("$majors")) {
            return QStringLiteral("%1: $majors stands only as a hello's majors").arg(path);
        }
        if (text.startsWith(QStringLiteral("$uuid:"))) {
            return QStringLiteral("%1: %2 stands only in a client message").arg(path, text);
        }
        if (text == QStringLiteral("$object")) {
            return actual.isObject()
                       ? QString()
                       : QStringLiteral("%1: expected an object, got %2").arg(path, shown(actual));
        }
        if (text == QStringLiteral("$string") || text.startsWith(QStringLiteral("$string:"))) {
            if (!actual.isString()) {
                return QStringLiteral("%1: expected a string, got %2").arg(path, shown(actual));
            }
            const QString name = placeholderArgument(text, QStringLiteral("$string:"));
            if (!name.isEmpty()) {
                captures->insert(name, actual);
            }
            return QString();
        }
        if (text == QStringLiteral("$int") || text.startsWith(QStringLiteral("$int:"))) {
            NamedInt named;
            if (text != QStringLiteral("$int") && !parseNamedInt(text, &named)) {
                return QStringLiteral("%1: %2 is not $int:<name>[:<min>:<max>]").arg(path, text);
            }
            if (!isWholeNumber(actual)) {
                return QStringLiteral("%1: expected a whole number, got %2")
                    .arg(path, shown(actual));
            }
            if (named.ranged && (actual.toDouble() < named.min || actual.toDouble() > named.max)) {
                return QStringLiteral("%1: expected a whole number from %2 to %3, got %4")
                    .arg(path, text.section(QLatin1Char(':'), 2, 2),
                         text.section(QLatin1Char(':'), 3, 3), shown(actual));
            }
            if (!named.name.isEmpty()) {
                captures->insert(named.name, actual);
            }
            return QString();
        }
        if (text.startsWith(QStringLiteral("$within:"))) {
            double tolerance = 0.0;
            double centre = 0.0;
            if (!parseWithin(text, &tolerance, &centre)) {
                return QStringLiteral("%1: %2 is not $within:<tolerance>:<value>").arg(path, text);
            }
            if (!actual.isDouble() || std::abs(actual.toDouble() - centre) > tolerance) {
                const QStringList written = text.mid(8).split(QLatin1Char(':'));
                return QStringLiteral("%1: expected a number within %2 of %3, got %4")
                    .arg(path, written.at(0), written.at(1), shown(actual));
            }
            return QString();
        }
        const QString capture = placeholderArgument(text, QStringLiteral("$capture:"));
        if (!capture.isEmpty()) {
            captures->insert(capture, actual);
            return QString();
        }
        const QString ref = placeholderArgument(text, QStringLiteral("$ref:"));
        if (!captures->contains(ref)) {
            return QStringLiteral("%1: $ref:%2 names nothing captured").arg(path, ref);
        }
        return match(captures->value(ref), actual, captures, path);
    }

    if (isJsonForm(expected)) {
        // {"$json": <expectation>}: a string the station sends, holding
        // JSON, matched against the expectation (section 16.1). Object key
        // order inside it is the parser's business; array order matters.
        const QJsonObject form = expected.toObject();
        if (form.size() != 1) {
            return QStringLiteral("%1: {\"$json\": ...} stands alone in its object").arg(path);
        }
        if (actual.isUndefined()) {
            return QStringLiteral("%1: expected a string holding JSON, the key is absent")
                .arg(path);
        }
        if (!actual.isString()) {
            return QStringLiteral("%1: expected a string holding JSON, got %2")
                .arg(path, shown(actual));
        }
        QString why;
        const QJsonValue parsed = parseJsonText(actual.toString(), &why);
        if (!why.isEmpty()) {
            return QStringLiteral("%1: not JSON (%2), got %3").arg(path, why, shown(actual));
        }
        return match(form.value(QStringLiteral("$json")), parsed, captures,
                     path + QStringLiteral("($json)"));
    }

    if (expected.isObject()) {
        if (!actual.isObject()) {
            return QStringLiteral("%1: expected an object, got %2").arg(path, shown(actual));
        }
        const QJsonObject e = expected.toObject();
        const QJsonObject a = actual.toObject();
        for (auto it = e.constBegin(); it != e.constEnd(); ++it) {
            const QString child = path + QLatin1Char('.') + it.key();
            if (!a.contains(it.key())) {
                return QStringLiteral("%1: expected %2, the key is absent")
                    .arg(child, shown(it.value()));
            }
            if (it.value() == QJsonValue(QStringLiteral("$majors"))) {
                if (it.key() != QStringLiteral("majors")) {
                    return QStringLiteral("%1: $majors stands only as a hello's majors").arg(child);
                }
                const QString problem =
                    majorsProblem(a.value(it.key()), a.value(QStringLiteral("major")), child);
                if (!problem.isEmpty()) {
                    return problem;
                }
                continue;
            }
            const QString difference = match(it.value(), a.value(it.key()), captures, child);
            if (!difference.isEmpty()) {
                return difference;
            }
        }
        for (auto it = a.constBegin(); it != a.constEnd(); ++it) {
            if (!e.contains(it.key())) {
                return QStringLiteral("%1.%2: not expected, got %3")
                    .arg(path, it.key(), shown(it.value()));
            }
        }
        return QString();
    }

    if (expected.isArray()) {
        if (!actual.isArray()) {
            return QStringLiteral("%1: expected an array, got %2").arg(path, shown(actual));
        }
        const QJsonArray e = expected.toArray();
        const QJsonArray a = actual.toArray();
        const qsizetype common = std::min(e.size(), a.size());
        for (qsizetype i = 0; i < common; ++i) {
            const QString difference =
                match(e.at(i), a.at(i), captures, QStringLiteral("%1[%2]").arg(path).arg(i));
            if (!difference.isEmpty()) {
                return difference;
            }
        }
        if (e.size() != a.size()) {
            return QStringLiteral("%1: expected %2 elements, got %3; first extra: %4")
                .arg(path)
                .arg(e.size())
                .arg(a.size())
                .arg(e.size() > a.size() ? shown(e.at(common)) : shown(a.at(common)));
        }
        return QString();
    }

    if (expected.isDouble()) {
        return actual.isDouble() && actual.toDouble() == expected.toDouble()
                   ? QString()
                   : QStringLiteral("%1: expected %2, got %3")
                         .arg(path, shown(expected), shown(actual));
    }

    return expected == actual ? QString()
                              : QStringLiteral("%1: expected %2, got %3")
                                    .arg(path, shown(expected), shown(actual));
}

QJsonValue LinkFixtures::currentCoreStationExpectation(const QJsonValue& expected)
{
    if (!expected.isObject()) { return expected; }
    QJsonObject message = expected.toObject();
    if (message.value(QStringLiteral("type")) != QStringLiteral("hello")
        || !message.value(QStringLiteral("features")).isObject()) {
        return expected;
    }
    QJsonObject features = message.value(QStringLiteral("features")).toObject();
    if (features.value(QStringLiteral("deviceAuth")) != 1
        || features.contains(QStringLiteral("radioMic"))) {
        return expected;
    }
    features.insert(QStringLiteral("radioMic"), 2);
    message.insert(QStringLiteral("features"), features);
    return message;
}

QJsonValue LinkFixtures::substitute(const QJsonValue& value, Captures* captures,
                                    int* counter, QString* error)
{
    if (isPlaceholder(value)) {
        const QString text = value.toString();
        if (text == QStringLiteral("$string") || text.startsWith(QStringLiteral("$string:"))) {
            const QJsonValue filled(QStringLiteral("conformance"));
            const QString name = placeholderArgument(text, QStringLiteral("$string:"));
            if (!name.isEmpty()) {
                captures->insert(name, filled);
            }
            return filled;
        }
        if (text == QStringLiteral("$int")) {
            return QJsonValue(0);
        }
        if (text.startsWith(QStringLiteral("$int:"))) {
            NamedInt named;
            if (!parseNamedInt(text, &named)) {
                *error = QStringLiteral("%1 is not $int:<name>[:<min>:<max>]").arg(text);
                return {};
            }
            const QJsonValue filled(++*counter);
            if (named.ranged && (filled.toDouble() < named.min || filled.toDouble() > named.max)) {
                *error = QStringLiteral("%1: the counter's %2 is outside its range")
                             .arg(text).arg(*counter);
                return {};
            }
            captures->insert(named.name, filled);
            return filled;
        }
        if (text == QStringLiteral("$object")) {
            return QJsonObject{};
        }
        const QString uuid = placeholderArgument(text, QStringLiteral("$uuid:"));
        if (!uuid.isEmpty()) {
            // A media connection id is a canonical UUID the client makes
            // for each connection (the media document's start), so the
            // runner makes a new one each time, as a client does.
            const QJsonValue filled(QUuid::createUuid().toString(QUuid::WithoutBraces));
            captures->insert(uuid, filled);
            return filled;
        }
        const QString ref = placeholderArgument(text, QStringLiteral("$ref:"));
        if (ref.isEmpty()) {
            *error = QStringLiteral("%1 cannot stand in a client message").arg(text);
            return {};
        }
        if (!captures->contains(ref)) {
            *error = QStringLiteral("$ref:%1 names nothing captured").arg(ref);
            return {};
        }
        return captures->value(ref);
    }
    if (isJsonForm(value)) {
        // Only the station sends a {"$json": ...}; no runner fills one.
        *error = QStringLiteral("{\"$json\": ...} cannot stand in a client message");
        return {};
    }
    if (value.isObject()) {
        QJsonObject out;
        const QJsonObject in = value.toObject();
        for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
            if (it.value() == QJsonValue(QStringLiteral("$majors"))) {
                continue;  // Filled below, from the filled major.
            }
            out.insert(it.key(), substitute(it.value(), captures, counter, error));
            if (!error->isEmpty()) {
                return {};
            }
        }
        for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
            if (it.value() != QJsonValue(QStringLiteral("$majors"))) {
                continue;
            }
            if (it.key() != QStringLiteral("majors") || !isWholeNumber(out.value(QStringLiteral("major")))) {
                *error = QStringLiteral("$majors stands only as a hello's majors");
                return {};
            }
            // The sender's own list: the major it chose, alone.
            out.insert(it.key(), QJsonArray{out.value(QStringLiteral("major"))});
        }
        return out;
    }
    if (value.isArray()) {
        QJsonArray out;
        for (const QJsonValue& element : value.toArray()) {
            out.append(substitute(element, captures, counter, error));
            if (!error->isEmpty()) {
                return {};
            }
        }
        return out;
    }
    return value;
}

QString LinkFixtures::runControl(const QJsonObject& fixture)
{
    const QString problem = expectKeys(fixture,
                                       {QStringLiteral("from"), QStringLiteral("wire"),
                                        QStringLiteral("decodes")},
                                       {}, QStringLiteral("control fixture"));
    if (!problem.isEmpty()) {
        return problem;
    }
    const QString from = fixture.value(QStringLiteral("from")).toString();
    if (from != QStringLiteral("station") && from != QStringLiteral("client")) {
        return QStringLiteral("control fixture: from must be \"station\" or \"client\"");
    }
    if (!fixture.value(QStringLiteral("wire")).isObject()
        || !fixture.value(QStringLiteral("decodes")).isBool()) {
        return QStringLiteral("control fixture: wire must be an object and decodes a bool");
    }
    const QJsonObject wire = fixture.value(QStringLiteral("wire")).toObject();
    const bool decodes = fixture.value(QStringLiteral("decodes")).toBool();

    SessionMessage message;
    const bool decoded = SessionMessages::decode(
        QJsonDocument(wire).toJson(QJsonDocument::Compact), &message);
    if (decoded != decodes) {
        return decodes ? QStringLiteral("the station's decoder refused a message the fixture "
                                        "says decodes: %1")
                             .arg(shown(wire))
                       : QStringLiteral("the station's decoder accepted a message the fixture "
                                        "says it refuses: %1")
                             .arg(shown(wire));
    }
    if (!decoded) {
        return QString();
    }
    const QJsonObject again =
        QJsonDocument::fromJson(SessionMessages::encode(message)).object();
    Captures none;
    const QString difference = match(wire, again, &none);
    if (!difference.isEmpty()) {
        return QStringLiteral("encoding the decoded message again differs at %1")
            .arg(difference);
    }
    return QString();
}

bool LinkFixtures::runsOn(const QJsonObject& fixture, const QString& end)
{
    for (const QJsonValue& value : fixture.value(QStringLiteral("runs")).toArray()) {
        if (value.toString() == end) {
            return true;
        }
    }
    return false;
}

namespace {

// The phone's direct addresses: the Core's interfaces as a fixture gives
// them (stationSetup.coreInterfaces and the coreInterfaces step), each
// {"ip"} with an optional whole-number "prefix" and true or false
// "temporary" (a privacy address) and "deprecated" (a renumbered prefix's).
QString readCoreInterfaces(const QJsonValue& value, const QString& where,
                           QList<QNetworkAddressEntry>* entries)
{
    if (!value.isArray()) {
        return where + QStringLiteral(": coreInterfaces must be an array");
    }
    for (const QJsonValue& item : value.toArray()) {
        const QJsonObject object = item.toObject();
        const QString problem =
            expectKeys(object, {QStringLiteral("ip")},
                       {QStringLiteral("prefix"), QStringLiteral("temporary"),
                        QStringLiteral("deprecated")},
                       where + QStringLiteral(" coreInterfaces entry"));
        if (!problem.isEmpty()) {
            return problem;
        }
        const QHostAddress ip(object.value(QStringLiteral("ip")).toString());
        const QJsonValue prefix = object.value(QStringLiteral("prefix"));
        if (ip.isNull() || (!prefix.isUndefined() && !prefix.isDouble())
            || (object.contains(QStringLiteral("temporary"))
                && !object.value(QStringLiteral("temporary")).isBool())
            || (object.contains(QStringLiteral("deprecated"))
                && !object.value(QStringLiteral("deprecated")).isBool())) {
            return where + QStringLiteral(": each coreInterfaces entry needs an ip address, "
                                          "and if any a whole-number prefix and true or "
                                          "false temporary and deprecated");
        }
        QNetworkAddressEntry entry;
        entry.setIp(ip);
        if (prefix.isDouble()) {
            entry.setPrefixLength(prefix.toInt());
        }
        if (object.value(QStringLiteral("temporary")).toBool(false)) {
            entry.setDnsEligibility(QNetworkAddressEntry::DnsIneligible);
        }
        if (object.value(QStringLiteral("deprecated")).toBool(false)) {
            entry.setAddressLifetime(QDeadlineTimer(0), QDeadlineTimer::Forever);
        }
        if (entries != nullptr) {
            entries->append(entry);
        }
    }
    return QString();
}

QString checkCoreAddressSetup(const QJsonObject& setup)
{
    const QJsonValue listener = setup.value(QStringLiteral("coreListener"));
    const QJsonValue interfaces = setup.value(QStringLiteral("coreInterfaces"));
    if (listener.isUndefined() && interfaces.isUndefined()) {
        return QString();
    }
    const QJsonObject object = listener.toObject();
    const double port = object.value(QStringLiteral("port")).toDouble(-1.0);
    if (!listener.isObject()
        || !expectKeys(object, {QStringLiteral("address"), QStringLiteral("port")}, {},
                       QStringLiteral("stationSetup.coreListener"))
                .isEmpty()
        || QHostAddress(object.value(QStringLiteral("address")).toString()).isNull()
        || port < 1.0 || port > 65535.0 || port != static_cast<double>(static_cast<int>(port))) {
        return QStringLiteral("session fixture: stationSetup.coreListener needs an address and "
                              "a port from 1 to 65535, with coreInterfaces");
    }
    return readCoreInterfaces(interfaces, QStringLiteral("session fixture: stationSetup"),
                              nullptr);
}

} // namespace

QString LinkFixtures::checkSessionFormat(const QJsonObject& fixture)
{
    QString problem = expectKeys(fixture,
                                 {QStringLiteral("runs"), QStringLiteral("stationSetup"),
                                  QStringLiteral("steps")},
                                 {}, QStringLiteral("session fixture"));
    if (!problem.isEmpty()) {
        return problem;
    }
    const QJsonValue runs = fixture.value(QStringLiteral("runs"));
    QSet<QString> ends;
    for (const QJsonValue& value : runs.toArray()) {
        const QString end = value.toString();
        if ((end != QStringLiteral("station") && end != QStringLiteral("app"))
            || ends.contains(end)) {
            ends.clear();
            break;
        }
        ends.insert(end);
    }
    if (!runs.isArray() || ends.isEmpty() || ends.size() != runs.toArray().size()) {
        return QStringLiteral("session fixture: runs must list \"station\" and/or \"app\", "
                              "each once");
    }
    if (!fixture.value(QStringLiteral("stationSetup")).isObject()) {
        return QStringLiteral("session fixture: stationSetup must be an object");
    }
    // The phone's direct addresses: the Core's listener and interfaces.
    problem = checkCoreAddressSetup(fixture.value(QStringLiteral("stationSetup")).toObject());
    if (!problem.isEmpty()) {
        return problem;
    }
    const bool coreInterfacesSet =
        fixture.value(QStringLiteral("stationSetup")).toObject().contains(
            QStringLiteral("coreListener"));
    // iPhone app Task 71: the other clients a fixture names, by name.
    QSet<QString> others;
    const QJsonValue otherClients =
        fixture.value(QStringLiteral("stationSetup")).toObject().value(QStringLiteral("otherClients"));
    if (!otherClients.isUndefined()) {
        if (!otherClients.isArray()) {
            return QStringLiteral("session fixture: stationSetup.otherClients must be an array");
        }
        for (const QJsonValue& value : otherClients.toArray()) {
            const QJsonObject other = value.toObject();
            problem = expectKeys(other,
                                 {QStringLiteral("name"), QStringLiteral("device"),
                                  QStringLiteral("features")},
                                 {QStringLiteral("shortName")},
                                 QStringLiteral("stationSetup.otherClients entry"));
            if (!problem.isEmpty()) {
                return problem;
            }
            const QString name = other.value(QStringLiteral("name")).toString();
            const QJsonValue device = other.value(QStringLiteral("device"));
            if (name.isEmpty() || others.contains(name)
                || (!(device.isDouble() && device.toDouble() >= 1.0
                      && device.toDouble() == static_cast<double>(device.toInt()))
                    && device.toString() != QStringLiteral("self"))
                || !other.value(QStringLiteral("features")).isObject()
                || (other.contains(QStringLiteral("shortName"))
                    && !other.value(QStringLiteral("shortName")).isString())) {
                return QStringLiteral("session fixture: stationSetup.otherClients: each needs a "
                                      "unique name, a device (a whole number from 1, or "
                                      "\"self\"), features (an object) and, if any, a string "
                                      "shortName");
            }
            others.insert(name);
        }
    }
    const auto knownClient = [&others](const QJsonObject& step, const QString& key) {
        return !step.contains(key) || others.contains(step.value(key).toString());
    };
    const QJsonArray steps = fixture.value(QStringLiteral("steps")).toArray();
    if (steps.isEmpty()) {
        return QStringLiteral("session fixture: no steps");
    }
    // The runner's own connection opened later, by an openOwnConnection
    // step, instead of before the first step: a fixture that passes more
    // virtual time than the station's sign-in deadline before its own
    // client signs in (placeFreed, 180 s after a device left).
    const QJsonValue deferValue =
        fixture.value(QStringLiteral("stationSetup")).toObject()
            .value(QStringLiteral("deferOwnConnection"));
    if (!deferValue.isUndefined() && !deferValue.isBool()) {
        return QStringLiteral("session fixture: stationSetup.deferOwnConnection must be true "
                              "or false");
    }
    const bool deferOwn = deferValue.toBool(false);
    if (deferOwn && runsOn(fixture, QStringLiteral("app"))) {
        return QStringLiteral("session fixture: stationSetup.deferOwnConnection runs on the "
                              "station only");
    }
    bool ownOpen = !deferOwn;
    for (int index = 0; index < steps.size(); ++index) {
        const QString where = QStringLiteral("step %1").arg(index);
        if (!steps.at(index).isObject()) {
            return where + QStringLiteral(": not an object");
        }
        const QJsonObject step = steps.at(index).toObject();
        // A step of the runner's own client names no other client.
        const bool ownStep =
            (step.contains(QStringLiteral("from"))
             && !step.contains(QStringLiteral("client")) && !step.contains(QStringLiteral("to")))
            || (step.contains(QStringLiteral("expectClosed"))
                && !step.value(QStringLiteral("expectClosed")).toObject()
                        .contains(QStringLiteral("client")));
        if (ownStep && !ownOpen) {
            return where + QStringLiteral(": the runner's own client has no connection before "
                                          "the openOwnConnection step");
        }
        if (step.contains(QStringLiteral("openOwnConnection"))) {
            problem = expectKeys(step, {QStringLiteral("openOwnConnection")}, {}, where);
            if (problem.isEmpty()
                && (step.value(QStringLiteral("openOwnConnection")) != QJsonValue(true)
                    || !deferOwn || ownOpen)) {
                problem = where + QStringLiteral(": openOwnConnection must be true, once, in a "
                                                 "fixture whose stationSetup.deferOwnConnection "
                                                 "is true");
            }
            ownOpen = true;
        } else if (step.contains(QStringLiteral("from"))) {
            const QString from = step.value(QStringLiteral("from")).toString();
            if (from == QStringLiteral("client")) {
                problem = expectKeys(step,
                                     {QStringLiteral("from"), QStringLiteral("role"),
                                      QStringLiteral("message")},
                                     {QStringLiteral("client")}, where);
                const QString role = step.value(QStringLiteral("role")).toString();
                if (problem.isEmpty() && role != QStringLiteral("behaviour")
                    && role != QStringLiteral("scripted")) {
                    problem = where + QStringLiteral(": role must be \"behaviour\" or "
                                                     "\"scripted\"");
                }
                if (problem.isEmpty() && !knownClient(step, QStringLiteral("client"))) {
                    problem = where + QStringLiteral(": client names no other client");
                }
            } else if (from == QStringLiteral("station")) {
                problem = expectKeys(step, {QStringLiteral("from"), QStringLiteral("message")},
                                     {QStringLiteral("to")}, where);
                if (problem.isEmpty() && !knownClient(step, QStringLiteral("to"))) {
                    problem = where + QStringLiteral(": to names no other client");
                }
            } else {
                problem = where + QStringLiteral(": from must be \"station\" or \"client\"");
            }
            if (problem.isEmpty() && !step.value(QStringLiteral("message")).isObject()) {
                problem = where + QStringLiteral(": message must be an object");
            }
        } else if (step.contains(QStringLiteral("advanceMs"))) {
            problem = expectKeys(step, {QStringLiteral("advanceMs")}, {}, where);
            const double ms = step.value(QStringLiteral("advanceMs")).toDouble(-1.0);
            if (problem.isEmpty()
                && (ms < 0.0 || ms != static_cast<double>(static_cast<qint64>(ms)))) {
                problem = where + QStringLiteral(": advanceMs must be a whole number of at "
                                                 "least 0");
            }
        } else if (step.contains(QStringLiteral("expectClosed"))) {
            problem = expectKeys(step, {QStringLiteral("expectClosed")}, {}, where);
            if (problem.isEmpty()) {
                const QJsonObject closed = step.value(QStringLiteral("expectClosed")).toObject();
                problem = expectKeys(closed, {QStringLiteral("retryable")},
                                     {QStringLiteral("client")},
                                     where + QStringLiteral(" expectClosed"));
                if (problem.isEmpty() && !knownClient(closed, QStringLiteral("client"))) {
                    problem = where + QStringLiteral(" expectClosed: client names no other "
                                                     "client");
                }
            }
        } else if (step.contains(QStringLiteral("radioPtt"))) {
            // iPhone app plan Task 77: the radio's own PTT pressed (true) or
            // released (false) at the station. A station fixture only: an
            // app's runner has no radio to press.
            problem = expectKeys(step, {QStringLiteral("radioPtt")}, {}, where);
            if (problem.isEmpty() && !step.value(QStringLiteral("radioPtt")).isBool()) {
                problem = where + QStringLiteral(": radioPtt must be true or false");
            }
            if (problem.isEmpty() && runsOn(fixture, QStringLiteral("app"))) {
                problem = where + QStringLiteral(": a radioPtt step runs on the station only");
            }
        } else if (step.contains(QStringLiteral("coreInterfaces"))) {
            // The phone's direct addresses: the Core's interfaces change
            // (a renumbering). A station fixture only, after
            // stationSetup.coreListener.
            problem = expectKeys(step, {QStringLiteral("coreInterfaces")}, {}, where);
            if (problem.isEmpty()) {
                problem = readCoreInterfaces(step.value(QStringLiteral("coreInterfaces")), where,
                                             nullptr);
            }
            if (problem.isEmpty()
                && (!coreInterfacesSet || runsOn(fixture, QStringLiteral("app")))) {
                problem = where + QStringLiteral(": a coreInterfaces step runs on the station "
                                                 "only, after stationSetup.coreListener");
            }
        } else if (step.contains(QStringLiteral("connect")) || step.contains(QStringLiteral("close"))) {
            // iPhone app Task 71: another client's whole connect sequence,
            // or its close.
            const QString key = step.contains(QStringLiteral("connect")) ? QStringLiteral("connect")
                                                                         : QStringLiteral("close");
            problem = expectKeys(step, {key}, {}, where);
            if (problem.isEmpty()
                && (!step.value(key).isString()
                    || !others.contains(step.value(key).toString()))) {
                problem = where + QStringLiteral(": %1 must name one of "
                                                 "stationSetup.otherClients").arg(key);
            }
        } else {
            problem = where + QStringLiteral(": not a message, advanceMs, expectClosed, "
                                             "connect, close, radioPtt, coreInterfaces or "
                                             "openOwnConnection "
                                             "step");
        }
        if (!problem.isEmpty()) {
            return problem;
        }
    }
    if (!ownOpen) {
        return QStringLiteral("session fixture: stationSetup.deferOwnConnection is true and no "
                              "step opens the runner's own connection");
    }
    return QString();
}

namespace {

// iPhone app Task 12: the runner's own device for "$device:<case>" (the
// link document, section 16.1). Its key is made at run time in a scratch
// directory; stationSetup's "pairedDevice" puts it in the Core's paired
// devices before the client connects.
struct ConformanceDevice {
    QTemporaryDir dir;
    StationIdentity key = StationIdentity::loadOrCreate(dir.path());
};

bool mentionsDevice(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString().startsWith(QStringLiteral("$device:"));
    }
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        return std::any_of(object.begin(), object.end(),
                           [](const QJsonValue& v) { return mentionsDevice(v); });
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        return std::any_of(array.begin(), array.end(),
                           [](const QJsonValue& v) { return mentionsDevice(v); });
    }
    return false;
}

// Fills every "$device:<case>" with the device block the runner's device
// sends: "signed" signs this connection's transcript (the challenge the
// station's hello gave, recorded as "challenge"); "otherChallenge" signs one
// with a challenge of the runner's own; "otherCertificate" signs one that
// binds a certificate other than the station's.
QJsonValue fillDevice(const QJsonValue& value, const ConformanceDevice& device,
                      const StationServer& server, const LinkFixtures::Captures& captures,
                      QString* error)
{
    if (value.isString() && value.toString().startsWith(QStringLiteral("$device:"))) {
        const QString which = value.toString().mid(8);
        if (!captures.contains(QStringLiteral("challenge"))) {
            *error = QStringLiteral("$device needs the station's challenge recorded as "
                                    "\"challenge\"");
            return {};
        }
        bool ok = false;
        QByteArray challenge = StationIdentity::fromBase64Url(
            captures.value(QStringLiteral("challenge")).toString(), &ok);
        QString pin = server.certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        QByteArray certSha256 = QByteArray::fromHex(pin.toLatin1());
        if (which == QStringLiteral("otherChallenge")) {
            quint32 words[DeviceAuthenticator::kChallengeBytes / sizeof(quint32)]{};
            QRandomGenerator::system()->fillRange(words);
            challenge = QByteArray(reinterpret_cast<const char*>(words), sizeof(words));
        } else if (which == QStringLiteral("otherCertificate")) {
            certSha256 = StationIdentity::fingerprintOf(QByteArrayLiteral("another certificate"));
        } else if (which != QStringLiteral("signed")) {
            *error = QStringLiteral("%1 is not $device:signed, $device:otherChallenge or "
                                    "$device:otherCertificate").arg(value.toString());
            return {};
        }
        if (!ok || !device.key.isValid()) {
            *error = QStringLiteral("$device: no usable challenge or device key");
            return {};
        }
        const QByteArray transcript = DeviceAuthenticator::transcript(
            challenge, certSha256, server.stationIdentity().publicKeySpki(),
            device.key.publicKeySpki());
        return QJsonObject{
            {QStringLiteral("id"), StationIdentity::toBase64Url(device.key.fingerprint())},
            {QStringLiteral("publicKey"), StationIdentity::toBase64Url(device.key.publicKeySpki())},
            {QStringLiteral("name"), QStringLiteral("Conformance device")},
            {QStringLiteral("kind"), QStringLiteral("phone")},
            {QStringLiteral("signature"),
             StationIdentity::toBase64Url(device.key.sign(transcript))},
            // Part C fix wave: every sign-in the station's runner makes
            // carries a short name, as the app's does.
            {QStringLiteral("shortName"), QStringLiteral("Conformance")},
        };
    }
    if (value.isObject()) {
        QJsonObject out;
        const QJsonObject in = value.toObject();
        for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
            out.insert(it.key(), fillDevice(it.value(), device, server, captures, error));
        }
        return out;
    }
    return value;
}

} // namespace

namespace {

// iPhone app Task 71: one client the player plays. The fixture's own client
// is the caller's transport; each of stationSetup's otherClients is made
// here when its {"connect"} step comes. Each client's station messages are
// matched in its own arrival order.
struct PlayedClient {
    QString name;
    LoopbackTransport* transport = nullptr;
    std::unique_ptr<LoopbackTransport> owned;
    int consumed = 0;
    bool lastRetryable = false;
    bool sawEnding = false;
    // For an other client: its device (the runner's own for "self") and
    // what its hello and sign-in carry.
    const ConformanceDevice* device = nullptr;
    QString deviceName;
    QString deviceKind;
    QString shortName;
    QHash<QByteArray, int> features;
};

} // namespace

void LinkFixtures::setSettleCheck(std::function<bool()> settled)
{
    settleCheck() = std::move(settled);
}

void LinkFixtures::setConnectClient(
    std::function<QString(LoopbackTransport* client, StationServer& server)> connect)
{
    connectClientHook() = std::move(connect);
}

namespace {

// The most bytes a framing fixture's message may hold: the desktop's cap
// and one more, for the fixture that passes it.
constexpr qint64 kMaxFramingMessageBytes =
    static_cast<qint64>(StationClient::kMaxIncomingMessageBytes) + 1;

// A framing fixture's message: its pieces joined.
QString framingMessage(const QJsonArray& pieces, QByteArray* message)
{
    message->clear();
    if (pieces.isEmpty()) {
        return QStringLiteral("message: no pieces");
    }
    for (int i = 0; i < pieces.size(); ++i) {
        const QString where = QStringLiteral("message[%1]").arg(i);
        if (!pieces.at(i).isObject()) {
            return where + QStringLiteral(": not an object");
        }
        const QJsonObject piece = pieces.at(i).toObject();
        QByteArray bytes;
        qint64 times = 1;
        if (piece.contains(QStringLiteral("text"))) {
            const QString problem = expectKeys(piece, {QStringLiteral("text")}, {}, where);
            if (!problem.isEmpty()) {
                return problem;
            }
            bytes = piece.value(QStringLiteral("text")).toString().toUtf8();
        } else {
            const QString problem = expectKeys(
                piece, {QStringLiteral("repeat"), QStringLiteral("times")}, {}, where);
            if (!problem.isEmpty()) {
                return problem;
            }
            bytes = piece.value(QStringLiteral("repeat")).toString().toUtf8();
            const double value = piece.value(QStringLiteral("times")).toDouble(-1.0);
            if (value < 1.0 || value != static_cast<double>(static_cast<qint64>(value))) {
                return where + QStringLiteral(": times must be a whole number of at least 1");
            }
            times = static_cast<qint64>(value);
        }
        if (bytes.isEmpty()) {
            return where + QStringLiteral(": an empty piece");
        }
        if (message->size() + bytes.size() * times > kMaxFramingMessageBytes) {
            return where + QStringLiteral(": the message is longer than a fixture may hold");
        }
        for (qint64 n = 0; n < times; ++n) {
            message->append(bytes);
        }
    }
    return QString();
}

// A framing fixture's frames, over its message.
QString framingFrames(const QJsonArray& frames, const QByteArray& message, QList<QByteArray>* out,
                      bool* allChunks)
{
    out->clear();
    *allChunks = true;
    if (frames.isEmpty()) {
        return QStringLiteral("frames: none");
    }
    for (int i = 0; i < frames.size(); ++i) {
        const QString where = QStringLiteral("frames[%1]").arg(i);
        if (!frames.at(i).isObject()) {
            return where + QStringLiteral(": not an object");
        }
        const QJsonObject frame = frames.at(i).toObject();
        if (frame.contains(QStringLiteral("hex"))) {
            const QString problem = expectKeys(frame, {QStringLiteral("hex")}, {}, where);
            if (!problem.isEmpty()) {
                return problem;
            }
            const QString hex = frame.value(QStringLiteral("hex")).toString();
            static const QRegularExpression lowerHex(QStringLiteral("^([0-9a-f]{2})*$"));
            if (!frame.value(QStringLiteral("hex")).isString() || !lowerHex.match(hex).hasMatch()) {
                return where + QStringLiteral(": hex must be pairs of lower-case hex digits");
            }
            out->append(QByteArray::fromHex(hex.toLatin1()));
            *allChunks = false;
            continue;
        }
        const QString problem = expectKeys(
            frame, {QStringLiteral("chunk"), QStringLiteral("from"), QStringLiteral("length")}, {},
            where);
        if (!problem.isEmpty()) {
            return problem;
        }
        const QString chunk = frame.value(QStringLiteral("chunk")).toString();
        if (chunk != QStringLiteral("more") && chunk != QStringLiteral("last")) {
            return where + QStringLiteral(": chunk must be \"more\" or \"last\"");
        }
        const double from = frame.value(QStringLiteral("from")).toDouble(-1.0);
        const double length = frame.value(QStringLiteral("length")).toDouble(-1.0);
        if (from < 0.0 || length < 0.0 || from != static_cast<double>(static_cast<qint64>(from))
            || length != static_cast<double>(static_cast<qint64>(length))
            || from + length > static_cast<double>(message.size())) {
            return where + QStringLiteral(": from and length must be whole numbers within the "
                                          "message");
        }
        QByteArray bytes(1, static_cast<char>(chunk == QStringLiteral("more")
                                                  ? ControlFraming::kChunkMore
                                                  : ControlFraming::kChunkLast));
        bytes.append(message.mid(static_cast<qsizetype>(from), static_cast<qsizetype>(length)));
        out->append(bytes);
    }
    return QString();
}

} // namespace

QString LinkFixtures::runFraming(const QJsonObject& fixture)
{
    QString problem = expectKeys(fixture,
                                 {QStringLiteral("receiver"), QStringLiteral("message"),
                                  QStringLiteral("frames"), QStringLiteral("encodes"),
                                  QStringLiteral("outcome"), QStringLiteral("replies")},
                                 {}, QStringLiteral("framing fixture"));
    if (!problem.isEmpty()) {
        return problem;
    }
    const QString receiver = fixture.value(QStringLiteral("receiver")).toString();
    if (receiver != QStringLiteral("station") && receiver != QStringLiteral("client")) {
        return QStringLiteral("receiver must be \"station\" or \"client\"");
    }
    const QString outcome = fixture.value(QStringLiteral("outcome")).toString();
    if (outcome != QStringLiteral("delivered") && outcome != QStringLiteral("refused")) {
        return QStringLiteral("outcome must be \"delivered\" or \"refused\"");
    }
    if (!fixture.value(QStringLiteral("encodes")).isBool()) {
        return QStringLiteral("encodes must be true or false");
    }
    QByteArray message;
    problem = framingMessage(fixture.value(QStringLiteral("message")).toArray(), &message);
    if (!problem.isEmpty()) {
        return problem;
    }
    QList<QByteArray> frames;
    bool allChunks = true;
    problem = framingFrames(fixture.value(QStringLiteral("frames")).toArray(), message, &frames,
                            &allChunks);
    if (!problem.isEmpty()) {
        return problem;
    }
    QList<QByteArray> replies;
    for (const QJsonValue& value : fixture.value(QStringLiteral("replies")).toArray()) {
        replies.append(QByteArray::fromHex(value.toString().toLatin1()));
    }

    // The sender: the fixture's frames are exactly what cutting the message
    // makes.
    if (fixture.value(QStringLiteral("encodes")).toBool()) {
        if (!allChunks) {
            return QStringLiteral("a fixture that encodes holds chunks only");
        }
        const QList<QByteArray> made = ControlFraming::chunk(message);
        if (made.size() != frames.size()) {
            return QStringLiteral("sender: %1 chunks, the fixture has %2")
                .arg(made.size())
                .arg(frames.size());
        }
        for (int i = 0; i < made.size(); ++i) {
            if (made.at(i) != frames.at(i)) {
                return QStringLiteral("sender: chunk %1 differs (%2 bytes, the fixture's %3)")
                    .arg(i)
                    .arg(made.at(i).size())
                    .arg(frames.at(i).size());
            }
        }
    }

    // The receiver.
    ControlFraming::Reassembler joiner(receiver == QStringLiteral("station")
                                           ? StationServer::kMaxIncomingMessageBytes
                                           : StationClient::kMaxIncomingMessageBytes);
    QList<QByteArray> delivered;
    QList<QByteArray> answered;
    bool refused = false;
    int refusedAt = -1;
    for (int i = 0; i < frames.size() && !refused; ++i) {
        switch (joiner.feed(frames.at(i))) {
        case ControlFraming::Reassembler::Result::Pending:
        case ControlFraming::Reassembler::Result::Pong:
            break;
        case ControlFraming::Reassembler::Result::Message:
            delivered.append(joiner.message());
            break;
        case ControlFraming::Reassembler::Result::Ping:
            answered.append(ControlFraming::pong(joiner.id()));
            break;
        case ControlFraming::Reassembler::Result::Refused:
            refused = true;
            refusedAt = i;
            break;
        }
    }
    if (answered != replies) {
        return QStringLiteral("receiver: answered %1 pings, the fixture expects %2 replies (or "
                              "they differ)")
            .arg(answered.size())
            .arg(replies.size());
    }
    if (outcome == QStringLiteral("refused")) {
        if (!refused) {
            return QStringLiteral("receiver: the connection was not ended");
        }
        if (!delivered.isEmpty()) {
            return QStringLiteral("receiver: delivered a message before ending the connection");
        }
        return QString();
    }
    if (refused) {
        return QStringLiteral("receiver: ended the connection at frame %1 (%2)")
            .arg(refusedAt)
            .arg(joiner.reason());
    }
    if (delivered.size() != 1 || delivered.first() != message) {
        return QStringLiteral("receiver: delivered %1 messages, not the fixture's one")
            .arg(delivered.size());
    }
    if (joiner.pendingBytes() != 0) {
        return QStringLiteral("receiver: bytes left over after the last chunk");
    }
    return QString();
}

QString LinkFixtures::runSession(const QJsonObject& fixture, StationServer& server,
                                 LoopbackTransport& transport)
{
    QString problem = checkSessionFormat(fixture);
    if (!problem.isEmpty()) {
        return problem;
    }
    if (!runsOn(fixture, QStringLiteral("station"))) {
        return QStringLiteral("session fixture: \"runs\" does not name the station");
    }
    const QJsonObject setup = fixture.value(QStringLiteral("stationSetup")).toObject();
    const QJsonArray steps = fixture.value(QStringLiteral("steps")).toArray();
    if (steps.isEmpty()) {
        return QStringLiteral("session fixture: no steps");
    }

    transport.setAnswersPings(setup.value(QStringLiteral("clientAnswersPings")).toBool(true));
    // stationSetup.deferOwnConnection: the caller has not connected the
    // runner's own client; the openOwnConnection step does.
    bool ownConnected = !setup.value(QStringLiteral("deferOwnConnection")).toBool(false);

    Captures captures;
    captures.insert(QStringLiteral("token"), server.token());
    int counter = 0;

    // iPhone app Task 12: the runner's device, when the fixture uses one.
    std::unique_ptr<ConformanceDevice> device;
    const bool paired = setup.value(QStringLiteral("pairedDevice")).toBool(false);
    const QJsonArray otherClientsSetup = setup.value(QStringLiteral("otherClients")).toArray();
    bool selfClient = false;
    for (const QJsonValue& other : otherClientsSetup) {
        selfClient = selfClient
            || other.toObject().value(QStringLiteral("device")).toString() == QStringLiteral("self");
    }
    if (paired || selfClient || mentionsDevice(QJsonValue(steps))) {
        device = std::make_unique<ConformanceDevice>();
        if (!device->key.isValid()) {
            return QStringLiteral("the runner's device key could not be made");
        }
    }
    if (paired) {
        PairedDevice record;
        record.id = device->key.fingerprint();
        record.publicKeySpki = device->key.publicKeySpki();
        record.name = QStringLiteral("Conformance device");
        record.kind = QStringLiteral("phone");
        if (server.deviceStore() == nullptr || !server.deviceStore()->add(record)) {
            return QStringLiteral("stationSetup.pairedDevice: the device could not be paired");
        }
        captures.insert(QStringLiteral("device:self"),
                        StationIdentity::toBase64Url(device->key.fingerprint()));
    }
    // iPhone app Task 13: devices besides the runner's own, paired before
    // the client connects, their keys made here and their ids recorded as
    // "device:1" to "device:<n>" in the order they were paired.
    std::vector<std::unique_ptr<ConformanceDevice>> otherDevices;
    const QJsonValue othersValue = setup.value(QStringLiteral("otherPairedDevices"));
    const double othersCount = othersValue.toDouble(0.0);
    if (!othersValue.isUndefined()
        && (!othersValue.isDouble() || othersCount < 0.0 || othersCount > 16.0
            || othersCount != static_cast<double>(static_cast<int>(othersCount)))) {
        return QStringLiteral("stationSetup.otherPairedDevices must be a whole number from 0 "
                              "to 16");
    }
    for (int n = 1; n <= static_cast<int>(othersCount); ++n) {
        auto other = std::make_unique<ConformanceDevice>();
        PairedDevice record;
        record.id = other->key.fingerprint();
        record.publicKeySpki = other->key.publicKeySpki();
        record.name = QStringLiteral("Other device %1").arg(n);
        record.kind = QStringLiteral("tablet");
        if (!other->key.isValid() || server.deviceStore() == nullptr
            || !server.deviceStore()->add(record)) {
            return QStringLiteral("stationSetup.otherPairedDevices: device %1 could not be "
                                  "paired").arg(n);
        }
        captures.insert(QStringLiteral("device:%1").arg(n),
                        StationIdentity::toBase64Url(record.id));
        otherDevices.push_back(std::move(other));
    }

    // The phone's direct addresses: the Core's listener and interfaces, in
    // place of this computer's, before the client connects.
    auto coreInterfaces = std::make_shared<QList<QNetworkAddressEntry>>();
    if (setup.contains(QStringLiteral("coreListener"))) {
        const QJsonObject listener = setup.value(QStringLiteral("coreListener")).toObject();
        problem = readCoreInterfaces(setup.value(QStringLiteral("coreInterfaces")),
                                     QStringLiteral("stationSetup"), coreInterfaces.get());
        if (!problem.isEmpty()) {
            return problem;
        }
        CoreAddressWatcher* watcher = server.coreAddressWatcher();
        if (watcher == nullptr) {
            return QStringLiteral("stationSetup.coreListener: the station has no address "
                                  "watcher");
        }
        watcher->setEntrySource([coreInterfaces]() { return *coreInterfaces; });
        // Read as the Core reads remote_bind: "::" is every address, both
        // families.
        watcher->start(DaemonConfig::listenAddressFor(
                           listener.value(QStringLiteral("address")).toString()),
                       static_cast<quint16>(listener.value(QStringLiteral("port")).toInt()));
    }

    // iPhone app Task 71: the clients the player plays, its own first.
    std::vector<std::unique_ptr<PlayedClient>> clients;
    {
        auto own = std::make_unique<PlayedClient>();
        own->transport = &transport;
        clients.push_back(std::move(own));
    }
    for (const QJsonValue& value : otherClientsSetup) {
        const QJsonObject other = value.toObject();
        auto client = std::make_unique<PlayedClient>();
        client->name = other.value(QStringLiteral("name")).toString();
        const QJsonValue which = other.value(QStringLiteral("device"));
        if (which.toString() == QStringLiteral("self")) {
            client->device = device.get();
            client->deviceName = QStringLiteral("Conformance device");
            client->deviceKind = QStringLiteral("phone");
        } else {
            const int n = which.toInt();
            if (n < 1 || n > static_cast<int>(otherDevices.size())) {
                return QStringLiteral("stationSetup.otherClients: %1 names device %2, which "
                                      "otherPairedDevices does not pair")
                    .arg(client->name)
                    .arg(n);
            }
            client->device = otherDevices.at(static_cast<size_t>(n - 1)).get();
            client->deviceName = QStringLiteral("Other device %1").arg(n);
            client->deviceKind = QStringLiteral("tablet");
        }
        client->shortName = other.value(QStringLiteral("shortName")).toString();
        const QJsonObject features = other.value(QStringLiteral("features")).toObject();
        for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
            client->features.insert(it.key().toUtf8(), it.value().toInt());
        }
        clients.push_back(std::move(client));
    }
    const auto clientNamed = [&clients](const QString& name) -> PlayedClient* {
        for (const auto& client : clients) {
            if (client->name == name) {
                return client.get();
            }
        }
        return nullptr;
    };

    // The station's timers fire on this clock only (LinkVirtualClock.h).
    LinkVirtualClock clock(&server, [] { drain(); });
    // iPhone app Task 73: a device's 180 s count on the same virtual time as
    // the timer that ends them, so an advanceMs of 180000 ends them.
    if (server.deviceSessions() != nullptr) {
        server.deviceSessions()->setClock([&clock]() { return clock.now(); });
    }

    const auto describe = [&steps](int index) {
        const QJsonObject step = steps.at(index).toObject();
        QString what;
        if (step.contains(QStringLiteral("from"))) {
            what = QStringLiteral("%1 %2")
                       .arg(step.value(QStringLiteral("from")).toString(),
                            step.value(QStringLiteral("message"))
                                .toObject()
                                .value(QStringLiteral("type"))
                                .toString());
            const QString who = step.value(QStringLiteral("client"))
                                    .toString(step.value(QStringLiteral("to")).toString());
            if (!who.isEmpty()) {
                what += QStringLiteral(" (%1)").arg(who);
            }
        } else if (step.contains(QStringLiteral("advanceMs"))) {
            what = QStringLiteral("advanceMs");
        } else if (step.contains(QStringLiteral("radioPtt"))) {
            what = QStringLiteral("radioPtt");
        } else if (step.contains(QStringLiteral("coreInterfaces"))) {
            what = QStringLiteral("coreInterfaces");
        } else if (step.contains(QStringLiteral("openOwnConnection"))) {
            what = QStringLiteral("openOwnConnection");
        } else if (step.contains(QStringLiteral("connect"))) {
            what = QStringLiteral("connect %1").arg(step.value(QStringLiteral("connect")).toString());
        } else if (step.contains(QStringLiteral("close"))) {
            what = QStringLiteral("close %1").arg(step.value(QStringLiteral("close")).toString());
        } else {
            what = QStringLiteral("expectClosed");
        }
        return QStringLiteral("step %1 (%2)").arg(index).arg(what);
    };

    const auto unconsumed = [](const PlayedClient& client) {
        QStringList kinds;
        const QList<QByteArray> received = client.transport->received();
        for (int i = client.consumed; i < received.size(); ++i) {
            kinds.append(QString::fromUtf8(received.at(i).left(kMaxShownChars)));
        }
        return kinds;
    };

    // The queued work between two awaited messages, which takes no virtual
    // time: the station's timers do not fire in real time meanwhile
    // (LinkVirtualClock::Hold).
    const auto settle = [&clock]() {
        const LinkVirtualClock::Hold hold(clock);
        drain();
    };

    // Waits, in real time, for `client` to have received more than it
    // consumed (work on another thread of the fake radio, or a delta the
    // 50 ms flush sends in real time).
    const auto waitForMessage = [&clock, &settle](const PlayedClient& client) {
        settle();
        if (client.transport->received().size() <= client.consumed) {
            const QDeadlineTimer deadline(kStationReplyWaitMs);
            while (client.transport->received().size() <= client.consumed
                   && !deadline.hasExpired()) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            }
        }
        clock.scan();
    };

    settle();
    clock.scan();

    for (int index = 0; index < steps.size(); ++index) {
        if (!steps.at(index).isObject()) {
            return QStringLiteral("step %1: not an object").arg(index);
        }
        const QJsonObject step = steps.at(index).toObject();

        if (step.contains(QStringLiteral("from"))) {
            const QString from = step.value(QStringLiteral("from")).toString();
            const QJsonValue message = step.value(QStringLiteral("message"));
            if (!message.isObject()) {
                return QStringLiteral("%1: message must be an object").arg(describe(index));
            }
            const QString who = step.value(from == QStringLiteral("client")
                                               ? QStringLiteral("client")
                                               : QStringLiteral("to"))
                                    .toString();
            PlayedClient* client = clientNamed(who);
            if (client == nullptr) {
                return QStringLiteral("%1: no such client").arg(describe(index));
            }
            if (from == QStringLiteral("client")) {
                QString error;
                // Behaviour or scripted, the station's runner sends the
                // message itself, placeholders filled (section 16.3).
                QJsonValue outgoing = message;
                if (device && client == clients.front().get()) {
                    outgoing = fillDevice(message, *device, server, captures, &error);
                    if (!error.isEmpty()) {
                        return QStringLiteral("%1: %2").arg(describe(index), error);
                    }
                }
                const QJsonValue sent = substitute(outgoing, &captures, &counter, &error);
                if (!error.isEmpty()) {
                    return QStringLiteral("%1: %2").arg(describe(index), error);
                }
                if (client->transport == nullptr || !client->transport->isOpen()) {
                    return QStringLiteral("%1: the link is already closed").arg(describe(index));
                }
                client->transport->sendText(
                    QJsonDocument(sent.toObject()).toJson(QJsonDocument::Compact));
                settle();
                clock.scan();
            } else if (from == QStringLiteral("station")) {
                if (client->transport == nullptr) {
                    return QStringLiteral("%1: that client has not connected").arg(describe(index));
                }
                waitForMessage(*client);
                const QList<QByteArray> received = client->transport->received();
                if (received.size() <= client->consumed) {
                    return QStringLiteral("%1: the station sent nothing more (the link is %2); "
                                          "expected %3")
                        .arg(describe(index),
                             client->transport->isOpen() ? QStringLiteral("open")
                                                         : QStringLiteral("closed"),
                             shown(message));
                }
                const QByteArray wire = received.at(client->consumed++);
                const QJsonDocument doc = QJsonDocument::fromJson(wire);
                if (!doc.isObject()) {
                    return QStringLiteral("%1: the station sent something that is not a JSON "
                                          "object: %2")
                        .arg(describe(index), QString::fromUtf8(wire.left(kMaxShownChars)));
                }
                const QJsonObject actual = doc.object();
                const QString difference = match(currentCoreStationExpectation(message), actual, &captures);
                if (!difference.isEmpty()) {
                    return QStringLiteral("%1: %2\n  the station sent: %3")
                        .arg(describe(index), difference, shown(actual));
                }
                const QString type = actual.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("session.end") || type == QStringLiteral("auth.result")) {
                    client->lastRetryable = actual.value(QStringLiteral("retryable")).toBool(false);
                    client->sawEnding = true;
                }
            } else {
                return QStringLiteral("step %1: from must be \"station\" or \"client\"").arg(index);
            }
        } else if (step.contains(QStringLiteral("advanceMs"))) {
            problem = expectKeys(step, {QStringLiteral("advanceMs")}, {},
                                 QStringLiteral("step %1").arg(index));
            const double ms = step.value(QStringLiteral("advanceMs")).toDouble(-1.0);
            if (!problem.isEmpty() || ms < 0.0 || ms != static_cast<double>(static_cast<qint64>(ms))) {
                return QStringLiteral("step %1: advanceMs must be a whole number of at least 0")
                    .arg(index);
            }
            const QString error = clock.advance(static_cast<qint64>(ms));
            if (!error.isEmpty()) {
                return QStringLiteral("%1: %2").arg(describe(index), error);
            }
        } else if (step.contains(QStringLiteral("openOwnConnection"))) {
            // stationSetup.deferOwnConnection: the runner's own client
            // connects now, the way another client's connect step reaches
            // the station, and the fixture plays its hello and sign-in as
            // usual. Its sign-in deadline starts here.
            if (ownConnected) {
                return QStringLiteral("%1: the runner's own connection is already open")
                    .arg(describe(index));
            }
            ownConnected = true;
            {
                const LinkVirtualClock::Hold connecting(clock);
                if (connectClientHook()) {
                    const QString failure = connectClientHook()(&transport, server);
                    if (!failure.isEmpty()) {
                        return QStringLiteral("%1: %2").arg(describe(index), failure);
                    }
                } else {
                    auto* stationEnd =
                        new LoopbackTransport(QStringLiteral("conformance"), &server);
                    stationEnd->linkTo(&transport);
                    server.acceptTransport(stationEnd);
                }
            }
            settle();
            clock.scan();
        } else if (step.contains(QStringLiteral("radioPtt"))) {
            // iPhone app plan Task 77: the radio's own PTT level, as its
            // status frames report it.
            RadioModel* model = server.radioModel();
            if (model == nullptr || model->moxController() == nullptr) {
                return QStringLiteral("%1: the station has no radio to press").arg(describe(index));
            }
            model->moxController()->onMicPttFromRadio(step.value(QStringLiteral("radioPtt")).toBool());
            settle();
            clock.scan();
        } else if (step.contains(QStringLiteral("coreInterfaces"))) {
            // The phone's direct addresses: the Core's interfaces change,
            // and it reads them again as its timer would.
            QList<QNetworkAddressEntry> changed;
            problem = readCoreInterfaces(step.value(QStringLiteral("coreInterfaces")),
                                         describe(index), &changed);
            if (!problem.isEmpty()) {
                return problem;
            }
            *coreInterfaces = changed;
            server.coreAddressWatcher()->refresh();
            settle();
            clock.scan();
        } else if (step.contains(QStringLiteral("connect"))) {
            // iPhone app Task 71: another client's whole connect sequence;
            // its messages up to snapshot.complete are taken unmatched.
            PlayedClient* client = clientNamed(step.value(QStringLiteral("connect")).toString());
            if (client == nullptr || client->transport != nullptr || client->device == nullptr) {
                return QStringLiteral("%1: that client cannot connect").arg(describe(index));
            }
            // A connect takes no virtual time, however long its handshakes
            // take in real time (over a data channel, DTLS and SCTP).
            const LinkVirtualClock::Hold connecting(clock);
            client->owned = std::make_unique<LoopbackTransport>(
                QStringLiteral("conformance-%1-client").arg(client->name));
            client->transport = client->owned.get();
            if (connectClientHook()) {
                // Task 28: over a control channel of its own. A channel that
                // did not open says so here, with its stage, rather than as
                // a missing greeting after the reply wait.
                const QString failure = connectClientHook()(client->transport, server);
                if (!failure.isEmpty()) {
                    return QStringLiteral("%1: %2").arg(describe(index), failure);
                }
            } else {
                auto* stationEnd = new LoopbackTransport(
                    QStringLiteral("conformance-%1").arg(client->name), &server);
                stationEnd->linkTo(client->transport);
                server.acceptTransport(stationEnd);
            }
            waitForMessage(*client);
            const QJsonObject hello =
                QJsonDocument::fromJson(client->transport->received().value(0)).object();
            bool ok = false;
            const QByteArray challenge = StationIdentity::fromBase64Url(
                hello.value(QStringLiteral("challenge")).toString(), &ok);
            if (!ok || hello.value(QStringLiteral("type")).toString() != QStringLiteral("hello")) {
                return QStringLiteral("%1: the station did not greet that client").arg(describe(index));
            }
            QString pin = server.certificateFingerprint();
            pin.remove(QLatin1Char(':'));
            const ConformanceDevice& key = *client->device;
            const SessionDeviceBlock block{
                StationIdentity::toBase64Url(key.key.fingerprint()),
                StationIdentity::toBase64Url(key.key.publicKeySpki()), client->deviceName,
                client->deviceKind,
                StationIdentity::toBase64Url(key.key.sign(DeviceAuthenticator::transcript(
                    challenge, QByteArray::fromHex(pin.toLatin1()),
                    server.stationIdentity().publicKeySpki(), key.key.publicKeySpki()))),
                client->shortName};
            const QList<quint16> majors = server.supportedMajors();
            client->transport->sendText(SessionMessages::encode(SessionMessages::hello(
                majors.last(), kSessionProtocolMinor, 0,
                QStringLiteral("conformance-%1").arg(client->name), {majors.last()},
                client->features)));
            client->transport->sendText(
                SessionMessages::encode(SessionMessages::authRequest(QString(), block)));
            const QDeadlineTimer deadline(kStationReplyWaitMs);
            while (!client->transport->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"))
                   && client->transport->isOpen() && !deadline.hasExpired()) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            }
            clock.scan();
            const int complete =
                client->transport->receivedKinds().indexOf(QByteArrayLiteral("snapshot.complete"));
            if (complete < 0) {
                return QStringLiteral("%1: that client did not finish connecting (the link is "
                                      "%2)")
                    .arg(describe(index),
                         client->transport->isOpen() ? QStringLiteral("open")
                                                     : QStringLiteral("closed"));
            }
            client->consumed = complete + 1;
        } else if (step.contains(QStringLiteral("close"))) {
            PlayedClient* client = clientNamed(step.value(QStringLiteral("close")).toString());
            if (client == nullptr || client->transport == nullptr) {
                return QStringLiteral("%1: that client has not connected").arg(describe(index));
            }
            client->transport->closeLink(QStringLiteral("conformance close"));
            settle();
            clock.scan();
        } else if (step.contains(QStringLiteral("expectClosed"))) {
            problem = expectKeys(step, {QStringLiteral("expectClosed")}, {},
                                 QStringLiteral("step %1").arg(index));
            const QJsonObject closed = step.value(QStringLiteral("expectClosed")).toObject();
            if (problem.isEmpty()) {
                problem = expectKeys(closed, {QStringLiteral("retryable")},
                                     {QStringLiteral("client")},
                                     QStringLiteral("step %1 expectClosed").arg(index));
            }
            if (!problem.isEmpty()) {
                return problem;
            }
            PlayedClient* client = clientNamed(closed.value(QStringLiteral("client")).toString());
            if (client == nullptr || client->transport == nullptr) {
                return QStringLiteral("%1: that client has not connected").arg(describe(index));
            }
            settle();
            if (client->transport->isOpen()) {
                const QDeadlineTimer deadline(kStationReplyWaitMs);
                while (client->transport->isOpen() && !deadline.hasExpired()) {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                }
            }
            if (client->transport->isOpen()) {
                return QStringLiteral("%1: the station did not close the link").arg(describe(index));
            }
            const QStringList left = unconsumed(*client);
            if (!left.isEmpty()) {
                return QStringLiteral("%1: the station sent %2 message(s) the fixture does not "
                                      "list before closing, first: %3")
                    .arg(describe(index))
                    .arg(left.size())
                    .arg(left.first());
            }
            if (!client->sawEnding) {
                return QStringLiteral("%1: the station closed without a session.end or "
                                      "auth.result")
                    .arg(describe(index));
            }
            const bool retryable = closed.value(QStringLiteral("retryable")).toBool();
            if (retryable != client->lastRetryable) {
                return QStringLiteral("%1: expected retryable %2, the station said %3")
                    .arg(describe(index))
                    .arg(retryable ? QStringLiteral("true") : QStringLiteral("false"))
                    .arg(client->lastRetryable ? QStringLiteral("true") : QStringLiteral("false"));
            }
        } else {
            return QStringLiteral("step %1: not a message, advanceMs, expectClosed, connect, "
                                  "close, radioPtt or openOwnConnection step")
                .arg(index);
        }
    }

    const QJsonObject last = steps.last().toObject();
    if (!last.contains(QStringLiteral("expectClosed"))
        || last.value(QStringLiteral("expectClosed")).toObject().contains(QStringLiteral("client"))) {
        settle();
        if (!transport.isOpen()) {
            return QStringLiteral("after the last step: the station closed the link; the "
                                  "fixture does not expect it");
        }
    }
    return QString();
}

// ── Media vectors ────────────────────────────────────────────────────────

namespace {

QJsonArray numbers(const std::vector<double>& values)
{
    QJsonArray out;
    for (const double value : values) {
        out.append(value);
    }
    return out;
}

bool readNumbers(const QJsonObject& json, const QString& key, std::vector<double>* out,
                 QString* error)
{
    if (!json.value(key).isArray()) {
        *error = QStringLiteral("%1 must be an array of numbers").arg(key);
        return false;
    }
    out->clear();
    for (const QJsonValue& value : json.value(key).toArray()) {
        if (!value.isDouble()) {
            *error = QStringLiteral("%1 must be an array of numbers").arg(key);
            return false;
        }
        out->push_back(value.toDouble());
    }
    return true;
}

// A whole number that fits a double exactly (the JSON rule of the link
// document's section 4.1).
bool readWhole(const QJsonObject& json, const QString& key, qint64* out, QString* error)
{
    const QJsonValue value = json.value(key);
    const double d = value.toDouble();
    if (!value.isDouble() || d != static_cast<double>(static_cast<qint64>(d))) {
        *error = QStringLiteral("%1 must be a whole number").arg(key);
        return false;
    }
    *out = static_cast<qint64>(d);
    return true;
}

} // namespace

StationLanAnnouncement LinkMediaVectors::lanAnnouncement()
{
    StationLanAnnouncement value;
    value.controlPort = 50055;
    // A made-up certificate pin in the station's format: 32 bytes as
    // uppercase hex pairs joined by colons. Not any station's.
    QStringList pairs;
    for (int i = 0; i < 32; ++i) {
        pairs.append(QStringLiteral("%1").arg(0xA0 + i, 2, 16, QLatin1Char('0')).toUpper());
    }
    value.fingerprint = pairs.join(QLatin1Char(':'));
    value.coreName = QStringLiteral("Shack Core");
    value.radioName = QStringLiteral("Bench HL2");
    value.radioMac = QStringLiteral("AA:BB:CC:DD:EE:01");
    value.radioConnected = true;
    return value;
}

namespace {

// A made-up identity fingerprint, 32 bytes. Not any Core's.
QByteArray vectorIdentity()
{
    QByteArray identity;
    for (int i = 0; i < kStationLanIdentityBytes; ++i) {
        identity.append(static_cast<char>(0x10 + i * 7));
    }
    return identity;
}

QByteArray base64Url(const QByteArray& bytes)
{
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

} // namespace

StationLanAnnouncement LinkMediaVectors::lanAnnouncement2()
{
    StationLanAnnouncement value = lanAnnouncement();
    value.schema = kStationLanAnnouncementSchema2;
    value.claimed = true;
    value.identity = vectorIdentity();
    value.label = QStringLiteral("KG4VCF/shack");
    value.pairing = StationLanPairing::Code;
    return value;
}

StationLanAnnouncement LinkMediaVectors::lanAnnouncement2Devices()
{
    StationLanAnnouncement value = lanAnnouncement2();
    value.devicesConnected = 2;
    return value;
}

StationLanAnnouncement LinkMediaVectors::lanAnnouncement2Radio()
{
    StationLanAnnouncement value = lanAnnouncement2Devices();
    value.radio = StationLanRadio::Connected;
    return value;
}

StationLanAnnouncement LinkMediaVectors::lanAnnouncement2Waiting()
{
    StationLanAnnouncement value = lanAnnouncement2Devices();
    value.radioConnected = false;
    value.radioName.clear();
    value.radioMac = QStringLiteral("00:00:00:00:00:00");
    value.radio = StationLanRadio::Waiting;
    return value;
}

QByteArray LinkMediaVectors::lanAnnouncementTrailingBytes()
{
    // Shaped like a future field: a tag, a length and three bytes.
    return QByteArray("\x07\x03\x61\x62\x63", 5);
}

QJsonObject LinkMediaVectors::toJson(const StationLanAnnouncement& value)
{
    QJsonObject json{
        {QStringLiteral("schema"), int(value.schema)},
        {QStringLiteral("controlPort"), value.controlPort},
        {QStringLiteral("fingerprint"), value.fingerprint},
        {QStringLiteral("coreName"), value.coreName},
        {QStringLiteral("radioName"), value.radioName},
        {QStringLiteral("radioMac"), value.radioMac},
        {QStringLiteral("radioConnected"), value.radioConnected},
    };
    if (value.schema == kStationLanAnnouncementSchema2) {
        json.insert(QStringLiteral("claimed"), value.claimed);
        json.insert(QStringLiteral("identity"), QString::fromLatin1(base64Url(value.identity)));
        json.insert(QStringLiteral("label"), value.label);
        json.insert(QStringLiteral("pairing"), stationLanPairingName(value.pairing));
        if (value.devicesConnected) {
            json.insert(QStringLiteral("devicesConnected"), *value.devicesConnected);
        }
        if (value.radio) {
            json.insert(QStringLiteral("radio"), stationLanRadioName(*value.radio));
        }
    }
    return json;
}

bool LinkMediaVectors::fromJson(const QJsonObject& json, StationLanAnnouncement* value,
                                QString* error)
{
    const QStringList schemaOne{QStringLiteral("schema"), QStringLiteral("controlPort"),
                                QStringLiteral("fingerprint"), QStringLiteral("coreName"),
                                QStringLiteral("radioName"), QStringLiteral("radioMac"),
                                QStringLiteral("radioConnected")};
    const QStringList schemaTwo{QStringLiteral("claimed"), QStringLiteral("identity"),
                                QStringLiteral("label"), QStringLiteral("pairing")};
    const int schema = json.value(QStringLiteral("schema")).toInt(-1);
    const QString problem = expectKeys(
        json, schema == kStationLanAnnouncementSchema2 ? schemaOne + schemaTwo : schemaOne,
        schema == kStationLanAnnouncementSchema2
            ? QStringList{QStringLiteral("devicesConnected"), QStringLiteral("radio")}
            : QStringList{},
        QStringLiteral("announcement expect"));
    if (!problem.isEmpty()) {
        *error = problem;
        return false;
    }
    qint64 port = 0;
    if (!readWhole(json, QStringLiteral("controlPort"), &port, error) || port < 0 || port > 65535
        || !json.value(QStringLiteral("radioConnected")).isBool()
        || (schema != kStationLanAnnouncementSchema1 && schema != kStationLanAnnouncementSchema2)) {
        if (error->isEmpty()) {
            *error = QStringLiteral("schema, controlPort or radioConnected is out of range");
        }
        return false;
    }
    value->schema = static_cast<quint8>(schema);
    value->controlPort = static_cast<quint16>(port);
    value->fingerprint = json.value(QStringLiteral("fingerprint")).toString();
    value->coreName = json.value(QStringLiteral("coreName")).toString();
    value->radioName = json.value(QStringLiteral("radioName")).toString();
    value->radioMac = json.value(QStringLiteral("radioMac")).toString();
    value->radioConnected = json.value(QStringLiteral("radioConnected")).toBool();
    if (schema == kStationLanAnnouncementSchema2) {
        const auto pairing =
            stationLanPairingFromName(json.value(QStringLiteral("pairing")).toString());
        const auto identity = QByteArray::fromBase64Encoding(
            json.value(QStringLiteral("identity")).toString().toLatin1(),
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals
                | QByteArray::AbortOnBase64DecodingErrors);
        if (!json.value(QStringLiteral("claimed")).isBool() || !pairing || !identity) {
            *error = QStringLiteral("claimed, identity or pairing is not readable");
            return false;
        }
        value->claimed = json.value(QStringLiteral("claimed")).toBool();
        value->identity = *identity;
        value->label = json.value(QStringLiteral("label")).toString();
        value->pairing = *pairing;
        // iPhone app Task 71: absent is a Core from before the count.
        value->devicesConnected.reset();
        if (json.contains(QStringLiteral("devicesConnected"))) {
            qint64 count = 0;
            if (!readWhole(json, QStringLiteral("devicesConnected"), &count, error)) {
                return false;
            }
            value->devicesConnected = static_cast<int>(count);
        }
        // iPhone app plan Task 25: absent is a Core from before the state.
        value->radio.reset();
        if (json.contains(QStringLiteral("radio"))) {
            const auto radio =
                stationLanRadioFromName(json.value(QStringLiteral("radio")).toString());
            if (!radio) {
                *error = QStringLiteral("radio is not offline, connected or waiting");
                return false;
            }
            value->radio = *radio;
        }
    }
    return true;
}

DnsSdRecord LinkMediaVectors::dnsSdRecord()
{
    const StationLanAnnouncement announcement = lanAnnouncement2Radio();
    DnsSdRecord record;
    record.instanceName = dnsSdInstanceName(announcement.displayName());
    record.label = announcement.label;
    record.identity = announcement.identity;
    record.claimed = announcement.claimed;
    record.pairing = announcement.pairing;
    record.devicesConnected = announcement.devicesConnected.value_or(0);
    record.radio = announcement.radio.value_or(StationLanRadio::Offline);
    return record;
}

QJsonObject LinkMediaVectors::toJson(const DnsSdRecord& record)
{
    QJsonObject txt;
    for (const auto& [key, value] : dnsSdTxtEntries(record)) {
        txt.insert(QString::fromLatin1(key), QString::fromLatin1(value));
    }
    return QJsonObject{{QStringLiteral("serviceType"), QString::fromLatin1(kDnsSdServiceType)},
                       {QStringLiteral("txt"), txt}};
}

Ps3Snapshot LinkMediaVectors::ps3Snapshot()
{
    // Eight measured points and four correction points. Every value is a
    // multiple of 1/8, so it reads the same in any JSON library.
    Ps3Snapshot snapshot;
    snapshot.channelId = 5;
    snapshot.sessionGeneration = 7;
    snapshot.sequence = 3;
    snapshot.capturedAtUnixMilliseconds = 1790000000000;
    snapshot.sampleCount = 8;
    snapshot.correctionCount = 4;
    for (int i = 0; i < snapshot.sampleCount; ++i) {
        snapshot.x.push_back(0.125 * i);
        snapshot.ym.push_back(0.5 + 0.0625 * i);
        // Written so the first value is +0.0: JSON cannot carry -0.0.
        snapshot.yc.push_back(0.25 * static_cast<double>(-i));
        snapshot.ys.push_back(1.0 - 0.125 * i);
    }
    for (int i = 0; i < snapshot.correctionCount; ++i) {
        snapshot.xmCorrection.push_back(0.25 * i);
        snapshot.ymCorrection.push_back(1.0 + 0.125 * i);
        snapshot.xaCorrection.push_back(0.25 * i);
        snapshot.yaCorrection.push_back(-2.5 + 0.5 * i);
    }
    snapshot.phaseReferenceDegrees = 12.5;
    return snapshot;
}

QJsonObject LinkMediaVectors::toJson(const Ps3Snapshot& snapshot)
{
    return QJsonObject{
        {QStringLiteral("channelId"), snapshot.channelId},
        {QStringLiteral("sessionGeneration"), static_cast<qint64>(snapshot.sessionGeneration)},
        {QStringLiteral("sequence"), static_cast<qint64>(snapshot.sequence)},
        {QStringLiteral("capturedAtUnixMilliseconds"),
         static_cast<qint64>(snapshot.capturedAtUnixMilliseconds)},
        {QStringLiteral("sampleCount"), snapshot.sampleCount},
        {QStringLiteral("correctionCount"), snapshot.correctionCount},
        {QStringLiteral("x"), numbers(snapshot.x)},
        {QStringLiteral("ym"), numbers(snapshot.ym)},
        {QStringLiteral("yc"), numbers(snapshot.yc)},
        {QStringLiteral("ys"), numbers(snapshot.ys)},
        {QStringLiteral("xmCorrection"), numbers(snapshot.xmCorrection)},
        {QStringLiteral("ymCorrection"), numbers(snapshot.ymCorrection)},
        {QStringLiteral("xaCorrection"), numbers(snapshot.xaCorrection)},
        {QStringLiteral("yaCorrection"), numbers(snapshot.yaCorrection)},
        {QStringLiteral("phaseReferenceDegrees"), snapshot.phaseReferenceDegrees},
    };
}

bool LinkMediaVectors::fromJson(const QJsonObject& json, Ps3Snapshot* snapshot, QString* error)
{
    const QStringList keys{
        QStringLiteral("channelId"),     QStringLiteral("sessionGeneration"),
        QStringLiteral("sequence"),      QStringLiteral("capturedAtUnixMilliseconds"),
        QStringLiteral("sampleCount"),   QStringLiteral("correctionCount"),
        QStringLiteral("x"),             QStringLiteral("ym"),
        QStringLiteral("yc"),            QStringLiteral("ys"),
        QStringLiteral("xmCorrection"),  QStringLiteral("ymCorrection"),
        QStringLiteral("xaCorrection"),  QStringLiteral("yaCorrection"),
        QStringLiteral("phaseReferenceDegrees"),
    };
    const QString problem = expectKeys(json, keys, {}, QStringLiteral("PS3D expect"));
    if (!problem.isEmpty()) {
        *error = problem;
        return false;
    }
    qint64 channel = 0;
    qint64 generation = 0;
    qint64 sequence = 0;
    qint64 captured = 0;
    qint64 samples = 0;
    qint64 corrections = 0;
    if (!readWhole(json, QStringLiteral("channelId"), &channel, error)
        || !readWhole(json, QStringLiteral("sessionGeneration"), &generation, error)
        || !readWhole(json, QStringLiteral("sequence"), &sequence, error)
        || !readWhole(json, QStringLiteral("capturedAtUnixMilliseconds"), &captured, error)
        || !readWhole(json, QStringLiteral("sampleCount"), &samples, error)
        || !readWhole(json, QStringLiteral("correctionCount"), &corrections, error)
        || !json.value(QStringLiteral("phaseReferenceDegrees")).isDouble()) {
        if (error->isEmpty()) {
            *error = QStringLiteral("phaseReferenceDegrees must be a number");
        }
        return false;
    }
    Ps3Snapshot out;
    out.channelId = static_cast<int>(channel);
    out.sessionGeneration = static_cast<std::uint64_t>(generation);
    out.sequence = static_cast<std::uint64_t>(sequence);
    out.capturedAtUnixMilliseconds = captured;
    out.sampleCount = static_cast<int>(samples);
    out.correctionCount = static_cast<int>(corrections);
    out.phaseReferenceDegrees = json.value(QStringLiteral("phaseReferenceDegrees")).toDouble();
    if (!readNumbers(json, QStringLiteral("x"), &out.x, error)
        || !readNumbers(json, QStringLiteral("ym"), &out.ym, error)
        || !readNumbers(json, QStringLiteral("yc"), &out.yc, error)
        || !readNumbers(json, QStringLiteral("ys"), &out.ys, error)
        || !readNumbers(json, QStringLiteral("xmCorrection"), &out.xmCorrection, error)
        || !readNumbers(json, QStringLiteral("ymCorrection"), &out.ymCorrection, error)
        || !readNumbers(json, QStringLiteral("xaCorrection"), &out.xaCorrection, error)
        || !readNumbers(json, QStringLiteral("yaCorrection"), &out.yaCorrection, error)) {
        return false;
    }
    *snapshot = out;
    return true;
}

QList<DisplayCodecFrame> LinkMediaVectors::nsdcFrames()
{
    // One endpoint, one context: 32 trace and 32 waterfall samples between
    // -140 and -40 dBm, no wide row. Each frame drifts the rows by 0.5 dB
    // so the deltas carry small residuals.
    DisplayCodecContext context;
    context.endpointId = 1;
    context.contextGeneration = 1;
    context.minDbm = -140.0f;
    context.maxDbm = -40.0f;
    context.traceSamples = 32;
    context.waterfallSamples = 32;
    context.wideSamples = 0;
    QList<DisplayCodecFrame> frames;
    for (quint32 sequence = 1; sequence <= 4; ++sequence) {
        DisplayCodecFrame frame;
        frame.context = context;
        frame.encoderSequence = sequence;
        frame.producerTimestamp = 20'000'000ULL * sequence;
        frame.waterfallAdvance = true;
        const float drift = 0.5f * static_cast<float>(sequence - 1);
        for (int i = 0; i < 32; ++i) {
            frame.traceDbm.append(-120.0f + 20.0f * static_cast<float>(std::sin(0.3 * i)) + drift);
            frame.waterfallDbm.append(-110.0f + 10.0f * static_cast<float>(std::cos(0.2 * i))
                                      + drift);
        }
        frames.append(frame);
    }
    return frames;
}

DisplayCodecFrame LinkMediaVectors::nsdcTransmitFrame()
{
    DisplayCodecContext context;
    context.endpointId = 1;
    context.contextGeneration = 2;
    context.minDbm = -80.0f;
    context.maxDbm = 20.0f;
    context.traceSamples = 32;
    context.waterfallSamples = 32;
    context.wideSamples = 0;
    DisplayCodecFrame frame;
    frame.context = context;
    frame.encoderSequence = 0;
    frame.producerTimestamp = 66'666'667ULL;
    frame.waterfallAdvance = true;
    for (int i = 0; i < 32; ++i) {
        const double offset = static_cast<double>(i - 16);
        const float tone = static_cast<float>(-70.0 + 80.0 * std::exp(-offset * offset / 4.0));
        frame.traceDbm.append(tone);
        frame.waterfallDbm.append(tone - 3.0f);
    }
    return frame;
}

// The first plane's prefix starts right after the 42-byte header:
// blockSizeCode at 42, blockCount at 43..44; the sequence is at 16..19.
QByteArray LinkMediaVectors::nsdcBadPlaneDelta(const QByteArray& delta)
{
    QByteArray bytes = delta;
    bytes[42] = 4;
    return bytes;
}

QByteArray LinkMediaVectors::nsdcStaleTruncatedDelta(const QByteArray& delta)
{
    QByteArray bytes = delta;
    for (int i = 16; i < 20; ++i) {
        bytes[i] = 0;
    }
    bytes.chop(1);
    return bytes;
}

QByteArray LinkMediaVectors::nsdcBadBlockCountKeyframe(const QByteArray& keyframe)
{
    QByteArray bytes = keyframe;
    const quint16 count = static_cast<quint16>(
        (static_cast<quint8>(bytes.at(43)) << 8) | static_cast<quint8>(bytes.at(44)));
    const quint16 wrong = static_cast<quint16>(count + 1U);
    bytes[43] = static_cast<char>(wrong >> 8);
    bytes[44] = static_cast<char>(wrong & 0xFF);
    return bytes;
}

QString LinkMediaVectors::nsdcDispositionName(DisplayCodecDisposition disposition)
{
    switch (disposition) {
    case DisplayCodecDisposition::Accepted: return QStringLiteral("accepted");
    case DisplayCodecDisposition::NeedKeyframe: return QStringLiteral("needKeyframe");
    case DisplayCodecDisposition::Rejected: return QStringLiteral("rejected");
    }
    return QString();
}

QString LinkMediaVectors::nsdcReasonName(DisplayCodecReason reason)
{
    switch (reason) {
    case DisplayCodecReason::None: return QStringLiteral("none");
    case DisplayCodecReason::InvalidInput: return QStringLiteral("invalidInput");
    case DisplayCodecReason::NoHistory: return QStringLiteral("noHistory");
    case DisplayCodecReason::SequenceGap: return QStringLiteral("sequenceGap");
    case DisplayCodecReason::StaleSequence: return QStringLiteral("staleSequence");
    case DisplayCodecReason::OldContext: return QStringLiteral("oldContext");
    case DisplayCodecReason::ContextMismatch: return QStringLiteral("contextMismatch");
    case DisplayCodecReason::BadMagic: return QStringLiteral("badMagic");
    case DisplayCodecReason::UnsupportedVersion: return QStringLiteral("unsupportedVersion");
    case DisplayCodecReason::UnknownFlags: return QStringLiteral("unknownFlags");
    case DisplayCodecReason::Truncated: return QStringLiteral("truncated");
    case DisplayCodecReason::Oversized: return QStringLiteral("oversized");
    case DisplayCodecReason::Malformed: return QStringLiteral("malformed");
    }
    return QString();
}

QJsonObject LinkMediaVectors::toJson(const DisplayCodecDecodeResult& result)
{
    QJsonObject out{
        {QStringLiteral("disposition"), nsdcDispositionName(result.disposition)},
        {QStringLiteral("reason"), nsdcReasonName(result.reason)},
    };
    if (result.disposition != DisplayCodecDisposition::Accepted) {
        return out;
    }
    const auto rows = [](const QVector<float>& values) {
        QJsonArray array;
        for (const float value : values) {
            array.append(static_cast<double>(value));
        }
        return array;
    };
    const DisplayCodecFrame& frame = result.frame;
    out.insert(QStringLiteral("endpointId"), static_cast<qint64>(frame.context.endpointId));
    out.insert(QStringLiteral("contextGeneration"),
               static_cast<qint64>(frame.context.contextGeneration));
    out.insert(QStringLiteral("minDbm"), static_cast<double>(frame.context.minDbm));
    out.insert(QStringLiteral("maxDbm"), static_cast<double>(frame.context.maxDbm));
    out.insert(QStringLiteral("encoderSequence"), static_cast<qint64>(frame.encoderSequence));
    out.insert(QStringLiteral("producerTimestamp"), static_cast<qint64>(frame.producerTimestamp));
    out.insert(QStringLiteral("waterfallAdvance"), frame.waterfallAdvance);
    out.insert(QStringLiteral("traceDbm"), rows(frame.traceDbm));
    out.insert(QStringLiteral("waterfallDbm"), rows(frame.waterfallDbm));
    out.insert(QStringLiteral("wideDbm"), rows(frame.wideDbm));
    return out;
}

DisplayCodecContext LinkMediaVectors::nsdxContext()
{
    DisplayCodecContext context;
    context.endpointId = 1;
    context.contextGeneration = 1;
    context.minDbm = -140.0f;
    context.maxDbm = -40.0f;
    context.traceSamples = 32;
    context.waterfallSamples = 32;
    context.wideSamples = 0;
    return context;
}

DisplayExtrasFrame LinkMediaVectors::nsdxFrame()
{
    // Beside nsdcFrames()'s frame 1: three blobs, the hold row a little
    // above that frame's trace, the noise floor and the waterfall's levels.
    DisplayExtrasFrame frame;
    frame.endpointId = 1;
    frame.contextGeneration = 1;
    frame.encoderSequence = 1;
    frame.peakBlobs = QVector<DisplayExtrasBlob>{{5, -100.5f}, {26, -101.25f}, {15, -118.0f}};
    QVector<float> hold;
    for (int i = 0; i < 32; ++i) {
        hold.append(-117.0f + 20.0f * static_cast<float>(std::sin(0.3 * i)));
    }
    frame.peakHoldDbm = hold;
    frame.noiseFloorDbm = -127.5f;
    frame.waterfallLevelsDbm = std::make_pair(-131.0f, -71.0f);
    return frame;
}

DisplayExtrasFrame LinkMediaVectors::nsdxNoiseFloorFrame()
{
    DisplayExtrasFrame frame;
    frame.endpointId = 1;
    frame.contextGeneration = 1;
    frame.encoderSequence = 2;
    frame.noiseFloorDbm = -126.75f;
    return frame;
}

DisplayExtrasFrame LinkMediaVectors::nsdxNoiseFloorStateFrame()
{
    // Display extras version 4: the floor and its state section (0x10),
    // here in fast attack.
    DisplayExtrasFrame frame = nsdxNoiseFloorFrame();
    frame.encoderSequence = 3;
    frame.noiseFloorFastAttack = true;
    return frame;
}

QByteArray LinkMediaVectors::nsdxUnknownSection(const QByteArray& full)
{
    // 0x10 is the noise floor state section since display extras version 4;
    // 0x20 is still unknown.
    QByteArray bytes = full;
    bytes[5] = static_cast<char>(static_cast<quint8>(bytes.at(5)) | 0x20);
    return bytes;
}

QJsonObject LinkMediaVectors::toJson(const DisplayCodecContext& context)
{
    return {{QStringLiteral("endpointId"), static_cast<qint64>(context.endpointId)},
            {QStringLiteral("contextGeneration"), static_cast<qint64>(context.contextGeneration)},
            {QStringLiteral("minDbm"), static_cast<double>(context.minDbm)},
            {QStringLiteral("maxDbm"), static_cast<double>(context.maxDbm)},
            {QStringLiteral("traceSamples"), static_cast<int>(context.traceSamples)}};
}

bool LinkMediaVectors::fromJson(const QJsonObject& json, DisplayCodecContext* context,
                                QString* error)
{
    const QStringList keys{QStringLiteral("contextGeneration"), QStringLiteral("endpointId"),
                           QStringLiteral("maxDbm"), QStringLiteral("minDbm"),
                           QStringLiteral("traceSamples")};
    QStringList present = json.keys();
    present.sort();
    if (present != keys) {
        *error = QStringLiteral("an NSDX context is {endpointId, contextGeneration, minDbm, "
                                "maxDbm, traceSamples}");
        return false;
    }
    for (const QString& key : keys) {
        if (!json.value(key).isDouble()) {
            *error = QStringLiteral("NSDX context %1 is not a number").arg(key);
            return false;
        }
    }
    DisplayCodecContext out;
    out.endpointId = static_cast<quint32>(json.value(QStringLiteral("endpointId")).toDouble());
    out.contextGeneration =
        static_cast<quint32>(json.value(QStringLiteral("contextGeneration")).toDouble());
    out.minDbm = static_cast<float>(json.value(QStringLiteral("minDbm")).toDouble());
    out.maxDbm = static_cast<float>(json.value(QStringLiteral("maxDbm")).toDouble());
    out.traceSamples = static_cast<quint16>(json.value(QStringLiteral("traceSamples")).toInt());
    out.waterfallSamples = out.traceSamples;
    *context = out;
    return true;
}

QString LinkMediaVectors::nsdxReasonName(DisplayExtrasReason reason)
{
    switch (reason) {
    case DisplayExtrasReason::None: return QStringLiteral("none");
    case DisplayExtrasReason::BadMagic: return QStringLiteral("badMagic");
    case DisplayExtrasReason::UnsupportedVersion: return QStringLiteral("unsupportedVersion");
    case DisplayExtrasReason::UnknownSections: return QStringLiteral("unknownSections");
    case DisplayExtrasReason::Truncated: return QStringLiteral("truncated");
    case DisplayExtrasReason::Oversized: return QStringLiteral("oversized");
    case DisplayExtrasReason::Malformed: return QStringLiteral("malformed");
    case DisplayExtrasReason::ContextMismatch: return QStringLiteral("contextMismatch");
    }
    return QString();
}

QJsonObject LinkMediaVectors::toJson(const DisplayExtrasDecodeResult& result)
{
    QJsonObject out{
        {QStringLiteral("accepted"), result.accepted},
        {QStringLiteral("reason"), nsdxReasonName(result.reason)},
    };
    if (!result.accepted) {
        return out;
    }
    const DisplayExtrasFrame& frame = result.frame;
    out.insert(QStringLiteral("endpointId"), static_cast<qint64>(frame.endpointId));
    out.insert(QStringLiteral("contextGeneration"), static_cast<qint64>(frame.contextGeneration));
    out.insert(QStringLiteral("encoderSequence"), static_cast<qint64>(frame.encoderSequence));
    if (frame.peakBlobs) {
        QJsonArray blobs;
        for (const DisplayExtrasBlob& blob : *frame.peakBlobs) {
            blobs.append(QJsonObject{{QStringLiteral("pixel"), static_cast<int>(blob.pixel)},
                                     {QStringLiteral("dbm"), static_cast<double>(blob.dbm)}});
        }
        out.insert(QStringLiteral("peakBlobs"), blobs);
    }
    if (frame.peakHoldDbm) {
        QJsonArray row;
        for (const float value : *frame.peakHoldDbm) {
            row.append(static_cast<double>(value));
        }
        out.insert(QStringLiteral("peakHoldDbm"), row);
    }
    if (frame.noiseFloorDbm) {
        out.insert(QStringLiteral("noiseFloorDbm"), static_cast<double>(*frame.noiseFloorDbm));
    }
    if (frame.noiseFloorFastAttack) {
        out.insert(QStringLiteral("noiseFloorFastAttack"), *frame.noiseFloorFastAttack);
    }
    if (frame.waterfallLevelsDbm) {
        out.insert(QStringLiteral("waterfallLevelsDbm"),
                   QJsonObject{{QStringLiteral("lowDbm"),
                                static_cast<double>(frame.waterfallLevelsDbm->first)},
                               {QStringLiteral("highDbm"),
                                static_cast<double>(frame.waterfallLevelsDbm->second)}});
    }
    return out;
}

QVector<float> LinkMediaVectors::opusInput(int index)
{
    constexpr double kPi = 3.14159265358979323846;
    const int frames = OpusAudioCodecConfig::kFrameSamples;
    QVector<float> pcm;
    pcm.reserve(frames * OpusAudioCodecConfig::kChannels);
    for (int i = 0; i < frames; ++i) {
        const double t = static_cast<double>(index * frames + i)
                         / static_cast<double>(OpusAudioCodecConfig::kSampleRate);
        pcm.append(static_cast<float>(0.3 * std::sin(2.0 * kPi * 440.0 * t)));
        pcm.append(static_cast<float>(0.2 * std::sin(2.0 * kPi * 1000.0 * t)));
    }
    return pcm;
}

QString LinkMediaVectors::opusStatusName(OpusAudioCodecStatus status)
{
    switch (status) {
    case OpusAudioCodecStatus::Accepted: return QStringLiteral("accepted");
    case OpusAudioCodecStatus::Concealed: return QStringLiteral("concealed");
    case OpusAudioCodecStatus::InvalidInput: return QStringLiteral("invalidInput");
    case OpusAudioCodecStatus::EncodeFailed: return QStringLiteral("encodeFailed");
    case OpusAudioCodecStatus::DecodeFailed: return QStringLiteral("decodeFailed");
    case OpusAudioCodecStatus::MalformedRtp: return QStringLiteral("malformedRtp");
    case OpusAudioCodecStatus::UnexpectedSsrc: return QStringLiteral("unexpectedSsrc");
    case OpusAudioCodecStatus::Oversized: return QStringLiteral("oversized");
    }
    return QString();
}

QJsonObject LinkMediaVectors::toJson(const OpusRtpDecodeResult& result)
{
    QJsonArray pcm;
    for (const float sample : result.pcmInterleaved) {
        const double scaled = std::round(static_cast<double>(sample) * 32767.0);
        pcm.append(static_cast<qint64>(std::clamp(scaled, -32768.0, 32767.0)));
    }
    return QJsonObject{
        {QStringLiteral("status"), opusStatusName(result.status)},
        {QStringLiteral("sequence"), static_cast<qint64>(result.sequence)},
        {QStringLiteral("timestamp"), static_cast<qint64>(result.timestamp)},
        {QStringLiteral("channels"), result.packetInfo.channels},
        {QStringLiteral("bandwidth"), result.packetInfo.bandwidth},
        {QStringLiteral("samplesPerChannel"), result.packetInfo.samplesPerChannel},
        {QStringLiteral("pcm16"), pcm},
    };
}

} // namespace NereusSDR::Test
