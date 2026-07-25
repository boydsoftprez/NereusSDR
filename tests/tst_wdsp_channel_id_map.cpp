// =================================================================
// tests/tst_wdsp_channel_id_map.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F: pins the WDSP channel-id allocation table.
//
// WDSP keeps ONE global channel array, `struct _ch ch[MAX_CHANNELS]`
// (third_party/wdsp/src/channel.c:29).  OpenChannel overwrites
// `ch[channel]` and runs build_channel -> post_main_build ->
// start_thread WITHOUT closing the previous occupant
// (channel.c:75-101, :60-66, :32-35).  Two subsystems that pick the
// same id therefore each get a wdspmain thread on the same slot, both
// dispatch, and the first one's iobuffs / semaphores / critical
// sections leak.  Nothing in WDSP diagnoses it.
//
// The bug this file guards against shipped: the Sub-Epic I channel
// pool ran `for (ch = 1; ch < maxSlices; ++ch) createRxChannel(ch)`
// while the TX channel was opened at a literal 1.  On any SKU with
// maxSlices > 1 (every SKU except HL2) channel 1 was opened as an RXA
// and then overwritten by a TXA a few hundred lines later in the same
// connect.
//
// Numbering is upstream's, not ours.  From Thetis
// ChannelMaster/cmsetup.c:176-191 [v2.10.3.15] — chid():
//   case 0 (rx):  ch_id = pcm->cmSubRCVR * stream + subrx;
//   case 1 (tx):  ch_id = stream + (pcm->cmSubRCVR - 1) * pcm->cmRCVR;
// C# mirror, From Thetis dsp.cs:926-944 [v2.10.3.15] — WDSP.id():
//   case 2:       return cmaster.CMsubrcvr * cmaster.CMrcvr;  // txa
// with cmRCVR = 5 and cmSubRCVR = 2 (From Thetis cmaster.cs:412,419
// [v2.10.3.15]), cmXMTR = 1 (cmaster.cs:502).
//
// NereusSDR's structure is one channel per slice and no sub-receivers:
// cmSubRCVR = 1, cmRCVR = kMaxSliceChannels, cmXMTR = 1.  The same
// formula then yields ch_id = slice for RX (the Phase 3F "channel id
// == slice index" invariant, unchanged) and ch_id = kMaxSliceChannels
// for TX.
// =================================================================
#include <QtTest/QtTest>

#include "core/BoardCapabilities.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestWdspChannelIdMap : public QObject {
    Q_OBJECT
