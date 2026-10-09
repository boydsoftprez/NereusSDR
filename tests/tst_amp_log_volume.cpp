// =================================================================
// tests/tst_amp_log_volume.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No upstream port.
//
// The Core journal on the Rock 5C bench rotated every ~1.5 h at ~37 MB
// because the 4O3A amp paths logged every poll, reply and S-frame at
// info. Pins that repeats log at debug while state changes, operator
// commands and errors stay at info or above.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QTcpSocket>

#include "core/PgxlConnection.h"
#include "core/SmartSdrApiListener.h"
#include "core/TgxlConnection.h"

using NereusSDR::PgxlConnection;
using NereusSDR::SmartSdrApiListener;
using NereusSDR::TgxlConnection;

namespace {

struct Captured {
    QtMsgType type;
    QString   category;
    QString   text;
};

QList<Captured> g_messages;
QtMessageHandler g_previousHandler = nullptr;

void captureHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    const QString category = QString::fromLatin1(ctx.category ? ctx.category : "");
    if (category.startsWith(QLatin1String("nereus."))) {
        g_messages.append({type, category, msg});
    }
}

int countOf(const QString& category, QtMsgType type, const QString& needle)
{
    int n = 0;
    for (const Captured& m : g_messages) {
        if (m.category == category && m.type == type && m.text.contains(needle)) {
            ++n;
        }
    }
    return n;
}

template<typename Pred>
bool waitFor(Pred pred, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!pred() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return pred();
}

}  // namespace

class AmpLogVolumeTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "nereus.pgxl.debug=true\n"
            "nereus.tgxl.debug=true\n"
            "nereus.smartsdr.debug=true\n"));
        g_previousHandler = qInstallMessageHandler(captureHandler);
    }
    void cleanupTestCase() { qInstallMessageHandler(g_previousHandler); }
    void init() { g_messages.clear(); }

    void pgxlRepeatsLogAtDebug();
    void tgxlRepeatsLogAtDebug();
    void listenerRepeatsLogAtDebug();
};

void AmpLogVolumeTest::pgxlRepeatsLogAtDebug()
{
    const QString cat = QStringLiteral("nereus.pgxl");
    PgxlConnection conn;
    conn.injectLineForTesting(QStringLiteral("V3.8.9"));
    QVERIFY(conn.isConnected());

    // The handshake's `info` is a setup command (info); its `status` is
    // the poll (debug).
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("cmd: \"info\"")), 1);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("cmd: \"status\"")), 0);
    QCOMPARE(countOf(cat, QtDebugMsg, QStringLiteral("cmd: \"status\"")), 1);

    conn.sendCommand(QStringLiteral("status"));
    conn.ping(QStringLiteral("auto"));
    conn.sendCommand(QStringLiteral("operate=1"));
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("cmd: \"status\"")), 0);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("cmd: \"ping\"")), 0);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("cmd: \"operate=1\"")), 1);

    // Poll answers: success at debug, a refusal stays a warning.
    conn.injectLineForTesting(QStringLiteral("R2|0|state=IDLE temp=30.0"));
    conn.injectLineForTesting(QStringLiteral("R3|50000015|refused"));
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("RX R-frame")), 0);
    QCOMPARE(countOf(cat, QtDebugMsg, QStringLiteral("RX R-frame")), 1);
    QCOMPARE(countOf(cat, QtWarningMsg, QStringLiteral("RX R-frame")), 1);

    // S-frames: info on each state change, debug on each repeat.
    for (int i = 0; i < 3; ++i) {
        conn.injectLineForTesting(QStringLiteral("S0|state state=IDLE fwd=0.0"));
    }
    conn.injectLineForTesting(QStringLiteral("S0|state state=OPERATE fwd=0.0"));
    conn.injectLineForTesting(QStringLiteral("S0|state state=OPERATE fwd=1.0"));
    conn.injectLineForTesting(QStringLiteral("S0|state state=IDLE fwd=0.0"));
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("RX S-frame")), 3);
    QCOMPARE(countOf(cat, QtDebugMsg, QStringLiteral("RX S-frame")), 3);
}

void AmpLogVolumeTest::tgxlRepeatsLogAtDebug()
{
    const QString cat = QStringLiteral("nereus.tgxl");
    TgxlConnection conn;
    conn.injectLineForTesting(QStringLiteral("V1.2.3"));
    QVERIFY(conn.isConnected());

    conn.sendCommand(QStringLiteral("status"));
    conn.ping(QStringLiteral("auto"));
    conn.sendCommand(QStringLiteral("autotune"));
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("cmd: \"status\"")), 0);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("cmd: \"ping\"")), 0);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("cmd: \"autotune\"")), 1);

    conn.injectLineForTesting(QStringLiteral("R4|0|fwd=0.0 swr=1.0"));
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("RX R-frame")), 0);
    QCOMPARE(countOf(cat, QtDebugMsg, QStringLiteral("RX R-frame")), 1);

    // Each object is tracked on its own; a tuning sweep logs both edges.
    conn.injectLineForTesting(QStringLiteral("S5|status fwd=0.0 swr=1.0"));
    conn.injectLineForTesting(QStringLiteral("S6|status fwd=0.0 swr=1.0"));
    conn.injectLineForTesting(QStringLiteral("S0|state bypassA=0 tuning=0"));
    conn.injectLineForTesting(QStringLiteral("S0|state bypassA=0 tuning=0"));
    conn.injectLineForTesting(QStringLiteral("S0|state bypassA=0 tuning=1"));
    conn.injectLineForTesting(QStringLiteral("S0|state bypassA=0 tuning=1"));
    conn.injectLineForTesting(QStringLiteral("S0|state bypassA=0 tuning=0"));
    conn.injectLineForTesting(QStringLiteral("S7|status fwd=5.0 swr=1.2"));
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("object= \"status\"")), 1);
    QCOMPARE(countOf(cat, QtDebugMsg, QStringLiteral("object= \"status\"")), 2);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("object= \"state\"")), 3);
    QCOMPARE(countOf(cat, QtDebugMsg, QStringLiteral("object= \"state\"")), 2);
}

void AmpLogVolumeTest::listenerRepeatsLogAtDebug()
{
    const QString cat = QStringLiteral("nereus.smartsdr");
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, listener.serverPort());
    QVERIFY(client.waitForConnected(1000));

    // The first slice and transmit S-frames to a new client are news.
    QVERIFY(waitFor([&] {
        return countOf(cat, QtInfoMsg, QStringLiteral("TX S-frame")) >= 2;
    }, 1000));

    // Inbound commands log at debug.
    client.write("C1|ping\n");
    client.flush();
    QVERIFY(waitFor([&] {
        return countOf(cat, QtDebugMsg, QStringLiteral("RX from")) >= 1;
    }, 1000));
    QCOMPARE(countOf(cat, QtDebugMsg, QStringLiteral("RX raw from")), 1);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("RX raw from")), 0);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("RX from")), 0);

    // The 1 Hz push repeats unchanged bodies at debug.
    QVERIFY(waitFor([&] {
        return countOf(cat, QtDebugMsg, QStringLiteral("TX S-frame")) >= 2;
    }, 2500));
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("TX S-frame")), 2);

    // A frequency change is news again: both bodies carry the frequency.
    listener.setSliceFrequencyHz(0, 7074000);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("TX S-frame")), 4);
    QCOMPARE(countOf(cat, QtInfoMsg, QStringLiteral("7.074000")), 2);

    client.disconnectFromHost();
    listener.stop();
}

QTEST_GUILESS_MAIN(AmpLogVolumeTest)
#include "tst_amp_log_volume.moc"
