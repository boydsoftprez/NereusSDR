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
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QPushButton>

#include "gui/applets/RxApplet.h"
#include "gui/applets/RxControlSections.h"
#include "gui/applets/RxSectionContent.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/styles/WorkspacePreview.h"
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

QPushButton* appletSql(RxApplet& applet)
{
    for (QPushButton* button : applet.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("SQL")) { return button; }
    }
    return nullptr;
}

} // namespace

class TestRxSectionContent : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
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
};

QTEST_MAIN(TestRxSectionContent)
#include "tst_rx_section_content.moc"
