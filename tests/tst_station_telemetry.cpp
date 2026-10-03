// no-port-check: NereusSDR-original telemetry wire contract tests (R-R3-32).
#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>
#include <limits>

#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"

using namespace NereusSDR;

namespace {
StationTelemetrySnapshot measured()
{
    StationTelemetrySnapshot sample;
    sample.sequence = 17;
    sample.sampledElapsedMs = 3400;
    sample.radio.connected = true;
    sample.radio.rxMbps = 7.25;
    sample.radio.txMbps = 0.0; // A measured zero must survive.
    sample.radio.rttMs = 3;
    sample.radio.rttAgeMs = 900;
    sample.audio.active = true;
    sample.audio.contextGeneration = 11;
    sample.audio.sourceFramesPerSecond = 48000;
    sample.audio.sourceDropsPerSecond = 0.5;
    sample.audio.encodedPacketsPerSecond = 25;
    sample.audio.encodeFailuresPerSecond = 0;
    sample.audio.sendAcceptedPerSecond = 24;
    sample.audio.sendRejectedPerSecond = 1;
    return sample;
}

StationHostTelemetry measuredHost()
{
    StationHostTelemetry host;
    host.systemCpuPercent = 37.5;
    host.processCpuPercent = 0.0; // A measured zero must survive.
    host.memoryAvailableKiB = 6500000;
    host.memoryTotalKiB = 8000000;
    host.processResidentKiB = 51234;
    host.hottestZoneCelsius = 52.5;
    host.hottestZoneName = QStringLiteral("bigcore0-thermal");
    return host;
}

QVector<StationReceiverTelemetry> measuredReceivers()
{
    StationReceiverTelemetry a;
    a.sliceId = 0;
    a.loadPercent = 42.5;
    a.inputDelayMs = 3;
    a.skippedInputMs = 0; // A measured zero must survive.
    StationReceiverTelemetry b;
    b.sliceId = 2;
    b.loadPercent = 131.25; // Over 100: this receiver cannot keep up.
    b.inputDelayMs = 480;
    b.skippedInputMs = 1250;
    StationReceiverTelemetry idle;
    idle.sliceId = 4; // processed nothing: load absent, never zero
    idle.inputDelayMs = 0;
    idle.skippedInputMs = 7;
    return {a, b, idle};
}
}

class TestStationTelemetry : public QObject {
    Q_OBJECT
private slots:
    void measuredValuesRetainUnitsAndMeaning()
    {
        SessionMessage message;
        message.kind = SessionMessageKind::StationTelemetry;
        message.telemetry = measured();
        const QByteArray wire = SessionMessages::encode(message);
        QVERIFY(!wire.isEmpty());
        QVERIFY(wire.size() < kMaxStationTelemetryBytes);
        QCOMPARE(QJsonDocument::fromJson(wire).object().value("type").toString(),
                 QStringLiteral("station.metrics.v1"));
        SessionMessage decoded;
        QVERIFY(SessionMessages::decode(wire, &decoded));
        QCOMPARE(decoded.kind, SessionMessageKind::StationTelemetry);
        QCOMPARE(decoded.telemetry.sequence, 17u);
        QCOMPARE(decoded.telemetry.sampledElapsedMs, 3400);
        QCOMPARE(decoded.telemetry.radio.rxMbps, std::optional<double>(7.25));
        QCOMPARE(decoded.telemetry.radio.txMbps, std::optional<double>(0));
        QCOMPARE(decoded.telemetry.radio.rttMs, std::optional<qint64>(3));
        QCOMPARE(decoded.telemetry.radio.rttAgeMs, std::optional<qint64>(900));
        const auto& audio = decoded.telemetry.audio;
        QCOMPARE(audio.contextGeneration, 11u);
        QCOMPARE(audio.sourceFramesPerSecond, std::optional<double>(48000));
        QCOMPARE(audio.sourceDropsPerSecond, std::optional<double>(0.5));
        QCOMPARE(audio.encodedPacketsPerSecond, std::optional<double>(25));
        QCOMPARE(audio.encodeFailuresPerSecond, std::optional<double>(0));
        QCOMPARE(audio.sendAcceptedPerSecond, std::optional<double>(24));
        QCOMPARE(audio.sendRejectedPerSecond, std::optional<double>(1));
    }

