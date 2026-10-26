// no-port-check: NereusSDR-original tests for the text overlay token table.
// Modification history (NereusSDR):
//   2026-10-26 - The picker's list has a row cap, opens at its button,
//                 opens leftward near the window's right edge and leaves the
//                 dialog's buttons uncovered. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - The token picker's list scrolls inside the screen.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Token table: case-insensitive names, %precis=N% for the
//                 line, time and date integers, slice-letter tokens, text
//                 that is not a token kept as written, the Thetis text
//                 translator, the offered list and the editor's picker.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QAction>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <QScreen>
#include <QStyle>
#include <QGuiApplication>
#include <QTimeZone>
#include <QToolButton>
#include <memory>

#include "gui/containers/meter_property_editors/TextOverlayItemEditor.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/OverlayTokens.h"
#include "gui/meters/TextOverlayItem.h"

using namespace NereusSDR;

namespace {

VfoSliceState openSlice(int index, qint64 hz, const QString& mode, bool transmitting = false)
{
    VfoSliceState s;
    s.index = index;
    s.frequencyHz = hz;
    s.mode = mode;
    s.filter = QStringLiteral("2.7k");
    s.band = QStringLiteral("20m");
    s.stepHz = 100;
    s.transmitting = transmitting;
    s.open = true;
    return s;
}

OverlayContext contextAt(const QDateTime& utc, const QDateTime& local)
{
    OverlayContext c;
    c.nowUtc = utc;
    c.nowLocal = local;
    return c;
}

} // namespace

