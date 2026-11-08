// no-port-check: NereusSDR-original production-path display regressions.
// Modification history (NereusSDR):
//   2026-10-14 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//              saved narrow zoom and active toast anchoring regressions.
//   2026-10-16 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//              the saved span is read from pan 0's own key, where the
//              per-pan display settings save it.
//   2026-11-05 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//              each window on a Core keeps its own paint rate.

#include <QtTest/QtTest>
#include <QFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <memory>

#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/settings/PanDisplayKeys.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace NereusSDR {

// Access only: every notification still executes production showToast,
// restacking, destruction wiring and the window's real geometry events.
class DisplaySettingsCorrectnessAccess {
public:
    static StatusToast* showToast(MainWindow& window, const QString& message)
    {
        return window.showToast(message, ToastSeverity::Warning, 60000);
    }

};

} // namespace NereusSDR

using namespace NereusSDR;

class TestDisplaySettingsCorrectness : public QObject {
    Q_OBJECT

private:
    static void settleLayout()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }

    static std::unique_ptr<MainWindow> makeWindow()
    {
        RemoteStationOptions station;
        station.url = QStringLiteral("ws://127.0.0.1:1");
        return std::make_unique<MainWindow>(station, nullptr,
                                            MainWindow::ConnectionStartup::Deferred);
    }

    static QPoint expectedPosition(const MainWindow& window,
                                   const StatusToast& toast, int belowHeight = 0)
    {
        // The existing operator contract, including its global frame anchor.
        const QRect frame = window.frameGeometry();
        return {frame.right() - 20 - toast.width(),
                frame.bottom() - 60 - belowHeight - toast.height()};
    }

    // The FPS row of the active pan's Display pill.
    static SpectrumOverlayPanel* pillOf(SpectrumWidget* spectrum)
    {
        return spectrum ? spectrum->findChild<SpectrumOverlayPanel*>() : nullptr;
    }

    // A Core's settings, as a remote window holds them. Only the Core's rate
    // is named, so a window with no key of its own has something to read.
    static void holdCoreRate(SettingsProxy& proxy, const char* fps)
    {
        proxy.applySnapshot({{QLatin1String(ControlRanges::kDisplaySpectrumFpsKey),
                              QLatin1String(fps)}});
        proxy.setReady(true);
    }

    static void arriveFromCore(MainWindow& window, SettingsProxy& proxy, const char* fps)
    {
        const QString key = QLatin1String(ControlRanges::kDisplaySpectrumFpsKey);
        proxy.applyRemoteValue(key, QLatin1String(fps), QStringLiteral("another-window"));
        emit window.radioModel()->stationSettingChanged(key);
    }

    static int paintPeriodMs(int fps) { return 1000 / fps; }

    static void addToastRows()
    {
        QTest::addColumn<int>("count");
        QTest::newRow("one-toast") << 1;
        QTest::newRow("two-toasts") << 2;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        const QString profile = QStringLiteral("display-settings-correctness-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
        qApp->setStyle(QStringLiteral("Fusion"));
    }

    void init()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        AppSettings::instance().clear();
        Test::markAudioFirstRunDone();
        AppSettings::instance().setValue(QStringLiteral("TciServerEnabled"), QStringLiteral("False"));
        for (const QString& key : {QStringLiteral("DxClusterAutoConnect"), QStringLiteral("RbnAutoConnect"),
                                   QStringLiteral("WsjtxAutoStart"), QStringLiteral("SpotCollectorAutoStart"),
                                   QStringLiteral("PotaAutoStart"), QStringLiteral("FreeDvAutoStart"),
                                   QStringLiteral("PskReporterAutoStart")}) {
            AppSettings::instance().setValue(key, QStringLiteral("False"));
        }
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void savedZoomSurvivesFirstSliceWiring_data()
    {
        QTest::addColumn<double>("savedSpan");
        QTest::addColumn<bool>("extended");
        QTest::addColumn<double>("expectedSpan");
        QTest::newRow("minimum-1k") << 1000.0 << false << 1000.0;
        QTest::newRow("narrow-2.5k") << 2500.0 << false << 2500.0;
        QTest::newRow("narrow-9999") << 9999.0 << false << 9999.0;
        QTest::newRow("legacy-10k-boundary") << 10000.0 << false << 10000.0;
        QTest::newRow("below-minimum") << 999.0 << false << 768000.0;
        QTest::newRow("nonpositive") << 0.0 << false << 768000.0;
        QTest::newRow("ddc-ceiling") << 768000.0 << false << 768000.0;
        QTest::newRow("above-ddc-ceiling") << 768001.0 << false << 768000.0;
        QTest::newRow("extended-ceiling") << 2000000.0 << true << 2000000.0;
        QTest::newRow("above-extended-ceiling") << 2001000.0 << true << 768000.0;
    }

    void savedZoomSurvivesFirstSliceWiring()
    {
        QFETCH(double, savedSpan);
        QFETCH(bool, extended);
        QFETCH(double, expectedSpan);

        // Save through the real receive-span persistence path, then reload
        // the file and let production pan construction read that setting.
        {
            SpectrumWidget previous;
            previous.setFrequencyRange(14200000.0, savedSpan);
            previous.saveSettings();
            QVERIFY(AppSettings::instance().save());
        }
        AppSettings::instance().load();
        // A pan saves its span under its own key (DisplayBandwidth_0); the
        // bare key is the Setup default, which no pan writes.
        QVERIFY(PanDisplayKeys::hasOverride(QStringLiteral("DisplayBandwidth"), 0));
        QCOMPARE(PanDisplayKeys::read(QStringLiteral("DisplayBandwidth"), 0, QVariant()).toDouble(),
                 savedSpan);

        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        const auto dropBackend = qScopeGuard([] { AppSettings::instance().setRemoteBackend(nullptr); });
        QSignalSpy outbound(&proxy, &SettingsProxy::outboundWriteRequested);
        auto window = makeWindow();
        QVERIFY(!proxy.ready());
        QVERIFY(!window->radioModel()->ownsLocalDsp());
        QVERIFY(window->radioModel()->connection() == nullptr);
        SpectrumWidget* spectrum = window->activeSpectrumWidget();
        QVERIFY(spectrum != nullptr);
        QCOMPARE(spectrum->bandwidth(), savedSpan);
        spectrum->setExtendedViewAllowed(extended);
        spectrum->setWidebandAdcRateHz(4000000.0);
        // Withdrawing extended permission may clamp the live view. Reload
        // the original persisted input so invalid-value rows specifically
        // exercise startup validation, rather than that unrelated toggle.
        spectrum->loadSettings();
        QCOMPARE(spectrum->bandwidth(), savedSpan);
        // No remote spectrum context has arrived: startup uses the existing
        // pre-context extended ceiling. Preserve the provisional center.
        const double initialCenter = spectrum->centerFrequency();
        QVERIFY(window->radioModel()->sliceById(0) == nullptr);
        QCOMPARE(window->radioModel()->addSliceWithStationId(0), 0);
        QCOMPARE(spectrum->bandwidth(), expectedSpan);
        QCOMPARE(spectrum->centerFrequency(), initialCenter);
        QVERIFY(window->radioModel()->sliceById(0)->panKey().isEmpty());
        QCOMPARE(outbound.count(), 0);
        QVERIFY(!proxy.ready());
        QVERIFY(window->radioModel()->connection() == nullptr);
    }

    // JJ's ruling of 2026-11-05: each window keeps its own paint rate; the
    // Core's rate stays one shared setting.
    void remoteWindowsPillSetsItsOwnRateAndLeavesTheCores()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        const auto dropBackend = qScopeGuard([] { AppSettings::instance().setRemoteBackend(nullptr); });
        holdCoreRate(proxy, "30");
        QSignalSpy outbound(&proxy, &SettingsProxy::outboundWriteRequested);
        auto window = makeWindow();
        SpectrumWidget* spectrum = window->activeSpectrumWidget();
        QVERIFY(spectrum);
        SpectrumOverlayPanel* pill = pillOf(spectrum);
        QVERIFY(pill);

        emit pill->fftFpsRequested(45);

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QLatin1String(ControlRanges::kDisplayPaintFpsKey)).toString(),
                 QStringLiteral("45"));
        QCOMPARE(s.value(QLatin1String(ControlRanges::kDisplaySpectrumFpsKey)).toString(),
                 QStringLiteral("30"));
        QCOMPARE(outbound.count(), 0);
        QCOMPARE(spectrum->displayTimerIntervalForTest(), paintPeriodMs(45));
        QCOMPARE(pill->fftState().fps, 45);
    }

    void remoteWindowsOwnRateSurvivesRestartAndTheCoresValueArriving()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        const auto dropBackend = qScopeGuard([] { AppSettings::instance().setRemoteBackend(nullptr); });
        holdCoreRate(proxy, "30");
        {
            auto window = makeWindow();
            emit pillOf(window->activeSpectrumWidget())->fftFpsRequested(45);
        }
        QVERIFY(AppSettings::instance().save());
        AppSettings::instance().load();

        auto window = makeWindow();
        SpectrumWidget* spectrum = window->activeSpectrumWidget();
        QVERIFY(spectrum);
        SpectrumOverlayPanel* pill = pillOf(spectrum);
        QVERIFY(pill);
        QCOMPARE(spectrum->displayTimerIntervalForTest(), paintPeriodMs(45));
        QCOMPARE(pill->fftState().fps, 45);

        // Another window moves the Core's rate: this one keeps its own.
        arriveFromCore(*window, proxy, "20");
        QCOMPARE(spectrum->displayTimerIntervalForTest(), paintPeriodMs(45));
        QCOMPARE(pill->fftState().fps, 45);
        QCOMPARE(AppSettings::instance().value(
                     QLatin1String(ControlRanges::kDisplayPaintFpsKey)).toString(),
                 QStringLiteral("45"));

        // Setup moves the Core's rate from this window: the Core's key moves,
        // and this window keeps painting at its own.
        QSignalSpy outbound(&proxy, &SettingsProxy::outboundWriteRequested);
        window->applyDisplayFps(25);
        QCOMPARE(AppSettings::instance().value(
                     QLatin1String(ControlRanges::kDisplaySpectrumFpsKey)).toString(),
                 QStringLiteral("25"));
        QCOMPARE(outbound.count(), 1);
        QCOMPARE(spectrum->displayTimerIntervalForTest(), paintPeriodMs(45));
        QCOMPARE(pill->fftState().fps, 45);
    }

    void remoteWindowWithNoKeyOfItsOwnTakesTheCoresRate()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        const auto dropBackend = qScopeGuard([] { AppSettings::instance().setRemoteBackend(nullptr); });
        holdCoreRate(proxy, "20");
        auto window = makeWindow();
        SpectrumWidget* spectrum = window->activeSpectrumWidget();
        QVERIFY(spectrum);
        SpectrumOverlayPanel* pill = pillOf(spectrum);
        QVERIFY(pill);
        QVERIFY(!AppSettings::instance().contains(QLatin1String(ControlRanges::kDisplayPaintFpsKey)));
        QCOMPARE(spectrum->displayTimerIntervalForTest(), paintPeriodMs(20));
        QCOMPARE(pill->fftState().fps, 20);

        // Still the Core's: a new value is followed, and no key appears.
        arriveFromCore(*window, proxy, "15");
        QCOMPARE(spectrum->displayTimerIntervalForTest(), paintPeriodMs(15));
        QCOMPARE(pill->fftState().fps, 15);
        window->applyDisplayFps(25);
        QCOMPARE(spectrum->displayTimerIntervalForTest(), paintPeriodMs(25));
        QCOMPARE(pill->fftState().fps, 25);
        QVERIFY(!AppSettings::instance().contains(QLatin1String(ControlRanges::kDisplayPaintFpsKey)));
    }

    void directToARadioThePillStillSetsTheOneRate()
    {
        auto& s = AppSettings::instance();
        // A key left from a session on a Core is not read here.
        s.setValue(QLatin1String(ControlRanges::kDisplayPaintFpsKey), QStringLiteral("12"));
        s.setValue(QLatin1String(ControlRanges::kDisplaySpectrumFpsKey), QStringLiteral("20"));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        QVERIFY(window.radioModel()->ownsLocalDsp());
        SpectrumWidget* spectrum = window.activeSpectrumWidget();
        QVERIFY(spectrum);
        SpectrumOverlayPanel* pill = pillOf(spectrum);
        QVERIFY(pill);
        QCOMPARE(spectrum->displayTimerIntervalForTest(), paintPeriodMs(20));
        QCOMPARE(pill->fftState().fps, 20);

        emit pill->fftFpsRequested(45);

        QCOMPARE(s.value(QLatin1String(ControlRanges::kDisplaySpectrumFpsKey)).toString(),
                 QStringLiteral("45"));
        QCOMPARE(s.value(QLatin1String(ControlRanges::kDisplayPaintFpsKey)).toString(),
                 QStringLiteral("12"));
        QCOMPARE(spectrum->displayTimerIntervalForTest(), paintPeriodMs(45));
        QCOMPARE(pill->fftState().fps, 45);
    }

    void activeToastsFollowWindowMove_data() { addToastRows(); }
    void activeToastsFollowWindowMove() { checkGeometryChange(true); }
    void activeToastsFollowWindowResize_data() { addToastRows(); }
    void activeToastsFollowWindowResize() { checkGeometryChange(false); }

    void dismissingNewestRestacksRemainingToast()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        const auto dropBackend = qScopeGuard([] { AppSettings::instance().setRemoteBackend(nullptr); });
        auto window = makeWindow();
        QVERIFY(!window->radioModel()->ownsLocalDsp());
        QVERIFY(window->radioModel()->connection() == nullptr);
        window->show();
        settleLayout();
        QPointer<StatusToast> older = DisplaySettingsCorrectnessAccess::showToast(*window, QStringLiteral("older notice"));
        QPointer<StatusToast> newer = DisplaySettingsCorrectnessAccess::showToast(*window, QStringLiteral("newer notice"));
        QVERIFY(older && newer);
        QCOMPARE(older->pos(), expectedPosition(*window, *older, newer->height() + 8));
        QCOMPARE(newer->pos(), expectedPosition(*window, *newer));
        newer->close();
        QTRY_VERIFY(newer.isNull());
        QCOMPARE(older->pos(), expectedPosition(*window, *older));
        QVERIFY(!proxy.ready());
        QVERIFY(window->radioModel()->connection() == nullptr);
    }

