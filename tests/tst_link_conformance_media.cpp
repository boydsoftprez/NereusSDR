// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_link_conformance_media.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 3 (R-IOS-01): the station runs the link's media
// vectors (tests/data/link/v1/media/*.bin with *.expect.json). Each vector
// is bytes the station's own encoder wrote (tst_link_conformance_regen) and
// the values they decode to. The station decodes the bytes and compares
// them with the expectation, then encodes the expectation again and
// compares the result with the bytes, so a change to either the decoder or
// the encoder shows here. The app decodes the same bytes with its own
// decoders.
//
//   codec "nrsc1"   the LAN announcement datagram (link document section
//                   14.1), schema 1 or 2, decoded by
//                   decodeStationLanAnnouncement
//   codec "dnssd-txt" the Bonjour TXT record (section 14.2), split by
//                   decodeDnsSdTxtRecord; encoded again in the key order
//                   v, id, claimed, pair, name, devices, radio
//   codec "ps3d"    one PureSignal display chunk, assembled by a fresh
//                   Ps3DisplayAssembler for the expectation's
//                   sessionGeneration; values are exact (tolerance 0)
//   codec "nsdc1"   one display codec frame, decoded by DisplayCodecDecoder;
//                   rows within the expectation's dBm tolerance
//   codec "nsdx1"   one display extras datagram, decoded by
//                   decodeDisplayExtras against the endpoint context the
//                   expectation names; values within its dBm tolerance
//   codec "opus"    one RTP packet of Opus audio, decoded by
//                   OpusAudioDecoder; PCM by SNR or by 16-bit steps, as the
//                   expectation states
//
// A vector's expectation may name others in "after": a fresh decoder
// decodes those first, in order (their own expectations are not checked
// there), then this vector, and only this vector's expectation is
// compared. "after" naming the vector itself, a vector of another codec, a
// missing vector, or forming a cycle is a malformed vector.
//
//   cmake --build build --target tst_link_conformance_media
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build \
//       -R '^tst_link_conformance_media$' --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 3 (R-IOS-01): media
//                                    conformance runner. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 3 (R-IOS-01): NSDC and
//                                    Opus vectors, and "after" for the
//                                    stateful decoders. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): runs once per link major in
//                                    the manifest.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A (R-R3-03, R-IOS-01):
//                                    the malformed-and-refused NSDC
//                                    vectors. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 20 (R-IOS-27): the
//                                    NSDX display extras vectors.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): the lan-
//               announcement-2-devices vector and the TXT record's sixth entry.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 25 (R-IOS-16): the radio state
//               vectors and the TXT record's seventh entry, `radio`. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 28 (R-R3-49, A11): the
//                                    transmit display's NSDC vector
//                                    (nsdc1-transmit). AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include <cmath>
#include <optional>

#include "core/safety/RemoteTxWatchdog.h"
#include "core/dsp/Ps3Snapshot.h"
#include "core/session/LinkVersion.h"
#include "core/session/Ps3DisplayCodec.h"
#include "core/session/StationLanAnnouncement.h"
#include "core/session/DnsSdAdvertiser.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/DisplayExtras.h"
#include "core/session/media/OpusAudioCodec.h"

#include "LinkFixtures.h"

using namespace NereusSDR;
using NereusSDR::Test::LinkFixtures;
using NereusSDR::Test::LinkMediaVectors;

