// =================================================================
// tests/tst_wideband_spectrum_cache.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original coverage for physical-ADC latest-frame
// identity, validation, generation, and rollback contracts.
// =================================================================

#include <QtTest/QtTest>

#include "core/WidebandFftEngine.h"
#include "core/spectrum/WidebandSpectrumCache.h"

#include <limits>

using namespace NereusSDR;

namespace {

WidebandCaptureIdentity identity(quint64 connection = 1,
                                 quint64 capture = 2,
                                 quint64 geometry = 3)
{
    return {connection, capture, geometry};
}

WidebandSpectrumFrame frameFor(const WidebandSourceDescriptor& source,
                               qint64 producedAtNs = 1,
                               float level = -100.0f)
{
    return {source, producedAtNs,
            QVector<float>(WidebandFftEngine::kOutputBins, level)};
}

} // namespace

class TestWidebandSpectrumCache : public QObject
{
    Q_OBJECT

private slots:
    void configuresAndPublishesExactFftShape()
    {
        WidebandSpectrumCache cache;
        const auto configured = cache.configureSource(0, identity(), 122'880'000.0);
        QVERIFY(configured);
        QVERIFY(configured->sourceGeneration != 0);
        QVERIFY(cache.source(0) == configured);

        const WidebandSpectrumFrame frame = frameFor(*configured, 42, -87.5f);
        QVERIFY(cache.publish(frame));
        const auto latest = cache.latest(0);
        QVERIFY(latest);
        QVERIFY(latest->source == *configured);
        QCOMPARE(latest->producedAtNs, 42);
        QCOMPARE(latest->rawDbBins.size(), WidebandFftEngine::kOutputBins);
        QCOMPARE(latest->rawDbBins[0], -87.5f);
    }

    void invalidConfigurationRetiresOnlyItsValidAdc()
    {
        WidebandSpectrumCache cache;
        const auto adc0 = cache.configureSource(0, identity(), 1.0);
        const auto adc1 = cache.configureSource(1, identity(4, 5, 6), 2.0);
        QVERIFY(adc0);
        QVERIFY(adc1);
        QVERIFY(cache.publish(frameFor(*adc0)));
        QVERIFY(cache.publish(frameFor(*adc1)));

        for (const WidebandCaptureIdentity& invalid : {
                 identity(0, 1, 1), identity(1, 0, 1), identity(1, 1, 0)}) {
            QVERIFY(!cache.configureSource(0, invalid, 1.0));
            QVERIFY(!cache.source(0));
            QVERIFY(!cache.latest(0));
            QVERIFY(cache.source(1));
            QVERIFY(cache.latest(1));
            QVERIFY(cache.configureSource(0, identity(), 1.0));
            QVERIFY(cache.publish(frameFor(*cache.source(0))));
        }

        for (double invalidRate : {0.0, -1.0,
                                   std::numeric_limits<double>::infinity(),
                                   std::numeric_limits<double>::quiet_NaN()}) {
            QVERIFY(!cache.configureSource(0, identity(), invalidRate));
            QVERIFY(!cache.source(0));
            QVERIFY(!cache.latest(0));
            QVERIFY(cache.source(1));
        }
        // The largest finite normal rate still has a finite, positive Nyquist.
        QVERIFY(cache.configureSource(0, identity(), std::numeric_limits<double>::max()));
    }

    void materialChangesMintGenerationAndRejectOldFrames()
    {
        WidebandSpectrumCache cache;
        const auto original = cache.configureSource(0, identity(), 1.0);
        QVERIFY(original);
        QVERIFY(cache.publish(frameFor(*original, 7)));
        const auto identical = cache.configureSource(0, identity(), 1.0);
        QVERIFY(identical == original);
        QCOMPARE(cache.latest(0)->producedAtNs, 7);

        const auto captureChanged = cache.configureSource(0, identity(1, 9, 3), 1.0);
        QVERIFY(captureChanged->sourceGeneration > original->sourceGeneration);
        QVERIFY(!cache.latest(0));
        QVERIFY(!cache.publish(frameFor(*original, 8)));

        const auto connectionChanged = cache.configureSource(0, identity(8, 9, 3), 1.0);
        QVERIFY(connectionChanged->sourceGeneration > captureChanged->sourceGeneration);
        const auto rateChanged = cache.configureSource(0, identity(8, 9, 3), 2.0);
        QVERIFY(rateChanged->sourceGeneration > connectionChanged->sourceGeneration);
        QVERIFY(!cache.publish(frameFor(*connectionChanged, 9)));
    }

    void sourcesAreIndependentAndRetirementNeverRewindsGeneration()
    {
        WidebandSpectrumCache cache;
        const auto adc0 = cache.configureSource(0, identity(), 1.0);
        const auto adc1 = cache.configureSource(1, identity(4, 5, 6), 1.0);
        QVERIFY(adc0);
        QVERIFY(adc1);
        QVERIFY(adc1->sourceGeneration > adc0->sourceGeneration);
        QVERIFY(cache.publish(frameFor(*adc0, 10)));
        QVERIFY(cache.publish(frameFor(*adc1, 20)));

        cache.invalidate(0);
        QVERIFY(!cache.source(0));
        QVERIFY(!cache.latest(0));
        QCOMPARE(cache.latest(1)->producedAtNs, 20);
        QVERIFY(!cache.publish(frameFor(*adc0, 11)));
        const auto afterInvalidate = cache.configureSource(0, identity(), 1.0);
        QVERIFY(afterInvalidate->sourceGeneration > adc1->sourceGeneration);

        cache.clear();
        QVERIFY(!cache.source(0));
        QVERIFY(!cache.source(1));
        QVERIFY(!cache.publish(frameFor(*afterInvalidate, 12)));
        const auto afterClear = cache.configureSource(1, identity(7, 8, 9), 1.0);
        QVERIFY(afterClear->sourceGeneration > afterInvalidate->sourceGeneration);
        cache.invalidate(-1);
        cache.invalidate(WidebandSpectrumCache::kMaxSources);
        QVERIFY(cache.source(1) == afterClear);
    }

    void malformedAndOutOfOrderFramesPreserveLatest()
    {
        WidebandSpectrumCache cache;
        const auto descriptor = cache.configureSource(0, identity(), 1.0);
        QVERIFY(descriptor);
        QVERIFY(cache.publish(frameFor(*descriptor, 100, -80.0f)));

        WidebandSpectrumFrame malformed = frameFor(*descriptor, 101, -70.0f);
        malformed.rawDbBins.removeLast();
        QVERIFY(!cache.publish(malformed));
        malformed = frameFor(*descriptor, 101, -70.0f);
        malformed.rawDbBins[4] = std::numeric_limits<float>::quiet_NaN();
        QVERIFY(!cache.publish(malformed));
        malformed = frameFor(*descriptor, -1, -70.0f);
        QVERIFY(!cache.publish(malformed));
        QVERIFY(!cache.publish(frameFor(*descriptor, 99, -70.0f)));
        QCOMPARE(cache.latest(0)->producedAtNs, 100);
        QCOMPARE(cache.latest(0)->rawDbBins[0], -80.0f);

        QVERIFY(cache.publish(frameFor(*descriptor, 100, -60.0f)));
        QCOMPARE(cache.latest(0)->rawDbBins[0], -60.0f);
    }
};

QTEST_GUILESS_MAIN(TestWidebandSpectrumCache)
#include "tst_wideband_spectrum_cache.moc"
