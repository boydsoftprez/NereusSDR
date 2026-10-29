// no-port-check: NereusSDR-original tests for the rotator's MMIO command
// path (container item parity, Task 46).
//
// The behaviour under test is ported from Thetis clsRotatorItem
// (MeterManager.cs:16402-16505, 16704-16717, 37205-37217 [v2.10.3.15]):
// with control allowed, a drag sends the AZ or ELE string and a press on the
// centre sends the STOP string to the chosen data source. Data sources here
// are UDP on 127.0.0.1 inside the test; nothing reaches a rotor.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26  J.J. Boyd / KG4VCF  Created (container item parity, Task
//                                    46). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-26  J.J. Boyd / KG4VCF  Fix round 1: a record saved before
//                                    Allow control keeps its rotor link
//                                    control. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QCheckBox>
#include <QComboBox>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMouseEvent>
#include <QNetworkDatagram>
#include <QPushButton>
#include <QTest>
#include <QUdpSocket>
#include <QUuid>

#include <cmath>
#include <limits>
#include <memory>
#include <numbers>

#include "core/AppSettings.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/RotatorItemEditor.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/RotatorItem.h"
#include "models/RotorCommandSink.h"
#include "models/RotorModel.h"

using namespace NereusSDR;
using RotorLink::RotorModel;

namespace {

constexpr int kW = 400;
constexpr int kH = 200;
// Long enough for the data source's worker to have sent anything queued.
constexpr int kQuietMs = 400;

const QUuid kOutGuid(QStringLiteral("6a1f0c2e-3b4d-4e5f-8a9b-0c1d2e3f4a5b"));
const QUuid kInGuid(QStringLiteral("7b2e1d3f-4c5e-4f60-9bac-1d2e3f4a5b6c"));

const QString kPstAz = QStringLiteral("<PST><AZIMUTH>%AZ%</AZIMUTH></PST>");
const QString kPstEle = QStringLiteral("<PST><ELEVATION>%ELE%</ELEVATION></PST>");
const QString kPstStop = QStringLiteral("<PST><STOP>1</STOP></PST>");

// A UDP data source on 127.0.0.1 (any free port) that sends to targetPort.
void writeEndpoint(const QUuid& guid, const QString& direction, quint16 targetPort)
{
    auto& s = AppSettings::instance();
    const auto key = [&guid](const QString& prop) {
        return QStringLiteral("MmioEndpoints/%1/%2").arg(guid.toString(QUuid::WithoutBraces), prop);
    };
    s.setValue(key(QStringLiteral("Name")), direction);
    s.setValue(key(QStringLiteral("Transport")), QStringLiteral("UdpListener"));
    s.setValue(key(QStringLiteral("Format")), QStringLiteral("Raw"));
    s.setValue(key(QStringLiteral("Host")), QStringLiteral("127.0.0.1"));
    s.setValue(key(QStringLiteral("Port")), QStringLiteral("0"));
    s.setValue(key(QStringLiteral("Device")), QString());
    s.setValue(key(QStringLiteral("Baud")), QStringLiteral("9600"));
    s.setValue(key(QStringLiteral("Direction")), direction);
    s.setValue(key(QStringLiteral("UdpTargetHost")), QStringLiteral("127.0.0.1"));
    s.setValue(key(QStringLiteral("UdpTargetPort")), QString::number(targetPort));
}

// Everything that arrived at the socket so far, appended to text.
void drain(QUdpSocket& socket, QString& text)
{
    while (socket.hasPendingDatagrams()) {
        text += QString::fromLatin1(socket.receiveDatagram().data());
    }
}

class FakeCommands : public RotorCommandSink {
public:
    QList<double> azimuths;
    int stops = 0;