namespace {

struct Vector {
    QString id;
    QByteArray bytes;
    QJsonObject expectation;
};
using Vectors = QHash<QString, Vector>;

QString codecOf(const Vector& vector)
{
    return vector.expectation.value(QStringLiteral("codec")).toString();
}

QJsonObject expectOf(const Vector& vector)
{
    return vector.expectation.value(QStringLiteral("expect")).toObject();
}

// The ids a vector's "after" names, checked: strings, present, the same
// codec, not the vector itself, and no cycle through anyone's "after".
QString resolveAfter(const Vectors& all, const Vector& vector, QStringList* after)
{
    after->clear();
    const QJsonValue value = expectOf(vector).value(QStringLiteral("after"));
    if (value.isUndefined()) {
        return QString();
    }
    if (!value.isArray()) {
        return QStringLiteral("%1: after must be an array of fixture ids").arg(vector.id);
    }
    for (const QJsonValue& entry : value.toArray()) {
        const QString id = entry.toString();
        if (!entry.isString() || !all.contains(id)) {
            return QStringLiteral("%1: after names no media fixture \"%2\"").arg(vector.id, id);
        }
        if (id == vector.id) {
            return QStringLiteral("%1: after names the fixture itself").arg(vector.id);
        }
        if (codecOf(all.value(id)) != codecOf(vector)) {
            return QStringLiteral("%1: after names %2, a fixture of another codec")
                .arg(vector.id, id);
        }
        after->append(id);
    }
    // A cycle: some vector reachable through "after" names this one again.
    QStringList stack = *after;
    QSet<QString> seen;
    while (!stack.isEmpty()) {
        const QString id = stack.takeLast();
        if (id == vector.id) {
            return QStringLiteral("%1: after forms a cycle").arg(vector.id);
        }
        if (seen.contains(id)) {
            continue;
        }
        seen.insert(id);
        for (const QJsonValue& next :
             expectOf(all.value(id)).value(QStringLiteral("after")).toArray()) {
            stack.append(next.toString());
        }
    }
    return QString();
}

// Like LinkFixtures::match, with every number allowed to differ by up to
// `tolerance`.
QString matchWithin(const QJsonValue& expected, const QJsonValue& actual, double tolerance,
                    const QString& path)
{
    if (expected.isDouble()) {
        if (!actual.isDouble() || std::fabs(actual.toDouble() - expected.toDouble()) > tolerance) {
            return QStringLiteral("%1: expected %2 within %3, got %4")
                .arg(path)
                .arg(expected.toDouble(), 0, 'g', 12)
                .arg(tolerance)
                .arg(actual.isDouble() ? QString::number(actual.toDouble(), 'g', 12)
                                       : QStringLiteral("a non-number"));
        }
        return QString();
    }
    if (expected.isArray()) {
        const QJsonArray e = expected.toArray();
        const QJsonArray a = actual.toArray();
        if (!actual.isArray() || e.size() != a.size()) {
            return QStringLiteral("%1: expected %2 elements").arg(path).arg(e.size());
        }
        for (qsizetype i = 0; i < e.size(); ++i) {
            const QString difference =
                matchWithin(e.at(i), a.at(i), tolerance, QStringLiteral("%1[%2]").arg(path).arg(i));
            if (!difference.isEmpty()) {
                return difference;
            }
        }
        return QString();
    }
    if (expected.isObject()) {
        const QJsonObject e = expected.toObject();
        const QJsonObject a = actual.toObject();
        if (!actual.isObject()) {
            return QStringLiteral("%1: expected an object").arg(path);
        }
        for (auto it = e.constBegin(); it != e.constEnd(); ++it) {
            const QString difference =
                matchWithin(it.value(), a.value(it.key()), tolerance, path + QLatin1Char('.') + it.key());
            if (!difference.isEmpty()) {
                return difference;
            }
        }
        for (auto it = a.constBegin(); it != a.constEnd(); ++it) {
            if (!e.contains(it.key())) {
                return QStringLiteral("%1.%2: not expected").arg(path, it.key());
            }
        }
        return QString();
    }
    LinkFixtures::Captures none;
    return LinkFixtures::match(expected, actual, &none, path);
}

QString checkAnnouncement(const QByteArray& bytes, QJsonObject expect)
{
    // iPhone app Task 16: "ignoredTrailingBytes" N marks a schema-2 vector
    // whose last N bytes are fields a reader does not know (section 14.1):
    // they decode to the same fields, and the encoder writes the rest.
    qsizetype trailing = 0;
    if (expect.contains(QStringLiteral("ignoredTrailingBytes"))) {
        const QJsonValue count = expect.take(QStringLiteral("ignoredTrailingBytes"));
        const double n = count.toDouble(-1);
        if (!count.isDouble() || n < 1 || n != std::floor(n) || n >= bytes.size()) {
            return QStringLiteral("media expectation: ignoredTrailingBytes is not a count of "
                                  "the vector's last bytes");
        }
        trailing = static_cast<qsizetype>(n);
    }
    QString error;
    const std::optional<StationLanAnnouncement> decoded =
        decodeStationLanAnnouncement(bytes, &error);
    if (!decoded) {
        return QStringLiteral("the station's decoder refused the announcement: %1").arg(error);
    }
    LinkFixtures::Captures none;
    const QString difference =
        LinkFixtures::match(expect, LinkMediaVectors::toJson(*decoded), &none);
    if (!difference.isEmpty()) {
        return QStringLiteral("the decoded announcement differs at %1").arg(difference);
    }
    StationLanAnnouncement value;
    if (!LinkMediaVectors::fromJson(expect, &value, &error)) {
        return error;
    }
    if (encodeStationLanAnnouncement(value, &error) != bytes.chopped(trailing)) {
        return QStringLiteral("the station's encoder no longer writes these bytes for this "
                              "announcement %1")
            .arg(error);
    }
    return QString();
}

// iPhone app Task 16: the Bonjour TXT record. The expectation is the
// service type and the entries as strings; the bytes are those entries in
// the station's order, each preceded by its length.
// Fix wave M9: the "tx" data channel's 13-byte keepalive.
QString checkTxKeepalive(const QByteArray& bytes, const QJsonObject& expect)
{
    for (auto it = expect.constBegin(); it != expect.constEnd(); ++it) {
        if (it.key() != QStringLiteral("sequence") && it.key() != QStringLiteral("epoch")) {
            return QStringLiteral("media expectation: unknown field \"%1\"").arg(it.key());
        }
    }
    if (bytes.size() != 13) {
        return QStringLiteral("tx-keepalive: %1 bytes, not 13").arg(bytes.size());
    }
    quint64 sequence = 0;
    quint32 epoch = 0;
    if (!RemoteTxWatchdog::readChannelKeepalive(bytes, &sequence, &epoch)) {
        return QStringLiteral("tx-keepalive: the station does not read it");
    }
    if (qint64(sequence) != expect.value(QStringLiteral("sequence")).toInteger()
        || qint64(epoch) != expect.value(QStringLiteral("epoch")).toInteger()) {
        return QStringLiteral("tx-keepalive: read sequence %1 epoch %2").arg(sequence).arg(epoch);
    }
    if (RemoteTxWatchdog::channelKeepalive(sequence, epoch) != bytes) {
        return QStringLiteral("tx-keepalive: the station encodes it differently");
    }
    return QString();
}

QString checkDnsSdTxt(const QByteArray& bytes, const QJsonObject& expect)
{
    for (auto it = expect.constBegin(); it != expect.constEnd(); ++it) {
        if (it.key() != QStringLiteral("serviceType") && it.key() != QStringLiteral("txt")) {
            return QStringLiteral("media expectation: unknown field \"%1\"").arg(it.key());
        }
    }
    if (expect.value(QStringLiteral("serviceType")).toString()
        != QString::fromLatin1(kDnsSdServiceType)) {
        return QStringLiteral("the decoded record differs at $.serviceType");
    }
    QString error;
    const std::optional<DnsSdTxtEntries> entries = decodeDnsSdTxtRecord(bytes, &error);
    if (!entries) {
        return QStringLiteral("the station's decoder refused the TXT record: %1").arg(error);
    }
    QJsonObject decoded;
    for (const auto& [key, value] : *entries) {
        decoded.insert(QString::fromLatin1(key), QString::fromLatin1(value));
    }
    LinkFixtures::Captures none;
    const QString difference = LinkFixtures::match(
        expect.value(QStringLiteral("txt")).toObject(), decoded, &none, QStringLiteral("$.txt"));
    if (!difference.isEmpty()) {
        return QStringLiteral("the decoded record differs at %1").arg(difference);
    }
    QByteArray encoded;
    const QJsonObject txt = expect.value(QStringLiteral("txt")).toObject();
    // iPhone app Task 71: `devices`, the sixth, after name; iPhone app plan
    // Task 25: `radio`, the seventh.
    for (const QString& key : {QStringLiteral("v"), QStringLiteral("id"), QStringLiteral("claimed"),
                               QStringLiteral("pair"), QStringLiteral("name"),
                               QStringLiteral("devices"), QStringLiteral("radio")}) {
        const QByteArray entry = key.toLatin1() + '=' + txt.value(key).toString().toLatin1();
        encoded.append(static_cast<char>(entry.size()));
        encoded.append(entry);
    }
    if (encoded != bytes) {
        return QStringLiteral("the TXT record's bytes are not its entries in the order v, id, "
                              "claimed, pair, name, devices, radio");
    }
    return QString();
}

QString checkPs3d(const QList<QByteArray>& before, const QByteArray& bytes, QJsonObject expect)
{
    if (expect.value(QStringLiteral("tolerance")).toObject()
        != QJsonObject{{QStringLiteral("absolute"), 0}}) {
        return QStringLiteral("a PS3D vector carries exact values: tolerance must be "
                              "{\"absolute\": 0}");
    }
    expect.remove(QStringLiteral("tolerance"));
    QString error;
    Ps3Snapshot expected;
    if (!LinkMediaVectors::fromJson(expect, &expected, &error)) {
        return error;
    }
    Ps3DisplayAssembler assembler(expected.sessionGeneration);
    for (const QByteArray& earlier : before) {
        assembler.accept(earlier);
    }
    const std::optional<Ps3Snapshot> decoded = assembler.accept(bytes, &error);
    if (!decoded) {
        return QStringLiteral("the station's assembler refused the chunk: %1").arg(error);
    }
    LinkFixtures::Captures none;
    const QString difference =
        LinkFixtures::match(expect, LinkMediaVectors::toJson(*decoded), &none);
    if (!difference.isEmpty()) {
        return QStringLiteral("the decoded frame differs at %1").arg(difference);
    }
    const QList<QByteArray> chunks = Ps3DisplayCodec::encode(expected, &error);
    if (chunks.size() != 1 || chunks.first() != bytes) {
        return QStringLiteral("the station's encoder no longer writes these bytes for this "
                              "frame %1")
            .arg(error);
    }
    return QString();
}

QString checkNsdc(const QList<QByteArray>& before, const QByteArray& bytes, QJsonObject expect)
{
    double tolerance = 0.0;
    if (expect.contains(QStringLiteral("tolerance"))) {
        const QJsonObject t = expect.take(QStringLiteral("tolerance")).toObject();
        if (t.keys() != QStringList{QStringLiteral("dbm")} || !t.value(QStringLiteral("dbm")).isDouble()) {
            return QStringLiteral("an NSDC tolerance is {\"dbm\": <number>}");
        }
        tolerance = t.value(QStringLiteral("dbm")).toDouble();
    }
    DisplayCodecDecoder decoder;
    for (const QByteArray& earlier : before) {
        decoder.decode(earlier);
    }
    QJsonObject decoded = LinkMediaVectors::toJson(decoder.decode(bytes));
    decoded.insert(QStringLiteral("keyframe"), bytes.size() > 5 && (bytes.at(5) & 0x01) != 0);
    // What the decoder did with the packet first, then the frame it made.
    for (const QString& key : {QStringLiteral("disposition"), QStringLiteral("reason"),
                               QStringLiteral("keyframe")}) {
        LinkFixtures::Captures none;
        const QString first = LinkFixtures::match(expect.value(key), decoded.value(key), &none,
                                                  QStringLiteral("$.") + key);
        if (!first.isEmpty()) {
            return QStringLiteral("the decoded display frame differs at %1").arg(first);
        }
    }
    const QString difference = matchWithin(expect, decoded, tolerance, QStringLiteral("$"));
    return difference.isEmpty()
               ? QString()
               : QStringLiteral("the decoded display frame differs at %1").arg(difference);
}

QString checkNsdx(const QList<QByteArray>& before, const QByteArray& bytes, QJsonObject expect)
{
    if (!before.isEmpty()) {
        return QStringLiteral("an NSDX datagram stands alone: it has no \"after\"");
    }
    double tolerance = 0.0;
    if (expect.contains(QStringLiteral("tolerance"))) {
        const QJsonObject t = expect.take(QStringLiteral("tolerance")).toObject();
        if (t.keys() != QStringList{QStringLiteral("dbm")} || !t.value(QStringLiteral("dbm")).isDouble()) {
            return QStringLiteral("an NSDX tolerance is {\"dbm\": <number>}");
        }
        tolerance = t.value(QStringLiteral("dbm")).toDouble();
    }
    QString error;
    DisplayCodecContext context;
    if (!LinkMediaVectors::fromJson(expect.take(QStringLiteral("context")).toObject(), &context,
                                    &error)) {
        return error;
    }
    const QJsonObject decoded = LinkMediaVectors::toJson(decodeDisplayExtras(bytes, context));
    // Whether the decoder took it and why first, then exactly the sections
    // the expectation lists, within the tolerance.
    for (const QString& key : {QStringLiteral("accepted"), QStringLiteral("reason")}) {
        LinkFixtures::Captures none;
        const QString first = LinkFixtures::match(expect.value(key), decoded.value(key), &none,
                                                  QStringLiteral("$.") + key);
        if (!first.isEmpty()) {
            return QStringLiteral("the decoded display extras differ at %1").arg(first);
        }
    }
    QStringList expectedKeys = expect.keys();
    QStringList decodedKeys = decoded.keys();
    expectedKeys.sort();
    decodedKeys.sort();
    if (expectedKeys != decodedKeys) {
        return QStringLiteral("the decoded display extras carry %1, the vector %2")
            .arg(decodedKeys.join(QLatin1Char(',')), expectedKeys.join(QLatin1Char(',')));
    }
    const QString difference = matchWithin(expect, decoded, tolerance, QStringLiteral("$"));
    return difference.isEmpty()
               ? QString()
               : QStringLiteral("the decoded display extras differ at %1").arg(difference);
}

QString checkOpus(const QList<QByteArray>& before, const QByteArray& bytes, QJsonObject expect)
{
    const QJsonObject tolerance = expect.take(QStringLiteral("tolerance")).toObject();
    const bool bySnr = tolerance.keys() == QStringList{QStringLiteral("minSnrDb")};
    const bool bySteps = tolerance.keys() == QStringList{QStringLiteral("lsb16")};
    if (!bySnr && !bySteps) {
        return QStringLiteral("an Opus tolerance is {\"minSnrDb\": <dB>} or {\"lsb16\": <steps>}");
    }
    const double limit = tolerance.value(tolerance.keys().first()).toDouble();
    const QJsonValue ssrcValue = expect.value(QStringLiteral("ssrc"));
    if (!ssrcValue.isDouble()) {
        return QStringLiteral("an Opus expectation names the packets' ssrc");
    }
    const quint32 ssrc = static_cast<quint32>(ssrcValue.toDouble());
    OpusAudioDecoder decoder;
    if (!decoder.isReady()) {
        return QStringLiteral("the station's Opus decoder is not ready");
    }
    for (const QByteArray& earlier : before) {
        decoder.decodeRtp(earlier, ssrc);
    }
    QJsonObject decoded = LinkMediaVectors::toJson(decoder.decodeRtp(bytes, ssrc));
    decoded.insert(QStringLiteral("ssrc"), static_cast<qint64>(ssrc));

    const QJsonArray reference = expect.take(QStringLiteral("pcm16")).toArray();
    const QJsonArray pcm = decoded.take(QStringLiteral("pcm16")).toArray();
    LinkFixtures::Captures none;
    const QString difference = LinkFixtures::match(expect, decoded, &none);
    if (!difference.isEmpty()) {
        return QStringLiteral("the decoded packet differs at %1").arg(difference);
    }
    if (pcm.size() != reference.size()) {
        return QStringLiteral("$.pcm16: expected %1 samples, got %2")
            .arg(reference.size())
            .arg(pcm.size());
    }
    double signal = 0.0;
    double noise = 0.0;
    double worst = 0.0;
    qsizetype worstAt = 0;
    for (qsizetype i = 0; i < pcm.size(); ++i) {
        const double r = reference.at(i).toDouble();
        const double d = pcm.at(i).toDouble() - r;
        signal += r * r;
        noise += d * d;
        if (std::fabs(d) > worst) {
            worst = std::fabs(d);
            worstAt = i;
        }
    }
    if (bySteps && worst > limit) {
        return QStringLiteral("$.pcm16[%1]: %2 steps from the reference, more than %3")
            .arg(worstAt)
            .arg(worst)
            .arg(limit);
    }
    if (bySnr && noise > 0.0) {
        const double snr = signal > 0.0 ? 10.0 * std::log10(signal / noise) : -INFINITY;
        if (snr < limit) {
            return QStringLiteral("$.pcm16: %1 dB from the reference, less than %2 dB "
                                  "(largest difference at [%3])")
                .arg(snr, 0, 'f', 1)
                .arg(limit)
                .arg(worstAt);
        }
    }
    return QString();
}

QString checkVector(const Vectors& all, const QString& id)
{
    const Vector vector = all.value(id);
    for (auto it = vector.expectation.constBegin(); it != vector.expectation.constEnd(); ++it) {
        if (it.key() != QStringLiteral("codec") && it.key() != QStringLiteral("expect")) {
            return QStringLiteral("media expectation: unknown field \"%1\"").arg(it.key());
        }
    }
    QStringList after;
    const QString malformed = resolveAfter(all, vector, &after);
    if (!malformed.isEmpty()) {
        return malformed;
    }
    QList<QByteArray> before;
    for (const QString& earlier : after) {
        before.append(all.value(earlier).bytes);
    }
    QJsonObject expect = expectOf(vector);
    expect.remove(QStringLiteral("after"));
    const QString codec = codecOf(vector);
    if (codec == QStringLiteral("nrsc1")) {
        return checkAnnouncement(vector.bytes, expect);
    }
    if (codec == QStringLiteral("dnssd-txt")) {
        return checkDnsSdTxt(vector.bytes, expect);
    }
    if (codec == QStringLiteral("tx-keepalive")) {
        return checkTxKeepalive(vector.bytes, expect);
    }
    if (codec == QStringLiteral("ps3d")) {
        return checkPs3d(before, vector.bytes, expect);
    }
    if (codec == QStringLiteral("nsdc1")) {
        return checkNsdc(before, vector.bytes, expect);
    }
    if (codec == QStringLiteral("nsdx1")) {
        return checkNsdx(before, vector.bytes, expect);
    }
    if (codec == QStringLiteral("opus")) {
        return checkOpus(before, vector.bytes, expect);
    }
    return QStringLiteral("media expectation: unknown codec \"%1\"").arg(codec);
}

} // namespace

