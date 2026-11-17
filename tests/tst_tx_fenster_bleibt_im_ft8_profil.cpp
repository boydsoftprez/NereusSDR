// =================================================================
// tests/tst_tx_fenster_bleibt_im_ft8_profil.cpp  (Longpath)
// =================================================================
//
// Betreiber, 2026-11-17:
//
//   „Das TX Fenster verschwindet immer bei FT8 nach neustart, aber auch
//    wenn ich ein Profil kurz aendere und danach wieder auf FT8
//    zurueckkehre."
//
// Gemessen am selben Tag in seiner Einstellungsdatei: das Profil „FT8"
// fuehrt TX ZWEIMAL --
//
//     floatingApplets: { Tx: x 0, y 592, 397x253 }   (eigenes Fenster)
//     kombiInhalt:     [ RotorLog, Tx, Rx ]          (im Kombi-Feld)
//
// -- und das Kombi-Feld schwebt in FT8 nicht einmal. Beim Anwenden
// baute Schritt 3 das TX-Fenster, und am Ende zog die Kombi-Fuellung TX
// wieder heraus und in ein Feld, das dort niemand sieht. Im Protokoll
// (longpath-20261117-063802.log) bei jedem Wechsel nach FT8:
//
//     09:03:50.343 Kombi:  3 von 3 gemerkten Panels zurueckgeholt
//     09:04:24.370 bringAppletToFront("Tx"): "detached" ... hidden=true
//
// Der Prüfstand nimmt GENAU diesen Profilsatz (tests/data, unveraendert
// bis auf `current`, das beim Start ohnehin Profil 1 ist) und faehrt
// seinen Weg: Start in „Neu", nach FT8, zurueck, wieder nach FT8.
//
// Zwei Dinge muessen dabei stimmen, und das zweite ist so wichtig wie
// das erste: in FT8 bleibt TX ein eigenes Fenster -- und in „Neu", wo
// das Profil TX wirklich IM Feld haben will, liegt es weiter im Feld.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "core/AppSettings.h"
#include "gui/LayoutProfiles.h"
#include "gui/MainWindow.h"
#include "gui/applets/AppletKeys.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstTxFensterBleibtImFt8Profil : public QObject {
    Q_OBJECT

private:
    MainWindow*     m_mw{nullptr};
    LayoutProfiles* m_profile{nullptr};

    // Bis sich alles gesetzt hat: Raster (180 ms), Profilrahmen-
    // Nachlauf (bis 3000 ms), Kombi-Fuellung.
    static void warten() { QTest::qWait(3500); }

    QVariantMap aufnahme(const QString& name)
    {
        m_profile->captureIntoCurrent();
        return m_profile->snapshot(name);
    }

    static QString text(const QVariantMap& s)
    {
        return QStringLiteral("schwebend=[%1] kombiInhalt=[%2]")
            .arg(s.value(QStringLiteral("floatingApplets")).toMap().keys()
                     .join(QLatin1Char(',')),
                 s.value(QStringLiteral("kombiInhalt")).toStringList()
                     .join(QLatin1Char(',')));
    }

