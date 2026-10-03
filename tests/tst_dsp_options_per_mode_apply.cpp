// no-port-check: NereusSDR-original test — no Thetis source ported here.
// No upstream attribution required (NereusSDR per-mode live-apply test).
// Comments below cite Thetis source paths (radio.cs, etc.) for context
// only — they are reference pointers, not ports.
//
// Tests for Task 4.2: per-mode buffer/filter/filter-type live-apply via
// RxChannel::onModeChanged() and TxChannel::onModeChanged().
//
// Design Section 4B:
//   - Changing a combo whose mode matches the slice's current mode triggers
//     RxChannel::onModeChanged() → rebuild() → dspChangeMeasured signal.
//   - Changing a combo for a different mode only persists to AppSettings;
//     no rebuild is triggered (applies on next mode-switch).
//   - Mode-switch triggers onModeChanged() which reads AppSettings for the
//     new mode and rebuilds if any value differs.
//   - Same-mode-same-settings → no rebuild (idempotent guard).
//
// The tests here work without a live WDSP session (no radio connected).
// The guard paths (m_wdspEngine == nullptr, channel not in map) return 0 or -1
// and are the values we test in the unconnected / no-engine scenarios.
//
// Test structure:
//   Part A — RxChannel::onModeChanged() unit tests (no WDSP).
//   Part B — TxChannel::onModeChanged() unit tests (no WDSP).
//   Part C — RadioModel::rebuildDspOptionsForMode() guard-path tests.
//   Part D - RadioModel::scheduleRemoteDspOptionsApply() (R-R3-21): a
//            remote window's RX write re-runs the mode-change apply for
//            each matching slice, one apply per burst.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/dsp/ChannelConfig.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QList>
#include <QRegularExpression>
#include <QPair>

using namespace NereusSDR;

static constexpr int kTestChannel  = 97;   // Never opened in WDSP
static constexpr int kTestBufSize  = 64;
static constexpr int kTestRate     = 48000;

// ── Helpers to set/clear per-mode AppSettings keys ────────────────────────────

namespace {

void setAppSettingsDefault()
{
    auto& s = AppSettings::instance();
    // Reset all DspOptions keys to known defaults so tests are hermetic.
    //
    // Schema-v5 split: separate Rx/Tx keys per mode (radio.cs:519-574 +
    // 2604-2662 [v2.10.3.13]).  RxChannel::onModeChanged reads keys
    // suffixed `Rx`; TxChannel::onModeChanged reads keys suffixed `Tx`.
    // Earlier revisions of this file used the unsuffixed names (e.g.
    // `DspOptionsBufferSizePhone`) which the code never reads — so the
    // defaults from s.value(..., 64).toInt() were silently applied and
    // the tests' "matching" assertion only "passed" by coincidence of
    // memory layout when the WDSP setters dereferenced ch[97] (UB).
    //
    // Set values to NOT match m_dspBlockSize (4096 RX / 2048 TX) so
    // tests that intentionally trigger a rebuild ("changed_*" tests)
    // still see a mismatch.  "Same settings" tests override on top.
    s.setValue("DspOptionsBufferSizePhoneRx", "256");
    s.setValue("DspOptionsBufferSizeCwRx",    "256");
    s.setValue("DspOptionsBufferSizeDigRx",   "256");
    s.setValue("DspOptionsBufferSizeFmRx",    "256");
    s.setValue("DspOptionsBufferSizePhoneTx", "256");

    s.setValue("DspOptionsFilterSizePhoneRx", "4096");
    s.setValue("DspOptionsFilterSizeCwRx",    "4096");
    s.setValue("DspOptionsFilterSizeDigRx",   "4096");
    s.setValue("DspOptionsFilterSizeFmRx",    "4096");
    s.setValue("DspOptionsFilterSizePhoneTx", "2048");

    s.setValue("DspOptionsFilterTypePhoneRx", "Low Latency");
    s.setValue("DspOptionsFilterTypePhoneTx", "Linear Phase");
    s.setValue("DspOptionsFilterTypeCwRx",    "Low Latency");
    s.setValue("DspOptionsFilterTypeDigRx",   "Linear Phase");
    s.setValue("DspOptionsFilterTypeDigTx",   "Linear Phase");
    s.setValue("DspOptionsFilterTypeFmRx",    "Low Latency");
    s.setValue("DspOptionsFilterTypeFmTx",    "Linear Phase");

    s.setValue("DspOptionsCacheImpulse",                 "False");
    s.setValue("DspOptionsCacheImpulseSaveRestore",      "False");
    s.setValue("DspOptionsHighResFilterCharacteristics", "False");
}

// addSlice on a model with no radio connection logs that the TX frequency
// was not pushed (RadioModel::pushTxFrequencyFromTxSlice). Expected here, so
// Part D tests declare it once per model that gets a slice.
void expectNoConnectionTxPushWarning()
{
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("TX frequency NOT pushed: no connection yet")));
}

}  // namespace

