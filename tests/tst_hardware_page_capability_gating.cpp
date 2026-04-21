// tst_hardware_page_capability_gating.cpp
//
// Phase 3I Task 21 — data-driven test asserting that each HPSDRHW board type
// shows the correct set of tabs in HardwarePage.

#include <QtTest/QtTest>
#include "gui/setup/HardwarePage.h"
#include "core/HpsdrModel.h"
#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestHardwarePageGating : public QObject {
    Q_OBJECT
private slots:
    // HardwarePage::onCurrentRadioChanged derives capability gating from
    // m_model->hardwareProfile().caps (Phase 3I-RP). In production
    // RadioModel::connectToRadio seeds the hardware profile via
    // profileForModel(info.modelOverride | defaultModelForBoard(info))
    // before emitting currentRadioChanged, so caps is live by the time
    // HardwarePage responds. Tests that drive onCurrentRadioChanged
    // directly must prime the profile via setBoardForTest, or the
    // dereference at HardwarePage.cpp:198 (*caps) segfaults on the
    // default-constructed HardwareProfile whose caps pointer is null.

    void hl2_hides_alex_pa_diversity()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        HardwarePage page(&model);

        RadioInfo info;
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        info.firmwareVersion = 72;
        page.onCurrentRadioChanged(info);

        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::RadioInfo));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::AntennaAlex));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::OcOutputs));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Xvtr));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::PureSignal));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Diversity));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::PaCalibration));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::Hl2IoBoard));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::BandwidthMonitor));
    }

    void angelia_shows_alex_pa_diversity()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Angelia);
        HardwarePage page(&model);

        RadioInfo info;
        info.boardType  = HPSDRHW::Angelia;
        info.protocol   = ProtocolVersion::Protocol1;
        info.macAddress = QStringLiteral("aa:bb:cc:44:55:66");
        page.onCurrentRadioChanged(info);

        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::AntennaAlex));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::OcOutputs));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::PureSignal));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::Diversity));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::PaCalibration));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Hl2IoBoard));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::BandwidthMonitor));
    }

    void atlas_shows_only_radio_info()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Atlas);
        HardwarePage page(&model);

        RadioInfo info;
        info.boardType = HPSDRHW::Atlas;
        info.protocol  = ProtocolVersion::Protocol1;
        page.onCurrentRadioChanged(info);

        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::RadioInfo));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::AntennaAlex));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::OcOutputs));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Xvtr));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::PureSignal));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Diversity));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::PaCalibration));
    }

    void saturn_shows_nearly_all_tabs()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        HardwarePage page(&model);

        RadioInfo info;
        info.boardType = HPSDRHW::Saturn;
        info.protocol  = ProtocolVersion::Protocol2;
        page.onCurrentRadioChanged(info);

        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::AntennaAlex));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::OcOutputs));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::PureSignal));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::Diversity));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::PaCalibration));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Hl2IoBoard));
    }
};

QTEST_MAIN(TestHardwarePageGating)
#include "tst_hardware_page_capability_gating.moc"
