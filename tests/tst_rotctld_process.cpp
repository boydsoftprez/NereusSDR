// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - starting Hamlib's rotctld (rotor control plan, Task 3b).
//
// Adapted from Longpath tests/tst_rotctld_process.cpp [@551576e]
// (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath, GPLv3; no
// top-of-file GPL header upstream, the project-level LICENSE applies).
// The command-line and stderr cases are Longpath's, comments kept
// verbatim; its model-list cases are not taken (tst_rotor_route checks
// RotorModels.h). Added for NereusSDR: finding the binary, and starting,
// stopping and restarting a stand-in shell script in rotctld's place, so
// no test needs Hamlib installed or starts the real rotctld.
//
// Modification history (NereusSDR):
//   2026-10-08: Adapted by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//
// --- From tst_rotctld_process.cpp (Longpath) ---
//
// The rotctld command line, and the curated controller list.
//
// The command line is the whole of the "easy installation": get an
// argument wrong and the operator sees a rotator that will not connect,
// with the reason buried in a process they never asked to start. It is
// built by a static function so it can be checked without Hamlib being
// installed, which is also the state most CI machines are in.

#include <QtTest>

#include "core/RotctldProcess.h"
#include "OperatorWording.h"

#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>

using namespace NereusSDR;

class TstRotctldProcess : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();

    void a_serial_rotator_gets_model_device_and_speed();
    void an_empty_device_is_left_out_entirely();
    void it_listens_on_loopback_only();
    void der_grund_steht_nicht_in_der_letzten_zeile();

    void findBinaryTakesTheTestOverride();
    void missingBinaryRefusesWithTheDocumentReason();
    void startRunsTheBinaryWithTheArguments();
    void aHeldPortIsSidestepped();
    void stopIsNotReportedAsAnExit();
    void anExitOnItsOwnIsReportedWithStderr();
    void restartNeedsAnEarlierStart();

private:
    QString writeStandIn(QTemporaryDir& dir, const QString& body);
};

void TstRotctldProcess::initTestCase()
{
    // "started ..." is not test output; expected warnings are named with
    // QTest::ignoreMessage where they occur.
    QLoggingCategory::setFilterRules(QStringLiteral(
        "nereus.rotctld.debug=false\nnereus.rotctld.info=false"));
}

void TstRotctldProcess::cleanup()
{
    RotctldProcess::setBinaryOverrideForTesting(std::nullopt);
}

