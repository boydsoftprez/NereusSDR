// no-port-check: NereusSDR-original test file.
//
// A container can hold a copy of the Phone/CW panel. The copy hides each
// row, and the Phone and CW buttons, from its entry. The docked panel is
// unchanged.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - The Phone/CW panel copy's hide toggles, page choice and
//                 live wiring. Brian Sutton, AI-assisted via xAI Grok.
// =================================================================

#include <QtTest/QtTest>

#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>

#include "gui/HGauge.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/PhoneCwSectionContent.h"
#include "gui/applets/SectionParts.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

QPushButton* buttonNamed(QWidget* root, const QString& text)
{
    for (QPushButton* button : root->findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

HGauge* gaugeNamed(QWidget* root, const QString& name)
{
    for (HGauge* gauge : root->findChildren<HGauge*>()) {
        if (gauge->accessibleName() == name) {
            return gauge;
        }
    }
    return nullptr;
}

} // namespace

class TstPhoneCwSectionContent : public QObject {
    Q_OBJECT
private slots:
    void copyHidesRowsTheAppletKeeps()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        auto* copy = applet.createPanelCopy(&host, false);
        QVERIFY(copy);
        QVERIFY(copy != &applet);

        QJsonObject config;
        config.insert(SectionParts::hideKey(QStringLiteral("level")), true);
        config.insert(SectionParts::hideKey(QStringLiteral("pageButtons")), true);
        config.insert(QStringLiteral("page"), QStringLiteral("cw"));
        copy->applyContainerConfig(config);

        HGauge* copyLevel = gaugeNamed(copy, QStringLiteral("Microphone level gauge"));
        HGauge* appletLevel = gaugeNamed(&applet, QStringLiteral("Microphone level gauge"));
        QVERIFY(copyLevel);
        QVERIFY(appletLevel);
        QVERIFY(copyLevel->isHidden());
        QVERIFY(!appletLevel->isHidden());

        QPushButton* copyPhone = buttonNamed(copy, QStringLiteral("Phone"));
        QPushButton* appletPhone = buttonNamed(&applet, QStringLiteral("Phone"));
        QVERIFY(copyPhone);
        QVERIFY(appletPhone);
        QVERIFY(copyPhone->isHidden());
        QVERIFY(!appletPhone->isHidden());

        auto* delay = copy->findChild<QSlider*>(QStringLiteral("cwDelaySlider"));
        QVERIFY(delay);
        QVERIFY(!delay->isHidden());
        QCOMPARE(copy->findChild<QStackedWidget*>()->currentIndex(), 1);

        // MON is not built, so the copy keeps it hidden and offers no toggle.
        QPushButton* mon = buttonNamed(copy, QStringLiteral("MON"));
        QVERIFY(mon);
        QVERIFY(mon->isHidden());
    }

    void liveCopySetsTheModelAndPreviewDoesNot()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        auto* copy = applet.createPanelCopy(&host, false);
        auto* proc = copy->findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        QVERIFY(proc);
        QVERIFY(proc->isEnabled());
        proc->setChecked(true);
        QVERIFY(radio.transmitModel().cpdrOn());
        auto* appletProc = applet.findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        QVERIFY(appletProc->isChecked());

        auto* preview = applet.createPanelCopy(nullptr, true);
        QVERIFY(preview);
        QVERIFY(!preview->isEnabled());
        auto* previewProc = preview->findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        QVERIFY(previewProc);
        previewProc->setChecked(true);
        previewProc->setChecked(false);
        QVERIFY(radio.transmitModel().cpdrOn());
        delete preview;
    }

    void propertiesNameThePageAndEachHideToggle()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        ContainerContentRegistry registry;
        registerPhoneCwSectionContent(registry, &applet);
        const ContentDescriptor descriptor = [&] {
            for (const ContentDescriptor& item : registry.descriptors()) {
                if (item.typeId == QLatin1String("phcw.panel")) {
                    return item;
                }
            }
            return ContentDescriptor{};
        }();
        QCOMPARE(descriptor.title, QStringLiteral("Phone / CW sections"));
        QVERIFY(descriptor.group == ContentGroup::PanelSections);

        const QVector<ContentProperty> properties = registry.declaredProperties(QStringLiteral("phcw.panel"));
        QStringList labels;
        for (const ContentProperty& property : properties) {
            labels << property.label;
            QVERIFY(property.kind != ContentProperty::Kind::Order);
        }
        QCOMPARE(labels.first(), QStringLiteral("Page"));
        QVERIFY(labels.contains(QStringLiteral("Hide page buttons")));
        QVERIFY(labels.contains(QStringLiteral("Hide level gauge")));
        QVERIFY(labels.contains(QStringLiteral("Hide compression gauge")));
        QVERIFY(labels.contains(QStringLiteral("Hide mic profile")));
        QVERIFY(labels.contains(QStringLiteral("Hide mic source and gain")));
        QVERIFY(labels.contains(QStringLiteral("Hide PROC")));
        QVERIFY(labels.contains(QStringLiteral("Hide DEXP")));
        QVERIFY(labels.contains(QStringLiteral("Hide AM carrier")));
        QVERIFY(labels.contains(QStringLiteral("Hide CW delay")));
        QVERIFY(labels.contains(QStringLiteral("Hide CW speed")));
        QVERIFY(labels.contains(QStringLiteral("Hide CW sidetone")));
        QVERIFY(labels.contains(QStringLiteral("Hide CW pan")));
        QVERIFY(labels.contains(QStringLiteral("Hide break-in, Iambic and pitch")));
        QVERIFY(labels.contains(QStringLiteral("Hide KEY, TX and keyer lamps")));
        QVERIFY(labels.contains(QStringLiteral("Hide keyer status")));
        QVERIFY(!labels.contains(QStringLiteral("Hide MON")));
    }
};

QTEST_MAIN(TstPhoneCwSectionContent)
#include "tst_phone_cw_section_content.moc"
