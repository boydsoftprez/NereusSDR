// =================================================================
// tests/tst_panadapter_stack_layouts.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic D Task 3: PanadapterStack skeleton (default single layout).
// =================================================================
#include <QtTest/QtTest>
#include "gui/PanadapterStack.h"

using namespace NereusSDR;

class TestPanadapterStackLayouts : public QObject {
    Q_OBJECT
private slots:
    void stack_starts_with_layout_single()
    {
        PanadapterStack stack;
        QCOMPARE(stack.currentLayoutId(), QStringLiteral("1"));
        QCOMPARE(stack.count(), 1);
    }

    void apply_layout_2v_creates_two_pans_stacked()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("pan-0"), QStringLiteral("pan-1")};
        stack.applyLayout(QStringLiteral("2v"), ids);
        QCOMPARE(stack.count(), 2);
        QCOMPARE(stack.currentLayoutId(), QStringLiteral("2v"));
    }

    void apply_layout_2h_creates_two_pans_side_by_side()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("pan-0"), QStringLiteral("pan-1")};
        stack.applyLayout(QStringLiteral("2h"), ids);
        QCOMPARE(stack.count(), 2);
        QCOMPARE(stack.currentLayoutId(), QStringLiteral("2h"));
    }

    void apply_layout_12h_creates_3_pans_with_wide_top()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("pan-0"), QStringLiteral("pan-1"), QStringLiteral("pan-2")};
        stack.applyLayout(QStringLiteral("12h"), ids);
        QCOMPARE(stack.count(), 3);
        QCOMPARE(stack.currentLayoutId(), QStringLiteral("12h"));
    }

    void apply_layout_2x2_creates_4_pans_in_grid()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("p0"), QStringLiteral("p1"), QStringLiteral("p2"), QStringLiteral("p3")};
        stack.applyLayout(QStringLiteral("2x2"), ids);
        QCOMPARE(stack.count(), 4);
        QCOMPARE(stack.currentLayoutId(), QStringLiteral("2x2"));
    }
};

QTEST_MAIN(TestPanadapterStackLayouts)
#include "tst_panadapter_stack_layouts.moc"