    void unavailableStaysAbsentAndFutureFieldsAreIgnored()
    {
        StationTelemetrySnapshot sample;
        sample.sequence = 1;
        auto payload = StationTelemetryCodec::encode(sample);
        QVERIFY(payload);
        QVERIFY(!payload->value("radio").toObject().contains("rxMbps"));
        QVERIFY(!payload->value("radio").toObject().contains("rttMs"));
        payload->insert("futureObservation", QJsonObject{{"value", 7}});
        StationTelemetrySnapshot decoded = measured();
        QVERIFY(StationTelemetryCodec::decode(*payload, &decoded));
        QVERIFY(!decoded.radio.connected);
        QVERIFY(!decoded.radio.rxMbps);
        QVERIFY(!decoded.radio.rttMs);
        QVERIFY(!decoded.audio.active);
        QVERIFY(!decoded.audio.sourceFramesPerSecond);
        QVERIFY(!decoded.audio.sendAcceptedPerSecond);
    }

    void malformedSampleIsTransactional_data()
    {
        QTest::addColumn<QJsonObject>("payload");
        const QJsonObject valid = *StationTelemetryCodec::encode(measured());
        auto field = [&](const char* name, const char* key, const QJsonValue& value) {
            QJsonObject broken = valid;
            broken.insert(QString::fromLatin1(key), value);
            QTest::newRow(name) << broken;
        };
        field("missing-sequence", "sequence", QJsonValue(QJsonValue::Undefined));
        field("zero-sequence", "sequence", 0);
        field("fractional-sequence", "sequence", 1.5);
        field("oversized-sequence", "sequence", 4294967296.0);
        field("negative-time", "sampledElapsedMs", -1);
        field("inexact-time", "sampledElapsedMs", 9007199254740992.0);
        field("not-radio-object", "radio", true);
        auto radioField = [&](const char* name, const char* key, const QJsonValue& value) {
            QJsonObject radio = valid.value("radio").toObject();
            radio.insert(QString::fromLatin1(key), value);
            field(name, "radio", radio);
        };
        radioField("not-connected-bool", "connected", "true");
        radioField("negative-rate", "rxMbps", -0.1);
        radioField("null-is-not-absence", "rxMbps", QJsonValue(QJsonValue::Null));
        radioField("string-rate", "rxMbps", "7.25");
        radioField("fractional-rtt", "rttMs", 1.5);
        radioField("rtt-without-age", "rttAgeMs", QJsonValue(QJsonValue::Undefined));
        radioField("disconnected-with-current-rates", "connected", false);
        radioField("negative-radio-connection-age", "connectionAgeMs", -1);
        radioField("fractional-radio-connection-age", "connectionAgeMs", 1.5);
        radioField("inexact-radio-connection-age", "connectionAgeMs", 9007199254740992.0);
        radioField("zero-radio-udp-port", "radioUdpBasePort", 0);
        radioField("fractional-radio-udp-port", "radioUdpBasePort", 1024.5);
        radioField("oversized-radio-udp-port", "radioUdpBasePort", 65536);
        radioField("adc-not-array", "adcOverloads", true);
        radioField("adc-too-many", "adcOverloads", QJsonArray{
            QJsonObject{{"adc", 0}, {"eventsSinceConnection", 0}},
            QJsonObject{{"adc", 1}, {"eventsSinceConnection", 0}},
            QJsonObject{{"adc", 2}, {"eventsSinceConnection", 0}},
            QJsonObject{{"adc", 0}, {"eventsSinceConnection", 0}}});
        radioField("duplicate-adc-index", "adcOverloads", QJsonArray{
            QJsonObject{{"adc", 0}, {"eventsSinceConnection", 1}},
            QJsonObject{{"adc", 0}, {"eventsSinceConnection", 2}}});
        const auto oneAdc = [&](const char* name, QJsonObject entry) {
            radioField(name, "adcOverloads", QJsonArray{entry});
        };
        oneAdc("adc-index-out-of-range", {{"adc", 3}, {"eventsSinceConnection", 0}});
        oneAdc("adc-index-fractional", {{"adc", 0.5}, {"eventsSinceConnection", 0}});
        oneAdc("adc-missing-count", {{"adc", 0}});
        oneAdc("adc-negative-count", {{"adc", 0}, {"eventsSinceConnection", -1}});
        oneAdc("adc-fractional-count", {{"adc", 0}, {"eventsSinceConnection", 1.5}});
        oneAdc("adc-overloaded-not-boolean", {{"adc", 0}, {"eventsSinceConnection", 0},
                                             {"statusAgeMs", 0}, {"overloaded", 1}});
        oneAdc("adc-overloaded-without-status-age", {{"adc", 0},
                {"eventsSinceConnection", 1}, {"overloaded", true}});
        oneAdc("adc-overloaded-with-stale-status", {{"adc", 0},
                {"eventsSinceConnection", 1}, {"statusAgeMs", 3001}, {"overloaded", true}});
        oneAdc("adc-overloaded-without-event", {{"adc", 0},
                {"eventsSinceConnection", 0}, {"statusAgeMs", 1}, {"overloaded", true}});
        oneAdc("adc-last-overload-without-event", {{"adc", 0},
                {"eventsSinceConnection", 0}, {"lastOverloadAgeMs", 1}});
        oneAdc("adc-last-overload-newer-than-status", {{"adc", 0},
                {"eventsSinceConnection", 1}, {"statusAgeMs", 100},
                {"lastOverloadAgeMs", 50}});
        oneAdc("adc-negative-status-age", {{"adc", 0},
                {"eventsSinceConnection", 0}, {"statusAgeMs", -1}});
        StationTelemetrySnapshot disconnectedSample = measured();
        disconnectedSample.radio = {};
        QJsonObject disconnected = *StationTelemetryCodec::encode(disconnectedSample);
        QJsonObject disconnectedRadio = disconnected.value("radio").toObject();
        disconnectedRadio.insert("adcOverloads", QJsonArray{});
        disconnected.insert("radio", disconnectedRadio);
        QTest::newRow("disconnected-even-empty-adc-array") << disconnected;
        auto audioField = [&](const char* name, const char* key, const QJsonValue& value) {
            QJsonObject audio = valid.value("audio").toObject();
            audio.insert(QString::fromLatin1(key), value);
            field(name, "audio", audio);
        };
        audioField("active-without-context", "contextGeneration", 0);
        audioField("negative-audio-rate", "sendAcceptedPerSecond", -1);
        audioField("retired-with-current-rates", "active", false);
        audioField("not-active-bool", "active", 1);
    }

