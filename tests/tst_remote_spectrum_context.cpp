// =================================================================
// tests/tst_remote_spectrum_context.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-01/04/08/09: the spectrum-context
// wire codec Core and GUI share, in the minor-8 shape and the minor-9 shape
// that reports Core's grant.
//
// Modification history (NereusSDR):
//   2026-09-26 : Parity Task 28 (R-R3-49, A11): the `transmit` field, only
//                 for a peer that declared txDisplayVersion. J.J. Boyd
//                 (KG4VCF), AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

#include <QtTest>

#include "core/FFTEngine.h"
#include "core/session/media/RemoteSpectrumContext.h"

#include <QJsonDocument>

#include <limits>

using namespace NereusSDR;

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";

const QList<SpectrumLimitReason> kAllLimits{
    SpectrumLimitReason::None, SpectrumLimitReason::LargestSize,
    SpectrumLimitReason::SharedEngine, SpectrumLimitReason::SourceBins};

const QStringList kGrantKeys{
    QStringLiteral("grantedFftSize"), QStringLiteral("grantedTier"),
    QStringLiteral("requestedPixels"), QStringLiteral("grantedPixels"),
    QStringLiteral("limit")};

// The raw values DaemonMediaController::sendContext read from its endpoint
// entry, in their own types.
struct LegacyInputs {
    quint32 endpointId = 7;
    quint32 revision = 3;
    quint32 contextGeneration = 42;
    int streamIndex = 2;
    double sourceCentreHz = 14'225'000.0;
    double sourceSampleRateHz = 192'000.0;
    double exactCentreHz = 14'225'023.4375;
    double exactSpanHz = 24'046.875;
    double wideCentreHz = 14'225'000.0;
    double wideSpanHz = 96'000.0;
    quint16 traceSamples = 257;
    quint16 waterfallSamples = 257;
    quint16 wideSamples = 512;
    float minDbm = -163.25f;
    float maxDbm = -17.5f;
    int targetFps = 30;
    int framesPerLine = 2;
};

WidebandDisplayContext activeWideband()
{
    WidebandDisplayContext wideband;
    wideband.available = true;
    wideband.active = true;
    wideband.physicalAdcIndex = 0;
    wideband.filterChainIndex = 1;
    wideband.sourceGeneration = 9;
    wideband.adcRateHz = 122'880'000.0;
    return wideband;
}

// The context exactly as DaemonMediaController::sendContext built it inline
// before minor 9: the reference every minor-8 GUI parses.
QJsonObject todaysContext(const LegacyInputs& in, std::optional<WidebandDisplayContext> wideband)
{
    QJsonObject message{
        {QStringLiteral("op"), QStringLiteral("context")},
        {QStringLiteral("connectionId"), QString::fromLatin1(kConnectionId)},
        {QStringLiteral("endpointId"), static_cast<qint64>(in.endpointId)},
        {QStringLiteral("revision"), static_cast<qint64>(in.revision)},
        {QStringLiteral("contextGeneration"), static_cast<qint64>(in.contextGeneration)},
        {QStringLiteral("sourceStream"), in.streamIndex},
        {QStringLiteral("sourceCentreHz"), in.sourceCentreHz},
        {QStringLiteral("sampleRateHz"), in.sourceSampleRateHz},
        {QStringLiteral("centreHz"), in.exactCentreHz},
        {QStringLiteral("spanHz"), in.exactSpanHz},
        {QStringLiteral("wideCentreHz"), in.wideCentreHz},
        {QStringLiteral("wideSpanHz"), in.wideSpanHz},
        {QStringLiteral("traceSamples"), in.traceSamples},
        {QStringLiteral("waterfallSamples"), in.waterfallSamples},
        {QStringLiteral("wideSamples"), in.wideSamples},
        {QStringLiteral("minDbm"), in.minDbm},
        {QStringLiteral("maxDbm"), in.maxDbm},
        {QStringLiteral("fps"), in.targetFps},
        {QStringLiteral("framesPerLine"), in.framesPerLine},
    };
    if (wideband) {
        message.insert(QStringLiteral("wideband"), wideband->toJson());
    }
    return message;
}

