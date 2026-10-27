// no-port-check: NereusSDR-original test file.
//
// =================================================================
// tests/tst_rx_section_content.cpp  (NereusSDR)
// =================================================================
//
// The RX applet's jobs as independent container content.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - aBlankBandButtonIsDisabledWithAReason. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-23 - Brian Sutton, AI-assisted via xAI Grok.
//   2026-10-24 - The attenuator and RX1 preamp copies, AUTO AGC and
//                 Shift-click on a copy, inert previews, the shared face
//                 defaults. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-24 - aCopiedPassbandCentreDragKeepsTheWidth. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-24 - Its drag goes on past the cap with the slice already
//                 there, and the copy shows the stored filter. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - thePropertyEditorDoesNotCallARowUnsupported. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Each job's parts: the default look, the hide keys, the
//                 dynamic shows, the order, the editor and the store round
//                 trip. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - The Thetis band groups: the layouts, the jumps, a group
//                 per slice, the Nereus grid's same buttons and the saved
//                 visibility bits. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QJsonArray>
#include <QListWidget>
#include <QLineEdit>
#include <QLayout>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/HpsdrModel.h"
#include "core/P2RadioConnection.h"
#include "core/StepAttenuatorFacade.h"
#include "gui/applets/RxApplet.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/applets/RxControlSections.h"
#include "gui/applets/RxSectionContent.h"
#include "gui/applets/SectionParts.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/BandGroups.h"
#include "gui/meters/NereusBandButtonItem.h"
#include "gui/meters/NereusButtonFace.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

const char* const kTypes[] = {
    "rx.antennas", "rx.mode", "rx.step", "rx.filter", "rx.pan",
    "rx.squelch", "rx.attenuator", "rx.agc", "rx.rit", "rx.xit",
};

const char* const kTitles[] = {
    "RX Antennas", "RX Mode", "RX Step", "RX Filter", "RX Pan",
    "RX Squelch", "RX Attenuator", "RX AGC", "RX RIT", "RX XIT",
};

// The jobs that have parts: every one but Mode, which has one control.
const char* const kPartTypes[] = {
    "rx.antennas", "rx.step", "rx.filter", "rx.pan",
    "rx.squelch", "rx.attenuator", "rx.agc", "rx.rit", "rx.xit",
};

// The fixed width the geometry checks lay a copy out at.
constexpr int kCopyWidth = 320;

// A copy the way the factories built one before a job had parts: the
// applet's copy with no config applied.
QWidget* copyWithoutConfig(RxApplet& applet, const QString& type, QWidget* parent)
{
    if (type == QLatin1String("rx.antennas")) { return applet.createAntennasSection(parent); }
    if (type == QLatin1String("rx.step")) { return applet.createStepSection(parent); }
    if (type == QLatin1String("rx.filter")) { return applet.createFilterSection(parent); }
    if (type == QLatin1String("rx.pan")) { return applet.createPanSection(parent); }
    if (type == QLatin1String("rx.squelch")) { return applet.createSquelchSection(parent); }
    if (type == QLatin1String("rx.attenuator")) { return applet.createAttenuatorSection(parent); }
    if (type == QLatin1String("rx.agc")) { return applet.createAgcSection(parent); }
    if (type == QLatin1String("rx.rit")) { return applet.createRitSection(parent); }
    if (type == QLatin1String("rx.xit")) { return applet.createXitSection(parent); }
    return nullptr;
}

// A copy's parts, whatever its section type.
const SectionParts* partsOf(QWidget* copy)
{
    if (auto* s = qobject_cast<RxAntennasSection*>(copy)) { return &s->parts(); }
    if (auto* s = qobject_cast<RxStepSection*>(copy)) { return &s->parts(); }
    if (auto* s = qobject_cast<RxFilterSection*>(copy)) { return &s->parts(); }
    if (auto* s = qobject_cast<RxPanSection*>(copy)) { return &s->parts(); }
    if (auto* s = qobject_cast<RxSquelchSection*>(copy)) { return &s->parts(); }
    if (auto* s = qobject_cast<RxAttenuatorSection*>(copy)) { return &s->parts(); }
    if (auto* s = qobject_cast<RxAgcSection*>(copy)) { return &s->parts(); }
    if (auto* s = qobject_cast<RxRitSection*>(copy)) { return &s->parts(); }
    if (auto* s = qobject_cast<RxXitSection*>(copy)) { return &s->parts(); }
    return nullptr;
}

// Lays a copy out at the fixed width and returns its own rect, then every
// child's geometry and hidden state, in creation order.
QStringList laidOut(QWidget* copy)
{
    copy->setFixedWidth(kCopyWidth);
    copy->resize(kCopyWidth, copy->sizeHint().height());
    copy->layout()->activate();
    QStringList result;
    const auto describe = [](const QWidget* w) {
        const QRect g = w->geometry();
        return QStringLiteral("%1 %2 %3,%4 %5x%6 %7").arg(w->metaObject()->className(), w->objectName())
            .arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height())
            .arg(w->isHidden() ? QStringLiteral("hidden") : QStringLiteral("shown"));
    };
    result.append(describe(copy));
    for (const QWidget* child : copy->findChildren<QWidget*>()) {
        result.append(describe(child));
    }
    return result;
}

// The index of the item of a copy's column that is or holds `widget`.
int columnIndex(QWidget* copy, QWidget* widget)
{
    QLayout* column = copy->layout();
    for (int i = 0; i < column->count(); ++i) {
        QLayoutItem* item = column->itemAt(i);
        if (item->widget() == widget) {
            return i;
        }
        if (QLayout* line = item->layout()) {
            if (line->indexOf(widget) >= 0) {
                return i;
            }
        }
    }
    return -1;
}

// The index of `widget` in the line of a copy's column that holds it.
int lineIndex(QWidget* copy, QWidget* widget)
{
    const int index = columnIndex(copy, widget);
    if (index < 0 || !copy->layout()->itemAt(index)->layout()) {
        return -1;
    }
    return copy->layout()->itemAt(index)->layout()->indexOf(widget);
}

QList<QPushButton*> appletPresets(RxApplet& applet)
{
    QList<QPushButton*> presets;
    for (QPushButton* b : applet.findChildren<QPushButton*>()) {
        if (b->isCheckable() && b->toolTip().contains(QStringLiteral(" Hz to "))) {
            presets.append(b);
        }
    }
    return presets;
}

QPushButton* appletSql(RxApplet& applet)
{
    for (QPushButton* button : applet.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("SQL")) { return button; }
    }
    return nullptr;
}

// Every label text and tooltip the property editor shows.
QString editorText(const QWidget& editor)
{
    QStringList parts;
    for (const QLabel* label : editor.findChildren<QLabel*>()) {
        parts.append(label->text());
    }
    for (const QWidget* widget : editor.findChildren<QWidget*>()) {
        parts.append(widget->toolTip());
    }
    return parts.join(QLatin1Char('\n'));
}

} // namespace

