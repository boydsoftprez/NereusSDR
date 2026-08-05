// =================================================================
// tests/tst_tci_multislice_surface.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The wire formats it asserts
// are cited to Thetis; the multi-slice questions it asks have no Thetis
// answer, because Thetis tops out at two receivers and one transmitter
// bound to RX1.
//
// Codex review round 6, PR #293. Two findings, both about Phase 3F
// giving TCI more slices than TCI ever agreed to talk about.
//
//   tx_frequency was emitted for rxIndex == 0 and nothing else, under a
//   comment saying the caller would decide once 3F landed the real TXFreq
//   logic. 3F landed. TxSliceArbiter binds TX to any slice, so the test
//   was wrong in both directions at once: tuning Slice A advertised A as
//   the transmit frequency while the transmitter sat on B, and tuning B,
//   the slice actually transmitting, advertised nothing.
//
//   The init burst negotiates trx_count:2 and the design doc records that
//   as locked, with Slice C/D/E internal. The local-broadcast wiring
//   ignored it and pushed tagged frames for receivers 2, 3 and 4 at
//   clients that had sized their state from the 2 they were told.
//
// The two interact, which is why they are tested together: the fix for
// the first wants the TX-bound slice to publish tx_frequency wherever it
// is, and the fix for the second wants slices past the count to stay
// quiet. Both are right, because tx_frequency carries no receiver index
// and vfo:N does. A fix for either one alone breaks the other.
// =================================================================

#include <QtTest/QtTest>

#include "core/TciProtocol.h"

using namespace NereusSDR;
using namespace Qt::StringLiterals;

namespace {

/// TciProtocol reads rx2Enabled off its radio handle by name. Enough of a
/// stand-in to exercise the broadcast paths without a full RadioModel.
class MockRadio : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE bool rx2Enabled() const { return m_rx2; }
    void setRx2Enabled(bool on) { m_rx2 = on; }
private:
    bool m_rx2 {false};
};

/// Drain everything the protocol has queued, coalescer included.
QStringList drain(TciProtocol& p)
{
    p.drainCoalescedNotifications();
    QStringList frames;
    while (p.hasPendingNotification()) {
        frames << p.takePendingNotification();
    }
    return frames;
}

bool anyStartsWith(const QStringList& frames, const QString& prefix)
{
    for (const QString& f : frames) {
        if (f.startsWith(prefix)) { return true; }
    }
    return false;
}

} // namespace

class TestTciMultisliceSurface : public QObject
{
    Q_OBJECT

private slots:

    // ── 1. The advertised count has one home ─────────────────────────────
    //
    // trx_count was a bare literal in the init burst that no other code
    // could read, which is precisely why the broadcast path drifted from
    // it. This asserts the constant and the wire agree, so they cannot
    // drift again silently.
    void the_init_burst_advertises_the_exposed_receiver_count()
    {
        MockRadio radio;
        TciProtocol p(&radio);

        const QStringList burst = p.buildInitBurst();
        const QString expected =
            QStringLiteral("trx_count:%1;").arg(TciProtocol::kExposedReceiverCount);

        QVERIFY2(burst.contains(expected),
            qPrintable(QStringLiteral("init burst must advertise %1")
                           .arg(expected)));
    }

    // ── 2. Locked at two ─────────────────────────────────────────────────
    //
    // Phase 3J-1 design doc §1.2 calls this a locked decision for strict
    // Thetis client compatibility, matching Thetis TCIServer.cs:2530
    // [v2.10.3.13]. Raising it is an opt-in feature with a widened init
    // burst behind it, not an incidental edit, so it gets a tripwire.
    void the_exposed_receiver_count_is_two()
    {
        QVERIFY2(TciProtocol::kExposedReceiverCount == 2,
            "trx_count is locked at 2 (design doc §1.2, Thetis parity). "
            "Raising it requires widening the per-receiver init burst to "
            "match, which is the out-of-scope opt-in, not a review fix.");
    }

