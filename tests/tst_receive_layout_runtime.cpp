// no-port-check: NereusSDR-original Core receive-layout runtime regressions.
//
// R-R3-34: exercise the DaemonApp-owned lifecycle against the real
// AppSettings file.  The primed-board seam deliberately replaces only radio
// discovery/connection; it still uses the real board capability table and
// stream allocator.  RADE worker acceptance belongs in its worker tests.

#include <QtTest/QtTest>

#include <QFile>
#include <QDir>
#include <QScopeGuard>

#include "core/AppSettings.h"
#include "core/ReceiveLayoutStore.h"
#include "core/daemon/DaemonConfig.h"
#define private public
#include "core/daemon/DaemonApp.h"
#undef private
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

namespace {

constexpr auto kLayoutKey = "receiveLayout";
const QString kMacA = QStringLiteral("AA:BB:CC:DD:EE:31");
const QString kMacB = QStringLiteral("AA:BB:CC:DD:EE:32");

QList<ReceiveSliceState> twoReceiverLayout()
{
    return {
        {0, QStringLiteral("pan-0"), 14'293'200.0, DSPMode::USB},
        {2, QStringLiteral("pan-1"), 7'200'000.0, DSPMode::LSB},
    };
}

QList<ReceiveSliceState> sparseLayout()
{
    return {
        {2, QStringLiteral("pan-1"), 7'200'000.0, DSPMode::LSB},
        {4, QStringLiteral("pan-3"), 14'250'000.0, DSPMode::USB},
    };
}

bool saveLayout(const QString& mac, const QList<ReceiveSliceState>& layout)
{
    QString error;
    return ReceiveLayoutStore::stage(AppSettings::instance(), mac, layout, &error)
        && AppSettings::instance().save(&error);
}

ReceiveLayoutStore::LoadResult layoutFromDisk(const QString& mac)
{
    AppSettings reloaded(AppSettings::instance().filePath());
    reloaded.load();
    return ReceiveLayoutStore::load(reloaded, mac);
}

QString rawLayoutFromDisk(const QString& mac)
{
    AppSettings reloaded(AppSettings::instance().filePath());
    reloaded.load();
    return reloaded.hardwareValue(AppSettings::normalizedRadioMac(mac),
                                  QLatin1String(kLayoutKey)).toString();
}

void reloadSingletonFromDisk()
{
    // DaemonApp lifetimes do not reset AppSettings' process singleton.  Clear
    // and reload at the boundary so the next daemon observes its predecessor
    // through the actual file, as a process restart would.
    AppSettings::instance().clear();
    AppSettings::instance().load();
}

RadioModel* model(DaemonApp& app)
{
    return app.m_radioModel.get();
}

bool sliceMatches(RadioModel* radio, int id, double frequencyHz,
                  DSPMode mode, const QString& panKey)
{
    SliceModel* const slice = radio->sliceById(id);
    return slice && slice->frequency() == frequencyHz && slice->dspMode() == mode
        && slice->panKey() == panKey;
}

DaemonConfig configWithCount(int count)
{
    DaemonConfig config = DaemonConfig::defaults();
    config.sliceCount = count;
    config.remotePort = 0;
    return config;
}

const QString kKeptLayout = QStringLiteral("Your saved layout is kept.");

// The operator reads restore messages in the Core connection panel and in a
// warning toast. Empty when the text is plain and ends exactly once.
QString restoreMessageProblem(const QString& message)
{
    const QStringList internal{QStringLiteral(".."), QStringLiteral("stream"),
                               QStringLiteral("owner"), QStringLiteral("admitted"),
                               QStringLiteral("pan-"), QStringLiteral("pan key"),
                               QStringLiteral("DDC"), QStringLiteral("schema"),
                               QStringLiteral("slice ID"), QStringLiteral("DSP mode"),
                               QStringLiteral("JSON")};
    for (const QString& word : internal) {
        if (message.contains(word)) {
            return QStringLiteral("\"%1\" in: %2").arg(word, message);
        }
    }
    if (!message.endsWith(kKeptLayout) || message.count(kKeptLayout) != 1) {
        return QStringLiteral("must end once with \"%1\": %2").arg(kKeptLayout, message);
    }
    return {};
}

} // namespace

