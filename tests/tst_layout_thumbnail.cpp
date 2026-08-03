// no-port-check: AetherSDR-derived. Structure from AetherSDR
// PanLayoutDialog.cpp LayoutThumbnail [@c6481cb]; registered in
// docs/attribution/aethersdr-contributor-index.md.
#include <QtTest/QtTest>
#include "gui/widgets/LayoutThumbnail.h"

using namespace NereusSDR;

namespace {
const PanLayoutGeometry& byId(const QString& id) {
    for (const PanLayoutGeometry& g : kPanLayouts) {
        if (g.id == id) { return g; }
    }
    Q_ASSERT(false);
    return kPanLayouts.first();
}
} // namespace

class TstLayoutThumbnail : public QObject {
    Q_OBJECT
private slots:
    void nineLayoutsShip() {
        QCOMPARE(kPanLayouts.size(), 9);
    }

    void singleComesFirst() {
        QCOMPARE(kPanLayouts.first().id, QStringLiteral("1"));
    }

    void noLayoutExceedsFivePans() {
        for (const PanLayoutGeometry& g : kPanLayouts) {
            QVERIFY2(g.panCount <= 5, qPrintable(g.id));
        }
    }

    void panCountMatchesRowSum() {
        for (const PanLayoutGeometry& g : kPanLayouts) {
            int total = 0;
            for (int cols : g.rows) { total += cols; }
            QCOMPARE(total, g.panCount);
        }
    }

    void cellRectsMatchTheGeometry() {
        LayoutThumbnail t(byId(QStringLiteral("3h2")), false, true);
        QCOMPARE(t.cellRects().size(), 5);
    }

    void cellRectsDoNotOverlap() {
        LayoutThumbnail t(byId(QStringLiteral("2x2")), false, true);
        const QVector<QRect> r = t.cellRects();
        QCOMPARE(r.size(), 4);
        for (int i = 0; i < r.size(); ++i) {
            for (int j = i + 1; j < r.size(); ++j) {
                QVERIFY2(!r[i].intersects(r[j]),
                         qPrintable(QStringLiteral("cell %1 overlaps %2")
                                        .arg(i).arg(j)));
            }
        }
    }

    void cellRectsStayInsideTheWidget() {
        // cellRects() must return exactly panCount rects for every layout
        // BEFORE we check containment -- otherwise an empty-vector stub
        // would pass this test vacuously (the inner loop below would never
        // execute). See task-B2-brief.md "Test rigour".
        for (const PanLayoutGeometry& g : kPanLayouts) {
            LayoutThumbnail t(g, false, true);
            const QVector<QRect> cells = t.cellRects();
            QCOMPARE(cells.size(), g.panCount);
            for (const QRect& r : cells) {
                QVERIFY2(t.rect().contains(r), qPrintable(g.id));
            }
        }
    }

    void fixedSizeMatchesAetherSdr() {
        LayoutThumbnail t(byId(QStringLiteral("1")), false, true);
        QCOMPARE(t.size(), QSize(120, 90));
    }
};
QTEST_MAIN(TstLayoutThumbnail)
#include "tst_layout_thumbnail.moc"
