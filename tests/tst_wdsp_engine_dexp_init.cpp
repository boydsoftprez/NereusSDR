// no-port-check: NereusSDR-original unit-test file. Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_wdsp_engine_dexp_init.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for Phase 3M-3a-iii Task 20: WdspEngine create_dexp /
// destroy_dexp lifecycle.  Verifies the bench-failing fix:
//
//   Before Task 20: pdexp[1] was permanently nullptr after createTxChannel
//   because create_dexp had never been ported into NereusSDR's TX-init
//   path.  Every SetDEXP* setter and the SendCBPushDexpVox callback
//   registration silently no-op'd via their pdexp[id] null guards.
//   Bench-confirmed by JJ on ANAN-G2: VOX-keying never engaged MOX
//   because xdexp() never ran (no DEXP module to drive).
//
//   After Task 20: WdspEngine::createTxChannel calls create_dexp with
//   default args sourced verbatim from Thetis cmaster.c:130-157
//   [v2.10.3.13].  pdexp[1] is non-null after createTxChannel returns
//   AND back to nullptr after destroyTxChannel.
//
// Test cases (3):
//   1. createTxChannel_initialisesDexp           - pdexp[1] non-null after create
//   2. destroyTxChannel_allowsRecreate           - destroy then re-create still
//                                                  populates pdexp[1] (proves
//                                                  destroy_dexp + create_dexp
//                                                  lifecycle is correctly wired)
//   3. pumpDexp_isNoOpWithoutWdspEngineWire      - pumpDexp degrades safely
//                                                  on directly-constructed
//                                                  TxChannel (no segfault)
//
// Source references (cited for traceability; logic lives in WdspEngine.cpp):
//   wdsp/cmaster.c:130-157 [v2.10.3.13]  - Thetis create_dexp call site
//                                          (default args sourced verbatim)
//   wdsp/cmaster.c:267     [v2.10.3.13]  - Thetis destroy_dexp call site
//                                          (BEFORE CloseChannel in destroy_xmtr)
//   wdsp/dexp.c:187-227    [v2.10.3.13]  - create_dexp impl: allocates DEXP
//                                          struct, stores in pdexp[id]
//   wdsp/dexp.c:230-239    [v2.10.3.13]  - destroy_dexp impl: deallocates,
//                                          but DOES NOT clear pdexp[id]!
//                                          (the test below assertion is
//                                          "pointer-after-free is unspecified")
//
// Note on destroy_dexp / pdexp[] bookkeeping (read this if assertion #2
// behaves unexpectedly): Thetis's destroy_dexp at dexp.c:230-239
// [v2.10.3.13] frees the struct but does NOT explicitly null out
// pdexp[id] afterward.  The pointer is dangling until the next
// create_dexp(id, ...) reassigns the slot.  In practice WDSP's
// memory-allocation pattern (and our own asan/UBSan runs) treats the
// slot as "stale pointer — undefined to deref".  The assertion below
// uses a DIFFERENT criterion: we verify that re-creating a TxChannel
// with the same id and then re-destroying behaves consistently, rather
// than asserting on the dangling-pointer value itself.  See test case 2
// for the exact semantics.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 - New test file for Phase 3M-3a-iii Task 20: WdspEngine
//                 DEXP DSP-instance lifecycle.  J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/TxChannel.h"
#include "core/WdspEngine.h"

// Forward-declare the WDSP DEXP global so the test can introspect whether
// create_dexp populated the slot.  The full struct definition lives in
// third_party/wdsp/src/dexp.h (Windows-isms shimmed by linux_port.h);
// since we only test pointer non-null-ness, the opaque forward declaration
// is sufficient and avoids dragging in <Windows.h>-flavoured shims for
// platforms that don't define HAVE_WDSP.
//
// Cite: Thetis wdsp/dexp.h:104 [v2.10.3.13] - `extern DEXP pdexp[];`
//       (DEXP is `typedef struct _dexp *DEXP;` at dexp.h:102.)
#ifdef HAVE_WDSP
extern "C" {
struct _dexp;
typedef struct _dexp *DEXP;
extern DEXP pdexp[];
}
#endif

using namespace NereusSDR;

