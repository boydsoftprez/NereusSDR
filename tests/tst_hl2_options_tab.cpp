// no-port-check: smoke test for Hl2OptionsTab + Hl2OptionsModel UI/state
// wiring.  Phase 3L commit #9.  Cite comments to mi0bot setup.designer.cs
// are documentary only — the ported logic lives in the .cpp where the
// attribution header + PROVENANCE row already cover it.

#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/Hl2OptionsModel.h"
#include "gui/setup/hardware/Hl2OptionsTab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestHl2OptionsTab : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }

    // ── Hl2OptionsModel ─────────────────────────────────────────────────────

    // Defaults match mi0bot setup.designer.cs values verbatim.
    void model_defaults_match_mi0bot_designer_values()
    {
        Hl2OptionsModel m;
        QVERIFY(!m.swapAudioChannels());
        QVERIFY(!m.cl2Enabled());
        QCOMPARE(m.cl2FreqMHz(), Hl2OptionsModel::kDefaultCl2FreqMHz); // 116
        QVERIFY(!m.ext10MHz());
        QVERIFY(!m.disconnectReset());
        QCOMPARE(m.pttHangMs(),  Hl2OptionsModel::kDefaultPttHangMs);   // 12
        QCOMPARE(m.txLatencyMs(), Hl2OptionsModel::kDefaultTxLatencyMs); // 20
        QVERIFY(!m.psSync());
        QVERIFY(!m.bandVolts());
    }

    // Setters fire per-property + changed() signals and clamp to mi0bot ranges.
    void model_setters_clamp_to_mi0bot_ranges()
    {
        Hl2OptionsModel m;

        QSignalSpy changedSpy(&m, &Hl2OptionsModel::changed);

        // CL2 freq above max clamps to 200.
        m.setCl2FreqMHz(500);
        QCOMPARE(m.cl2FreqMHz(), Hl2OptionsModel::kCl2FreqMaxMHz);  // 200
        // Below min clamps to 1.
        m.setCl2FreqMHz(0);
        QCOMPARE(m.cl2FreqMHz(), Hl2OptionsModel::kCl2FreqMinMHz);  // 1

        // PTT hang above max clamps to 30.
        m.setPttHangMs(99);
        QCOMPARE(m.pttHangMs(), Hl2OptionsModel::kPttHangMaxMs);    // 30

        // TX buffer latency above max clamps to 70.
        m.setTxLatencyMs(999);
        QCOMPARE(m.txLatencyMs(), Hl2OptionsModel::kTxLatencyMaxMs); // 70

        // changed() fired at least once per mutation that produced a delta.
        QVERIFY(changedSpy.count() >= 4);
    }

    // No-op setter (same value) does NOT re-emit changed().
    void model_setter_is_idempotent()
    {
        Hl2OptionsModel m;
        m.setPttHangMs(15);
        QSignalSpy spy(&m, &Hl2OptionsModel::changed);
        m.setPttHangMs(15);
        QCOMPARE(spy.count(), 0);
    }

    // Per-MAC AppSettings round-trip.
    void model_per_mac_appsettings_roundtrip()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:ff");

        // Wipe any pre-existing keys for a clean slate.
        auto& s = AppSettings::instance();
        s.setHardwareValue(mac, QStringLiteral("hl2/swapAudioChannels"), QStringLiteral("False"));
        s.setHardwareValue(mac, QStringLiteral("hl2/pttHangMs"),         12);
        s.setHardwareValue(mac, QStringLiteral("hl2/cl2FreqMHz"),        116);

        {
            Hl2OptionsModel writer;
            writer.setMacAddress(mac);
            writer.setSwapAudioChannels(true);
            writer.setPttHangMs(25);
            writer.setCl2FreqMHz(50);
        }

        Hl2OptionsModel reader;
        reader.setMacAddress(mac);
        reader.load();
        QVERIFY(reader.swapAudioChannels());
        QCOMPARE(reader.pttHangMs(), 25);
        QCOMPARE(reader.cl2FreqMHz(), 50);
    }

    // ── Hl2OptionsTab construction ──────────────────────────────────────────

    void tab_construction_does_not_crash()
    {
        RadioModel model;
        Hl2OptionsTab tab(&model);
        QVERIFY(!tab.isVisible());
    }

    // Model → UI sync: changing the model after construction updates the
    // QSpinBox / QCheckBox values on the tab.
    void tab_syncs_from_model()
    {
        RadioModel model;
        Hl2OptionsTab tab(&model);

        model.hl2OptionsMutable().setSwapAudioChannels(true);
        model.hl2OptionsMutable().setPttHangMs(7);
        model.hl2OptionsMutable().setTxLatencyMs(33);

        QVERIFY(tab.swapAudioChannelsCheckedForTest());
        QCOMPARE(tab.pttHangMsForTest(),  7);
        QCOMPARE(tab.txLatencyMsForTest(), 33);
    }

    // I/O Pin State output strip starts blank; receiving a currentOcByteChanged
    // signal from the IoBoardHl2 model updates the visible bits.
    void output_strip_reflects_io_board_oc_byte()
    {
        RadioModel model;
        Hl2OptionsTab tab(&model);
        QCOMPARE(tab.outputBitsForTest(), quint8(0));

        emit model.ioBoardMutable().currentOcByteChanged(
            /*ocByte=*/0x42, /*bandIdx=*/3, /*mox=*/false);
        QCOMPARE(tab.outputBitsForTest(), quint8(0x42));
    }

    // Write button stays disabled until BOTH chkI2CEnable and write-enable
    // gates are checked.  Default state: both unchecked → button disabled.
    void i2c_write_button_default_disabled()
    {
        RadioModel model;
        Hl2OptionsTab tab(&model);
        QVERIFY(!tab.isI2cWriteEnabledForTest());
    }
};

QTEST_MAIN(TestHl2OptionsTab)
#include "tst_hl2_options_tab.moc"
