// =================================================================
// src/core/FlexRadioConnection.cpp  (Longpath)
// =================================================================
//
// Herkunft, Lizenz und der Grund, warum `client gui` hier fehlt:
// siehe FlexRadioConnection.h.
// Ported from AetherSDR src/core/CommandParser.cpp,
// src/core/RadioConnection.cpp, src/models/RadioModel.cpp and
// src/core/backends/flex/FlexBackend.cpp at d58e2b8a (GPLv3, primary
// author Jeremy [KK7GWY]) — project-level citation per
// docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include "FlexRadioConnection.h"

#include <QLoggingCategory>
#include <QStringList>
#include <QUuid>

namespace Longpath {

Q_LOGGING_CATEGORY(lcFlexCmd, "longpath.flex.cmd", QtInfoMsg)

namespace {

// From AetherSDR RadioConnection.cpp:58 [@d58e2b8a] — kMaxReadBuffer.
// Eine Maschine, die 16 MB ohne Zeilenende schickt, redet nicht dieses
// Protokoll; dann wird aufgelegt statt weiter gesammelt.
constexpr int kMaxReadBuffer = 16 * 1024 * 1024;

}  // namespace

// ─── FlexCommandWire ─────────────────────────────────────────────────

namespace FlexCommandWire {

// From AetherSDR CommandParser.cpp:9-22 [@d58e2b8a] — parseKVs().
QMap<QString, QString> parseKVs(const QString& body)
{
    QMap<QString, QString> result;
    // Rumpf sieht ueblicherweise so aus:
    //   "freq=14.225000 mode=USB filter_lo=-1500 filter_hi=1500"
    //
    // Es gibt aber einen zweiten Trenner. Gemessen am 2026-10-21 nach
    // `sub meter all`:
    //
    //   meter 1.src=COD-#1.num=1#1.nam=MICPEAK#1.low=-150.0#1.hi=20.0#
    //   1.desc=Signal strength of MIC output in CODEC#1.unit=dBFS#
    //
    // Hier trennt '#', und ein Wert (`desc`) enthaelt LEERZEICHEN. An
    // Leerzeichen zerlegt zerfiel die Zeile in Unsinn -- aus der
    // Beschreibung wurden die nackten Woerter `MIC=`, `in=`, `of=`,
    // `output=`, `strength=`, und `desc` trug nur das erste Wort.
    //
    // Die Unterscheidung ist der Trenner selbst: wo '#' vorkommt, ist
    // es das Trennzeichen. Ein Leerzeichen-Rumpf enthaelt keins.
    const QChar trenner = body.contains(QLatin1Char('#'))
                              ? QLatin1Char('#')
                              : QLatin1Char(' ');
    for (const QString& token : body.split(trenner, Qt::SkipEmptyParts)) {
        const int eq = token.indexOf('=');
        if (eq < 0) {
            // nacktes Wort — mit leerem Wert ablegen, damit der Leser
            // seine Anwesenheit erkennen kann
            result.insert(token, QString{});
        } else {
            result.insert(token.left(eq), token.mid(eq + 1));
        }
    }
    return result;
}

// From AetherSDR CommandParser.cpp:34-129 [@d58e2b8a] — parseLine().
//
// Satzbau (ASCII, je Zeile mit \n):
//   V3.3.28.0                         → Fassung
//   H0A1B2C3D                         → Clientkennung, hex
//   R1|0|                             → Antwort auf Befehl 1, Code 0 (gut)
//   R2|50001001|No Such Object        → Antwort mit Fehler
//   S0A1B2C3D|slice 0 freq=14.225000  → Zustandsschub einer Scheibe
//   M0A1B2C3D|...                     → Meldung
FlexLine parseLine(const QString& rawLine)
{
    FlexLine msg;
    msg.raw = rawLine.trimmed();

    if (msg.raw.isEmpty()) {
        return msg;
    }

    const QChar   tag  = msg.raw[0];
    const QString body = msg.raw.mid(1);   // alles nach dem Satzzeichen

    switch (tag.toLatin1()) {
    case 'V':
        msg.type   = FlexLineType::Version;
        msg.object = body;   // Fassungszeichenkette
        break;

    case 'H':
        msg.type   = FlexLineType::Handle;
        msg.handle = body.toUInt(nullptr, 16);
        break;

    case 'R': {
        // R<seq>|<code>|<rumpf>
        msg.type = FlexLineType::Response;
        const QStringList parts = body.split('|');
        if (parts.size() >= 1) {
            msg.sequence = parts[0].toUInt();
        }
        if (parts.size() >= 2) {
            // SmartSDR-Ergebniscodes sind vorzeichenlose 32-Bit-Werte.
            // Schwere Codes wie F3000001 haben das oberste Bit gesetzt;
            // QString::toInt() lehnt sie als Ueberlauf ab und gibt
            // null — damit wuerde eine schwere Ablehnung wie Erfolg
            // aussehen. (AetherSDR schreibt genau diesen Hinweis an
            // dieselbe Stelle, CommandParser.cpp:63-70.) Longpath
            // fuehrt den Code deshalb von Anfang an als quint32.
            bool ok = false;
            const quint32 resultCode = parts[1].toUInt(&ok, 16);
            if (ok) {
                msg.resultCode = resultCode;
            }
        }
        if (parts.size() >= 3) {
            msg.object = parts[2];
            msg.kvs    = parseKVs(parts[2]);
        }
        break;
    }

    case 'S': {
        // S<handle>|<objektname> [schluessel=wert ...]
        //
        // Objektnamen koennen mehrteilig sein, z. B. "slice 0" oder
        // "display pan 0x40000000". Schluesselpaare enthalten immer ein
        // '=', Objektwoerter nie. Die Grenze ist also das letzte
        // Leerzeichen VOR dem ersten '='.
        msg.type = FlexLineType::Status;
        const int pipe = body.indexOf('|');
        if (pipe < 0) {
            break;
        }
        msg.handle = body.left(pipe).toUInt(nullptr, 16);
        const QString statusBody = body.mid(pipe + 1);

        const int firstEq = statusBody.indexOf('=');
        if (firstEq < 0) {
            // keine Schluesselpaare — der ganze Rumpf ist der Objektname
            msg.object = statusBody.trimmed();
        } else {
            const int kvStart = statusBody.lastIndexOf(' ', firstEq);
            if (kvStart < 0) {
                // Rumpf beginnt direkt mit einem Schluesselpaar
                msg.kvs = parseKVs(statusBody);
            } else {
                msg.object = statusBody.left(kvStart).trimmed();
                msg.kvs    = parseKVs(statusBody.mid(kvStart + 1));
            }
        }
        break;
    }

    case 'M': {
        msg.type = FlexLineType::Message;
        const int pipe = body.indexOf('|');
        if (pipe >= 0) {
            // Die Zahl vor dem '|' ist hex. Bit 24-25 tragen die
            // Schwere (Info=0, Warning=1, Error=2, Fatal=3) per FlexLib
            // Radio.cs:4498-4516; der Rest ist eine undurchsichtige
            // Meldungsnummer. Sie liegt im selben Feld wie das Handle,
            // damit die Satzstruktur nicht je Satzart anders aussieht.
            msg.handle   = body.left(pipe).toUInt(nullptr, 16);
            msg.severity = static_cast<FlexSeverity>((msg.handle >> 24) & 0x3);
            msg.object   = body.mid(pipe + 1);
        }
        break;
    }

    default:
        msg.type = FlexLineType::Unknown;
        break;
    }

    return msg;
}

// From AetherSDR CommandParser.cpp:133-136 [@d58e2b8a] — buildCommand().
QByteArray buildCommand(quint32 seq, const QString& command)
{
    return QStringLiteral("C%1|%2\n").arg(seq).arg(command).toUtf8();
}

// From AetherSDR RadioStatusOwnership.h:27-41 [@d58e2b8a] —
// normalizedFlexId().
QString tuneSliceCommand(const QString& sliceId, double hz)
{
    if (sliceId.isEmpty() || !(hz > 0.0)) {
        return QString();
    }
    return QStringLiteral("slice tune %1 %2 autopan=0")
        .arg(sliceId)
        .arg(hz / 1.0e6, 0, 'f', 6);
}

QString setSliceModeCommand(const QString& sliceId, const QString& mode)
{
    if (sliceId.isEmpty()) { return QString(); }
    // Die Liste der Maschine, nicht unsere. Ein Name, den sie nicht
    // kennt, wird abgewiesen und laesst die Scheibe im alten Zustand —
    // also gar nicht erst schicken.
    static const QStringList kKennt = {
        QStringLiteral("LSB"),  QStringLiteral("USB"),
        QStringLiteral("AM"),   QStringLiteral("CW"),
        QStringLiteral("DIGL"), QStringLiteral("DIGU"),
        QStringLiteral("SAM"),  QStringLiteral("FM"),
        QStringLiteral("NFM"),  QStringLiteral("DFM"),
        QStringLiteral("RTTY"),
    };
    const QString gross = mode.trimmed().toUpper();
    if (!kKennt.contains(gross)) { return QString(); }
    return QStringLiteral("slice set %1 mode=%2").arg(sliceId, gross);
}

QString setSliceFilterCommand(const QString& sliceId, int lowHz, int highHz)
{
    if (sliceId.isEmpty() || highHz <= lowHz) { return QString(); }
    return QStringLiteral("filt %1 %2 %3")
        .arg(sliceId).arg(lowHz).arg(highHz);
}

// Die fuenf Minderer der Maschine. Nur diese Namen -- ein
// erfundener Schluessel waere eine Ablehnung je Klick.
QString setSliceNrKindCommand(const QString& sliceId, const QString& schluessel,
                              bool an)
{
    static const QStringList kBekannt{
        QStringLiteral("nr"), QStringLiteral("lms_nr"),
        QStringLiteral("speex_nr"), QStringLiteral("nrf"),
        QStringLiteral("rnnoise")};
    if (sliceId.isEmpty() || !kBekannt.contains(schluessel)) { return QString(); }
    return QStringLiteral("slice set %1 %2=%3")
        .arg(sliceId, schluessel).arg(an ? 1 : 0);
}

QString setSliceNrLevelCommand(const QString& sliceId, const QString& schluessel,
                               int stufe)
{
    // RNNoise hat keinen Pegel -- es ist ein Netz, kein Regler.
    static const QStringList kMitPegel{
        QStringLiteral("nr"), QStringLiteral("lms_nr"),
        QStringLiteral("speex_nr"), QStringLiteral("nrf")};
    if (sliceId.isEmpty() || !kMitPegel.contains(schluessel)) { return QString(); }
    if (stufe < 0 || stufe > 100) { return QString(); }
    return QStringLiteral("slice set %1 %2_level=%3")
        .arg(sliceId, schluessel).arg(stufe);
}

QString setSliceAnfLevelCommand(const QString& sliceId, int stufe)
{
    if (sliceId.isEmpty()) { return QString(); }
    if (stufe < 0 || stufe > 100) { return QString(); }
    return QStringLiteral("slice set %1 anf_level=%2").arg(sliceId).arg(stufe);
}

QString setSliceNoiseCommand(const QString& sliceId,
                             bool nr, bool nb, bool anf)
{
    if (sliceId.isEmpty()) { return QString(); }
    // Ein Befehl für alle drei Schalter. Die Maschine nimmt mehrere
    // Schlüssel in einem `slice set` an und antwortet mit einem
    // Zustandsschub, der alle drei zurückmeldet — damit kommt der
    // Rückweg von selbst.
    return QStringLiteral("slice set %1 nr=%2 nb=%3 anf=%4")
        .arg(sliceId)
        .arg(nr ? 1 : 0)
        .arg(nb ? 1 : 0)
        .arg(anf ? 1 : 0);
}

QString setPanCenterCommand(const QString& panId, double hz)
{
    if (panId.isEmpty() || !(hz > 0.0)) { return QString(); }
    return QStringLiteral("display pan set %1 center=%2")
        .arg(panId).arg(hz / 1.0e6, 0, 'f', 6);
}

QString setWaterfallSetupCommand(const QString& waterfallId, bool rueckfall)
{
    if (waterfallId.isEmpty()) { return QString(); }
    return QStringLiteral("display %1 set %2 "
                          "auto_black=0 black_level=0 color_gain=50 "
                          "line_duration=100")
        .arg(rueckfall ? QStringLiteral("waterfall")
                       : QStringLiteral("panafall"),
             waterfallId);
}

QString normalizedFlexId(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return QString();
    }

    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        text = text.mid(2);
    }

