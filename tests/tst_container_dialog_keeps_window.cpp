// no-port-check: NereusSDR-original test that adding content keeps the dialog's window.
// Modification history (NereusSDR):
//   2026-11-06 - Issue 743: run the real QDialog::exec(), the shape of Qt's own
//                 execBlocksEvenIfRecreated test for QTBUG-139109, and check the
//                 main window gets its input back. J.J. Boyd (KG4VCF),
//                 AI-assisted via Claude Code.
//   2026-11-05 - Issue 743: the container dialog keeps its native window when
//                 the first meter is added. J.J. Boyd (KG4VCF), AI-assisted via
//                 Claude Code.
//
// What each platform proves. MeterWidget is a native leaf on macOS and Windows,
// so Qt never recreates the dialog there and the dialog cases pass with or
// without the anchor. They can only fail on Linux, and the Linux legs are the
// only CI that runs this test: macOS CI builds no tests, and Windows CI builds
// only its named audio tests. On Linux Qt's check runs under the offscreen
// platform too: the QRhiWidget constructor enables the RHI config whether or
// not the platform has RhiBasedRendering, and only the render to texture setup
// depends on that capability. The qt* cases use plain non-native QRhiWidgets,
// so they exercise Qt's behaviour wherever the test is built, a Mac developer
// build included.
//
// Which Qt can show the failure. Every Qt still recreates the window without
// the anchor, 6.12.0 included, so the checks that the window survives go red on
// any Linux Qt. QTBUG-139109 (exec() returning early after a recreate) is fixed
// in Qt 6.11.1 and 6.12.0, so only an older Qt can fail the check that exec()
// kept blocking: 6.8.3 on the Linux CI legs.
#include <QtTest>
#include <QDialog>
#include <QGuiApplication>
#include <QListWidget>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <memory>
#ifdef NEREUS_GPU_SPECTRUM
#include <QRhiWidget>
#endif

#include "core/AppSettings.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/BarPresetItem.h"

using namespace NereusSDR;

namespace {

bool clickAdd(ContainerSettingsDialog& dialog, const QString& typeId)
{
    auto* list = dialog.findChild<QListWidget*>("containerAvailableContents");
    if (!list) {
        return false;
    }
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->data(Qt::UserRole).toString() != typeId) {
            continue;
        }
        list->setCurrentRow(i);
        for (auto* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Add →")) {
                button->click();
                return true;
            }
        }
    }
    return false;
}

// Enough of a workspace to open the real dialog on container "A".
struct DialogFixture {
    QTemporaryDir dir;
    AppSettings settings{dir.filePath("settings")};
    ContainerWorkspaceStore store{settings};
    ContainerContentRegistry registry;
    MeterPoller poller;
    QWidget root;
    QSplitter splitter{&root};
    ContainerManager manager{&root, &splitter};

    bool commit(const QStringList& existing)
    {
        manager.setWorkspaceAdapter(&store, &registry);
        manager.setPreviewPoller(&poller);
        WorkspaceDocument document;
        document.mainContainerId = "A";
        ContainerDocument container;
        container.id = "A";
        container.layout = ContentLayout::VerticalStack;
        for (const QString& typeId : existing) {
            container.contents.append(registry.makeEntry(typeId));
        }
        document.containers = {container};
        return manager.commitWorkspace(document, 0).status == CommitStatus::Saved;
    }
};

// Stands in for the main window: counts the presses that reach it, which is
// what QGuiApplication drops while a modal window blocks it.
class PressCounter : public QWidget
{
public:
    PressCounter()
    {
        resize(200, 200);
        show();
        QCoreApplication::processEvents();
    }

    bool pressReachesIt()
    {
        const int before = m_presses;
        QTest::mouseClick(windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
        QCoreApplication::processEvents();
        return m_presses > before;
    }

protected:
    void mousePressEvent(QMouseEvent*) override { ++m_presses; }

private:
    int m_presses{0};
};

struct ExecRun {
    bool reachedAccept{false};
    int result{-1};
};

// Runs dialog.exec(), calls act() from inside its event loop, and accepts a few
// turns of the loop later. No clock is involved: an exec() that returns early
// has left the loop, so the timer never gets to the accept.
template <typename Act>
ExecRun runExec(QDialog& dialog, Act act)
{
    constexpr int kAcceptOnTurn = 5;
    ExecRun run;
    int turn = 0;
    QTimer timer;
    timer.setInterval(0);
    QObject::connect(&timer, &QTimer::timeout, &dialog, [&] {
        ++turn;
        if (turn == 1) {
            act();
        } else if (turn == kAcceptOnTurn) {
            timer.stop();
            run.reachedAccept = true;
            dialog.accept();
        }
    });
    timer.start();
    run.result = dialog.exec();
    timer.stop();
    return run;
}

} // namespace

