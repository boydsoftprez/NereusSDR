// tst_hardware_page_persistence.cpp
//
// Phase 3I Task 21 — persistence round-trip tests for AppSettings hardware
// value API.
//
// no-port-check: test fixture exercises NereusSDR HardwarePage persistence.

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/session/StationCapabilities.h"
#include "core/settings/SettingsProxy.h"
#include "gui/setup/HardwarePage.h"
#include "gui/setup/hardware/OcOutputsHfTab.h"
#include "models/Band.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestHardwarePagePersistence : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void writeReadRoundTrip()
    {
        AppSettings s(m_dir.filePath(QStringLiteral("hw1.xml")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"),        192000);
        s.setHardwareValue(mac, QStringLiteral("antennaAlex/rxAnt[0]"),        QStringLiteral("ANT2"));
        s.setHardwareValue(mac, QStringLiteral("pureSignal/enabled"),          false);
        s.save();

        AppSettings s2(m_dir.filePath(QStringLiteral("hw1.xml")));
        s2.load();
        QCOMPARE(s2.hardwareValue(mac, QStringLiteral("radioInfo/sampleRate")).toInt(), 192000);
        QCOMPARE(s2.hardwareValue(mac, QStringLiteral("antennaAlex/rxAnt[0]")).toString(),
                 QStringLiteral("ANT2"));
        QCOMPARE(s2.hardwareValue(mac, QStringLiteral("pureSignal/enabled")).toBool(), false);
    }

    void radioSwapPreservesEachRadiosValues()
    {
        AppSettings s(m_dir.filePath(QStringLiteral("hw2.xml")));
        const QString hl2 = QStringLiteral("aa:bb:cc:11:22:33");
        const QString g2  = QStringLiteral("aa:bb:cc:44:55:66");

        // Write HL2 values
        s.setHardwareValue(hl2, QStringLiteral("radioInfo/sampleRate"),           48000);
        s.setHardwareValue(hl2, QStringLiteral("bandwidthMonitor/thresholdMbps"), 80);

        // Write G2 values
        s.setHardwareValue(g2,  QStringLiteral("radioInfo/sampleRate"),   384000);
        s.setHardwareValue(g2,  QStringLiteral("diversity/phase"),        45);

        s.save();

        AppSettings s2(m_dir.filePath(QStringLiteral("hw2.xml")));
        s2.load();

        // HL2 values intact
        QCOMPARE(s2.hardwareValue(hl2, QStringLiteral("radioInfo/sampleRate")).toInt(), 48000);
        QCOMPARE(s2.hardwareValue(hl2, QStringLiteral("bandwidthMonitor/thresholdMbps")).toInt(), 80);

        // G2 values intact
        QCOMPARE(s2.hardwareValue(g2, QStringLiteral("radioInfo/sampleRate")).toInt(), 384000);
        QCOMPARE(s2.hardwareValue(g2, QStringLiteral("diversity/phase")).toInt(), 45);

        // Cross-pollution check: HL2 should not see G2's diversity/phase
        QVERIFY(!s2.hardwareValue(hl2, QStringLiteral("diversity/phase")).isValid());
    }

    void hardwareValuesReturnsNamespacedMap()
    {
        AppSettings s(m_dir.filePath(QStringLiteral("hw3.xml")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"),      192000);
        s.setHardwareValue(mac, QStringLiteral("ocOutputs/txMask[20m][0]"),  true);
        s.save();

        auto all = s.hardwareValues(mac);
        QVERIFY(all.contains(QStringLiteral("radioInfo/sampleRate")));
        QVERIFY(all.contains(QStringLiteral("ocOutputs/txMask[20m][0]")));
        QCOMPARE(all.size(), 2);
    }

    void activeRxCountRoundTrip()
    {
        AppSettings s(m_dir.filePath(QStringLiteral("hw4.xml")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/activeRxCount"), 3);
        s.save();

        AppSettings s2(m_dir.filePath(QStringLiteral("hw4.xml")));
        s2.load();
        QCOMPARE(s2.hardwareValue(mac, QStringLiteral("radioInfo/activeRxCount")).toInt(), 3);
    }

    // R-R3-46: in a remote window Hardware Config shows the Core's radio's
    // saved values and writes an edit through to the Core's settings for
    // that radio (the Core then applies it). Building the page writes
    // nothing; against a Core that does not offer Hardware Config an edit
    // is dropped; an OC pin click writes the one key that changed.
    void remoteEditsWriteThroughToTheCoresRadio()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        const auto restore = qScopeGuard([] { AppSettings::instance().setRemoteBackend(nullptr); });
        RadioModel remote(RadioModel::Role::Remote);
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:50");
        StationCapabilities caps;
        caps.macAddress = mac;
        caps.board = HPSDRHW::Angelia;
        caps.radioConnected = true;
        caps.radioIdentityEntries = true;
        caps.hpsdrModel = HPSDRModel::ANAN100D;
        caps.radioProtocol = 1;
        remote.applyStationCapabilities(caps);
        proxy.applySnapshot({
            {QLatin1String(AppSettings::kDaemonProfileSeededKey), QStringLiteral("True")},
            {QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(mac), QStringLiteral("96000")},
            {QStringLiteral("hardware/%1/oc/rx/40m/pin3").arg(mac), QStringLiteral("True")},
            {QStringLiteral("hardware/%1/cal/freqFactor").arg(mac), QStringLiteral("1.000001")},
        });
        proxy.setReady(true);
        AlexAntennaFacade* alex = remote.alexAntennaFacade();
        alex->setWindowAvailability(false, QStringLiteral("Connect to the Core to change the "
                                                          "radio's hardware settings."));
        QSignalSpy writes(&proxy, &SettingsProxy::outboundWriteRequested);

        HardwarePage page(&remote);
        QCOMPARE(writes.size(), 0);

        // The Core's values.
        QComboBox* rate = nullptr;
        for (QComboBox* combo : page.tabWidgetForTest(HardwarePage::Tab::RadioInfo)
                                    ->findChildren<QComboBox*>()) {
            if (combo->findData(96000) >= 0 && combo->findData(48000) >= 0) { rate = combo; break; }
        }
        QVERIFY(rate != nullptr);
        QCOMPARE(rate->currentData().toInt(), 96000);
        auto* hf = page.findChild<OcOutputsHfTab*>();
        QVERIFY(hf != nullptr);
        QVERIFY(hf->rxPinCheckedForTest(static_cast<int>(Band::Band40m), 2));
        bool factorShown = false;
        for (QDoubleSpinBox* spin : page.tabWidgetForTest(HardwarePage::Tab::Calibration)
                                        ->findChildren<QDoubleSpinBox*>()) {
            factorShown = factorShown || (spin->decimals() == 9
                                          && qFuzzyCompare(spin->value(), 1.000001));
        }
        QVERIFY(factorShown);

        // Against a Core that does not offer it, the tabs are disabled and
        // an edit that gets through anyway is dropped.
        QVERIFY(!rate->isEnabled());
        rate->setCurrentIndex(rate->findData(192000));
        QCOMPARE(writes.size(), 0);

        alex->setWindowAvailability(true, {});
        rate->setCurrentIndex(rate->findData(48000));
        QCOMPARE(writes.size(), 1);
        QCOMPARE(writes.last().at(0).toString(),
                 QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(mac));
        QCOMPARE(writes.last().at(1).toInt(), 48000);

        // One OC receive pin: the one key that changed.
        QCheckBox* pin = nullptr;
        for (QCheckBox* box : hf->findChildren<QCheckBox*>()) {
            if (box->toolTip() == QStringLiteral("RX OC pin 4, band 20m")) { pin = box; }
        }
        QVERIFY(pin != nullptr);
        writes.clear();
        pin->click();
        QCOMPARE(writes.size(), 1);
        QCOMPARE(writes.last().at(0).toString(),
                 QStringLiteral("hardware/%1/oc/rx/20m/pin4").arg(mac));
        QCOMPARE(writes.last().at(1).toString(), QStringLiteral("True"));
    }
};

QTEST_MAIN(TestHardwarePagePersistence)
#include "tst_hardware_page_persistence.moc"
