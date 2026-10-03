// =================================================================
// tests/tst_rfkit_page_master_gate.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No upstream source file ported.
//
// Modification history (NereusSDR):
//   2026-05-24 -- Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-05-26 -- Per-radio peripherals refactor: RfKitPage now drives
//                 RfKit_* via RadioModel::peripheralValue, which writes
//                 under hardware/<mac>/peripherals/.  Tests prime a
//                 fake connection so the page can resolve a MAC scope.
//   2026-09-24 -- R-R3-47 / R-R3-48: in a remote window the page asks the
//                 Core (switch, connect, disconnect) and never this
//                 computer's own connection; the band-follow line, local
//                 and remote. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QPushButton>
#include "gui/setup/RfKitPage.h"
#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"   // RadioInfo
#include "core/RadioConnection.h"  // ConnectionState
#include "core/session/IStationLink.h"
#include "models/RfKitModel.h"
#include "OperatorWording.h"
#include <QLabel>

using namespace NereusSDR;

class RfKitPageMasterGateTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void masterCheckboxReflectsModel();
    void togglingCheckboxFlipsModel();
    void detailTabGreysWhenMasterOff();
    void hostPortInputsPersist();
    void antennaLabelsRoundTrip();
    void testConnectionButtonPresent();
    // R-R3-47 / R-R3-48.
    void remotePageAsksTheCore();
    void remotePageWithoutCoreSupportSaysWhy();
    void bandFollowLineLocalAndRemote();

private:
    static void primeConnectedRadio(RadioModel& m,
                                    const QString& mac =
                                        QStringLiteral("aa:bb:cc:dd:ee:02"));
};

void RfKitPageMasterGateTest::primeConnectedRadio(RadioModel& m,
                                                  const QString& mac)
{
    RadioInfo info;
    info.macAddress = mac;
    m.setLastRadioInfoForTest(info);
    m.setConnectionStateForTest(ConnectionState::Connected);
    m.applyPeripheralsForTest();
}

void RfKitPageMasterGateTest::initTestCase() {
    AppSettings::instance().clear();
}

void RfKitPageMasterGateTest::cleanup() {
    AppSettings::instance().clear();
}

void RfKitPageMasterGateTest::masterCheckboxReflectsModel() {
    RadioModel m;
    primeConnectedRadio(m);
    AppSettings::instance().setHardwareValue(
        m.currentRadioMac(),
        QStringLiteral("peripherals/RfKit_Enabled"),
        QStringLiteral("True"));
    RfKitPage page(&m);
    QVERIFY(page.masterCheckboxForTesting()->isChecked());
}

void RfKitPageMasterGateTest::togglingCheckboxFlipsModel() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    page.masterCheckboxForTesting()->setChecked(true);
    QCOMPARE(m.rfKitEnabled(), true);
    page.masterCheckboxForTesting()->setChecked(false);
    QCOMPARE(m.rfKitEnabled(), false);
}

void RfKitPageMasterGateTest::detailTabGreysWhenMasterOff() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    page.masterCheckboxForTesting()->setChecked(true);
    QVERIFY(page.detailTabIsEnabledForTesting());
    page.masterCheckboxForTesting()->setChecked(false);
    QVERIFY(!page.detailTabIsEnabledForTesting());
}

void RfKitPageMasterGateTest::hostPortInputsPersist() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    page.setHostForTesting("10.0.0.5");
    page.setPortForTesting(8080);
    page.clickSaveForTesting();
    QCOMPARE(AppSettings::instance()
        .hardwareValue(m.currentRadioMac(),
                       QStringLiteral("peripherals/RfKit_ManualIp"))
        .toString(),
        QString("10.0.0.5"));
}

void RfKitPageMasterGateTest::antennaLabelsRoundTrip() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    page.setAntennaLabelForTesting(1, "80m dipole");
    page.setAntennaLabelForTesting(2, "20m beam");
    page.clickSaveForTesting();
    // Antenna labels stay GLOBAL (operator preference, not per-radio).
    QCOMPARE(AppSettings::instance()
        .value(QStringLiteral("RfKit_Ant1_Label")).toString(),
        QString("80m dipole"));
}

void RfKitPageMasterGateTest::testConnectionButtonPresent() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    QVERIFY(page.testConnectionButtonForTesting() != nullptr);
}

namespace {
class RecordingRfKitLink final : public IStationLink {
public:
    bool available{true};
    int switchCalls{0};
    bool switchedOn{false};
    int configureCalls{0};
    int disconnectCalls{0};
    QString host;
    quint16 port{0};
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool stationLinkReady() const override { return true; }
    bool remoteRfKitControlAvailable() const override { return available; }
    CommandOutcome requestRfKitEnabled(bool on) override
    {
        ++switchCalls;
        switchedOn = on;
        return {true, {}};
    }
    CommandOutcome requestConfigureRfKit(const QString& h, quint16 p) override
    {
        ++configureCalls;
        host = h;
        port = p;
        return {true, {}};
    }
    CommandOutcome requestDisconnectRfKit() override
    {
        ++disconnectCalls;
        return {true, {}};
    }
};
} // namespace

