// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_remote_core_log.cpp  (NereusSDR)
// =================================================================
//
// Remote-window parity Task 22 / the iPhone app plan's Task 25 (R-R3-49,
// R-IOS-18; acceptance B6.4, B6.6): the Core's log, its logging categories
// and its support bundle in a remote window, over the in-process loopback.
//
//   - The Core offers supportBundleVersion 1 after relayAllowed.
//   - The `coreLog` stream: a viewer's backlog, then new lines; secrets
//     removed; nothing once the last viewer lets go; Refresh reads the
//     backlog again (B6.6).
//   - radio's logCategories: the window sees the Core's categories and
//     follows a change made at the Core; the window's change turns the
//     Core's categories on and off (B6.4). The window's own logging stays
//     its own.
//   - support.collect: the Core's bundle, a ZIP of at most 2 MiB with the
//     Core's radio, written on a worker thread and answered later.
//   - On the air (the Core's MoxController, receive-only pre-check lifted)
//     both are taken, as a window at the Core takes them.
//   - The Support dialog and Setup > Diagnostics > Logs in the remote
//     window show the Core's categories and log and change them.
//
// No RF, no audio device; nothing keys a radio (the MoxController has no
// radio connection).
//
//   cmake --build build --target tst_remote_core_log
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_remote_core_log$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: split-message private-key case added with OpenAI Codex
//               assistance.
// =================================================================

#include <QtTest>

#include <QCheckBox>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/LogSink.h"
#include "core/MoxController.h"
#include "core/SupportBundle.h"
#include "core/ZipArchive.h"
#include "core/session/RecordStream.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/RemoteWindowHarness.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/SupportDialog.h"
#include "gui/diagnostics/DiagnosticsPhaseHPages.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

std::unique_ptr<RadioModel> makeCore()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:22");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A line in the Core's log, as the Core's message handler offers it.
void coreLogs(const QString& line)
{
    LogSink::instance().offer(line + QLatin1Char('\n'));
    LogSink::instance().drainNow();
}

