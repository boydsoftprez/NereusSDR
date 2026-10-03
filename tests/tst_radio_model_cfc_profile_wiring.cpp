// no-port-check: NereusSDR-original production routing regression tests.
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QSemaphore>
#include <QThread>
#include <QScopeGuard>
#include <memory>
#include "core/AppSettings.h"
#include "core/CfcProfile.h"
#include "core/CfcEditProfile.h"
#include "core/MicProfileManager.h"
#include "core/TxChannel.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#ifdef HAVE_WDSP
extern "C" {
void OpenChannel(int, int, int, int, int, int, int, int, double, double, double, double, int);
void CloseChannel(int);
}
#endif
using namespace NereusSDR;
static CfcEditProfile configuredProfile(int count = 18)
{
    CfcEditProfile p;
    p.compression.frequencyMaxHz = p.postEq.frequencyMaxHz = 10000;
    p.compression.globalGainDb = 3.5;
    p.postEq.globalGainDb = -2.5;
    p.compression.useQ = p.postEq.useQ = true;
    for (int i = 0; i < count; ++i) {
        p.compression.frequenciesHz.append(i == 1 ? 125.125 : i * 10000.0 / (count - 1));
        p.compression.gainsDb.append(3.5);
        p.compression.q.append(1.2345);
        p.postEq.gainsDb.append(-4.5);
        p.postEq.q.append(2.3456);
    }
    p.postEq.frequenciesHz = p.compression.frequenciesHz;
    return p;
}
class TstRadioModelCfcProfileWiring : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
#ifdef HAVE_WDSP
        // Open only a local WDSP channel; no radio transport or audio hardware.
        OpenChannel(1, 64, 1024, 48000, 96000, 48000, 1, 0, 0, 0.010, 0, 0.010, 1);