    void malformedSampleIsTransactional()
    {
        QFETCH(QJsonObject, payload);
        StationTelemetrySnapshot previous = measured();
        previous.sequence = 88;
        QVERIFY(!StationTelemetryCodec::decode(payload, &previous));
        QCOMPARE(previous.sequence, 88u);
        QCOMPARE(previous.radio.rxMbps, std::optional<double>(7.25));
    }

    void radioDiagnosticsRoundTripPreservesUnknownAndMeasuredClear()
    {
        StationTelemetrySnapshot sample = measured();
        sample.radio.connectionAgeMs = 123456;
        sample.radio.radioUdpBasePort = 42000;
        sample.radio.adcOverloads = QVector<StationAdcOverloadTelemetry>{
            {0, 0, 25, false, std::nullopt},
            {1, 2, 100, true, 100},
            {2, 3, 4100, std::nullopt, 4200}};
        const std::optional<QJsonObject> payload = StationTelemetryCodec::encode(sample);
        QVERIFY(payload);
        const QJsonObject radio = payload->value("radio").toObject();
        QCOMPARE(radio.value("connectionAgeMs").toInteger(), 123456);
        QCOMPARE(radio.value("radioUdpBasePort").toInteger(), 42000);
        QCOMPARE(radio.value("adcOverloads").toArray().size(), 3);
        const QJsonObject stale = radio.value("adcOverloads").toArray().at(2).toObject();
        QVERIFY(!stale.contains("overloaded"));
        StationTelemetrySnapshot decoded;
        QVERIFY(StationTelemetryCodec::decode(*payload, &decoded));
        QCOMPARE(decoded.radio.connectionAgeMs, std::optional<qint64>(123456));
        QCOMPARE(decoded.radio.radioUdpBasePort, std::optional<qint64>(42000));
        QVERIFY(decoded.radio.adcOverloads);
        QCOMPARE(decoded.radio.adcOverloads->at(0).overloaded, std::optional<bool>(false));
        QCOMPARE(decoded.radio.adcOverloads->at(1).eventsSinceConnection, 2);
        QCOMPARE(decoded.radio.adcOverloads->at(2).statusAgeMs,
                 std::optional<qint64>(4100));
        QVERIFY(!decoded.radio.adcOverloads->at(2).overloaded);
        decoded.radio.clearRadioDiagnostics();
        QVERIFY(decoded.radio.hasNoRadioDiagnostics());
    }

