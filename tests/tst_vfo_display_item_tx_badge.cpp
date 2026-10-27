// no-port-check: NereusSDR-original unit-test file.  VfoDisplayItem is an
// AetherSDR-pattern + NereusSDR-native widget (3G-8); the TX-state colour
// swap is NereusSDR-original UX (no Thetis port).  The Phase 3M-1c chunk
// G test surface is documented in pre-code review §1 + plan §G.
// =================================================================
// tests/tst_vfo_display_item_tx_badge.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for VfoDisplayItem TX-state colour swap (Phase 3M-1c G.1)
// and the per-rx routing pattern that Phase L will wire from
// MoxController::moxChanged(int rx, bool, bool) (Phase 3M-1c G.2).
//
// G.1 — verifies the existing widget machinery:
//   - setTransmitting(bool) / isTransmitting() round-trip
//   - paint() renders the left-edge badge in m_txColour when transmitting,
//     m_rxColour when not (rendered to QImage; pixel sampled at the badge).
//
// G.2 — demonstrates the Phase L wiring pattern:
//   - A lambda connected to MoxController::moxChanged(int rx, bool, bool)
//     dispatches setTransmitting(newMox) to the VfoDisplayItem matching
//     the carried rx index (rx=1 → VFO-A, rx=2 → VFO-B).
//   - Verifies independence: when rx=1 fires, only VFO-A's state changes.
//
// Modification history (NereusSDR):
//   2026-10-26 - Per-slice TX marker: with slice states fed, only the
//                 slice transmit comes off shows the TX colour while
//                 transmitting. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QColor>
#include <QImage>
#include <QPainter>

#include "core/MoxController.h"
#include "gui/meters/VfoDisplayItem.h"

using namespace NereusSDR;

namespace {

// Render the item to a 200x40 image and return the pixel sampled inside
// the left-edge badge stripe (the stateColour fillRect at paint() line 97
// covers x=[0..2], y=full height).  Sampling (1, height/2) is solidly in
// the middle of that stripe.
QColor renderBadgeColour(VfoDisplayItem& item)
{
    constexpr int kW = 200;
    constexpr int kH = 40;
    QImage img(kW, kH, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    item.paint(p, kW, kH);
    p.end();
    return img.pixelColor(1, kH / 2);
}

}  // namespace

class TstVfoDisplayItemTxBadge : public QObject
{
    Q_OBJECT

private slots:

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // G.1 — setTransmitting / isTransmitting round-trip
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void default_isTransmitting_isFalse()
    {
        VfoDisplayItem item;
        QCOMPARE(item.isTransmitting(), false);
    }