void RfKitPageMasterGateTest::remotePageAsksTheCore() {
    RadioModel window(RadioModel::Role::Remote);
    RecordingRfKitLink link;
    window.attachStation(&link);
    TunerModel::StationConnectionState core;
    core.configuredHost = QStringLiteral("192.0.2.41");
    core.configuredPort = 8080;
    core.phase = TunerModel::ConnectionPhase::Disconnected;
    window.rfKitModel()->setStationConnectionState(core);
    RfKitPage page(&window);
    QCheckBox* master = page.masterCheckboxForTesting();
    QVERIFY(master->isEnabled());
    QVERIFY(!master->isChecked());

    // The switch asks the Core; the box follows the Core's answer.
    master->setChecked(true);
    QCOMPARE(link.switchCalls, 1);
    QVERIFY(link.switchedOn);
    QVERIFY(!window.rfKitEnabled());   // nothing changed here
    QVERIFY(window.applyMirroredValue("rfKitEnabled", QVariant(true)).isEmpty());
    QVERIFY(window.rfKitEnabled());
    QVERIFY(master->isChecked());
    QVERIFY(page.detailTabIsEnabledForTesting());

    // Connect sends the address to the Core; Disconnect asks it.
    page.setHostForTesting(QStringLiteral("192.0.2.42"));
    page.setPortForTesting(8081);
    page.testConnectionButtonForTesting()->click();
    QCOMPARE(link.configureCalls, 1);
    QCOMPARE(link.host, QStringLiteral("192.0.2.42"));
    QCOMPARE(link.port, quint16(8081));
    page.disconnectButtonForTesting()->click();
    QCOMPARE(link.disconnectCalls, 1);

    // Nothing was dialled from this computer, and nothing saved here.
    QVERIFY(window.rfKitConnection()->peerAddress().isEmpty());
    QVERIFY(!window.rfKitConnection()->isConnected());

    // Every word the page shows is plain.
    for (QLabel* label : page.findChildren<QLabel*>()) {
        if (!label->text().isEmpty() && label->textFormat() == Qt::PlainText) {
            QVERIFY2(OperatorWording::isPlain(label->text()), qPrintable(label->text()));
        }
    }
    for (QWidget* w : page.findChildren<QWidget*>()) {
        if (!w->toolTip().isEmpty() && !w->isEnabled()) {
            QVERIFY2(OperatorWording::isPlain(w->toolTip()), qPrintable(w->toolTip()));
        }
    }

    // Switching off asks the Core too.
    master->setChecked(false);
    QCOMPARE(link.switchCalls, 2);
    QVERIFY(!link.switchedOn);
}

void RfKitPageMasterGateTest::remotePageWithoutCoreSupportSaysWhy() {
    RadioModel window(RadioModel::Role::Remote);
    RecordingRfKitLink link;
    link.available = false;
    window.attachStation(&link);
    RfKitPage page(&window);
    QVERIFY(!page.masterCheckboxForTesting()->isEnabled());
    QVERIFY(!page.detailTabIsEnabledForTesting());
    bool said = false;
    for (QLabel* label : page.findChildren<QLabel*>()) {
        said = said || label->text()
            == QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app.");
    }
    QVERIFY(said);
    QCOMPARE(link.switchCalls, 0);
}

void RfKitPageMasterGateTest::bandFollowLineLocalAndRemote() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    QCOMPARE(page.bandFollowTextForTesting(), m.rfKitModel()->bandFollowText());
    m.rfKitModel()->setBandFollow(TunerModel::BandFollow::Following, {}, 50001);
    QCOMPARE(page.bandFollowTextForTesting(), QStringLiteral("Band follow: following the radio"));

    RadioModel window(RadioModel::Role::Remote);
    RecordingRfKitLink link;
    window.attachStation(&link);
    RfKitPage remote(&window);
    window.rfKitModel()->applyStationValue("bandFollow", QVariant(1));
    window.rfKitModel()->applyStationValue("bandFollowAddress", QStringLiteral("192.168.1.20"));
    window.rfKitModel()->applyStationValue("bandFollowPort", QVariant(50001));
    QCOMPARE(remote.bandFollowTextForTesting(),
             QStringLiteral("Band follow: enter 192.168.1.20, port 50001 as the TCI server on the "
                            "amplifier."));
    QVERIFY(OperatorWording::isPlain(remote.bandFollowTextForTesting()));
}

QTEST_MAIN(RfKitPageMasterGateTest)
#include "tst_rfkit_page_master_gate.moc"