QString TstRotctldProcess::writeStandIn(QTemporaryDir& dir, const QString& body)
{
    const QString path = dir.filePath(QStringLiteral("rotctld"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    f.write(body.toUtf8());
    f.close();
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                     | QFileDevice::ExeOwner);
    return path;
}

// From Longpath tests/tst_rotctld_process.cpp:30-41 [@551576e]
void TstRotctldProcess::a_serial_rotator_gets_model_device_and_speed()
{
    const QStringList a = RotctldProcess::arguments(
        603, QStringLiteral("/dev/tty.usbserial-1410"), 9600, 4533);

    QCOMPARE(a, QStringList({
        QStringLiteral("-m"), QStringLiteral("603"),
        QStringLiteral("-r"), QStringLiteral("/dev/tty.usbserial-1410"),
        QStringLiteral("-s"), QStringLiteral("9600"),
        QStringLiteral("-T"), QStringLiteral("127.0.0.1"),
        QStringLiteral("-t"), QStringLiteral("4533")}));

    // NereusSDR: the Core on the Rock 5C, the ERC's own Hamlib driver.
    QCOMPARE(RotctldProcess::arguments(404, QStringLiteral("/dev/ttyUSB0"), 9600, 4533)
                 .join(QLatin1Char(' ')),
             QStringLiteral("-m 404 -r /dev/ttyUSB0 -s 9600 -T 127.0.0.1 -t 4533"));
}

// From Longpath tests/tst_rotctld_process.cpp:43-57 [@551576e]
void TstRotctldProcess::an_empty_device_is_left_out_entirely()
{
    // A bare "-r" with nothing after it makes rotctld take the next
    // flag as the device name, and it then fails complaining about a
    // serial port called "-T". Leaving the pair out lets Hamlib use its
    // own default, which is what a network model wants anyway.
    const QStringList a = RotctldProcess::arguments(1501, QString{}, 0, 4533);
    QVERIFY(!a.contains(QStringLiteral("-r")));
    QVERIFY(!a.contains(QStringLiteral("-s")));
    QVERIFY(a.contains(QStringLiteral("-m")));

    const QStringList blank =
        RotctldProcess::arguments(601, QStringLiteral("   "), 9600, 4533);
    QVERIFY(!blank.contains(QStringLiteral("-r")));
}

// From Longpath tests/tst_rotctld_process.cpp:59-72 [@551576e]
void TstRotctldProcess::it_listens_on_loopback_only()
{
    // rotctld has no authentication of any kind. Bound to 0.0.0.0 —
    // which is the example in most instructions — anyone on the network
    // can turn the mast. This is the one argument that must not drift.
    for (int model : {601, 603, 404, 403, 901}) {
        const QStringList a = RotctldProcess::arguments(
            model, QStringLiteral("/dev/ttyUSB0"), 9600, 4533);
        const int at = a.indexOf(QStringLiteral("-T"));
        QVERIFY2(at >= 0 && at + 1 < a.size(), "no listen address given");
        QCOMPARE(a.at(at + 1), QStringLiteral("127.0.0.1"));
        QVERIFY(!a.contains(QStringLiteral("0.0.0.0")));
    }
}

// From Longpath tests/tst_rotctld_process.cpp:130-178 [@551576e]
// Hamlibs stderr ist ein Ablaufprotokoll, und die letzte Zeile ist die
// FOLGE, nicht die Ursache. Am 2026-10-06 stand in Martins Protokoll
// unter "Rotator: rotctld exited" genau ein Wort: "IO error". Wer nichts
// erfaehrt, sucht beim Programm statt beim Kabel.
//
// Die Vorlage unten ist die echte Ausgabe aus jenem Protokoll, gekuerzt
// um die rot_register-Zeilen, die nichts sagen.
void TstRotctldProcess::der_grund_steht_nicht_in_der_letzten_zeile()
{
    const QString echt = QStringLiteral(
        "rot_open: error = rot_register (609)\n"
        "gs232a_rot_init called\n"
        "rot_open called\n"
        "rot_open: using network address 192.168.1.16:4001:TCP\n"
        "network_open: TCP connect\n"
        "network_open: hoststr=192.168.1.16, portstr=4001\n"
        "connect to 192.168.1.16:4001 failed, (trying next interface): "
        "Network error 60: Operation timed out\n"
        "network_open: failed to connect to 192.168.1.16:4001\n"
        "IO error\n");

    const QString grund = RotctldProcess::reasonFromStderr(echt);
    // Die Ursache, nicht die Folge -- und mit der Adresse daran, denn genau
    // die war hier falsch.
    QVERIFY2(grund.contains(QStringLiteral("timed out")), qPrintable(grund));
    QVERIFY2(grund.contains(QStringLiteral("192.168.1.16:4001")), qPrintable(grund));
    QVERIFY2(grund != QStringLiteral("IO error"), qPrintable(grund));

    // Zweite Stufe: nennt keine Zeile eine Ursache, nimmt er die letzte, die
    // ueberhaupt von einem Fehlschlag spricht.
    const QString ohneUrsache = QStringLiteral(
        "rot_open called\n"
        "network_open: failed to connect to 10.0.0.9:4533\n"
        "IO error\n");
    QCOMPARE(RotctldProcess::reasonFromStderr(ohneUrsache),
             QStringLiteral("network_open: failed to connect to 10.0.0.9:4533"));

    // Dritte Stufe: sagt gar nichts etwas aus, bleibt die letzte nichtleere
    // Zeile -- lieber wenig als nichts.
    QCOMPARE(RotctldProcess::reasonFromStderr(QStringLiteral("abc\ndef\n\n")),
             QStringLiteral("def"));
    QCOMPARE(RotctldProcess::reasonFromStderr(QString()), QString());

    // Ein belegter Port ist der zweite Fall, der Martin schon einmal
    // getroffen hat (2026-09-16, verwaister rotctld auf 4533).
    QVERIFY(RotctldProcess::reasonFromStderr(QStringLiteral(
                "bind: Address already in use\nIO error\n"))
                .contains(QStringLiteral("in use")));
}

// ── NereusSDR additions ──────────────────────────────────────────────

void TstRotctldProcess::findBinaryTakesTheTestOverride()
{
    RotctldProcess::setBinaryOverrideForTesting(QStringLiteral("/somewhere/rotctld"));
    QCOMPARE(RotctldProcess::findBinary(), QStringLiteral("/somewhere/rotctld"));
    RotctldProcess::setBinaryOverrideForTesting(QString());
    QCOMPARE(RotctldProcess::findBinary(), QString());

    // Without the override it searches: whatever it finds is executable.
    RotctldProcess::setBinaryOverrideForTesting(std::nullopt);
    const QString found = RotctldProcess::findBinary();
    if (!found.isEmpty()) {
        QVERIFY2(QFileInfo(found).isExecutable(), qPrintable(found));
    }
}

void TstRotctldProcess::missingBinaryRefusesWithTheDocumentReason()
{
    RotctldProcess::setBinaryOverrideForTesting(QString());
    RotctldProcess p;
    QString error;
    QVERIFY(!p.start(404, QStringLiteral("/dev/ttyUSB0"), 9600, 4533, &error));
    // The remote rotor control document's refusal, word for word.
    QCOMPARE(error, QStringLiteral("Hamlib's rotctld is not installed on the Core's computer."));
    QCOMPARE(RotctldProcess::notInstalledReason(), error);
    QVERIFY2(OperatorWording::isPlain(error), qPrintable(error));
    QVERIFY(!error.contains(QChar(0x2014)));
    QVERIFY(!p.isRunning());
}

void TstRotctldProcess::startRunsTheBinaryWithTheArguments()
{
#ifdef Q_OS_WIN
    QSKIP("The rotctld stand-in is a POSIX shell script.");
#endif
    QTemporaryDir dir;
    const QString out = dir.filePath(QStringLiteral("args"));
    const QString script = writeStandIn(dir, QStringLiteral(
        "#!/bin/sh\n"
        "trap 'echo stopped >> \"%1\"; exit 0' TERM\n"
        "echo \"$@\" > \"%1\"\n"
        "while :; do sleep 0.2 & wait $!; done\n").arg(out));
    RotctldProcess::setBinaryOverrideForTesting(script);

    RotctldProcess p;
    QString error;
    QVERIFY2(p.start(404, QStringLiteral("/dev/ttyUSB0"), 9600, 4533, &error),
             qPrintable(error));
    QVERIFY(p.isRunning());
    QVERIFY(p.listenPort() != 0);
    // A second start while running is a no-op.
    QVERIFY(p.start(404, QStringLiteral("/dev/ttyUSB0"), 9600, 4533, &error));

    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(out).size() > 0, 3000);
    QFile f(out);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(f.readAll()).trimmed(),
             QStringLiteral("-m 404 -r /dev/ttyUSB0 -s 9600 -T 127.0.0.1 -t %1")
                 .arg(p.listenPort()));
    f.close();

    p.stop();
    QVERIFY(!p.isRunning());
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(f.readAll()).contains(QStringLiteral("stopped")));
}

