// =================================================================
// tests/tst_native_cursors.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It checks the cursor shapes of
// two widgets; no upstream logic is ported here.
//
// R-R3-49: the desktop window crashed on macOS (2026-09-25, 2026-09-26)
// with EXC_BREAKPOINT in CGImageCreate under QCocoaCursor::createCursorData,
// reached from the container title bar on hover. Qt 6.11.0 draws
// SizeAllCursor, WaitCursor and BusyCursor from its own ICC-tagged PNGs on
// macOS, and QImage::toCGImage() frees the colour space before
// CGImageCreate uses it. Native shapes never take that path.
//
// The operator approved the native hands in place of the four-way move
// cursor: open hand while hovering a draggable thing, closed hand while
// dragging. This test drives the real widgets through hover, press and
// release and asserts every shape they show is a native one.
// scripts/verify-no-image-cursors.py is the source-level half of the guard.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-49 native cursors. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QMouseEvent>
#include <QStandardPaths>

#include "gui/containers/ContainerWidget.h"
#include "gui/widgets/FilterPassbandWidget.h"

using namespace NereusSDR;

namespace {

// Shapes Qt draws from an image on macOS (the Qt 6.11 crash path), plus
// custom bitmap cursors, which go through the same conversion.
bool isImageDrawn(Qt::CursorShape s)
{
    return s == Qt::SizeAllCursor || s == Qt::WaitCursor
        || s == Qt::BusyCursor || s == Qt::BitmapCursor;
}

void sendMouse(QWidget* w, QEvent::Type type, const QPoint& pos)
{
    const Qt::MouseButton button =
        (type == QEvent::MouseMove) ? Qt::NoButton : Qt::LeftButton;
    const Qt::MouseButtons buttons =
        (type == QEvent::MouseButtonRelease || type == QEvent::MouseMove)
            ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent ev(type, QPointF(pos), QPointF(w->mapToGlobal(pos)),
                   button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &ev);
}

// The title label reads "<slice> <first notes line>"; the notes give it a
// marker to find it by.
QLabel* titleLabelOf(ContainerWidget& c)
{
    const QList<QLabel*> labels = c.findChildren<QLabel*>();
    for (QLabel* l : labels) {
        if (l->text().endsWith(QStringLiteral(" CursorProbe"))) {
            return l;
        }
    }
    return nullptr;
}

} // namespace

class TstNativeCursors : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void containerTitleBarUsesHands()
    {
        QWidget host;
        host.resize(800, 600);
        ContainerWidget c(&host);
        c.setDockMode(DockMode::OverlayDocked);
        c.resize(300, 200);
        c.setNotes(QStringLiteral("CursorProbe"));

        QLabel* title = titleLabelOf(c);
        QVERIFY(title != nullptr);
        QWidget* bar = title->parentWidget();
        QVERIFY(bar != nullptr);

        // Hover: open hand on the title label.
        QCOMPARE(title->cursor().shape(), Qt::OpenHandCursor);
        QVERIFY(!isImageDrawn(bar->cursor().shape()));

        // Drag: closed hand on the label and on the bar beside it.
        sendMouse(title, QEvent::MouseButtonPress, QPoint(5, 5));
        QCOMPARE(title->cursor().shape(), Qt::ClosedHandCursor);
        QCOMPARE(bar->cursor().shape(), Qt::ClosedHandCursor);

        // Release: back to the open hand; the bar returns to its own cursor.
        sendMouse(title, QEvent::MouseButtonRelease, QPoint(5, 5));
        QCOMPARE(title->cursor().shape(), Qt::OpenHandCursor);
        QVERIFY(!bar->testAttribute(Qt::WA_SetCursor));

        // Nothing in the container shows an image-drawn shape.
        const QList<QWidget*> all = c.findChildren<QWidget*>();
        for (QWidget* w : all) {
            QVERIFY2(!isImageDrawn(w->cursor().shape()),
                     qPrintable(w->metaObject()->className()));
        }
        QVERIFY(!isImageDrawn(c.cursor().shape()));
    }

    void filterPassbandShiftAreaUsesHands()
    {
        FilterPassbandWidget w;
        w.resize(210, 100);

        // Constructed: open hand.
        QCOMPARE(w.cursor().shape(), Qt::OpenHandCursor);

        // Hover the edge zone: horizontal resize (unchanged).
        sendMouse(&w, QEvent::MouseMove, QPoint(5, 50));
        QCOMPARE(w.cursor().shape(), Qt::SizeHorCursor);

        // Hover the centre (shift) zone: open hand.
        sendMouse(&w, QEvent::MouseMove, QPoint(105, 50));
        QCOMPARE(w.cursor().shape(), Qt::OpenHandCursor);

        // Press in the centre: shift drag, closed hand.
        sendMouse(&w, QEvent::MouseButtonPress, QPoint(105, 50));
        QCOMPARE(w.cursor().shape(), Qt::ClosedHandCursor);
        sendMouse(&w, QEvent::MouseButtonRelease, QPoint(105, 50));
        QCOMPARE(w.cursor().shape(), Qt::OpenHandCursor);

        // Press on an edge: horizontal resize (unchanged).
        sendMouse(&w, QEvent::MouseButtonPress, QPoint(5, 50));
        QCOMPARE(w.cursor().shape(), Qt::SizeHorCursor);
        sendMouse(&w, QEvent::MouseButtonRelease, QPoint(5, 50));
        QVERIFY(!isImageDrawn(w.cursor().shape()));
    }
};

QTEST_MAIN(TstNativeCursors)
#include "tst_native_cursors.moc"
