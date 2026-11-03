// no-port-check: NereusSDR-original regression tests for issue #734.
// Exercises effective defaults and the real coordinator-to-WDSP boundary;
// no radio, audio device, network connection, or settings file is opened.
#include <QtTest>

#include <cmath>
#include <limits>

#include "core/BoardCapabilities.h"
#include "core/PureSignal.h"
#include "core/TxChannel.h"
#include "core/wdsp_api.h"
#include "models/PureSignalSettings.h"

using namespace NereusSDR;

namespace {
constexpr int kChannel = 1;
}

class TstPureSignalHardwarePeak : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
#ifdef HAVE_WDSP
        OpenChannel(kChannel, 64, 1024, 48000, 96000, 48000,
                    1, 0, 0.0, 0.010, 0.0, 0.010, 1);
#else
        QSKIP("requires real WDSP");
#endif
    }

    void cleanupTestCase()
    {
#ifdef HAVE_WDSP
        CloseChannel(kChannel);
#endif
    }

    void everyPureSignalBoardHasEffectiveDefaultsForBothProtocols()
    {
        for (const auto& canonical : BoardCapsTable::all()) {
            if (!canonical.hasPureSignal) {
                continue;
            }
            QVERIFY2(std::isfinite(canonical.psDefaultPeak)
                         && canonical.psDefaultPeak > 0.0,
                     canonical.displayName);
            for (const auto protocol : {ProtocolVersion::Protocol1,
                                        ProtocolVersion::Protocol2}) {
                auto caps = canonical;
                caps.protocol = protocol;
                PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                ps.setTimersEnabled(false);
                ps.applyBoardCapabilities(caps);
                double expected = protocol == ProtocolVersion::Protocol1 ? 0.4072 : 0.2899;
                if (caps.board == HPSDRHW::HermesLite) {
                    expected = 0.233;
                } else if (protocol == ProtocolVersion::Protocol2
                           && (caps.board == HPSDRHW::Saturn
                               || caps.board == HPSDRHW::SaturnMKII)) {
                    expected = 0.6121;
                }
                QVERIFY2(std::isfinite(ps.hwPeak()) && ps.hwPeak() > 0.0,
                         canonical.displayName);
                QCOMPARE(ps.hwPeak(), expected);
            }
        }
    }

    void acceptedSettingsReachNativeEngine_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("protocol");
        QTest::addColumn<double>("expected");
        QTest::newRow("ANAN100-P1") << int(HPSDRHW::Hermes) << 1 << 0.4072;
        QTest::newRow("ANAN200D-P2") << int(HPSDRHW::Orion) << 2 << 0.2899;
        QTest::newRow("HL2-P1") << int(HPSDRHW::HermesLite) << 1 << 0.233;
        QTest::newRow("Saturn-P2") << int(HPSDRHW::Saturn) << 2 << 0.6121;
    }

    void acceptedSettingsReachNativeEngine()
    {
        QFETCH(int, board);
        QFETCH(int, protocol);
        QFETCH(double, expected);
        TxChannel tx(kChannel);
        PureSignalSettings settings;
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setTimersEnabled(false);
        ps.setSettings(&settings);
        const auto& caps = BoardCapsTable::forBoard(static_cast<HPSDRHW>(board));
        ps.applyBoardCapabilities(caps, static_cast<ProtocolVersion>(protocol));
        QVERIFY(ps.applyAcceptedSettingsToEngine());
        QCOMPARE(ps.hwPeak(), expected);
        QCOMPARE(tx.getPSHWPeak(), expected);
    }

    void protocolChangesPreserveExplicitOverride()
    {
        TxChannel tx(kChannel);
        PureSignalSettings settings;
        auto values = settings.values();
        values.hardwarePeakOverrideEnabled = true;
        values.hardwarePeakOverride = 0.53;
        QVERIFY(settings.apply(values));
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setTimersEnabled(false);
        ps.setSettings(&settings);
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Orion);
        for (const auto protocol : {ProtocolVersion::Protocol1, ProtocolVersion::Protocol2}) {
            ps.applyBoardCapabilities(caps, protocol);
            QVERIFY(ps.applyAcceptedSettingsToEngine());
            QCOMPARE(ps.hwPeak(), 0.53);
            QCOMPARE(tx.getPSHWPeak(), 0.53);
            QVERIFY(settings.values() == values);
        }
        ps.setDefaultPeaks();
        QCOMPARE(ps.hwPeak(), 0.2899);
        QCOMPARE(tx.getPSHWPeak(), 0.2899);
        QCOMPARE(settings.hardwarePeakOverride(), 0.53);
    }

    void unsupportedCapabilitiesDoNotApplySettings()
    {
        TxChannel tx(kChannel);
        tx.setPSHWPeak(0.53);
        PureSignalSettings settings;
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setTimersEnabled(false);
        ps.setSettings(&settings);
        ps.applyBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::Unknown));
        QVERIFY(!ps.applyAcceptedSettingsToEngine());
        QCOMPARE(tx.getPSHWPeak(), 0.53);
    }

    void invalidPeakNeverReachesNativeEngine_data()
    {
        QTest::addColumn<double>("peak");
        QTest::newRow("zero") << 0.0;
        QTest::newRow("negative") << -0.25;
        QTest::newRow("NaN") << std::numeric_limits<double>::quiet_NaN();
        QTest::newRow("infinity") << std::numeric_limits<double>::infinity();
    }

    void invalidPeakNeverReachesNativeEngine()
    {
        QFETCH(double, peak);
        TxChannel tx(kChannel);
        tx.setPSHWPeak(0.53);
        tx.setPSHWPeak(peak);
        QCOMPARE(tx.getPSHWPeak(), 0.53);
    }
};

QTEST_GUILESS_MAIN(TstPureSignalHardwarePeak)
#include "tst_puresignal_hardware_peak.moc"