    bool requestRotorTarget(double azimuthDeg, double, QString*) override
    {
        azimuths.append(azimuthDeg);
        return true;
    }
    bool requestStopRotor(QString*) override
    {
        ++stops;
        return true;
    }
    bool rotorControlAvailable(QString*) const override { return true; }
    bool requestTurnRotorToCall(const QString&, bool, QString*) override { return true; }
    bool requestNudgeRotor(Nudge, bool, QString*) override { return true; }
    bool requestConfigureRotor(const Setup&, QString*) override { return true; }
    bool requestRotorPresets(const QString&, QString*) override { return true; }
    quint32 lastRotorCommandId() const override { return 0; }
};

RotorModel::State connectedRotor()
{
    RotorModel::State s;
    s.driver = RotorModel::Driver::Gs232b;
    s.connectionPhase = TunerModel::ConnectionPhase::Connected;
    s.axes = RotorModel::Axes::AzimuthElevation;
    s.positionFresh = true;
    s.azimuthDeg = 90.0;
    s.elevationDeg = 10.0;
    return s;
}

QPointF compassPoint(const QPointF& centre, double degrees, double radius)
{
    const double rad = degrees * std::numbers::pi / 180.0;
    return {centre.x() + radius * std::sin(rad), centre.y() - radius * std::cos(rad)};
}

QPointF elevationPoint(const QPointF& centre, double degrees, double radius)
{
    const double rad = degrees * std::numbers::pi / 180.0;
    return {centre.x() + radius * std::cos(rad), centre.y() - radius * std::sin(rad)};
}

bool press(RotatorItem& item, const QPointF& at)
{
    QMouseEvent e(QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    return item.handleMousePress(&e, kW, kH);
}

bool release(RotatorItem& item, const QPointF& at)
{
    QMouseEvent e(QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    return item.handleMouseRelease(&e, kW, kH);
}

// A drag on the azimuth face let go at degrees, then a press and release on
// the stop circle.
void dragAzimuthThenStop(RotatorItem& item, double degrees)
{
    const QPointF az = item.azimuthCentre(kW, kH);
    const float r = item.pointerRadius(kW, kH);
    press(item, compassPoint(az, degrees, r * 0.8));
    release(item, compassPoint(az, degrees, r * 0.8));
    press(item, az);
    release(item, az);
}

} // namespace

class TestRotatorItemMmio : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        ExternalVariableEngine::instance().shutdown();
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        ExternalVariableEngine::instance().shutdown();
        AppSettings::instance().clear();
    }

    void aNewItemHasControlOffAndThePstRotatorStrings()
    {
        RotatorItem item;
        QVERIFY(!item.allowControl());
        QCOMPARE(item.controlColour(), QColor(50, 205, 50));  // LimeGreen
        QCOMPARE(item.commandPath(), RotatorItem::CommandPath::RotorLink);
        QVERIFY(item.commandEndpointGuid().isNull());
        QCOMPARE(item.azControlString(), kPstAz);
        QCOMPARE(item.eleControlString(), kPstEle);
        QCOMPARE(item.stopControlString(), kPstStop);
        QVERIFY(!item.rotorControllable());
    }

