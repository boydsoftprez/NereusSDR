// no-port-check: NereusSDR-original test of the Setup description wire surface.
// 2026-09-30: Audio version 24 (radio codec lane): Line In Gain's 1.5 dB
// steps, Saturn Mic Tip-Ring, the Red Pitaya's Orion rows and the HL2's
// Hermes rows; the cap is 24. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
#include <QtTest>
#include <QRegularExpression>

#include "core/setup/SetupDescriptionService.h"
#include "core/setup/SetupDescriptionV15.h"
#include "core/settings/SettingsScope.h"
#include "core/BoardCapabilities.h"
#include "core/PaCalProfile.h"
#include "core/RadioInfoFacts.h"
#include "core/SampleRateCatalog.h"
#include "core/codec/AlexFilterMap.h"
#include "core/session/MirrorSchema.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/StationCapabilities.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationServer.h"
#include "core/AppSettings.h"
#include "core/settings/SettingsProxyServer.h"
#include "core/ConnectionState.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "models/NotchModel.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QSet>
#include <QTemporaryDir>

#include <memory>
#include <cmath>
#include <utility>

using namespace NereusSDR;

namespace {
// A category as a peer that declared `version` receives it.
QJsonObject projectedCategory(const QString& description, int version)
{
    return QJsonDocument::fromJson(
        SetupDescriptionService::fitCategoryForVersion(description, version).toUtf8()).object();
}

// Controls of a category (0: all; otherwise those needing `version`).
int controlCount(const QJsonObject& category, int version)
{
    int count = 0;
    for (const QJsonValue& page : category.value("pages").toArray()) {
        for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
            for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                if (version == 0
                    || raw.toObject().value("requiresDescriptionVersion") == QJsonValue(version)) {
                    ++count;
                }
            }
        }
    }
    return count;
}

QStringList pageIdsOf(const QJsonObject& category)
{
    QStringList ids;
    for (const QJsonValue& page : category.value("pages").toArray()) {
        ids << page.toObject().value("id").toString();
    }
    return ids;
}

QJsonObject controlById(const QJsonObject& category, const QString& id)
{
    for (const QJsonValue& page : category.value("pages").toArray()) {
        for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
            for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                if (raw.toObject().value("id") == QJsonValue(id)) { return raw.toObject(); }
            }
        }
    }
    return {};
}
}

namespace {
// A category without its rows that need `version`, empty sections and pages
// dropped, as an older peer's projection is built.
QJsonObject withoutRowsOf(const QJsonObject& category, int version)
{
    QJsonArray pages;
    for (const QJsonValue& rawPage : category.value("pages").toArray()) {
        QJsonObject page = rawPage.toObject();
        QJsonArray sections;
        for (const QJsonValue& rawSection : page.value("sections").toArray()) {
            QJsonObject section = rawSection.toObject();
            QJsonArray controls;
            for (const QJsonValue& control : section.value("controls").toArray()) {
                if (control.toObject().value("requiresDescriptionVersion") != QJsonValue(version)) {
                    controls.append(control);
                }
            }
            if (!controls.isEmpty()) {
                section.insert("controls", controls);
                sections.append(section);
            }
        }
        if (!sections.isEmpty()) {
            page.insert("sections", sections);
            pages.append(page);
        }
    }
    QJsonObject older = category;
    older.insert("pages", pages);
    return older;
}

QJsonObject pageById(const QJsonObject& category, const QString& id)
{
    for (const QJsonValue& page : category.value("pages").toArray()) {
        if (page.toObject().value("id") == QJsonValue(id)) { return page.toObject(); }
    }
    return {};
}

QJsonArray rowsOf(const QJsonObject& page)
{
    QJsonArray rows;
    for (const QJsonValue& section : page.value("sections").toArray()) {
        for (const QJsonValue& control : section.toObject().value("controls").toArray()) {
            rows.append(control);
        }
    }
    return rows;
}

// The resource's own rows that need `version` (the form the Core validates).
QList<QJsonObject> resourceRows(const QString& category, int version)
{
    QFile file(QStringLiteral(":/setup/%1.json").arg(category));
    if (!file.open(QIODevice::ReadOnly)) { return {}; }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QList<QJsonObject> rows;
    for (const QJsonValue& page : root.value("pages").toArray()) {
        for (const QJsonValue& row : rowsOf(page.toObject())) {
            if (row.toObject().value("requiresDescriptionVersion") == QJsonValue(version)) {
                rows.append(row.toObject());
            }
        }
    }
    return rows;
}

// Every single-field change of a closed row: each field removed, each
// value replaced, and one field added.
QList<QJsonObject> mutationsOf(const QJsonObject& row)
{
    QList<QJsonObject> changed;
    for (auto it = row.constBegin(); it != row.constEnd(); ++it) {
        QJsonObject removed = row;
        removed.remove(it.key());
        changed.append(removed);
        QJsonObject replaced = row;
        replaced.insert(it.key(), it.value().isString() ? QJsonValue(it.value().toString() + "x")
                                  : it.value().isBool() ? QJsonValue(!it.value().toBool())
                                  : it.value().isDouble() ? QJsonValue(it.value().toDouble() + 1)
                                                          : QJsonValue(QStringLiteral("other")));
        changed.append(replaced);
    }
    QJsonObject added = row;
    added.insert("write", true);
    changed.append(added);
    return changed;
}
}

namespace {
struct WireCore {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<StationServer> server;
    std::unique_ptr<Test::LoopbackTransport> app;

    explicit WireCore(HPSDRHW board = HPSDRHW::HermesLite)
    {
        settings = std::make_unique<AppSettings>(
            settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        model = std::make_unique<RadioModel>();
        model->setBoardForTest(board);
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:43");
        info.name = QStringLiteral("Setup test HL2");
        info.boardType = board;
        model->setLastRadioInfoForTest(info);
        model->setConnectionStateForTest(ConnectionState::Connected);
        model->addSlice(QStringLiteral("pan-0"));
        server = std::make_unique<StationServer>(
            model.get(), *settings, Test::seedUpgradedCoreToken(securityDir.path()));
    }

    bool connect(const QHash<QByteArray, int>& features = {},
                 quint16 minor = kSessionProtocolMinor)
    {
        app = std::make_unique<Test::LoopbackTransport>(QStringLiteral("app"));
        auto* station = new Test::LoopbackTransport(QStringLiteral("station"), server.get());
        station->linkTo(app.get());
        server->acceptTransport(station);
        if (!QTest::qWaitFor([this] { return !app->received().isEmpty(); }, 5000)) {
            return false;
        }
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, minor, 0, QStringLiteral("Setup test"),
            {kSessionProtocolMajor}, features)));
        app->sendText(SessionMessages::encode(SessionMessages::authRequest(server->token())));
        return QTest::qWaitFor([this] {
            return app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"));
        }, 5000);
    }
};

bool hasSetupTraffic(const Test::LoopbackTransport& app)
{
    for (const QByteArray& wire : app.received()) {
        const QJsonObject message = QJsonDocument::fromJson(wire).object();
        const QString type = message.value(QStringLiteral("type")).toString();
        if (type != QLatin1String("schema") && type != QLatin1String("object.create")
            && type != QLatin1String("delta")) {
            continue;
        }
        if (message.value(QStringLiteral("key")).toString() == QStringLiteral("setup")
            || wire.contains("SetupDescription")) {
            return true;
        }
    }
    return false;
}

QString setupCategoryOnWire(const Test::LoopbackTransport& app, const QByteArray& name,
                           SessionMessageKind kind)
{
    for (const QByteArray& wire : app.received()) {
        SessionMessage message;
        if (!SessionMessages::decode(wire, &message) || message.kind != kind
            || message.objectKey != "setup") { continue; }
        for (const MirrorUpdate& update : message.updates) {
            if (update.name == name) { return update.value.toString(); }
        }
    }
    return {};
}

int setupCapabilityOnWire(const Test::LoopbackTransport& app)
{
    for (const QByteArray& wire : app.received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::Capabilities) {
            return StationCapabilities::fromUpdates(message.updates).setupDescriptionVersion;
        }
    }
    return -1;
}

// A renderer stand-in: it rejects a stale gesture before resolving the
// table's typed sources. Production clients must also apply these checks.
QList<MirrorUpdate> materializeTnfRowAction(const QJsonObject& action,
                                            const QString& listJson, quint32 listRevision,
                                            quint32 gestureRevision, int rowId,
                                            const QJsonObject& edits,
                                            const QString& gestureSession,
                                            const QString& liveSession,
                                            quint64 gestureEpoch, quint64 liveEpoch)
{
    if (gestureSession != liveSession || gestureEpoch != liveEpoch
        || listRevision != gestureRevision) { return {}; }
    const QJsonDocument document = QJsonDocument::fromJson(listJson.toUtf8());
    if (!document.isArray() || document.array().size() > NotchModel::kMaxNotches) { return {}; }
    QSet<int> seen;
    bool found = false;
    for (const QJsonValue& raw : document.array()) {
        if (!raw.isObject()) { return {}; }
        const QJsonObject row = raw.toObject();
        const QJsonValue id = row.value("id");
        const QJsonValue centre = row.value("centreHz");
        const QJsonValue width = row.value("widthHz");
        if (!id.isDouble() || id.toInteger(-1) < 0
            || !centre.isDouble() || !std::isfinite(centre.toDouble())
            || centre.toDouble() < NotchModel::kMinNotchCentreHz
            || centre.toDouble() > NotchModel::kMaxNotchCentreHz
            || !width.isDouble() || !std::isfinite(width.toDouble())
            || width.toDouble() < 0 || width.toDouble() > NotchModel::kMaxNotchWidthHz
            || !row.value("active").isBool() || seen.contains(int(id.toInteger()))) {
            return {};
        }
        seen.insert(int(id.toInteger()));
        found |= id.toInteger() == rowId;
    }
    if (!found) { return {}; }
    const QJsonObject command = action.value("command").toObject();
    if (command.value("verb").toString().isEmpty()) { return {}; }
    const QJsonObject arguments = command.value("arguments").toObject();
    QList<MirrorUpdate> resolved;
    for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
        const QJsonObject source = it.value().toObject();
        if (source.size() != 1) { return {}; }
        if (source.value("$row") == QJsonValue("id") && it.key() == QLatin1String("id")) {
            resolved.append({0, "id", MirrorWireKind::Int64, qlonglong(rowId)});
        } else if (source.value("$edit") == QJsonValue(it.key())
                   && (it.key() == QLatin1String("centreHz")
                       || it.key() == QLatin1String("widthHz"))) {
            const QJsonValue value = edits.value(it.key());
            const double min = it.key() == QLatin1String("centreHz")
                ? NotchModel::kMinNotchCentreHz : 0;
            const double max = it.key() == QLatin1String("centreHz")
                ? NotchModel::kMaxNotchCentreHz : NotchModel::kMaxNotchWidthHz;
            if (!value.isDouble() || !std::isfinite(value.toDouble())
                || value.toDouble() < min || value.toDouble() > max) { return {}; }
            resolved.append({0, it.key().toUtf8(), MirrorWireKind::Float64, value.toDouble()});
        } else if (source.value("$edit") == QJsonValue("active")
                   && it.key() == QLatin1String("active")
                   && edits.value("active").isBool()) {
            resolved.append({0, "active", MirrorWireKind::Bool, edits.value("active").toBool()});
        } else { return {}; }
    }
    return resolved;
}

QList<MirrorUpdate> materializeTnfAdd(const QJsonObject& control,
                                     int capturedSliceId, int selectedSliceId,
                                     bool selectedOwned, const QString& gestureSession,
                                     const QString& liveSession,
                                     quint64 gestureEpoch, quint64 liveEpoch)
{
    const QJsonObject source = control.value("binding").toObject()
        .value("command").toObject().value("arguments").toObject()
        .value("sliceId").toObject();
    if (source != QJsonObject{{"$selectedOwnedSliceId", true}}
        || capturedSliceId < 0 || capturedSliceId != selectedSliceId || !selectedOwned
        || gestureSession != liveSession || gestureEpoch != liveEpoch) { return {}; }
    return {{0, "sliceId", MirrorWireKind::Int64, qlonglong(selectedSliceId)}};
}

// A small stand-in for Task 58's renderer. The Core validates static source
// names and types; the client must materialize them from one live epoch.
QJsonObject resolveTciArguments(const QJsonObject& control, bool changed,
                                const QJsonObject& properties,
                                const QString& sourceSession,
                                const QString& liveSession,
                                quint64 sourceEpoch, quint64 liveEpoch)
{
    if (sourceSession != liveSession || sourceEpoch != liveEpoch) { return {}; }
    QJsonObject result;
    const QJsonObject arguments = control.value("binding").toObject()
        .value("command").toObject().value("arguments").toObject();
    for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
        const QJsonObject source = it.value().toObject();
        if (source.contains("$controlValue")) {
            result.insert(it.key(), changed);
        } else {
            const QJsonObject ref = source.value("$property").toObject();
            if (ref.value("object") != QJsonValue("stationTci")) { return {}; }
            const QJsonValue value = properties.value(ref.value("name").toString());
            if (!value.isBool()) { return {}; }
            result.insert(it.key(), value);
        }
    }
    return result;
}
} // namespace

class SetupDescriptionServiceTest : public QObject {
    Q_OBJECT
private slots:
    void dspV15DescribesTheRestOfDspAndKeepsOlderProjections()
    {
        SetupDescriptionService service;
        const QString dsp = service.dsp();
        QVERIFY(!dsp.isEmpty());
        // Version 13 and older keep what they had: 105 controls on 9 pages,
        // version 3, the old coverage words.
        const QJsonObject v13 = projectedCategory(dsp, 13);
        QCOMPARE(v13.value("version"), QJsonValue(3));
        QCOMPARE(controlCount(v13, 0), 105);
        QCOMPARE(controlCount(v13, 15), 0);
        QVERIFY(!pageIdsOf(v13).contains("dsp.filterPresets"));
        QVERIFY(!QJsonDocument(v13).toJson().contains("coverageV15"));
        QVERIFY(v13.value("coverage").toString().startsWith("partial: Filter Presets"));
        // Version 15: 30 new rows, Filter Presets before Options.
        const QJsonObject v15 = projectedCategory(dsp, 15);
        QCOMPARE(v15.value("version"), QJsonValue(15));
        QCOMPARE(controlCount(v15, 0), 135);
        QCOMPARE(controlCount(v15, 15), 30);
        QCOMPARE(pageIdsOf(v15), (QStringList{"dsp.agcAlc", "dsp.nrAnf", "dsp.nbSnb", "dsp.cw",
                                              "dsp.amSam", "dsp.fm", "dsp.cfc", "dsp.tnf",
                                              "dsp.filterPresets", "dsp.options"}));
        QVERIFY(!QJsonDocument(v15).toJson().contains("coverageV15"));
        for (const QJsonValue& raw : v15.value("pages").toArray()) {
            const QJsonObject page = raw.toObject();
            if (page.value("id") == QJsonValue("dsp.tnf") || page.value("id") == QJsonValue("dsp.options")
                || page.value("id") == QJsonValue("dsp.filterPresets")) {
                QVERIFY2(!page.contains("coverage"), qPrintable(page.value("id").toString()));
            }
            for (const QJsonValue& section : page.value("sections").toArray()) {
                QString why;
                QVERIFY2(SetupDescriptionV15::validateSection(
                             "dsp", section.toObject().value("controls").toArray(), &why),
                         qPrintable(why));
            }
        }
    }