    void setTransmitting_true_thenFalse_roundTrip()
    {
        VfoDisplayItem item;
        item.setTransmitting(true);
        QCOMPARE(item.isTransmitting(), true);
        item.setTransmitting(false);
        QCOMPARE(item.isTransmitting(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // G.1 — render-path verification (badge colour reflects state)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void render_atRx_usesRxColour()
    {
        VfoDisplayItem item;
        // Default rxColour is LimeGreen (0,255,0) per VfoDisplayItem.h:130.
        const QColor sampled = renderBadgeColour(item);
        QCOMPARE(sampled, QColor(0x00, 0xff, 0x00));
    }

    void render_atTx_usesTxColour()
    {
        VfoDisplayItem item;
        item.setTransmitting(true);
        // Default txColour is Red (255,0,0) per VfoDisplayItem.h:131.
        const QColor sampled = renderBadgeColour(item);
        QCOMPARE(sampled, QColor(0xff, 0x00, 0x00));
    }

    void render_customTxColour_isHonoured()
    {
        VfoDisplayItem item;
        const QColor magenta(0xff, 0x00, 0xff);
        item.setTxColour(magenta);
        item.setTransmitting(true);
        const QColor sampled = renderBadgeColour(item);
        QCOMPARE(sampled, magenta);
    }

    void render_toggleBack_returnsToRxColour()
    {
        VfoDisplayItem item;
        item.setTransmitting(true);
        item.setTransmitting(false);
        const QColor sampled = renderBadgeColour(item);
        QCOMPARE(sampled, QColor(0x00, 0xff, 0x00));  // back to LimeGreen
    }

    // The TX marker lights only on the slice transmit comes off: a
    // display on a slice that does not transmit stays at the RX colour.
    void render_txMarker_onTransmitSliceOnly()
    {
        VfoDisplayItem item;
        VfoSliceState a;
        a.index = 0; a.frequencyHz = 14200000; a.mode = QStringLiteral("USB");
        a.transmitting = true; a.active = true; a.open = true;
        VfoSliceState b = a;
        b.index = 1; b.transmitting = false; b.active = false;
        item.setSliceState(0, a);
        item.setSliceState(1, b);
        item.setTransmitting(true);

        item.setSourceSliceIndex(0);
        QCOMPARE(renderBadgeColour(item), QColor(0xff, 0x00, 0x00));
        item.setSourceSliceIndex(1);
        QCOMPARE(renderBadgeColour(item), QColor(0x00, 0xff, 0x00));
        item.setTransmitting(false);
        item.setSourceSliceIndex(0);
        QCOMPARE(renderBadgeColour(item), QColor(0x00, 0xff, 0x00));
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // G.2 — routing pattern (Phase L will use the same lambda shape)
    //
    // Demonstrates the routing rule from pre-code review §6.5 / §1.3:
    //   rx == 1 → VFO-A (default; TX comes off VFO-A)
    //   rx == 2 → VFO-B (only when RX2 enabled AND VFOBTX)
    //
    // This test does the connect() that Phase L will do.  It is NOT a
    // production-wiring test (Phase L owns that); it documents the lambda
    // shape as the canonical reference.
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void routing_rx1_targetsVfoA_only()
    {
        MoxController mox;
        VfoDisplayItem vfoA;
        VfoDisplayItem vfoB;

        QObject::connect(&mox, &MoxController::moxChanged, &vfoA,
            [&vfoA, &vfoB](int rx, bool /*oldMox*/, bool newMox) {
                if (rx == 1) { vfoA.setTransmitting(newMox); }
                else if (rx == 2) { vfoB.setTransmitting(newMox); }
            },
            Qt::DirectConnection);

        // Default rx2_enabled=false, vfobTx=false → moxChanged carries rx=1.
        mox.setMox(true);
        // Drive the timer walk to completion so moxChanged Post fires.
        QTRY_COMPARE_WITH_TIMEOUT(vfoA.isTransmitting(), true, 2000);
        QCOMPARE(vfoB.isTransmitting(), false);
    }

    void routing_rx2_targetsVfoB_only()
    {
        MoxController mox;
        // Set the conditions for rx=2: rx2_enabled AND vfobTx.
        mox.setRx2Enabled(true);
        mox.setVfobTx(true);

        VfoDisplayItem vfoA;
        VfoDisplayItem vfoB;

        QObject::connect(&mox, &MoxController::moxChanged, &vfoA,
            [&vfoA, &vfoB](int rx, bool /*oldMox*/, bool newMox) {
                if (rx == 1) { vfoA.setTransmitting(newMox); }
                else if (rx == 2) { vfoB.setTransmitting(newMox); }
            },
            Qt::DirectConnection);

        mox.setMox(true);
        QTRY_COMPARE_WITH_TIMEOUT(vfoB.isTransmitting(), true, 2000);
        QCOMPARE(vfoA.isTransmitting(), false);
    }

    void routing_rx2EnabledOnly_stillTargetsVfoA()
    {
        // RX2 enabled but VFOBTX off → TX still comes off VFO-A → rx=1.
        // From Thetis console.cs:29677 [v2.10.3.13] semantic:
        //   rx2_enabled && VFOBTX ? 2 : 1
        MoxController mox;
        mox.setRx2Enabled(true);
        mox.setVfobTx(false);

        VfoDisplayItem vfoA;
        VfoDisplayItem vfoB;

        QObject::connect(&mox, &MoxController::moxChanged, &vfoA,
            [&vfoA, &vfoB](int rx, bool /*oldMox*/, bool newMox) {
                if (rx == 1) { vfoA.setTransmitting(newMox); }
                else if (rx == 2) { vfoB.setTransmitting(newMox); }
            },
            Qt::DirectConnection);

        mox.setMox(true);
        QTRY_COMPARE_WITH_TIMEOUT(vfoA.isTransmitting(), true, 2000);
        QCOMPARE(vfoB.isTransmitting(), false);
    }

    void routing_moxOff_clearsTransmitting()
    {
        MoxController mox;
        VfoDisplayItem vfoA;
        VfoDisplayItem vfoB;

        QObject::connect(&mox, &MoxController::moxChanged, &vfoA,
            [&vfoA, &vfoB](int rx, bool /*oldMox*/, bool newMox) {
                if (rx == 1) { vfoA.setTransmitting(newMox); }
                else if (rx == 2) { vfoB.setTransmitting(newMox); }
            },
            Qt::DirectConnection);

        // Drive on then off; verify the TX badge clears.
        mox.setMox(true);
        QTRY_COMPARE_WITH_TIMEOUT(vfoA.isTransmitting(), true, 2000);
        mox.setMox(false);
        QTRY_COMPARE_WITH_TIMEOUT(vfoA.isTransmitting(), false, 2000);
    }
};

QTEST_MAIN(TstVfoDisplayItemTxBadge)
#include "tst_vfo_display_item_tx_badge.moc"