    void encoderRefusesNonFiniteAndInconsistentValues()
    {
        auto sample = measured();
        sample.radio.rxMbps = std::numeric_limits<double>::infinity();
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample = measured();
        sample.audio.sourceFramesPerSecond = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample = measured();
        sample.radio.rttAgeMs.reset();
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample = measured();
        sample.sampledElapsedMs = std::numeric_limits<qint64>::max();
        QVERIFY(!StationTelemetryCodec::encode(sample));
    }

    void wholeMessageIsBoundedIncludingUnknownFields()
    {
        QJsonObject envelope{{"type", "station.metrics.v1"},
            {"payload", *StationTelemetryCodec::encode(measured())},
            {"future", QString(kMaxStationTelemetryBytes, QChar('x'))}};
        SessionMessage previous;
        previous.kind = SessionMessageKind::SnapshotComplete;
        QVERIFY(!SessionMessages::decode(QJsonDocument(envelope).toJson(), &previous));
        QCOMPARE(previous.kind, SessionMessageKind::SnapshotComplete);
        envelope.remove("future");
        envelope.insert("payload", QJsonValue(QJsonValue::Null));
        QVERIFY(!SessionMessages::decode(QJsonDocument(envelope).toJson(), &previous));
    }

    // R-R3-32/33: the host section in both shapes. A snapshot with host
    // values carries a "host" object; one without it encodes exactly as
    // before, and a decoder sees the section absent either way.
    void hostSectionRoundTripsInBothShapes()
    {
        StationTelemetrySnapshot withHost = measured();
        withHost.host = measuredHost();
        const auto payload = StationTelemetryCodec::encode(withHost);
        QVERIFY(payload);
        const QJsonObject host = payload->value("host").toObject();
        QCOMPARE(host.size(), 7);
        StationTelemetrySnapshot decoded;
        QVERIFY(StationTelemetryCodec::decode(*payload, &decoded));
        QCOMPARE(decoded.host.systemCpuPercent, std::optional<double>(37.5));
        QCOMPARE(decoded.host.processCpuPercent, std::optional<double>(0.0));
        QCOMPARE(decoded.host.memoryAvailableKiB, std::optional<qint64>(6500000));
        QCOMPARE(decoded.host.memoryTotalKiB, std::optional<qint64>(8000000));
        QCOMPARE(decoded.host.processResidentKiB, std::optional<qint64>(51234));
        QCOMPARE(decoded.host.hottestZoneCelsius, std::optional<double>(52.5));
        QCOMPARE(decoded.host.hottestZoneName, QStringLiteral("bigcore0-thermal"));

        // Without host values the payload is today's four keys, byte for byte.
        const auto plain = StationTelemetryCodec::encode(measured());
        QVERIFY(plain);
        QVERIFY(!plain->contains("host"));
        QCOMPARE(plain->keys(), (QStringList{"audio", "radio", "sampledElapsedMs", "sequence"}));
        QJsonObject stripped = *payload;
        stripped.remove("host");
        QCOMPARE(QJsonDocument(stripped).toJson(QJsonDocument::Compact),
                 QJsonDocument(*plain).toJson(QJsonDocument::Compact));

        // A version 1 payload decodes with the host section absent and
        // replaces any host values the destination held.
        decoded = withHost;
        QVERIFY(StationTelemetryCodec::decode(*plain, &decoded));
        QVERIFY(decoded.host.isEmpty());

        // Partial sections: only what was measured is sent.
        StationTelemetrySnapshot partial = measured();
        partial.host.memoryTotalKiB = 8000000;
        partial.host.hottestZoneCelsius = -12.5; // sub-zero is a reading
        const auto partialPayload = StationTelemetryCodec::encode(partial);
        QVERIFY(partialPayload);
        QCOMPARE(partialPayload->value("host").toObject().keys(),
                 (QStringList{"hottestZoneCelsius", "memoryTotalKiB"}));
        QVERIFY(StationTelemetryCodec::decode(*partialPayload, &decoded));
        QVERIFY(!decoded.host.systemCpuPercent);
        QVERIFY(!decoded.host.memoryAvailableKiB);
        QCOMPARE(decoded.host.hottestZoneCelsius, std::optional<double>(-12.5));
        QVERIFY(decoded.host.hottestZoneName.isEmpty());

        // An empty host object means nothing measured, and unknown host
        // fields from a later version are ignored.
        QJsonObject future = *plain;
        future.insert("host", QJsonObject{{"futureHostValue", 3}});
        QVERIFY(StationTelemetryCodec::decode(future, &decoded));
        QVERIFY(decoded.host.isEmpty());
    }