// What sendContext now hands the codec for the same entry.
SpectrumContextMessage messageFrom(const LegacyInputs& in,
                                   std::optional<WidebandDisplayContext> wideband)
{
    SpectrumContextMessage message;
    message.connectionId = QString::fromLatin1(kConnectionId);
    message.endpointId = in.endpointId;
    message.revision = in.revision;
    message.contextGeneration = in.contextGeneration;
    message.sourceStream = in.streamIndex;
    message.sourceCentreHz = in.sourceCentreHz;
    message.sampleRateHz = in.sourceSampleRateHz;
    message.centreHz = in.exactCentreHz;
    message.spanHz = in.exactSpanHz;
    message.wideCentreHz = in.wideCentreHz;
    message.wideSpanHz = in.wideSpanHz;
    message.traceSamples = in.traceSamples;
    message.waterfallSamples = in.waterfallSamples;
    message.wideSamples = in.wideSamples;
    message.minDbm = in.minDbm;
    message.maxDbm = in.maxDbm;
    message.fps = in.targetFps;
    message.framesPerLine = in.framesPerLine;
    message.wideband = wideband;
    return message;
}

SpectrumContextGrant sampleGrant(SpectrumLimitReason limit = SpectrumLimitReason::SourceBins)
{
    SpectrumContextGrant grant;
    grant.grantedFftSize = 4096;
    grant.grantedTier = FftTier::Fine;
    grant.requestedPixels = 1024;
    grant.grantedPixels = 257;
    grant.limit = limit;
    return grant;
}

SpectrumContextMessage sampleMessage(bool withWideband, bool withGrant)
{
    SpectrumContextMessage message =
        messageFrom(LegacyInputs{}, withWideband ? std::optional(activeWideband()) : std::nullopt);
    if (withGrant) {
        message.grant = sampleGrant();
    }
    return message;
}

QByteArray compact(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void compareMessages(const SpectrumContextMessage& actual, const SpectrumContextMessage& expected)
{
    QCOMPARE(actual.connectionId, expected.connectionId);
    QCOMPARE(actual.endpointId, expected.endpointId);
    QCOMPARE(actual.revision, expected.revision);
    QCOMPARE(actual.contextGeneration, expected.contextGeneration);
    QCOMPARE(actual.sourceStream, expected.sourceStream);
    QCOMPARE(actual.sourceCentreHz, expected.sourceCentreHz);
    QCOMPARE(actual.sampleRateHz, expected.sampleRateHz);
    QCOMPARE(actual.centreHz, expected.centreHz);
    QCOMPARE(actual.spanHz, expected.spanHz);
    QCOMPARE(actual.wideCentreHz, expected.wideCentreHz);
    QCOMPARE(actual.wideSpanHz, expected.wideSpanHz);
    QCOMPARE(actual.traceSamples, expected.traceSamples);
    QCOMPARE(actual.waterfallSamples, expected.waterfallSamples);
    QCOMPARE(actual.wideSamples, expected.wideSamples);
    QCOMPARE(actual.minDbm, expected.minDbm);
    QCOMPARE(actual.maxDbm, expected.maxDbm);
    QCOMPARE(actual.fps, expected.fps);
    QCOMPARE(actual.framesPerLine, expected.framesPerLine);
    QVERIFY(actual.wideband == expected.wideband);
    QVERIFY(actual.grant == expected.grant);
}

} // namespace

