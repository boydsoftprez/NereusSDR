// =================================================================
// tests/tst_pan_rate_follows_stream.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Issue 701: MainWindow's RadioModel::wireSampleRateChanged handler. On
// Protocol 2 a stream can run at another rate than the wire rate (R-R3-49: a
// band's saved slice rate), and the handler must leave its FFT engine and the
// pan showing it on the stream's rate, or the bins are drawn across the wrong
// scale. Protocol 1 has one rate for the radio, and an inactive stream has no
// rate of its own; both take the wire rate. Runs a real MainWindow and fires
// the real signal.
//
// Limits: the signal is fired by hand rather than through connectToRadio or
// setSampleRateLive, and a null connection stands in for Protocol 2. A live
// change is staged by moveAllocatorQuietly, which leaves the state
// startSampleRateChange leaves. tst_desktop_station_window has the hosted row,
// a pan nothing picks a stream for.
//
// Modification history (NereusSDR):
//   2026-11-06: Live Setup change rows: inactive pans, the cached stream
//               windows, Clarity's source, the clamp, and Protocol 1.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-11-05: Created. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================
#include <QtTest/QtTest>

#include <QLoggingCategory>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QThread>

#include "core/AppSettings.h"
#include "core/FFTEngine.h"
#include "core/FFTRouter.h"
#include "core/HpsdrModel.h"
#include "core/P1RadioConnection.h"
#include "core/spectrum/FftEnginePool.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/MainWindow.h"
#include "gui/PanClarityRegistry.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kStreamRateHz = 192000;
constexpr double kWireRateHz = 1536000.0;

// The handler hands an engine its rate on the engine's own thread. Run a
// no-op there so everything posted before this has landed; a poll for the
// expected rate could pass before a wrong rate arrives.
void settleEngine(NereusSDR::FFTEngine* engine)
{
    if (engine->thread() != QThread::currentThread()) {
        QMetaObject::invokeMethod(engine, []() {}, Qt::BlockingQueuedConnection);
    }
}

// Detaches a stack-injected connection on scope exit, however the slot ends.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

} // namespace

class TestPanRateFollowsStream : public QObject {
    Q_OBJECT
    QString m_isolatedSettingsPath;

