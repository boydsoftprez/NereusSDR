// SPDX-License-Identifier: GPL-2.0-or-later
// NereusSDR-original tests for the local/remote station DSP asset contract.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/dsp/DspAssetService.h"

#include <QCryptographicHash>
#include <QScopeGuard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

extern "C" {
extern const unsigned char nnr_model_0_data[];
extern const unsigned int nnr_model_0_size;
}

using namespace NereusSDR;

namespace {

QByteArray standardModel()
{
    return QByteArray(reinterpret_cast<const char*>(nnr_model_0_data),
                      qsizetype(nnr_model_0_size));
}

QVariantMap beginArgs(const QByteArray& bytes, const QString& label = QStringLiteral("standard"),
                      int kind = 0)
{
    return {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("label"), label},
        {QStringLiteral("size"), qint64(bytes.size())},
        {QStringLiteral("hash"), QString::fromLatin1(
             QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
        {QStringLiteral("radioIdentity"), QString()}
    };
}

DspAssetServiceResult upload(DspAssetService& service, const QByteArray& bytes,
                             const QString& owner, int kind = 0,
                             const QString& label = QStringLiteral("standard"))
{
    const auto begun = service.execute("dspAssets.beginImport", beginArgs(bytes, label, kind),
                                       owner);
    if (!begun.accepted) return begun;
    const QString transferId = begun.values.value(QStringLiteral("transferId")).toString();
    for (qsizetype offset = 0; offset < bytes.size(); offset += DspAssetStore::kTransferChunkBytes) {
        const QByteArray chunk = bytes.mid(offset, DspAssetStore::kTransferChunkBytes);
        const auto appended = service.execute("dspAssets.chunk", {
            {QStringLiteral("transferId"), transferId},
            {QStringLiteral("offset"), qint64(offset)},
            {QStringLiteral("data"), QString::fromLatin1(chunk.toBase64())}
        }, owner);
        if (!appended.accepted) return appended;
    }
    return service.execute("dspAssets.finishImport",
                           {{QStringLiteral("transferId"), transferId}}, owner);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString bundledSmallPath() { return QFINDTESTDATA("../third_party/rnnoise/models/Default_small.bin"); }

QString largeId() { return QString::fromLatin1(DspAssetService::kNr3BundledLargeId); }
QString smallId() { return QString::fromLatin1(DspAssetService::kNr3BundledSmallId); }

constexpr int kNr3Kind = 2;

} // namespace

class TestDspAssetService final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAndQueuedLocalRequest();
    void customSelectionIsPendingUntilMarkedApplied();
    void savedMissingSelectionIsKeptWithBundledFallback();
    void commandsRequireExactKeysAndTypes();
    void transfersEnforceOwnerOffsetSizeHashAndCancellation();
    void exportReturnsBoundedCanonicalChunks();
    void remoteRequestsRetireAndNeverOpenLocalStore();
    void nr3DefaultIsTheBundledLargeFileNeverEmpty();
    void nr3ImportSelectAndLiveApplyThroughCommands();
    void nr3DamagedSelectionFallsBackToBundledFile();
    void nr3CommandsRequireExactShapes();
    void nr3LegacyPathImportsOnce();
    void nr3RemoteServiceMirrorsAndNeverLoads();
    void nr3CannotRunWithoutAnyModelFile();
    void dfnrAvailabilityIsTheCoresAndMirrored();
    void mnrAvailabilityIsTheCoresAndMirrored();
};

void TestDspAssetService::defaultsAndQueuedLocalRequest()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);

    QCOMPARE(service.nnrStandardAsset(), QStringLiteral("bundled:0"));
    QCOMPARE(service.nnrPremiumAsset(), QStringLiteral("bundled:1"));
    QVERIFY(!service.nnrModelSelectionPending());
    QVERIFY(service.selectionRevision() != 0);
    QVERIFY(service.store());
    const auto paths = service.resolveNnrModelPaths();
    QVERIFY(paths[0].isEmpty());
    QVERIFY(paths[1].isEmpty());

    QSignalSpy completed(&service, &DspAssetService::requestCompleted);
    const quint32 requestId = service.request("dspAssets.list", {});
    QVERIFY(requestId != 0);
    QCOMPARE(completed.count(), 0); // completion is queued so callers can register it
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed.first().at(0).toUInt(), requestId);
    QVERIFY(completed.first().at(1).toBool());
    const QVariantMap values = completed.first().at(3).toMap();
    QVERIFY(values.value(QStringLiteral("assets")).metaType().id() == QMetaType::QString);
    QVERIFY(!QJsonDocument::fromJson(values.value(QStringLiteral("assets")).toString().toUtf8())
                 .isNull());
}

