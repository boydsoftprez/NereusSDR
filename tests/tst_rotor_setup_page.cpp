// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_rotor_setup_page.cpp  (NereusSDR)
// =================================================================
//
// Rotor control plan Task 7: CAT & Network > Rotor. In a remote window the
// page lists the Core's serial ports, shows the Core's setup and sends
// configureRotor and setRotorPresets to the Core; the Core's refusal shows
// on the page while it is open and as a notice otherwise; a Core too old
// for the rotor greys every control with the reason. In a local window the
// page sets up this process's own rotor and shows its refusals at once.
// No test opens a real serial port or turns a real rotor.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 7).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>

#include "core/AppSettings.h"
#include "core/RotorConnection.h"
#include "core/StationRotorController.h"
#include "core/session/IStationLink.h"
#include "gui/setup/RotorSetupPage.h"
#include "models/RadioModel.h"
#include "models/RotorModel.h"
#include "OperatorWording.h"

using namespace NereusSDR;
using RotorLink::RotorModel;

namespace {

const QString kUnknownPort = QStringLiteral("That serial port is not on the Core's computer.");

class RecordingRotorLink final : public IStationLink {
public:
    struct Configure {
        int driver;
        QString serialPort;
        int baud;
        QString host;
        int port;
        int hamlibModel;
        int axes;
        int endStop;
        int rangeDeg;
        double offsetDeg;
    };
    bool available{true};
    QList<Configure> configures;
    QStringList presets;
    quint32 nextId{100};

    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool stationLinkReady() const override { return true; }
    bool rotorControlAvailable() const override { return available; }
    CommandOutcome requestConfigureRotor(int driver, const QString& serialPort, int baud,
                                         const QString& host, int port, int hamlibModel,
                                         int axes, int endStop, int rangeDeg,
                                         double offsetDeg) override
    {
        if (!available) {
            return IStationLink::requestConfigureRotor(driver, serialPort, baud, host, port,
                                                       hamlibModel, axes, endStop, rangeDeg,
                                                       offsetDeg);
        }
        configures.append({driver, serialPort, baud, host, port, hamlibModel, axes, endStop,
                           rangeDeg, offsetDeg});
        return {true, {}, ++nextId};
    }
    CommandOutcome requestRotorPresets(const QString& text) override
    {
        if (!available) {
            return IStationLink::requestRotorPresets(text);
        }
        presets.append(text);
        return {true, {}, ++nextId};
    }
};

// A rotor byte stream that never opens: the rotor stays connecting.
class SilentTransport : public RotorTransport {
public:
    void open() override {}
    void close() override {}
    qint64 write(const QByteArray& bytes) override { return bytes.size(); }
    QByteArray readAll() override { return {}; }
};

// The Core's rotor as JJ's is set up: an ERC (GS-232B) on the Core's
// second port, south end stop, 450 degrees.
RotorModel::State coreRotor()
{
    RotorModel::State s;
    s.connectionPhase = TunerModel::ConnectionPhase::Connected;
    s.driver = RotorModel::Driver::Gs232b;
    s.label = QStringLiteral("Yaesu GS-232B on /dev/ttyCORE2");
    s.serialPort = QStringLiteral("/dev/ttyCORE2");
    s.baud = 9600;
    s.serialPorts = QStringLiteral("/dev/ttyCORE1\n/dev/ttyCORE2");
    s.endStop = RotorModel::EndStop::South;
    s.rangeDeg = 450;
    s.offsetDeg = 2.5;
    s.presets = QStringLiteral("Europe\t45\nJapan\t330");
    return s;
}

QStringList items(const QComboBox* combo)
{
    QStringList out;
    for (int i = 0; i < combo->count(); ++i) {
        out.append(combo->itemText(i));
    }
    return out;
}

void selectData(QComboBox* combo, const QVariant& value)
{
    combo->setCurrentIndex(combo->findData(value));
}

void verifyPlain(const RotorSetupPage& page)
{
    for (const QLabel* label : page.findChildren<QLabel*>()) {
        if (!label->text().isEmpty() && label->textFormat() == Qt::PlainText) {
            QVERIFY2(OperatorWording::isPlain(label->text()), qPrintable(label->text()));
            QVERIFY2(!label->text().contains(QChar(0x2014)), qPrintable(label->text()));
        }
    }
    for (const QWidget* w : page.findChildren<QWidget*>()) {
        if (!w->toolTip().isEmpty()) {
            QVERIFY2(OperatorWording::isPlain(w->toolTip()), qPrintable(w->toolTip()));
        }
    }
}

} // namespace