    bool ok = false;
    const quint32 id = text.toUInt(&ok, 16);
    if (!ok) {
        return QString();
    }
    return QString::number(id, 16).rightJustified(8, QLatin1Char('0'));
}

// From AetherSDR RadioStatusOwnership.h:48-60 [@d58e2b8a] —
// parsePanafallCreatePanId().
// ── Senden (2026-10-23, Stufe 2) ─────────────────────────────────────

QString xmitCommand(bool tasten)
{
    return QStringLiteral("xmit %1").arg(tasten ? 1 : 0);
}

// ── Die Rauschsperre der Maschine (2026-10-31) ──────────────────────
//
// Betreiber: „wo kann ich den squelch einstellen" -- und auf die
// Antwort („RX-Applet, Knopf SQL"): „der funktioniert nicht",
// „schieberegler ohne änderung".
//
// Richtig. Unsere Rauschsperre schreibt in `RxChannel` (WDSP), und
// `MainWindow_Flex.cpp:45` sagt es selbst: „Flex-Scheibe ist eine
// Scheibe OHNE RxChannel". Der Regler hatte also schlicht kein Ziel.
//
// Die Maschine hat eine eigene. Am 2026-10-31 direkt von ihr gelesen
// (`sub slice all`, nur lesend):
//
//     squelch=0 squelch_level=20
//
// Beide Namen sind damit vom Geraet bestaetigt, nicht geraten. Die
// Stufe laeuft wie unser Schieber von 0 bis 100.
QString setSliceSquelchCommand(const QString& sliceId, bool an)
{
    if (sliceId.isEmpty()) { return QString(); }
    return QStringLiteral("slice set %1 squelch=%2").arg(sliceId).arg(an ? 1 : 0);
}