void TestDspAssetService::customSelectionIsPendingUntilMarkedApplied()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    const auto imported = upload(service, standardModel(), QStringLiteral("local-ui"));
    QVERIFY2(imported.accepted, qPrintable(imported.reason));
    const QString id = imported.values.value(QStringLiteral("id")).toString();
    const quint32 before = service.selectionRevision();

    const auto selected = service.execute("dspAssets.selectNnrModel", {
        {QStringLiteral("slot"), 0}, {QStringLiteral("id"), id}
    }, QStringLiteral("local-ui"));
    QVERIFY2(selected.accepted, qPrintable(selected.reason));
    QCOMPARE(service.nnrStandardAsset(), id);
    QVERIFY(service.nnrModelSelectionPending());
    QVERIFY(service.selectionRevision() > before);
    QCOMPARE(settings.value(QStringLiteral("DspAssets/NnrModel0")).toString(), id);

    QString reason;
    const auto paths = service.resolveNnrModelPaths(&reason);
    QVERIFY2(!paths[0].isEmpty(), qPrintable(reason));
    QVERIFY(QFileInfo::exists(paths[0]));
    service.markNnrModelsApplied();
    QVERIFY(!service.nnrModelSelectionPending());
    QCOMPARE(service.activeNnrModelAssets()[0], id);
}

void TestDspAssetService::savedMissingSelectionIsKeptWithBundledFallback()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    const QString missing = QStringLiteral("sha256:") + QString(64, QLatin1Char('a'));
    settings.setValue(QStringLiteral("DspAssets/NnrModel0"), missing);
    settings.setValue(QStringLiteral("DspAssets/SelectionRevision"), QStringLiteral("19"));

    DspAssetService service(settings, true);
    QCOMPARE(service.nnrStandardAsset(), missing);
    QCOMPARE(service.selectionRevision(), quint32(19));
    QString reason;
    const auto paths = service.resolveNnrModelPaths(&reason);
    QVERIFY(paths[0].isEmpty());
    QVERIFY2(reason.contains(QStringLiteral("built-in one is used"), Qt::CaseInsensitive),
             qPrintable(reason));
    service.markNnrModelsApplied();
    QCOMPARE(service.activeNnrModelAssets()[0], QStringLiteral("bundled:0"));
    QVERIFY(service.nnrModelSelectionPending());
    QCOMPARE(settings.value(QStringLiteral("DspAssets/NnrModel0")).toString(), missing);
}

void TestDspAssetService::commandsRequireExactKeysAndTypes()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    const QByteArray bytes = standardModel();

    QVariantMap extra = beginArgs(bytes);
    extra.insert(QStringLiteral("path"), QStringLiteral("/tmp/model"));
    QVERIFY(!service.execute("dspAssets.beginImport", extra, QStringLiteral("owner")).accepted);

    QVariantMap wrongSize = beginArgs(bytes);
    wrongSize.insert(QStringLiteral("size"), QString::number(bytes.size()));
    QVERIFY(!service.execute("dspAssets.beginImport", wrongSize, QStringLiteral("owner")).accepted);
    QVERIFY(!service.execute("dspAssets.selectNnrModel", {
        {QStringLiteral("slot"), QStringLiteral("0")},
        {QStringLiteral("id"), QStringLiteral("bundled:0")}
    }, QStringLiteral("owner")).accepted);
    QVERIFY(!service.execute("dspAssets.list", {{QStringLiteral("unused"), 1}},
                             QStringLiteral("owner")).accepted);
    QVERIFY(!service.execute("dspAssets.unknown", {}, QStringLiteral("owner")).accepted);
    QVERIFY(!service.execute("dspAssets.list", {}, {}).accepted);
}

