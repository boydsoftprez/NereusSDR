// no-port-check: NereusSDR-original. The Thetis controls (groupBoxTS32 on
// tpOptions2, setup.designer.cs; the chkToTMox / chkToTPing handlers in
// setup.cs) are cited beside the port in GeneralOptionsPage.cpp; this test
// translates no C#.
//
// iPhone app plan Task 38 (R-IOS-04, D29): Setup > General > Options gains
// Thetis's Time Out Timers group and the time-out for phones and tablets.
//
//   1. The group shows Thetis's labels, tooltips, ranges and defaults (MOX
//      off at 180 s, Ping off at 180 s to 8.8.8.8), and the phone and iPad
//      row on at 180 s.
//   2. Each seconds box follows its checkbox's enabled state; so do the
//      host and its Def button.
//   3. Changing a box writes its key: MoxTimeOut*, PingTimeOut*,
//      RemoteMoxTimeOut*; all seven are Station scope (the Core's).
//   4. A host that does not parse turns red and saves nothing; Def puts
//      8.8.8.8 back and saves it.
//   5. Saved values are shown when the page opens, and opening writes
//      nothing.
//   6. Without the Core's settings the whole group is disabled with the
//      reason, and comes back as it was.
//   7. (Merge of Tasks 38 and 39) A Core older than the time-out, which
//      stores these settings and ignores them, shows the group disabled
//      with an "update the Core" reason; a Core with it leaves it usable.
//   8. (Merge of Tasks 38 and 39) In the dark theme a disabled control
//      looks disabled: a disabled seconds box is drawn differently from an
//      enabled one showing the same value.

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "core/session/IStationLink.h"
#include "core/settings/SettingsScope.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

QString setting(const QString& key)
{
    return AppSettings::instance().value(key).toString();
}

template <typename T>
T* child(QWidget& page, const char* name)
{
    T* widget = page.findChild<T*>(QString::fromLatin1(name));
    if (widget == nullptr) {
        qFatal("%s not found", name);
    }
    return widget;
}

// A remote window's link to a Core, with or without the transmit time-out.
class TimeOutLink final : public IStationLink {
public:
    bool hasTimeOut{false};
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool stationLinkReady() const override { return true; }
    bool transmitTimeOutAvailable() const override { return hasTimeOut; }
};

// The mean lightness of a widget as drawn.
double meanLightness(QWidget& widget)
{
    const QImage image = widget.grab().toImage();
    double sum = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            sum += QColor(image.pixel(x, y)).lightnessF();
        }
    }
    return sum / std::max(1, image.width() * image.height());
}

} // namespace

