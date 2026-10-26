// no-port-check: NereusSDR-original tests for data out: the Thetis-format
// payload by slice and radio, the send interval, the endpoint by 4-char ID
// and the MMIO direction rule.
// Modification history (NereusSDR):
//   2026-10-26: a preview copy never sends; the live copy does.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: first version. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
#include <QtTest>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QNetworkDatagram>
#include <QSpinBox>
#include <QUdpSocket>
#include <QWidget>
#include <QXmlStreamReader>
#include <memory>

#include "core/AppSettings.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/DataOutItemEditor.h"
#include "gui/meters/DataOutItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"

using namespace NereusSDR;

namespace {

const QUuid kInGuid(QStringLiteral("8c0c8a52-5f8b-4b8e-9d0e-1a2b3c4d5e6f"));
const QUuid kOutGuid(QStringLiteral("3f2504e0-4f89-11d3-9a0c-0305e82c3301"));

// A record as a build before the send interval saved it: 11 parts.
const QString kOldRecord = QStringLiteral("DATAOUT|0|0|1|1|-1|0|||0|0");

const QString kMac = QStringLiteral("00:1C:C0:A2:13:DD");

// A UDP listener endpoint that sends to targetPort on loopback.
void writeEndpoint(const QUuid& guid, const QString& direction, quint16 targetPort)
{
    auto& s = AppSettings::instance();
    const auto key = [&guid](const QString& prop) {
        return QStringLiteral("MmioEndpoints/%1/%2").arg(guid.toString(QUuid::WithoutBraces), prop);
    };
    s.setValue(key(QStringLiteral("Name")), direction);
    s.setValue(key(QStringLiteral("Transport")), QStringLiteral("UdpListener"));
    s.setValue(key(QStringLiteral("Format")), QStringLiteral("Json"));
    s.setValue(key(QStringLiteral("Host")), QStringLiteral("127.0.0.1"));
    // Port 0: the listener takes any free loopback port.
    s.setValue(key(QStringLiteral("Port")), QStringLiteral("0"));
    s.setValue(key(QStringLiteral("Device")), QString());
    s.setValue(key(QStringLiteral("Baud")), QStringLiteral("9600"));
    s.setValue(key(QStringLiteral("Direction")), direction);
    s.setValue(key(QStringLiteral("UdpTargetHost")), QStringLiteral("127.0.0.1"));
    s.setValue(key(QStringLiteral("UdpTargetPort")), QString::number(targetPort));
}

// A saved RAW data out entry sending every 100 ms to guid.
ContentEntry sendingEntry(const QUuid& guid)
{
    ContentEntry entry;
    entry.id = QStringLiteral("dataout-sending");
    entry.typeId = QStringLiteral("DATAOUT");
    entry.name = QStringLiteral("DataOut");
    entry.config.insert(QStringLiteral("legacyRecord"),
        QStringLiteral("DATAOUT|0|0|1|1|-1|0|||2|0|100|") + guid.toString(QUuid::WithoutBraces));
    return entry;
}

QHash<int, double> swrAndPower()
{
    return {{MeterBinding::TxSwr, 1.5}, {MeterBinding::TxPower, 50.0}};
}

} // namespace

class TestDataOutItemSend : public QObject {
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

    void jsonNamesSliceRadioAndReadings()
    {
        DataOutItem item;
        item.setOutputFormat(DataOutItem::OutputFormat::Json);
        const QString payload = item.buildPayload(swrAndPower(), {}, QChar(u'A'), kMac);
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(doc.isObject());
        const QJsonObject obj = doc.object();
        QVERIFY(payload.contains(QStringLiteral("\"slice\":\"A\"")));
        QCOMPARE(obj.value(QStringLiteral("radio")).toString(), kMac);
        // Every value is a string, in the Thetis 0.0##### format.
        QCOMPARE(obj.value(QStringLiteral("swr")).toString(), QStringLiteral("1.5"));
        QCOMPARE(obj.value(QStringLiteral("pwr")).toString(), QStringLiteral("50.0"));
        QVERIFY(!obj.contains(QStringLiteral("rx")));
        QCOMPARE(obj.size(), 4);
    }

    void jsonRadioIsPresentAndEmptyWithoutSession()
    {
        DataOutItem item;
        const QJsonObject obj = QJsonDocument::fromJson(
            item.buildPayload(swrAndPower(), {}, QChar(), QString()).toUtf8()).object();
        QVERIFY(obj.contains(QStringLiteral("radio")));
        QCOMPARE(obj.value(QStringLiteral("radio")).toString(), QString());
        QCOMPARE(obj.value(QStringLiteral("slice")).toString(), QString());
    }