void TestDspAssetService::transfersEnforceOwnerOffsetSizeHashAndCancellation()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    const QByteArray bytes = standardModel();

    auto begun = service.execute("dspAssets.beginImport", beginArgs(bytes), QStringLiteral("alice"));
    QVERIFY(begun.accepted);
    const QString token = begun.values.value(QStringLiteral("transferId")).toString();
    const QString data = QString::fromLatin1(bytes.left(32).toBase64());
    QVERIFY(!service.execute("dspAssets.chunk", {
        {QStringLiteral("transferId"), token}, {QStringLiteral("offset"), qint64(0)},
        {QStringLiteral("data"), data}
    }, QStringLiteral("bob")).accepted);
    QVERIFY(!service.execute("dspAssets.chunk", {
        {QStringLiteral("transferId"), token}, {QStringLiteral("offset"), qint64(1)},
        {QStringLiteral("data"), data}
    }, QStringLiteral("alice")).accepted);
    QVERIFY(!service.execute("dspAssets.chunk", {
        {QStringLiteral("transferId"), token}, {QStringLiteral("offset"), qint64(0)},
        {QStringLiteral("data"), QStringLiteral("not base64")}
    }, QStringLiteral("alice")).accepted);
    QVERIFY(service.execute("dspAssets.chunk", {
        {QStringLiteral("transferId"), token}, {QStringLiteral("offset"), qint64(0)},
        {QStringLiteral("data"), data}
    }, QStringLiteral("alice")).accepted);
    QVERIFY(!service.execute("dspAssets.finishImport",
        {{QStringLiteral("transferId"), token}}, QStringLiteral("alice")).accepted);
    QVERIFY(!service.execute("dspAssets.cancelImport",
        {{QStringLiteral("transferId"), token}}, QStringLiteral("alice")).accepted);

    QVariantMap wrongHash = beginArgs(bytes);
    wrongHash.insert(QStringLiteral("hash"), QString(64, QLatin1Char('0')));
    begun = service.execute("dspAssets.beginImport", wrongHash, QStringLiteral("alice"));
    QVERIFY(begun.accepted);
    const QString wrongToken = begun.values.value(QStringLiteral("transferId")).toString();
    for (qsizetype offset = 0; offset < bytes.size(); offset += DspAssetStore::kTransferChunkBytes) {
        const QByteArray chunk = bytes.mid(offset, DspAssetStore::kTransferChunkBytes);
        QVERIFY(service.execute("dspAssets.chunk", {
            {QStringLiteral("transferId"), wrongToken}, {QStringLiteral("offset"), qint64(offset)},
            {QStringLiteral("data"), QString::fromLatin1(chunk.toBase64())}
        }, QStringLiteral("alice")).accepted);
    }
    QVERIFY(!service.execute("dspAssets.finishImport",
        {{QStringLiteral("transferId"), wrongToken}}, QStringLiteral("alice")).accepted);

    const auto first = service.execute("dspAssets.beginImport",
                                       beginArgs(bytes, QStringLiteral("one")),
                                       QStringLiteral("alice"));
    const auto second = service.execute("dspAssets.beginImport",
                                        beginArgs(bytes, QStringLiteral("two")),
                                        QStringLiteral("alice"));
    QVERIFY(first.accepted);
    QVERIFY(second.accepted);
    QVERIFY(!service.execute("dspAssets.beginImport",
                             beginArgs(bytes, QStringLiteral("three")),
                             QStringLiteral("alice")).accepted);
    service.cancelOwner(QStringLiteral("alice"));
    QVERIFY(!service.execute("dspAssets.cancelImport",
        {{QStringLiteral("transferId"),
          first.values.value(QStringLiteral("transferId")).toString()}},
        QStringLiteral("alice")).accepted);
}

void TestDspAssetService::exportReturnsBoundedCanonicalChunks()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    const QByteArray bytes = standardModel();
    const auto imported = upload(service, bytes, QStringLiteral("alice"));
    QVERIFY2(imported.accepted, qPrintable(imported.reason));
    const QString id = imported.values.value(QStringLiteral("id")).toString();

    QByteArray exported;
    qint64 offset = 0;
    bool eof = false;
    while (!eof) {
        const auto result = service.execute("dspAssets.export", {
            {QStringLiteral("id"), id}, {QStringLiteral("offset"), offset}
        }, QStringLiteral("alice"));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        const QByteArray encoded = result.values.value(QStringLiteral("data")).toString().toLatin1();
        const QByteArray decoded = QByteArray::fromBase64(encoded);
        QVERIFY(decoded.size() <= DspAssetStore::kTransferChunkBytes);
        QCOMPARE(decoded.toBase64(), encoded);
        exported.append(decoded);
        offset += decoded.size();
        eof = result.values.value(QStringLiteral("eof")).toBool();
    }
    QCOMPARE(exported, bytes);
    QVERIFY(!service.execute("dspAssets.export", {
        {QStringLiteral("id"), id}, {QStringLiteral("offset"), qint64(bytes.size() + 1)}
    }, QStringLiteral("alice")).accepted);
}