class TstContainerDialogKeepsWindow : public QObject
{
    Q_OBJECT

private slots:
#ifdef NEREUS_GPU_SPECTRUM
    // The failure this test file guards: a non-native RHI child parented into
    // a shown raster window replaces that window.
    void qtRecreatesARasterWindowForANonNativeRhiChild()
    {
        QDialog dialog;
        dialog.show();
        QCoreApplication::processEvents();
        const QPointer<QWindow> before = dialog.windowHandle();
        QVERIFY(before);
        QCOMPARE(before->surfaceType(), QSurface::RasterSurface);

        auto child = std::make_unique<QRhiWidget>();
        child->setParent(&dialog);
        QVERIFY2(!before, "Qt kept the window; the mechanism behind issue 743 is gone");
        child.release();
    }

    // The fix's mechanism, nested as the preview is: a hidden RHI child placed
    // before the first show gives the window its surface, so a later child
    // does not replace it.
    void qtKeepsTheWindowWhenAHiddenRhiChildCameFirst()
    {
        QDialog dialog;
        auto* layout = new QVBoxLayout(&dialog);
        auto* holder = new QWidget;
        (new QRhiWidget(holder))->hide();
        layout->addWidget(holder);
        dialog.show();
        QCoreApplication::processEvents();
        const QPointer<QWindow> before = dialog.windowHandle();
        QVERIFY(before);
        QVERIFY(before->surfaceType() != QSurface::RasterSurface);

        auto child = std::make_unique<QRhiWidget>();
        child->setParent(holder);
        QVERIFY(before);
        QCOMPARE(dialog.windowHandle(), before.data());
        child.release();
    }

    // QTBUG-139109 itself, and what it leaves behind: the recreate ends exec()
    // early, and the main window's input stays blocked after the dialog is
    // gone. This is what the reporter's "dialog vanishes, app blocks" would be.
    // A Qt with the fix has nothing to show here, and neither does a run with
    // QT_WIDGETS_RHI=1, which forces RHI from the start so nothing is recreated.
    void qtExecReturnsEarlyAndLeavesTheBlockAfterARecreate()
    {
        PressCounter mainWindow;
        QVERIFY2(mainWindow.pressReachesIt(), "a press has to reach an unblocked window");

        auto dialog = std::make_unique<QDialog>(&mainWindow);
        const ExecRun run = runExec(*dialog, [&] {
            auto child = std::make_unique<QRhiWidget>();
            child->setParent(dialog.get());
            child.release();
        });
        if (run.reachedAccept) {
            QSKIP("exec() kept blocking: this Qt has the fix for QTBUG-139109, or QT_WIDGETS_RHI is set");
        }
        QCOMPARE(run.result, int(QDialog::Rejected));

        dialog.reset();
        QCoreApplication::processEvents();
        QVERIFY2(!mainWindow.pressReachesIt(),
                 "the main window got its input back, so the stuck block is not what hangs issue 743");
    }

    // The fix's mechanism under exec(): with a hidden RHI child there from the
    // start, adding another does not end exec() or strand the main window.
    void qtExecKeepsBlockingWhenAHiddenRhiChildCameFirst()
    {
        PressCounter mainWindow;
        auto dialog = std::make_unique<QDialog>(&mainWindow);
        auto* layout = new QVBoxLayout(dialog.get());
        auto* holder = new QWidget;
        (new QRhiWidget(holder))->hide();
        layout->addWidget(holder);

        const ExecRun run = runExec(*dialog, [&] {
            auto child = std::make_unique<QRhiWidget>();
            child->setParent(holder);
            child.release();
        });
        QVERIFY2(run.reachedAccept, "exec() returned before accept()");
        QCOMPARE(run.result, int(QDialog::Accepted));

        dialog.reset();
        QCoreApplication::processEvents();
        QVERIFY(mainWindow.pressReachesIt());
    }
#endif

    void addingAMeterKeepsTheDialogsWindow_data()
    {
        addExistingContentRows();
    }

