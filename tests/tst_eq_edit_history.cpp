// no-port-check: NereusSDR-original opaque session-history tests.
#include "gui/widgets/EqEditHistory.h"

#include <QSignalSpy>
#include <QTest>

using NereusSDR::EqEditHistory;

class TestEqEditHistory : public QObject {
    Q_OBJECT
private slots:
    void noOpDoesNotCreateEntry();
    void gestureIsOneEntry();
    void newEditDropsRedo();
    void resetDropsPendingAndHistory();
    void historyRetainsExactBytes();
    void boundedHistory();
    void cancelLeavesCommittedState();
    void availabilityOnlyEmitsOnChange();
};

void TestEqEditHistory::noOpDoesNotCreateEntry() {
    EqEditHistory history;
    history.reset("a");
    history.beginEdit("a");
    history.commitEdit("a");
    QVERIFY(!history.canUndo());
    QVERIFY(!history.undo());
    QVERIFY(!history.redo());

    history.commitEdit("b"); // No begin uses the current baseline.
    QCOMPARE(history.undo(), std::optional<QByteArray>{QByteArray("a")});
    history.beginEdit("a");
    history.commitEdit("a");
    QVERIFY(history.canRedo()); // No-op must preserve the redo branch.
    QCOMPARE(history.redo(), std::optional<QByteArray>{QByteArray("b")});
}

void TestEqEditHistory::gestureIsOneEntry() {
    EqEditHistory history;
    history.reset("before");
    history.beginEdit("before");
    history.beginEdit("intermediate-1");
    history.beginEdit("intermediate-2");
    history.commitEdit("after");
    QCOMPARE(history.undo(), std::optional<QByteArray>{QByteArray("before")});
    QVERIFY(!history.canUndo());
    QCOMPARE(history.redo(), std::optional<QByteArray>{QByteArray("after")});
    QVERIFY(!history.canRedo());
}

void TestEqEditHistory::newEditDropsRedo() {
    EqEditHistory history;
    history.reset("a");
    history.commitEdit("b");
    history.commitEdit("c");
    QCOMPARE(history.undo(), std::optional<QByteArray>{QByteArray("b")});
    history.commitEdit("d");
    QVERIFY(!history.canRedo());
    QVERIFY(!history.redo());
    QCOMPARE(history.undo(), std::optional<QByteArray>{QByteArray("b")});
    QCOMPARE(history.undo(), std::optional<QByteArray>{QByteArray("a")});
    QCOMPARE(history.redo(), std::optional<QByteArray>{QByteArray("b")});
    QCOMPARE(history.redo(), std::optional<QByteArray>{QByteArray("d")});
}

void TestEqEditHistory::resetDropsPendingAndHistory() {
    EqEditHistory history;
    history.reset("old-profile");
    history.commitEdit("old-edit");
    history.undo();
    history.beginEdit("old-pending");
    history.reset("new-profile");
    QVERIFY(!history.canUndo());
    QVERIFY(!history.canRedo());
    history.commitEdit("new-edit");
    QCOMPARE(history.undo(), std::optional<QByteArray>{QByteArray("new-profile")});
    QVERIFY(!history.undo());
}

void TestEqEditHistory::historyRetainsExactBytes() {
    EqEditHistory history;
    QByteArray before = QByteArray::fromHex("0001ff803ff0000000000001");
    QByteArray after = QByteArray::fromHex("0001ff803ff0000000000002");
    const QByteArray expectedBefore = before;
    const QByteArray expectedAfter = after;
    history.reset(before);
    history.beginEdit(before);
    before.fill('x');
    history.commitEdit(after);
    after.fill('y');
    QCOMPARE(history.undo(), std::optional<QByteArray>{expectedBefore});
    QCOMPARE(history.redo(), std::optional<QByteArray>{expectedAfter});
}

void TestEqEditHistory::boundedHistory() {
    EqEditHistory history;
    history.reset("0");
    for (int i = 1; i <= 101; ++i) {
        history.commitEdit(QByteArray::number(i));
    }
    for (int i = 100; i >= 1; --i) {
        const auto state = history.undo();
        QVERIFY(state.has_value());
        QCOMPARE(*state, QByteArray::number(i));
    }
    QVERIFY(!history.canUndo());
    QVERIFY(!history.undo());
    for (int i = 2; i <= 101; ++i) {
        const auto state = history.redo();
        QVERIFY(state.has_value());
        QCOMPARE(*state, QByteArray::number(i));
    }
    QVERIFY(!history.redo());
}

void TestEqEditHistory::cancelLeavesCommittedState() {
    EqEditHistory history;
    history.reset("a");
    history.commitEdit("b");
    history.beginEdit("uncommitted");
    history.cancelEdit();
    history.cancelEdit();
    history.commitEdit("c");
    QCOMPARE(history.undo(), std::optional<QByteArray>{QByteArray("b")});
    QCOMPARE(history.undo(), std::optional<QByteArray>{QByteArray("a")});
}

void TestEqEditHistory::availabilityOnlyEmitsOnChange() {
    EqEditHistory history;
    QSignalSpy availability(&history, &EqEditHistory::availabilityChanged);
    history.reset("a");
    history.beginEdit("a");
    history.cancelEdit();
    history.commitEdit("a");
    QCOMPARE(availability.count(), 0);
    history.commitEdit("b");
    QCOMPARE(availability.count(), 1);
    QCOMPARE(availability.at(0), QVariantList({true, false}));
    history.commitEdit("c");
    QCOMPARE(availability.count(), 1);
    history.undo();
    QCOMPARE(availability.count(), 2);
    QCOMPARE(availability.at(1), QVariantList({true, true}));
    history.undo();
    QCOMPARE(availability.count(), 3);
    QCOMPARE(availability.at(2), QVariantList({false, true}));
    history.undo();
    QCOMPARE(availability.count(), 3);
    history.reset("new-profile");
    QCOMPARE(availability.count(), 4);
    QCOMPARE(availability.at(3), QVariantList({false, false}));
}

QTEST_GUILESS_MAIN(TestEqEditHistory)
#include "tst_eq_edit_history.moc"
