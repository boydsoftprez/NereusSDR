// tst_hardware_page_capability_gating.cpp
//
// Phase 3I Task 21 — data-driven test asserting that each HPSDRHW board type
// shows the correct set of tabs in HardwarePage.
//
// no-port-check: test fixture exercises NereusSDR HardwarePage; cite
// comments to mi0bot setup.cs:20232 are documentary only — the ported
// logic itself lives in HardwarePage.cpp where the attribution header
// + PROVENANCE row already cover it.

#include <QtTest/QtTest>
#include "core/SampleRateCatalog.h"
#include "gui/setup/HardwarePage.h"
#include "gui/setup/hardware/RadioInfoTab.h"
#include "gui/UnbuiltFeatures.h"
#include "core/HpsdrModel.h"
#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/accessories/AlexController.h"
#include "core/session/StationCapabilities.h"
#include "gui/setup/hardware/AntennaAlexAntennaControlTab.h"
#include "models/Band.h"

#include <QCheckBox>
#include <QRadioButton>
#include <QScopeGuard>
#include <QTabWidget>

using namespace NereusSDR;

namespace {
// R-R3-49: the Diversity tab was removed; no tab of any tab strip on the
// page carries the name, on any board.
bool hasTabTitled(const QWidget& page, const QString& title)
{
    for (const QTabWidget* tabs : page.findChildren<QTabWidget*>()) {
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == title) {
                return true;
            }
        }
    }
    return false;
}
} // namespace

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
        // OC Outputs tab is visible for HL2 (relabeled "Hermes Lite Control")
        // because the I/O board pattern reuses OcMatrix for N2ADR Filter
        // pin assignments — mi0bot setup.cs:20232 [v2.10.3.13-beta2].
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::OcOutputs));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Xvtr));
        // Setup → Hardware → PureSignal tab retired in Phase 3M-4 Task 14;
        // no Tab::PureSignal enum value to assert against any more.
        QVERIFY(!hasTabTitled(page, QStringLiteral("Diversity")));
        // Calibration tab is always visible after IA reshape Phase 5 —
        // PA-specific groups moved to PA → Watt Meter, so the per-board
        // hasPaProfile gate at the parent-tab level was dropped.
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::Calibration));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::Hl2IoBoard));
        // R-R3-49: the Bandwidth Monitor tab is hidden until it is built,
        // even on a board that has the monitor (next case).
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::BandwidthMonitor));
        // HL2 Options tab is a new Phase 3L surface — also gated on
        // caps.hasIoBoardHl2 like the existing HL2 I/O Board tab.
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::Hl2Options));
    }

    // R-R3-49: once the Bandwidth Monitor is built, HL2 shows its tab again
    // (the board gate still applies; Angelia below does not show it).
    void hl2_shows_bandwidth_monitor_once_built()
    {
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::BandwidthMonitor, true);
        const auto reset = qScopeGuard([] { UnbuiltFeatures::resetForTest(); });
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        HardwarePage page(&model);

        RadioInfo info;
        info.boardType  = HPSDRHW::HermesLite;
        info.protocol   = ProtocolVersion::Protocol1;
        info.macAddress = QStringLiteral("aa:bb:cc:11:22:33");
        page.onCurrentRadioChanged(info);

        QVERIFY(page.isTabVisibleForTest(HardwarePage::Tab::BandwidthMonitor));
    }

    // Hermes Lite Control relabel — mi0bot setup.cs:20232 [v2.10.3.13-beta2]
    //   tpPennyCtrl.Text = "Hermes Lite Control";
    void hl2_oc_outputs_tab_relabeled_to_hermes_lite_control()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        HardwarePage page(&model);

        RadioInfo info;
        info.boardType  = HPSDRHW::HermesLite;
        info.protocol   = ProtocolVersion::Protocol1;
        info.macAddress = QStringLiteral("aa:bb:cc:11:22:33");
        page.onCurrentRadioChanged(info);

        QCOMPARE(page.tabTextForTest(HardwarePage::Tab::OcOutputs),
                 QStringLiteral("Hermes Lite Control"));
    }

    // Non-HL2 boards retain the standard "OC Outputs" title.
    void angelia_oc_outputs_tab_keeps_standard_title()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Angelia);
        HardwarePage page(&model);

        RadioInfo info;
        info.boardType  = HPSDRHW::Angelia;
        info.protocol   = ProtocolVersion::Protocol1;
        info.macAddress = QStringLiteral("aa:bb:cc:44:55:66");
        page.onCurrentRadioChanged(info);

        QCOMPARE(page.tabTextForTest(HardwarePage::Tab::OcOutputs),
                 QStringLiteral("OC Outputs"));
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
        // Setup → Hardware → PureSignal tab retired in Phase 3M-4 Task 14;
        // PsForm at Tools > PureSignal is the entire PS control surface.
        QVERIFY(!hasTabTitled(page, QStringLiteral("Diversity")));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::Calibration));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Hl2IoBoard));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::BandwidthMonitor));
        // Non-HL2 boards must NOT see the HL2 Options tab.
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Hl2Options));
    }

    // Plan Task 5: Radio Info shows the top rate for the protocol the radio
    // is running. An ANAN-100D tops out at 192 kHz on Protocol 1 and at
    // 1536 kHz on Protocol 2 (Thetis setup.cs:848-850 [v2.10.3.15]).
    void angelia_radio_info_top_rate_follows_the_protocol()
    {
        for (const auto& [proto, expected] :
             {std::pair{ProtocolVersion::Protocol1, 192000},
              std::pair{ProtocolVersion::Protocol2, 1536000}}) {
            RadioModel model;
            model.setBoardForTest(HPSDRHW::Angelia);
            HardwarePage page(&model);

            RadioInfo info;
            info.boardType  = HPSDRHW::Angelia;
            info.protocol   = proto;
            info.macAddress = QStringLiteral("aa:bb:cc:44:55:66");
            page.onCurrentRadioChanged(info);

            auto* tab = qobject_cast<RadioInfoTab*>(
                page.tabWidgetForTest(HardwarePage::Tab::RadioInfo));
            QVERIFY(tab);
            QVERIFY2(tab->supportInfoForTest().contains(
                         QStringLiteral("Max sample rate: %1 Hz").arg(expected)),
                     qPrintable(tab->supportInfoForTest()));
        }
    }

    // Plan Task 15: the older radios and the HL2 on Protocol 2 top out at
    // Thetis's 1536 kHz (setup.cs:850 [v2.10.3.15]); on Protocol 1 at 192,
    // or 384 for the HL2 and its receive-only kit (mi0bot setup.cs:849-851
    // [v2.10.3.13-beta2]). The kit's Radio Info reads the HL2 model.
    void older_radios_radio_info_top_rate_follows_the_protocol_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("protocol1");
        QTest::addColumn<int>("protocol2");
        QTest::newRow("Atlas")            << int(HPSDRHW::Atlas)            << 192000 << 1536000;
        QTest::newRow("Hermes")           << int(HPSDRHW::Hermes)           << 192000 << 1536000;
        QTest::newRow("HermesII")         << int(HPSDRHW::HermesII)         << 192000 << 1536000;
        QTest::newRow("HermesLite")       << int(HPSDRHW::HermesLite)       << 384000 << 1536000;
        QTest::newRow("HermesLiteRxOnly") << int(HPSDRHW::HermesLiteRxOnly) << 384000 << 1536000;
    }

    void older_radios_radio_info_top_rate_follows_the_protocol()
    {
        QFETCH(int, board);
        QFETCH(int, protocol1);
        QFETCH(int, protocol2);
        const auto hw = static_cast<HPSDRHW>(board);
        for (const auto& [proto, expected] :
             {std::pair{ProtocolVersion::Protocol1, protocol1},
              std::pair{ProtocolVersion::Protocol2, protocol2}}) {
            RadioModel model;
            model.setBoardForTest(hw);
            QCOMPARE(model.boardCapabilities().board, hw);
            HardwarePage page(&model);

            RadioInfo info;
            info.boardType  = hw;
            info.protocol   = proto;
            info.macAddress = QStringLiteral("aa:bb:cc:44:55:67");
            page.onCurrentRadioChanged(info);

            auto* tab = qobject_cast<RadioInfoTab*>(
                page.tabWidgetForTest(HardwarePage::Tab::RadioInfo));
            QVERIFY(tab);
            QVERIFY2(tab->supportInfoForTest().contains(
                         QStringLiteral("Max sample rate: %1 Hz").arg(expected)),
                     qPrintable(tab->supportInfoForTest()));
        }
    }

    // The kit keeps its own row (the transmit block) under the HL2 model.
    void hl2_receive_only_kit_is_an_hl2_that_cannot_transmit()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLiteRxOnly);
        QCOMPARE(model.hardwareProfile().model, HPSDRModel::HERMESLITE);
        QCOMPARE(model.boardCapabilities().board, HPSDRHW::HermesLiteRxOnly);
        QVERIFY(model.boardCapabilities().isRxOnlySku);
    }

    // Plan Task 15: a remote window on a Core running the kit (or an HL2 on
    // Protocol 2) offers the Core's rates: the same profile, row and list a
    // local window builds.
    void remote_window_offers_the_cores_rates_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("coreModel");
        QTest::addColumn<int>("protocol");
        QTest::newRow("kit P1") << int(HPSDRHW::HermesLiteRxOnly) << int(HPSDRModel::HERMESLITE) << 1;
        QTest::newRow("kit P1, no model") << int(HPSDRHW::HermesLiteRxOnly) << int(HPSDRModel::FIRST) << 1;
        QTest::newRow("HL2 P2") << int(HPSDRHW::HermesLite) << int(HPSDRModel::HERMESLITE) << 2;
        QTest::newRow("Hermes P2") << int(HPSDRHW::Hermes) << int(HPSDRModel::HERMES) << 2;
    }

    void remote_window_offers_the_cores_rates()
    {
        QFETCH(int, board);
        QFETCH(int, coreModel);
        QFETCH(int, protocol);
        const auto hw = static_cast<HPSDRHW>(board);
        const auto proto = protocol == 2 ? ProtocolVersion::Protocol2 : ProtocolVersion::Protocol1;

        RadioModel local;
        local.setBoardForTest(hw);

        RadioModel remote(RadioModel::Role::Remote);
        StationCapabilities caps;
        caps.macAddress = QStringLiteral("AA:BB:CC:DD:EE:15");
        caps.board = hw;
        caps.radioConnected = true;
        caps.radioIdentityEntries = true;
        caps.hpsdrModel = static_cast<HPSDRModel>(coreModel);
        caps.radioProtocol = protocol;
        remote.applyStationCapabilities(caps);

        QCOMPARE(remote.hardwareProfile().model, local.hardwareProfile().model);
        QCOMPARE(&remote.boardCapabilities(), &local.boardCapabilities());
        QCOMPARE(allowedSampleRates(proto, remote.boardCapabilities(),
                                    remote.hardwareProfile().model),
                 allowedSampleRates(proto, local.boardCapabilities(),
                                    local.hardwareProfile().model));
        QCOMPARE(remote.boardCapabilities().isRxOnlySku, hw == HPSDRHW::HermesLiteRxOnly);
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
        // Setup → Hardware → PureSignal tab retired in Phase 3M-4 Task 14.
        QVERIFY(!hasTabTitled(page, QStringLiteral("Diversity")));
        // Calibration tab is always visible after IA reshape Phase 5 —
        // PA-specific groups moved to PA → Watt Meter, so the per-board
        // hasPaProfile gate at the parent-tab level was dropped. Atlas
        // shows the freq/level/HPSDR/TX-display/Volts-Amps Cal groups now.
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::Calibration));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Hl2Options));
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
        // Setup → Hardware → PureSignal tab retired in Phase 3M-4 Task 14.
        QVERIFY(!hasTabTitled(page, QStringLiteral("Diversity")));
        QVERIFY( page.isTabVisibleForTest(HardwarePage::Tab::Calibration));
        QVERIFY(!page.isTabVisibleForTest(HardwarePage::Tab::Hl2IoBoard));
    }

    // R-R3-46: in a remote window the Antenna Control grid shows the
    // Core's antennas (its `alexAntennas` object) and a receive edit goes
    // to that object, never to the window's own AlexController. A refused
    // edit shows the Core's value again.
    void remote_antenna_control_shows_and_writes_the_cores_antennas()
    {
        RadioModel remote(RadioModel::Role::Remote);
        StationCapabilities caps;
        caps.macAddress = QStringLiteral("AA:BB:CC:DD:EE:51");
        caps.board = HPSDRHW::Angelia;
        caps.radioConnected = true;
        caps.radioIdentityEntries = true;
        caps.hpsdrModel = HPSDRModel::ANAN100D;
        caps.radioProtocol = 1;
        remote.applyStationCapabilities(caps);

        AlexAntennaFacade* core = remote.alexAntennaFacade();
        QVERIFY(!core->isBound());
        core->setRxAntennas(QStringLiteral("1,1,1,2,1,1,1,1,1,1,1,1,1,1"));  // 40m on 2
        QVERIFY(core->applyRemoteProperty("txAntennas",
                                          QStringLiteral("1,1,1,1,1,3,1,1,1,1,1,1,1,1")));
        QVERIFY(core->applyRemoteProperty("blockTxAnt2", true));
        core->setWindowAvailability(true, {});  // the Core offers Hardware Config

        HardwarePage page(&remote);
        auto* ctl = page.findChild<AntennaAlexAntennaControlTab*>();
        QVERIFY(ctl != nullptr);
        QVERIFY(ctl->rxButtonForTest(Band::Band40m, 2)->isChecked());
        QVERIFY(ctl->txButtonForTest(Band::Band20m, 3)->isChecked());
        QVERIFY(ctl->blockTxAnt2ForTest()->isChecked());

        ctl->rxButtonForTest(Band::Band40m, 3)->click();
        QCOMPARE(core->rxAnt(Band::Band40m), 3);
        QCOMPARE(remote.alexController().rxAnt(Band::Band40m), 1);  // not the window's own

        core->setRxAntennas(QStringLiteral("1,2,1,3,1,1,1,1,1,1,1,1,1,1"));  // from the Core
        QVERIFY(ctl->rxButtonForTest(Band::Band80m, 2)->isChecked());

        core->setEditGate([](QString* reason) {
            if (reason) { *reason = QStringLiteral("Connect to the Core to change the radio's hardware settings."); }
            return false;
        });
        ctl->rxButtonForTest(Band::Band40m, 1)->click();
        QCOMPARE(core->rxAnt(Band::Band40m), 3);
        QVERIFY(ctl->rxButtonForTest(Band::Band40m, 3)->isChecked());
        ctl->useTxAntForRxForTest()->click();
        QVERIFY(!core->useTxAntennaForRx());
        QVERIFY(!ctl->useTxAntForRxForTest()->isChecked());
    }
};

QTEST_MAIN(TestHardwarePageGating)
#include "tst_hardware_page_capability_gating.moc"