void TestDspAssetService::remoteRequestsRetireAndNeverOpenLocalStore()
{
    QTemporaryDir directory;
    const QString settingsPath = directory.filePath(QStringLiteral("client/settings.xml"));
    AppSettings settings(settingsPath);
    DspAssetService service(settings, false);
    QVERIFY(!service.store());
    quint32 wireId = 41;
    service.setRemoteRequestHandler([&](const QByteArray&, const QVariantMap&) {
        return ++wireId;
    });
    QSignalSpy completed(&service, &DspAssetService::requestCompleted);

    const quint32 id = service.request("dspAssets.list", {});
    QCOMPARE(id, quint32(42));
    QVERIFY(!service.receiveRemoteResult(id + 1, "dspAssets.list", true, {}, {}));
    QVERIFY(!service.receiveRemoteResult(id, "dspAssets.export", true, {}, {}));
    QVERIFY(service.receiveRemoteResult(id, "dspAssets.list", true, {},
                                        {{QStringLiteral("assets"), QStringLiteral("[]")}}));
    QCOMPARE(completed.count(), 1);
    QVERIFY(!service.receiveRemoteResult(id, "dspAssets.list", true, {}, {}));

    const quint32 retired = service.request("dspAssets.list", {});
    service.resetSession();
    QVERIFY(!service.receiveRemoteResult(retired, "dspAssets.list", true, {}, {}));
    QVERIFY(service.applyRemoteProperty("nnrStandardAsset", QStringLiteral("sha256:remote")));
    QVERIFY(service.applyRemoteProperty("nnrModelSelectionPending", true));
    QCOMPARE(service.nnrStandardAsset(), QStringLiteral("sha256:remote"));
    QVERIFY(service.nnrModelSelectionPending());
    QVERIFY(!service.applyRemoteProperty("selectionRevision", QStringLiteral("7")));
    QVERIFY(!QFileInfo::exists(QFileInfo(settingsPath).absolutePath()
                               + QStringLiteral("/dsp-assets")));
    QVERIFY(!settings.contains(QStringLiteral("DspAssets/NnrModel0")));
}

void TestDspAssetService::nr3DefaultIsTheBundledLargeFileNeverEmpty()
{
    // Red first (R-R3-21). Before this change the Setup "Default" button
    // stored Nr3ModelPath = "" and the Core read it at connect with
    // value("Nr3ModelPath", <bundled path>). A stored empty string wins
    // over the fallback, so the Core skipped RNNRloadModel and NR3 stayed
    // on rnnr.c's NULL model (built-in weights are compiled out,
    // third_party/rnnoise/CMakeLists.txt). This reproduces that read:
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    settings.setValue(QStringLiteral("Nr3ModelPath"), QString());
    const QString legacyRead = settings.value(QStringLiteral("Nr3ModelPath"),
                                              QStringLiteral("/bundled/Default_large.bin")).toString();
    QVERIFY(legacyRead.isEmpty()); // today's "Default": nothing loaded

    // The Core now resolves "Default" to the bundled file itself.
    DspAssetService service(settings, true);
    QCOMPARE(service.nr3ModelAsset(), largeId());
    QVERIFY(service.nr3ModelsSupported());
    QStringList loaded;
    service.setNr3ModelLoader([&](const QString& path) { loaded.append(path); });
    QString resolvedId;
    const QString path = service.resolveNr3ModelPath(&resolvedId);
    QVERIFY(!path.isEmpty());
    QCOMPARE(resolvedId, largeId());
    QVERIFY(path.endsWith(QStringLiteral("Default_large.bin")));
    QCOMPARE(path, DspAssetService::bundledNr3ModelPath(largeId()));
    QVERIFY(service.applyNr3Model());
    QCOMPARE(loaded, QStringList{path});
    QCOMPARE(service.activeNr3ModelAsset(), largeId());
    QCOMPARE(service.nr3ModelStatus(), QStringLiteral("Using the bundled large model."));
    // Nr3ModelPath "" was not an import request.
    QVERIFY(service.store()->assets().isEmpty());
}

