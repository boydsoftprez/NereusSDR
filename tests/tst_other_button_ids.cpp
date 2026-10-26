// =================================================================
// tests/tst_other_button_ids.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test of the other-buttons grid's Thetis
// button numbers; it checks the ported table and the saved record, and
// ports no upstream logic itself.
//
// Container item parity, Task 58: other buttons use Thetis button numbers
// (bitfield group * 32 + bit) and every button's visibility is saved.
//   - A record from before (no field 15) loads through the old numbering
//     with the same buttons in the same order; Power is still dropped; the
//     old numbers 32 and 33 count as visible.
//   - An old single control loads as that single control.
//   - A new grid and each control.* shortcut show what they showed before.
//   - Every group and a non-identity button order survive a save, directly
//     and through the registry and the workspace document.
//   - Bit 31 is never a button; field 8 keeps the old mask.
//   - An id with no NereusSDR action is never drawn and keeps its saved
//     visibility; a gated id keeps its gate.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25  J.J. Boyd / KG4VCF  Container item parity Task 58.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>

#include "core/UnbuiltFeatureList.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerControlCatalog.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/meters/OtherButtonCatalog.h"
#include "gui/meters/OtherButtonItem.h"

#include <memory>

using namespace NereusSDR;
using Id = OtherButtonItem::ButtonId;

namespace {

// The ids drawn, in grid order.
QList<int> shownIds(const OtherButtonItem& item)
{
    QList<int> ids;
    const QVector<short> map = item.buttonMap();
    for (int slot = 0; slot < item.buttonCount(); ++slot) {
        if (item.ButtonBoxItem::isButtonShown(slot)) { ids.append(map[slot]); }
    }
    return ids;
}

QList<int> ints(std::initializer_list<Id> ids)
{
    QList<int> out;
    for (Id id : ids) { out.append(int(id)); }
    return out;
}

QStringList fieldsOf(const OtherButtonItem& item)
{
    return item.serialize().split(QLatin1Char('|'));
}

// Builds an item the way a saved layout does.
std::unique_ptr<OtherButtonItem> fromRegistry(ContainerContentRegistry& registry,
                                              const ContentEntry& entry)
{
    std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
    auto* other = qobject_cast<OtherButtonItem*>(item.get());
    if (!other) { return {}; }
    item.release();
    return std::unique_ptr<OtherButtonItem>(other);
}

// True when the pixel is painted (anything but the transparent background).
bool painted(const QImage& image, int x, int y)
{
    return qAlpha(image.pixel(x, y)) != 0;
}

} // namespace

