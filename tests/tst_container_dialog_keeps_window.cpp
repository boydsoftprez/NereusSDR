// no-port-check: NereusSDR-original test that adding content keeps the dialog's window.
// Modification history (NereusSDR):
//   2026-11-05 - Issue 743: the container dialog keeps its native window when
//                 the first meter is added. J.J. Boyd (KG4VCF), AI-assisted via
//                 Claude Code.
//
// What each platform proves. MeterWidget is a native leaf on macOS and Windows,
// so Qt never recreates the dialog there and the dialog cases pass with or
// without the fix. They can only fail on Linux. There Qt's check runs under the
// offscreen platform too: QRhiWidgetPrivate::init() enables the RHI config
// whether or not the platform has RhiBasedRendering, and only the render to
// texture setup depends on that capability. The two qt* cases use plain
// non-native QRhiWidgets, so they exercise Qt's behaviour on every platform.
#include <QtTest>
#include <QDialog>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QTemporaryDir>
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
#endif

    void addingAMeterKeepsTheDialogsWindow_data()
    {
        QTest::addColumn<QStringList>("existing");
        // Containers > New: nothing to preview before the first Add.
        QTest::newRow("new container") << QStringList{};
        // The Main Panel: its applets preview as plain tiles, no MeterWidget.
        QTest::newRow("main panel tiles") << QStringList{QStringLiteral("applet:s_meter"),
                                                         QStringLiteral("applet:rx")};
    }

    void addingAMeterKeepsTheDialogsWindow()
    {
        QFETCH(QStringList, existing);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings"));
        ContainerWorkspaceStore store(settings);
        ContainerContentRegistry registry;
        MeterPoller poller;
        QWidget root;
        QSplitter splitter(&root);
        ContainerManager manager(&root, &splitter);
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
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);

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
};

QTEST_MAIN(TstContainerDialogKeepsWindow)
#include "tst_container_dialog_keeps_window.moc"