    // A window on one pan showing stream 0, running at kStreamRateHz. No
    // connection, so sampleRateIsRadioWide() is false, as on Protocol 2.
    static void bringUp(MainWindow& window)
    {
        window.resize(1280, 800);
        window.show();
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, kStreamRateHz);
        model->setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(model->addSlice(QStringLiteral("pan-0")) >= 0);
        QCOMPARE(model->streamSampleRateHzForTest(0), kStreamRateHz);
        QVERIFY(!model->sampleRateIsRadioWide());
        PanadapterStack* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        QTRY_COMPARE(stack->spectrum(QStringLiteral("pan-0"))->sampleRate(),
                     static_cast<double>(kStreamRateHz));
    }

    // What a live Setup change leaves (RadioModel::startSampleRateChange):
    // the allocator on the new rate, nothing published.
    static void moveAllocatorQuietly(RadioModel* model, const QList<int>& streams, int rateHz)
    {
        const QSignalBlocker quiet(model);
        for (int stream : streams) {
            QVERIFY(model->setStreamSampleRate(stream, rateHz));
            QCOMPARE(model->streamSampleRateHzForTest(stream), rateHz);
        }
    }

    // bringUp, then a second pan with its own slice on its own stream.
    // Returns that stream through secondStream; pan-0 is left active.
    static void bringUpTwoPans(MainWindow& window, int& secondStream)
    {
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        QVERIFY(QMetaObject::invokeMethod(&window, "applyPanLayout",
                                          Q_ARG(QString, QStringLiteral("2v"))));
        PanadapterStack* stack = window.findChild<PanadapterStack*>();
        SpectrumWidget* secondary = stack->spectrum(QStringLiteral("pan-1"));
        QVERIFY(secondary);
        QTRY_VERIFY(secondary->vfoWidget(1));
        const SliceModel* second = model->sliceById(1);
        QVERIFY(second);
        secondStream = second->streamIndex();
        QVERIFY(secondStream > 0);
        QTRY_COMPARE(secondary->sampleRate(), static_cast<double>(kStreamRateHz));
        stack->setActivePan(QStringLiteral("pan-0"));
        QCOMPARE(stack->activePanId(), QStringLiteral("pan-0"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        const QString profile =
            QStringLiteral("pan-rate-follows-stream-%1").arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        m_isolatedSettingsPath = AppSettings::resolveSettingsPath(profile);
        QCOMPARE(AppSettings::instance().filePath(), m_isolatedSettingsPath);
        AppSettings::instance().clear();
    }

    void init()
    {
        Test::markAudioFirstRunDone();
        AppSettings::instance().setValue(QStringLiteral("PanLayoutId"), QStringLiteral("1"));
    }

    void cleanup()
    {
        if (AppSettings::instance().filePath() == m_isolatedSettingsPath) {
            AppSettings::instance().clear();
        }
    }

    // The reported case: the stream came back at 192 kHz, the wire says
    // 1536 kHz. The engine stays on the stream's rate.
    void p2EngineKeepsTheStreamRateAgainstTheWire()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        NereusSDR::FFTEngine* engine = window.fftEnginePoolForTest()->engineForStream(0);
        QVERIFY(engine);

        emit window.radioModel()->wireSampleRateChanged(kWireRateHz);
        settleEngine(engine);

        QCOMPARE(engine->sampleRate(), static_cast<double>(kStreamRateHz));
    }

    // So does the pan. With extended view off the ceiling is the DDC rate, so
    // a view wider than the stream, legal only against the wire rate, comes
    // in to the stream's rate.
    void p2PanKeepsTheStreamRateAndClampsAgainstIt()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        SpectrumWidget* sw =
            window.findChild<PanadapterStack*>()->spectrum(QStringLiteral("pan-0"));
        sw->setExtendedViewAllowed(false);
        sw->setFrequencyRange(sw->centerFrequency(), kWireRateHz);

        emit window.radioModel()->wireSampleRateChanged(kWireRateHz);

        QCOMPARE(sw->sampleRate(), static_cast<double>(kStreamRateHz));
        QCOMPARE(sw->bandwidth(), static_cast<double>(kStreamRateHz));
    }

    // With extended view on, a span past the DDC is a legal state and stays,
    // now drawn as the stream's island with wings rather than stretched.
    void p2ExtendedSpanStaysOverTheStreamRate()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        SpectrumWidget* sw =
            window.findChild<PanadapterStack*>()->spectrum(QStringLiteral("pan-0"));
        QVERIFY(sw->extendedViewAllowed());
        QVERIFY(sw->maxZoomOutBandwidthHz() >= kWireRateHz);
        sw->setFrequencyRange(sw->centerFrequency(), kWireRateHz);

        emit window.radioModel()->wireSampleRateChanged(kWireRateHz);

        QCOMPARE(sw->sampleRate(), static_cast<double>(kStreamRateHz));
        QCOMPARE(sw->bandwidth(), kWireRateHz);
    }

    // The same signal does not throw away a zoom inside the stream.
    void p2ZoomInsideTheStreamSurvives()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        SpectrumWidget* sw =
            window.findChild<PanadapterStack*>()->spectrum(QStringLiteral("pan-0"));
        const double zoomedSpanHz = kStreamRateHz / 4.0;
        sw->setFrequencyRange(sw->centerFrequency(), zoomedSpanHz);
        const double centreHz = sw->centerFrequency();

        emit window.radioModel()->wireSampleRateChanged(kWireRateHz);

        QCOMPARE(sw->sampleRate(), static_cast<double>(kStreamRateHz));
        QCOMPARE(sw->bandwidth(), zoomedSpanHz);
        QCOMPARE(sw->centerFrequency(), centreHz);
    }

    // Two streams at two rates: each engine keeps its own, and the active pan
    // takes the rate of the stream it shows, which is not stream 0 here.
    void p2ActivePanUsesTheStreamItShows()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        QVERIFY(QMetaObject::invokeMethod(&window, "applyPanLayout",
                                          Q_ARG(QString, QStringLiteral("2v"))));
        PanadapterStack* stack = window.findChild<PanadapterStack*>();
        SpectrumWidget* secondary = stack->spectrum(QStringLiteral("pan-1"));
        QVERIFY(secondary);
        QTRY_VERIFY(secondary->vfoWidget(1));
        const SliceModel* second = model->sliceById(1);
        QVERIFY(second);
        const int stream = second->streamIndex();
        QVERIFY(stream > 0);

        constexpr int kSecondRateHz = 96000;
        QVERIFY(model->setStreamSampleRate(stream, kSecondRateHz));
        QVERIFY(model->waitForReceiveLaneForTest());
        QTRY_COMPARE(secondary->sampleRate(), static_cast<double>(kSecondRateHz));
        secondary->setExtendedViewAllowed(false);
        secondary->setFrequencyRange(secondary->centerFrequency(), kWireRateHz);
        stack->setActivePan(QStringLiteral("pan-1"));
        QCOMPARE(stack->activePanId(), QStringLiteral("pan-1"));

        FftEnginePool* pool = window.fftEnginePoolForTest();
        NereusSDR::FFTEngine* firstEngine = pool->engineForStream(0);
        NereusSDR::FFTEngine* secondEngine = pool->engineForStream(stream);
        QVERIFY(firstEngine);
        QVERIFY(secondEngine);

        emit model->wireSampleRateChanged(kWireRateHz);
        settleEngine(firstEngine);
        settleEngine(secondEngine);

        QCOMPARE(secondary->sampleRate(), static_cast<double>(kSecondRateHz));
        QCOMPARE(firstEngine->sampleRate(), static_cast<double>(kStreamRateHz));
        QCOMPARE(secondEngine->sampleRate(), static_cast<double>(kSecondRateHz));
        QCOMPARE(secondary->bandwidth(), static_cast<double>(kSecondRateHz));
    }

    // Live Setup change, two pans on two streams: the inactive pan moves with
    // the active one, at once and not only when it is clicked.
    void liveChangeReachesTheInactivePan()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        int stream = -1;
        bringUpTwoPans(window, stream);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        PanadapterStack* stack = window.findChild<PanadapterStack*>();
        SpectrumWidget* primary = stack->spectrum(QStringLiteral("pan-0"));
        SpectrumWidget* secondary = stack->spectrum(QStringLiteral("pan-1"));
        constexpr int kNewRateHz = 1536000;
        moveAllocatorQuietly(model, {0, stream}, kNewRateHz);
        if (QTest::currentTestFailed()) { return; }
        QCOMPARE(secondary->sampleRate(), static_cast<double>(kStreamRateHz));

        emit model->wireSampleRateChanged(kNewRateHz);

        QCOMPARE(primary->sampleRate(), static_cast<double>(kNewRateHz));
        QCOMPARE(secondary->sampleRate(), static_cast<double>(kNewRateHz));
        stack->setActivePan(QStringLiteral("pan-1"));
        QCOMPARE(secondary->sampleRate(), static_cast<double>(kNewRateHz));
    }

    // Live Setup change down, extended view off: the inactive pan's span comes
    // in to the new rate as the active pan's does.
    void liveChangeClampsTheInactivePan()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        int stream = -1;
        bringUpTwoPans(window, stream);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        SpectrumWidget* secondary =
            window.findChild<PanadapterStack*>()->spectrum(QStringLiteral("pan-1"));
        constexpr int kHighRateHz = 768000;
        QVERIFY(model->setStreamSampleRate(stream, kHighRateHz));
        QTRY_COMPARE(secondary->sampleRate(), static_cast<double>(kHighRateHz));
        secondary->setExtendedViewAllowed(false);
        secondary->setFrequencyRange(secondary->centerFrequency(), kHighRateHz);
        moveAllocatorQuietly(model, {0, stream}, kStreamRateHz);
        if (QTest::currentTestFailed()) { return; }

        emit model->wireSampleRateChanged(kStreamRateHz);

        QCOMPARE(secondary->sampleRate(), static_cast<double>(kStreamRateHz));
        QCOMPARE(secondary->bandwidth(), static_cast<double>(kStreamRateHz));
    }

    // Live Setup change, one pan with slices A and B on two streams: selecting
    // B afterwards replays the cached window, which has to hold the new rate.
    void liveChangeReachesTheCachedWindowOfTheOtherSlice()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        PanadapterStack* stack = window.findChild<PanadapterStack*>();
        SpectrumWidget* sw = stack->spectrum(QStringLiteral("pan-0"));
        PanadapterApplet* applet = stack->panadapter(QStringLiteral("pan-0"));
        QVERIFY(applet);
        const int aId = model->slices().first()->sliceIndex();
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(bId >= 0);
        SliceModel* a = model->sliceById(aId);
        SliceModel* b = model->sliceById(bId);
        QVERIFY(a && b);
        b->setFrequency(a->frequency() + 2'000'000.0);
        const int aStream = a->streamIndex();
        const int bStream = b->streamIndex();
        QVERIFY(aStream >= 0 && bStream >= 0 && aStream != bStream);
        applet->setActiveSliceIndex(aId);
        constexpr int kNewRateHz = 1536000;
        moveAllocatorQuietly(model, {aStream, bStream}, kNewRateHz);
        if (QTest::currentTestFailed()) { return; }
        QCOMPARE(b->streamIndex(), bStream);

        emit model->wireSampleRateChanged(kNewRateHz);
        QCOMPARE(sw->sampleRate(), static_cast<double>(kNewRateHz));

        applet->setActiveSliceIndex(bId);

        QCOMPARE(applet->activeSliceIndex(), bId);
        QCOMPARE(sw->sampleRate(), static_cast<double>(kNewRateHz));
    }

    // Clarity's source is the cached window, so it hears of a live change too.
    void liveChangeReachesClaritysSource()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        auto* registry = window.findChild<PanClarityRegistry*>();
        QVERIFY(registry);
        const QString pan = QStringLiteral("pan-0");
        QVERIFY(registry->sourceForPanForTest(pan).has_value());
        QCOMPARE(registry->sourceForPanForTest(pan)->sampleRateHz,
                 static_cast<double>(kStreamRateHz));
        constexpr int kNewRateHz = 1536000;
        moveAllocatorQuietly(model, {0}, kNewRateHz);
        if (QTest::currentTestFailed()) { return; }

        emit model->wireSampleRateChanged(kNewRateHz);

        QVERIFY(registry->sourceForPanForTest(pan).has_value());
        QCOMPARE(registry->sourceForPanForTest(pan)->sampleRateHz,
                 static_cast<double>(kNewRateHz));
    }

    // Protocol 1 carries one rate for the radio: the wire rate wins even with
    // the allocator holding another.
    void p1FollowsTheWire()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        P1RadioConnection connection;
        DetachConnection detach{model};
        model->injectConnectionForTest(&connection);
        QVERIFY(model->sampleRateIsRadioWide());
        NereusSDR::FFTEngine* engine = window.fftEnginePoolForTest()->engineForStream(0);
        QVERIFY(engine);
        SpectrumWidget* sw =
            window.findChild<PanadapterStack*>()->spectrum(QStringLiteral("pan-0"));
        constexpr double kP1WireRateHz = 384000.0;

        emit model->wireSampleRateChanged(kP1WireRateHz);
        settleEngine(engine);

        QCOMPARE(engine->sampleRate(), kP1WireRateHz);
        QCOMPARE(sw->sampleRate(), kP1WireRateHz);
    }

    // Protocol 1, two pans: the inactive pan takes the wire rate as well.
    void p1InactivePanFollowsTheWire()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        int stream = -1;
        bringUpTwoPans(window, stream);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        P1RadioConnection connection;
        DetachConnection detach{model};
        model->injectConnectionForTest(&connection);
        QVERIFY(model->sampleRateIsRadioWide());
        SpectrumWidget* secondary =
            window.findChild<PanadapterStack*>()->spectrum(QStringLiteral("pan-1"));
        constexpr double kP1WireRateHz = 384000.0;

        emit model->wireSampleRateChanged(kP1WireRateHz);

        QCOMPARE(secondary->sampleRate(), kP1WireRateHz);
    }

    // An inactive pan carrying no stream has no bins to scale: it keeps its
    // view until a stream reaches it. The active pan is the wire-rate case.
    void inactivePanWithNoStreamKeepsItsView()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        int stream = -1;
        bringUpTwoPans(window, stream);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        SpectrumWidget* secondary =
            window.findChild<PanadapterStack*>()->spectrum(QStringLiteral("pan-1"));
        model->removeSlice(1);
        QTRY_VERIFY(!model->sliceById(1));
        QTRY_VERIFY(!model->fftRouter()->receiversForPan(QStringLiteral("pan-1")).contains(stream));
        constexpr double kLeftRateHz = 384000.0;
        secondary->setSampleRate(kLeftRateHz);

        emit model->wireSampleRateChanged(kWireRateHz);

        QCOMPARE(secondary->sampleRate(), kLeftRateHz);
    }

    // A stream the allocator holds no active DDC for has no rate of its own.
    void p2InactiveStreamFollowsTheWire()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        bringUp(window);
        if (QTest::currentTestFailed()) { return; }
        RadioModel* model = window.radioModel();
        constexpr int kInactiveStream = 3;
        QVERIFY(!model->streamActiveForTest(kInactiveStream));
        // The signal that builds a stream's engine, with the allocator left
        // alone so the stream stays inactive there.
        emit model->streamCentreChanged(kInactiveStream, 14200000.0, kStreamRateHz);
        NereusSDR::FFTEngine* engine = window.fftEnginePoolForTest()->engineForStream(kInactiveStream);
        QVERIFY(engine);
        settleEngine(engine);
        QCOMPARE(engine->sampleRate(), static_cast<double>(kStreamRateHz));

        emit model->wireSampleRateChanged(kWireRateHz);
        settleEngine(engine);

        QCOMPARE(engine->sampleRate(), kWireRateHz);
    }
};

QTEST_MAIN(TestPanRateFollowsStream)
#include "tst_pan_rate_follows_stream.moc"