class TstRemoteSpectrumContext : public QObject {
    Q_OBJECT

private slots:
    void limitWireStringsAreExact()
    {
        QCOMPARE(spectrumLimitReasonToWire(SpectrumLimitReason::None), QStringLiteral("none"));
        QCOMPARE(spectrumLimitReasonToWire(SpectrumLimitReason::LargestSize),
                 QStringLiteral("largest-size"));
        QCOMPARE(spectrumLimitReasonToWire(SpectrumLimitReason::SharedEngine),
                 QStringLiteral("shared"));
        QCOMPARE(spectrumLimitReasonToWire(SpectrumLimitReason::SourceBins),
                 QStringLiteral("source-bins"));
        for (SpectrumLimitReason limit : kAllLimits) {
            QCOMPARE(spectrumLimitReasonFromWire(spectrumLimitReasonToWire(limit)),
                     std::optional(limit));
        }
    }

    void limitRejectsEverythingElse_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("capitalised") << QJsonValue(QStringLiteral("None"));
        QTest::newRow("enum name") << QJsonValue(QStringLiteral("shared-engine"));
        QTest::newRow("underscore") << QJsonValue(QStringLiteral("largest_size"));
        QTest::newRow("padded") << QJsonValue(QStringLiteral(" shared"));
        QTest::newRow("empty") << QJsonValue(QString());
        QTest::newRow("number") << QJsonValue(0);
        QTest::newRow("bool") << QJsonValue(false);
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
    }

    void limitRejectsEverythingElse()
    {
        QFETCH(QJsonValue, value);
        QVERIFY(!spectrumLimitReasonFromWire(value).has_value());
    }

    void coreGrantMapsToTheReportedFields()
    {
        SpectrumGrant grant;
        grant.requestedFftSize = 1 << 20;
        grant.grantedFftSize = 262'144;
        grant.grantedTier = FftTier::Fine;
        grant.requestedPixels = 2048;
        grant.grantedPixels = 900;
        grant.reason = SpectrumLimitReason::LargestSize;
        const SpectrumContextGrant reported = spectrumContextGrant(grant);
        QCOMPARE(reported.grantedFftSize, 262'144);
        QCOMPARE(reported.grantedTier, FftTier::Fine);
        QCOMPARE(reported.requestedPixels, 2048);
        QCOMPARE(reported.grantedPixels, 900);
        QCOMPARE(reported.limit, SpectrumLimitReason::LargestSize);
    }

    // The golden: a minor-8 peer receives the same bytes the inline builder
    // produced, with and without wideband, whatever grant Core holds.
    void legacyEncodingIsTodaysContextByteForByte()
    {
        const LegacyInputs in;
        for (const bool withWideband : {false, true}) {
            const std::optional<WidebandDisplayContext> wideband =
                withWideband ? std::optional(activeWideband()) : std::nullopt;
            const QJsonObject expected = todaysContext(in, wideband);
            SpectrumContextMessage message = messageFrom(in, wideband);
            for (const bool withGrant : {false, true}) {
                if (withGrant) { message.grant = sampleGrant(); }
                const QJsonObject encoded = encodeRemoteSpectrumContext(message, false);
                QCOMPARE(encoded.size(), withWideband ? 20 : 19);
                QCOMPARE(encoded, expected);
                QCOMPARE(compact(encoded), compact(expected));
            }
        }
    }

    void grantEncodingAddsExactlyTheFiveGrantFields()
    {
        const LegacyInputs in;
        for (const bool withWideband : {false, true}) {
            for (SpectrumLimitReason limit : kAllLimits) {
                for (const FftTier tier : {FftTier::Wide, FftTier::Fine}) {
                    const std::optional<WidebandDisplayContext> wideband =
                        withWideband ? std::optional(activeWideband()) : std::nullopt;
                    SpectrumContextMessage message = messageFrom(in, wideband);
                    message.grant = sampleGrant(limit);
                    message.grant->grantedTier = tier;
                    const QJsonObject encoded = encodeRemoteSpectrumContext(message, true);
                    QCOMPARE(encoded.size(), withWideband ? 25 : 24);
                    QJsonObject legacyPart = encoded;
                    for (const QString& key : kGrantKeys) {
                        QVERIFY2(legacyPart.contains(key), qPrintable(key));
                        legacyPart.remove(key);
                    }
                    QCOMPARE(legacyPart, todaysContext(in, wideband));
                    QCOMPARE(encoded.value(QStringLiteral("grantedFftSize")), QJsonValue(4096));
                    QCOMPARE(encoded.value(QStringLiteral("grantedTier")).toString(),
                             tier == FftTier::Fine ? QStringLiteral("fine")
                                                   : QStringLiteral("wide"));
                    QCOMPARE(encoded.value(QStringLiteral("requestedPixels")), QJsonValue(1024));
                    QCOMPARE(encoded.value(QStringLiteral("grantedPixels")), QJsonValue(257));
                    QCOMPARE(encoded.value(QStringLiteral("limit")).toString(),
                             spectrumLimitReasonToWire(limit));
                }
            }
        }
    }

    void negotiatedWithoutAGrantIsNotAcceptedAsAGrant()
    {
        const SpectrumContextMessage message = sampleMessage(false, false);
        const QJsonObject encoded = encodeRemoteSpectrumContext(message, true);
        QCOMPARE(encoded.size(), 19);
        QVERIFY(!decodeRemoteSpectrumContext(encoded, true).has_value());
    }

    void encodeThenDecodeRoundTripsEveryField_data()
    {
        QTest::addColumn<bool>("withWideband");
        QTest::addColumn<bool>("grantNegotiated");
        QTest::addColumn<int>("limit");
        QTest::addColumn<int>("tier");
        for (const bool withWideband : {false, true}) {
            QTest::addRow("legacy%s", withWideband ? " with wideband" : "")
                << withWideband << false << 0 << 0;
            for (SpectrumLimitReason limit : kAllLimits) {
                for (const FftTier tier : {FftTier::Wide, FftTier::Fine}) {
                    QTest::addRow("grant %s %s%s",
                                  qPrintable(spectrumLimitReasonToWire(limit)),
                                  tier == FftTier::Fine ? "fine" : "wide",
                                  withWideband ? " with wideband" : "")
                        << withWideband << true << static_cast<int>(limit)
                        << static_cast<int>(tier);
                }
            }
        }
    }

    void encodeThenDecodeRoundTripsEveryField()
    {
        QFETCH(bool, withWideband);
        QFETCH(bool, grantNegotiated);
        QFETCH(int, limit);
        QFETCH(int, tier);
        SpectrumContextMessage message = sampleMessage(withWideband, grantNegotiated);
        if (grantNegotiated) {
            message.grant->limit = static_cast<SpectrumLimitReason>(limit);
            message.grant->grantedTier = static_cast<FftTier>(tier);
        }
        const std::optional<SpectrumContextMessage> decoded =
            decodeRemoteSpectrumContext(encodeRemoteSpectrumContext(message, grantNegotiated),
                                        grantNegotiated);
        QVERIFY(decoded.has_value());
        compareMessages(*decoded, message);
    }

    void boundaryGrantValuesRoundTrip()
    {
        SpectrumContextMessage message = sampleMessage(false, true);
        message.grant->grantedFftSize = NereusSDR::FFTEngine::maximumFftSize();
        message.grant->requestedPixels = 4096;
        message.grant->grantedPixels = 4096;
        auto decoded = decodeRemoteSpectrumContext(encodeRemoteSpectrumContext(message, true), true);
        QVERIFY(decoded.has_value());
        QVERIFY(decoded->grant == message.grant);
        message.grant->grantedFftSize = 1;
        message.grant->requestedPixels = 1;
        message.grant->grantedPixels = 1;
        decoded = decodeRemoteSpectrumContext(encodeRemoteSpectrumContext(message, true), true);
        QVERIFY(decoded.has_value());
        QVERIFY(decoded->grant == message.grant);
    }

    void eachShapeIsAcceptedOnlyWhereItWasNegotiated()
    {
        for (const bool withWideband : {false, true}) {
            const SpectrumContextMessage message = sampleMessage(withWideband, true);
            const QJsonObject legacy = encodeRemoteSpectrumContext(message, false);
            const QJsonObject granted = encodeRemoteSpectrumContext(message, true);
            QVERIFY(decodeRemoteSpectrumContext(legacy, false).has_value());
            QVERIFY(!decodeRemoteSpectrumContext(legacy, true).has_value());
            QVERIFY(decodeRemoteSpectrumContext(granted, true).has_value());
            QVERIFY(!decodeRemoteSpectrumContext(granted, false).has_value());
            // What a minor-8 GUI always accepted, from the inline builder.
            const auto golden = decodeRemoteSpectrumContext(
                todaysContext(LegacyInputs{},
                              withWideband ? std::optional(activeWideband()) : std::nullopt),
                false);
            QVERIFY(golden.has_value());
            QVERIFY(!golden->grant.has_value());
            QCOMPARE(golden->wideband.has_value(), withWideband);
        }
    }

    // Parity Task 28: `transmit` travels only to a peer that declared
    // txDisplayVersion, on the grant shape, as a boolean; without it the
    // encoding is byte-for-byte today's.
    void transmitIsOneMoreFieldOnlyWhereDeclared()
    {
        for (const bool withWideband : {false, true}) {
            SpectrumContextMessage message = sampleMessage(withWideband, true);
            const QJsonObject today = encodeRemoteSpectrumContext(message, true);
            QVERIFY(!today.contains(QStringLiteral("transmit")));
            for (const bool transmit : {false, true}) {
                message.transmit = transmit;
                const QJsonObject declared = encodeRemoteSpectrumContext(message, true);
                QCOMPARE(declared.size(), today.size() + 1);
                QCOMPARE(declared.value(QStringLiteral("transmit")).toBool(), transmit);
                QJsonObject stripped = declared;
                stripped.remove(QStringLiteral("transmit"));
                QCOMPARE(compact(stripped), compact(today));
                const auto decoded = decodeRemoteSpectrumContext(declared, true, true);
                QVERIFY(decoded.has_value());
                compareMessages(*decoded, message);
                QCOMPARE(decoded->transmit, std::optional<bool>(transmit));
                QVERIFY(!decodeRemoteSpectrumContext(declared, true).has_value());
                QVERIFY(!decodeRemoteSpectrumContext(declared, false, true).has_value());
                QJsonObject notBool = declared;
                notBool.insert(QStringLiteral("transmit"), QStringLiteral("true"));
                QVERIFY(!decodeRemoteSpectrumContext(notBool, true, true).has_value());
            }
            // A declared peer refuses a context without the field.
            QVERIFY(!decodeRemoteSpectrumContext(today, true, true).has_value());
            message.transmit.reset();
            QVERIFY(!decodeRemoteSpectrumContext(today, true)->transmit.has_value());
        }
    }

    void missingOrExtraKeysAreRejected()
    {
        for (const bool grantNegotiated : {false, true}) {
            for (const bool withWideband : {false, true}) {
                const QJsonObject valid = encodeRemoteSpectrumContext(
                    sampleMessage(withWideband, true), grantNegotiated);
                QVERIFY(decodeRemoteSpectrumContext(valid, grantNegotiated).has_value());
                for (const QString& key : valid.keys()) {
                    QJsonObject missing = valid;
                    missing.remove(key);
                    if (key == QLatin1String("wideband")) {
                        // Without it this is the other valid wideband shape;
                        // the GUI refuses it because its subscription
                        // negotiated the extended view.
                        const auto decoded =
                            decodeRemoteSpectrumContext(missing, grantNegotiated);
                        QVERIFY(decoded.has_value() && !decoded->wideband.has_value());
                    } else {
                        QVERIFY2(!decodeRemoteSpectrumContext(missing, grantNegotiated)
                                      .has_value(),
                                 qPrintable(key));
                    }
                    // Swapping the key for an unknown one keeps the count.
                    missing.insert(QStringLiteral("unknown"), valid.value(key));
                    QVERIFY2(!decodeRemoteSpectrumContext(missing, grantNegotiated).has_value(),
                             qPrintable(key));
                }
                QJsonObject extra = valid;
                extra.insert(QStringLiteral("extra"), 1);
                QVERIFY(!decodeRemoteSpectrumContext(extra, grantNegotiated).has_value());
            }
        }
    }

    void fieldDefectsAreRejected_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QJsonValue>("value");
        QTest::addColumn<bool>("grantOnly");
        const auto row = [](const char* name, const char* key, const QJsonValue& value,
                            bool grantOnly) {
            QTest::newRow(name) << QString::fromLatin1(key) << value << grantOnly;
        };
        row("op", "op", QStringLiteral("contexts"), false);
        row("connectionId number", "connectionId", 1, false);
        row("endpointId zero", "endpointId", 0, false);
        row("endpointId fraction", "endpointId", 7.5, false);
        row("revision string", "revision", QStringLiteral("3"), false);
        row("generation fraction", "contextGeneration", 1.5, false);
        row("generation above u32", "contextGeneration", 4294967296.0, false);
        row("sourceStream 256", "sourceStream", 256, false);
        row("sampleRateHz zero", "sampleRateHz", 0, false);
        row("spanHz zero", "spanHz", 0, false);
        row("wideSpanHz above rate", "wideSpanHz", 192'001.0, false);
        row("traceSamples fraction", "traceSamples", 128.5, false);
        row("traceSamples 4097", "traceSamples", 4097, false);
        row("waterfallSamples zero", "waterfallSamples", 0, false);
        row("wideSamples 769", "wideSamples", 769, false);
        row("wideSamples zero with coverage", "wideSamples", 0, false);
        row("minDbm at maxDbm", "minDbm", -17.5, false);
        row("maxDbm above limit", "maxDbm", 100.5, false);
        row("fps fraction", "fps", 30.5, false);
        row("fps 61", "fps", 61, false);
        row("framesPerLine 10001", "framesPerLine", 10001, false);
        row("wideband not object", "wideband", 1, false);
        row("grantedFftSize fraction", "grantedFftSize", 4096.5, true);
        row("grantedFftSize string", "grantedFftSize", QStringLiteral("4096"), true);
        row("grantedFftSize zero", "grantedFftSize", 0, true);
        row("grantedFftSize above largest", "grantedFftSize",
            double(NereusSDR::FFTEngine::maximumFftSize()) * 2.0, true);
        row("grantedTier unknown", "grantedTier", QStringLiteral("medium"), true);
        row("grantedTier number", "grantedTier", 1, true);
        row("requestedPixels fraction", "requestedPixels", 1024.25, true);
        row("requestedPixels 4097", "requestedPixels", 4097, true);
        row("grantedPixels fraction", "grantedPixels", 256.5, true);
        row("grantedPixels zero", "grantedPixels", 0, true);
        row("grantedPixels above requested", "grantedPixels", 1025, true);
        row("limit unknown", "limit", QStringLiteral("shared-engine"), true);
        row("limit null", "limit", QJsonValue(QJsonValue::Null), true);
    }

    void fieldDefectsAreRejected()
    {
        QFETCH(QString, key);
        QFETCH(QJsonValue, value);
        QFETCH(bool, grantOnly);
        for (const bool grantNegotiated : {false, true}) {
            if (grantOnly && !grantNegotiated) { continue; }
            QJsonObject payload = encodeRemoteSpectrumContext(sampleMessage(true, true),
                                                              grantNegotiated);
            QVERIFY(payload.contains(key));
            payload.insert(key, value);
            QVERIFY(!decodeRemoteSpectrumContext(payload, grantNegotiated).has_value());
        }
    }
};

QTEST_APPLESS_MAIN(TstRemoteSpectrumContext)
#include "tst_remote_spectrum_context.moc"
