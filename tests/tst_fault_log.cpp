// =================================================================
// tests/tst_fault_log.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent; FaultLog is a
// NereusSDR-native class per design doc §4.7.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: ringBufferKeepsNewestTen, likelyCauseSwrTrip,
//                 persistsAcrossInstances.
//   2026-08-06  J.J. Boyd (KG4VCF), Remote daemon R2 Task 15: FaultLog::
//                 reload() (FaultLog.h) plus
//                 reloadPicksUpValueDeliveredThroughRemoteBackend, proving
//                 a FaultLog constructed before a remote-mode snapshot
//                 lands finds nothing at construction and the real data
//                 after reload(). AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-09-24  J.J. Boyd (KG4VCF), R-R3-47 / R-R3-22: every record names
//                 its device and says what happened in plain words; records
//                 saved before carry them too; a remote window's copy is
//                 replaced from the Core's list without saving; the Core's
//                 history (here an RF-Kit fault it recorded) survives a Core
//                 restart. AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include "core/AppSettings.h"
#include "core/FaultLog.h"
#include "core/settings/SettingsProxy.h"
#include "core/Rf2ksConnection.h"
#include "core/TxInterlockPolicy.h"
#include "models/AccessoryDataModel.h"
#include "models/RadioModel.h"
#include "OperatorWording.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {
// RAII guard so a QVERIFY/QCOMPARE failure partway through
// reloadPicksUpValueDeliveredThroughRemoteBackend() (an early `return`,
// not a throw -- still runs local destructors normally) cannot leave
// AppSettings::instance() pointed at a since-destroyed local
// SettingsProxy for whatever test slot in this SAME PROCESS runs next.
// tests/CMakeLists.txt's own comment on TestSandboxInit.cpp documents
// that every test file in this suite shares one sandboxed
// AppSettings::instance() singleton per process, not per slot.
class ScopedRemoteBackend {
public:
    explicit ScopedRemoteBackend(NereusSDR::ISettingsBackend* backend)
    {
        NereusSDR::AppSettings::instance().setRemoteBackend(backend);
    }
    ~ScopedRemoteBackend()
    {
        NereusSDR::AppSettings::instance().setRemoteBackend(nullptr);
    }
    ScopedRemoteBackend(const ScopedRemoteBackend&) = delete;
    ScopedRemoteBackend& operator=(const ScopedRemoteBackend&) = delete;
};
} // namespace

// Follow-up 2: this binary's own settings file (a profile named after the
// process), so the restart tests that remove and read it cannot race any
// other test binary under ctest -j (they all share the qttest sandbox's
// default file).
static QString privateProfile()
{
    return QStringLiteral("tst-fault-log-%1").arg(QCoreApplication::applicationPid());
}

class FaultLogTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        NereusSDR::AppSettings::setProfileOverride(privateProfile());
        QVERIFY(NereusSDR::AppSettings::instance().filePath().contains(privateProfile()));
    }
    void cleanupTestCase()
    {
        const QString path = NereusSDR::AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
        QDir().rmdir(QFileInfo(path).absolutePath());
    }
    void ringBufferKeepsNewestTen();
    void likelyCauseSwrTrip();
    void persistsAcrossInstances();
    void reloadPicksUpValueDeliveredThroughRemoteBackend();
    // R-R3-47 / R-R3-22
    void recordsNameTheirDeviceAndSayWhatHappened();
    void recordsSavedBeforeCarryDeviceAndText();
    void mirroredListReplacesWithoutSaving();
    void historySurvivesACoreRestart();
    void interlockAndPowerCapSurviveACoreRestart();
};

