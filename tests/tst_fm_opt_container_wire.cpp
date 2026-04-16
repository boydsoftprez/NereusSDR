// tst_fm_opt_container_wire.cpp
//
// S2.10a: Verify FmOptContainer round-trips client state through SliceModel.
//
// CTCSS decode is NOT wired to WDSP — FMSQ handles noise-level squelch only,
// not tone-coded squelch. The SliceModel properties (fmCtcssMode,
// fmCtcssValueHz) store the user's selection for when a tone detector is
// implemented (Phase 3M-3). These tests verify the client-side storage only.
//
// Simplex behavior: from Thetis console.cs:40412 chkFMTXSimplex_CheckedChanged —
// selecting simplex sets the TX mode enum but does NOT zero the offset spinbox
// (udFMOffset). The offset retains its last non-simplex value. TX effects
// (repeater shift on keydown) are Phase 3M-1.

#include <QtTest/QtTest>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include "gui/widgets/VfoModeContainers.h"
#include "gui/widgets/ScrollableLabel.h"
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

// Helper to find a named child widget
template<typename T>
static T* findNamed(QWidget* parent, const char* name) {
    return parent->findChild<T*>(QString::fromLatin1(name));
}

class TestFmOptContainerWire : public QObject {
    Q_OBJECT

private slots:
    // ── CTCSS mode round-trip ─────────────────────────────────────────────────

    void ctcssModeStoredInSlice() {
        // Setting via combo → SliceModel stores it
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* cmb = findNamed<QComboBox>(&c, "toneModeCmb");
        QVERIFY(cmb != nullptr);

        // Select "CTCSS Decode" (index 2, data value 2)
        cmb->setCurrentIndex(2);
        QCOMPARE(s.fmCtcssMode(), 2);
    }

    void ctcssValueHzStoredInSlice() {
        // Setting tone value combo → SliceModel stores the Hz value
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* cmb = findNamed<QComboBox>(&c, "toneValueCmb");
        QVERIFY(cmb != nullptr);

        // Find the 100.0 Hz entry and select it
        const int idx = cmb->findText(QStringLiteral("100.0"));
        QVERIFY(idx >= 0);
        cmb->setCurrentIndex(idx);
        QCOMPARE(s.fmCtcssValueHz(), 100.0);
    }

    void ctcssModeSyncFromSlice() {
        // SliceModel state → combo reflects it after syncFromSlice
        SliceModel s;
        s.setFmCtcssMode(3);  // CTCSS Enc+Dec

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* cmb = findNamed<QComboBox>(&c, "toneModeCmb");
        QVERIFY(cmb != nullptr);
        QCOMPARE(cmb->itemData(cmb->currentIndex()).toInt(), 3);
    }

    // ── Offset round-trip ─────────────────────────────────────────────────────

    void offsetStoredInSlice() {
        // Spinning the offset spinbox → SliceModel stores it in Hz
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* spin = findNamed<QSpinBox>(&c, "offsetKhzSpin");
        QVERIFY(spin != nullptr);

        spin->setValue(600);  // 600 kHz
        QCOMPARE(s.fmOffsetHz(), 600000);
    }

    void offsetSyncFromSlice() {
        // SliceModel stores Hz → spinbox shows kHz
        SliceModel s;
        s.setFmOffsetHz(146000);  // 146 kHz (common 2m repeater offset)

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* spin = findNamed<QSpinBox>(&c, "offsetKhzSpin");
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), 146);
    }

    // ── Simplex button — Thetis-faithful behavior ─────────────────────────────
    //
    // From Thetis console.cs:40412 chkFMTXSimplex_CheckedChanged:
    // Simplex only sets the TX mode enum; it does NOT zero udFMOffset.
    // The repeater offset spinbox retains its value — TX effects are Phase 3M-1.

    void simplexSetsModeSimplex() {
        SliceModel s;
        s.setFmTxMode(FmTxMode::Low);

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* simplexBtn = findNamed<QPushButton>(&c, "simplexBtn");
        QVERIFY(simplexBtn != nullptr);
        simplexBtn->click();

        QCOMPARE(s.fmTxMode(), FmTxMode::Simplex);
    }

    void simplexDoesNotZeroOffset() {
        // Source-first: Thetis console.cs:40412 — simplex does not zero offset.
        // The offset spinbox retains its value when simplex is selected.
        SliceModel s;
        s.setFmOffsetHz(600000);
        s.setFmTxMode(FmTxMode::High);

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* simplexBtn = findNamed<QPushButton>(&c, "simplexBtn");
        QVERIFY(simplexBtn != nullptr);
        simplexBtn->click();

        // TX mode switches to Simplex
        QCOMPARE(s.fmTxMode(), FmTxMode::Simplex);
        // Offset is preserved — not zeroed (Thetis-faithful)
        QCOMPARE(s.fmOffsetHz(), 600000);
    }

    // ── Reverse toggle — display-only ────────────────────────────────────────

    void reverseToggleStored() {
        SliceModel s;
        s.setFmReverse(false);

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* revBtn = findNamed<QPushButton>(&c, "revBtn");
        QVERIFY(revBtn != nullptr);
        revBtn->click();  // toggles checked state

        QVERIFY(s.fmReverse());
    }

    // ── TX direction button mutual exclusion ──────────────────────────────────

    void lowButtonChecksOnlyLow() {
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);

        auto* txLow   = findNamed<QPushButton>(&c, "txLowBtn");
        auto* simplex = findNamed<QPushButton>(&c, "simplexBtn");
        auto* txHigh  = findNamed<QPushButton>(&c, "txHighBtn");
        QVERIFY(txLow && simplex && txHigh);

        txLow->click();
        QVERIFY(txLow->isChecked());
        QVERIFY(!simplex->isChecked());
        QVERIFY(!txHigh->isChecked());
        QCOMPARE(s.fmTxMode(), FmTxMode::Low);
    }

    void highButtonChecksOnlyHigh() {
        SliceModel s;
        FmOptContainer c;
        c.setSlice(&s);

        auto* txLow   = findNamed<QPushButton>(&c, "txLowBtn");
        auto* simplex = findNamed<QPushButton>(&c, "simplexBtn");
        auto* txHigh  = findNamed<QPushButton>(&c, "txHighBtn");
        QVERIFY(txLow && simplex && txHigh);

        txHigh->click();
        QVERIFY(!txLow->isChecked());
        QVERIFY(!simplex->isChecked());
        QVERIFY(txHigh->isChecked());
        QCOMPARE(s.fmTxMode(), FmTxMode::High);
    }
};

QTEST_MAIN(TestFmOptContainerWire)
#include "tst_fm_opt_container_wire.moc"