class TstLinkConformanceMedia : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void mediaVectors_data();
    void mediaVectors();
    void vectorsCoverThePlan();
    void nsdcVectorsAreTheStationsEncoderOutput();
    void nsdxVectorsAreTheStationsEncoderOutput();
    void malformedAfterIsReported();
    void alteredVectorsFailReadably();

private:
    Vectors m_vectors;
    QList<quint16> m_linkMajors;
};

void TstLinkConformanceMedia::initTestCase()
{
    QString error;
    const QJsonObject manifest = LinkFixtures::readObject(
        QDir(LinkFixtures::dataDirectory()).filePath(QStringLiteral("manifest.json")), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    m_linkMajors = LinkFixtures::linkMajors(manifest);
    const QDir root(LinkFixtures::dataDirectory());
    for (const LinkFixtures::Entry& entry : LinkFixtures::entries(manifest, QStringLiteral("media"))) {
        QFile file(root.filePath(entry.file));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(entry.file));
        Vector vector;
        vector.id = entry.id;
        vector.bytes = file.readAll();
        vector.expectation = LinkFixtures::readObject(
            root.filePath(entry.file.chopped(4) + QStringLiteral(".expect.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        m_vectors.insert(entry.id, vector);
    }
    QVERIFY(!m_vectors.isEmpty());
}

void TstLinkConformanceMedia::mediaVectors_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<int>("major");
    QStringList ids = m_vectors.keys();
    ids.sort();
    // Once per link major the suite covers (manifest linkMajors).
    for (const quint16 major : m_linkMajors) {
        for (const QString& id : ids) {
            QTest::newRow(qPrintable(QStringLiteral("%1 link %2").arg(id).arg(major)))
                << id << int(major);
        }
    }
}

void TstLinkConformanceMedia::mediaVectors()
{
    QFETCH(QString, id);
    QFETCH(int, major);
    QVERIFY2(LinkVersion::supportedMajors().contains(quint16(major)),
             qPrintable(QStringLiteral("this station does not offer link major %1").arg(major)));
    const QString failure = checkVector(m_vectors, id);
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
}

void TstLinkConformanceMedia::vectorsCoverThePlan()
{
    // Three NSDC frames (full, delta, keyframe after loss), three malformed
    // NSDC datagrams (below) and four Opus
    // packets, besides the announcement and the PS3D frame.
    for (const QString& id :
         {QStringLiteral("media-nsdc1-full"), QStringLiteral("media-nsdc1-delta"),
          QStringLiteral("media-nsdc1-keyframe-after-loss"), QStringLiteral("media-opus-1"),
          QStringLiteral("media-opus-2"), QStringLiteral("media-opus-3"),
          QStringLiteral("media-opus-4"), QStringLiteral("media-lan-announcement"),
          QStringLiteral("media-lan-announcement-2"),
          QStringLiteral("media-lan-announcement-2-devices"),
          QStringLiteral("media-lan-announcement-2-radio"),
          QStringLiteral("media-lan-announcement-2-waiting"),
          QStringLiteral("media-lan-announcement-2-trailing"), QStringLiteral("media-dnssd-txt"),
          QStringLiteral("media-ps3d-frame"), QStringLiteral("media-tx-keepalive"),
          QStringLiteral("media-nsdc1-transmit")}) {
        QVERIFY2(m_vectors.contains(id), qPrintable(id));
    }
    // iPhone app Task 16: the announcement vectors name their schema; the
    // station sends schema 2, and a desktop still reads schema 1.
    QCOMPARE(expectOf(m_vectors.value(QStringLiteral("media-lan-announcement")))
                 .value(QStringLiteral("schema")).toInt(), 1);
    QCOMPARE(expectOf(m_vectors.value(QStringLiteral("media-lan-announcement-2")))
                 .value(QStringLiteral("schema")).toInt(), int(kStationLanAnnouncementSchema));
    // iPhone app Task 71 (ruling 10.4): the device count is one byte
    // appended after Pairing; the datagram without it (an older Core)
    // decodes with the count unknown.
    const QByteArray older = m_vectors.value(QStringLiteral("media-lan-announcement-2")).bytes;
    const Vector counted = m_vectors.value(QStringLiteral("media-lan-announcement-2-devices"));
    QCOMPARE(counted.bytes.size(), older.size() + 1);
    QCOMPARE(counted.bytes.left(older.size()), older);
    QVERIFY(!expectOf(m_vectors.value(QStringLiteral("media-lan-announcement-2")))
                 .contains(QStringLiteral("devicesConnected")));
    QCOMPARE(expectOf(counted).value(QStringLiteral("devicesConnected")).toInt(), 2);
    QJsonObject withoutCount = expectOf(counted);
    withoutCount.remove(QStringLiteral("devicesConnected"));
    QCOMPARE(withoutCount, expectOf(m_vectors.value(QStringLiteral("media-lan-announcement-2"))));
    QVERIFY(counted.bytes.size() <= kStationLanMaxSchema2DatagramBytes);
    // iPhone app plan Task 25 (R-IOS-16): the radio state is one byte
    // appended after the count; the counted datagram without it decodes
    // with the state unknown. The waiting vector names the state a list
    // shows as "Waiting for a radio".
    const Vector stated = m_vectors.value(QStringLiteral("media-lan-announcement-2-radio"));
    QCOMPARE(stated.bytes.size(), counted.bytes.size() + 1);
    QCOMPARE(stated.bytes.left(counted.bytes.size()), counted.bytes);
    QVERIFY(!expectOf(counted).contains(QStringLiteral("radio")));
    QCOMPARE(expectOf(stated).value(QStringLiteral("radio")).toString(),
             QStringLiteral("connected"));
    QJsonObject withoutRadio = expectOf(stated);
    withoutRadio.remove(QStringLiteral("radio"));
    QCOMPARE(withoutRadio, expectOf(counted));
    const QJsonObject waiting =
        expectOf(m_vectors.value(QStringLiteral("media-lan-announcement-2-waiting")));
    QCOMPARE(waiting.value(QStringLiteral("radio")).toString(), QStringLiteral("waiting"));
    QCOMPARE(waiting.value(QStringLiteral("radioConnected")).toBool(), false);
    // Schema 2 extends by appending: the trailing vector is the stated
    // vector with bytes after it, and decodes to the same fields.
    const Vector extended = m_vectors.value(QStringLiteral("media-lan-announcement-2-trailing"));
    const QByteArray known = stated.bytes;
    QVERIFY(extended.bytes.size() > known.size());
    QCOMPARE(extended.bytes.left(known.size()), known);
    QCOMPARE(expectOf(extended).value(QStringLiteral("ignoredTrailingBytes")).toInt(),
             int(extended.bytes.size() - known.size()));
    QJsonObject withoutMark = expectOf(extended);
    withoutMark.remove(QStringLiteral("ignoredTrailingBytes"));
    QCOMPARE(withoutMark, expectOf(stated));
    // The TXT vector is the station's encoder's output for the Core the
    // counted announcement describes, its sixth entry the count.
    QCOMPARE(m_vectors.value(QStringLiteral("media-dnssd-txt")).bytes,
             encodeDnsSdTxtRecord(LinkMediaVectors::dnsSdRecord()));
    QCOMPARE(LinkMediaVectors::dnsSdRecord().identity,
             LinkMediaVectors::lanAnnouncement2().identity);
    QCOMPARE(expectOf(m_vectors.value(QStringLiteral("media-dnssd-txt")))
                 .value(QStringLiteral("txt")).toObject()
                 .value(QStringLiteral("devices")).toString(),
             QStringLiteral("2"));
    QCOMPARE(expectOf(m_vectors.value(QStringLiteral("media-dnssd-txt")))
                 .value(QStringLiteral("txt")).toObject()
                 .value(QStringLiteral("radio")).toString(),
             QStringLiteral("connected"));
    const QJsonObject delta = expectOf(m_vectors.value(QStringLiteral("media-nsdc1-delta")));
    QCOMPARE(delta.value(QStringLiteral("keyframe")).toBool(), false);
    const QJsonObject recovered =
        expectOf(m_vectors.value(QStringLiteral("media-nsdc1-keyframe-after-loss")));
    QCOMPARE(recovered.value(QStringLiteral("keyframe")).toBool(), true);
    QCOMPARE(recovered.value(QStringLiteral("disposition")).toString(), QStringLiteral("accepted"));

    // Three datagrams both malformed and refused reject as malformed: the
    // structure is checked before the sequence and history rules.
    for (const QString& id : {QStringLiteral("media-nsdc1-malformed-delta"),
                              QStringLiteral("media-nsdc1-malformed-stale-delta"),
                              QStringLiteral("media-nsdc1-malformed-keyframe")}) {
        QVERIFY2(m_vectors.contains(id), qPrintable(id));
        const QJsonObject expect = expectOf(m_vectors.value(id));
        QCOMPARE(expect.value(QStringLiteral("disposition")).toString(), QStringLiteral("rejected"));
        QCOMPARE(expect.value(QStringLiteral("reason")).toString(), QStringLiteral("malformed"));
    }
}

void TstLinkConformanceMedia::nsdcVectorsAreTheStationsEncoderOutput()
{
    // The display codec is integer arithmetic, so the station's encoder
    // writes the same bytes on every machine: a change to it shows here.
    // (Opus is not held to its bytes: its floating-point encoder may differ
    // between processors; its vectors hold the decoder to them instead.)
    const QList<DisplayCodecFrame> frames = LinkMediaVectors::nsdcFrames();
    DisplayCodecEncoder encoder;
    const QByteArray full = encoder.encode(frames.at(0));
    const QByteArray delta = encoder.encode(frames.at(1));
    const QByteArray lost = encoder.encode(frames.at(2));
    const QByteArray keyframe = encoder.encode(frames.at(3), true);
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdc1-full")).bytes, full);
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdc1-delta")).bytes, delta);
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdc1-delta-after-loss")).bytes, lost);
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdc1-keyframe-after-loss")).bytes, keyframe);
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdc1-malformed-delta")).bytes,
             LinkMediaVectors::nsdcBadPlaneDelta(delta));
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdc1-malformed-stale-delta")).bytes,
             LinkMediaVectors::nsdcStaleTruncatedDelta(delta));
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdc1-malformed-keyframe")).bytes,
             LinkMediaVectors::nsdcBadBlockCountKeyframe(keyframe));
    // Parity Task 28: a transmit display frame is an ordinary NSDC frame.
    DisplayCodecEncoder transmitEncoder;
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdc1-transmit")).bytes,
             transmitEncoder.encode(LinkMediaVectors::nsdcTransmitFrame(), true));
}