class TestGeneralOptionsTimeOut : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void theGroupHasThetisLabelsRangesAndDefaults()
    {
        GeneralOptionsPage page(/*model=*/nullptr);
        auto* group = child<QGroupBox>(page, "grpTimeOutTimers");
        QCOMPARE(group->title(), QStringLiteral("Time Out Timers"));

        auto* mox = child<QCheckBox>(page, "chkToTMox");
        QCOMPARE(mox->text(), QStringLiteral("MOX"));
        QCOMPARE(mox->toolTip(), QStringLiteral("Time out Mox after X seconds"));
        QVERIFY(!mox->isChecked());
        auto* moxSeconds = child<QSpinBox>(page, "udMoxToTSeconds");
        QCOMPARE(moxSeconds->minimum(), 30);
        QCOMPARE(moxSeconds->maximum(), 1800);
        QCOMPARE(moxSeconds->value(), 180);
        QCOMPARE(moxSeconds->toolTip(),
                 QStringLiteral("Stop mox if it is enabled for this duration"));
        QCOMPARE(child<QLabel>(page, "lblMoxTotSec")->text(), QStringLiteral("secs"));

        auto* ping = child<QCheckBox>(page, "chkToTPing");
        QCOMPARE(ping->text(), QStringLiteral("Ping"));
        QVERIFY(ping->toolTip().startsWith(QStringLiteral("If ping fails for X seconds, then stop mox.")));
        QVERIFY(!ping->isChecked());
        auto* pingSeconds = child<QSpinBox>(page, "udPingToTSeconds");
        QCOMPARE(pingSeconds->minimum(), 30);
        QCOMPARE(pingSeconds->maximum(), 1800);
        QCOMPARE(pingSeconds->value(), 180);
        QCOMPARE(pingSeconds->toolTip(), QStringLiteral("If unable to ping for this long, stop mox"));
        QCOMPARE(child<QLabel>(page, "lblPingTotSec")->text(), QStringLiteral("secs"));
        auto* host = child<QLineEdit>(page, "txtToTPingIP");
        QCOMPARE(host->text(), QStringLiteral("8.8.8.8"));
        QCOMPARE(host->toolTip(), QStringLiteral("Try to ping this IP"));
        auto* def = child<QPushButton>(page, "btnPingDef");
        QCOMPARE(def->text(), QStringLiteral("Def"));
        QCOMPARE(def->toolTip(), QStringLiteral("Default value of 8.8.8.8 (Google DNS)"));

        auto* remote = child<QCheckBox>(page, "chkRemoteMoxTimeOut");
        QCOMPARE(remote->text(), QStringLiteral("Phone and iPad"));
        QVERIFY(remote->isChecked());
        auto* remoteSeconds = child<QSpinBox>(page, "udRemoteMoxTimeOutSeconds");
        QCOMPARE(remoteSeconds->minimum(), 30);
        QCOMPARE(remoteSeconds->maximum(), 1800);
        QCOMPARE(remoteSeconds->value(), 180);

        // Thetis's "if (initializing) return;": opening the page saves nothing.
        for (const char* key : {"MoxTimeOutEnabled", "MoxTimeOutSeconds", "PingTimeOutEnabled",
                                "PingTimeOutSeconds", "PingTimeOutHost",
                                "RemoteMoxTimeOutEnabled", "RemoteMoxTimeOutSeconds"}) {
            QVERIFY2(!AppSettings::instance().contains(QString::fromLatin1(key)), key);
        }
    }

    void secondsFollowTheirCheckboxes()
    {
        GeneralOptionsPage page(nullptr);
        auto* mox = child<QCheckBox>(page, "chkToTMox");
        auto* moxSeconds = child<QSpinBox>(page, "udMoxToTSeconds");
        auto* moxSecs = child<QLabel>(page, "lblMoxTotSec");
        QVERIFY(!moxSeconds->isEnabled());
        QVERIFY(!moxSecs->isEnabled());
        mox->setChecked(true);
        QVERIFY(moxSeconds->isEnabled());
        QVERIFY(moxSecs->isEnabled());

        auto* ping = child<QCheckBox>(page, "chkToTPing");
        auto* pingSeconds = child<QSpinBox>(page, "udPingToTSeconds");
        auto* host = child<QLineEdit>(page, "txtToTPingIP");
        auto* def = child<QPushButton>(page, "btnPingDef");
        QVERIFY(!pingSeconds->isEnabled() && !host->isEnabled() && !def->isEnabled());
        ping->setChecked(true);
        QVERIFY(pingSeconds->isEnabled() && host->isEnabled() && def->isEnabled());

        auto* remote = child<QCheckBox>(page, "chkRemoteMoxTimeOut");
        auto* remoteSeconds = child<QSpinBox>(page, "udRemoteMoxTimeOutSeconds");
        QVERIFY(remoteSeconds->isEnabled());
        remote->setChecked(false);
        QVERIFY(!remoteSeconds->isEnabled());
    }

    void changesWriteTheStationKeys()
    {
        GeneralOptionsPage page(nullptr);
        child<QCheckBox>(page, "chkToTMox")->setChecked(true);
        QCOMPARE(setting(QStringLiteral("MoxTimeOutEnabled")), QStringLiteral("True"));
        QCOMPARE(setting(QStringLiteral("MoxTimeOutSeconds")), QStringLiteral("180"));
        child<QSpinBox>(page, "udMoxToTSeconds")->setValue(600);
        QCOMPARE(setting(QStringLiteral("MoxTimeOutSeconds")), QStringLiteral("600"));

        child<QCheckBox>(page, "chkToTPing")->setChecked(true);
        QCOMPARE(setting(QStringLiteral("PingTimeOutEnabled")), QStringLiteral("True"));
        QCOMPARE(setting(QStringLiteral("PingTimeOutHost")), QStringLiteral("8.8.8.8"));
        child<QSpinBox>(page, "udPingToTSeconds")->setValue(45);
        QCOMPARE(setting(QStringLiteral("PingTimeOutSeconds")), QStringLiteral("45"));
        child<QLineEdit>(page, "txtToTPingIP")->setText(QStringLiteral("192.168.1.1"));
        QCOMPARE(setting(QStringLiteral("PingTimeOutHost")), QStringLiteral("192.168.1.1"));

        child<QSpinBox>(page, "udRemoteMoxTimeOutSeconds")->setValue(30);
        QCOMPARE(setting(QStringLiteral("RemoteMoxTimeOutSeconds")), QStringLiteral("30"));
        QCOMPARE(setting(QStringLiteral("RemoteMoxTimeOutEnabled")), QStringLiteral("True"));
        child<QCheckBox>(page, "chkRemoteMoxTimeOut")->setChecked(false);
        QCOMPARE(setting(QStringLiteral("RemoteMoxTimeOutEnabled")), QStringLiteral("False"));

        // Every one of them is the Core's.
        for (const char* key : {"MoxTimeOutEnabled", "MoxTimeOutSeconds", "PingTimeOutEnabled",
                                "PingTimeOutSeconds", "PingTimeOutHost",
                                "RemoteMoxTimeOutEnabled", "RemoteMoxTimeOutSeconds"}) {
            QVERIFY2(classifySettingsKey(QString::fromLatin1(key)) == SettingsScope::Station, key);
        }
    }

    void aHostThatDoesNotParseSavesNothingAndDefRestoresIt()
    {
        GeneralOptionsPage page(nullptr);
        child<QCheckBox>(page, "chkToTPing")->setChecked(true);
        auto* host = child<QLineEdit>(page, "txtToTPingIP");
        auto* seconds = child<QSpinBox>(page, "udPingToTSeconds");

        host->setText(QStringLiteral("8.8.8.x"));
        QVERIFY(host->styleSheet().contains(QStringLiteral("red")));
        QCOMPARE(setting(QStringLiteral("PingTimeOutHost")), QStringLiteral("8.8.8.8"));
        seconds->setValue(90);
        // As in Thetis, nothing reaches the time-out while the host is bad.
        QCOMPARE(setting(QStringLiteral("PingTimeOutSeconds")), QStringLiteral("180"));

        child<QPushButton>(page, "btnPingDef")->click();
        QCOMPARE(host->text(), QStringLiteral("8.8.8.8"));
        QVERIFY(host->styleSheet().isEmpty());
        QCOMPARE(setting(QStringLiteral("PingTimeOutHost")), QStringLiteral("8.8.8.8"));
        QCOMPARE(setting(QStringLiteral("PingTimeOutSeconds")), QStringLiteral("90"));
    }

    void savedValuesAreShown()
    {
        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("MoxTimeOutEnabled"), QStringLiteral("True"));
        s.setValue(QStringLiteral("MoxTimeOutSeconds"), 300);
        s.setValue(QStringLiteral("PingTimeOutEnabled"), QStringLiteral("True"));
        s.setValue(QStringLiteral("PingTimeOutSeconds"), 60);
        s.setValue(QStringLiteral("PingTimeOutHost"), QStringLiteral("10.0.0.1"));
        s.setValue(QStringLiteral("RemoteMoxTimeOutEnabled"), QStringLiteral("False"));
        s.setValue(QStringLiteral("RemoteMoxTimeOutSeconds"), 900);

        GeneralOptionsPage page(nullptr);
        QVERIFY(child<QCheckBox>(page, "chkToTMox")->isChecked());
        QCOMPARE(child<QSpinBox>(page, "udMoxToTSeconds")->value(), 300);
        QVERIFY(child<QSpinBox>(page, "udMoxToTSeconds")->isEnabled());
        QVERIFY(child<QCheckBox>(page, "chkToTPing")->isChecked());
        QCOMPARE(child<QSpinBox>(page, "udPingToTSeconds")->value(), 60);
        QCOMPARE(child<QLineEdit>(page, "txtToTPingIP")->text(), QStringLiteral("10.0.0.1"));
        QVERIFY(!child<QCheckBox>(page, "chkRemoteMoxTimeOut")->isChecked());
        QCOMPARE(child<QSpinBox>(page, "udRemoteMoxTimeOutSeconds")->value(), 900);
        QVERIFY(!child<QSpinBox>(page, "udRemoteMoxTimeOutSeconds")->isEnabled());
    }

    void withoutTheCoresSettingsTheGroupIsDisabled()
    {
        GeneralOptionsPage page(nullptr);
        auto* group = child<QGroupBox>(page, "grpTimeOutTimers");
        auto* remoteSeconds = child<QSpinBox>(page, "udRemoteMoxTimeOutSeconds");
        auto* moxSeconds = child<QSpinBox>(page, "udMoxToTSeconds");
        const QString reason = QStringLiteral("Connect to the Core to change this.");

        page.setStationSettingsAvailable(false, reason);
        QVERIFY(!group->isEnabled());
        QCOMPARE(group->toolTip(), reason);
        QVERIFY(!remoteSeconds->isEnabled());

        page.setStationSettingsAvailable(true, QString());
        QVERIFY(group->isEnabled());
        QVERIFY(remoteSeconds->isEnabled());
        QVERIFY(!moxSeconds->isEnabled());  // still follows its checkbox
    }
    void anOlderCoreShowsTheGroupDisabledWithWhy()
    {
        RadioModel model(RadioModel::Role::Remote);
        TimeOutLink link;
        model.attachStation(&link);
        {
            GeneralOptionsPage page(&model);
            auto* group = child<QGroupBox>(page, "grpTimeOutTimers");
            auto* region = child<QComboBox>(page, "comboFRSRegion");

            link.hasTimeOut = false;
            page.setStationSettingsAvailable(true, QString());
            QVERIFY(!group->isEnabled());
            QVERIFY(!child<QCheckBox>(page, "chkRemoteMoxTimeOut")->isEnabled());
            QCOMPARE(group->toolTip(), GeneralOptionsPage::timeOutNeedsNewerCoreText());
            QVERIFY(GeneralOptionsPage::timeOutNeedsNewerCoreText().contains(
                QStringLiteral("Update the Core")));
            QVERIFY(!region->isEnabled());  // TX region mapping still awaits the Core policy

            // Disconnected: the page's own reason, not the older Core's.
            const QString reason = QStringLiteral("Connect to the Core to change this.");
            page.setStationSettingsAvailable(false, reason);
            QVERIFY(!group->isEnabled());
            QCOMPARE(group->toolTip(), reason);

            // A Core with the time-out: usable.
            link.hasTimeOut = true;
            page.setStationSettingsAvailable(true, QString());
            QVERIFY(group->isEnabled());
            QVERIFY(child<QCheckBox>(page, "chkRemoteMoxTimeOut")->isEnabled());
        }
        model.detachStation();
    }

    void aDisabledControlLooksDisabledInTheDarkTheme()
    {
        GeneralOptionsPage page(nullptr);
        page.resize(640, 900);
        // Both show 180; the MOX box is disabled (its checkbox is off), the
        // phone-and-tablet box enabled (its checkbox is on by default).
        auto* disabled = child<QSpinBox>(page, "udMoxToTSeconds");
        auto* enabled = child<QSpinBox>(page, "udRemoteMoxTimeOutSeconds");
        QVERIFY(!disabled->isEnabled());
        QVERIFY(enabled->isEnabled());
        QCOMPARE(disabled->value(), enabled->value());
        disabled->resize(enabled->size());
        const double off = meanLightness(*disabled);
        const double on = meanLightness(*enabled);
        QVERIFY2(off < on - 0.005, qPrintable(QStringLiteral("disabled %1, enabled %2")
                                                  .arg(off).arg(on)));
    }
};

QTEST_MAIN(TestGeneralOptionsTimeOut)
#include "tst_general_options_time_out.moc"
