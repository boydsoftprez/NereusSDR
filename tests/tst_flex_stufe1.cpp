// =================================================================
// tests/tst_flex_stufe1.cpp  (Longpath)
// =================================================================
//
// Flex-Treiber Stufe 1 — Erkennung und Befehlskanal, gegen ECHTE
// Bytes geprueft, nicht gegen den eigenen Erzeuger.
//
// Die Erkennungsrufe unten sind am 2026-10-18 vom Draht gelesen, auf
// Martins Netz, mit einem Mitleser auf UDP 4992:
//
//   • 172.30.30.12  — die echte FLEX-8400, SmartSDR 4.3.16.42034,
//                     620 Byte je Ruf, im Sekundentakt.
//   • 172.30.30.110 — Longpath selbst. Es ruft auf demselben Port
//                     aus, damit die PGXL es findet
//                     (FlexRadioDiscoveryBroadcaster), und stand
//                     deshalb am Maestro in Martins Geraeteliste als
//                     Flex, das er nicht besitzt. Hier ist derselbe
//                     Ruf der Pruefstein fuer den Eigenruf-Filter.
//
// Damit sind beide Richtungen von echten Bytes gedeckt: was eine
// Maschine wirklich sendet, und was wir selbst senden und verwerfen
// muessen.
//
// Die Saetze des Befehlskanals (V/H/R/S/M) sind aus AetherSDRs
// Satzlehre uebernommen; die Pruefstuecke hier sind die Beispiele, die
// dort im Kopfkommentar stehen (CommandParser.cpp:26-32 [@d58e2b8a]),
// plus die Faelle, an denen ein Leser schweigend falsch liegt: ein
// Ergebniscode mit gesetztem oberstem Bit, ein mehrteiliger
// Objektname, eine unlesbare Frequenz.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file. Die Byte- und
// Zeilenfolgen sind gelesener Draht bzw. Beispiele aus dem Kopf der
// portierten Satzlehre, kein portierter Code.

#include <QtTest>

#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/AppSettings.h"
#include "core/FlexRadioConnection.h"
#include "core/FlexRadioDiscovery.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Der Kopf der echten 8400, Byte fuer Byte wie gelesen.
//   38      Typ 3, Kennung vorhanden
//   53      0x50 | 3 — TSI=UTC, TSF=Echtzeit, Zaehler 3
//   00 9b   155 Worte = 620 Byte
//   00 00 08 00   VITA-Mundart
//   00 00 1c 2d   pad + OUI 00-1C-2D (FlexRadio Systems)
//   53 4c ff ff   Info-Klasse "SL", Paketklasse 0xFFFF
//   6a d4 7a b9   Zeitstempel (UNIX-Sekunden)
//   00 00 00 00   Bruchteil
//   00 00 00 00   reserviert
const char* kHeader8400Hex =
    "3853009b00000800"
    "00001c2d534cffff"
    "6ad47ab900000000"
    "00000000";

// Die Nutzlast der echten FLEX-8400, wie gelesen.
const char* kPayload8400 =
    "discovery_protocol_version=3.1.0.4 model=FLEX-8400 "
    "serial=1025-1213-8400-1196 version=4.3.16.42034 nickname=Martin "
    "callsign=OE5SOS ip=172.30.30.12 port=4992 status=Available "
    "inuse_ip= inuse_host= max_licensed_version=v3 "
    "radio_license_id=00-1C-2D-05-34-28 fpc_mac= wan_connected=0 "
    "licensed_clients=2 available_clients=2 max_panadapters=2 "
    "available_panadapters=2 max_slices=2 available_slices=2 "
    "gui_client_ips= gui_client_hosts= gui_client_programs= "
    "gui_client_stations= gui_client_handles= "
    "min_software_version=3.8.0.0 external_port_link=1 "
    "license_is_unknown=0 is_system_model=0 turf_region=EU";

// Longpaths eigener Ruf, ebenfalls wie gelesen. Beachte model=FLEX-6400
// und nickname=Longpath — das war das Phantom am Maestro.
const char* kPayloadEigen =
    "discovery_protocol_version=3.1.0.4 model=FLEX-6400 "
    "serial=7341-1489-4925-4009 version=0.7.0 nickname=Longpath "
    "callsign=OE5SOS ip=172.30.30.110 port=4992 status=Available "
    "inuse_ip= inuse_host= max_licensed_version=v4 "
    "radio_license_id=00-1C-2D-00-00-00 fpc_mac=00:00:00:00:00:00 "
    "wan_connected=1 licensed_clients=2 available_clients=2 "
    "max_panadapters=4 available_panadapters=4 max_slices=4 "
    "available_slices=4 gui_client_ips= gui_client_hosts= "
    "gui_client_programs= gui_client_stations= gui_client_handles= "
    "min_software_version=2.13.0.0 external_port_link=1 "
    "license_is_unknown=0 is_system_model=0 turf_region=USA";

QByteArray beacon(const char* payload)
{
    QByteArray out = QByteArray::fromHex(kHeader8400Hex);
    out.append(payload);
    return out;
}

}  // namespace

// Eine falsche Maschine: spricht dieselben Saetze wie die echte, auf
// 127.0.0.1. Der Rundlauf durch den echten Socket ist der Punkt —
// `ingestLineForTesting` allein kann die Anmeldefolge nicht pruefen,
// weil `sendCommand` ohne Verbindung nichts sendet und damit auch kein
// Rueckruf faellig wird. Antworten kommen aus einer Tafel, die der
// Pruefstand je Fall setzt.
class FalscheMaschine : public QObject
{
    Q_OBJECT
public:
    explicit FalscheMaschine(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            m_client = m_server.nextPendingConnection();
            connect(m_client, &QTcpSocket::readyRead, this, &FalscheMaschine::lesen);
            // Genau wie die echte 8400: zuerst die API-Fassung, dann die
            // Clientkennung.
            schreiben(QStringLiteral("V1.4.0.0"));
            schreiben(QStringLiteral("H5081495E"));
        });
    }

    bool hoeren()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    quint16 port() const { return m_server.serverPort(); }

    // Was auf einen Befehl geantwortet wird. Schluessel ist der Anfang
    // des Befehls. Wert ist „<code-hex>|<rumpf>".
    QMap<QString, QString> antworten;

    // Zeilen, die nach einer bestimmten Antwort ungefragt nachkommen.
    QMap<QString, QStringList> nachschub;

    // Zeilen, die VOR der Antwort kommen. Die echte 8400 schiebt den
    // Scheibenzustand so — am 2026-10-18 13:40 gemessen: die Frequenz
    // stand im Protokoll, bevor „angemeldet" gemeldet wurde. AetherSDR
    // beschreibt dasselbe (RadioModel.cpp:6535-6539 [@d58e2b8a]).
    QMap<QString, QStringList> vorschub;

    QStringList empfangeneBefehle;

signals:
    void befehlGesehen(const QString& command);

private slots:
    void lesen()
    {
        m_puffer.append(m_client->readAll());
        int nl;
        while ((nl = m_puffer.indexOf('\n')) >= 0) {
            const QString zeile = QString::fromUtf8(m_puffer.left(nl)).trimmed();
            m_puffer.remove(0, nl + 1);
            if (zeile.isEmpty() || !zeile.startsWith(QLatin1Char('C'))) {
                continue;
            }
            const int pipe = zeile.indexOf(QLatin1Char('|'));
            if (pipe < 0) {
                continue;
            }
            const quint32 seq = zeile.mid(1, pipe - 1).toUInt();
            const QString befehl = zeile.mid(pipe + 1);
            empfangeneBefehle << befehl;
            emit befehlGesehen(befehl);

            QString antwort = QStringLiteral("0|");
            QString treffer;
            for (auto it = antworten.cbegin(); it != antworten.cend(); ++it) {
                if (befehl.startsWith(it.key())) {
                    antwort = it.value();
                    treffer = it.key();
                    break;
                }
            }

            if (!treffer.isEmpty()) {
                for (const QString& frueh : vorschub.value(treffer)) {
                    schreiben(frueh);
                }
            }

            schreiben(QStringLiteral("R%1|%2").arg(seq).arg(antwort));

            if (!treffer.isEmpty()) {
                for (const QString& extra : nachschub.value(treffer)) {
                    schreiben(extra);
                }
            }
        }
    }

private:
    void schreiben(const QString& zeile)
    {
        if (m_client) {
            m_client->write((zeile + QLatin1Char('\n')).toUtf8());
            m_client->flush();
        }
    }

    QTcpServer  m_server;
    QTcpSocket* m_client{nullptr};
    QByteArray  m_puffer;
};

class TestFlexStufe1 : public QObject
{
    Q_OBJECT

private slots:
    // ─── Erkennung: der echte Ruf ───────────────────────────────────