void TstLinkConformanceMedia::nsdxVectorsAreTheStationsEncoderOutput()
{
    // iPhone app Task 20: the extras datagram is integer and IEEE-754 bit
    // copies, so the station's encoder writes the same bytes everywhere.
    const DisplayCodecContext context = LinkMediaVectors::nsdxContext();
    const QByteArray full = encodeDisplayExtras(LinkMediaVectors::nsdxFrame(), context);
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdx1-full")).bytes, full);
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdx1-noise-floor")).bytes,
             encodeDisplayExtras(LinkMediaVectors::nsdxNoiseFloorFrame(), context));
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdx1-noise-floor-state")).bytes,
             encodeDisplayExtras(LinkMediaVectors::nsdxNoiseFloorStateFrame(), context));
    QCOMPARE(expectOf(m_vectors.value(QStringLiteral("media-nsdx1-noise-floor-state")))
                 .value(QStringLiteral("noiseFloorFastAttack")),
             QJsonValue(true));
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdx1-other-generation")).bytes, full);
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdx1-unknown-section")).bytes,
             LinkMediaVectors::nsdxUnknownSection(full));
    QCOMPARE(m_vectors.value(QStringLiteral("media-nsdx1-truncated")).bytes, full.chopped(1));
    const QJsonObject expect = expectOf(m_vectors.value(QStringLiteral("media-nsdx1-full")));
    QCOMPARE(expect.value(QStringLiteral("accepted")).toBool(), true);
    for (const QString& key : {QStringLiteral("peakBlobs"), QStringLiteral("peakHoldDbm"),
                               QStringLiteral("noiseFloorDbm"),
                               QStringLiteral("waterfallLevelsDbm")}) {
        QVERIFY2(expect.contains(key), qPrintable(key));
    }
    QCOMPARE(expectOf(m_vectors.value(QStringLiteral("media-nsdx1-other-generation")))
                 .value(QStringLiteral("reason")).toString(),
             QStringLiteral("contextMismatch"));
}