QString setSliceSquelchLevelCommand(const QString& sliceId, int stufe)
{
    if (sliceId.isEmpty() || stufe < 0 || stufe > 100) { return QString(); }
    return QStringLiteral("slice set %1 squelch_level=%2").arg(sliceId).arg(stufe);
}

// ── Die uebrigen Regler der Scheibe (2026-11-01) ───────────────────
//
// Alle Schluessel am Geraet gelesen, nicht geraten: eine eigene
// Scheibe angelegt (`slice create freq=7.1 ant=ANT1 mode=LSB`), ihre
// 101 Zustandsschluessel mitgeschrieben, Scheibe wieder entfernt.
// Kein Senden, kein Tasten.
//
// Der leere Rueckgabewert heiszt immer dasselbe: diesen Befehl gibt
// es so nicht, also geht nichts hinaus. Ein Befehl an niemanden
// waere schlimmer als keiner.
QString setSliceAgcModeCommand(const QString& sliceId, const QString& modus)
{
    if (sliceId.isEmpty() || modus.isEmpty()) { return QString(); }
    // Die Maschine fuehrt genau diese vier. Unser "Long" ist keiner
    // davon -- wer es trotzdem schickte, bekaeme eine Ablehnung und
    // die Scheibe bliebe auf dem alten Modus stehen.
    static const QStringList kBekannt{QStringLiteral("off"), QStringLiteral("slow"),
                                      QStringLiteral("med"), QStringLiteral("fast")};
    if (!kBekannt.contains(modus)) { return QString(); }
    return QStringLiteral("slice set %1 agc_mode=%2").arg(sliceId, modus);
}

QString setSliceAgcThresholdCommand(const QString& sliceId, int stufe)
{
    if (sliceId.isEmpty() || stufe < 0 || stufe > 100) { return QString(); }
    return QStringLiteral("slice set %1 agc_threshold=%2").arg(sliceId).arg(stufe);
}

QString setSliceAudioLevelCommand(const QString& sliceId, int stufe)
{
    if (sliceId.isEmpty() || stufe < 0 || stufe > 100) { return QString(); }
    return QStringLiteral("slice set %1 audio_level=%2").arg(sliceId).arg(stufe);
}

QString setSliceAudioMuteCommand(const QString& sliceId, bool stumm)
{
    if (sliceId.isEmpty()) { return QString(); }
    return QStringLiteral("slice set %1 audio_mute=%2").arg(sliceId).arg(stumm ? 1 : 0);
}

QString setSliceAudioPanCommand(const QString& sliceId, int links0bis100)
{
    if (sliceId.isEmpty() || links0bis100 < 0 || links0bis100 > 100) {
        return QString();
    }
    return QStringLiteral("slice set %1 audio_pan=%2").arg(sliceId).arg(links0bis100);
}

QString setSliceRitCommand(const QString& sliceId, bool an, int hz)
{
    if (sliceId.isEmpty()) { return QString(); }
    return QStringLiteral("slice set %1 rit_on=%2 rit_freq=%3")
        .arg(sliceId).arg(an ? 1 : 0).arg(hz);
}

QString setSliceXitCommand(const QString& sliceId, bool an, int hz)
{
    if (sliceId.isEmpty()) { return QString(); }
    return QStringLiteral("slice set %1 xit_on=%2 xit_freq=%3")
        .arg(sliceId).arg(an ? 1 : 0).arg(hz);
}

QString setSliceApfCommand(const QString& sliceId, bool an)
{
    if (sliceId.isEmpty()) { return QString(); }
    return QStringLiteral("slice set %1 apf=%2").arg(sliceId).arg(an ? 1 : 0);
}

QString setSliceTxCommand(const QString& sliceId, bool sendet)
{
    if (sliceId.isEmpty()) { return QString(); }
    return QStringLiteral("slice set %1 tx=%2").arg(sliceId).arg(sendet ? 1 : 0);
}

QString setRfPowerCommand(int prozent)
{
    if (prozent < 0 || prozent > 100) { return QString(); }
    return QStringLiteral("transmit set rfpower=%1").arg(prozent);
}

QString setTunePowerCommand(int prozent)
{
    if (prozent < 0 || prozent > 100) { return QString(); }
    return QStringLiteral("transmit set tunepower=%1").arg(prozent);
}

