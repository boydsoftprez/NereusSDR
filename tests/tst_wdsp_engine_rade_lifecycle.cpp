// no-port-check: NereusSDR-original unit-test file. No Thetis logic
// ported in this test file; RadeChannel is a NereusSDR-native wrapper
// around third_party/rade (the librade neural codec).  WDSP has no
// concept of RADE.
// =================================================================
// tests/tst_wdsp_engine_rade_lifecycle.cpp  (NereusSDR)
// =================================================================
//
// Phase 3R Task J2 unit tests: WdspEngine::createRadeChannel /
// destroyRadeChannel / radeChannel(id) lifecycle.
//
// Test cases (5):
//   1. createReturnsNonNull           - first create returns valid pointer
//   2. createTwiceReturnsSamePointer  - duplicate create with same id
//                                       returns the existing channel
//                                       (no second construction)
//   3. destroyMakesLookupReturnNull   - lookup returns nullptr after destroy
//   4. destroyNonExistentIsNoOp       - destroy on a never-created id is
//                                       a safe no-op
//   5. ownershipQObjectParentSetCorrectly - the RadeChannel is parented
//                                       to the WdspEngine so QObject
//                                       ownership cleans up correctly
//
// Note: createRadeChannel does NOT require m_initialized = true (unlike
// createRxChannel / createTxChannel).  RadeChannel is a pure-software
// codec and does not interact with WDSP's wisdom / impulse-cache state.
// The tests below construct a WdspEngine directly without initialize().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11 - New test file for Phase 3R Task J2.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/RadeChannel.h"
#include "core/WdspEngine.h"

using namespace NereusSDR;

// Slice-id namespace.  Picked 7 to stay clear of the kTxChannelId=1 /
// PsFeedbackChannelId=5 slots used by other WdspEngine tests; the value
// itself is arbitrary because createRadeChannel does not call into WDSP
// at all (no collision possible across runs).
static constexpr int kRadeChannelId = 7;

class TestWdspEngineRadeLifecycle : public QObject {
    Q_OBJECT

private slots:

    // ── Test 1: createReturnsNonNull ────────────────────────────────────────
    //
    // First-time create on an unused id returns a valid RadeChannel*.
    void createReturnsNonNull() {
        WdspEngine engine;
        RadeChannel* ch = engine.createRadeChannel(kRadeChannelId);
        QVERIFY(ch != nullptr);
        engine.destroyRadeChannel(kRadeChannelId);
    }

    // ── Test 2: createTwiceReturnsSamePointer ───────────────────────────────
    //
    // Calling createRadeChannel(id) twice without an intervening destroy
    // returns the existing channel pointer (and does NOT construct a
    // second RadeChannel that would leak).  Mirrors the createRxChannel
    // pattern at WdspEngine.cpp:368-371 [Phase 3R J2].
    void createTwiceReturnsSamePointer() {
        WdspEngine engine;
        RadeChannel* first  = engine.createRadeChannel(kRadeChannelId);
        RadeChannel* second = engine.createRadeChannel(kRadeChannelId);
        QVERIFY(first  != nullptr);
        QVERIFY(second != nullptr);
        QCOMPARE(first, second);
        engine.destroyRadeChannel(kRadeChannelId);
    }

    // ── Test 3: destroyMakesLookupReturnNull ────────────────────────────────
    //
    // After destroyRadeChannel(id), a radeChannel(id) lookup returns
    // nullptr.
    void destroyMakesLookupReturnNull() {
        WdspEngine engine;
        RadeChannel* ch = engine.createRadeChannel(kRadeChannelId);
        QVERIFY(ch != nullptr);
        QCOMPARE(engine.radeChannel(kRadeChannelId), ch);
        engine.destroyRadeChannel(kRadeChannelId);
        QCOMPARE(engine.radeChannel(kRadeChannelId), nullptr);
    }

    // ── Test 4: destroyNonExistentIsNoOp ────────────────────────────────────
    //
    // Calling destroyRadeChannel on an id that was never created (or has
    // already been destroyed) is a safe no-op.  Mirrors the
    // destroyRxChannel idempotency contract.
    void destroyNonExistentIsNoOp() {
        WdspEngine engine;
        engine.destroyRadeChannel(kRadeChannelId);  // never created
        QCOMPARE(engine.radeChannel(kRadeChannelId), nullptr);
        // Second destroy: also a safe no-op.
        engine.destroyRadeChannel(kRadeChannelId);
        QCOMPARE(engine.radeChannel(kRadeChannelId), nullptr);
    }

    // ── Test 5: ownershipQObjectParentSetCorrectly ──────────────────────────
    //
    // The RadeChannel is constructed as a QObject child of the
    // WdspEngine so that Qt's parent-driven destruction cleans up the
    // channel if WdspEngine is destroyed without explicitly calling
    // destroyRadeChannel.  Plan J2 wording: "parented to the WdspEngine
    // so QObject ownership cleans up correctly."
    void ownershipQObjectParentSetCorrectly() {
        WdspEngine engine;
        RadeChannel* ch = engine.createRadeChannel(kRadeChannelId);
        QVERIFY(ch != nullptr);
        QCOMPARE(ch->parent(), &engine);
        engine.destroyRadeChannel(kRadeChannelId);
    }
};

QTEST_APPLESS_MAIN(TestWdspEngineRadeLifecycle)
#include "tst_wdsp_engine_rade_lifecycle.moc"
