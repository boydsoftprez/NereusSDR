// =================================================================
// tests/tst_client_chain_applet.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test; no upstream logic is ported.
//
// The TCI clients box (ClientChainApplet) on a window connected to a Core:
// a refresh with the same Core client list leaves every row widget as it
// is (the 1 Hz rebuild made the column below the box jump), and a client
// connecting or leaving still changes the rows to match.
//
// No radio, Core or TCI app is contacted; FakeStationLink stands in for
// the Core and the client list is set on the window's StationTciModel.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-07  J.J. Boyd / KG4VCF  Written. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QLayout>
#include <QPointer>
#include <QPushButton>
#include <QWidget>

#include "fakes/FakeStationLink.h"
#include "gui/applets/ClientChainApplet.h"
#include "models/RadioModel.h"
#include "models/StationTciModel.h"

using namespace NereusSDR;
using NereusSDR::Test::FakeStationLink;

namespace {

StationTciClient client(const QString& id, const QString& name, const QString& address)
{
    StationTciClient c;
    c.id = id;
    c.name = name;
    c.address = address;
    c.subscriptions = {QStringLiteral("audio:0")};
    c.lastCommand = QStringLiteral("vfo:0,0,14074000;");
    return c;
}

QString rowText(const StationTciClient& c)
{
    return QStringLiteral("%1 (%2): %3; %4")
        .arg(c.name, c.address, c.subscriptions.join(QStringLiteral(", ")), c.lastCommand);
}

QWidget* corePanel(ClientChainApplet& applet)
{
    return applet.findChild<QWidget*>(QStringLiteral("tciCoreClients"));
}

QList<QWidget*> coreRowWidgets(ClientChainApplet& applet)
{
    QList<QWidget*> rows;
    QWidget* panel = corePanel(applet);
    if (!panel || !panel->layout()) {
        return rows;
    }
    for (int i = 0; i < panel->layout()->count(); ++i) {
        if (QWidget* w = panel->layout()->itemAt(i)->widget()) {
            rows.append(w);
        }
    }
    return rows;
}

QStringList clientRowTexts(ClientChainApplet& applet)
{
    QStringList texts;
    for (QWidget* row : coreRowWidgets(applet)) {
        if (row->findChild<QPushButton*>(QStringLiteral("disconnectCoreTciClient"))) {
            texts.append(row->findChild<QLabel*>()->text());
        }
    }
    return texts;
}

void refresh(ClientChainApplet& applet)
{
    QVERIFY(QMetaObject::invokeMethod(&applet, "refresh"));
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

} // namespace

class ClientChainAppletTest : public QObject {
    Q_OBJECT

private slots:
    void sameCoreClientsKeepEveryRow()
    {
        RadioModel window(RadioModel::Role::Remote);
        FakeStationLink link;
        window.attachStation(&link);
        const QList<StationTciClient> clients{
            client(QStringLiteral("1"), QStringLiteral("WSJT-X"), QStringLiteral("10.0.0.5:50001")),
            client(QStringLiteral("2"), QStringLiteral("JTDX"), QStringLiteral("10.0.0.6:50002"))};
        window.stationTciModel()->setClients(clients);

        ClientChainApplet applet(nullptr);
        applet.setStationModel(&window);
        QCoreApplication::processEvents();

        const QList<QWidget*> before = coreRowWidgets(applet);
        QCOMPARE(before.size(), 3);   // heading + two apps
        QList<QPointer<QWidget>> guards;
        for (QWidget* w : before) {
            guards.append(w);
        }
        const QSize hint = corePanel(applet)->sizeHint();

        refresh(applet);
        refresh(applet);
        // The same list arriving again from the Core changes nothing either.
        window.stationTciModel()->setClients(clients);
        emit window.stationTciModel()->clientsChanged();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QCOMPARE(coreRowWidgets(applet), before);
        for (const QPointer<QWidget>& w : guards) {
            QVERIFY(!w.isNull());
            QVERIFY(!w->isHidden());
        }
        QCOMPARE(corePanel(applet)->sizeHint(), hint);
        QCOMPARE(clientRowTexts(applet), QStringList({rowText(clients[0]), rowText(clients[1])}));
    }

    void changedCoreClientsChangeTheRows()
    {
        RadioModel window(RadioModel::Role::Remote);
        FakeStationLink link;
        window.attachStation(&link);
        const StationTciClient first =
            client(QStringLiteral("1"), QStringLiteral("WSJT-X"), QStringLiteral("10.0.0.5:50001"));
        const StationTciClient second =
            client(QStringLiteral("2"), QStringLiteral("JTDX"), QStringLiteral("10.0.0.6:50002"));
        window.stationTciModel()->setClients({first});

        ClientChainApplet applet(nullptr);
        applet.setStationModel(&window);
        QCOMPARE(clientRowTexts(applet), QStringList({rowText(first)}));

        // A second app connects.
        window.stationTciModel()->setClients({first, second});
        refresh(applet);
        QCOMPARE(coreRowWidgets(applet).size(), 3);
        QCOMPARE(clientRowTexts(applet), QStringList({rowText(first), rowText(second)}));

        // The first leaves.
        window.stationTciModel()->setClients({second});
        refresh(applet);
        QCOMPARE(coreRowWidgets(applet).size(), 2);
        QCOMPARE(clientRowTexts(applet), QStringList({rowText(second)}));

        // An app's details change: its row says so.
        StationTciClient moved = second;
        moved.lastCommand = QStringLiteral("trx:0,true;");
        window.stationTciModel()->setClients({moved});
        refresh(applet);
        QCOMPARE(clientRowTexts(applet), QStringList({rowText(moved)}));

        // All gone.
        window.stationTciModel()->setClients({});
        refresh(applet);
        const QList<QWidget*> rows = coreRowWidgets(applet);
        QCOMPARE(rows.size(), 2);
        auto* none = qobject_cast<QLabel*>(rows.last());
        QVERIFY(none);
        QCOMPARE(none->text(), QStringLiteral("No apps connected to the Core's TCI server."));
    }
};

QTEST_MAIN(ClientChainAppletTest)
#include "tst_client_chain_applet.moc"