    void valuesUseThetisFormat()
    {
        DataOutItem item;
        item.setOutputFormat(DataOutItem::OutputFormat::Raw);
        const QHash<int, double> readings{
            {MeterBinding::TxSwr, 1.0}, {MeterBinding::TxPower, 12.3456789},
            {MeterBinding::TxReversePower, -0.25}};
        const QString raw = item.buildPayload(readings, {}, QChar(u'B'), QStringLiteral("r"));
        QVERIFY2(raw.contains(QStringLiteral("slice:B:swr:1.0:")), qPrintable(raw));
        QVERIFY2(raw.contains(QStringLiteral("slice:B:pwr:12.345679:")), qPrintable(raw));
        QVERIFY2(raw.contains(QStringLiteral(":-0.25:")), qPrintable(raw));
    }

    void rawLeadsWithRadioAndNamesSlicePerReading()
    {
        DataOutItem item;
        item.setOutputFormat(DataOutItem::OutputFormat::Raw);
        const QString raw = item.buildPayload(swrAndPower(), {}, QChar(u'A'), QStringLiteral("abc123"));
        QVERIFY2(raw.startsWith(QStringLiteral("radio:abc123:")), qPrintable(raw));
        QVERIFY2(raw.contains(QStringLiteral("slice:A:swr:1.5:")), qPrintable(raw));
        QVERIFY2(raw.contains(QStringLiteral("slice:A:pwr:50.0:")), qPrintable(raw));
        QVERIFY(raw.endsWith(QLatin1Char(':')));
    }

    void rawRemovesColonsFromStrings()
    {
        DataOutItem item;
        item.setOutputFormat(DataOutItem::OutputFormat::Raw);
        const QHash<QString, QVariant> published{{QStringLiteral("time_utc"), QStringLiteral("12:34:56")}};
        const QString raw = item.buildPayload({}, published, QChar(u'A'), kMac);
        QVERIFY2(raw.startsWith(QStringLiteral("radio:001CC0A213DD:")), qPrintable(raw));
        QVERIFY2(raw.contains(QStringLiteral("slice:A:time_utc:123456:")), qPrintable(raw));
    }

    void xmlUsesDataSliceAndRadio()
    {
        DataOutItem item;
        item.setOutputFormat(DataOutItem::OutputFormat::Xml);
        const QHash<QString, QVariant> published{{QStringLiteral("time_utc"), QStringLiteral("12:34:56")}};
        const QString xml = item.buildPayload(swrAndPower(), published, QChar(u'C'), kMac);
        QVERIFY2(xml.startsWith(QStringLiteral("<data>")), qPrintable(xml));
        QVERIFY2(xml.contains(QStringLiteral("<slice>C</slice>")), qPrintable(xml));
        QVERIFY2(xml.contains(QStringLiteral("<radio>") + kMac + QStringLiteral("</radio>")), qPrintable(xml));
        QVERIFY2(xml.contains(QStringLiteral("<swr>1.5</swr>")), qPrintable(xml));
        QVERIFY2(xml.contains(QStringLiteral("<time_utc>12:34:56</time_utc>")), qPrintable(xml));
        QVERIFY(!xml.contains(QStringLiteral("<rx")));
        QXmlStreamReader reader(xml);
        while (!reader.atEnd()) { reader.readNext(); }
        QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    }

    void bindingsWithoutThetisNameAreLeftOut()
    {
        DataOutItem item;
        const QHash<int, double> readings{{MeterBinding::TxSwr, 2.0}, {-12345, 7.0}};
        const QJsonObject obj = QJsonDocument::fromJson(
            item.buildPayload(readings, {}, QChar(u'A'), QString()).toUtf8()).object();
        QCOMPARE(obj.size(), 3);
        QCOMPARE(obj.value(QStringLiteral("swr")).toString(), QStringLiteral("2.0"));
    }

    void oldRecordKeepsClassInterval()
    {
        DataOutItem direct;
        QVERIFY(direct.deserialize(kOldRecord));
        QCOMPARE(direct.sendIntervalMs(), kDataOutClassIntervalMs);
        QCOMPARE(direct.sendIntervalMs(), 1000);
        QVERIFY(direct.endpointGuid().isNull());

        ContainerContentRegistry registry;
        ContentEntry entry;
        entry.id = QStringLiteral("saved-dataout");
        entry.typeId = QStringLiteral("DATAOUT");
        entry.name = QStringLiteral("DataOut");
        entry.config.insert(QStringLiteral("legacyRecord"), kOldRecord);
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* out = qobject_cast<DataOutItem*>(item.get());
        QVERIFY(out != nullptr);
        QCOMPARE(out->sendIntervalMs(), 1000);
    }