void TestDspAssetService::nr3ImportSelectAndLiveApplyThroughCommands()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    QStringList loaded;
    service.setNr3ModelLoader([&](const QString& path) { loaded.append(path); });
    QSignalSpy changed(&service, &DspAssetService::nr3SelectionChanged);

    const QByteArray bytes = readFile(bundledSmallPath());
    QVERIFY(!bytes.isEmpty());
    const auto imported = upload(service, bytes, QStringLiteral("alice"), kNr3Kind,
                                 QStringLiteral("Quiet band"));
    QVERIFY2(imported.accepted, qPrintable(imported.reason));
    QCOMPARE(imported.values.value(QStringLiteral("kind")).toInt(), kNr3Kind);
    const QString id = imported.values.value(QStringLiteral("id")).toString();
    QVERIFY(loaded.isEmpty()); // importing never changes the model

    const auto listed = service.execute("dspAssets.list", {}, QStringLiteral("alice"));
    QVERIFY(listed.accepted);
    const QJsonArray rows = QJsonDocument::fromJson(
        listed.values.value(QStringLiteral("assets")).toString().toUtf8()).array();
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().toObject().value(QStringLiteral("kind")).toInt(), kNr3Kind);
    QVERIFY(rows.first().toObject().value(QStringLiteral("valid")).toBool());

    const auto selected = service.execute("dspAssets.selectNr3Model",
                                          {{QStringLiteral("id"), id}}, QStringLiteral("alice"));
    QVERIFY2(selected.accepted, qPrintable(selected.reason));
    QCOMPARE(service.nr3ModelAsset(), id);
    QCOMPARE(service.activeNr3ModelAsset(), id);
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(readFile(loaded.first()), bytes);
    QCOMPARE(settings.value(QStringLiteral("DspAssets/Nr3Model")).toString(), id);
    QCOMPARE(service.nr3ModelStatus(), QStringLiteral("Using the NR3 model \"Quiet band\"."));
    QVERIFY(changed.count() > 0);

    // Selecting the same model again loads nothing.
    QVERIFY(service.execute("dspAssets.selectNr3Model", {{QStringLiteral("id"), id}},
                            QStringLiteral("alice")).accepted);
    QCOMPARE(loaded.size(), 1);

    // The bundled small model is always selectable by id.
    QVERIFY(service.execute("dspAssets.selectNr3Model", {{QStringLiteral("id"), smallId()}},
                            QStringLiteral("alice")).accepted);
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.last(), DspAssetService::bundledNr3ModelPath(smallId()));
    QCOMPARE(service.nr3ModelStatus(), QStringLiteral("Using the bundled small model."));

    // An unknown id, or an NNR model's id, is refused and changes nothing.
    const auto nnr = upload(service, standardModel(), QStringLiteral("alice"));
    QVERIFY2(nnr.accepted, qPrintable(nnr.reason));
    for (const QString& bad : {nnr.values.value(QStringLiteral("id")).toString(),
                               QStringLiteral("sha256:") + QString(64, QLatin1Char('b')),
                               QStringLiteral("bundled:0")}) {
        const auto refused = service.execute("dspAssets.selectNr3Model",
                                             {{QStringLiteral("id"), bad}},
                                             QStringLiteral("alice"));
        QVERIFY(!refused.accepted);
        QCOMPARE(refused.reason, QStringLiteral("That NR3 model is missing or damaged on this Core."));
    }
    QCOMPARE(service.nr3ModelAsset(), smallId());
    QCOMPARE(loaded.size(), 2);

    // A restart keeps the choice.
    DspAssetService restarted(settings, true);
    QCOMPARE(restarted.nr3ModelAsset(), smallId());
}

void TestDspAssetService::nr3DamagedSelectionFallsBackToBundledFile()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    QStringList loaded;
    service.setNr3ModelLoader([&](const QString& path) { loaded.append(path); });
    const auto imported = upload(service, readFile(bundledSmallPath()), QStringLiteral("alice"),
                                 kNr3Kind, QStringLiteral("Custom"));
    QVERIFY2(imported.accepted, qPrintable(imported.reason));
    const QString id = imported.values.value(QStringLiteral("id")).toString();
    QVERIFY(service.execute("dspAssets.selectNr3Model", {{QStringLiteral("id"), id}},
                            QStringLiteral("alice")).accepted);
    QCOMPARE(loaded.size(), 1);

    // Damage the stored file behind the store's back.
    QFile stored(loaded.first());
    QVERIFY(stored.open(QIODevice::ReadWrite));
    QVERIFY(stored.seek(100));
    QCOMPARE(stored.write("XXXX", 4), qint64(4));
    stored.close();

    QString reason;
    QVERIFY(service.applyNr3Model(&reason));
    QCOMPARE(loaded.size(), 2);
    QVERIFY(!loaded.last().isEmpty());
    QCOMPARE(loaded.last(), DspAssetService::bundledNr3ModelPath(largeId()));
    QCOMPARE(service.nr3ModelAsset(), id); // the choice is kept for repair
    QCOMPARE(service.activeNr3ModelAsset(), largeId());
    QCOMPARE(service.nr3ModelStatus(),
             QStringLiteral("The chosen NR3 model is missing or damaged. Using the bundled large model."));
    for (const QString& path : std::as_const(loaded)) {
        QVERIFY(!path.isEmpty());
    }
}

