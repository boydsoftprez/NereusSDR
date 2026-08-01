// =================================================================
// tests/tst_panadapter_stack_layouts.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic D Task 3: PanadapterStack skeleton (default single layout).
// =================================================================
#include <QtTest/QtTest>
#include <QPointer>
#include "gui/PanadapterStack.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterApplet.h"
#include "core/AppSettings.h"

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

    // activePanId() feeds "Add slice on active pan" (Ctrl+R),
    // "Float active pan..." and rebuildFftRouting's last-resort pan
    // resolution. removePanadapter took the applet out of m_pans and
    // deleted it without touching m_activePanId, so after the active pan
    // was closed every one of those resolved to a destroyed pan.
    //
    // Nothing re-seeded it either: setActivePan's only caller is guarded on
    // m_activePanId.isEmpty(), so a stale non-empty id is permanent.
    void removing_the_active_pan_reseats_the_active_id()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("pan-0"), QStringLiteral("pan-1")};
        stack.applyLayout(QStringLiteral("2v"), ids);
        QCOMPARE(stack.activePanId(), QStringLiteral("pan-0"));

        QSignalSpy spy(&stack, &PanadapterStack::activePanChanged);
        stack.removePanadapter(QStringLiteral("pan-0"));

        // Must name a pan that still exists, and must say so.
        QCOMPARE(stack.activePanId(), QStringLiteral("pan-1"));
        QVERIFY(stack.panadapter(stack.activePanId()) != nullptr);
        QCOMPARE(spy.count(), 1);
    }

    // Removing a pan that is not the active one must leave the active id
    // alone -- re-seating on every removal would move the operator's
    // working pan out from under them.
    void removing_a_non_active_pan_leaves_the_active_id_alone()
    {
        PanadapterStack stack;
        QStringList ids = {QStringLiteral("pan-0"), QStringLiteral("pan-1")};
        stack.applyLayout(QStringLiteral("2v"), ids);
        QCOMPARE(stack.activePanId(), QStringLiteral("pan-0"));

        QSignalSpy spy(&stack, &PanadapterStack::activePanChanged);
        stack.removePanadapter(QStringLiteral("pan-1"));

        QCOMPARE(stack.activePanId(), QStringLiteral("pan-0"));
        QCOMPARE(spy.count(), 0);
    }

    // Removing the last pan clears the id rather than leaving it naming a
    // destroyed pan. Empty is also what re-arms setActivePan's isEmpty
    // guard, so the next pan created becomes active.
    void removing_the_last_pan_clears_the_active_id()
    {
        PanadapterStack stack;
        QCOMPARE(stack.count(), 1);
        const QString only = stack.activePanId();
        QVERIFY(!only.isEmpty());

        stack.removePanadapter(only);
        QCOMPARE(stack.activePanId(), QString());

        stack.addPanadapter(QStringLiteral("pan-fresh"));
        QCOMPARE(stack.activePanId(), QStringLiteral("pan-fresh"));
    }

    void splitter_state_round_trips_via_app_settings()
    {
        // AppSettings is process-global; clear it so the persistence keys
        // start clean and the round-trip assertion is deterministic.
        AppSettings::instance().clear();

        // Test the wire-format round trip: what saveSplitterState writes to
        // AppSettings must come back through restoreSplitterState as the same
        // sizes the splitter actually held at save time. We deliberately do
        // NOT assert that those sizes are an exact match for what was passed
        // to rootSplitterSetSizesForTest, because QSplitter normalizes the
        // requested sizes against the widget's actual geometry and the
        // splitter handle width. The contract this test pins down is:
        //   saved_sizes == restored_sizes (within the same geometry).
        // The exact pixel values are an implementation detail of Qt.
        const QSize kStackSize(800, 800);

        QList<int> savedSizes;
        QString savedLayoutId;
        {
            PanadapterStack stack;
            stack.resize(kStackSize);
            stack.applyLayout(QStringLiteral("2v"),
                              {QStringLiteral("p0"), QStringLiteral("p1")});
            stack.show();
            QTest::qWait(10);
            stack.rootSplitterSetSizesForTest({300, 500});
            savedSizes = stack.rootSplitterSizes();
            savedLayoutId = stack.currentLayoutId();
            stack.saveSplitterState();
        }

        // Verify the wire format landed in AppSettings.
        auto& s = AppSettings::instance();
        const QString raw = s.value(QStringLiteral("PanSplitter0Sizes"), QString()).toString();
        QVERIFY(!raw.isEmpty());
        QCOMPARE(s.value(QStringLiteral("PanLayoutId"), QString()).toString(), savedLayoutId);

        {
            PanadapterStack stack2;
            stack2.resize(kStackSize);
            stack2.applyLayout(QStringLiteral("2v"),
                               {QStringLiteral("p0"), QStringLiteral("p1")});
            stack2.show();
            QTest::qWait(10);
            stack2.restoreSplitterState();
            const QList<int> sizes = stack2.rootSplitterSizes();
            QCOMPARE(sizes.size(), savedSizes.size());
            // Round-trip equality: the second stack must hold the same sizes
            // the first stack saved.
            for (int i = 0; i < sizes.size(); ++i) {
                QCOMPARE(sizes[i], savedSizes[i]);
            }
        }
    }

    void layout_omitting_a_floating_pan_removes_it_once()
    {
        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2v"),
                          {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        QPointer<PanadapterApplet> omitted(stack.panadapter(QStringLiteral("pan-1")));
        stack.floatPanadapter(QStringLiteral("pan-1"));
        QPointer<PanFloatingWindow> floater(
            stack.floatingWindowForTest(QStringLiteral("pan-1")));
        QVERIFY(omitted);
        QVERIFY(floater);

        stack.applyLayout(QStringLiteral("1"), {QStringLiteral("pan-0")});
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QVERIFY(omitted.isNull());
        QVERIFY(floater.isNull());
        QCOMPARE(stack.count(), 1);
        QVERIFY(stack.panadapter(QStringLiteral("pan-0")) != nullptr);
    }

    void layout_retaining_a_floating_pan_docks_before_reparenting()
    {
        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2v"),
                          {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        QPointer<PanadapterApplet> retained(stack.panadapter(QStringLiteral("pan-1")));
        stack.floatPanadapter(QStringLiteral("pan-1"));
        QPointer<PanFloatingWindow> floater(
            stack.floatingWindowForTest(QStringLiteral("pan-1")));
        QVERIFY(retained);
        QVERIFY(floater);

        stack.applyLayout(QStringLiteral("2h"),
                          {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QVERIFY(retained);
        QVERIFY(floater.isNull());
        QCOMPARE(stack.panadapter(QStringLiteral("pan-1")), retained.data());
        QVERIFY(!retained->isWindow());
        QCOMPARE(stack.count(), 2);
    }
};

QTEST_MAIN(TestPanadapterStackLayouts)
#include "tst_panadapter_stack_layouts.moc"
