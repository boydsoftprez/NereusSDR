// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_bandstack_link.cpp  (NereusSDR)
// =================================================================
//
// The band stack over the link (design section 6.3): the `bandStack` object
// and bandStackVersion for a peer that declared the feature and nothing for one
// that did not, every bandStack.* verb through a loopback peer, the refusals of
// section 11, and a remote window's mirror and command path.
//
// Loopback link, no RF and no hardware. "On the air" keys the Core's own
// MoxController with the MOX pre-check lifted; nothing reaches a radio.
//
//   cmake --build build --target tst_bandstack_link
//   ctest --test-dir build -R '^tst_bandstack_link$' --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QLoggingCategory>
#include <QSignalSpy>

#include "core/TxChannel.h"
#include "core/session/BandStackFacade.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
#include "models/Band.h"
#include "models/bandstack/BandStackController.h"

namespace {

const QString kNotUnderstood = QStringLiteral("The request for the band stack was not understood.");
const QString kUpdateApp = QStringLiteral("Update this app to use band stacks on this Core.");
const QString kNoEntry = QStringLiteral("That band stack entry no longer exists.");
const QString kNoRadio = QStringLiteral("Connect to a radio to use band stacks.");

const QHash<QByteArray, int> kStack{{"deviceAuth", 1}, {"sessionHolder", 1}, {"bandStack", 1}};
const QHash<QByteArray, int> kSharesStack{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}, {"bandStack", 1}};

MirrorUpdate i64(const char* name, qint64 value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Int64, QVariant(qlonglong(value))};
}

MirrorUpdate text(const char* name, const QString& value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Utf8, QVariant(value)};
}

MirrorUpdate flag(const char* name, bool value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Bool, QVariant(value)};
}

QJsonObject parse(const QString& json)
{
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

QJsonObject mirrored(const LoopbackTransport* app)
{
    return parse(latest(app->received(), QStringLiteral("bandStack"), QStringLiteral("json")).toString());
}

QString reasonOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("reason")).toString(QStringLiteral("(no answer)"));
}

// Empty when accepted, otherwise why not.
QString refusalOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool(false) ? QString() : reasonOf(result);
}

QStringList orderOn(const QJsonObject& document, const QString& band)
{
    QStringList ids;
    const QJsonArray order = document.value(QStringLiteral("bands")).toObject().value(band).toObject()
                                 .value(QStringLiteral("order")).toArray();
    for (const QJsonValue& id : order) {
        ids.append(id.toString());
    }
    return ids;
}

QJsonObject entryIn(const QJsonObject& document, const QString& id)
{
    for (const QJsonValue& value : document.value(QStringLiteral("entries")).toArray()) {
        if (value.toObject().value(QStringLiteral("id")).toString() == id) {
            return value.toObject();
        }
    }
    return {};
}

QString placeOf(const QJsonObject& document, const QString& letter, const QString& band)
{
    return document.value(QStringLiteral("places")).toObject().value(letter).toObject()
        .value(band).toString();
}

// A Core that knows its radio, so the band stack is running.
void startStack(Core& core)
{
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    core.model->announceRadioForTest(info);
}

QJsonObject coreDocument(const Core& core)
{
    return parse(core.model->bandStackFacade()->json());
}

} // namespace

