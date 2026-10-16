// no-port-check: NereusSDR-original production-path display regressions.
// Modification history (NereusSDR):
//   2026-10-14 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//              saved narrow zoom and active toast anchoring regressions.
//   2026-10-16 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//              the saved span is read from pan 0's own key, where the
//              per-pan display settings save it.

#include <QtTest/QtTest>
#include <QFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <memory>

#include "core/AppSettings.h"
#include "core/settings/PanDisplayKeys.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/MainWindow.h"
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
