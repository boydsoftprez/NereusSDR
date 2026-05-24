// Verify AppletVisibilityController state, persistence, and signal emission.
// no-port-check: NereusSDR-original — no Thetis source.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/applets/AppletVisibilityController.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstAppletVisibilityController : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()    { AppSettings::instance().clear(); }
    void cleanup()         { AppSettings::instance().clear(); }

    void default_visible_when_no_settings_key();
    void default_hidden_when_no_settings_key();
    void persisted_value_overrides_default();
    void setVisible_persists_and_emits();
    void setVisible_idempotent_no_emit_on_same_value();
    void registeredIds_returns_insertion_order();
    void displayName_round_trip();
};

void TstAppletVisibilityController::default_visible_when_no_settings_key()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"), true);
    QVERIFY(c.isVisible(QStringLiteral("Rx")));
}

void TstAppletVisibilityController::default_hidden_when_no_settings_key()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Cwx"), QStringLiteral("CW Keyer"), false);
    QVERIFY(!c.isVisible(QStringLiteral("Cwx")));
}

void TstAppletVisibilityController::persisted_value_overrides_default()
{
    AppSettings::instance().setValue(QStringLiteral("AppletRxVisible"),
                                     QStringLiteral("False"));
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"), true);
    QVERIFY(!c.isVisible(QStringLiteral("Rx")));
}

void TstAppletVisibilityController::setVisible_persists_and_emits()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Tx"), QStringLiteral("TX"), true);
    QSignalSpy spy(&c, &AppletVisibilityController::visibilityChanged);

    c.setVisible(QStringLiteral("Tx"), false);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("Tx"));
    QCOMPARE(spy.first().at(1).toBool(), false);
    QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletTxVisible"))
             .toString(), QStringLiteral("False"));

    AppletVisibilityController c2;
    c2.registerApplet(QStringLiteral("Tx"), QStringLiteral("TX"), true);
    QVERIFY(!c2.isVisible(QStringLiteral("Tx")));
}

void TstAppletVisibilityController::setVisible_idempotent_no_emit_on_same_value()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Vax"), QStringLiteral("VAX"), true);
    QSignalSpy spy(&c, &AppletVisibilityController::visibilityChanged);

    c.setVisible(QStringLiteral("Vax"), true);
    QCOMPARE(spy.count(), 0);

    c.setVisible(QStringLiteral("Vax"), false);
    QCOMPARE(spy.count(), 1);

    c.setVisible(QStringLiteral("Vax"), false);
    QCOMPARE(spy.count(), 1);
}

void TstAppletVisibilityController::registeredIds_returns_insertion_order()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"),         QStringLiteral("RX"),          true);
    c.registerApplet(QStringLiteral("Tx"),         QStringLiteral("TX"),          true);
    c.registerApplet(QStringLiteral("PhoneCw"),    QStringLiteral("Phone / CW"),  true);
    c.registerApplet(QStringLiteral("Vax"),        QStringLiteral("VAX"),         true);
    c.registerApplet(QStringLiteral("PureSignal"), QStringLiteral("PureSignal"),  true);

    QStringList expected{
        QStringLiteral("Rx"), QStringLiteral("Tx"), QStringLiteral("PhoneCw"),
        QStringLiteral("Vax"), QStringLiteral("PureSignal")
    };
    QCOMPARE(c.registeredIds(), expected);
}

void TstAppletVisibilityController::displayName_round_trip()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("PhoneCw"), QStringLiteral("Phone / CW"), true);
    QCOMPARE(c.displayName(QStringLiteral("PhoneCw")),
             QStringLiteral("Phone / CW"));
    QCOMPARE(c.displayName(QStringLiteral("Unknown")), QString{});
}

QTEST_MAIN(TstAppletVisibilityController)
#include "tst_applet_visibility_controller.moc"