private:
    void checkGeometryChange(bool move)
    {
        QFETCH(int, count);
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        const auto dropBackend = qScopeGuard([] { AppSettings::instance().setRemoteBackend(nullptr); });
        QSignalSpy outbound(&proxy, &SettingsProxy::outboundWriteRequested);
        auto window = makeWindow();
        QVERIFY(!window->radioModel()->ownsLocalDsp());
        QVERIFY(window->radioModel()->connection() == nullptr);
        window->resize(1280, 800);
        window->move(100, 100);
        window->show();
        settleLayout();
        QPointer<StatusToast> older = DisplaySettingsCorrectnessAccess::showToast(*window, QStringLiteral("older notice"));
        QPointer<StatusToast> newer;
        if (count == 2) {
            newer = DisplaySettingsCorrectnessAccess::showToast(*window, QStringLiteral("newer notice"));
        }
        QVERIFY(older != nullptr);
        QVERIFY(older->isWindow());
        QCOMPARE(older->parentWidget(), window.get());
        const int belowHeight = newer ? newer->height() + 8 : 0;
        QCOMPARE(older->pos(), expectedPosition(*window, *older, belowHeight));
        if (newer) { QCOMPARE(newer->pos(), expectedPosition(*window, *newer)); }
        const QRect before = window->frameGeometry();
        if (move) {
            window->move(window->pos() + QPoint(93, 57));
        } else {
            window->resize(window->size() + QSize(170, 110));
        }
        settleLayout();
        QVERIFY(window->frameGeometry() != before);
        QTRY_COMPARE_WITH_TIMEOUT(older->pos(), expectedPosition(*window, *older, belowHeight), 1000);
        if (newer) { QCOMPARE(newer->pos(), expectedPosition(*window, *newer)); }
        QCOMPARE(outbound.count(), 0);
        QVERIFY(!proxy.ready());
        QVERIFY(window->radioModel()->connection() == nullptr);
    }
};

QTEST_MAIN(TestDisplaySettingsCorrectness)
#include "tst_display_settings_correctness.moc"