QString tuneCommand(bool an)
{
    return QStringLiteral("transmit tune %1").arg(an ? 1 : 0);
}

QString setDaxRouteCommand(bool ueberDax)
{
    return QStringLiteral("transmit set dax=%1").arg(ueberDax ? 1 : 0);
}

QString apdEnableCommand(bool an)
{
    return QStringLiteral("apd enable=%1").arg(an ? 1 : 0);
}

QString apdSamplerCommand(const QString& sendeantenne, const QString& messpunkt)
{
    // Ein halber Befehl ist schlimmer als keiner: die Maschine
    // beantwortet `apd sampler tx_ant= sample_port=` mit Code 0 und
    // aendert nichts — das sieht wie Erfolg aus und ist keiner.
    const QString ant = sendeantenne.trimmed();
    const QString punkt = messpunkt.trimmed();
    if (ant.isEmpty() || punkt.isEmpty()) { return QString(); }
    return QStringLiteral("apd sampler tx_ant=%1 sample_port=%2").arg(ant, punkt);
}

QString apdResetCommand()
{
    return QStringLiteral("apd reset");
}

QString setTuneModeCommand(bool zweiton)
{
    return QStringLiteral("transmit set tune_mode=%1")
        .arg(zweiton ? QStringLiteral("two_tone") : QStringLiteral("single_tone"));
}

QString createDaxTxStreamCommand()
{
    return QStringLiteral("stream create type=dax_tx");
}

quint32 streamIdFromCreateReply(const QString& body)
{
    auto lies = [](QString text) -> quint32 {
        text = text.trimmed();
        if (text.isEmpty()) { return 0; }
        bool ok = false;
        const quint32 id = text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)
            ? text.mid(2).toUInt(&ok, 16)
            : text.toUInt(&ok, 16);
        return ok ? id : 0;
    };

    const QMap<QString, QString> kvs = parseKVs(body);
    for (const QString& name : { QStringLiteral("stream_id"),
                                 QStringLiteral("stream"),
                                 QStringLiteral("id") }) {
        if (kvs.contains(name)) {
            const quint32 id = lies(kvs.value(name));
            if (id != 0) { return id; }
        }
    }

    // Nackt, als erstes Glied einer Kommaliste — dieselbe Form, in der
    // auch `display panafall create` antwortet.
    const QStringList parts = body.trimmed().split(QLatin1Char(','));
    return parts.isEmpty() ? 0 : lies(parts.constFirst());
}

QString panIdFromCreateReply(const QString& body)
{
    const QMap<QString, QString> kvs = parseKVs(body);
    if (kvs.contains(QStringLiteral("pan"))) {
        return normalizedFlexId(kvs.value(QStringLiteral("pan")));
    }
    if (kvs.contains(QStringLiteral("id"))) {
        return normalizedFlexId(kvs.value(QStringLiteral("id")));
    }

    const QStringList parts = body.trimmed().split(QLatin1Char(','));
    return parts.isEmpty() ? QString() : normalizedFlexId(parts.constFirst());
}

QList<int> sliceListFromReply(const QString& body)
{
    QList<int> ids;
    for (const QString& token : body.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int id = token.trimmed().toInt(&ok);
        if (ok) {
            ids.append(id);
        }
    }
    return ids;
}

int sliceIdFromObject(const QString& object)
{
    const QStringList parts = object.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 2 || parts[0] != QLatin1String("slice")) {
        return -1;
    }
    bool ok = false;
    const int id = parts[1].toInt(&ok);
    return ok ? id : -1;
}

// From AetherSDR FlexBackend.cpp:742 [@d58e2b8a] —
// carry(kvs, "RF_frequency", d.frequency), mit der dort begruendeten
// „nur wenn gemeldet, nur wenn lesbar"-Regel
// (FlexKvCarry.h parseReal, FlexBackend.cpp:731-736).
//
// Der Wert kommt in MHz ueber den Draht („RF_frequency=14.074000").
std::optional<double> frequencyHzFromSliceKvs(const QMap<QString, QString>& kvs)
{
    static const QString kKey = QStringLiteral("RF_frequency");
    if (!kvs.contains(kKey)) {
        return std::nullopt;
    }
    bool ok = false;
    const double mhz = kvs.value(kKey).toDouble(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return mhz * 1'000'000.0;
}

}  // namespace FlexCommandWire

// ─── FlexRadioConnection ─────────────────────────────────────────────

FlexRadioConnection::FlexRadioConnection(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected,
            this, &FlexRadioConnection::onSocketConnected);
    connect(&m_socket, &QTcpSocket::disconnected,
            this, &FlexRadioConnection::onSocketDisconnected);
    connect(&m_socket, &QTcpSocket::errorOccurred,
            this, &FlexRadioConnection::onSocketErrorOccurred);
    connect(&m_socket, &QTcpSocket::readyRead,
            this, &FlexRadioConnection::onReadyRead);

    m_connectTimer.setSingleShot(true);
    m_connectTimer.setInterval(kConnectTimeoutMs);
    connect(&m_connectTimer, &QTimer::timeout,
            this, &FlexRadioConnection::onConnectTimeout);
}

FlexRadioConnection::~FlexRadioConnection()
{
    m_socket.abort();
}

void FlexRadioConnection::connectToRadio(const QHostAddress& address, quint16 port)
{
    if (m_state != State::Disconnected && m_state != State::Error) {
        return;
    }

    m_readBuffer.clear();
    m_handle  = 0;
    m_nextSeq = 1;
    m_version.clear();
    m_sliceFreqHz.clear();
    m_pending.clear();
    m_guiRegistered = false;

    // Ein Socket, der im Fehler steht oder noch auf einen Handschlag
    // wartet, nimmt kein connectToHost() mehr an und meldet dazu
    // nichts. Am 2026-10-28 blieben zwei Wiederholversuche genau
    // deshalb stumm — kein „TCP steht", kein Fehler, nichts.
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
    }

    m_targetAddress = address;
    m_targetPort    = port;

    qCInfo(lcFlexCmd) << "verbinde nach" << address.toString() << ":" << port;
    setState(State::Connecting);
    m_connectTimer.start();
    m_socket.connectToHost(address, port);
}