    void v15RowsAreCheckedAgainstTheCoresOwnSources()
    {
        const auto base = [](const QJsonObject& extra) {
            QJsonObject control{{"id", "dsp.cw.apfCenter"}, {"label", "Center Freq"},
                                {"tooltip", ""}, {"kind", "slider"},
                                {"binding", QJsonObject{{"property", QJsonObject{
                                    {"object", "slice:active"}, {"name", "apfTuneHz"}}}}},
                                {"applies", "live"}, {"requiresDescriptionVersion", 15},
                                {"min", 100}, {"max", 1100}, {"step", 1}, {"unit", "Hz"},
                                {"valueOffset", 600}};
            for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
                if (it.value().isNull()) { control.remove(it.key()); }
                else { control.insert(it.key(), it.value()); }
            }
            return control;
        };
        QString why;
        QVERIFY2(SetupDescriptionV15::validateControl("dsp", base({}), &why), qPrintable(why));
        // An outbound-only value cannot be edited, an unknown value or
        // field is refused, and the id belongs to its category.
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", base({{"binding", QJsonObject{
            {"property", QJsonObject{{"object", "slice:active"}, {"name", "nnrLimit"}}}}}})));
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", base({{"binding", QJsonObject{
            {"property", QJsonObject{{"object", "slice:active"}, {"name", "noSuchValue"}}}}}})));
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", base({{"surprise", true}})));
        QVERIFY(!SetupDescriptionV15::validateControl("transmit", base({})));
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", base({{"requiresDescriptionVersion", 13}})));
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", base({{"kind", "toggle"}})));
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", base({{"min", QJsonValue::Null}})));
        // A key the Core's own model keeps is not a raw setting row.
        const QJsonObject owned{{"id", "dsp.tnf.owned"}, {"label", "Global"}, {"tooltip", ""},
                                {"kind", "toggle"},
                                {"binding", QJsonObject{{"setting", "NotchGlobalEnabled"}}},
                                {"applies", "live"}, {"requiresDescriptionVersion", 15},
                                {"valueEncoding", QJsonObject{{"true", "True"}, {"false", "False"}}}};
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", owned));
        // A window's own key (not the Core's) is not either.
        QJsonObject local = owned;
        local.insert("binding", QJsonObject{{"setting", "TciLogWindowAutoScroll"}});
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", local));
        // A button's staged argument must name a staged row of its section
        // whose value fits the verb.
        const QJsonObject apply{{"id", "dsp.nrAnf.apply"}, {"label", "Apply"}, {"tooltip", ""},
            {"kind", "button"}, {"applies", "live"}, {"requiresDescriptionVersion", 15},
            {"gate", QJsonObject{{"capability", "nnrVersion"}, {"min", 1}}},
            {"binding", QJsonObject{{"command", QJsonObject{{"verb", "nnr.setDiagnostics"},
                {"arguments", QJsonObject{
                    {"sliceId", QJsonObject{{"$selectedOwnedSliceId", true}}},
                    {"testMode", QJsonObject{{"$control", "dsp.nrAnf.test"}}},
                    {"outputMode", QJsonObject{{"$control", "dsp.nrAnf.output"}}}}}}}}}};
        const auto staged = [](const QString& id, const QString& name, const QString& kind) {
            QJsonObject control{{"id", id}, {"label", "Mode"}, {"tooltip", ""}, {"kind", kind},
                {"applies", "staged"}, {"requiresDescriptionVersion", 15},
                {"binding", QJsonObject{{"property", QJsonObject{
                    {"object", "slice:active"}, {"name", name}}}}}};
            if (kind == QLatin1String("choice")) {
                control.insert("options", QJsonArray{QJsonObject{{"value", 0}, {"label", "A"}}});
            }
            return control;
        };
        QVERIFY2(SetupDescriptionV15::validateSection("dsp", QJsonArray{
            staged("dsp.nrAnf.test", "nnrTestMode", "choice"),
            staged("dsp.nrAnf.output", "nnrOutputMode", "choice"), apply}, &why), qPrintable(why));
        QVERIFY(!SetupDescriptionV15::validateSection("dsp", QJsonArray{
            staged("dsp.nrAnf.test", "nnrTestMode", "choice"), apply}));
        QVERIFY(!SetupDescriptionV15::validateSection("dsp", QJsonArray{
            staged("dsp.nrAnf.test", "nnrTestMode", "decimal"),
            staged("dsp.nrAnf.output", "nnrOutputMode", "choice"), apply}));
        // Filter Presets rows are closed.
        const QJsonObject dsp = QJsonDocument::fromJson(SetupDescriptionService().dsp().toUtf8()).object();
        const QJsonObject table = controlById(dsp, "dsp.filterPresets.presets");
        QVERIFY(SetupDescriptionV15::validateControl("dsp", table));
        QJsonObject changed = table;
        changed.insert("label", "Other");
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", changed));
        changed = table;
        QJsonArray columns = changed.value("columns").toArray();
        columns.removeLast();
        changed.insert("columns", columns);
        QVERIFY(!SetupDescriptionV15::validateControl("dsp", changed));
    }

    void antennaRowsRequireVersionSixAndAlex()
    {
        SetupDescriptionService service;
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes),
                                HPSDRModel::HERMES);
        // Version 13 added pages before and after Antenna / ALEX; read the
        // version 12 projection this test was written for.
        const QJsonObject hardware = projectedCategory(service.hardware(), 12);
        QCOMPARE(hardware.value("version"), QJsonValue(6));
        const QJsonArray sections = hardware.value("pages").toArray().first().toObject()
            .value("sections").toArray();
        QCOMPARE(sections.size(), 1);
        const QJsonArray controls = sections.first().toObject().value("controls").toArray();
        QCOMPARE(controls.size(), 8);
        const QJsonObject tx = controls.at(6).toObject();
        const QJsonObject rx = controls.at(7).toObject();
        QCOMPARE(tx.value("id"), QJsonValue("hardware.antenna.txRows"));
        QCOMPARE(rx.value("id"), QJsonValue("hardware.antenna.rxRows"));
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(tx, HPSDRModel::HERMES));
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(rx, HPSDRModel::HERMES));
        for (int version = 1; version <= 5; ++version) {
            const QJsonObject older = QJsonDocument::fromJson(
                SetupDescriptionService::fitCategoryForVersion(service.hardware(), version).toUtf8())
                .object();
            const QJsonArray oldControls = older.value("pages").toArray().first().toObject()
                .value("sections").toArray().first().toObject().value("controls").toArray();
            QCOMPARE(oldControls.size(), 6);
        }
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::HermesLite),
                                HPSDRModel::HERMESLITE);
        QVERIFY(SetupDescriptionService::fitCategoryForVersion(service.hardware(), 12).isEmpty());
    }

    void antennaRowsRejectChangedEnvelopeAndProjectSkuLabels()
    {
        SetupDescriptionService service;
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes),
                                HPSDRModel::HERMES);
        const auto tables = [&service] {
            const QJsonArray controls = projectedCategory(service.hardware(), 12)
                .value("pages").toArray().first().toObject().value("sections").toArray()
                .first().toObject().value("controls").toArray();
            return std::pair{controls.at(controls.size() - 2).toObject(),
                             controls.last().toObject()};
        };
        const auto [tx, rx] = tables();
        // 160m .. XVTR, then 2 m (R-IOS-26).
        QCOMPARE(tx.value("rows").toArray().size(), 15);
        QCOMPARE(rx.value("rows").toArray().size(), 15);
        QCOMPARE(tx.value("rows").toArray().last().toObject().value("band"), QJsonValue(27));
        QCOMPARE(tx.value("columns").toArray().size(), 3);
        QCOMPARE(rx.value("columns").toArray().size(), 6);
        QVERIFY(!tx.contains("columnGroups"));
        QCOMPARE(rx.value("columnGroups").toArray().size(), 2);
        const auto reject = [](QJsonObject changed, HPSDRModel model) {
            QVERIFY(!SetupDescriptionService::validateAntennaRowsTable(changed, model));
        };
        QJsonObject bad = tx;
        bad.insert("command", QJsonObject{{"verb", "setAlexTxAntennaForRadio"}});
        reject(bad, HPSDRModel::HERMES);
        bad = tx;
        bad.insert("columnGroups", rx.value("columnGroups"));
        reject(bad, HPSDRModel::HERMES);
        bad = rx;
        QJsonArray duplicateColumns = bad.value("columns").toArray();
        duplicateColumns.append(duplicateColumns.first());
        bad.insert("columns", duplicateColumns);
        reject(bad, HPSDRModel::HERMES);
        bad = tx;
        QJsonObject gate = bad.value("gate").toObject();
        gate.remove("offAir");
        bad.insert("gate", gate);
        reject(bad, HPSDRModel::HERMES);
        bad = rx;
        bad.remove("columnGroups");
        reject(bad, HPSDRModel::HERMES);
        bad = rx;
        QJsonArray groups = bad.value("columnGroups").toArray();
        groups[0] = QJsonObject{{"label", "RX-only"},
                                {"columns", QJsonArray{"rx1", "rx2", "rx3"}}};
        bad.insert("columnGroups", groups);
        reject(bad, HPSDRModel::HERMES);
        bad = rx;
        QJsonObject binding = bad.value("binding").toObject();
        binding.insert("antennaRows", QJsonObject{{"object", "alexAntennas"},
                                                   {"mode", "tx"}});
        bad.insert("binding", binding);
        reject(bad, HPSDRModel::HERMES);
        bad = rx;
        QJsonArray rows = bad.value("rows").toArray();
        QJsonObject row = rows.at(0).toObject();
        row.insert("band", 1);
        rows[0] = row;
        bad.insert("rows", rows);
        reject(bad, HPSDRModel::HERMES);
        bad = rx;
        rows = bad.value("rows").toArray();
        rows.append(rows.first());
        bad.insert("rows", rows);
        reject(bad, HPSDRModel::HERMES);
        bad = rx;
        rows = bad.value("rows").toArray();
        row = rows.at(0).toObject();
        QJsonArray cells = row.value("cells").toArray();
        QJsonObject cell = cells.at(0).toObject();
        cell.insert("tooltip", "Wrong port");
        cells[0] = cell;
        row.insert("cells", cells);
        rows[0] = row;
        bad.insert("rows", rows);
        reject(bad, HPSDRModel::HERMES);

        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes),
                                HPSDRModel::ANAN100);
        const auto [classicTx, classicRx] = tables();
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(classicTx,
                                                                   HPSDRModel::ANAN100));
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(classicRx,
                                                                   HPSDRModel::ANAN100));
        QVERIFY(classicRx.value("columns") != rx.value("columns"));
        QVERIFY(!SetupDescriptionService::validateAntennaRowsTable(classicRx,
                                                                    HPSDRModel::HERMES));
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Saturn),
                                HPSDRModel::ANAN_G2);
        const auto [g2Tx, g2Rx] = tables();
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(g2Tx,
                                                                   HPSDRModel::ANAN_G2));
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(g2Rx,
                                                                   HPSDRModel::ANAN_G2));
        QVERIFY(g2Rx.value("columns") != classicRx.value("columns"));
    }
    void appearancePublishesTenPhoneColoursWithAlpha()
    {
        SetupDescriptionService service;
        const QJsonObject appearance = projectedCategory(service.appearance(), 11);
        QCOMPARE(appearance.value("version"), QJsonValue(7));
        QCOMPARE(appearance.value("category").toObject().value("where"), QJsonValue("phone"));
        const QJsonArray pages = appearance.value("pages").toArray();
        QCOMPARE(pages.size(), 2);
        const QJsonArray sections = pages.first().toObject().value("sections").toArray();
        QCOMPARE(sections.size(), 1);
        const QJsonArray controls = sections.first().toObject().value("controls").toArray();
        QCOMPARE(controls.size(), 10);
        for (const QJsonValue& raw : controls) {
            const QJsonObject control = raw.toObject();
            QCOMPARE(control.value("kind"), QJsonValue("colour"));
            QCOMPARE(control.value("applies"), QJsonValue("live"));
            const QString colour = control.value("default").toString();
            QVERIFY(QRegularExpression(QStringLiteral("^#[0-9A-F]{8}$")).match(colour).hasMatch());
        }
    }

    void meterStylesPublishedInVersionSeven()
    {
        SetupDescriptionService service;
        const QJsonObject appearance = projectedCategory(service.appearance(), 11);
        QCOMPARE(appearance.value("version"), QJsonValue(7));
        const QJsonArray pages = appearance.value("pages").toArray();
        QCOMPARE(pages.size(), 2);
        const QJsonArray controls = pages.at(1).toObject().value("sections").toArray()
            .first().toObject().value("controls").toArray();
        QCOMPARE(controls.size(), 3);
        QCOMPARE(controls.at(0).toObject().value("id"), QJsonValue("appearance.meterStyles.face"));
        QCOMPARE(controls.at(1).toObject().value("id"), QJsonValue("appearance.meterStyles.peakHold"));
        QCOMPARE(controls.at(2).toObject().value("id"), QJsonValue("appearance.meterStyles.peakDecay"));
    }

    void meterStyleBindingRejectsMutatedGrammar()
    {
        SetupDescriptionService service;
        const QJsonArray controls = projectedCategory(service.appearance(), 11)
            .value("pages").toArray().at(1).toObject().value("sections").toArray()
            .first().toObject().value("controls").toArray();
        QCOMPARE(controls.size(), 3);
        for (const QJsonValue& raw : controls) {
            const QJsonObject original = raw.toObject();
            QVERIFY(SetupDescriptionService::validateAppearanceMeterStyleBinding(original));
            const auto rejects = [&original](const QString& field, const QJsonValue& value) {
                QJsonObject changed = original;
                changed.insert(field, value);
                QVERIFY(!SetupDescriptionService::validateAppearanceMeterStyleBinding(changed));
            };
            rejects(QStringLiteral("id"), QStringLiteral("appearance.meterStyles.other"));
            rejects(QStringLiteral("label"), QStringLiteral("Different"));
            rejects(QStringLiteral("tooltip"), QStringLiteral("Different"));
            rejects(QStringLiteral("kind"), QStringLiteral("button"));
            rejects(QStringLiteral("binding"), QJsonObject{{"phone", "Other"}});
            rejects(QStringLiteral("binding"), QJsonObject{{"setting", "SMeter_FaceStyle"}});
            rejects(QStringLiteral("binding"), QJsonObject{{"phone", original.value("binding").toObject().value("phone")},
                                                            {"setting", "SMeter_FaceStyle"}});
            rejects(QStringLiteral("applies"), QStringLiteral("subscription"));
            rejects(QStringLiteral("requiresDescriptionVersion"), 6);
            rejects(QStringLiteral("gate"), QJsonObject{{"transmit", true}});
            rejects(QStringLiteral("valueEncoding"), QJsonObject{});
            if (original.value("kind") == QJsonValue("toggle")) {
                rejects(QStringLiteral("default"), 1);
                rejects(QStringLiteral("options"), QJsonArray{});
            } else {
                rejects(QStringLiteral("default"), true);
                rejects(QStringLiteral("default"), 42);
                rejects(QStringLiteral("default"), QStringLiteral("0"));
                rejects(QStringLiteral("options"), QJsonArray{});
                QJsonArray options = original.value("options").toArray();
                QJsonObject first = options.first().toObject();
                first.insert(QStringLiteral("value"), 99);
                options[0] = first;
                rejects(QStringLiteral("options"), options);
                options = original.value("options").toArray();
                first = options.first().toObject();
                first.insert(QStringLiteral("value"), QStringLiteral("0"));
                options[0] = first;
                rejects(QStringLiteral("options"), options);
                options = original.value("options").toArray();
                first = options.first().toObject();
                first.insert(QStringLiteral("label"), QStringLiteral("Different"));
                options[0] = first;
                rejects(QStringLiteral("options"), options);
                options = original.value("options").toArray();
                first = options.first().toObject();
                first.insert(QStringLiteral("extra"), 1);
                options[0] = first;
                rejects(QStringLiteral("options"), options);
                options = original.value("options").toArray();
                const QJsonValue firstOption = options.at(0);
                options[0] = options.at(1);
                options[1] = firstOption;
                rejects(QStringLiteral("options"), options);
            }
        }
    }

    void appearanceRejectsOtherBindingsAndMalformedRgba()
    {
        SetupDescriptionService service;
        const QJsonArray controls = projectedCategory(service.appearance(), 11)
            .value("pages").toArray().first().toObject().value("sections").toArray()
            .first().toObject().value("controls").toArray();
        QCOMPARE(controls.size(), 10);
        for (const QJsonValue& raw : controls) {
            QVERIFY(SetupDescriptionService::validateAppearanceColourBinding(raw.toObject()));
        }
        const QJsonObject original = controls.first().toObject();
        const auto rejects = [&original](const QString& field, const QJsonValue& value) {
            QJsonObject changed = original;
            changed.insert(field, value);
            QVERIFY(!SetupDescriptionService::validateAppearanceColourBinding(changed));
        };
        rejects(QStringLiteral("id"), QStringLiteral("appearance.colorsTheme.unbuilt"));
        rejects(QStringLiteral("kind"), QStringLiteral("text"));
        rejects(QStringLiteral("applies"), QStringLiteral("subscription"));
        rejects(QStringLiteral("default"), QStringLiteral("#00E5FF"));
        rejects(QStringLiteral("default"), QStringLiteral("#FF00E5FF")); // wrong channel order
        rejects(QStringLiteral("default"), QStringLiteral("#00E5FFFG"));
        rejects(QStringLiteral("default"), QStringLiteral("#00e5ffff"));
        rejects(QStringLiteral("gate"), QJsonObject{{"transmit", true}});
        rejects(QStringLiteral("valueEncoding"), QJsonObject{});
        rejects(QStringLiteral("binding"), QJsonObject{{"setting", "DisplayFillColor"}});
        rejects(QStringLiteral("binding"), QJsonObject{{"phone", "DisplayGridColor"}});
    }
    void paBypassIsAClosedVersionSixSettingAndSkuProjected()
    {
        RadioModel radio;
        radio.setHpsdrModelForTest(HPSDRModel::ANAN_G2E);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);
        // Version 13 added the Watt Meter page; read the version 12 projection.
        const QJsonObject pa = projectedCategory(service.pa(), 12);
        const QJsonArray pages = pa.value("pages").toArray();
        QCOMPARE(pages.size(), 2);
        QCOMPARE(pages.first().toObject().value("id"), QJsonValue("pa.gain"));
        const QJsonArray controls = pages.first().toObject().value("sections").toArray()
            .first().toObject().value("controls").toArray();
        QCOMPARE(controls.size(), 1);
        const QJsonObject valid = controls.first().toObject();
        QVERIFY(SetupDescriptionService::validatePaBypassBinding(valid));
        QCOMPARE(valid.value("id"), QJsonValue("pa.gain.bypassPaSettings"));
        QCOMPARE(valid.value("binding").toObject().value("property").toObject()
                     .value("name"), QJsonValue("paSettingsBypass"));
        QCOMPARE(valid.value("gate"), QJsonValue(QJsonObject{
            {"capability", "transmitSettingsVersion"}, {"min", 6}}));
        QVERIFY(!valid.value("gate").toObject().contains("transmit"));
        const auto reject = [&valid](const QString& field, const QJsonValue& value) {
            QJsonObject bad = valid;
            bad.insert(field, value);
            QVERIFY(!SetupDescriptionService::validatePaBypassBinding(bad));
        };
        reject(QStringLiteral("kind"), QStringLiteral("readout"));
        reject(QStringLiteral("label"), QStringLiteral("Bypass PA"));
        reject(QStringLiteral("tooltip"), QStringLiteral("Wrong tooltip"));
        reject(QStringLiteral("gate"), QJsonObject{{"capability", "transmitSettingsVersion"},
                                                   {"min", 5}});
        reject(QStringLiteral("gate"), QJsonObject{{"capability", "transmitSettingsVersion"},
                                                   {"min", 6}, {"transmit", true}});
        reject(QStringLiteral("gate"), QJsonObject{{"capability", "transmitSettingsVersion"},
                                                   {"min", 6}, {"offAir", true}});
        QJsonObject bad = valid;
        QJsonObject binding = bad.value("binding").toObject();
        QJsonObject ref = binding.value("property").toObject();
        ref.insert("name", "mox");
        binding.insert("property", ref);
        bad.insert("binding", binding);
        QVERIFY(!SetupDescriptionService::validatePaBypassBinding(bad));
        ref.insert("name", "paSettingsBypass");
        ref.insert("object", "txState");
        binding.insert("property", ref);
        bad.insert("binding", binding);
        QVERIFY(!SetupDescriptionService::validatePaBypassBinding(bad));

        const quint32 withGain = service.revision();
        BoardCapabilities sameBoard = radio.boardCapabilities();
        sameBoard.showsBypassPaSettingsUi = false;
        service.setRadioContext(sameBoard, radio.hardwareProfile().model);
        QVERIFY(service.revision() > withGain);
        const QJsonArray withoutGain = projectedCategory(service.pa(), 12)
            .value("pages").toArray();
        QCOMPARE(withoutGain.size(), 1);
        QCOMPARE(withoutGain.first().toObject().value("id"), QJsonValue("pa.values"));
        sameBoard.showsBypassPaSettingsUi = true;
        service.setRadioContext(sameBoard, radio.hardwareProfile().model);
        QCOMPARE(projectedCategory(service.pa(), 12).value("pages").toArray().size(), 2);
        sameBoard.isRxOnlySku = true;
        service.setRadioContext(sameBoard, radio.hardwareProfile().model);
        QVERIFY(service.pa().isEmpty());
        sameBoard.isRxOnlySku = false;
        sameBoard.hasPaProfile = false;
        service.setRadioContext(sameBoard, radio.hardwareProfile().model);
        QVERIFY(service.pa().isEmpty());
    }

    void paReadoutsAreClosedTypedBindingsAndAppendSchema()
    {
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::Saturn);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);
        const QJsonObject pa = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(service.pa(), 4).toUtf8()).object();
        QCOMPARE(pa.value("version"), QJsonValue(4));
        QCOMPARE(pa.value("category").toObject().value("where"), QJsonValue("station"));
        QCOMPARE(pa.value("category").toObject().value("coverage"), QJsonValue("partial"));
        const QJsonArray sections = pa.value("pages").toArray().first().toObject()
            .value("sections").toArray();
        QCOMPARE(sections.size(), 3);
        QCOMPARE(sections.at(0).toObject().value("title"), QJsonValue("Power"));
        QCOMPARE(sections.at(1).toObject().value("title"), QJsonValue("PA Telemetry"));
        QCOMPARE(sections.at(2).toObject().value("title"), QJsonValue("Raw ADC Values"));
        QJsonArray controls = sections.at(0).toObject().value("controls").toArray();
        for (const QJsonValue& value : sections.at(2).toObject().value("controls").toArray()) {
            controls.append(value);
        }
        QCOMPARE(controls.size(), 7);
        const QStringList names{QStringLiteral("forwardPowerWatts"),
                                QStringLiteral("forwardRawPowerWatts"),
                                QStringLiteral("reflectedPowerWatts"), QStringLiteral("swr"),
                                QStringLiteral("power"), QStringLiteral("forwardAdcRaw"),
                                QStringLiteral("reflectedAdcRaw")};
        for (int i = 0; i < controls.size(); ++i) {
            const QJsonObject valid = controls.at(i).toObject();
            if (i == 4) {
                QVERIFY(SetupDescriptionService::validatePaDriveReadoutBinding(valid));
                QVERIFY(!SetupDescriptionService::validatePaReadoutBinding(valid));
                QCOMPARE(valid.value("binding").toObject().value("property").toObject()
                             .value("object"), QJsonValue("transmit"));
                QCOMPARE(valid.value("binding").toObject().value("property").toObject()
                             .value("name"), QJsonValue("power"));
                QCOMPARE(valid.value("gate").toObject(),
                         (QJsonObject{{"capability", "transmitSettingsVersion"}, {"min", 1}}));
                QCOMPARE(valid.value("decimals"), QJsonValue(0));
                QCOMPARE(valid.value("unit"), QJsonValue("W"));
                auto rejectDrive = [&valid](const QString& key, const QJsonValue& value) {
                    QJsonObject bad = valid;
                    bad.insert(key, value);
                    QVERIFY(!SetupDescriptionService::validatePaDriveReadoutBinding(bad));
                };
                rejectDrive("kind", "integer");
                rejectDrive("kind", "toggle");
                rejectDrive("decimals", 1);
                rejectDrive("unit", "dB");
                rejectDrive("gate", QJsonObject{{"capability", "transmitSettingsVersion"},
                                                {"min", 1}, {"offAir", true}});
                rejectDrive("gate", QJsonObject{{"capability", "transmitSettingsVersion"},
                                                {"min", 1}, {"transmit", true}});
                rejectDrive("binding", QJsonObject{{"property", QJsonObject{{"object", "transmit"},
                                                                      {"name", "paSettingsBypass"}}}});
                rejectDrive("binding", QJsonObject{{"property", QJsonObject{{"object", "txState"},
                                                                      {"name", "power"}}}});
                rejectDrive("binding", QJsonObject{{"setting", QJsonObject{{"key", "power"}}}});
                rejectDrive("binding", QJsonObject{{"property", QJsonObject{{"object", "transmit"},
                                                                      {"name", "power"}}},
                                                      {"write", true}});
                continue;
            }
            QVERIFY(SetupDescriptionService::validatePaReadoutBinding(valid));
            QCOMPARE(valid.value("binding").toObject().value("property").toObject()
                         .value("name").toString(), names.at(i));
            const bool raw = i >= 5;
            const int minimum = i == 1 ? 2 : 1;
            QCOMPARE(valid.value("decimals").toInt(), raw ? 0 : 2);
            QCOMPARE(valid.value("unit"), QJsonValue(i <= 2 ? "W" : ""));
            QCOMPARE(valid.value("gate"), QJsonValue(QJsonObject{
                {"capability", "txReadingsVersion"}, {"min", minimum}}));
            auto rejected = [&valid](const QString& field, const QJsonValue& value) {
                QJsonObject bad = valid;
                bad.insert(field, value);
                QVERIFY(!SetupDescriptionService::validatePaReadoutBinding(bad));
            };
            rejected(QStringLiteral("kind"), QStringLiteral("decimal"));
            rejected(QStringLiteral("decimals"), raw ? QJsonValue(2) : QJsonValue(0));
            rejected(QStringLiteral("decimals"), 7);
            rejected(QStringLiteral("decimals"), 1.5);
            rejected(QStringLiteral("unit"), QStringLiteral("dB"));
            rejected(QStringLiteral("gate"), QJsonObject{{"capability", "txStateVersion"}, {"min", 1}});
            rejected(QStringLiteral("gate"), QJsonObject{{"capability", "txReadingsVersion"},
                                                          {"min", minimum == 1 ? 2 : 1}});
            rejected(QStringLiteral("gate"), QJsonObject{{"capability", "txReadingsVersion"}, {"min", 1}, {"transmit", true}});
            rejected(QStringLiteral("gate"), QJsonObject{{"capability", "txReadingsVersion"}, {"min", 1}, {"offAir", true}});
            QJsonObject bad = valid;
            QJsonObject binding = bad.value("binding").toObject();
            QJsonObject ref = binding.value("property").toObject();
            ref.insert("object", "transmit");
            binding.insert("property", ref);
            bad.insert("binding", binding);
            QVERIFY(!SetupDescriptionService::validatePaReadoutBinding(bad));
            ref.insert("object", "txState");
            ref.insert("name", "alcDb");
            binding.insert("property", ref);
            bad.insert("binding", binding);
            QVERIFY(!SetupDescriptionService::validatePaReadoutBinding(bad));
        }
        const MirrorSchema& schema = MirrorSchema::forObject(&service);
        const MirrorProperty* paField = schema.byName("pa");
        const MirrorProperty* revision = schema.byName("revision");
        QVERIFY(paField != nullptr);
        QVERIFY(revision != nullptr);
        QCOMPARE(paField->ordinal, revision->ordinal + 1);
        QCOMPARE(paField->kind, MirrorWireKind::Utf8);
        QVERIFY(!paField->isWritable);
        QCOMPARE(MirrorPolicy::directionFor("SetupDescription", "pa"), MirrorDirection::Outbound);

        const quint32 presentRevision = service.revision();
        BoardCapabilities absent = radio.boardCapabilities();
        absent.hasPaProfile = false;
        service.setRadioContext(absent, radio.hardwareProfile().model);
        QVERIFY(service.pa().isEmpty());
        QVERIFY(service.revision() > presentRevision);
        absent.hasPaProfile = true;
        absent.isRxOnlySku = true;
        service.setRadioContext(absent, radio.hardwareProfile().model);
        QVERIFY(service.pa().isEmpty());
    }

    void paTelemetryRequiresV5AndKeepsOlderProjection()
    {
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::Saturn);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);
        const QString source = service.pa();
        QVERIFY(!source.isEmpty());
        const auto findControls = [](const QJsonObject& category) {
            QJsonArray controls;
            for (const QJsonValue& page : category.value("pages").toArray()) {
                if (page.toObject().value("id") != QJsonValue("pa.values")) { continue; }
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& control : section.toObject().value("controls").toArray()) {
                        controls.append(control);
                    }
                }
            }
            return controls;
        };
        for (int version = 1; version <= 4; ++version) {
            const QJsonObject older = QJsonDocument::fromJson(
                SetupDescriptionService::fitCategoryForVersion(source, version).toUtf8()).object();
            QCOMPARE(findControls(older).size(), 9);
        }
        const QJsonObject current = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(source, 5).toUtf8()).object();
        QCOMPARE(current.value("version"), QJsonValue(5));
        QCOMPARE(findControls(current).size(), 11);
        const QJsonArray telemetry = current.value("pages").toArray().last().toObject()
            .value("sections").toArray().at(1).toObject().value("controls").toArray();
        QCOMPARE(telemetry.size(), 4);
        const QStringList scaledNames{QStringLiteral("forwardAdcVolts"),
                                      QStringLiteral("reflectedAdcVolts")};
        for (int i = 2; i < 4; ++i) {
            const QJsonObject valid = telemetry.at(i).toObject();
            QVERIFY(SetupDescriptionService::validatePaReadoutBinding(valid));
            QCOMPARE(valid.value("binding").toObject().value("property").toObject()
                         .value("name"), QJsonValue(scaledNames.at(i - 2)));
            QCOMPARE(valid.value("gate"), QJsonValue(QJsonObject{
                {"capability", "txReadingsVersion"}, {"min", 2}}));
            QCOMPARE(valid.value("decimals"), QJsonValue(2));
            QCOMPARE(valid.value("unit"), QJsonValue("V"));
            for (const QJsonObject& wrong : {
                     QJsonObject{{"capability", "txReadingsVersion"}, {"min", 1}},
                     QJsonObject{{"capability", "txReadingsVersion"}, {"min", 2}, {"offAir", true}}}) {
                QJsonObject bad = valid;
                bad.insert("gate", wrong);
                QVERIFY(!SetupDescriptionService::validatePaReadoutBinding(bad));
            }
            QJsonObject bad = valid;
            bad.insert("binding", QJsonObject{{"property", QJsonObject{
                {"object", "txState"}, {"name", "alcDb"}}}});
            QVERIFY(!SetupDescriptionService::validatePaReadoutBinding(bad));
            bad = valid;
            bad.insert("binding", QJsonObject{{"property", QJsonObject{
                {"object", "txState"}, {"name", scaledNames.at(i - 2)}}}, {"write", true}});
            QVERIFY(!SetupDescriptionService::validatePaReadoutBinding(bad));
        }
        const QStringList names{QStringLiteral("paCurrentAmps"), QStringLiteral("supplyVolts")};
        for (int i = 0; i < 2; ++i) {
            const QJsonObject valid = telemetry.at(i).toObject();
            QVERIFY(SetupDescriptionService::validatePaTelemetryReadoutBinding(valid));
            QCOMPARE(valid.value("binding").toObject().value("telemetry").toObject()
                         .value("name"), QJsonValue(names.at(i)));
            const auto rejects = [&valid](const QString& key, const QJsonValue& value) {
                QJsonObject bad = valid;
                bad.insert(key, value);
                QVERIFY(!SetupDescriptionService::validatePaTelemetryReadoutBinding(bad));
            };
            rejects("kind", "integer");
            rejects("kind", "toggle");
            rejects("id", "pa.values.other");
            rejects("label", "Other:");
            rejects("decimals", i == 0 ? 1 : 2);
            rejects("decimals", QJsonValue(QJsonValue::Undefined));
            rejects("unit", "W");
            rejects("unit", QJsonValue(QJsonValue::Undefined));
            rejects("requiresDescriptionVersion", 4);
            rejects("requiresDescriptionVersion", QJsonValue(QJsonValue::Undefined));
            rejects("gate", QJsonObject{{"capability", "stationTelemetryVersion"}, {"min", 3}});
            rejects("gate", QJsonValue(QJsonValue::Undefined));
            rejects("gate", QJsonObject{{"capability", "stationTelemetryVersion"},
                                     {"min", 4}, {"offAir", true}});
            rejects("binding", QJsonObject{{"telemetry", QJsonObject{{"object", "radio"},
                                                                        {"name", "paVolts"}}}});
            rejects("binding", QJsonValue(QJsonValue::Undefined));
            rejects("binding", QJsonObject{{"telemetry", QJsonObject{{"object", "radio"},
                                                                        {"name", names.at(i)},
                                                                        {"write", true}}}});
            rejects("binding", QJsonObject{{"telemetry", QJsonObject{{"object", "host"},
                                                                        {"name", names.at(i)}}}});
            rejects("binding", QJsonObject{{"telemetry", QJsonObject{{"object", "radio"},
                                                                        {"name", names.at(i)}}},
                                             {"write", true}});
        }
        BoardCapabilities ampsAbsent = radio.boardCapabilities();
        ampsAbsent.hasPaAmpsTelemetry = false;
        service.setRadioContext(ampsAbsent, radio.hardwareProfile().model);
        const QJsonObject noAmps = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(service.pa(), 5).toUtf8()).object();
        QCOMPARE(findControls(noAmps).size(), 10);
        BoardCapabilities voltsAbsent = radio.boardCapabilities();
        voltsAbsent.hasPaVoltsTelemetry = false;
        service.setRadioContext(voltsAbsent, radio.hardwareProfile().model);
        const QJsonObject noVolts = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(service.pa(), 5).toUtf8()).object();
        QCOMPARE(findControls(noVolts).size(), 10);
    }

    // Version 13 (R-R3-49, R-IOS-18): PA's Watt Meter page, PA Values'
    // temperature, ADC overload and Reset Peak/Min; version 12 unchanged.
    void paV13PublishesWattMeterTemperatureOverloadAndResets()
    {
        RadioModel radio;
        radio.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);
        const QJsonObject pa = projectedCategory(service.pa(), 13);
        QCOMPARE(pa.value("version"), QJsonValue(13));
        QStringList pageIds;
        for (const QJsonValue& page : pa.value("pages").toArray()) {
            pageIds << page.toObject().value("id").toString();
        }
        QCOMPARE(pageIds, (QStringList{"pa.wattMeter", "pa.values"}));

        // The Watt Meter: the G2's ten points (the ANAN-100 class), each
        // labelled and defaulted with its factory value and held to its own
        // Thetis box, then the page's two local controls.
        const QJsonObject watt = pageById(pa, "pa.wattMeter");
        QVERIFY(!watt.contains("coverage"));
        QCOMPARE(watt.value("where"), QJsonValue("mixed"));
        const QJsonArray wattSections = watt.value("sections").toArray();
        QCOMPARE(wattSections.size(), 2);
        QCOMPARE(wattSections.at(0).toObject().value("title"),
                 QJsonValue("PA Forward Power Calibration"));
        QCOMPARE(wattSections.at(1).toObject().value("title"), QJsonValue("PA Values"));
        const QJsonArray points = wattSections.at(0).toObject().value("controls").toArray();
        QCOMPARE(points.size(), 10);
        const double maxima[10] = {100, 100, 100, 100, 100, 100, 100, 100, 110, 120};
        for (int i = 0; i < 10; ++i) {
            const QJsonObject point = points.at(i).toObject();
            const int n = i + 1;
            QCOMPARE(point, (QJsonObject{
                {"id", QStringLiteral("pa.wattMeter.calPoint%1").arg(n)},
                {"label", QStringLiteral("%1 W").arg(n * 10)},
                {"tooltip", ""}, {"kind", "decimal"},
                {"binding", QJsonObject{{"radioSetting",
                                         QStringLiteral("paCalibration/calPoint%1").arg(n)}}},
                {"applies", "live"},
                {"gate", QJsonObject{{"capability", "transmitSettingsVersion"}, {"min", 6}}},
                {"requiresDescriptionVersion", 13}, {"unit", "W"},
                {"min", 0}, {"max", maxima[i]}, {"step", 0.1}, {"decimals", 1},
                {"default", n * 10.0}, {"boardClass", int(PaCalBoardClass::Anan100)}}));
            QVERIFY(!point.value("gate").toObject().contains("transmit"));
            // Thetis gives the table no transmit rule (grp10WattMeterTrim's
            // boxes have no MOX check; the table corrects the forward-power
            // reading only), and the Core takes a point on the air, so the
            // gate carries no offAir.
            QVERIFY(!point.value("gate").toObject().contains("offAir"));
        }
        const QJsonArray local = wattSections.at(1).toObject().value("controls").toArray();
        QCOMPARE(local.size(), 2);
        QCOMPARE(local.at(0).toObject().value("binding"),
                 QJsonValue(QJsonObject{{"phone", "display/showPaValuesPage"}}));
        QCOMPARE(local.at(0).toObject().value("default"), QJsonValue(true));
        QCOMPARE(local.at(1).toObject().value("binding"),
                 QJsonValue(QJsonObject{{"phone", "resetPaValues"}}));
        QVERIFY(!local.at(1).toObject().contains("gate"));

        // PA Values: temperature after current, ADC overload last, and the
        // Reset Peak/Min action in its own section.
        const QJsonObject values = pageById(pa, "pa.values");
        const QJsonArray valueSections = values.value("sections").toArray();
        QCOMPARE(valueSections.size(), 4);
        QStringList telemetryIds;
        for (const QJsonValue& row : valueSections.at(1).toObject().value("controls").toArray()) {
            telemetryIds << row.toObject().value("id").toString();
        }
        QCOMPARE(telemetryIds, (QStringList{"pa.values.paCurrent", "pa.values.paTemperature",
                                            "pa.values.dcVoltage", "pa.values.forwardVoltage",
                                            "pa.values.reflectedVoltage",
                                            "pa.values.adcOverload"}));
        const QJsonObject temperature = valueSections.at(1).toObject().value("controls")
            .toArray().at(1).toObject();
        QCOMPARE(temperature.value("binding"), QJsonValue(QJsonObject{{"telemetry", QJsonObject{
            {"object", "radio"}, {"name", "paTemperatureCelsius"}}}}));
        QCOMPARE(temperature.value("gate"), QJsonValue(QJsonObject{
            {"capability", "stationTelemetryVersion"}, {"min", 4}}));
        QCOMPARE(temperature.value("unit"), QJsonValue(QString::fromUtf8("\xC2\xB0""C")));
        QCOMPARE(temperature.value("decimals"), QJsonValue(1));
        QCOMPARE(temperature.value("temperatureUnit"), QJsonValue("PaTempUnit"));
        const QJsonObject overload = valueSections.at(1).toObject().value("controls")
            .toArray().last().toObject();
        QCOMPARE(overload.value("binding"), QJsonValue(QJsonObject{{"adcOverload", QJsonObject{
            {"object", "stepAtt"}}}}));
        QCOMPARE(overload.value("gate"), QJsonValue(QJsonObject{
            {"capability", "radioHardwareVersion"}, {"min", 1}}));
        QVERIFY(!overload.contains("decimals"));
        QCOMPARE(valueSections.at(3).toObject().value("title"), QJsonValue("Reset"));
        const QJsonObject reset = valueSections.at(3).toObject().value("controls")
            .toArray().first().toObject();
        QCOMPARE(reset.value("id"), QJsonValue("pa.values.resetPeakMin"));
        QCOMPARE(reset.value("binding"), QJsonValue(QJsonObject{{"phone", "resetPaValues"}}));

        // Every resource row is closed: exact, and any change is refused.
        const QList<QJsonObject> rows = resourceRows(QStringLiteral("pa"), 13);
        QCOMPARE(rows.size(), 15);
        for (const QJsonObject& row : rows) {
            QVERIFY2(SetupDescriptionService::validatePaV13Control(row),
                     qPrintable(row.value("id").toString()));
            QVERIFY(!SetupDescriptionService::validateHardwareV13Control(row));
            for (const QJsonObject& changed : mutationsOf(row)) {
                QVERIFY2(!SetupDescriptionService::validatePaV13Control(changed),
                         qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
            }
        }

        // Older peers: exactly version 12's rows, version 5, 11 readouts.
        for (int version = 5; version <= 12; ++version) {
            const QJsonObject older = projectedCategory(service.pa(), version);
            QCOMPARE(older.value("version"), QJsonValue(5));
            QCOMPARE(older, [&] {
                QJsonObject expected = withoutRowsOf(pa, 13);
                expected.insert("version", 5);
                return expected;
            }());
            QVERIFY(!QJsonDocument(older).toJson().contains("pa.wattMeter"));
        }
        // Version 14 adds only PA Gain's profile rows (paV14 test).
        QCOMPARE([&] {
            QJsonObject v13 = withoutRowsOf(projectedCategory(service.pa(), 14), 14);
            v13.insert("version", 13);
            return v13;
        }(), pa);

        // Each class has its own points; an unknown model has none, so the
        // page keeps only its two local controls.
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::HermesLite),
                                HPSDRModel::HERMESLITE);
        const QJsonArray hl2 = pageById(projectedCategory(service.pa(), 13), "pa.wattMeter")
            .value("sections").toArray().first().toObject().value("controls").toArray();
        QCOMPARE(hl2.size(), 10);
        QCOMPARE(hl2.first().toObject().value("label"), QJsonValue("1 W"));
        QCOMPARE(hl2.at(8).toObject().value("max"), QJsonValue(11));
        QCOMPARE(hl2.last().toObject().value("max"), QJsonValue(12));
        QCOMPARE(hl2.last().toObject().value("boardClass"),
                 QJsonValue(int(PaCalBoardClass::Anan10)));
        RadioModel dle;
        dle.setHpsdrModelForTest(HPSDRModel::ANAN8000D);
        service.setRadioContext(dle.boardCapabilities(), HPSDRModel::ANAN8000D);
        const QJsonArray big = pageById(projectedCategory(service.pa(), 13), "pa.wattMeter")
            .value("sections").toArray().first().toObject().value("controls").toArray();
        QCOMPARE(big.size(), 10);
        QCOMPARE(big.at(4).toObject().value("label"), QJsonValue("100 W"));
        QCOMPARE(big.at(4).toObject().value("max"), QJsonValue(140));
        QCOMPARE(big.last().toObject().value("max"), QJsonValue(240));
        service.setRadioContext(radio.boardCapabilities(), HPSDRModel::FIRST);
        const QJsonObject unknown = pageById(projectedCategory(service.pa(), 13), "pa.wattMeter");
        QCOMPARE(unknown.value("sections").toArray().size(), 1);
        QCOMPARE(unknown.value("sections").toArray().first().toObject().value("title"),
                 QJsonValue("PA Values"));
    }

    // Version 14 (R-R3-49, R-IOS-18): PA Gain's profile choice, New, Copy,
    // Delete and Reset Defaults, and the per-band table, bound to the
    // Core's `paProfiles` object and the paProfile verbs; on every radio
    // with a PA, the bypass box still the ANAN-G2E's only.
    void paV14PublishesPaGainProfiles()
    {
        RadioModel radio;
        radio.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);
        const QJsonObject pa = projectedCategory(service.pa(), 14);
        QCOMPARE(pa.value("version"), QJsonValue(14));
        const QJsonObject gain = pageById(pa, "pa.gain");
        const QJsonArray sections = gain.value("sections").toArray();
        QCOMPARE(sections.size(), 2);   // no bypass box on the G2
        QCOMPARE(sections.at(0).toObject().value("title"), QJsonValue("Profile"));
        QCOMPARE(sections.at(1).toObject().value("title"), QJsonValue("PA Gain by Band (dB)"));
        QStringList ids;
        for (const QJsonValue& row : rowsOf(gain)) {
            const QJsonObject control = row.toObject();
            ids << control.value("id").toString();
            QCOMPARE(control.value("gate"), QJsonValue(QJsonObject{
                {"capability", "paProfileVersion"}, {"min", 1}, {"offAir", true}}));
            QVERIFY(SetupDescriptionService::validatePaV14Control(control));
        }
        QCOMPARE(ids, (QStringList{"pa.gain.profile", "pa.gain.new", "pa.gain.copy",
                                   "pa.gain.delete", "pa.gain.reset", "pa.gain.table"}));
        const QJsonObject table = rowsOf(gain).last().toObject();
        QCOMPARE(table.value("binding"), QJsonValue(QJsonObject{
            {"paProfileGrid", QJsonObject{{"object", "paProfiles"}}}}));
        QCOMPARE(table.value("rows").toArray().size(), 14);
        QCOMPARE(table.value("rows").toArray().last().toObject().value("label"), QJsonValue("XVTR"));
        const QJsonArray columns = table.value("columns").toArray();
        QCOMPARE(columns.size(), 12);
        QCOMPARE(columns.first().toObject().value("min"), QJsonValue(38.8));
        QCOMPARE(columns.at(9).toObject().value("label"), QJsonValue("90%"));
        QCOMPARE(columns.at(9).toObject().value("driveStep"), QJsonValue(8));
        QCOMPARE(columns.at(10).toObject().value("max"), QJsonValue(1500));
        QCOMPARE(columns.last().toObject().value("kind"), QJsonValue("toggle"));
        QCOMPARE(rowsOf(gain).at(3).toObject().value("confirm"), QJsonValue("Delete profile \"%1\"?"));
        QCOMPARE(rowsOf(gain).at(2).toObject().value("prompt").toObject().value("default"),
                 QJsonValue("%1 (copy)"));

        const QList<QJsonObject> rows = resourceRows(QStringLiteral("pa"), 14);
        QCOMPARE(rows.size(), 6);
        for (const QJsonObject& row : rows) {
            QVERIFY(!SetupDescriptionService::validatePaV13Control(row));
            for (const QJsonObject& changed : mutationsOf(row)) {
                QVERIFY2(!SetupDescriptionService::validatePaV14Control(changed),
                         qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
            }
        }

        // Version 13 and older: no profile rows, and no PA Gain page on the G2.
        for (int version = 1; version <= 13; ++version) {
            QVERIFY(pageById(projectedCategory(service.pa(), version), "pa.gain").isEmpty());
        }
        // The G2E keeps its bypass box, last, after the profile rows.
        RadioModel g2e;
        g2e.setHpsdrModelForTest(HPSDRModel::ANAN_G2E);
        service.setRadioContext(g2e.boardCapabilities(), g2e.hardwareProfile().model);
        const QJsonArray g2eRows = rowsOf(pageById(projectedCategory(service.pa(), 14), "pa.gain"));
        QCOMPARE(g2eRows.size(), 7);
        QCOMPARE(g2eRows.last().toObject().value("id"), QJsonValue("pa.gain.bypassPaSettings"));
        const QJsonArray g2eV13 = rowsOf(pageById(projectedCategory(service.pa(), 13), "pa.gain"));
        QCOMPARE(g2eV13.size(), 1);
        QCOMPARE(g2eV13.first().toObject().value("id"), QJsonValue("pa.gain.bypassPaSettings"));
    }

    // Version 20 (R-R3-49, R-IOS-18, JJ's ruling: follow Thetis): on the
    // air PA Gain publishes which rows are locked, with the reason, through
    // the rows' `availability`. The profile choice and its buttons and
    // every band but the transmitting one are locked; the transmitting band
    // opens only for the device that holds transmit. Version 19 and older
    // keep the exact closed rows.
    // Version 21: TCI's Forget row greys out while Duplicate is off, as the
    // desktop's does. Version 20 and older keep the row without the
    // dependency, and CAT & Network at version 15.
    void catNetworkV21GreysForgetWithDuplicate()
    {
        SetupDescriptionService service;
        const QString forgetId =
            QStringLiteral("catNetwork.tciServer.core.forgetRx2VfoBOnDisconnect");
        const QJsonObject dependency{
            {"property", QJsonObject{{"object", "stationTci"}, {"name", "copyRx2VfobToVfoa"}}},
            {"oneOf", QJsonArray{true}}};
        const QJsonObject current = service.category(QStringLiteral("catNetwork"));
        QCOMPARE(current.value("version"), QJsonValue(21));
        QCOMPARE(controlById(current, forgetId).value("enabledWhen"), QJsonValue(dependency));

        for (int version : {21, 22}) {
            const QJsonObject v21 = projectedCategory(service.catNetwork(), version);
            QCOMPARE(v21.value("version"), QJsonValue(21));
            QCOMPARE(controlById(v21, forgetId).value("enabledWhen"), QJsonValue(dependency));
        }
        const QJsonObject v21 = projectedCategory(service.catNetwork(), 21);
        for (int version : {1, 3, 14, 15, 20}) {
            const QJsonObject older = projectedCategory(service.catNetwork(), version);
            QCOMPARE(older.value("version"), QJsonValue(version >= 15 ? 15 : qMin(version, 3)));
            const QJsonObject forget = controlById(older, forgetId);
            QCOMPARE(forget.value("id"), QJsonValue(forgetId));
            QVERIFY(!forget.contains("enabledWhen"));
            QVERIFY(!QJsonDocument(older).toJson().contains("enabledWhen"));
        }
        // Only the dependency and the version differ between 20 and 21.
        QJsonObject v20 = projectedCategory(service.catNetwork(), 20);
        v20.insert("version", 21);
        QVERIFY(v20 != v21);
        QJsonObject withDependency = controlById(v20, forgetId);
        withDependency.insert("enabledWhen", dependency);
        QCOMPARE(withDependency, controlById(v21, forgetId));
        QCOMPARE(controlCount(v20, 0), controlCount(v21, 0));

        // The Core accepts that exact dependency on that row only.
        QJsonObject forget = controlById(current, forgetId);
        QVERIFY(SetupDescriptionService::validateCatNetworkV21EnabledWhen(forget));
        QJsonObject altered = forget;
        altered.insert("enabledWhen", QJsonObject{
            {"property", QJsonObject{{"object", "stationTci"}, {"name", "copyRx2VfobToVfoa"}}},
            {"oneOf", QJsonArray{false}}});
        QVERIFY(!SetupDescriptionService::validateCatNetworkV21EnabledWhen(altered));
        altered = forget;
        altered.insert("id", QStringLiteral("catNetwork.tciServer.core.useRx1VfoaForRx2Vfoa"));
        QVERIFY(!SetupDescriptionService::validateCatNetworkV21EnabledWhen(altered));
        altered = forget;
        altered.remove("enabledWhen");
        QVERIFY(!SetupDescriptionService::validateCatNetworkV21EnabledWhen(altered));
    }

    void paV20PublishesTheOnAirLockPerRow()
    {
        RadioModel radio;
        radio.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);
        const QJsonObject locked{{"enabled", false},
                                 {"reason", RadioModel::paOnAirLockedReason()}};
        const QJsonObject holderOnly{{"enabled", false},
                                     {"reason", RadioModel::paHolderOnlyReason()}};
        const auto gainRows = [](const QString& pa, int version, bool holds) {
            return rowsOf(pageById(QJsonDocument::fromJson(
                SetupDescriptionService::fitCategoryForVersion(pa, version, true, holds)
                    .toUtf8()).object(), "pa.gain"));
        };

        // Off the air: nothing is locked; the table's gate has no off-air
        // part (the rows carry the lock instead).
        const QJsonObject offAir = projectedCategory(service.pa(), 20);
        QCOMPARE(offAir.value("version"), QJsonValue(20));
        QJsonArray rows = gainRows(service.pa(), 20, false);
        QCOMPARE(rows.size(), 6);
        for (const QJsonValue& raw : rows) {
            const QJsonObject control = raw.toObject();
            QVERIFY(!control.contains("availability"));
            QCOMPARE(control.value("requiresDescriptionVersion"), QJsonValue(14));
            if (control.value("kind") == QJsonValue("table")) {
                QCOMPARE(control.value("gate"), QJsonValue(QJsonObject{
                    {"capability", "paProfileVersion"}, {"min", 1}}));
                for (const QJsonValue& row : control.value("rows").toArray()) {
                    QVERIFY(!row.toObject().contains("availability"));
                }
            } else {
                QCOMPARE(control.value("gate"), QJsonValue(QJsonObject{
                    {"capability", "paProfileVersion"}, {"min", 1}, {"offAir", true}}));
            }
        }

        // On the air on 20 m (Band 5).
        const quint32 before = service.revision();
        QSignalSpy paSpy(&service, &SetupDescriptionService::paDescriptionChanged);
        QSignalSpy allSpy(&service, &SetupDescriptionService::descriptionsChanged);
        service.setPaOnAirState(true, 5);
        QCOMPARE(paSpy.count(), 1);
        QCOMPARE(allSpy.count(), 0);   // only PA is sent again
        QVERIFY(service.revision() > before);
        for (const bool holds : {false, true}) {
            rows = gainRows(service.pa(), 20, holds);
            QCOMPARE(rows.size(), 6);
            for (int i = 0; i < 5; ++i) {
                QCOMPARE(rows.at(i).toObject().value("availability"), QJsonValue(locked));
            }
            const QJsonArray bands = rows.last().toObject().value("rows").toArray();
            QCOMPARE(bands.size(), 14);
            for (const QJsonValue& raw : bands) {
                const QJsonObject row = raw.toObject();
                QCOMPARE(row.keys().contains("holderMayEdit"), false);
                if (row.value("band") != QJsonValue(5)) {
                    QCOMPARE(row.value("availability"), QJsonValue(locked));
                } else if (holds) {
                    QVERIFY(!row.contains("availability"));
                } else {
                    QCOMPARE(row.value("availability"), QJsonValue(holderOnly));
                }
            }
        }
        // Version 19 and older: the exact closed rows, as off the air.
        for (const bool holds : {false, true}) {
            for (const QJsonValue& raw : gainRows(service.pa(), 19, holds)) {
                QVERIFY(SetupDescriptionService::validatePaV14Control(raw.toObject()));
            }
        }
        QVERIFY(!SetupDescriptionService::fitCategoryForVersion(service.pa(), 19)
                     .contains("availability"));

        // A band with no PA values (-1): every band is locked.
        service.setPaOnAirState(true, -1);
        for (const QJsonValue& raw : gainRows(service.pa(), 20, true).last().toObject()
                                         .value("rows").toArray()) {
            QCOMPARE(raw.toObject().value("availability"), QJsonValue(locked));
        }
        service.setPaOnAirState(true, 5);

        // A change of holder on the air sends PA again (each peer's
        // projection resolves the transmitting band's row); off the air it
        // changes nothing.
        paSpy.clear();
        const quint32 onAirRevision = service.revision();
        service.noteTransmitHolderChanged();
        QCOMPARE(paSpy.count(), 1);
        QVERIFY(service.revision() > onAirRevision);
        service.setPaOnAirState(false, -1);
        QCOMPARE(paSpy.count(), 2);
        for (const QJsonValue& raw : gainRows(service.pa(), 20, false)) {
            QVERIFY(!QJsonDocument(raw.toObject()).toJson().contains("availability"));
        }
        const quint32 offRevision = service.revision();
        service.noteTransmitHolderChanged();
        QCOMPARE(paSpy.count(), 2);
        QCOMPARE(service.revision(), offRevision);
        // The same state again sends nothing.
        service.setPaOnAirState(false, -1);
        QCOMPARE(paSpy.count(), 2);
    }

    // Version 22: DSP > Options' four RX buffer size rows carry their
    // on-the-air lock and its reason, as Thetis greys grpDSPBufferSize
    // while MOX is on (setup.cs:5159 [v2.10.3.15]). A peer below 22 reads
    // the rows exactly as before; the TX rows keep their offAir gate.
    void dspRxBufferSizesLockOnTheAirFromVersion22()
    {
        RadioModel radio;
        radio.setHpsdrModelForTest(HPSDRModel::ANAN_G2E);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);

        QStringList rxIds;
        for (const char* mode : {"Phone", "Fm", "Cw", "Dig"}) {
            rxIds << QStringLiteral("dsp.options.DspOptionsBufferSize%1Rx").arg(QLatin1String(mode));
        }
        QStringList txIds;
        for (const char* mode : {"Phone", "Fm", "Dig"}) {
            txIds << QStringLiteral("dsp.options.DspOptionsBufferSize%1Tx").arg(QLatin1String(mode));
        }
        const QJsonObject locked{{"enabled", false},
                                 {"reason", "Can't change while transmitting."}};
        QCOMPARE(RadioModel::dspBufferOnAirLockedReason(), locked.value("reason").toString());

        // Off the air: no row carries a lock at any version.
        for (const int version : {19, 21, 22}) {
            const QJsonObject dsp = projectedCategory(service.dsp(), version);
            for (const QString& id : rxIds) {
                const QJsonObject row = controlById(dsp, id);
                QVERIFY2(!row.isEmpty(), qPrintable(id));
                QVERIFY2(!row.contains("availability"), qPrintable(id));
                QVERIFY2(!row.contains("gate"), qPrintable(id));
            }
        }
        QCOMPARE(projectedCategory(service.dsp(), 22).value("version"), QJsonValue(22));
        QCOMPARE(projectedCategory(service.dsp(), 21).value("version"), QJsonValue(19));
        const QString olderOffAir = SetupDescriptionService::fitCategoryForVersion(service.dsp(), 21);
        const QJsonObject txGate = controlById(projectedCategory(service.dsp(), 22), txIds.first())
                                       .value("gate").toObject();
        QCOMPARE(txGate.value("offAir"), QJsonValue(true));

        // On the air (the Core's on-air edge, PA's lock with it): DSP is
        // sent again and PA once with the revision, one revision for the
        // edge; the other categories are not sent.
        const quint32 before = service.revision();
        const QString paOffAir = service.pa();
        QSignalSpy dspSpy(&service, &SetupDescriptionService::dspDescriptionChanged);
        QSignalSpy paSpy(&service, &SetupDescriptionService::paDescriptionChanged);
        QSignalSpy allSpy(&service, &SetupDescriptionService::descriptionsChanged);
        service.setOnAirState(true, 5);
        QCOMPARE(dspSpy.count(), 1);
        QCOMPARE(paSpy.count(), 1);
        QCOMPARE(allSpy.count(), 0);
        QCOMPARE(service.revision(), before + 1);
        QVERIFY(service.pa() != paOffAir);
        const QJsonObject onAir = projectedCategory(service.dsp(), 22);
        int lockedRows = 0;
        for (const QJsonValue& page : onAir.value("pages").toArray()) {
            for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                    if (raw.toObject().value("availability") == QJsonValue(locked)) {
                        QVERIFY2(rxIds.contains(raw.toObject().value("id").toString()),
                                 qPrintable(raw.toObject().value("id").toString()));
                        ++lockedRows;
                    }
                }
            }
        }
        QCOMPARE(lockedRows, 4);
        for (const QString& id : txIds) {
            const QJsonObject row = controlById(onAir, id);
            QVERIFY2(!row.contains("availability"), qPrintable(id));
            QCOMPARE(row.value("gate").toObject(), txGate);
        }
        // A version 21 peer reads the same DSP as off the air.
        QCOMPARE(SetupDescriptionService::fitCategoryForVersion(service.dsp(), 21), olderOffAir);

        // The same state again sends nothing.
        const quint32 onAirRevision = service.revision();
        service.setOnAirState(true, 5);
        QCOMPARE(dspSpy.count(), 1);
        QCOMPARE(paSpy.count(), 1);
        QCOMPARE(service.revision(), onAirRevision);

        // Back on receive: the lock is gone, again in one revision.
        service.setOnAirState(false, -1);
        QCOMPARE(dspSpy.count(), 2);
        QCOMPARE(paSpy.count(), 2);
        QCOMPARE(service.revision(), onAirRevision + 1);
        QCOMPARE(service.pa(), paOffAir);
        for (const QString& id : rxIds) {
            QVERIFY2(!controlById(projectedCategory(service.dsp(), 22), id).contains("availability"),
                     qPrintable(id));
        }
    }

    // Version 13: Hardware Config's Radio Info (the Core's radio, as the
    // desktop tab shows it), TX Display Cal and the HL2's N2ADR switch.
    void hardwareV13PublishesRadioInfoCalibrationAndN2adr()
    {
        RadioInfo info;
        info.name = QStringLiteral("ANAN-100");
        info.macAddress = QStringLiteral("00:1C:C0:A2:12:34");
        info.address = QHostAddress(QStringLiteral("192.168.1.20"));
        info.firmwareVersion = 32;
        info.protocol = ProtocolVersion::Protocol1;
        const BoardCapabilities hermes = BoardCapsTable::forBoard(HPSDRHW::Hermes);
        SetupDescriptionService service;
        service.setRadioContext(hermes, HPSDRModel::ANAN100, info);
        const QJsonObject hardware = projectedCategory(service.hardware(), 13);
        QCOMPARE(hardware.value("version"), QJsonValue(13));
        QStringList pageIds;
        for (const QJsonValue& page : hardware.value("pages").toArray()) {
            pageIds << page.toObject().value("id").toString();
        }
        QCOMPARE(pageIds, (QStringList{"hardware.radioInfo", "hardware.antennaAlex",
                                       "hardware.alex1Filters", "hardware.calibration"}));
        const RadioInfoFacts facts = radioInfoFacts(info, hermes, HPSDRModel::ANAN100);
        QCOMPARE(facts.board, QStringLiteral("ANAN-100"));
        QCOMPARE(facts.firmware, QStringLiteral("32"));
        const QJsonArray radioRows = rowsOf(pageById(hardware, "hardware.radioInfo"));
        QCOMPARE(radioRows.size(), 9);
        const QStringList fields{"board", "protocol", "adcCount", "maxRx", "firmware", "mac", "ip"};
        const QStringList values{facts.board, facts.protocol, facts.adcCount, facts.maxRx,
                                 facts.firmware, facts.mac, facts.ip};
        for (int i = 0; i < fields.size(); ++i) {
            const QJsonObject row = radioRows.at(i).toObject();
            QCOMPARE(row.value("id"), QJsonValue("hardware.radioInfo." + fields.at(i)));
            QCOMPARE(row.value("binding"), QJsonValue(QJsonObject{{"radioInfo", fields.at(i)}}));
            QCOMPARE(row.value("value"), QJsonValue(values.at(i)));
            QVERIFY(!row.contains("gate"));
        }
        QCOMPARE(radioRows.at(6).toObject().value("value"), QJsonValue("192.168.1.20"));
        const QJsonObject copy = radioRows.last().toObject();
        QCOMPARE(copy.value("binding"), QJsonValue(QJsonObject{{"phone", "copySupportInfo"}}));
        QCOMPARE(copy.value("copyText"), QJsonValue(facts.supportText()));
        QVERIFY(copy.value("copyText").toString().startsWith("Board: ANAN-100\nProtocol: Protocol 1\n"));

        const QJsonArray calibration = rowsOf(pageById(hardware, "hardware.calibration"));
        QCOMPARE(calibration.size(), 1);
        const QJsonObject offset = calibration.first().toObject();
        QCOMPARE(offset.value("binding"), QJsonValue(QJsonObject{{"radioSetting",
                                                                  "cal/txDisplayOffset"}}));
        QCOMPARE(offset.value("gate"), QJsonValue(QJsonObject{
            {"capability", "transmitSettingsVersion"}, {"min", 8}}));
        QCOMPARE(offset.value("min"), QJsonValue(-100));
        QCOMPARE(offset.value("max"), QJsonValue(100));

        // A radio that has reported nothing reads as the tab does.
        const quint32 known = service.revision();
        service.setRadioContext(hermes, HPSDRModel::ANAN100, RadioInfo{});
        QVERIFY(service.revision() > known);
        const QJsonArray blank = rowsOf(pageById(projectedCategory(service.hardware(), 13),
                                                 "hardware.radioInfo"));
        const QString dash = QString(QChar(0x2014));
        QCOMPARE(blank.at(4).toObject().value("value"), QJsonValue(dash));
        QCOMPARE(blank.at(5).toObject().value("value"), QJsonValue(dash));
        QCOMPARE(blank.at(6).toObject().value("value"), QJsonValue(dash));
        // The same radio again changes nothing.
        service.setRadioContext(hermes, HPSDRModel::ANAN100, info);
        const quint32 again = service.revision();
        service.setRadioContext(hermes, HPSDRModel::ANAN100, info);
        QCOMPARE(service.revision(), again);

        // The HL2: no Antenna / ALEX, its I/O board's N2ADR switch last.
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::HermesLite),
                                HPSDRModel::HERMESLITE, info);
        const QJsonObject hl2 = projectedCategory(service.hardware(), 13);
        pageIds.clear();
        for (const QJsonValue& page : hl2.value("pages").toArray()) {
            pageIds << page.toObject().value("id").toString();
        }
        QCOMPARE(pageIds, (QStringList{"hardware.radioInfo", "hardware.calibration",
                                       "hardware.hl2Io"}));
        const QJsonObject n2adr = rowsOf(pageById(hl2, "hardware.hl2Io")).first().toObject();
        QCOMPARE(n2adr.value("binding"), QJsonValue(QJsonObject{{"radioSetting",
                                                                 "hl2IoBoard/n2adrFilter"}}));
        QCOMPARE(n2adr.value("valueEncoding"), QJsonValue(QJsonObject{{"true", "True"},
                                                                      {"false", "False"}}));
        QCOMPARE(n2adr.value("default"), QJsonValue(true));
        QVERIFY(SetupDescriptionService::validateSettingToggleEncoding(n2adr));
        QJsonObject unencoded = n2adr;
        unencoded.remove("valueEncoding");
        QVERIFY(!SetupDescriptionService::validateSettingToggleEncoding(unencoded));
        // Before 13 the HL2 still has no Hardware category.
        for (int version = 1; version <= 12; ++version) {
            QVERIFY(SetupDescriptionService::fitCategoryForVersion(service.hardware(), version)
                        .isEmpty());
        }

        // Every resource row is closed.
        const QList<QJsonObject> rows = resourceRows(QStringLiteral("hardware"), 13);
        // Radio Info's seven, its sample rate and copy button, TX Display
        // Cal, N2ADR, the Alex-1 tab's five switches, and the Alex receive
        // filter rows: three banks of six rows of three, and the Alex-2
        // master.
        QCOMPARE(rows.size(), 11 + 5 + 3 * 6 * 3 + 1);
        for (const QJsonObject& row : rows) {
            QVERIFY2(SetupDescriptionService::validateHardwareV13Control(row),
                     qPrintable(row.value("id").toString()));
            QVERIFY(!SetupDescriptionService::validatePaV13Control(row));
            QVERIFY(!SetupDescriptionService::validateHardwarePropertyBinding(row));
            for (const QJsonObject& changed : mutationsOf(row)) {
                QVERIFY2(!SetupDescriptionService::validateHardwareV13Control(changed),
                         qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
            }
        }

        // Older peers on an ALEX board: exactly version 12's rows, version 6.
        service.setRadioContext(hermes, HPSDRModel::ANAN100, info);
        const QJsonObject current = projectedCategory(service.hardware(), 13);
        for (int version = 6; version <= 12; ++version) {
            QJsonObject expected = withoutRowsOf(current, 13);
            expected.insert("version", 6);
            QCOMPARE(projectedCategory(service.hardware(), version), expected);
        }
        QCOMPARE(projectedCategory(service.hardware(), 14), current);
    }

    // Version 16: the HL2 Options rows on HL2 I/O, in the desktop tab's
    // order. The four the Core does not send the radio are described
    // disabled with the desktop's reason. Versions 13 to 15 are unchanged.
    void hardwareV16DescribesHl2Options()
    {
        SetupDescriptionService service;
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::HermesLite),
                                HPSDRModel::HERMESLITE, RadioInfo{});
        const QJsonObject current = projectedCategory(service.hardware(), 16);
        QCOMPARE(current.value("version"), QJsonValue(16));
        const QJsonArray sections = pageById(current, "hardware.hl2Io")
            .value("sections").toArray();
        QCOMPARE(sections.size(), 2);
        QCOMPARE(sections.at(0).toObject().value("title"), QJsonValue("Configuration"));
        const QJsonObject options = sections.at(1).toObject();
        QCOMPARE(options.value("title"), QJsonValue("Hermes Lite Options"));
        const QJsonArray rows = options.value("controls").toArray();
        QStringList ids;
        for (const QJsonValue& row : rows) { ids << row.toObject().value("id").toString(); }
        QCOMPARE(ids, (QStringList{
            "hardware.hl2Io.txLatency", "hardware.hl2Io.pttHang", "hardware.hl2Io.cl2Enable",
            "hardware.hl2Io.cl2Freq", "hardware.hl2Io.ext10MHz", "hardware.hl2Io.disconnectReset",
            "hardware.hl2Io.psSync", "hardware.hl2Io.bandVolts",
            "hardware.hl2Io.swapAudioChannels"}));
        const QString clock = QStringLiteral("NereusSDR does not change the radio's clock settings.");
        const QHash<QString, QString> reasons{
            {"hardware.hl2Io.cl2Enable", clock},
            {"hardware.hl2Io.cl2Freq", clock},
            {"hardware.hl2Io.ext10MHz", clock}};
        // Swap audio channels is open from version 16 (the radio codec
        // lane): the Core sends the HL2 its receive audio.
        for (const QJsonValue& raw : rows) {
            const QJsonObject row = raw.toObject();
            const QString id = row.value("id").toString();
            QVERIFY2(row.value("binding").toObject().value("radioSetting").toString()
                         .startsWith("hl2/"), qPrintable(id));
            // A version 16 peer keeps the rows it was built for, closed.
            QVERIFY2(SetupDescriptionService::validateHardwareV16Control(row), qPrintable(id));
            if (reasons.contains(id)) {
                QCOMPARE(row.value("availability"), QJsonValue(QJsonObject{
                    {"enabled", false}, {"reason", reasons.value(id)}}));
            } else {
                QVERIFY2(!row.contains("availability"), qPrintable(id));
            }
            // TX latency and PTT hang are transmit settings the Core
            // refuses from a receive-only device.
            QCOMPARE(row.value("gate").toObject().value("transmit").toBool(),
                     id == "hardware.hl2Io.txLatency" || id == "hardware.hl2Io.pttHang");
            if (row.value("kind") == QJsonValue("toggle")) {
                QVERIFY(SetupDescriptionService::validateSettingToggleEncoding(row));
            }
        }

        // Every resource row is closed. Six rows stay version 16's; the
        // three clock rows are version 18's.
        const QList<QJsonObject> resource = resourceRows(QStringLiteral("hardware"), 16);
        QCOMPARE(resource.size(), 6);
        for (const QJsonObject& row : resource) {
            QVERIFY2(SetupDescriptionService::validateHardwareV16Control(row),
                     qPrintable(row.value("id").toString()));
            QVERIFY(!SetupDescriptionService::validateHardwareV13Control(row));
            QVERIFY(!SetupDescriptionService::validateHardwareV18Control(row));
            for (const QJsonObject& changed : mutationsOf(row)) {
                QVERIFY2(!SetupDescriptionService::validateHardwareV16Control(changed),
                         qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
            }
        }

        // Versions 13 to 15 see exactly version 13's category.
        for (int version = 13; version <= 15; ++version) {
            QJsonObject expected = withoutRowsOf(current, 16);
            expected.insert("version", 13);
            QCOMPARE(projectedCategory(service.hardware(), version), expected);
        }
        // Version 17 adds only the Alex-1 low-pass rows, which the HL2 has no
        // Alex board for.
        QJsonObject v17 = projectedCategory(service.hardware(), 17);
        QCOMPARE(v17.value("version"), QJsonValue(17));
        v17.insert("version", 16);
        QCOMPARE(v17, current);

        // A radio without the HL2's I/O board has no HL2 I/O page.
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes),
                                HPSDRModel::ANAN100, RadioInfo{});
        QVERIFY(pageById(projectedCategory(service.hardware(), 16), "hardware.hl2Io").isEmpty());
    }

    // Version 18: HL2 Options' clock rows open, now that the Core sends
    // them to its radio (radioHardwareVersion 11). They carry mi0bot's
    // tooltips (setup.designer.cs:11158, 11174, 11187 [@c26a8a4]) and no
    // availability, and the frequency row is enabled only while Enable CL2
    // is on. The rest of the category is version 16's; a later declaration
    // sees version 18.
    void hardwareV18OpensHl2ClockRows()
    {
        SetupDescriptionService service;
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::HermesLite),
                                HPSDRModel::HERMESLITE, RadioInfo{});
        const QJsonObject current = projectedCategory(service.hardware(), 18);
        QCOMPARE(current.value("version"), QJsonValue(18));
        const QJsonObject older = projectedCategory(service.hardware(), 16);
        QCOMPARE(older.value("version"), QJsonValue(16));
        const QJsonArray rows = pageById(current, "hardware.hl2Io").value("sections")
            .toArray().at(1).toObject().value("controls").toArray();
        const QJsonArray olderRows = pageById(older, "hardware.hl2Io").value("sections")
            .toArray().at(1).toObject().value("controls").toArray();
        QCOMPARE(rows.size(), 9);
        QCOMPARE(olderRows.size(), 9);
        const QHash<QString, QString> tooltips{
            {"hardware.hl2Io.cl2Enable", "Enable frequency output on CL2"},
            {"hardware.hl2Io.cl2Freq", "Output frequency on CL2 output"},
            {"hardware.hl2Io.ext10MHz", "Enable external 10 MHz input on CL1"}};
        for (int i = 0; i < rows.size(); ++i) {
            const QJsonObject row = rows.at(i).toObject();
            const QJsonObject was = olderRows.at(i).toObject();
            const QString id = row.value("id").toString();
            QCOMPARE(was.value("id").toString(), id);
            if (!tooltips.contains(id)) {
                QCOMPARE(row, was);
                continue;
            }
            QCOMPARE(row.value("requiresDescriptionVersion"), QJsonValue(18));
            QCOMPARE(row.value("tooltip"), QJsonValue(tooltips.value(id)));
            QVERIFY2(!row.contains("availability"), qPrintable(id));
            QVERIFY2(SetupDescriptionService::validateHardwareV18Control(row), qPrintable(id));
            QVERIFY2(!SetupDescriptionService::validateHardwareV16Control(row), qPrintable(id));
            // Everything else is the version 16 row's, except that the
            // frequency is decimal to three places with a 0.1 step, as
            // mi0bot's udCl2Freq (setup.designer.cs:11133-11163 [@c26a8a4]).
            const bool isFreq = id == "hardware.hl2Io.cl2Freq";
            for (const QString& key : {"label", "kind", "binding", "valueEncoding", "applies",
                                       "gate", "min", "max", "step", "unit", "default"}) {
                if (isFreq && (key == "kind" || key == "step")) {
                    continue;
                }
                QCOMPARE(row.value(key), was.value(key));
            }
            if (isFreq) {
                QCOMPARE(row.value("kind"), QJsonValue("decimal"));
                QCOMPARE(row.value("step"), QJsonValue(0.1));
                QCOMPARE(row.value("decimals"), QJsonValue(3));
                QCOMPARE(was.value("kind"), QJsonValue("integer"));
                QCOMPARE(was.value("step"), QJsonValue(1));
                QVERIFY(!was.contains("decimals"));
            } else {
                QVERIFY2(!row.contains("decimals"), qPrintable(id));
            }
            QCOMPARE(row.value("enabledWhen"), id == "hardware.hl2Io.cl2Freq"
                ? QJsonValue(QJsonObject{{"radioSetting", "hl2/cl2Enable"},
                                         {"oneOf", QJsonArray{true}}})
                : QJsonValue(QJsonValue::Undefined));
        }
        QCOMPARE(QJsonValue(rows.at(3).toObject().value("default")), QJsonValue(116));
        QCOMPARE(rows.at(3).toObject().value("min"), QJsonValue(1));
        QCOMPARE(rows.at(3).toObject().value("max"), QJsonValue(200));

        // The resource's clock rows are closed, the dependency included.
        const QList<QJsonObject> resource = resourceRows(QStringLiteral("hardware"), 18);
        QCOMPARE(resource.size(), 3);
        for (const QJsonObject& row : resource) {
            QVERIFY2(SetupDescriptionService::validateHardwareV18Control(row),
                     qPrintable(row.value("id").toString()));
            for (const QJsonObject& changed : mutationsOf(row)) {
                QVERIFY2(!SetupDescriptionService::validateHardwareV18Control(changed),
                         qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
            }
            QJsonObject dependent = row;
            dependent.insert("enabledWhen", QJsonObject{{"radioSetting", "hl2/ext10MHz"},
                                                        {"oneOf", QJsonArray{true}}});
            QVERIFY(!SetupDescriptionService::validateHardwareV18Control(dependent));
        }

        // Versions 19 to 22 read version 18.
        QCOMPARE(projectedCategory(service.hardware(), 19), current);
        QCOMPARE(projectedCategory(service.hardware(), 22), current);
    }

    // Version 23: Calibration gains the desktop tab's Level Cal "Rx1 6m
    // LNA" row, bound to the per-radio key the Core applies to its receive
    // calibration, with the tab's range, step, places and default (Thetis
    // setup.designer.cs:12089-12116 [v2.10.3.15] ud6mLNAGainOffset: 0..25,
    // step 1, one decimal, 13). It is on every board, as the tab is. A peer
    // below 23 reads version 18 without it; a later declaration is capped
    // at 23.
    void hardwareV23AddsRx1SixMeterLnaRow()
    {
        for (const auto& [board, model] : {std::pair{HPSDRHW::HermesLite, HPSDRModel::HERMESLITE},
                                           std::pair{HPSDRHW::OrionMKII, HPSDRModel::ANAN7000D}}) {
            SetupDescriptionService service;
            service.setRadioContext(BoardCapsTable::forBoard(board), model, RadioInfo{});
            const QJsonObject current = projectedCategory(service.hardware(), 23);
            QCOMPARE(current.value("version"), QJsonValue(23));
            const QJsonArray sections = pageById(current, "hardware.calibration")
                .value("sections").toArray();
            QCOMPARE(sections.size(), 2);
            const QJsonObject levelCal = sections.at(0).toObject();
            QCOMPARE(levelCal.value("title"), QJsonValue("Level Cal"));
            QCOMPARE(sections.at(1).toObject().value("title"), QJsonValue("TX Display Cal"));
            const QJsonArray rows = levelCal.value("controls").toArray();
            QCOMPARE(rows.size(), 1);
            const QJsonObject row = rows.first().toObject();
            QCOMPARE(row, (QJsonObject{
                {"id", "hardware.calibration.rx1_6mLna"},
                {"label", "Rx1 6m LNA:"},
                {"tooltip", ""},
                {"kind", "decimal"},
                {"binding", QJsonObject{{"radioSetting", "cal/rx1_6mLna"}}},
                {"applies", "live"},
                {"gate", QJsonObject{{"capability", "radioHardwareVersion"}, {"min", 1}}},
                {"requiresDescriptionVersion", 23},
                {"min", 0}, {"max", 25}, {"step", 1}, {"decimals", 1},
                {"unit", "dB"}, {"default", 13}}));
            QVERIFY(SetupDescriptionService::validateHardwareV23Control(row));

            // Below 23: version 18 exactly, without the row.
            const QJsonObject older = projectedCategory(service.hardware(), 22);
            QCOMPARE(older.value("version"), QJsonValue(18));
            QJsonObject expected = withoutRowsOf(current, 23);
            expected.insert("version", 18);
            QCOMPARE(older, expected);
            QCOMPARE(projectedCategory(service.hardware(), 99), current);
        }

        // The resource's row is closed.
        const QList<QJsonObject> resource = resourceRows(QStringLiteral("hardware"), 23);
        QCOMPARE(resource.size(), 1);
        for (const QJsonObject& row : resource) {
            QVERIFY(SetupDescriptionService::validateHardwareV23Control(row));
            for (const QJsonObject& changed : mutationsOf(row)) {
                QVERIFY2(!SetupDescriptionService::validateHardwareV23Control(changed),
                         qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
            }
        }
    }

    // Version 24 (radio codec lane): TX Input's Line In Gain moves in the
    // 1.5 dB steps of Thetis's udLineInBoost with one decimal
    // (setup.designer.cs:47006-47034 [v2.10.3.15]), and the Saturn G2 group
    // gains Mic Tip-Ring (Thetis enables the ORION Tip / Ring panel on the
    // G2, setup.cs:20292). A peer below 24 reads version 15: Line In Gain in
    // whole decibels from -34, no Tip-Ring row. A later declaration is
    // capped at 24.
    void audioV24LineInStepsAndSaturnTipRing()
    {
        const auto sectionOf = [](const QJsonObject& category, const QString& title) {
            for (const QJsonValue& section : pageById(category, "audio.txInput")
                                                 .value("sections").toArray()) {
                if (section.toObject().value("title") == QJsonValue(title)) {
                    return section.toObject();
                }
            }
            return QJsonObject{};
        };
        const QJsonObject lineInGain{
            {"id", "audio.txInput.hermesLineInGain"}, {"label", "Line In Gain:"},
            {"tooltip", ""}, {"kind", "decimal"},
            {"binding", QJsonObject{{"property", QJsonObject{{"object", "transmit"},
                                                             {"name", "lineInBoost"}}}}},
            {"applies", "live"}, {"requiresDescriptionVersion", 24},
            {"gate", QJsonObject{{"capability", "transmitSettingsVersion"}, {"min", 3},
                                 {"transmit", true}}},
            {"min", -34.5}, {"max", 12}, {"step", 1.5}, {"decimals", 1}, {"unit", "dB"}};
        const QJsonObject tipRing{
            {"id", "audio.txInput.saturnMicTipRing"}, {"label", "Mic Tip-Ring (Tip is Mic)"},
            {"tooltip", ""}, {"kind", "toggle"},
            {"binding", QJsonObject{{"property", QJsonObject{{"object", "transmit"},
                                                             {"name", "micTipRing"}}}}},
            {"applies", "live"}, {"requiresDescriptionVersion", 24},
            {"gate", QJsonObject{{"capability", "transmitSettingsVersion"}, {"min", 3},
                                 {"transmit", true}}}};

        {
            SetupDescriptionService service;
            service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes),
                                    HPSDRModel::HERMES, RadioInfo{});
            const QJsonObject current = projectedCategory(service.audio(), 24);
            QCOMPARE(current.value("version"), QJsonValue(24));
            QCOMPARE(controlById(current, "audio.txInput.hermesLineInGain"), lineInGain);
            QVERIFY(SetupDescriptionService::validateAudioV24Control(lineInGain));
            QCOMPARE(projectedCategory(service.audio(), 99), current);

            // Versions 15 to 23 read version 15: whole decibels from -34.
            const QJsonObject older = projectedCategory(service.audio(), 23);
            QCOMPARE(older.value("version"), QJsonValue(15));
            QJsonObject olderRow = lineInGain;
            olderRow.insert("requiresDescriptionVersion", 15);
            olderRow.insert("min", -34);
            olderRow.insert("step", 1);
            olderRow.remove("decimals");
            QCOMPARE(controlById(older, "audio.txInput.hermesLineInGain"), olderRow);
            QString why;
            QVERIFY2(SetupDescriptionV15::validateControl("audio", olderRow, &why),
                     qPrintable(why));
            QCOMPARE(projectedCategory(service.audio(), 15), older);
        }
        {
            SetupDescriptionService service;
            service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Saturn),
                                    HPSDRModel::ANAN_G2, RadioInfo{});
            const QJsonObject current = projectedCategory(service.audio(), 24);
            const QJsonArray rows = sectionOf(current, "Radio Mic (Saturn G2)")
                .value("controls").toArray();
            QCOMPARE(rows.size(), 5);
            QCOMPARE(rows.at(0).toObject().value("id"), QJsonValue("audio.txInput.saturnMicXlr"));
            QCOMPARE(rows.at(1).toObject(), tipRing);
            QVERIFY(SetupDescriptionService::validateAudioV24Control(tipRing));
            // Below 24 the Saturn group is version 15's four rows.
            const QJsonObject older = projectedCategory(service.audio(), 23);
            QCOMPARE(sectionOf(older, "Radio Mic (Saturn G2)").value("controls").toArray().size(), 4);
            QVERIFY(controlById(older, "audio.txInput.saturnMicTipRing").isEmpty());
        }

        // The resource's two rows are closed.
        const QList<QJsonObject> resource = resourceRows(QStringLiteral("audio"), 24);
        QCOMPARE(resource.size(), 2);
        for (const QJsonObject& row : resource) {
            QVERIFY(SetupDescriptionService::validateAudioV24Control(row));
            for (const QJsonObject& changed : mutationsOf(row)) {
                QVERIFY2(!SetupDescriptionService::validateAudioV24Control(changed),
                         qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
            }
        }
    }

    // Radio codec lane: Thetis greys out the ORION mic panel on the Red
    // Pitaya (setup.cs:20440-20445 [v2.10.3.15], //DH1KLM): its Orion rows
    // are sent disabled with the reason, at every version from 15; another
    // Orion-MkII radio's are open.
    void audioOrionMicRowsAreDisabledOnTheRedPitaya()
    {
        for (const auto& [model, open] : {std::pair{HPSDRModel::REDPITAYA, false},
                                          std::pair{HPSDRModel::ANAN7000D, true}}) {
            RadioModel radio;
            radio.setHpsdrModelForTest(model);
            SetupDescriptionService service;
            service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);
            for (int version : {15, 24}) {
                const QJsonObject audio = projectedCategory(service.audio(), version);
                for (const char* name : {"orionMicTipRing", "orionMicBias",
                                         "orionMicPttDisabled", "orionMicBoost"}) {
                    const QJsonObject row = controlById(
                        audio, QStringLiteral("audio.txInput.") + QLatin1String(name));
                    QVERIFY2(!row.isEmpty(), name);
                    QVERIFY(!row.contains("availableOn"));
                    if (open) {
                        QVERIFY(!row.contains("availability"));
                    } else {
                        QCOMPARE(row.value("availability"), QJsonValue(QJsonObject{
                            {"enabled", false},
                            {"reason", "These mic settings do not apply to the Red Pitaya."}}));
                    }
                }
            }
        }
    }

    // Radio codec lane: the Hermes Lite 2 takes the Hermes group's rows
    // through its AK4951 audio add-on board, which its gateware cannot
    // report: the section is sent, titled for the HL2, each row with the
    // add-on note. The receive-only kit (no add-on) has no such section.
    void audioHermesRowsReachTheHermesLite2WithTheAddOnNote()
    {
        RadioModel radio;
        radio.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        QVERIFY(radio.boardCapabilities().radioMicNeedsAddOn);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);
        for (int version : {15, 24}) {
            const QJsonObject audio = projectedCategory(service.audio(), version);
            QJsonObject hermes;
            for (const QJsonValue& section : pageById(audio, "audio.txInput")
                                                 .value("sections").toArray()) {
                if (section.toObject().value("title")
                    == QJsonValue("Radio Mic (Hermes Lite 2)")) {
                    hermes = section.toObject();
                }
            }
            const QJsonArray rows = hermes.value("controls").toArray();
            QCOMPARE(rows.size(), 3);
            for (const QJsonValue& row : rows) {
                QCOMPARE(row.toObject().value("tooltip"), QJsonValue(
                    "Needs the Hermes Lite 2 audio add-on board. "
                    "A stock Hermes Lite 2 sends no mic audio."));
            }
            QVERIFY(controlById(audio, "audio.txInput.orionMicTipRing").isEmpty());
            QVERIFY(controlById(audio, "audio.txInput.saturnMicXlr").isEmpty());
        }

        BoardCapabilities kit = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        kit.radioMicNeedsAddOn = false;
        SetupDescriptionService kitService;
        kitService.setRadioContext(kit, HPSDRModel::HERMESLITE);
        QVERIFY(controlById(projectedCategory(kitService.audio(), 24),
                            "audio.txInput.hermesLineIn").isEmpty());
    }

    // Version 19 (R-R3-49): DSP > CFC's band editor, bound to transmit's
    // cfcProfile and applied with cfc.setProfile (transmitSettingsVersion
    // 15). The CFC page is complete to a version 19 peer; versions 15 to
    // 18 see version 15's category unchanged.
    void dspV19DescribesCfcBands()
    {
        SetupDescriptionService service;
        const QJsonObject current = projectedCategory(service.dsp(), 19);
        QCOMPARE(current.value("version"), QJsonValue(19));
        QCOMPARE(current.value("coverage"),
                 QJsonValue("partial: the NR3 and NNR model files are not described"));
        const QJsonObject cfc = pageById(current, "dsp.cfc");
        QVERIFY(!cfc.contains("coverage"));
        const QJsonArray sections = cfc.value("sections").toArray();
        QJsonObject cfcSection;
        for (const QJsonValue& raw : sections) {
            if (raw.toObject().value("title") == QJsonValue("CFC")) { cfcSection = raw.toObject(); }
        }
        const QJsonArray rows = cfcSection.value("controls").toArray();
        QVERIFY(!rows.isEmpty());
        const QJsonObject row = rows.last().toObject();
        QCOMPARE(row.value("id"), QJsonValue("dsp.cfc.bands"));
        QCOMPARE(row.value("kind"), QJsonValue("table"));
        QCOMPARE(row.value("requiresDescriptionVersion"), QJsonValue(19));
        QCOMPARE(row.value("binding"), QJsonValue(QJsonObject{{"cfcProfile", QJsonObject{
            {"object", "transmit"}, {"name", "cfcProfile"}, {"command", "cfc.setProfile"}}}}));
        // No off-air rule: Thetis's frmCFCConfig applies a change on the air
        // (frmCFCConfig.cs:333-392 [v2.10.3.15] has no MOX check), and a Core
        // at transmitSettingsVersion 13 or later takes it on the air from a
        // session permitted to change transmit settings.
        QCOMPARE(row.value("gate"), QJsonValue(QJsonObject{
            {"capability", "transmitSettingsVersion"}, {"min", 15}}));
        QVERIFY(!row.value("gate").toObject().contains("offAir"));
        QCOMPARE(row.value("bandCounts"), QJsonValue(QJsonArray{5, 10, 18}));
        QCOMPARE(row.value("minSpanHz"), QJsonValue(1000));
        // The ranges CfcProfile::fromPublishedJson takes, in its keys.
        const auto range = [](const QJsonArray& list, const QString& id) {
            for (const QJsonValue& raw : list) {
                const QJsonObject o = raw.toObject();
                if (o.value("id") == QJsonValue(id)) {
                    return QList<double>{o.value("min").toDouble(-1e9), o.value("max").toDouble(-1e9),
                                         o.value("step").toDouble(-1e9)};
                }
            }
            return QList<double>{};
        };
        const QJsonArray fields = row.value("fields").toArray();
        QCOMPARE(range(fields, "minHz"), (QList<double>{0, 20000, 1}));
        QCOMPARE(range(fields, "maxHz"), (QList<double>{0, 20000, 1}));
        QCOMPARE(range(fields, "precompDb"), (QList<double>{0, 16, 0.1}));
        QCOMPARE(range(fields, "postEqGainDb"), (QList<double>{-24, 24, 0.1}));
        QStringList fieldIds;
        for (const QJsonValue& f : fields) { fieldIds << f.toObject().value("id").toString(); }
        QCOMPARE(fieldIds, (QStringList{"minHz", "maxHz", "parametric", "precompDb",
                                        "postEqGainDb"}));
        const QJsonArray columns = row.value("columns").toArray();
        QStringList columnIds;
        for (const QJsonValue& c : columns) { columnIds << c.toObject().value("id").toString(); }
        QCOMPARE(columnIds, (QStringList{"frequencyHz", "compressionDb", "compressionQ",
                                         "postEqGainDb", "postEqQ"}));
        QCOMPARE(range(columns, "frequencyHz"), (QList<double>{0, 20000, 1}));
        QCOMPARE(range(columns, "compressionDb"), (QList<double>{0, 16, 0.1}));
        QCOMPARE(range(columns, "compressionQ"), (QList<double>{0.2, 20, 0.01}));
        QCOMPARE(range(columns, "postEqGainDb"), (QList<double>{-24, 24, 0.1}));
        QCOMPARE(range(columns, "postEqQ"), (QList<double>{0.2, 20, 0.01}));

        // The resource row is closed.
        const QList<QJsonObject> resource = resourceRows(QStringLiteral("dsp"), 19);
        QCOMPARE(resource.size(), 1);
        QVERIFY(SetupDescriptionService::validateDspV19Control(resource.first()));
        QCOMPARE(resource.first(), row);
        for (const QJsonObject& changed : mutationsOf(row)) {
            QVERIFY2(!SetupDescriptionService::validateDspV19Control(changed),
                     qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
        }

        // Versions 15 to 18 see version 15's category, without the row.
        const QJsonObject v15 = projectedCategory(service.dsp(), 15);
        QCOMPARE(v15.value("version"), QJsonValue(15));
        QCOMPARE(v15.value("coverage"), QJsonValue(
            "partial: the NR3 and NNR model files and the per-band CFC editor are not described"));
        QCOMPARE(pageById(v15, "dsp.cfc").value("coverage"),
                 QJsonValue("partial: the per-band CFC editor is not described"));
        QVERIFY(controlById(v15, "dsp.cfc.bands").isEmpty());
        QCOMPARE(controlCount(v15, 0), controlCount(current, 0) - 1);
        QCOMPARE(projectedCategory(service.dsp(), 16), v15);
        QCOMPARE(projectedCategory(service.dsp(), 17), v15);
        QCOMPARE(projectedCategory(service.dsp(), 18), v15);
        QCOMPARE(projectedCategory(service.dsp(), 20), current);
    }

    // The TX Leveler, TX ALC, Phase Rotator, CFC and CESSB rows carry no
    // off-air rule: the Core serving this description takes transmit
    // settings on the air (transmitSettingsVersion 13 or later) from a
    // session permitted to change them, as the desktop does.
    void dspTransmitProcessingRowsCarryNoOffAirRule()
    {
        SetupDescriptionService service;
        const QStringList ids{
            "dsp.agcAlc.txLevelerOn", "dsp.agcAlc.txLevelerMaxGain",
            "dsp.agcAlc.txLevelerDecay", "dsp.agcAlc.txAlcMaxGain", "dsp.agcAlc.txAlcDecay",
            "dsp.cfc.phaseRotatorEnabled", "dsp.cfc.phaseRotatorFreqHz",
            "dsp.cfc.phaseRotatorStages", "dsp.cfc.phaseReverseEnabled", "dsp.cfc.cfcEnabled",
            "dsp.cfc.cfcPostEqEnabled", "dsp.cfc.cfcPrecompDb", "dsp.cfc.cfcPostEqGainDb",
            "dsp.cfc.cessbOn"};
        for (const int version : {3, 15, 19}) {
            const QJsonObject dsp = projectedCategory(service.dsp(), version);
            for (const QString& id : ids) {
                const QJsonObject row = controlById(dsp, id);
                QVERIFY2(!row.isEmpty(), qPrintable(QStringLiteral("%1 at %2").arg(id).arg(version)));
                QCOMPARE(row.value("gate"), QJsonValue(QJsonObject{
                    {"capability", "transmitSettingsVersion"}, {"min", 4}}));
                QVERIFY(SetupDescriptionService::validateActiveSlicePropertyBinding(row));
                // The off-air rule is refused: the row is closed without it.
                QJsonObject offAir = row;
                QJsonObject gate = row.value("gate").toObject();
                gate.insert("offAir", true);
                offAir.insert("gate", gate);
                QVERIFY2(!SetupDescriptionService::validateActiveSlicePropertyBinding(offAir),
                         qPrintable(id));
            }
        }
        QVERIFY(!controlById(projectedCategory(service.dsp(), 19), "dsp.cfc.bands")
                     .value("gate").toObject().contains("offAir"));
    }

    // The off-air sweep: the Core has taken these transmit settings on the
    // air since transmitSettingsVersion 13, and Thetis disables none of them
    // during MOX (setup.cs:5132-5161 [v2.10.3.15]), so they carry no off-air
    // rule at any description version. Thetis greys only grpDSPBufferSize
    // (setup.cs:5159 [v2.10.3.15]), so the TX buffer sizes keep theirs.
    void transmitSettingsTakenOnTheAirCarryNoOffAirRule()
    {
        RadioModel radio;
        radio.setHpsdrModelForTest(HPSDRModel::ANAN_G2E);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), radio.hardwareProfile().model);

        QStringList audioIds;
        for (const char* name : {"micGain", "hermesLineIn", "hermesMicBoost", "hermesLineInGain",
                                 "orionMicTipRing", "orionMicBias", "orionMicPttDisabled",
                                 "orionMicBoost", "saturnMicXlr", "saturnMicPttDisabled",
                                 "saturnMicBias", "saturnMicBoost"}) {
            audioIds << QStringLiteral("audio.txInput.") + QLatin1String(name);
        }
        for (const char* name : {"activeProfile", "save", "delete", "filterLow", "filterHigh",
                                 "amCarrierLevel"}) {
            audioIds << QStringLiteral("audio.txProfile.") + QLatin1String(name);
        }
        QStringList transmitIds;
        for (const char* name : {"power", "attOnTx", "attOnTxValue", "forceAttWhenPsOff",
                                 "tuneDriveSource", "fixedTunePower", "SwrProtectionEnabled",
                                 "SwrProtectionLimit", "SwrTuneProtectionEnabled",
                                 "TunePowerSwrIgnore", "WindBackPowerSwr",
                                 "TxInhibitMonitorEnabled", "TxInhibitMonitorReversed"}) {
            transmitIds << QStringLiteral("transmit.power.") + QLatin1String(name);
        }
        for (const char* name : {"dexpEnabled", "dexpAttackTimeMs", "voxHangTimeMs",
                                 "dexpReleaseTimeMs", "voxThresholdDb", "dexpExpansionRatioDb",
                                 "dexpHysteresisRatioDb", "dexpDetectorTauMs",
                                 "dexpLookAheadEnabled", "dexpLookAheadMs",
                                 "dexpSideChannelFilterEnabled", "dexpLowCutHz", "dexpHighCutHz",
                                 "antiVoxRun", "antiVoxGainDb", "antiVoxTauMs"}) {
            transmitIds << QStringLiteral("transmit.dexpVox.") + QLatin1String(name);
        }
        QStringList dspIds;
        QStringList bufferIds;
        for (const char* mode : {"Phone", "Fm", "Dig"}) {
            dspIds << QStringLiteral("dsp.options.DspOptionsFilterSize%1Tx").arg(QLatin1String(mode))
                   << QStringLiteral("dsp.options.DspOptionsFilterType%1Tx").arg(QLatin1String(mode));
            bufferIds << QStringLiteral("dsp.options.DspOptionsBufferSize%1Tx").arg(QLatin1String(mode));
        }
        QStringList paIds{QStringLiteral("pa.gain.bypassPaSettings")};
        for (int point = 1; point <= 10; ++point) {
            paIds << QStringLiteral("pa.wattMeter.calPoint%1").arg(point);
        }
        QCOMPARE(audioIds.size() + transmitIds.size() + dspIds.size() + paIds.size(),
                 18 + 29 + 6 + 11);

        const auto withOffAir = [](QJsonObject row) {
            QJsonObject gate = row.value("gate").toObject();
            gate.insert("offAir", true);
            row.insert("gate", gate);
            return row;
        };
        QSet<QString> seen;
        const auto check = [&](const QString& description, const QStringList& ids, int version) {
            const QJsonObject category = projectedCategory(description, version);
            for (const QString& id : ids) {
                const QJsonObject row = controlById(category, id);
                const QString where = QStringLiteral("%1 at %2").arg(id).arg(version);
                if (row.isEmpty()) {
                    continue;   // another board family's row; see `seen` below
                }
                seen.insert(where);
                QVERIFY2(!row.value("gate").toObject().contains("offAir"), qPrintable(where));
                // The closed validators refuse the row with the rule put back.
                const QJsonObject locked = withOffAir(row);
                if (SetupDescriptionService::validateTransmitPropertyBinding(row)) {
                    QVERIFY2(!SetupDescriptionService::validateTransmitPropertyBinding(locked),
                             qPrintable(where));
                }
                if (SetupDescriptionService::validateTransmitSettingBinding(row)) {
                    QVERIFY2(!SetupDescriptionService::validateTransmitSettingBinding(locked),
                             qPrintable(where));
                }
                if (SetupDescriptionService::validateAudioPropertyBinding(row)) {
                    QVERIFY2(!SetupDescriptionService::validateAudioPropertyBinding(locked),
                             qPrintable(where));
                }
                if (SetupDescriptionService::validateDspSettingBinding(row)) {
                    QVERIFY2(!SetupDescriptionService::validateDspSettingBinding(locked),
                             qPrintable(where));
                }
                if (SetupDescriptionService::validatePaBypassBinding(row)) {
                    QVERIFY2(!SetupDescriptionService::validatePaBypassBinding(locked),
                             qPrintable(where));
                }
            }
        };
        for (const int version : {15, 19, 20}) {
            check(service.transmit(), transmitIds, version);
            check(service.dsp(), dspIds, version);
            check(service.pa(), paIds, version);
        }
        // The TX Input page describes the connected radio's Radio Mic
        // family only: read it on a Hermes, an Orion-MkII and a Saturn.
        for (const HPSDRHW board : {HPSDRHW::Hermes, HPSDRHW::OrionMKII, HPSDRHW::Saturn}) {
            SetupDescriptionService family;
            family.setRadioContext(BoardCapsTable::forBoard(board), radio.hardwareProfile().model);
            for (const int version : {15, 19, 20}) {
                check(family.audio(), audioIds, version);
            }
        }
        for (const int version : {15, 19, 20}) {
            for (const QStringList* ids : {&audioIds, &transmitIds, &dspIds, &paIds}) {
                for (const QString& id : *ids) {
                    QVERIFY2(seen.contains(QStringLiteral("%1 at %2").arg(id).arg(version)),
                             qPrintable(QStringLiteral("%1 at %2 is not described")
                                            .arg(id).arg(version)));
                }
            }
        }
        // The closed validators accept each family's row without the rule.
        const QJsonObject current = projectedCategory(service.transmit(), 20);
        QVERIFY(SetupDescriptionService::validateTransmitPropertyBinding(
            controlById(current, "transmit.dexpVox.dexpEnabled")));
        QVERIFY(SetupDescriptionService::validateTransmitSettingBinding(
            controlById(current, "transmit.power.SwrProtectionEnabled")));
        QVERIFY(SetupDescriptionService::validateAudioPropertyBinding(
            controlById(projectedCategory(service.audio(), 20), "audio.txProfile.filterLow")));
        QVERIFY(SetupDescriptionService::validatePaBypassBinding(
            controlById(projectedCategory(service.pa(), 20), "pa.gain.bypassPaSettings")));

        // The TX buffer sizes keep the rule, and the validator requires it.
        for (const int version : {15, 19, 20}) {
            const QJsonObject dsp = projectedCategory(service.dsp(), version);
            for (const QString& id : bufferIds) {
                const QJsonObject row = controlById(dsp, id);
                const QString where = QStringLiteral("%1 at %2").arg(id).arg(version);
                QVERIFY2(!row.isEmpty(), qPrintable(where));
                QCOMPARE(row.value("gate").toObject().value("offAir"), QJsonValue(true));
                QVERIFY2(SetupDescriptionService::validateDspSettingBinding(row), qPrintable(where));
                QJsonObject unlocked = row;
                QJsonObject gate = unlocked.value("gate").toObject();
                gate.remove("offAir");
                unlocked.insert("gate", gate);
                QVERIFY2(!SetupDescriptionService::validateDspSettingBinding(unlocked),
                         qPrintable(where));
            }
        }
    }

    // Version 13 (R-R3-49): Transmit > Power's "Disable HF PA", which the
    // Core applies on and off the air (transmitSettingsVersion 11), in its
    // own PA Control section after External TX Inhibit, as the desktop page
    // orders its groups. Older peers keep the rows and coverage they had.
    void transmitV13DescribesDisableHfPa()
    {
        SetupDescriptionService service;
        const QJsonObject current = projectedCategory(service.transmit(), 13);
        QCOMPARE(current.value("version"), QJsonValue(13));
        const QJsonObject power = pageById(current, "transmit.power");
        QCOMPARE(power.value("coverage"), QJsonValue(
            "partial: ATT on TX uses the stepAtt facade; Tune fixed drive has SKU-dependent "
            "display conversion; TX TUN Meter has no Core apply binding"));
        const QJsonArray sections = power.value("sections").toArray();
        QCOMPARE(sections.last().toObject().value("title"), QJsonValue("PA Control"));
        const QJsonArray rows = sections.last().toObject().value("controls").toArray();
        QCOMPARE(rows.size(), 1);
        const QJsonObject row = rows.first().toObject();
        QCOMPARE(row.value("id"), QJsonValue("transmit.power.DisableHfPa"));
        QCOMPARE(row.value("label"), QJsonValue("Disable HF PA"));
        QCOMPARE(row.value("tooltip"), QJsonValue("Disables HF PA."));
        QCOMPARE(row.value("binding"), QJsonValue(QJsonObject{{"setting", "DisableHfPa"}}));
        QCOMPARE(QString::fromUtf8(RadioModel::kDisableHfPaKey), QStringLiteral("DisableHfPa"));
        QCOMPARE(row.value("gate"), QJsonValue(QJsonObject{
            {"capability", "transmitSettingsVersion"}, {"min", 11}, {"transmit", true}}));
        QCOMPARE(row.value("default"), QJsonValue(false));
        QVERIFY(SetupDescriptionService::validateSettingToggleEncoding(row));
        QVERIFY(!row.contains("availability"));
        QVERIFY(SetupDescriptionService::validateTransmitV13Control(row));
        QVERIFY(!SetupDescriptionService::validateTransmitSettingBinding(row));
        for (const QJsonObject& changed : mutationsOf(row)) {
            QVERIFY2(!SetupDescriptionService::validateTransmitV13Control(changed),
                     qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
        }
        QJsonObject offAir = row;
        QJsonObject gate = offAir.value("gate").toObject();
        gate.insert("offAir", true);
        offAir.insert("gate", gate);
        QVERIFY(!SetupDescriptionService::validateTransmitV13Control(offAir));

        // A peer before 13: the rows it was built for, version 3 as before,
        // and the coverage it was sent.
        for (int version = 3; version <= 12; ++version) {
            const QJsonObject older = projectedCategory(service.transmit(), version);
            QCOMPARE(older.value("version"), QJsonValue(3));
            QVERIFY(!QJsonDocument(older).toJson().contains("DisableHfPa"));
            QCOMPARE(pageById(older, "transmit.power").value("coverage"), QJsonValue(
                "partial: ATT on TX uses the stepAtt facade; Tune fixed drive has SKU-dependent "
                "display conversion; TX TUN Meter and Disable HF PA have no Core apply binding"));
        }

        // A radio with no HF PA switch keeps the row, disabled with the
        // desktop's reason; the others have it open.
        for (const HPSDRModel model : {HPSDRModel::HERMES, HPSDRModel::HPSDR}) {
            service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes), model);
            const QJsonObject closed = rowsOf(pageById(projectedCategory(service.transmit(), 13),
                                                       "transmit.power")).last().toObject();
            QCOMPARE(closed.value("id"), QJsonValue("transmit.power.DisableHfPa"));
            QCOMPARE(closed.value("availability"), QJsonValue(QJsonObject{
                {"enabled", false}, {"reason", RadioModel::hfPaSwitchUnavailableReason()}}));
        }
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Saturn), HPSDRModel::ANAN_G2);
        QVERIFY(!rowsOf(pageById(projectedCategory(service.transmit(), 13), "transmit.power"))
                     .last().toObject().contains("availability"));
    }

    // Version 13 (R-R3-46, R-R3-49): the Alex receive filter rows the Core
    // applies (radioHardwareVersion 8), on the pages the desktop's Alex-1 and
    // Alex-2 Filters tabs are shown for, and Radio Info's sample rate, the
    // whole radio's (setRadioSampleRate, radioHardwareVersion 9).
    void hardwareV13DescribesAlexFilterRowsAndSampleRate()
    {
        RadioInfo info;
        info.name = QStringLiteral("ANAN-G2");
        info.macAddress = QStringLiteral("00:1C:C0:A2:12:34");
        info.protocol = ProtocolVersion::Protocol2;
        SetupDescriptionService service;
        const BoardCapabilities saturn = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        service.setRadioContext(saturn, HPSDRModel::ANAN_G2, info);
        const QJsonObject g2 = projectedCategory(service.hardware(), 13);
        QStringList pageIds;
        for (const QJsonValue& page : g2.value("pages").toArray()) {
            pageIds << page.toObject().value("id").toString();
        }
        QCOMPARE(pageIds, (QStringList{"hardware.radioInfo", "hardware.antennaAlex",
                                       "hardware.alex1Filters", "hardware.alex2Filters",
                                       "hardware.calibration"}));
        const QJsonObject alex1 = pageById(g2, "hardware.alex1Filters");
        QCOMPARE(alex1.value("title"), QJsonValue("Alex-1 Filters"));
        const QJsonArray alex1Sections = alex1.value("sections").toArray();
        // The G2 is a BPF-panel model: Thetis hides the Alex HPF panel and
        // moves the five switches into the BPF panel (setup.cs:6336-6360
        // and 20313-20325 [v2.10.3.15]), so the page has one section.
        QCOMPARE(alex1Sections.size(), 1);
        QCOMPARE(alex1Sections.at(0).toObject().value("title"), QJsonValue("Saturn BPF1 Bands"));
        const codec::alex::AlexHpfEdges defaults = codec::alex::AlexHpfEdges::thetisDefaults();
        const QStringList slugs{"1_5MHz", "6_5MHz", "9_5MHz", "13MHz", "20MHz", "6mBP"};
        const auto checkBank = [&slugs](const QJsonArray& rows, const QString& idBase,
                                        const QString& keyBase, const QStringList& labels,
                                        const codec::alex::AlexHpfRows& edges) {
            QCOMPARE(rows.size(), 18);
            for (int i = 0; i < 6; ++i) {
                const QJsonObject bypass = rows.at(3 * i).toObject();
                const QJsonObject start = rows.at(3 * i + 1).toObject();
                const QJsonObject end = rows.at(3 * i + 2).toObject();
                const QString id = idBase + slugs.at(i);
                const QString key = keyBase + slugs.at(i);
                QCOMPARE(bypass.value("id"), QJsonValue(id + ".bypass"));
                QCOMPARE(bypass.value("label"), QJsonValue(labels.at(i) + " Bypass"));
                QCOMPARE(bypass.value("binding"), QJsonValue(QJsonObject{{"radioSetting",
                                                                         key + "/enabled"}}));
                QCOMPARE(bypass.value("default"), QJsonValue(false));
                QVERIFY(SetupDescriptionService::validateSettingToggleEncoding(bypass));
                QCOMPARE(start.value("label"), QJsonValue(labels.at(i) + " Start"));
                QCOMPARE(start.value("binding"), QJsonValue(QJsonObject{{"radioSetting",
                                                                        key + "/start"}}));
                QCOMPARE(start.value("default").toDouble(), edges[size_t(i)].startMhz);
                QCOMPARE(end.value("label"), QJsonValue(labels.at(i) + " End"));
                QCOMPARE(end.value("binding"), QJsonValue(QJsonObject{{"radioSetting",
                                                                      key + "/end"}}));
                QCOMPARE(end.value("default").toDouble(), edges[size_t(i)].endMhz);
                for (const QJsonObject& edge : {start, end}) {
                    QCOMPARE(edge.value("kind"), QJsonValue("decimal"));
                    QCOMPARE(edge.value("min").toDouble(), 0.0);
                    QCOMPARE(edge.value("max").toDouble(), 200.0);
                    QCOMPARE(edge.value("step").toDouble(), 0.001);
                    QCOMPARE(edge.value("decimals"), QJsonValue(6));
                    QCOMPARE(edge.value("unit"), QJsonValue("MHz"));
                }
                for (const QJsonObject& row : {bypass, start, end}) {
                    // No off-air rule: Thetis's per-row setters have no MOX check.
                    QCOMPARE(row.value("gate"), QJsonValue(QJsonObject{
                        {"capability", "radioHardwareVersion"}, {"min", 8}}));
                    QCOMPARE(row.value("requiresDescriptionVersion"), QJsonValue(13));
                }
            }
        };
        const QStringList hpfLabels{"1.5 MHz HPF", "6.5 MHz HPF", "9.5 MHz HPF", "13 MHz HPF",
                                    "20 MHz HPF", "6m Bypass"};
        // The five switches above the rows, in the desktop's order, each a
        // True/False radioSetting under alex/master (radioHardwareVersion 8,
        // no off-air rule). The three the Core counts as transmit hardware
        // (isTransmitHardwareKey) carry the transmit gate.
        QJsonArray alex1Rows = alex1Sections.at(0).toObject().value("controls").toArray();
        QCOMPARE(alex1Rows.size(), 5 + 18);
        QJsonArray switchRows;
        struct Switch { const char* field; const char* label; bool transmit; bool on; };
        const Switch switches[] = {
            {"hpfBypass", "HPF Bypass (master)", false, false},
            {"hpfBypassOnTx", "HPF Bypass on TX", true, false},
            {"hpfBypassOnPs", "HPF Bypass on PureSignal feedback", true, true},
            {"disable6mLnaOnTx", "Disable 6m LNA on TX", true, true},
            {"disable6mLnaOnRx", "Disable 6m LNA on RX", false, false}};
        for (const Switch& expected : switches) {
            const QJsonObject row = alex1Rows.takeAt(0).toObject();
            switchRows.append(row);
            const QString field = QString::fromLatin1(expected.field);
            QCOMPARE(row.value("id"), QJsonValue("hardware.alex1Filters." + field));
            QCOMPARE(row.value("label"), QJsonValue(QString::fromLatin1(expected.label)));
            QCOMPARE(row.value("tooltip"), QJsonValue(""));
            QCOMPARE(row.value("kind"), QJsonValue("toggle"));
            QCOMPARE(row.value("binding"), QJsonValue(QJsonObject{{"radioSetting",
                                                                   "alex/master/" + field}}));
            QVERIFY(SetupDescriptionService::validateSettingToggleEncoding(row));
            QCOMPARE(row.value("default"), QJsonValue(expected.on));
            QJsonObject gate{{"capability", "radioHardwareVersion"}, {"min", 8}};
            if (expected.transmit) {
                gate.insert("transmit", true);
            }
            QCOMPARE(row.value("gate"), QJsonValue(gate));
            QCOMPARE(row.value("requiresDescriptionVersion"), QJsonValue(13));
            // The desktop asks before clearing HPF Bypass on PureSignal
            // feedback (the IMD warning); the description carries the same
            // question and the value it guards. No other switch asks.
            if (field == QLatin1String("hpfBypassOnPs")) {
                QCOMPARE(row.value("confirm"), QJsonValue(QStringLiteral(
                    "Including the BPFs during a PureSignal transmission may "
                    "produce passive Inter-Modulation Distortion in the "
                    "inductors of the bandpass filters.\n\n"
                    "You will NOT be able to observe this degraded performance "
                    "on the panadapter because PS is correcting to the distorted "
                    "feedback and the panadapter is \"seeing\" that same "
                    "distorted feedback. It can only be observed with an "
                    "external spectrum analyzer.\n\n"
                    "Please ensure you understand the implications of including "
                    "the BPFs when transmitting a PureSignal based signal. "
                    "It is not recommended.")));
                QCOMPARE(row.value("confirmWhen"), QJsonValue(false));
                QCOMPARE(row.size(), 12);
            } else {
                QVERIFY(!row.contains("confirm"));
                QVERIFY(!row.contains("confirmWhen"));
                QCOMPARE(row.size(), 10);
            }
        }
        checkBank(alex1Rows, "hardware.alex1Filters.bpf1.", "alex/bpf1/",
                  QStringList{"160m BPF", "80/60m BPF", "40/30m BPF", "20/17/15m BPF",
                              "12/10m BPF", "6m BPF/LNA"}, defaults.bpf1);
        // The ANAN-200D (Orion board) is programmed through the Alex HPF
        // bank (usesBpf1Preselector false, as Thetis console.cs:6827-6837
        // [v2.10.3.15] routes Orion to setAlexHPF), so it keeps the Alex HPF
        // rows with the same five switches and no BPF1.
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Orion),
                                HPSDRModel::ANAN200D, info);
        const QJsonArray orionSections = pageById(projectedCategory(service.hardware(), 13),
                                                  "hardware.alex1Filters")
            .value("sections").toArray();
        QCOMPARE(orionSections.size(), 1);
        QCOMPARE(orionSections.at(0).toObject().value("title"), QJsonValue("Alex HPF Bands"));
        QJsonArray orionRows = orionSections.at(0).toObject().value("controls").toArray();
        QCOMPARE(orionRows.size(), 5 + 18);
        for (const QJsonValue& expected : switchRows) {
            QCOMPARE(orionRows.takeAt(0), expected);
        }
        checkBank(orionRows, "hardware.alex1Filters.hpf.", "alex/hpf/", hpfLabels, defaults.hpf);
        service.setRadioContext(saturn, HPSDRModel::ANAN_G2, info);
        const QJsonObject alex2 = pageById(g2, "hardware.alex2Filters");
        QCOMPARE(alex2.value("title"), QJsonValue("Alex-2 Filters"));
        QJsonArray alex2Rows = rowsOf(alex2);
        QCOMPARE(alex2Rows.size(), 19);
        const QJsonObject master = alex2Rows.takeAt(0).toObject();
        QCOMPARE(master.value("label"), QJsonValue("ByPass / 55 MHz BPF (master)"));
        QCOMPARE(master.value("binding"), QJsonValue(QJsonObject{
            {"radioSetting", "alex2/master/bypass55MhzBpf"}}));
        QCOMPARE(master.value("default"), QJsonValue(false));
        checkBank(alex2Rows, "hardware.alex2Filters.hpf.", "alex2/hpf/", hpfLabels,
                  defaults.alex2);

        // Radio Info's sample rate: the desktop's list for this radio, the
        // whole radio's rate through setRadioSampleRate, off the air.
        QJsonObject rate;
        for (const QJsonValue& raw : rowsOf(pageById(g2, "hardware.radioInfo"))) {
            if (raw.toObject().value("id") == QJsonValue("hardware.radioInfo.sampleRate")) {
                rate = raw.toObject();
            }
        }
        QCOMPARE(rate.value("label"), QJsonValue("Sample rate (Hz):"));
        QCOMPARE(rate.value("kind"), QJsonValue("choice"));
        QCOMPARE(rate.value("gate"), QJsonValue(QJsonObject{
            {"capability", "radioHardwareVersion"}, {"min", 9}, {"offAir", true}}));
        QVERIFY(SetupDescriptionService::validateCommandBinding(rate));
        QCOMPARE(rate.value("binding").toObject().value("command").toObject().value("verb"),
                 QJsonValue("setRadioSampleRate"));
        const std::vector<int> allowed = allowedSampleRates(ProtocolVersion::Protocol2, saturn,
                                                            HPSDRModel::ANAN_G2);
        QVERIFY(!allowed.empty());
        const QJsonArray options = rate.value("options").toArray();
        QCOMPARE(options.size(), qsizetype(allowed.size()));
        for (qsizetype i = 0; i < options.size(); ++i) {
            QCOMPARE(options.at(i).toObject(), (QJsonObject{
                {"value", allowed[size_t(i)]}, {"label", QString::number(allowed[size_t(i)])}}));
        }
        QVERIFY(!rate.contains("availability"));

        // An ALEX board without Alex-2 or BPF1: only the Alex HPF bank.
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes), HPSDRModel::ANAN100,
                                info);
        const QJsonObject hermes = projectedCategory(service.hardware(), 13);
        QVERIFY(pageById(hermes, "hardware.alex2Filters").isEmpty());
        const QJsonArray hermesSections = pageById(hermes, "hardware.alex1Filters")
            .value("sections").toArray();
        QCOMPARE(hermesSections.size(), 1);
        QCOMPARE(hermesSections.first().toObject().value("title"), QJsonValue("Alex HPF Bands"));
        // Every Alex model: the one Alex-1 bank the page describes is the
        // bank the Core programs for that model's board
        // (codec::alex::usesBpf1Preselector). The plain ORION MKII model is
        // on the OrionMKII board, so it gets BPF1, as Thetis programs it.
        for (int m = int(HPSDRModel::FIRST) + 1; m < int(HPSDRModel::LAST); ++m) {
            const HPSDRModel model = HPSDRModel(m);
            const HPSDRHW board = boardForModel(model);
            const BoardCapabilities caps = BoardCapsTable::forBoard(board);
            if (!caps.hasAlexFilters) {
                continue;
            }
            service.setRadioContext(caps, model, info);
            const QJsonArray sections = pageById(projectedCategory(service.hardware(), 13),
                                                 "hardware.alex1Filters")
                .value("sections").toArray();
            QVERIFY2(sections.size() == 1, qPrintable(QString::number(m)));
            QCOMPARE(sections.at(0).toObject().value("title"),
                     QJsonValue(codec::alex::usesBpf1Preselector(board)
                                    ? "Saturn BPF1 Bands" : "Alex HPF Bands"));
        }
        // The ANAN-G2E shows the BPF1 bank but has no Alex-2.
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::HermesC10),
                                HPSDRModel::ANAN_G2E, info);
        const QJsonObject g2e = projectedCategory(service.hardware(), 13);
        const QJsonArray g2eSections = pageById(g2e, "hardware.alex1Filters")
            .value("sections").toArray();
        QCOMPARE(g2eSections.size(), 1);
        QCOMPARE(g2eSections.at(0).toObject().value("title"), QJsonValue("Saturn BPF1 Bands"));
        QCOMPARE(pageById(g2e, "hardware.alex2Filters").isEmpty(),
                 !BoardCapsTable::forBoard(HPSDRHW::HermesC10).hasAlex2);
        // The ANAN-7000DLE and 8000DLE are BPF-panel models too: BPF1 with
        // the switches, no Alex HPF rows (Thetis setup.cs:20208-20220 and
        // 20260-20272 [v2.10.3.15]).
        for (const HPSDRModel dle : {HPSDRModel::ANAN7000D, HPSDRModel::ANAN8000D}) {
            service.setRadioContext(BoardCapsTable::forBoard(boardForModel(dle)), dle, info);
            const QJsonArray dleSections = pageById(projectedCategory(service.hardware(), 13),
                                                    "hardware.alex1Filters")
                .value("sections").toArray();
            QCOMPARE(dleSections.size(), 1);
            QCOMPARE(dleSections.at(0).toObject().value("title"),
                     QJsonValue("Saturn BPF1 Bands"));
            const QJsonArray dleRows = dleSections.at(0).toObject().value("controls").toArray();
            QCOMPARE(dleRows.size(), 5 + 18);
            QCOMPARE(dleRows.at(0).toObject().value("id"),
                     QJsonValue("hardware.alex1Filters.hpfBypass"));
            QCOMPARE(dleRows.at(5).toObject().value("id"),
                     QJsonValue("hardware.alex1Filters.bpf1.1_5MHz.bypass"));
        }
        // The HL2 has neither page.
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::HermesLite),
                                HPSDRModel::HERMESLITE, info);
        const QJsonObject hl2 = projectedCategory(service.hardware(), 13);
        QVERIFY(pageById(hl2, "hardware.alex1Filters").isEmpty());
        QVERIFY(pageById(hl2, "hardware.alex2Filters").isEmpty());

        // Before 13 none of it.
        service.setRadioContext(saturn, HPSDRModel::ANAN_G2, info);
        const QByteArray v12 = QJsonDocument(projectedCategory(service.hardware(), 12)).toJson();
        QVERIFY(!v12.contains("alex1Filters"));
        QVERIFY(!v12.contains("alex2Filters"));
        QVERIFY(!v12.contains("sampleRate"));
    }

    // Version 17 (R-R3-46, R-R3-49): the Alex-1 tab's low-pass rows and
    // 6m/ByPass on RX (radioHardwareVersion 10), in their own section after
    // the high-pass or BPF1 rows, on every Alex board. The rows are closed;
    // the bypass is disabled with the desktop's reason on the models Thetis
    // forces it off for; an older peer keeps what it had (16 without the
    // low-pass rows, 13 unchanged).
    void hardwareV17DescribesAlexLpfRows()
    {
        RadioInfo info;
        info.name = QStringLiteral("ANAN-100D");
        info.macAddress = QStringLiteral("00:1C:C0:A2:12:34");
        info.protocol = ProtocolVersion::Protocol2;
        SetupDescriptionService service;
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Angelia),
                                HPSDRModel::ANAN100D, info);

        const QList<QJsonObject> rows = resourceRows(QStringLiteral("hardware"), 17);
        QCOMPARE(rows.size(), 7 * 2 + 1);
        for (const QJsonObject& row : rows) {
            QVERIFY2(SetupDescriptionService::validateHardwareV13Control(row),
                     qPrintable(row.value("id").toString()));
            for (const QJsonObject& changed : mutationsOf(row)) {
                QVERIFY2(!SetupDescriptionService::validateHardwareV13Control(changed),
                         qPrintable(QJsonDocument(changed).toJson(QJsonDocument::Compact)));
            }
        }

        const QJsonObject v17 = projectedCategory(service.hardware(), 17);
        QCOMPARE(v17.value("version"), QJsonValue(17));
        const QJsonArray sections = pageById(v17, "hardware.alex1Filters")
            .value("sections").toArray();
        QCOMPARE(sections.size(), 2);
        QCOMPARE(sections.at(0).toObject().value("title"), QJsonValue("Alex HPF Bands"));
        const QJsonObject lpf = sections.at(1).toObject();
        QCOMPARE(lpf.value("title"), QJsonValue("Alex LPF Bands"));
        const QJsonArray lpfRows = lpf.value("controls").toArray();
        QCOMPARE(lpfRows.size(), 15);
        const QStringList slugs{"160m", "80m", "40m", "20m", "15m", "10m", "6m"};
        const QStringList labels{"160m", "80m", "60/40m", "30/20m", "17/15m", "12/10m", "6m"};
        const codec::alex::AlexLpfEdges defaults = codec::alex::AlexLpfEdges::thetisDefaults();
        for (int i = 0; i < 7; ++i) {
            const QJsonObject start = lpfRows.at(2 * i).toObject();
            const QJsonObject end = lpfRows.at(2 * i + 1).toObject();
            QCOMPARE(start.value("id"),
                     QJsonValue("hardware.alex1Filters.lpf." + slugs[i] + ".start"));
            QCOMPARE(end.value("id"),
                     QJsonValue("hardware.alex1Filters.lpf." + slugs[i] + ".end"));
            QCOMPARE(start.value("label"), QJsonValue(labels[i] + " LPF Start"));
            QCOMPARE(start.value("binding").toObject().value("radioSetting"),
                     QJsonValue("alex/lpf/" + slugs[i] + "/start"));
            QCOMPARE(end.value("binding").toObject().value("radioSetting"),
                     QJsonValue("alex/lpf/" + slugs[i] + "/end"));
            QCOMPARE(start.value("default").toDouble(), defaults.rows[size_t(i)].startMhz);
            QCOMPARE(end.value("default").toDouble(), defaults.rows[size_t(i)].endMhz);
            // Alex LPF review C1: each edge's range is its Thetis spinner's
            // (setup.designer.cs [v2.10.3.15], codec::alex::kAlexLpfEdgeLimits),
            // the one the Core enforces (StationServer alexLpfKeyValueRefusal).
            const codec::alex::AlexLpfEdgeLimits& lim =
                codec::alex::kAlexLpfEdgeLimits[size_t(i)];
            QCOMPARE(start.value("min").toDouble(), lim.startMin);
            QCOMPARE(start.value("max").toDouble(), lim.startMax);
            QCOMPARE(end.value("min").toDouble(), lim.endMin);
            QCOMPARE(end.value("max").toDouble(), lim.endMax);
            QCOMPARE(start.value("gate").toObject().value("min"), QJsonValue(10));
            QCOMPARE(start.value("gate").toObject().value("transmit"), QJsonValue(true));
        }
        const QJsonObject bypass = lpfRows.at(14).toObject();
        QCOMPARE(bypass.value("id"), QJsonValue("hardware.alex1Filters.lpfBypass"));
        QCOMPARE(bypass.value("label"), QJsonValue("6m/ByPass on RX"));
        QCOMPARE(bypass.value("tooltip"),
                 QJsonValue("Selects the 6m LPF during receive regardless of frequency."));
        QVERIFY(!bypass.value("gate").toObject().contains("transmit"));
        QVERIFY(!bypass.contains("availability"));

        // A BPF-panel radio: the rows follow the BPF1 section; Thetis
        // forces the bypass off on the G2 (setup.cs), so it is disabled
        // with the desktop's reason.
        info.name = QStringLiteral("ANAN-G2");
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Saturn),
                                HPSDRModel::ANAN_G2, info);
        const QJsonArray g2Sections = pageById(projectedCategory(service.hardware(), 17),
                                               "hardware.alex1Filters")
            .value("sections").toArray();
        QCOMPARE(g2Sections.size(), 2);
        QCOMPARE(g2Sections.at(0).toObject().value("title"), QJsonValue("Saturn BPF1 Bands"));
        const QJsonObject g2Bypass = g2Sections.at(1).toObject().value("controls").toArray()
            .at(14).toObject();
        QCOMPARE(g2Bypass.value("id"), QJsonValue("hardware.alex1Filters.lpfBypass"));
        QCOMPARE(g2Bypass.value("availability").toObject().value("enabled"), QJsonValue(false));
        QCOMPARE(g2Bypass.value("availability").toObject().value("reason"),
                 QJsonValue(RadioModel::lpfBypassUnavailableReason()));

        // Older peers: exactly what they had.
        const QJsonObject v13 = projectedCategory(service.hardware(), 13);
        QCOMPARE(v13.value("version"), QJsonValue(13));
        QVERIFY(!QJsonDocument(v13).toJson().contains("lpf"));
        QCOMPARE(projectedCategory(service.hardware(), 14), v13);
        QCOMPARE(projectedCategory(service.hardware(), 15), v13);
        QJsonObject v16 = withoutRowsOf(projectedCategory(service.hardware(), 17), 17);
        v16.insert("version", 16);
        QCOMPARE(projectedCategory(service.hardware(), 16), v16);
        QVERIFY(!QJsonDocument(projectedCategory(service.hardware(), 16)).toJson()
                     .contains("lpf"));
        QJsonObject expected = withoutRowsOf(v16, 16);
        expected.insert("version", 13);
        QCOMPARE(expected, v13);
    }

    void categoriesLoadAndMirrorAsStrings()
    {
        SetupDescriptionService service;
        for (const QString& id : {QStringLiteral("general"), QStringLiteral("test"),
                                  QStringLiteral("catNetwork"), QStringLiteral("dsp")}) {
            const QJsonObject category = service.category(id);
            // Version 15 carries DSP and CAT & Network rows; 19, CFC's band
            // editor; 21, TCI Forget's dependency on Duplicate.
            QCOMPARE(category.value(QStringLiteral("version")).toInt(),
                     id == QLatin1String("dsp") ? 19
                         : id == QLatin1String("catNetwork") ? 21 : 1);
            QCOMPARE(category.value(QStringLiteral("category")).toObject()
                         .value(QStringLiteral("id")).toString(), id);
            QVERIFY(!category.value(QStringLiteral("pages")).toArray().isEmpty());
            QVERIFY(!service.property(id.toLatin1().constData()).toString().isEmpty());
        }
        const QJsonObject diagnostics = service.category(QStringLiteral("diagnostics"));
        // Version 15: Radio Status and Connection Quality before Settings
        // Validation, the desktop's order.
        QCOMPARE(diagnostics.value(QStringLiteral("version")), QJsonValue(15));
        QCOMPARE(pageIdsOf(diagnostics), (QStringList{"diagnostics.radioStatus",
                                                      "diagnostics.connectionQuality",
                                                      "diagnostics.settingsValidation"}));
        QVERIFY(!service.diagnostics().isEmpty());
        QCOMPARE(service.revision(), quint32(1));
        const MirrorSchema& schema = MirrorSchema::forObject(&service);
        QVERIFY(schema.byName("general"));
        QVERIFY(schema.byName("catNetwork"));
        QVERIFY(schema.byName("revision"));
    }

    void settingsHygienePanelIsClosedAndOnlyV3()
    {
        SetupDescriptionService service;
        const QJsonObject diagnostics = service.category(QStringLiteral("diagnostics"));
        QCOMPARE(diagnostics.value("category").toObject().value("where"), QJsonValue("station"));
        QCOMPARE(diagnostics.value("category").toObject().value("coverage"), QJsonValue("partial"));
        const QJsonObject page = diagnostics.value("pages").toArray().last().toObject();
        QCOMPARE(page.value("id"), QJsonValue("diagnostics.settingsValidation"));
        QCOMPARE(page.value("title"), QJsonValue("Settings Validation"));
        const QJsonObject section = page.value("sections").toArray().first().toObject();
        QCOMPARE(section.value("title"), QJsonValue("Validation Issues"));
        const QJsonArray controls = section.value("controls").toArray();
        QCOMPARE(controls.size(), 1);
        const QJsonObject panel = controls.first().toObject();
        QVERIFY(SetupDescriptionService::validateSettingsHygienePanel(panel));
        QCOMPARE(panel.value("id"), QJsonValue("diagnostics.settingsValidation.health"));
        QCOMPARE(panel.value("label"), QJsonValue("Validation Issues"));
        QCOMPARE(panel.value("tooltip"), QJsonValue(""));
        QCOMPARE(panel.value("kind"), QJsonValue("settingsHygiene"));
        QCOMPARE(panel.value("requiresDescriptionVersion"), QJsonValue(3));
        QCOMPARE(panel.value("gate").toObject(),
                 (QJsonObject{{"capability", "settingsHygieneVersion"}, {"min", 1}}));
        QCOMPARE(panel.value("binding").toObject(),
                 (QJsonObject{{"settingsHygiene", QJsonObject{{"version", 1}}}}));
        QCOMPARE(panel.value("actions").toArray().size(), 3);
        QCOMPARE(panel.value("actions").toArray().at(0).toObject().value("label"),
                 QJsonValue("Re-validate"));
        // G-38: Repair Invalid Settings, with Forget's gates, needs
        // settingsHygieneVersion 2 (an older Core keeps it disabled).
        const QJsonObject repair = panel.value("actions").toArray().at(1).toObject();
        QCOMPARE(repair.value("id"), QJsonValue("repair"));
        QCOMPARE(repair.value("label"), QJsonValue("Repair Invalid Settings"));
        QCOMPARE(repair.value("paired"), QJsonValue(true));
        QCOMPARE(repair.value("offAir"), QJsonValue(true));
        QCOMPARE(repair.value("gate").toObject(),
                 (QJsonObject{{"capability", "settingsHygieneVersion"}, {"min", 2}}));
        QVERIFY(!repair.contains("enabled"));
        QCOMPARE(panel.value("actions").toArray().at(2).toObject().value("label"),
                 QJsonValue("Forget This Radio"));

        const auto reject = [&panel](const QString& key, const QJsonValue& value) {
            QJsonObject changed = panel;
            changed.insert(key, value);
            QVERIFY(!SetupDescriptionService::validateSettingsHygienePanel(changed));
        };
        reject("extra", true);
        reject("kind", "button");
        reject("requiresDescriptionVersion", 2);
        reject("binding", QJsonObject{{"command", QJsonObject{{"verb", "station.forgetSettings"}}}});
        reject("binding", QJsonObject{{"settingsHygiene", QJsonObject{{"version", 2}}}});
        reject("gate", QJsonObject{{"capability", "settingsHygieneVersion"}, {"min", 0}});
        reject("gate", QJsonObject{{"capability", "settingsHygieneVersion"},
                                    {"min", 1}, {"transmit", true}});
        QJsonArray actions = panel.value("actions").toArray();
        QJsonObject repairAction = actions.at(1).toObject();
        repairAction.remove("gate");
        actions[1] = repairAction;
        reject("actions", actions);
        actions = panel.value("actions").toArray();
        repairAction = actions.at(1).toObject();
        repairAction.remove("paired");
        actions[1] = repairAction;
        reject("actions", actions);
        actions = panel.value("actions").toArray();
        QJsonObject validate = actions.at(0).toObject();
        validate.insert("verb", "station.forgetSettings");
        actions[0] = validate;
        reject("actions", actions);
        actions = panel.value("actions").toArray();
        const QJsonValue firstAction = actions.at(0);
        actions[0] = actions.at(2);
        actions[2] = firstAction;
        reject("actions", actions);
        actions = panel.value("actions").toArray();
        QJsonObject forget = actions.at(2).toObject();
        forget.remove("paired");
        actions[2] = forget;
        reject("actions", actions);
        forget = panel.value("actions").toArray().at(2).toObject();
        forget.remove("offAir");
        actions[2] = forget;
        reject("actions", actions);
        forget = panel.value("actions").toArray().at(2).toObject();
        forget.remove("confirmation");
        actions[2] = forget;
        reject("actions", actions);
        forget = panel.value("actions").toArray().at(2).toObject();
        forget.insert("verb", "station.forgetRadio");
        actions[2] = forget;
        reject("actions", actions);
        actions = panel.value("actions").toArray();
        forget = actions.at(2).toObject();
        QJsonObject confirmation = forget.value("confirmation").toObject();
        confirmation.insert("default", "confirm");
        forget.insert("confirmation", confirmation);
        actions[2] = forget;
        reject("actions", actions);

        QVERIFY(SetupDescriptionService::fitCategoryForVersion(service.diagnostics(), 1).isEmpty());
        QVERIFY(SetupDescriptionService::fitCategoryForVersion(service.diagnostics(), 2).isEmpty());
        const QJsonObject v3 = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(service.diagnostics(), 3).toUtf8())
            .object();
        QCOMPARE(v3.value("version"), QJsonValue(3));
        QCOMPARE(v3.value("pages").toArray().size(), 1);
    }

    void settingToggleEncodingIsExactAndPersistsAsReaderStrings()
    {
        SetupDescriptionService service;
        const auto findControl = [&service](const QString& category, const QString& id) {
            const QJsonObject root = service.category(category);
            for (const QJsonValue& page : root.value(QStringLiteral("pages")).toArray()) {
                for (const QJsonValue& section : page.toObject()
                         .value(QStringLiteral("sections")).toArray()) {
                    for (const QJsonValue& raw : section.toObject()
                             .value(QStringLiteral("controls")).toArray()) {
                        const QJsonObject control = raw.toObject();
                        if (control.value(QStringLiteral("id")) == QJsonValue(id)) {
                            return control;
                        }
                    }
                }
            }
            return QJsonObject{};
        };
        const QStringList generalKeys = {
            QStringLiteral("RxOnly"), QStringLiteral("NetworkWatchdogEnabled"),
            QStringLiteral("MoxTimeOutEnabled"), QStringLiteral("PingTimeOutEnabled"),
            QStringLiteral("RemoteMoxTimeOutEnabled"), QStringLiteral("ExtendedTransmit"),
            QStringLiteral("PreventTxOnDifferentBandToRx")};
        int generalCount = 0;
        for (const QJsonValue& page : service.category(QStringLiteral("general"))
                 .value(QStringLiteral("pages")).toArray()) {
            for (const QJsonValue& section : page.toObject()
                     .value(QStringLiteral("sections")).toArray()) {
                for (const QJsonValue& raw : section.toObject()
                         .value(QStringLiteral("controls")).toArray()) {
                    const QJsonObject control = raw.toObject();
                    if (control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("toggle"))
                        || !control.value(QStringLiteral("binding")).toObject()
                                .contains(QStringLiteral("setting"))) {
                        continue;
                    }
                    QVERIFY(generalKeys.contains(control.value(QStringLiteral("binding"))
                        .toObject().value(QStringLiteral("setting")).toString()));
                    QVERIFY(SetupDescriptionService::validateSettingToggleEncoding(control));
                    ++generalCount;
                }
            }
        }
        QCOMPARE(generalCount, 7);

        const QJsonObject watchdog = findControl(QStringLiteral("general"),
            QStringLiteral("general.options.networkWatchdog"));
        QVERIFY(!watchdog.isEmpty());
        QJsonObject bad = watchdog;
        bad.remove(QStringLiteral("valueEncoding"));
        QVERIFY(!SetupDescriptionService::validateSettingToggleEncoding(bad));
        bad = watchdog;
        bad.insert(QStringLiteral("valueEncoding"), QJsonObject{
            {QStringLiteral("true"), QStringLiteral("True")}});
        QVERIFY(!SetupDescriptionService::validateSettingToggleEncoding(bad));
        bad = watchdog;
        bad.insert(QStringLiteral("valueEncoding"), QJsonObject{
            {QStringLiteral("true"), QStringLiteral("true")},
            {QStringLiteral("false"), QStringLiteral("False")}});
        QVERIFY(!SetupDescriptionService::validateSettingToggleEncoding(bad));
        bad = watchdog;
        bad.insert(QStringLiteral("valueEncoding"), QJsonObject{
            {QStringLiteral("true"), QStringLiteral("True")},
            {QStringLiteral("false"), QStringLiteral("False")},
            {QStringLiteral("fallback"), QStringLiteral("False")}});
        QVERIFY(!SetupDescriptionService::validateSettingToggleEncoding(bad));
        bad = findControl(QStringLiteral("dsp"),
            QStringLiteral("dsp.agcAlc.autoAgcEnabled"));
        QVERIFY(!bad.isEmpty());
        bad.insert(QStringLiteral("valueEncoding"), watchdog.value(QStringLiteral("valueEncoding")));
        QVERIFY(!SetupDescriptionService::validateSettingToggleEncoding(bad));

        AppSettings& store = AppSettings::instance();
        const QStringList keys = {QStringLiteral("RxOnly"),
            QStringLiteral("MoxTimeOutEnabled"), QStringLiteral("PingTimeOutEnabled"),
            QStringLiteral("RemoteMoxTimeOutEnabled"), QStringLiteral("NetworkWatchdogEnabled"),
            QStringLiteral("ExtendedTransmit"), QStringLiteral("PreventTxOnDifferentBandToRx"),
            QStringLiteral("DspOptionsCacheImpulse"),
            QStringLiteral("DspOptionsCacheImpulseSaveRestore")};
        QHash<QString, QVariant> oldValues;
        for (const QString& key : keys) {
            if (store.contains(key)) {
                oldValues.insert(key, store.value(key));
            }
        }
        const auto restore = qScopeGuard([&store, &keys, &oldValues] {
            for (const QString& key : keys) {
                if (oldValues.contains(key)) {
                    store.setValue(key, oldValues.value(key));
                } else {
                    store.remove(key);
                }
            }
        });
        SettingsProxyServer server(store);
        QSignalSpy changed(&server, &SettingsProxyServer::outboundValueChanged);
        const QStringList ids = {QStringLiteral("general.options.rxOnly"),
            QStringLiteral("general.options.moxTimeoutEnabled"),
            QStringLiteral("general.options.pingTimeoutEnabled"),
            QStringLiteral("general.options.remoteTimeoutEnabled"),
            QStringLiteral("general.options.networkWatchdog"),
            QStringLiteral("general.options.extended"),
            QStringLiteral("general.options.preventDifferentBand"),
            QStringLiteral("dsp.options.DspOptionsCacheImpulse"),
            QStringLiteral("dsp.options.DspOptionsCacheImpulseSaveRestore")};
        for (const QString& id : ids) {
            const QJsonObject control = findControl(id.startsWith(QStringLiteral("general."))
                ? QStringLiteral("general") : QStringLiteral("dsp"), id);
            QVERIFY2(!control.isEmpty(), qPrintable(id));
            QVERIFY(SetupDescriptionService::validateSettingToggleEncoding(control));
            const QString key = control.value(QStringLiteral("binding"))
                .toObject().value(QStringLiteral("setting")).toString();
            const QJsonObject encoding = control.value(QStringLiteral("valueEncoding")).toObject();
            for (const bool on : {true, false}) {
                const QString token = encoding.value(on ? QStringLiteral("true")
                    : QStringLiteral("false")).toString();
                QVERIFY(server.applyInboundWrite(key, token, QStringLiteral("phone-test")).accepted);
                QCOMPARE(store.value(key).toString(), token);
                QVERIFY(!changed.isEmpty());
                const QList<QVariant> broadcast = changed.takeLast();
                QCOMPARE(broadcast.at(0).toString(), key);
                QCOMPARE(broadcast.at(1).metaType(), QMetaType::fromType<QString>());
                QCOMPARE(broadcast.at(1).toString(), token);
                if (key == QLatin1String("NetworkWatchdogEnabled")) {
                    QCOMPARE(RadioModel::networkWatchdogSetting(), on);
                } else if (key == QLatin1String("RxOnly")) {
                    QCOMPARE(RadioModel::rxOnlySetting(), on);
                } else if (key == QLatin1String("ExtendedTransmit")) {
                    // Addendum G-42: the transmit gate's reader.
                    QCOMPARE(RadioModel::extendedTransmitSetting(), on);
                } else if (key == QLatin1String("PreventTxOnDifferentBandToRx")) {
                    QCOMPARE(RadioModel::preventTxOnDifferentBandSetting(), on);
                } else if (key == QLatin1String("MoxTimeOutEnabled")) {
                    QCOMPARE(RadioModel::txTimeOutSettingsFor(QStringLiteral("station")).moxEnabled, on);
                } else if (key == QLatin1String("PingTimeOutEnabled")) {
                    QCOMPARE(RadioModel::txTimeOutSettingsFor(QStringLiteral("station")).pingEnabled, on);
                } else if (key == QLatin1String("RemoteMoxTimeOutEnabled")) {
                    QCOMPARE(RadioModel::txTimeOutSettingsFor(QStringLiteral("phone")).moxEnabled, on);
                    QCOMPARE(RadioModel::txTimeOutSettingsFor(QStringLiteral("tablet")).moxEnabled, on);
                } else {
                    // WdspEngine::finishInitialization/shutdown read both
                    // cache keys with this exact string comparison.
                    QCOMPARE(store.value(key, QStringLiteral("False")).toString()
                                 == QStringLiteral("True"), on);
                }
            }
        }
    }

    void transmitDexpBindingsAreExactMirroredSettings()
    {
        SetupDescriptionService service;
        // Version 12's rows; 13's Disable HF PA is transmitV13DescribesDisableHfPa's.
        const QJsonObject transmit = projectedCategory(service.transmit(), 12);
        QVERIFY(!transmit.isEmpty());
        int count = 0;
        for (const QJsonValue& rawPage : transmit.value(QStringLiteral("pages")).toArray()) {
            for (const QJsonValue& rawSection : rawPage.toObject().value(QStringLiteral("sections")).toArray()) {
                for (const QJsonValue& raw : rawSection.toObject().value(QStringLiteral("controls")).toArray()) {
                    const QJsonObject control = raw.toObject();
                    // Version 15 rows are checked against the Core's sources
                    // (dspV15DescribesTheRestOfDsp... below).
                    if (control.value(QStringLiteral("requiresDescriptionVersion")) == QJsonValue(15)) {
                        continue;
                    }
                    QVERIFY(SetupDescriptionService::validateTransmitPropertyBinding(control)
                        || SetupDescriptionService::validateTransmitSettingBinding(control));
                    QVERIFY(control.value(QStringLiteral("gate")).toObject()
                        .value(QStringLiteral("transmit")) == QJsonValue(true));
                    ++count;
                }
            }
        }
        QCOMPARE(count, 24);
        QJsonObject invalid = controlById(transmit, QStringLiteral("transmit.dexpVox.dexpEnabled"));
        QJsonObject gate = invalid.value(QStringLiteral("gate")).toObject();
        gate.remove(QStringLiteral("transmit"));
        invalid.insert(QStringLiteral("gate"), gate);
        QVERIFY(!SetupDescriptionService::validateTransmitPropertyBinding(invalid));
        gate.insert(QStringLiteral("transmit"), true);
        gate.insert(QStringLiteral("offAir"), true);
        invalid.insert(QStringLiteral("gate"), gate);
        QVERIFY(!SetupDescriptionService::validateTransmitPropertyBinding(invalid));
    }

    void audioTxFilterBindingsAreExactMirroredProperties()
    {
        SetupDescriptionService service;
        const QJsonObject audio = service.category(QStringLiteral("audio"));
        QVERIFY(!audio.isEmpty());
        // Version 15 put TX Input first and the profile rows before TX Filter.
        const QJsonArray described = audio.value(QStringLiteral("pages")).toArray()
            .last().toObject().value(QStringLiteral("sections")).toArray()
            .last().toObject().value(QStringLiteral("controls")).toArray();
        QCOMPARE(described.size(), 3);
        for (const QJsonValue& raw : described) {
            QVERIFY(SetupDescriptionService::validateAudioPropertyBinding(raw.toObject()));
        }
        QJsonObject bad = described.first().toObject();
        QJsonObject gate = bad.value(QStringLiteral("gate")).toObject();
        gate.remove(QStringLiteral("transmit"));
        bad.insert(QStringLiteral("gate"), gate);
        QVERIFY(!SetupDescriptionService::validateAudioPropertyBinding(bad));
        gate.insert(QStringLiteral("transmit"), true);
        gate.insert(QStringLiteral("offAir"), true);
        bad.insert(QStringLiteral("gate"), gate);
        QVERIFY(!SetupDescriptionService::validateAudioPropertyBinding(bad));
        bad = described.first().toObject();
        QJsonObject binding = bad.value(QStringLiteral("binding")).toObject();
        QJsonObject property = binding.value(QStringLiteral("property")).toObject();
        property.insert(QStringLiteral("name"), QStringLiteral("mox"));
        binding.insert(QStringLiteral("property"), property);
        bad.insert(QStringLiteral("binding"), binding);
        QVERIFY(!SetupDescriptionService::validateAudioPropertyBinding(bad));
    }

    // R-R3-46 / R-R3-11: Setup > General > Options describes RX2
    // Attenuation (the other ADC's own attenuator) on a two-ADC radio, bound
    // to stepAtt.rx2AttenuationDb and gated on adcAttenuatorVersion 1; a
    // one-ADC radio has no such control.
    void generalDescribesRx2AttenuationOnTwoAdcRadios()
    {
        const auto find = [](const SetupDescriptionService& service) {
            for (const QJsonValue& page : service.category(QStringLiteral("general"))
                     .value(QStringLiteral("pages")).toArray()) {
                for (const QJsonValue& section : page.toObject()
                         .value(QStringLiteral("sections")).toArray()) {
                    for (const QJsonValue& control : section.toObject()
                             .value(QStringLiteral("controls")).toArray()) {
                        if (control.toObject().value(QStringLiteral("id")).toString()
                            == QStringLiteral("general.options.rx2StepAtt")) {
                            return control.toObject();
                        }
                    }
                }
            }
            return QJsonObject{};
        };
        SetupDescriptionService g2;
        g2.setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::Saturn));
        const QJsonObject rx2 = find(g2);
        QVERIFY(!rx2.isEmpty());
        QCOMPARE(rx2.value(QStringLiteral("label")).toString(), QStringLiteral("RX2 Attenuation"));
        QCOMPARE(rx2.value(QStringLiteral("binding")).toObject().value(QStringLiteral("property")),
                 QJsonValue(QJsonObject{{QStringLiteral("object"), QStringLiteral("stepAtt")},
                                        {QStringLiteral("name"), QStringLiteral("rx2AttenuationDb")}}));
        QCOMPARE(rx2.value(QStringLiteral("gate")).toObject().value(QStringLiteral("capability")),
                 QJsonValue(QStringLiteral("adcAttenuatorVersion")));
        QCOMPARE(rx2.value(QStringLiteral("gate")).toObject().value(QStringLiteral("min")),
                 QJsonValue(1));
        QCOMPARE(rx2.value(QStringLiteral("max")).toInt(),
                 BoardCapsTable::forBoard(HPSDRHW::Saturn).attenuator.maxDb);

        // RX2's own enable and auto-attenuate switches, the same gate.
        const auto findId = [](const SetupDescriptionService& service, const QString& id) {
            for (const QJsonValue& page : service.category(QStringLiteral("general"))
                     .value(QStringLiteral("pages")).toArray()) {
                for (const QJsonValue& section : page.toObject()
                         .value(QStringLiteral("sections")).toArray()) {
                    for (const QJsonValue& control : section.toObject()
                             .value(QStringLiteral("controls")).toArray()) {
                        if (control.toObject().value(QStringLiteral("id")).toString() == id) {
                            return control.toObject();
                        }
                    }
                }
            }
            return QJsonObject{};
        };
        const QHash<QString, QString> rx2Switches{
            {QStringLiteral("general.options.rx2StepAttEnable"), QStringLiteral("rx2StepAttEnabled")},
            {QStringLiteral("general.options.rx2AutoAttEnable"), QStringLiteral("rx2AutoAttEnabled")},
            {QStringLiteral("general.options.rx2AutoAttUndo"), QStringLiteral("rx2AutoAttUndo")}};
        for (auto it = rx2Switches.cbegin(); it != rx2Switches.cend(); ++it) {
            const QJsonObject control = findId(g2, it.key());
            QVERIFY2(!control.isEmpty(), qPrintable(it.key()));
            QCOMPARE(control.value(QStringLiteral("kind")).toString(), QStringLiteral("toggle"));
            QCOMPARE(control.value(QStringLiteral("binding")).toObject()
                         .value(QStringLiteral("property")).toObject()
                         .value(QStringLiteral("name")).toString(), it.value());
            QCOMPARE(control.value(QStringLiteral("gate")).toObject()
                         .value(QStringLiteral("capability")).toString(),
                     QStringLiteral("adcAttenuatorVersion"));
        }

        SetupDescriptionService hl2;
        hl2.setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::HermesLite));
        QVERIFY(find(hl2).isEmpty());
        for (auto it = rx2Switches.cbegin(); it != rx2Switches.cend(); ++it) {
            QVERIFY2(findId(hl2, it.key()).isEmpty(), qPrintable(it.key()));
        }
    }

    void boardRefreshOnlyMovesRevisionOnChange()
    {
        SetupDescriptionService service;
        const auto hasStepAtt = [&service]() {
            const QJsonArray pages = service.category(QStringLiteral("general"))
                                         .value(QStringLiteral("pages")).toArray();
            for (const QJsonValue& page : pages) {
                for (const QJsonValue& section : page.toObject()
                         .value(QStringLiteral("sections")).toArray()) {
                    for (const QJsonValue& control : section.toObject()
                             .value(QStringLiteral("controls")).toArray()) {
                        if (control.toObject().value(QStringLiteral("id")).toString()
                            == QStringLiteral("general.options.rx1StepAtt")) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        QVERIFY(!hasStepAtt());
        const quint32 first = service.revision();
        service.setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::HermesLite));
        QVERIFY(service.revision() > first);
        QVERIFY(hasStepAtt());
        const quint32 second = service.revision();
        service.setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::HermesLite));
        QCOMPARE(service.revision(), second);
    }

    void hardwareScalarsProjectAndRejectUnsafeBindings()
    {
        SetupDescriptionService service;
        // Version 13's Radio Info and Calibration pages are on every board;
        // before 13 a board without ALEX filters has no Hardware category.
        QVERIFY(SetupDescriptionService::fitCategoryForVersion(service.hardware(), 12).isEmpty());
        service.setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::Hermes));
        const auto v1Hardware = [&service] {
            return QJsonDocument::fromJson(SetupDescriptionService::fitCategoryForVersion(
                service.hardware(), 1).toUtf8()).object();
        };
        const QJsonObject hardware = v1Hardware();
        QCOMPARE(hardware.value("version").toInt(), 1);
        QCOMPARE(hardware.value("category").toObject().value("coverage"), QJsonValue("partial"));
        const QJsonArray pages = hardware.value("pages").toArray();
        QCOMPARE(pages.size(), 1);
        QCOMPARE(pages.first().toObject().value("id"), QJsonValue("hardware.antennaAlex"));
        const QJsonArray controls = pages.first().toObject().value("sections").toArray()
            .first().toObject().value("controls").toArray();
        QCOMPARE(controls.size(), 3);
        QCOMPARE(controls.at(0).toObject().value("id"), QJsonValue("hardware.antennaAlex.blockTxAnt2"));
        QCOMPARE(controls.at(1).toObject().value("id"), QJsonValue("hardware.antennaAlex.blockTxAnt3"));
        QCOMPARE(controls.at(2).toObject().value("id"), QJsonValue("hardware.antennaAlex.useTxAntennaForRx"));
        for (const QJsonValue& raw : controls) {
            QVERIFY(SetupDescriptionService::validateHardwarePropertyBinding(raw.toObject()));
        }
        const QJsonObject tx = controls.first().toObject();
        const QJsonObject rx = controls.last().toObject();
        const auto rejectGate = [](const QJsonObject& source, const QString& key,
                                   const QJsonValue& value) {
            QJsonObject changed = source;
            QJsonObject gate = changed.value("gate").toObject();
            gate.insert(key, value);
            changed.insert("gate", gate);
            QVERIFY(!SetupDescriptionService::validateHardwarePropertyBinding(changed));
        };
        rejectGate(tx, QStringLiteral("offAir"), false);
        rejectGate(tx, QStringLiteral("min"), 5);
        rejectGate(tx, QStringLiteral("transmit"), true);
        rejectGate(rx, QStringLiteral("offAir"), true);
        rejectGate(rx, QStringLiteral("board"), QStringLiteral("hasAlexFilters"));
        QJsonObject mislabeled = tx;
        mislabeled.insert("label", "Use TX antenna for RX");
        QVERIFY(!SetupDescriptionService::validateHardwarePropertyBinding(mislabeled));
        QJsonObject wrongProperty = tx;
        QJsonObject binding = wrongProperty.value("binding").toObject();
        QJsonObject ref = binding.value("property").toObject();
        ref.insert("name", "rxOutOnTx");
        binding.insert("property", ref);
        wrongProperty.insert("binding", binding);
        QVERIFY(!SetupDescriptionService::validateHardwarePropertyBinding(wrongProperty));

        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes),
                                HPSDRModel::ANAN100);
        const QJsonArray classic = v1Hardware()
            .value("pages").toArray().first().toObject().value("sections").toArray()
            .first().toObject().value("controls").toArray();
        QCOMPARE(classic.size(), 7);
        QCOMPARE(classic.at(2).toObject().value("id"), QJsonValue("hardware.antennaAlex.rxOutOnTx"));
        QCOMPARE(classic.at(3).toObject().value("id"), QJsonValue("hardware.antennaAlex.ext1OutOnTx"));
        QCOMPARE(classic.at(4).toObject().value("id"), QJsonValue("hardware.antennaAlex.ext2OutOnTx"));
        QCOMPARE(classic.at(5).toObject().value("id"), QJsonValue("hardware.antennaAlex.rxOutOverride"));
        for (int index = 2; index <= 5; ++index) {
            const QJsonObject relay = classic.at(index).toObject();
            QVERIFY(SetupDescriptionService::validateHardwarePropertyBinding(relay,
                                                                               HPSDRModel::ANAN100));
            QVERIFY(!SetupDescriptionService::validateHardwarePropertyBinding(relay,
                                                                                HPSDRModel::ANAN10));
            QJsonObject unsafe = relay;
            QJsonObject gate = unsafe.value("gate").toObject();
            gate.remove("offAir");
            unsafe.insert("gate", gate);
            QVERIFY(!SetupDescriptionService::validateHardwarePropertyBinding(unsafe,
                                                                                HPSDRModel::ANAN100));
            unsafe = relay;
            unsafe.insert("label", "Wrong relay");
            QVERIFY(!SetupDescriptionService::validateHardwarePropertyBinding(unsafe,
                                                                                HPSDRModel::ANAN100));
        }
        QCOMPARE(classic.at(2).toObject().value("gate").toObject().value("min"), QJsonValue(5));
        QCOMPARE(classic.at(3).toObject().value("gate").toObject().value("min"), QJsonValue(6));

        const quint32 classicRevision = service.revision();
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes),
                                HPSDRModel::ANAN10);
        QVERIFY(service.revision() > classicRevision);
        QVERIFY(!service.hardware().contains(QStringLiteral("ext1OutOnTx")));
        const quint32 hiddenRevision = service.revision();
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes),
                                HPSDRModel::ANAN10E);
        QCOMPARE(service.revision(), hiddenRevision);
        service.setRadioContext(BoardCapsTable::forBoard(HPSDRHW::Hermes),
                                HPSDRModel::ANAN100);
        QCOMPARE(v1Hardware().value("pages").toArray()
                     .first().toObject().value("sections").toArray().first().toObject()
                     .value("controls").toArray(), classic);
        service.setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::Hermes));
        QCOMPARE(v1Hardware(), hardware);

        const quint32 beforeRetire = service.revision();
        service.setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::HermesLite));
        QVERIFY(SetupDescriptionService::fitCategoryForVersion(service.hardware(), 12).isEmpty());
        QVERIFY(!service.hardware().contains(QStringLiteral("hardware.antennaAlex")));
        QVERIFY(service.revision() > beforeRetire);
        service.setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::Hermes));
        QCOMPARE(v1Hardware(), hardware);

        WireCore described(HPSDRHW::Hermes);
        QVERIFY(described.connect({{QByteArrayLiteral("setupDescription"), 1}}));
        QVERIFY(!setupCategoryOnWire(*described.app, "hardware",
                                     SessionMessageKind::ObjectCreate).isEmpty());
        WireCore oldMinor(HPSDRHW::Hermes);
        QVERIFY(oldMinor.connect({{QByteArrayLiteral("setupDescription"), 1}},
                                 quint16(kRadioIdentitySessionProtocolMinor - 1)));
        QVERIFY(!hasSetupTraffic(*oldMinor.app));
        WireCore undeclared(HPSDRHW::Hermes);
        QVERIFY(undeclared.connect());
        QVERIFY(!hasSetupTraffic(*undeclared.app));
    }

    void sameBoardSkuChangePublishesRelayDelta()
    {
        WireCore core(HPSDRHW::Hermes);
        QVERIFY(core.connect({{QByteArrayLiteral("setupDescription"), 1}}));
        core.app->clearReceived();
        core.model->setHpsdrModelForTest(HPSDRModel::ANAN100);
        core.model->currentRadioChanged(core.model->currentRadioInfo());
        QString published;
        QTRY_VERIFY(!(published = setupCategoryOnWire(
            *core.app, "hardware", SessionMessageKind::Delta)).isEmpty());
        QVERIFY(published.contains(QStringLiteral("rxOutOverride")));
        const quint32 classicRevision = core.server->setupDescription()->revision();

        core.app->clearReceived();
        core.model->setHpsdrModelForTest(HPSDRModel::ANAN10);
        core.model->currentRadioChanged(core.model->currentRadioInfo());
        QTRY_VERIFY(!(published = setupCategoryOnWire(
            *core.app, "hardware", SessionMessageKind::Delta)).isEmpty());
        QVERIFY(!published.contains(QStringLiteral("rxOutOnTx")));
        QVERIFY(!published.contains(QStringLiteral("ext1OutOnTx")));
        QVERIFY(core.server->setupDescription()->revision() > classicRevision);
        const quint32 hiddenRevision = core.server->setupDescription()->revision();

        core.app->clearReceived();
        core.model->setHpsdrModelForTest(HPSDRModel::ANAN10E);
        core.model->currentRadioChanged(core.model->currentRadioInfo());
        QCoreApplication::processEvents();
        QCOMPARE(core.server->setupDescription()->revision(), hiddenRevision);
        QVERIFY(setupCategoryOnWire(*core.app, "hardware", SessionMessageKind::Delta).isEmpty());
    }

    void dspPropertiesAreWritableOnTheSelectedSliceAlias()
    {
        SetupDescriptionService service;
        const QJsonArray pages = service.category(QStringLiteral("dsp"))
                                     .value(QStringLiteral("pages")).toArray();
        QCOMPARE(pages.size(), 10);
        const MirrorSchema& sliceSchema = MirrorSchema::forMetaObject(
            &SliceModel::staticMetaObject);
        const MirrorSchema& transmitSchema = MirrorSchema::forMetaObject(
            &TransmitModel::staticMetaObject);
        const MirrorSchema& notchSchema = MirrorSchema::forMetaObject(
            &NotchModel::staticMetaObject);
        int count = 0;
        int transmitCount = 0;
        int settingCount = 0;
        int notchCount = 0;
        int readoutCount = 0;
        for (const QJsonValue& rawPage : pages) {
            for (const QJsonValue& rawSection : rawPage.toObject()
                     .value(QStringLiteral("sections")).toArray()) {
                for (const QJsonValue& rawControl : rawSection.toObject()
                         .value(QStringLiteral("controls")).toArray()) {
                    // Version 15 rows: dspV15DescribesTheRestOfDsp...; version 19's
                    // band editor: dspV19DescribesCfcBands.
                    if (rawControl.toObject().value("requiresDescriptionVersion") == QJsonValue(15)
                        || rawControl.toObject().value("requiresDescriptionVersion") == QJsonValue(19)) {
                        continue;
                    }
                    if (rawControl.toObject().value("kind") == QJsonValue("table")) {
                        QString error;
                        QVERIFY2(SetupDescriptionService::validateTnfTable(
                            rawControl.toObject(), &error), qPrintable(error));
                        continue;
                    }
                    if (rawControl.toObject().value("binding").toObject()
                            .contains("command")) {
                        QString error;
                        QVERIFY2(SetupDescriptionService::validateCommandBinding(
                            rawControl.toObject(), &error), qPrintable(error));
                        continue;
                    }
                    if (rawControl.toObject().value("binding").toObject()
                            .contains("setting")) {
                        QVERIFY(SetupDescriptionService::validateDspSettingBinding(
                            rawControl.toObject()));
                        ++settingCount;
                        continue;
                    }
                    const QJsonObject ref = rawControl.toObject().value("binding")
                        .toObject().value("property").toObject();
                    QVERIFY(SetupDescriptionService::validateActiveSlicePropertyBinding(
                        rawControl.toObject()));
                    const QString object = ref.value("object").toString();
                    QVERIFY(object == QLatin1String("slice:active")
                            || object == QLatin1String("transmit")
                            || object == QLatin1String("notches"));
                    const QByteArray name = ref.value("name").toString().toUtf8();
                    const bool transmit = object == QLatin1String("transmit");
                    const bool notch = object == QLatin1String("notches");
                    const MirrorProperty* property = transmit ? transmitSchema.byName(name)
                        : notch ? notchSchema.byName(name) : sliceSchema.byName(name);
                    QVERIFY2(property != nullptr, name.constData());
                    if (rawControl.toObject().value("kind") == QJsonValue("readout")) {
                        QVERIFY(!property->isWritable);
                        ++readoutCount;
                        continue;
                    }
                    QVERIFY(property->isWritable);
                    QVERIFY(MirrorPolicy::inboundAllowed(
                        transmit ? "TransmitModel" : notch ? "NotchModel" : "SliceModel", name));
                    if (transmit) { ++transmitCount; }
                    else if (notch) { ++notchCount; }
                    else { ++count; }
                }
            }
        }
        QCOMPARE(count, 63);
        QCOMPARE(transmitCount, 14);
        QCOMPARE(settingCount, 23);
        QCOMPARE(notchCount, 1);
        QCOMPARE(readoutCount, 1);
        QJsonObject valid = pages.first().toObject().value("sections").toArray().first()
            .toObject().value("controls").toArray().first().toObject();
        QJsonObject bad = valid;
        bad.insert("kind", "toggle");
        QVERIFY(!SetupDescriptionService::validateActiveSlicePropertyBinding(bad));
        QJsonObject binding = valid.value("binding").toObject();
        QJsonObject ref = binding.value("property").toObject();
        ref.insert("name", "nnrStatus"); // outbound-only diagnostic
        binding.insert("property", ref);
        bad = valid;
        bad.insert("binding", binding);
        QVERIFY(!SetupDescriptionService::validateActiveSlicePropertyBinding(bad));
        QJsonObject options = pages.at(9).toObject().value("sections").toArray()
            .first().toObject().value("controls").toArray().first().toObject();
        QVERIFY(SetupDescriptionService::validateDspSettingBinding(options));
        bad = options;
        bad.insert("kind", "toggle");
        QVERIFY(!SetupDescriptionService::validateDspSettingBinding(bad));
        bad = options;
        QJsonObject settingBinding = bad.value("binding").toObject();
        settingBinding.insert("setting", "DspOptionsBufferSizeCwTx");
        bad.insert("binding", settingBinding);
        QVERIFY(!SetupDescriptionService::validateDspSettingBinding(bad));
        bad = options;
        QJsonArray wrongChoices = bad.value("choices").toArray();
        wrongChoices[0] = "2048";
        bad.insert("choices", wrongChoices);
        QVERIFY(!SetupDescriptionService::validateDspSettingBinding(bad));
        ref.insert("name", "nr1Taps");
        ref.insert("object", "slice:0"); // must be late-bound to this device's selection
        binding.insert("property", ref);
        bad.insert("binding", binding);
        QVERIFY(!SetupDescriptionService::validateActiveSlicePropertyBinding(bad));
    }

    void dspWritesMutateOnlyTheNamedLiveSlice()
    {
        WireCore core;
        QCOMPARE(core.model->addSlice(QStringLiteral("pan-0")), 1);
        QVERIFY(core.connect({{QByteArrayLiteral("setupDescription"), 1}}));
        SliceModel* first = core.model->sliceById(0);
        SliceModel* second = core.model->sliceById(1);
        QVERIFY(first != nullptr);
        QVERIFY(second != nullptr);
        const int firstTaps = first->nr1Taps();
        const double secondK1 = second->snbK1();
        core.app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:1", {MirrorUpdate{0, "nr1Taps", MirrorWireKind::Int64, qint64(88)}}, 201)));
        core.app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:0", {MirrorUpdate{0, "snbK1", MirrorWireKind::Float64, 7.5}}, 202)));
        QTRY_COMPARE(second->nr1Taps(), 88);
        QTRY_COMPARE(first->snbK1(), 7.5);
        QCOMPARE(first->nr1Taps(), firstTaps);
        QCOMPARE(second->snbK1(), secondK1);
        core.model->removeSlice(1);
        QVERIFY(core.model->sliceById(1) == nullptr);
        core.app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:1", {MirrorUpdate{0, "nr1Taps", MirrorWireKind::Int64, qint64(99)}}, 203)));
        auto retiredResult = [&core]() {
            for (const QByteArray& wire : core.app->received()) {
                const QJsonObject message = QJsonDocument::fromJson(wire).object();
                if (message.value("type") == QJsonValue("property.result")
                    && message.value("writeId").toInt() == 203) {
                    return message;
                }
            }
            return QJsonObject{};
        };
        QTRY_VERIFY(!retiredResult().isEmpty());
        const QJsonArray results = retiredResult().value("results").toArray();
        QCOMPARE(results.size(), 1);
        QVERIFY(!results.first().toObject().value("accepted").toBool());
    }

    void legacyCapabilitiesWireIsUnchanged()
    {
        StationCapabilities older;
        const QList<MirrorUpdate> baseline = older.toUpdates();
        for (const MirrorUpdate& update : baseline) {
            QVERIFY(update.name != "setupDescriptionVersion");
        }
        older.radioIdentityEntries = true;
        const QList<MirrorUpdate> legacyMinor11 = older.toUpdates();
        for (const MirrorUpdate& update : legacyMinor11) {
            QVERIFY(update.name != "setupDescriptionVersion");
        }
        older.setupDescriptionVersion = 1;
        const QList<MirrorUpdate> described = older.toUpdates();
        QCOMPARE(described.size(), legacyMinor11.size() + 1);
        QCOMPARE(described.at(described.size() - 2).name, QByteArray("setupDescriptionVersion"));
        QCOMPARE(described.last().name, QByteArray("accessoryTxVersion"));
        QCOMPARE(StationCapabilities::fromUpdates(described).setupDescriptionVersion, 1);
    }

    void onlyDeclaringPeerReceivesSetup()
    {
        WireCore legacy;
        QVERIFY(legacy.connect());
        QJsonObject stableHello = QJsonDocument::fromJson(legacy.app->received().first()).object();
        QVERIFY(stableHello.value("challenge").isString());
        QVERIFY(stableHello.value("identity").isObject());
        stableHello.remove("challenge");
        stableHello.remove("identity");
        QCOMPARE(QJsonDocument(stableHello).toJson(QJsonDocument::Compact), QByteArrayLiteral(
            "{\"features\":{\"deviceAuth\":1,\"pairing\":1,\"sessionHolder\":1},"
            "\"major\":1,\"majors\":[1],\"minor\":11,\"peer\":\"nereusd\","
            "\"settingsSchema\":0,\"type\":\"hello\"}"));
        QVERIFY(!hasSetupTraffic(*legacy.app));
        for (const QByteArray& wire : legacy.app->received()) {
            QVERIFY(!wire.contains("setupDescriptionVersion"));
        }
        legacy.app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "setup", {MirrorUpdate{0, "general", MirrorWireKind::Utf8,
                                   QStringLiteral("{}")}}, 43)));
        QTRY_VERIFY(legacy.app->receivedKinds().contains(QByteArrayLiteral("property.result")));
        QVERIFY(!hasSetupTraffic(*legacy.app));
        legacy.app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "tx.twoTonePreset", 44,
            {MirrorUpdate{0, "name", MirrorWireKind::Utf8, QStringLiteral("stealth")}})));
        QTRY_VERIFY(legacy.app->receivedKinds().contains(QByteArrayLiteral("command.result")));
        bool refusedPreset = false;
        for (const QByteArray& wire : legacy.app->received()) {
            const QJsonObject result = QJsonDocument::fromJson(wire).object();
            refusedPreset |= result.value("type") == QJsonValue("command.result")
                && result.value("id").toInt() == 44 && !result.value("accepted").toBool();
        }
        QVERIFY(refusedPreset);

        WireCore current;
        QVERIFY(current.connect({{QByteArrayLiteral("setupDescription"), 1}}));
        QVERIFY(hasSetupTraffic(*current.app));
        bool capability = false;
        for (const QByteArray& wire : current.app->received()) {
            capability |= wire.contains("setupDescriptionVersion");
        }
        QVERIFY(capability);
    }

    void negotiatedVersionsFitInitialSnapshotAndLaterDelta()
    {
        const auto check = [](int declared, quint16 minor, int expected) {
            WireCore core(HPSDRHW::HermesC10);
            QHash<QByteArray, int> features;
            if (declared > 0) { features.insert("setupDescription", declared); }
            QVERIFY(core.connect(features, minor));
            QCOMPARE(setupCapabilityOnWire(*core.app), expected);
            if (expected == 0) {
                QVERIFY(!hasSetupTraffic(*core.app));
                return;
            }
            const QString dsp = setupCategoryOnWire(
                *core.app, "dsp", SessionMessageKind::ObjectCreate);
            QVERIFY(!dsp.isEmpty());
            const QJsonObject display = QJsonDocument::fromJson(setupCategoryOnWire(
                *core.app, "display", SessionMessageKind::ObjectCreate).toUtf8()).object();
            QCOMPARE(display.value("version"),
                     QJsonValue(expected >= 12 ? 12 : expected >= 11 ? 11 : expected >= 10 ? 10 : expected >= 9 ? 9 : expected >= 8 ? 8 : qMin(expected, 4)));
            int displayControls = 0;
            for (const QJsonValue& page : display.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    displayControls += section.toObject().value("controls").toArray().size();
                }
            }
            QCOMPARE(displayControls, expected >= 12 ? 100 : expected >= 11 ? 48 : expected >= 10 ? 33 : expected >= 9 ? 29 : expected >= 8 ? 21 : expected >= 4 ? 14 : 11);
            QCOMPARE(display.value("pages").toArray().size(), expected >= 12 ? 7 : expected >= 11 ? 5 : expected >= 8 ? 4 : 3);
            const QJsonObject appearance = QJsonDocument::fromJson(setupCategoryOnWire(
                *core.app, "appearance", SessionMessageKind::ObjectCreate).toUtf8()).object();
            QCOMPARE(appearance.value("version"),
                     QJsonValue(expected >= 12 ? 12 : expected >= 7 ? 7 : qMin(expected, 4)));
            QCOMPARE(appearance.value("pages").toArray().size(), expected >= 7 ? 2 : 1);
            QCOMPARE(appearance.value("pages").toArray().first().toObject()
                         .value("sections").toArray().size(), expected >= 12 ? 2 : 1);
            QCOMPARE(appearance.value("pages").toArray().first().toObject()
                         .value("sections").toArray().first().toObject()
                         .value("controls").toArray().size(), 10);
            if (expected < 4) {
                for (const QJsonObject& projected : {display, appearance}) {
                    for (const QJsonValue& page : projected.value("pages").toArray()) {
                        for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                            for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                                const QJsonObject control = raw.toObject();
                                QVERIFY(!control.contains("default"));
                                if (control.value("kind") == QJsonValue("decimal")) {
                                    QVERIFY(!control.contains("decimals"));
                                }
                            }
                        }
                    }
                }
            }
            const QString pa = setupCategoryOnWire(
                *core.app, "pa", SessionMessageKind::ObjectCreate);
            QVERIFY(!pa.isEmpty());
            const QJsonObject paObject = QJsonDocument::fromJson(pa.toUtf8()).object();
            QCOMPARE(paObject.value("version").toInt(),
                     expected >= 20 ? 20 : expected >= 14 ? 14 : expected >= 13 ? 13 : qMin(expected, 5));
            QCOMPARE(paObject.value("pages").toArray().size(), expected >= 13 ? 3 : 2);
            for (const QJsonValue& page : paObject.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                        const QJsonObject control = raw.toObject();
                        // Version 13's ADC overload is text (No / Yes (ADC n)).
                        if (control.value("kind") == QJsonValue("readout")
                            && control.value("id") != QJsonValue("pa.values.adcOverload")) {
                            QVERIFY(control.value("decimals").isDouble());
                        }
                    }
                }
            }
            // CAT & Network: 15 to V15-V20, 21 (Forget's dependency) to V21.
            const QString catNetwork = setupCategoryOnWire(
                *core.app, "catNetwork", SessionMessageKind::ObjectCreate);
            if (expected >= 1) {
                QCOMPARE(QJsonDocument::fromJson(catNetwork.toUtf8()).object()
                             .value("version").toInt(),
                         expected >= 21 ? 21 : expected >= 15 ? 15 : qMin(expected, 3));
                QCOMPARE(catNetwork.contains(QStringLiteral("enabledWhen")), expected >= 21);
            }
            const QString diagnostics = setupCategoryOnWire(
                *core.app, "diagnostics", SessionMessageKind::ObjectCreate);
            if (expected < 3) {
                QVERIFY(diagnostics.isEmpty());
            } else {
                const QJsonObject panel = QJsonDocument::fromJson(diagnostics.toUtf8()).object();
                QCOMPARE(panel.value("version"), QJsonValue(expected >= 15 ? 15 : 3));
                QCOMPARE(panel.value("pages").toArray().size(), expected >= 15 ? 3 : 1);
            }
            const QJsonObject category = QJsonDocument::fromJson(dsp.toUtf8()).object();
            QCOMPARE(category.value("version").toInt(),
                     expected >= 22 ? 22 : expected >= 19 ? 19
                         : expected >= 15 ? 15 : qMin(expected, 3));
            QCOMPARE(category.value("pages").toArray().size(), expected >= 15 ? 10 : 9);
            bool hasTable = false;
            bool hasAdd = false;
            for (const QJsonValue& page : category.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                        const QString id = raw.toObject().value("id").toString();
                        hasTable |= id == QLatin1String("dsp.tnf.list");
                        hasAdd |= id == QLatin1String("dsp.tnf.add");
                    }
                }
            }
            QCOMPARE(hasTable, expected >= 2);
            QCOMPARE(hasAdd, expected >= 2);
            const QString v1 = SetupDescriptionService::fitCategoryForVersion(
                core.server->setupDescription()->dsp(), 1);
            QVERIFY(!v1.contains(QStringLiteral("requiresDescriptionVersion")));
            QCOMPARE(QJsonDocument::fromJson(v1.toUtf8()).object().value("version").toInt(), 1);
            BoardCapabilities changed = core.model->boardCapabilities();
            changed.attenuator.present = !changed.attenuator.present;
            core.server->setupDescription()->setBoardCapabilities(changed);
            QString general;
            QTRY_VERIFY(!(general = setupCategoryOnWire(
                *core.app, "general", SessionMessageKind::Delta)).isEmpty());
            QCOMPARE(QJsonDocument::fromJson(general.toUtf8()).object()
                         .value("version").toInt(), qMin(expected, 3));
        };
        check(0, kSessionProtocolMinor, 0);
        check(1, kSessionProtocolMinor, 1);
        check(2, kSessionProtocolMinor, 2);
        check(3, kSessionProtocolMinor, 3);
        check(4, kSessionProtocolMinor, 4);
        check(5, kSessionProtocolMinor, 5);
        check(6, kSessionProtocolMinor, 6);
        check(7, kSessionProtocolMinor, 7);
        check(8, kSessionProtocolMinor, 8);
        check(9, kSessionProtocolMinor, 9);
        check(10, kSessionProtocolMinor, 10);
        check(11, kSessionProtocolMinor, 11);
        check(12, kSessionProtocolMinor, 12);
        check(13, kSessionProtocolMinor, 13);
        check(14, kSessionProtocolMinor, 14);
        check(15, kSessionProtocolMinor, 15);
        check(16, kSessionProtocolMinor, 16);
        check(17, kSessionProtocolMinor, 17);
        check(18, kSessionProtocolMinor, 18);
        check(19, kSessionProtocolMinor, 19);
        check(20, kSessionProtocolMinor, 20);
        // 21 is CAT & Network's TCI Forget enabledWhen; 22 is the RX
        // buffer sizes' on-the-air lock; 23 is Calibration's Rx1 6m LNA
        // row; 24 is TX Input's Line In Gain steps and Saturn Mic
        // Tip-Ring, and the cap.
        check(21, kSessionProtocolMinor, 21);
        check(22, kSessionProtocolMinor, 22);
        check(23, kSessionProtocolMinor, 23);
        check(24, kSessionProtocolMinor, 24);
        check(25, kSessionProtocolMinor, 24);
        check(2, quint16(kRadioIdentitySessionProtocolMinor - 1), 0);
        check(3, quint16(kRadioIdentitySessionProtocolMinor - 1), 0);
    }

    void v4MetadataDoesNotLeakIntoOlderProjectedControls()
    {
        SetupDescriptionService service;
        const auto controlsOf = [](const QJsonObject& category) {
            QJsonArray controls;
            for (const QJsonValue& page : category.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& control : section.toObject().value("controls").toArray()) {
                        controls.append(control);
                    }
                }
            }
            return controls;
        };
        for (int version = 1; version <= 12; ++version) {
            const QJsonObject display = QJsonDocument::fromJson(
                SetupDescriptionService::fitCategoryForVersion(service.display(), version).toUtf8()).object();
            const QJsonObject appearance = QJsonDocument::fromJson(
                SetupDescriptionService::fitCategoryForVersion(service.appearance(), version).toUtf8()).object();
            QCOMPARE(display.value("version"),
                     QJsonValue(version >= 12 ? 12 : version >= 11 ? 11 : version >= 10 ? 10 : version >= 9 ? 9 : version >= 8 ? 8 : qMin(version, 4)));
            QCOMPARE(appearance.value("version"),
                     QJsonValue(version >= 12 ? 12 : version >= 7 ? 7 : qMin(version, 4)));
            QCOMPARE(controlsOf(display).size(), version < 4 ? 11 : version < 8 ? 14 : version < 9 ? 21 : version < 10 ? 29 : version < 11 ? 33 : version < 12 ? 48 : 100);
            QCOMPARE(display.value("pages").toArray().size(), version < 8 ? 3 : version < 11 ? 4 : version < 12 ? 5 : 7);
            QCOMPARE(controlsOf(appearance).size(), version < 7 ? 10 : version < 12 ? 13 : 14);
            QCOMPARE(appearance.value("pages").toArray().size(), version < 7 ? 1 : 2);
            for (const QJsonObject& category : {display, appearance}) {
                for (const QJsonValue& raw : controlsOf(category)) {
                    const QJsonObject control = raw.toObject();
                    const QString kind = control.value("kind").toString();
                    if (version < 4) {
                        QVERIFY2(!control.contains("default"), qPrintable(control.value("id").toString()));
                        if (kind == QLatin1String("decimal")) {
                            QVERIFY(!control.contains("decimals"));
                        }
                    } else if (kind == QLatin1String("colour")) {
                        const QString rgba = control.value("default").toString();
                        QVERIFY(QRegularExpression(QStringLiteral("^#[0-9A-F]{8}$")).match(rgba).hasMatch());
                    } else if (kind == QLatin1String("toggle")) {
                        QVERIFY(control.value("default").isBool());
                    } else if (kind == QLatin1String("button")) {
                        // Version 12 actions carry no value.
                        QVERIFY(!control.contains("default"));
                        QVERIFY(version >= 12);
                    } else {
                        QVERIFY(control.value("default").isDouble());
                        if (kind == QLatin1String("decimal")) {
                            // Hz/bin has two places (V4); the V12 noise
                            // floor shift and line width have one.
                            QCOMPARE(control.value("decimals"),
                                     QJsonValue(control.value("id")
                                         == QJsonValue("display.spectrumDefaults.hzPerBinTarget") ? 2 : 1));
                        } else {
                            const double ordinal = control.value("default").toDouble();
                            QCOMPARE(std::floor(ordinal), ordinal);
                        }
                    }
                }
            }
        }
    }

    void displayV10PublishesFourClosedWaterfallOverlays()
    {
        SetupDescriptionService service;
        const QJsonObject display = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(service.display(), 10).toUtf8()).object();
        QCOMPARE(display.value("version"), QJsonValue(10));
        const QJsonArray sections = display.value("pages").toArray().at(1).toObject()
            .value("sections").toArray();
        QCOMPARE(sections.size(), 2);
        QCOMPARE(sections.at(1).toObject().value("title"), QJsonValue("Overlays"));
        const QJsonArray overlays = sections.at(1).toObject().value("controls").toArray();
        QCOMPARE(overlays.size(), 4);
        const QStringList ids{"display.waterfallDefaults.showRxFilter",
                              "display.waterfallDefaults.showTxFilter",
                              "display.waterfallDefaults.showRxZeroLine",
                              "display.waterfallDefaults.showTxZeroLine"};
        const QStringList keys{"DisplayShowRxFilterOnWaterfall",
                               "DisplayShowTxFilterOnRxWaterfall",
                               "DisplayShowRxZeroLine", "DisplayShowTxZeroLine"};
        for (int i = 0; i < overlays.size(); ++i) {
            const QJsonObject control = overlays.at(i).toObject();
            QCOMPARE(control.value("id"), QJsonValue(ids.at(i)));
            QCOMPARE(control.value("binding"), QJsonValue(QJsonObject{{"phone", keys.at(i)}}));
            QCOMPARE(control.value("kind"), QJsonValue("toggle"));
            QCOMPARE(control.value("applies"), QJsonValue("live"));
            QCOMPARE(control.value("default"), QJsonValue(i == 1));
            QCOMPARE(control.value("requiresDescriptionVersion"), QJsonValue(10));
            QVERIFY(SetupDescriptionService::validateDisplayPhoneBinding(control));
            const auto rejects = [&control](const QString& field, const QJsonValue& value) {
                QJsonObject changed = control;
                changed.insert(field, value);
                QVERIFY(!SetupDescriptionService::validateDisplayPhoneBinding(changed));
            };
            rejects("binding", QJsonObject{{"setting", keys.at(i)}});
            rejects("binding", QJsonObject{{"phone", "Unknown"}});
            rejects("default", i != 1);
            rejects("requiresDescriptionVersion", 9);
            rejects("kind", "integer");
            rejects("applies", "subscription");
            rejects("min", 0);
        }
        const QJsonObject v9 = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(service.display(), 9).toUtf8()).object();
        QCOMPARE(v9.value("version"), QJsonValue(9));
        QCOMPARE(v9.value("pages").toArray().at(1).toObject()
                     .value("sections").toArray().size(), 1);
        const auto countControls = [](const QJsonObject& category) {
            int count = 0;
            for (const QJsonValue& page : category.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    count += section.toObject().value("controls").toArray().size();
                }
            }
            return count;
        };
        QCOMPARE(countControls(v9), 29);
        QCOMPARE(countControls(display), 33);
        QJsonObject prior = display;
        prior.insert("version", 9);
        QJsonArray priorPages = prior.value("pages").toArray();
        QJsonObject priorWaterfall = priorPages.at(1).toObject();
        QJsonArray priorSections = priorWaterfall.value("sections").toArray();
        priorSections.removeAt(1);
        priorWaterfall.insert("sections", priorSections);
        priorPages[1] = priorWaterfall;
        prior.insert("pages", priorPages);
        QCOMPARE(v9, prior);
    }

    void displayV11PublishesSpectrumPeaksPage()
    {
        SetupDescriptionService service;
        const QJsonObject display = projectedCategory(service.display(), 11);
        QCOMPARE(display.value("version"), QJsonValue(11));
        const QJsonArray pages = display.value("pages").toArray();
        QCOMPARE(pages.size(), 5);
        const QJsonObject peaks = pages.at(1).toObject();
        QCOMPARE(peaks.value("id"), QJsonValue("display.spectrumPeaks"));
        QCOMPARE(peaks.value("title"), QJsonValue("Spectrum Peaks"));
        QCOMPARE(peaks.value("where"), QJsonValue("phone"));
        QCOMPARE(peaks.value("coverage"), QJsonValue("partial"));
        QCOMPARE(pages.at(2).toObject().value("id"), QJsonValue("display.waterfallDefaults"));
        const QJsonArray sections = peaks.value("sections").toArray();
        QCOMPARE(sections.size(), 2);
        QCOMPARE(sections.at(0).toObject().value("title"), QJsonValue("Active Peak Hold"));
        QCOMPARE(sections.at(1).toObject().value("title"), QJsonValue("Peak Blobs"));
        QJsonArray controls;
        for (const QJsonValue& section : sections) {
            for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                controls.append(raw);
            }
        }
        QCOMPARE(sections.at(0).toObject().value("controls").toArray().size(), 6);
        QCOMPARE(sections.at(1).toObject().value("controls").toArray().size(), 9);
        const QStringList ids{
            "display.spectrumPeaks.activePeakHold", "display.spectrumPeaks.activePeakHoldTime",
            "display.spectrumPeaks.activePeakHoldDropRate",
            "display.spectrumPeaks.activePeakHoldFill", "display.spectrumPeaks.activePeakHoldOnTx",
            "display.spectrumPeaks.activePeakHoldColor",
            "display.spectrumPeaks.peakBlobs", "display.spectrumPeaks.peakBlobCount",
            "display.spectrumPeaks.peakBlobInsideFilter", "display.spectrumPeaks.peakBlobHold",
            "display.spectrumPeaks.peakBlobHoldTime", "display.spectrumPeaks.peakBlobHoldDrop",
            "display.spectrumPeaks.peakBlobFallRate", "display.spectrumPeaks.peakBlobColor",
            "display.spectrumPeaks.peakBlobTextColor"};
        const QStringList keys{
            "DisplayActivePeakHoldEnabled", "DisplayActivePeakHoldDurationMs",
            "DisplayActivePeakHoldDropDbPerSec",
            "DisplayActivePeakHoldFill", "DisplayActivePeakHoldOnTx", "DisplayActivePeakHoldColor",
            "DisplayPeakBlobsEnabled", "DisplayPeakBlobsCount",
            "DisplayPeakBlobsInsideFilterOnly", "DisplayPeakBlobsHoldEnabled",
            "DisplayPeakBlobsHoldMs", "DisplayPeakBlobsHoldDrop",
            "DisplayPeakBlobsFallDbPerSec", "DisplayPeakBlobColor", "DisplayPeakBlobTextColor"};
        const QStringList kinds{
            "toggle", "integer", "integer", "toggle", "toggle", "colour", "toggle", "integer",
            "toggle", "toggle", "integer", "toggle", "integer", "colour", "colour"};
        const QStringList applies{
            "subscription", "subscription", "subscription", "live", "subscription", "live",
            "subscription", "subscription", "subscription", "subscription", "subscription",
            "subscription", "subscription", "live", "live"};
        const QList<QJsonValue> defaults{
            false, 2000, 6, false, false, "#FFD700FF", false, 3, false, false, 500, false, 6,
            "#FF4500FF", "#7FFF00FF"};
        QCOMPARE(controls.size(), ids.size());
        // The peak hold's hold time and transmit switch need the Core that
        // honours them (displayExtrasVersion 3); the rest need 1.
        const auto gateFor = [&ids](int i) {
            const bool three = ids.at(i).endsWith("activePeakHoldTime")
                || ids.at(i).endsWith("activePeakHoldOnTx");
            return QJsonObject{{"capability", "displayExtrasVersion"}, {"min", three ? 3 : 1}};
        };
        for (int i = 0; i < controls.size(); ++i) {
            const QJsonObject control = controls.at(i).toObject();
            QCOMPARE(control.value("id"), QJsonValue(ids.at(i)));
            QCOMPARE(control.value("binding"), QJsonValue(QJsonObject{{"phone", keys.at(i)}}));
            QCOMPARE(control.value("kind"), QJsonValue(kinds.at(i)));
            QCOMPARE(control.value("applies"), QJsonValue(applies.at(i)));
            QCOMPARE(control.value("default"), defaults.at(i));
            QCOMPARE(control.value("requiresDescriptionVersion"), QJsonValue(11));
            QCOMPARE(control.value("gate"), QJsonValue(gateFor(i)));
            QVERIFY2(SetupDescriptionService::validateDisplayPhoneBinding(control),
                     qPrintable(ids.at(i)));
            const auto rejects = [&control, &ids, i](const QString& field, const QJsonValue& value) {
                QJsonObject changed = control;
                changed.insert(field, value);
                QVERIFY2(!SetupDescriptionService::validateDisplayPhoneBinding(changed),
                         qPrintable(ids.at(i) + QLatin1Char(' ') + field));
            };
            const auto rejectsWithout = [&control, &ids, i](const QString& field) {
                QJsonObject changed = control;
                changed.remove(field);
                QVERIFY2(!SetupDescriptionService::validateDisplayPhoneBinding(changed),
                         qPrintable(ids.at(i) + QStringLiteral(" without ") + field));
            };
            rejects("binding", QJsonObject{{"setting", keys.at(i)}});
            rejects("binding", QJsonObject{{"phone", "Unknown"}});
            rejects("requiresDescriptionVersion", 10);
            rejects("gate", QJsonObject{{"capability", "displayExtrasVersion"}, {"min", 2}});
            rejects("applies", applies.at(i) == "live" ? "subscription" : "live");
            rejects("label", "Different");
            rejects("tooltip", "Different");
            rejectsWithout("gate");
            if (kinds.at(i) == "integer") {
                rejects("default", -1);
                rejects("min", -1);
                rejects("max", 61000);
                rejects("step", 7);
                rejects("unit", "W");
            } else if (kinds.at(i) == "colour") {
                rejects("default", "#00000000");
                rejects("default", "red");
                rejects("min", 0);
            } else {
                rejects("default", !defaults.at(i).toBool());
                rejects("min", 0);
            }
        }
        const QString source = service.display();

        // Version 10 keeps its page list and its 33 controls exactly.
        const QJsonObject v10 = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(source, 10).toUtf8()).object();
        QCOMPARE(v10.value("version"), QJsonValue(10));
        QJsonObject prior = display;
        prior.insert("version", 10);
        QJsonArray priorPages = pages;
        priorPages.removeAt(1);
        prior.insert("pages", priorPages);
        QCOMPARE(v10, prior);
        int v10Controls = 0;
        for (const QJsonValue& page : v10.value("pages").toArray()) {
            for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                v10Controls += section.toObject().value("controls").toArray().size();
            }
        }
        QCOMPARE(v10Controls, 33);
        const QJsonObject v11 = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(source, 11).toUtf8()).object();
        QCOMPARE(v11, display);
    }

    void displayV12PublishesTheRestOfDisplay()
    {
        SetupDescriptionService service;
        const QJsonObject display = service.category(QStringLiteral("display"));
        QCOMPARE(display.value("version"), QJsonValue(12));
        const QJsonArray pages = display.value("pages").toArray();
        QStringList pageIds;
        for (const QJsonValue& page : pages) { pageIds << page.toObject().value("id").toString(); }
        QCOMPARE(pageIds, (QStringList{"display.spectrumDefaults", "display.spectrumPeaks",
                                       "display.waterfallDefaults", "display.gridScales",
                                       "display.multimeter", "display.txDisplay",
                                       "display.threeD"}));
        // The two new pages describe every control they have.
        for (const int index : {3, 6}) {
            const QJsonObject page = pages.at(index).toObject();
            QCOMPARE(page.value("where"), QJsonValue("phone"));
            QVERIFY(!page.contains("coverage"));
        }
        QCOMPARE(pages.at(3).toObject().value("title"), QJsonValue("Grid & Scales"));
        QCOMPARE(pages.at(6).toObject().value("title"), QJsonValue("3D View"));

        const QStringList ids{
            "display.spectrumDefaults.smoothDefaults", "display.spectrumDefaults.clarity",
            "display.spectrumDefaults.showCursorFreq", "display.spectrumDefaults.showBinWidth",
            "display.spectrumDefaults.showNoiseFloor", "display.spectrumDefaults.noiseFloorShift",
            "display.spectrumDefaults.noiseFloorLineWidth", "display.spectrumDefaults.noiseFloorColor",
            "display.spectrumDefaults.noiseFloorTextColor",
            "display.spectrumDefaults.noiseFloorFastColor", "display.spectrumDefaults.normalize",
            "display.spectrumDefaults.showPeakValue", "display.spectrumDefaults.peakValuePosition",
            "display.spectrumDefaults.peakTextDelay", "display.spectrumDefaults.getMonitorHz",
            "display.waterfallDefaults.highThreshold", "display.waterfallDefaults.lowThreshold",
            "display.waterfallDefaults.agc", "display.waterfallDefaults.useSpectrumMinMax",
            "display.waterfallDefaults.copySpectrumMinMax", "display.waterfallDefaults.nfAgc",
            "display.waterfallDefaults.nfAgcOffset", "display.waterfallDefaults.colorScheme",
            "display.waterfallDefaults.historyDepth", "display.waterfallDefaults.timestampPosition",
            "display.waterfallDefaults.timestampMode", "display.gridScales.showGrid",
            "display.gridScales.dbmScale", "display.gridScales.dbMax",
            "display.gridScales.dbMin", "display.gridScales.dbStep",
            "display.gridScales.freqLabelAlign", "display.gridScales.zeroLine",
            "display.gridScales.showFps", "display.gridScales.adjustGridMinToNoiseFloor",
            "display.gridScales.noiseFloorOffset", "display.gridScales.maintainGridRange",
            "display.gridScales.copyWaterfallThresholds", "display.multimeter.showDecimal",
            "display.multimeter.unitMode", "display.multimeter.historyDuration",
            "display.txDisplay.wfLowLevel", "display.txDisplay.wfHighLevel",
            "display.txDisplay.wfPalette", "display.txDisplay.wfLowColor",
            "display.threeD.reset", "display.threeD.renderMode",
            "display.threeD.floor", "display.threeD.gain",
            "display.threeD.span", "display.threeD.angle",
            "display.threeD.sliceShadow"};
        const QStringList keys{
            "smoothDefaults", "ClarityEnabled", "DisplayShowCursorFreq",
            "DisplayShowBinWidth", "DisplayShowNoiseFloor", "DisplayNoiseFloorShiftDb",
            "DisplayNoiseFloorLineWidth", "DisplayNoiseFloorColor", "DisplayNoiseFloorTextColor",
            "DisplayNoiseFloorFastColor",
            "DisplayDispNormalize", "DisplayShowPeakValueOverlay", "DisplayPeakValuePosition",
            "DisplayPeakTextDelayMs", "getMonitorHz", "DisplayWfHighLevel",
            "DisplayWfLowLevel", "DisplayWfAgc", "DisplayWfUseSpectrumMinMax",
            "copySpectrumMinMax", "WaterfallNFAGCEnabled", "WaterfallAGCOffsetDb",
            "DisplayWfColorScheme", "DisplayWaterfallHistoryMs", "DisplayWfTimestampPos",
            "DisplayWfTimestampMode", "DisplayGridEnabled", "DisplayDbmScaleVisible",
            "DisplayGridMax", "DisplayGridMin", "DisplayGridStep",
            "DisplayFreqLabelAlign", "DisplayShowZeroLine", "DisplayShowFps",
            "DisplayAdjustGridMinToNoiseFloor", "DisplayNFOffsetGridFollow", "DisplayMaintainNFAdjustDelta",
            "copyWaterfallThresholds", "MultimeterShowDecimal", "MultimeterUnitMode",
            "MultimeterSignalHistoryDurationMs", "DisplayTxWfLowLevel", "DisplayTxWfHighLevel",
            "DisplayTxWfPalette", "DisplayTxWfLowColor", "reset3d",
            "DisplaySpectrumRenderMode", "Display3DFloorDepth", "Display3DGain",
            "Display3DSpan", "Display3DAngle", "Display3DSliceShadow"};
        QJsonArray v12;
        int total = 0;
        for (const QJsonValue& page : pages) {
            for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                    ++total;
                    if (raw.toObject().value("requiresDescriptionVersion") == QJsonValue(12)) {
                        v12.append(raw);
                    }
                }
            }
        }
        QCOMPARE(total, 100);
        QCOMPARE(v12.size(), ids.size());
        for (int i = 0; i < v12.size(); ++i) {
            const QJsonObject control = v12.at(i).toObject();
            const QString kind = control.value("kind").toString();
            QCOMPARE(control.value("id"), QJsonValue(ids.at(i)));
            QCOMPARE(control.value("binding"), QJsonValue(QJsonObject{{"phone", keys.at(i)}}));
            QVERIFY2(SetupDescriptionService::validateDisplayPhoneBinding(control),
                     qPrintable(ids.at(i)));
            QVERIFY(!SetupDescriptionService::validateDisplaySettingBinding(control));
            QCOMPARE(control.contains("default"), kind != QLatin1String("button"));
            const auto rejects = [&control, &ids, i](const QString& field, const QJsonValue& value) {
                QJsonObject changed = control;
                changed.insert(field, value);
                QVERIFY2(!SetupDescriptionService::validateDisplayPhoneBinding(changed),
                         qPrintable(ids.at(i) + QLatin1Char(' ') + field));
            };
            const auto rejectsWithout = [&control, &ids, i](const QString& field) {
                QJsonObject changed = control;
                changed.remove(field);
                QVERIFY2(!SetupDescriptionService::validateDisplayPhoneBinding(changed),
                         qPrintable(ids.at(i) + QStringLiteral(" without ") + field));
            };
            rejects("binding", QJsonObject{{"setting", keys.at(i)}});
            rejects("binding", QJsonObject{{"phone", "Unknown"}});
            rejects("requiresDescriptionVersion", 11);
            rejects("applies", control.value("applies") == QJsonValue("live") ? "subscription" : "live");
            rejects("label", "Different");
            rejects("tooltip", "Different");
            rejects("kind", kind == QLatin1String("toggle") ? "button" : "toggle");
            rejects("valueEncoding", QJsonObject{{"true", "True"}, {"false", "False"}});
            rejects("availability", QJsonObject{{"enabled", false}, {"reason", "x"}});
            for (const QString& field : control.keys()) {
                if (field != QLatin1String("id")) { rejectsWithout(field); }
            }
            if (control.contains("gate")) {
                QJsonObject gate = control.value("gate").toObject();
                gate.insert("min", gate.value("min").toInt() + 1);
                rejects("gate", gate);
            } else {
                rejects("gate", QJsonObject{{"capability", "displayExtrasVersion"}, {"min", 1}});
            }
            if (control.contains("min")) {
                rejects("min", control.value("min").toDouble() - 1);
                rejects("max", control.value("max").toDouble() + 1);
                rejects("step", control.value("step").toDouble() * 2);
            }
            if (control.contains("options")) {
                QJsonArray options = control.value("options").toArray();
                const QJsonValue first = options.at(0);
                options[0] = options.at(1);
                options[1] = first;
                rejects("options", options);
            }
            if (kind == QLatin1String("toggle")) {
                rejects("default", !control.value("default").toBool());
            } else if (kind == QLatin1String("colour")) {
                rejects("default", "#00000001");
            } else if (kind != QLatin1String("button")) {
                rejects("default", control.value("default").toDouble() + 1);
            }
        }
        // Closed dependencies and per-band values.
        const auto byId = [&v12](const QString& id) {
            for (const QJsonValue& raw : v12) {
                if (raw.toObject().value("id") == QJsonValue(id)) { return raw.toObject(); }
            }
            return QJsonObject{};
        };
        QCOMPARE(byId("display.spectrumDefaults.normalize").value("enabledWhen"),
                 QJsonValue(QJsonObject{{"phone", "DisplaySpectrumDetector"},
                                        {"oneOf", QJsonArray{2, 3, 4}}}));
        for (const QString& id : {QStringLiteral("display.waterfallDefaults.highThreshold"),
                                  QStringLiteral("display.waterfallDefaults.lowThreshold"),
                                  QStringLiteral("display.waterfallDefaults.agc"),
                                  QStringLiteral("display.waterfallDefaults.nfAgc"),
                                  QStringLiteral("display.waterfallDefaults.nfAgcOffset")}) {
            QCOMPARE(byId(id).value("enabledWhen"),
                     QJsonValue(QJsonObject{{"phone", "DisplayWfUseSpectrumMinMax"},
                                            {"oneOf", QJsonArray{false}}}));
        }
        QCOMPARE(byId("display.gridScales.dbMax").value("perBand"),
                 QJsonValue(QJsonObject{{"label", "dB Max (%1):"}}));
        QCOMPARE(byId("display.gridScales.dbMin").value("perBand"),
                 QJsonValue(QJsonObject{{"label", "dB Min (%1):"}}));
        QCOMPARE(byId("display.threeD.floor").value("perBand"),
                 QJsonValue(QJsonObject{{"label", "3D Floor:"}}));
        // The surface starts at the pan's noise floor less this depth
        // (SpectrumWidget::dssFloorDbm), kept per band (PanadapterModel).
        QCOMPARE(byId("display.threeD.floor").value("tooltip"),
                 QJsonValue("How far below the noise floor the surface starts. "
                            "Each band keeps its own value."));
        QStringList buttons;
        for (const QJsonValue& raw : v12) {
            if (raw.toObject().value("kind") == QJsonValue("button")) {
                buttons << raw.toObject().value("binding").toObject().value("phone").toString();
            }
        }
        QCOMPARE(buttons, (QStringList{"smoothDefaults", "getMonitorHz", "copySpectrumMinMax",
                                       "copyWaterfallThresholds", "reset3d"}));
        QVERIFY(byId("display.spectrumDefaults.smoothDefaults").value("confirm").isString());
        QVERIFY(byId("display.threeD.reset").value("confirm").isString());
        QVERIFY(!byId("display.gridScales.copyWaterfallThresholds").contains("confirm"));

        // Built on the desktop but not described: no effect for a phone or a
        // remote window (cal offset, thread priority, the deprecated NF text
        // position), the phone owns its own (line width, D75), no editor kind (TX custom
        // gradient), or hidden as unbuilt (Multimeter holds and averaging).
        const QString source = service.display();
        for (const char* key : {"DisplayLineWidth", "DisplayCalOffset",
                                "DisplayShowNoiseFloorPosition",
                                "DisplayTxWfGradient", "MultimeterPeakHoldMs",
                                "MultimeterTextHoldMs", "MultimeterAverageWindow",
                                "MultimeterDigitalDelayMs", "MultimeterSignalHistoryEnabled"}) {
            QVERIFY2(!source.contains(QLatin1String(key)), key);
        }

        // Version 11 is this description without its version 12 rows.
        QJsonObject prior = display;
        prior.insert("version", 11);
        QJsonArray priorPages;
        for (const QJsonValue& rawPage : pages) {
            QJsonObject page = rawPage.toObject();
            QJsonArray sections;
            for (const QJsonValue& rawSection : page.value("sections").toArray()) {
                QJsonObject section = rawSection.toObject();
                QJsonArray controls;
                for (const QJsonValue& raw : section.value("controls").toArray()) {
                    if (raw.toObject().value("requiresDescriptionVersion") != QJsonValue(12)) {
                        controls.append(raw);
                    }
                }
                if (!controls.isEmpty()) {
                    section.insert("controls", controls);
                    sections.append(section);
                }
            }
            if (!sections.isEmpty()) {
                page.insert("sections", sections);
                priorPages.append(page);
            }
        }
        prior.insert("pages", priorPages);
        QCOMPARE(projectedCategory(source, 11), prior);
        QCOMPARE(priorPages.size(), 5);
        QCOMPARE(projectedCategory(source, 13), display);
    }

    void appearanceV12PublishesResetColours()
    {
        SetupDescriptionService service;
        const QJsonObject appearance = service.category(QStringLiteral("appearance"));
        QCOMPARE(appearance.value("version"), QJsonValue(12));
        const QJsonArray sections = appearance.value("pages").toArray().first().toObject()
            .value("sections").toArray();
        QCOMPARE(sections.size(), 2);
        QCOMPARE(sections.at(1).toObject().value("title"), QJsonValue("Reset"));
        const QJsonArray controls = sections.at(1).toObject().value("controls").toArray();
        QCOMPARE(controls.size(), 1);
        const QJsonObject reset = controls.first().toObject();
        QCOMPARE(reset.value("id"), QJsonValue("appearance.colorsTheme.resetColors"));
        QCOMPARE(reset.value("kind"), QJsonValue("button"));
        QCOMPARE(reset.value("binding"), QJsonValue(QJsonObject{{"phone", "resetColors"}}));
        QCOMPARE(reset.value("requiresDescriptionVersion"), QJsonValue(12));
        QVERIFY(SetupDescriptionService::validateAppearanceResetColours(reset));
        QVERIFY(!SetupDescriptionService::validateAppearanceColourBinding(reset));
        for (const QString& field : reset.keys()) {
            QJsonObject changed = reset;
            changed.remove(field);
            QVERIFY2(!SetupDescriptionService::validateAppearanceResetColours(changed),
                     qPrintable(field));
        }
        QJsonObject changed = reset;
        changed.insert("label", "Different");
        QVERIFY(!SetupDescriptionService::validateAppearanceResetColours(changed));
        changed = reset;
        changed.insert("gate", QJsonObject{{"transmit", true}});
        QVERIFY(!SetupDescriptionService::validateAppearanceResetColours(changed));
        // Versions 7 to 11 keep version 7 exactly; the low level colour
        // stays hidden and undescribed.
        for (int version = 7; version <= 11; ++version) {
            const QJsonObject older = projectedCategory(service.appearance(), version);
            QCOMPARE(older.value("version"), QJsonValue(7));
            QCOMPARE(older.value("pages").toArray().first().toObject()
                         .value("sections").toArray().size(), 1);
        }
        QVERIFY(!service.appearance().contains(QStringLiteral("waterfallLowColor")));
    }

    void displayV9PublishesClosedLocalRendererControls()
    {
        SetupDescriptionService service;
        const QJsonObject display = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(service.display(), 9).toUtf8()).object();
        QCOMPARE(display.value("version"), QJsonValue(9));
        const QJsonArray pages = display.value("pages").toArray();
        QCOMPARE(pages.size(), 4);
        const QJsonArray spectrum = pages.at(0).toObject().value("sections").toArray()
            .at(1).toObject().value("controls").toArray();
        const QJsonArray waterfall = pages.at(1).toObject().value("sections").toArray()
            .at(0).toObject().value("controls").toArray();
        QCOMPARE(spectrum.size(), 10);
        QCOMPARE(waterfall.size(), 6);
        const QStringList ids{
            "display.spectrumDefaults.panFill", "display.spectrumDefaults.fillAlpha",
            "display.spectrumDefaults.gradient", "display.spectrumDefaults.peakHold",
            "display.spectrumDefaults.peakDelay", "display.waterfallDefaults.updatePeriod",
            "display.waterfallDefaults.stopOnTx", "display.waterfallDefaults.opacity"};
        const QStringList keys{
            "DisplayPanFill", "DisplayFftFillAlpha", "DisplayGradientEnabled",
            "DisplayPeakHoldEnabled", "DisplayPeakHoldResetMs", "DisplayWfUpdatePeriodMs",
            "WaterfallStopOnTx", "DisplayWfOpacity"};
        QJsonArray controls;
        for (int i = 5; i < spectrum.size(); ++i) { controls.append(spectrum.at(i)); }
        for (int i = 3; i < waterfall.size(); ++i) { controls.append(waterfall.at(i)); }
        QCOMPARE(controls.size(), ids.size());
        for (int i = 0; i < controls.size(); ++i) {
            const QJsonObject control = controls.at(i).toObject();
            QCOMPARE(control.value("id"), QJsonValue(ids.at(i)));
            QCOMPARE(control.value("binding"), QJsonValue(QJsonObject{{"phone", keys.at(i)}}));
            QCOMPARE(control.value("requiresDescriptionVersion"), QJsonValue(9));
            QVERIFY(SetupDescriptionService::validateDisplayPhoneBinding(control));
            const auto rejects = [&control, &keys, i](const QString& field, const QJsonValue& value) {
                QJsonObject changed = control;
                changed.insert(field, value);
                QVERIFY(!SetupDescriptionService::validateDisplayPhoneBinding(changed));
            };
            rejects(QStringLiteral("binding"), QJsonObject{{"setting", keys.at(i)}});
            rejects(QStringLiteral("binding"), QJsonObject{{"phone", "Unknown"}});
            rejects(QStringLiteral("default"), -1);
            rejects(QStringLiteral("requiresDescriptionVersion"), 8);
            if (control.contains("min")) {
                rejects(QStringLiteral("min"), -1);
                rejects(QStringLiteral("max"), 10001);
                rejects(QStringLiteral("step"), 2);
            } else {
                rejects(QStringLiteral("min"), 0);
            }
        }
        QCOMPARE(controls.at(1).toObject().value("step"), QJsonValue(1));
        QCOMPARE(controls.at(5).toObject().value("step"), QJsonValue(1));
        for (int version = 1; version <= 8; ++version) {
            const QJsonObject older = QJsonDocument::fromJson(
                SetupDescriptionService::fitCategoryForVersion(service.display(), version).toUtf8()).object();
            QCOMPARE(older.value("version"), QJsonValue(version >= 8 ? 8 : qMin(version, 4)));
            QCOMPARE(older.value("pages").toArray().size(), version >= 8 ? 4 : 3);
            const QJsonArray oldSpectrum = older.value("pages").toArray().at(0).toObject()
                .value("sections").toArray().at(1).toObject().value("controls").toArray();
            QCOMPARE(oldSpectrum.size(), version >= 8 ? 5 : 1);
            if (version >= 8) {
                const QJsonArray oldWaterfall = older.value("pages").toArray().at(1).toObject()
                    .value("sections").toArray().at(0).toObject().value("controls").toArray();
                QCOMPARE(oldWaterfall.size(), 3);
            }
        }
    }

    void displayV8PublishesSevenPhoneOwnedRxControls()
    {
        SetupDescriptionService service;
        const QJsonObject source = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(service.display(), 8).toUtf8()).object();
        QCOMPARE(source.value("version"), QJsonValue(8));
        const QJsonArray pages = source.value("pages").toArray();
        QCOMPARE(pages.size(), 4);
        const QJsonArray spectrum = pages.at(0).toObject().value("sections").toArray()
            .at(1).toObject().value("controls").toArray();
        QCOMPARE(spectrum.size(), 5);
        const QJsonArray waterfall = pages.at(1).toObject().value("sections").toArray()
            .at(0).toObject().value("controls").toArray();
        QCOMPARE(waterfall.size(), 3);
        QJsonArray newControls;
        for (int i = 1; i < spectrum.size(); ++i) { newControls.append(spectrum.at(i)); }
        for (const QJsonValue& raw : waterfall) { newControls.append(raw); }
        const QStringList expectedIds{
            QStringLiteral("display.spectrumDefaults.detector"),
            QStringLiteral("display.spectrumDefaults.averaging"),
            QStringLiteral("display.spectrumDefaults.averageTime"),
            QStringLiteral("display.spectrumDefaults.decimation"),
            QStringLiteral("display.waterfallDefaults.detector"),
            QStringLiteral("display.waterfallDefaults.averaging"),
            QStringLiteral("display.waterfallDefaults.averageTime")};
        QCOMPARE(newControls.size(), expectedIds.size());
        int index = 0;
        for (const QJsonValue& raw : newControls) {
            const QJsonObject control = raw.toObject();
            QCOMPARE(control.value("id"), QJsonValue(expectedIds.at(index++)));
            QCOMPARE(control.value("requiresDescriptionVersion"), QJsonValue(8));
            QCOMPARE(control.value("binding").toObject().size(), 1);
            QVERIFY(control.value("binding").toObject().contains("phone"));
            QVERIFY(SetupDescriptionService::validateDisplayPhoneBinding(control));
            const auto rejects = [&control](const QString& field, const QJsonValue& value) {
                QJsonObject changed = control;
                changed.insert(field, value);
                QVERIFY(!SetupDescriptionService::validateDisplayPhoneBinding(changed));
            };
            rejects(QStringLiteral("binding"), QJsonObject{{"setting", "DisplaySpectrumDetector"}});
            rejects(QStringLiteral("binding"), QJsonObject{{"phone", "UnknownDisplayControl"}});
            rejects(QStringLiteral("kind"), QStringLiteral("toggle"));
            rejects(QStringLiteral("applies"), QStringLiteral("live"));
            rejects(QStringLiteral("requiresDescriptionVersion"), 7);
            rejects(QStringLiteral("default"), -1);
            rejects(QStringLiteral("gate"), QJsonObject{{"capability", "txDisplayVersion"}, {"min", 1}});
            if (control.value("kind") == QJsonValue("choice")) {
                rejects(QStringLiteral("options"), QJsonArray{});
                rejects(QStringLiteral("min"), 0);
                QJsonArray reordered = control.value("options").toArray();
                const QJsonValue first = reordered.at(0);
                reordered[0] = reordered.at(1);
                reordered[1] = first;
                rejects(QStringLiteral("options"), reordered);
            } else {
                rejects(QStringLiteral("min"), -1);
                rejects(QStringLiteral("max"), 10000);
                rejects(QStringLiteral("step"), 2);
                rejects(QStringLiteral("unit"), QStringLiteral("W"));
                rejects(QStringLiteral("options"), QJsonArray{});
            }
        }
        QCOMPARE(pages.at(1).toObject().value("id"), QJsonValue("display.waterfallDefaults"));
        QCOMPARE(pages.at(1).toObject().value("where"), QJsonValue("phone"));
        for (int version = 1; version <= 7; ++version) {
            const QJsonObject older = QJsonDocument::fromJson(
                SetupDescriptionService::fitCategoryForVersion(service.display(), version).toUtf8()).object();
            QCOMPARE(older.value("pages").toArray().size(), 3);
            QCOMPARE(older.value("pages").toArray().at(0).toObject().value("sections")
                         .toArray().at(1).toObject().value("controls").toArray().size(), 1);
        }
    }

    void displayDescriptionKeepsVersionFourFftOptions()
    {
        SetupDescriptionService service;
        const QJsonObject display = service.category(QStringLiteral("display"));
        QCOMPARE(display.value("version"), QJsonValue(12));
        QCOMPARE(display.value("pages").toArray().size(), 7);
        const QString source = service.display();
        for (int version = 1; version <= 12; ++version) {
            const QJsonObject fitted = QJsonDocument::fromJson(
                SetupDescriptionService::fitCategoryForVersion(source, version).toUtf8()).object();
            QCOMPARE(fitted.value("version"),
                     QJsonValue(version >= 12 ? 12 : version >= 11 ? 11 : version >= 10 ? 10 : version >= 9 ? 9 : version >= 8 ? 8 : qMin(version, 4)));
            int count = 0;
            int optionSliders = 0;
            for (const QJsonValue& page : fitted.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                        const QJsonObject control = raw.toObject();
                        ++count;
                        if (control.value("kind") == QJsonValue("slider")
                            && control.contains("options")) {
                            ++optionSliders;
                            QCOMPARE(control.value("kind"), QJsonValue("slider"));
                            QVERIFY(!control.contains("min"));
                            QVERIFY(!control.contains("max"));
                            QVERIFY(!control.contains("step"));
                        }
                    }
                }
            }
            QCOMPARE(count, version >= 12 ? 100 : version >= 11 ? 48 : version >= 10 ? 33 : version >= 9 ? 29 : version >= 8 ? 21 : version >= 4 ? 14 : 11);
            QCOMPARE(optionSliders, version >= 4 ? 2 : 0);
        }
    }

    void displayV4RejectsMalformedOptionsAndNormalizeDependency()
    {
        SetupDescriptionService service;
        const QJsonArray pages = projectedCategory(service.display(), 11).value("pages").toArray();
        const QJsonObject rxFft = pages.at(0).toObject().value("sections").toArray().at(0)
            .toObject().value("controls").toArray().at(0).toObject();
        QVERIFY(SetupDescriptionService::validateDisplaySettingBinding(rxFft));
        const QJsonArray original = rxFft.value("options").toArray();
        const auto rejects = [&rxFft](const QJsonArray& options) {
            QJsonObject changed = rxFft;
            changed.insert("options", options);
            QVERIFY(!SetupDescriptionService::validateDisplaySettingBinding(changed));
        };
        rejects({});
        QJsonArray duplicate = original;
        duplicate[1] = duplicate[0];
        rejects(duplicate);
        QJsonArray reversed = original;
        reversed[0] = original[1];
        reversed[1] = original[0];
        rejects(reversed);
        QJsonArray unexpected = original;
        unexpected[6] = QJsonObject{{"value", 524288}, {"label", "524288"}};
        rejects(unexpected);
        QJsonArray nonfinite = original;
        nonfinite[0] = QJsonObject{{"value", QJsonValue()}, {"label", "4096"}};
        rejects(nonfinite);
        QJsonObject wrongDefault = rxFft;
        wrongDefault.insert("default", 1024);
        QVERIFY(!SetupDescriptionService::validateDisplaySettingBinding(wrongDefault));
        QJsonObject numericRange = rxFft;
        numericRange.insert("min", 4096);
        QVERIFY(!SetupDescriptionService::validateDisplaySettingBinding(numericRange));

        const QJsonObject normalize = pages.at(4).toObject().value("sections").toArray().at(1)
            .toObject().value("controls").toArray().at(3).toObject();
        QVERIFY(SetupDescriptionService::validateDisplaySettingBinding(normalize));
        for (const QJsonObject& dependency : {
                 QJsonObject{},
                 QJsonObject{{"setting", "DisplayTxWfDetector"},
                             {"oneOf", QJsonArray{"2", "3", "4"}}},
                 QJsonObject{{"setting", "DisplayTxPanDetector"},
                             {"oneOf", QJsonArray{"2", "4"}}},
                 QJsonObject{{"setting", "DisplayTxPanDetector"},
                             {"oneOf", QJsonArray{"2", "3", "4"}}, {"extra", true}}}) {
            QJsonObject changed = normalize;
            changed.insert("enabledWhen", dependency);
            QVERIFY(!SetupDescriptionService::validateDisplaySettingBinding(changed));
        }
    }

    void tnfTableRejectsMalformedSourcesAndActions()
    {
        SetupDescriptionService service;
        QJsonObject table;
        QJsonObject add;
        for (const QJsonValue& page : service.category(QStringLiteral("dsp"))
                 .value("pages").toArray()) {
            for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                    const QJsonObject control = raw.toObject();
                    if (control.value("id") == QJsonValue("dsp.tnf.list")) { table = control; }
                    if (control.value("id") == QJsonValue("dsp.tnf.add")) { add = control; }
                }
            }
        }
        QVERIFY(!table.isEmpty());
        QVERIFY(!add.isEmpty());
        QString error;
        QVERIFY2(SetupDescriptionService::validateTnfTable(table, &error), qPrintable(error));
        QVERIFY2(SetupDescriptionService::validateCommandBinding(add, &error), qPrintable(error));
        const auto rejects = [&](const QJsonObject& bad) {
            QVERIFY(!SetupDescriptionService::validateTnfTable(bad, &error));
        };
        QJsonObject bad = table;
        QJsonObject binding = bad.value("binding").toObject();
        QJsonObject source = binding.value("table").toObject();
        source.insert("format", "json-path");
        binding.insert("table", source);
        bad.insert("binding", binding);
        rejects(bad);
        bad = table;
        QJsonArray columns = bad.value("columns").toArray();
        QJsonObject column = columns.at(0).toObject();
        column.insert("min", 0);
        columns[0] = column;
        bad.insert("columns", columns);
        rejects(bad);
        bad = table;
        QJsonArray actions = bad.value("rowActions").toArray();
        QJsonObject action = actions.at(0).toObject();
        QJsonObject command = action.value("command").toObject();
        QJsonObject args = command.value("arguments").toObject();
        args.insert("id", QJsonObject{{"$row", "id"}, {"fallback", 0}});
        command.insert("arguments", args);
        action.insert("command", command);
        actions[0] = action;
        bad.insert("rowActions", actions);
        rejects(bad);
        bad = table;
        actions = bad.value("rowActions").toArray();
        action = actions.at(0).toObject();
        command = action.value("command").toObject();
        command.insert("verb", "notch.add");
        action.insert("command", command);
        actions[0] = action;
        bad.insert("rowActions", actions);
        rejects(bad);
        bad = table;
        QJsonObject gate = bad.value("gate").toObject();
        gate.insert("min", 0);
        bad.insert("gate", gate);
        rejects(bad);
        bad = add;
        binding = bad.value("binding").toObject();
        command = binding.value("command").toObject();
        args = command.value("arguments").toObject();
        args.insert("sliceId", QJsonObject{{"$selectedOwnedSliceId", true}, {"fallback", 0}});
        command.insert("arguments", args);
        binding.insert("command", command);
        bad.insert("binding", binding);
        QVERIFY(!SetupDescriptionService::validateCommandBinding(bad, &error));
        args.insert("sliceId", QJsonObject{{"$selectedOwnedSliceId", false}});
        command.insert("arguments", args);
        binding.insert("command", command);
        bad.insert("binding", binding);
        QVERIFY(!SetupDescriptionService::validateCommandBinding(bad, &error));
    }

    void tnfDescribedVerbsExecuteThroughCoreSession()
    {
        WireCore core;
        QVERIFY(core.connect({{QByteArrayLiteral("setupDescription"), 2}}));
        QCOMPARE(setupCapabilityOnWire(*core.app), 2);
        const QJsonObject dsp = QJsonDocument::fromJson(setupCategoryOnWire(
            *core.app, "dsp", SessionMessageKind::ObjectCreate).toUtf8()).object();
        QJsonObject table;
        QJsonObject add;
        for (const QJsonValue& page : dsp.value("pages").toArray()) {
            for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                    const QJsonObject control = raw.toObject();
                    if (control.value("id") == QJsonValue("dsp.tnf.list")) { table = control; }
                    if (control.value("id") == QJsonValue("dsp.tnf.add")) { add = control; }
                }
            }
        }
        QVERIFY(!table.isEmpty());
        QVERIFY(!add.isEmpty());
        quint32 nextId = 100;
        const auto invoke = [&](const QByteArray& verb, const QList<MirrorUpdate>& args) {
            const quint32 id = ++nextId;
            core.app->sendText(SessionMessages::encode(
                SessionMessages::commandInvoke(verb, id, args)));
            SessionMessage result;
            result.reason = QStringLiteral("no command.result arrived");
            (void)QTest::qWaitFor([&] {
                for (const QByteArray& wire : core.app->received()) {
                    SessionMessage message;
                    if (SessionMessages::decode(wire, &message)
                        && message.kind == SessionMessageKind::CommandResult
                        && message.commandId == id) {
                        result = message;
                        return true;
                    }
                }
                return false;
            }, 3000);
            return result;
        };
        const auto i64 = [](const QByteArray& name, int value) {
            return MirrorUpdate{0, name, MirrorWireKind::Int64, qlonglong(value)};
        };
        const QByteArray addVerb = add.value("binding").toObject()
            .value("command").toObject().value("verb").toString().toUtf8();
        const QString session = QStringLiteral("phone-A");
        const quint64 epoch = 7;
        const QList<MirrorUpdate> addArgs = materializeTnfAdd(
            add, 0, 0, true, session, session, epoch, epoch);
        QCOMPARE(addArgs.size(), 1);
        QVERIFY(materializeTnfAdd(add, 0, 1, true, session, session, epoch, epoch).isEmpty());
        QVERIFY(materializeTnfAdd(add, 0, 0, false, session, session, epoch, epoch).isEmpty());
        QVERIFY(materializeTnfAdd(add, 0, 0, true, session, QStringLiteral("phone-B"),
                                 epoch, epoch).isEmpty());
        QVERIFY(materializeTnfAdd(add, 0, 0, true, session, session,
                                 epoch, epoch + 1).isEmpty());
        SessionMessage result = invoke(addVerb, addArgs);
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(core.model->notchModel()->notches().size(), 1);
        const int rowId = core.model->notchModel()->notches().first().id;
        const QJsonArray rowActions = table.value("rowActions").toArray();
        const QByteArray moveVerb = rowActions.at(0).toObject().value("command")
            .toObject().value("verb").toString().toUtf8();
        const QByteArray activeVerb = rowActions.at(1).toObject().value("command")
            .toObject().value("verb").toString().toUtf8();
        const QByteArray deleteVerb = rowActions.at(2).toObject().value("command")
            .toObject().value("verb").toString().toUtf8();
        const quint32 revision = core.model->notchModel()->revision();
        const QString list = core.model->notchModel()->listJson();
        const QJsonObject edits{{"centreHz", 14075000.0}, {"widthHz", 300.0}};
        const QJsonObject move = rowActions.at(0).toObject();
        const QList<MirrorUpdate> moveArgs = materializeTnfRowAction(
            move, list, revision, revision, rowId, edits, session, session, epoch, epoch);
        QCOMPARE(moveArgs.size(), 3);
        QVERIFY(materializeTnfRowAction(move, list, revision + 1, revision, rowId,
                edits, session, session, epoch, epoch).isEmpty());
        QVERIFY(materializeTnfRowAction(move, list, revision, revision, rowId,
                edits, session, QStringLiteral("phone-B"), epoch, epoch).isEmpty());
        QVERIFY(materializeTnfRowAction(move, list, revision, revision, rowId,
                edits, session, session, epoch, epoch + 1).isEmpty());
        QVERIFY(materializeTnfRowAction(move, list, revision, revision, rowId + 500,
                edits, session, session, epoch, epoch).isEmpty());
        QVERIFY(materializeTnfRowAction(move,
                QStringLiteral("[{\"id\":1,\"centreHz\":14074000,\"widthHz\":200,\"active\":true},"
                               "{\"id\":1,\"centreHz\":14074000,\"widthHz\":200,\"active\":true}]"),
                revision, revision, 1, edits, session, session, epoch, epoch).isEmpty());
        QVERIFY(materializeTnfRowAction(move, QStringLiteral("not-an-array"),
                revision, revision, rowId, edits, session, session, epoch, epoch).isEmpty());
        QVERIFY(materializeTnfRowAction(move,
                QStringLiteral("[{\"id\":1,\"centreHz\":\"14074000\",\"widthHz\":200,\"active\":true}]"),
                revision, revision, 1, edits, session, session, epoch, epoch).isEmpty());
        QVERIFY(materializeTnfRowAction(move, list, revision, revision, rowId,
                QJsonObject{{"centreHz", QStringLiteral("14075000")}, {"widthHz", 300.0}},
                session, session, epoch, epoch).isEmpty());
        result = invoke(moveVerb, moveArgs);
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(core.model->notchModel()->notchById(rowId)->centerHz, 14075000.0);
        QCOMPARE(core.model->notchModel()->notchById(rowId)->widthHz, 300.0);
        const QJsonObject active = rowActions.at(1).toObject();
        const quint32 movedRevision = core.model->notchModel()->revision();
        result = invoke(activeVerb, materializeTnfRowAction(active,
            core.model->notchModel()->listJson(), movedRevision, movedRevision, rowId,
            QJsonObject{{"active", false}}, session, session, epoch, epoch));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QVERIFY(!core.model->notchModel()->notchById(rowId)->active);
        const QJsonObject remove = rowActions.at(2).toObject();
        const quint32 activeRevision = core.model->notchModel()->revision();
        result = invoke(deleteVerb, materializeTnfRowAction(remove,
            core.model->notchModel()->listJson(), activeRevision, activeRevision, rowId,
            {}, session, session, epoch, epoch));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QVERIFY(core.model->notchModel()->notchById(rowId) == nullptr);
        // The client cancels the old row; a stale direct command still gets
        // the Core's missing-row refusal.
        QVERIFY(materializeTnfRowAction(move, core.model->notchModel()->listJson(),
            core.model->notchModel()->revision(), revision, rowId, edits,
            session, session, epoch, epoch).isEmpty());
        result = invoke(moveVerb, {i64("id", rowId),
            MirrorUpdate{0, "centreHz", MirrorWireKind::Float64, 14076000.0},
            MirrorUpdate{0, "widthHz", MirrorWireKind::Float64, 200.0}});
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("That notch is no longer on this Core."));
        result = invoke(addVerb, {i64("sliceId", 999)});
        QVERIFY(!result.accepted);
        QVERIFY(core.model->notchModel()->notches().isEmpty());
    }

    void commandSourcesHaveOneTypedSource()
    {
        const QByteArray fixture = R"({
          "kind":"toggle", "gate":{"capability":"stationTciVersion","min":2},
          "binding":{"command":{"verb":"setStationTciOptions",
            "valueProperty":{"object":"stationTci","name":"emulateExpertSdr3"},
            "arguments":{
              "emulateExpertSdr3":{"$controlValue":true},
              "emulateSunSdr2Pro":{"$property":{"object":"stationTci","name":"emulateSunSdr2Pro"}},
              "cwluBecomesCw":{"$property":{"object":"stationTci","name":"cwluBecomesCw"}},
              "sendInitialState":{"$property":{"object":"stationTci","name":"sendInitialState"}}
            }}}})";
        QJsonObject control = QJsonDocument::fromJson(fixture).object();
        QString error;
        QVERIFY2(SetupDescriptionService::validateCommandBinding(control, &error),
                 qPrintable(error));
        const QJsonObject original = control;
        auto mutateArg = [&control, &original](const QJsonValue& replacement) {
            control = original;
            QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
            QJsonObject command = binding.value(QStringLiteral("command")).toObject();
            QJsonObject args = command.value(QStringLiteral("arguments")).toObject();
            args.insert(QStringLiteral("emulateSunSdr2Pro"), replacement);
            command.insert(QStringLiteral("arguments"), args);
            binding.insert(QStringLiteral("command"), command);
            control.insert(QStringLiteral("binding"), binding);
        };
        mutateArg(QJsonObject{{QStringLiteral("$controlValue"), true},
                              {QStringLiteral("$property"), QJsonObject{{"object", "stationTci"},
                                                                        {"name", "emulateSunSdr2Pro"}}}});
        QVERIFY(!SetupDescriptionService::validateCommandBinding(control, &error));
        mutateArg(QJsonObject{{QStringLiteral("$property"), QStringLiteral("stationTci")}});
        QVERIFY(!SetupDescriptionService::validateCommandBinding(control, &error));
        mutateArg(QJsonObject{{QStringLiteral("$property"), QJsonObject{{"object", "stationTci"},
                                                                        {"name", "port"}}}});
        QVERIFY(!SetupDescriptionService::validateCommandBinding(control, &error));
        control = original;
        QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
        gate.insert(QStringLiteral("min"), 1);
        control.insert(QStringLiteral("gate"), gate);
        QVERIFY(!SetupDescriptionService::validateCommandBinding(control, &error));
    }

    void shippedCommandBindingsPassStaticValidation()
    {
        SetupDescriptionService service;
        for (const QString& categoryId : {QStringLiteral("general"), QStringLiteral("test"),
                                          QStringLiteral("diagnostics"), QStringLiteral("catNetwork")}) {
            for (const QJsonValue& page : service.category(categoryId).value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                        const QJsonObject control = raw.toObject();
                        if (!control.value("binding").toObject().contains("command")) { continue; }
                        QString error;
                        QVERIFY2(SetupDescriptionService::validateCommandBinding(control, &error),
                                 qPrintable(control.value("id").toString() + ": " + error));
                    }
                }
            }
        }
    }

    void tciEditUsesOneLiveSessionEpochAndPreservesOtherOptions()
    {
        SetupDescriptionService service;
        const QJsonObject page = service.category(QStringLiteral("catNetwork"))
            .value("pages").toArray().first().toObject();
        const QJsonArray controls = page.value("sections").toArray().first()
            .toObject().value("controls").toArray();
        const QJsonObject current{{"emulateExpertSdr3", true}, {"emulateSunSdr2Pro", true},
                                  {"cwluBecomesCw", false}, {"sendInitialState", true}};
        for (const QJsonValue& raw : controls) {
            const QJsonObject control = raw.toObject();
            const QString changedName = control.value("binding").toObject()
                .value("command").toObject().value("valueProperty").toObject()
                .value("name").toString();
            const bool edited = !current.value(changedName).toBool();
            const QJsonObject args = resolveTciArguments(control, edited, current,
                QStringLiteral("phone-A"), QStringLiteral("phone-A"), 7, 7);
            QCOMPARE(args.size(), 4);
            for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
                QCOMPARE(args.value(it.key()).toBool(),
                         it.key() == changedName ? edited : it.value().toBool());
            }
            QVERIFY(resolveTciArguments(control, edited, current,
                QStringLiteral("phone-A"), QStringLiteral("phone-B"), 7, 7).isEmpty());
            QVERIFY(resolveTciArguments(control, edited, current,
                QStringLiteral("phone-A"), QStringLiteral("phone-A"), 7, 8).isEmpty());
            QJsonObject missing = current;
            missing.remove(QStringLiteral("emulateSunSdr2Pro"));
            if (changedName != QLatin1String("emulateSunSdr2Pro")) {
                QVERIFY(resolveTciArguments(control, edited, missing,
                    QStringLiteral("phone-A"), QStringLiteral("phone-A"), 7, 7).isEmpty());
            }
        }
    }
};

QTEST_MAIN(SetupDescriptionServiceTest)
#include "tst_setup_description_service.moc"