void TstLinkConformanceMedia::malformedAfterIsReported()
{
    const auto withAfter = [](Vector vector, const QJsonArray& after) {
        QJsonObject expect = vector.expectation.value(QStringLiteral("expect")).toObject();
        expect.insert(QStringLiteral("after"), after);
        vector.expectation.insert(QStringLiteral("expect"), expect);
        return vector;
    };
    Vectors all = m_vectors;
    const QString delta = QStringLiteral("media-nsdc1-delta");
    const QString full = QStringLiteral("media-nsdc1-full");

    all.insert(delta, withAfter(m_vectors.value(delta), {delta}));
    QString failure = checkVector(all, delta);
    QVERIFY2(failure.contains(QStringLiteral("the fixture itself")), qPrintable(failure));

    all.insert(delta, withAfter(m_vectors.value(delta), {QStringLiteral("media-opus-1")}));
    failure = checkVector(all, delta);
    QVERIFY2(failure.contains(QStringLiteral("another codec")), qPrintable(failure));

    all.insert(delta, withAfter(m_vectors.value(delta), {QStringLiteral("media-no-such-vector")}));
    failure = checkVector(all, delta);
    QVERIFY2(failure.contains(QStringLiteral("names no media fixture")), qPrintable(failure));

    all.insert(delta, withAfter(m_vectors.value(delta), {full}));
    all.insert(full, withAfter(m_vectors.value(full), {delta}));
    failure = checkVector(all, delta);
    QVERIFY2(failure.contains(QStringLiteral("cycle")), qPrintable(failure));
}