// Capture 12 events and verify only the 10 newest are retained, newest first.
void FaultLogTest::ringBufferKeepsNewestTen()
{
    NereusSDR::FaultLog log("PGXL_FaultHistory");
    log.clear();
    for (int i = 0; i < 12; ++i) {
        NereusSDR::FaultEvent ev;
        ev.whenMs       = static_cast<qint64>(i) * 1000;
        ev.state        = "FAULT";
        ev.fwdAtFaultW  = 1500.0f;
        ev.swrAtFault   = 1.5f;
        ev.tempAtFaultC = 70.0f;
        ev.likelyCause  = "X";
        log.capture(ev);
    }
    QCOMPARE(log.events().size(), 10);
    // Events are prepended so the last captured (i=11, whenMs=11000) is first.
    QCOMPARE(log.events().first().whenMs, qint64(11000));
    // The oldest surviving event should be i=2 (whenMs=2000); i=0 and i=1 were evicted.
    QCOMPARE(log.events().last().whenMs, qint64(2000));
}

// Verify the SWR-trip heuristic fires when SWR exceeds 2.5.
void FaultLogTest::likelyCauseSwrTrip()
{
    const QString cause = NereusSDR::FaultLog::likelyCauseFor(1500.0f, 3.0f, 60.0f);
    QCOMPARE(cause, QString("SWR trip"));
}

// Store events via instance A, construct instance B with the same key, and
// verify the events round-trip through AppSettings JSON correctly.
void FaultLogTest::persistsAcrossInstances()
{
    const QString key = "PGXL_FaultHistory_PersistTest";

    // Write via instance A.
    {
        NereusSDR::FaultLog a(key);
        a.clear();
        NereusSDR::FaultEvent ev;
        ev.whenMs       = qint64(99000);
        ev.state        = "FAULT_PROTECT";
        ev.fwdAtFaultW  = 1800.0f;
        ev.swrAtFault   = 2.9f;
        ev.tempAtFaultC = 72.0f;
        ev.likelyCause  = NereusSDR::FaultLog::likelyCauseFor(
                              ev.fwdAtFaultW, ev.swrAtFault, ev.tempAtFaultC);
        a.capture(ev);
        QCOMPARE(a.events().size(), 1);
    }

    // Read via instance B with the same key -- should see the persisted event.
    {
        NereusSDR::FaultLog b(key);
        const QVector<NereusSDR::FaultEvent> evs = b.events();
        QCOMPARE(evs.size(), 1);
        const NereusSDR::FaultEvent& ev = evs.first();
        QCOMPARE(ev.whenMs,       qint64(99000));
        QCOMPARE(ev.state,        QString("FAULT_PROTECT"));
        QCOMPARE(ev.likelyCause,  QString("SWR trip"));
        QVERIFY(qFuzzyCompare(ev.fwdAtFaultW, 1800.0f));

        // Clean up the test key so repeated runs start clean.
        b.clear();
    }
}

