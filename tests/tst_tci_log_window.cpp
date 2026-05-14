// no-port-check: NereusSDR-original test for TciLogWindow append + filter.
//
// Phase 3J-1 closeout Item 2 (2026-05-12) verification:
//   - appendEntry adds to the log view
//   - direction filter hides mismatched entries
//   - clear() empties the view
//   - pause stops appends until released
//
// Headless: TciLogWindow is a QDialog, but the test never show()s it.  Qt's
// off-screen platform is fine for QPlainTextEdit operations.

#include <QtTest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QDateTime>

#include "gui/setup/TciLogWindow.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestTciLogWindow : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void append_adds_line();
    void filter_in_only_drops_out_direction();
    void filter_out_only_drops_in_direction();
    void clear_empties_view();
    void pause_blocks_appends();
};

void TestTciLogWindow::initTestCase()
{
    // Pin AppSettings so persisted geometry / autoscroll defaults don't
    // bleed across test runs.
    AppSettings::instance().setValue(
        QStringLiteral("TciLogWindowAutoScroll"), QStringLiteral("True"));
}

static QPlainTextEdit* findView(QDialog* win)
{
    return win->findChild<QPlainTextEdit*>();
}

static QPushButton* findPause(QDialog* win)
{
    const auto buttons = win->findChildren<QPushButton*>();
    for (auto* b : buttons) {
        if (b->isCheckable()) { return b; }
    }
    return nullptr;
}

static QPushButton* findClear(QDialog* win)
{
    const auto buttons = win->findChildren<QPushButton*>();
    for (auto* b : buttons) {
        if (!b->isCheckable()) { return b; }
    }
    return nullptr;
}

static QComboBox* findFilter(QDialog* win)
{
    return win->findChild<QComboBox*>();
}

void TestTciLogWindow::append_adds_line()
{
    TciLogWindow win;
    auto* view = findView(&win);
    QVERIFY(view);
    QVERIFY(view->toPlainText().isEmpty());

    const qint64 t = QDateTime::currentMSecsSinceEpoch();
    win.appendEntry(QStringLiteral("in"),
                    QStringLiteral("127.0.0.1:50001"),
                    QStringLiteral("trx:0,true"),
                    t);

    // QPlainTextEdit::appendPlainText reuses the empty initial block on
    // first call, so we assert via toPlainText().contains rather than
    // blockCount.
    QVERIFY(!view->toPlainText().isEmpty());
    QVERIFY(view->toPlainText().contains(QStringLiteral("trx:0,true")));
    QVERIFY(view->toPlainText().contains(QStringLiteral("in")));
    QVERIFY(view->toPlainText().contains(QStringLiteral("127.0.0.1:50001")));
}

void TestTciLogWindow::filter_in_only_drops_out_direction()
{
    TciLogWindow win;
    auto* view = findView(&win);
    auto* filter = findFilter(&win);
    QVERIFY(view);
    QVERIFY(filter);
    // Index 1 = "In only"
    filter->setCurrentIndex(1);

    const qint64 t = QDateTime::currentMSecsSinceEpoch();
    win.appendEntry(QStringLiteral("in"),  QStringLiteral("peer1"),
                    QStringLiteral("vfo:0,0,14250000"), t);
    win.appendEntry(QStringLiteral("out"), QStringLiteral("peer1"),
                    QStringLiteral("trx:0,true"),       t);

    const QString text = view->toPlainText();
    QVERIFY(text.contains(QStringLiteral("vfo:0,0,14250000")));
    QVERIFY(!text.contains(QStringLiteral("trx:0,true")));
}

void TestTciLogWindow::filter_out_only_drops_in_direction()
{
    TciLogWindow win;
    auto* view = findView(&win);
    auto* filter = findFilter(&win);
    QVERIFY(view);
    QVERIFY(filter);
    filter->setCurrentIndex(2);  // Out only

    const qint64 t = QDateTime::currentMSecsSinceEpoch();
    win.appendEntry(QStringLiteral("in"),  QStringLiteral("peer1"),
                    QStringLiteral("vfo:0,0,14250000"), t);
    win.appendEntry(QStringLiteral("out"), QStringLiteral("peer1"),
                    QStringLiteral("trx:0,true"),       t);

    const QString text = view->toPlainText();
    QVERIFY(!text.contains(QStringLiteral("vfo:0,0,14250000")));
    QVERIFY(text.contains(QStringLiteral("trx:0,true")));
}

void TestTciLogWindow::clear_empties_view()
{
    TciLogWindow win;
    auto* view = findView(&win);
    auto* clearBtn = findClear(&win);
    QVERIFY(view);
    QVERIFY(clearBtn);

    const qint64 t = QDateTime::currentMSecsSinceEpoch();
    win.appendEntry(QStringLiteral("in"), QStringLiteral("peer1"),
                    QStringLiteral("trx:0,true"), t);
    QVERIFY(!view->toPlainText().isEmpty());

    clearBtn->click();
    QVERIFY(view->toPlainText().isEmpty());
}

void TestTciLogWindow::pause_blocks_appends()
{
    TciLogWindow win;
    auto* view = findView(&win);
    auto* pauseBtn = findPause(&win);
    QVERIFY(view);
    QVERIFY(pauseBtn);

    pauseBtn->setChecked(true);  // pause

    const qint64 t = QDateTime::currentMSecsSinceEpoch();
    win.appendEntry(QStringLiteral("in"), QStringLiteral("peer1"),
                    QStringLiteral("trx:0,true"), t);

    // While paused, the entry is dropped — view stays empty.
    QVERIFY(view->toPlainText().isEmpty());

    pauseBtn->setChecked(false);  // resume
    win.appendEntry(QStringLiteral("in"), QStringLiteral("peer1"),
                    QStringLiteral("trx:0,true"), t);
    QVERIFY(!view->toPlainText().isEmpty());
    QVERIFY(view->toPlainText().contains(QStringLiteral("trx:0,true")));
}

QTEST_MAIN(TestTciLogWindow)
#include "tst_tci_log_window.moc"