    void malformedHostSectionIsTransactional_data()
    {
        QTest::addColumn<QJsonObject>("payload");
        StationTelemetrySnapshot sample = measured();
        sample.host = measuredHost();
        const QJsonObject valid = *StationTelemetryCodec::encode(sample);
        auto hostField = [&](const char* name, const char* key, const QJsonValue& value) {
            QJsonObject host = valid.value("host").toObject();
            host.insert(QString::fromLatin1(key), value);
            QJsonObject broken = valid;
            broken.insert("host", host);
            QTest::newRow(name) << broken;
        };
        QJsonObject notObject = valid;
        notObject.insert("host", 1);
        QTest::newRow("host-not-object") << notObject;
        hostField("cpu-over-100", "systemCpuPercent", 100.5);
        hostField("negative-cpu", "processCpuPercent", -1);
        hostField("string-cpu", "systemCpuPercent", "37.5");
        hostField("null-is-not-absence", "systemCpuPercent", QJsonValue(QJsonValue::Null));
        hostField("fractional-kib", "memoryTotalKiB", 8000000.5);
        hostField("negative-kib", "processResidentKiB", -1);
        hostField("below-absolute-zero", "hottestZoneCelsius", -274);
        hostField("empty-zone-name", "hottestZoneName", "");
        hostField("long-zone-name", "hottestZoneName",
                  QString(kMaxHostZoneNameLength + 1, QChar('z')));
        hostField("numeric-zone-name", "hottestZoneName", 3);
        hostField("name-without-temperature", "hottestZoneCelsius",
                  QJsonValue(QJsonValue::Undefined));
    }

    void malformedHostSectionIsTransactional()
    {
        QFETCH(QJsonObject, payload);
        StationTelemetrySnapshot previous = measured();
        previous.host = measuredHost();
        previous.sequence = 88;
        QVERIFY(!StationTelemetryCodec::decode(payload, &previous));
        QCOMPARE(previous.sequence, 88u);
        QCOMPARE(previous.host.systemCpuPercent, std::optional<double>(37.5));
    }

    void encoderRefusesInvalidHostValues()
    {
        auto sample = measured();
        sample.host = measuredHost();
        sample.host.systemCpuPercent = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.host = measuredHost();
        sample.host.processCpuPercent = 101.0;
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.host = measuredHost();
        sample.host.hottestZoneCelsius = std::numeric_limits<double>::infinity();
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.host = measuredHost();
        sample.host.hottestZoneCelsius.reset(); // a name needs its reading
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.host = measuredHost();
        sample.host.memoryTotalKiB = -1;
        QVERIFY(!StationTelemetryCodec::encode(sample));
    }

