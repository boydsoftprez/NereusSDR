// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_roundtable_spothub.cpp  (Longpath)
// =================================================================
//
// Der Roundtable-Reiter im Spot Hub: er steht vor der Spot-Liste, fragt
// vor der Antwort mit "Go live" / "Stay hidden", schreibt die Antwort in
// Roundtable/*, und nach der Antwort wird daraus ein Umschalter. Gefahren
// wird die Bedienung (click), nicht die Methoden dahinter.
//
// Die Verbindung zeigt auf einen Server auf diesem Rechner, den es nicht
// gibt; im Pruefmodus spricht der Client ohnehin keinen fremden an.
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableClient.h"
#include "gui/SpotHubDialog.h"
#include "models/RadioModel.h"
#include "models/RoundtableHub.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

using namespace Longpath;

class TstRoundtableSpotHub : public QObject {
    Q_OBJECT

    // Fuer den Betreiber: mit LONGPATH_BILD_DIR legt der Lauf ein Bild des
    // Reiters ab, damit die Gestaltung ohne laufendes Programm zu sehen ist.
    static void bildAblegen(QWidget& w, const QString& name)
    {
        const QString dir = QString::fromLocal8Bit(qgetenv("LONGPATH_BILD_DIR"));
        if (dir.isEmpty()) {
            return;
        }
        w.resize(760, 520);
        QCoreApplication::processEvents();
        w.grab().save(dir + QLatin1Char('/') + name);
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qunsetenv("LONGPATH_CONFIG_DIR");
    }

    void init()
    {
        auto& s = AppSettings::instance();
        for (const char* k : {"Roundtable/AutoStart", "Roundtable/FirstRunAnswer",
                              "Roundtable/Hidden", "Roundtable/Callsign", "Roundtable/GridSquare"}) {
            s.remove(QLatin1String(k));
        }
        s.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("OE5SOS"));
        s.setValue(QStringLiteral("Roundtable/GridSquare"), QStringLiteral("JN78"));
        s.setValue(QStringLiteral("Roundtable/ServerUrl"),
                   QStringLiteral("ws://127.0.0.1:9/v1/station"));
    }

    void erstfrageDannUmschalter()
    {
        QTemporaryDir dir;
        RadioModel model;
        RoundtableHub* hub = model.roundtableHub();
        QVERIFY(hub);
        hub->setKeyPathForTest(dir.filePath(QStringLiteral("roundtable-identity.pem")));
        hub->reloadSettings();

        SpotHubDialog dlg(model.dxCluster(), model.rbn(), model.wsjtx(),
                          model.spotCollector(), model.pota(), model.sota(),
                          model.freeDvReporter(), model.pskReporter(),
                          model.spotModel(), model.spotTableModel(),
                          model.dxccColorProvider(), nullptr);
        dlg.attachRoundtable(hub);
        dlg.show();

        // Der Reiter steht vor der Spot-Liste.
        auto* tabs = dlg.findChild<QTabWidget*>();
        QVERIFY(tabs);
        int rt = -1;
        int list = -1;
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == QLatin1String("Roundtable")) {
                rt = i;
            } else if (tabs->tabText(i) == QLatin1String("Spot List")) {
                list = i;
            }
        }
        QVERIFY(rt >= 0);
        QCOMPARE(rt + 1, list);
        tabs->setCurrentIndex(rt);

        auto* goLive = dlg.findChild<QPushButton*>(QStringLiteral("roundtableGoLiveBtn"));
        auto* stay = dlg.findChild<QPushButton*>(QStringLiteral("roundtableStayHiddenBtn"));
        auto* toggle = dlg.findChild<QPushButton*>(QStringLiteral("roundtableHiddenToggleBtn"));
        auto* status = dlg.findChild<QLabel*>(QStringLiteral("roundtableStatusLabel"));
        auto* call = dlg.findChild<QLabel*>(QStringLiteral("roundtableCallLabel"));
        QVERIFY(goLive && stay && toggle && status && call);

        // Vor der Antwort: die Frage, keine Verbindung.
        QVERIFY(goLive->isVisible());
        QVERIFY(stay->isVisible());
        QVERIFY(!toggle->isVisible());
        QVERIFY(status->text().contains(QStringLiteral("nothing is sent")));
        QVERIFY(call->text().contains(QStringLiteral("OE5SOS")));
        QCOMPARE(hub->client()->linkState(), RoundtableClient::LinkState::Off);
        bildAblegen(dlg, QStringLiteral("roundtable-reiter-vor-der-antwort.png"));

        // "Stay hidden": die Antwort steht in den Einstellungen.
        QTest::mouseClick(stay, Qt::LeftButton);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Roundtable/FirstRunAnswer")).toString(),
                 QStringLiteral("Hidden"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Roundtable/Hidden")).toString(),
                 QStringLiteral("True"));
        QVERIFY(!goLive->isVisible());
        QVERIFY(!stay->isVisible());
        QVERIFY(toggle->isVisible());
        QCOMPARE(toggle->text(), QStringLiteral("Go live"));
        bildAblegen(dlg, QStringLiteral("roundtable-reiter-verborgen.png"));

        // Umschalten auf live und zurueck.
        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Roundtable/Hidden")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(toggle->text(), QStringLiteral("Go hidden"));
        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Roundtable/Hidden")).toString(),
                 QStringLiteral("True"));

        // Aus: der Client haelt an.
        auto* autoStart = dlg.findChild<QPushButton*>(QStringLiteral("roundtableAutoStartBtn"));
        QVERIFY(autoStart && autoStart->isChecked());
        QTest::mouseClick(autoStart, Qt::LeftButton);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Roundtable/AutoStart")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(hub->client()->linkState(), RoundtableClient::LinkState::Off);
        QCOMPARE(status->text(), QStringLiteral("Off"));
    }

    void quelleLaesstSichAbschalten()
    {
        RadioModel model;
        SpotHubDialog dlg(model.dxCluster(), model.rbn(), model.wsjtx(),
                          model.spotCollector(), model.pota(), model.sota(),
                          model.freeDvReporter(), model.pskReporter(),
                          model.spotModel(), model.spotTableModel(),
                          model.dxccColorProvider(), nullptr);
        // Wie die anderen Quellen: ein Schalter im Display-Reiter und
        // eine Pille in der Spot-Liste.
        auto* show = dlg.findChild<QCheckBox*>(QStringLiteral("displaySourceShow_Roundtable"));
        auto* pill = dlg.findChild<QPushButton*>(QStringLiteral("spotListSourcePill_RT"));
        QVERIFY(show);
        QVERIFY(pill);
        QVERIFY(show->isChecked());
        show->setChecked(false);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("SpotSourceVisible/Roundtable"))
                     .toString(),
                 QStringLiteral("False"));
        show->setChecked(true);
    }
};

QTEST_MAIN(TstRoundtableSpotHub)
#include "tst_roundtable_spothub.moc"
