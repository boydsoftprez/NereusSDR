// no-port-check: NereusSDR-original routing test; the fade rules it checks are
// cited where they live (FadeCoverItem.cpp, ButtonBoxItem.cpp).
// Modification history (NereusSDR):
//   2026-10-25 - Transmit state reaches fade covers, VFO displays and
//                 button grids through ContainerManager::setTransmitting,
//                 on change and for items added or loaded while
//                 transmitting. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include <QtTest>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QSplitter>
#include <QTemporaryDir>
#include <optional>
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/ButtonBoxItem.h"
#include "gui/meters/FadeCoverItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
using namespace NereusSDR;

namespace {

constexpr int kW = 120;
constexpr int kH = 60;

// True when the cover leaves paint on a transparent canvas.
bool coverPaints(FadeCoverItem& cover)
{
    QImage img(kW, kH, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    cover.paint(p, kW, kH);
    p.end();
    return img.pixelColor(kW / 2, kH / 2).alpha() > 0;
}

QImage renderVfo(VfoDisplayItem& vfo)
{
    QImage img(200, 40, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    vfo.paint(p, 200, 40);
    p.end();
    return img;
}

bool pressAt(ButtonBoxItem& buttons, const QPointF& pos)
{
    QMouseEvent press(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    return buttons.handleMousePress(&press, kW, kH);
}

// A point that lands on a button while the grid takes clicks.
std::optional<QPointF> buttonPoint(ButtonBoxItem& buttons)
{
    for (int y = 1; y < kH; y += 2) {
        for (int x = 1; x < kW; x += 2) {
            if (pressAt(buttons, QPointF(x, y))) {
                return QPointF(x, y);
            }
        }
    }
    return std::nullopt;
}

struct Items {
    QVector<FadeCoverItem*> covers;
    QVector<VfoDisplayItem*> vfos;
    QVector<ButtonBoxItem*> buttons;
};

Items itemsIn(ContainerContentHost* host)
{
    Items found;
    if (!host) {
        return found;
    }
    for (MeterWidget* meter : host->meterSurfaces()) {
        for (MeterItem* item : meter->items()) {
            if (auto* cover = qobject_cast<FadeCoverItem*>(item)) { found.covers.append(cover); }
            if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) { found.vfos.append(vfo); }
            if (auto* grid = qobject_cast<ButtonBoxItem*>(item)) { found.buttons.append(grid); }
        }
    }
    return found;
}

ContainerDocument containerWith(const QString& id, const QVector<ContentEntry>& contents)
{
    ContainerDocument c;
    c.id = id;
    c.name = id;
    c.layout = ContentLayout::VerticalStack;
    c.dockMode = DockMode::OverlayDocked;
    c.geometry = QRect(10, 10, 200, 160);
    c.contents = contents;
    return c;
}

}  // namespace

class TstContainerItemTxState : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("tx-state-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    // Toggling the manager reaches all three item types in a workspace
    // container, and the fade rules follow it.
    void setTransmittingReachesEveryItemType()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        ContainerContentRegistry registry;
        QWidget dock;
        QSplitter splitter;
        ContainerManager manager(&dock, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        WorkspaceDocument d;
        d.mainContainerId = "main";
        d.containers = {containerWith("main", {registry.makeEntry("FADECOVER"), registry.makeEntry("FADECOVER"),
                                               registry.makeEntry("VFO"), registry.makeEntry("OTHERBTNS")})};
        QCOMPARE(manager.commitWorkspace(d, 0).status, CommitStatus::Saved);

        const Items items = itemsIn(manager.contentHost("main"));
        QCOMPARE(items.covers.size(), 2);
        QCOMPARE(items.vfos.size(), 1);
        QCOMPARE(items.buttons.size(), 1);
        FadeCoverItem* onRx = items.covers[0];
        FadeCoverItem* onTx = items.covers[1];
        onRx->setRect(0, 0, 1, 1);
        onTx->setRect(0, 0, 1, 1);
        // Turning the flags on takes effect at once, in the current state.
        onRx->setFadeOnRx(true);
        onTx->setFadeOnTx(true);
        VfoDisplayItem* vfo = items.vfos.first();
        ButtonBoxItem* grid = items.buttons.first();
        grid->setRect(0, 0, 1, 1);

        // Receiving.
        QVERIFY(coverPaints(*onRx));
        QVERIFY(!coverPaints(*onTx));
        QVERIFY(!vfo->isTransmitting());
        const QImage rxVfo = renderVfo(*vfo);
        grid->setFadeOnTx(true);
        const std::optional<QPointF> hit = buttonPoint(*grid);
        QVERIFY(hit.has_value());

        manager.setTransmitting(true);
        QVERIFY(!coverPaints(*onRx));
        QVERIFY(coverPaints(*onTx));
        QVERIFY(vfo->isTransmitting());
        QVERIFY(renderVfo(*vfo) != rxVfo);
        QVERIFY(!pressAt(*grid, *hit));

        manager.setTransmitting(false);
        QVERIFY(coverPaints(*onRx));
        QVERIFY(!coverPaints(*onTx));
        QVERIFY(!vfo->isTransmitting());
        QCOMPARE(renderVfo(*vfo), rxVfo);
        QVERIFY(pressAt(*grid, *hit));
    }

    // A container loaded while transmitting, and an item added to a live
    // meter while transmitting, start in the transmitting state.
    void itemsAddedWhileTransmittingStartTransmitting()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        ContainerContentRegistry registry;
        QWidget dock;
        QSplitter splitter;
        ContainerManager manager(&dock, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        WorkspaceDocument d;
        d.mainContainerId = "main";
        d.containers = {containerWith("main", {registry.makeEntry("VFO")})};
        QCOMPARE(manager.commitWorkspace(d, 0).status, CommitStatus::Saved);

        manager.setTransmitting(true);
        d = store.snapshot();
        d.containers.append(containerWith("later", {registry.makeEntry("FADECOVER"), registry.makeEntry("VFO"),
                                                    registry.makeEntry("OTHERBTNS")}));
        QCOMPARE(manager.commitWorkspace(d, d.revision).status, CommitStatus::Saved);
        const Items later = itemsIn(manager.contentHost("later"));
        QCOMPARE(later.covers.size(), 1);
        QCOMPARE(later.vfos.size(), 1);
        QCOMPARE(later.buttons.size(), 1);
        QVERIFY(later.covers.first()->txState());
        QVERIFY(later.vfos.first()->isTransmitting());
        // A cover with Fade on TX paints at once.
        later.covers.first()->setRect(0, 0, 1, 1);
        later.covers.first()->setFadeOnTx(true);
        QVERIFY(coverPaints(*later.covers.first()));
        // A grid with Fade on TX ignores clicks at once.
        ButtonBoxItem* grid = later.buttons.first();
        grid->setRect(0, 0, 1, 1);
        QVERIFY(buttonPoint(*grid).has_value());
        grid->setFadeOnTx(true);
        QVERIFY(!buttonPoint(*grid).has_value());

        // An item added to a meter that already exists.
        const QVector<MeterWidget*> surfaces = manager.contentHost("main")->meterSurfaces();
        QVERIFY(!surfaces.isEmpty());
        auto* added = new FadeCoverItem();
        surfaces.first()->addItem(added);
        QVERIFY(added->txState());
        QVERIFY(surfaces.first()->itemTxState());
    }

    // A container's meter given as content (the restore and rebuild path)
    // while transmitting takes the state when it is announced.
    void meterSetAsContentWhileTransmitting()
    {
        QWidget dock;
        QSplitter splitter;
        ContainerManager manager(&dock, &splitter);
        manager.setTransmitting(true);
        ContainerWidget* container = manager.createContainer(0, DockMode::OverlayDocked);
        QVERIFY(container);
        auto* meter = new MeterWidget();
        auto* cover = new FadeCoverItem();
        auto* vfo = new VfoDisplayItem();
        auto* grid = new OtherButtonItem();
        meter->addItem(cover);
        meter->addItem(vfo);
        meter->addItem(grid);
        QVERIFY(!cover->txState());
        container->setContent(meter);
        QVERIFY(cover->txState());
        QVERIFY(vfo->isTransmitting());
        QVERIFY(meter->itemTxState());

        manager.setTransmitting(false);
        QVERIFY(!cover->txState());
        QVERIFY(!vfo->isTransmitting());
    }

    // Loading the Fade on RX flag from a saved record makes the cover
    // paint while receiving without waiting for a transmit change.
    void loadedFlagsTakeEffect()
    {
        FadeCoverItem cover;
        cover.setTxState(false);
        QVERIFY(cover.deserialize(QStringLiteral("FADECOVER|0|0|1|1|-1|0|#ff202020|#ff202020|0.75|2")));
        QVERIFY(cover.fadeOnRx());
        QVERIFY(coverPaints(cover));
        cover.setTxState(true);
        QVERIFY(!coverPaints(cover));
    }
};

QTEST_MAIN(TstContainerItemTxState)
#include "tst_container_item_tx_state.moc"