class TstTextOverlayTokens : public QObject {
    Q_OBJECT

private slots:
    void namesMatchWithoutRegardToCase()
    {
        OverlayContext c;
        c.slices = {openSlice(0, 14074000, QStringLiteral("USB"))};
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%SliceA_Freq%"), c),
                 resolveOverlayTokens(QStringLiteral("%slicea_freq%"), c));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%SliceA_Freq%"), c), QStringLiteral("14.074.000"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%SLICEA_MODE%"), c), QStringLiteral("USB"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("a%NL%b"), c), QStringLiteral("a\nb"));
    }

    void precisionAppliesToTheWholeLine()
    {
        OverlayContext c;
        c.readings.insert(MeterBinding::TxSwr, 1.234);
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%swr% %precis=1%"), c), QStringLiteral("1.2 "));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%PRECIS=3%%SWR%"), c), QStringLiteral("1.234"));
        // The first valid one counts; every one shows nothing.
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%precis=x%%precis=0%%swr%%precis=4%"), c),
                 QStringLiteral("1"));
        // Kept to 0..20.
        QCOMPARE(overlayTokenPrecision(QStringLiteral("%precis=99%")), 20);
        QCOMPARE(overlayTokenPrecision(QStringLiteral("%precis=-3%")), 0);
        QCOMPARE(overlayTokenPrecision(QStringLiteral("%swr%")), -1);
    }

    void readingsWithoutPrecisionShowOneToSixDecimals()
    {
        OverlayContext c;
        c.readings.insert(MeterBinding::TxSwr, 2.0);
        c.readings.insert(MeterBinding::TxPower, 1.2345678);
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%swr%"), c), QStringLiteral("2.0"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%pwr%"), c), QStringLiteral("1.234568"));
        // A reading the meter has no value for reads 0, as Thetis.
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%mic%"), c), QStringLiteral("0.0"));
    }

    void timeIntegers()
    {
        const QDateTime afternoon(QDate(2026, 10, 25), QTime(13, 5, 9), QTimeZone::UTC);
        const QDateTime morning(QDate(2026, 10, 25), QTime(9, 5, 7), QTimeZone::UTC);
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%time_utc_int%"), contextAt(afternoon, morning)),
                 QStringLiteral("130509"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%time_utc_int%"), contextAt(morning, afternoon)),
                 QStringLiteral("90507"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%time_loc_int%"), contextAt(afternoon, morning)),
                 QStringLiteral("90507"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%time_utc%"), contextAt(afternoon, morning)),
                 QStringLiteral("13:05:09"));
    }

    void dateIntegersAreTheDate()
    {
        const QDateTime utc(QDate(2026, 10, 26), QTime(1, 2, 3), QTimeZone::UTC);
        const QDateTime local(QDate(2026, 10, 25), QTime(21, 2, 3));
        const OverlayContext c = contextAt(utc, local);
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%date_loc_int%"), c), QStringLiteral("20261025"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%date_utc_int%"), c), QStringLiteral("20261026"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%date_loc%"), c), QStringLiteral("Sun 25 Oct 2026"));
    }

    void sliceTokensByLetter()
    {
        OverlayContext c;
        c.slices = {openSlice(0, 7074000, QStringLiteral("LSB")),
                    openSlice(1, 14074000, QStringLiteral("DIGU"), true)};
        c.txSlice = QLatin1Char('B');
        c.mox = true;
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%sliceB_open%|%sliceC_open%|%sliceC_freq%"), c),
                 QStringLiteral("Slice B on||"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%tx_slice% %mox%"), c), QStringLiteral("B MOX"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%sliceA_filter% %sliceA_band% %sliceA_step%"), c),
                 QStringLiteral("2.7k 20m 100"));
        c.mox = false;
        c.txSlice = QChar();
        QCOMPARE(resolveOverlayTokens(QStringLiteral("[%tx_slice%%mox%]"), c), QStringLiteral("[]"));
    }

    void textThatIsNotATokenStays()
    {
        OverlayContext c;
        c.readings.insert(MeterBinding::TxSwr, 1.5);
        // Thetis leaves unknown tokens as written; %1, %20 and %% are text.
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%1 and %20 100%% %foo% %swr%"), c),
                 QStringLiteral("%1 and %20 100%% %foo% 1.5"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%foo%"), c), QStringLiteral("%foo%"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("50%"), c), QStringLiteral("50%"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%sliceA_colour%"), c), QStringLiteral("%sliceA_colour%"));
        // A value is not read again as tokens.
        c.named.insert(QStringLiteral("note"), QStringLiteral("%swr%"));
        QCOMPARE(resolveOverlayTokens(QStringLiteral("%note%"), c), QStringLiteral("%swr%"));
    }

    void thetisTextTranslatesToSliceLetters()
    {
        QCOMPARE(translateThetisOverlayText(QStringLiteral("%VFOA% %rx2%")),
                 QStringLiteral("%sliceA_freq% %sliceB_open%"));
        QCOMPARE(translateThetisOverlayText(QStringLiteral("%mode_vfob% %band_vfoa% %filter_vfoa_name%")),
                 QStringLiteral("%sliceB_mode% %sliceA_band% %sliceA_filter%"));
        QCOMPARE(translateThetisOverlayText(QStringLiteral("%1 %swr% %vfoasub%")),
                 QStringLiteral("%1 %swr% %vfoasub%"));
    }

    void offeredTokensUseSliceLetters()
    {
        const QStringList offered = offeredOverlayTokens();
        for (const QString& token : offered) {
            QVERIFY2(!token.contains(QLatin1String("vfoa"), Qt::CaseInsensitive), qPrintable(token));
            QVERIFY2(!token.contains(QLatin1String("vfob"), Qt::CaseInsensitive), qPrintable(token));
            QVERIFY2(!token.contains(QLatin1String("rx2"), Qt::CaseInsensitive), qPrintable(token));
            QVERIFY(token.startsWith(QLatin1Char('%')) && token.endsWith(QLatin1Char('%')));
        }
        for (const char* expected : {"%sliceA_freq%", "%sliceD_open%", "%tx_slice%", "%mox%",
                                     "%time_utc_int%", "%date_loc_int%", "%swr%", "%nl%", "%group%"}) {
            QVERIFY2(offered.contains(QLatin1String(expected)), expected);
        }
    }

    void itemResolvesItsOwnValuesAndSlices()
    {
        TextOverlayItem overlay;
        overlay.setBindingId(MeterBinding::SignalPeak);
        overlay.setOverlaySlices({openSlice(0, 14074000, QStringLiteral("USB"))});
        overlay.setDisplayGroupText(QStringLiteral("2"));
        overlay.setVariable(QStringLiteral("Call"), QStringLiteral("KG4VCF"));
        overlay.setValue(-73.26);
        overlay.setText1(QStringLiteral("%sliceA_mode% %VALUE% %group% %call% %1"));
        QCOMPARE(overlay.resolvedText1(), QStringLiteral("USB -73.3 2 KG4VCF %1"));
        overlay.setText2(QStringLiteral("%precis=0%%value%"));
        QCOMPARE(overlay.resolvedText2(), QStringLiteral("-73"));
        // Line 1 keeps its own precision.
        QCOMPARE(overlay.resolvedText1(), QStringLiteral("USB -73.3 2 KG4VCF %1"));
    }

    void tokensThatChangeOnTheirOwnRepaint()
    {
        TextOverlayItem overlay;
        overlay.setText1(QStringLiteral("%sliceA_freq%"));
        overlay.setOverlaySlices({openSlice(0, 14074000, QStringLiteral("USB"))});
        QVERIFY(overlay.advanceMeter(0));
        QVERIFY(!overlay.advanceMeter(10));
        overlay.setOverlaySlices({openSlice(0, 14075000, QStringLiteral("USB"))});
        QVERIFY(overlay.advanceMeter(20));
        QCOMPARE(overlay.resolvedText1(), QStringLiteral("14.075.000"));
    }

    void pickerInsertsATokenAndSaves()
    {
        auto item = std::make_unique<TextOverlayItem>();
        item->setText1(QStringLiteral("F "));
        TextOverlayItemEditor editor;
        editor.setItem(item.get());
        auto* picker = editor.findChild<QToolButton*>(QStringLiteral("overlayTokenPicker1"));
        QVERIFY(picker && picker->menu());
        QCOMPARE(picker->text(), QStringLiteral("%"));
        QAction* freq = nullptr;
        for (QAction* action : picker->menu()->actions()) {
            QVERIFY(!action->text().contains(QLatin1String("vfo"), Qt::CaseInsensitive));
            if (action->text() == QLatin1String("%sliceA_freq%")) { freq = action; }
        }
        QVERIFY(freq);
        freq->trigger();
        QCOMPARE(item->text1(), QStringLiteral("F %sliceA_freq%"));
        QVERIFY(editor.findChild<QToolButton*>(QStringLiteral("overlayTokenPicker2")));
    }

    // The picker's list scrolls and stays inside the screen: it is never
    // taller than the screen, whatever the number of tokens.
    void pickerListScrollsInsideTheScreen()
    {
        auto item = std::make_unique<TextOverlayItem>();
        TextOverlayItemEditor editor;
        editor.setItem(item.get());
        auto* picker = editor.findChild<QToolButton*>(QStringLiteral("overlayTokenPicker1"));
        QVERIFY(picker && picker->menu());
        QMenu* menu = picker->menu();
        QVERIFY(menu->style()->styleHint(QStyle::SH_Menu_Scrollable, nullptr, menu));
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect available = screen->availableGeometry();
        // Many tokens: more than a screen of rows.
        QVERIFY2(menu->sizeHint().height() > available.height() || menu->actions().size() * 16 > available.height(),
                 qPrintable(QStringLiteral("%1 tokens, screen %2 px").arg(menu->actions().size()).arg(available.height())));
        for (const QPoint at : {available.topLeft() + QPoint(20, 20),
                                QPoint(available.left() + 20, available.bottom() - 40)}) {
            menu->popup(at);
            QTRY_VERIFY(menu->isVisible());
            const QRect placed = menu->geometry();
            QVERIFY2(placed.height() <= available.height() && available.contains(placed),
                     qPrintable(QStringLiteral("menu %1x%2 at %3,%4; screen %5x%6 at %7,%8")
                                    .arg(placed.width()).arg(placed.height()).arg(placed.x()).arg(placed.y())
                                    .arg(available.width()).arg(available.height()).arg(available.x()).arg(available.y())));
            menu->close();
            QTRY_VERIFY(!menu->isVisible());
        }
    }

    // The picker's list is at most kTokenMenuMaxRows rows and the available
    // screen height, opens at its button (below it, or above when below has
    // no room), scrolls to its last token, and at the parity scene's dialog
    // size (1200x720, the "%" button at 1136,224) stays clear of the
    // dialog's Apply / Cancel / OK row.
    void pickerListIsCappedAndOpensAtItsButton()
    {
        auto item = std::make_unique<TextOverlayItem>();
        QWidget dialog;
        dialog.resize(1200, 720);
        auto* editor = new TextOverlayItemEditor(&dialog);
        editor->setItem(item.get());
        editor->resize(780, editor->sizeHint().height());
        QList<QPushButton*> buttonRow;
        int x = 1030;
        for (const char* text : {"Apply", "Cancel", "OK"}) {
            auto* b = new QPushButton(QString::fromLatin1(text), &dialog);
            b->setGeometry(x, 688, 52, 23);
            buttonRow.append(b);
            x += 56;
        }
        auto* picker = editor->findChild<QToolButton*>(QStringLiteral("overlayTokenPicker1"));
        QVERIFY(picker && picker->menu());
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        editor->move(QPoint(1136, 224) - picker->mapTo(&dialog, QPoint(0, 0)) + editor->pos());
        QCOMPARE(picker->mapTo(&dialog, QPoint(0, 0)), QPoint(1136, 224));
        QMenu* menu = picker->menu();
        QVERIFY(menu->actions().size() > TextOverlayItemEditor::kTokenMenuMaxRows);
        QRect placed;
        bool lastShown = false;
        QTimer::singleShot(0, menu, [&]() {
            QTRY_VERIFY(menu->isVisible());
            placed = menu->geometry();
            QAction* last = menu->actions().constLast();
            // End goes to the last token, scrolling the list to it.
            QTest::keyClick(menu, Qt::Key_End);
            QCoreApplication::processEvents();
            lastShown = menu->rect().contains(menu->actionGeometry(last));
            menu->close();
        });
        picker->showMenu();
        QVERIFY(placed.isValid());
        const QRect first = menu->actionGeometry(menu->actions().constFirst());
        const int rows = TextOverlayItemEditor::kTokenMenuMaxRows;
        // The rows plus the menu's frame and margins (a few pixels).
        QVERIFY2(placed.height() <= rows * first.height() + 12,
                 qPrintable(QStringLiteral("menu %1 px tall, row %2 px").arg(placed.height()).arg(first.height())));
        QVERIFY(placed.height() <= picker->screen()->availableGeometry().height());
        const QRect anchor(picker->mapToGlobal(QPoint(0, 0)), picker->size());
        QVERIFY2(qAbs(placed.top() - (anchor.bottom() + 1)) <= 2 || qAbs(placed.bottom() - (anchor.top() - 1)) <= 2,
                 qPrintable(QStringLiteral("menu %1,%2 %3x%4; button %5,%6 %7x%8").arg(placed.x()).arg(placed.y())
                                .arg(placed.width()).arg(placed.height()).arg(anchor.x()).arg(anchor.y())
                                .arg(anchor.width()).arg(anchor.height())));
        for (QPushButton* b : buttonRow) {
            const QRect r(b->mapToGlobal(QPoint(0, 0)), b->size());
            QVERIFY2(!placed.intersects(r), qPrintable(b->text()));
        }
        // The list stays inside the window and the screen on the right.
        const int windowRight = dialog.mapToGlobal(QPoint(dialog.width() - 1, 0)).x();
        const int screenRight = picker->screen()->availableGeometry().right();
        QVERIFY2(placed.right() <= qMin(windowRight, screenRight),
                 qPrintable(QStringLiteral("menu right %1, window right %2, screen right %3")
                                .arg(placed.right()).arg(windowRight).arg(screenRight)));
        QVERIFY2(lastShown, "the list does not scroll to its last token");
    }

    // With its button near the window's right edge (and the window inside
    // the screen), the list opens leftward, right-aligned to the button, and
    // its right edge stays inside the window.
    void pickerListOpensLeftwardNearTheRightEdge()
    {
        auto item = std::make_unique<TextOverlayItem>();
        QWidget window;
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect room = screen->availableGeometry();
        window.setGeometry(room.left() + 20, room.top() + 20, qMin(700, room.width() - 40), qMin(500, room.height() - 40));
        auto* editor = new TextOverlayItemEditor(&window);
        editor->setItem(item.get());
        editor->resize(window.width(), editor->sizeHint().height());
        auto* picker = editor->findChild<QToolButton*>(QStringLiteral("overlayTokenPicker1"));
        QVERIFY(picker && picker->menu());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // The button 20 px in from the window's right edge, near its top.
        const QPoint target(window.width() - picker->width() - 20, 40);
        editor->move(target - picker->mapTo(&window, QPoint(0, 0)) + editor->pos());
        QCOMPARE(picker->mapTo(&window, QPoint(0, 0)), target);
        QMenu* menu = picker->menu();
        QRect placed;
        QTimer::singleShot(0, menu, [&]() {
            QTRY_VERIFY(menu->isVisible());
            placed = menu->geometry();
            menu->close();
        });
        picker->showMenu();
        QVERIFY(placed.isValid());
        const QRect anchor(picker->mapToGlobal(QPoint(0, 0)), picker->size());
        const int windowRight = window.mapToGlobal(QPoint(window.width() - 1, 0)).x();
        QVERIFY2(placed.right() <= windowRight,
                 qPrintable(QStringLiteral("menu %1..%2, window right %3").arg(placed.left()).arg(placed.right()).arg(windowRight)));
        QCOMPARE(placed.right(), anchor.right());
    }
};

QTEST_MAIN(TstTextOverlayTokens)
#include "tst_text_overlay_tokens.moc"
