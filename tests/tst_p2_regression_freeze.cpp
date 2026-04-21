// no-port-check: test fixture loads pre-refactor baseline JSON
//
// Phase 3P-B Task 7: P2 regression-freeze gate.
//
// Loads tests/data/p2_baseline_bytes.json (captured at Task 1, snapshots
// the pre-refactor P2RadioConnection compose output for every captured
// (board × MOX × rxCount × rxFreq) tuple) and asserts the post-refactor
// codec output matches byte-for-byte for every captured byte region.
//
// Saturn rows where p2SaturnBpf1Edges are empty (the default) MUST also
// match — Saturn falls through to OrionMkII's Alex defaults when BPF1
// isn't configured, so byte parity is preserved.

#include <QtTest/QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/P2RadioConnection.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestP2RegressionFreeze : public QObject {
    Q_OBJECT

private:
    void compareRegions(const QJsonArray& regions, const quint8* actual,
                        const QJsonObject& row, const char* packetName) {
        for (const QJsonValue& rv : regions) {
            QJsonObject region = rv.toObject();
            const int start = region["start"].toInt();
            const int end   = region["end"].toInt();
            const QJsonArray expected = region["bytes"].toArray();
            for (int i = start; i < end; ++i) {
                const int expByte = expected[i - start].toInt();
                if (int(actual[i]) != expByte) {
                    QFAIL(qPrintable(QString(
                        "regression %1[%2]: board=%3 mox=%4 rxCount=%5 rxFreq=%6 "
                        "expected=0x%7 actual=0x%8")
                        .arg(packetName).arg(i)
                        .arg(row["board"].toInt())
                        .arg(row["mox"].toBool() ? "true" : "false")
                        .arg(row["rxCount"].toInt())
                        .arg(qint64(row["rxFreqHz"].toDouble()))
                        .arg(expByte, 2, 16, QChar('0'))
                        .arg(int(actual[i]), 2, 16, QChar('0'))));
                }
            }
        }
    }

private slots:
    void allBoards_byteIdenticalToBaseline() {
        const QString path = QStringLiteral(NEREUS_TEST_DATA_DIR) + "/p2_baseline_bytes.json";
        QFile f(path);
        QVERIFY2(f.open(QIODevice::ReadOnly),
                 qPrintable(QString("baseline JSON not found at: %1").arg(path)));
        QJsonArray rows = QJsonDocument::fromJson(f.readAll()).array();
        QVERIFY2(rows.size() > 0, "baseline JSON is empty");

        int compared = 0;
        for (const QJsonValue& v : rows) {
            QJsonObject row = v.toObject();
            const auto board    = HPSDRHW(row["board"].toInt());
            const bool mox      = row["mox"].toBool();
            const int  rxCount  = row["rxCount"].toInt();
            const quint64 freq  = quint64(row["rxFreqHz"].toDouble());

            P2RadioConnection conn(nullptr);
            conn.setBoardForTest(board);
            conn.setActiveReceiverCount(rxCount);
            conn.setReceiverFrequency(0, freq);
            conn.setMox(mox);

            quint8 cmdGeneral[60]   = {};
            quint8 cmdHighPri[1444] = {};
            quint8 cmdRx[1444]      = {};
            quint8 cmdTx[60]        = {};
            conn.composeCmdGeneralForTest(cmdGeneral);
            conn.composeCmdHighPriorityForTest(cmdHighPri);
            conn.composeCmdRxForTest(cmdRx);
            conn.composeCmdTxForTest(cmdTx);

            compareRegions(row["cmdGeneral"].toArray(),      cmdGeneral, row, "cmdGeneral");
            compareRegions(row["cmdHighPriority"].toArray(), cmdHighPri, row, "cmdHighPriority");
            compareRegions(row["cmdRx"].toArray(),           cmdRx,      row, "cmdRx");
            compareRegions(row["cmdTx"].toArray(),           cmdTx,      row, "cmdTx");

            ++compared;
        }
        qInfo() << "P2 regression-freeze: compared" << compared << "tuples";
        QVERIFY2(compared > 30, "expected > 30 tuples — baseline may be corrupt");
    }
};

QTEST_APPLESS_MAIN(TestP2RegressionFreeze)
#include "tst_p2_regression_freeze.moc"