    void theMessagesAreFormattedAsThetis()
    {
        RotatorItem item;
        using Kind = RotatorItem::CommandKind;
        QCOMPARE(item.buildRotatorMessage(Kind::Azimuth, 7.0),
                 QStringLiteral("<PST><AZIMUTH>007</AZIMUTH></PST>"));
        // (int) drops the fraction.
        QCOMPARE(item.buildRotatorMessage(Kind::Azimuth, 7.9),
                 QStringLiteral("<PST><AZIMUTH>007</AZIMUTH></PST>"));
        QCOMPARE(item.buildRotatorMessage(Kind::Azimuth, 359.99),
                 QStringLiteral("<PST><AZIMUTH>359</AZIMUTH></PST>"));
        QCOMPARE(item.buildRotatorMessage(Kind::Elevation, 5.0),
                 QStringLiteral("<PST><ELEVATION>05</ELEVATION></PST>"));
        QCOMPARE(item.buildRotatorMessage(Kind::Elevation, 45.6),
                 QStringLiteral("<PST><ELEVATION>45</ELEVATION></PST>"));
        QCOMPARE(item.buildRotatorMessage(Kind::Stop, -1.0), kPstStop);
        // C# ToString("000") puts the sign before the zeros.
        QCOMPARE(item.buildRotatorMessage(Kind::Azimuth, -7.0),
                 QStringLiteral("<PST><AZIMUTH>-007</AZIMUTH></PST>"));
        // No number, nothing to send.
        QVERIFY(item.buildRotatorMessage(Kind::Azimuth,
                                         std::numeric_limits<double>::quiet_NaN()).isEmpty());

        // Only the command's own token is replaced, then \n, \r and \0 as
        // typed become the characters.
        item.setAzControlString(QStringLiteral("AZ %AZ% %ELE%\\r\\n"));
        item.setEleControlString(QStringLiteral("EL%ELE%\\n"));
        item.setStopControlString(QStringLiteral("S|\\0"));
        QCOMPARE(item.buildRotatorMessage(Kind::Azimuth, 123.4),
                 QStringLiteral("AZ 123 %ELE%\r\n"));
        QCOMPARE(item.buildRotatorMessage(Kind::Elevation, 9.0), QStringLiteral("EL09\n"));
        QCOMPARE(item.buildRotatorMessage(Kind::Stop, 0.0),
                 QStringLiteral("S|") + QChar(u'\0'));
    }

    void theControlSettingsRoundTripAndOlderRecordsLoadOff()
    {
        RotatorItem item;
        item.setAllowControl(true);
        item.setControlColour(QColor(1, 2, 3, 200));
        item.setCommandPath(RotatorItem::CommandPath::Mmio);
        item.setCommandEndpointGuid(kOutGuid);
        item.setAzControlString(QStringLiteral("A|%AZ%\\r"));
        item.setEleControlString(QString());
        item.setStopControlString(QStringLiteral("stop"));
        const QString record = item.serialize();
        QCOMPARE(record.split(QLatin1Char('|')).size(), 31);
        RotatorItem copy;
        QVERIFY(copy.deserialize(record));
        QVERIFY(copy.allowControl());
        QCOMPARE(copy.controlColour(), QColor(1, 2, 3, 200));
        QCOMPARE(copy.commandPath(), RotatorItem::CommandPath::Mmio);
        QCOMPARE(copy.commandEndpointGuid(), kOutGuid);
        QCOMPARE(copy.azControlString(), QStringLiteral("A|%AZ%\\r"));
        QCOMPARE(copy.eleControlString(), QString());
        QCOMPARE(copy.stopControlString(), QStringLiteral("stop"));
        QCOMPARE(copy.serialize(), record);

        // A record saved before fields 24-30: control on (it could turn the
        // rotor by the rotor link), the rotor link, the amber its control
        // was drawn in, the pstRotator strings.
        const QString older = QStringLiteral(
            "ROTATOR|0|0|1|1|300|3|2|1|0|0|30|0.6|0|0.5|"
            "#ffff0000|#ffffffff|#ff808080|#ffffffff|#ff404040|#ff202020||1|1");
        RotatorItem old;
        QVERIFY(old.deserialize(older));
        QVERIFY(old.allowControl());
        QCOMPARE(old.controlColour(), QColor(0xff, 0xb8, 0x00));
        QCOMPARE(old.commandPath(), RotatorItem::CommandPath::RotorLink);
        QVERIFY(old.commandEndpointGuid().isNull());
        QCOMPARE(old.azControlString(), kPstAz);
        QCOMPARE(old.stopControlString(), kPstStop);
        QVERIFY(old.serialize().startsWith(older + QStringLiteral("|1|")));

        // An explicitly saved off stays off.
        RotatorItem fresh;
        QVERIFY(!fresh.allowControl());
        RotatorItem reloaded;
        QVERIFY(reloaded.deserialize(fresh.serialize()));
        QVERIFY(!reloaded.allowControl());
        old.setAllowControl(false);
        QVERIFY(reloaded.deserialize(old.serialize()));
        QVERIFY(!reloaded.allowControl());
    }