    void newItemSendsEvery500Ms()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("DATAOUT"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* out = qobject_cast<DataOutItem*>(item.get());
        QVERIFY(out != nullptr);
        QCOMPARE(out->sendIntervalMs(), kDataOutNewIntervalMs);
        QCOMPARE(out->sendIntervalMs(), 500);
    }

    void recordRoundTripsIntervalAndEndpoint()
    {
        DataOutItem item;
        item.setOutputFormat(DataOutItem::OutputFormat::Raw);
        item.setSendIntervalMs(250);
        item.setEndpointGuid(kOutGuid);
        const QString record = item.serialize();
        QCOMPARE(record.split(QLatin1Char('|')).size(), 13);
        DataOutItem copy;
        QVERIFY(copy.deserialize(record));
        QCOMPARE(copy.sendIntervalMs(), 250);
        QCOMPARE(copy.endpointGuid(), kOutGuid);
        QCOMPARE(copy.outputFormat(), DataOutItem::OutputFormat::Raw);
        QCOMPARE(copy.serialize(), record);

        // The interval stays within 100-5000.
        item.setSendIntervalMs(5);
        QCOMPARE(item.sendIntervalMs(), kDataOutMinMs);
        item.setSendIntervalMs(60000);
        QCOMPARE(item.sendIntervalMs(), kDataOutMaxMs);
    }

    void editorEnforcesIntervalAndPicksEndpointByFourChar()
    {
        writeEndpoint(kOutGuid, QStringLiteral("Out"), 9);
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kOutGuid);
        QVERIFY(ep);

        DataOutItem item;
        DataOutItemEditor editor;
        editor.setItem(&item);
        auto* spin = editor.findChild<QSpinBox*>(QStringLiteral("dataOutSendIntervalMs"));
        auto* id = editor.findChild<QLineEdit*>(QStringLiteral("dataOutFourCharId"));
        QVERIFY(spin);
        QVERIFY(id);
        QCOMPARE(spin->minimum(), 100);
        QCOMPARE(spin->maximum(), 5000);
        QCOMPARE(spin->singleStep(), 10);
        QCOMPARE(spin->value(), 1000);
        QCOMPARE(id->maxLength(), 4);
        QVERIFY(id->text().isEmpty());

        spin->setValue(50);
        QCOMPARE(spin->value(), 100);
        QCOMPARE(item.sendIntervalMs(), 100);
        spin->stepBy(1);
        QCOMPARE(item.sendIntervalMs(), 110);
        spin->setValue(9000);
        QCOMPARE(item.sendIntervalMs(), 5000);

        id->setText(ep->fourChar());
        emit id->editingFinished();
        QCOMPARE(item.endpointGuid(), kOutGuid);

        // Reopening shows the endpoint's ID.
        DataOutItemEditor reopened;
        reopened.setItem(&item);
        QCOMPARE(reopened.findChild<QLineEdit*>(QStringLiteral("dataOutFourCharId"))->text(),
                 ep->fourChar());

