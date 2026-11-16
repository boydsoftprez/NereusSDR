#include <QtTest/QtTest>
#include "gui/widgets/VfoLevelBar.h"
#include <limits>
#include <QApplication>
#include <QDir>
#include <QFontMetrics>
#include <QPaintEvent>
#include "gui/styles/WorkspacePreview.h"
using namespace NereusSDR;

namespace {
// Counts what a widget is asked to do, and the area it is asked to paint.
class PaintWatch : public QObject {
public:
    explicit PaintWatch(QWidget* watched) { watched->installEventFilter(this); }
    int paints = 0;
    int layouts = 0;
    QRegion painted;
protected:
    bool eventFilter(QObject*, QEvent* event) override {
        if (event->type() == QEvent::Paint) {
            ++paints;
            painted += static_cast<QPaintEvent*>(event)->region();
        } else if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize) {
            ++layouts;
        }
        return false;
    }
};
// Every point where two grabs of the same bar differ, in widget coordinates.
QList<QPoint> differences(const QPixmap& a, const QPixmap& b) {
    const QImage ia = a.toImage().convertToFormat(QImage::Format_ARGB32);
    const QImage ib = b.toImage().convertToFormat(QImage::Format_ARGB32);
    const qreal ratio = a.devicePixelRatio();
    QList<QPoint> points;
    for (int y = 0; y < ia.height(); ++y) {
        for (int x = 0; x < ia.width(); ++x) {
            if (ia.pixel(x, y) != ib.pixel(x, y)) {
                points.append(QPoint(qFloor(x / ratio), qFloor(y / ratio)));
            }
        }
    }
    return points;
}
void capture(QWidget& widget, const QString& stem) {
    const QString dir = qEnvironmentVariable("NEREUS_FLAG_CAPTURE_DIR");
    if (!dir.isEmpty()) {
        QDir().mkpath(dir);
        widget.grab().save(QStringLiteral("%1/%2.png").arg(dir, stem));
    }
}
} // namespace

