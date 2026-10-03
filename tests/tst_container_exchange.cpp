// no-port-check: NereusSDR-original lossless portable exchange regressions.
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include "core/AppSettings.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/LegacyContainerImporter.h"
using namespace NereusSDR;
class TstContainerExchange:public QObject {
 Q_OBJECT
private slots:
 void envelopesPreserveConfigurationAndRemapIdentities() {
    ContainerContentRegistry registry; ContainerDocument c;c.id="original";c.name="Customized";
    auto first=registry.makeEntry("meter.mic"),second=registry.makeEntry("future.face");
    first.context={{"sessionId","foreign"},{"sliceId",2},{"mmioGuid","00000000-0000-0000-0000-000000000001"},{"mmioVariable","alpha"},{"unknown",QJsonObject{{"nested",17}}}};
    first.config["unknown"]=QJsonObject{{"preserve",true}};first.extensions["portableRecovery"]="user supplied";
    second.config["raw"]="exact bytes"; second.returnLocation=ReturnLocation{c.id,first.id,"external",{{"future",true}}};c.contents={first,second};
    auto bytes=ContainerDocumentCodec::exportContainer(c);auto root=QJsonDocument::fromJson(bytes).object();root["futureEnvelope"]=QJsonObject{{"x",9}};bytes=QJsonDocument(root).toJson();
    auto result=ContainerDocumentCodec::importContainer(bytes);QVERIFY2(result.ok,qPrintable(result.error));auto imported=result.document.containers.first();
    QVERIFY(imported.id!=c.id);QVERIFY(imported.contents[0].id!=first.id);QCOMPARE(imported.contents[0].context,first.context);QCOMPARE(imported.contents[0].config,first.config);
    QCOMPARE(imported.contents[0].extensions["portableRecovery"].toString(),QString("user supplied"));
    QCOMPARE(imported.contents[1].returnLocation->containerId,imported.id);QCOMPARE(imported.contents[1].returnLocation->beforeId,imported.contents[0].id);QCOMPARE(imported.contents[1].returnLocation->afterId,QString("external"));
    const auto backup=imported.contents[0].extensions["nereusPortableRecovery"].toObject()["records"].toArray().first().toObject();QCOMPARE(QByteArray::fromBase64(backup["sourceBase64"].toString().toLatin1()),bytes);QCOMPARE(backup["envelopeExtensions"].toObject()["futureEnvelope"],root["futureEnvelope"]);
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);QCOMPARE(store.commit(result.document,0).status,CommitStatus::Saved);
    ContainerWorkspaceStore reload(settings);QCOMPARE(reload.load().document.containers.first().contents[0].extensions["nereusPortableRecovery"].toObject()["records"].toArray().first(),QJsonValue(backup));
    const auto once=ContainerDocumentCodec::exportContainer(imported);const auto again=ContainerDocumentCodec::importContainer(once);QVERIFY(again.ok);
    const auto twice=ContainerDocumentCodec::exportContainer(again.document.containers.first());QVERIFY2(twice.size()<once.size()+300,"Portable cycles recursively multiplied backup");
    QCOMPARE(again.document.containers[0].contents[0].extensions["nereusPortableRecovery"].toObject()["records"].toArray().first(),QJsonValue(backup));
    auto copied=ContainerDocumentCodec::importEntries(ContainerDocumentCodec::exportEntries(imported.contents));QVERIFY(copied.ok);QCOMPARE(copied.document.containers[0].contents[0].context,first.context);
 }
 void recoverySlotsDoNotOverwriteForeignExtensions() {
    ContainerContentRegistry registry;ContainerDocument c;c.id="A";auto e=registry.makeEntry("meter.mic");
    e.extensions={{"nereusPortableOriginal",QJsonObject{{"foreign",true}}},{"nereusPortableEnvelopeExtensions","old user envelope"},{"nereusPortableRecovery","user slot"},{"nereusPortableRecovery1",QJsonObject{{"format","nereus.portable-recovery"},{"schemaVersion",1},{"records","malformed foreign record"}}}};c.contents={e};c.extensions=e.extensions;
    const auto bytes=ContainerDocumentCodec::exportContainer(c);const auto imported=ContainerDocumentCodec::importContainer(bytes);QVERIFY(imported.ok);const auto extensions=imported.document.containers[0].contents[0].extensions;
    for(auto i=e.extensions.begin();i!=e.extensions.end();++i) {QCOMPARE(extensions[i.key()],i.value());}
    const auto original=extensions["nereusPortableRecovery2"].toObject()["records"].toArray()[0].toObject();QCOMPARE(QByteArray::fromBase64(original["sourceBase64"].toString().toLatin1()),bytes);
    const auto again=ContainerDocumentCodec::importContainer(ContainerDocumentCodec::exportContainer(imported.document.containers[0]));QVERIFY(again.ok);QCOMPARE(again.document.containers[0].contents[0].extensions,extensions);
 }
 void compatibleLegacyAndStrictInvalidStructured() {
    const QByteArray raw="TEXT|0|0|1|1|0|0|hello";
    auto a=ContainerDocumentCodec::importContainer(raw);QVERIFY(a.ok);auto b=ContainerDocumentCodec::importEntries(QString::fromLatin1(raw.toBase64()));QVERIFY(b.ok);auto c=ContainerDocumentCodec::importEntries(QString::fromUtf8(raw));QVERIFY(c.ok);
    QCOMPARE(a.document.containers[0].contents[0].config["legacyRecord"].toString(),QString::fromUtf8(raw));
    for(const QByteArray& malformed:{QByteArray("{\"format\":\"nereus.container\",\"schemaVersion\":2,\"workspace\":{}}"),QByteArray("{\"schemaVersion\":99}"),QByteArray("{broken"),QByteArray("[]")}) {QVERIFY(!ContainerDocumentCodec::importContainer(malformed).ok);}
    auto bytes=ContainerDocumentCodec::exportContainer(a.document.containers.first());QVERIFY(LegacyContainerImporter::fromContainerFile(bytes).ok);
    auto entries=ContainerDocumentCodec::exportEntries(a.document.containers.first().contents);QVERIFY(LegacyContainerImporter::fromClipboard(entries).ok);QVERIFY(ContainerDocumentCodec::importEntries(QString::fromLatin1(entries.toUtf8().toBase64())).ok);
 }
};
QTEST_MAIN(TstContainerExchange)
#include "tst_container_exchange.moc"