void TstLinkConformanceMedia::alteredVectorsFailReadably()
{
    // One byte of the announcement's core name changed: the decoded name
    // differs, and the failure names the field.
    Vectors all = m_vectors;
    Vector announcement = all.value(QStringLiteral("media-lan-announcement"));
    const qsizetype at = announcement.bytes.indexOf(QByteArrayLiteral("Shack Core"));
    QVERIFY(at >= 0);
    announcement.bytes[at] = 'W';
    all.insert(announcement.id, announcement);
    QString failure = checkVector(all, announcement.id);
    QVERIFY2(failure.contains(QStringLiteral("$.coreName")), qPrintable(failure));

    // One expected PS3D value changed: the failure names the element.
    Vector ps3d = all.value(QStringLiteral("media-ps3d-frame"));
    QJsonObject expect = expectOf(ps3d);
    QJsonArray x = expect.value(QStringLiteral("x")).toArray();
    x.replace(2, 99.0);
    expect.insert(QStringLiteral("x"), x);
    ps3d.expectation.insert(QStringLiteral("expect"), expect);
    all.insert(ps3d.id, ps3d);
    failure = checkVector(all, ps3d.id);
    QVERIFY2(failure.contains(QStringLiteral("$.x[2]")), qPrintable(failure));

    // The delta decoded without the frame before it: the decoder has no
    // history, and the failure names the disposition.
    Vector delta = all.value(QStringLiteral("media-nsdc1-delta"));
    expect = expectOf(delta);
    expect.remove(QStringLiteral("after"));
    delta.expectation.insert(QStringLiteral("expect"), expect);
    all.insert(delta.id, delta);
    failure = checkVector(all, delta.id);
    QVERIFY2(failure.contains(QStringLiteral("$.disposition")), qPrintable(failure));

    // A display value moved by more than 0.01 dB.
    Vector keyframe = all.value(QStringLiteral("media-nsdc1-keyframe-after-loss"));
    expect = expectOf(keyframe);
    QJsonArray trace = expect.value(QStringLiteral("traceDbm")).toArray();
    trace.replace(5, trace.at(5).toDouble() + 0.02);
    expect.insert(QStringLiteral("traceDbm"), trace);
    keyframe.expectation.insert(QStringLiteral("expect"), expect);
    all.insert(keyframe.id, keyframe);
    failure = checkVector(all, keyframe.id);
    QVERIFY2(failure.contains(QStringLiteral("$.traceDbm[5]")), qPrintable(failure));

    // Opus packet 3 decoded without packets 1 and 2: its PCM falls below
    // the reference's SNR.
    Vector opus = all.value(QStringLiteral("media-opus-3"));
    expect = expectOf(opus);
    expect.remove(QStringLiteral("after"));
    opus.expectation.insert(QStringLiteral("expect"), expect);
    all.insert(opus.id, opus);
    failure = checkVector(all, opus.id);
    QVERIFY2(failure.contains(QStringLiteral("$.pcm16")), qPrintable(failure));
}

QTEST_MAIN(TstLinkConformanceMedia)
#include "tst_link_conformance_media.moc"