void TestDspAssetService::nr3CommandsRequireExactShapes()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    const QByteArray bytes = readFile(bundledSmallPath());

    QVERIFY(!service.execute("dspAssets.selectNr3Model", {}, QStringLiteral("o")).accepted);
    QVERIFY(!service.execute("dspAssets.selectNr3Model",
        {{QStringLiteral("id"), 2}}, QStringLiteral("o")).accepted);
    QVERIFY(!service.execute("dspAssets.selectNr3Model",
        {{QStringLiteral("id"), largeId()}, {QStringLiteral("slot"), 0}}, QStringLiteral("o")).accepted);
    QVERIFY(!service.execute("dspAssets.beginImport", beginArgs(bytes, QStringLiteral("x"), 3),
                             QStringLiteral("o")).accepted);
    QVariantMap scoped = beginArgs(bytes, QStringLiteral("x"), kNr3Kind);
    scoped.insert(QStringLiteral("radioIdentity"), QStringLiteral("AA:BB:CC:DD:EE:FF"));
    const auto scopedResult = service.execute("dspAssets.beginImport", scoped, QStringLiteral("o"));
    QVERIFY(!scopedResult.accepted);
    QCOMPARE(scopedResult.reason, QStringLiteral("NR3 models belong to the Core, not to one radio."));
    QVariantMap oversized = beginArgs(bytes, QStringLiteral("x"), kNr3Kind);
    oversized.insert(QStringLiteral("size"), DspAssetValidation::kMaxNr3ModelBytes + 1);
    QVERIFY(!service.execute("dspAssets.beginImport", oversized, QStringLiteral("o")).accepted);

    // Junk with the right size and hash reaches the trial load and is
    // refused with a plain reason; nothing is stored.
    QByteArray junk(8192, '\x11');
    junk.replace(0, 4, "DNNw");
    const auto refused = upload(service, junk, QStringLiteral("o"), kNr3Kind);
    QVERIFY(!refused.accepted);
    QCOMPARE(refused.reason, QStringLiteral("This file is not an NR3 model this Core can use."));
    QVERIFY(service.store()->assets().isEmpty());
}

void TestDspAssetService::nr3LegacyPathImportsOnce()
{
    QTemporaryDir directory;
    const QByteArray bytes = readFile(bundledSmallPath());
    const QString legacy = directory.filePath(QStringLiteral("My Model.bin"));
    {
        QFile file(legacy);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), qint64(bytes.size()));
    }
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    settings.setValue(QStringLiteral("Nr3ModelPath"), legacy);

    QString importedId;
    {
        QTest::ignoreMessage(QtInfoMsg,
            QRegularExpression(QStringLiteral("^NR3: imported the older NR3 model file as")));
        DspAssetService service(settings, true);
        importedId = service.nr3ModelAsset();
        QVERIFY(importedId.startsWith(QStringLiteral("sha256:")));
        const auto records = service.store()->assets();
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.first().kind, DspAssetKind::Nr3Model);
        QCOMPARE(records.first().label, QStringLiteral("My Model"));
        QCOMPARE(settings.value(QStringLiteral("DspAssets/Nr3ModelPathImported")).toString(),
                 QStringLiteral("True"));
        QStringList loaded;
        service.setNr3ModelLoader([&](const QString& path) { loaded.append(path); });
        QVERIFY(service.applyNr3Model());
        QCOMPARE(readFile(loaded.value(0)), bytes);
    }

    // Once only: a later path, even a valid one, is not imported again, and
    // the operator's choice since then is kept.
    settings.setValue(QStringLiteral("Nr3ModelPath"),
                      DspAssetService::bundledNr3ModelPath(largeId()));
    settings.setValue(QStringLiteral("DspAssets/Nr3Model"), smallId());
    DspAssetService again(settings, true);
    QCOMPARE(again.store()->assets().size(), 1);
    QCOMPARE(again.nr3ModelAsset(), smallId());

    // A damaged older file is tried once and leaves the bundled default.
    QTemporaryDir other;
    const QString damaged = other.filePath(QStringLiteral("broken.bin"));
    {
        QFile file(damaged);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("DNNw not really a model");
    }
    AppSettings fresh(other.filePath(QStringLiteral("station.settings")));
    fresh.setValue(QStringLiteral("Nr3ModelPath"), damaged);
    QTest::ignoreMessage(QtWarningMsg,
        "NR3: the older NR3 model file was not imported: \"This file is not an NR3 model.\"");
    DspAssetService withDamaged(fresh, true);
    QCOMPARE(withDamaged.nr3ModelAsset(), largeId());
    QVERIFY(withDamaged.store()->assets().isEmpty());
    QCOMPARE(fresh.value(QStringLiteral("DspAssets/Nr3ModelPathImported")).toString(),
             QStringLiteral("True"));
}