    // R-R3-40: the receivers section in its three shapes. Absent means the
    // Core did not measure receivers; an empty list means it measured and
    // no receiver had a reading; each entry's load is absent when that
    // receiver processed nothing.
    void receiversSectionRoundTripsInEveryShape()
    {
        StationTelemetrySnapshot withReceivers = measured();
        withReceivers.host = measuredHost();
        withReceivers.receivers = measuredReceivers();
        const auto payload = StationTelemetryCodec::encode(withReceivers);
        QVERIFY(payload);
        const QJsonArray array = payload->value("receivers").toArray();
        QCOMPARE(array.size(), 3);
        QCOMPARE(array.at(0).toObject().keys(),
                 (QStringList{"inputDelayMs", "loadPercent", "skippedInputMs", "sliceId"}));
        QCOMPARE(array.at(2).toObject().keys(),
                 (QStringList{"inputDelayMs", "skippedInputMs", "sliceId"}));
        StationTelemetrySnapshot decoded;
        QVERIFY(StationTelemetryCodec::decode(*payload, &decoded));
        QVERIFY(decoded.receivers);
        QCOMPARE(decoded.receivers->size(), 3);
        const StationReceiverTelemetry& a = decoded.receivers->at(0);
        QCOMPARE(a.sliceId, 0);
        QCOMPARE(a.loadPercent, std::optional<double>(42.5));
        QCOMPARE(a.inputDelayMs, 3LL);
        QCOMPARE(a.skippedInputMs, 0LL);
        const StationReceiverTelemetry& b = decoded.receivers->at(1);
        QCOMPARE(b.sliceId, 2);
        QCOMPARE(b.loadPercent, std::optional<double>(131.25));
        QCOMPARE(b.inputDelayMs, 480LL);
        QCOMPARE(b.skippedInputMs, 1250LL);
        const StationReceiverTelemetry& idle = decoded.receivers->at(2);
        QCOMPARE(idle.sliceId, 4);
        QVERIFY(!idle.loadPercent);
        QCOMPARE(idle.skippedInputMs, 7LL);
        QCOMPARE(decoded.host.hottestZoneName, QStringLiteral("bigcore0-thermal"));

        // Measured with no receiver reading: an empty list, kept distinct
        // from absent.
        StationTelemetrySnapshot none = measured();
        none.receivers = QVector<StationReceiverTelemetry>{};
        const auto nonePayload = StationTelemetryCodec::encode(none);
        QVERIFY(nonePayload);
        QVERIFY(nonePayload->value("receivers").isArray());
        QVERIFY(StationTelemetryCodec::decode(*nonePayload, &decoded));
        QVERIFY(decoded.receivers);
        QVERIFY(decoded.receivers->isEmpty());

        // Without the section the payload is the version 2 payload byte for
        // byte, and decoding it clears any receivers the destination held.
        StationTelemetrySnapshot versionTwo = withReceivers;
        versionTwo.receivers.reset();
        const auto plain = StationTelemetryCodec::encode(versionTwo);
        QVERIFY(plain);
        QVERIFY(!plain->contains("receivers"));
        QJsonObject stripped = *payload;
        stripped.remove("receivers");
        QCOMPARE(QJsonDocument(stripped).toJson(QJsonDocument::Compact),
                 QJsonDocument(*plain).toJson(QJsonDocument::Compact));
        decoded = withReceivers;
        QVERIFY(StationTelemetryCodec::decode(*plain, &decoded));
        QVERIFY(!decoded.receivers);

        // Unknown receiver fields from a later version are ignored.
        QJsonObject future = *plain;
        future.insert("receivers", QJsonArray{QJsonObject{
            {"sliceId", 1}, {"inputDelayMs", 0}, {"skippedInputMs", 0},
            {"futureReceiverValue", 9}}});
        QVERIFY(StationTelemetryCodec::decode(future, &decoded));
        QVERIFY(decoded.receivers);
        QCOMPARE(decoded.receivers->size(), 1);
        QCOMPARE(decoded.receivers->at(0).sliceId, 1);
    }