struct Session {
    Session(RadioModel* core, const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
        , coreModel(core)
    {
        server = std::make_unique<StationServer>(
            coreModel, settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    int resets(const QString& stream) const
    {
        int n = 0;
        for (const QByteArray& wire : windowEnd->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::RecordBatch
                && message.recordBatch.stream == stream && message.recordBatch.reset) {
                ++n;
            }
        }
        return n;
    }
    RecordStream* coreLog() const
    {
        return server->recordStreamForTest(QStringLiteral("coreLog"));
    }

    QTemporaryDir settingsDir;
    AppSettings settings;
    RadioModel* coreModel = nullptr;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
};

bool anyLineContains(const QStringList& lines, const QString& text)
{
    for (const QString& line : lines) {
        if (line.contains(text)) {
            return true;
        }
    }
    return false;
}

} // namespace

class TstRemoteCoreLog : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(m_securityDir.isValid());
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("remote-core-log")));
        m_categoriesBefore = LogManager::instance().enabledList();
    }

    void init() { QVERIFY(RemoteWindowHarness::clearIsolatedProfile()); }

    void cleanup()
    {
        LogManager::instance().setEnabledList(
            m_categoriesBefore.split(QLatin1Char(','), Qt::SkipEmptyParts));
    }

    void cleanupTestCase() { QVERIFY(RemoteWindowHarness::removeIsolatedProfile()); }

    void theCoreOffersItAfterRelayAllowed()
    {
        StationCapabilities caps;
        caps.radioIdentityEntries = true;
        caps.supportBundleVersion = 1;
        const QList<MirrorUpdate> updates = caps.toUpdates();
        // Later capabilities may follow this deployed contiguous block.
        const QList<QByteArray> originalBlock{
            "relayAllowed", "supportBundleVersion", "mediaTunnelVersion",
            "mediaRelayRoutingVersion"};
        qsizetype first = -1;
        for (qsizetype i = 0; i < updates.size(); ++i) {
            if (updates.at(i).name == originalBlock.first()) {
                QVERIFY(first < 0);
                first = i;
            }
        }
        QVERIFY(first >= 0);
        QVERIFY(first + originalBlock.size() <= updates.size());
        for (qsizetype i = 0; i < originalBlock.size(); ++i) {
            QCOMPARE(updates.at(first + i).name, originalBlock.at(i));
        }
        QCOMPARE(StationCapabilities::fromUpdates(updates).supportBundleVersion, 1);

        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        QCOMPARE(s.server->supportBundleVersion(), 1);
        QVERIFY(s.connect());
        QCOMPARE(s.client->capabilities().supportBundleVersion, 1);
        QVERIFY(s.client->supportBundleAvailable());
        QVERIFY(s.window.stationSupportUnavailableReason().isEmpty());
    }

    void aViewerFollowsTheCoresLog()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        coreLogs(QStringLiteral("[10:00:00.000] INF: before the window"));
        QVERIFY(s.connect());
        // Nobody follows it yet: nothing is sent.
        QVERIFY(s.coreLog() != nullptr);
        QCOMPARE(s.coreLog()->subscriberCount(), 0);

        QSignalSpy changed(&s.window, &RadioModel::stationCoreLogChanged);
        s.window.addStationCoreLogViewer();
        QTRY_VERIFY(anyLineContains(s.window.stationCoreLog(),
                                    QStringLiteral("before the window")));
        QVERIFY(s.window.stationCoreLog().size() <= RadioModel::kStationCoreLogLines);

        // A new line follows; a secret in it does not.
        const QString token = QStringLiteral("Qm9ndXNUb2tlbjAxMjM0NTY3ODlhYmNkZWZnaGlqa2xt");
        coreLogs(QStringLiteral("[10:00:01.000] INF: the next line %1").arg(token));
        QTRY_VERIFY(anyLineContains(s.window.stationCoreLog(), QStringLiteral("the next line")));
        QVERIFY(!anyLineContains(s.window.stationCoreLog(), token));

        // A private key can reach the sink in separate Qt messages. The
        // short body line must remain hidden until its END marker.
        coreLogs(QStringLiteral("[10:00:01.100] INF: -----BEGIN PRIVATE KEY-----"));
        coreLogs(QStringLiteral("shortPemBody"));
        coreLogs(QStringLiteral("-----END PRIVATE KEY-----"));
        QTRY_VERIFY(anyLineContains(s.window.stationCoreLog(),
                                    QStringLiteral("[REDACTED PRIVATE KEY]")));
        QVERIFY(!anyLineContains(s.window.stationCoreLog(), QStringLiteral("shortPemBody")));

        // A second viewer shares the subscription; the last one to go ends
        // it, and nothing more is sent.
        s.window.addStationCoreLogViewer();
        s.window.removeStationCoreLogViewer();
        QCOMPARE(s.coreLog()->subscriberCount(), 1);
        s.window.removeStationCoreLogViewer();
        QTRY_COMPARE(s.coreLog()->subscriberCount(), 0);
        coreLogs(QStringLiteral("[10:00:02.000] INF: nobody follows this"));
        QTest::qWait(600);
        QVERIFY(!anyLineContains(s.window.stationCoreLog(), QStringLiteral("nobody follows")));

        // Following again, and Refresh, read the backlog again (a reset).
        const int resetsBefore = s.resets(QStringLiteral("coreLog"));
        s.window.addStationCoreLogViewer();
        QTRY_VERIFY(anyLineContains(s.window.stationCoreLog(), QStringLiteral("nobody follows")));
        QTRY_COMPARE(s.resets(QStringLiteral("coreLog")), resetsBefore + 1);
        s.window.refreshStationCoreLog();
        QTRY_COMPARE(s.resets(QStringLiteral("coreLog")), resetsBefore + 2);
        QVERIFY(anyLineContains(s.window.stationCoreLog(), QStringLiteral("nobody follows")));
        QVERIFY(changed.count() > 0);
        s.window.removeStationCoreLogViewer();
    }

    void theWindowSeesAndChangesTheCoresCategories()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        LogManager::instance().setEnabledList({QStringLiteral("nereus.discovery")});
        QVERIFY(s.connect());
        QTRY_COMPARE(s.window.logCategories(), QStringLiteral("nereus.discovery"));

        // A change at the Core reaches the window live.
        LogManager::instance().setEnabled(QStringLiteral("nereus.tci"), true);
        QTRY_COMPARE(s.window.logCategories(), QStringLiteral("nereus.discovery,nereus.tci"));

        // The window's change turns the Core's on and off; an id the Core
        // does not keep is ignored.
        QVERIFY(s.client->requestLogCategories(
                    QStringLiteral("nereus.dsp,nereus.audio,nereus.notACategory")).sent);
        QTRY_COMPARE(LogManager::instance().enabledList(), QStringLiteral("nereus.audio,nereus.dsp"));
        QTRY_COMPARE(s.window.logCategories(), QStringLiteral("nereus.audio,nereus.dsp"));
        QVERIFY(s.client->requestLogCategories(QString()).sent);
        QTRY_VERIFY(LogManager::instance().enabledList().isEmpty());
        QTRY_VERIFY(s.window.logCategories().isEmpty());
    }

    void theWindowGetsTheCoresBundle()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        coreLogs(QStringLiteral("[10:00:00.000] INF: in the Core's log"));
        QSignalSpy finished(&s.window, &RadioModel::stationSupportBundleFinished);
        const IStationLink::CommandOutcome outcome = s.client->requestSupportBundle();
        QVERIFY(outcome.sent);
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
        QCOMPARE(finished.first().at(0).toUInt(), outcome.commandId);
        QVERIFY2(finished.first().at(1).toBool(), qPrintable(finished.first().at(2).toString()));
        const QByteArray zip = finished.first().at(3).toByteArray();
        QVERIFY(!zip.isEmpty());
        QVERIFY(zip.size() <= SupportBundle::kMaxCoreBundleBytes);
        const std::optional<QList<ZipEntry>> entries = ZipArchive::read(zip);
        QVERIFY(entries);
        QHash<QString, QByteArray> byName;
        for (const ZipEntry& entry : *entries) {
            byName.insert(entry.name, entry.data);
        }
        QVERIFY(byName.contains(QStringLiteral("system-info.json")));
        QVERIFY(byName.contains(QStringLiteral("telemetry.json")));
        QVERIFY(byName.value(QStringLiteral("radio-info.json")).contains("Bench HL2"));
    }

    void bothAreTakenOnTheAirAsAtTheCore()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        MoxController* const mox = core->moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        QString reason;
        QTRY_VERIFY(core->stationOnAirRefusal(&reason));
        QTRY_VERIFY(s.window.isCoreOnAir());

        QVERIFY(s.client->requestLogCategories(QStringLiteral("nereus.spots")).sent);
        QTRY_COMPARE(s.window.logCategories(), QStringLiteral("nereus.spots"));
        QSignalSpy finished(&s.window, &RadioModel::stationSupportBundleFinished);
        QVERIFY(s.client->requestSupportBundle().sent);
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
        QVERIFY2(finished.first().at(1).toBool(), qPrintable(finished.first().at(2).toString()));
        mox->setMox(false);
        QTRY_VERIFY(!s.window.isCoreOnAir());
    }

    void aWindowWithNoCoreSaysWhy()
    {
        RadioModel window{RadioModel::Role::Remote};
        const QString reason = window.stationSupportUnavailableReason();
        QVERIFY(!reason.isEmpty());
        QVERIFY(OperatorWording::isPlain(reason));
        QVERIFY(OperatorWording::isPlain(IStationLink::supportBundleUnavailableReason()));
        // An older Core sends no entry: the version reads 0.
        StationCapabilities older;
        older.radioIdentityEntries = true;
        QList<MirrorUpdate> updates = older.toUpdates();
        updates.removeIf([](const MirrorUpdate& u) { return u.name == "supportBundleVersion"; });
        QCOMPARE(StationCapabilities::fromUpdates(updates).supportBundleVersion, 0);
    }

    void theSupportDialogAndLogsPageShowTheCore()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        LogManager::instance().setEnabledList({QStringLiteral("nereus.connection")});
        QVERIFY(s.connect());
        coreLogs(QStringLiteral("[10:00:00.000] INF: shown in the window"));

        SupportDialog dialog(&s.window);
        dialog.show();
        QTRY_COMPARE(s.coreLog()->subscriberCount(), 1);
        auto* coreView = dialog.findChild<QPlainTextEdit*>(QStringLiteral("supportCoreLogViewer"));
        QVERIFY(coreView);
        QTRY_VERIFY(coreView->toPlainText().contains(QStringLiteral("shown in the window")));
        // The checkboxes show the Core's categories; ticking one turns it
        // on at the Core.
        QCheckBox* connection = nullptr;
        QCheckBox* tci = nullptr;
        for (QCheckBox* box : dialog.findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("Connection")) { connection = box; }
            if (box->text() == QStringLiteral("TCI")) { tci = box; }
        }
        QVERIFY(connection && tci);
        QTRY_VERIFY(connection->isChecked());
        QVERIFY(!tci->isChecked());
        QVERIFY(tci->isEnabled());
        tci->setChecked(true);
        QTRY_COMPARE(LogManager::instance().enabledList(),
                     QStringLiteral("nereus.connection,nereus.tci"));
        // A change at the Core moves the box.
        LogManager::instance().setEnabled(QStringLiteral("nereus.connection"), false);
        QTRY_VERIFY(!connection->isChecked());
        dialog.hide();
        QTRY_COMPARE(s.coreLog()->subscriberCount(), 0);

        // Setup > Diagnostics > Logs: the Core's log above this computer's.
        LogsPage page(&s.window);
        page.show();
        QTRY_COMPARE(s.coreLog()->subscriberCount(), 1);
        auto* coreLogView = page.findChild<QPlainTextEdit*>(QStringLiteral("coreLogsView"));
        QVERIFY(coreLogView);
        QVERIFY(page.findChild<QPlainTextEdit*>(QStringLiteral("logsView")));
        QTRY_VERIFY(coreLogView->toPlainText().contains(QStringLiteral("shown in the window")));
        page.hide();
        QTRY_COMPARE(s.coreLog()->subscriberCount(), 0);
    }

private:
    QTemporaryDir m_securityDir;
    QString m_categoriesBefore;
};

QTEST_MAIN(TstRemoteCoreLog)
#include "tst_remote_core_log.moc"