private slots:
    // ── Die Regel selbst, an seinen gemessenen Listen ──────────────
    void einPanelLiegtAnEinemOrt()
    {
        // Genau die zwei Listen aus seinem FT8-Profil.
        const QStringList gewuenscht{QStringLiteral("RotorLog"),
                                     QStringLiteral("Tx"), QStringLiteral("Rx")};
        const QStringList schwebend{QStringLiteral("Ft8Decoder"),
                                    QStringLiteral("Ft8Slots"), QStringLiteral("Tx")};
        QCOMPARE(AppletKeys::kombiInhaltOhneEigeneFenster(gewuenscht, schwebend),
                 (QStringList{QStringLiteral("RotorLog"), QStringLiteral("Rx")}));

        // Sein Profil „Neu": TX schwebt dort NICHT -- also bleibt es im
        // Feld. Die Regel nimmt nichts weg, was nicht doppelt steht.
        const QStringList neuSchwebend{
            QStringLiteral("Asr"), QStringLiteral("BwFilter"), QStringLiteral("Kombi"),
            QStringLiteral("SignalInstrument"), QStringLiteral("SwrInstrument")};
        QCOMPARE(AppletKeys::kombiInhaltOhneEigeneFenster(gewuenscht, neuSchwebend),
                 gewuenscht);

        // Leer bleibt leer, Doppelte und leere Eintraege fallen weg, die
        // Reihenfolge bleibt.
        QVERIFY(AppletKeys::kombiInhaltOhneEigeneFenster({}, schwebend).isEmpty());
        QCOMPARE(AppletKeys::kombiInhaltOhneEigeneFenster(
                     {QStringLiteral("Rx"), QString(), QStringLiteral("Rx"),
                      QStringLiteral("Eq")}, {}),
                 (QStringList{QStringLiteral("Rx"), QStringLiteral("Eq")}));
    }

    void initTestCase()
    {
        QFile f(QStringLiteral(LONGPATH_TEST_DATA_DIR)
                + QStringLiteral("/layoutprofile_ft8_tx_doppelt_2026-11-17.json"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "Profilsatz fehlt in tests/data");
        const QByteArray json = f.readAll();
        const QJsonObject wurzel = QJsonDocument::fromJson(json).object();
        QVERIFY(!wurzel.isEmpty());

        // ── Voraussetzung mitpruefen ─────────────────────────────────
        // Die Vorlage MUSS den Widerspruch tragen, sonst prueft dieser
        // Lauf nichts und ist trotzdem gruen.
        bool widerspruch = false;
        for (const QJsonValue& v : wurzel.value(QStringLiteral("profiles")).toArray()) {
            const QJsonObject p = v.toObject();
            if (p.value(QStringLiteral("name")).toString() != QStringLiteral("FT8")) {
                continue;
            }
            const QJsonObject st = p.value(QStringLiteral("state")).toObject();
            const bool schwebt = st.value(QStringLiteral("floatingApplets"))
                                     .toObject().contains(QStringLiteral("Tx"));
            bool imFeld = false;
            for (const QJsonValue& k : st.value(QStringLiteral("kombiInhalt")).toArray()) {
                if (k.toString() == QStringLiteral("Tx")) { imFeld = true; }
            }
            widerspruch = schwebt && imFeld;
        }
        QVERIFY2(widerspruch,
                 "Voraussetzung: die Vorlage fuehrt TX im FT8-Profil doppelt "
                 "(Fenster UND Kombi-Feld)");

        AppSettings::instance().setValue(LayoutProfiles::settingsKey(),
                                         QString::fromUtf8(json));
        // Die globale Liste, wie sie in seiner Datei stand -- der Weg,
        // ueber den der Widerspruch ins Profil kam.
        AppSettings::instance().setValue(QStringLiteral("Kombi/Inhalt"),
                                         QStringLiteral("RotorLog,Tx,Rx"));

        // Das Contestprogramm fragt im leeren Pruefordner nach seinen
        // Einstellungen (modaler Dialog) -- jeden gleich wieder zumachen.
        auto* zu = new QTimer(this);
        connect(zu, &QTimer::timeout, this, []() {
            if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                d->reject();
            }
        });
        zu->start(100);

        m_mw = new MainWindow();
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        warten();

        m_profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(m_profile);
        QCOMPARE(m_profile->current(), QStringLiteral("Neu"));
        QVERIFY(m_profile->exists(QStringLiteral("FT8")));
    }

    // 1. Sein erster Griff nach dem Start: FT8. TX ist dort ein eigenes
    //    Fenster und muss es bleiben.
    void nachDemStartInFt8BleibtTxEinFenster()
    {
        QVERIFY(m_profile->activate(QStringLiteral("FT8")));
        warten();
        const QVariantMap ft8 = aufnahme(QStringLiteral("FT8"));
        QVERIFY2(ft8.value(QStringLiteral("floatingApplets")).toMap()
                     .contains(QStringLiteral("Tx")),
                 qPrintable(QStringLiteral("TX ist in FT8 kein Fenster mehr: ")
                            + text(ft8)));
        QVERIFY2(!ft8.value(QStringLiteral("kombiInhalt")).toStringList()
                      .contains(QStringLiteral("Tx")),
                 qPrintable(QStringLiteral("TX liegt in FT8 wieder im Kombi-Feld: ")
                            + text(ft8)));
    }

    // 2. „wenn ich ein Profil kurz aendere und danach wieder auf FT8
    //    zurueckkehre" -- und in „Neu" liegt TX weiter IM Feld, wie
    //    dieses Profil es will.
    void kurzWegUndZurueck()
    {
        QVERIFY(m_profile->activate(QStringLiteral("Neu")));
        warten();
        const QVariantMap neu = aufnahme(QStringLiteral("Neu"));
        QVERIFY2(neu.value(QStringLiteral("kombiInhalt")).toStringList()
                     .contains(QStringLiteral("Tx")),
                 qPrintable(QStringLiteral("in „Neu\" gehoert TX ins Kombi-Feld: ")
                            + text(neu)));
        QVERIFY2(!neu.value(QStringLiteral("floatingApplets")).toMap()
                      .contains(QStringLiteral("Tx")),
                 qPrintable(QStringLiteral("in „Neu\" schwebt TX nicht: ") + text(neu)));

        QVERIFY(m_profile->activate(QStringLiteral("FT8")));
        warten();
        const QVariantMap ft8 = aufnahme(QStringLiteral("FT8"));
        QVERIFY2(ft8.value(QStringLiteral("floatingApplets")).toMap()
                     .contains(QStringLiteral("Tx")),
                 qPrintable(QStringLiteral("nach dem Zurueckkehren ist TX weg: ")
                            + text(ft8)));
    }

    // 3. Das Profil heilt sich dabei selbst: die naechste Aufnahme von
    //    FT8 fuehrt TX nur noch EINMAL. Sonst haelt sich der Widerspruch
    //    in der Datei, und die Regel muss ihn bei jedem Wechsel neu
    //    aufloesen.
    void dasProfilIstDanachOhneWiderspruch()
    {
        const QVariantMap ft8 = aufnahme(QStringLiteral("FT8"));
        const QStringList schwebend =
            ft8.value(QStringLiteral("floatingApplets")).toMap().keys();
        for (const QString& id :
             ft8.value(QStringLiteral("kombiInhalt")).toStringList()) {
            QVERIFY2(!schwebend.contains(id),
                     qPrintable(QStringLiteral("%1 steht in FT8 immer noch doppelt: ")
                                    .arg(id) + text(ft8)));
        }
    }

    // Kein cleanupTestCase(): das Hauptfenster bleibt bis zum Prozessende
    // stehen, wie in tst_profil_loeschen_laesst_andere_stehen -- sein
    // Abbau ist ein eigenes Thema und nicht das, was hier geprueft wird.
};

QTEST_MAIN(TstTxFensterBleibtImFt8Profil)
#include "tst_tx_fenster_bleibt_im_ft8_profil.moc"
