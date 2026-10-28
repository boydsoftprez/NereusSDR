// =================================================================
// tests/tst_meter_item_fade.cpp  (NereusSDR)
// =================================================================
//
// Fade on RX / Fade on TX for every container item: the port of Thetis
// fade() (ContentFade::nextValue), the item ramp on a fake clock, the veil,
// persistence, the legacy composite fade, the fade cover ramp and
// background option, the widget overlay, the preview's transmit look, the
// repaint a transmit change schedules and the history cleared on a
// transmit or band change.
//
// Modification history (NereusSDR):
//   2026-10-26 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest>
#include <QCheckBox>
#include <QImage>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContentFade.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/meters/ButtonBoxItem.h"
#include "gui/meters/FadeCoverItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/presets/CompositePresetItem.h"

using namespace NereusSDR;

namespace {

constexpr int kW = 200;
constexpr int kH = 100;

// A plain item for the base fade.
class PlainItem : public MeterItem {
public:
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override
    {
        p.fillRect(pixelRect(widgetW, widgetH), Qt::white);
    }
};

QImage veilImage(MeterWidget& meter)
{
    QImage img(meter.width(), meter.height(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    meter.drawFadeVeilsForTest(p);
    p.end();
    return img;
}

QImage paintComposite(MeterItem& item)
{
    QImage img(300, 160, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    item.paint(p, 300, 160);
    p.end();
    return img;
}

class UpdateRequestCounter : public QObject {
public:
    int requests{0};

protected:
    bool eventFilter(QObject*, QEvent* ev) override
    {
        if (ev->type() == QEvent::UpdateRequest || ev->type() == QEvent::Paint) {
            ++requests;
        }
        return false;
    }
};

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

// Channels within 1 (alpha blending rounds).
bool nearColour(const QColor& a, const QColor& b)
{
    return qAbs(a.red() - b.red()) <= 1 && qAbs(a.green() - b.green()) <= 1 && qAbs(a.blue() - b.blue()) <= 1;
}

QString compact(const QJsonObject& o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace

class TstMeterItemFade : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("meter-fade-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    // --- Old records first ---------------------------------------------

    // A layout saved before the fade keys loads with fade off, draws no
    // veil and re-saves without the keys, byte for byte.
    void oldRecordLoadsOffAndResavesIdentically()
    {
        ContainerContentRegistry registry;
        for (const QString& type : {QStringLiteral("CLOCK"), QStringLiteral("VFO"), QStringLiteral("meter.powerSwr"),
                                    QStringLiteral("FADECOVER"), QStringLiteral("OTHERBTNS")}) {
            const ContentEntry saved = registry.makeEntry(type);
            QVERIFY2(!saved.context.contains(ContentFade::kFadeOnRxKey), qPrintable(type));
            std::unique_ptr<MeterItem> item(registry.createMeterItem(saved, nullptr));
            QVERIFY2(item, qPrintable(type));
            if (qobject_cast<FadeCoverItem*>(item.get()) == nullptr) {
                QVERIFY(!item->fadeOnRx());
                QVERIFY(!item->fadeOnTx());
            }
            QVERIFY(!item->hasExplicitFade());
            QCOMPARE(item->fadeValue(), 255);
            const ContentEntry first = registry.captureMeterItem(*item, saved);
            QVERIFY(!first.context.contains(ContentFade::kFadeOnRxKey));
            QVERIFY(!first.context.contains(ContentFade::kFadeOnTxKey));
            std::unique_ptr<MeterItem> again(registry.createMeterItem(first, nullptr));
            QVERIFY(again);
            const ContentEntry second = registry.captureMeterItem(*again, first);
            QCOMPARE(compact(second.context), compact(first.context));
            QCOMPARE(compact(second.config), compact(first.config));
        }

        // Painted pixel-identical: no veil while the fade is off, on a
        // transmit change too.
        MeterWidget meter;
        meter.resize(kW, kH);
        auto* item = new PlainItem();
        item->setRect(0.1f, 0.1f, 0.5f, 0.5f);
        meter.addItem(item);
        QImage blank(kW, kH, QImage::Format_ARGB32);
        blank.fill(Qt::transparent);
        item->advanceFade(0);
        meter.setItemTxState(true);
        item->advanceFade(1000);
        QCOMPARE(veilImage(meter), blank);
        meter.setItemTxState(false);
        item->advanceFade(2000);
        QCOMPARE(veilImage(meter), blank);
    }

    // A fade cover record without the new field paints its gradient as
    // before, once the ramp has run.
    void oldFadeCoverRecordPaintsItsGradient()
    {
        FadeCoverItem cover;
        QVERIFY(cover.deserialize(QStringLiteral("FADECOVER|0.25|0.25|0.5|0.5|-1|0|#ff202020|#ff202020|0.75|2")));
        QVERIFY(cover.fadeOnRx());
        QVERIFY(!cover.useContainerBackground());
        QVERIFY(!cover.hasExplicitFade());
        cover.advanceFade(0);
        cover.advanceFade(500);
        QCOMPARE(cover.fadeValue(), ContentFade::kDimmedLevel);
        QImage img(kW, kH, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        cover.paint(p, kW, kH);
        p.end();
        const QColor centre = img.pixelColor(kW / 2, kH / 2);
        QCOMPARE(centre.alpha(), static_cast<int>(0.75f * 255.0f));
        QCOMPARE(centre.red(), 0x20);
        // The serialized record gains only the new field, off.
        QCOMPARE(cover.serialize(), QStringLiteral("FADECOVER|0.25|0.25|0.5|0.5|-1|0|#ff202020|#ff202020|0.75|2|0"));
    }

    // --- ContentFade::nextValue ------------------------------------------

    void nextValueStepsAsThetis()
    {
        // 100 ms: ceil(207 / 5) = 42.
        int v = 255;
        const QVector<int> down{213, 171, 129, 87, 48};
        for (int expected : down) {
            v = ContentFade::nextValue(v, false, true, true, 100);
            QCOMPARE(v, expected);
        }
        const QVector<int> up{90, 132, 174, 216, 255};
        for (int expected : up) {
            v = ContentFade::nextValue(v, false, true, false, 100);
            QCOMPARE(v, expected);
        }
        // 50 ms: ceil(207 / 10) = 21; ten calls reach 48.
        v = 255;
        for (int i = 0; i < 9; ++i) {
            v = ContentFade::nextValue(v, true, false, false, 50);
            QVERIFY(v > 48);
        }
        QCOMPARE(v, 255 - 9 * 21);
        v = ContentFade::nextValue(v, true, false, false, 50);
        QCOMPARE(v, 48);
        // Early returns keep 255; a long interval is capped at 500 ms
        // (one step of 207).
        QCOMPARE(ContentFade::nextValue(255, false, false, true, 100), 255);
        QCOMPARE(ContentFade::nextValue(255, true, false, true, 100), 255);
        QCOMPARE(ContentFade::nextValue(255, false, true, true, 5000), 48);
        // Stage-disabled (both flags) dims in either state.
        QCOMPARE(ContentFade::nextValue(255, true, true, false, 100), 213);
        QCOMPARE(ContentFade::nextValue(255, true, true, true, 100), 213);
    }

    void coverRectInflates()
    {
        QCOMPARE(ContentFade::coverRect(QRectF(10, 10, 100, 50)), QRectF(8, 8, 104, 54));
        QCOMPARE(ContentFade::coverRect(QRectF(0, 0, 1000, 400)), QRectF(-10, -4, 1020, 408));
    }

    // --- Item ramp ------------------------------------------------------

    void fadeOnTxRampsOnTheFakeClock()
    {
        PlainItem item;
        item.setFadeOnTx(true);
        QVERIFY(item.hasExplicitFade());
        QCOMPARE(item.fadeIntervalMs(), kDefaultUpdateIntervalMs);
        item.advanceFade(0);
        QCOMPARE(item.fadeValue(), 255);
        item.setTransmitting(true);
        QVERIFY(item.advanceFade(250));
        QCOMPARE(item.fadeValue(), 171);
        item.advanceFade(500);
        QCOMPARE(item.fadeValue(), 48);
        item.setTransmitting(false);
        item.advanceFade(750);
        QVERIFY(item.fadeValue() > 48);
        QVERIFY(item.fadeValue() < 255);
        item.advanceFade(1000);
        QCOMPARE(item.fadeValue(), 255);
        QVERIFY(!item.advanceFade(2000));
    }

    void fadeOnRxIsTheMirrorImage()
    {
        PlainItem item;
        item.setFadeOnRx(true);
        item.advanceFade(0);
        item.advanceFade(250);
        QCOMPARE(item.fadeValue(), 171);
        item.advanceFade(500);
        QCOMPARE(item.fadeValue(), 48);
        item.setTransmitting(true);
        item.advanceFade(750);
        QVERIFY(item.fadeValue() > 48 && item.fadeValue() < 255);
        item.advanceFade(1000);
        QCOMPARE(item.fadeValue(), 255);
    }

    void itemIntervalSetsTheStep()
    {
        PlainItem item;
        item.setUpdateIntervalMs(50);
        item.setFadeOnTx(true);
        item.setTransmitting(true);
        item.advanceFade(0);
        item.advanceFade(450);
        QCOMPARE(item.fadeValue(), 255 - 9 * 21);
        item.advanceFade(500);
        QCOMPARE(item.fadeValue(), 48);
        item.setUpdateIntervalMs(4000);
        QCOMPARE(item.fadeIntervalMs(), 500);
    }

    void foreignSessionStaysUndimmed()
    {
        PlainItem item;
        item.setFadeOnTx(true);
        item.setForeignSession(true);
        item.setTransmitting(true);
        item.advanceFade(0);
        item.advanceFade(1000);
        QCOMPARE(item.fadeValue(), 255);
        item.setForeignSession(false);
        item.advanceFade(1500);
        QCOMPARE(item.fadeValue(), 48);
    }

    // --- The veil -------------------------------------------------------

    void veilCoversTheItemInTheCoverColour()
    {
        MeterWidget meter;
        meter.resize(kW, kH);
        QPalette pal = meter.palette();
        pal.setColor(QPalette::Window, QColor(0x12, 0x34, 0x56));
        meter.setPalette(pal);
        auto* item = new PlainItem();
        item->setRect(0.25f, 0.25f, 0.5f, 0.5f);
        meter.addItem(item);
        item->setFadeOnTx(true);
        item->advanceFade(0);
        meter.setItemTxState(true);
        QVERIFY(item->transmitting());
        meter.advanceMeters(250);
        QCOMPARE(item->fadeValue(), 171);

        const QImage img = veilImage(meter);
        const QColor centre = img.pixelColor(kW / 2, kH / 2);
        QVERIFY2(nearColour(centre, QColor(0x12, 0x34, 0x56)), qPrintable(centre.name()));
        QCOMPARE(centre.alpha(), 255 - 171);
        // Inflated by max(2 px, 1%): a pixel just outside the item rect is
        // covered, one well outside is not.
        const QRectF expected = ContentFade::coverRect(QRectF(50, 25, 100, 50));
        QVERIFY(img.pixelColor(static_cast<int>(expected.left()), kH / 2).alpha() > 0);
        QCOMPARE(img.pixelColor(static_cast<int>(expected.left()) - 2, kH / 2).alpha(), 0);
        QCOMPARE(img.pixelColor(kW / 2, 10).alpha(), 0);

        // A fade cover draws its own fade and gets no veil.
        auto* cover = new FadeCoverItem();
        cover->setRect(0, 0, 0.1f, 0.1f);
        cover->setFadeOnTx(true);
        meter.addItem(cover);
        meter.advanceMeters(500);
        meter.advanceMeters(1000);
        QCOMPARE(cover->fadeValue(), 48);
        QCOMPARE(veilImage(meter).pixelColor(5, 5).alpha(), 0);
    }

#ifdef NEREUS_GPU_SPECTRUM
    // The GPU path draws the veil on the dynamic overlay: a moving fade
    // marks it dirty, a fade at rest does not.
    void movingFadeDirtiesTheGpuOverlay()
    {
        MeterWidget meter;
        meter.resize(kW, kH);
        auto* item = new PlainItem();
        meter.addItem(item);
        item->setFadeOnTx(true);
        meter.advanceMeters(0);
        meter.setItemTxState(true);
        meter.clearReadingLayerDirtyFlagsForTest();
        meter.advanceMeters(100);
        QVERIFY(meter.overlayDynamicDirtyForTest());
        meter.advanceMeters(500);
        meter.clearReadingLayerDirtyFlagsForTest();
        meter.advanceMeters(600);
        QVERIFY(!meter.overlayDynamicDirtyForTest());
    }
#endif

    // --- Input ----------------------------------------------------------

    // The veil takes no input: a faded VFO display still gets its click.
    // A faded button grid with Fade on TX still ignores clicks while
    // transmitting (its own guard).
    void clicksReachFadedItems()
    {
        MeterWidget meter;
        meter.resize(kW, kH);
        auto* vfo = new VfoDisplayItem();
        vfo->setRect(0, 0, 1, 1);
        meter.addItem(vfo);
        vfo->setFadeOnTx(true);
        meter.advanceMeters(0);
        meter.setItemTxState(true);
        meter.advanceMeters(500);
        QCOMPARE(vfo->fadeValue(), 48);
        QSignalSpy band(vfo, &VfoDisplayItem::bandStackRequested);
        meter.show();
        QTest::mouseClick(&meter, Qt::RightButton, Qt::NoModifier, QPoint(kW - 20, kH - 10));
        QCOMPARE(band.count(), 1);

        OtherButtonItem grid;
        grid.setRect(0, 0, 1, 1);
        grid.setFadeOnTx(true);
        QVERIFY(!grid.hasExplicitFade());
        grid.setTransmitting(true);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), QPointF(5, 5), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QVERIFY(!grid.handleMousePress(&press, kW, kH));
        grid.setTransmitting(false);
        bool anyHit = false;
        for (int y = 1; y < kH && !anyHit; y += 3) {
            for (int x = 1; x < kW && !anyHit; x += 3) {
                QMouseEvent at(QEvent::MouseButtonPress, QPointF(x, y), QPointF(x, y), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                anyHit = grid.handleMousePress(&at, kW, kH);
            }
        }
        QVERIFY(anyHit);
    }

    // --- Persistence ----------------------------------------------------

    void bothKeysRoundTrip()
    {
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("CLOCK"));
        entry.context.insert(ContentFade::kFadeOnRxKey, true);
        entry.context.insert(ContentFade::kFadeOnTxKey, false);
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        QVERIFY(item);
        QVERIFY(item->fadeOnRx());
        QVERIFY(!item->fadeOnTx());
        QVERIFY(item->hasExplicitFade());
        const ContentEntry captured = registry.captureMeterItem(*item, entry);
        QCOMPARE(captured.context.value(ContentFade::kFadeOnRxKey), QJsonValue(true));
        QCOMPARE(captured.context.value(ContentFade::kFadeOnTxKey), QJsonValue(false));
        std::unique_ptr<MeterItem> again(registry.createMeterItem(captured, nullptr));
        QVERIFY(again);
        const ContentEntry second = registry.captureMeterItem(*again, captured);
        QCOMPARE(compact(second.context), compact(captured.context));
        QCOMPARE(compact(second.config), compact(captured.config));
    }

    // Fade on RX with Only when TX: hidden while receiving; the two save
    // independently.
    void fadeOnRxWithOnlyWhenTx()
    {
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("CLOCK"));
        entry.context.insert(QStringLiteral("onlyWhenTx"), true);
        entry.context.insert(ContentFade::kFadeOnRxKey, true);
        auto* item = registry.createMeterItem(entry, nullptr);
        QVERIFY(item);
        MeterWidget meter;
        meter.addItem(item);
        QVERIFY(!meter.shouldRender(item));
        meter.setMox(true);
        QVERIFY(meter.shouldRender(item));
        const ContentEntry captured = registry.captureMeterItem(*item, entry);
        QCOMPARE(captured.context.value(QStringLiteral("onlyWhenTx")), QJsonValue(true));
        QCOMPARE(captured.context.value(QStringLiteral("onlyWhenRx")), QJsonValue(false));
        QCOMPARE(captured.context.value(ContentFade::kFadeOnRxKey), QJsonValue(true));
        QCOMPARE(captured.context.value(ContentFade::kFadeOnTxKey), QJsonValue(false));
    }

    // --- Legacy composite fade -----------------------------------------

    void legacyCompositeKeepsItsInstantFade()
    {
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("meter.powerSwr"));
        std::unique_ptr<MeterItem> plain(registry.createMeterItem(entry, nullptr));
        QVERIFY(plain);
        QJsonObject props = entry.config.value(QStringLiteral("properties")).toObject();
        props.insert(QStringLiteral("fadeTx"), true);
        entry.config.insert(QStringLiteral("properties"), props);
        std::unique_ptr<MeterItem> legacy(registry.createMeterItem(entry, nullptr));
        QVERIFY(legacy);
        QVERIFY(!legacy->hasExplicitFade());
        for (MeterItem* item : {plain.get(), legacy.get()}) {
            item->setRect(0, 0, 1, 1);
            item->resetForTxTransition(true);
        }
        const QImage plainTx = paintComposite(*plain);
        // 25% opacity while transmitting.
        QVERIFY(paintComposite(*legacy) != plainTx);

        // The editor's Fade on TX row writes the context keys and turns
        // the configuration's fadeTx off.
        ContentPropertyEditor editor(registry);
        QSignalSpy edited(&editor, &ContentPropertyEditor::entryEdited);
        editor.setEntry(entry);
        auto* fadeTx = editor.findChild<QCheckBox*>(QStringLiteral("fadeOnTx"));
        auto* fadeRx = editor.findChild<QCheckBox*>(QStringLiteral("fadeOnRx"));
        QVERIFY(fadeTx);
        QVERIFY(fadeRx);
        QVERIFY(fadeTx->isChecked());
        QVERIFY(!fadeRx->isChecked());
        fadeTx->setChecked(false);
        fadeTx->setChecked(true);
        QVERIFY(edited.count() >= 1);
        const ContentEntry result = edited.last().first().value<ContentEntry>();
        QCOMPARE(result.context.value(ContentFade::kFadeOnTxKey), QJsonValue(true));
        QCOMPARE(result.context.value(ContentFade::kFadeOnRxKey), QJsonValue(false));
        QCOMPARE(result.config.value(QStringLiteral("properties")).toObject().value(QStringLiteral("fadeTx")), QJsonValue(false));

        // From then on the composite paints without the 25% rule (the
        // animated veil dims it instead).
        std::unique_ptr<MeterItem> edited2(registry.createMeterItem(result, nullptr));
        QVERIFY(edited2);
        QVERIFY(edited2->hasExplicitFade());
        QVERIFY(edited2->fadeOnTx());
        edited2->setRect(0, 0, 1, 1);
        edited2->resetForTxTransition(true);
        QCOMPARE(paintComposite(*edited2), plainTx);
    }

    // --- Fade cover background option ----------------------------------

    void fadeCoverUsesTheContainerBackground()
    {
        MeterWidget meter;
        meter.resize(kW, kH);
        QPalette pal = meter.palette();
        pal.setColor(QPalette::Window, QColor(0x12, 0x34, 0x56));
        meter.setPalette(pal);
        auto* cover = new FadeCoverItem();
        QVERIFY(cover->deserialize(QStringLiteral("FADECOVER|0.25|0.25|0.5|0.5|-1|0|#ff202020|#ffa0a0a0|0.75|1|1")));
        QVERIFY(cover->useContainerBackground());
        meter.addItem(cover);
        meter.advanceMeters(0);
        meter.setItemTxState(true);
        meter.advanceMeters(500);
        QCOMPARE(cover->fadeValue(), 48);
        QImage img(kW, kH, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        cover->paint(p, kW, kH);
        p.end();
        const QColor centre = img.pixelColor(kW / 2, kH / 2);
        QVERIFY2(nearColour(centre, QColor(0x12, 0x34, 0x56)), qPrintable(centre.name()));
        QCOMPARE(centre.alpha(), 255 - 48);
        QVERIFY(cover->serialize().endsWith(QStringLiteral("|1|1")));
    }

    // --- ContentFade overlay -------------------------------------------

    void overlayAttachesOnlyWhenLiveAndOn()
    {
        QWidget off;
        ContentFade::attach(&off, QJsonObject{}, ContentRenderMode::Live);
        QVERIFY(!ContentFade::overlayFor(&off));

        QWidget preview;
        ContentFade::attach(&preview, QJsonObject{{ContentFade::kFadeOnRxKey, true}}, ContentRenderMode::Preview);
        QVERIFY(!ContentFade::overlayFor(&preview));

        QWidget content;
        content.resize(120, 60);
        content.show();
        ContentFade::attach(&content, QJsonObject{{ContentFade::kFadeOnRxKey, true}}, ContentRenderMode::Live);
        ContentFadeOverlay* overlay = ContentFade::overlayFor(&content);
        QVERIFY(overlay);
        QVERIFY(overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
        QCOMPARE(overlay->geometry(), content.rect());
        QCOMPARE(content.childAt(60, 30), nullptr);
        QCOMPARE(overlay->value(), 255);
        for (int i = 0; i < 5; ++i) { overlay->advance(); }
        QCOMPARE(overlay->value(), 48);
        ContentFade::setTransmitting(&content, true);
        QVERIFY(overlay->transmitting());
        for (int i = 0; i < 5; ++i) { overlay->advance(); }
        QCOMPARE(overlay->value(), 255);
        content.resize(200, 90);
        QCOMPARE(overlay->geometry(), content.rect());

        const QVector<ContentProperty> props = ContentFade::properties();
        QCOMPARE(props.size(), 2);
        QCOMPARE(props[0].key, QString::fromLatin1(ContentFade::kFadeOnRxKey));
        QCOMPARE(props[1].key, QString::fromLatin1(ContentFade::kFadeOnTxKey));
        QCOMPARE(props[0].label, QStringLiteral("Fade on RX"));
        QCOMPARE(props[1].label, QStringLiteral("Fade on TX"));
        QCOMPARE(props[0].kind, ContentProperty::Kind::Toggle);
        QCOMPARE(props[0].defaultValue, QJsonValue(false));
    }

    // --- Preview (PR 664 review) ----------------------------------------

    // The container preview shows the transmit look while the radio
    // transmits, read only.
    void previewTakesTheTransmitState()
    {
        ContainerContentRegistry registry;
        MeterPoller poller;
        ContentEntry vfoEntry = registry.makeEntry(QStringLiteral("VFO"));
        ContentEntry fadeEntry = registry.makeEntry(QStringLiteral("CLOCK"));
        fadeEntry.context.insert(ContentFade::kFadeOnTxKey, true);
        ContainerDocument d = containerWith(QStringLiteral("A"), {vfoEntry, fadeEntry});
        poller.setInTx(true);
        ContainerPreviewWidget preview(registry, poller);
        preview.setDocument(d);
        VfoDisplayItem* vfo = nullptr;
        MeterItem* faded = nullptr;
        for (MeterWidget* meter : preview.findChildren<MeterWidget*>()) {
            for (MeterItem* item : meter->items()) {
                if (auto* v = qobject_cast<VfoDisplayItem*>(item)) { vfo = v; }
                if (item->hasExplicitFade()) { faded = item; }
            }
        }
        QVERIFY(vfo);
        QVERIFY(faded);
        QVERIFY(vfo->isTransmitting());
        QVERIFY(faded->transmitting());
        emit poller.frameAdvanced(1000);
        emit poller.frameAdvanced(1500);
        QCOMPARE(faded->fadeValue(), 48);

        poller.setInTx(false);
        emit poller.frameAdvanced(2000);
        QVERIFY(!vfo->isTransmitting());
        QVERIFY(!faded->transmitting());
        emit poller.frameAdvanced(2500);
        QCOMPARE(faded->fadeValue(), 255);
    }

    // --- Repaint (PR 664 review) ----------------------------------------

    // A transmit change schedules a repaint of the meter, not only a flag.
    void transmitChangeSchedulesARepaint()
    {
        QWidget window;
        window.resize(kW, kH);
        auto* meter = new MeterWidget(&window);
        meter->setGeometry(0, 0, kW, kH);
        auto* vfo = new VfoDisplayItem();
        meter->addItem(vfo);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);
        UpdateRequestCounter counter;
        window.installEventFilter(&counter);
        meter->installEventFilter(&counter);
#ifdef NEREUS_GPU_SPECTRUM
        meter->clearReadingLayerDirtyFlagsForTest();
#endif
        meter->setItemTxState(true);
#ifdef NEREUS_GPU_SPECTRUM
        QVERIFY(meter->overlayDynamicDirtyForTest());
#endif
        QTRY_VERIFY(counter.requests > 0);

        // Through the manager, as the radio's transmittingChanged arrives.
        QWidget dock;
        QSplitter splitter;
        ContainerManager manager(&dock, &splitter);
        ContainerWidget* container = manager.createContainer(0, DockMode::OverlayDocked);
        QVERIFY(container);
        auto* hosted = new MeterWidget();
        hosted->addItem(new VfoDisplayItem());
        container->setContent(hosted);
#ifdef NEREUS_GPU_SPECTRUM
        hosted->clearReadingLayerDirtyFlagsForTest();
        manager.setTransmitting(true);
        QVERIFY(hosted->overlayDynamicDirtyForTest());
        QVERIFY(hosted->backgroundDirtyForTest());
#else
        manager.setTransmitting(true);
#endif
        QVERIFY(hosted->itemTxState());
    }

    // --- History (Tasks 12 and 18) --------------------------------------

    // Thetis clears every item's history on a MOX change (OnMox -> ZeroOut)
    // and on a band change (bandChange -> ZeroOut).
    void historyClearsOnTransmitAndBandChange()
    {
        MeterWidget meter;
        auto* bar = new BarItem();
        auto* needle = new NeedleItem();
        bar->setIgnoreHistoryMs(1000);
        needle->setIgnoreHistoryMs(1000);
        meter.addItem(bar);
        meter.addItem(needle);
        QCOMPARE(bar->ignoringReadings(), 0);
        QCOMPARE(needle->ignoringReadings(), 0);
        // No change of state: nothing cleared.
        meter.setItemTxState(false);
        QCOMPARE(bar->ignoringReadings(), 0);
        meter.setItemTxState(true);
        QVERIFY(bar->ignoringReadings() > 0);
        QVERIFY(needle->ignoringReadings() > 0);

        // Band change, through the manager.
        QWidget dock;
        QSplitter splitter;
        ContainerManager manager(&dock, &splitter);
        ContainerWidget* container = manager.createContainer(0, DockMode::OverlayDocked);
        QVERIFY(container);
        auto* hosted = new MeterWidget();
        auto* hostedBar = new BarItem();
        hostedBar->setIgnoreHistoryMs(1000);
        hosted->addItem(hostedBar);
        container->setContent(hosted);
        QCOMPARE(hostedBar->ignoringReadings(), 0);
        manager.clearItemHistory();
        QVERIFY(hostedBar->ignoringReadings() > 0);
    }
};

QTEST_MAIN(TstMeterItemFade)
#include "tst_meter_item_fade.moc"