void TstRotctldProcess::aHeldPortIsSidestepped()
{
#ifdef Q_OS_WIN
    QSKIP("The rotctld stand-in is a POSIX shell script.");
#endif
    QTemporaryDir dir;
    const QString script = writeStandIn(dir, QStringLiteral(
        "#!/bin/sh\n"
        "trap 'exit 0' TERM\n"
        "while :; do sleep 0.2 & wait $!; done\n"));
    RotctldProcess::setBinaryOverrideForTesting(script);

    // Something (a leftover rotctld) holds the preferred port.
    QTcpServer holder;
    QVERIFY(holder.listen(QHostAddress::LocalHost, 0));
    const quint16 held = holder.serverPort();

    RotctldProcess p;
    QString error;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
        QStringLiteral("port %1 is already taken").arg(held)));
    QVERIFY2(p.start(603, QStringLiteral("/dev/ttyUSB0"), 9600, held, &error),
             qPrintable(error));
    QVERIFY(p.listenPort() != 0);
    QVERIFY(p.listenPort() != held);
    p.stop();
}

void TstRotctldProcess::stopIsNotReportedAsAnExit()
{
#ifdef Q_OS_WIN
    QSKIP("The rotctld stand-in is a POSIX shell script.");
#endif
    QTemporaryDir dir;
    const QString script = writeStandIn(dir, QStringLiteral(
        "#!/bin/sh\n"
        "trap 'exit 0' TERM\n"
        "while :; do sleep 0.2 & wait $!; done\n"));
    RotctldProcess::setBinaryOverrideForTesting(script);

    RotctldProcess p;
    QSignalSpy exited(&p, &RotctldProcess::exited);
    QString error;
    QVERIFY(p.start(404, QStringLiteral("/dev/ttyUSB0"), 9600, 4533, &error));
    p.stop();
    QVERIFY(!p.isRunning());
    QTest::qWait(100);
    QCOMPARE(exited.count(), 0);
}

