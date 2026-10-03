// no-port-check: NereusSDR-original. Strict negotiated ADC metadata contract.
#include <QtTest>
#include "core/session/media/WidebandDisplayContext.h"

#include <limits>

using namespace NereusSDR;

namespace {
WidebandDisplayContext activeSource()
{
    return {true, true, 1, 0, 17, 122880000.0};
}
}

class TestWidebandDisplayContext : public QObject {
    Q_OBJECT
private slots:
    void roundTripAvailabilityAndCapture()
    {
        for (int adc = 0; adc < 2; ++adc) {
            for (bool active : {false, true}) {
                auto source = activeSource();
                source.physicalAdcIndex = adc;
                source.active = active;
                source.sourceGeneration = active ? 17 : 0;
                const auto wire = source.toJson();
                QCOMPARE(wire.size(), 11);
                const auto decoded = WidebandDisplayContext::fromJson(wire);
                QVERIFY(decoded);
                QCOMPARE(*decoded, source);
            }
        }
        const WidebandDisplayContext unavailable;
        const auto wire = unavailable.toJson();
        QCOMPARE(wire.size(), 3);
        QVERIFY(!wire.contains(QStringLiteral("adcRateHz")));
        const auto decoded = WidebandDisplayContext::fromJson(wire);
        QVERIFY(decoded);
        QCOMPARE(*decoded, unavailable);
    }

    void malformedWire_data()
    {
        QTest::addColumn<QJsonObject>("wire");
        const auto valid = activeSource().toJson();
        const auto changed = [&](const char* name, const char* key, QJsonValue value) {
            auto wire = valid;
            wire.insert(QLatin1String(key), value);
            QTest::newRow(name) << wire;
        };
        for (const auto& key : valid.keys()) {
            auto wire = valid;
            wire.remove(key);
            QTest::newRow(qPrintable(QStringLiteral("missing-") + key)) << wire;
        }
        auto unknown = valid;
        unknown.insert(QStringLiteral("future"), 1);
        QTest::newRow("unknown-field") << unknown;
        changed("future-version", "version", 2);
        changed("string-version", "version", QStringLiteral("1"));
        changed("nonbool-availability", "available", 1);
        changed("nonbool-activation", "active", QStringLiteral("true"));
        changed("unavailable-with-source", "available", false);
        changed("inactive-with-generation", "active", false);
        changed("adc-fraction", "physicalAdcIndex", 0.5);
        changed("adc-negative", "physicalAdcIndex", -1);
        changed("adc-unsupported", "physicalAdcIndex", 2);
        changed("chain-string", "filterChainIndex", QStringLiteral("0"));
        changed("chain-negative", "filterChainIndex", -1);
        changed("chain-unsupported", "filterChainIndex", 2);
        changed("generation-zero", "sourceGeneration", 0);
        changed("generation-negative", "sourceGeneration", -1);
        changed("generation-fraction", "sourceGeneration", 3.5);
        changed("generation-overflow", "sourceGeneration", 4294967296.0);
        changed("generation-string", "sourceGeneration", QStringLiteral("17"));
        changed("rate-zero", "adcRateHz", 0);
        changed("rate-negative", "adcRateHz", -1);
        changed("rate-string", "adcRateHz", QStringLiteral("122880000"));
        changed("rate-null", "adcRateHz", QJsonValue::Null);
        changed("low-not-zero", "lowHz", 1);
        changed("high-not-nyquist", "highHz", 192000);
        changed("false-rate-basis", "geometryRateBasis", QStringLiteral("measured"));
        changed("false-level-reference", "levelReference", QStringLiteral("absoluteDbm"));
        auto unavailable = WidebandDisplayContext{}.toJson();
        unavailable.insert(QStringLiteral("active"), true);
        QTest::newRow("active-unavailable") << unavailable;
        unavailable = WidebandDisplayContext{}.toJson();
        unavailable.insert(QStringLiteral("sourceGeneration"), 0);
        QTest::newRow("unavailable-extra-source") << unavailable;
    }

    void malformedWire()
    {
        QFETCH(QJsonObject, wire);
        QVERIFY(!WidebandDisplayContext::fromJson(wire));
    }

    void invalidLocalMetadataCannotBeSerialized()
    {
        const auto rejected = [](const WidebandDisplayContext& source) {
            QVERIFY(!source.valid());
            QVERIFY(source.toJson().isEmpty());
            QVERIFY(!WidebandDisplayContext::fromJson(source.toJson()));
        };
        auto source = activeSource();
        source.sourceGeneration = 0;
        rejected(source);
        source = activeSource();
        source.adcRateHz = std::numeric_limits<double>::infinity();
        rejected(source);
        source.adcRateHz = std::numeric_limits<double>::quiet_NaN();
        rejected(source);
        source.adcRateHz = std::numeric_limits<double>::denorm_min();
        rejected(source); // Nyquist underflows to zero.
        source = {};
        source.physicalAdcIndex = 0;
        rejected(source);
        source = activeSource();
        source.active = false;
        rejected(source);
    }
};

QTEST_APPLESS_MAIN(TestWidebandDisplayContext)
#include "tst_wideband_display_context.moc"