// Remote Daemon R2, Task 15: proves the FaultLog.h "STATED ORDERING"
// comment's claim is actually true, not just asserted. A FaultLog
// constructed BEFORE a remote-mode GUI's connect-time snapshot lands
// finds nothing (mirrors RadioModel always constructing before Task 18's
// handshake can possibly complete); reload(), called after the snapshot
// arrives, picks up the real data because it re-reads through the SAME
// AppSettings::instance().value() the constructor used, which by then
// resolves through the installed SettingsProxy instead of the (empty)
// local store.
void FaultLogTest::reloadPicksUpValueDeliveredThroughRemoteBackend()
{
    // "PGXL_" is a Task 14 (SettingsScope.cpp) Station prefix, so this
    // key delegates to a remote backend once one is installed --
    // exactly like the real "PGXL_FaultHistory"/"TGXL_FaultHistory" keys
    // FaultLog.h documents.
    const QString key = QStringLiteral("PGXL_FaultHistory_RemoteBackendTest");

    NereusSDR::SettingsProxy proxy;
    ScopedRemoteBackend guard(&proxy); // see its class comment for why this is RAII, not a bare call

    // Constructed before any snapshot -- finds nothing, exactly like a
    // real RadioModel-owned FaultLog at GUI launch, before Task 18's
    // handshake completes.
    NereusSDR::FaultLog log(key);
    QVERIFY(log.events().isEmpty());

    // One JSON-encoded FaultEvent, matching FaultLog::save()'s own wire
    // shape -- as it would arrive via SettingsProxyServer::buildSnapshot()
    // on the daemon side once Task 18 relays it.
    const QString json = QStringLiteral(
        "[{\"whenMs\":99000,\"state\":\"FAULT_PROTECT\",\"fwdAtFaultW\":1800,"
        "\"swrAtFault\":2.9,\"tempAtFaultC\":72,\"likelyCause\":\"SWR trip\"}]");
    proxy.applySnapshot(QMap<QString, QString>{{key, json}});

    // Still nothing -- the constructor already ran; nothing has told
    // THIS instance to look again yet. This is the exact gap FaultLog.h
    // describes: the snapshot landed, but no re-read happened.
    QVERIFY(log.events().isEmpty());

    log.reload();

    QCOMPARE(log.events().size(), 1);
    QCOMPARE(log.events().first().whenMs, qint64(99000));
    QCOMPARE(log.events().first().state, QStringLiteral("FAULT_PROTECT"));
    QCOMPARE(log.events().first().likelyCause, QStringLiteral("SWR trip"));
    QVERIFY(qFuzzyCompare(log.events().first().fwdAtFaultW, 1800.0f));
}

// R-R3-47: a record names its device and says what happened in plain words;
// a notice (a Tuner Genius or RF-Kit problem) keeps what the device said.
void FaultLogTest::recordsNameTheirDeviceAndSayWhatHappened()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::FaultLog pgxl(QStringLiteral("PGXL_FaultHistory"));
    NereusSDR::FaultEvent ev;
    ev.whenMs = 1790000000000;
    ev.state = QStringLiteral("FAULT");
    ev.fwdAtFaultW = 1000.0f;
    ev.swrAtFault = 3.0f;
    ev.tempAtFaultC = 60.0f;
    ev.likelyCause = NereusSDR::FaultLog::likelyCauseFor(1000.0f, 3.0f, 60.0f);
    pgxl.capture(ev);
    const NereusSDR::FaultEvent got = pgxl.events().first();
    QCOMPARE(got.device, QStringLiteral("pgxl"));
    QCOMPARE(got.text, QStringLiteral("The Power Genius reported a fault. Likely cause: high SWR."));
    QVERIFY(NereusSDR::OperatorWording::isPlain(got.text));

    NereusSDR::FaultLog tgxl(QStringLiteral("TGXL_FaultHistory"));
    NereusSDR::FaultLog rfkit(QStringLiteral("RfKit_FaultHistory"));
    tgxl.captureNotice(QStringLiteral("link"),
                       QStringLiteral("The Tuner Genius stopped answering."), QString());
    rfkit.captureNotice(QStringLiteral("interface"),
                        QStringLiteral("The RF-Kit amplifier reported a problem."),
                        QStringLiteral("CAT timeout"));
    QCOMPARE(tgxl.events().first().device, QStringLiteral("tgxl"));
    QCOMPARE(rfkit.events().first().device, QStringLiteral("rfkit"));
    QCOMPARE(rfkit.events().first().detail, QStringLiteral("CAT timeout"));
    QVERIFY(rfkit.events().first().whenMs > 0);
    for (const QString& text : { tgxl.events().first().text, rfkit.events().first().text,
                                 NereusSDR::FaultLog::plainTextFor(QStringLiteral("pgxl"),
                                     QStringLiteral("FAULT"), QStringLiteral("Overtemp")),
                                 NereusSDR::FaultLog::plainTextFor(QStringLiteral("pgxl"),
                                     QStringLiteral("FAULT"), QStringLiteral("Drive too high")),
                                 NereusSDR::FaultLog::plainTextFor(QStringLiteral("pgxl"),
                                     QStringLiteral("FAULT"), QStringLiteral("Unknown")) }) {
        QVERIFY2(NereusSDR::OperatorWording::isPlain(text), qPrintable(text));
    }

    // The JSON the settings key and the Core's object carry has all of it.
    const QJsonObject saved = QJsonDocument::fromJson(
        NereusSDR::AppSettings::instance().value(QStringLiteral("RfKit_FaultHistory"))
            .toString().toUtf8()).array().first().toObject();
    QCOMPARE(saved.value(QStringLiteral("device")).toString(), QStringLiteral("rfkit"));
    QCOMPARE(saved.value(QStringLiteral("state")).toString(), QStringLiteral("interface"));
    QCOMPARE(saved.value(QStringLiteral("detail")).toString(), QStringLiteral("CAT timeout"));
    QVERIFY(!saved.value(QStringLiteral("text")).toString().isEmpty());
    NereusSDR::AppSettings::instance().clear();
}

