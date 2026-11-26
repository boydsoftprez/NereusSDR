// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_new_slice_band_notice.cpp  (NereusSDR)
// =================================================================
//
// PR 816 final review: Setup > General > New Slice names a band no
// receiver can be given to, so a slice a device adds opens on the band
// it was copied from. The Core's own window says why in a toast
// (RadioModel::newSliceBandNotUsed). A device that pressed +RX is told the
// same over the link (notice newSliceBand, about its own request), and a
// remote desktop window shows it as the local window does.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), PR 816 final review, on Brian Sutton's (W4KZO)
//               New Slice page. AI-assisted via Claude Code / Opus 5.5.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QSignalSpy>

#include "core/session/RemoteDevicesState.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
#include "models/Band.h"

namespace {

// One receiver, in use by slice 0 on 20 m, and New Slice asks for 40 m.
void useOneReceiverAndAskFor40m(RadioModel& model)
{
    model.configureStreamPool(/*userDdcCount=*/1, /*maxSlices=*/5, 192000);
    model.sliceById(0)->setFrequency(14074000.0);
    AppSettings::instance().setValue(QStringLiteral("NewSlice/Band"),
                                     static_cast<int>(Band::Band40m));
}

int sliceIdOf(const QJsonObject& result)
{
    const QString key = result.value(QStringLiteral("affected")).toArray().first().toString();
    return key.startsWith(QStringLiteral("slice:")) ? key.mid(6).toInt() : -1;
}

} // namespace

class TstNewSliceBandNotice : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("new-slice-band-notice-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void init() { AppSettings::instance().clear(); }

    // The device that added the slice is told, in the Core window's words,
    // with the new slice; another device is not.
    void theDeviceThatAddedTheSliceIsTold()
    {
        Core core;
        core.model->setCapsForTest(true);
        Device a;
        Device b(QStringLiteral("Grant's iPad"), QStringLiteral("tablet"), QStringLiteral("Grant's iPad"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        useOneReceiverAndAskFor40m(*core.model);
        QSignalSpy notUsed(core.model.get(), &RadioModel::newSliceBandNotUsed);

        const QJsonObject result =
            core.invoke(appA, "addSlice", {utf8("initialPanId", QString())});
        QVERIFY2(result.value(QStringLiteral("accepted")).toBool(false),
                 qPrintable(result.value(QStringLiteral("reason")).toString()));
        const int id = sliceIdOf(result);
        QVERIFY(id >= 0);
        QCOMPARE(notUsed.count(), 1);

        QTRY_COMPARE(countOfType(appA->received(), 0, QStringLiteral("notice")), 1);
        const QJsonObject told = ofType(appA->received(), QStringLiteral("notice")).first();
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("newSliceBand"));
        QCOMPARE(told.value(QStringLiteral("reason")).toString(), notUsed.first().at(1).toString());
        QVERIFY(told.value(QStringLiteral("reason")).toString().contains(QStringLiteral("40m")));
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(true), false);
        QVERIFY(!told.contains(QStringLiteral("byName")));
        QCOMPARE(told.value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("sliceId")).toInt(),
                 id);
        QTest::qWait(3 * StationServer::kDefaultDeltaFlushMs);
        QCOMPARE(countOfType(appB->received(), 0, QStringLiteral("notice")), 0);
    }

    // The Core window's own +RX is its toast alone: no device is told.
    void theCoresOwnSliceTellsNoDevice()
    {
        Core core;
        core.model->setCapsForTest(true);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a);
        QVERIFY(admitted(appA));
        useOneReceiverAndAskFor40m(*core.model);
        QSignalSpy notUsed(core.model.get(), &RadioModel::newSliceBandNotUsed);
        QVERIFY(core.model->addSlice(QString(), false, /*newReceiver=*/true) >= 0);
        QCOMPARE(notUsed.count(), 1);
        QTest::qWait(3 * StationServer::kDefaultDeltaFlushMs);
        QCOMPARE(countOfType(appA->received(), 0, QStringLiteral("notice")), 0);
    }

    // A remote desktop window shows it as a local window does: its
    // RadioModel says newSliceBandNotUsed (MainWindow's toast), and no
    // card is added.
    void aRemoteWindowSaysItAsALocalOneDoes()
    {
        Core core(/*upgradedWithToken=*/true);
        core.model->setCapsForTest(true);
        RadioModel window{RadioModel::Role::Remote};
        SettingsProxy proxy;
        StationClient client(&window, &proxy);
        auto* coreEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
        auto* windowEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
        coreEnd->linkTo(windowEnd);
        // Signed in as a device-key window is: it declares sessionHolder,
        // and the Core numbers it token:1 (as tst_bandstack_link does).
        client.setTokenSessionHolderForTest(QStringLiteral("token:1"));
        client.setTokenSliceAccessForTest(true);
        client.declareFeatureForTest(QByteArrayLiteral("deviceAuth"), 1);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(windowEnd, core.server->token());
        core.server->acceptTransport(coreEnd);
        QVERIFY(completed.wait(5000) || completed.count() == 1);
        useOneReceiverAndAskFor40m(*core.model);

        QSignalSpy notUsed(&window, &RadioModel::newSliceBandNotUsed);
        QSignalSpy rejected(&window, &RadioModel::sliceAddRejected);
        QVERIFY(client.requestAddSlice(QString()).sent);
        QTRY_COMPARE(notUsed.count(), 1);
        QVERIFY(notUsed.first().at(0).toInt() >= 0);
        QVERIFY(notUsed.first().at(1).toString().contains(QStringLiteral("40m")));
        QCOMPARE(rejected.count(), 0);
        QVERIFY(client.remoteDevices()->notices().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstNewSliceBandNotice)
#include "tst_new_slice_band_notice.moc"
