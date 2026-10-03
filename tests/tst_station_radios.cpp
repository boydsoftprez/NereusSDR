// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_radios.cpp  (NereusSDR)
// =================================================================
//
// Parity Task 21 (R-IOS-18, R-R3-38, R-R3-49; the iPhone app plan's Task 25
// station half): the Core's radios and its choice of one.
//
//   - The choice order: a radio chosen from an app, then radio_mac, then
//     the one radio in sight; with two in sight (a G2E among them) the Core
//     waits and never takes the first; a chosen radio out of sight or in use
//     is waited for; a run keeps the radio it identified.
//   - The requests: a choice is saved before the switch starts; a radio the
//     Core cannot see, a second choice mid-switch, a model the board does
//     not allow and forgetting the Core's radio are refused in plain words;
//     every request is refused while the radio is on the air, and nothing is
//     saved or switched then.
//   - The Core (DaemonApp): with two radios in sight it lists both and
//     waits; a choice restarts it on that radio; the saved choice beats
//     radio_mac on the next start.
//
// No radio is contacted: discovery is the test's own list and nothing
// connects; the on-air case keys the station model's own MoxController with
// its receive-only pre-check lifted, reaching no radio.
//
//   cmake --build build --target tst_station_radios
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_station_radios$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>
#include <algorithm>

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioDiscovery.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "core/session/SettingsHygieneWire.h"
#include "core/station/StationRadios.h"
#include "models/RadioModel.h"
#define private public
#include "core/daemon/DaemonApp.h"
#undef private
#include "core/daemon/DaemonConfig.h"

using namespace NereusSDR;

namespace {

RadioInfo radio(const QString& mac, HPSDRHW board, const QString& name, bool inUse = false)
{
    RadioInfo info;
    info.macAddress = mac;
    info.boardType = board;
    info.name = name;
    info.inUse = inUse;
    info.address = QHostAddress(QStringLiteral("127.0.0.1"));
    info.port = 1;
    return info;
}

const QString kHl2 = QStringLiteral("AA:BB:CC:00:00:01");
const QString kG2 = QStringLiteral("AA:BB:CC:00:00:02");
const QString kG2e = QStringLiteral("AA:BB:CC:00:00:03");

DaemonConfig testCoreConfig()
{
    DaemonConfig config = DaemonConfig::defaults();
    config.remotePort = 0;
    config.remoteTransmitAllowed = false;
    return config;
}

SessionMessage invoke(const QByteArray& verb, quint32 id, const QList<MirrorUpdate>& args)
{
    return SessionMessages::commandInvoke(verb, id, args);
}

MirrorUpdate macArg(const QString& mac)
{
    return MirrorUpdate{0, "mac", MirrorWireKind::Utf8, mac};
}

} // namespace