void FlexRadioConnection::disconnectFromRadio()
{
    if (m_socket.state() == QAbstractSocket::UnconnectedState) {
        setState(State::Disconnected);
        return;
    }
    // Stufe 1 haelt keine Stroeme und keine Scheiben — es gibt nichts
    // abzuraeumen. AetherSDRs gracefulDisconnect (RadioConnection.cpp:295
    // [@d58e2b8a]) nimmt vorher Stroeme zurueck; das gehoert in die
    // Stufe, die Stroeme oeffnet.
    m_socket.disconnectFromHost();
}

void FlexRadioConnection::setGuiClientId(const QString& id)
{
    m_guiClientId = id.trimmed();
}

quint32 FlexRadioConnection::sendCommand(const QString& command,
                                         ResponseCallback onResponse)
{
    const quint32 seq = sendCommand(command);
    if (seq != 0 && onResponse) {
        m_pending.insert(seq, std::move(onResponse));
    }
    return seq;
}

quint32 FlexRadioConnection::sendCommand(const QString& command)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        qCWarning(lcFlexCmd) << "Befehl verworfen, keine Verbindung:" << command;
        return 0;
    }
    const quint32 seq = m_nextSeq++;
    const QByteArray wire = FlexCommandWire::buildCommand(seq, command);
    if (m_socket.write(wire) != wire.size()) {
        qCWarning(lcFlexCmd) << "Schreiben unvollstaendig:" << m_socket.errorString();
    }
    qCDebug(lcFlexCmd) << "TX:" << QStringLiteral("C%1|%2").arg(seq).arg(command);
    // Wortlaut merken, bis die Antwort da ist: eine Ablehnung soll
    // sagen koennen, WAS abgelehnt wurde.
    m_wortlaut.insert(seq, command);
    m_wortlautReihe.append(seq);
    while (m_wortlautReihe.size() > kWortlautDecke) {
        m_wortlaut.remove(m_wortlautReihe.takeFirst());
    }
    return seq;
}

void FlexRadioConnection::onSocketConnected()
{
    m_connectTimer.stop();
    m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    qCInfo(lcFlexCmd) << "TCP steht;" << "warte auf V/H";
    // Noch NICHT Connected: die Maschine schickt zuerst V (Fassung) und
    // H (Clientkennung). Erst mit dem Handle ist der Kanal brauchbar.
    // From AetherSDR RadioConnection.cpp:552-559 [@d58e2b8a] — dort
    // setzt genau der H-Satz den Zustand auf Connected.
}

void FlexRadioConnection::onSocketDisconnected()
{
    m_connectTimer.stop();
    qCInfo(lcFlexCmd) << "TCP getrennt";
    m_handle = 0;
    setState(State::Disconnected);
    emit disconnected();
}

// ── Diese Texte SIEHT der Betreiber (2026-11-01) ────────────────────
//
// Sie standen auf Deutsch, weil Fehlertexte unter src/core als
// Protokolltexte galten — `tst_oberflaeche_englisch` nimmt sie aus
// genau diesem Grund ausdruecklich aus („die liest der Betreiber,
// nicht der Benutzer").
//
// Das stimmt fuer diese hier nicht: `onSocketErrorOccurred` schickt
// den Text als `errorOccurred` weiter, und die Verbindungsmaske
// stellt ihn in ihre Zustandszeile. Am 2026-11-01 stand dort
// „Disconnected — Socketfehler ohne Zuordnung (errno unbekannt)
// (Unknown error, Nr. -1, Ziel 172.30.30.12:4992)" — deutscher Satz,
// englische Oberflaeche, und inhaltlich ein Schulterzucken.
//
// Also englisch wie der Rest der Oberflaeche, und der unbekannte Fall
// sagt, was man TUN kann.
QString FlexRadioConnection::socketErrorText(QAbstractSocket::SocketError error)
{
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        return QObject::tr("Connection refused — nothing is listening on that port");
    case QAbstractSocket::RemoteHostClosedError:
        return QObject::tr("The radio hung up");
    case QAbstractSocket::HostNotFoundError:
        return QObject::tr("Address not found");
    case QAbstractSocket::SocketTimeoutError:
        return QObject::tr("No answer — timed out");
    case QAbstractSocket::NetworkError:
        return QObject::tr("Network unreachable — cable, Wi-Fi or route");
    case QAbstractSocket::SocketAccessError:
        return QObject::tr("The operating system refused the socket");
    case QAbstractSocket::SocketResourceError:
        return QObject::tr("No socket resources left");
    case QAbstractSocket::AddressInUseError:
        return QObject::tr("Address already in use");
    case QAbstractSocket::SocketAddressNotAvailableError:
        return QObject::tr("That address does not exist on this machine");
    case QAbstractSocket::UnsupportedSocketOperationError:
        return QObject::tr("The system does not support this socket operation");
    case QAbstractSocket::UnfinishedSocketOperationError:
        return QObject::tr("A previous socket operation is still running");
    case QAbstractSocket::DatagramTooLargeError:
        return QObject::tr("Datagram too large");
    case QAbstractSocket::UnknownSocketError:
        // Qt kennt den errno nicht und schreibt „Unknown error". Mehr
        // ueber die URSACHE gibt es hier nicht — aber ueber den
        // Umgang damit schon: am 2026-11-01 war das Geraet in der
        // Erkennung sichtbar („just now"), und derselbe Anruf ging
        // Minuten spaeter durch. Bei einem Geraet am WLAN heisst
        // dieser Fehler fast immer: ein Paket ist unterwegs verloren
        // gegangen. Das gehoert in den Satz, sonst sucht der Betreiber
        // an der falschen Stelle.
        return QObject::tr("The connection attempt was lost and the system "
                           "gave no reason — try again");
    default:
        break;
    }
    return QObject::tr("Socket error %1").arg(static_cast<int>(error));
}

void FlexRadioConnection::onSocketErrorOccurred(QAbstractSocket::SocketError error)
{
    m_connectTimer.stop();

    // Drei Stuecke statt einem: was Qt sagt, was die Nummer bedeutet,
    // und wohin ueberhaupt gewaehlt wurde. Die Nummer selbst kommt mit,
    // damit ein Protokoll auch ohne diese Tabelle auswertbar bleibt.
    const QString ziel = m_targetPort != 0
        ? QStringLiteral("%1:%2").arg(m_targetAddress.toString()).arg(m_targetPort)
        : QStringLiteral("—");
    const QString msg = QStringLiteral("%1 (%2, no. %3, target %4)")
                            .arg(socketErrorText(error),
                                 m_socket.errorString(),
                                 QString::number(static_cast<int>(error)),
                                 ziel);

    qCWarning(lcFlexCmd) << "Socketfehler:" << msg
                         << "Socketzustand" << static_cast<int>(m_socket.state());
    setState(State::Error);
    emit errorOccurred(msg);
}

