// Temporary helper — captures current P1RadioConnection::composeCcForBank
// output for every (bank × board × MOX) tuple to JSON. Run once on the
// pre-refactor codebase, commit the JSON, then this file is deleted.
//
// The shipping regression test (tst_p1_regression_freeze.cpp) loads the
// JSON and asserts the codec output matches for every non-HL2 board.
//
// NOTE: The plan template listed HPSDRHW::AnvelinaPro3, RedPitaya, AnanG2,
// AnanG2_1K — but those are HPSDRModel (logical) values, not HPSDRHW
// (physical board) values. The actual HPSDRHW enum has 9 distinct physical
// boards. AnvelinaPro3 and RedPitaya map to OrionMKII; AnanG2/AnanG2_1K
// map to Saturn. This capture uses the 9 real HPSDRHW values.

#include <QtTest/QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/P1RadioConnection.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class CaptureP1Baseline : public QObject {
    Q_OBJECT
private slots:
    void emitBaseline() {
        // All distinct physical HPSDRHW boards (9 total).
        // AnvelinaPro3 → OrionMKII, RedPitaya → OrionMKII, AnanG2/AnanG2_1K → Saturn.
        const QList<HPSDRHW> boards = {
            HPSDRHW::Atlas,
            HPSDRHW::Hermes,
            HPSDRHW::HermesII,
            HPSDRHW::Angelia,
            HPSDRHW::Orion,
            HPSDRHW::OrionMKII,
            HPSDRHW::HermesLite,
            HPSDRHW::Saturn,
            HPSDRHW::SaturnMKII,
        };

        QJsonArray rows;
        for (HPSDRHW board : boards) {
            P1RadioConnection conn(nullptr);
            conn.setBoardForTest(board);
            for (bool mox : {false, true}) {
                conn.setMox(mox);
                for (int bank = 0; bank <= 17; ++bank) {
                    quint8 out[5] = {};
                    conn.composeCcForBankForTest(bank, out);
                    QJsonObject row;
                    row["board"] = static_cast<int>(board);
                    row["mox"]   = mox;
                    row["bank"]  = bank;
                    QJsonArray bytes;
                    for (int i = 0; i < 5; ++i) { bytes.append(int(out[i])); }
                    row["bytes"] = bytes;
                    rows.append(row);
                }
            }
        }

        const QString path = QStringLiteral(NEREUS_TEST_DATA_DIR) + "/p1_baseline_bytes.json";
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(rows).toJson(QJsonDocument::Indented));
        qInfo() << "Wrote baseline:" << path << rows.size() << "rows";
    }
};

QTEST_APPLESS_MAIN(CaptureP1Baseline)
#include "tst_p1_regression_freeze_capture.moc"