    // ── 3. The TX-bound receiver emits tx_frequency ──────────────────────
    void the_tx_bound_receiver_emits_tx_frequency()
    {
        MockRadio radio;
        TciProtocol p(&radio);

        p.enqueueLocalBroadcastVfo(/*rxIndex=*/1, /*hz=*/14250000LL,
                                   /*isTxBound=*/true);

        const QStringList frames = drain(p);
        QVERIFY2(anyStartsWith(frames, u"tx_frequency:"_s),
            "the receiver driving the transmitter must publish tx_frequency, "
            "whichever receiver it is");
        QVERIFY(frames.contains(u"tx_frequency:14250000;"_s));
    }

    // ── 4. A receiver that is not transmitting does not ──────────────────
    //
    // The half of the finding that silently misinformed clients: Slice A
    // kept advertising itself as the transmit frequency after TX moved to
    // B, so a client following tx_frequency was pointed at the wrong band.
    void a_receiver_that_is_not_transmitting_emits_no_tx_frequency()
    {
        MockRadio radio;
        TciProtocol p(&radio);

        p.enqueueLocalBroadcastVfo(/*rxIndex=*/0, /*hz=*/7100000LL,
                                   /*isTxBound=*/false);

        const QStringList frames = drain(p);
        QVERIFY2(!anyStartsWith(frames, u"tx_frequency:"_s),
            "receiver 0 no longer owns the transmitter by default; tuning it "
            "must not claim to move the transmit frequency");
        QVERIFY2(!anyStartsWith(frames, u"tx_frequency_thetis:"_s),
            "the bespoke Thetis variant must follow the same rule");

        // Its own tagged frames still go out; this gates TX only.
        QVERIFY2(anyStartsWith(frames, u"vfo:0,"_s),
            "the receiver's own VFO frames are unaffected");
    }

    // ── 5. tx_frequency is publishable without a receiver tag ────────────
    //
    // The seam that lets findings 1 and 2 both be satisfied. A TX handoff
    // to an internal slice, and a retune of that slice, must update
    // tx_frequency without emitting a vfo:N naming a receiver the client
    // was never told exists.
    void tx_frequency_can_be_published_without_any_receiver_frames()
    {
        MockRadio radio;
        TciProtocol p(&radio);

        p.enqueueLocalBroadcastTxFrequency(10125000LL);

        const QStringList frames = drain(p);
        QVERIFY(frames.contains(u"tx_frequency:10125000;"_s));
        QVERIFY2(!anyStartsWith(frames, u"vfo:"_s),
            "publishing a transmit frequency must not announce a receiver");
        QVERIFY2(!anyStartsWith(frames, u"dds:"_s),
            "nor a dds, which is equally receiver-tagged");
    }

    // ── 6. The tagged frames name only exposed receivers ─────────────────
    //
    // Reads every frame the VFO path produces for an exposed receiver and
    // checks the index it carries. A frame tagged 2 or beyond is outside
    // the negotiated surface no matter how well-formed it is.
    void tagged_frames_never_name_a_receiver_past_the_advertised_count()
    {
        MockRadio radio;
        TciProtocol p(&radio);

        for (int rx = 0; rx < TciProtocol::kExposedReceiverCount; ++rx) {
            p.enqueueLocalBroadcastVfo(rx, 14200000LL + rx, /*isTxBound=*/false);
        }

        const QStringList frames = drain(p);
        QVERIFY(!frames.isEmpty());

        for (const QString& f : frames) {
            if (!f.startsWith(u"vfo:"_s) && !f.startsWith(u"dds:"_s)) {
                continue;
            }
            // vfo:RX,CHAN,HZ;  and  dds:RX,HZ;
            const QString body = f.section(u':', 1);
            const int rx = body.section(u',', 0, 0).toInt();
            QVERIFY2(rx < TciProtocol::kExposedReceiverCount,
                qPrintable(QStringLiteral("frame names receiver %1, past the "
                    "advertised trx_count %2: %3")
                    .arg(rx).arg(TciProtocol::kExposedReceiverCount).arg(f)));
        }
    }
};

QTEST_MAIN(TestTciMultisliceSurface)
#include "tst_tci_multislice_surface.moc"
