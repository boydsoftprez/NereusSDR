// =================================================================
// tests/tst_spectrum_zoom_row_placement.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Workspace visual review Task 26: the pan's zoom row (S, B, minus, plus)
// never covers the pan's button column. On a short pan, where the open
// column reaches the row, the row moves to just right of the column at the
// same height; with the column folded, or on a tall pan, it stays at the
// bottom left where it has always been. It moves again when the column
// folds or opens and when the pan resizes.
//
// Roundtable strip review: on a real pan the row never rises above the
// waterfall's top (into the Roundtable strip or the frequency scale), and
// it follows the waterfall top when the split changes.
// =================================================================
#include <QtTest/QtTest>
#include <QPushButton>

#include "core/AppSettings.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/roundtable/RoundtableTagLayout.h"

using namespace NereusSDR;

namespace {

// The default bottom-left place (SpectrumOverlayPanel::repositionZoomButtons:
// 4 px margin, 32 px left for the frequency scale).
constexpr int kMargin = 4;
constexpr int kFreqScaleH = 32;

struct Pan {
    QWidget spectrum;
    SpectrumOverlayPanel* panel{nullptr};
    explicit Pan(int height)
    {
        spectrum.resize(700, height);
        panel = new SpectrumOverlayPanel(&spectrum);
        panel->move(4, 4);  // MainWindow places it here
        spectrum.show();
        QCoreApplication::processEvents();
    }
    QWidget* zoomRow() const
    {
        for (QPushButton* b : spectrum.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("+") && b->parentWidget()
                && b->parentWidget()->parentWidget() == &spectrum) {
                return b->parentWidget();
            }
        }
        return nullptr;
    }
    QPushButton* collapseButton() const
    {
        for (QPushButton* b : panel->findChildren<QPushButton*>(QString(),
                                                              Qt::FindDirectChildrenOnly)) {
            if (b->text() == QStringLiteral("◀") || b->text() == QStringLiteral("▶")) {
                return b;
            }
        }
        return nullptr;
    }
    // Every shown button of the column, in the spectrum's coordinates.
    QList<QRect> columnButtons() const
    {
        QList<QRect> rects;
        if (panel->isHidden()) {
            return rects;
        }
        for (QPushButton* b : panel->findChildren<QPushButton*>(QString(),
                                                              Qt::FindDirectChildrenOnly)) {
            if (!b->isHidden()) {
                rects.append(b->geometry().translated(panel->geometry().topLeft()));
            }
        }
        return rects;
    }
    QPoint defaultPlace() const
    {
        const QWidget* row = zoomRow();
        return {kMargin,
                std::max(kMargin, spectrum.height() - kFreqScaleH - row->height() - kMargin)};
    }
    QString overlaps() const
    {
        QStringList hits;
        const QRect row = zoomRow()->geometry();
        for (const QRect& r : columnButtons()) {
            if (r.intersects(row)) {
                hits << QStringLiteral("(%1,%2 %3x%4)").arg(r.x()).arg(r.y())
                            .arg(r.width()).arg(r.height());
            }
        }
        return hits.join(QLatin1Char(' '));
    }
};

} // namespace

class TestSpectrumZoomRowPlacement : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void short_pan_row_clears_the_open_column()
    {
        Pan pan(205);
        QVERIFY(pan.zoomRow());
        QVERIFY(!pan.columnButtons().isEmpty());
        const QString hits = pan.overlaps();
        QVERIFY2(hits.isEmpty(), qPrintable(QStringLiteral("row %1 covers %2")
            .arg(QStringLiteral("(%1,%2)").arg(pan.zoomRow()->x()).arg(pan.zoomRow()->y()), hits)));
        // Just right of the column, at the height it always has.
        QCOMPARE(pan.zoomRow()->x(), pan.panel->geometry().right() + 1 + kMargin);
        QCOMPARE(pan.zoomRow()->y(), pan.defaultPlace().y());
    }

    void row_stays_on_a_short_waterfall_under_the_strip()
    {
        // Offscreen there is no GPU: the pan says so once.
        static QtMessageHandler previous = nullptr;
        previous = qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext& context, const QString& message) {
                if (message.startsWith(QLatin1String("QRhiWidget: QRhi is not supported"))) {
                    return;
                }
                previous(type, context, message);
            });
        SpectrumWidget widget;
        widget.setUpdatesEnabled(false);
        widget.resize(700, 600);
        auto* panel = new SpectrumOverlayPanel(&widget);
        panel->move(4, 4);
        widget.show();
        QCoreApplication::processEvents();
        QWidget* row = nullptr;
        for (QPushButton* b : widget.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("+") && b->parentWidget()
                && b->parentWidget()->parentWidget() == &widget) {
                row = b->parentWidget();
            }
        }
        QVERIFY(row);
        const auto fromBottom = [&widget, row]() {
            return std::max(kMargin, widget.height() - (widget.freqScaleHeight() + 4) - row->height() - kMargin);
        };

        widget.setRoundtableStripShown(true);
        widget.setSpectrumFrac(0.85f);
        QVERIFY(widget.roundtableStripLaidOut());
        // The usual place would reach up into the strip: the row stays under it.
        QVERIFY(fromBottom() < widget.waterfallTop() + kMargin);
        QCOMPARE(row->y(), widget.waterfallTop() + kMargin);
        QVERIFY(!row->geometry().intersects(widget.roundtableStripRectForTest()));

        // A taller waterfall: back at the bottom, as it always was.
        widget.setSpectrumFrac(0.40f);
        QVERIFY(fromBottom() >= widget.waterfallTop() + kMargin);
        QCOMPARE(row->y(), fromBottom());
        qInstallMessageHandler(previous);
    }

    void tall_pan_row_is_unchanged()
    {
        Pan pan(700);
        QVERIFY(pan.overlaps().isEmpty());
        QCOMPARE(pan.zoomRow()->pos(), pan.defaultPlace());
    }

    void row_follows_column_fold_and_pan_resize()
    {
        Pan pan(205);
        QPushButton* collapse = pan.collapseButton();
        QVERIFY(collapse);

        // Folded column: the row is back at its usual place.
        QTest::mouseClick(collapse, Qt::LeftButton);
        QCOMPARE(pan.zoomRow()->pos(), pan.defaultPlace());
        QVERIFY(pan.overlaps().isEmpty());

        // Opened again on the short pan: it moves clear again.
        QTest::mouseClick(collapse, Qt::LeftButton);
        QVERIFY(pan.overlaps().isEmpty());
        QCOMPARE(pan.zoomRow()->x(), pan.panel->geometry().right() + 1 + kMargin);

        // The pan grows: the row returns to its usual place.
        pan.spectrum.resize(700, 700);
        QCoreApplication::processEvents();
        QCOMPARE(pan.zoomRow()->pos(), pan.defaultPlace());

        // And shrinks again: clear of the column.
        pan.spectrum.resize(700, 205);
        QCoreApplication::processEvents();
        QVERIFY(pan.overlaps().isEmpty());
        QCOMPARE(pan.zoomRow()->y(), pan.defaultPlace().y());
    }
};

QTEST_MAIN(TestSpectrumZoomRowPlacement)
#include "tst_spectrum_zoom_row_placement.moc"