        // An ID no endpoint has sends nothing.
        const QString unknown = ep->fourChar() == QStringLiteral("zzzz")
            ? QStringLiteral("yyyy") : QStringLiteral("zzzz");
        id->setText(unknown);
        emit id->editingFinished();
        QVERIFY(item.endpointGuid().isNull());
    }

    void timerSendsMeterReadingsToOutEndpoint()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        writeEndpoint(kOutGuid, QStringLiteral("Out"), receiver.localPort());
        ExternalVariableEngine::instance().init();

        MeterWidget meter;
        auto* item = new DataOutItem(&meter);
        item->setOutputFormat(DataOutItem::OutputFormat::Raw);
        item->setSendIntervalMs(kDataOutMinMs);
        item->setEndpointGuid(kOutGuid);
        item->setSliceLetter(QChar(u'A'));
        item->setRadioId(QStringLiteral("abc123"));
        meter.addItem(item);
        meter.updateMeterValue(MeterBinding::TxSwr, 1.5);

        QByteArray received;
        const auto drain = [&receiver, &received] {
            while (receiver.hasPendingDatagrams()) {
                received.append(receiver.receiveDatagram().data());
            }
            return !received.isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(drain(), 5000);
        const QString text = QString::fromLatin1(received);
        QVERIFY2(text.startsWith(QStringLiteral("radio:abc123:")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("slice:A:swr:1.5:")), qPrintable(text));
    }

    void previewNeverSendsWhileLiveCopySends()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        writeEndpoint(kOutGuid, QStringLiteral("Out"), receiver.localPort());
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kOutGuid);
        QVERIFY(ep);
        const ContentEntry entry = sendingEntry(kOutGuid);
        ContainerContentRegistry registry;

        // The preview path the settings dialog and workspace preview use.
        QWidget host;
        std::unique_ptr<QWidget> preview(registry.createPreview(entry, &host));
        QVERIFY(preview);
        auto* previewItem = preview->findChild<DataOutItem*>();
        QVERIFY(previewItem);
        QCOMPARE(previewItem->endpointGuid(), kOutGuid);
        QCOMPARE(previewItem->sendIntervalMs(), 100);
        QVERIFY(!previewItem->sendEnabled());
        QVERIFY(!previewItem->sendTimerActive());
        auto* previewMeter = qobject_cast<MeterWidget*>(preview.get());
        QVERIFY(previewMeter);
        previewMeter->updateMeterValue(MeterBinding::TxSwr, 1.5);
        // Not even a direct send, nor a later endpoint change, makes it act.
        previewItem->sendNow();
        previewItem->setEndpointGuid(kOutGuid);
        QVERIFY(!previewItem->sendTimerActive());
        QCOMPARE(ep->outboundQueueSize(), 0);

        // Validation copies are inert too.
        std::unique_ptr<MeterItem> validation(
            registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
        auto* validationItem = qobject_cast<DataOutItem*>(validation.get());
        QVERIFY(validationItem);
        QVERIFY(!validationItem->sendTimerActive());

        // Five send intervals pass and nothing arrives.
        QTest::qWait(5 * 100);
        QVERIFY(!receiver.hasPendingDatagrams());

        // The live copy of the same entry sends.
        MeterWidget liveMeter;
        MeterItem* live = registry.createMeterItem(entry, &liveMeter);
        auto* liveItem = qobject_cast<DataOutItem*>(live);
        QVERIFY(liveItem);
        QVERIFY(liveItem->sendEnabled());
        QVERIFY(liveItem->sendTimerActive());
        liveMeter.addItem(live);
        liveMeter.updateMeterValue(MeterBinding::TxSwr, 1.5);
        QByteArray received;
        const auto drain = [&receiver, &received] {
            while (receiver.hasPendingDatagrams()) {
                received.append(receiver.receiveDatagram().data());
            }
            return !received.isEmpty();
        };
        QTRY_VERIFY_WITH_TIMEOUT(drain(), 5000);
        QVERIFY2(QString::fromLatin1(received).contains(QStringLiteral("swr:1.5:")),
                 received.constData());
    }

    void timerRunsOnlyWithAnEndpoint()
    {
        DataOutItem item;
        QVERIFY(!item.sendTimerActive());
        item.setEndpointGuid(kOutGuid);
        QVERIFY(item.sendTimerActive());
        item.setSendEnabled(false);
        QVERIFY(!item.sendTimerActive());
        item.setSendEnabled(true);
        QVERIFY(item.sendTimerActive());
        item.setEndpointGuid({});
        QVERIFY(!item.sendTimerActive());
    }

    void inEndpointReceivesNothing()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        writeEndpoint(kInGuid, QStringLiteral("In"), receiver.localPort());
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kInGuid);
        QVERIFY(ep);
        QCOMPARE(ep->direction(), MmioEndpoint::Direction::In);

        MeterWidget meter;
        auto* item = new DataOutItem(&meter);
        item->setSendIntervalMs(kDataOutMinMs);
        item->setEndpointGuid(kInGuid);
        meter.addItem(item);
        meter.updateMeterValue(MeterBinding::TxSwr, 1.5);

        item->sendNow();
        QCOMPARE(ep->outboundQueueSize(), 0);
        // Several timer periods pass and nothing arrives.
        QTest::qWait(4 * kDataOutMinMs);
        QCOMPARE(ep->outboundQueueSize(), 0);
        QVERIFY(!receiver.hasPendingDatagrams());
    }
};

QTEST_MAIN(TestDataOutItemSend)
#include "tst_data_out_item_send.moc"