void TstRotctldProcess::anExitOnItsOwnIsReportedWithStderr()
{
#ifdef Q_OS_WIN
    QSKIP("The rotctld stand-in is a POSIX shell script.");
#endif
    QTemporaryDir dir;
    const QString script = writeStandIn(dir, QStringLiteral(
        "#!/bin/sh\n"
        "echo 'rot_open: cannot open /dev/ttyUSB7: No such file or directory' >&2\n"
        "echo 'IO error' >&2\n"
        "exit 3\n"));
    RotctldProcess::setBinaryOverrideForTesting(script);

    RotctldProcess p;
    QSignalSpy exited(&p, &RotctldProcess::exited);
    QString error;
    const QRegularExpression exitedLine(QStringLiteral("rotctld exited on its own, code 3"));
    QTest::ignoreMessage(QtWarningMsg, exitedLine);
    QVERIFY(p.start(404, QStringLiteral("/dev/ttyUSB7"), 9600, 4533, &error));
    QTRY_COMPARE_WITH_TIMEOUT(exited.count(), 1, 5000);
    QCOMPARE(exited.at(0).at(0).toInt(), 3);
    const QString text = exited.at(0).at(1).toString();
    QVERIFY2(text.contains(QStringLiteral("No such file")), qPrintable(text));
    QVERIFY(RotctldProcess::reasonFromStderr(text).contains(QStringLiteral("/dev/ttyUSB7")));

    // restart() starts it again with what start() was given.
    QTest::ignoreMessage(QtWarningMsg, exitedLine);
    QVERIFY(p.restart(&error));
    QTRY_COMPARE_WITH_TIMEOUT(exited.count(), 2, 5000);
}

void TstRotctldProcess::restartNeedsAnEarlierStart()
{
    RotctldProcess p;
    QString error;
    QVERIFY(!p.restart(&error));
    QVERIFY2(OperatorWording::isPlain(error), qPrintable(error));
}

QTEST_GUILESS_MAIN(TstRotctldProcess)
#include "tst_rotctld_process.moc"
