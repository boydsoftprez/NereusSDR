// tests/tst_setup_pages_start_at_top.cpp  (NereusSDR)
//
// R-SPK-21 (problem 3): every Setup page's content starts at the top.
//
// no-port-check: NereusSDR-original UI test fixture. SetupDialog and
// SetupPage are not Thetis ports, so no upstream attribution applies here.
//
// SetupPage builds its content layout with one trailing stretch, and
// addSection() inserts before it. A page that appended straight to
// contentLayout() put its controls after the stretch, so on a tall window
// the stretch took the free space above them and the page opened with its
// controls pushed to the bottom. SetupPage::addContent() inserts before the
// stretch the way addSection() does, and every page uses it.
//
// The sweep walks every leaf SetupDialog registers, by index (labels are not
// unique), shows each one in a 1400 px tall dialog offscreen, and checks the
// two properties that make a page start at the top:
//   1. the first visible item in the content layout sits within 24 px of the
//      content area's top;
//   2. the last item in the content layout is the stretch.
// Pages that replace the stretch on purpose with a tab pane that fills the
// page (NR/ANF, AGC/ALC, Cores) are exempt from the second check only.
//
// Setting NEREUS_SETUP_TOP_CAPTURE_DIR saves a PNG of each page there, for
// before and after evidence; the test does nothing extra without it.

#include <QtTest/QtTest>
#include <QApplication>
#include <QDir>
#include <QLayout>
#include <QScrollArea>
#include <QSet>
#include <QSpacerItem>
#include <QStackedWidget>

#include "core/AppSettings.h"
#include "gui/SetupDialog.h"
#include "gui/SetupPage.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

constexpr int kDialogWidth = 1200;
constexpr int kDialogHeight = 1400;
constexpr int kMaxTopOffsetPx = 24;

// Pages that remove the trailing stretch so a tab pane fills the page.
const QSet<QString> kStretchReplacedPages = {
    QStringLiteral("NereusSDR::NrAnfSetupPage"),
    QStringLiteral("NereusSDR::AgcAlcSetupPage"),
    QStringLiteral("NereusSDR::CoresSetupPage"),
};

// The content widget of a SetupPage: the widget inside its own scroll area
// (the first QScrollArea that is a direct child of the page).
QWidget* contentWidgetOf(SetupPage* page)
{
    const auto areas = page->findChildren<QScrollArea*>(QString(), Qt::FindDirectChildrenOnly);
    for (QScrollArea* area : areas) {
        if (area->widget() != nullptr) {
            return area->widget();
        }
    }
    return nullptr;
}

// Top of the first item in the layout that shows anything, or -1 when no
// item does. Spacers (the stretch, or fixed spacing) are skipped.
int firstVisibleTop(QLayout* layout)
{
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (item == nullptr || item->spacerItem() != nullptr || item->isEmpty()) {
            continue;
        }
        if (QWidget* w = item->widget()) {
            if (!w->isVisible()) {
                continue;
            }
            return w->geometry().top();
        }
        return item->geometry().top();
    }
    return -1;
}

bool lastItemIsStretch(QLayout* layout)
{
    if (layout->count() == 0) {
        return false;
    }
    QLayoutItem* last = layout->itemAt(layout->count() - 1);
    QSpacerItem* spacer = last != nullptr ? last->spacerItem() : nullptr;
    if (spacer == nullptr) {
        return false;
    }
    if (auto* box = qobject_cast<QBoxLayout*>(layout)) {
        if (box->stretch(layout->count() - 1) > 0) {
            return true;
        }
    }
    return (spacer->expandingDirections() & Qt::Vertical) != 0;
}

QString captureName(int index, const QString& label)
{
    QString safe = label;
    for (QChar& c : safe) {
        if (!c.isLetterOrNumber()) {
            c = QLatin1Char('_');
        }
    }
    return QStringLiteral("%1-%2.png").arg(index, 2, 10, QLatin1Char('0')).arg(safe);
}

} // namespace

class TstSetupPagesStartAtTop : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    void every_page_starts_at_the_top_and_ends_with_the_stretch();
};

void TstSetupPagesStartAtTop::every_page_starts_at_the_top_and_ends_with_the_stretch()
{
    RadioModel model;
    SetupDialog dialog(&model);
    dialog.resize(kDialogWidth, kDialogHeight);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    auto* stack = dialog.findChild<QStackedWidget*>();
    QVERIFY(stack != nullptr);

    const QString captureDir = qEnvironmentVariable("NEREUS_SETUP_TOP_CAPTURE_DIR");
    if (!captureDir.isEmpty()) {
        QDir().mkpath(captureDir);
    }

    const QStringList labels = dialog.pageLabelsForTest();
    QVERIFY(!labels.isEmpty());

    QStringList failures;
    int checkedPages = 0;
    for (int index = 0; index < labels.size(); ++index) {
        QWidget* leaf = dialog.realizePageAtForTest(index);
        if (leaf == nullptr) {
            continue;
        }
        stack->setCurrentWidget(leaf);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        if (!captureDir.isEmpty()) {
            leaf->grab().save(QDir(captureDir).filePath(captureName(index, labels[index])));
        }

        QList<SetupPage*> pages;
        if (auto* self = qobject_cast<SetupPage*>(leaf)) {
            pages << self;
        }
        pages << leaf->findChildren<SetupPage*>();

        for (SetupPage* page : pages) {
            if (!page->isVisible()) {
                continue;  // a page in a hidden tab has no geometry to check
            }
            QWidget* content = contentWidgetOf(page);
            if (content == nullptr || content->layout() == nullptr) {
                continue;
            }
            ++checkedPages;
            QLayout* layout = content->layout();
            const QString who = QStringLiteral("%1 (%2)")
                                    .arg(labels[index],
                                         QString::fromLatin1(page->metaObject()->className()));

            const int top = firstVisibleTop(layout);
            if (top > kMaxTopOffsetPx) {
                failures << QStringLiteral("%1: first item at %2 px").arg(who).arg(top);
            }
            const QString cls = QString::fromLatin1(page->metaObject()->className());
            if (!kStretchReplacedPages.contains(cls) && !lastItemIsStretch(layout)) {
                failures << QStringLiteral("%1: last content item is not the stretch").arg(who);
            }
        }
    }

    QVERIFY2(checkedPages > 40,
             qPrintable(QStringLiteral("only %1 pages were checked").arg(checkedPages)));
    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
}

QTEST_MAIN(TstSetupPagesStartAtTop)
#include "tst_setup_pages_start_at_top.moc"