    void anOlderRecordStillTurnsTheRotorByTheRotorLink()
    {
        // Saved before this task (24 fields): Both, rotor link control.
        const QString older = QStringLiteral(
            "ROTATOR|0|0|1|1|300|3|2|1|0|0|30|0.6|0|0.5|"
            "#ffff0000|#ffffffff|#ff808080|#ffffffff|#ff404040|#ff202020||1|1");
        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeCommands commands;
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("ROTATOR"));
        entry.config.insert(QStringLiteral("legacyRecord"), older);
        entry.canvasRect = {};
        entry.paintOrder = 3;  // the record's z-order

        MeterWidget liveWidget;
        auto* live = qobject_cast<RotatorItem*>(
            registry.createMeterItem(entry, &liveWidget, ContentRenderMode::Live));
        QVERIFY(live);
        QCOMPARE(live->commandPath(), RotatorItem::CommandPath::RotorLink);
        live->setRotor(&rotor, &commands);
        QVERIFY(live->rotorControllable());
        dragAzimuthThenStop(*live, 7.5);
        QCOMPARE(commands.azimuths, QList<double>{7.0});
        QCOMPARE(commands.stops, 2);
        // Captured again, it keeps no override: the record is unchanged.
        const ContentEntry captured = registry.captureMeterItem(*live, entry);
        QCOMPARE(captured.config.value(QStringLiteral("legacyRecord")).toString(), older);
        const QJsonObject overrides = captured.config.value(QStringLiteral("overrides")).toObject();
        QVERIFY2(overrides.isEmpty(), qPrintable(QJsonDocument(overrides).toJson(QJsonDocument::Compact)));

        // Its preview copy still refuses.
        MeterWidget previewWidget;
        auto* preview = qobject_cast<RotatorItem*>(
            registry.createMeterItem(entry, &previewWidget, ContentRenderMode::Preview));
        QVERIFY(preview);
        QVERIFY(preview->allowControl());
        preview->setRotor(&rotor, &commands);
        QVERIFY(!preview->rotorControllable());
        dragAzimuthThenStop(*preview, 100.5);
        QCOMPARE(commands.azimuths.size(), 1);
        QCOMPARE(commands.stops, 2);

