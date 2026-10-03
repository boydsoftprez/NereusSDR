// =================================================================
// tests/tst_remote_audio_context.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-23: the audio-context wire codec
// Core and GUI share, in the minor-7 shape, the minor-8 detail shape and the
// audio-profile shape (lossless, audioProfileVersion 1). R-R3-43: the
// receiver-audio-context (receiverAudioVersion 1).
//
// =================================================================

#include <QtTest>

#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/RemoteAudioContext.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <limits>

using namespace NereusSDR;

namespace NereusSDR {

// Readable QCOMPARE failures for the profile value type (found by ADL).
char* toString(const OpusEncoderProfile& profile)
{
    return QTest::toString(QStringLiteral("{%1 Hz, %2 ch, %3 samples, %4 bit/s, %5 Hz}")
                               .arg(profile.sampleRate)
                               .arg(profile.channels)
                               .arg(profile.frameSamples)
                               .arg(profile.targetBitrate)
                               .arg(profile.audioBandwidthHz));
}

} // namespace NereusSDR

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";
constexpr quint32 kMaxU32 = std::numeric_limits<quint32>::max();

const QList<RemoteAudioOffReason> kAllReasons{
    RemoteAudioOffReason::ClientDisabled, RemoteAudioOffReason::MediaNotReady,
    RemoteAudioOffReason::RadioOffline, RemoteAudioOffReason::EncoderUnavailable};

OpusEncoderProfile defaultProfile()
{
    return {48'000, 2, 1'920, 48'000, 20'000}; // R-R3-21: fullband default
}

// The exact encoder object R-R3-23 specifies for the default encoder.
// R-R3-21: the default encoder is 48 kbit/s fullband.
QJsonObject defaultEncoderJson()
{
    return {{QStringLiteral("codec"), QStringLiteral("opus")},
            {QStringLiteral("sampleRate"), 48000},
            {QStringLiteral("channels"), 2},
            {QStringLiteral("frameSamples"), 1920},
            {QStringLiteral("targetBitrate"), 48000},
            {QStringLiteral("audioBandwidthHz"), 20000}};
}

// The context exactly as DaemonMediaController::sendAudioContext built it
// before minor 8: the reference every minor-7 GUI parses.
QJsonObject todaysContext(quint32 revision, quint32 generation, bool enabled, quint32 ssrc,
                          quint16 firstSequence, quint32 firstTimestamp)
{
    return {
        {QStringLiteral("op"), QStringLiteral("audio-context")},
        {QStringLiteral("connectionId"), QString::fromLatin1(kConnectionId)},
        {QStringLiteral("revision"), static_cast<qint64>(revision)},
        {QStringLiteral("generation"), static_cast<qint64>(generation)},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("ssrc"), static_cast<qint64>(ssrc)},
        {QStringLiteral("firstSequence"), static_cast<qint64>(firstSequence)},
        {QStringLiteral("firstTimestamp"), static_cast<qint64>(firstTimestamp)},
    };
}

RemoteAudioContextMessage sampleMessage(bool enabled)
{
    RemoteAudioContextMessage message;
    message.connectionId = QString::fromLatin1(kConnectionId);
    message.revision = 41;
    message.generation = 7;
    message.enabled = enabled;
    message.ssrc = 0x6e657265U;
    message.firstSequence = 65'000;
    message.firstTimestamp = 0xfffff000U;
    return message;
}