// R-R3-47: a history saved before device and text existed reads with both.
void FaultLogTest::recordsSavedBeforeCarryDeviceAndText()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::AppSettings::instance().setValue(QStringLiteral("PGXL_FaultHistory"),
        QStringLiteral("[{\"whenMs\":99000,\"state\":\"FAULT\",\"fwdAtFaultW\":1800,"
                       "\"swrAtFault\":1.2,\"tempAtFaultC\":90,\"likelyCause\":\"Overtemp\"}]"));
    NereusSDR::FaultLog log(QStringLiteral("PGXL_FaultHistory"));
    QCOMPARE(log.events().size(), 1);
    QCOMPARE(log.events().first().device, QStringLiteral("pgxl"));
    QCOMPARE(log.events().first().text,
             QStringLiteral("The Power Genius reported a fault. Likely cause: the amplifier was "
                            "too hot."));
    NereusSDR::AppSettings::instance().clear();
}

// R-R3-47: a remote window's copy takes the Core's list as it is and saves
// nothing (the Core keeps the history).
void FaultLogTest::mirroredListReplacesWithoutSaving()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::FaultLog log(QStringLiteral("TGXL_FaultHistory"));
    QSignalSpy changed(&log, &NereusSDR::FaultLog::changed);
    log.applyMirroredJson(QStringLiteral(
        "[{\"whenMs\":5,\"device\":\"tgxl\",\"state\":\"link\","
        "\"text\":\"The Tuner Genius stopped answering.\",\"detail\":\"\"}]"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(log.events().size(), 1);
    QCOMPARE(log.events().first().text, QStringLiteral("The Tuner Genius stopped answering."));
    QVERIFY(!NereusSDR::AppSettings::instance().contains(QStringLiteral("TGXL_FaultHistory")));
    log.applyMirroredJson(QStringLiteral("[]"));
    QVERIFY(log.events().isEmpty());
    QCOMPARE(changed.count(), 2);
}

namespace {

// What the Core's settings file on disk holds for `key` right now (never
// written by the test: only the Core's own save path writes it).
QString onDisk(const QString& key)
{
    NereusSDR::AppSettings disk(NereusSDR::AppSettings::instance().filePath());
    disk.load();
    return disk.value(key).toString();
}

// The Core stops without saving (a power loss): its settings in memory are
// gone, and the next Core reads the file.
void loseMemoryAndReload()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::AppSettings::instance().load();
}

} // namespace

