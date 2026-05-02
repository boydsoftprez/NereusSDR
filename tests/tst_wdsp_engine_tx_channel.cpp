// =================================================================
// tests/tst_wdsp_engine_tx_channel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/ChannelMaster/cmaster.c, original licence below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-25 — New test for Phase 3M-1a Task C.1: WdspEngine TX channel
//                 API (createTxChannel / destroyTxChannel / txChannel).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

/*  cmaster.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014-2019 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

// no-port-check: Thetis-derived constants (kTxChannelType, kTxBlockOnOutput,
// kTxTSlewUpSecs, kTxTSlewDownSecs, kTxDspSampleRate, kTxDspBufferSize) are
// already ported with full attribution in WdspEngine.h; this test file only
// asserts that those values match the upstream cmaster.c:177-190 [v2.10.3.13]
// values and verifies the channel API behaves correctly. The Thetis-derived
// logic is tested via the constants, not duplicated here.

#include <QtTest/QtTest>

#include "core/TxChannel.h"
#include "core/WdspEngine.h"

using namespace NereusSDR;

// ---------------------------------------------------------------------------
// Channel ID convention (Thetis cmaster.c / dsp.cs [v2.10.3.13]):
//   RX channel 0: WDSP.id(0, 0) = 0 (rx0, subrx0)
//   TX channel  : WDSP.id(1, 0) = CMsubrcvr * CMrcvr = 1 * 1 = 1
//     (dsp.cs:926-944 case 2, with NereusSDR CMsubrcvr=CMrcvr=1)
// ---------------------------------------------------------------------------
static constexpr int kRxChannelId = 0;
static constexpr int kTxChannelId = 1;   // WDSP.id(1, 0) with single-RX layout

// ---------------------------------------------------------------------------

class TestWdspEngineTxChannel : public QObject {
    Q_OBJECT

private slots:

    // ── TX channel constants match Thetis cmaster.c:177-190 [v2.10.3.13] ─────

    void txChannelTypeIsOne() {
        // From cmaster.c:184  — channel type 1 = TX (RX = 0)
        QCOMPARE(WdspEngine::kTxChannelType, 1);
    }

    void txBlockOnOutputIsOne() {
        // From cmaster.c:190  — bfo=1: block until output available for TX
        QCOMPARE(WdspEngine::kTxBlockOnOutput, 1);
    }

    void txTSlewUpIsPointZeroOne() {
        // From cmaster.c:187  — tslewup = 0.010 s (10 ms channel-level envelope)
        QCOMPARE(WdspEngine::kTxTSlewUpSecs, 0.010);
    }

    void txTSlewDownIsPointZeroOne() {
        // From cmaster.c:189  — tslewdown = 0.010 s (10 ms channel-level envelope)
        QCOMPARE(WdspEngine::kTxTSlewDownSecs, 0.010);
    }

    void txDspSampleRateIs96000() {
        // From cmaster.c:182  — dsp sample rate = 96000 Hz for TX
        QCOMPARE(WdspEngine::kTxDspSampleRate, 96000);
    }

    void txDspBufferSizeIs2048() {
        // 3M-1a r2-ring fix (2026-04-27): NereusSDR uses dsp_size=2048
        // (deskhpsdr/src/transmitter.c:1072 [@120188f]) instead of Thetis's
        // hardcoded 4096 (cmaster.c:180 [v2.10.3.13]).  Two reasons:
        //   1. WDSP iobuffs.c:577 wraps r2_outidx with `==` not modulo —
        //      with dsp_size=4096 + in_size=256, out_size=1024 doesn't
        //      divide r2_active_buffsize=16384 cleanly.  With dsp_size=2048
        //      + in_size=256, out_size=1024 divides r2_active_buffsize=8192
        //      cleanly (8 wraps per cycle).
        //   2. ~50 % lower TX pipeline latency (42 ms instead of 85 ms).
        QCOMPARE(WdspEngine::kTxDspBufferSize, 2048);
    }

    // ── WdspEngine::txChannel lookup before any channel is created ───────────

    void txChannelLookupReturnsNullWhenNotCreated() {
        // Before createTxChannel, txChannel() must return nullptr.
        WdspEngine engine;
        QVERIFY(engine.txChannel(kTxChannelId) == nullptr);
    }

    // ── createTxChannel without WDSP initialized ─────────────────────────────

    void createTxChannelFailsWhenNotInitialized() {
        // WdspEngine not initialized — createTxChannel must return nullptr
        // (WDSP not ready) and must NOT record a phantom channel entry.
        WdspEngine engine;
        TxChannel* result = engine.createTxChannel(kTxChannelId);
        QVERIFY(result == nullptr);
        // txChannel lookup must also return nullptr — no entry was stored.
        QVERIFY(engine.txChannel(kTxChannelId) == nullptr);
    }

    // ── destroyTxChannel is idempotent (no WDSP) ─────────────────────────────

    void destroyTxChannelIsIdempotentWhenNeverCreated() {
        // Calling destroyTxChannel on an ID that was never opened must not crash.
        WdspEngine engine;
        engine.destroyTxChannel(kTxChannelId);  // should be a safe no-op
        engine.destroyTxChannel(kTxChannelId);  // second call also safe
    }

    void destroyTxChannelIsIdempotentAfterCreate() {
        // Create (fails because not initialized) then destroy twice — must not crash.
        WdspEngine engine;
        engine.createTxChannel(kTxChannelId);   // returns nullptr, not initialized
        engine.destroyTxChannel(kTxChannelId);  // channel was never stored
        engine.destroyTxChannel(kTxChannelId);  // idempotent second call
    }

    // ── createTxChannel returns nullptr when WDSP not initialized ───────────────
    // (Task C.2 updated this from Approach-A stub to full TxChannel construction,
    //  but without WDSP the not-initialized guard fires first — result still nullptr.)

    void createTxChannelReturnsNullptrWithoutWdsp() {
        // Without WDSP at unit-test time, createTxChannel fails at the
        // not-initialized guard and returns nullptr without constructing TxChannel.
        // The real non-null return path is exercised in tst_tx_channel_pipeline
        // via the stub path (no WDSP, but WdspEngine initialized with no-op).
        WdspEngine engine;
        TxChannel* result = engine.createTxChannel(kTxChannelId);
        QVERIFY(result == nullptr);
    }

    // ── txChannel lookup returns nullptr after destroy ────────────────────────

    void txChannelLookupIsNullAfterDestroy() {
        // After destroyTxChannel, the lookup must return nullptr (channel erased).
        // In stub mode (no WDSP) createTxChannel fails before recording anything,
        // so txChannel was never inserted. Both code paths leave txChannel==nullptr.
        WdspEngine engine;
        engine.createTxChannel(kTxChannelId);
        engine.destroyTxChannel(kTxChannelId);
        QVERIFY(engine.txChannel(kTxChannelId) == nullptr);
    }

    // ── Multi-channel: RX and TX can co-exist ────────────────────────────────

    void rxAndTxChannelsDontInterfere() {
        // Creating RX channel 0 and TX channel 1 on the same engine must
        // not corrupt each other's lookup tables. In stub mode (no WDSP)
        // neither createRxChannel nor createTxChannel records an entry
        // (both return nullptr on not-initialized). Destroying each is a no-op.
        // Verify no crash and that IDs remain independent.
        WdspEngine engine;

        RxChannel* rx = engine.createRxChannel(kRxChannelId);
        TxChannel* tx = engine.createTxChannel(kTxChannelId);

        // Both fail because not initialized — pointers are nullptr.
        QVERIFY(rx == nullptr);
        QVERIFY(tx == nullptr);

        // Lookups must not cross-contaminate.
        QVERIFY(engine.rxChannel(kRxChannelId) == nullptr);
        QVERIFY(engine.txChannel(kTxChannelId) == nullptr);

        // Destroy is idempotent even when nothing was created.
        engine.destroyRxChannel(kRxChannelId);
        engine.destroyTxChannel(kTxChannelId);

        // Lookups must still return nullptr after destroy.
        QVERIFY(engine.rxChannel(kRxChannelId) == nullptr);
        QVERIFY(engine.txChannel(kTxChannelId) == nullptr);
    }

    // ── TX channel ID does not alias an RX channel ID ────────────────────────

    void txChannelIdIsDistinctFromRxChannelId() {
        // TX channel uses ID 1 (WDSP.id(1, 0)); RX channel uses ID 0.
        // Verify these are not equal — the channel ID space must not alias.
        QVERIFY(kTxChannelId != kRxChannelId);
        QCOMPARE(kTxChannelId, 1);   // TX ID per WDSP.id(1,0) + CMsubrcvr=CMrcvr=1
        QCOMPARE(kRxChannelId, 0);
    }

    // ── TxChannel created via WdspEngine must have no Qt parent ─────────────
    //
    // Regression introduced by Phase 3M-1c TX pump v3 redesign: WdspEngine::
    // createTxChannel constructed TxChannel with parent=this (the engine —
    // main-thread-affined), but RadioModel::connectToRadio then does
    // m_txChannel->moveToThread(workerThread).  Qt invariant: a QObject with
    // a parent CANNOT be moved across threads.  moveToThread silently fails
    // (one warning), TxChannel stays on the main thread, and TxWorkerThread::
    // run drains QCoreApplication::sendPostedEvents(m_txChannel) from the
    // worker thread — producing tens of thousands to millions of "Cannot send
    // posted events for objects in another thread" warnings per session and
    // starving the main thread.
    //
    // Ownership of TxChannel is held by std::unique_ptr in
    // WdspEngine::m_txChannels — a Qt parent on top of that is redundant and
    // breaks moveToThread.
    //
    // We use the test-only friend access (WdspEngine.h, gated on
    // NEREUS_BUILD_TESTS) to set m_initialized = true synchronously, bypassing
    // the async wisdom path which would otherwise need a running event loop
    // and a real WDSP wisdom file to complete.
    void createdTxChannelHasNoQtParentForThreadAffinity() {
        WdspEngine engine;
        engine.m_initialized = true;   // friend access, test-only path

        TxChannel* tx = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx != nullptr);
        QVERIFY2(tx->parent() == nullptr,
                 "TxChannel created via WdspEngine::createTxChannel must "
                 "have no Qt parent so RadioModel::connectToRadio can move "
                 "it onto TxWorkerThread without tripping Qt's "
                 "'cannot move objects with a parent' invariant");

        engine.destroyTxChannel(kTxChannelId);
    }
};

QTEST_APPLESS_MAIN(TestWdspEngineTxChannel)
#include "tst_wdsp_engine_tx_channel.moc"