QJsonObject sampleLegacy(bool enabled)
{
    return todaysContext(41, 7, enabled, 0x6e657265U, 65'000, 0xfffff000U);
}

QJsonObject withKey(QJsonObject object, const QString& key, const QJsonValue& value)
{
    object.insert(key, value);
    return object;
}

QJsonObject withoutKey(QJsonObject object, const QString& key)
{
    object.remove(key);
    return object;
}

QJsonObject sampleDetailEnabled()
{
    return withKey(sampleLegacy(true), QStringLiteral("encoder"), defaultEncoderJson());
}

QJsonObject sampleDetailDisabled(const QString& reason = QStringLiteral("client-disabled"))
{
    return withKey(sampleLegacy(false), QStringLiteral("reason"), reason);
}

QByteArray wire(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// Through JSON text and back, the way the session transport carries it.
QJsonObject overTheWire(const QJsonObject& object)
{
    return QJsonDocument::fromJson(wire(object)).object();
}

void compareMessages(const RemoteAudioContextMessage& actual,
                     const RemoteAudioContextMessage& expected)
{
    QCOMPARE(actual.connectionId, expected.connectionId);
    QCOMPARE(actual.revision, expected.revision);
    QCOMPARE(actual.generation, expected.generation);
    QCOMPARE(actual.enabled, expected.enabled);
    QCOMPARE(actual.ssrc, expected.ssrc);
    QCOMPARE(actual.firstSequence, expected.firstSequence);
    QCOMPARE(actual.firstTimestamp, expected.firstTimestamp);
    QCOMPARE(actual.encoder.has_value(), expected.encoder.has_value());
    if (actual.encoder && expected.encoder) {
        QCOMPARE(*actual.encoder, *expected.encoder);
    }
    QCOMPARE(actual.offReason.has_value(), expected.offReason.has_value());
    if (actual.offReason && expected.offReason) {
        QCOMPARE(*actual.offReason, *expected.offReason);
    }
}

} // namespace

class TstRemoteAudioContext : public QObject {
    Q_OBJECT

private slots:
    void offReasonWireStringsAreExact()
    {
        QCOMPARE(remoteAudioOffReasonToWire(RemoteAudioOffReason::ClientDisabled),
                 QStringLiteral("client-disabled"));
        QCOMPARE(remoteAudioOffReasonToWire(RemoteAudioOffReason::MediaNotReady),
                 QStringLiteral("media-not-ready"));
        QCOMPARE(remoteAudioOffReasonToWire(RemoteAudioOffReason::RadioOffline),
                 QStringLiteral("radio-offline"));
        QCOMPARE(remoteAudioOffReasonToWire(RemoteAudioOffReason::EncoderUnavailable),
                 QStringLiteral("encoder-unavailable"));
        for (RemoteAudioOffReason reason : kAllReasons) {
            const std::optional<RemoteAudioOffReason> parsed =
                remoteAudioOffReasonFromWire(QJsonValue(remoteAudioOffReasonToWire(reason)));
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, reason);
        }
    }

    void offReasonRejectsEverythingElse_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("capitalised") << QJsonValue(QStringLiteral("Client-Disabled"));
        QTest::newRow("underscore") << QJsonValue(QStringLiteral("client_disabled"));
        QTest::newRow("trailing space") << QJsonValue(QStringLiteral("radio-offline "));
        QTest::newRow("words") << QJsonValue(QStringLiteral("radio offline"));
        QTest::newRow("empty") << QJsonValue(QString());
        QTest::newRow("unknown") << QJsonValue(QStringLiteral("muted"));
        QTest::newRow("number") << QJsonValue(1);
        QTest::newRow("bool") << QJsonValue(true);
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("undefined") << QJsonValue(QJsonValue::Undefined);
        QTest::newRow("array") << QJsonValue(QJsonArray{QStringLiteral("client-disabled")});
        QTest::newRow("object") << QJsonValue(
            QJsonObject{{QStringLiteral("reason"), QStringLiteral("client-disabled")}});
    }

    void offReasonRejectsEverythingElse()
    {
        QFETCH(QJsonValue, value);
        QVERIFY(!remoteAudioOffReasonFromWire(value).has_value());
        QVERIFY(!receiverAudioOffReasonFromWire(value).has_value());
    }

    // R-R3-43: two reasons belong to receiver contexts only. The main
    // context's parser refuses them, so an audio-context carrying one is
    // refused whole, exactly as before they existed.
    void receiverReasonsAreReceiverOnly()
    {
        QCOMPARE(remoteAudioOffReasonToWire(RemoteAudioOffReason::SliceRemoved),
                 QStringLiteral("slice-removed"));
        QCOMPARE(remoteAudioOffReasonToWire(RemoteAudioOffReason::ReceiverLimit),
                 QStringLiteral("receiver-limit"));
        for (RemoteAudioOffReason reason :
             {RemoteAudioOffReason::SliceRemoved, RemoteAudioOffReason::ReceiverLimit}) {
            const QJsonValue wireName(remoteAudioOffReasonToWire(reason));
            QVERIFY(!remoteAudioOffReasonFromWire(wireName).has_value());
            QCOMPARE(receiverAudioOffReasonFromWire(wireName), std::optional{reason});
            const QJsonObject mainContext =
                sampleDetailDisabled(remoteAudioOffReasonToWire(reason));
            QVERIFY(!decodeRemoteAudioContext(mainContext, true).has_value());
            QVERIFY(!decodeRemoteAudioContext(
                withKey(mainContext, QStringLiteral("profile"), QStringLiteral("opus")),
                true, true).has_value());
        }
        for (RemoteAudioOffReason reason : kAllReasons) {
            QCOMPARE(receiverAudioOffReasonFromWire(
                         QJsonValue(remoteAudioOffReasonToWire(reason))),
                     std::optional{reason});
        }
    }

    // The receiver-audio-context golden: the audio-profile shape with op
    // "receiver-audio-context" and sliceId, key for key and type for type.
    void receiverContextShapeIsTheProfileShapeWithTheSlice()
    {
        RemoteReceiverAudioContextMessage on;
        on.sliceId = 3;
        on.context = sampleMessage(true);
        on.context.profile = RemoteAudioProfile::Opus;
        on.context.encoder = defaultProfile();
        on.context.profileRefusal = RemoteAudioProfileRefusal::Unavailable;
        const QJsonObject onWire = encodeReceiverAudioContext(on);
        QCOMPARE(wire(onWire),
                 QByteArrayLiteral("{\"connectionId\":\"11111111-2222-4333-8444-555555555555\","
                                   "\"enabled\":true,\"encoder\":{\"audioBandwidthHz\":20000,"
                                   "\"channels\":2,\"codec\":\"opus\",\"frameSamples\":1920,"
                                   "\"sampleRate\":48000,\"targetBitrate\":48000},"
                                   "\"firstSequence\":65000,\"firstTimestamp\":4294963200,"
                                   "\"generation\":7,\"op\":\"receiver-audio-context\","
                                   "\"profile\":\"opus\",\"profileRefusal\":\"lossless-unavailable\","
                                   "\"revision\":41,\"sliceId\":3,\"ssrc\":1852142181}"));

        RemoteReceiverAudioContextMessage lossless = on;
        lossless.context.profile = RemoteAudioProfile::Lossless;
        lossless.context.losslessEncoder = l16EncoderProfile();
        lossless.context.encoder.reset();
        lossless.context.profileRefusal.reset();

        RemoteReceiverAudioContextMessage limit;
        limit.sliceId = 0;
        limit.context.connectionId = QString::fromLatin1(kConnectionId);
        limit.context.revision = 2;
        limit.context.generation = 9;
        limit.context.offReason = RemoteAudioOffReason::ReceiverLimit;
        limit.context.profile = RemoteAudioProfile::Lossless;
        const QJsonObject limitWire = encodeReceiverAudioContext(limit);
        QCOMPARE(wire(limitWire),
                 QByteArrayLiteral("{\"connectionId\":\"11111111-2222-4333-8444-555555555555\","
                                   "\"enabled\":false,\"firstSequence\":0,\"firstTimestamp\":0,"
                                   "\"generation\":9,\"op\":\"receiver-audio-context\","
                                   "\"profile\":\"lossless\",\"reason\":\"receiver-limit\","
                                   "\"revision\":2,\"sliceId\":0,\"ssrc\":0}"));

        RemoteReceiverAudioContextMessage removed = limit;
        removed.sliceId = 1;
        removed.context.ssrc = 0x12345678U;
        removed.context.firstSequence = 17;
        removed.context.firstTimestamp = 3840;
        removed.context.offReason = RemoteAudioOffReason::SliceRemoved;
        removed.context.profile = RemoteAudioProfile::Opus;

        for (const RemoteReceiverAudioContextMessage& message : {on, lossless, limit, removed}) {
            const QJsonObject encoded = overTheWire(encodeReceiverAudioContext(message));
            const std::optional<RemoteReceiverAudioContextMessage> decoded =
                decodeReceiverAudioContext(encoded);
            QVERIFY2(decoded.has_value(), wire(encoded).constData());
            QCOMPARE(decoded->sliceId, message.sliceId);
            compareMessages(decoded->context, message.context);
            QCOMPARE(decoded->context.profile,
                     message.context.profile.value_or(RemoteAudioProfile::Opus));
            QCOMPARE(decoded->context.losslessEncoder.has_value(),
                     message.context.profile == RemoteAudioProfile::Lossless
                         && message.context.enabled);
            // Never mistaken for the main context, in any shape.
            QVERIFY(!decodeRemoteAudioContext(encoded, false).has_value());
            QVERIFY(!decodeRemoteAudioContext(encoded, true).has_value());
            QVERIFY(!decodeRemoteAudioContext(encoded, true, true).has_value());
        }
        // And the main context is never taken for a receiver one.
        QVERIFY(!decodeReceiverAudioContext(
            withKey(withKey(sampleDetailEnabled(), QStringLiteral("profile"),
                            QStringLiteral("opus")),
                    QStringLiteral("sliceId"), 1)).has_value());
    }

    void receiverContextRejectsMismatches_data()
    {
        QTest::addColumn<QJsonObject>("payload");
        RemoteReceiverAudioContextMessage on;
        on.sliceId = 2;
        on.context = sampleMessage(true);
        on.context.encoder = defaultProfile();
        const QJsonObject good = encodeReceiverAudioContext(on);
        RemoteReceiverAudioContextMessage off;
        off.sliceId = 2;
        off.context = sampleMessage(false);
        off.context.offReason = RemoteAudioOffReason::SliceRemoved;
        const QJsonObject goodOff = encodeReceiverAudioContext(off);
        QVERIFY(decodeReceiverAudioContext(good).has_value());
        QVERIFY(decodeReceiverAudioContext(goodOff).has_value());
        QVERIFY(decodeReceiverAudioContext(
            withKey(goodOff, QStringLiteral("ssrc"), 0)).has_value());

        QTest::newRow("main-op")
            << withKey(good, QStringLiteral("op"), QStringLiteral("audio-context"));
        QTest::newRow("missing-slice") << withoutKey(good, QStringLiteral("sliceId"));
        QTest::newRow("negative-slice") << withKey(good, QStringLiteral("sliceId"), -1);
        QTest::newRow("fractional-slice") << withKey(good, QStringLiteral("sliceId"), 1.5);
        QTest::newRow("string-slice")
            << withKey(good, QStringLiteral("sliceId"), QStringLiteral("2"));
        QTest::newRow("missing-profile") << withoutKey(good, QStringLiteral("profile"));
        QTest::newRow("extra-key") << withKey(good, QStringLiteral("extra"), 1);
        QTest::newRow("enabled-with-no-stream-id")
            << withKey(good, QStringLiteral("ssrc"), 0);
        QTest::newRow("unknown-reason")
            << withKey(goodOff, QStringLiteral("reason"), QStringLiteral("muted"));
        QTest::newRow("reason-while-on")
            << withKey(good, QStringLiteral("reason"), QStringLiteral("receiver-limit"));
    }

    void receiverContextRejectsMismatches()
    {
        QFETCH(QJsonObject, payload);
        QVERIFY(!decodeReceiverAudioContext(payload).has_value());
    }

    void defaultEncoderProducesTheExactEncoderObject()
    {
        OpusAudioEncoder encoder;
        QVERIFY(encoder.isReady());
        const std::optional<OpusEncoderProfile> profile = encoder.profile();
        QVERIFY(profile.has_value());
        const QJsonObject json = remoteAudioEncoderToJson(*profile);
        QCOMPARE(json, defaultEncoderJson());
        // Integral JSON numbers on the wire, with no fractional rendering.
        QCOMPARE(wire(json), wire(defaultEncoderJson()));
        QVERIFY(!wire(json).contains('.'));

        const std::optional<OpusEncoderProfile> parsed =
            remoteAudioEncoderFromJson(overTheWire(json));
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, *profile);
    }

    void encoderObjectRoundTripsTheValidRange_data()
    {
        QTest::addColumn<int>("targetBitrate");
        QTest::addColumn<int>("audioBandwidthHz");
        QTest::newRow("lowest bitrate") << 6'000 << 8'000;
        QTest::newRow("default") << 48'000 << 20'000;
        QTest::newRow("alternate") << 24'000 << 8'000;
        QTest::newRow("highest bitrate") << 510'000 << 8'000;
        QTest::newRow("narrowband") << 24'000 << 4'000;
        QTest::newRow("mediumband") << 24'000 << 6'000;
        QTest::newRow("superwideband") << 24'000 << 12'000;
        QTest::newRow("fullband") << 24'000 << 20'000;
    }

    void encoderObjectRoundTripsTheValidRange()
    {
        QFETCH(int, targetBitrate);
        QFETCH(int, audioBandwidthHz);
        OpusEncoderProfile profile = defaultProfile();
        profile.targetBitrate = targetBitrate;
        profile.audioBandwidthHz = audioBandwidthHz;
        const std::optional<OpusEncoderProfile> parsed =
            remoteAudioEncoderFromJson(overTheWire(remoteAudioEncoderToJson(profile)));
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, profile);
    }

    void encoderObjectRejectsAnythingElse_data()
    {
        QTest::addColumn<QJsonValue>("value");
        const QJsonObject valid = defaultEncoderJson();
        QTest::newRow("extra key") << QJsonValue(withKey(valid, QStringLiteral("fec"), false));
        for (const QString& key : valid.keys()) {
            QTest::addRow("missing %s", qPrintable(key)) << QJsonValue(withoutKey(valid, key));
        }
        QTest::newRow("string number")
            << QJsonValue(withKey(valid, QStringLiteral("sampleRate"), QStringLiteral("48000")));
        QTest::newRow("44100 Hz")
            << QJsonValue(withKey(valid, QStringLiteral("sampleRate"), 44100));
        QTest::newRow("one channel")
            << QJsonValue(withKey(valid, QStringLiteral("channels"), 1));
        QTest::newRow("bool channels")
            << QJsonValue(withKey(valid, QStringLiteral("channels"), true));
        QTest::newRow("20 ms frames")
            << QJsonValue(withKey(valid, QStringLiteral("frameSamples"), 960));
        QTest::newRow("fractional bitrate")
            << QJsonValue(withKey(valid, QStringLiteral("targetBitrate"), 24000.5));
        QTest::newRow("bitrate 510001")
            << QJsonValue(withKey(valid, QStringLiteral("targetBitrate"), 510001));
        QTest::newRow("bitrate 5999")
            << QJsonValue(withKey(valid, QStringLiteral("targetBitrate"), 5999));
        QTest::newRow("negative bitrate")
            << QJsonValue(withKey(valid, QStringLiteral("targetBitrate"), -24000));
        QTest::newRow("bandwidth 16000")
            << QJsonValue(withKey(valid, QStringLiteral("audioBandwidthHz"), 16000));
        QTest::newRow("bandwidth 0")
            << QJsonValue(withKey(valid, QStringLiteral("audioBandwidthHz"), 0));
        QTest::newRow("fractional bandwidth")
            << QJsonValue(withKey(valid, QStringLiteral("audioBandwidthHz"), 8000.5));
        QTest::newRow("codec case")
            << QJsonValue(withKey(valid, QStringLiteral("codec"), QStringLiteral("OPUS")));
        QTest::newRow("codec number") << QJsonValue(withKey(valid, QStringLiteral("codec"), 1));
        QTest::newRow("string") << QJsonValue(QStringLiteral("opus"));
        QTest::newRow("number") << QJsonValue(48000);
        QTest::newRow("array") << QJsonValue(QJsonArray{valid});
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("undefined") << QJsonValue(QJsonValue::Undefined);
    }

    void encoderObjectRejectsAnythingElse()
    {
        QFETCH(QJsonValue, value);
        QVERIFY(!remoteAudioEncoderFromJson(value).has_value());
    }

    void legacyEncodingIsTodaysEightKeys()
    {
        for (bool enabled : {true, false}) {
            RemoteAudioContextMessage message = sampleMessage(enabled);
            // Detail values present in the message are never written for a
            // minor-7 peer.
            message.encoder = defaultProfile();
            message.offReason = RemoteAudioOffReason::RadioOffline;
            const QJsonObject encoded = encodeRemoteAudioContext(message, false);
            QCOMPARE(encoded.size(), 8);
            QCOMPARE(encoded, sampleLegacy(enabled));
            QCOMPARE(wire(encoded), wire(sampleLegacy(enabled)));
        }
    }

    void detailEncodingAddsOnlyTheEncoderOrTheReason()
    {
        RemoteAudioContextMessage on = sampleMessage(true);
        on.encoder = defaultProfile();
        on.offReason = RemoteAudioOffReason::MediaNotReady; // not written when enabled
        const QJsonObject enabled = encodeRemoteAudioContext(on, true);
        QCOMPARE(enabled.size(), 9);
        QCOMPARE(enabled.value(QStringLiteral("encoder")).toObject(), defaultEncoderJson());
        QVERIFY(!enabled.contains(QStringLiteral("reason")));
        QCOMPARE(withoutKey(enabled, QStringLiteral("encoder")), sampleLegacy(true));

        for (RemoteAudioOffReason reason : kAllReasons) {
            RemoteAudioContextMessage off = sampleMessage(false);
            off.offReason = reason;
            off.encoder = defaultProfile(); // not written when disabled
            const QJsonObject disabled = encodeRemoteAudioContext(off, true);
            QCOMPARE(disabled.size(), 9);
            QCOMPARE(disabled.value(QStringLiteral("reason")).toString(),
                     remoteAudioOffReasonToWire(reason));
            QVERIFY(!disabled.contains(QStringLiteral("encoder")));
            QCOMPARE(withoutKey(disabled, QStringLiteral("reason")), sampleLegacy(false));
        }
    }

    // An enabled context with no encoder profile would reach a minor-8 GUI
    // with eight keys and be refused, leaving it silent with no reason. It
    // goes out disabled with reason encoder-unavailable instead, a shape
    // the GUI decodes; the legacy shape is unchanged.
    void enabledWithoutEncoderGoesOutAsEncoderUnavailable()
    {
        RemoteAudioContextMessage on = sampleMessage(true);
        on.encoder.reset();
        const QJsonObject detail = overTheWire(encodeRemoteAudioContext(on, true));
        QCOMPARE(detail.size(), 9);
        QCOMPARE(detail, sampleDetailDisabled(QStringLiteral("encoder-unavailable")));
        const std::optional<RemoteAudioContextMessage> decoded =
            decodeRemoteAudioContext(detail, true);
        QVERIFY(decoded.has_value());
        QVERIFY(!decoded->enabled);
        QCOMPARE(decoded->offReason, std::optional(RemoteAudioOffReason::EncoderUnavailable));

        QCOMPARE(encodeRemoteAudioContext(on, false), sampleLegacy(true));
    }

    void legacyDecodingAcceptsWhatTheGuiAlwaysAccepted_data()
    {
        QTest::addColumn<QJsonObject>("payload");
        QTest::addColumn<quint32>("revision");
        QTest::addColumn<quint32>("generation");
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<quint32>("ssrc");
        QTest::addColumn<quint16>("firstSequence");
        QTest::addColumn<quint32>("firstTimestamp");
        QTest::newRow("lower bounds")
            << todaysContext(1, 1, false, 1, 0, 0) << 1U << 1U << false << 1U
            << quint16{0} << 0U;
        QTest::newRow("upper bounds")
            << todaysContext(kMaxU32, kMaxU32, true, kMaxU32, 65535, kMaxU32) << kMaxU32
            << kMaxU32 << true << kMaxU32 << quint16{65535} << kMaxU32;
        // Today's parser reads any integral JSON number, however it is stored.
        QJsonObject doubles = todaysContext(3, 4, true, 5, 6, 7);
        for (const char* key : {"revision", "generation", "ssrc", "firstSequence",
                                "firstTimestamp"}) {
            doubles.insert(QLatin1String(key),
                           doubles.value(QLatin1String(key)).toDouble());
        }
        QTest::newRow("numbers stored as doubles")
            << doubles << 3U << 4U << true << 5U << quint16{6} << 7U;
    }

    void legacyDecodingAcceptsWhatTheGuiAlwaysAccepted()
    {
        QFETCH(QJsonObject, payload);
        QFETCH(quint32, revision);
        QFETCH(quint32, generation);
        QFETCH(bool, enabled);
        QFETCH(quint32, ssrc);
        QFETCH(quint16, firstSequence);
        QFETCH(quint32, firstTimestamp);
        for (const QJsonObject& candidate : {payload, overTheWire(payload)}) {
            const std::optional<RemoteAudioContextMessage> decoded =
                decodeRemoteAudioContext(candidate, false);
            QVERIFY(decoded.has_value());
            QCOMPARE(decoded->connectionId, QString::fromLatin1(kConnectionId));
            QCOMPARE(decoded->revision, revision);
            QCOMPARE(decoded->generation, generation);
            QCOMPARE(decoded->enabled, enabled);
            QCOMPARE(decoded->ssrc, ssrc);
            QCOMPARE(decoded->firstSequence, firstSequence);
            QCOMPARE(decoded->firstTimestamp, firstTimestamp);
            QVERIFY(!decoded->encoder.has_value());
            QVERIFY(!decoded->offReason.has_value());
        }
    }

    // The eight common fields are validated identically in both shapes, so
    // every defect is checked against a valid legacy and a valid detail base.
    void fieldDefectsAreRejectedInBothShapes_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QJsonValue>("value"); // Undefined removes the key
        for (const char* key : {"op", "connectionId", "revision", "generation", "enabled",
                                "ssrc", "firstSequence", "firstTimestamp"}) {
            QTest::addRow("missing %s", key)
                << QString::fromLatin1(key) << QJsonValue(QJsonValue::Undefined);
        }
        QTest::newRow("op audio") << QStringLiteral("op") << QJsonValue(QStringLiteral("audio"));
        QTest::newRow("op number") << QStringLiteral("op") << QJsonValue(1);
        QTest::newRow("connectionId number") << QStringLiteral("connectionId") << QJsonValue(1);
        QTest::newRow("connectionId null")
            << QStringLiteral("connectionId") << QJsonValue(QJsonValue::Null);
        QTest::newRow("revision zero") << QStringLiteral("revision") << QJsonValue(0);
        QTest::newRow("revision negative") << QStringLiteral("revision") << QJsonValue(-1);
        QTest::newRow("revision fractional") << QStringLiteral("revision") << QJsonValue(1.5);
        QTest::newRow("revision too large")
            << QStringLiteral("revision") << QJsonValue(qint64{4294967296});
        QTest::newRow("revision string")
            << QStringLiteral("revision") << QJsonValue(QStringLiteral("1"));
        QTest::newRow("generation zero") << QStringLiteral("generation") << QJsonValue(0);
        QTest::newRow("generation too large")
            << QStringLiteral("generation") << QJsonValue(qint64{4294967296});
        QTest::newRow("ssrc zero") << QStringLiteral("ssrc") << QJsonValue(0);
        QTest::newRow("ssrc bool") << QStringLiteral("ssrc") << QJsonValue(true);
        QTest::newRow("enabled number") << QStringLiteral("enabled") << QJsonValue(1);
        QTest::newRow("enabled string")
            << QStringLiteral("enabled") << QJsonValue(QStringLiteral("true"));
        QTest::newRow("firstSequence 65536")
            << QStringLiteral("firstSequence") << QJsonValue(65536);
        QTest::newRow("firstSequence negative")
            << QStringLiteral("firstSequence") << QJsonValue(-1);
        QTest::newRow("firstSequence fractional")
            << QStringLiteral("firstSequence") << QJsonValue(0.5);
        QTest::newRow("firstSequence string")
            << QStringLiteral("firstSequence") << QJsonValue(QStringLiteral("0"));
        QTest::newRow("firstTimestamp too large")
            << QStringLiteral("firstTimestamp") << QJsonValue(qint64{4294967296});
        QTest::newRow("firstTimestamp negative")
            << QStringLiteral("firstTimestamp") << QJsonValue(-1);
        QTest::newRow("firstTimestamp fractional")
            << QStringLiteral("firstTimestamp") << QJsonValue(0.5);
    }

    void fieldDefectsAreRejectedInBothShapes()
    {
        QFETCH(QString, key);
        QFETCH(QJsonValue, value);
        const auto defect = [&](const QJsonObject& base) {
            return value.isUndefined() ? withoutKey(base, key) : withKey(base, key, value);
        };
        for (bool enabled : {true, false}) {
            const QJsonObject legacy = sampleLegacy(enabled);
            const QJsonObject detail = enabled ? sampleDetailEnabled() : sampleDetailDisabled();
            QVERIFY(decodeRemoteAudioContext(legacy, false).has_value());
            QVERIFY(decodeRemoteAudioContext(detail, true).has_value());
            QVERIFY(!decodeRemoteAudioContext(defect(legacy), false).has_value());
            QVERIFY(!decodeRemoteAudioContext(defect(detail), true).has_value());
        }
    }

    void eachShapeIsAcceptedOnlyWhereItWasNegotiated_data()
    {
        QTest::addColumn<QJsonObject>("payload");
        QTest::addColumn<bool>("legacyAccepts");
        QTest::addColumn<bool>("detailAccepts");
        QTest::newRow("eight keys enabled") << sampleLegacy(true) << true << false;
        QTest::newRow("eight keys disabled") << sampleLegacy(false) << true << false;
        QTest::newRow("enabled with encoder") << sampleDetailEnabled() << false << true;
        for (RemoteAudioOffReason reason : kAllReasons) {
            QTest::addRow("disabled, %s", qPrintable(remoteAudioOffReasonToWire(reason)))
                << sampleDetailDisabled(remoteAudioOffReasonToWire(reason)) << false << true;
        }
        QTest::newRow("enabled with encoder and reason")
            << withKey(sampleDetailEnabled(), QStringLiteral("reason"),
                       QStringLiteral("client-disabled"))
            << false << false;
        QTest::newRow("disabled with reason and encoder")
            << withKey(sampleDetailDisabled(), QStringLiteral("encoder"), defaultEncoderJson())
            << false << false;
        QTest::newRow("enabled with reason instead of encoder")
            << withKey(sampleLegacy(true), QStringLiteral("reason"),
                       QStringLiteral("client-disabled"))
            << false << false;
        QTest::newRow("disabled with encoder instead of reason")
            << withKey(sampleLegacy(false), QStringLiteral("encoder"), defaultEncoderJson())
            << false << false;
        QTest::newRow("enabled with an unrelated ninth key")
            << withKey(sampleLegacy(true), QStringLiteral("codec"), QStringLiteral("opus"))
            << false << false;
        QTest::newRow("disabled with an unrelated ninth key")
            << withKey(sampleLegacy(false), QStringLiteral("why"), QStringLiteral("muted"))
            << false << false;
        QTest::newRow("unknown reason")
            << sampleDetailDisabled(QStringLiteral("muted")) << false << false;
        QTest::newRow("reason not a string")
            << withKey(sampleLegacy(false), QStringLiteral("reason"), 1) << false << false;
        QTest::newRow("encoder at 44100 Hz")
            << withKey(sampleLegacy(true), QStringLiteral("encoder"),
                       withKey(defaultEncoderJson(), QStringLiteral("sampleRate"), 44100))
            << false << false;
        QTest::newRow("encoder with an extra key")
            << withKey(sampleLegacy(true), QStringLiteral("encoder"),
                       withKey(defaultEncoderJson(), QStringLiteral("fec"), false))
            << false << false;
        QTest::newRow("encoder not an object")
            << withKey(sampleLegacy(true), QStringLiteral("encoder"), QStringLiteral("opus"))
            << false << false;
    }

    void eachShapeIsAcceptedOnlyWhereItWasNegotiated()
    {
        QFETCH(QJsonObject, payload);
        QFETCH(bool, legacyAccepts);
        QFETCH(bool, detailAccepts);
        QCOMPARE(decodeRemoteAudioContext(payload, false).has_value(), legacyAccepts);
        QCOMPARE(decodeRemoteAudioContext(payload, true).has_value(), detailAccepts);
    }

    void encodeThenDecodeRoundTripsEveryField_data()
    {
        QTest::addColumn<bool>("detailNegotiated");
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<int>("reason"); // -1 when enabled
        for (bool detail : {false, true}) {
            const char* shape = detail ? "detail" : "legacy";
            QTest::addRow("%s enabled", shape) << detail << true << -1;
            for (RemoteAudioOffReason reason : kAllReasons) {
                QTest::addRow("%s disabled, %s", shape,
                              qPrintable(remoteAudioOffReasonToWire(reason)))
                    << detail << false << static_cast<int>(reason);
            }
        }
    }

    void encodeThenDecodeRoundTripsEveryField()
    {
        QFETCH(bool, detailNegotiated);
        QFETCH(bool, enabled);
        QFETCH(int, reason);
        RemoteAudioContextMessage sent = sampleMessage(enabled);
        sent.revision = kMaxU32;
        sent.ssrc = kMaxU32 - 1;
        OpusEncoderProfile alternate = defaultProfile();
        alternate.targetBitrate = 48'000;
        if (enabled) {
            sent.encoder = alternate;
        } else {
            sent.offReason = static_cast<RemoteAudioOffReason>(reason);
        }
        const QJsonObject encoded = encodeRemoteAudioContext(sent, detailNegotiated);
        QCOMPARE(encoded.size(), detailNegotiated ? 9 : 8);
        const std::optional<RemoteAudioContextMessage> received =
            decodeRemoteAudioContext(overTheWire(encoded), detailNegotiated);
        QVERIFY(received.has_value());
        RemoteAudioContextMessage expected = sent;
        if (!detailNegotiated) {
            // A minor-7 exchange carries no detail at all.
            expected.encoder.reset();
            expected.offReason.reset();
        }
        compareMessages(*received, expected);
    }

    // ---- Audio profile (R-R3-23, audioProfileVersion 1) ----

    void profileWireStringsAreExact()
    {
        QCOMPARE(remoteAudioProfileToWire(RemoteAudioProfile::Opus), QStringLiteral("opus"));
        QCOMPARE(remoteAudioProfileToWire(RemoteAudioProfile::Lossless),
                 QStringLiteral("lossless"));
        QCOMPARE(remoteAudioProfileRefusalToWire(RemoteAudioProfileRefusal::NotAllowed),
                 QStringLiteral("lossless-not-allowed"));
        QCOMPARE(remoteAudioProfileRefusalToWire(RemoteAudioProfileRefusal::Unavailable),
                 QStringLiteral("lossless-unavailable"));
        QCOMPARE(remoteAudioProfileFromWire(QStringLiteral("lossless")),
                 std::optional{RemoteAudioProfile::Lossless});
        for (const QJsonValue& bad : {QJsonValue(QStringLiteral("Lossless")),
                                      QJsonValue(QStringLiteral("l16")), QJsonValue(1),
                                      QJsonValue(true), QJsonValue()}) {
            QVERIFY(!remoteAudioProfileFromWire(bad).has_value());
            QVERIFY(!remoteAudioProfileRefusalFromWire(bad).has_value());
        }
    }

    // The exact lossless encoder object Task 6 consumes.
    void l16EncoderObjectIsExactAndStrict()
    {
        const QJsonObject expected{{QStringLiteral("codec"), QStringLiteral("l16")},
                                   {QStringLiteral("sampleRate"), 48000},
                                   {QStringLiteral("channels"), 2},
                                   {QStringLiteral("frameSamples"), 192},
                                   {QStringLiteral("bitsPerSample"), 16},
                                   {QStringLiteral("payloadType"), 96}};
        QCOMPARE(remoteAudioL16EncoderToJson(l16EncoderProfile()), expected);
        QCOMPARE(remoteAudioL16EncoderFromJson(overTheWire(expected)),
                 std::optional{l16EncoderProfile()});
        QVERIFY(!remoteAudioL16EncoderFromJson(defaultEncoderJson()).has_value());
        QVERIFY(!remoteAudioEncoderFromJson(expected).has_value());
        for (const QString& key : expected.keys()) {
            QVERIFY(!remoteAudioL16EncoderFromJson(withoutKey(expected, key)).has_value());
        }
        QVERIFY(!remoteAudioL16EncoderFromJson(
            withKey(expected, QStringLiteral("extra"), 1)).has_value());
        QVERIFY(!remoteAudioL16EncoderFromJson(
            withKey(expected, QStringLiteral("frameSamples"), 1920)).has_value());
        QVERIFY(!remoteAudioL16EncoderFromJson(
            withKey(expected, QStringLiteral("payloadType"), 111)).has_value());
        QVERIFY(!remoteAudioL16EncoderFromJson(
            withKey(expected, QStringLiteral("bitsPerSample"), 16.5)).has_value());
        QVERIFY(!remoteAudioL16EncoderFromJson(
            withKey(expected, QStringLiteral("channels"), QStringLiteral("2"))).has_value());
    }

    // Without the profile negotiated nothing about profiles is written,
    // whatever the message holds: an older GUI sees today's shapes (golden).
    void profileFieldsNeverReachAnOlderGui()
    {
        for (bool enabled : {true, false}) {
            RemoteAudioContextMessage message = sampleMessage(enabled);
            message.encoder = defaultProfile();
            message.offReason = RemoteAudioOffReason::ClientDisabled;
            message.profile = RemoteAudioProfile::Opus;
            message.profileRefusal = RemoteAudioProfileRefusal::NotAllowed;
            message.losslessEncoder = l16EncoderProfile();
            QCOMPARE(encodeRemoteAudioContext(message, false), sampleLegacy(enabled));
            QCOMPARE(encodeRemoteAudioContext(message, false, true), sampleLegacy(enabled));
            QCOMPARE(wire(encodeRemoteAudioContext(message, true)),
                     wire(enabled ? sampleDetailEnabled() : sampleDetailDisabled()));
        }
    }

    void profileShapeCarriesProfileEncoderAndRefusal()
    {
        const QJsonObject l16 = remoteAudioL16EncoderToJson(l16EncoderProfile());

        RemoteAudioContextMessage lossless = sampleMessage(true);
        lossless.profile = RemoteAudioProfile::Lossless;
        lossless.losslessEncoder = l16EncoderProfile();
        lossless.encoder = defaultProfile(); // unused when lossless
        const QJsonObject on = encodeRemoteAudioContext(lossless, true, true);
        QCOMPARE(on, withKey(withKey(sampleLegacy(true), QStringLiteral("encoder"), l16),
                             QStringLiteral("profile"), QStringLiteral("lossless")));

        RemoteAudioContextMessage refused = sampleMessage(true);
        refused.profile = RemoteAudioProfile::Opus;
        refused.encoder = defaultProfile();
        refused.profileRefusal = RemoteAudioProfileRefusal::NotAllowed;
        const QJsonObject opus = encodeRemoteAudioContext(refused, true, true);
        QCOMPARE(opus, withKey(withKey(sampleDetailEnabled(), QStringLiteral("profile"),
                                       QStringLiteral("opus")),
                               QStringLiteral("profileRefusal"),
                               QStringLiteral("lossless-not-allowed")));

        // Absent profile encodes as opus; a refusal beside lossless is dropped.
        RemoteAudioContextMessage plain = sampleMessage(false);
        plain.offReason = RemoteAudioOffReason::MediaNotReady;
        QCOMPARE(encodeRemoteAudioContext(plain, true, true),
                 withKey(sampleDetailDisabled(QStringLiteral("media-not-ready")),
                         QStringLiteral("profile"), QStringLiteral("opus")));
        RemoteAudioContextMessage offLossless = plain;
        offLossless.profile = RemoteAudioProfile::Lossless;
        offLossless.profileRefusal = RemoteAudioProfileRefusal::Unavailable;
        QCOMPARE(encodeRemoteAudioContext(offLossless, true, true),
                 withKey(sampleDetailDisabled(QStringLiteral("media-not-ready")),
                         QStringLiteral("profile"), QStringLiteral("lossless")));

        // Lossless on with no packetiser profile goes out off, encoder-unavailable.
        RemoteAudioContextMessage missing = sampleMessage(true);
        missing.profile = RemoteAudioProfile::Lossless;
        QCOMPARE(encodeRemoteAudioContext(missing, true, true),
                 withKey(sampleDetailDisabled(QStringLiteral("encoder-unavailable")),
                         QStringLiteral("profile"), QStringLiteral("lossless")));

        // Every shape decodes back, and only as the profile shape.
        for (const RemoteAudioContextMessage& message : {lossless, refused, plain, offLossless}) {
            const QJsonObject encoded = overTheWire(encodeRemoteAudioContext(message, true, true));
            const std::optional<RemoteAudioContextMessage> decoded =
                decodeRemoteAudioContext(encoded, true, true);
            QVERIFY2(decoded.has_value(), wire(encoded).constData());
            QCOMPARE(decoded->profile, message.profile.value_or(RemoteAudioProfile::Opus));
            QCOMPARE(decoded->enabled, message.enabled);
            const bool isLossless = message.profile == RemoteAudioProfile::Lossless;
            QCOMPARE(decoded->losslessEncoder.has_value(), isLossless && message.enabled);
            QCOMPARE(decoded->encoder.has_value(), !isLossless && message.enabled);
            QCOMPARE(decoded->profileRefusal.has_value(),
                     !isLossless && message.profileRefusal.has_value());
            QVERIFY(!decodeRemoteAudioContext(encoded, true).has_value());
            QVERIFY(!decodeRemoteAudioContext(encoded, false).has_value());
        }
        const auto refusedDecoded =
            decodeRemoteAudioContext(overTheWire(opus), true, true);
        QCOMPARE(refusedDecoded->profileRefusal,
                 std::optional{RemoteAudioProfileRefusal::NotAllowed});
        // The older shapes are not the profile shape.
        QVERIFY(!decodeRemoteAudioContext(sampleDetailEnabled(), true, true).has_value());
        QVERIFY(!decodeRemoteAudioContext(sampleLegacy(true), true, true).has_value());
        // profileNegotiated without detail is the legacy shape.
        QVERIFY(decodeRemoteAudioContext(sampleLegacy(true), false, true).has_value());
    }

    void profileShapeRejectsMismatches_data()
    {
        QTest::addColumn<QJsonObject>("payload");
        const QJsonObject l16 = remoteAudioL16EncoderToJson(l16EncoderProfile());
        const QJsonObject losslessOn = withKey(
            withKey(sampleLegacy(true), QStringLiteral("encoder"), l16),
            QStringLiteral("profile"), QStringLiteral("lossless"));
        const QJsonObject opusOn = withKey(sampleDetailEnabled(), QStringLiteral("profile"),
                                           QStringLiteral("opus"));
        QTest::newRow("lossless-with-opus-encoder")
            << withKey(losslessOn, QStringLiteral("encoder"), defaultEncoderJson());
        QTest::newRow("opus-with-l16-encoder")
            << withKey(opusOn, QStringLiteral("encoder"), l16);
        QTest::newRow("missing-profile") << withoutKey(opusOn, QStringLiteral("profile"));
        QTest::newRow("unknown-profile")
            << withKey(opusOn, QStringLiteral("profile"), QStringLiteral("flac"));
        QTest::newRow("refusal-beside-lossless")
            << withKey(losslessOn, QStringLiteral("profileRefusal"),
                       QStringLiteral("lossless-not-allowed"));
        QTest::newRow("unknown-refusal")
            << withKey(opusOn, QStringLiteral("profileRefusal"), QStringLiteral("no"));
        QTest::newRow("extra-key") << withKey(opusOn, QStringLiteral("extra"), 1);
        QTest::newRow("lossless-with-reason")
            << withKey(losslessOn, QStringLiteral("reason"), QStringLiteral("client-disabled"));
    }

    void profileShapeRejectsMismatches()
    {
        QFETCH(QJsonObject, payload);
        QVERIFY(!decodeRemoteAudioContext(payload, true, true).has_value());
    }

    // R-R3-45: the headphones-audio-context is the audio-profile shape under
    // its own op, always with the headphones stream id. It carries the four
    // main reasons and no-headphones-receiver, never a receiver-only one,
    // and neither other context accepts it or its new reason.
    void headphonesContextRoundTripsAndStaysApart()
    {
        RemoteAudioContextMessage on = sampleMessage(true);
        on.encoder = defaultProfile();
        on.profile = RemoteAudioProfile::Opus;
        RemoteAudioContextMessage lossless = on;
        lossless.profile = RemoteAudioProfile::Lossless;
        lossless.losslessEncoder = l16EncoderProfile();
        lossless.encoder.reset();
        RemoteAudioContextMessage none = sampleMessage(false);
        none.offReason = RemoteAudioOffReason::NoHeadphonesReceiver;
        none.profile = RemoteAudioProfile::Opus;
        for (const RemoteAudioContextMessage& message : {on, lossless, none}) {
            const QJsonObject encoded = overTheWire(encodeHeadphonesAudioContext(message));
            QCOMPARE(encoded.value(QStringLiteral("op")).toString(),
                     QStringLiteral("headphones-audio-context"));
            const std::optional<RemoteAudioContextMessage> decoded =
                decodeHeadphonesAudioContext(encoded);
            QVERIFY2(decoded.has_value(), wire(encoded).constData());
            compareMessages(*decoded, message);
            QVERIFY(!decodeRemoteAudioContext(encoded, true, true).has_value());
            QVERIFY(!decodeReceiverAudioContext(
                withKey(encoded, QStringLiteral("sliceId"), 1)).has_value());
        }
        const QJsonObject noneWire = encodeHeadphonesAudioContext(none);
        QCOMPARE(noneWire.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("no-headphones-receiver"));
        // Its reason in the other contexts is malformed.
        QVERIFY(!decodeRemoteAudioContext(
            withKey(noneWire, QStringLiteral("op"), QStringLiteral("audio-context")), true, true)
                     .has_value());
        QVERIFY(!decodeReceiverAudioContext(
            withKey(withKey(noneWire, QStringLiteral("op"),
                            QStringLiteral("receiver-audio-context")),
                    QStringLiteral("sliceId"), 1)).has_value());
        // A receiver-only reason, no stream id or another key is refused.
        QVERIFY(!decodeHeadphonesAudioContext(
            withKey(noneWire, QStringLiteral("reason"), QStringLiteral("slice-removed")))
                     .has_value());
        QVERIFY(!decodeHeadphonesAudioContext(withKey(noneWire, QStringLiteral("ssrc"), 0))
                     .has_value());
        QVERIFY(!decodeHeadphonesAudioContext(withKey(noneWire, QStringLiteral("sliceId"), 1))
                     .has_value());
        QVERIFY(!decodeHeadphonesAudioContext(
            withKey(noneWire, QStringLiteral("op"), QStringLiteral("audio-context"))).has_value());
    }
};

QTEST_APPLESS_MAIN(TstRemoteAudioContext)
#include "tst_remote_audio_context.moc"
