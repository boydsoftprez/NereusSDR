#include <QtTest/QtTest>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include "gui/widgets/VfoModeContainers.h"
#include "gui/widgets/ScrollableLabel.h"
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

// Helper to locate a named widget child of any type T
template<typename T>
static T* findNamed(QWidget* parent, const char* name) {
    return parent->findChild<T*>(QString::fromLatin1(name));
}

class TestVfoModeContainers : public QObject {
    Q_OBJECT

private slots:
    // ── FmOptContainer ────────────────────────────────────────────────────
    void toneModeReflectsSlice() {
        SliceModel s;
        s.setFmCtcssMode(2);  // CTCSS Decode

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* cmb = findNamed<QComboBox>(&c, "toneModeCmb");
        QVERIFY(cmb != nullptr);
        QCOMPARE(cmb->itemData(cmb->currentIndex()).toInt(), 2);
    }

    void toneValueReflectsSlice() {
        SliceModel s;
        s.setFmCtcssValueHz(100.0);

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* cmb = findNamed<QComboBox>(&c, "toneValueCmb");
        QVERIFY(cmb != nullptr);
        QCOMPARE(cmb->currentText(), QStringLiteral("100.0"));
    }

    void offsetReflectsSlice() {
        SliceModel s;
        s.setFmOffsetHz(600000);  // 600 kHz stored as Hz

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* spin = findNamed<QSpinBox>(&c, "offsetKhzSpin");
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), 600);
    }

    void txModeReflectsSlice() {
        SliceModel s;
        s.setFmTxMode(FmTxMode::High);

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* txLow   = findNamed<QPushButton>(&c, "txLowBtn");
        auto* simplex = findNamed<QPushButton>(&c, "simplexBtn");
        auto* txHigh  = findNamed<QPushButton>(&c, "txHighBtn");

        QVERIFY(txLow  != nullptr);
        QVERIFY(simplex != nullptr);
        QVERIFY(txHigh != nullptr);

        QCOMPARE(txLow->isChecked(),   false);
        QCOMPARE(simplex->isChecked(), false);
        QCOMPARE(txHigh->isChecked(),  true);
    }

    void reverseReflectsSlice() {
        SliceModel s;
        s.setFmReverse(true);

        FmOptContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* rev = findNamed<QPushButton>(&c, "revBtn");
        QVERIFY(rev != nullptr);
        QCOMPARE(rev->isChecked(), true);
    }

    // ── DigOffsetContainer ────────────────────────────────────────────────
    void offsetReflectsSliceDigl() {
        SliceModel s;
        s.setDspMode(DSPMode::DIGL);
        s.setDiglOffsetHz(500);

        DigOffsetContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* lbl = findNamed<ScrollableLabel>(&c, "offsetLabel");
        QVERIFY(lbl != nullptr);
        QCOMPARE(lbl->value(), 500);
    }

    void offsetReflectsSliceDigu() {
        SliceModel s;
        s.setDspMode(DSPMode::DIGU);
        s.setDiguOffsetHz(-300);

        DigOffsetContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* lbl = findNamed<ScrollableLabel>(&c, "offsetLabel");
        QVERIFY(lbl != nullptr);
        QCOMPARE(lbl->value(), -300);
    }

    void applyRoutesByMode() {
        SliceModel s;
        s.setDspMode(DSPMode::DIGL);
        s.setDiglOffsetHz(0);
        s.setDiguOffsetHz(0);

        DigOffsetContainer c;
        c.setSlice(&s);

        // Set value via the ScrollableLabel — triggers valueChanged → applyOffset
        auto* lbl = findNamed<ScrollableLabel>(&c, "offsetLabel");
        QVERIFY(lbl != nullptr);

        lbl->setValue(100);
        QCOMPARE(s.diglOffsetHz(), 100);
        QCOMPARE(s.diguOffsetHz(), 0);   // DIGU must remain unchanged

        // Switch to DIGU and write a different value
        s.setDspMode(DSPMode::DIGU);
        lbl->setValue(200);
        QCOMPARE(s.diguOffsetHz(), 200);
        QCOMPARE(s.diglOffsetHz(), 100); // DIGL must remain unchanged
    }

    // ── RttyMarkShiftContainer ────────────────────────────────────────────
    void markReflectsSlice() {
        SliceModel s;
        s.setRttyMarkHz(2295);

        RttyMarkShiftContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* lbl = findNamed<ScrollableLabel>(&c, "markLabel");
        QVERIFY(lbl != nullptr);
        QCOMPARE(lbl->value(), 2295);
    }

    void shiftReflectsSlice() {
        SliceModel s;
        s.setRttyShiftHz(170);

        RttyMarkShiftContainer c;
        c.setSlice(&s);
        c.syncFromSlice();

        auto* lbl = findNamed<ScrollableLabel>(&c, "shiftLabel");
        QVERIFY(lbl != nullptr);
        QCOMPARE(lbl->value(), 170);
    }
};

QTEST_MAIN(TestVfoModeContainers)
#include "tst_vfo_mode_containers.moc"
