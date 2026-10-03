// =================================================================
// tests/tst_diversity_dialog_owner.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// RadioModel runs diversity for the slice with id 0 (slice A) and ignores
// diversity settings on any other slice. The dialog must edit that same
// slice, found by id rather than by list position: with slice A closed the
// first slice in the list is B, and the dialog used to edit B, which
// RadioModel then ignored. With no slice A the controls are disabled, not
// hidden, and the status line says why.
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

#include "gui/DiversityDialog.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

QSlider* phaseSlider(QWidget& dialog)
{
    for (QSlider* s : dialog.findChildren<QSlider*>()) {
        if (s->maximum() == 3600) { return s; }
    }
    return nullptr;
}

QSlider* gainSlider(QWidget& dialog)
{
    for (QSlider* s : dialog.findChildren<QSlider*>()) {
        if (s->minimum() == -200) { return s; }
    }
    return nullptr;
}

QLabel* statusLabel(QWidget& dialog)
{
    for (QLabel* l : dialog.findChildren<QLabel*>()) {
        if (l->text().startsWith(QStringLiteral("Status:"))) { return l; }
    }
    return nullptr;
}

} // namespace

class TestDiversityDialogOwner : public QObject {
    Q_OBJECT

private slots:
    void edits_slice_a_by_id_not_the_first_slice()
    {
        RadioModel model;
        QCOMPARE(model.addSlice(), 0);
        QCOMPARE(model.addSlice(), 1);
        model.removeSlice(0);
        SliceModel* b = model.sliceById(1);
        QVERIFY(b != nullptr);
        QVERIFY(model.sliceById(0) == nullptr);

        DiversityDialog dialog(&model);
        auto* enable = dialog.findChild<QCheckBox*>();
        QVERIFY(enable != nullptr);
        QSlider* phase = phaseSlider(dialog);
        QVERIFY(phase != nullptr);

        // The dialog must not edit B. The controls are disabled; force the
        // handlers anyway, as a stale signal would.
        enable->setChecked(true);
        phase->setValue(900);
        QCOMPARE(b->diversityEnabled(), false);
        QCOMPARE(b->diversityPhaseDeg(), 0.0);
    }

    void controls_disabled_with_a_reason_while_slice_a_is_closed()
    {
        RadioModel model;
        QCOMPARE(model.addSlice(), 0);
        QCOMPARE(model.addSlice(), 1);

        DiversityDialog dialog(&model);
        auto* enable = dialog.findChild<QCheckBox*>();
        QSlider* phase = phaseSlider(dialog);
        QSlider* gain = gainSlider(dialog);
        QLabel* status = statusLabel(dialog);
        QVERIFY(enable && phase && gain && status);
        QVERIFY(enable->isEnabled());
        QVERIFY(phase->isEnabled());

        model.removeSlice(0);
        QVERIFY(!enable->isEnabled());
        QVERIFY(!phase->isEnabled());
        QVERIFY(!gain->isEnabled());
        for (QPushButton* b : dialog.findChildren<QPushButton*>()) {
            if (b->text().startsWith(QLatin1Char('M'))) {
                QVERIFY2(!b->isEnabled(), qPrintable(b->text()));
            }
        }
        // Disabled, never hidden.
        QVERIFY(!enable->isHidden());
        QVERIFY(!phase->isHidden());
        QCOMPARE(status->text(),
                 QStringLiteral("Status: Slice A is closed. Open Slice A to use diversity."));

        // Slice A comes back (addSlice hands out the lowest free id): the
        // dialog follows the new slice A and edits it.
        QCOMPARE(model.addSlice(), 0);
        SliceModel* a = model.sliceById(0);
        QVERIFY(a != nullptr);
        QVERIFY(enable->isEnabled());
        QVERIFY(phase->isEnabled());
        phase->setValue(450);
        QCOMPARE(a->diversityPhaseDeg(), 45.0);
        QCOMPARE(model.sliceById(1)->diversityPhaseDeg(), 0.0);

        // And follows the new slice's changes back into the controls.
        a->setDiversityGainDb(-3.0);
        QCOMPARE(gain->value(), -30);
    }
};

QTEST_MAIN(TestDiversityDialogOwner)
#include "tst_diversity_dialog_owner.moc"