class TstStationRadios : public QObject {
    Q_OBJECT

private slots:
    void settingsHygieneUsesOnlyTheCurrentCoreMacAndRefusesOnAirMutation()
    {
        auto& settings = AppSettings::instance();
        RadioModel core;
        core.setBoardForTest(HPSDRHW::Hermes);
        core.setLastRadioInfoForTest(radio(kHl2, HPSDRHW::Hermes, QStringLiteral("Hermes")));
        core.setConnectionStateForTest(ConnectionState::Connected);
        settings.setValue(QStringLiteral("hardware/%1/sAtt").arg(kHl2), 99);
        settings.setValue(QStringLiteral("hardware/%1/sAtt").arg(kG2), 99);
        SessionCommandDispatcher dispatcher(&core);
        QList<SessionMessage> results;
        connect(&dispatcher, &SessionCommandDispatcher::commandResultReady, this,
                [&results](const SessionMessage& m) { results.append(m); });

        dispatcher.dispatch(invoke("station.validateSettings", 1, {macArg(kG2)}));
        QCOMPARE(results.size(), 1);
        QVERIFY(!results.takeFirst().accepted);
        QCOMPARE(settings.value(QStringLiteral("hardware/%1/sAtt").arg(kG2)).toInt(), 99);
        dispatcher.dispatch(invoke("station.validateSettings", 2, {macArg(kHl2)}));
        QCOMPARE(results.size(), 1);
        QVERIFY(results.first().accepted);
        const auto issues = SettingsHygieneWire::decode(results.takeFirst().updates);
        QVERIFY(issues);
        QCOMPARE(issues->mac, kHl2);
        QVERIFY(!issues->issues.isEmpty());

        MoxController* mox = core.moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        QVERIFY(mox->isMox());
        dispatcher.dispatch(invoke("station.forgetSettings", 3, {macArg(kHl2)}));
        QCOMPARE(results.size(), 1);
        QVERIFY(!results.takeFirst().accepted);
        QCOMPARE(settings.value(QStringLiteral("hardware/%1/sAtt").arg(kHl2)).toInt(), 99);
        mox->setMox(false);
        QTRY_VERIFY(!core.stationOnAirRefusal(nullptr));

        dispatcher.dispatch(invoke("station.forgetSettings", 4, {macArg(kG2)}));
        QVERIFY(!results.takeFirst().accepted);
        dispatcher.dispatch(invoke("station.forgetSettings", 5, {macArg(kHl2)}));
        QCOMPARE(results.size(), 1);
        QVERIFY(results.takeFirst().accepted);
        QVERIFY(!settings.contains(QStringLiteral("hardware/%1/sAtt").arg(kHl2)));
        QCOMPARE(settings.value(QStringLiteral("hardware/%1/sAtt").arg(kG2)).toInt(), 99);
    }