void FlexRadioConnection::onConnectTimeout()
{
    // Der Handschlag kam nicht zustande und der Socket schweigt. Ohne
    // diese Frist bleibt der Zustand fuer immer auf Connecting stehen
    // und das Verbindungsfenster zeigt gar nichts an.
    const QString ziel = QStringLiteral("%1:%2")
                             .arg(m_targetAddress.toString())
                             .arg(m_targetPort);
    const QString msg = QObject::tr("Keine Antwort von %1 nach %2 s")
                            .arg(ziel)
                            .arg(kConnectTimeoutMs / 1000);

    qCWarning(lcFlexCmd) << "Verbindungsfrist abgelaufen:" << msg
                         << "Socketzustand" << static_cast<int>(m_socket.state());
    m_socket.abort();
    setState(State::Error);
    emit errorOccurred(msg);
}

// From AetherSDR RadioConnection.cpp:512-530 [@d58e2b8a] — onReadyRead().
void FlexRadioConnection::onReadyRead()
{
    m_readBuffer.append(m_socket.readAll());
    if (m_readBuffer.size() > kMaxReadBuffer) {
        qCWarning(lcFlexCmd) << "Lesepuffer ueber" << kMaxReadBuffer
                             << "Byte ohne Zeilenende — lege auf";
        m_socket.disconnectFromHost();
        m_readBuffer.clear();
        return;
    }
    int newlinePos;
    while ((newlinePos = m_readBuffer.indexOf('\n')) >= 0) {
        const QString line =
            QString::fromUtf8(m_readBuffer.left(newlinePos)).trimmed();
        m_readBuffer.remove(0, newlinePos + 1);
        if (!line.isEmpty()) {
            processLine(line);
        }
    }
}

// From AetherSDR RadioConnection.cpp:536-585 [@d58e2b8a] — processLine().
void FlexRadioConnection::processLine(const QString& line)
{
    // GPS-Koordinaten gehoeren in kein Protokoll. AetherSDR haelt sie
    // an genau dieser Stelle aus dem Log heraus (RadioConnection.cpp:539
    // [@d58e2b8a]) — uebernommen, aus demselben Grund.
    const bool isGps = line.contains(QLatin1String("|gps "));
    if (!isGps) {
        qCDebug(lcFlexCmd) << "RX:" << line;
    }

    const FlexLine msg = FlexCommandWire::parseLine(line);
    emit lineReceived(msg);

    switch (msg.type) {
    case FlexLineType::Version:
        m_version = msg.object;
        qCInfo(lcFlexCmd) << "Maschine meldet Fassung" << m_version;
        emit versionReceived(m_version);
        break;

    case FlexLineType::Handle:
        m_handle = msg.handle;
        qCInfo(lcFlexCmd) << "Clientkennung"
                          << QStringLiteral("0x%1").arg(m_handle, 0, 16);
        setState(State::Connected);
        emit handleAssigned(m_handle);
        emit connected();
        sendRegistrationSequence();
        break;

    case FlexLineType::Response: {
        emit commandResponse(msg.sequence, msg.resultCode, msg.object);
        const QString wortlaut = m_wortlaut.value(msg.sequence);
        if (msg.resultCode != 0) {
            qCWarning(lcFlexCmd).nospace()
                << "Befehl " << msg.sequence << " abgelehnt, Code 0x"
                << QString::number(msg.resultCode, 16) << ": \""
                << (wortlaut.isEmpty() ? QStringLiteral("(Wortlaut nicht mehr da)")
                                       : wortlaut)
                << "\" — " << msg.object;
        }
        if (!wortlaut.isEmpty()) {
            m_wortlaut.remove(msg.sequence);
            m_wortlautReihe.removeOne(msg.sequence);
        }
        // Der Rueckruf wird VOR dem Aufruf entnommen: laeuft er genau
        // einmal, auch wenn er selbst wieder sendet und antwortet.
        const auto it = m_pending.find(msg.sequence);
        if (it != m_pending.end()) {
            const ResponseCallback cb = it.value();
            m_pending.erase(it);
            if (cb) {
                cb(msg.resultCode, msg.object);
            }
        }
        break;
    }

    case FlexLineType::Status: {
        emit statusReceived(msg.object, msg.kvs);

        const int sliceId = FlexCommandWire::sliceIdFromObject(msg.object);
        if (sliceId >= 0) {
            if (const auto hz = FlexCommandWire::frequencyHzFromSliceKvs(msg.kvs)) {
                // Nur melden, wenn sich wirklich etwas aendert: die
                // Maschine schiebt denselben Zustand mehrfach, und ein
                // unveraenderter Wert ist keine Nachricht.
                const auto it = m_sliceFreqHz.constFind(sliceId);
                if (it == m_sliceFreqHz.constEnd() || !qFuzzyCompare(it.value(), *hz)) {
                    m_sliceFreqHz[sliceId] = *hz;
                    qCInfo(lcFlexCmd) << "Scheibe" << sliceId << "auf"
                                      << QStringLiteral("%1 Hz").arg(*hz, 0, 'f', 0);
                    emit sliceFrequencyChanged(sliceId, *hz);
                }
            }
        }
        break;
    }

    case FlexLineType::Message:
        emit messageFromRadio(msg.severity, msg.object);
        if (msg.severity == FlexSeverity::Error || msg.severity == FlexSeverity::Fatal) {
            qCWarning(lcFlexCmd) << "Meldung der Maschine:" << msg.object;
        }
        break;

    case FlexLineType::Unknown:
        break;
    }
}