void TestDspAssetService::nr3RemoteServiceMirrorsAndNeverLoads()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("client.settings")));
    settings.setValue(QStringLiteral("Nr3ModelPath"), bundledSmallPath());
    DspAssetService service(settings, false);
    QVERIFY(!service.nr3ModelsSupported());
    QCOMPARE(service.nr3ModelAsset(), largeId());
    int loads = 0;
    service.setNr3ModelLoader([&](const QString&) { ++loads; });
    QVERIFY(!service.applyNr3Model());
    QCOMPARE(loads, 0);
    QVERIFY(!settings.contains(QStringLiteral("DspAssets/Nr3ModelPathImported")));

    QSignalSpy changed(&service, &DspAssetService::nr3SelectionChanged);
    service.setRemoteNr3ModelsSupported(true);
    QVERIFY(service.nr3ModelsSupported());
    QCOMPARE(changed.count(), 1);
    QVERIFY(service.applyRemoteProperty("nr3ModelAsset", smallId()));
    QVERIFY(service.applyRemoteProperty("nr3ModelStatus", QStringLiteral("Using the bundled small model.")));
    QVERIFY(!service.applyRemoteProperty("nr3ModelAsset", 7));
    QCOMPARE(service.nr3ModelAsset(), smallId());
    QCOMPARE(service.nr3ModelStatus(), QStringLiteral("Using the bundled small model."));
    QCOMPARE(changed.count(), 3);
    QVERIFY(!service.execute("dspAssets.selectNr3Model", {{QStringLiteral("id"), largeId()}},
                             QStringLiteral("o")).accepted);
}

// Fix wave I3: with no usable NR3 model file at all, the Core says NR3
// cannot run (nr3Runnable false, with the plain status) and loads nothing;
// once a file is usable again it can. A window mirrors the flag, and reads
// true until a Core says otherwise, so an older Core changes nothing.
void TestDspAssetService::nr3CannotRunWithoutAnyModelFile()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService::setBundledNr3ModelPathsForTest(
        [](const QString&) { return QString(); });
    const auto restore = qScopeGuard([] { DspAssetService::setBundledNr3ModelPathsForTest({}); });
    DspAssetService service(settings, true);
    QVERIFY(!service.nr3Runnable());
    const QString none = QStringLiteral("No NR3 model file was found on this Core, so NR3 cannot run.");
    QCOMPARE(service.nr3ModelStatus(), none);
    int loads = 0;
    service.setNr3ModelLoader([&](const QString&) { ++loads; });
    QString reason;
    QVERIFY(!service.applyNr3Model(&reason));
    QCOMPARE(reason, none);
    QCOMPARE(loads, 0);
    QVERIFY(!service.nr3Runnable());

    QSignalSpy changed(&service, &DspAssetService::nr3SelectionChanged);
    DspAssetService::setBundledNr3ModelPathsForTest({});
    QVERIFY(service.applyNr3Model(&reason));
    QVERIFY(service.nr3Runnable());
    QCOMPARE(loads, 1);
    QVERIFY(changed.count() >= 1);
    QCOMPARE(service.nr3ModelStatus(), QStringLiteral("Using the bundled large model."));

    DspAssetService remote(settings, false);
    QVERIFY(remote.nr3Runnable());
    QSignalSpy remoteChanged(&remote, &DspAssetService::nr3SelectionChanged);
    QVERIFY(remote.applyRemoteProperty("nr3Runnable", false));
    QVERIFY(!remote.nr3Runnable());
    QCOMPARE(remoteChanged.count(), 1);
    QVERIFY(!remote.applyRemoteProperty("nr3Runnable", QStringLiteral("false")));
    QVERIFY(!remote.applyRemoteProperty("nr3Runnable", 0));
    QVERIFY(!remote.nr3Runnable());
    QVERIFY(remote.applyRemoteProperty("nr3Runnable", true));
    QVERIFY(remote.nr3Runnable());
    // A remote service keeps its own value when a local call tries.
    QVERIFY(!service.applyRemoteProperty("nr3Runnable", false));
    QVERIFY(service.nr3Runnable());
}

