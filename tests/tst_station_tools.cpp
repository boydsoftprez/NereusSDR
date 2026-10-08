// no-port-check: NereusSDR-original. iPhone app plan Task 25 (D41,
// R-IOS-18): the catalogue's Tools and Radio items say what this radio and
// this Core can open. Built from StationCatalog::build and from a Core's own
// RadioModel; no radio, no socket listens, no audio device is opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Created (iPhone app plan Task 25, D41).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/UnbuiltFeatureList.h"
#include "core/session/StationCatalog.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

QHash<QString, bool> offeredById(const QJsonObject& catalog, const QString& key)
{
    QHash<QString, bool> offered;
    for (const QJsonValue& value : catalog.value(key).toArray()) {
        const QJsonObject entry = value.toObject();
        offered.insert(entry.value(QStringLiteral("id")).toString(),
                       entry.value(QStringLiteral("offered")).toBool());
    }
    return offered;
}

StationCatalog::Inputs inputsFor(HPSDRModel model, bool stationTci, bool vax)
{
    StationCatalog::Inputs inputs;
    inputs.model = model;
    inputs.board = BoardCapsTable::forModel(model);
    inputs.stationTciServer = stationTci;
    inputs.vaxDevices = vax;
    return inputs;
}

} // namespace

class TstStationTools : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("station-tools-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }
    void cleanup() { UnbuiltFeatures::resetForTest(); }

    // The rules for an ANAN-G2 on a station the desktop hosts.
    void anan2HostedOffersItsHardwareTools()
    {
        const BoardCapabilities& caps = BoardCapsTable::forModel(HPSDRModel::ANAN_G2);
        const QJsonObject catalog = StationCatalog::build(
            inputsFor(HPSDRModel::ANAN_G2, /*stationTci=*/true, /*vax=*/true));
        const QHash<QString, bool> tools = offeredById(catalog, QStringLiteral("tools"));
        QCOMPARE(tools.size(), 12);
        for (const char* always : {"spotHub", "freedvReporter", "txEqualizer",
                                   "networkDiagnostics", "supportBundle"}) {
            QVERIFY2(tools.value(QString::fromLatin1(always)), always);
        }
        QVERIFY(caps.hasPureSignal);
        QCOMPARE(tools.value(QStringLiteral("pureSignal")), true);
        QCOMPARE(tools.value(QStringLiteral("diversity")), caps.hasDiversityReceiver);
        QCOMPARE(tools.value(QStringLiteral("tciServer")), true);
        QCOMPARE(tools.value(QStringLiteral("vaxAudio")), true);
        // CAT is built; the other two desktop tools remain unavailable.
        QCOMPARE(tools.value(QStringLiteral("cwx")), false);
        QCOMPARE(tools.value(QStringLiteral("memoryManager")), false);
        QCOMPARE(tools.value(QStringLiteral("catControl")), true);

        const QHash<QString, bool> radio = offeredById(catalog, QStringLiteral("radioItems"));
        QCOMPARE(radio.size(), 4);
        QVERIFY(caps.hasAlex && caps.antennaInputCount >= 3);
        QCOMPARE(radio.value(QStringLiteral("antennaSetup")), true);
        QCOMPARE(radio.value(QStringLiteral("manageRadios")), true);
        QCOMPARE(radio.value(QStringLiteral("protocolInfo")), true);
        QCOMPARE(radio.value(QStringLiteral("transverters")), false);
    }

    // A Hermes Lite 2 on a headless Core without a station TCI server.
    void hermesLite2HeadlessOffersLess()
    {
        const BoardCapabilities& caps = BoardCapsTable::forModel(HPSDRModel::HERMESLITE);
        const QJsonObject catalog = StationCatalog::build(
            inputsFor(HPSDRModel::HERMESLITE, /*stationTci=*/false, /*vax=*/false));
        const QHash<QString, bool> tools = offeredById(catalog, QStringLiteral("tools"));
        QCOMPARE(tools.value(QStringLiteral("pureSignal")), caps.hasPureSignal);
        QVERIFY(!caps.hasDiversityReceiver);
        QCOMPARE(tools.value(QStringLiteral("diversity")), false);
        QCOMPARE(tools.value(QStringLiteral("tciServer")), false);
        QCOMPARE(tools.value(QStringLiteral("vaxAudio")), false);
        QCOMPARE(tools.value(QStringLiteral("spotHub")), true);
        const QHash<QString, bool> radio = offeredById(catalog, QStringLiteral("radioItems"));
        // The desktop hides antenna controls without Alex or with fewer than
        // three antenna inputs.
        QVERIFY(!caps.hasAlex || caps.antennaInputCount < 3);
        QCOMPARE(radio.value(QStringLiteral("antennaSetup")), false);
        QCOMPARE(radio.value(QStringLiteral("manageRadios")), true);
        QCOMPARE(radio.value(QStringLiteral("protocolInfo")), true);
    }

    // The unbuilt features list decides CWX, the Memory Manager, CAT Control
    // and Transverters: building one offers it.
    void unbuiltFeaturesListDecidesTheUnbuiltTools()
    {
        const StationCatalog::Inputs inputs = inputsFor(HPSDRModel::ANAN_G2, true, true);
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::Cwx, true);
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::Memories, true);
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::Cat, true);
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::Transverters, true);
        const QJsonObject catalog = StationCatalog::build(inputs);
        const QHash<QString, bool> tools = offeredById(catalog, QStringLiteral("tools"));
        QCOMPARE(tools.value(QStringLiteral("cwx")), true);
        QCOMPARE(tools.value(QStringLiteral("memoryManager")), true);
        QCOMPARE(tools.value(QStringLiteral("catControl")), true);
        QCOMPARE(offeredById(catalog, QStringLiteral("radioItems"))
                     .value(QStringLiteral("transverters")), true);
    }

    // A Core's own model: the station TCI server and its VAX devices.
    void coreModelSaysWhatItRuns()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        StationCatalog::Inputs inputs = StationCatalog::inputsFrom(model);
        QVERIFY(!inputs.stationTciServer);
        AudioEngine* audio = model.localAudioDevices();
        QVERIFY(audio != nullptr);
        // A Core the desktop hosts publishes VAX devices; nereusd does not.
        QVERIFY(inputs.vaxDevices);
        audio->setVaxOutputsAllowed(false);
        QVERIFY(!StationCatalog::inputsFrom(model).vaxDevices);
        audio->setVaxOutputsAllowed(true);

        // The catalogue follows the station TCI server once it starts.
        StationCatalog catalog;
        catalog.bind(&model);
        QVERIFY(!offeredById(QJsonDocument::fromJson(catalog.json().toUtf8()).object(),
                             QStringLiteral("tools"))
                     .value(QStringLiteral("tciServer")));
        model.enableStationTci(QStringLiteral("127.0.0.1"));
        QVERIFY(StationCatalog::inputsFrom(model).stationTciServer);
        QTRY_VERIFY(offeredById(QJsonDocument::fromJson(catalog.json().toUtf8()).object(),
                                QStringLiteral("tools"))
                        .value(QStringLiteral("tciServer")));
    }
};

QTEST_MAIN(TstStationTools)
#include "tst_station_tools.moc"