class TestVfoLevelBar : public QObject {
    Q_OBJECT
private slots:
    void workspaceSizeHintReservesTwoReadableRows() {
        WorkspacePreview::setEnabledForTest(false);
        VfoLevelBar bar;
        QCOMPARE(bar.sizeHint(), QSize(230, 26));
        WorkspacePreview::setEnabledForTest(true);
        QFont ticks = bar.font();
        ticks.setPixelSize(11);
        QFont readout = ticks;
        readout.setBold(true);
        QVERIFY(bar.sizeHint().height() >= 4 + QFontMetrics(ticks, &bar).height()
                                                   + QFontMetrics(readout, &bar).height());
        bar.resize(bar.sizeHint());
        bar.setValue(-130.0f);
        QVERIFY(!bar.grab().isNull());
        WorkspacePreview::setEnabledForTest(false);
        QCOMPARE(bar.sizeHint(), QSize(230, 26));
    }
    // Overlay design D41: the tag prints above the readout; nothing already
    // drawn moves or changes, in either look.
    void sourceTagPrintsAboveTheReadoutAndMovesNothing_data() {
        QTest::addColumn<bool>("workspace");
        QTest::newRow("classic") << false;
        QTest::newRow("workspace") << true;
    }
    void sourceTagPrintsAboveTheReadoutAndMovesNothing() {
        QFETCH(bool, workspace);
        WorkspacePreview::setEnabledForTest(workspace);
        VfoLevelBar bar;
        bar.resize(bar.sizeHint());
        bar.setValue(-120.0f);  // the widest readout the tag could meet
        const QSize size = bar.size();
        const QRect readout = bar.readoutRect();
        const QRect tag = bar.sourceTagRect();
        QVERIFY(tag.bottom() < readout.top());
        QCOMPARE(tag.left(), readout.left());
        // On the readout's column, and the longest tag ends inside the bar.
        QFont bold = bar.font();
        bold.setPixelSize(WorkspacePreview::readableTextPixelSize(8));
        bold.setBold(true);
        QVERIFY(tag.top() >= 0);
        QVERIFY(tag.left() + QFontMetrics(bold, &bar).horizontalAdvance(QStringLiteral("MAX")) <= bar.width());

        QCOMPARE(bar.sourceTag(), QString());
        const QPixmap plain = bar.grab();
        for (const QString& text : {QStringLiteral("MAX"), QStringLiteral("AV")}) {
            bar.setSourceTag(text);
            QCOMPARE(bar.sourceTag(), text);
            const QList<QPoint> changed = differences(plain, bar.grab());
            QVERIFY2(!changed.isEmpty(), qPrintable(text));
            for (const QPoint& point : changed) {
                QVERIFY2(point.y() < readout.top() && point.x() >= tag.left(),
                         qPrintable(QStringLiteral("%1 touched %2,%3").arg(text).arg(point.x()).arg(point.y())));
            }
            QCOMPARE(bar.size(), size);
            QCOMPARE(bar.readoutRect(), readout);
            QCOMPARE(bar.readoutText(), QStringLiteral("-120 dBm"));
            capture(bar, QStringLiteral("bar-%1-%2").arg(text, workspace ? QStringLiteral("workspace") : QStringLiteral("classic")));
        }
        bar.setSourceTag(QString());
        QVERIFY(differences(plain, bar.grab()).isEmpty());
        WorkspacePreview::setEnabledForTest(false);
    }
    // A tag switch alone repaints the bar, and only the bar.
    void sourceTagRepaintsOnlyTheBar() {
        QWidget host;
        host.resize(300, 100);
        auto* bar = new VfoLevelBar(&host);
        bar->setGeometry(10, 10, 230, 26);
        auto* neighbour = new QWidget(&host);
        neighbour->setGeometry(10, 50, 230, 26);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QApplication::processEvents();
        PaintWatch onBar(bar);
        PaintWatch onNeighbour(neighbour);
        PaintWatch onHost(&host);

        bar->setSourceTag(QStringLiteral("MAX"));
        QTRY_VERIFY(onBar.paints > 0);
        QApplication::processEvents();
        QCOMPARE(onNeighbour.paints, 0);
        QVERIFY(bar->geometry().contains(onHost.painted.boundingRect()) || onHost.painted.isEmpty());
        QCOMPARE(onBar.layouts + onHost.layouts, 0);

        // The same tag again asks for nothing.
        const int before = onBar.paints;
        bar->setSourceTag(QStringLiteral("MAX"));
        QApplication::processEvents();
        QCOMPARE(onBar.paints, before);
    }
    void fillFractionAtFloor() {
        VfoLevelBar bar; bar.resize(200, 24);
        bar.setValue(-130.0f);
        QCOMPARE(bar.fillFraction(), 0.0);
    }
    void fillFractionAtCeiling() {
        VfoLevelBar bar; bar.resize(200, 24);
        bar.setValue(-20.0f);
        QCOMPARE(bar.fillFraction(), 1.0);
    }
    void fillFractionAtS9() {
        VfoLevelBar bar; bar.resize(200, 24);
        bar.setValue(-73.0f);  // S9 boundary
        QCOMPARE(bar.fillFraction(), (-73.0 - -130.0) / (-20.0 - -130.0));
    }
    void clampsBelowFloor() {
        VfoLevelBar bar; bar.setValue(-200.0f);
        QCOMPARE(bar.fillFraction(), 0.0);
    }
    void clampsAboveCeiling() {
        VfoLevelBar bar; bar.setValue(10.0f);
        QCOMPARE(bar.fillFraction(), 1.0);
    }
    void colorSwitchesAtS9() {
        VfoLevelBar bar;
        bar.setValue(-74.0f); QCOMPARE(bar.isAboveS9(), false);
        bar.setValue(-73.0f); QCOMPARE(bar.isAboveS9(), true);
    }
    // R-R3-13: the -400 dBm sentinel (and a non-finite level) is no
    // reading: an empty bar and "-- dBm", never "-400 dBm".
    void noReadingShowsDashesAndEmptyBar() {
        VfoLevelBar bar; bar.resize(230, 26);
        bar.setValue(-65.0f);
        QCOMPARE(bar.readoutText(), QStringLiteral("-65 dBm"));
        QVERIFY(bar.hasReading());
        bar.setValue(VfoLevelBar::kNoReadingDbm);
        QVERIFY(!bar.hasReading());
        QCOMPARE(bar.fillFraction(), 0.0);
        QCOMPARE(bar.isAboveS9(), false);
        QCOMPARE(bar.readoutText(), QStringLiteral("-- dBm"));
        QVERIFY(!bar.grab().isNull());
        bar.setValue(std::numeric_limits<float>::quiet_NaN());
        QCOMPARE(bar.readoutText(), QStringLiteral("-- dBm"));
        // The next real level displays exactly as before.
        bar.setValue(-130.0f);
        QVERIFY(bar.hasReading());
        QCOMPARE(bar.readoutText(), QStringLiteral("-130 dBm"));
        QCOMPARE(bar.fillFraction(), 0.0);
        bar.setValue(-20.0f);
        QCOMPARE(bar.fillFraction(), 1.0);
    }
};
QTEST_MAIN(TestVfoLevelBar)
#include "tst_vfo_level_bar.moc"
