// no-port-check: NereusSDR-original presentation document contract tests.
#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <limits>
#include "gui/containers/ContainerDocumentCodec.h"

using namespace NereusSDR;

class TstContainerDocument : public QObject {
    Q_OBJECT
private:
    static WorkspaceDocument sample()
    {
        WorkspaceDocument doc;
        doc.revision = std::numeric_limits<quint64>::max();
        doc.mainContainerId = QStringLiteral("main");
        doc.extensions = {{"futureWorkspace", QJsonArray{1, "opaque", QJsonObject{{"flag", true}}}}};
        ContainerDocument container;
        container.id = doc.mainContainerId;
        container.name = QStringLiteral("RX | A\n日本語");
        container.layout = ContentLayout::VerticalStack;
        container.dockMode = DockMode::Floating;
        container.anchor = AxisLock::BottomLeft;
        container.header = HeaderMode::Hidden;
        container.geometry = QRect(-400, 220, 730, 540);
        container.visible = false;
        container.locked = true;
        container.autoHeight = true;
        container.popOutShell = true;
        container.config = {{"rxPolicy", "both"}, {"splitter", "opaque bytes"}};
        container.extensions = {{"futureContainer", QJsonObject{{"newMode", 4}}}};
        ContentEntry entry;
        entry.id = QStringLiteral("meter-1");
        entry.typeId = QStringLiteral("unknown.future.meter");
        entry.name = QStringLiteral("Mic | complete\nface");
        entry.context = {{"rxSource", 3}, {"sliceId", "slice-C"}, {"sessionId", "session-one"},
                         {"mmio", QJsonObject{{"endpoint", "amp"}, {"expression", "a|b"}}}};
        entry.config = {{"legacyRecord", "line|with|tails"}, {"futureStyle", QJsonArray{true, 2, "blue"}}};
        entry.extensions = {{"futureContent", QJsonObject{{"value", QJsonValue::Null}}}};
        entry.canvasRect = QRectF(-3.25, 2.5, 0.665, 0.99);
        entry.paintOrder = -3;
        entry.visible = false;
        entry.returnLocation = ReturnLocation{QStringLiteral("deleted-destination"),
                                              QStringLiteral("before"), QStringLiteral("after"),
                                              {{"futurePlacement", QJsonArray{1, "x"}}}};
        container.contents.append(entry);
        entry.id = QStringLiteral("meter-2");
        entry.name = container.contents.first().name; // Names need not be unique; IDs do.
        entry.paintOrder = 7;
        entry.returnLocation.reset();
        container.contents.prepend(entry);
        doc.containers.append(container);
        return doc;
    }
private slots:
    void roundTripPreservesEveryValue()
    {
        const WorkspaceDocument doc = sample();
        const DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QCOMPARE(int(DockMode::PanelDocked), 0);
        QCOMPARE(int(DockMode::OverlayDocked), 1);
        QCOMPARE(int(DockMode::Floating), 2);
        QCOMPARE(int(AxisLock::BottomLeft), 7);
    }
    void unknownFieldsKeepTheirOriginalNesting()
    {
        const QByteArray json = ContainerDocumentCodec::encode(sample());
        const QJsonObject original = QJsonDocument::fromJson(json).object();
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY(decoded.ok);
        QCOMPARE(QJsonDocument::fromJson(ContainerDocumentCodec::encode(decoded.document)).object(), original);
        QVERIFY(original.contains("futureWorkspace"));
        const QJsonObject container = original["containers"].toArray().first().toObject();
        QVERIFY(container.contains("futureContainer"));
        const QJsonObject entry = container["contents"].toArray().at(1).toObject();
        QVERIFY(entry.contains("futureContent"));
        QVERIFY(entry["returnLocation"].toObject().contains("futurePlacement"));
    }
    void duplicateIdsAreRejected()
    {
        WorkspaceDocument doc = sample();
        doc.containers.append(doc.containers.first());
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        QVERIFY(!ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc)).ok);
        doc = sample();
        doc.containers[0].contents[1].id = doc.containers[0].contents[0].id;
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        ContainerDocument other;
        other.id = "other";
        other.contents.append(doc.containers[0].contents[0]);
        doc.containers.append(other);
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
    }
    void invalidValuesAreRejected()
    {
        WorkspaceDocument doc = sample();
        doc.mainContainerId = "missing";
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        doc.containers[0].header = static_cast<HeaderMode>(7);
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        doc.containers[0].contents[0].canvasRect.setWidth(-1);
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        doc.containers[0].contents[0].canvasRect.setX(std::numeric_limits<double>::infinity());
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        doc.extensions.insert("revision", 99); // Never silently discard a colliding extension.
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
    }
    void malformedOrFutureDataIsRejected_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("truncated") << QByteArray("{\"schemaVersion\":1");
        QTest::newRow("not-object") << QByteArray("[]");
        QTest::newRow("missing-version") << QByteArray("{}");
        QTest::newRow("future") << QByteArray("{\"schemaVersion\":2,\"future-format\":[1,2]}");
        QTest::newRow("fractional-version") << QByteArray("{\"schemaVersion\":1.5}");
        QTest::newRow("invalid-revision") << QByteArray("{\"schemaVersion\":1,\"revision\":\"-1\",\"mainContainerId\":\"\",\"containers\":[]}");
        QTest::newRow("wrong-containers") << QByteArray("{\"schemaVersion\":1,\"revision\":\"0\",\"mainContainerId\":\"\",\"containers\":{}}");
    }
    void malformedOrFutureDataIsRejected()
    {
        QFETCH(QByteArray, json);
        const DocumentResult result = ContainerDocumentCodec::decode(json);
        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.document.containers.isEmpty());
    }
    void wrongKnownFieldTypeIsNotDefaulted()
    {
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(sample())).object();
        QJsonArray containers = root["containers"].toArray();
        QVERIFY(!containers.isEmpty());
        QJsonObject container = containers.first().toObject();
        container["visible"] = "False";
        containers[0] = container;
        root["containers"] = containers;
        QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
    }
};
QTEST_GUILESS_MAIN(TstContainerDocument)
#include "tst_container_document.moc"