// R-R3-47 / R-R3-22 (I3): a fault the Core records reaches its settings
// file through the Core's own save path, shortly after it happens, so a
// Core that loses power and starts again has it, and says so on its
// `accessoryData` object. Clearing the history reaches the file the same
// way. The test never writes the file itself.
void FaultLogTest::historySurvivesACoreRestart()
{
    const QString file = NereusSDR::AppSettings::instance().filePath();
    QVERIFY(!file.isEmpty());
    QVERIFY(file.contains(privateProfile()));   // never the shared sandbox file
    QFile::remove(file);
    NereusSDR::AppSettings::instance().clear();
    {
        NereusSDR::RadioModel core;
        // The RF-Kit reports an interface error (the amp's own words).
        core.rfKitConnection()->injectJsonForTesting(
            QStringLiteral("/operational-interface"),
            R"({"operational_interface":"UDP","error":"CAT timeout"})");
        QCOMPARE(core.rfkitFaultLog()->events().size(), 1);
        QVERIFY(core.accessoryDataModel()->rfkitFaults().contains(QStringLiteral("CAT timeout")));
        QTRY_VERIFY_WITH_TIMEOUT(
            onDisk(QStringLiteral("RfKit_FaultHistory")).contains(QStringLiteral("CAT timeout")),
            3000);
        // No clean stop: the model goes without a save of its own.
    }
    loseMemoryAndReload();

    {
        NereusSDR::RadioModel restarted;
        QCOMPARE(restarted.rfkitFaultLog()->events().size(), 1);
        const NereusSDR::FaultEvent ev = restarted.rfkitFaultLog()->events().first();
        QCOMPARE(ev.device, QStringLiteral("rfkit"));
        QCOMPARE(ev.detail, QStringLiteral("CAT timeout"));
        QVERIFY(NereusSDR::OperatorWording::isPlain(ev.text));
        QVERIFY(restarted.accessoryDataModel()->rfkitFaults().contains(
            QStringLiteral("CAT timeout")));

        // A window clears the history: the file follows.
        QString reason;
        QVERIFY(restarted.clearAccessoryFaultsForStation(QStringLiteral("rfkit"), &reason));
        QTRY_VERIFY_WITH_TIMEOUT(
            !onDisk(QStringLiteral("RfKit_FaultHistory")).contains(QStringLiteral("CAT timeout")),
            3000);
    }
    loseMemoryAndReload();
    {
        NereusSDR::RadioModel again;
        QVERIFY(again.rfkitFaultLog()->events().isEmpty());
    }
    NereusSDR::AppSettings::instance().clear();
    QFile::remove(file);
}

// I3: the interlock policy and the output limit a window sets reach the
// Core's settings file through the Core's own save path.
void FaultLogTest::interlockAndPowerCapSurviveACoreRestart()
{
    const QString file = NereusSDR::AppSettings::instance().filePath();
    QVERIFY(file.contains(privateProfile()));   // never the shared sandbox file
    QFile::remove(file);
    NereusSDR::AppSettings::instance().clear();
    {
        NereusSDR::RadioModel core;
        QString reason;
        QVERIFY(core.setTxInterlockPolicyForStation(2, 1500, true, 2.5, &reason));
        QVERIFY(core.setPgxlPowerCapForStation(true, 800, &reason));
        QTRY_VERIFY_WITH_TIMEOUT(onDisk(QStringLiteral("PGXL_PowerCapW")) == QStringLiteral("800"),
                                 3000);
    }
    loseMemoryAndReload();
    {
        NereusSDR::RadioModel restarted;
        QCOMPARE(restarted.txInterlockPolicy()->mode(), NereusSDR::TxInterlockPolicy::Block);
        QCOMPARE(restarted.txInterlockPolicy()->graceMs(), 1500);
        QVERIFY(restarted.txInterlockPolicy()->swrGateEnabled());
        QCOMPARE(NereusSDR::AppSettings::instance().value(
                     QStringLiteral("PGXL_PowerCapEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(NereusSDR::AppSettings::instance().value(
                     QStringLiteral("PGXL_PowerCapW")).toString(),
                 QStringLiteral("800"));
    }
    NereusSDR::AppSettings::instance().clear();
    QFile::remove(file);
}

QTEST_GUILESS_MAIN(FaultLogTest)
#include "tst_fault_log.moc"