    void addingAMeterKeepsTheDialogsWindow()
    {
        QFETCH(QStringList, existing);
        DialogFixture fixture;
        QVERIFY(fixture.commit(existing));
        ContainerManager& manager = fixture.manager;

        ContainerSettingsDialog dialog(manager.container("A"), nullptr, &manager);
        auto* preview = dialog.findChild<ContainerPreviewWidget*>();
        QVERIFY(preview);
        // The state the report describes: no meter before the first show.
        QCOMPARE(preview->findChildren<MeterWidget*>().size(), 0);

        dialog.show();
        QCoreApplication::processEvents();
        const QPointer<QWindow> before = dialog.windowHandle();
        QVERIFY(before);

        QVERIFY(clickAdd(dialog, BarPresetItem::typeIdForFlavor(QStringLiteral("AlcGroup"))));
        QCoreApplication::processEvents();

        // The add reached the preview, as a meter.
        QCOMPARE(preview->findChildren<MeterWidget*>().size(), 1);
        QVERIFY2(before, "the dialog's window was destroyed by adding the first meter");
        QCOMPARE(dialog.windowHandle(), before.data());
        QVERIFY(dialog.isVisible());

#if defined(NEREUS_GPU_SPECTRUM) && !defined(Q_OS_MAC) && !defined(Q_OS_WIN)
        // The anchor: there from the start, hidden, outside the layout, and
        // never counted as a meter or swept away with the old views.
        QVERIFY(before->surfaceType() != QSurface::RasterSurface);
        auto* anchor = preview->findChild<QRhiWidget*>("containerPreviewRhiAnchor");
        QVERIFY(anchor);
        QVERIFY(anchor->isHidden());
        QCOMPARE(preview->layout()->indexOf(anchor), -1);
        QVERIFY(!qobject_cast<MeterWidget*>(anchor));
        ContainerDocument emptied = preview->document();
        emptied.contents.clear();
        preview->setDocument(emptied);
        QCOMPARE(preview->findChild<QRhiWidget*>("containerPreviewRhiAnchor"), anchor);
        QVERIFY(anchor->isHidden());
#endif
        dialog.reject();
    }

    // The issue's own path: the real dialog in the real exec(), Add clicked from
    // inside it. Before the anchor, on a Qt without the QTBUG-139109 fix and
    // where meters are not native, exec() returned Rejected at this click and
    // Containers > New then destroyed the container and the dialog.
    void addingAMeterKeepsExecBlocking_data()
    {
        addExistingContentRows();
    }

    void addingAMeterKeepsExecBlocking()
    {
        QFETCH(QStringList, existing);
        PressCounter mainWindow;
        DialogFixture fixture;
        QVERIFY(fixture.commit(existing));

        ContainerSettingsDialog dialog(fixture.manager.container("A"), nullptr, &fixture.manager);
        auto* preview = dialog.findChild<ContainerPreviewWidget*>();
        QVERIFY(preview);
        QCOMPARE(preview->findChildren<MeterWidget*>().size(), 0);

        QPointer<QWindow> before;
        bool added = false;
        const ExecRun run = runExec(dialog, [&] {
            before = dialog.windowHandle();
            added = clickAdd(dialog, BarPresetItem::typeIdForFlavor(QStringLiteral("AlcGroup")));
        });

        QVERIFY(added);
        QCOMPARE(preview->findChildren<MeterWidget*>().size(), 1);
        QVERIFY2(run.reachedAccept, "exec() returned at the Add click, before accept()");
        QCOMPARE(run.result, int(QDialog::Accepted));
        QVERIFY2(before, "the dialog's window was destroyed by adding the first meter");
        QCOMPARE(dialog.windowHandle(), before.data());

        // accept() hid the dialog, so the main window takes input again.
        QVERIFY(!QGuiApplication::modalWindow());
        QVERIFY(mainWindow.pressReachesIt());
    }

private:
    static void addExistingContentRows()
    {
        QTest::addColumn<QStringList>("existing");
        // Containers > New: nothing to preview before the first Add.
        QTest::newRow("new container") << QStringList{};
        // The Main Panel: its applets preview as plain tiles, no MeterWidget.
        QTest::newRow("main panel tiles") << QStringList{QStringLiteral("applet:s_meter"),
                                                         QStringLiteral("applet:rx")};
    }
};

QTEST_MAIN(TstContainerDialogKeepsWindow)
#include "tst_container_dialog_keeps_window.moc"