class TstReceiveLayoutRuntime : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // AppSettings is a process singleton.  Set the profile before its
        // first use so these file-backed checks cannot touch a user profile.
        AppSettings::setProfileOverride(QStringLiteral("receive-layout-runtime-%1")
                                        .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        AppSettings::instance().clear();
        QString error;
        QVERIFY2(AppSettings::instance().save(&error), qPrintable(error));
    }

    void cleanupTestCase()
    {
        QFile::remove(AppSettings::instance().filePath());
    }

    void savedMembershipOverridesConfiguredCountAcrossDaemonLifetimes()
    {
        QVERIFY(saveLayout(kMacA, twoReceiverLayout()));

        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
            QVERIFY(app.start(configWithCount(1)));
            RadioModel* const radio = model(app);
            QVERIFY(radio != nullptr);
            QCOMPARE(app.sliceCount(), 2);
            QVERIFY(radio->receiveLayoutOverridesConfiguredCount());
            QCOMPARE(radio->receiveLayoutRestoreState(), QStringLiteral("accepted"));
            // The same top-up helper runs when the selected radio first
            // connects.  A valid saved manifest must keep it from creating
            // a cfg-count receiver after admission has completed.
            app.createConfiguredSlices(3);
            QCOMPARE(app.sliceCount(), 2);
            QVERIFY(sliceMatches(radio, 0, 14'293'200.0, DSPMode::USB,
                                 QStringLiteral("pan-0")));
            QVERIFY(sliceMatches(radio, 2, 7'200'000.0, DSPMode::LSB,
                                 QStringLiteral("pan-1")));
            app.stop();
        }
        reloadSingletonFromDisk();

        // A second DaemonApp must reload membership from the file, not reuse
        // its prior object's topology or top up to cfg.sliceCount.
        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
            QVERIFY(app.start(configWithCount(1)));
            QCOMPARE(app.sliceCount(), 2);
            QVERIFY(model(app)->receiveLayoutOverridesConfiguredCount());
            QVERIFY(sliceMatches(model(app), 2, 7'200'000.0, DSPMode::LSB,
                                 QStringLiteral("pan-1")));
            app.stop();
        }
    }

    void deletionSurvivesRestartDespiteLargerConfiguredCount()
    {
        QVERIFY(saveLayout(kMacA, twoReceiverLayout()));

        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
            QVERIFY(app.start(configWithCount(2)));
            model(app)->removeSlice(2);
            QCOMPARE(app.sliceCount(), 1);
            app.stop(); // must flush the changed membership synchronously.
        }
        reloadSingletonFromDisk();

        const auto saved = layoutFromDisk(kMacA);
        QCOMPARE(static_cast<int>(saved.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(saved.slices.size(), 1);
        QCOMPARE(saved.slices.first().id, 0);

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
        QVERIFY(app.start(configWithCount(2)));
        QCOMPARE(app.sliceCount(), 1);
        QVERIFY(model(app)->sliceById(2) == nullptr);
        QVERIFY(model(app)->receiveLayoutOverridesConfiguredCount());
        app.stop();
    }

    void sparseIdsAndDistinctPansReceiveIndependentResources()
    {
        QVERIFY(saveLayout(kMacA, sparseLayout()));

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
        QVERIFY(app.start(configWithCount(1)));
        RadioModel* const radio = model(app);
        QCOMPARE(app.sliceCount(), 2);
        QVERIFY(radio->sliceById(0) == nullptr);
        QVERIFY(sliceMatches(radio, 2, 7'200'000.0, DSPMode::LSB,
                             QStringLiteral("pan-1")));
        QVERIFY(sliceMatches(radio, 4, 14'250'000.0, DSPMode::USB,
                             QStringLiteral("pan-3")));
        QVERIFY(radio->sliceById(2)->streamIndex() >= 0);
        QVERIFY(radio->sliceById(4)->streamIndex() >= 0);
        QVERIFY(radio->sliceById(2)->streamIndex()
                != radio->sliceById(4)->streamIndex());
        QCOMPARE(radio->receiveLayoutRestoreState(), QStringLiteral("accepted"));
        app.stop();
    }

    void perMacLayoutsRemainIndependent()
    {
        QVERIFY(saveLayout(kMacA, twoReceiverLayout()));
        const QList<ReceiveSliceState> bLayout{
            {1, QStringLiteral("pan-2"), 10'100'000.0, DSPMode::AM},
        };
        QVERIFY(saveLayout(kMacB, bLayout));

        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
            QVERIFY(app.start(configWithCount(1)));
            QCOMPARE(app.sliceCount(), 2);
            QVERIFY(model(app)->sliceById(2) != nullptr);
            app.stop();
        }
        reloadSingletonFromDisk();
        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite, kMacB);
            QVERIFY(app.start(configWithCount(3)));
            QCOMPARE(app.sliceCount(), 1);
            QVERIFY(sliceMatches(model(app), 1, 10'100'000.0, DSPMode::AM,
                                 QStringLiteral("pan-2")));
            QVERIFY(model(app)->sliceById(0) == nullptr);
            app.stop();
        }

        const auto a = layoutFromDisk(kMacA);
        const auto b = layoutFromDisk(kMacB);
        QCOMPARE(a.slices.size(), 2);
        QCOMPARE(a.slices.at(1).id, 2);
        QCOMPARE(b.slices.size(), 1);
        QCOMPARE(b.slices.first().id, 1);
    }

    void offlineStartupDoesNotOverwriteSavedLayoutBeforeIdentityDiscovery()
    {
        QVERIFY(saveLayout(kMacA, twoReceiverLayout()));
        const QString original = rawLayoutFromDisk(kMacA);
        QVERIFY(!original.isEmpty());

        DaemonApp app;
        DaemonConfig config = configWithCount(1);
        // No selected MAC, primed board, or discovery responder: startup must
        // await identity rather than create a provisional bootstrap manifest
        // in any saved radio namespace.
        QVERIFY(app.start(config));
        QVERIFY(model(app) != nullptr);
        QCOMPARE(model(app)->receiveLayoutRestoreState(), QStringLiteral("pending"));
        app.stop();

        QCOMPARE(rawLayoutFromDisk(kMacA), original);
    }

    void invalidAndDegradedLayoutsPreserveTheOriginalManifest()
    {
        const QString invalid = QStringLiteral("{not valid json");
        AppSettings::instance().setHardwareValue(AppSettings::normalizedRadioMac(kMacA),
                                                 QLatin1String(kLayoutKey), invalid);
        QVERIFY(AppSettings::instance().save());
        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
            QVERIFY(app.start(configWithCount(1)));
            QCOMPARE(model(app)->receiveLayoutRestoreState(), QStringLiteral("invalid"));
            const QString problem =
                restoreMessageProblem(model(app)->receiveLayoutRestoreMessage());
            QVERIFY2(problem.isEmpty(), qPrintable(problem));
            QCOMPARE(model(app)->receiveLayoutRestoreMessage(),
                     QStringLiteral("The saved receive layout could not be loaded. "
                                    "It is damaged or was written by a different version "
                                    "of this program. The configured receivers are used "
                                    "instead. Your saved layout is kept."));
            app.stop();
        }
        QCOMPARE(rawLayoutFromDisk(kMacA), invalid);

        const QList<ReceiveSliceState> oversizedForHermesII{
            {0, QStringLiteral("pan-0"), 14'293'200.0, DSPMode::USB},
            {1, QStringLiteral("pan-1"), 7'200'000.0, DSPMode::LSB},
            {2, QStringLiteral("pan-2"), 10'100'000.0, DSPMode::AM},
        };
        QVERIFY(saveLayout(kMacA, oversizedForHermesII));
        const QString original = rawLayoutFromDisk(kMacA);
        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesII, kMacA);
            QVERIFY(app.start(configWithCount(1)));
            QCOMPARE(model(app)->receiveLayoutRestoreState(), QStringLiteral("degraded"));
            QCOMPARE(model(app)->receiveLayoutRestoreMessage(),
                     QStringLiteral("Receiver C (10.1000\u00A0MHz AM) could not be restored "
                                    "because this radio supports only receivers A and B. "
                                    "Your saved layout is kept."));
            QVERIFY(model(app)->sliceById(0) != nullptr);
            QVERIFY(model(app)->sliceById(1) != nullptr);
            QVERIFY(model(app)->sliceById(2) == nullptr);
            app.stop();
        }
        QCOMPARE(rawLayoutFromDisk(kMacA), original);
    }

    // R-R3-34: a layout the store refuses reaches the operator as plain
    // sentences; the store's own reason ("pan key", "receive owner") is
    // only logged.
    void refusedLayoutReasonsReachTheOperatorInPlainWords_data()
    {
        QTest::addColumn<QString>("from");
        QTest::addColumn<QString>("to");
        QTest::addColumn<QString>("expected");
        QTest::newRow("non-canonical panadapter")
            << QStringLiteral("\"panKey\":\"pan-1\"")
            << QStringLiteral("\"panKey\":\"pan-01\"")
            << QStringLiteral("The saved receive layout could not be loaded. It places "
                              "a receiver on a panadapter this program does not "
                              "recognize. The configured receivers are used instead. "
                              "Your saved layout is kept.");
        QTest::newRow("missing RADE audio receiver")
            << QStringLiteral("\"radeRxOwnerId\":2")
            << QStringLiteral("\"radeRxOwnerId\":null")
            << QStringLiteral("The saved receive layout could not be loaded. It has more "
                              "than one receiver in RADE mode and does not say which one "
                              "plays RADE audio. The configured receivers are used "
                              "instead. Your saved layout is kept.");
        QTest::newRow("RADE audio receiver out of range")
            << QStringLiteral("\"radeRxOwnerId\":2")
            << QStringLiteral("\"radeRxOwnerId\":5")
            << QStringLiteral("The saved receive layout could not be loaded. It is "
                              "damaged or was written by a different version of this "
                              "program. The configured receivers are used instead. "
                              "Your saved layout is kept.");
        QTest::newRow("RADE audio receiver not in RADE mode")
            << QStringLiteral("\"dspMode\":13")
            << QStringLiteral("\"dspMode\":1")
            << QStringLiteral("The saved receive layout could not be loaded. The "
                              "receiver it names for RADE audio is not in RADE mode. "
                              "The configured receivers are used instead. Your saved "
                              "layout is kept.");
    }

    void refusedLayoutReasonsReachTheOperatorInPlainWords()
    {
        QFETCH(QString, from);
        QFETCH(QString, to);
        QFETCH(QString, expected);
        const QList<ReceiveSliceState> twoRade{
            {0, QStringLiteral("pan-0"), 14'236'000.0, DSPMode::RADE_U},
            {2, QStringLiteral("pan-1"), 7'177'000.0, DSPMode::RADE_L},
        };
        QString error;
        QVERIFY2(ReceiveLayoutStore::stage(AppSettings::instance(), kMacA, twoRade,
                                           &error, 2), qPrintable(error));
        const QString normalized = AppSettings::normalizedRadioMac(kMacA);
        const QString valid = AppSettings::instance()
            .hardwareValue(normalized, QLatin1String(kLayoutKey)).toString();
        QVERIFY2(valid.contains(from), qPrintable(valid));
        const QString broken = QString(valid).replace(from, to);
        AppSettings::instance().setHardwareValue(normalized, QLatin1String(kLayoutKey),
                                                 broken);
        QVERIFY(AppSettings::instance().save());
        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
            QVERIFY(app.start(configWithCount(1)));
            QCOMPARE(model(app)->receiveLayoutRestoreState(), QStringLiteral("invalid"));
            const QString message = model(app)->receiveLayoutRestoreMessage();
            const QString problem = restoreMessageProblem(message);
            QVERIFY2(problem.isEmpty(), qPrintable(problem));
            QCOMPARE(message, expected);
            app.stop();
        }
        QCOMPARE(rawLayoutFromDisk(kMacA), broken);
    }

    void immediateStopCapturesTunePanAndMembershipWithoutIdZero()
    {
        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
            QVERIFY(app.start(configWithCount(2)));
            RadioModel* const radio = model(app);
            QVERIFY(radio->sliceById(0) != nullptr);
            QVERIFY(radio->sliceById(1) != nullptr);

            // Add and remove a receiver in the same run, then retire A.
            // B remains as the last valid slice, proving capture does not
            // manufacture stable ID 0 during shutdown.
            QCOMPARE(radio->addSlice(QStringLiteral("pan-4")), 2);
            radio->removeSlice(2);
            radio->removeSlice(0);
            SliceModel* const survivor = radio->sliceById(1);
            QVERIFY(survivor != nullptr);
            survivor->setFrequency(3'850'000.0);
            survivor->setDspMode(DSPMode::LSB);
            survivor->setPanKey(QStringLiteral("pan-3"));
            app.stop();
        }

        const auto saved = layoutFromDisk(kMacA);
        QCOMPARE(static_cast<int>(saved.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(saved.slices.size(), 1);
        QCOMPARE(saved.slices.first().id, 1);
        QCOMPARE(saved.slices.first().frequencyHz, 3'850'000.0);
        QCOMPARE(saved.slices.first().dspMode, DSPMode::LSB);
        QCOMPARE(saved.slices.first().panKey, QStringLiteral("pan-3"));
    }

    void nearbyDistinctPansCannotSilentlyShareOneReceiver()
    {
        const QList<ReceiveSliceState> layout{
            {0, "pan-0", 14293000, DSPMode::USB},
            {2, "pan-1", 14294000, DSPMode::USB},
            {4, "pan-2", 14295000, DSPMode::USB},
        };
        QVERIFY(saveLayout(kMacA, layout));
        const QString original = rawLayoutFromDisk(kMacA);
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, kMacA); // two user DDCs
        QVERIFY(app.start(configWithCount(1)));
        RadioModel* radio = model(app);
        QCOMPARE(radio->slices().size(), 2);
        QVERIFY(radio->sliceById(0)->streamIndex() != radio->sliceById(2)->streamIndex());
        QVERIFY(!radio->sliceById(4));
        QCOMPARE(radio->receiveLayoutRestoreState(), QStringLiteral("degraded"));
        QCOMPARE(radio->receiveLayoutRestoreMessage(),
                 QStringLiteral("Receiver E (14.2950\u00A0MHz USB) could not be restored because "
                                "all of the radio's receivers are in use. Add it again with "
                                "+RX after closing another receiver. Your saved layout is kept."));
        radio->sliceById(0)->setFrequency(14296000);
        radio->flushPendingSettingsSave();
        app.stop();
        QCOMPARE(rawLayoutFromDisk(kMacA), original);
    }

    void samePanMayShareWhileOtherPanGetsItsOwnStream()
    {
        QVERIFY(saveLayout(kMacA, {{0, "pan-0", 14293000, DSPMode::USB},
                                   {2, "pan-1", 14294000, DSPMode::USB},
                                   {4, "pan-1", 14295000, DSPMode::USB}}));
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
        QVERIFY(app.start(configWithCount(1)));
        RadioModel* radio = model(app);
        QCOMPARE(radio->receiveLayoutRestoreState(), QStringLiteral("accepted"));
        QCOMPARE(radio->sliceById(2)->streamIndex(), radio->sliceById(4)->streamIndex());
        QVERIFY(radio->sliceById(0)->streamIndex() != radio->sliceById(4)->streamIndex());
        app.stop();
    }

    void laterPanMemberCannotBorrowAnotherPansWindow()
    {
        // Pan 0 ran two bands on two receivers, as live tuning and radio
        // recovery allow. 7.201 MHz lies outside pan 0's 20 m receiver and
        // inside pan 1's 40 m one: it gets a receiver of its own and stays
        // on pan 0, never sharing the other pan's receiver.
        QVERIFY(saveLayout(kMacA, {{0, "pan-0", 14293000, DSPMode::USB},
                                   {2, "pan-1", 7200000, DSPMode::LSB},
                                   {4, "pan-0", 7201000, DSPMode::LSB}}));
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::Saturn, kMacA); // ANAN-G2: five receivers
        QVERIFY(app.start(configWithCount(1)));
        RadioModel* radio = model(app);
        QCOMPARE(radio->slices().size(), 3);
        QCOMPARE(radio->receiveLayoutRestoreState(), QStringLiteral("accepted"));
        QVERIFY2(radio->receiveLayoutRestoreMessage().isEmpty(),
                 qPrintable(radio->receiveLayoutRestoreMessage()));
        QVERIFY(sliceMatches(radio, 4, 7'201'000.0, DSPMode::LSB, QStringLiteral("pan-0")));
        const int later = radio->sliceById(4)->streamIndex();
        QVERIFY(later >= 0);
        QVERIFY(later != radio->sliceById(0)->streamIndex());
        QVERIFY(later != radio->sliceById(2)->streamIndex());
        app.stop();
        const auto saved = layoutFromDisk(kMacA);
        QCOMPARE(saved.slices.size(), 3);
        QCOMPARE(saved.slices.at(2).id, 4);
        QCOMPARE(saved.slices.at(2).panKey, QStringLiteral("pan-0"));
    }

    void laterPanMemberIsRefusedWhenEveryReceiverIsInUse()
    {
        // Same layout on a board whose two receivers the first two pans
        // already hold. Refused, and still not folded into pan 1's receiver.
        QVERIFY(saveLayout(kMacA, {{0, "pan-0", 14293000, DSPMode::USB},
                                   {2, "pan-1", 7200000, DSPMode::LSB},
                                   {4, "pan-0", 7201000, DSPMode::LSB}}));
        const QString original = rawLayoutFromDisk(kMacA);
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, kMacA); // two receivers
        QVERIFY(app.start(configWithCount(1)));
        RadioModel* radio = model(app);
        QCOMPARE(radio->slices().size(), 2);
        QVERIFY(!radio->sliceById(4));
        QVERIFY(radio->sliceById(0)->streamIndex() != radio->sliceById(2)->streamIndex());
        QCOMPARE(radio->receiveLayoutRestoreState(), QStringLiteral("degraded"));
        QCOMPARE(radio->receiveLayoutRestoreMessage(),
                 QStringLiteral("Receiver E (7.2010\u00A0MHz LSB) could not be restored because "
                                "all of the radio's receivers are in use. Add it again with "
                                "+RX after closing another receiver. Your saved layout is kept."));
        app.stop();
        QCOMPARE(rawLayoutFromDisk(kMacA), original);
    }

    // ── Fix wave 1, M2: "restored" only for slices the saved layout held ──

    // A window adds slices while the Core waits for its radio, beside a
    // saved layout. The one the smaller board cannot host was never saved:
    // it is closed, not "could not be restored". The saved layout is still
    // kept.
    void windowAddedSliceBesideASavedLayoutIsClosedNotRestored()
    {
        QVERIFY(saveLayout(kMacA, {{0, QStringLiteral("pan-0"), 14'293'200.0, DSPMode::USB}}));
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::HermesII); // two slices
        radio.prepareReceiveLayout(kMacA);
        QVERIFY(radio.receiveLayoutOverridesConfiguredCount());
        radio.addSliceOnPan(QStringLiteral("pan-0"));
        radio.addSliceOnPan(QStringLiteral("pan-0"));
        QCOMPARE(radio.slices().size(), 3);
        SliceModel* added = radio.sliceById(2);
        QVERIFY(added);
        const QString mhz = QString::number(added->frequency() / 1.0e6, 'f', 4);
        const QString mode = SliceModel::modeName(added->dspMode());

        const auto& caps = radio.boardCapabilities();
        radio.configureStreamPool(caps.userDdcCount, caps.maxSlices, 192000);
        radio.bindUnboundSlices();

        QCOMPARE(radio.slices().size(), 2);
        QCOMPARE(radio.receiveLayoutRestoreState(), QStringLiteral("degraded"));
        const QString message = radio.receiveLayoutRestoreMessage();
        QCOMPARE(message,
                 QStringLiteral("Receiver C (%1\u00A0MHz %2) was closed because this radio "
                                "supports only receivers A and B. Add it again with +RX after "
                                "closing another receiver. Your saved layout is kept.")
                     .arg(mhz, mode));
        QVERIFY2(restoreMessageProblem(message).isEmpty(),
                 qPrintable(restoreMessageProblem(message)));
    }

    // No saved layout: a slice refused only because every receiver is busy
    // was never restored from anything, so it is closed.
    void busyRefusalWithNoSavedLayoutIsClosedNotRestored()
    {
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::HermesLite); // two receivers
        radio.prepareReceiveLayout(kMacB);          // nothing saved
        QVERIFY(!radio.receiveLayoutOverridesConfiguredCount());
        radio.addSliceOnPan(QStringLiteral("pan-0"));
        radio.addSliceOnPan(QStringLiteral("pan-1"));
        radio.addSliceOnPan(QStringLiteral("pan-2"));
        QCOMPARE(radio.slices().size(), 3);
        radio.sliceById(0)->setFrequency(14'200'000.0);
        radio.sliceById(1)->setFrequency(7'150'000.0);
        radio.sliceById(2)->setFrequency(3'700'000.0);
        radio.sliceById(2)->setDspMode(DSPMode::LSB);

        const auto& caps = radio.boardCapabilities();
        radio.configureStreamPool(caps.userDdcCount, caps.maxSlices, 192000);
        radio.bindUnboundSlices();

        QCOMPARE(radio.slices().size(), 2);
        QVERIFY(!radio.sliceById(2));
        QCOMPARE(radio.receiveLayoutRestoreState(), QStringLiteral("degraded"));
        QCOMPARE(radio.receiveLayoutRestoreMessage(),
                 QStringLiteral("Receiver C (3.7000\u00A0MHz LSB) was closed because all of "
                                "the radio's receivers are in use. Add it again with +RX after "
                                "closing another receiver."));
    }

    // ── Fix wave 1, M1: no kept layout claimed when none was saved ──────

    void receiversThatDidNotStartClaimNoSavedLayoutWhenNoneWasSaved()
    {
        RadioModel radio;
        radio.prepareReceiveLayout(kMacB); // nothing saved
        QCOMPARE(radio.receiveLayoutRestoreState(), QStringLiteral("pending"));
        // No radio has sized a receiver pool, so this slice cannot start.
        QVERIFY(radio.addSlice(QStringLiteral("pan-0")) >= 0);
        QVERIFY(radio.sliceById(0)->streamIndex() < 0);

        radio.completeReceiveLayoutStartup();

        QCOMPARE(radio.receiveLayoutRestoreState(), QStringLiteral("fallback"));
        QCOMPARE(radio.receiveLayoutRestoreMessage(),
                 QStringLiteral("Some receivers did not start; check the radio."));
        // Nothing is held back: the receiver the operator has is saved.
        radio.sliceById(0)->setFrequency(10'125'000.0);
        radio.flushPendingSettingsSave();
        const auto saved = layoutFromDisk(kMacB);
        QCOMPARE(static_cast<int>(saved.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(saved.slices.size(), 1);
        QCOMPARE(saved.slices.first().frequencyHz, 10'125'000.0);
    }

    // Fix wave (2026-09-30): a save held back while the layout awaited
    // admission (a receiver closed then) must not stop later edits from
    // being saved once admission is over.
    void saveHeldBackDuringAdmissionDoesNotBlockLaterSaves()
    {
        RadioModel radio;
        radio.prepareReceiveLayout(kMacB); // nothing saved
        QVERIFY(radio.addSlice(QStringLiteral("pan-0")) >= 0);
        const int closing = radio.addSlice(QStringLiteral("pan-0"));
        QVERIFY(closing >= 0);
        // Closed while admission is pending: its save is held back.
        radio.removeSlice(closing);
        QVERIFY(radio.receiveLayoutPendingAdmission());

        radio.completeReceiveLayoutStartup();
        QVERIFY(!radio.receiveLayoutPendingAdmission());

        // An edit after admission reaches the file through the coalescing
        // timer, with no explicit flush.
        radio.sliceById(0)->setFrequency(10'125'000.0);
        QTRY_VERIFY_WITH_TIMEOUT(
            [] {
                const auto saved = layoutFromDisk(kMacB);
                return saved.state == ReceiveLayoutStore::LoadState::Loaded
                    && saved.slices.size() == 1
                    && saved.slices.first().frequencyHz == 10'125'000.0;
            }(),
            5000);
    }

    void refusedRadeReceiverSaysOnceThatItsAudioStaysOff()
    {
        // The Rock's two-band pan 0, with a third pan listed ahead of B so
        // it takes the radio's last receiver. B carried the RADE audio.
        QString error;
        QVERIFY2(ReceiveLayoutStore::stage(AppSettings::instance(), kMacA,
                     {{0, "pan-0", 14290000, DSPMode::USB},
                      {2, "pan-1", 3700000, DSPMode::LSB},
                      {1, "pan-0", 7227600, DSPMode::RADE_U}},
                     &error, std::optional<int>(1)),
                 qPrintable(error));
        QVERIFY2(AppSettings::instance().save(&error), qPrintable(error));
        const QString original = rawLayoutFromDisk(kMacA);
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, kMacA); // two receivers
        QVERIFY(app.start(configWithCount(1)));
        RadioModel* radio = model(app);
        QCOMPARE(radio->slices().size(), 2);
        QVERIFY(!radio->sliceById(1));
        QCOMPARE(radio->receiveLayoutRestoreState(), QStringLiteral("degraded"));
        QCOMPARE(radio->receiveLayoutRestoreMessage(),
                 QStringLiteral("Receiver B (7.2276\u00A0MHz RADE-U) could not be restored because "
                                "all of the radio's receivers are in use. Add it again with "
                                "+RX after closing another receiver. RADE audio from receiver "
                                "B stays off until that receiver is back. Your saved layout "
                                "is kept."));
        app.stop();
        QCOMPARE(rawLayoutFromDisk(kMacA), original);
    }

    void rockRecordPlacesBothBandsOnPanZero()
    {
        // The record the Rock refused at its 14:39 restart, as stored.
        const QString rock = QStringLiteral(
            R"({"radeRxOwnerId":1,"slices":[)"
            R"({"id":0,"panKey":"pan-0","frequencyHz":14290000,"dspMode":1},)"
            R"({"id":1,"panKey":"pan-0","frequencyHz":7227600,"dspMode":12}],"version":1})");
        AppSettings::instance().setHardwareValue(AppSettings::normalizedRadioMac(kMacA),
                                                 QLatin1String(kLayoutKey), rock);
        QVERIFY(AppSettings::instance().save());
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::Saturn, kMacA); // ANAN-G2, as on the Rock
        QVERIFY(app.start(configWithCount(1)));
        RadioModel* radio = model(app);
        QCOMPARE(radio->slices().size(), 2);
        QVERIFY(sliceMatches(radio, 0, 14'290'000.0, DSPMode::USB, QStringLiteral("pan-0")));
        QVERIFY(sliceMatches(radio, 1, 7'227'600.0, DSPMode::RADE_U, QStringLiteral("pan-0")));
        QVERIFY(radio->sliceById(0)->streamIndex() >= 0);
        QVERIFY(radio->sliceById(1)->streamIndex() >= 0);
        QVERIFY(radio->sliceById(1)->streamIndex() != radio->sliceById(0)->streamIndex());
        QCOMPARE(radio->restoredRadeReceiveOwner(), std::optional<int>(1));
        // Placement refused nothing. A primed board has no DSP worker, so the
        // RADE decoder cannot start here, and that is the one thing reported;
        // tst_daemon_radio_recovery's livePanMoveSurvivesCoreRestart covers
        // the running path to an accepted restore.
        QCOMPARE(radio->receiveLayoutRestoreState(), QStringLiteral("degraded"));
        QCOMPARE(radio->receiveLayoutRestoreMessage(),
                 QStringLiteral("RADE audio from receiver B stays off because that receiver "
                                "is not running. Your saved layout is kept."));
        // RADE reason: B's flag says the same, and A (USB) says nothing.
        QCOMPARE(radio->sliceById(1)->radeReason(),
                 QStringLiteral("RADE could not start on slice B: that receiver is not "
                                "running."));
        QVERIFY(radio->sliceById(0)->radeReason().isEmpty());
        app.stop();
        QCOMPARE(rawLayoutFromDisk(kMacA), rock);
    }

    void allUnsupportedIdsUseExplicitFallbackWithoutErasingSavedState()
    {
        QVERIFY(saveLayout(kMacA, {{4, "pan-3", 7200000, DSPMode::LSB}}));
        const QString original = rawLayoutFromDisk(kMacA);
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesII, kMacA); // two channel IDs, not five
        QVERIFY(app.start(configWithCount(2)));
        RadioModel* radio = model(app);
        QCOMPARE(radio->slices().size(), 2);
        QVERIFY(!radio->receiveLayoutOverridesConfiguredCount());
        QCOMPARE(radio->receiveLayoutRestoreState(), QStringLiteral("fallback"));
        QCOMPARE(radio->receiveLayoutRestoreMessage(),
                 QStringLiteral("Receiver E (7.2000\u00A0MHz LSB) could not be restored because "
                                "this radio supports only receivers A and B. Your saved layout "
                                "is kept."));
        QVERIFY(radio->sliceById(0));
        QVERIFY(radio->sliceById(0)->streamIndex() >= 0);
        QVERIFY(!radio->sliceById(4));
        app.stop();
        QCOMPARE(rawLayoutFromDisk(kMacA), original);
    }

    void failedAtomicSaveRetainsLiveLayoutForRetry()
    {
        QVERIFY(saveLayout(kMacA, twoReceiverLayout()));
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
        QVERIFY(app.start(configWithCount(1)));
        RadioModel* radio = model(app);
        radio->sliceById(2)->setFrequency(7225000);
        const QString path = AppSettings::instance().filePath();
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkpath(path));
        const auto removeBlocker = qScopeGuard([path] { QDir().rmdir(path); });
        radio->flushPendingSettingsSave();
        QVERIFY(!radio->settingsSaveError().isEmpty());
        QCOMPARE(radio->sliceById(2)->frequency(), 7225000.0);
        QVERIFY(QDir().rmdir(path));
        radio->flushPendingSettingsSave();
        QVERIFY(radio->settingsSaveError().isEmpty());
        app.stop();
        const auto saved = layoutFromDisk(kMacA);
        QCOMPARE(saved.slices.size(), 2);
        QCOMPARE(saved.slices.at(1).id, 2);
        QCOMPARE(saved.slices.at(1).frequencyHz, 7225000.0);
    }

    // R-R3-34: a live layout the store refuses to save reaches the operator
    // as a plain sentence, not the store's "pan key" reason.
    void unsavableLivePanadapterIsRefusedInPlainWords()
    {
        QVERIFY(saveLayout(kMacA, twoReceiverLayout()));
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, kMacA);
        QVERIFY(app.start(configWithCount(1)));
        RadioModel* radio = model(app);
        radio->sliceById(2)->setPanKey(QStringLiteral("pan-01"));
        radio->flushPendingSettingsSave();
        QCOMPARE(radio->settingsSaveError(),
                 QStringLiteral("Your receivers were not saved. It places a receiver "
                                "on a panadapter this program does not recognize."));
        app.stop();
    }
};

QTEST_MAIN(TstReceiveLayoutRuntime)
#include "tst_receive_layout_runtime.moc"