class TestRxSectionContent : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        WorkspacePreview::setEnabledForTest(true);
    }

    void registryOffersEveryJob()
    {
        RadioModel radio;
        RxApplet applet(nullptr, &radio);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);

        for (int i = 0; i < 10; ++i) {
            const QString id = QString::fromLatin1(kTypes[i]);
            QVERIFY2(registry.hasWidgetFactory(id), qPrintable(id));
            QVERIFY2(registry.isAvailable(id), qPrintable(id));
            bool listed = false;
            for (const auto& descriptor : registry.descriptors()) {
                if (descriptor.typeId == id) {
                    listed = true;
                    QCOMPARE(descriptor.title, QString::fromLatin1(kTitles[i]));
                }
            }
            QVERIFY2(listed, qPrintable(id));
        }
    }

    void aCopyIsItsOwnInstance()
    {
        RadioModel radio;
        RxApplet applet(nullptr, &radio);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* first = registry.createWidget(registry.makeEntry(QStringLiteral("rx.squelch")), &host);
        auto* second = registry.createWidget(registry.makeEntry(QStringLiteral("rx.squelch")), &host);
        QVERIFY(first);
        QVERIFY(second);
        QVERIFY(first != second);
        QVERIFY(first != &applet);
        auto* firstSql = first->findChild<QPushButton*>(QStringLiteral("RxCopySqlButton"));
        auto* secondSql = second->findChild<QPushButton*>(QStringLiteral("RxCopySqlButton"));
        QVERIFY(firstSql);
        QVERIFY(secondSql);
        QVERIFY(firstSql != secondSql);
        QVERIFY(firstSql != appletSql(applet));
    }

    void theCopyShowsAndSetsTheSlice()
    {
        RadioModel radio;
        SliceModel slice(0);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry(QStringLiteral("rx.squelch")), &host);
        auto* button = copy->findChild<QPushButton*>(QStringLiteral("RxCopySqlButton"));
        QPushButton* own = appletSql(applet);
        QVERIFY(button);
        QVERIFY(own);
        QCOMPARE(slice.ssqlEnabled(), false);
        QVERIFY(!button->isChecked());
        QVERIFY(!own->isChecked());

        button->click();
        QCOMPARE(slice.ssqlEnabled(), true);
        QVERIFY(own->isChecked());
        QVERIFY(button->isChecked());

        own->click();
        QCOMPARE(slice.ssqlEnabled(), false);
        QVERIFY(!button->isChecked());
    }

    void theAppletPanelKeepsItsOwnFace()
    {
        RadioModel radio;
        RxApplet applet(nullptr, &radio);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry(QStringLiteral("rx.antennas")), &host);
        auto* copyRx = copy->findChild<QPushButton*>(QStringLiteral("RxCopyRxAntButton"));
        auto* ownRx = applet.findChild<QPushButton*>(QStringLiteral("m_rxAntBtn"));
        QVERIFY(copyRx);
        QVERIFY(ownRx);
        QVERIFY(copyRx->styleSheet().contains(QStringLiteral("qlineargradient")));
        QVERIFY(ownRx->styleSheet().contains(QStringLiteral("#4488ff")));
        QVERIFY(!ownRx->styleSheet().contains(QStringLiteral("qlineargradient")));
    }

    void listeningHoldsTheCopy()
    {
        RadioModel radio;
        RxApplet applet(nullptr, &radio);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry(QStringLiteral("rx.squelch")), &host);
        auto* button = copy->findChild<QPushButton*>(QStringLiteral("RxCopySqlButton"));
        QVERIFY(button);
        QVERIFY(button->isEnabled());

        VfoWidget::SliceAccess access;
        access.state = VfoWidget::SliceAccess::State::Listening;
        access.heldReason = QStringLiteral("Another device controls this slice");
        applet.setSliceAccess(access);

        QVERIFY(!button->isEnabled());
        QCOMPARE(button->toolTip(), access.heldReason);
    }

    void theFilterCopyHasThreeSections()
    {
        RadioModel radio;
        SliceModel slice(0);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = qobject_cast<RxFilterSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.filter")), &host));
        QVERIFY(copy);
        QLayout* layout = copy->layout();
        QVERIFY(layout);
        QCOMPARE(layout->count(), 3);

        auto* width = qobject_cast<QLabel*>(layout->itemAt(0)->widget());
        QVERIFY(width);
        QCOMPARE(width->objectName(), QStringLiteral("RxCopyFilterWidth"));

        QWidget* gridHost = layout->itemAt(1)->widget();
        QVERIFY(gridHost);
        QVERIFY(gridHost->layout());
        QCOMPARE(gridHost->layout()->count(), copy->buttons().size());
        QVERIFY(copy->buttons().size() > 0);
        QCOMPARE(copy->grid()->columnCount(), 3);

        auto* passband = qobject_cast<FilterPassbandWidget*>(layout->itemAt(2)->widget());
        QVERIFY(passband);
        QCOMPARE(passband->objectName(), QStringLiteral("RxCopyFilterPassband"));
        host.show();
        QVERIFY(width->isVisible());
        QVERIFY(gridHost->isVisible());
        QVERIFY(passband->isVisible());

        const QVector<ContentProperty> properties = registry.declaredProperties(QStringLiteral("rx.filter"));
        QCOMPARE(properties.size(), 10);
        QCOMPARE(properties.at(0).label, QStringLiteral("Hide width label"));
        QCOMPARE(properties.at(1).label, QStringLiteral("Hide passband"));
        QCOMPARE(properties.at(2).key, QStringLiteral("columns"));
        QCOMPARE(properties.at(2).label, QStringLiteral("Columns"));
        QCOMPARE(properties.at(2).minimum, 1);
        QCOMPARE(properties.at(2).maximum, 32);
        QCOMPARE(properties.at(3).label, QStringLiteral("Button text"));
        QCOMPARE(properties.at(4).label, QStringLiteral("Background top"));
        QCOMPARE(properties.at(5).label, QStringLiteral("Background bottom"));
        QCOMPARE(properties.at(6).label, QStringLiteral("Selected top"));
        QCOMPARE(properties.at(7).label, QStringLiteral("Selected bottom"));
        // The preset grid's toggle and the order come after.
        QCOMPARE(properties.at(8).key, QStringLiteral("hidePresets"));
        QCOMPARE(properties.at(8).label, QStringLiteral("Hide Preset buttons"));
        QCOMPARE(properties.at(8).kind, ContentProperty::Kind::Toggle);
        QCOMPARE(properties.at(9).key, QStringLiteral("order"));
        QCOMPARE(properties.at(9).kind, ContentProperty::Kind::Order);
        QCOMPARE(properties.at(9).defaultValue.toArray(), (QJsonArray{"top", "presets", "bottom"}));

        ContentEntry hidden = registry.makeEntry(QStringLiteral("rx.filter"));
        hidden.config.insert(QStringLiteral("hideTop"), true);
        hidden.config.insert(QStringLiteral("hideBottom"), true);
        auto* folded = qobject_cast<RxFilterSection*>(registry.createWidget(hidden, &host));
        QVERIFY(folded);
        QVERIFY(folded->widthLabel()->isHidden());
        QVERIFY(!folded->grid()->parentWidget()->isHidden());
        QVERIFY(folded->passband()->isHidden());

        ContentEntry narrow = registry.makeEntry(QStringLiteral("rx.filter"));
        narrow.config.insert(QStringLiteral("columns"), 2);
        narrow.config.insert(QStringLiteral("buttonText"), QStringLiteral("#112233"));
        narrow.config.insert(QStringLiteral("backgroundTop"), QStringLiteral("#222227"));
        narrow.config.insert(QStringLiteral("selectedTop"), QStringLiteral("#306247"));
        auto* shaped = qobject_cast<RxFilterSection*>(registry.createWidget(narrow, &host));
        QVERIFY(shaped);
        QCOMPARE(shaped->columns(), 2);
        QVERIFY(shaped->buttons().size() >= 3);
        QVERIFY(shaped->grid()->itemAtPosition(0, 0));
        QVERIFY(shaped->grid()->itemAtPosition(0, 1));
        QVERIFY(!shaped->grid()->itemAtPosition(0, 2));
        QVERIFY(shaped->grid()->itemAtPosition(1, 0));
        const QString face = shaped->buttons().first()->styleSheet();
        QVERIFY(face.contains(QStringLiteral("#112233")));
        QVERIFY(face.contains(QStringLiteral("#222227")));
        QVERIFY(face.contains(QStringLiteral("#141417")));
        QVERIFY(face.contains(QStringLiteral("#306247")));
        QVERIFY(face.contains(QStringLiteral("#1d3d2c")));
    }

    void aDestroyedCopyLeavesTheApplet()
    {
        RadioModel radio;
        SliceModel slice(0);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry(QStringLiteral("rx.squelch")), &host);
        QVERIFY(copy);
        delete copy;
        slice.setSsqlEnabled(true);
        QPushButton* own = appletSql(applet);
        QVERIFY(own);
        QVERIFY(own->isChecked());
    }

    // A copy's RX1 preamp is the applet's own toggle: ticking it in the
    // copy ticks the applet's box and reaches the radio, and a later
    // refresh does not put the stale state back.
    void theRx1PreampCopyIsTheAppletsToggle()
    {
        P2RadioConnection connection;
        connection.setBoardForTest(HPSDRHW::OrionMKII);
        RadioModel model;
        model.injectConnectionForTest(&connection);
        const auto uninject = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        SliceModel slice(0);
        RxApplet applet(nullptr, &model);
        applet.setSlice(&slice);
        model.setBoardForTest(HPSDRHW::OrionMKII);
        applet.setBoardCapabilities(model.boardCapabilities());
        auto* own = applet.findChild<QCheckBox*>(QStringLiteral("RxRx1PreampToggle"));
        QVERIFY(own);
        QVERIFY(!own->isChecked());

        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = qobject_cast<RxAttenuatorSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.attenuator")), &host));
        QVERIFY(copy);
        QCheckBox* preamp = copy->rx1Preamp();
        QVERIFY(preamp);
        QVERIFY(!preamp->isChecked());

        preamp->click();
        QVERIFY(preamp->isChecked());
        QVERIFY(own->isChecked());
        QTRY_VERIFY(([&] {
            quint8 packet[1444]{};
            connection.composeCmdHighPriorityForTest(packet);
            return (packet[1403] & 0x02) != 0;
        })());

        // Any later refresh of the copies keeps the shared state.
        slice.setSsqlEnabled(true);
        QVERIFY(preamp->isChecked());

        own->click();
        QVERIFY(!own->isChecked());
        QVERIFY(!preamp->isChecked());
        QCoreApplication::processEvents();
        quint8 after[1444]{};
        connection.composeCmdHighPriorityForTest(after);
        QCOMPARE(after[1403] & 0x02, 0);
    }

    // A copy's step attenuator and preamp choice reach the Core through
    // the applet's own handlers and show on the applet's row.
    void theAttenuatorCopyEditsThroughTheApplet()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setBoardForTest(HPSDRHW::Hermes);
        RxApplet applet(nullptr, &remote);
        StepAttenuatorFacade* stepAtt = remote.stepAttFacade();
        QVERIFY(stepAtt);
        stepAtt->setWindowAvailability(true, QString());
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = qobject_cast<RxAttenuatorSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.attenuator")), &host));
        QVERIFY(copy);
        QComboBox* preamp = copy->preampCombo();
        QCOMPARE(preamp->count(), applet.preampComboItemCountForTest());
        QVERIFY(preamp->count() > 5);
        QVERIFY(preamp->isEnabled());

        preamp->setCurrentIndex(5);
        QCOMPARE(stepAtt->preampMode(), preamp->itemData(5).toInt());
        // The Core's echo comes back to the copy through the applet.
        stepAtt->setPreampMode(preamp->itemData(3).toInt());
        QCOMPARE(preamp->currentIndex(), 3);

        QSpinBox* spin = copy->stepSpin();
        QVERIFY(spin);
        QVERIFY(spin->maximum() >= 10);
        spin->setValue(10);
        QCOMPARE(stepAtt->attenuationDbForSlice(0), 10);
        stepAtt->setAttenuationDb(4);
        QCOMPARE(spin->value(), 4);
    }

    void autoAgcOnACopy()
    {
        RadioModel radio;
        SliceModel slice(0);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = qobject_cast<RxAgcSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.agc")), &host));
        QVERIFY(copy);
        QSignalSpy toggled(&applet, &RxApplet::autoAgcToggled);
        QVERIFY(!slice.autoAgcEnabled());

        copy->autoButton()->click();
        QCOMPARE(slice.autoAgcEnabled(), true);
        QCOMPARE(toggled.count(), 1);
        QCOMPARE(toggled.at(0).at(0).toBool(), true);
    }

    // Shift-click on a copied preset sets the RX filter and matches the TX
    // passband, as the applet's own preset does.
    void shiftClickOnACopiedPreset()
    {
        RadioModel radio;
        SliceModel slice(0);
        slice.setDspMode(DSPMode::LSB);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = qobject_cast<RxFilterSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.filter")), &host));
        QVERIFY(copy);
        auto* column = new QVBoxLayout(&host);
        column->addWidget(copy);
        host.resize(400, 300);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QVERIFY(copy->buttons().size() > 1);
        QCOMPARE(copy->buttons().size(), appletPresets(applet).size());

        QPushButton* preset = copy->buttons().at(1);
        const QString tip = preset->toolTip();
        TransmitModel& tx = radio.transmitModel();
        QSignalSpy txFilter(&tx, &TransmitModel::filterChanged);
        QTest::mouseClick(preset, Qt::LeftButton, Qt::ShiftModifier);

        const int low = slice.filterLow();
        const int high = slice.filterHigh();
        QVERIFY2(tip.contains(QStringLiteral(": %1 Hz to %2 Hz").arg(low).arg(high)),
                 qPrintable(tip));
        QVERIFY(txFilter.count() > 0);
        QCOMPARE(tx.filterLow(), qMin(qAbs(low), qAbs(high)));
        QCOMPARE(tx.filterHigh(), qMax(qAbs(low), qAbs(high)));
        QVERIFY(preset->isChecked());
    }

    // A preview (ContentRenderMode::Preview) is inert: it changes nothing,
    // takes no focus, follows no slice and never rebuilds the applet's own
    // filter buttons.
    void aPreviewIsInert()
    {
        RadioModel radio;
        SliceModel slice(0);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;

        const QList<QPushButton*> before = appletPresets(applet);
        QVERIFY(!before.isEmpty());
        const QPointer<QPushButton> firstOwn(before.first());

        for (const char* type : kTypes) {
            QWidget* preview = registry.createWidget(
                registry.makeEntry(QString::fromLatin1(type)), &host, ContentRenderMode::Preview);
            QVERIFY2(preview, type);
            QVERIFY2(!preview->isEnabled(), type);
            for (QWidget* child : preview->findChildren<QWidget*>()) {
                QVERIFY2(child->focusPolicy() == Qt::NoFocus, type);
            }
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(firstOwn);
        QCOMPARE(appletPresets(applet), before);

        auto* filter = qobject_cast<RxFilterSection*>(registry.createWidget(
            registry.makeEntry(QStringLiteral("rx.filter")), &host, ContentRenderMode::Preview));
        QVERIFY(filter);
        QCOMPARE(filter->buttons().size(), before.size());

        auto* squelch = registry.createWidget(
            registry.makeEntry(QStringLiteral("rx.squelch")), &host, ContentRenderMode::Preview);
        auto* sql = squelch->findChild<QPushButton*>(QStringLiteral("RxCopySqlButton"));
        QVERIFY(sql);
        QVERIFY(!sql->isChecked());
        slice.setSsqlEnabled(true);
        QVERIFY(appletSql(applet)->isChecked());
        QVERIFY(!sql->isChecked());

        // A preview is not one of the applet's copies: listening does not
        // touch its tooltip.
        const QString tipBefore = sql->toolTip();
        VfoWidget::SliceAccess access;
        access.state = VfoWidget::SliceAccess::State::Listening;
        access.heldReason = QStringLiteral("Another device controls this slice");
        applet.setSliceAccess(access);
        QCOMPARE(sql->toolTip(), tipBefore);
    }

    // The band buttons and the RX Filter copy share one set of face
    // defaults.
    void theFaceDefaultsHaveOneSource()
    {
        NereusBandButtonItem bands;
        QCOMPARE(bands.buttonTextColor(), QColor(NereusButtonFace::kText));
        QCOMPARE(bands.backgroundTopColor(), QColor(NereusButtonFace::kRestingTop));
        QCOMPARE(bands.backgroundBottomColor(), QColor(NereusButtonFace::kRestingBottom));
        QCOMPARE(bands.selectedTopColor(), QColor(NereusButtonFace::kSelectedTop));
        QCOMPARE(bands.selectedBottomColor(), QColor(NereusButtonFace::kSelectedBottom));

        RadioModel radio;
        RxApplet applet(nullptr, &radio);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        const QVector<ContentProperty> properties = registry.declaredProperties(QStringLiteral("rx.filter"));
        QCOMPARE(properties.size(), 10);
        QCOMPARE(properties.at(3).defaultValue.toString(), QString::fromLatin1(NereusButtonFace::kText));
        QCOMPARE(properties.at(4).defaultValue.toString(), QString::fromLatin1(NereusButtonFace::kRestingTop));
        QCOMPARE(properties.at(5).defaultValue.toString(), QString::fromLatin1(NereusButtonFace::kRestingBottom));
        QCOMPARE(properties.at(6).defaultValue.toString(), QString::fromLatin1(NereusButtonFace::kSelectedTop));
        QCOMPARE(properties.at(7).defaultValue.toString(), QString::fromLatin1(NereusButtonFace::kSelectedBottom));
    }

    // A copy's passband centre drag moves the filter with shiftFilter, as
    // the applet's own passband does: past the cap the width is kept
    // (8100..10000).
    void aCopiedPassbandCentreDragKeepsTheWidth()
    {
        RadioModel radio;
        SliceModel slice(0);
        slice.setDspMode(DSPMode::USB);
        slice.setFilter(8000, 9900);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* copy = qobject_cast<RxFilterSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.filter")), &host));
        QVERIFY(copy);
        FilterPassbandWidget* passband = copy->passband();
        QVERIFY(passband);
        // 632 px: 600 usable, 10 Hz per pixel; the centre zone runs from
        // x 105 to 527.
        passband->resize(632, 48);
        QCOMPARE(passband->filterLo(), 8000);
        QCOMPARE(passband->filterHi(), 9900);
        QSignalSpy shifted(passband, &FilterPassbandWidget::passbandShifted);

        const auto mouse = [passband](QEvent::Type type, int x, Qt::MouseButton button,
                                      Qt::MouseButtons buttons) {
            QMouseEvent me(type, QPointF(x, 20), passband->mapToGlobal(QPointF(x, 20)),
                           button, buttons, Qt::NoModifier);
            QApplication::sendEvent(passband, &me);
        };
        // 300 px right in the centre zone: +3000 Hz, 11000..12900 asked.
        // Then 310 px (11100..13000) with the slice already at the cap.
        mouse(QEvent::MouseButtonPress, 150, Qt::LeftButton, Qt::LeftButton);
        mouse(QEvent::MouseMove, 450, Qt::NoButton, Qt::LeftButton);
        QCOMPARE(slice.filterLow(), 8100);
        QCOMPARE(slice.filterHigh(), 10000);
        mouse(QEvent::MouseMove, 460, Qt::NoButton, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, 460, Qt::LeftButton, Qt::NoButton);

        QCOMPARE(shifted.count(), 2);
        QCOMPARE(slice.filterLow(), 8100);
        QCOMPARE(slice.filterHigh(), 10000);
        QCOMPARE(passband->filterLo(), 8100);
        QCOMPARE(passband->filterHi(), 10000);
    }

    // ── The property editor: an RX row is a known type with no options ──
    void thePropertyEditorDoesNotCallARowUnsupported()
    {
        RadioModel radio;
        RxApplet applet(nullptr, &radio);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);

        for (const char* type : kTypes) {
            const QString id = QString::fromLatin1(type);
            ContentPropertyEditor editor(registry);
            editor.setEntry(registry.makeEntry(id));
            QVERIFY2(!editorText(editor).contains(QStringLiteral("Unsupported")), qPrintable(id));
            QVERIFY2(editor.findChild<QLineEdit*>(QStringLiteral("contentName")), qPrintable(id));
            QVERIFY2(editor.findChild<QCheckBox*>(QStringLiteral("contentVisible")), qPrintable(id));
            // Only a row that declares options (TX Keying, RX Filter) has
            // an item-specific group; every other row has none.
            QCOMPARE(editor.findChild<QGroupBox*>(QStringLiteral("declaredProperties")) != nullptr,
                     !registry.declaredProperties(id).isEmpty());
        }

        // A type the registry does not know (a layout from a newer build)
        // still says so: the registry's unknown-type reason, the original
        // data retained.
        ContentEntry unknown = registry.makeEntry(QString::fromLatin1(kTypes[0]));
        unknown.typeId = QStringLiteral("rx.fromANewerBuild");
        ContentPropertyEditor unknownEditor(registry);
        unknownEditor.setEntry(unknown);
        QVERIFY(!registry.isAvailable(unknown.typeId));
        QVERIFY(editorText(unknownEditor).contains(registry.unavailableReason(unknown.typeId)));
        QVERIFY(editorText(unknownEditor).contains(QStringLiteral("Unknown content type; original data retained")));

        // An unavailable row shows its reason.
        const QString first = QString::fromLatin1(kTypes[0]);
        registry.setAvailable(first, false, QStringLiteral("Test reason: not available here"));
        ContentPropertyEditor unavailableEditor(registry);
        unavailableEditor.setEntry(registry.makeEntry(first));
        QVERIFY(editorText(unavailableEditor).contains(QStringLiteral("Test reason: not available here")));
        QVERIFY(!editorText(unavailableEditor).contains(QStringLiteral("Unsupported configuration")));
    }
    // ── Each job's parts ────────────────────────────────────────────────
    // With no part keys, and with an order equal to the built one, a copy
    // lays out exactly as a copy built without parts did.
    void theDefaultLookIsUnchanged()
    {
        RadioModel radio;
        SliceModel slice(0);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);

        for (const char* type : kPartTypes) {
            const QString id = QString::fromLatin1(type);
            QWidget host;
            QWidget* before = copyWithoutConfig(applet, id, &host);
            QWidget* after = registry.createWidget(registry.makeEntry(id), &host);
            QVERIFY2(before && after, type);
            const SectionParts* parts = partsOf(after);
            QVERIFY2(parts && !parts->ids().isEmpty(), type);

            ContentEntry ordered = registry.makeEntry(id);
            ordered.config.insert(SectionParts::orderKey(), QJsonArray::fromStringList(parts->ids()));
            QWidget* builtOrder = registry.createWidget(ordered, &host);
            QVERIFY2(builtOrder, type);

            const QStringList expected = laidOut(before);
            QCOMPARE(laidOut(after), expected);
            QCOMPARE(laidOut(builtOrder), expected);
        }

        // Mode has one control and no parts.
        QVERIFY(registry.declaredProperties(QStringLiteral("rx.mode")).isEmpty());
    }

    void eachHideKeyHidesExactlyItsPart()
    {
        RadioModel radio;
        SliceModel slice(0);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);

        for (const char* type : kPartTypes) {
            const QString id = QString::fromLatin1(type);
            QWidget host;
            QWidget* plain = registry.createWidget(registry.makeEntry(id), &host);
            QVERIFY2(plain, type);
            const QStringList partIds = partsOf(plain)->ids();
            for (const QString& part : partIds) {
                ContentEntry entry = registry.makeEntry(id);
                entry.config.insert(SectionParts::hideKey(part), true);
                QWidget* copy = registry.createWidget(entry, &host);
                QVERIFY2(copy, qPrintable(part));
                const SectionParts* parts = partsOf(copy);
                for (const QString& other : partIds) {
                    QCOMPARE(parts->isHidden(other), other == part);
                    const QList<QWidget*> mine = parts->widgets(other);
                    const QList<QWidget*> theirs = partsOf(plain)->widgets(other);
                    QCOMPARE(mine.size(), theirs.size());
                    for (int i = 0; i < mine.size(); ++i) {
                        if (other == part) {
                            QVERIFY2(mine.at(i)->isHidden(), qPrintable(id + QLatin1Char(' ') + part));
                        } else {
                            QCOMPARE(mine.at(i)->isHidden(), theirs.at(i)->isHidden());
                        }
                    }
                }
            }
        }

        // RX Filter's old keys name its top and bottom; hidePresets hides
        // the preset grid.
        QCOMPARE(SectionParts::hideKey(QStringLiteral("top")), QStringLiteral("hideTop"));
        QCOMPARE(SectionParts::hideKey(QStringLiteral("bottom")), QStringLiteral("hideBottom"));
        QWidget host;
        ContentEntry noPresets = registry.makeEntry(QStringLiteral("rx.filter"));
        noPresets.config.insert(QStringLiteral("hidePresets"), true);
        auto* filter = qobject_cast<RxFilterSection*>(registry.createWidget(noPresets, &host));
        QVERIFY(filter);
        QVERIFY(filter->grid()->parentWidget()->isHidden());
        QVERIFY(!filter->widthLabel()->isHidden());
        QVERIFY(!filter->passband()->isHidden());
        QVERIFY(!filter->buttons().isEmpty());
    }

    // A hidden antenna button or preamp check box stays hidden when the
    // board brings them.
    void aHiddenPartStaysHiddenWhenTheAppletShowsIt()
    {
        P2RadioConnection connection;
        connection.setBoardForTest(HPSDRHW::OrionMKII);
        RadioModel model;
        model.injectConnectionForTest(&connection);
        const auto uninject = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        SliceModel slice(0);
        RxApplet applet(nullptr, &model);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;

        // The copies follow what the applet's own panel shows, so the
        // panel is on screen.
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));

        ContentEntry noRxAnt = registry.makeEntry(QStringLiteral("rx.antennas"));
        noRxAnt.config.insert(QStringLiteral("hideRxAntenna"), true);
        auto* hiddenAnt = qobject_cast<RxAntennasSection*>(registry.createWidget(noRxAnt, &host));
        auto* shownAnt = qobject_cast<RxAntennasSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.antennas")), &host));
        ContentEntry noPreamp = registry.makeEntry(QStringLiteral("rx.attenuator"));
        noPreamp.config.insert(QStringLiteral("hidePreamp"), true);
        auto* hiddenAtt = qobject_cast<RxAttenuatorSection*>(registry.createWidget(noPreamp, &host));
        auto* shownAtt = qobject_cast<RxAttenuatorSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.attenuator")), &host));
        QVERIFY(hiddenAnt && shownAnt && hiddenAtt && shownAtt);

        model.setBoardForTest(HPSDRHW::OrionMKII);
        const BoardCapabilities caps = model.boardCapabilities();
        QVERIFY(caps.hasAlex && caps.antennaInputCount >= 3);
        QVERIFY(caps.p2PreampPerAdc);
        applet.setBoardCapabilities(caps);

        QVERIFY(hiddenAnt->rxButton()->isHidden());
        QVERIFY(!hiddenAnt->txButton()->isHidden());
        QVERIFY(!shownAnt->rxButton()->isHidden());
        QVERIFY(!shownAnt->txButton()->isHidden());
        QVERIFY(hiddenAtt->rx1Preamp()->isHidden());
        QVERIFY(!shownAtt->rx1Preamp()->isHidden());

        // Copies made once the board is known follow the same rule.
        auto* lateHiddenAnt = qobject_cast<RxAntennasSection*>(registry.createWidget(noRxAnt, &host));
        auto* lateShownAnt = qobject_cast<RxAntennasSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.antennas")), &host));
        auto* lateHiddenAtt = qobject_cast<RxAttenuatorSection*>(registry.createWidget(noPreamp, &host));
        auto* lateShownAtt = qobject_cast<RxAttenuatorSection*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("rx.attenuator")), &host));
        QVERIFY(lateHiddenAnt && lateShownAnt && lateHiddenAtt && lateShownAtt);
        QVERIFY(lateHiddenAnt->rxButton()->isHidden());
        QVERIFY(!lateHiddenAnt->txButton()->isHidden());
        QVERIFY(!lateShownAnt->rxButton()->isHidden());
        QVERIFY(lateHiddenAtt->rx1Preamp()->isHidden());
        QVERIFY(!lateShownAtt->rx1Preamp()->isHidden());

        // The applet's own panel keeps its buttons.
        auto* ownRx = applet.findChild<QPushButton*>(QStringLiteral("m_rxAntBtn"));
        QVERIFY(ownRx);
        QVERIFY(!ownRx->isHidden());
    }

    void theOrderMovesParts()
    {
        RadioModel radio;
        SliceModel slice(0);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;

        const auto copyWithOrder = [&](const char* type, const QStringList& order) {
            ContentEntry entry = registry.makeEntry(QString::fromLatin1(type));
            entry.config.insert(SectionParts::orderKey(), QJsonArray::fromStringList(order));
            QWidget* copy = registry.createWidget(entry, &host);
            if (copy) {
                laidOut(copy);
            }
            return copy;
        };

        // RIT reversed.
        auto* rit = qobject_cast<RxRitSection*>(
            copyWithOrder("rx.rit", {"plus", "offset", "minus", "zero", "on"}));
        QVERIFY(rit);
        QCOMPARE(lineIndex(rit, rit->plus()), 0);
        QCOMPARE(lineIndex(rit, rit->label()), 1);
        QCOMPARE(lineIndex(rit, rit->minus()), 2);
        QCOMPARE(lineIndex(rit, rit->zeroButton()), 3);
        QCOMPARE(lineIndex(rit, rit->onButton()), 4);
        QVERIFY(rit->plus()->geometry().x() < rit->label()->geometry().x());
        QVERIFY(rit->zeroButton()->geometry().x() < rit->onButton()->geometry().x());
        // Each keeps its stretch.
        auto* ritLine = qobject_cast<QBoxLayout*>(rit->layout()->itemAt(0)->layout());
        QVERIFY(ritLine);
        QCOMPARE(ritLine->stretch(lineIndex(rit, rit->onButton())), 1);
        QCOMPARE(ritLine->stretch(lineIndex(rit, rit->label())), 1);
        QCOMPARE(ritLine->stretch(lineIndex(rit, rit->zeroButton())), 0);

        // XIT the same way.
        auto* xit = qobject_cast<RxXitSection*>(
            copyWithOrder("rx.xit", {"plus", "offset", "minus", "zero", "on"}));
        QVERIFY(xit);
        QCOMPARE(lineIndex(xit, xit->plus()), 0);
        QCOMPARE(lineIndex(xit, xit->onButton()), 4);

        // RX Filter with the passband first.
        auto* filter = qobject_cast<RxFilterSection*>(
            copyWithOrder("rx.filter", {"bottom", "presets", "top"}));
        QVERIFY(filter);
        QCOMPARE(columnIndex(filter, filter->passband()), 0);
        QCOMPARE(columnIndex(filter, filter->grid()->parentWidget()), 1);
        QCOMPARE(columnIndex(filter, filter->widthLabel()), 2);
        QVERIFY(filter->passband()->geometry().y() < filter->widthLabel()->geometry().y());
        QCOMPARE(filter->buttons().size(), appletPresets(applet).size());

        // AGC: the slider above the mode line; the line moves as a unit.
        auto* agc = qobject_cast<RxAgcSection*>(copyWithOrder("rx.agc", {"slider", "auto", "mode"}));
        QVERIFY(agc);
        QCOMPARE(columnIndex(agc, agc->slider()), 0);
        QCOMPARE(columnIndex(agc, agc->combo()), 1);
        QCOMPARE(columnIndex(agc, agc->autoButton()), 1);
        QVERIFY(lineIndex(agc, agc->autoButton()) < lineIndex(agc, agc->combo()));
        QCOMPARE(columnIndex(agc, agc->infoLabel()), 2);
    }

    void theEditorOffersTheTogglesAndTheOrder()
    {
        RadioModel radio;
        RxApplet applet(nullptr, &radio);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        const QString reason =
            QStringLiteral("These controls follow the RX panel; they have no source of their own.");
        const auto advancedOf = [](const QWidget& editor) -> QGroupBox* {
            for (QGroupBox* group : editor.findChildren<QGroupBox*>()) {
                if (group->title() == QStringLiteral("Advanced bindings")) {
                    return group;
                }
            }
            return nullptr;
        };

        for (const char* type : kPartTypes) {
            const QString id = QString::fromLatin1(type);
            QWidget host;
            QWidget* copy = registry.createWidget(registry.makeEntry(id), &host);
            QVERIFY2(copy, type);
            const QStringList partIds = partsOf(copy)->ids();

            const QVector<ContentProperty> properties = registry.declaredProperties(id);
            QVERIFY2(!properties.isEmpty(), type);
            QCOMPARE(properties.last().kind, ContentProperty::Kind::Order);
            QCOMPARE(properties.last().key, QStringLiteral("order"));
            QCOMPARE(properties.last().choices.size(), partIds.size());
            for (const ContentProperty& property : properties) {
                QVERIFY2(!property.label.contains(QStringLiteral("RX1"))
                         && !property.label.contains(QStringLiteral("RX2")), qPrintable(property.label));
            }

            ContentPropertyEditor editor(registry);
            editor.setEntry(registry.makeEntry(id));
            QVERIFY2(!editorText(editor).contains(QStringLiteral("Unsupported")), type);
            QVERIFY2(!editorText(editor).contains(QStringLiteral("RX1")), type);
            QVERIFY2(!editorText(editor).contains(QStringLiteral("RX2")), type);
            QVERIFY2(editor.findChild<QGroupBox*>(QStringLiteral("declaredProperties")), type);
            auto* list = editor.findChild<QListWidget*>(QStringLiteral("property_orderList"));
            QVERIFY2(list, type);
            QCOMPARE(list->count(), partIds.size());
            for (const QString& part : partIds) {
                auto* toggle = editor.findChild<QCheckBox*>(
                    QStringLiteral("property_") + SectionParts::hideKey(part));
                QVERIFY2(toggle, qPrintable(id + QLatin1Char(' ') + part));
                QVERIFY(!toggle->isChecked());
            }

            QGroupBox* advanced = advancedOf(editor);
            QVERIFY2(advanced, type);
            QVERIFY(!advanced->isEnabled());
            QCOMPARE(advanced->toolTip(), reason);

            // Each toggle writes its key.
            QSignalSpy edited(&editor, &ContentPropertyEditor::entryEdited);
            for (const QString& part : partIds) {
                const QString key = SectionParts::hideKey(part);
                editor.findChild<QCheckBox*>(QStringLiteral("property_") + key)->setChecked(true);
                QCOMPARE(qvariant_cast<ContentEntry>(edited.last().at(0)).config.value(key), QJsonValue(true));
            }
        }

        // Mode: no item group, nothing unsupported, Advanced disabled.
        ContentPropertyEditor modeEditor(registry);
        modeEditor.setEntry(registry.makeEntry(QStringLiteral("rx.mode")));
        QVERIFY(!modeEditor.findChild<QGroupBox*>(QStringLiteral("declaredProperties")));
        QVERIFY(!editorText(modeEditor).contains(QStringLiteral("Unsupported")));
        QVERIFY(advancedOf(modeEditor));
        QVERIFY(!advancedOf(modeEditor)->isEnabled());
        QCOMPARE(advancedOf(modeEditor)->toolTip(), reason);

        // RX Filter's two old toggles keep their labels.
        ContentPropertyEditor filterEditor(registry);
        filterEditor.setEntry(registry.makeEntry(QStringLiteral("rx.filter")));
        QVERIFY(editorText(filterEditor).contains(QStringLiteral("Hide width label")));
        QVERIFY(editorText(filterEditor).contains(QStringLiteral("Hide passband")));
        QVERIFY(editorText(filterEditor).contains(QStringLiteral("Hide Preset buttons")));

        // The order list moves the passband up; back to the built order
        // writes no order.
        auto* list = filterEditor.findChild<QListWidget*>(QStringLiteral("property_orderList"));
        auto* up = filterEditor.findChild<QPushButton*>(QStringLiteral("property_orderUp"));
        auto* down = filterEditor.findChild<QPushButton*>(QStringLiteral("property_orderDown"));
        QVERIFY(list && up && down);
        QCOMPARE(list->item(2)->text(), QStringLiteral("Passband"));
        QSignalSpy edited(&filterEditor, &ContentPropertyEditor::entryEdited);
        list->setCurrentRow(2);
        up->click();
        QCOMPARE(qvariant_cast<ContentEntry>(edited.last().at(0)).config.value(QStringLiteral("order")),
                 QJsonValue(QJsonArray{"top", "bottom", "presets"}));
        down->click();
        QVERIFY(!qvariant_cast<ContentEntry>(edited.last().at(0)).config.contains(QStringLiteral("order")));
    }

    // A copy's part keys and order survive a save and a load, as the
    // workspace store writes them, and lay the loaded copy out.
    void thePartsSurviveTheStore()
    {
        WorkspaceDocument doc;
        doc.mainContainerId = QStringLiteral("main");
        ContainerDocument container;
        container.id = doc.mainContainerId;
        container.name = QStringLiteral("RX");
        container.layout = ContentLayout::VerticalStack;
        ContentEntry rit;
        rit.id = QStringLiteral("rx-rit-1");
        rit.typeId = QStringLiteral("rx.rit");
        rit.name = QStringLiteral("RX RIT");
        rit.config = {{"hideZero", true}, {"order", QJsonArray{"plus", "offset", "minus", "zero", "on"}}};
        container.contents.append(rit);
        ContentEntry filter;
        filter.id = QStringLiteral("rx-filter-1");
        filter.typeId = QStringLiteral("rx.filter");
        filter.name = QStringLiteral("RX Filter");
        filter.config = {{"hideTop", true}, {"hidePresets", false}, {"columns", 2},
                         {"order", QJsonArray{"bottom", "presets", "top"}}};
        container.contents.append(filter);
        doc.containers.append(container);

        const DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        const ContentEntry& loadedRit = decoded.document.containers.at(0).contents.at(0);
        const ContentEntry& loadedFilter = decoded.document.containers.at(0).contents.at(1);
        QCOMPARE(loadedRit.typeId, QStringLiteral("rx.rit"));
        QCOMPARE(loadedFilter.config.value("order").toArray(), (QJsonArray{"bottom", "presets", "top"}));

        RadioModel radio;
        SliceModel slice(0);
        RxApplet applet(nullptr, &radio);
        applet.setSlice(&slice);
        ContainerContentRegistry registry;
        registerRxSectionContent(registry, &applet);
        QWidget host;
        auto* ritCopy = qobject_cast<RxRitSection*>(registry.createWidget(loadedRit, &host));
        QVERIFY(ritCopy);
        QVERIFY(ritCopy->zeroButton()->isHidden());
        QCOMPARE(lineIndex(ritCopy, ritCopy->plus()), 0);
        auto* filterCopy = qobject_cast<RxFilterSection*>(registry.createWidget(loadedFilter, &host));
        QVERIFY(filterCopy);
        QVERIFY(filterCopy->widthLabel()->isHidden());
        QCOMPARE(filterCopy->columns(), 2);
        QCOMPARE(columnIndex(filterCopy, filterCopy->passband()), 0);
    }

    // Task 35: the Thetis band groups (MeterManager.cs setupButtons), one
    // table for both band grids.
    void theBandGroupLayoutsAreThetis()
    {
        const QVector<BandGroupSlot>& hf = bandGroupLayout(BandGroup::Hf);
        QCOMPARE(hf.size(), kBandGroupButtons);
        const Band hfBands[] = {Band::Band160m, Band::Band80m, Band::Band60m, Band::Band40m,
                                Band::Band30m, Band::Band20m, Band::Band17m, Band::Band15m,
                                Band::Band12m, Band::Band10m, Band::Band6m};
        for (int i = 0; i < 11; ++i) {
            QCOMPARE(hf[i].kind, BandGroupSlot::Band);
            QCOMPARE(hf[i].band, hfBands[i]);
        }
        QCOMPARE(hf[11].kind, BandGroupSlot::Blank);
        QCOMPARE(hf[12].kind, BandGroupSlot::JumpVhf);
        QCOMPARE(hf[13].kind, BandGroupSlot::Band);
        QCOMPARE(hf[13].band, Band::WWV);
        QCOMPARE(hf[14].kind, BandGroupSlot::JumpGen);

        const QVector<BandGroupSlot>& gen = bandGroupLayout(BandGroup::Gen);
        QCOMPARE(gen.size(), kBandGroupButtons);
        QCOMPARE(gen[0].kind, BandGroupSlot::Band);
        QCOMPARE(gen[0].band, Band::GEN);
        for (int i = 1; i <= kSwlBandCount; ++i) {
            QCOMPARE(gen[i].kind, BandGroupSlot::Band);
            QCOMPARE(gen[i].band, static_cast<Band>(static_cast<int>(Band::SwlFirst) + i - 1));
        }
        QCOMPARE(gen[13].band, Band::Band11m);
        QCOMPARE(gen[14].kind, BandGroupSlot::JumpHf);

        const QVector<BandGroupSlot>& vhf = bandGroupLayout(BandGroup::Vhf);
        QCOMPARE(vhf.size(), kBandGroupButtons);
        QCOMPARE(vhf[0].band, Band::Band2m);
        QCOMPARE(vhf[1].band, Band::XVTR);
        for (int i = 2; i < 14; ++i) { QCOMPARE(vhf[i].kind, BandGroupSlot::Blank); }
        QCOMPARE(vhf[14].kind, BandGroupSlot::JumpHf);

        QCOMPARE(bandGroupForBand(Band::Band20m), BandGroup::Hf);
        QCOMPARE(bandGroupForBand(Band::WWV), BandGroup::Hf);
        QCOMPARE(bandGroupForBand(Band::Band31m), BandGroup::Gen);
        QCOMPARE(bandGroupForBand(Band::GEN), BandGroup::Gen);
        QCOMPARE(bandGroupForBand(Band::Band2m), BandGroup::Vhf);
        QCOMPARE(bandGroupForBand(Band::XVTR), BandGroup::Vhf);
        QCOMPARE(bandGroupPosition(BandGroup::Hf, Band::WWV), 13);
        QCOMPARE(bandGroupPosition(BandGroup::Hf, Band::Band2m), -1);

        // A new grid shows the whole HF group, its blank included.
        BandButtonItem grid;
        grid.setSliceBand(0, Band::Band20m);
        QCOMPARE(grid.buttonCount(), kBandGroupButtons);
        QCOMPARE(grid.button(11).text, QString());
        QVERIFY(grid.isButtonShown(11));
        QCOMPARE(grid.button(12).text, QStringLiteral("VHF"));
        QCOMPARE(grid.button(13).text, QStringLiteral("WWV"));
        QCOMPARE(grid.button(14).text, QStringLiteral("SWL"));
        QVERIFY(grid.button(5).on);
    }

    void aBandGridJumpsBetweenGroups()
    {
        BandButtonItem grid;
        grid.setSliceBand(0, Band::Band20m);
        // bandClicked is on BandButtonItem, which the GUI DLL does not
        // export on Windows; the string form finds it through the object.
        QSignalSpy bands(&grid, SIGNAL(bandClicked(int)));
        QSignalSpy stacks(&grid, SIGNAL(bandStackRequested(int)));
        QVERIFY(bands.isValid());

        grid.buttonClicked(14, Qt::LeftButton);  // SWL
        QCOMPARE(grid.shownGroup(), BandGroup::Gen);
        QCOMPARE(grid.group(0), BandGroup::Gen);
        QCOMPARE(grid.button(1).text, QStringLiteral("120m"));
        QCOMPARE(grid.button(14).text, QStringLiteral("HF"));
        QCOMPARE(bands.count(), 0);  // a jump chooses no band

        grid.buttonClicked(6, Qt::LeftButton);  // 31m
        QCOMPARE(bands.count(), 1);
        QCOMPARE(bands.last().at(0).toInt(), static_cast<int>(Band::Band31m));

        grid.buttonClicked(14, Qt::LeftButton);  // HF
        QCOMPARE(grid.shownGroup(), BandGroup::Hf);
        grid.buttonClicked(12, Qt::LeftButton);  // VHF
        QCOMPARE(grid.shownGroup(), BandGroup::Vhf);
        QCOMPARE(grid.button(0).text, QStringLiteral("2m"));
        QCOMPARE(grid.button(14).text, QStringLiteral("HF"));
        grid.buttonClicked(14, Qt::RightButton);  // a jump takes either button
        QCOMPARE(grid.shownGroup(), BandGroup::Hf);

        // WWV sets the band for either button; it has no band stack.
        grid.buttonClicked(13, Qt::RightButton);
        QCOMPARE(bands.count(), 2);
        QCOMPARE(bands.last().at(0).toInt(), static_cast<int>(Band::WWV));
        QCOMPARE(stacks.count(), 0);
        // A band's right-click asks for its stack.
        grid.buttonClicked(3, Qt::RightButton);
        QCOMPARE(stacks.count(), 1);
        QCOMPARE(stacks.last().at(0).toInt(), static_cast<int>(Band::Band40m));
        // The blank does nothing.
        grid.buttonClicked(11, Qt::LeftButton);
        QCOMPARE(bands.count(), 2);
        QCOMPARE(grid.shownGroup(), BandGroup::Hf);

        // A jumped-to group stays while the band stays in its own group,
        // and follows the band into another group.
        grid.buttonClicked(14, Qt::LeftButton);
        grid.setSliceBand(0, Band::Band40m);
        QCOMPARE(grid.shownGroup(), BandGroup::Gen);
        grid.setSliceBand(0, Band::Band2m);
        QCOMPARE(grid.shownGroup(), BandGroup::Vhf);
        QVERIFY(grid.button(0).on);
        grid.setSliceBand(0, Band::WWV);
        QCOMPARE(grid.shownGroup(), BandGroup::Hf);
        QVERIFY(!grid.button(13).on);  // WWV is never lit, as in Thetis
    }

    // Screenshot fix: a blank in a band group (Thetis's empty HF button
    // 11, the VHF group's unused places) is drawn but never a blank button
    // that takes clicks: it is unavailable and a click says why.
    void aBlankBandButtonIsDisabledWithAReason()
    {
        const QString reason = QString::fromLatin1(kBlankButtonReason);
        BandButtonItem grid;
        grid.setSliceBand(0, Band::Band20m);
        QVERIFY(grid.isButtonShown(11));
        QCOMPARE(grid.button(11).text, QString());
        QVERIFY(!grid.isButtonAvailable(11));
        QCOMPARE(grid.buttonUnavailableReason(11), reason);
        for (int i = 0; i < kBandGroupButtons; ++i) {
            if (i != 11) {
                QVERIFY2(grid.isButtonAvailable(i), qPrintable(QString::number(i)));
            }
        }
        // Making the whole grid available (a slice arrives) never wakes it,
        // and a grid-wide reason gives way to it on the blank.
        grid.setAllButtonsAvailable(false, QStringLiteral("No live slice is attached"));
        QCOMPARE(grid.buttonUnavailableReason(11), reason);
        QCOMPARE(grid.buttonUnavailableReason(3), QStringLiteral("No live slice is attached"));
        grid.setAllButtonsAvailable(true);
        QVERIFY(!grid.isButtonAvailable(11));
        QVERIFY(grid.isButtonAvailable(3));
        // A band's own reason still applies beside the blank.
        grid.setBandUnavailableReason(Band::Band6m, QStringLiteral("Not on this radio"));
        QVERIFY(!grid.isButtonAvailable(10));
        QCOMPARE(grid.buttonUnavailableReason(10), QStringLiteral("Not on this radio"));
        // The VHF group's blanks too; its bands and jump are not blanks.
        grid.buttonClicked(12, Qt::LeftButton);
        QCOMPARE(grid.shownGroup(), BandGroup::Vhf);
        QVERIFY(grid.isButtonAvailable(0));
        QVERIFY(grid.isButtonAvailable(14));
        for (int i = 2; i < 14; ++i) {
            QCOMPARE(grid.buttonUnavailableReason(i), reason);
        }
        // Back in HF, a band button is no placeholder.
        grid.buttonClicked(14, Qt::LeftButton);
        QCOMPARE(grid.button(3).placeholderReason, QString());

        // A click on a placeholder changes nothing and says why.
        ButtonBoxItem box;
        box.setButtonCount(1);
        box.setColumns(1);
        box.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        box.button(0).placeholderReason = reason;
        QSignalSpy clicked(&box, &ButtonBoxItem::buttonClicked);
        QSignalSpy unavailable(&box, &ButtonBoxItem::unavailableButtonClicked);
        // Inside the one button (it starts at the top, a row of the grid).
        const QPointF centre(100, 10);
        QMouseEvent press(QEvent::MouseButtonPress, centre, centre, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, centre, centre, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QVERIFY(box.handleMousePress(&press, 200, 100));
        QVERIFY(box.handleMouseRelease(&release, 200, 100));
        QCOMPARE(clicked.count(), 0);
        QCOMPARE(unavailable.count(), 1);
        QCOMPARE(unavailable.last().at(1).toString(), reason);
    }

    void eachSliceKeepsItsOwnBandGroup()
    {
        BandButtonItem grid;
        grid.setSliceBand(0, Band::Band20m);
        grid.buttonClicked(14, Qt::LeftButton);  // Slice A: GEN
        grid.setSliceBand(1, Band::Band80m);
        QCOMPARE(grid.shownGroup(), BandGroup::Hf);  // Slice B: HF
        QCOMPARE(grid.group(0), BandGroup::Gen);
        QCOMPARE(grid.group(1), BandGroup::Hf);
        grid.setSliceBand(0, Band::Band20m);
        QCOMPARE(grid.shownGroup(), BandGroup::Gen);
        QCOMPARE(grid.group(1), BandGroup::Hf);
        // With no slice, nothing is lit and the group stays.
        grid.clearSliceBand();
        QCOMPARE(grid.activeBand(), -1);
        QCOMPARE(grid.shownGroup(), BandGroup::Gen);
    }

    void theNereusBandGridShowsTheSameButtons()
    {
        for (BandGroup g : {BandGroup::Gen, BandGroup::Hf, BandGroup::Vhf}) {
            BandButtonItem classic;
            NereusBandButtonItem nereus;
            QCOMPARE(nereus.buttonCount(), classic.buttonCount());
            classic.setSliceBand(0, Band::Band20m);
            nereus.setSliceBand(0, Band::Band20m);
            classic.setGroup(0, g);
            nereus.setGroup(0, g);
            for (int i = 0; i < classic.buttonCount(); ++i) {
                QCOMPARE(nereus.button(i).text, classic.button(i).text);
                QCOMPARE(nereus.isButtonShown(i), classic.isButtonShown(i));
                QCOMPARE(nereus.button(i).on, classic.button(i).on);
            }
        }
        NereusBandButtonItem nereus;
        nereus.setSliceBand(0, Band::Band20m);
        nereus.buttonClicked(14, Qt::LeftButton);
        QCOMPARE(nereus.shownGroup(), BandGroup::Gen);
        QCOMPARE(nereus.button(1).text, QStringLiteral("120m"));
    }

    void savedBandGridVisibleBitsKeepTheirMeaning()
    {
        // Saved before the groups: GEN (slot 11) and WWV (slot 12) hidden.
        const uint32_t legacy = 0x7FFFu & ~(1u << 11) & ~(1u << 12);
        BandButtonItem grid;
        QVERIFY(grid.deserialize(QStringLiteral("BANDBTNS|0|0|1|1|-1|0|4|5|%1|0.005|0|3|1|0|0")
                                     .arg(legacy)));
        QCOMPARE(grid.shownGroup(), BandGroup::Hf);
        QVERIFY(grid.isButtonShown(5));    // 20m
        QVERIFY(!grid.isButtonShown(13));  // WWV stays hidden
        QVERIFY(grid.isButtonShown(12));   // the jumps came after: shown
        QVERIFY(grid.isButtonShown(14));
        QVERIFY(!grid.isButtonShown(11));  // a group with a hidden button has no blank
        grid.setGroup(grid.shownSlice(), BandGroup::Gen);
        QVERIFY(!grid.isButtonShown(0));   // GEN stays hidden
        QVERIFY(grid.isButtonShown(1));    // 120m came after: shown
        grid.setGroup(grid.shownSlice(), BandGroup::Vhf);
        QVERIFY(grid.isButtonShown(0));    // 2m (slot 14) stays shown

        // Saved again, the old record comes back as it was, and reloads the
        // same way.
        const QString saved = grid.serialize();
        QVERIFY(saved.startsWith(QStringLiteral("BANDBTNS|0|0|1|1|-1|0|4|5|%1|").arg(legacy)));
        QVERIFY(!saved.endsWith(QStringLiteral("|BG1")));
        BandButtonItem reloaded;
        QVERIFY(reloaded.deserialize(saved));
        QCOMPARE(reloaded.visibleBits(), legacy);
        QVERIFY(!reloaded.isButtonShown(13));
        QVERIFY(reloaded.isButtonShown(14));

        // Once its visibility is set, every bit has its meaning: the record
        // carries the marker.
        grid.setVisibleBits(legacy);
        QVERIFY(grid.serialize().endsWith(QStringLiteral("|BG1")));
        QVERIFY(!grid.isButtonShown(14));

        // With the marker, a hidden jump stays hidden.
        const int swlJump = BandButtonItem::visibleBitFor({Band::GEN, BandGroupSlot::JumpGen});
        BandButtonItem hiding;
        hiding.setVisibleBits(0xFFFFFFFFu & ~(1u << swlJump));
        BandButtonItem hidden;
        QVERIFY(hidden.deserialize(hiding.serialize()));
        QCOMPARE(hidden.shownGroup(), BandGroup::Hf);
        QVERIFY(!hidden.isButtonShown(14));
        QVERIFY(hidden.isButtonShown(12));

        // The Nereus grid's record, saved before the groups.
        NereusBandButtonItem nereus;
        QVERIFY(nereus.deserialize(
            QStringLiteral("NEREUSBAND|0|0|1|1|-1|0|4|5|%1|#ffffffff|#ff000000|#ff111111|#ff222222|#ff333333")
                .arg(legacy)));
        QVERIFY(!nereus.isButtonShown(13));
        QVERIFY(nereus.isButtonShown(14));
        QCOMPARE(nereus.buttonTextColor(), QColor(QStringLiteral("#ffffffff")));
        const QString nereusSaved = nereus.serialize();
        QVERIFY(!nereusSaved.endsWith(QStringLiteral("|BG1")));
        NereusBandButtonItem nereusReloaded;
        QVERIFY(nereusReloaded.deserialize(nereusSaved));
        QCOMPARE(nereusReloaded.visibleBits(), nereus.visibleBits());
        QCOMPARE(nereusReloaded.buttonTextColor(), nereus.buttonTextColor());
        QVERIFY(nereusReloaded.isButtonShown(14));
        nereusReloaded.setVisibleBits(0xFFFFFFFFu);
        const QString nereusMarked = nereusReloaded.serialize();
        QVERIFY(nereusMarked.endsWith(QStringLiteral("|BG1")));
        NereusBandButtonItem nereusMarkedReloaded;
        QVERIFY(nereusMarkedReloaded.deserialize(nereusMarked));
        QCOMPARE(nereusMarkedReloaded.serialize(), nereusMarked);
        QCOMPARE(nereusMarkedReloaded.selectedBottomColor(), nereusReloaded.selectedBottomColor());
    }
};

QTEST_MAIN(TestRxSectionContent)
#include "tst_rx_section_content.moc"