class TstRotorSetupPage : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

    void remoteWindowListsTheCoresSerialPorts()
    {
        RadioModel window(RadioModel::Role::Remote);
        RecordingRotorLink link;
        window.attachStation(&link);
        window.rotorModel()->setState(coreRotor());
        RotorSetupPage page(&window);

        // The Core's ports, not this computer's.
        QCOMPARE(items(page.serialPortComboForTesting()),
                 QStringList({QStringLiteral("/dev/ttyCORE1"), QStringLiteral("/dev/ttyCORE2")}));
        QCOMPARE(page.serialPortComboForTesting()->currentText(), QStringLiteral("/dev/ttyCORE2"));
        // The Core's setup.
        QCOMPARE(page.driverComboForTesting()->currentData().toInt(),
                 static_cast<int>(RotorModel::Driver::Gs232b));
        QCOMPARE(page.baudComboForTesting()->currentData().toInt(), 9600);
        QCOMPARE(page.endStopComboForTesting()->currentData().toInt(),
                 static_cast<int>(RotorModel::EndStop::South));
        QCOMPARE(page.rangeComboForTesting()->currentData().toInt(), 450);
        QCOMPARE(page.offsetSpinForTesting()->value(), 2.5);
        QCOMPARE(page.presetsTableForTesting()->rowCount(), 2);
        QCOMPARE(page.presetsTableForTesting()->item(1, 0)->text(), QStringLiteral("Japan"));
        QCOMPARE(page.presetsTableForTesting()->item(1, 1)->text(), QStringLiteral("330"));
        QVERIFY(page.statusTextForTesting().contains(QStringLiteral("connected")));

        // A port the Core gains shows; the operator's choice stays.
        RotorModel::State more = coreRotor();
        more.serialPorts = QStringLiteral("/dev/ttyCORE1\n/dev/ttyCORE2\n/dev/ttyCORE3");
        window.rotorModel()->setState(more);
        QCOMPARE(page.serialPortComboForTesting()->count(), 3);
        QCOMPARE(page.serialPortComboForTesting()->currentText(), QStringLiteral("/dev/ttyCORE2"));

        // Fields this driver does not use are greyed, not hidden.
        QVERIFY(!page.hostEditForTesting()->isEnabled());
        QVERIFY(!page.hostEditForTesting()->isHidden());
        QVERIFY(!page.hamlibComboForTesting()->isEnabled());
        QVERIFY(page.serialPortComboForTesting()->isEnabled());
        verifyPlain(page);
    }

    void remoteSaveSendsConfigureRotor()
    {
        RadioModel window(RadioModel::Role::Remote);
        RecordingRotorLink link;
        window.attachStation(&link);
        window.rotorModel()->setState(coreRotor());
        RotorSetupPage page(&window);

        page.serialPortComboForTesting()->setCurrentText(QStringLiteral("/dev/ttyCORE1"));
        selectData(page.baudComboForTesting(), 19200);
        selectData(page.axesComboForTesting(), static_cast<int>(RotorModel::Axes::AzimuthElevation));
        page.offsetSpinForTesting()->setValue(-4.0);
        page.saveButtonForTesting()->click();
        QCOMPARE(link.configures.size(), 1);
        const RecordingRotorLink::Configure& c = link.configures.constFirst();
        QCOMPARE(c.driver, 2);
        QCOMPARE(c.serialPort, QStringLiteral("/dev/ttyCORE1"));
        QCOMPARE(c.baud, 19200);
        QCOMPARE(c.axes, 1);
        QCOMPARE(c.endStop, 2);
        QCOMPARE(c.rangeDeg, 450);
        QCOMPARE(c.offsetDeg, -4.0);
        QCOMPARE(c.hamlibModel, 0);   // driver 4 only
        QCOMPARE(c.port, 4533);

        // The Core has not answered: its old setup does not overwrite the
        // operator's fields.
        window.rotorModel()->setState(coreRotor());
        QCOMPARE(page.serialPortComboForTesting()->currentText(), QStringLiteral("/dev/ttyCORE1"));
        // Accepted: the fields follow the Core again.
        window.reportStationCommandFinished(link.nextId, true, {});
        QCOMPARE(page.serialPortComboForTesting()->currentText(), QStringLiteral("/dev/ttyCORE2"));

        // rotctld started by the Core: the Hamlib model goes with it.
        selectData(page.driverComboForTesting(),
                   static_cast<int>(RotorModel::Driver::RotctldStarted));
        QVERIFY(page.hamlibComboForTesting()->isEnabled());
        QVERIFY(page.portSpinForTesting()->isEnabled());
        selectData(page.hamlibComboForTesting(), 404);   // ERC
        QCOMPARE(page.hamlibModelSpinForTesting()->value(), 404);
        page.saveButtonForTesting()->click();
        QCOMPARE(link.configures.size(), 2);
        QCOMPARE(link.configures.constLast().driver, 4);
        QCOMPARE(link.configures.constLast().hamlibModel, 404);
        // rotctld is not on the Core here: the page says so.
        bool said = false;
        for (const QLabel* label : page.findChildren<QLabel*>()) {
            said = said || label->text() == StationRotorController::rotctldMissingReason();
        }
        QVERIFY(said);

        // rotctld already running: host and port.
        selectData(page.driverComboForTesting(), static_cast<int>(RotorModel::Driver::Rotctld));
        QVERIFY(page.hostEditForTesting()->isEnabled());
        QVERIFY(!page.serialPortComboForTesting()->isEnabled());
        page.hostEditForTesting()->setText(QStringLiteral("192.0.2.7"));
        page.portSpinForTesting()->setValue(4600);
        page.saveButtonForTesting()->click();
        QCOMPARE(link.configures.constLast().driver, 3);
        QCOMPARE(link.configures.constLast().host, QStringLiteral("192.0.2.7"));
        QCOMPARE(link.configures.constLast().port, 4600);
        QCOMPARE(link.configures.constLast().serialPort, QString());
    }

    void remoteRefusalShowsOnThePageWhileOpen()
    {
        RadioModel window(RadioModel::Role::Remote);
        RecordingRotorLink link;
        window.attachStation(&link);
        window.rotorModel()->setState(coreRotor());
        QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);
        RotorSetupPage page(&window);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        page.saveButtonForTesting()->click();
        const quint32 id = link.nextId;
        // StationClient's order: the accessory refusal, then the result.
        window.reportStationAccessoryRefusal(QStringLiteral("rotor"), kUnknownPort, id);
        window.reportStationCommandFinished(id, false, kUnknownPort);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("rotor"));
        QVERIFY(refused.at(0).at(2).toBool());   // shown on the page: no notice
        QCOMPARE(page.messageTextForTesting(), kUnknownPort);

        // Closed: the refusal is a notice (MainWindow toasts shownOnPage false).
        page.saveButtonForTesting()->click();
        QVERIFY(page.messageTextForTesting().isEmpty());
        page.hide();
        const quint32 second = link.nextId;
        window.reportStationAccessoryRefusal(QStringLiteral("rotor"), kUnknownPort, second);
        window.reportStationCommandFinished(second, false, kUnknownPort);
        QCOMPARE(refused.count(), 2);
        QVERIFY(!refused.at(1).at(2).toBool());
    }

    void remotePresetsGoToTheCore()
    {
        RadioModel window(RadioModel::Role::Remote);
        RecordingRotorLink link;
        window.attachStation(&link);
        window.rotorModel()->setState(coreRotor());
        RotorSetupPage page(&window);
        QTableWidget* table = page.presetsTableForTesting();

        page.addPresetButtonForTesting()->click();
        QCOMPARE(table->rowCount(), 3);
        table->item(2, 0)->setText(QStringLiteral("Long\tpath"));
        table->item(2, 1)->setText(QStringLiteral("360"));
        page.savePresetsButtonForTesting()->click();
        QCOMPARE(link.presets,
                 QStringList({QStringLiteral("Europe\t45\nJapan\t330\nLong path\t0")}));

        // An empty or unreadable heading is refused here, never north.
        table->item(2, 1)->setText(QString());
        table->item(2, 0)->setText(QStringLiteral("Nowhere"));
        page.savePresetsButtonForTesting()->click();
        QCOMPARE(link.presets.size(), 1);
        QCOMPARE(page.messageTextForTesting(), StationRotorController::notANumberReason());
        table->item(2, 1)->setText(QStringLiteral("400"));
        page.savePresetsButtonForTesting()->click();
        QCOMPARE(link.presets.size(), 1);
        QCOMPARE(page.messageTextForTesting(), StationRotorController::outOfRangeReason());

        table->setCurrentCell(2, 0);
        page.removePresetButtonForTesting()->click();
        page.savePresetsButtonForTesting()->click();
        QCOMPARE(link.presets.size(), 2);
        QCOMPARE(link.presets.constLast(), QStringLiteral("Europe\t45\nJapan\t330"));
        QVERIFY(page.messageTextForTesting().isEmpty());
    }

    void coreTooOldGreysEverythingWithTheReason()
    {
        RadioModel window(RadioModel::Role::Remote);
        RecordingRotorLink link;
        link.available = false;
        window.attachStation(&link);
        RotorSetupPage page(&window);
        const QString reason = IStationLink::rotorUnavailableReason();
        QCOMPARE(page.availabilityTextForTesting(), reason);
        for (QGroupBox* box : page.findChildren<QGroupBox*>()) {
            QVERIFY2(!box->isEnabled(), qPrintable(box->title()));
            QVERIFY2(!box->isHidden(), qPrintable(box->title()));
            QCOMPARE(box->toolTip(), reason);
        }
        QVERIFY(!page.saveButtonForTesting()->isEnabled());
        QVERIFY(!page.saveButtonForTesting()->isHidden());
        page.saveButtonForTesting()->click();
        page.savePresetsButtonForTesting()->click();
        QVERIFY(link.configures.isEmpty());
        QVERIFY(link.presets.isEmpty());
        verifyPlain(page);
    }

    void localWindowSetsUpItsOwnRotor()
    {
        RadioModel radio;
        radio.enableStationRotor();
        StationRotorController* controller = radio.stationRotorController();
        QVERIFY(controller != nullptr);
        QStringList ports{QStringLiteral("COM4"), QStringLiteral("COM7")};
        controller->setSerialPortListerForTesting([&ports] { return ports; });
        controller->connection()->setTransportFactoryForTesting([](const RotorTransportTarget&) {
            return std::unique_ptr<RotorTransport>(std::make_unique<SilentTransport>());
        });
        // Look at the ports again (the object does every few seconds).
        radio.rotorModel()->bindController(controller);
        RotorSetupPage page(&radio);
        QCOMPARE(page.availabilityTextForTesting(), QStringLiteral("This computer runs the rotor."));
        QCOMPARE(items(page.serialPortComboForTesting()),
                 QStringList({QStringLiteral("COM4"), QStringLiteral("COM7")}));
        // The contract's defaults with no rotor set up.
        QCOMPARE(page.driverComboForTesting()->currentData().toInt(), 0);
        QCOMPARE(page.endStopComboForTesting()->currentData().toInt(),
                 static_cast<int>(RotorModel::EndStop::North));
        QCOMPARE(page.rangeComboForTesting()->currentData().toInt(), 360);
        QCOMPARE(page.baudComboForTesting()->currentData().toInt(), 9600);
        QVERIFY(!page.axesComboForTesting()->isEnabled());

        selectData(page.driverComboForTesting(), static_cast<int>(RotorModel::Driver::Gs232b));
        page.serialPortComboForTesting()->setCurrentText(QStringLiteral("COM7"));
        selectData(page.endStopComboForTesting(), static_cast<int>(RotorModel::EndStop::South));
        selectData(page.rangeComboForTesting(), 450);
        page.saveButtonForTesting()->click();
        QVERIFY(page.messageTextForTesting().isEmpty());
        QCOMPARE(controller->config().driver, RotorDriver::Gs232b);
        QCOMPARE(controller->config().serialPort, QStringLiteral("COM7"));
        QCOMPARE(controller->config().endStop, RotorRoute::EndStop::South);
        QCOMPARE(controller->config().rangeDeg, 450.0);
        QCOMPARE(radio.rotorModel()->serialPort(), QStringLiteral("COM7"));
        QCOMPARE(page.serialPortComboForTesting()->currentText(), QStringLiteral("COM7"));

        // The port goes away: the rotor's own refusal shows at once.
        ports = QStringList{QStringLiteral("COM4")};
        page.offsetSpinForTesting()->setValue(1.0);
        page.saveButtonForTesting()->click();
        QCOMPARE(page.messageTextForTesting(), kUnknownPort);
        QCOMPARE(controller->config().offsetDeg, 0.0);

        // Presets on this process's rotor.
        page.addPresetButtonForTesting()->click();
        QTableWidget* table = page.presetsTableForTesting();
        table->item(0, 0)->setText(QStringLiteral("Europe"));
        table->item(0, 1)->setText(QStringLiteral("45"));
        page.savePresetsButtonForTesting()->click();
        QCOMPARE(controller->presets(), QStringLiteral("Europe\t45"));

        // No rotor: disconnected and forgotten.
        selectData(page.driverComboForTesting(), static_cast<int>(RotorModel::Driver::None));
        page.saveButtonForTesting()->click();
        QCOMPARE(controller->config().driver, RotorDriver::None);
        QCOMPARE(page.statusTextForTesting(), QStringLiteral("No rotor is set up."));
        verifyPlain(page);
    }

    void localSetupOutsideTheTablesIsRefused()
    {
        RadioModel radio;
        radio.enableStationRotor();
        RotorCommandSink::Setup setup;
        setup.driver = 2;
        setup.serialPort = QStringLiteral("COM4");
        setup.rangeDeg = 400;
        QString reason;
        QVERIFY(!radio.requestConfigureRotor(setup, &reason));
        QCOMPARE(reason, QStringLiteral("That rotor setup is not valid."));
        setup.rangeDeg = 360;
        setup.driver = 4;
        setup.hamlibModel = 0;
        QVERIFY(!radio.requestConfigureRotor(setup, &reason));
        QCOMPARE(reason, QStringLiteral("That rotor setup is not valid."));
        QCOMPARE(radio.lastRotorCommandId(), quint32(0));
    }
};

QTEST_MAIN(TstRotorSetupPage)
#include "tst_rotor_setup_page.moc"