    void malformedReceiversSectionIsTransactional_data()
    {
        QTest::addColumn<QJsonObject>("payload");
        StationTelemetrySnapshot sample = measured();
        sample.receivers = measuredReceivers();
        const QJsonObject valid = *StationTelemetryCodec::encode(sample);
        auto receiverField = [&](const char* name, const char* key, const QJsonValue& value) {
            QJsonArray receivers = valid.value("receivers").toArray();
            QJsonObject first = receivers.at(0).toObject();
            if (value.isUndefined()) {
                first.remove(QString::fromLatin1(key));
            } else {
                first.insert(QString::fromLatin1(key), value);
            }
            receivers.replace(0, first);
            QJsonObject broken = valid;
            broken.insert("receivers", receivers);
            QTest::newRow(name) << broken;
        };
        QJsonObject notArray = valid;
        notArray.insert("receivers", QJsonObject{});
        QTest::newRow("receivers-not-array") << notArray;
        QJsonObject entryNotObject = valid;
        entryNotObject.insert("receivers", QJsonArray{3});
        QTest::newRow("entry-not-object") << entryNotObject;
        QJsonArray tooMany;
        for (int i = 0; i <= kMaxStationReceivers; ++i) {
            tooMany.append(QJsonObject{{"sliceId", i}, {"inputDelayMs", 0},
                                       {"skippedInputMs", 0}});
        }
        QJsonObject overLimit = valid;
        overLimit.insert("receivers", tooMany);
        QTest::newRow("too-many-receivers") << overLimit;
        QJsonArray duplicated = valid.value("receivers").toArray();
        duplicated.append(duplicated.at(0));
        QJsonObject duplicate = valid;
        duplicate.insert("receivers", duplicated);
        QTest::newRow("duplicate-slice") << duplicate;
        receiverField("missing-slice", "sliceId", QJsonValue(QJsonValue::Undefined));
        receiverField("negative-slice", "sliceId", -1);
        receiverField("slice-over-limit", "sliceId", 65536);
        receiverField("fractional-slice", "sliceId", 1.5);
        receiverField("negative-load", "loadPercent", -0.5);
        receiverField("string-load", "loadPercent", "42.5");
        receiverField("null-load-is-not-absence", "loadPercent", QJsonValue(QJsonValue::Null));
        receiverField("missing-input-delay", "inputDelayMs", QJsonValue(QJsonValue::Undefined));
        receiverField("negative-input-delay", "inputDelayMs", -1);
        receiverField("fractional-input-delay", "inputDelayMs", 3.5);
        receiverField("missing-skipped-input", "skippedInputMs", QJsonValue(QJsonValue::Undefined));
        receiverField("negative-skipped-input", "skippedInputMs", -1);
    }

    void malformedReceiversSectionIsTransactional()
    {
        QFETCH(QJsonObject, payload);
        StationTelemetrySnapshot previous = measured();
        previous.receivers = measuredReceivers();
        previous.sequence = 88;
        QVERIFY(!StationTelemetryCodec::decode(payload, &previous));
        QCOMPARE(previous.sequence, 88u);
        QVERIFY(previous.receivers);
        QCOMPARE(previous.receivers->size(), 3);
    }

    void encoderRefusesInvalidReceivers()
    {
        auto sample = measured();
        sample.receivers = measuredReceivers();
        (*sample.receivers)[0].loadPercent = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.receivers = measuredReceivers();
        (*sample.receivers)[0].loadPercent = std::numeric_limits<double>::infinity();
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.receivers = measuredReceivers();
        (*sample.receivers)[0].loadPercent = -1.0;
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.receivers = measuredReceivers();
        (*sample.receivers)[1].sliceId = 0; // duplicate
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.receivers = measuredReceivers();
        (*sample.receivers)[1].sliceId = -1;
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.receivers = measuredReceivers();
        (*sample.receivers)[1].inputDelayMs = -1;
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.receivers = measuredReceivers();
        (*sample.receivers)[1].skippedInputMs = -1;
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.receivers = QVector<StationReceiverTelemetry>(kMaxStationReceivers + 1);
        for (int i = 0; i < sample.receivers->size(); ++i) {
            (*sample.receivers)[i].sliceId = i;
        }
        QVERIFY(!StationTelemetryCodec::encode(sample));
        sample.receivers->removeLast();
        QVERIFY(StationTelemetryCodec::encode(sample)); // the limit itself is fine
    }

    void olderCapabilitiesDefaultToUnsupported()
    {
        QCOMPARE(StationCapabilities::fromUpdates({}).stationTelemetryVersion, 0);
        StationCapabilities caps;
        caps.stationTelemetryVersion = 1;
        QCOMPARE(StationCapabilities::fromUpdates(caps.toUpdates()).stationTelemetryVersion, 1);
        caps.stationTelemetryVersion = 2; // adds the Core host section
        QCOMPARE(StationCapabilities::fromUpdates(caps.toUpdates()).stationTelemetryVersion, 2);
        caps.stationTelemetryVersion = 3; // adds the receivers section
        QCOMPARE(StationCapabilities::fromUpdates(caps.toUpdates()).stationTelemetryVersion, 3);
        caps.stationTelemetryVersion = -1;
        QCOMPARE(StationCapabilities::fromUpdates(caps.toUpdates()).stationTelemetryVersion, 0);
    }
};

QTEST_GUILESS_MAIN(TestStationTelemetry)
#include "tst_station_telemetry.moc"