// R-R3-49, Sub-epic C-1: whether the Core can run DFNR, and why not. The
// Core sets it; a window takes the Core's (true and no reason until a Core
// says otherwise, so an older Core changes nothing), and a new session
// starts from those defaults again.
void TestDspAssetService::dfnrAvailabilityIsTheCoresAndMirrored()
{
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    QVERIFY(service.dfnrRunnable());
    QVERIFY(service.dfnrModelStatus().isEmpty());
    QSignalSpy changed(&service, &DspAssetService::dfnrAvailabilityChanged);
    const QString missing =
        QStringLiteral("No DFNR model file was found on this Core, so DFNR cannot run.");
    service.setDfnrAvailability(false, missing);
    QVERIFY(!service.dfnrRunnable());
    QCOMPARE(service.dfnrModelStatus(), missing);
    QCOMPARE(changed.count(), 1);
    service.setDfnrAvailability(false, missing);   // no change, no signal
    QCOMPARE(changed.count(), 1);
    service.setDfnrAvailability(true, missing);    // runnable carries no reason
    QVERIFY(service.dfnrRunnable());
    QVERIFY(service.dfnrModelStatus().isEmpty());
    // A local service takes no remote value.
    QVERIFY(!service.applyRemoteProperty("dfnrRunnable", false));
    QVERIFY(service.dfnrRunnable());

    DspAssetService remote(settings, false);
    QVERIFY(remote.dfnrRunnable());
    QSignalSpy remoteChanged(&remote, &DspAssetService::dfnrAvailabilityChanged);
    remote.setDfnrAvailability(false, missing);    // only the Core sets it
    QVERIFY(remote.dfnrRunnable());
    QVERIFY(remote.applyRemoteProperty("dfnrModelStatus", missing));
    QVERIFY(remote.applyRemoteProperty("dfnrRunnable", false));
    QVERIFY(!remote.dfnrRunnable());
    QCOMPARE(remote.dfnrModelStatus(), missing);
    QCOMPARE(remoteChanged.count(), 2);
    QVERIFY(!remote.applyRemoteProperty("dfnrRunnable", QStringLiteral("false")));
    QVERIFY(!remote.applyRemoteProperty("dfnrModelStatus", 3));
    remote.resetSession();
    QVERIFY(remote.dfnrRunnable());
    QVERIFY(remote.dfnrModelStatus().isEmpty());
    QCOMPARE(remoteChanged.count(), 3);
}

void TestDspAssetService::mnrAvailabilityIsTheCoresAndMirrored()
{
    // R-R3-49, Sub-epic C-1 (dspAssetVersion 4): MNR's pair, as DFNR's.
    QTemporaryDir directory;
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    DspAssetService service(settings, true);
    QVERIFY(service.mnrRunnable());
    QVERIFY(service.mnrStatus().isEmpty());
    QSignalSpy changed(&service, &DspAssetService::mnrAvailabilityChanged);
    const QString notMac =
        QStringLiteral("MNR runs only on a Mac, and this Core is not a Mac, so MNR cannot run.");
    service.setMnrAvailability(false, notMac);
    QVERIFY(!service.mnrRunnable());
    QCOMPARE(service.mnrStatus(), notMac);
    QCOMPARE(changed.count(), 1);
    service.setMnrAvailability(false, notMac);   // no change, no signal
    QCOMPARE(changed.count(), 1);
    service.setMnrAvailability(true, notMac);    // runnable carries no reason
    QVERIFY(service.mnrRunnable());
    QVERIFY(service.mnrStatus().isEmpty());
    QVERIFY(!service.applyRemoteProperty("mnrRunnable", false));
    QVERIFY(service.mnrRunnable());

    DspAssetService remote(settings, false);
    QVERIFY(remote.mnrRunnable());
    QSignalSpy remoteChanged(&remote, &DspAssetService::mnrAvailabilityChanged);
    remote.setMnrAvailability(false, notMac);    // only the Core sets it
    QVERIFY(remote.mnrRunnable());
    QVERIFY(remote.applyRemoteProperty("mnrStatus", notMac));
    QVERIFY(remote.applyRemoteProperty("mnrRunnable", false));
    QVERIFY(!remote.mnrRunnable());
    QCOMPARE(remote.mnrStatus(), notMac);
    QCOMPARE(remoteChanged.count(), 2);
    QVERIFY(!remote.applyRemoteProperty("mnrRunnable", QStringLiteral("false")));
    QVERIFY(!remote.applyRemoteProperty("mnrStatus", 3));
    remote.resetSession();
    QVERIFY(remote.mnrRunnable());
    QVERIFY(remote.mnrStatus().isEmpty());
    QCOMPARE(remoteChanged.count(), 3);
}

QTEST_GUILESS_MAIN(TestDspAssetService)
#include "tst_dsp_asset_service.moc"
