// =================================================================
// tests/tst_title_bar.cpp  (NereusSDR)
// =================================================================
//
// Smoke tests for TitleBar — the 32 px host strip that holds the
// QMenuBar and MasterOutputWidget. Phase 3O Sub-Phase 10 Task 10c.
//
// Coverage:
//   1. constructsWithoutCrash — TitleBar builds against a real
//      AudioEngine without crashing.
//   2. setMenuBarInserts — setMenuBar() actually re-parents the
//      supplied QMenuBar into the strip at position 0.
//   3. masterOutputAccessible — masterOutput() returns the embedded
//      MasterOutputWidget.
//   4. fixedHeight32 — strip height is pinned to 32 px.
//   5. featureButtonEmitsSignal — clicking the 💡 feature button emits
//      featureRequestClicked() exactly once (Task 10d).
//
// Live UI smoke (hosted-inside-QMainWindow + menu bar re-parenting
// visuals) is the canonical verify; these tests only guard the
// construction contract, not appearance.
//
// Design spec: docs/architecture/2026-04-19-vax-design.md §6.3 + §7.3.
// =================================================================

#include <QtTest/QtTest>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AudioEngine.h"
#include "gui/TitleBar.h"
#include "gui/widgets/MasterOutputWidget.h"

using namespace NereusSDR;

class TstTitleBar : public QObject {
    Q_OBJECT

private slots:

    // ── 1. Constructs without crashing ─────────────────────────────────────

    void constructsWithoutCrash() {
        AudioEngine engine;
        TitleBar bar(&engine);
        QVERIFY(bar.masterOutput() != nullptr);
    }

    // ── 2. setMenuBar re-parents the menu bar into the strip ──────────────

    void setMenuBarInserts() {
        AudioEngine engine;
        TitleBar bar(&engine);

        auto* mb = new QMenuBar;
        QMenu* dummy = mb->addMenu(QStringLiteral("Dummy"));
        dummy->addAction(QStringLiteral("Nothing"));

        bar.setMenuBar(mb);

        // After setMenuBar() the menu bar must be reachable as a child
        // of the title bar AND parented to it.
        QMenuBar* found = bar.findChild<QMenuBar*>();
        QVERIFY(found != nullptr);
        QCOMPARE(found, mb);
        QCOMPARE(mb->parentWidget(), &bar);
    }

    // ── 3. masterOutput accessor ───────────────────────────────────────────

    void masterOutputAccessible() {
        AudioEngine engine;
        TitleBar bar(&engine);

        MasterOutputWidget* m = bar.masterOutput();
        QVERIFY(m != nullptr);
        // Round-trip check: the widget sits inside the title bar tree.
        QVERIFY(bar.findChild<MasterOutputWidget*>() == m);
    }

    // ── 4. Fixed strip height of 32 px ─────────────────────────────────────

    void fixedHeight32() {
        AudioEngine engine;
        TitleBar bar(&engine);

        // setFixedHeight() pins minimumHeight == maximumHeight == 32. The
        // widget's visible height only resolves after show() / polish, but
        // the height contract is already locked in by setFixedHeight() at
        // construction time and is the canonical check.
        QCOMPARE(bar.minimumHeight(), 32);
        QCOMPARE(bar.maximumHeight(), 32);

        // adjustSize() is enough to resolve the size hint without needing a
        // windowing system (QTEST_MAIN uses a QApplication so this is safe).
        bar.adjustSize();
        QCOMPARE(bar.height(), 32);
    }

    // ── 5. Feature button emits featureRequestClicked ──────────────────────

    void featureButtonEmitsSignal() {
        AudioEngine engine;
        TitleBar bar(&engine);

        // The 💡 button is a QPushButton with objectName "featureButton",
        // set in TitleBar's constructor (Task 10d).
        auto* btn = bar.findChild<QPushButton*>(QStringLiteral("featureButton"));
        QVERIFY(btn != nullptr);

        QSignalSpy spy(&bar, &TitleBar::featureRequestClicked);
        QVERIFY(spy.isValid());

        btn->click();
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TstTitleBar)
#include "tst_title_bar.moc"