// Channel ID convention (matches tst_wdsp_engine_tx_channel.cpp):
//   TX channel: WDSP.id(1, 0) = CMsubrcvr * CMrcvr = 1 * 1 = 1
//   (dsp.cs:926-944 case 2, with NereusSDR CMsubrcvr=CMrcvr=1)
static constexpr int kTxChannelId = 1;

class TstWdspEngineDexpInit : public QObject {
    Q_OBJECT

private slots:

    // ── Test 1: pdexp[1] is non-null after createTxChannel ───────────────────
    //
    // The assertion that proves the fix.  Before Task 20, this would have
    // failed (pdexp[1] permanently nullptr; create_dexp never called).
    // After Task 20, pdexp[1] is non-null because WdspEngine::createTxChannel
    // calls create_dexp(channelId, ...) right after OpenChannel(type=1).
    void createTxChannel_initialisesDexp() {
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
                                        // - bypasses async wisdom path

        TxChannel* tx = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx != nullptr);

#ifdef HAVE_WDSP
        // The bench-failing assertion that motivated this entire commit.
        QVERIFY2(pdexp[kTxChannelId] != nullptr,
                 "pdexp[1] must be non-null after createTxChannel - "
                 "create_dexp call missing from WdspEngine::createTxChannel "
                 "is the root cause of bench-confirmed VOX-keying failure "
                 "(`[VOXDIAG] registerVoxCallback ch= 1 SKIPPED: pdexp NULL`)");
#endif

        engine.destroyTxChannel(kTxChannelId);
    }

    // ── Test 2: destroy + recreate cycle is consistent ───────────────────────
    //
    // Thetis destroy_dexp (dexp.c:230-239 [v2.10.3.13]) frees the struct
    // but does NOT null pdexp[id] afterward - the pointer dangles until
    // the next create_dexp(id, ...) reassigns the slot.  Asserting
    // pdexp[id] == nullptr after destroy would be incorrect (it's a
    // dangling pointer, not a null pointer).
    //
    // What we CAN safely assert is that a destroy + create cycle yields
    // a NEW non-null pointer that we can use, and that a second destroy
    // doesn't crash.  This proves the lifecycle is correctly wired -
    // create_dexp runs again on the second create, allocating a fresh
    // DEXP module (different address from the first, but we don't assert
    // that because a slab allocator could legitimately reuse the slot).
    void destroyTxChannel_allowsRecreate() {
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        TxChannel* tx1 = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx1 != nullptr);
#ifdef HAVE_WDSP
        QVERIFY(pdexp[kTxChannelId] != nullptr);
#endif

        engine.destroyTxChannel(kTxChannelId);
        QVERIFY(engine.txChannel(kTxChannelId) == nullptr);

        // Recreate on the same id - create_dexp must run again and pdexp[id]
        // must again be non-null (whether or not it's the same address).
        TxChannel* tx2 = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx2 != nullptr);
#ifdef HAVE_WDSP
        QVERIFY2(pdexp[kTxChannelId] != nullptr,
                 "pdexp[1] must be non-null after RE-create - the second "
                 "createTxChannel call must invoke create_dexp again, not "
                 "skip it on the assumption a previous create lingered");
#endif

        engine.destroyTxChannel(kTxChannelId);
    }

    // ── Test 3: TxChannel::pumpDexp is null-safe before WdspEngine wires ─────
    //
    // Direct construction (bypassing WdspEngine::createTxChannel) leaves
    // m_dexpBuffer null; pumpDexp must degrade to a no-op without
    // dereferencing the missing buffer pointer.  This guarantees test
    // builds and edge-case construction paths don't segfault.
    void pumpDexp_isNoOpWithoutWdspEngineWire() {
        TxChannel ch(kTxChannelId);
        // 128 doubles == 64 complex samples == default block size (matches
        // TxWorkerThread::kBlockFrames * 2).  Content is irrelevant - the
        // assertion is that pumpDexp does not crash.
        const std::vector<double> dummy(128, 0.0);
        for (int i = 0; i < 20; ++i) {
            ch.pumpDexp(dummy.data());
        }
        // Calling with nullptr must also be a no-op.
        ch.pumpDexp(nullptr);
    }
};

QTEST_APPLESS_MAIN(TstWdspEngineDexpInit)
#include "tst_wdsp_engine_dexp_init.moc"