private slots:

    // ── The formula ────────────────────────────────────────────────────
    //
    // chid() case 1 with cmSubRCVR = 1, cmRCVR = kMaxSliceChannels,
    // stream = cmRCVR (the first transmitter stream, cmsetup.c:64-66
    // inid(1, 0) = 0 + cmRCVR):
    //   ch_id = cmRCVR + (1 - 1) * cmRCVR = kMaxSliceChannels
    // which is also dsp.cs's `CMsubrcvr * CMrcvr`.
    void tx_channel_id_is_the_thetis_chid_formula()
    {
        constexpr int kSubRcvrPerSlice = 1;   // NereusSDR cmSubRCVR
        QCOMPARE(WdspEngine::kTxChannelId,
                 kSubRcvrPerSlice * WdspEngine::kMaxSliceChannels);
    }

    // The RX block is [kFirstSliceChannelId, kMaxSliceChannels) and TX is
    // the next id up, so no slice index can ever equal the TX id.
    void tx_channel_sits_immediately_above_the_rx_block()
    {
        QCOMPARE(WdspEngine::kFirstSliceChannelId, 0);
        QVERIFY(WdspEngine::kTxChannelId >= WdspEngine::kMaxSliceChannels);
        // The highest slice index the pool can ever hand out.
        const int topSliceChannel = WdspEngine::kMaxSliceChannels - 1;
        QVERIFY(topSliceChannel < WdspEngine::kTxChannelId);
    }

    // PS feedback is a NereusSDR extension (upstream runs PureSignal
    // feedback inside the TX channel), and upstream numbers its "special"
    // streams after the transmitters — From Thetis
    // ChannelMaster/cmsetup.c:86-89 [v2.10.3.15]:
    //   sp0id(stream) = stream - pcm->cmRCVR - pcm->cmXMTR
    // so it goes above TX, not into the RX block.
    void ps_feedback_channel_sits_above_the_tx_channel()
    {
        QVERIFY(WdspEngine::kPsFeedbackChannelId > WdspEngine::kTxChannelId);
        QVERIFY(WdspEngine::kPsFeedbackChannelId >= WdspEngine::kMaxSliceChannels);
    }

    // ── The ceiling holds for every SKU ────────────────────────────────
    //
    // kMaxSliceChannels is what makes kTxChannelId a compile-time
    // constant rather than something that moves per radio (upstream gets
    // the same property from cmRCVR being a compile-time maximum).  That
    // only works while no SKU asks for more slices than the block holds.
    void no_sku_asks_for_more_slices_than_the_reserved_block()
    {
        for (const auto& caps : BoardCapsTable::all()) {
            QVERIFY2(caps.maxSlices <= WdspEngine::kMaxSliceChannels,
                     qPrintable(QStringLiteral("%1: maxSlices %2 exceeds the "
                                               "reserved RX block of %3; raising "
                                               "it must also raise "
                                               "WdspEngine::kMaxSliceChannels, "
                                               "which moves kTxChannelId and "
                                               "kPsFeedbackChannelId")
                                    .arg(QString::fromLatin1(caps.displayName))
                                    .arg(caps.maxSlices)
                                    .arg(WdspEngine::kMaxSliceChannels)));
        }
    }

    // ── The pool, behaviourally ────────────────────────────────────────
    //
    // Drives the production path (RadioModel::openRxChannelPool, the same
    // call connectToRadio's WDSP-init lambda makes) and asks the engine
    // which ids actually got opened.
    void the_pool_opens_the_rx_block_and_nothing_else()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        const int rateHz = 192000;
        model.openRxChannelPool(WdspEngine::kMaxSliceChannels,
                                bufferSizeForRate(rateHz), rateHz);

        for (int ch = 0; ch < WdspEngine::kMaxSliceChannels; ++ch) {
            QVERIFY2(engine->rxChannel(ch) != nullptr,
                     qPrintable(QStringLiteral("pool did not open slice channel %1")
                                    .arg(ch)));
        }

        // The whole point: the TX slot is untouched, so the TXA that
        // createTxChannel opens later has ch[kTxChannelId] to itself.
        QVERIFY(engine->rxChannel(WdspEngine::kTxChannelId) == nullptr);
        QVERIFY(engine->rxChannel(WdspEngine::kPsFeedbackChannelId) == nullptr);
    }

    // A single-slice SKU (HL2, maxSlices = 1) opens exactly one channel.
    void a_single_slice_sku_opens_only_channel_zero()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        const int rateHz = 192000;
        model.openRxChannelPool(1, bufferSizeForRate(rateHz), rateHz);

        QVERIFY(engine->rxChannel(0) != nullptr);
        for (int ch = 1; ch <= WdspEngine::kPsFeedbackChannelId; ++ch) {
            QVERIFY2(engine->rxChannel(ch) == nullptr,
                     qPrintable(QStringLiteral("single-slice pool opened channel %1")
                                    .arg(ch)));
        }
    }

    // A future SKU (or a caps-table typo) asking for more slices than the
    // reserved block gets clamped rather than allowed to walk into the TX
    // and PS slots.  Losing a slice is recoverable; silently stealing the
    // TXA's channel id is not.
    void an_oversized_pool_request_is_clamped_off_the_tx_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        const int rateHz = 192000;
        model.openRxChannelPool(WdspEngine::kPsFeedbackChannelId + 4,
                                bufferSizeForRate(rateHz), rateHz);

        QVERIFY(engine->rxChannel(WdspEngine::kMaxSliceChannels - 1) != nullptr);
        QVERIFY(engine->rxChannel(WdspEngine::kTxChannelId) == nullptr);
        QVERIFY(engine->rxChannel(WdspEngine::kPsFeedbackChannelId) == nullptr);
    }

    // Idempotent: a reconnect re-runs the pool open against channels that
    // already exist and must not re-OpenChannel them (which would orphan
    // the running thread exactly the way the id collision did).
    void reopening_the_pool_reuses_the_existing_channels()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        const int rateHz = 192000;
        model.openRxChannelPool(WdspEngine::kMaxSliceChannels,
                                bufferSizeForRate(rateHz), rateHz);

        QVector<RxChannel*> first;
        for (int ch = 0; ch < WdspEngine::kMaxSliceChannels; ++ch) {
            first.append(engine->rxChannel(ch));
        }

        model.openRxChannelPool(WdspEngine::kMaxSliceChannels,
                                bufferSizeForRate(rateHz), rateHz);

        for (int ch = 0; ch < WdspEngine::kMaxSliceChannels; ++ch) {
            QCOMPARE(engine->rxChannel(ch), first.at(ch));
        }
    }
};

QTEST_MAIN(TestWdspChannelIdMap)
#include "tst_wdsp_channel_id_map.moc"