// From AetherSDR RadioModel.cpp:6523-6596 [@d58e2b8a] — die
// Anmeldefolge. Die Reihenfolge ist nicht beliebig, siehe unten.
void FlexRadioConnection::sendRegistrationSequence()
{
    // „client program <name>" zuerst — die Maschine will die Identitaet
    // des Clients, bevor sie Client-Einstellungen annimmt
    // (RadioModel.cpp:6522-6530 begruendet die Reihenfolge an einem
    // echten Fehler: AetherSDR-Fehlerbericht 2447, low_bw_connect wurde
    // still ignoriert, wenn es vor `client program` kam. Die Nummer hier
    // ohne Raute, weil `verify-style-drift.py` ein „#2447" als namenloses
    // Farbliteral zaehlt — vier Hexstellen hinter einer Raute).
    //
    // Die 8400 lehnt das mit `R|10000002|unknown client program Longpath`
    // ab (am 2026-10-18 gemessen) — sie prueft den Namen gegen eine ihr
    // bekannte Liste. Das ist NICHT toedlich: alles danach wird
    // angenommen. Der Befehl bleibt, weil er die dokumentierte Folge ist;
    // einen fremden Programmnamen zu senden, um durchzukommen, waere
    // Impersonieren.
    sendCommand(QStringLiteral("client program Longpath"));

    // Die Kennung, unter der die Maschine unsere Scheiben und
    // Panadapter zurueckbehaelt. Keine gesetzt ⇒ eine erzeugen und
    // melden, damit der Aufrufer sie behalten kann — sonst legt jeder
    // Start eine neue Vorgabescheibe auf der Maschine an.
    // Form aus AppSettings.cpp:1256 [@d58e2b8a]: UUID ohne Klammern,
    // gross.
    if (m_guiClientId.isEmpty()) {
        m_guiClientId =
            QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
        qCInfo(lcFlexCmd) << "keine GUI-Kennung gesetzt, neue erzeugt";
        emit guiClientIdChosen(m_guiClientId);
    }

    sendCommand(QStringLiteral("client gui %1").arg(m_guiClientId),
                [this](quint32 code, const QString& body) {
        if (code != 0) {
            qCWarning(lcFlexCmd) << "GUI-Anmeldung abgelehnt, Code"
                                 << QStringLiteral("0x%1").arg(code, 0, 16)
                                 << body;
            emit guiClientRejected(code, body);
            return;
        }
        onGuiClientAccepted(body);
    });
}

void FlexRadioConnection::onGuiClientAccepted(const QString& body)
{
    m_guiRegistered = true;

    // From AetherSDR RadioModel.cpp:6570-6576 [@d58e2b8a]: die Maschine
    // antwortet mit der Kennung, unter der sie uns gefuehrt hat. Die
    // behalten — sie stellt bei der naechsten Verbindung Scheiben und
    // Frequenzen wieder her.
    const QString returned = body.trimmed();
    if (!returned.isEmpty() && returned != m_guiClientId) {
        m_guiClientId = returned;
        emit guiClientIdChosen(m_guiClientId);
    }

    qCInfo(lcFlexCmd) << "als GUI-Client angemeldet, Kennung" << m_guiClientId;
    emit guiClientRegistered(m_guiClientId);

    sendCommand(QStringLiteral("client station %1").arg(m_stationName));

    // Abonnements sind unabhaengige Themen, und die Maschine arbeitet
    // Befehle in Sendereihenfolge ab — sie koennen also hintereinander
    // raus, ohne auf je eine Antwort zu warten
    // (RadioModel.cpp:6588-6592 [@d58e2b8a] begruendet das: die alte
    // Kette kostete elf Umlaeufe, rund 0,7 s auf einem LAN, ohne
    // Protokollgrund). Stufe 1 braucht genau eines davon.
    sendCommand(QStringLiteral("sub slice all"));

    // Die Messwerte bestellen. Ohne das schickt die Maschine die
    // Klasse 0x8002 gar nicht — am 2026-10-21 gemessen: nach
    // `sub meter all` kamen 653 Pakete in 22 Sekunden, davor keines
    // (docs/protocols/flex-vita49-gemessen.md §5b). Die Antwort ist
    // je Messwert eine Statuszeile mit Name, Einheit und Bereich; sie
    // ist mit `#` getrennt statt mit Leerzeichen, was `parseKVs` seit
    // #297 beruecksichtigt.
    sendCommand(QStringLiteral("sub meter all"));

    // ── Ohne das kommt NIE eine Verriegelungsmeldung (2026-10-27) ───
    //
    // Die Maschine schickt den `interlock`-Zustand nur an Clients, die
    // die Sendeebene bestellt haben. Stufe 1 brauchte sie nicht und
    // bestellte sie nicht — mit Stufe 2 ist sie das TOR vor dem
    // Tasten, und ein Tor, das nie eine Freigabe hoert, bleibt fuer
    // immer zu.
    //
    // Genau daran scheiterte der erste Sendeversuch des Betreibers am
    // 2026-10-27: „TUNE ist aber nicht gegangen". `flexAbstimmen` und
    // `flexTasten` fragen `m_flexDarfSenden`, und das stand mangels
    // Meldung auf seinem Vorgabewert false. Ein stilles Nein, das in
    // keiner Zeile Code falsch aussieht.
    //
    // From AetherSDR RadioModel.cpp:6596 [@d58e2b8a] — `sub tx all`
    // steht dort in derselben Reihe wie `sub slice all` und
    // `sub meter all`.
    sendCommand(QStringLiteral("sub tx all"));

    // ── Die Vorverzerrung der Maschine bestellen (2026-11-01) ───────
    //
    // Ohne `sub apd all` kommt keine einzige `apd`-Zeile — am Geraet
    // gemessen: vor der Bestellung nichts, danach sofort der Schalter,
    // drei Messpunkt-Zeilen und die Scheibenzeile.
    //
    // Eine Maschine ohne Vorverzerrung (6000er-Reihe, aeltere
    // Firmware) antwortet mit Code 0 und schickt nichts. Das ist in
    // Ordnung: `FlexApdState::bekannt()` bleibt dann false, und die
    // Bedienung zeigt sich gar nicht erst.
    sendCommand(QStringLiteral("sub apd all"));

    queryAndRecoverSlices();
}