class TstBandStackLink : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("bandstack-link-%1").arg(QCoreApplication::applicationPid()));
        // Every tune on a model with no radio logs "TX frequency NOT pushed", the
        // harness turns its heartbeat off on purpose, and a window with no radio
        // has no way to apply a radio's name, and a test TX channel is never opened. The control log is per message.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false\n"
                                                        "nereus.*.info=false\n"
                                                        "nereussdr.*.info=false\n"
                                                        "nereus.connection.warning=false\n"
                                                        "nereus.dsp.warning=false\n"
                                                        "nereus.station.warning=false\n"
                                                        "nereus.stationclient.warning=false"));
    }

    void init()
    {
        // Each case starts with no saved stack and no band memory.
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void theVerbsAreDeclaredWithTheirCapability()
    {
        const QSet<QByteArray> expected{
            "bandStack.press", "bandStack.step", "bandStack.goTo", "bandStack.add",
            "bandStack.saveHere", "bandStack.edit", "bandStack.delete", "bandStack.reorder",
            "bandStack.setReturn", "bandStack.clear", "bandStack.reseed", "bandStack.options",
            "bandStack.restoreExpired"};
        QSet<QByteArray> found;
        for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
            if (spec.verb.startsWith("bandStack.")) {
                found.insert(spec.verb);
                QCOMPARE(spec.capability, QByteArrayLiteral("bandStackVersion"));
                QCOMPARE(spec.capabilityVersion, 1);
                QCOMPARE(int(spec.minMinor), int(kRadioIdentitySessionProtocolMinor));
            }
        }
        QCOMPARE(found, expected);
    }

    void theCapabilityRoundTripsBeforeCoreBuildInfo()
    {
        StationCapabilities caps;
        caps.nnrLoadWarningVersion = 1;
        caps.bandStackVersion = 1;
        caps.coreBuildInfo = CoreBuildInfo{QStringLiteral("2026.11.0"), QStringLiteral("v2026.11.0")};
        const QList<MirrorUpdate> updates = caps.toUpdates();
        QVERIFY(updates.size() >= 3);
        QCOMPARE(updates.at(updates.size() - 1).name, QByteArrayLiteral("coreBuildInfo"));
        QCOMPARE(updates.at(updates.size() - 2).name, QByteArrayLiteral("bandStackVersion"));
        QCOMPARE(updates.at(updates.size() - 3).name, QByteArrayLiteral("nnrLoadWarningVersion"));
        const StationCapabilities decoded = StationCapabilities::fromUpdates(updates);
        QCOMPARE(decoded.bandStackVersion, 1);
        // It did not fall into the minor-11 block's catch-all.
        QCOMPARE(decoded.bandSelectVersion, 0);

        caps.bandStackVersion = 0;
        for (const MirrorUpdate& update : caps.toUpdates()) {
            QVERIFY(update.name != "bandStackVersion");
        }
        const MirrorUpdate wrongKind = text("bandStackVersion", QStringLiteral("1"));
        QCOMPARE(StationCapabilities::fromUpdates({wrongKind}).bandStackVersion, 0);
    }

    void thePeerThatDidNotDeclareItGetsNothing()
    {
        Core core;
        startStack(core);
        Device older(QStringLiteral("Older iPhone"), QStringLiteral("phone"));
        core.pair(older);
        LoopbackTransport* app = core.signIn(older, kHolder);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("bandStackVersion")),
                 std::optional<qint64>());
        QVERIFY(latest(app->received(), QStringLiteral("bandStack"), QStringLiteral("json")).isNull());
        for (const QJsonObject& schema : ofType(app->received(), QStringLiteral("schema"))) {
            QVERIFY(schema.value(QStringLiteral("class")).toString() != QLatin1String("BandStackFacade"));
        }

        const quint32 before = core.model->bandStackFacade()->revision();
        for (const QByteArray& verb :
             {QByteArrayLiteral("bandStack.reseed"), QByteArrayLiteral("bandStack.clear")}) {
            QCOMPARE(reasonOf(core.invoke(app, verb)), kUpdateApp);
        }
        QCOMPARE(reasonOf(core.invoke(app, "bandStack.press",
                                      {i64("sliceId", 0), text("band", QStringLiteral("40m")),
                                       text("direction", QStringLiteral("next"))})),
                 kUpdateApp);
        QCOMPARE(core.model->bandStackFacade()->revision(), before);
        // The verb an older client uses is still there.
        QCOMPARE(refusalOf(core.invoke(app, "slice.selectBand",
                                       {i64("sliceId", 0),
                                        i64("band", static_cast<int>(Band::Band40m))})),
                 QString());
    }

    void aPeerBelowTheMinorGetsNothingEvenIfItDeclares()
    {
        Core core(/*upgradedWithToken=*/true);
        startStack(core);
        LoopbackTransport* app = core.open(QStringLiteral("192.0.2.21"));
        Core::send(app, SessionMessages::authRequest(core.server->token()),
                   kRadioIdentitySessionProtocolMinor - 1, {{"bandStack", 1}});
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("bandStackVersion")),
                 std::optional<qint64>());
        QVERIFY(latest(app->received(), QStringLiteral("bandStack"), QStringLiteral("json")).isNull());
        QCOMPARE(reasonOf(core.invoke(app, "bandStack.reseed")), kUpdateApp);
    }

    void thePeerThatDeclaredItGetsTheDocumentAndUpdates()
    {
        Core core;
        startStack(core);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kStack);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("bandStackVersion")),
                 std::optional<qint64>(1));

        const QJsonObject document = mirrored(app);
        // Design section 6.2.
        QCOMPARE(document.value(QStringLiteral("schema")).toInt(), 1);
        QCOMPARE(document.value(QStringLiteral("region")).toString(), QStringLiteral("United States"));
        const QJsonObject options = document.value(QStringLiteral("options")).toObject();
        QCOMPARE(options.value(QStringLiteral("dupeProtection")).toBool(), true);
        QCOMPARE(options.value(QStringLiteral("expiryMinutes")).toInt(-1), 0);
        QCOMPARE(options.value(QStringLiteral("dwellSeconds")).toInt(-1), 0);
        QVERIFY(!document.value(QStringLiteral("entries")).toArray().isEmpty());
        QVERIFY(!orderOn(document, QStringLiteral("40m")).isEmpty());
        QVERIFY(document.value(QStringLiteral("places")).isObject());
        const QJsonObject expiry = document.value(QStringLiteral("lastExpiry")).toObject();
        QCOMPARE(expiry.value(QStringLiteral("batchId")).toString(), QString());
        QCOMPARE(expiry.value(QStringLiteral("count")).toInt(-1), 0);
        const QJsonObject recall = document.value(QStringLiteral("lastRecall")).toObject();
        QCOMPARE(recall.value(QStringLiteral("seq")).toInt(-1), 0);
        QCOMPARE(recall.value(QStringLiteral("sliceId")).toInt(), -1);
        QCOMPARE(recall.value(QStringLiteral("applyView")).toBool(true), false);
        QCOMPARE(recall.value(QStringLiteral("centerHz")).toInt(-1), 0);
        QCOMPARE(recall.value(QStringLiteral("spanHz")).toInt(-1), 0);
        const QJsonObject first = document.value(QStringLiteral("entries")).toArray().first().toObject();
        for (const char* key : {"id", "band", "frequencyHz", "mode", "filterLowHz", "filterHighHz",
                                "centerHz", "spanHz", "locked", "seeded", "name", "lastUsedMs"}) {
            QVERIFY2(first.contains(QLatin1String(key)), key);
        }

        // An update reaches it.
        QCOMPARE(refusalOf(core.invoke(app, "bandStack.options", {i64("dwellSeconds", 30)})), QString());
        QTRY_COMPARE(mirrored(app).value(QStringLiteral("options")).toObject()
                         .value(QStringLiteral("dwellSeconds")).toInt(), 30);
    }

    void theRevisionMovesOnlyWhenTheDocumentChanges()
    {
        Core core;
        startStack(core);
        BandStackFacade* facade = core.model->bandStackFacade();
        BandStackController* controller = core.model->bandStackController();
        QVERIFY(controller != nullptr);
        QVERIFY(!facade->json().isEmpty());
        QCOMPARE(facade->json(), BandStackFacade::jsonFor(*controller));

        const quint32 start = facade->revision();
        QSignalSpy changed(facade, &BandStackFacade::changed);
        QString refusal;
        // The same value again: the controller says it changed, the document did not.
        const int dwell = controller->store().options().dwellSeconds;
        QVERIFY(controller->options(std::nullopt, std::nullopt, dwell, &refusal));
        QCOMPARE(facade->revision(), start);
        QCOMPARE(changed.count(), 0);
        facade->refresh();
        QCOMPARE(facade->revision(), start);

        QVERIFY(controller->options(std::nullopt, std::nullopt, 30, &refusal));
        QCOMPARE(facade->revision(), start + 1);
        QCOMPARE(changed.count(), 1);
        QVERIFY(controller->options(std::nullopt, std::nullopt, 30, &refusal));
        QCOMPARE(facade->revision(), start + 1);
    }

    void withNoRadioTheVerbsAreRefusedAndTheDocumentIsEmpty()
    {
        Core core;
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kStack);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("bandStackVersion")),
                 std::optional<qint64>(1));
        QCOMPARE(core.model->bandStackFacade()->json(), QString());
        QCOMPARE(reasonOf(core.invoke(app, "bandStack.reseed")), kNoRadio);
        // A radio arrives: the document follows, on the same session.
        startStack(core);
        QTRY_VERIFY(!mirrored(app).isEmpty());
    }

    void eachVerbChangesTheDocument()
    {
        Core core;
        startStack(core);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kStack);
        QVERIFY(admitted(app));
        SliceModel* slice = core.model->sliceById(0);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14123000.0);

        const auto ok = [&core, app](const QByteArray& verb, const QList<MirrorUpdate>& arguments) {
            return refusalOf(core.invoke(app, verb, arguments));
        };
        const auto mirrorCaughtUp = [&core, app]() { return mirrored(app) == coreDocument(core); };

        // add: the current spot with the device's view, and the slice's place.
        const QStringList before20 = orderOn(coreDocument(core), QStringLiteral("20m"));
        QCOMPARE(ok("bandStack.add",
                    {i64("sliceId", 0), i64("viewCenterHz", 14100000), i64("viewSpanHz", 96000)}),
                 QString());
        QJsonObject document = coreDocument(core);
        QCOMPARE(orderOn(document, QStringLiteral("20m")).size(), before20.size() + 1);
        const QString added = placeOf(document, QStringLiteral("A"), QStringLiteral("20m"));
        QCOMPARE(entryIn(document, added).value(QStringLiteral("frequencyHz")).toInteger(), 14123000);
        QCOMPARE(entryIn(document, added).value(QStringLiteral("centerHz")).toInteger(), 14100000);
        QCOMPARE(entryIn(document, added).value(QStringLiteral("spanHz")).toInteger(), 96000);
        QTRY_VERIFY(mirrorCaughtUp());
        // The same spot again is the store's refusal, whole.
        const quint32 revision = core.model->bandStackFacade()->revision();
        QCOMPARE(ok("bandStack.add", {i64("sliceId", 0)}),
                 QStringLiteral("14.123 is already in 20m"));
        QCOMPARE(core.model->bandStackFacade()->revision(), revision);

        // press: a band's stack, with the result naming both objects.
        const QJsonObject press = core.invoke(app, "bandStack.press",
            {i64("sliceId", 0), text("band", QStringLiteral("40m")),
             text("direction", QStringLiteral("next")), i64("viewCenterHz", 14100000),
             i64("viewSpanHz", 96000)});
        QCOMPARE(refusalOf(press), QString());
        const QJsonArray affected = press.value(QStringLiteral("affected")).toArray();
        QVERIFY(affected.contains(QJsonValue(QStringLiteral("bandStack"))));
        QVERIFY(affected.contains(QJsonValue(QStringLiteral("slice:0"))));
        document = coreDocument(core);
        const QString on40 = placeOf(document, QStringLiteral("A"), QStringLiteral("40m"));
        QVERIFY(!on40.isEmpty());
        QCOMPARE(qRound64(slice->frequency()),
                 entryIn(document, on40).value(QStringLiteral("frequencyHz")).toInteger());
        const QJsonObject recall = document.value(QStringLiteral("lastRecall")).toObject();
        QCOMPARE(recall.value(QStringLiteral("seq")).toInt(), 1);
        QCOMPARE(recall.value(QStringLiteral("sliceId")).toInt(), 0);

        // step: on, within the band.
        QCOMPARE(ok("bandStack.step", {i64("sliceId", 0), text("direction", QStringLiteral("next"))}),
                 QString());
        document = coreDocument(core);
        QVERIFY(placeOf(document, QStringLiteral("A"), QStringLiteral("40m")) != on40);
        QCOMPARE(document.value(QStringLiteral("lastRecall")).toObject()
                     .value(QStringLiteral("seq")).toInt(), 2);

        // goTo: a row, on any band.
        QCOMPARE(ok("bandStack.goTo", {i64("sliceId", 0), text("entryId", added)}), QString());
        QCOMPARE(qRound64(slice->frequency()), qint64(14123000));
        QCOMPARE(placeOf(coreDocument(core), QStringLiteral("A"), QStringLiteral("20m")), added);

        // press accepts what slice.selectBand does not: GEN and a shortwave band.
        const QString shortwave = bandKeyName(Band::Band31m);
        const QString general = bandKeyName(Band::GEN);
        QVERIFY(!orderOn(coreDocument(core), general).isEmpty());
        QCOMPARE(ok("bandStack.press", {i64("sliceId", 0), text("band", general),
                                        text("direction", QStringLiteral("next"))}), QString());
        QVERIFY(!placeOf(coreDocument(core), QStringLiteral("A"), general).isEmpty());
        QCOMPARE(ok("bandStack.press", {i64("sliceId", 0), text("band", shortwave),
                                        text("direction", QStringLiteral("next"))}), QString());
        QCOMPARE(ok("bandStack.goTo", {i64("sliceId", 0), text("entryId", added)}), QString());

        // saveHere: the entry takes where the slice is.
        slice->setFrequency(14124000.0);
        QCOMPARE(ok("bandStack.saveHere", {i64("sliceId", 0), text("entryId", added)}), QString());
        QCOMPARE(entryIn(coreDocument(core), added).value(QStringLiteral("frequencyHz")).toInteger(),
                 14124000);

        // edit: either field, or both.
        QCOMPARE(ok("bandStack.edit", {text("entryId", added), text("name", QStringLiteral("Net"))}),
                 QString());
        QCOMPARE(entryIn(coreDocument(core), added).value(QStringLiteral("name")).toString(),
                 QStringLiteral("Net"));
        QCOMPARE(ok("bandStack.edit", {text("entryId", added), flag("locked", true)}), QString());
        QVERIFY(entryIn(coreDocument(core), added).value(QStringLiteral("locked")).toBool());
        QCOMPARE(ok("bandStack.saveHere", {i64("sliceId", 0), text("entryId", added)}),
                 QStringLiteral("That entry is locked. Unlock it to save here."));
        QCOMPARE(ok("bandStack.edit", {text("entryId", added), flag("locked", false)}), QString());

        // reorder: exactly the band's entries.
        const QString band40 = QStringLiteral("40m");
        QStringList reversed = orderOn(coreDocument(core), band40);
        QVERIFY(reversed.size() >= 2);
        std::reverse(reversed.begin(), reversed.end());
        const auto idsText = [](const QStringList& ids) {
            return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(ids))
                                         .toJson(QJsonDocument::Compact));
        };
        QCOMPARE(ok("bandStack.reorder", {text("band", band40), text("ids", idsText(reversed))}),
                 QString());
        QCOMPARE(orderOn(coreDocument(core), band40), reversed);

        // setReturn: the modes as the document names them.
        const auto returnOf = [&core, &band40](const char* key) {
            return coreDocument(core).value(QStringLiteral("bands")).toObject().value(band40).toObject()
                .value(QLatin1String(key)).toString();
        };
        QCOMPARE(ok("bandStack.setReturn", {text("band", band40), text("mode", QStringLiteral("lastUsed"))}),
                 QString());
        QCOMPARE(returnOf("returnMode"), QStringLiteral("lastUsed"));
        QCOMPARE(ok("bandStack.setReturn", {text("band", band40), text("mode", QStringLiteral("specific")),
                                            text("entryId", reversed.first())}), QString());
        QCOMPARE(returnOf("returnMode"), QStringLiteral("specific"));
        QCOMPARE(returnOf("specificId"), reversed.first());
        QCOMPARE(ok("bandStack.setReturn", {text("band", band40), text("mode", QStringLiteral("current"))}),
                 QString());
        QCOMPARE(returnOf("returnMode"), QStringLiteral("current"));

        // options: any of the three.
        QCOMPARE(ok("bandStack.options", {flag("dupeProtection", false), i64("expiryMinutes", 15),
                                          i64("dwellSeconds", 60)}), QString());
        const QJsonObject options = coreDocument(core).value(QStringLiteral("options")).toObject();
        QCOMPARE(options.value(QStringLiteral("dupeProtection")).toBool(true), false);
        QCOMPARE(options.value(QStringLiteral("expiryMinutes")).toInt(), 15);
        QCOMPARE(options.value(QStringLiteral("dwellSeconds")).toInt(), 60);
        QCOMPARE(ok("bandStack.options", {i64("expiryMinutes", 0), i64("dwellSeconds", 0)}), QString());

        // delete, then the same again: a device behind the mirror is refused.
        const QString victim = orderOn(coreDocument(core), band40).last();
        QVERIFY(placeOf(coreDocument(core), QStringLiteral("A"), band40) != victim);
        QCOMPARE(ok("bandStack.delete", {text("entryId", victim)}), QString());
        QVERIFY(entryIn(coreDocument(core), victim).isEmpty());
        QCOMPARE(ok("bandStack.delete", {text("entryId", victim)}), kNoEntry);

        // clear: the unlocked entries on one band, then everywhere.
        QCOMPARE(ok("bandStack.clear", {text("band", band40)}), QString());
        QVERIFY(!orderOn(coreDocument(core), QStringLiteral("80m")).isEmpty());
        for (const QString& id : orderOn(coreDocument(core), band40)) {
            QVERIFY(entryIn(coreDocument(core), id).value(QStringLiteral("locked")).toBool());
        }

        // reseed: the region's seeds again. No slice moves.
        const double held = slice->frequency();
        QCOMPARE(ok("bandStack.reseed", {}), QString());
        QVERIFY(orderOn(coreDocument(core), band40).size() >= 2);
        QCOMPARE(slice->frequency(), held);

        QCOMPARE(ok("bandStack.clear", {}), QString());
        QVERIFY(orderOn(coreDocument(core), band40).size() < reversed.size());
        QTRY_VERIFY(mirrorCaughtUp());
    }

    void restoreExpiredBringsTheBatchBackOnceWithinTheWindow()
    {
        Core core;
        startStack(core);
        BandStackController* controller = core.model->bandStackController();
        qint64 clock = 1000000;
        controller->setClockForTest([&clock]() { return clock; });
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kStack);
        QVERIFY(admitted(app));
        SliceModel* slice = core.model->sliceById(0);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14123000.0);

        const auto ok = [&core, app](const QByteArray& verb, const QList<MirrorUpdate>& arguments) {
            return refusalOf(core.invoke(app, verb, arguments));
        };
        QCOMPARE(ok("bandStack.add", {i64("sliceId", 0)}), QString());
        const QString added = placeOf(coreDocument(core), QStringLiteral("A"), QStringLiteral("20m"));
        // Off that spot, so it is no slice's place and can expire.
        QCOMPARE(ok("bandStack.step", {i64("sliceId", 0), text("direction", QStringLiteral("next"))}),
                 QString());
        QVERIFY(placeOf(coreDocument(core), QStringLiteral("A"), QStringLiteral("20m")) != added);
        QCOMPARE(ok("bandStack.options", {i64("expiryMinutes", 5)}), QString());

        clock += 6 * 60 * 1000;
        controller->runExpiryForTest();
        QJsonObject document = coreDocument(core);
        QVERIFY(entryIn(document, added).isEmpty());
        const QJsonObject expiry = document.value(QStringLiteral("lastExpiry")).toObject();
        const QString batch = expiry.value(QStringLiteral("batchId")).toString();
        QVERIFY(!batch.isEmpty());
        QVERIFY(expiry.value(QStringLiteral("count")).toInt() >= 1);
        QTRY_COMPARE(mirrored(app).value(QStringLiteral("lastExpiry")).toObject()
                         .value(QStringLiteral("batchId")).toString(), batch);

        const QString tooLate = QStringLiteral("Those entries can no longer be brought back.");
        QCOMPARE(ok("bandStack.restoreExpired", {text("batchId", QStringLiteral("someone-else"))}), tooLate);
        QVERIFY(entryIn(coreDocument(core), added).isEmpty());
        QCOMPARE(ok("bandStack.restoreExpired", {text("batchId", batch)}), QString());
        document = coreDocument(core);
        QVERIFY(!entryIn(document, added).isEmpty());
        QCOMPARE(document.value(QStringLiteral("lastExpiry")).toObject()
                     .value(QStringLiteral("batchId")).toString(), QString());
        QCOMPARE(ok("bandStack.restoreExpired", {text("batchId", batch)}), tooLate);

        // Past the 5 second window.
        clock += 6 * 60 * 1000;
        controller->runExpiryForTest();
        const QString second = coreDocument(core).value(QStringLiteral("lastExpiry")).toObject()
                                   .value(QStringLiteral("batchId")).toString();
        QVERIFY(!second.isEmpty());
        clock += 5001;
        QCOMPARE(ok("bandStack.restoreExpired", {text("batchId", second)}), tooLate);
        QVERIFY(entryIn(coreDocument(core), added).isEmpty());
        controller->setClockForTest({});
    }

    void badArgumentsAreRefusedWithNoChange_data()
    {
        QTest::addColumn<QByteArray>("verb");
        QTest::addColumn<QList<MirrorUpdate>>("arguments");
        QTest::addColumn<QString>("reason");
        const MirrorUpdate slice = i64("sliceId", 0);
        const MirrorUpdate band = text("band", QStringLiteral("40m"));
        const MirrorUpdate next = text("direction", QStringLiteral("next"));
        const MirrorUpdate unknown = text("entryId", QStringLiteral("no-such-entry"));
        const auto row = [](const char* name, const QByteArray& verb, const QList<MirrorUpdate>& args,
                            const QString& reason) {
            QTest::newRow(name) << verb << args << reason;
        };
        row("press no direction", "bandStack.press", {slice, band}, kNotUnderstood);
        row("press no band", "bandStack.press", {slice, next}, kNotUnderstood);
        row("press no slice", "bandStack.press", {band, next}, kNotUnderstood);
        row("press unknown band", "bandStack.press",
            {slice, text("band", QStringLiteral("41 m")), next}, kNotUnderstood);
        row("press band as a number", "bandStack.press", {slice, i64("band", 3), next}, kNotUnderstood);
        row("press sideways", "bandStack.press",
            {slice, band, text("direction", QStringLiteral("sideways"))}, kNotUnderstood);
        row("press slice as text", "bandStack.press",
            {text("sliceId", QStringLiteral("0")), band, next}, kNotUnderstood);
        row("press huge slice", "bandStack.press",
            {i64("sliceId", 4294967296), band, next}, kNotUnderstood);
        row("press negative span", "bandStack.press",
            {slice, band, next, i64("viewSpanHz", -1)}, kNotUnderstood);
        row("press negative center", "bandStack.press",
            {slice, band, next, i64("viewCenterHz", -1)}, kNotUnderstood);
        row("press extra argument", "bandStack.press",
            {slice, band, next, flag("conformanceWrongName", true)}, kNotUnderstood);
        row("press twice the band", "bandStack.press", {slice, band, band, next}, kNotUnderstood);
        row("press no such slice", "bandStack.press", {i64("sliceId", 3), band, next},
            QStringLiteral("That slice is not open."));
        row("step no direction", "bandStack.step", {slice}, kNotUnderstood);
        row("goTo no entry", "bandStack.goTo", {slice}, kNotUnderstood);
        row("goTo empty entry", "bandStack.goTo", {slice, text("entryId", QString())}, kNotUnderstood);
        row("goTo unknown entry", "bandStack.goTo", {slice, unknown}, kNoEntry);
        row("add no slice", "bandStack.add", {}, kNotUnderstood);
        row("add no such slice", "bandStack.add", {i64("sliceId", 3)},
            QStringLiteral("That slice is not open."));
        row("saveHere no entry", "bandStack.saveHere", {slice}, kNotUnderstood);
        row("saveHere unknown entry", "bandStack.saveHere", {slice, unknown}, kNoEntry);
        row("edit nothing", "bandStack.edit", {text("entryId", QStringLiteral("x"))}, kNotUnderstood);
        row("edit no entry", "bandStack.edit", {text("name", QStringLiteral("x"))}, kNotUnderstood);
        row("edit unknown entry", "bandStack.edit", {unknown, text("name", QStringLiteral("x"))}, kNoEntry);
        row("delete no entry", "bandStack.delete", {}, kNotUnderstood);
        row("delete unknown entry", "bandStack.delete", {unknown}, kNoEntry);
        row("reorder no ids", "bandStack.reorder", {band}, kNotUnderstood);
        row("reorder ids not a list", "bandStack.reorder",
            {band, text("ids", QStringLiteral("a,b"))}, kNotUnderstood);
        row("reorder ids not text", "bandStack.reorder",
            {band, text("ids", QStringLiteral("[1,2]"))}, kNotUnderstood);
        row("reorder unknown band", "bandStack.reorder",
            {text("band", QStringLiteral("nope")), text("ids", QStringLiteral("[]"))}, kNotUnderstood);
        row("reorder wrong list", "bandStack.reorder", {band, text("ids", QStringLiteral("[\"x\"])"))},
            kNotUnderstood);
        row("reorder list of the wrong ids", "bandStack.reorder", {band, text("ids", QStringLiteral("[\"x\"]"))},
            QStringLiteral("That list does not match the entries on 40m."));
        row("setReturn no mode", "bandStack.setReturn", {band}, kNotUnderstood);
        row("setReturn unknown mode", "bandStack.setReturn",
            {band, text("mode", QStringLiteral("elsewhere"))}, kNotUnderstood);
        row("setReturn specific with no entry", "bandStack.setReturn",
            {band, text("mode", QStringLiteral("specific"))}, kNoEntry);
        row("setReturn specific unknown", "bandStack.setReturn",
            {band, text("mode", QStringLiteral("specific")), unknown}, kNoEntry);
        row("clear unknown band", "bandStack.clear", {text("band", QStringLiteral("nope"))}, kNotUnderstood);
        row("reseed with an argument", "bandStack.reseed", {flag("now", true)}, kNotUnderstood);
        row("options wrong kind", "bandStack.options", {flag("expiryMinutes", true)}, kNotUnderstood);
        row("options huge", "bandStack.options", {i64("expiryMinutes", 4294967296)}, kNotUnderstood);
        row("options expiry not offered", "bandStack.options", {i64("expiryMinutes", 7)},
            QStringLiteral("Auto-expiry can be off, or 5, 15, 30 or 60 minutes."));
        row("options dwell not offered", "bandStack.options", {i64("dwellSeconds", 7)},
            QStringLiteral("Auto-save can be off, or 10, 30 or 60 seconds."));
        row("restoreExpired no batch", "bandStack.restoreExpired", {}, kNotUnderstood);
    }

    void badArgumentsAreRefusedWithNoChange()
    {
        QFETCH(QByteArray, verb);
        QFETCH(QList<MirrorUpdate>, arguments);
        QFETCH(QString, reason);
        Core core;
        startStack(core);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kStack);
        QVERIFY(admitted(app));
        SliceModel* slice = core.model->sliceById(0);
        const double frequency = slice->frequency();
        const QString before = core.model->bandStackFacade()->json();
        const quint32 revision = core.model->bandStackFacade()->revision();

        const QJsonObject result = core.invoke(app, verb, arguments);
        QCOMPARE(result.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(reasonOf(result), reason);
        QVERIFY(OperatorWording::isPlain(reason));
        QCOMPARE(result.value(QStringLiteral("verb")).toString(), QString::fromLatin1(verb));

        QCOMPARE(core.model->bandStackFacade()->json(), before);
        QCOMPARE(core.model->bandStackFacade()->revision(), revision);
        QCOMPARE(slice->frequency(), frequency);
    }

    void aListenerAndAnotherDeviceAreRefusedOnTheSlice()
    {
        Core core;
        startStack(core);
        Device owner;
        Device listener(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device other(QStringLiteral("Other iPhone"), QStringLiteral("phone"));
        core.pair(owner);
        core.pair(listener);
        core.pair(other);
        LoopbackTransport* appOwner = core.signIn(owner, kSharesStack);
        LoopbackTransport* appListener = core.signIn(listener, kSharesStack);
        LoopbackTransport* appOther = core.signIn(other, kStack);
        QVERIFY(admitted(appOwner) && admitted(appListener) && admitted(appOther));
        QTRY_VERIFY(!latest(appListener->received(), QStringLiteral("access:0"),
                            QStringLiteral("incarnation")).isNull());
        const qint64 incarnation =
            latest(appListener->received(), QStringLiteral("access:0"), QStringLiteral("incarnation"))
                .toInteger();
        QCOMPARE(refusalOf(core.invoke(appListener, "slice.listen",
                                       {i64("sliceId", 0), i64("incarnation", incarnation)})),
                 QString());
        QCOMPARE(core.model->sliceOwnership()->listenersOf(0).size(), 2);

        SliceModel* slice = core.model->sliceById(0);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14123000.0);
        const double frequency = slice->frequency();
        const QString before = core.model->bandStackFacade()->json();
        const QString entry = orderOn(coreDocument(core), QStringLiteral("40m")).first();

        const QString listening = QStringLiteral(
            "You are only listening to Slice A. Take control of it to change its band.");
        const QString elsewhere =
            QStringLiteral("That slice belongs to iPhone. It can be changed only there.");
        QVERIFY(OperatorWording::isPlain(listening));

        const QList<QPair<QByteArray, QList<MirrorUpdate>>> sliceVerbs{
            {"bandStack.press", {i64("sliceId", 0), text("band", QStringLiteral("40m")),
                                 text("direction", QStringLiteral("next"))}},
            {"bandStack.step", {i64("sliceId", 0), text("direction", QStringLiteral("next"))}},
            {"bandStack.goTo", {i64("sliceId", 0), text("entryId", entry)}},
            {"bandStack.add", {i64("sliceId", 0)}},
            {"bandStack.saveHere", {i64("sliceId", 0), text("entryId", entry)}}};
        for (const auto& verb : sliceVerbs) {
            QCOMPARE(reasonOf(core.invoke(appListener, verb.first, verb.second)), listening);
            QCOMPARE(reasonOf(core.invoke(appOther, verb.first, verb.second)), elsewhere);
        }
        QCOMPARE(slice->frequency(), frequency);
        QCOMPARE(core.model->bandStackFacade()->json(), before);

        // Verbs that name no slice are anyone's.
        QCOMPARE(refusalOf(core.invoke(appListener, "bandStack.options", {i64("dwellSeconds", 10)})),
                 QString());
        // The slice's own controller is not refused.
        QCOMPARE(refusalOf(core.invoke(appOwner, "bandStack.press", sliceVerbs.first().second)), QString());
        QVERIFY(slice->frequency() != frequency);
    }

    void aLockedSliceIsRefusedInTheDesktopsWords()
    {
        Core core;
        startStack(core);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kStack);
        QVERIFY(admitted(app));
        SliceModel* slice = core.model->sliceById(0);
        slice->setLocked(true);
        const double frequency = slice->frequency();
        QCOMPARE(reasonOf(core.invoke(app, "bandStack.press",
                                      {i64("sliceId", 0), text("band", QStringLiteral("40m")),
                                       text("direction", QStringLiteral("next"))})),
                 QStringLiteral("Band 40m ignored: the slice is locked. Unlock it to change bands."));
        QCOMPARE(slice->frequency(), frequency);
    }

    void aRecallWaitsWhileTheCoresOwnKeyIsOnTheAir()
    {
        Core core;
        TxChannel txChannel(/*channelId=*/1);
        core.model->wireTransmitChainForTest(&txChannel);
        const auto unwire = qScopeGuard([&core] { core.model->injectTxChannelForTest(nullptr); });
        startStack(core);
        Device phone;
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone, kStack);
        QVERIFY(admitted(app));
        SliceModel* slice = core.model->sliceById(0);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14123000.0);
        MoxController* const mox = core.model->moxController();
        mox->setMoxCheck({});
        mox->setMox(true); // logical test state, no radio transport
        QVERIFY(core.model->stationOnAirRefusal(nullptr));
        const QString revisionBefore = core.model->bandStackFacade()->json();

        const QString words = QStringLiteral("The radio is on the air. Try again when it stops.");
        const QString entry = orderOn(coreDocument(core), QStringLiteral("40m")).first();
        const MirrorUpdate sliceId = i64("sliceId", 0);
        QCOMPARE(reasonOf(core.invoke(app, "bandStack.press",
                                      {sliceId, text("band", QStringLiteral("40m")),
                                       text("direction", QStringLiteral("next"))})), words);
        QCOMPARE(reasonOf(core.invoke(app, "bandStack.step",
                                      {sliceId, text("direction", QStringLiteral("next"))})), words);
        QCOMPARE(reasonOf(core.invoke(app, "bandStack.goTo", {sliceId, text("entryId", entry)})), words);
        QCOMPARE(qRound64(slice->frequency()), qint64(14123000));
        QCOMPARE(core.model->bandStackFacade()->json(), revisionBefore);
        mox->setMox(false);
        QTRY_VERIFY(!core.model->stationOnAirRefusal(nullptr));
    }

    void aRemoteWindowMirrorsTheStackAndSendsVerbs()
    {
        Core core(/*upgradedWithToken=*/true);
        startStack(core);
        RadioModel window{RadioModel::Role::Remote};
        SettingsProxy proxy;
        StationClient client(&window, &proxy);
        // No controller in a remote window: its stack is the Core's.
        QVERIFY(window.bandStackController() == nullptr);
        QVERIFY(window.bandStackFacade() != nullptr);
        QVERIFY(!client.bandStackAvailable());
        QCOMPARE(client.sendBandStackCommand(QStringLiteral("bandStack.reseed"), {}).reason,
                 QStringLiteral("This Core does not have band stacks yet. Updating the Core adds them."));

        auto* coreEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
        auto* windowEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
        coreEnd->linkTo(windowEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(windowEnd, core.server->token());
        core.server->acceptTransport(coreEnd);
        QVERIFY(completed.wait(5000) || completed.count() == 1);

        QTRY_VERIFY(client.bandStackAvailable());
        BandStackFacade* facade = window.bandStackFacade();
        QTRY_COMPARE(facade->json(), core.model->bandStackFacade()->json());
        QCOMPARE(facade->revision(), core.model->bandStackFacade()->revision());

        // A verb goes out and the mirror comes back.
        QSignalSpy changed(facade, &BandStackFacade::changed);
        IStationLink::CommandOutcome outcome = client.sendBandStackCommand(
            QStringLiteral("bandStack.options"),
            QJsonObject{{QStringLiteral("dupeProtection"), false},
                        {QStringLiteral("expiryMinutes"), 15}});
        QVERIFY2(outcome.sent, qPrintable(outcome.reason));
        QTRY_COMPARE(parse(facade->json()).value(QStringLiteral("options")).toObject()
                         .value(QStringLiteral("expiryMinutes")).toInt(), 15);
        QVERIFY(changed.count() >= 1);
        QTRY_COMPARE(facade->revision(), core.model->bandStackFacade()->revision());

        // An array goes as the text the verb takes.
        QStringList reversed = orderOn(coreDocument(core), QStringLiteral("40m"));
        std::reverse(reversed.begin(), reversed.end());
        outcome = client.sendBandStackCommand(
            QStringLiteral("bandStack.reorder"),
            QJsonObject{{QStringLiteral("band"), QStringLiteral("40m")},
                        {QStringLiteral("ids"), QJsonArray::fromStringList(reversed)}});
        QVERIFY2(outcome.sent, qPrintable(outcome.reason));
        QTRY_COMPARE(orderOn(parse(facade->json()), QStringLiteral("40m")), reversed);

        // A press: numbers go as whole numbers, and the slice moves on the Core.
        SliceModel* slice = core.model->sliceById(0);
        const double before = slice->frequency();
        outcome = client.sendBandStackCommand(
            QStringLiteral("bandStack.press"),
            QJsonObject{{QStringLiteral("sliceId"), 0},
                        {QStringLiteral("band"), QStringLiteral("17m")},
                        {QStringLiteral("direction"), QStringLiteral("next")},
                        {QStringLiteral("viewCenterHz"), 14200000},
                        {QStringLiteral("viewSpanHz"), 96000}});
        QVERIFY2(outcome.sent, qPrintable(outcome.reason));
        QTRY_VERIFY(slice->frequency() != before);
        QTRY_COMPARE(parse(facade->json()).value(QStringLiteral("lastRecall")).toObject()
                         .value(QStringLiteral("seq")).toInt(), 1);

        // Not this link's verb, and a value with no wire form, do not leave.
        QVERIFY(!client.sendBandStackCommand(QStringLiteral("slice.selectBand"), {}).sent);
        QVERIFY(!client.sendBandStackCommand(QStringLiteral("bandStack.edit"),
                                             QJsonObject{{QStringLiteral("name"), QJsonObject{}}}).sent);

        // The session ending leaves no copy.
        client.disconnectFromStation(QStringLiteral("test over"));
        QTRY_COMPARE(facade->json(), QString());
        QCOMPARE(facade->revision(), quint32(0));
    }

    void aWindowThatWithholdsTheFeatureDoesNotHoldTheObject()
    {
        Core core(/*upgradedWithToken=*/true);
        startStack(core);
        RadioModel window{RadioModel::Role::Remote};
        SettingsProxy proxy;
        StationClient client(&window, &proxy);
        client.withholdFeatureForTest(QByteArrayLiteral("bandStack"));
        auto* coreEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
        auto* windowEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
        coreEnd->linkTo(windowEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(windowEnd, core.server->token());
        core.server->acceptTransport(coreEnd);
        QVERIFY(completed.wait(5000) || completed.count() == 1);
        QVERIFY(!client.bandStackAvailable());
        QCOMPARE(window.bandStackFacade()->json(), QString());
        client.disconnectFromStation(QStringLiteral("test over"));
    }
};


QTEST_MAIN(TstBandStackLink)
#include "tst_bandstack_link.moc"