// ── Test class ────────────────────────────────────────────────────────────────

class TestDspOptionsPerModeApply : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        // Reset AppSettings keys to defaults before each test.
        setAppSettingsDefault();
    }

    // ── Part A: RxChannel::onModeChanged() ───────────────────────────────────

    // Without a WdspEngine attached, onModeChanged() must return 0 immediately.
    void rx_no_engine_returns_zero()
    {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        // m_wdspEngine is null by default.
        const qint64 r = ch.onModeChanged(DSPMode::USB);
        QCOMPARE(r, qint64(0));
    }

    // With a WdspEngine attached but the channel not in the engine's map,
    // rebuild() returns -1 (channel not found). onModeChanged propagates -1.
    // Pre-condition: AppSettings buffer != RxChannel default (256 != 64) so
    // the equality guard does NOT skip the rebuild call.
    void rx_engine_attached_channel_not_in_map_returns_minus_one()
    {
        // RxChannel default m_bufferSize = kTestBufSize = 64.
        // AppSettings default for Phone = 256. 256 != 64 → no early-return.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);

        WdspEngine engine;  // uninitialized — kTestChannel never created
        ch.setWdspEngine(&engine);

        const qint64 r = ch.onModeChanged(DSPMode::USB);
        // Channel not in engine map → rebuildRxChannel returns -1.
        QCOMPARE(r, qint64(-1));
    }

    // If the per-mode AppSettings values exactly match the current channel
    // config, onModeChanged() must return 0 (no rebuild).
    //
    // RxChannel defaults: m_dspBlockSize=4096, m_filterSize=4096,
    // m_filterType=0 (LowLatency).  Set AppSettings keys to those values
    // using the schema-v5 `Rx` suffix that RxChannel::onModeChanged
    // actually reads.
    void rx_same_settings_returns_zero_no_rebuild()
    {
        AppSettings::instance().setValue("DspOptionsBufferSizePhoneRx", "4096");
        AppSettings::instance().setValue("DspOptionsFilterSizePhoneRx", "4096");
        // R-IOS-13: a channel opens linear phase (WDSP create_nbp mp 0).
        AppSettings::instance().setValue("DspOptionsFilterTypePhoneRx", "Linear Phase");

        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);

        WdspEngine engine;
        ch.setWdspEngine(&engine);

        const qint64 r = ch.onModeChanged(DSPMode::USB);
        // All three values match channel state → no rebuild → 0.
        QCOMPARE(r, qint64(0));
    }

    // CW mode uses the "Cw" key suffix.  Verify the key-part routing.
    void rx_cw_mode_reads_cw_key_suffix()
    {
        AppSettings::instance().setValue("DspOptionsBufferSizeCwRx",  "4096");
        AppSettings::instance().setValue("DspOptionsFilterSizeCwRx",  "4096");
        AppSettings::instance().setValue("DspOptionsFilterTypeCwRx",  "Linear Phase");

        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        WdspEngine engine;
        ch.setWdspEngine(&engine);

        // All values match channel state → no rebuild → 0.
        QCOMPARE(ch.onModeChanged(DSPMode::CWU), qint64(0));
        QCOMPARE(ch.onModeChanged(DSPMode::CWL), qint64(0));
    }

    // Dig mode uses "Dig" suffix.
    void rx_dig_mode_reads_dig_key_suffix()
    {
        AppSettings::instance().setValue("DspOptionsBufferSizeDigRx", "4096");
        AppSettings::instance().setValue("DspOptionsFilterSizeDigRx", "4096");
        AppSettings::instance().setValue("DspOptionsFilterTypeDigRx", "Linear Phase");

        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        WdspEngine engine;
        ch.setWdspEngine(&engine);

        QCOMPARE(ch.onModeChanged(DSPMode::DIGU), qint64(0));
        QCOMPARE(ch.onModeChanged(DSPMode::DIGL), qint64(0));
    }

    // FM mode uses "Fm" suffix.
    void rx_fm_mode_reads_fm_key_suffix()
    {
        AppSettings::instance().setValue("DspOptionsBufferSizeFmRx",  "4096");
        AppSettings::instance().setValue("DspOptionsFilterSizeFmRx",  "4096");
        AppSettings::instance().setValue("DspOptionsFilterTypeFmRx",  "Linear Phase");

        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        WdspEngine engine;
        ch.setWdspEngine(&engine);

        QCOMPARE(ch.onModeChanged(DSPMode::FM), qint64(0));
    }

    // Changing AppSettings phone filter size to a different value causes
    // onModeChanged to attempt rebuild (returns -1 since channel not in map).
    void rx_changed_filter_size_triggers_rebuild_attempt()
    {
        // 8192 != 4096 (m_filterSize default) → rebuild is attempted.
        // Keep bufferSize / filterType matching defaults so only filterSize
        // differs from channel state.
        AppSettings::instance().setValue("DspOptionsFilterSizePhoneRx",  "8192");
        AppSettings::instance().setValue("DspOptionsBufferSizePhoneRx",  "4096");
        AppSettings::instance().setValue("DspOptionsFilterTypePhoneRx",  "Linear Phase");

        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        WdspEngine engine;
        ch.setWdspEngine(&engine);

        // filterSize 8192 != 4096 → channel-in-map check fires → -1.
        QCOMPARE(ch.onModeChanged(DSPMode::USB), qint64(-1));
    }

    // Changing filter type from the channel's opening Linear Phase (1) to
    // Low Latency (0) triggers a rebuild attempt (R-IOS-13: the cache
    // starts where WDSP opens the channel, linear phase).
    void rx_changed_filter_type_triggers_rebuild_attempt()
    {
        AppSettings::instance().setValue("DspOptionsBufferSizePhoneRx",  "4096");
        AppSettings::instance().setValue("DspOptionsFilterSizePhoneRx",  "4096");
        // "Low Latency" maps to filterType=0; m_filterType starts at 1.
        AppSettings::instance().setValue("DspOptionsFilterTypePhoneRx",  "Low Latency");

        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        WdspEngine engine;
        ch.setWdspEngine(&engine);

        // filterType 0 != 1 → channel-in-map check fires → -1.
        QCOMPARE(ch.onModeChanged(DSPMode::USB), qint64(-1));
    }

    // ── Part B: TxChannel::onModeChanged() ───────────────────────────────────

    // Without a WdspEngine attached, TxChannel::onModeChanged() returns 0.
    void tx_no_engine_returns_zero()
    {
        // TxChannel constructor: (channelId, inputBufferSize, outputBufferSize, parent)
        TxChannel tx(97, 64, 64);
        const qint64 r = tx.onModeChanged(DSPMode::USB);
        QCOMPARE(r, qint64(0));
    }

    // TxChannel: filter size matching kTxDspBufferSize (2048) + Linear
    // Phase (1) → no rebuild (idempotent guard fires).
    // m_txDspBlockSize / m_txFilterSize initialise to
    // WdspEngine::kTxDspBufferSize = 2048; m_txFilterType to 1 (Linear
    // Phase, where WDSP opens the channel; R-IOS-13).  Schema-v5 reads
    // `Tx`-suffixed keys.
    void tx_same_settings_returns_zero()
    {
        AppSettings::instance().setValue("DspOptionsBufferSizePhoneTx", "2048");
        AppSettings::instance().setValue("DspOptionsFilterSizePhoneTx", "2048");
        AppSettings::instance().setValue("DspOptionsFilterTypePhoneTx", "Linear Phase");

        TxChannel tx(97, 64, 64);
        WdspEngine engine;
        tx.setWdspEngine(&engine);

        // bufSize 2048 == 2048, filterSize 2048 == 2048, filterType 1 == 1
        // → no rebuild → 0.
        QCOMPARE(tx.onModeChanged(DSPMode::USB), qint64(0));
    }

    // TxChannel: changed filter size → rebuild attempt → -1 (channel not in map).
    void tx_changed_filter_size_triggers_rebuild_attempt()
    {
        // 4096 != kTxDspBufferSize (2048) → rebuild attempted.
        AppSettings::instance().setValue("DspOptionsBufferSizePhoneTx", "2048");
        AppSettings::instance().setValue("DspOptionsFilterSizePhoneTx", "4096");
        AppSettings::instance().setValue("DspOptionsFilterTypePhoneTx", "Low Latency");

        TxChannel tx(97, 64, 64);
        WdspEngine engine;
        tx.setWdspEngine(&engine);

        QCOMPARE(tx.onModeChanged(DSPMode::USB), qint64(-1));
    }

    // ── Part C: RadioModel::rebuildDspOptionsForMode() guard paths ────────────

    // rebuildDspOptionsForMode() must be a no-op (no crash, no emission)
    // when WDSP is not initialized (radio disconnected).
    void radiomodel_rebuild_noop_when_disconnected()
    {
        RadioModel model;
        QSignalSpy spy(&model, &RadioModel::dspChangeMeasured);

        // Not connected → m_wdspEngine not initialized.
        model.rebuildDspOptionsForMode(DSPMode::USB);

        // No emission expected when unconnected.
        QCOMPARE(spy.count(), 0);
    }

    // ── Part D: RadioModel::scheduleRemoteDspOptionsApply() (R-R3-21) ────────

    // A burst of RX keys for one mode group applies once, to the slice in
    // that group, after the coalescing window and not before.
    void remote_rx_burst_applies_matching_slice_once()
    {
        RadioModel model;
        expectNoConnectionTxPushWarning();
        QList<QPair<int, DSPMode>> applied;
        model.setDspOptionsApplyObserverForTest([&applied](int index, DSPMode mode) {
            applied.append(qMakePair(index, mode));
        });
        SliceModel* phone = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        SliceModel* cw = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(phone != nullptr);
        QVERIFY(cw != nullptr);
        phone->setDspMode(DSPMode::USB);
        cw->setDspMode(DSPMode::CWU);

        model.scheduleRemoteDspOptionsApply(QStringLiteral("DspOptionsBufferSizePhoneRx"));
        model.scheduleRemoteDspOptionsApply(QStringLiteral("DspOptionsFilterSizePhoneRx"));
        model.scheduleRemoteDspOptionsApply(QStringLiteral("DspOptionsFilterTypePhoneRx"));
        QVERIFY(applied.isEmpty());

        QTRY_COMPARE(applied.size(), 1);
        QCOMPARE(applied.first().first, phone->sliceIndex());
        QCOMPARE(applied.first().second, DSPMode::USB);
        QTest::qWait(200);
        QCOMPARE(applied.size(), 1);
    }

    // Keys for two groups in one burst apply once to each matching slice.
    void remote_rx_burst_across_groups_applies_each_slice_once()
    {
        RadioModel model;
        expectNoConnectionTxPushWarning();
        QList<QPair<int, DSPMode>> applied;
        model.setDspOptionsApplyObserverForTest([&applied](int index, DSPMode mode) {
            applied.append(qMakePair(index, mode));
        });
        SliceModel* phone = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        SliceModel* dig = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(phone != nullptr);
        QVERIFY(dig != nullptr);
        phone->setDspMode(DSPMode::USB);
        dig->setDspMode(DSPMode::DIGU);

        model.scheduleRemoteDspOptionsApply(QStringLiteral("DspOptionsBufferSizeDigRx"));
        model.scheduleRemoteDspOptionsApply(QStringLiteral("DspOptionsBufferSizePhoneRx"));
        model.scheduleRemoteDspOptionsApply(QStringLiteral("DspOptionsFilterTypeDigRx"));

        QTRY_COMPARE(applied.size(), 2);
        QTest::qWait(200);
        QCOMPARE(applied.size(), 2);
        QVERIFY(applied.contains(qMakePair(phone->sliceIndex(), DSPMode::USB)));
        QVERIFY(applied.contains(qMakePair(dig->sliceIndex(), DSPMode::DIGU)));
    }

    // TX keys, other DSP > Options keys, unknown groups and unrelated keys
    // apply nothing; neither does an RX key for a group no slice is in.
    void remote_unrelated_keys_apply_nothing()
    {
        RadioModel model;
        expectNoConnectionTxPushWarning();
        int applied = 0;
        model.setDspOptionsApplyObserverForTest([&applied](int, DSPMode) { ++applied; });
        SliceModel* slice = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);

        for (const char* key : {"DspOptionsBufferSizePhoneTx",
                                "DspOptionsFilterTypePhoneTx",
                                "DspOptionsCacheImpulse",
                                "DspOptionsHighResFilterCharacteristics",
                                "DspOptionsBufferSizeAmRx",
                                "DspOptionsRx",
                                "DisplayFftAverage",
                                "DspOptionsBufferSizeFmRx",
                                "DspOptionsFilterSizeCwRx"}) {
            model.scheduleRemoteDspOptionsApply(QString::fromLatin1(key));
        }
        QTest::qWait(200);
        QCOMPARE(applied, 0);
    }

    // Local half: a remote-role model never applies, and a plain local
    // AppSettings write (what DspOptionsPage does before its own
    // rebuildDspOptionsForMode) schedules nothing.
    void remote_apply_is_core_only()
    {
        RadioModel remote(RadioModel::Role::Remote);
        int remoteApplied = 0;
        remote.setDspOptionsApplyObserverForTest([&remoteApplied](int, DSPMode) {
            ++remoteApplied;
        });
        remote.scheduleRemoteDspOptionsApply(QStringLiteral("DspOptionsBufferSizePhoneRx"));

        RadioModel local;
        expectNoConnectionTxPushWarning();
        int localApplied = 0;
        local.setDspOptionsApplyObserverForTest([&localApplied](int, DSPMode) {
            ++localApplied;
        });
        SliceModel* slice = local.sliceById(local.addSlice(QStringLiteral("pan-0")));
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);
        AppSettings::instance().setValue(QStringLiteral("DspOptionsBufferSizePhoneRx"),
                                         QStringLiteral("512"));

        QTest::qWait(200);
        QCOMPARE(remoteApplied, 0);
        QCOMPARE(localApplied, 0);
    }

    // The one mode-group mapping RxChannel, RadioModel's remote apply and
    // DspOptionsPage's live-apply gate share (core/RxChannel.h).
    void mode_group_mapping_is_pinned_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("group");
        const QList<QPair<DSPMode, QString>> rows{
            {DSPMode::USB, QStringLiteral("Phone")}, {DSPMode::LSB, QStringLiteral("Phone")},
            {DSPMode::AM, QStringLiteral("Phone")},  {DSPMode::SAM, QStringLiteral("Phone")},
            {DSPMode::DSB, QStringLiteral("Phone")}, {DSPMode::CWU, QStringLiteral("Cw")},
            {DSPMode::CWL, QStringLiteral("Cw")},    {DSPMode::DIGU, QStringLiteral("Dig")},
            {DSPMode::DIGL, QStringLiteral("Dig")},  {DSPMode::SPEC, QStringLiteral("Dig")},
            {DSPMode::DRM, QStringLiteral("Dig")},   {DSPMode::FM, QStringLiteral("Fm")},
            {DSPMode::RADE_U, QStringLiteral("Phone")},
            {DSPMode::RADE_L, QStringLiteral("Phone")},
        };
        for (const auto& [mode, group] : rows) {
            QTest::newRow(qPrintable(QString::number(static_cast<int>(mode))))
                << static_cast<int>(mode) << group;
        }
    }

    void mode_group_mapping_is_pinned()
    {
        QFETCH(int, mode);
        QFETCH(QString, group);
        QCOMPARE(dspOptionsModeGroup(static_cast<DSPMode>(mode)), group);
    }
};

QTEST_MAIN(TestDspOptionsPerModeApply)
#include "tst_dsp_options_per_mode_apply.moc"
