// no-port-check: NereusSDR-original presentation document contract tests.
// Modification history (NereusSDR):
//   2026-10-11 - Unknown keys inside screen are kept. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - A free home's geometryPane round trips. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - home round trip (strip and free), absent, unknown keys
//                 kept, and malformed. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - header loads and saves as Shown (0), Reveal (1) or Hidden
//                 (2); anything else is rejected. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - locked is written only when true, and a document without
//                 the key loads unlocked. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-11 - screen round trip, absent and malformed, and a floating
//                 container's unreadable geometry. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - geometryPane round trip, absent and malformed. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - A TX section row's part keys and order round trip.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - longpathDirectionBecomesBindingOnLoad. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - longpathDirectionBecomesBindingOnLoad: the frequency
//                 entry also loads with secondSlice "first". J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Container edge and workspace arrangement round trips for
//                 the workspace preview by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <limits>
#include <optional>
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ScreenSource.h"

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
    // A TX section row's hide keys and part order survive a save and a
    // load, as the workspace store writes them.
    void txSectionPartsRoundTrip()
    {
        WorkspaceDocument doc = sample();
        ContentEntry keying;
        keying.id = QStringLiteral("tx-keying-1");
        keying.typeId = QStringLiteral("tx.keying");
        keying.name = QStringLiteral("TX Keying");
        keying.config = {{"hideSliceIndicators", true}, {"hideMox", false},
                         {"order", QJsonArray{"mox", "tune", "sliceIndicators"}}};
        doc.containers[0].contents.append(keying);
        ContentEntry monitor;
        monitor.id = QStringLiteral("tx-monitor-1");
        monitor.typeId = QStringLiteral("tx.monitor");
        monitor.config = {{"hideNotice", true}, {"order", QJsonArray{"volume", "mon"}}};
        doc.containers[0].contents.append(monitor);

        const DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        const ContentEntry& loaded = decoded.document.containers[0].contents.at(doc.containers[0].contents.size() - 2);
        QCOMPARE(loaded.typeId, QStringLiteral("tx.keying"));
        QCOMPARE(loaded.config.value("order").toArray(), (QJsonArray{"mox", "tune", "sliceIndicators"}));
        QCOMPARE(loaded.config.value("hideSliceIndicators"), QJsonValue(true));
    }
    void lockedIsWrittenOnlyWhenTrue()
    {
        const auto containerObject = [](const WorkspaceDocument& doc) {
            return QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object()["containers"].toArray().first().toObject();
        };
        WorkspaceDocument doc = sample();
        QVERIFY(doc.containers[0].locked);
        QJsonObject written = containerObject(doc);
        QVERIFY(written.contains("locked"));
        QCOMPARE(written["locked"], QJsonValue(true));
        DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(decoded.document.containers[0].locked);

        doc.containers[0].locked = false;
        written = containerObject(doc);
        QVERIFY(!written.contains("locked"));
        QVERIFY(written.contains("autoHeight"));
        // Without the key the container loads unlocked.
        decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(!decoded.document.containers[0].locked);
        QCOMPARE(decoded.document, doc);

        // A document that carries "locked": false, as older builds wrote it,
        // still loads, unlocked.
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["locked"] = false;
        containers[0] = first;
        root["containers"] = containers;
        decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson(QJsonDocument::Compact));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
    }
    void headerLoadsAndSavesItsThreeValues_data()
    {
        QTest::addColumn<int>("saved");
        QTest::addColumn<bool>("loads");
        QTest::addColumn<HeaderMode>("header");
        QTest::addColumn<int>("writtenBack");
        QTest::newRow("0 is shown") << 0 << true << HeaderMode::Shown << 0;
        QTest::newRow("2 is hidden") << 2 << true << HeaderMode::Hidden << 2;
        // The codec does not know the look: the look-off hover mode
        // survives a load and a save. The manager hides it with the look on.
        QTest::newRow("1 is reveal") << 1 << true << HeaderMode::Reveal << 1;
        QTest::newRow("3 is rejected") << 3 << false << HeaderMode::Shown << 0;
        QTest::newRow("-1 is rejected") << -1 << false << HeaderMode::Shown << 0;
    }
    void headerLoadsAndSavesItsThreeValues()
    {
        QFETCH(int, saved);
        QFETCH(bool, loads);
        QFETCH(HeaderMode, header);
        QFETCH(int, writtenBack);
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(sample())).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["header"] = saved;
        containers[0] = first;
        root["containers"] = containers;
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson(QJsonDocument::Compact));
        QCOMPARE(decoded.ok, loads);
        if (!loads) {
            QVERIFY(!decoded.error.isEmpty());
            return;
        }
        QCOMPARE(decoded.document.containers[0].header, header);
        const QJsonObject written = QJsonDocument::fromJson(ContainerDocumentCodec::encode(decoded.document))
                                        .object()["containers"].toArray().first().toObject();
        QCOMPARE(written["header"].toInt(-1), writtenBack);
        // The other container values are untouched by the header read.
        WorkspaceDocument expected = sample();
        expected.containers[0].header = header;
        QCOMPARE(decoded.document, expected);
    }
    void onlyTheThreeHeaderValuesAreAValidDocument()
    {
        WorkspaceDocument doc = sample();
        for (HeaderMode mode : {HeaderMode::Shown, HeaderMode::Reveal, HeaderMode::Hidden}) {
            doc.containers[0].header = mode;
            QVERIFY(ContainerDocumentCodec::validate(doc).isEmpty());
        }
        doc.containers[0].header = static_cast<HeaderMode>(3);
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        QCOMPARE(int(HeaderMode::Shown), 0);
        QCOMPARE(int(HeaderMode::Reveal), 1);
        QCOMPARE(int(HeaderMode::Hidden), 2);
    }
    void freeCanvasKeepsPrecisePlacementAndLegacyRecord()
    {
        auto doc=sample();
        doc.containers[0].layout=static_cast<ContentLayout>(2);
        auto& entry=doc.containers[0].contents[0];
        entry.extensions["freeCanvasRect"]=QJsonArray{-17.1234567890123,22.9876543210987,460.123456789012,113.987654321098};
        const auto decoded=ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok,qPrintable(decoded.error));
        QCOMPARE(decoded.document,doc);
        QCOMPARE(int(ContentLayout::LegacyCanvas),0);
        QCOMPARE(int(ContentLayout::VerticalStack),1);
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
    void geometryPaneRoundTripsForAFreeContainer()
    {
        WorkspaceDocument doc = sample();
        doc.containers[0].dockMode = DockMode::OverlayDocked;
        doc.containers[0].geometryPane = QSize(1185, 872);
        const QByteArray json = ContainerDocumentCodec::encode(doc);
        const QJsonObject container = QJsonDocument::fromJson(json).object()["containers"].toArray().first().toObject();
        QCOMPARE(container["geometryPane"].toArray(), (QJsonArray{1185, 872}));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QCOMPARE(decoded.document.containers.first().geometryPane, std::optional<QSize>(QSize(1185, 872)));
        QVERIFY(!decoded.document.containers.first().extensions.contains("geometryPane"));
    }
    void geometryPaneAbsentStaysAbsent()
    {
        // A document from before the field: read as empty, written without it.
        const QByteArray json = ContainerDocumentCodec::encode(sample());
        QVERIFY(!json.contains("geometryPane"));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(!decoded.document.containers.first().geometryPane.has_value());
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
        // Written for free containers only.
        WorkspaceDocument floating = sample();
        floating.containers[0].geometryPane = QSize(1000, 700);
        QCOMPARE(floating.containers[0].dockMode, DockMode::Floating);
        QVERIFY(!ContainerDocumentCodec::encode(floating).contains("geometryPane"));
    }
    void malformedGeometryPaneIsDroppedForThatContainerOnly_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("string") << QJsonValue(QStringLiteral("1000x700"));
        QTest::newRow("three numbers") << QJsonValue(QJsonArray{1000, 700, 1});
        QTest::newRow("zero width") << QJsonValue(QJsonArray{0, 700});
        QTest::newRow("fraction") << QJsonValue(QJsonArray{1000.5, 700});
        QTest::newRow("object") << QJsonValue(QJsonObject{{"w", 1000}});
    }
    void malformedGeometryPaneIsDroppedForThatContainerOnly()
    {
        QFETCH(QJsonValue, value);
        WorkspaceDocument doc = sample();
        doc.containers[0].dockMode = DockMode::OverlayDocked;
        ContainerDocument other;
        other.id = QStringLiteral("other");
        other.dockMode = DockMode::OverlayDocked;
        other.geometry = QRect(10, 20, 300, 200);
        other.geometryPane = QSize(900, 600);
        doc.containers.append(other);
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["geometryPane"] = value;
        containers[0] = first;
        root["containers"] = containers;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unreadable geometryPane")));
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QVERIFY(!decoded.document.containers[0].geometryPane.has_value());
        QVERIFY(!decoded.document.containers[0].extensions.contains("geometryPane"));
        QCOMPARE(decoded.document.containers[1].geometryPane, std::optional<QSize>(QSize(900, 600)));
    }
    void homeRoundTripsForAWindow()
    {
        WorkspaceDocument doc = sample();
        QCOMPARE(doc.containers[0].dockMode, DockMode::Floating);
        ContainerHome strip;
        strip.dockMode = DockMode::PanelDocked;
        strip.edge = DockEdge::Left;
        strip.beforeId = QStringLiteral("first");
        strip.afterId = QStringLiteral("third");
        doc.containers[0].home = strip;
        QByteArray json = ContainerDocumentCodec::encode(doc);
        QJsonObject container = QJsonDocument::fromJson(json).object()["containers"].toArray().first().toObject();
        QCOMPARE(container["home"].toObject(),
                 (QJsonObject{{"dockMode", 0}, {"edge", "Left"}, {"before", "first"}, {"after", "third"}}));
        DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QVERIFY(!decoded.document.containers.first().extensions.contains("home"));
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);

        ContainerHome free;
        free.dockMode = DockMode::OverlayDocked;
        free.geometry = QRect(200, 150, 320, 240);
        doc.containers[0].home = free;
        json = ContainerDocumentCodec::encode(doc);
        container = QJsonDocument::fromJson(json).object()["containers"].toArray().first().toObject();
        QCOMPARE(container["home"].toObject(),
                 (QJsonObject{{"dockMode", 1}, {"geometry", QJsonArray{200, 150, 320, 240}}}));
        decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);

        // With the pane that rect was placed in.
        doc.containers[0].home->geometryPane = QSize(1100, 760);
        json = ContainerDocumentCodec::encode(doc);
        container = QJsonDocument::fromJson(json).object()["containers"].toArray().first().toObject();
        QCOMPARE(container["home"].toObject(),
                 (QJsonObject{{"dockMode", 1}, {"geometry", QJsonArray{200, 150, 320, 240}},
                              {"geometryPane", QJsonArray{1100, 760}}}));
        decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QVERIFY(!decoded.document.containers.first().home->extensions.contains("geometryPane"));
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
    }
    void homeAbsentStaysAbsentAndIsWrittenOnlyWhileFloating()
    {
        const QByteArray json = ContainerDocumentCodec::encode(sample());
        QVERIFY(!json.contains("\"home\""));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(!decoded.document.containers.first().home.has_value());
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
        WorkspaceDocument docked = sample();
        docked.containers[0].popOutShell = false;
        docked.containers[0].home = ContainerHome{};
        QVERIFY(ContainerDocumentCodec::encode(docked).contains("\"home\""));
        docked.containers[0].dockMode = DockMode::OverlayDocked;
        QVERIFY(!ContainerDocumentCodec::encode(docked).contains("\"home\""));
    }
    void unknownKeysInsideHomeSurviveALoadAndSave()
    {
        WorkspaceDocument doc = sample();
        doc.containers[0].home = ContainerHome{};
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        QJsonObject home = first["home"].toObject();
        home["futureIndex"] = 4;
        home["futureNote"] = QJsonObject{{"a", true}};
        first["home"] = home;
        containers[0] = first;
        root["containers"] = containers;
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(decoded.document.containers[0].home.has_value());
        QCOMPARE(decoded.document.containers[0].home->extensions,
                 (QJsonObject{{"futureIndex", 4}, {"futureNote", QJsonObject{{"a", true}}}}));
        const QJsonObject again = QJsonDocument::fromJson(ContainerDocumentCodec::encode(decoded.document))
                                      .object()["containers"].toArray().first().toObject()["home"].toObject();
        QCOMPARE(again, home);
        // A known key hidden among the unknown ones is not a valid document.
        WorkspaceDocument bad = doc;
        bad.containers[0].home->extensions["edge"] = QStringLiteral("Top");
        QVERIFY(!ContainerDocumentCodec::validate(bad).isEmpty());
    }
    void malformedHomeIsDroppedForThatContainerOnly_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("string") << QJsonValue(QStringLiteral("Right"));
        QTest::newRow("array") << QJsonValue(QJsonArray{200, 150, 320, 240});
        QTest::newRow("no dockMode") << QJsonValue(QJsonObject{{"edge", "Left"}});
        QTest::newRow("floating dockMode") << QJsonValue(QJsonObject{{"dockMode", 2}});
        QTest::newRow("unknown edge") << QJsonValue(QJsonObject{{"dockMode", 0}, {"edge", "Middle"}});
        QTest::newRow("before not a string") << QJsonValue(QJsonObject{{"dockMode", 0}, {"before", 3}});
        QTest::newRow("free with no geometry") << QJsonValue(QJsonObject{{"dockMode", 1}});
        QTest::newRow("free geometry of three")
            << QJsonValue(QJsonObject{{"dockMode", 1}, {"geometry", QJsonArray{0, 0, 320}}});
        QTest::newRow("free empty geometry")
            << QJsonValue(QJsonObject{{"dockMode", 1}, {"geometry", QJsonArray{0, 0, 0, 0}}});
        QTest::newRow("free with a pane of one number")
            << QJsonValue(QJsonObject{{"dockMode", 1}, {"geometry", QJsonArray{0, 0, 320, 240}},
                                      {"geometryPane", QJsonArray{1100}}});
    }
    void malformedHomeIsDroppedForThatContainerOnly()
    {
        QFETCH(QJsonValue, value);
        WorkspaceDocument doc = sample();
        ContainerDocument other;
        other.id = QStringLiteral("other");
        other.dockMode = DockMode::Floating;
        other.geometry = QRect(2100, 200, 400, 300);
        ContainerHome home;
        home.dockMode = DockMode::OverlayDocked;
        home.geometry = QRect(10, 20, 300, 200);
        other.home = home;
        doc.containers.append(other);
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["home"] = value;
        containers[0] = first;
        root["containers"] = containers;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unreadable home")));
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QVERIFY(!decoded.document.containers[0].home.has_value());
        QVERIFY(!decoded.document.containers[0].extensions.contains("home"));
        QCOMPARE(decoded.document.containers[1].home, other.home);
    }
    void screenRoundTripsForAWindow()
    {
        WorkspaceDocument doc = sample();
        QCOMPARE(doc.containers[0].dockMode, DockMode::Floating);
        doc.containers[0].screen = SavedScreen{QStringLiteral("DELL U2720Q"), QStringLiteral("SN-7"),
                                               QRect(1920, 0, 2560, 1440)};
        const QByteArray json = ContainerDocumentCodec::encode(doc);
        const QJsonObject container = QJsonDocument::fromJson(json).object()["containers"].toArray().first().toObject();
        QCOMPARE(container["screen"].toObject(),
                 (QJsonObject{{"name", "DELL U2720Q"}, {"serial", "SN-7"},
                              {"geometry", QJsonArray{1920, 0, 2560, 1440}}}));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QCOMPARE(decoded.document.containers.first().screen, doc.containers[0].screen);
        QVERIFY(!decoded.document.containers.first().extensions.contains("screen"));
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
    }
    void screenAbsentStaysAbsent()
    {
        // A document from before the field: read as empty, written without it.
        const QByteArray json = ContainerDocumentCodec::encode(sample());
        QVERIFY(!json.contains("\"screen\""));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(!decoded.document.containers.first().screen.has_value());
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
        // Written for floating and popped-out containers only.
        WorkspaceDocument docked = sample();
        docked.containers[0].dockMode = DockMode::OverlayDocked;
        docked.containers[0].popOutShell = false;
        docked.containers[0].screen = SavedScreen{QStringLiteral("A"), QString(), QRect(0, 0, 1920, 1080)};
        QVERIFY(!ContainerDocumentCodec::encode(docked).contains("\"screen\""));
        docked.containers[0].popOutShell = true;
        QVERIFY(ContainerDocumentCodec::encode(docked).contains("\"screen\""));
    }
    void unknownKeysInsideScreenSurviveALoadAndSave()
    {
        WorkspaceDocument doc = sample();
        doc.containers[0].screen = SavedScreen{QStringLiteral("A"), QStringLiteral("SN-7"), QRect(0, 0, 1920, 1080)};
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        QJsonObject screen = first["screen"].toObject();
        screen["futureScale"] = 2;
        screen["futureNote"] = QJsonObject{{"a", true}};
        first["screen"] = screen;
        containers[0] = first;
        root["containers"] = containers;
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(decoded.document.containers[0].screen.has_value());
        QCOMPARE(decoded.document.containers[0].screen->extensions,
                 (QJsonObject{{"futureScale", 2}, {"futureNote", QJsonObject{{"a", true}}}}));
        // The unknown keys play no part in matching the monitor.
        QVERIFY(screenMatches(ScreenInfo{QStringLiteral("A"), QStringLiteral("SN-7"), QRect(0, 0, 1920, 1080),
                                         QRect(0, 0, 1920, 1080)},
                              *decoded.document.containers[0].screen));
        const QJsonObject again = QJsonDocument::fromJson(ContainerDocumentCodec::encode(decoded.document))
                                      .object()["containers"].toArray().first().toObject()["screen"].toObject();
        QCOMPARE(again, screen);
        // A known key hidden among the unknown ones is not a valid document.
        WorkspaceDocument bad = doc;
        bad.containers[0].screen->extensions["serial"] = QStringLiteral("SN-8");
        QVERIFY(!ContainerDocumentCodec::validate(bad).isEmpty());
    }
    void malformedScreenIsDroppedForThatContainerOnly_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("string") << QJsonValue(QStringLiteral("DELL U2720Q"));
        QTest::newRow("array") << QJsonValue(QJsonArray{1920, 0, 2560, 1440});
        QTest::newRow("no geometry") << QJsonValue(QJsonObject{{"name", "A"}, {"serial", ""}});
        QTest::newRow("geometry of three") << QJsonValue(QJsonObject{{"name", "A"}, {"serial", ""},
                                                                      {"geometry", QJsonArray{0, 0, 1920}}});
        QTest::newRow("empty geometry") << QJsonValue(QJsonObject{{"name", "A"}, {"serial", ""},
                                                                   {"geometry", QJsonArray{0, 0, 0, 0}}});
        QTest::newRow("name not a string") << QJsonValue(QJsonObject{{"name", 7}, {"serial", ""},
                                                                      {"geometry", QJsonArray{0, 0, 1920, 1080}}});
    }
    void malformedScreenIsDroppedForThatContainerOnly()
    {
        QFETCH(QJsonValue, value);
        WorkspaceDocument doc = sample();
        ContainerDocument other;
        other.id = QStringLiteral("other");
        other.dockMode = DockMode::Floating;
        other.geometry = QRect(2100, 200, 400, 300);
        other.screen = SavedScreen{QStringLiteral("B"), QString(), QRect(1920, 0, 1920, 1080)};
        doc.containers.append(other);
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["screen"] = value;
        containers[0] = first;
        root["containers"] = containers;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unreadable screen")));
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QVERIFY(!decoded.document.containers[0].screen.has_value());
        QVERIFY(!decoded.document.containers[0].extensions.contains("screen"));
        QCOMPARE(decoded.document.containers[1].screen, other.screen);
    }
    void unreadableGeometryCostsAWindowItsPlaceAndNothingElse()
    {
        WorkspaceDocument doc = sample();
        ContainerDocument other;
        other.id = QStringLiteral("other");
        other.dockMode = DockMode::Floating;
        other.geometry = QRect(2100, 200, 400, 300);
        doc.containers.append(other);
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["geometry"] = QStringLiteral("100,200,300,400");
        containers[0] = first;
        root["containers"] = containers;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unreadable geometry; it takes a default place")));
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document.containers.size(), 2);
        QVERIFY(!decoded.document.containers[0].geometry.isValid());
        WorkspaceDocument expected = doc;
        expected.containers[0].geometry = QRect();
        QCOMPARE(decoded.document, expected);
        QCOMPARE(decoded.document.containers[1].geometry, other.geometry);
        // A docked or free container's geometry is still a known field
        // that is never defaulted.
        first["dockMode"] = int(DockMode::OverlayDocked);
        containers[0] = first;
        root["containers"] = containers;
        QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
    }
    void longpathDirectionBecomesBindingOnLoad()
    {
        // A needle or bar saved with "direction" and no "binding" loads
        // with the binding that direction meant (LongpathContent::
        // primaryBinding) and without "direction"; one with both keeps its
        // binding. The ids: SignalPeak 0, TxPower 100, TxSwr 102.
        const auto entryFor = [](const QString& id, const QString& typeId, const QJsonObject& config) {
            ContentEntry entry;
            entry.id = id;
            entry.typeId = typeId;
            entry.name = id;
            entry.config = config;
            return entry;
        };
        WorkspaceDocument doc = sample();
        QVector<ContentEntry>& contents = doc.containers[0].contents;
        const QVector<ContentEntry> untouched = contents;
        contents.append(entryFor(QStringLiteral("needle-tx"), QStringLiteral("longpath.needle"),
                                 {{"direction", "tx"}}));
        contents.append(entryFor(QStringLiteral("needle-rx"), QStringLiteral("longpath.needle"),
                                 {{"direction", "rx"}}));
        contents.append(entryFor(QStringLiteral("bar-tx"), QStringLiteral("longpath.bar"),
                                 {{"direction", "tx"}, {"compact", false}}));
        contents.append(entryFor(QStringLiteral("needle-both"), QStringLiteral("longpath.needle"),
                                 {{"direction", "tx"}, {"binding", 7}}));
        contents.append(entryFor(QStringLiteral("needle-bound"), QStringLiteral("longpath.needle"),
                                 {{"binding", 7}}));
        contents.append(entryFor(QStringLiteral("other-direction"), QStringLiteral("longpath.frequency"),
                                 {{"direction", "tx"}}));

        const DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        const QVector<ContentEntry>& loaded = decoded.document.containers[0].contents;
        QCOMPARE(loaded.size(), untouched.size() + 6);
        const auto configOf = [&loaded](const QString& id) {
            for (const ContentEntry& e : loaded) {
                if (e.id == id) { return e.config; }
            }
            return QJsonObject{{"missing", true}};
        };
        QCOMPARE(configOf(QStringLiteral("needle-tx")), (QJsonObject{{"binding", 100}}));
        QCOMPARE(configOf(QStringLiteral("needle-rx")), (QJsonObject{{"binding", 0}}));
        QCOMPARE(configOf(QStringLiteral("bar-tx")), (QJsonObject{{"binding", 102}, {"compact", false}}));
        QCOMPARE(configOf(QStringLiteral("needle-both")), (QJsonObject{{"binding", 7}}));
        QCOMPARE(configOf(QStringLiteral("needle-bound")), (QJsonObject{{"binding", 7}}));
        // Not a needle or bar: "direction" is left as saved. A frequency
        // instrument saved without "secondSlice" gets "first", so it keeps
        // the second line it showed before the pick existed.
        QCOMPARE(configOf(QStringLiteral("other-direction")),
                 (QJsonObject{{"direction", "tx"}, {"secondSlice", "first"}}));
        for (int i = 0; i < untouched.size(); ++i) {
            QCOMPARE(loaded.at(i), untouched.at(i));
        }

        // Decoding again changes nothing.
        const QByteArray once = ContainerDocumentCodec::encode(decoded.document);
        const DocumentResult again = ContainerDocumentCodec::decode(once);
        QVERIFY2(again.ok, qPrintable(again.error));
        QCOMPARE(again.document, decoded.document);
        QCOMPARE(ContainerDocumentCodec::encode(again.document), once);

        // An imported container takes the same path.
        ContainerDocument single = doc.containers[0];
        const DocumentResult imported =
            ContainerDocumentCodec::importContainer(ContainerDocumentCodec::exportContainer(single));
        QVERIFY2(imported.ok, qPrintable(imported.error));
        bool sawNeedle = false;
        for (const ContainerDocument& c : imported.document.containers) {
            for (const ContentEntry& e : c.contents) {
                if (e.name == QStringLiteral("needle-tx")) {
                    sawNeedle = true;
                    QCOMPARE(e.config, (QJsonObject{{"binding", 100}}));
                }
            }
        }
        QVERIFY(sawNeedle);
    }
    void documentWithoutEdgesStaysByteIdentical()
    {
        // A document from before edges: every container on the right, no
        // arrangement. Neither member is written, so it re-encodes exactly.
        const QByteArray json = ContainerDocumentCodec::encode(sample());
        QVERIFY(!json.contains("\"edge\""));
        QVERIFY(!json.contains("\"arrangement\""));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document.containers.first().edge, DockEdge::Right);
        QVERIFY(decoded.document.arrangement.isEmpty());
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
    }
    void edgeAndArrangementRoundTrip()
    {
        QCOMPARE(int(DockEdge::Left), 0);
        QCOMPARE(int(DockEdge::Right), 1);
        QCOMPARE(int(DockEdge::Top), 2);
        QCOMPARE(int(DockEdge::Bottom), 3);
        WorkspaceDocument doc = sample();
        doc.containers[0].dockMode = DockMode::PanelDocked;
        doc.containers[0].edge = DockEdge::Bottom;
        EdgeStripLayout left;
        left.thickness = 300;
        left.extents = {{"main", 420}, {"other", 210}};
        left.extensions = {{"futureStrip", QJsonArray{1, 2}}};
        doc.arrangement.strips.insert(DockEdge::Left, left);
        EdgeStripLayout top;
        top.thickness = 112;
        doc.arrangement.strips.insert(DockEdge::Top, top);
        doc.arrangement.gridAlignment = true;
        doc.arrangement.extensions = {{"futureArrangement", "kept"}};
        const QByteArray json = ContainerDocumentCodec::encode(doc);
        const QJsonObject root = QJsonDocument::fromJson(json).object();
        QCOMPARE(root["containers"].toArray().first().toObject()["edge"].toString(), QStringLiteral("Bottom"));
        QVERIFY(root["arrangement"].toObject()["strips"].toObject().contains("Left"));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
    }
    void unknownEdgeIsRejected()
    {
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(sample())).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject container = containers.first().toObject();
        for (const QJsonValue& bad : {QJsonValue("Middle"), QJsonValue(2), QJsonValue("left")}) {
            container["edge"] = bad;
            containers[0] = container;
            root["containers"] = containers;
            QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
        }
        root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(sample())).object();
        root["arrangement"] = QJsonObject{{"strips", QJsonObject{{"Middle", QJsonObject{{"thickness", 10}}}}}};
        QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
        root["arrangement"] = QJsonObject{{"strips", QJsonObject{{"Left", QJsonObject{{"thickness", -4}}}}}};
        QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
        WorkspaceDocument doc = sample();
        doc.containers[0].edge = static_cast<DockEdge>(9);
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
    }
};
QTEST_GUILESS_MAIN(TstContainerDocument)
#include "tst_container_document.moc"