    // G-38: Repair invalid settings from a remote window runs the same
    // repair as a local window (SettingsHygiene::resetSettingsToDefaults):
    // the current radio's MAC only, refused on the air, and it answers with
    // the re-validated issue list.
    void settingsRepairRunsTheLocalRepairOnTheCurrentRadioOnly()
    {
        auto& settings = AppSettings::instance();
        RadioModel core;
        core.setBoardForTest(HPSDRHW::Hermes);
        core.setLastRadioInfoForTest(radio(kHl2, HPSDRHW::Hermes, QStringLiteral("Hermes")));
        core.setConnectionStateForTest(ConnectionState::Connected);
        const QString attKey = QStringLiteral("hardware/%1/sAtt").arg(kHl2);
        const QString otherAttKey = QStringLiteral("hardware/%1/sAtt").arg(kG2);
        const QString apolloKey = QStringLiteral("hardware/%1/apollo/enabled").arg(kHl2);
        settings.setValue(attKey, 99);
        settings.setValue(otherAttKey, 99);
        settings.setValue(apolloKey, QStringLiteral("True"));
        SessionCommandDispatcher dispatcher(&core);
        QList<SessionMessage> results;
        connect(&dispatcher, &SessionCommandDispatcher::commandResultReady, this,
                [&results](const SessionMessage& m) { results.append(m); });

        const auto spec = std::find_if(
            SessionCommandDispatcher::verbSpecs().cbegin(),
            SessionCommandDispatcher::verbSpecs().cend(),
            [](const CommandVerbSpec& s) { return s.verb == "station.repairSettings"; });
        QVERIFY(spec != SessionCommandDispatcher::verbSpecs().cend());
        QCOMPARE(spec->capability, QByteArray("settingsHygieneVersion"));
        QCOMPARE(spec->capabilityVersion, 2);

        MoxController* mox = core.moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        dispatcher.dispatch(invoke("station.repairSettings", 1, {macArg(kHl2)}));
        QCOMPARE(results.size(), 1);
        QVERIFY(!results.takeFirst().accepted);
        QCOMPARE(settings.value(attKey).toInt(), 99);
        mox->setMox(false);
        QTRY_VERIFY(!core.stationOnAirRefusal(nullptr));

        dispatcher.dispatch(invoke("station.repairSettings", 2, {macArg(kG2)}));
        QCOMPARE(results.size(), 1);
        QVERIFY(!results.takeFirst().accepted);
        QCOMPARE(settings.value(otherAttKey).toInt(), 99);

        dispatcher.dispatch(invoke("station.repairSettings", 3, {macArg(kHl2)}));
        QCOMPARE(results.size(), 1);
        const SessionMessage repaired = results.takeFirst();
        QVERIFY2(repaired.accepted, qPrintable(repaired.reason));
        const BoardCapabilities& caps = core.boardCapabilities();
        QCOMPARE(settings.value(attKey).toInt(), caps.attenuator.maxDb);
        QCOMPARE(settings.contains(apolloKey), caps.hasApollo);
        QCOMPARE(settings.value(otherAttKey).toInt(), 99);
        const auto issues = SettingsHygieneWire::decode(repaired.updates);
        QVERIFY(issues);
        QCOMPARE(issues->mac, kHl2);
    }

    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("station-radios-%1").arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        AppSettings::instance().clear();
        QString error;
        QVERIFY2(AppSettings::instance().save(&error), qPrintable(error));
    }

    void cleanupTestCase() { QFile::remove(AppSettings::instance().filePath()); }

    void theChoiceOrder()
    {
        const RadioInfo hl2 = radio(kHl2, HPSDRHW::HermesLite, QStringLiteral("HL2"));
        const RadioInfo g2 = radio(kG2, HPSDRHW::Saturn, QStringLiteral("G2"));
        const RadioInfo g2e = radio(kG2e, HPSDRHW::HermesC10, QStringLiteral("G2E"));
        using Pick = StationRadios::Pick;

        // Exactly one in sight: automatic.
        auto c = StationRadios::choose({hl2}, {}, {}, {});
        QCOMPARE(c.pick, Pick::Radio);
        QCOMPARE(c.radio.macAddress, kHl2);
        // Two in sight, nothing chosen: wait, never the first (nor the G2E).
        c = StationRadios::choose({g2e, g2}, {}, {}, {});
        QCOMPARE(c.pick, Pick::WaitForChoice);
        QVERIFY(OperatorWording::isPlain(c.reason));
        c = StationRadios::choose({}, {}, {}, {});
        QCOMPARE(c.pick, Pick::NoRadio);
        // radio_mac beats the automatic pick; the saved choice beats both.
        c = StationRadios::choose({hl2, g2}, {}, {}, kG2.toLower());
        QCOMPARE(c.pick, Pick::Radio);
        QCOMPARE(c.radio.macAddress, kG2);
        c = StationRadios::choose({hl2, g2}, {}, kHl2, kG2);
        QCOMPARE(c.radio.macAddress, kHl2);
        // A run keeps the radio it identified.
        c = StationRadios::choose({hl2, g2}, kG2, kHl2, {});
        QCOMPARE(c.radio.macAddress, kG2);
        // The chosen radio out of sight: wait for it, even with one other in
        // sight.
        c = StationRadios::choose({hl2}, {}, kG2, {});
        QCOMPARE(c.pick, Pick::WaitForChosen);
        // In use by another program: wait.
        const RadioInfo busyG2 = radio(kG2, HPSDRHW::Saturn, QStringLiteral("G2"), true);
        c = StationRadios::choose({busyG2}, {}, kG2, {});
        QCOMPARE(c.pick, Pick::WaitForChosen);
        c = StationRadios::choose({busyG2}, {}, {}, {});
        QCOMPARE(c.pick, Pick::WaitForChosen);
        for (const QString& reason :
             {StationRadios::choose({}, {}, {}, {}).reason,
              StationRadios::choose({hl2}, {}, kG2, {}).reason,
              StationRadios::choose({busyG2}, {}, {}, {}).reason}) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
    }

    void theRequests()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        StationRadios radios(settings);
        QStringList selected;
        int rescans = 0;
        radios.onSelect = [&](const QString& mac) {
            // Fix wave, I6: pending, not saved, when the switch starts.
            QCOMPARE(radios.pendingChoice(), mac);
            QVERIFY(radios.savedChoice().isEmpty());
            selected.append(mac);
        };
        radios.onRescan = [&]() { ++rescans; };
        radios.setVisible({radio(kHl2, HPSDRHW::HermesLite, QStringLiteral("HL2")),
                           radio(kG2, HPSDRHW::Saturn, QStringLiteral("G2"))});
        radios.setCurrent(radio(kHl2, HPSDRHW::HermesLite, QStringLiteral("HL2")));

        const QList<StationRadioEntry> entries = radios.entries();
        QCOMPARE(entries.size(), 2);
        QVERIFY(entries.first().inUse);
        QCOMPARE(entries.first().mac, kHl2);
        QCOMPARE(entries.first().model, static_cast<int>(HPSDRModel::HERMESLITE));
        QCOMPARE(entries.last().model, static_cast<int>(HPSDRModel::ANAN_G2));
        QCOMPARE(StationRadioEntry::fromFields(entries.last().id, entries.last().toFields()),
                 std::optional<StationRadioEntry>(entries.last()));

        QString reason;
        QVERIFY(!radios.select(QStringLiteral("AA:BB:CC:00:00:99"), &reason));
        QCOMPARE(reason, StationRadios::unknownRadioReason());
        // The Core's radio again: taken, nothing switched.
        QVERIFY(radios.select(kHl2, &reason));
        QVERIFY(selected.isEmpty());
        QVERIFY(radios.select(kG2.toLower(), &reason));
        QCOMPARE(selected, QStringList{kG2});
        QVERIFY(settings.value(QLatin1String(StationRadios::kChoiceKey)).toString().isEmpty());
        QVERIFY(!radios.select(kG2, &reason));
        QCOMPARE(reason, StationRadios::switchingReason());
        // Fix wave, M1: the pending choice is not forgotten.
        QVERIFY(!radios.forget(kG2, &reason));
        QCOMPARE(reason, StationRadios::inUseReason());
        // Another radio's connect does not save it; its own does.
        radios.confirmChoice(kHl2);
        QVERIFY(radios.savedChoice().isEmpty());
        radios.confirmChoice(kG2);
        QCOMPARE(settings.value(QLatin1String(StationRadios::kChoiceKey)).toString(), kG2);
        QVERIFY(radios.pendingChoice().isEmpty());
        radios.setSwitching(false);
        // Fix wave, M1: nor the radio the Core is reconnecting to or waiting
        // for.
        radios.setTarget(kG2.toLower());
        QVERIFY(!radios.forget(kG2, &reason));
        QCOMPARE(reason, StationRadios::inUseReason());
        radios.setTarget(kHl2);

        QVERIFY(radios.rescan(&reason));
        QCOMPARE(rescans, 1);

        QVERIFY(!radios.setModel(kG2, static_cast<int>(HPSDRModel::HERMESLITE), &reason));
        QCOMPARE(reason, QStringLiteral("That model does not match this radio."));
        QSignalSpy changed(&radios, &StationRadios::entriesChanged);
        QVERIFY(radios.setModel(kG2, static_cast<int>(HPSDRModel::ANAN_G2_1K), &reason));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(settings.modelOverride(kG2), HPSDRModel::ANAN_G2_1K);
        QCOMPARE(radios.entries().last().model, static_cast<int>(HPSDRModel::ANAN_G2_1K));

        QVERIFY(!radios.forget(kHl2, &reason));
        QCOMPARE(reason, StationRadios::inUseReason());
        QVERIFY(radios.forget(kG2, &reason));
        QCOMPARE(settings.modelOverride(kG2), HPSDRModel::FIRST);
        QVERIFY(radios.savedChoice().isEmpty());
        QCOMPARE(radios.entries().size(), 1);

        for (const QString& text :
             {StationRadios::unknownRadioReason(), StationRadios::switchingReason(),
              StationRadios::inUseReason()}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }

        // The Core's radio drops off: still listed, no longer in use.
        radios.clearCurrent();
        QCOMPARE(radios.entries().size(), 1);
        QVERIFY(!radios.entries().first().inUse);
    }

    // Phone wire batch (radioModelsVersion 1): each record names its model
    // as Setup does and lists the models its board can run as, in the model
    // combo's order, the same list setModel() accepts.
    void eachRadioCarriesItsModelChoices()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        StationRadios radios(settings);
        const QString kHermes = QStringLiteral("AA:BB:CC:00:00:04");
        radios.setVisible({radio(kG2, HPSDRHW::Saturn, QStringLiteral("G2")),
                           radio(kHermes, HPSDRHW::Hermes, QStringLiteral("Hermes"))});

        QList<StationRadioEntry> entries = radios.entries();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).modelLabel, QStringLiteral("ANAN-G2"));
        QCOMPARE(entries.at(0).models, (QList<int>{static_cast<int>(HPSDRModel::ANAN_G2),
                                                   static_cast<int>(HPSDRModel::ANAN_G2_1K)}));
        // A Hermes board runs as its own models, the ANAN-10E and -100B
        // (Hermes or Hermes II) and the Red Pitaya (Hermes or Orion MkII):
        // the board check's cross-board cases.
        QCOMPARE(entries.at(1).models, (QList<int>{static_cast<int>(HPSDRModel::HERMES),
                                                   static_cast<int>(HPSDRModel::ANAN10),
                                                   static_cast<int>(HPSDRModel::ANAN10E),
                                                   static_cast<int>(HPSDRModel::ANAN100),
                                                   static_cast<int>(HPSDRModel::ANAN100B),
                                                   static_cast<int>(HPSDRModel::REDPITAYA)}));
        for (const StationRadioEntry& entry : entries) {
            for (int m : entry.models) {
                QString reason;
                QVERIFY2(radios.setModel(entry.mac, m, &reason), qPrintable(reason));
            }
        }

        // The label follows the model the Core runs it as.
        QString reason;
        QVERIFY(radios.setModel(kG2, static_cast<int>(HPSDRModel::ANAN_G2_1K), &reason));
        entries = radios.entries();
        QCOMPARE(entries.at(0).modelLabel, QStringLiteral("ANAN-G2 1K"));

        // On the wire: {model, label} per choice.
        const QJsonObject fields = entries.at(0).toFields();
        QCOMPARE(fields.value(QStringLiteral("modelLabel")).toString(),
                 QStringLiteral("ANAN-G2 1K"));
        const QJsonArray models = fields.value(QStringLiteral("models")).toArray();
        QCOMPARE(models.size(), 2);
        QCOMPARE(models.at(1).toObject(),
                 (QJsonObject{{QStringLiteral("model"), static_cast<int>(HPSDRModel::ANAN_G2_1K)},
                              {QStringLiteral("label"), QStringLiteral("ANAN-G2 1K")}}));
        QCOMPARE(StationRadioEntry::fromFields(entries.at(0).id, fields),
                 std::optional<StationRadioEntry>(entries.at(0)));
    }

    void everyRequestWaitsWhileTheRadioIsOnTheAir()
    {
        RadioModel core;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        StationRadios radios(settings);
        int switches = 0;
        int rescans = 0;
        radios.onSelect = [&](const QString&) { ++switches; };
        radios.onRescan = [&]() { ++rescans; };
        radios.setVisible({radio(kHl2, HPSDRHW::HermesLite, QStringLiteral("HL2")),
                           radio(kG2, HPSDRHW::Saturn, QStringLiteral("G2"))});
        radios.setCurrent(radio(kHl2, HPSDRHW::HermesLite, QStringLiteral("HL2")));
        SessionCommandDispatcher dispatcher(&core);
        dispatcher.setStationRadios(&radios);
        QList<SessionMessage> results;
        connect(&dispatcher, &SessionCommandDispatcher::commandResultReady, this,
                [&results](const SessionMessage& m) { results.append(m); });

        MoxController* const mox = core.moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        QVERIFY(mox->isMox());
        dispatcher.dispatch(invoke("station.selectRadio", 1, {macArg(kG2)}));
        dispatcher.dispatch(invoke("station.rescanRadios", 2, {}));
        dispatcher.dispatch(invoke(
            "station.setRadioModel", 3,
            {macArg(kG2), MirrorUpdate{0, "model", MirrorWireKind::Int64,
                                       QVariant(qlonglong(HPSDRModel::ANAN_G2_1K))}}));
        dispatcher.dispatch(invoke("station.forgetRadio", 4, {macArg(kG2)}));
        QTRY_COMPARE(results.size(), 4);
        for (const SessionMessage& r : results) {
            QVERIFY(!r.accepted);
            QCOMPARE(r.reason, RadioModel::onAirReason());
        }
        QCOMPARE(switches, 0);
        QCOMPARE(rescans, 0);
        QVERIFY(radios.savedChoice().isEmpty());
        QCOMPARE(settings.modelOverride(kG2), HPSDRModel::FIRST);
        QCOMPARE(radios.entries().size(), 2);

        mox->setMox(false);
        QTRY_VERIFY(!core.stationOnAirRefusal(nullptr));
        results.clear();
        dispatcher.dispatch(invoke("station.selectRadio", 5, {macArg(kG2)}));
        QTRY_COMPARE(results.size(), 1);
        QVERIFY(results.first().accepted);
        QCOMPARE(switches, 1);
        // A dispatcher with no radios (a Core that is not nereusd) says so.
        SessionCommandDispatcher bare(&core);
        QList<SessionMessage> bareResults;
        connect(&bare, &SessionCommandDispatcher::commandResultReady, this,
                [&bareResults](const SessionMessage& m) { bareResults.append(m); });
        bare.dispatch(invoke("station.rescanRadios", 6, {}));
        QTRY_COMPARE(bareResults.size(), 1);
        QCOMPARE(bareResults.first().reason,
                 QStringLiteral("This Core does not change its radio from this app."));
    }

    void theCoreWaitsListsAndSwitches()
    {
        const RadioInfo hl2 = radio(kHl2, HPSDRHW::HermesLite, QStringLiteral("HL2"));
        const RadioInfo g2 = radio(kG2, HPSDRHW::Saturn, QStringLiteral("G2"));
        std::atomic<bool> offer {true};
        {
            DaemonApp app;
            app.m_radioRetryInitialMs = 10;
            app.m_radioRetryMaximumMs = 40;
            RadioDiscovery::clearHoldOffForTest();
            app.m_discoveryProviderForTest = [&]() {
                return offer.load() ? QList<RadioInfo>{hl2, g2} : QList<RadioInfo>{};
            };
            QVERIFY(app.start(testCoreConfig()));
            QTRY_COMPARE(app.m_stationRadios->entries().size(), 2);
            QVERIFY(!app.m_stationRadios->waitingReason().isEmpty());
            QVERIFY(app.m_radioModel->connection() == nullptr);
            QVERIFY(app.m_selectedRadioMac.isEmpty());

            // A choice from an app: pending, then the Core restarts on it
            // (nothing is in sight afterwards, so nothing connects here).
            offer = false;
            QString reason;
            QVERIFY(app.m_stationRadios->select(kG2, &reason));
            QTRY_COMPARE(app.m_selectedRadioMac, kG2);
            QCOMPARE(app.m_stationRadios->pendingChoice(), kG2);
            // Fix wave, I6: never saved, since it never connected.
            QVERIFY(app.m_stationRadios->savedChoice().isEmpty());
            // Fix wave, M1: the radio the Core waits for is not forgotten.
            QVERIFY(!app.m_stationRadios->forget(kG2, &reason));
            QCOMPARE(reason, StationRadios::inUseReason());
            app.stop();
        }
        {
            // Fix wave, I6 (a crash loop): a restarted Core does not reload
            // the pending choice; radio_mac holds.
            DaemonApp app;
            app.m_discoveryProviderForTest = []() { return QList<RadioInfo>{}; };
            DaemonConfig cfg = testCoreConfig();
            cfg.radioMac = kHl2;
            QVERIFY(app.start(cfg));
            QCOMPARE(app.m_selectedRadioMac, kHl2);
            app.stop();
        }
        {
            // A choice that connected (saved) beats radio_mac.
            StationRadios(AppSettings::instance()).saveChoice(kG2);
            DaemonApp app;
            app.m_discoveryProviderForTest = []() { return QList<RadioInfo>{}; };
            DaemonConfig cfg = testCoreConfig();
            cfg.radioMac = kHl2;
            QVERIFY(app.start(cfg));
            QCOMPARE(app.m_selectedRadioMac, kG2);
            app.stop();
        }
    }
};

QTEST_MAIN(TstStationRadios)
#include "tst_station_radios.moc"