    void kopfIstDerBeschriebene()
    {
        const QByteArray header = QByteArray::fromHex(kHeader8400Hex);
        QCOMPARE(header.size(), FlexDiscoveryWire::kHeaderBytes);

        // Byte 0 / 1 / 2-3 nach dem Satzbau in
        // FlexRadioDiscoveryBroadcaster.cpp:281-294.
        QCOMPARE(static_cast<quint8>(header.at(0)), quint8(0x38));
        QCOMPARE(static_cast<quint8>(header.at(1)) & 0xF0, 0x50);
        QCOMPARE(static_cast<quint8>(header.at(1)) & 0x0F, 3);   // Zaehler

        const int words = (static_cast<quint8>(header.at(2)) << 8)
                          | static_cast<quint8>(header.at(3));
        QCOMPARE(words, 155);
        QCOMPARE(words * 4, 620);   // genau die gemessene Rufgroesse
    }

    void echterRufWirdGelesen()
    {
        const FlexBeacon b = FlexDiscoveryWire::parseBeacon(beacon(kPayload8400));

        QVERIFY(b.valid);
        QCOMPARE(b.discoveryProtocolVersion, QStringLiteral("3.1.0.4"));
        QCOMPARE(b.model,    QStringLiteral("FLEX-8400"));
        QCOMPARE(b.serial,   QStringLiteral("1025-1213-8400-1196"));
        QCOMPARE(b.version,  QStringLiteral("4.3.16.42034"));
        QCOMPARE(b.nickname, QStringLiteral("Martin"));
        QCOMPARE(b.callsign, QStringLiteral("OE5SOS"));
        QCOMPARE(b.address,  QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(b.port,     quint16(4992));
        QCOMPARE(b.status,   QStringLiteral("Available"));
        QCOMPARE(b.radioLicenseId, QStringLiteral("00-1C-2D-05-34-28"));
        QCOMPARE(b.minSoftwareVersion, QStringLiteral("3.8.0.0"));
        QCOMPARE(b.turfRegion, QStringLiteral("EU"));

        // Martins Angaben vom 2026-10-18: 2 Slices, 2 Panadapter.
        QCOMPARE(b.maxSlices,       2);
        QCOMPARE(b.availableSlices, 2);
        QCOMPARE(b.maxPanadapters,       2);
        QCOMPARE(b.availablePanadapters, 2);
        QCOMPARE(b.licensedClients,  2);
        QCOMPARE(b.availableClients, 2);

        QVERIFY(!b.wanConnected);
        QVERIFY(!b.isSystemModel);
        QVERIFY(!b.licenseIsUnknown);
        QVERIFY(b.guiClientStations.isEmpty());
    }

    // Die drei Stellen, an denen AetherSDRs Leser etwas anderes
    // erwartet als die Maschine sendet. Steht hier als Pruefstand,
    // damit die Abweichung nicht beim naechsten Abgleich still
    // zurueckfaellt.
    void abweichungenGegenAetherSdrsLeser()
    {
        const FlexBeacon b = FlexDiscoveryWire::parseBeacon(beacon(kPayload8400));

        // 1. Es gibt kein `inuse`. Die Maschine sendet inuse_ip /
        //    inuse_host, beide leer, solange niemand sitzt.
        QVERIFY(!b.kvs.contains(QStringLiteral("inuse")));
        QVERIFY(b.kvs.contains(QStringLiteral("inuse_ip")));
        QVERIFY(b.kvs.contains(QStringLiteral("inuse_host")));
        QVERIFY(b.inUseIp.isEmpty());
        QVERIFY(b.inUseHost.isEmpty());
        QVERIFY(!b.inUse());

        // 2. Es gibt kein `mf_enable`. Stattdessen die Clientzahlen.
        QVERIFY(!b.kvs.contains(QStringLiteral("mf_enable")));

        // 3. max_licensed_version ist „v3" — eine Zeichenkette. Als
        //    Zahl gelesen gaebe das still 0.
        QCOMPARE(b.maxLicensedVersion, QStringLiteral("v3"));
        bool alsZahl = false;
        b.maxLicensedVersion.toInt(&alsZahl);
        QVERIFY(!alsZahl);
    }

    void eigenerRufIstAuchEinGueltigerRuf()
    {
        // Wichtig fuer den Filter: unser Ruf ist nicht kaputt, er ist
        // nur unserer. Er muss an der ADRESSE scheitern, nicht am Satz.
        const FlexBeacon b = FlexDiscoveryWire::parseBeacon(beacon(kPayloadEigen));
        QVERIFY(b.valid);
        QCOMPARE(b.model,    QStringLiteral("FLEX-6400"));
        QCOMPARE(b.nickname, QStringLiteral("Longpath"));
        QCOMPARE(b.maxSlices, 4);
    }

    void fremderVerkehrAufDemPortIstKeinRuf()
    {
        // Auf 4992 liegt auch anderes. Ohne die Kennung in Byte 8-15
        // gilt ein Datagramm nicht als Flex-Ruf.
        QByteArray fremd = QByteArray::fromHex("3853009b0000080000000000534cffff");
        fremd.append(12, '\0');
        fremd.append(kPayload8400);
        QVERIFY(!FlexDiscoveryWire::parseBeacon(fremd).valid);

        QVERIFY(!FlexDiscoveryWire::parseBeacon(QByteArray()).valid);
        QVERIFY(!FlexDiscoveryWire::parseBeacon(QByteArray("hallo")).valid);
    }

    // ─── MAC und Uebersetzung in die Geraetezeile ───────────────────

    void macAusRadioLicenseId()
    {
        QCOMPARE(FlexDiscoveryWire::macFromRadioLicenseId(
                     QStringLiteral("00-1C-2D-05-34-28")),
                 QStringLiteral("00:1C:2D:05:34:28"));
        // Kleinschreibung kommt gross zurueck — Longpath fuehrt MACs
        // als "AA:BB:CC:DD:EE:FF" (RadioDiscovery.h).
        QCOMPARE(FlexDiscoveryWire::macFromRadioLicenseId(
                     QStringLiteral("00-1c-2d-05-34-28")),
                 QStringLiteral("00:1C:2D:05:34:28"));

        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(QString()).isEmpty());
        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(
                    QStringLiteral("00-1C-2D-05-34")).isEmpty());        // zu kurz
        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(
                    QStringLiteral("00-1C-2D-05-34-28-99")).isEmpty());  // zu lang
        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(
                    QStringLiteral("ZZ-1C-2D-05-34-28")).isEmpty());     // nicht hex
        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(
                    QStringLiteral("0-1C-2D-05-34-28")).isEmpty());      // Gruppe nicht 2
    }

    void uebersetzungInDieGeraetezeile()
    {
        const FlexBeacon b = FlexDiscoveryWire::parseBeacon(beacon(kPayload8400));
        const RadioInfo info = FlexDiscoveryWire::toRadioInfo(
            b, QHostAddress(QStringLiteral("172.30.30.12")));

        QCOMPARE(info.protocol,  ProtocolVersion::Flex);
        QCOMPARE(info.boardType, HPSDRHW::Flex8400);
        QCOMPARE(info.macAddress, QStringLiteral("00:1C:2D:05:34:28"));
        QCOMPARE(info.address, QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(info.port, quint16(4992));
        QCOMPARE(info.name, QStringLiteral("FLEX-8400 (Martin)"));
        QVERIFY(!info.inUse);

        // Die Empfaengerzahl kommt aus dem Ruf, nicht aus einer
        // Platinentabelle — bei Flex rechnet das Radio.
        QCOMPARE(info.maxReceivers, 2);
        QCOMPARE(info.adcCount, 1);
        QVERIFY(!info.hasDiversityReceiver);

        // Keine erfundene Firmwarezahl: die Maschine meldet
        // „4.3.16.42034", das passt in kein int.
        QCOMPARE(info.firmwareVersion, 0);

        // ... sie geht aber als Text mit. Vorher wurde `version` aus dem
        // Ruf gelesen und dann weggeworfen: der Wert stand nur in
        // FlexBeacon, und niemand las ihn. Folge: die Anzeige zeigte
        // einen Gedankenstrich und das Hilfsbuendel Firmware „0",
        // obwohl die Maschine ihre Fassung gesendet hatte.
        QCOMPARE(info.firmwareText, QStringLiteral("4.3.16.42034"));
    }

    void messwertDefinitionenTrennenMitRaute()
    {
        // Echte Zeile, am 2026-10-21 an der 8400 mitgeschnitten
        // (Fassung 4.3.16.42034) nach `sub meter all`.
        const FlexLine l = FlexCommandWire::parseLine(QStringLiteral(
            "S4ED321C5|meter 1.src=COD-#1.num=1#1.nam=MICPEAK#"
            "1.low=-150.0#1.hi=20.0#"
            "1.desc=Signal strength of MIC output in CODEC#"
            "1.unit=dBFS#1.fps=40#"));

        QCOMPARE(l.type, FlexLineType::Status);
        QCOMPARE(l.object, QStringLiteral("meter"));

        // An Leerzeichen zerlegt zerfiel das in Unsinn: aus der
        // Beschreibung wurden die nackten Woerter „MIC=", „in=", „of=",
        // „output=", „strength=", und `1.desc` trug nur „Signal".
        QCOMPARE(l.kvs.value(QStringLiteral("1.nam")),
                 QStringLiteral("MICPEAK"));
        QCOMPARE(l.kvs.value(QStringLiteral("1.unit")),
                 QStringLiteral("dBFS"));
        QCOMPARE(l.kvs.value(QStringLiteral("1.low")),
                 QStringLiteral("-150.0"));
        QCOMPARE(l.kvs.value(QStringLiteral("1.desc")),
                 QStringLiteral("Signal strength of MIC output in CODEC"));
        QVERIFY2(!l.kvs.contains(QStringLiteral("strength")),
                 "die Beschreibung ist wieder in Woerter zerfallen");
        QCOMPARE(l.kvs.size(), 8);
    }

    void leerzeichenRuempfeBleibenWieSieWaren()
    {
        // Gegenstueck: der ueblichere Fall darf sich NICHT aendern.
        const FlexLine l = FlexCommandWire::parseLine(QStringLiteral(
            "S12345678|slice 0 freq=14.225000 mode=USB filter_lo=-1500"));
        QCOMPARE(l.object, QStringLiteral("slice 0"));
        QCOMPARE(l.kvs.value(QStringLiteral("freq")),
                 QStringLiteral("14.225000"));
        QCOMPARE(l.kvs.value(QStringLiteral("mode")), QStringLiteral("USB"));
        QCOMPARE(l.kvs.value(QStringLiteral("filter_lo")),
                 QStringLiteral("-1500"));
        QCOMPARE(l.kvs.size(), 3);
    }

    void fehlendeFassungImRufLaesstDenTextLeer()
    {
        QString payload = QString::fromLatin1(kPayload8400);
        payload.replace(QStringLiteral("version=4.3.16.42034"),
                        QStringLiteral("version="));
        const FlexBeacon b =
            FlexDiscoveryWire::parseBeacon(beacon(payload.toLatin1().constData()));
        const RadioInfo info = FlexDiscoveryWire::toRadioInfo(b, QHostAddress());

        // Leer heisst „nimm die Zahl", nicht „zeige nichts". Ein leerer
        // Text wuerde in connectionFirmwareText() den Gedankenstrich
        // verdraengen und nichts an seine Stelle setzen.
        QVERIFY(info.firmwareText.isEmpty());
        QCOMPARE(info.firmwareVersion, 0);
    }

    void serienummerTraegtWennDieMacFehlt()
    {
        QString payload = QString::fromLatin1(kPayload8400);
        payload.replace(QStringLiteral("radio_license_id=00-1C-2D-05-34-28"),
                        QStringLiteral("radio_license_id="));
        const FlexBeacon b =
            FlexDiscoveryWire::parseBeacon(beacon(payload.toLatin1().constData()));
        const RadioInfo info = FlexDiscoveryWire::toRadioInfo(b, QHostAddress());

        // Ohne Rueckfall waere die Zeile schluessellos und wuerde bei
        // jedem Ruf neu angelegt.
        QCOMPARE(info.macAddress, QStringLiteral("1025-1213-8400-1196"));
    }

    void adresseFaelltAufDenAbsenderZurueck()
    {
        QString payload = QString::fromLatin1(kPayload8400);
        payload.replace(QStringLiteral("ip=172.30.30.12"), QStringLiteral("ip="));
        const FlexBeacon b =
            FlexDiscoveryWire::parseBeacon(beacon(payload.toLatin1().constData()));
        QVERIFY(b.address.isNull());

        const RadioInfo info = FlexDiscoveryWire::toRadioInfo(
            b, QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(info.address, QHostAddress(QStringLiteral("172.30.30.12")));
    }

    // ─── Die Liste: anlegen, nachfuehren, vergessen ─────────────────

    void rufWirdGemeldetUndNachgefuehrt()
    {
        FlexRadioDiscovery disc;
        disc.setOwnAddressesForTesting({QHostAddress(QStringLiteral("203.0.113.7"))});

        QSignalSpy gefunden(&disc, &FlexRadioDiscovery::radioDiscovered);
        QSignalSpy geaendert(&disc, &FlexRadioDiscovery::radioUpdated);

        const QHostAddress von(QStringLiteral("172.30.30.12"));
        disc.ingestDatagramForTesting(beacon(kPayload8400), von);
        QCOMPARE(gefunden.count(), 1);
        QCOMPARE(geaendert.count(), 0);
        QCOMPARE(disc.discoveredRadios().size(), 1);

        // Zweiter Ruf derselben Maschine: nachfuehren, nicht anlegen.
        disc.ingestDatagramForTesting(beacon(kPayload8400), von);
        QCOMPARE(gefunden.count(), 1);
        QCOMPARE(geaendert.count(), 1);
        QCOMPARE(disc.discoveredRadios().size(), 1);
    }

    void eigenerRufStehtNichtInDerListe()
    {
        // Genau der Fehler vom 2026-10-18: Longpath als Flex in der
        // eigenen Liste. Der Ruf ist gueltig (siehe oben), er kommt nur
        // von einer eigenen Adresse.
        FlexRadioDiscovery disc;
        disc.setOwnAddressesForTesting({QHostAddress(QStringLiteral("172.30.30.110"))});

        QSignalSpy gefunden(&disc, &FlexRadioDiscovery::radioDiscovered);
        disc.ingestDatagramForTesting(beacon(kPayloadEigen),
                                      QHostAddress(QStringLiteral("172.30.30.110")));
        QCOMPARE(gefunden.count(), 0);
        QVERIFY(disc.discoveredRadios().isEmpty());

        // Dieselbe Maschine von einer fremden Adresse waere eine
        // fremde Maschine — der Filter haengt an der Adresse, nicht am
        // Spitznamen.
        disc.ingestDatagramForTesting(beacon(kPayloadEigen),
                                      QHostAddress(QStringLiteral("172.30.30.99")));
        QCOMPARE(gefunden.count(), 1);
    }

    void rufOhneSeriennummerWirdVerworfen()
    {
        QString payload = QString::fromLatin1(kPayload8400);
        payload.replace(QStringLiteral("serial=1025-1213-8400-1196"),
                        QStringLiteral("serial="));

        FlexRadioDiscovery disc;
        disc.setOwnAddressesForTesting({QHostAddress(QStringLiteral("203.0.113.7"))});
        QSignalSpy gefunden(&disc, &FlexRadioDiscovery::radioDiscovered);
        disc.ingestDatagramForTesting(beacon(payload.toLatin1().constData()),
                                      QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(gefunden.count(), 0);
    }

    // ─── Befehlskanal: die Satzlehre ────────────────────────────────

    void fassungUndKennung()
    {
        FlexLine v = FlexCommandWire::parseLine(QStringLiteral("V3.3.28.0"));
        QCOMPARE(v.type, FlexLineType::Version);
        QCOMPARE(v.object, QStringLiteral("3.3.28.0"));

        FlexLine h = FlexCommandWire::parseLine(QStringLiteral("H0A1B2C3D"));
        QCOMPARE(h.type, FlexLineType::Handle);
        QCOMPARE(h.handle, quint32(0x0A1B2C3D));
    }

    void antwortMitUndOhneFehler()
    {
        FlexLine ok = FlexCommandWire::parseLine(QStringLiteral("R1|0|"));
        QCOMPARE(ok.type, FlexLineType::Response);
        QCOMPARE(ok.sequence, quint32(1));
        QCOMPARE(ok.resultCode, quint32(0));

        FlexLine err = FlexCommandWire::parseLine(
            QStringLiteral("R2|50001001|No Such Object"));
        QCOMPARE(err.sequence, quint32(2));
        QCOMPARE(err.resultCode, quint32(0x50001001));
        QCOMPARE(err.object, QStringLiteral("No Such Object"));
    }

    void schwererCodeBehaeltSeinOberstesBit()
    {
        // Der Fall, an dem ein int schweigend luegt: F3000001 hat das
        // oberste Bit gesetzt. Als int gelesen gibt QString::toInt()
        // einen Ueberlauf und damit 0 — eine schwere Ablehnung saehe
        // dann wie Erfolg aus. (AetherSDR schreibt denselben Hinweis an
        // dieselbe Stelle, CommandParser.cpp:63-70 [@d58e2b8a].)
        FlexLine fatal = FlexCommandWire::parseLine(
            QStringLiteral("R7|F3000001|Fatal"));
        QCOMPARE(fatal.resultCode, quint32(0xF3000001));
        QVERIFY(fatal.resultCode != 0);

        // Gegenprobe, dass die Gefahr echt ist und nicht behauptet:
        bool ok = false;
        QStringLiteral("F3000001").toInt(&ok, 16);
        QVERIFY(!ok);
    }

    void zustandsschubUndDieObjektgrenze()
    {
        FlexLine s = FlexCommandWire::parseLine(
            QStringLiteral("S0A1B2C3D|slice 0 RF_frequency=14.225000 mode=USB"));
        QCOMPARE(s.type, FlexLineType::Status);
        QCOMPARE(s.handle, quint32(0x0A1B2C3D));
        QCOMPARE(s.object, QStringLiteral("slice 0"));
        QCOMPARE(s.kvs.value(QStringLiteral("RF_frequency")),
                 QStringLiteral("14.225000"));
        QCOMPARE(s.kvs.value(QStringLiteral("mode")), QStringLiteral("USB"));

        // Mehrteiliger Objektname: die Grenze ist das letzte Leerzeichen
        // VOR dem ersten '=', nicht das erste Leerzeichen.
        FlexLine pan = FlexCommandWire::parseLine(
            QStringLiteral("S0A1B2C3D|display pan 0x40000000 center=14.100 bandwidth=0.2"));
        QCOMPARE(pan.object, QStringLiteral("display pan 0x40000000"));
        QCOMPARE(pan.kvs.value(QStringLiteral("center")), QStringLiteral("14.100"));

        // Ganz ohne Schluesselpaare ist der Rumpf nur ein Objektname.
        FlexLine nackt = FlexCommandWire::parseLine(
            QStringLiteral("S0A1B2C3D|slice 1 removed"));
        QCOMPARE(nackt.object, QStringLiteral("slice 1 removed"));
        QVERIFY(nackt.kvs.isEmpty());
    }

    void meldungTraegtIhreSchwereInBit24Und25()
    {
        struct Fall { const char* zahl; FlexSeverity schwere; };
        const Fall faelle[] = {
            {"00000001", FlexSeverity::Info},
            {"01000001", FlexSeverity::Warning},
            {"02000001", FlexSeverity::Error},
            {"03000001", FlexSeverity::Fatal},
        };
        for (const Fall& f : faelle) {
            const FlexLine m = FlexCommandWire::parseLine(
                QStringLiteral("M%1|etwas ist passiert").arg(QLatin1String(f.zahl)));
            QCOMPARE(m.type, FlexLineType::Message);
            QCOMPARE(m.severity, f.schwere);
            QCOMPARE(m.object, QStringLiteral("etwas ist passiert"));
        }
    }

    void unbekannteSatzartUndLeereZeile()
    {
        QCOMPARE(FlexCommandWire::parseLine(QStringLiteral("X1|2")).type,
                 FlexLineType::Unknown);
        QCOMPARE(FlexCommandWire::parseLine(QString()).type,
                 FlexLineType::Unknown);
    }

    void befehlWirdRichtigEingepackt()
    {
        QCOMPARE(FlexCommandWire::buildCommand(5, QStringLiteral("sub slice all")),
                 QByteArray("C5|sub slice all\n"));
    }

    // ─── Die Frequenz, das Ziel der Stufe 1 ────────────────────────

    void scheibennummerAusDemObjektnamen()
    {
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("slice 0")), 0);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("slice 3")), 3);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("slice x")), -1);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("slice")), -1);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("display pan 0x4")), -1);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QString()), -1);
    }

    void frequenzKommtInMegahertzUndWirdZuHertz()
    {
        QMap<QString, QString> kvs;
        kvs.insert(QStringLiteral("RF_frequency"), QStringLiteral("14.074000"));
        const auto hz = FlexCommandWire::frequencyHzFromSliceKvs(kvs);
        QVERIFY(hz.has_value());
        QCOMPARE(*hz, 14'074'000.0);
    }

    void unlesbareFrequenzWirdNichtZuNull()
    {
        // Der Grund fuer die Regel: eine verstuemmelte RF_frequency
        // wuerde sonst auf 0 Hz stimmen. FlexLib faellt hier zu, und
        // AetherSDR schreibt genau das an dieselbe Stelle
        // (FlexBackend.cpp:731-736 [@d58e2b8a]).
        QMap<QString, QString> kaputt;
        kaputt.insert(QStringLiteral("RF_frequency"), QStringLiteral("NOT_A_NUMBER"));
        QVERIFY(!FlexCommandWire::frequencyHzFromSliceKvs(kaputt).has_value());

        // Fehlt der Schluessel ganz, kommt auch nichts.
        QMap<QString, QString> ohne;
        ohne.insert(QStringLiteral("mode"), QStringLiteral("USB"));
        QVERIFY(!FlexCommandWire::frequencyHzFromSliceKvs(ohne).has_value());
    }

    // ─── Die Verbindung als Ganzes, ohne Netz ──────────────────────

    void kennungSchaltetAufVerbundenUndFrequenzKommtDurch()
    {
        FlexRadioConnection conn;
        QSignalSpy fassung(&conn, &FlexRadioConnection::versionReceived);
        QSignalSpy verbunden(&conn, &FlexRadioConnection::connected);
        QSignalSpy frequenz(&conn, &FlexRadioConnection::sliceFrequencyChanged);

        QCOMPARE(conn.state(), FlexRadioConnection::State::Disconnected);

        conn.ingestLineForTesting(QStringLiteral("V4.3.16.42034"));
        QCOMPARE(fassung.count(), 1);
        QCOMPARE(conn.radioVersion(), QStringLiteral("4.3.16.42034"));
        // Die Fassung allein macht noch keine Verbindung — erst die
        // Kennung. (So setzt es auch AetherSDR,
        // RadioConnection.cpp:552-559 [@d58e2b8a].)
        QCOMPARE(verbunden.count(), 0);

        conn.ingestLineForTesting(QStringLiteral("H0A1B2C3D"));
        QCOMPARE(verbunden.count(), 1);
        QCOMPARE(conn.state(), FlexRadioConnection::State::Connected);
        QCOMPARE(conn.clientHandle(), quint32(0x0A1B2C3D));

        conn.ingestLineForTesting(
            QStringLiteral("S0A1B2C3D|slice 0 RF_frequency=14.074000 mode=DIGU"));
        QCOMPARE(frequenz.count(), 1);
        QCOMPARE(frequenz.at(0).at(0).toInt(), 0);
        QCOMPARE(frequenz.at(0).at(1).toDouble(), 14'074'000.0);
        QCOMPARE(conn.sliceFrequenciesHz().value(0), 14'074'000.0);
    }

    void gleicheFrequenzZweimalMeldetEinmal()
    {
        // Die Maschine schiebt denselben Zustand mehrfach. Ein
        // unveraenderter Wert ist keine Nachricht.
        FlexRadioConnection conn;
        conn.ingestLineForTesting(QStringLiteral("H1"));
        QSignalSpy frequenz(&conn, &FlexRadioConnection::sliceFrequencyChanged);

        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 0 RF_frequency=14.074000"));
        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 0 RF_frequency=14.074000"));
        QCOMPARE(frequenz.count(), 1);

        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 0 RF_frequency=14.075000"));
        QCOMPARE(frequenz.count(), 2);
    }

    void zweiScheibenWerdenGetrenntGefuehrt()
    {
        FlexRadioConnection conn;
        conn.ingestLineForTesting(QStringLiteral("H1"));

        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 0 RF_frequency=14.074000"));
        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 1 RF_frequency=7.074000"));

        QCOMPARE(conn.sliceFrequenciesHz().size(), 2);
        QCOMPARE(conn.sliceFrequenciesHz().value(0), 14'074'000.0);
        QCOMPARE(conn.sliceFrequenciesHz().value(1),  7'074'000.0);
    }

    void zustandsschubOhneFrequenzMeldetNichts()
    {
        FlexRadioConnection conn;
        conn.ingestLineForTesting(QStringLiteral("H1"));
        QSignalSpy frequenz(&conn, &FlexRadioConnection::sliceFrequencyChanged);

        conn.ingestLineForTesting(QStringLiteral("S1|slice 0 mode=USB audio_level=50"));
        conn.ingestLineForTesting(QStringLiteral("S1|display pan 0x40000000 center=14.1"));
        conn.ingestLineForTesting(QStringLiteral("S1|slice 0 RF_frequency=kaputt"));
        QCOMPARE(frequenz.count(), 0);
        QVERIFY(conn.sliceFrequenciesHz().isEmpty());
    }

    void befehlOhneVerbindungWirdNichtGesendet()
    {
        FlexRadioConnection conn;
        QCOMPARE(conn.sendCommand(QStringLiteral("sub slice all")), quint32(0));
    }

    // ─── Kennungen und Listen aus Antworten ────────────────────────

    void flexKennungWirdVereinheitlicht()
    {
        using FlexCommandWire::normalizedFlexId;
        QCOMPARE(normalizedFlexId(QStringLiteral("0x40000000")),
                 QStringLiteral("40000000"));
        QCOMPARE(normalizedFlexId(QStringLiteral("40000000")),
                 QStringLiteral("40000000"));
        // Nicht auf acht Stellen gefuellt kommt gefuellt zurueck.
        QCOMPARE(normalizedFlexId(QStringLiteral("0x4")),
                 QStringLiteral("00000004"));
        QCOMPARE(normalizedFlexId(QStringLiteral("  0X4A  ")),
                 QStringLiteral("0000004a"));
        QVERIFY(normalizedFlexId(QString()).isEmpty());
        QVERIFY(normalizedFlexId(QStringLiteral("keine-zahl")).isEmpty());
    }

    // Die drei Formen sind NICHT geraten: sie stehen so in AetherSDRs
    // eigenem Pruefstand, tests/radio_status_ownership_test.cpp:354-362
    // [@d58e2b8a]. Ich hatte sie beim ersten Versuch erfunden („wf="
    // statt „waterfall=", und die Schluesselform mit Komma verbunden) —
    // der Pruefstand fiel durch, und das war richtig so.
    void panKennungAusDreiAntwortformen()
    {
        using FlexCommandWire::panIdFromCreateReply;
        // Kommaform — so antwortet die Maschine wirklich; das zweite
        // Glied ist der Wasserfall.
        QCOMPARE(panIdFromCreateReply(QStringLiteral("0x40000000,0x42000000")),
                 QStringLiteral("40000000"));
        QCOMPARE(panIdFromCreateReply(QStringLiteral("40000000,0x42000000")),
                 QStringLiteral("40000000"));
        // Schluesselform, an LEERZEICHEN getrennt.
        QCOMPARE(panIdFromCreateReply(
                     QStringLiteral("pan=0x40000001 waterfall=0x42000001")),
                 QStringLiteral("40000001"));
        QCOMPARE(panIdFromCreateReply(QStringLiteral("id=0x40000002")),
                 QStringLiteral("40000002"));
        QVERIFY(panIdFromCreateReply(QString()).isEmpty());
        QVERIFY(panIdFromCreateReply(QStringLiteral("nichts brauchbares")).isEmpty());

        // Die Falle, in die ich gelaufen bin: eine Schluesselform mit
        // Komma dahinter ist KEINE gueltige Antwort, und der Leser faellt
        // dabei zu statt eine halbe Kennung zu melden.
        QVERIFY(panIdFromCreateReply(
                    QStringLiteral("pan=0x40000000,waterfall=0x42000000")).isEmpty());
    }

    // Der Befehl, mit dem Longpath die Flex abstimmt.
    //
    // Bis zum 2026-10-22 gab es ihn nicht: die Frequenz der Maschine
    // erreichte Longpaths VFO nie (`flexSliceFrequencyChanged` wurde
    // ausgesendet und von niemandem gehoert), und drehen liess sich
    // die Maschine gar nicht.
    void abstimmbefehlTraegtAutopanNull()
    {
        using FlexCommandWire::tuneSliceCommand;

        // Hertz hinein, Megahertz mit sechs Nachkommastellen hinaus —
        // dieselbe Schreibweise, in der die Maschine meldet.
        QCOMPARE(tuneSliceCommand(QStringLiteral("0"), 14'225'000.0),
                 QStringLiteral("slice tune 0 14.225000 autopan=0"));
        QCOMPARE(tuneSliceCommand(QStringLiteral("1"), 7'108'700.0),
                 QStringLiteral("slice tune 1 7.108700 autopan=0"));

        // Der Faenger: OHNE `autopan=0` zentriert die Maschine den
        // Panadapter neu. Unser Bild haengt am Ausschnitt, den die
        // Wasserfallkachel meldet — der sprengt dann mit jeder
        // Abstimmung weg (From AetherSDR SliceModel.cpp:124
        // [@d58e2b8a]).
        QVERIFY2(tuneSliceCommand(QStringLiteral("0"), 14'225'000.0)
                     .endsWith(QStringLiteral("autopan=0")),
                 "ohne autopan=0 zentriert die Maschine den Panadapter neu");

        // Unsinn gibt keinen Befehl, nicht einen halben.
        QVERIFY(tuneSliceCommand(QString(), 14'225'000.0).isEmpty());
        QVERIFY(tuneSliceCommand(QStringLiteral("0"), 0.0).isEmpty());
        QVERIFY(tuneSliceCommand(QStringLiteral("0"), -1.0).isEmpty());
    }

    // ── Rauschminderung in die Maschine (2026-10-28) ─────────────────
    //
    // Der Betreiber: „ich kann zwar klicken, aber kein unterschied zu
    // hören." Flex-Ton kommt fertig demoduliert in den Mischer und
    // läuft nie durch den WDSP-Empfangskanal, in dem Longpaths
    // NR/NB/ANF sitzen. Der Knopf schaltet deshalb jetzt die
    // Rauschminderung DER MASCHINE.
    void rauschbefehlTraegtAlleDrei()
    {
        using FlexCommandWire::setSliceNoiseCommand;

        QCOMPARE(setSliceNoiseCommand(QStringLiteral("0"), true, false, false),
                 QStringLiteral("slice set 0 nr=1 nb=0 anf=0"));
        QCOMPARE(setSliceNoiseCommand(QStringLiteral("1"), false, true, true),
                 QStringLiteral("slice set 1 nr=0 nb=1 anf=1"));
        QCOMPARE(setSliceNoiseCommand(QStringLiteral("0"), false, false, false),
                 QStringLiteral("slice set 0 nr=0 nb=0 anf=0"));

        // EIN Befehl für alle drei, nicht drei einzelne: so kann kein
        // halber Zustand entstehen, wenn einer verloren geht.
        const QString b = setSliceNoiseCommand(QStringLiteral("0"),
                                               true, true, true);
        QCOMPARE(b.count(QStringLiteral("slice set")), 1);
        QVERIFY(b.contains(QStringLiteral("nr=1")));
        QVERIFY(b.contains(QStringLiteral("nb=1")));
        QVERIFY(b.contains(QStringLiteral("anf=1")));

        // Die Pegel bleiben draußen: sie stehen in der Maschine, und
        // ein mitgeschickter Wert überschriebe die Einstellung des
        // Betreibers bei jedem Knopfdruck.
        QVERIFY2(!b.contains(QStringLiteral("_level")),
                 "nr_level/nb_level/anf_level gehoeren der Maschine");

        // Ohne Scheibennummer kein Befehl, nicht ein halber.
        QVERIFY(setSliceNoiseCommand(QString(), true, true, true).isEmpty());
    }

    // ── Betriebsart und Filterbreite (2026-10-22) ────────────────────
    //
    // Bis heute war `slice tune` der EINZIGE Befehl an eine
    // Flex-Scheibe. Der Betreiber hoerte Stationen, aber keine
    // verstaendliche Sprache: „als waere es eine andere modulation".
    // Longpath zeigte LSB, die Maschine stand auf USB — die
    // Betriebsart wurde nur beim Verbinden gesetzt und danach nie.
    void betriebsartbefehl()
    {
        using FlexCommandWire::setSliceModeCommand;

        QCOMPARE(setSliceModeCommand(QStringLiteral("0"),
                                     QStringLiteral("LSB")),
                 QStringLiteral("slice set 0 mode=LSB"));
        QCOMPARE(setSliceModeCommand(QStringLiteral("2"),
                                     QStringLiteral("digu")),
                 QStringLiteral("slice set 2 mode=DIGU"));

        // Nur was die Maschine in ihrer eigenen `mode_list` fuehrt.
        // Ein unbekannter Name wird abgewiesen und liesze die Scheibe
        // im alten Zustand, die Anzeige aber im neuen — also wieder
        // zwei Wahrheiten. Lieber gar nichts schicken.
        QVERIFY(setSliceModeCommand(QStringLiteral("0"),
                                    QStringLiteral("CWL")).isEmpty());
        QVERIFY(setSliceModeCommand(QStringLiteral("0"),
                                    QStringLiteral("SPEC")).isEmpty());
        QVERIFY(setSliceModeCommand(QString(),
                                    QStringLiteral("USB")).isEmpty());
    }

    // From AetherSDR SliceModel.cpp:231-232 [@d58e2b8a]:
    //   // FlexAPI: "filt <id> <low_hz> <high_hz>"
    void filterbefehl()
    {
        using FlexCommandWire::setSliceFilterCommand;

        QCOMPARE(setSliceFilterCommand(QStringLiteral("0"), 100, 2900),
                 QStringLiteral("filt 0 100 2900"));
        QCOMPARE(setSliceFilterCommand(QStringLiteral("1"), -2900, -100),
                 QStringLiteral("filt 1 -2900 -100"));

        // Verdrehte oder gleiche Flanken geben keinen Befehl: die
        // Maschine wuerde ihn abweisen, und die Anzeige stuende
        // danach auf etwas anderes als die Scheibe.
        QVERIFY(setSliceFilterCommand(QStringLiteral("0"), 2900, 100).isEmpty());
        QVERIFY(setSliceFilterCommand(QStringLiteral("0"), 100, 100).isEmpty());
        QVERIFY(setSliceFilterCommand(QString(), 100, 2900).isEmpty());
    }

    // ── Der Panadapter muss der Scheibe folgen (2026-10-22) ─────────
    //
    // `slice tune` traegt `autopan=0`, damit das Bild beim Drehen
    // nicht springt. Bei einem WEITEN Sprung stand die Scheibe dann
    // aber neben dem Bild: am Geraet um 21:22 gemessen, Scheibe auf
    // 14.382279 MHz, Panadapter auf 230 kHz um 14.0999 MHz. Der
    // Betreiber hoerte an einer Stelle und sah eine andere, 170 kHz
    // daneben.
    //
    // From AetherSDR FlexBackend.cpp:420 [@d58e2b8a].
    void panadapterMitteBefehl()
    {
        using FlexCommandWire::setPanCenterCommand;

        QCOMPARE(setPanCenterCommand(QStringLiteral("0x40000000"),
                                     14'225'000.0),
                 QStringLiteral("display pan set 0x40000000 center=14.225000"));

        QVERIFY(setPanCenterCommand(QString(), 14'225'000.0).isEmpty());
        QVERIFY(setPanCenterCommand(QStringLiteral("0x40000000"), 0.0).isEmpty());
    }

    void scheibenlisteAusDerAntwort()
    {
        using FlexCommandWire::sliceListFromReply;
        // Der Normalfall ohne GUI-Client: LEER. Das ist ein Ergebnis,
        // kein Fehler — genau dieser Befund hat die Stufe entschieden.
        QVERIFY(sliceListFromReply(QString()).isEmpty());
        QVERIFY(sliceListFromReply(QStringLiteral("   ")).isEmpty());

        QCOMPARE(sliceListFromReply(QStringLiteral("0")), QList<int>({0}));
        QCOMPARE(sliceListFromReply(QStringLiteral("0,1")), QList<int>({0, 1}));
        QCOMPARE(sliceListFromReply(QStringLiteral(" 0 , 1 , 2 ")),
                 QList<int>({0, 1, 2}));
        // Unlesbare Glieder fallen heraus, lesbare bleiben.
        QCOMPARE(sliceListFromReply(QStringLiteral("0,xx,2")), QList<int>({0, 2}));
    }

    // ─── Der Rundlauf: Anmeldung gegen eine falsche Maschine ───────

    void anmeldungLegtScheibeAnWennKeineDaIst()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());

        // Genau die Antworten, die die echte 8400 am 2026-10-18 gab,
        // plus die, die sie ohne GUI-Anmeldung nicht gab.
        radio.antworten.insert(QStringLiteral("client program"),
                               QStringLiteral("10000002|unknown client program Longpath"));
        radio.antworten.insert(QStringLiteral("client gui"),
                               QStringLiteral("0|9F2C1A4E-0000-4000-8000-0123456789AB"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|"));
        // Kommaform, wie AetherSDRs eigener Pruefstand sie fuehrt
        // (radio_status_ownership_test.cpp:354 [@d58e2b8a]).
        radio.antworten.insert(QStringLiteral("display panafall create"),
                               QStringLiteral("0|0x40000000,0x42000000"));
        radio.antworten.insert(QStringLiteral("slice create"), QStringLiteral("0|0"));
        // Die echte Maschine schiebt den Scheibenzustand hinterher.
        radio.nachschub.insert(QStringLiteral("slice create"),
            {QStringLiteral("S5081495E|slice 0 in_use=1 RF_frequency=14.225000 "
                            "mode=USB rxant=ANT1 ant_list=ANT1,ANT2,RX_A")});

        FlexRadioConnection conn;
        conn.setGuiClientId(QStringLiteral("ABCDEF01-2345-6789-ABCD-EF0123456789"));

        QSignalSpy angemeldet(&conn, &FlexRadioConnection::guiClientRegistered);
        QSignalSpy listeGelesen(&conn, &FlexRadioConnection::sliceListRead);
        QSignalSpy angelegt(&conn, &FlexRadioConnection::defaultSliceCreated);
        QSignalSpy frequenz(&conn, &FlexRadioConnection::sliceFrequencyChanged);
        QSignalSpy kennung(&conn, &FlexRadioConnection::guiClientIdChosen);

        conn.connectToRadio(QHostAddress::LocalHost, radio.port());
        QVERIFY(frequenz.wait(5000));

        QCOMPARE(angemeldet.count(), 1);
        QCOMPARE(listeGelesen.count(), 1);
        QVERIFY(listeGelesen.at(0).at(0).value<QList<int>>().isEmpty());
        QCOMPARE(angelegt.count(), 1);
        QCOMPARE(angelegt.at(0).at(0).toString(), QStringLiteral("40000000"));

        QCOMPARE(frequenz.count(), 1);
        QCOMPARE(frequenz.at(0).at(0).toInt(), 0);
        QCOMPARE(frequenz.at(0).at(1).toDouble(), 14'225'000.0);

        // Die Maschine hat eine andere Kennung zurueckgemeldet als die
        // gesetzte — die gilt, und sie wird gemeldet, damit der
        // Aufrufer sie behalten kann.
        QCOMPARE(kennung.count(), 1);
        QCOMPARE(kennung.at(0).at(0).toString(),
                 QStringLiteral("9F2C1A4E-0000-4000-8000-0123456789AB"));
        QCOMPARE(conn.guiClientId(),
                 QStringLiteral("9F2C1A4E-0000-4000-8000-0123456789AB"));

        // Die Reihenfolge ist Teil des Protokolls: Programm, dann GUI,
        // dann Station und Abonnement, dann nachsehen.
        const QStringList b = radio.empfangeneBefehle;
        QVERIFY(b.size() >= 6);
        QVERIFY(b.at(0).startsWith(QStringLiteral("client program")));
        QVERIFY(b.at(1).startsWith(QStringLiteral("client gui")));
        QVERIFY(b.contains(QStringLiteral("client station Longpath")));
        QVERIFY(b.contains(QStringLiteral("sub slice all")));
        QVERIFY(b.contains(QStringLiteral("slice list")));

        // ── Die Sendeebene muss bestellt sein (2026-10-27) ──────────
        //
        // Ohne `sub tx all` schickt die Maschine NIE einen
        // `interlock`-Zustand. Stufe 1 brauchte ihn nicht; seit
        // Stufe 2 ist er das Tor vor dem Tasten — und ein Tor, das nie
        // eine Freigabe hoert, bleibt fuer immer zu. Genau daran
        // scheiterte der erste Sendeversuch des Betreibers: „TUNE ist
        // aber nicht gegangen", ohne dass eine Zeile Code falsch
        // aussah.
        QVERIFY2(b.contains(QStringLiteral("sub tx all")),
                 "ohne `sub tx all` kommt nie eine Verriegelungsmeldung, "
                 "und ohne die wird nie getastet");
        QVERIFY(b.contains(QStringLiteral("display panafall create x=100 y=100")));
        QVERIFY(b.contains(QStringLiteral(
            "slice create pan=40000000 freq=14.225000 mode=USB")));

        // Der Faenger, nicht der Waechter: Martin HOERTE das Relais im
        // FLEX-8400 bei jedem Verbinden klicken, 2026-10-21. Ursache war
        // `antenna=ANT1` in genau dieser Zeile — die Maschine schaltet
        // das Relais auch dann, wenn dort schon ANT1 steht. Kein Befehl
        // aus der Anmeldung darf die Antenne anfassen; sie gehoert der
        // Maschine (CLAUDE.md, Radio-Authoritative Settings Policy).
        for (const QString& befehl : b) {
            QVERIFY2(!befehl.contains(QStringLiteral("antenna=")),
                     qPrintable(QStringLiteral("schaltet das Antennenrelais: ")
                                + befehl));
            QVERIFY2(!befehl.contains(QStringLiteral("rxant=")),
                     qPrintable(QStringLiteral("schaltet das Antennenrelais: ")
                                + befehl));
        }

        // Die Reihenfolge, an der AetherSDR einmal haengen geblieben
        // ist: Programm VOR GUI, GUI VOR allem Weiteren.
        const int iProgramm = 0;   // b.at(0), oben geprueft
        const int iGui      = 1;   // b.at(1), oben geprueft
        QVERIFY(iProgramm < iGui);
        QVERIFY(iGui < b.indexOf(QStringLiteral("client station Longpath")));
        QVERIFY(iGui < b.indexOf(QStringLiteral("sub slice all")));
        QVERIFY(iGui < b.indexOf(QStringLiteral("slice list")));
        // Und nachgesehen wird, bevor etwas angelegt wird.
        QVERIFY(b.indexOf(QStringLiteral("slice list"))
                < b.indexOf(QStringLiteral("display panafall create x=100 y=100")));
    }

    void vorhandeneScheibeWirdNichtVerdoppelt()
    {
        // Die Maschine hat uns Scheiben zurueckgelegt (sie kennt unsere
        // Kennung). Dann wird NICHTS angelegt — sonst entsteht genau der
        // Doppelfehler, den AetherSDRs SliceRecreatePolicy verhindert.
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client gui"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|0,1"));

        FlexRadioConnection conn;
        conn.setGuiClientId(QStringLiteral("ABCDEF01-2345-6789-ABCD-EF0123456789"));
        QSignalSpy listeGelesen(&conn, &FlexRadioConnection::sliceListRead);
        QSignalSpy angelegt(&conn, &FlexRadioConnection::defaultSliceCreated);

        conn.connectToRadio(QHostAddress::LocalHost, radio.port());
        QVERIFY(listeGelesen.wait(5000));

        QCOMPARE(listeGelesen.at(0).at(0).value<QList<int>>(), QList<int>({0, 1}));
        QTest::qWait(300);
        QCOMPARE(angelegt.count(), 0);
        QVERIFY(!radio.empfangeneBefehle.contains(
            QStringLiteral("display panafall create x=100 y=100")));
    }

    void abgelehnteAnmeldungLegtNichtsAn()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client gui"),
                               QStringLiteral("50000015|client gui refused"));

        FlexRadioConnection conn;
        conn.setGuiClientId(QStringLiteral("ABCDEF01-2345-6789-ABCD-EF0123456789"));
        QSignalSpy abgelehnt(&conn, &FlexRadioConnection::guiClientRejected);
        QSignalSpy angemeldet(&conn, &FlexRadioConnection::guiClientRegistered);

        conn.connectToRadio(QHostAddress::LocalHost, radio.port());
        QVERIFY(abgelehnt.wait(5000));

        QCOMPARE(abgelehnt.at(0).at(0).toUInt(), 0x50000015u);
        QCOMPARE(angemeldet.count(), 0);
        QTest::qWait(300);
        // Nach einer Ablehnung wird nichts abonniert und nichts angelegt.
        QVERIFY(!radio.empfangeneBefehle.contains(QStringLiteral("sub slice all")));
        QVERIFY(!radio.empfangeneBefehle.contains(QStringLiteral("slice list")));
    }

    void ohneErlaubnisWirdNurNachgesehen()
    {
        // setMayCreateDefaultSlice(false) heisst: nachsehen ja, auf der
        // Maschine etwas aendern nein.
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client gui"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|"));

        FlexRadioConnection conn;
        conn.setGuiClientId(QStringLiteral("ABCDEF01-2345-6789-ABCD-EF0123456789"));
        conn.setMayCreateDefaultSlice(false);
        QSignalSpy listeGelesen(&conn, &FlexRadioConnection::sliceListRead);

        conn.connectToRadio(QHostAddress::LocalHost, radio.port());
        QVERIFY(listeGelesen.wait(5000));
        QTest::qWait(300);

        QVERIFY(!radio.empfangeneBefehle.contains(
            QStringLiteral("display panafall create x=100 y=100")));
        QVERIFY(radio.empfangeneBefehle.contains(QStringLiteral("slice list")));
    }

    void ohneGesetzteKennungWirdEineErzeugtUndGemeldet()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client gui"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|0"));

        FlexRadioConnection conn;   // keine Kennung gesetzt
        QSignalSpy kennung(&conn, &FlexRadioConnection::guiClientIdChosen);
        QSignalSpy listeGelesen(&conn, &FlexRadioConnection::sliceListRead);

        conn.connectToRadio(QHostAddress::LocalHost, radio.port());
        QVERIFY(listeGelesen.wait(5000));

        QCOMPARE(kennung.count(), 1);
        const QString erzeugt = kennung.at(0).at(0).toString();
        // Form wie bei AetherSDR: UUID ohne Klammern, gross.
        QCOMPARE(erzeugt.size(), 36);
        QVERIFY(!erzeugt.startsWith(QLatin1Char('{')));
        QCOMPARE(erzeugt, erzeugt.toUpper());
        QVERIFY(radio.empfangeneBefehle.contains(
            QStringLiteral("client gui %1").arg(erzeugt)));
    }

    void panafallFaelltAufPanadapterCreateZurueck()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client gui"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("display panafall create"),
                               QStringLiteral("50001000|"));
        radio.antworten.insert(QStringLiteral("panadapter create"),
                               QStringLiteral("0|0x40000009"));
        radio.antworten.insert(QStringLiteral("slice create"), QStringLiteral("0|0"));

        FlexRadioConnection conn;
        conn.setGuiClientId(QStringLiteral("ABCDEF01-2345-6789-ABCD-EF0123456789"));
        QSignalSpy angelegt(&conn, &FlexRadioConnection::defaultSliceCreated);

        conn.connectToRadio(QHostAddress::LocalHost, radio.port());
        QVERIFY(angelegt.wait(5000));

        QCOMPARE(angelegt.at(0).at(0).toString(), QStringLiteral("40000009"));
        QVERIFY(radio.empfangeneBefehle.contains(QStringLiteral("panadapter create")));
    }

    void misslungenesAnlegenMeldetDenCode()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client gui"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("display panafall create"),
                               QStringLiteral("0|0x40000000,0x42000000"));
        radio.antworten.insert(QStringLiteral("slice create"),
                               QStringLiteral("50000039|slice limit reached"));

        FlexRadioConnection conn;
        conn.setGuiClientId(QStringLiteral("ABCDEF01-2345-6789-ABCD-EF0123456789"));
        QSignalSpy misslungen(&conn, &FlexRadioConnection::defaultSliceFailed);

        conn.connectToRadio(QHostAddress::LocalHost, radio.port());
        QVERIFY(misslungen.wait(5000));
        QCOMPARE(misslungen.at(0).at(0).toUInt(), 0x50000039u);
    }

    // ─── Die Verdrahtung: RadioModel oeffnet den Kanal ─────────────

    void radioModelOeffnetDenFlexKanal()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client program"),
                               QStringLiteral("10000002|unknown client program Longpath"));
        radio.antworten.insert(QStringLiteral("client gui"),
                               QStringLiteral("0|11112222-3333-4444-5555-666677778888"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("display panafall create"),
                               QStringLiteral("0|0x40000000,0x42000000"));
        radio.antworten.insert(QStringLiteral("slice create"), QStringLiteral("0|0"));
        radio.nachschub.insert(QStringLiteral("slice create"),
            {QStringLiteral("S5081495E|slice 0 in_use=1 RF_frequency=14.074000 mode=DIGU")});

        RadioModel model;
        QVERIFY(model.flexConnection() == nullptr);   // erst beim Verbinden

        QSignalSpy angemeldet(&model, &RadioModel::flexGuiClientRegistered);
        QSignalSpy frequenz(&model, &RadioModel::flexSliceFrequencyChanged);
        QSignalSpy zustand(&model, &RadioModel::connectionStateChanged);

        RadioInfo info;
        info.protocol   = ProtocolVersion::Flex;
        info.boardType  = HPSDRHW::Flex8400;
        info.name       = QStringLiteral("FLEX-8400 (Pruefstand)");
        info.macAddress = QStringLiteral("00:1C:2D:05:34:28");
        info.address    = QHostAddress(QHostAddress::LocalHost);
        info.port       = radio.port();

        model.connectToRadio(info);
        QVERIFY(model.flexConnection() != nullptr);
        QVERIFY(frequenz.wait(8000));

        QCOMPARE(angemeldet.count(), 1);
        QCOMPARE(frequenz.at(0).at(0).toInt(), 0);
        QCOMPARE(frequenz.at(0).at(1).toDouble(), 14'074'000.0);
        QCOMPARE(model.connectionState(), ConnectionState::Connected);

        // Die Kennung, die die MASCHINE zurueckgemeldet hat, ist
        // behalten — sonst legt die naechste Verbindung wieder eine
        // Vorgabescheibe auf dem Geraet an.
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("FlexGuiClientId")).toString(),
                 QStringLiteral("11112222-3333-4444-5555-666677778888"));

        // Und beim Trennen wird der Kanal zugemacht: ein GUI-Client,
        // der nach „Disconnect" angemeldet bleibt, haelt die Maschine
        // weiter besetzt.
        model.disconnectFromRadio();
        QTRY_COMPARE(model.connectionState(), ConnectionState::Disconnected);
    }

    // ─── Die Maschine stellt eine Lauschscheibe wieder her ────────
    //
    // Am Geraet, 2026-11-19 14:06, nach einem Neustart der Flex: beim
    // Anmelden kamen unter unserer Kennung ZWEI Scheiben,
    //   slice 0 RF=14.074 tx=1   und   slice 1 RF=21.074 tx=0 audio_mute=1
    // (die Lauschscheibe einer frueheren Sitzung). Longpath hielt die
    // zweite fuer die eigene: „Flex-Spur: Scheibe 0 14.225 -> 21.074,
    // von der Maschine", Panadapter auf 21,074 -- die Sendescheibe stand
    // auf 14,074. Hier dieselben Zeilen, vor UND nach der Anmeldung.
    void uebrigeLauschscheibeZiehtDenVfoNicht_data()
    {
        QTest::addColumn<bool>("uebrigeZuerst");
        QTest::newRow("Hauptscheibe zuerst (wie am Geraet)") << false;
        QTest::newRow("uebrige zuerst") << true;
    }

    void uebrigeLauschscheibeZiehtDenVfoNicht()
    {
        QFETCH(bool, uebrigeZuerst);
        const QString haupt = QStringLiteral(
            "S5081495E|slice 0 in_use=1 RF_frequency=14.074000 mode=DIGU tx=1 "
            "audio_level=50 audio_mute=0 client_handle=0x5081495E");
        const QString uebrig = QStringLiteral(
            "S5081495E|slice 1 in_use=1 RF_frequency=21.074000 mode=DIGU tx=0 "
            "audio_level=50 audio_mute=1 client_handle=0x5081495E");
        const QStringList zustand = uebrigeZuerst ? QStringList{uebrig, haupt}
                                                  : QStringList{haupt, uebrig};

        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client program"),
                               QStringLiteral("10000002|unknown client program Longpath"));
        radio.antworten.insert(QStringLiteral("client gui"),
                               QStringLiteral("0|11112222-3333-4444-5555-666677778888"));
        radio.vorschub.insert(QStringLiteral("client gui"), zustand);
        // Wie am Geraet nach dem Neustart: die Liste antwortet LEER,
        // obwohl beide Scheiben schon gemeldet sind.
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("stream create"), QStringLiteral("0|4000008"));
        radio.nachschub.insert(QStringLiteral("stream create"), zustand);

        RadioModel model;
        QSignalSpy frequenz(&model, &RadioModel::flexSliceFrequencyChanged);

        RadioInfo info;
        info.protocol   = ProtocolVersion::Flex;
        info.boardType  = HPSDRHW::Flex8400;
        info.name       = QStringLiteral("FLEX-8400 (Pruefstand)");
        info.macAddress = QStringLiteral("00:1C:2D:05:34:28");
        info.address    = QHostAddress(QHostAddress::LocalHost);
        info.port       = radio.port();
        model.connectToRadio(info);

        // Die uebrige wird entfernt, sobald die Hauptscheibe feststeht.
        QTRY_VERIFY_WITH_TIMEOUT(
            radio.empfangeneBefehle.contains(QStringLiteral("slice remove 1")), 8000);
        QVERIFY(!frequenz.isEmpty());
        // Am Ende steht der VFO auf der Sendescheibe ...
        QCOMPARE(frequenz.last().at(1).toDouble(), 14'074'000.0);
        if (!uebrigeZuerst) {
            // ... und in der Reihenfolge vom Geraet nie, auch nicht kurz,
            // auf dem Band der uebrigen.
            for (const auto& a : frequenz) {
                QVERIFY2(a.at(1).toDouble() != 21'074'000.0,
                         "die uebrige Lauschscheibe hat den VFO gezogen");
            }
        }
        // Kein Rueckfall-Panadapter: die Scheiben sind ja gemeldet.
        QVERIFY2(!radio.empfangeneBefehle.join('\n')
                     .contains(QStringLiteral("display panafall create")),
                 qPrintable(radio.empfangeneBefehle.join('\n')));
        // Und an Scheibe 1 geht ausser dem Entfernen nichts.
        for (const QString& b : radio.empfangeneBefehle) {
            QVERIFY2(!b.startsWith(QStringLiteral("slice set 1"))
                         && !b.startsWith(QStringLiteral("slice tune 1"))
                         && !b.startsWith(QStringLiteral("filt 1")),
                     qPrintable(b));
        }
        model.disconnectFromRadio();
        QTRY_COMPARE(model.connectionState(), ConnectionState::Disconnected);
    }

    void gespeicherteKennungWirdWiederverwendet()
    {
        AppSettings::instance().setValue(
            QStringLiteral("FlexGuiClientId"),
            QStringLiteral("AAAABBBB-CCCC-DDDD-EEEE-FFFF00001111"));

        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client gui"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|0"));

        RadioModel model;
        QSignalSpy angemeldet(&model, &RadioModel::flexGuiClientRegistered);

        RadioInfo info;
        info.protocol = ProtocolVersion::Flex;
        info.address  = QHostAddress(QHostAddress::LocalHost);
        info.port     = radio.port();
        model.connectToRadio(info);
        QVERIFY(angemeldet.wait(8000));

        QVERIFY(radio.empfangeneBefehle.contains(QStringLiteral(
            "client gui AAAABBBB-CCCC-DDDD-EEEE-FFFF00001111")));
        model.disconnectFromRadio();
    }

    void einFlexGehtNichtDurchDenOpenHpsdrWeg()
    {
        // Der Abzweig muss VOR der ganzen OpenHPSDR-Maschinerie sitzen:
        // HardwareProfile, Alex-Wege, OC-Matrix und Platinenfaehigkeiten
        // gelten fuer ein Flex nicht. Belegt daran, dass kein
        // RadioConnection entsteht.
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client gui"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|0"));

        RadioModel model;
        RadioInfo info;
        info.protocol = ProtocolVersion::Flex;
        info.address  = QHostAddress(QHostAddress::LocalHost);
        info.port     = radio.port();
        model.connectToRadio(info);

        QSignalSpy angemeldet(&model, &RadioModel::flexGuiClientRegistered);
        QVERIFY(angemeldet.wait(8000) || model.flexConnection()->isConnected());
        QVERIFY(model.connection() == nullptr);
        model.disconnectFromRadio();
    }

    // Am 2026-10-18 13:40 an der echten FLEX-8400 gemessen: die
    // Maschine legt Panadapter und Scheibe SELBST an, sobald sich ein
    // GUI-Client anmeldet — und schiebt den Scheibenzustand VOR der
    // Antwort auf `client gui`. `slice list` meldet dann 1, nicht 0,
    // und der Anlegeweg darf nicht greifen.
    //
    // Das ist an dieser Maschine der NORMALFALL, nicht der Randfall.
    void echteMaschineLegtSelbstAnUndSchiebtVorDerAntwort()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client program"),
                               QStringLiteral("10000002|unknown client program Longpath"));
        radio.antworten.insert(QStringLiteral("client gui"), QStringLiteral("0|"));
        // Die Scheibe ist schon da, wenn `slice list` gefragt wird.
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|0"));
        // …und ihr Zustand kommt VOR der Anmeldeantwort.
        radio.vorschub.insert(QStringLiteral("client gui"), {
            QStringLiteral("S57468A0B|display pan 0x40000000 center=14.100000 "
                           "bandwidth=0.200000 waterfall=0x42000000"),
            QStringLiteral("S57468A0B|slice 0 in_use=1 RF_frequency=14.100000 "
                           "mode=USB rxant=ANT1 ant_list=ANT1,ANT2,RX_A,XVTA")});

        FlexRadioConnection conn;
        conn.setGuiClientId(QStringLiteral("ABCDEF01-2345-6789-ABCD-EF0123456789"));

        QSignalSpy frequenz(&conn, &FlexRadioConnection::sliceFrequencyChanged);
        QSignalSpy angemeldet(&conn, &FlexRadioConnection::guiClientRegistered);
        QSignalSpy listeGelesen(&conn, &FlexRadioConnection::sliceListRead);
        QSignalSpy angelegt(&conn, &FlexRadioConnection::defaultSliceCreated);

        conn.connectToRadio(QHostAddress::LocalHost, radio.port());
        QVERIFY(listeGelesen.wait(5000));

        // Die Frequenz war VOR der Anmeldung da — der Hoerer haengt
        // seit dem Erzeugen dran, nicht erst seit der Anmeldung. Haette
        // man ihn erst nach `guiClientRegistered` verdrahtet, waere sie
        // verloren.
        QCOMPARE(frequenz.count(), 1);
        QCOMPARE(frequenz.at(0).at(1).toDouble(), 14'100'000.0);
        QCOMPARE(angemeldet.count(), 1);

        // Eine Scheibe ist da ⇒ nichts anlegen.
        QCOMPARE(listeGelesen.at(0).at(0).value<QList<int>>(), QList<int>({0}));
        QTest::qWait(300);
        QCOMPARE(angelegt.count(), 0);
        QVERIFY(!radio.empfangeneBefehle.contains(
            QStringLiteral("display panafall create x=100 y=100")));
        QVERIFY(radio.empfangeneBefehle.filter(QStringLiteral("slice create")).isEmpty());
    }

    void rueckrufLaeuftGenauEinmal()
    {
        FalscheMaschine radio;
        QVERIFY(radio.hoeren());
        radio.antworten.insert(QStringLiteral("client gui"), QStringLiteral("0|"));
        radio.antworten.insert(QStringLiteral("slice list"), QStringLiteral("0|0"));

        FlexRadioConnection conn;
        conn.setGuiClientId(QStringLiteral("ABCDEF01-2345-6789-ABCD-EF0123456789"));
        QSignalSpy listeGelesen(&conn, &FlexRadioConnection::sliceListRead);
        conn.connectToRadio(QHostAddress::LocalHost, radio.port());
        QVERIFY(listeGelesen.wait(5000));

        int laeufe = 0;
        const quint32 seq = conn.sendCommand(
            QStringLiteral("info"),
            [&laeufe](quint32, const QString&) { ++laeufe; });
        QVERIFY(seq != 0);
        QTest::qWait(400);
        QCOMPARE(laeufe, 1);

        // Genau DIESE Folgenummer ein zweites Mal: der Rueckruf ist
        // entnommen, bevor er laeuft, also laeuft er nicht wieder.
        conn.ingestLineForTesting(QStringLiteral("R%1|0|").arg(seq));
        QTest::qWait(100);
        QCOMPARE(laeufe, 1);
    }
};

QTEST_MAIN(TestFlexStufe1)
#include "tst_flex_stufe1.moc"