#endif
    }
    void cleanupTestCase()
    {
#ifdef HAVE_WDSP
        CloseChannel(1);
#endif
    }
    void init() { AppSettings::instance().clear(); }
    void directFallbackBandEditsReachChannel_data()
    {
        QTest::addColumn<QString>("blob");
        QTest::addColumn<int>("field");
        for (const QString& blob : {QString(), QStringLiteral("opaque-future-profile")}) {
            for (int field = 0; field < 3; ++field) {
                QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(blob.isEmpty() ? "empty" : "opaque").arg(field))) << blob << field;
            }
        }
    }
    void directFallbackBandEditsReachChannel()
    {
        QFETCH(QString, blob); QFETCH(int, field);
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        tx.setCfcParaEqData(blob);
        TxChannel channel(1, 64, 64);
        radio.bindCfcProfileChannelForTest(&channel);
        QSignalSpy aggregate(&tx, &TransmitModel::cfcEditProfileChanged);
        const quint64 before = channel.cfcProfileApplyCountForTest();
        const auto change = [&] {
            if (field == 0) { tx.setCfcEqFreq(1, 130); }
            else if (field == 1) { tx.setCfcCompression(1, 7); }
            else { tx.setCfcPostEqBandGain(1, -6); }
        };
        change();
        QCOMPARE(aggregate.count(), 1);
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before + 1);
        QCOMPARE(channel.lastCfcProfileForTest()[field][1], field == 0 ? 130.0 : field == 1 ? 7.0 : -6.0);
        QCOMPARE(tx.cfcParaEqData(), blob);
        change();
        QCOMPARE(aggregate.count(), 1);
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before + 1);
    }
    void fallbackArraysAndNestedRestoresPublishOnce()
    {
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        TxChannel channel(1, 64, 64);
        radio.bindCfcProfileChannelForTest(&channel);
        QSignalSpy aggregate(&tx, &TransmitModel::cfcEditProfileChanged);
        quint64 before = channel.cfcProfileApplyCountForTest();
        tx.setCfcCompressionJson(QStringLiteral("[7,7,7,7,7,7,7,7,7,7]"));
        QCOMPARE(aggregate.count(), 1);
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before + 1);
        QCOMPARE(channel.lastCfcProfileForTest()[1], std::vector<double>(10, 7));
        tx.setCfcCompressionJson(QStringLiteral("[7,7,7,7,7,7,7,7,7,7]"));
        QCOMPARE(aggregate.count(), 1);
        before = channel.cfcProfileApplyCountForTest();
        tx.beginCfcProfileRestore(); tx.beginCfcProfileRestore();
        tx.setCfcEqFreq(1, 130); tx.setCfcCompression(1, 9); tx.setCfcPostEqBandGain(1, -6);
        tx.endCfcProfileRestore();
        QCOMPARE(aggregate.count(), 1);
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before);
        tx.endCfcProfileRestore();
        QCOMPARE(aggregate.count(), 2);
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before + 1);
        QCOMPARE(channel.lastCfcProfileForTest()[0][1], 130.0);
        QCOMPARE(channel.lastCfcProfileForTest()[1][1], 9.0);
        QCOMPARE(channel.lastCfcProfileForTest()[2][1], -6.0);
    }
    void destroyedProfileReceiverDoesNotLeaveCallbacks_data()
    {
        QTest::addColumn<bool>("cfc"); QTest::addColumn<bool>("queued");
        QTest::newRow("eq-direct") << false << false;
        QTest::newRow("cfc-direct") << true << false;
        QTest::newRow("eq-queued") << false << true;
        QTest::newRow("cfc-queued") << true << true;
    }
    void destroyedProfileReceiverDoesNotLeaveCallbacks()
    {
        QFETCH(bool, cfc); QFETCH(bool, queued);
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        auto owner = std::make_unique<TxChannel>(1, 64, 64);
        QPointer<TxChannel> channel(owner.get());
        QThread worker;
        QSemaphore entered, release;
        const auto cleanup = qScopeGuard([&] { release.release(); worker.quit(); worker.wait(); });
        if (queued) { channel->moveToThread(&worker); worker.start(); }
        if (cfc) { radio.bindCfcProfileChannelForTest(channel); }
        else { radio.bindTxEqProfileChannelForTest(channel); }
        if (queued) {
            QMetaObject::invokeMethod(channel, [&] { entered.release(); release.acquire(); }, Qt::QueuedConnection);
            QVERIFY(entered.tryAcquire(1, 5000));
            // Delete is queued before immutable profile work: QObject must cancel
            // that work when its receiver disappears, without touching a successor.
            TxChannel* transferred = owner.release();
            QMetaObject::invokeMethod(channel, [transferred] { std::unique_ptr<TxChannel> destroy(transferred); }, Qt::QueuedConnection);
            if (cfc) { tx.setCfcProfile(configuredProfile(5)); }
            else { tx.setTxEqBand(1, 7); }
            release.release();
            QTRY_VERIFY(channel.isNull());
        } else { owner.reset(); }
        QVERIFY(channel.isNull());
        // Leave destruction bookkeeping queued while edits hit the model.
        if (cfc) { tx.setCfcProfile(configuredProfile(18)); tx.setCfcEnabled(true); tx.setCfcPostEqEnabled(true); }
        else { tx.setTxEqBand(1, 9); tx.setTxEqEnabled(true); }
        TxChannel replacement(1, 64, 64);
        if (cfc) {
            radio.bindCfcProfileChannelForTest(&replacement);
            const quint64 before = replacement.cfcProfileApplyCountForTest();
            tx.setCfcProfile(configuredProfile(5));
            QCOMPARE(replacement.cfcProfileApplyCountForTest(), before + 1);
            QCOMPARE(replacement.lastCfcProfileForTest()[0].size(), std::size_t(5));
        } else {
            radio.bindTxEqProfileChannelForTest(&replacement);
            const quint64 before = replacement.eqProfileApplyCountForTest();
            tx.setTxEqBand(1, 8);
            QCOMPARE(replacement.eqProfileApplyCountForTest(), before + 1);
            QCOMPARE(replacement.lastEqProfileForTest()[1][2], 8.0);
        }
        QCoreApplication::processEvents();
        if (cfc) { tx.setCfcPrecompDb(8); QCOMPARE(replacement.lastCfcPrecompDbForTest(), 8.0); }
        else { tx.setTxEqBand(1, 6); QCOMPARE(replacement.lastEqProfileForTest()[1][2], 6.0); }
    }
    void receiverDestructionAndExplicitRetirementCancelEqTimer()
    {
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        auto channel = std::make_unique<TxChannel>(1, 64, 64);
        radio.bindTxEqProfileChannelForTest(channel.get());
        tx.setTxEqUseLegacy(false); // leaves an expensive parametric push pending
        channel.reset();
        TxChannel replacement(1, 64, 64);
        radio.bindTxEqProfileChannelForTest(&replacement);
        const quint64 rebound = replacement.eqProfileApplyCountForTest();
        QTest::qWait(150);
        QCOMPARE(replacement.eqProfileApplyCountForTest(), rebound);
        tx.setTxEqEnabled(true); // another pending tick
        radio.bindTxEqProfileChannelForTest(nullptr); // production teardown boundary
        QTest::qWait(150);
        QCOMPARE(replacement.eqProfileApplyCountForTest(), rebound);
        radio.bindTxEqProfileChannelForTest(&replacement);
        QCOMPARE(replacement.eqProfileApplyCountForTest(), rebound + 1);
    }
    void pairedBlobProjectionPublishesOneCompleteProfile()
    {
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        TxChannel channel(1, 64, 64);
        radio.bindCfcProfileChannelForTest(&channel);
        QSignalSpy aggregate(&tx, &TransmitModel::cfcEditProfileChanged);
        const quint64 before = channel.cfcProfileApplyCountForTest();
        CfcProfile::Profile paired;
        QVERIFY(CfcProfile::decode(encodeCfcEditProfile(configuredProfile(10)), paired));
        paired.f[1] = paired.postF[1] = 130;
        paired.g[1] = 7; paired.e[1] = -6;
        const QString blob = CfcProfile::encode(paired);
        tx.setCfcParaEqData(blob);
        QCOMPARE(aggregate.count(), 1);
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before + 1);
        QCOMPARE(channel.lastCfcProfileForTest()[0][1], 130.0);
        QCOMPARE(channel.lastCfcProfileForTest()[1][1], 7.0);
        QCOMPARE(channel.lastCfcProfileForTest()[2][1], -6.0);
        QCOMPARE(tx.cfcEqFreq(1), 130);
        QCOMPARE(tx.cfcCompression(1), 7);
        QCOMPARE(tx.cfcPostEqBandGain(1), -6);
        tx.setCfcParaEqData(blob);
        QCOMPARE(aggregate.count(), 1);
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before + 1);
    }
    void configuredBandsAndQReachChannel_data()
    {
        QTest::addColumn<int>("count");
        for (int count : {5, 10, 18}) { QTest::newRow(qPrintable(QString::number(count))) << count; }
    }
    void configuredBandsAndQReachChannel()
    {
        QFETCH(int, count);
        RadioModel radio;
        TxChannel channel(1, 64, 64);
        radio.bindCfcProfileChannelForTest(&channel);
        const CfcEditProfile p = configuredProfile(count);
        QVERIFY(radio.transmitModel().setCfcProfile(p));
        const auto values = channel.lastCfcProfileForTest();
        QCOMPARE(values[0].size(), std::size_t(count));
        QCOMPARE(values[0][1], 125.125);
        QCOMPARE(values[1][1], 3.5);
        QCOMPARE(values[2][1], -4.5);
        QCOMPARE(values[3][1], 1.2345);
        QCOMPARE(values[4][1], 2.3456);
        QCOMPARE(channel.lastCfcPrecompDbForTest(), 3.5);
        QCOMPARE(channel.lastCfcPostEqGainDbForTest(), -2.5);
    }
    void useQRequiresBothGraphs()
    {
        RadioModel radio;
        TxChannel channel(1, 64, 64);
        radio.bindCfcProfileChannelForTest(&channel);
        for (int flags = 0; flags < 4; ++flags) {
            CfcEditProfile p = configuredProfile(5);
            p.compression.useQ = (flags & 1) != 0;
            p.postEq.useQ = (flags & 2) != 0;
            QVERIFY(radio.transmitModel().setCfcProfile(p));
            const auto values = channel.lastCfcProfileForTest();
            QCOMPARE(values[3].empty(), flags != 3);
            QCOMPARE(values[4].empty(), flags != 3);
        }
    }
    void emptyBlobUsesLegacyTenBands()
    {
        RadioModel radio;
        TxChannel channel(1, 64, 64);
        radio.transmitModel().setCfcPrecompDb(6);
        radio.transmitModel().setCfcPostEqGainDb(-7);
        radio.bindCfcProfileChannelForTest(&channel);
        const auto values = channel.lastCfcProfileForTest();
        QCOMPARE(values[0], (std::vector<double>{0,125,250,500,1000,2000,3000,4000,5000,10000}));
        QCOMPARE(values[1], std::vector<double>(10, 5));
        QCOMPARE(values[2], std::vector<double>(10, 0));
        QVERIFY(values[3].empty()); QVERIFY(values[4].empty());
        QCOMPARE(radio.transmitModel().effectiveCfcProfile().compression.frequencyMaxHz, 10000.0);
        QCOMPARE(channel.lastCfcPrecompDbForTest(), 6.0);
        QCOMPARE(channel.lastCfcPostEqGainDbForTest(), -7.0);
    }
    void typedEditPublishesOneCompleteProfile()
    {
        RadioModel radio;
        TxChannel channel(1, 64, 64);
        radio.bindCfcProfileChannelForTest(&channel);
        QSignalSpy spy(&radio.transmitModel(), &TransmitModel::cfcEditProfileChanged);
        const quint64 before = channel.cfcProfileApplyCountForTest();
        const CfcEditProfile p = configuredProfile(10);
        QVERIFY(radio.transmitModel().setCfcProfile(p));
        QCOMPARE(spy.count(), 1);
        QVERIFY(qvariant_cast<CfcEditProfile>(spy[0][0]) == p);
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before + 1);
        QCOMPARE(radio.transmitModel().cfcEqFreq(1), 125);
        radio.transmitModel().setCfcProfile(p);
        QCOMPARE(spy.count(), 1);
        radio.transmitModel().setCfcCompression(1, 7);
        QCOMPARE(channel.lastCfcProfileForTest()[1][1], 7.0);
        const auto saved = decodeCfcEditProfile(radio.transmitModel().cfcParaEqData());
        QVERIFY(saved);
        QCOMPARE(saved->compression.gainsDb[1], 7.0);
    }
    void legacyEditCannotReplaceVariableProfile()
    {
        RadioModel radio;
        TxChannel channel(1, 64, 64);
        radio.bindCfcProfileChannelForTest(&channel);
        const CfcEditProfile original = configuredProfile(18);
        QVERIFY(radio.transmitModel().setCfcProfile(original));
        const QString saved = radio.transmitModel().cfcParaEqData();
        const quint64 count = channel.cfcProfileApplyCountForTest();
        radio.transmitModel().setCfcCompression(3, 9);
        QCOMPARE(channel.lastCfcProfileForTest()[0].size(), std::size_t(18));
        QCOMPARE(channel.cfcProfileApplyCountForTest(), count);
        QCOMPARE(radio.transmitModel().cfcParaEqData(), saved);
        QCOMPARE(radio.transmitModel().effectiveCfcProfile(), original);
    }
    void fullPrecisionReachesChannelBeforeSave()
    {
        RadioModel radio;
        TxChannel channel(1, 64, 64);
        radio.bindCfcProfileChannelForTest(&channel);
        radio.transmitModel().setCfcProfile(configuredProfile(5));
        QCOMPARE(channel.lastCfcProfileForTest()[3][1], 1.2345);
        const QString saved = radio.transmitModel().cfcParaEqData();
        QCOMPARE(decodeCfcEditProfile(saved)->compression.q[1], 1.23);
        radio.transmitModel().setCfcParaEqData(saved);
        QCOMPARE(channel.lastCfcProfileForTest()[3][1], 1.23);
    }
    void legacyScalarEditsKeepTypedBlobCoherent()
    {
        RadioModel radio;
        TxChannel channel(1, 64, 64);
        radio.bindCfcProfileChannelForTest(&channel);
        TransmitModel& tx = radio.transmitModel();
        QVERIFY(tx.setCfcProfile(configuredProfile(18)));
        QSignalSpy precomp(&tx, &TransmitModel::cfcPrecompDbChanged);
        QSignalSpy aggregate(&tx, &TransmitModel::cfcEditProfileChanged);
        tx.setCfcPrecompDb(7);
        tx.setCfcPostEqGainDb(-8);
        QCOMPARE(precomp.count(), 1);
        QCOMPARE(aggregate.count(), 2);
        QCOMPARE(channel.lastCfcPrecompDbForTest(), 7.0);
        QCOMPARE(channel.lastCfcPostEqGainDbForTest(), -8.0);
        QCOMPARE(channel.lastCfcProfileForTest()[0].size(), std::size_t(18));
        const auto saved = decodeCfcEditProfile(tx.cfcParaEqData());
        QVERIFY(saved);
        QCOMPARE(saved->compression.globalGainDb, 7.0);
        QCOMPARE(saved->postEq.globalGainDb, -8.0);
        tx.setCfcPrecompDb(7);
        QCOMPARE(precomp.count(), 1);
        QCOMPARE(aggregate.count(), 2);
    }

    void settingsReloadRestoresSameBlobSavedValues()
    {
        const QString mac = "aa:bb:cc:11:22:33";
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        TxChannel channel(1, 64, 64);
        tx.loadFromSettings(mac);
        radio.bindCfcProfileChannelForTest(&channel);
        tx.setCfcProfile(configuredProfile(5));
        const QString saved = tx.cfcParaEqData();
        tx.persistToSettings(mac);
        CfcEditProfile edited = configuredProfile(5);
        edited.compression.q[1] = 1.2349;
        tx.setCfcProfile(edited);
        QCOMPARE(tx.cfcParaEqData(), saved);
        QSignalSpy aggregate(&tx, &TransmitModel::cfcEditProfileChanged);
        const quint64 before = channel.cfcProfileApplyCountForTest();
        tx.loadFromSettings(mac);
        QCOMPARE(aggregate.count(), 1);
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before + 1);
        QCOMPARE(channel.lastCfcProfileForTest()[3][1], 1.23);
    }

    void rejectedChannelCallsDoNotUpdateAcceptedSnapshot()
    {
        TxChannel channel(1, 64, 64);
        const std::vector<double> f{0,125,250,1000,4000};
        const std::vector<double> g(5, 5), e(5, 0), q(5, 4);
        channel.setTxCfcProfile(f, g, e, q, q);
        const auto accepted = channel.lastCfcProfileForTest();
        const quint64 count = channel.cfcProfileApplyCountForTest();
        channel.setTxCfcProfile(f, {1}, e, q, q);
        channel.setTxCfcProfile({}, {}, {}, {}, {});
        QCOMPARE(channel.cfcProfileApplyCountForTest(), count);
        QVERIFY(channel.lastCfcProfileForTest() == accepted);
    }

    void queuedProfilesRetainTheirOwnValues()
    {
        RadioModel radio;
        TxChannel channel(1, 64, 64);
        QThread worker;
        QSemaphore started, release;
        channel.moveToThread(&worker);
        worker.start();
        const auto finish = qScopeGuard([&] {
            release.release();
            radio.bindCfcProfileChannelForTest(nullptr);
            QMetaObject::invokeMethod(&channel, [&] { channel.moveToThread(radio.thread()); }, Qt::BlockingQueuedConnection);
            worker.quit(); worker.wait();
        });
        radio.bindCfcProfileChannelForTest(&channel);
        QList<std::array<std::vector<double>, 5>> observed;
        connect(&radio.transmitModel(), &TransmitModel::cfcEditProfileChanged, &channel,
                [&](const CfcEditProfile&) { observed.append(channel.lastCfcProfileForTest()); });
        QMetaObject::invokeMethod(&channel, [&] { started.release(); release.acquire(); }, Qt::QueuedConnection);
        QVERIFY(started.tryAcquire(1, 5000));
        CfcEditProfile first = configuredProfile(5);
        CfcEditProfile second = configuredProfile(18);
        second.compression.q[1] = 9.8765;
        second.compression.gainsDb[1] = 12.345;
        radio.transmitModel().setCfcProfile(first);
        radio.transmitModel().setCfcProfile(second);
        release.release();
        QMetaObject::invokeMethod(&channel, [] {}, Qt::BlockingQueuedConnection);
        QCOMPARE(observed.size(), 2);
        QCOMPARE(observed[0][0].size(), std::size_t(5));
        QCOMPARE(observed[0][3][1], 1.2345);
        QCOMPARE(observed[1][0].size(), std::size_t(18));
        QCOMPARE(observed[1][1][1], 12.345);
        QCOMPARE(observed[1][3][1], 9.8765);
    }
    void reconnectAndProfileSwitchRestoreWithoutDialog()
    {
        RadioModel radio;
        TxChannel channel(1, 64, 64);
        TransmitModel& tx = radio.transmitModel();
        tx.loadFromSettings("aa:bb:cc:11:22:33");
        MicProfileManager* mgr = radio.micProfileManager();
        QVERIFY(mgr);
        mgr->setMacAddress("aa:bb:cc:11:22:33"); mgr->load();
        tx.setCfcProfile(configuredProfile(5));
        QVERIFY(mgr->saveProfile("Five", &tx));
        const QString saved = tx.cfcParaEqData();
        radio.bindCfcProfileChannelForTest(&channel);
        QCOMPARE(channel.lastCfcProfileForTest()[0].size(), std::size_t(5));
        radio.bindCfcProfileChannelForTest(nullptr);
        tx.setCfcProfile(configuredProfile(18));
        radio.bindCfcProfileChannelForTest(&channel);
        QCOMPARE(channel.lastCfcProfileForTest()[0].size(), std::size_t(18));
        const quint64 switchBefore = channel.cfcProfileApplyCountForTest();
        QVERIFY(mgr->setActiveProfile("Five", &tx));
        QCOMPARE(channel.cfcProfileApplyCountForTest(), switchBefore + 1);
        QCOMPARE(channel.lastCfcProfileForTest()[0].size(), std::size_t(5));
        QCOMPARE(channel.lastCfcProfileForTest()[3][1], 1.23);
        CfcEditProfile unsaved = configuredProfile(5); unsaved.compression.q[1] = 1.2349;
        tx.setCfcProfile(unsaved);
        QCOMPARE(tx.cfcParaEqData(), saved);
        const quint64 reloadBefore = channel.cfcProfileApplyCountForTest();
        QVERIFY(mgr->setActiveProfile("Five", &tx));
        QCOMPARE(channel.cfcProfileApplyCountForTest(), reloadBefore + 1);
        QCOMPARE(channel.lastCfcProfileForTest()[3][1], 1.23);
        const quint64 unchangedBefore = channel.cfcProfileApplyCountForTest();
        QVERIFY(mgr->setActiveProfile("Five", &tx));
        QCOMPARE(channel.cfcProfileApplyCountForTest(), unchangedBefore + 1);
        const quint64 before = channel.cfcProfileApplyCountForTest();
        radio.replayCfcProfileForTest();
        QCOMPARE(channel.cfcProfileApplyCountForTest(), before + 1);
    }
};
QTEST_GUILESS_MAIN(TstRadioModelCfcProfileWiring)
#include "tst_radio_model_cfc_profile_wiring.moc"
