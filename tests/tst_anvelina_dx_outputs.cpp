// no-port-check: test fixture for the NereusSDR-original AnvelinaDxOutputs
// model; expected bytes come from the Anvelina DX spec (page 2, "Structure of
// Byte 1397") and the gateware (Orion.v:920-923 [@8e86a61]).
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AnvelinaDxOutputs.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/P2RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/codec/CodecContext.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:0D:01");
const QString kOtherMac = QStringLiteral("AA:BB:CC:DD:0D:02");

QString dxKey(const QString& mac, int out, const char* field)
{
    return QStringLiteral("hardware/%1/oc/dx/out%2/%3").arg(mac).arg(out).arg(QLatin1String(field));
}

} // namespace

class TstAnvelinaDxOutputs : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clearHardwareValues(kMac);
        AppSettings::instance().clearHardwareValues(kOtherMac);
    }
    void cleanup()
    {
        AppSettings::instance().clearHardwareValues(kMac);
        AppSettings::instance().clearHardwareValues(kOtherMac);
    }

    void theMaskIsBitsOneToFour()
    {
        QCOMPARE(int(kDxWireMask), 0x1E);
    }

    // Spec page 2, "Value Examples".
    void wireByteFollowsTheSpec()
    {
        AnvelinaDxOutputs dx;
        QCOMPARE(int(dx.wireByte()), 0x00);
        dx.setOutputOn(7, true);
        QCOMPARE(int(dx.wireByte()), 0x02);
        dx.setOutputOn(9, true);
        QCOMPARE(int(dx.wireByte()), 0x0A);
        dx.setAll(true);
        QCOMPARE(int(dx.wireByte()), 0x1E);
        for (int out = 7; out <= 10; ++out) {
            QVERIFY(dx.outputOn(out));
        }
        dx.setAll(false);
        QCOMPARE(int(dx.wireByte()), 0x00);
        dx.setOutputOn(10, true);
        QCOMPARE(int(dx.wireByte()), 0x10);  // USEROUT10, past Thetis's 0x0f
    }

    void outputsOutsideSevenToTenAreIgnored()
    {
        AnvelinaDxOutputs dx;
        QSignalSpy spy(&dx, &AnvelinaDxOutputs::changed);
        dx.setOutputOn(6, true);
        dx.setOutputOn(11, true);
        dx.setLabel(6, QStringLiteral("x"));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(int(dx.wireByte()), 0x00);
        QVERIFY(!dx.outputOn(6));
        QVERIFY(dx.label(11).isEmpty());
    }

    void aChangeIsAnnouncedOnce()
    {
        AnvelinaDxOutputs dx;
        QSignalSpy spy(&dx, &AnvelinaDxOutputs::changed);
        dx.setOutputOn(8, true);
        dx.setOutputOn(8, true);
        QCOMPARE(spy.count(), 1);
        dx.setAll(false);
        dx.setAll(false);
        QCOMPARE(spy.count(), 2);
        dx.setLabel(8, QStringLiteral("ANT Switch"));
        dx.setLabel(8, QStringLiteral("ANT Switch"));
        QCOMPARE(spy.count(), 3);
    }

    void stateAndNamesPersistPerMac()
    {
        {
            AnvelinaDxOutputs dx;
            dx.setMacAddress(kMac);
            dx.setOutputOn(7, true);
            dx.setOutputOn(10, true);
            dx.setLabel(7, QStringLiteral("ANT Switch"));
        }
        QCOMPARE(AppSettings::instance().value(dxKey(kMac, 7, "enabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(AppSettings::instance().value(dxKey(kMac, 10, "enabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(AppSettings::instance().value(dxKey(kMac, 7, "label")).toString(),
                 QStringLiteral("ANT Switch"));
        // Unchanged defaults are not written.
        QVERIFY(!AppSettings::instance().contains(dxKey(kMac, 8, "enabled")));
        QVERIFY(!AppSettings::instance().contains(dxKey(kMac, 8, "label")));

        AnvelinaDxOutputs back;
        back.setMacAddress(kMac);
        back.load();
        QCOMPARE(int(back.wireByte()), 0x12);
        QCOMPARE(back.label(7), QStringLiteral("ANT Switch"));
        QVERIFY(back.label(8).isEmpty());

        // A second radio keeps its own values.
        AnvelinaDxOutputs other;
        other.setMacAddress(kOtherMac);
        other.load();
        QCOMPARE(int(other.wireByte()), 0x00);
        other.setOutputOn(9, true);
        other.setLabel(9, QStringLiteral("Amp"));
        back.load();
        QCOMPARE(int(back.wireByte()), 0x12);
        QCOMPARE(back.label(9), QString());
        AnvelinaDxOutputs otherBack;
        otherBack.setMacAddress(kOtherMac);
        otherBack.load();
        QCOMPARE(int(otherBack.wireByte()), 0x08);
        QCOMPARE(otherBack.label(9), QStringLiteral("Amp"));

        // Switching an output off saves "False".
        back.setOutputOn(7, false);
        QCOMPARE(AppSettings::instance().value(dxKey(kMac, 7, "enabled")).toString(),
                 QStringLiteral("False"));
    }

    // The connection carries the byte to high-priority byte 1397 on an
    // Anvelina PRO3 only.
    void theP2ConnectionSendsTheByteOnAnAnvelinaOnly()
    {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        HardwareProfile profile = conn.hardwareProfile();
        profile.model = HPSDRModel::ANVELINAPRO3;
        conn.setHardwareProfile(profile);
        conn.setOcExtrasByte(0x0A);
        QCOMPARE(int(conn.ocExtrasByte()), 0x0A);
        QByteArray pkt = conn.cmdHighPriorityPacketForTest();
        QCOMPARE(int(quint8(pkt.at(1397))), 0x0A);

        profile.model = HPSDRModel::ANAN7000D;
        conn.setHardwareProfile(profile);
        pkt = conn.cmdHighPriorityPacketForTest();
        QCOMPARE(int(quint8(pkt.at(1397))), 0x00);
    }

    // A remote window's change reaches the Core's outputs at once, keyed or
    // not, while an OC band pin written on the air waits for receive.
    void aWindowsChangeAppliesOnTheCoreEvenOnTheAir()
    {
        RadioModel core;
        core.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        RadioInfo info;
        info.macAddress = kMac;
        core.setLastRadioInfoForTest(info);
        core.setConnectionStateForTest(ConnectionState::Connected);
        QStringList reloads;
        core.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

        core.transmitModel().setTune(true);
        QVERIFY(core.stationOnAirRefusal(nullptr));

        const QString out8 = dxKey(kMac, 8, "enabled");
        AppSettings::instance().setValue(out8, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(out8);
        const QString pin = QStringLiteral("hardware/%1/oc/rx/40m/pin3").arg(kMac);
        AppSettings::instance().setValue(pin, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(pin);

        QTRY_VERIFY(core.anvelinaDxOutputs().outputOn(8));
        QCOMPARE(int(core.anvelinaDxOutputs().wireByte()), 0x04);
        // The band pin is held for receive (flushRemoteHardwareApply); only
        // the DX outputs were reloaded.
        QCOMPARE(reloads, QStringList{QStringLiteral("ocDx")});
        QVERIFY(!core.ocMatrix().pinEnabled(Band::Band40m, 2, /*tx=*/false));
        core.transmitModel().setTune(false);
    }
};

QTEST_MAIN(TstAnvelinaDxOutputs)
#include "tst_anvelina_dx_outputs.moc"