        // A new item sends nothing until Allow control is turned on.
        const ContentEntry fresh = registry.makeEntry(QStringLiteral("ROTATOR"));
        MeterWidget newWidget;
        auto* added = qobject_cast<RotatorItem*>(
            registry.createMeterItem(fresh, &newWidget, ContentRenderMode::Live));
        QVERIFY(added);
        QVERIFY(!added->allowControl());
        added->setRotor(&rotor, &commands);
        QVERIFY(!added->rotorControllable());
        dragAzimuthThenStop(*added, 200.5);
        QCOMPARE(commands.azimuths.size(), 1);
        QCOMPARE(commands.stops, 2);
        added->setAllowControl(true);
        dragAzimuthThenStop(*added, 200.5);
        QCOMPARE(commands.azimuths.size(), 2);
        QCOMPARE(commands.azimuths.last(), 200.0);
        QCOMPARE(commands.stops, 4);
    }

    void withControlOffNothingIsSent()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        writeEndpoint(kOutGuid, QStringLiteral("Out"), receiver.localPort());
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kOutGuid);
        QVERIFY(ep);

        RotatorItem item;
        item.setCommandPath(RotatorItem::CommandPath::Mmio);
        item.setCommandEndpointGuid(kOutGuid);
        QVERIFY(!item.rotorControllable());
        const QPointF az = item.azimuthCentre(kW, kH);
        QVERIFY(!press(item, compassPoint(az, 7.5, item.pointerRadius(kW, kH) * 0.8)));
        QVERIFY(!press(item, az));
        QVERIFY(!release(item, az));

        // Turned off mid drag: the release sends nothing.
        item.setAllowControl(true);
        QVERIFY(item.rotorControllable());
        QVERIFY(press(item, compassPoint(az, 7.5, item.pointerRadius(kW, kH) * 0.8)));
        item.setAllowControl(false);
        QVERIFY(!release(item, compassPoint(az, 7.5, item.pointerRadius(kW, kH) * 0.8)));

        QTest::qWait(kQuietMs);
        QCOMPARE(ep->outboundQueueSize(), 0);
        QVERIFY(!receiver.hasPendingDatagrams());
    }

    void aDragAndTheStopCircleSendThePstRotatorStrings()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        writeEndpoint(kOutGuid, QStringLiteral("Out"), receiver.localPort());
        ExternalVariableEngine::instance().init();

        RotatorItem item;  // Both
        item.setAllowControl(true);
        item.setCommandPath(RotatorItem::CommandPath::Mmio);
        item.setCommandEndpointGuid(kOutGuid);
        QVERIFY(item.rotorControllable());
        QVERIFY(item.elevationControllable());

        QString got;
        const QPointF az = item.azimuthCentre(kW, kH);
        const float r = item.pointerRadius(kW, kH);
        QVERIFY(press(item, compassPoint(az, 7.5, r * 0.8)));
        QVERIFY(release(item, compassPoint(az, 7.5, r * 0.8)));
        QCOMPARE(item.targetAzimuthMarker(), 7.0f);
        QTRY_VERIFY_WITH_TIMEOUT((drain(receiver, got), got.contains(
            QStringLiteral("<PST><AZIMUTH>007</AZIMUTH></PST>"))), 5000);

        // A press on the centre stops, and so does letting go there.
        got.clear();
        QVERIFY(press(item, az));
        QVERIFY(release(item, az));
        QTRY_VERIFY_WITH_TIMEOUT((drain(receiver, got), got.count(kPstStop) == 2), 5000);

        // The elevation face sends the ELE string, two digits.
        got.clear();
        const QPointF ele = item.elevationCentre(kW, kH);
        const float er = 0.39f * kH;
        QVERIFY(press(item, elevationPoint(ele, 30.5, er * 0.8)));
        QVERIFY(release(item, elevationPoint(ele, 30.5, er * 0.8)));
        QCOMPARE(item.targetElevationMarker(), 30.0f);
        QTRY_VERIFY_WITH_TIMEOUT((drain(receiver, got), got.contains(
            QStringLiteral("<PST><ELEVATION>30</ELEVATION></PST>"))), 5000);
        QTest::qWait(kQuietMs);
        drain(receiver, got);
        QVERIFY2(!got.contains(QStringLiteral("AZIMUTH")), qPrintable(got));
    }

    void aDirectionInDataSourceReceivesNothing()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        writeEndpoint(kInGuid, QStringLiteral("In"), receiver.localPort());
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kInGuid);
        QVERIFY(ep);
        QCOMPARE(ep->direction(), MmioEndpoint::Direction::In);

        RotatorItem item;
        item.setAllowControl(true);
        item.setCommandPath(RotatorItem::CommandPath::Mmio);
        item.setCommandEndpointGuid(kInGuid);
        QVERIFY(!item.rotorControllable());
        dragAzimuthThenStop(item, 7.5);
        QCOMPARE(item.targetAzimuthMarker(), RotatorItem::kNoAngle);

        // An ID no data source has sends nothing either.
        item.setCommandEndpointGuid(QUuid::createUuid());
        QVERIFY(!item.rotorControllable());
        item.setCommandEndpointGuid({});
        QVERIFY(!item.rotorControllable());

        QTest::qWait(kQuietMs);
        QCOMPARE(ep->outboundQueueSize(), 0);
        QVERIFY(!receiver.hasPendingDatagrams());
    }

    void theRotorLinkPathUsesTheSinkAndEnqueuesNothing()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        writeEndpoint(kOutGuid, QStringLiteral("Out"), receiver.localPort());
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kOutGuid);
        QVERIFY(ep);

        RotorModel rotor;
        rotor.setState(connectedRotor());
        FakeCommands commands;
        RotatorItem item;
        item.setAllowControl(true);
        item.setCommandEndpointGuid(kOutGuid);  // chosen, but not the path
        item.setRotor(&rotor, &commands);
        QCOMPARE(item.commandPath(), RotatorItem::CommandPath::RotorLink);
        QVERIFY(item.rotorControllable());
        dragAzimuthThenStop(item, 7.5);
        QCOMPARE(commands.azimuths, QList<double>{7.0});
        QCOMPARE(commands.stops, 2);

        // With control off the sink gets nothing either.
        item.setAllowControl(false);
        dragAzimuthThenStop(item, 100.5);
        QCOMPARE(commands.azimuths.size(), 1);
        QCOMPARE(commands.stops, 2);

        // On the Mmio path the rotor link is not used.
        item.setAllowControl(true);
        item.setCommandPath(RotatorItem::CommandPath::Mmio);
        QString got;
        dragAzimuthThenStop(item, 200.5);
        QCOMPARE(commands.azimuths.size(), 1);
        QCOMPARE(commands.stops, 2);
        QTRY_VERIFY_WITH_TIMEOUT((drain(receiver, got), got.contains(
            QStringLiteral("<PST><AZIMUTH>200</AZIMUTH></PST>")) && got.contains(kPstStop)), 5000);

        // Back on the rotor link: nothing more is enqueued.
        item.setCommandPath(RotatorItem::CommandPath::RotorLink);
        got.clear();
        dragAzimuthThenStop(item, 45.5);
        QCOMPARE(commands.azimuths.size(), 2);
        QTest::qWait(kQuietMs);
        drain(receiver, got);
        QVERIFY2(got.isEmpty(), qPrintable(got));
        QCOMPARE(ep->outboundQueueSize(), 0);
    }

    void aPreviewCopyNeverSends()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        writeEndpoint(kOutGuid, QStringLiteral("Out"), receiver.localPort());
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kOutGuid);
        QVERIFY(ep);

        RotatorItem saved;
        saved.setAllowControl(true);
        saved.setCommandPath(RotatorItem::CommandPath::Mmio);
        saved.setCommandEndpointGuid(kOutGuid);
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("ROTATOR"));
        entry.config.insert(QStringLiteral("legacyRecord"), saved.serialize());

        for (const ContentRenderMode mode : {ContentRenderMode::Preview,
                                             ContentRenderMode::Validation}) {
            MeterWidget widget;
            auto* copy = qobject_cast<RotatorItem*>(registry.createMeterItem(entry, &widget, mode));
            QVERIFY(copy);
            QVERIFY(copy->allowControl());
            QCOMPARE(copy->commandPath(), RotatorItem::CommandPath::Mmio);
            QCOMPARE(copy->commandEndpointGuid(), kOutGuid);
            QVERIFY(!copy->controlEnabled());
            QVERIFY(!copy->rotorControllable());
            const QPointF az = copy->azimuthCentre(kW, kH);
            QVERIFY(!press(*copy, compassPoint(az, 7.5, copy->pointerRadius(kW, kH) * 0.8)));
            QVERIFY(!release(*copy, compassPoint(az, 7.5, copy->pointerRadius(kW, kH) * 0.8)));
            QVERIFY(!press(*copy, az));
            QVERIFY(!release(*copy, az));
        }
        QTest::qWait(kQuietMs);
        QCOMPARE(ep->outboundQueueSize(), 0);
        QVERIFY(!receiver.hasPendingDatagrams());

        // The live copy of the same entry sends.
        MeterWidget liveWidget;
        auto* live = qobject_cast<RotatorItem*>(
            registry.createMeterItem(entry, &liveWidget, ContentRenderMode::Live));
        QVERIFY(live);
        QVERIFY(live->rotorControllable());
        const QPointF az = live->azimuthCentre(kW, kH);
        QVERIFY(press(*live, az));
        QString got;
        QTRY_VERIFY_WITH_TIMEOUT((drain(receiver, got), got.contains(kPstStop)), 5000);
    }

    void editorRowsFollowAllowControlAndThePath()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        writeEndpoint(kOutGuid, QStringLiteral("Out"), receiver.localPort());
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kOutGuid);
        QVERIFY(ep);

        RotatorItem item;
        RotatorItemEditor editor;
        editor.setItem(&item);
        auto* allow = editor.findChild<QCheckBox*>(QStringLiteral("rotatorAllowControl"));
        auto* colour = editor.findChild<QPushButton*>(QStringLiteral("rotatorControlColour"));
        auto* path = editor.findChild<QComboBox*>(QStringLiteral("rotatorCommandPath"));
        auto* id = editor.findChild<QLineEdit*>(QStringLiteral("rotatorFourCharId"));
        auto* az = editor.findChild<QLineEdit*>(QStringLiteral("rotatorAzCommand"));
        auto* ele = editor.findChild<QLineEdit*>(QStringLiteral("rotatorEleCommand"));
        auto* stop = editor.findChild<QLineEdit*>(QStringLiteral("rotatorStopCommand"));
        auto* pst = editor.findChild<QPushButton*>(QStringLiteral("rotatorPstRotatorDefaults"));
        QVERIFY(allow && colour && path && id && az && ele && stop && pst);

        QVERIFY(!allow->isChecked());
        QVERIFY(!colour->isEnabled());
        QVERIFY(!path->isEnabled());
        QVERIFY(!id->isEnabled());
        QVERIFY(!az->isEnabled());
        QCOMPARE(az->text(), kPstAz);
        QCOMPARE(path->currentData().toInt(), static_cast<int>(RotatorItem::CommandPath::RotorLink));

        allow->setChecked(true);
        QVERIFY(item.allowControl());
        QVERIFY(colour->isEnabled());
        QVERIFY(path->isEnabled());
        // The data source rows wait for the data source path.
        QVERIFY(!id->isEnabled());
        QVERIFY(!pst->isEnabled());
        path->setCurrentIndex(path->findData(static_cast<int>(RotatorItem::CommandPath::Mmio)));
        QCOMPARE(item.commandPath(), RotatorItem::CommandPath::Mmio);
        QVERIFY(id->isEnabled());
        QVERIFY(az->isEnabled());
        QVERIFY(ele->isEnabled());
        QVERIFY(stop->isEnabled());
        QVERIFY(pst->isEnabled());

        // The data source by its 4-char ID.
        id->setText(ep->fourChar());
        QMetaObject::invokeMethod(id, "editingFinished");
        QCOMPARE(item.commandEndpointGuid(), kOutGuid);
        QVERIFY(item.rotorControllable());

        az->setText(QStringLiteral("AZ%AZ%"));
        QMetaObject::invokeMethod(az, "editingFinished");
        QCOMPARE(item.azControlString(), QStringLiteral("AZ%AZ%"));
        pst->click();
        QCOMPARE(item.azControlString(), kPstAz);
        QCOMPARE(az->text(), kPstAz);

        // A loaded item shows its own settings.
        RotatorItem other;
        other.setAllowControl(true);
        other.setCommandPath(RotatorItem::CommandPath::Mmio);
        other.setCommandEndpointGuid(kOutGuid);
        other.setStopControlString(QStringLiteral("halt"));
        editor.setItem(&other);
        QVERIFY(allow->isChecked());
        QCOMPARE(id->text(), ep->fourChar());
        QCOMPARE(stop->text(), QStringLiteral("halt"));
        QVERIFY(stop->isEnabled());

        allow->setChecked(false);
        QVERIFY(!other.allowControl());
        QVERIFY(!path->isEnabled());
        QVERIFY(!stop->isEnabled());
    }
};

QTEST_MAIN(TestRotatorItemMmio)
#include "tst_rotator_item_mmio.moc"