class TstOtherButtonIds : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { UnbuiltFeatures::resetForTest(); }

    // ── The ported table ────────────────────────────────────────────────

    void theCatalogHoldsTheThetisTable()
    {
        const OtherButtonInfo anf = OtherButtonCatalog::info(Id::Anf);
        QCOMPARE(anf.group, 1);
        QCOMPARE(anf.bit, 5);
        QCOMPARE(anf.caption, QStringLiteral("ANF"));
        QCOMPARE(anf.tooltip, QStringLiteral("Auto notch filter"));
        QCOMPARE(anf.section, QStringLiteral("Noise / ATT"));

        const OtherButtonInfo play = OtherButtonCatalog::info(Id::Play);
        QCOMPARE(play.caption, QStringLiteral("Play"));
        QCOMPARE(play.section, QStringLiteral("General"));
        QCOMPARE(OtherButtonCatalog::info(Id::Vac1).caption, QStringLiteral("Vac1"));
        QCOMPARE(OtherButtonCatalog::info(Id::Vac1).section, QStringLiteral("Audio / DSP"));
        QCOMPARE(OtherButtonCatalog::info(Id::DisplayOff).caption, QStringLiteral("Off"));
        QCOMPARE(OtherButtonCatalog::info(Id::Macro0).caption, QStringLiteral("M1"));
        QCOMPARE(OtherButtonCatalog::info(Id::Macro0).section, QStringLiteral("Macros"));
        QCOMPARE(OtherButtonCatalog::info(static_cast<Id>(382)).caption, QStringLiteral("M31"));
        // A gap in the table (group 5 bit 2) and bit 31 are not buttons.
        QVERIFY(!OtherButtonCatalog::isDefined(static_cast<Id>(5 * 32 + 2)));
        QVERIFY(OtherButtonCatalog::info(static_cast<Id>(31)).caption.isEmpty());
        QVERIFY(OtherButtonCatalog::isDefined(static_cast<Id>(32)));  // NR
    }

    void theOldNumbersMapToThetisNumbers()
    {
        const QList<int> expected = {0, 1, 2, 3, 4, 5, 6, 7, 10, 9, 37, 41, 42, 96, 97, 98,
                                     128, 129, 130, 132, 133, 134, 8,
                                     99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109};
        for (int old = 0; old < expected.size(); ++old) {
            QCOMPARE(int(OtherButtonCatalog::legacyToButtonId(old)), expected[old]);
            QCOMPARE(OtherButtonCatalog::buttonIdToLegacy(static_cast<Id>(expected[old])), old);
        }
        for (int i = 0; i < 31; ++i) {
            QCOMPARE(int(OtherButtonCatalog::legacyToButtonId(64 + i)), 352 + i);
            QCOMPARE(OtherButtonCatalog::buttonIdToLegacy(static_cast<Id>(352 + i)), 64 + i);
        }
        QCOMPARE(int(OtherButtonCatalog::legacyToButtonId(34)), -1);
        QCOMPARE(int(OtherButtonCatalog::legacyToButtonId(95)), -1);
        QCOMPARE(OtherButtonCatalog::buttonIdToLegacy(static_cast<Id>(32)), -1);
        QCOMPARE(OtherButtonItem::kGroupCount, 12);
        QCOMPARE(OtherButtonItem::kSlotCount, 384);
    }

    // ── Records from before ─────────────────────────────────────────────

    void anOldRecordLoadsTheSameButtonsInTheSameOrder()
    {
        OtherButtonItem item;
        QVERIFY(item.deserialize(QStringLiteral("OTHERBTNS|0.1|0.2|0.3|0.4|5|6|3|328708")));
        QCOMPARE(shownIds(item), ints({Id::Mon, Id::Anf, Id::Vac1, Id::Mute}));
        QCOMPARE(item.visibleBits(), 328708u);
        QCOMPARE(item.columns(), 3);
        // The old numbers 32 and 33 were never hidden by the mask.
        QVERIFY(item.isButtonVisible(Id::Spectrascope));
        QVERIFY(item.isButtonVisible(Id::DisplayOff));
        QVERIFY(!item.isButtonShown(Id::Spectrascope));
        QVERIFY(!item.isButtonVisible(Id::Tun));
    }

    void anOldPowerBitIsStillDropped()
    {
        OtherButtonItem item;
        QVERIFY(item.deserialize(QStringLiteral("OTHERBTNS|0|0|1|1|0|0|6|%1").arg(1u | 4u)));
        QVERIFY(!item.isButtonVisible(Id::Power));
        QVERIFY(item.isButtonVisible(Id::Mon));
        QCOMPARE(item.visibleBits(), 4u);
        QCOMPARE(fieldsOf(item).at(8), QStringLiteral("4"));
        // A Thetis-numbered record holding Power loads without it too.
        OtherButtonItem thetis;
        QStringList fields = fieldsOf(thetis);
        fields[16] = QStringLiteral("5");  // Power and MON: bits 0 and 2
        QVERIFY(thetis.deserialize(fields.join(QLatin1Char('|'))));
        QVERIFY(!thetis.isButtonVisible(Id::Power));
        QVERIFY(thetis.isButtonVisible(Id::Mon));
    }

    void anOldSingleControlLoadsAsThatControl()
    {
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("OTHERBTNS"));
        entry.config.insert(QStringLiteral("legacyRecord"),
                            QStringLiteral("OTHERBTNS|0|0|1|1|0|0|1|%1").arg(1u << 4));
        auto item = fromRegistry(registry, entry);
        QVERIFY(item);
        QCOMPARE(shownIds(*item), ints({Id::Mox}));
        QVERIFY(isSingleContainerControl(item.get()));
        QVERIFY(item->property("containerSingleControl").toBool());
        QCOMPARE(item->visibleBits(), 1u << 4);

        // The single-control geometry fills the viewport's height: a tall
        // narrow viewport is painted near its foot, which a grid cell of
        // the legacy aspect ratio would not reach.
        QImage image(100, 300, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        {
            QPainter p(&image);
            item->paint(p, image.width(), image.height());
        }
        QVERIFY(painted(image, 50, 250));
        item->setProperty("containerSingleControl", false);
        image.fill(Qt::transparent);
        {
            QPainter p(&image);
            item->paint(p, image.width(), image.height());
        }
        QVERIFY(!painted(image, 50, 250));
    }

    // ── New items ───────────────────────────────────────────────────────

    void aNewGridShowsWhatItShowedBefore()
    {
        OtherButtonItem item;
        QCOMPARE(shownIds(item), ints({Id::Mon, Id::Tun, Id::Mox, Id::TwoTon, Id::Dup, Id::PsA,
                                       Id::Anf, Id::Snb, Id::Mnf, Id::PeakHold, Id::Ctun,
                                       Id::Vac1, Id::Vac2, Id::Mute, Id::Bin}));
        QCOMPARE(item.visibleBits(), 0xFFFFFFFEu);
        // Every earlier core button but Power is saved visible, the macro
        // buttons are not.
        for (int old = 1; old < 34; ++old) {
            QVERIFY(item.isButtonVisible(OtherButtonCatalog::legacyToButtonId(old)));
        }
        QVERIFY(!item.isButtonVisible(Id::Power));
        QVERIFY(!item.isButtonVisible(Id::Macro0));
        const QStringList fields = fieldsOf(item);
        QCOMPARE(fields.size(), 29);
        QCOMPARE(fields.mid(15).join(QLatin1Char('|')),
                 QStringLiteral("2|2046|1568|0|16383|119|0|0|0|0|0|0|0|"));
    }

    void eachControlShortcutShowsItsOneButton()
    {
        ContainerContentRegistry registry;
        for (const auto& control : supportedContainerControls()) {
            const ContentEntry entry = registry.makeEntry(control.creationId);
            auto item = fromRegistry(registry, entry);
            QVERIFY2(item, qPrintable(control.creationId));
            QCOMPARE(shownIds(*item), QList<int>{int(control.buttonId)});
            QVERIFY(isSingleContainerControl(item.get()));
            QCOMPARE(item->columns(), 1);
            const int old = OtherButtonCatalog::buttonIdToLegacy(control.buttonId);
            QCOMPARE(item->visibleBits(), 1u << old);
        }
    }

    // ── Saving ──────────────────────────────────────────────────────────

    void everyGroupAndTheOrderSurviveASave()
    {
        OtherButtonItem item;
        QVector<qint32> groups(OtherButtonItem::kGroupCount);
        for (int g = 0; g < groups.size(); ++g) { groups[g] = (qint32(1) << (g + 1)) | (qint32(1) << 30); }
        item.setVisibleGroups(groups);
        QVector<short> map(OtherButtonItem::kSlotCount);
        for (int i = 0; i < map.size(); ++i) { map[i] = static_cast<short>(map.size() - 1 - i); }
        item.setButtonMap(map);

        OtherButtonItem restored;
        QVERIFY(restored.deserialize(item.serialize()));
        QCOMPARE(restored.visibleGroups(), groups);
        QCOMPARE(restored.buttonMap(), map);
        QVERIFY(restored.isButtonVisible(static_cast<Id>(11 * 32 + 12)));
        QCOMPARE(restored.serialize(), item.serialize());

        // The path a saved layout takes: capture, document, reload.
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("OTHERBTNS"));
        auto live = fromRegistry(registry, entry);
        QVERIFY(live);
        live->setVisibleGroups(groups);
        live->setButtonMap(map);
        entry = registry.captureMeterItem(*live, entry);
        WorkspaceDocument doc;
        doc.mainContainerId = QStringLiteral("A");
        ContainerDocument container;
        container.id = doc.mainContainerId;
        container.contents.append(entry);
        doc.containers.append(container);
        const DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        auto reloaded = fromRegistry(registry, decoded.document.containers.first().contents.first());
        QVERIFY(reloaded);
        QCOMPARE(reloaded->visibleGroups(), groups);
        QCOMPARE(reloaded->buttonMap(), map);
    }

    void theOrderMovesButtonsAndClicks()
    {
        OtherButtonItem item;
        QVector<short> map = item.buttonMap();
        std::swap(map[int(Id::Mon)], map[int(Id::Mox)]);
        item.setButtonState(Id::Mox, true);
        item.setButtonMap(map);
        QCOMPARE(shownIds(item).mid(0, 3), ints({Id::Mox, Id::Tun, Id::Mon}));
        QCOMPARE(item.indexOf(Id::Mox), int(Id::Mon));
        QVERIFY(item.buttonState(Id::Mox));    // the lit state moved with it
        QVERIFY(!item.buttonState(Id::Mon));
        QCOMPARE(item.button(item.indexOf(Id::Mox)).text, QStringLiteral("MOX"));

        // The first cell now clicks MOX.
        QSignalSpy clicked(&item, &OtherButtonItem::otherButtonClicked);
        item.setRect(0, 0, 1, 1);
        const QPointF at(20, 20);
        QMouseEvent press(QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        item.handleMousePress(&press, 600, 600);
        item.handleMouseRelease(&release, 600, 600);
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(clicked.at(0).at(0).toInt(), int(Id::Mox));

        // A map that is not one of every button keeps the identity order.
        QVector<short> broken = map;
        broken[0] = broken[1];
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("button order")));
        item.setButtonMap(broken);
        QCOMPARE(item.indexOf(Id::Mox), int(Id::Mox));
        QVERIFY(fieldsOf(item).at(28).isEmpty());
    }

    void bit31IsNeverAButton()
    {
        OtherButtonItem item;
        QVector<qint32> groups(OtherButtonItem::kGroupCount, 0);
        groups[1] = (qint32(1) << 5) | qint32(1u << 31);
        item.setVisibleGroups(groups);
        QCOMPARE(shownIds(item), ints({Id::Anf}));
        QCOMPARE(item.visibleGroups().at(1), qint32(1) << 5);
        item.setButtonVisible(static_cast<Id>(31), true);
        QVERIFY(!item.isButtonVisible(static_cast<Id>(31)));
        QCOMPARE(item.visibleGroups().at(0), 0);

        // A record with bit 31 set in a group reads it as clear.
        QStringList fields = fieldsOf(item);
        fields[17] = QString::number(quint32(1u << 31) | 32u);
        OtherButtonItem restored;
        QVERIFY(restored.deserialize(fields.join(QLatin1Char('|'))));
        QCOMPARE(restored.visibleGroups().at(1), qint32(32));
        QCOMPARE(shownIds(restored), ints({Id::Anf}));
    }

    void field8HoldsTheOldMask()
    {
        OtherButtonItem item;
        item.setOnlyVisible(Id::Anf);
        QCOMPARE(fieldsOf(item).at(8), QString::number(1u << 10));
        item.setButtonVisible(Id::Xpa, true);   // old number 22
        item.setButtonVisible(Id::DisplayOff, true);  // old number 33: not in the mask
        item.setButtonVisible(static_cast<Id>(32), true);  // NR: no old number
        QCOMPARE(fieldsOf(item).at(8), QString::number((1u << 10) | (1u << 22)));
        // An older build reading field 8 alone loads the same subset.
        OtherButtonItem older;
        QVERIFY(older.deserialize(fieldsOf(item).mid(0, 15).join(QLatin1Char('|'))));
        QCOMPARE(older.isButtonVisible(Id::Anf), true);
        QCOMPARE(older.isButtonVisible(Id::Xpa), true);
        QCOMPARE(older.isButtonVisible(Id::Mon), false);
    }

    void anIdWithNoActionIsNeverDrawnAndKeepsItsVisibility()
    {
        const Id nr = static_cast<Id>(32);
        OtherButtonItem item;
        QVERIFY(!item.isOffered(nr));
        QVERIFY(!item.isOffered(Id::Power));
        QVERIFY(item.isOffered(Id::Anf));
        QVERIFY(item.isOffered(Id::Play));
        QVERIFY(item.isOffered(Id::Macro0));
        item.setButtonVisible(nr, true);
        QVERIFY(item.isButtonVisible(nr));
        QVERIFY(!item.isButtonShown(nr));
        OtherButtonItem restored;
        QVERIFY(restored.deserialize(item.serialize()));
        QVERIFY(restored.isButtonVisible(nr));
        QVERIFY(!restored.isButtonShown(nr));

        // A gated button keeps its gate.
        QVERIFY(item.isButtonVisible(Id::Play));
        QVERIFY(!item.isButtonShown(Id::Play));
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::Voice, true);
        OtherButtonItem built;
        QVERIFY(built.isButtonShown(Id::Play));
        QVERIFY(!built.isButtonShown(nr));
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::MacroButtons, true);
        OtherButtonItem macros;
        macros.setButtonVisible(Id::Macro0, true);
        QVERIFY(macros.isButtonShown(Id::Macro0));
        QCOMPARE(macros.button(macros.indexOf(Id::Macro0)).text, QStringLiteral("M0"));
    }
};

QTEST_MAIN(TstOtherButtonIds)
#include "tst_other_button_ids.moc"
