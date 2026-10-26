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
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
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
#include "gui/widgets/FilterPassbandWidget.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/styles/WorkspacePreview.h"
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
        QCOMPARE(properties.size(), 8);
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
        QCOMPARE(properties.size(), 8);
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
};

QTEST_MAIN(TestRxSectionContent)
#include "tst_rx_section_content.moc"