// From AetherSDR SliceRecreatePolicy.h [@d58e2b8a] — der dort
// beschriebene Fall „die Maschine meldet null Scheiben beim Anmelden".
// Bei AetherSDR ist das die Ausnahme (Sitzungswiederherstellung brachte
// den Panadapter ohne seine Scheibe zurueck, deren #3212); hier ist es
// der Normalfall, weil eine Maschine ohne GUI-Client ueberhaupt keine
// Scheiben hat.
void FlexRadioConnection::queryAndRecoverSlices()
{
    sendCommand(QStringLiteral("slice list"),
                [this](quint32 code, const QString& body) {
        if (code != 0) {
            qCWarning(lcFlexCmd) << "slice list abgelehnt, Code"
                                 << QStringLiteral("0x%1").arg(code, 0, 16);
            return;
        }

        const QList<int> ids = FlexCommandWire::sliceListFromReply(body);
        qCInfo(lcFlexCmd) << "Scheiben auf der Maschine:" << ids.size();
        emit sliceListRead(ids);

        if (!ids.isEmpty()) {
            // Die Maschine hat uns Scheiben zurueckgelegt. Ihre
            // Zustaende kommen ueber `sub slice all` von selbst; es
            // gibt nichts anzulegen. Eine zweite Vorgabescheibe hier
            // waere genau der Doppelfehler, den AetherSDRs
            // SliceRecreatePolicy verhindert.
            return;
        }

        if (!m_mayCreateSlice) {
            qCInfo(lcFlexCmd) << "keine Scheibe da, und Anlegen ist aus "
                                 "— es gibt nichts zu lesen";
            return;
        }
        // Die Liste kann leer sein, obwohl die Maschine schon Scheiben
        // GEMELDET hat: am 2026-11-19 nach ihrem Neustart kamen slice 0
        // und slice 1 im Zustandsschub, und `slice list` antwortete
        // leer. Der Rueckfall legte daraufhin einen zweiten Panadapter
        // an (x=100) -- der blieb auf der Maschine, belegte den zweiten
        // Platz und mischte seine 100 Bins ins Bild. Wer gemeldet ist,
        // ist da.
        if (!m_sliceFreqHz.isEmpty()) {
            qCInfo(lcFlexCmd) << "Liste leer, aber" << m_sliceFreqHz.size()
                              << "Scheibe(n) schon gemeldet — nichts angelegt";
            return;
        }
        createDefaultSlice();
    });
}

namespace {

// From AetherSDR SliceRecreatePolicy.h:52-56 [@d58e2b8a] — die
// Vorgaben der Decision-Struktur. 14,225 MHz liegt im 20-m-Band;
// Empfang, kein Senden. Stufe 1 hat keine gespeicherte „letzte
// Frequenz", die der Entscheider dort bevorzugen wuerde, also bleibt
// es bei der Vorgabe.
constexpr double kDefaultSliceFreqMhz = 14.225000;

}  // namespace

// From AetherSDR RadioModel::createPanadapter [@d58e2b8a] — eine
// Scheibe braucht einen Panadapter, auf dem sie sitzt. Der Panadapter
// entsteht auf der MASCHINE; Longpath liest seine Daten nicht (das
// waeren die Stroeme, und die sind Stufe 2).
void FlexRadioConnection::createDefaultSlice()
{
    sendCommand(QStringLiteral("display panafall create x=100 y=100"),
                [this](quint32 code, const QString& body) {
        if (code == 0) {
            const QString panId = FlexCommandWire::panIdFromCreateReply(body);
            if (panId.isEmpty()) {
                qCWarning(lcFlexCmd) << "panafall angelegt, aber keine Kennung "
                                        "in der Antwort:" << body;
                emit defaultSliceFailed(code, body);
                return;
            }
            createSliceOnPan(panId);
            return;
        }

        qCWarning(lcFlexCmd) << "display panafall create abgelehnt, Code"
                             << QStringLiteral("0x%1").arg(code, 0, 16)
                             << "— versuche panadapter create";
        sendCommand(QStringLiteral("panadapter create"),
                    [this](quint32 legacyCode, const QString& legacyBody) {
            if (legacyCode != 0) {
                emit defaultSliceFailed(legacyCode, legacyBody);
                return;
            }
            const QString panId = FlexCommandWire::panIdFromCreateReply(legacyBody);
            if (panId.isEmpty()) {
                emit defaultSliceFailed(legacyCode, legacyBody);
                return;
            }
            createSliceOnPan(panId);
        });
    });
}

// From AetherSDR RadioModel::createDefaultSliceOnPan [@d58e2b8a] —
// „slice create pan=<id> freq=<mhz> antenna=<ant> mode=<mode>".
void FlexRadioConnection::createSliceOnPan(const QString& panId)
{
    // 2026-10-21, an Martins FLEX-8400 gemessen und von ihm GEHOERT,
    // bevor es jemand gesucht hat: `antenna=ANT1` im Anlegen schaltet
    // das Antennenrelais, auch wenn dort schon ANT1 steht — bei jedem
    // Verbinden ein Klicken im Geraet. Die Antenne ist Sache der
    // Maschine (CLAUDE.md, Radio-Authoritative Settings Policy): wir
    // legen die Scheibe an und lassen das Relais, wo es steht. Die
    // Vorlage in AetherSDR setzt sie mit, deshalb stand sie hier.
    const QString cmd =
        QStringLiteral("slice create pan=%1 freq=%2 mode=USB")
            .arg(panId)
            .arg(kDefaultSliceFreqMhz, 0, 'f', 6);

    sendCommand(cmd, [this, panId](quint32 code, const QString& body) {
        if (code != 0) {
            qCWarning(lcFlexCmd) << "slice create abgelehnt fuer pan" << panId
                                 << "Code" << QStringLiteral("0x%1").arg(code, 0, 16)
                                 << body;
            emit defaultSliceFailed(code, body);
            return;
        }
        qCInfo(lcFlexCmd) << "Vorgabescheibe angelegt auf pan" << panId
                          << "— Nummer" << body.trimmed();
        // Die Maschine schickt jetzt von selbst „S|slice N …"; der
        // Zustandszweig oben liest die Frequenz daraus.
        emit defaultSliceCreated(panId);
    });
}

void FlexRadioConnection::setState(State s)
{
    if (m_state == s) {
        return;
    }
    m_state = s;
    emit stateChanged(s);
}

}  // namespace Longpath
