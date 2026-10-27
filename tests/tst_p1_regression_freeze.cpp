// no-port-check: test fixture loads pre-refactor baseline JSON, no Thetis port
//
// Phase 3P-A Task 16: regression-freeze gate.
//
// Loads tests/data/p1_baseline_bytes.json (captured at Task 1, snapshots
// the pre-refactor P1RadioConnection::composeCcForBank output for every
// (board × MOX × bank) tuple) and asserts the post-refactor codec output
// matches byte-for-byte for every NON-HL2 board.
//
// HL2 rows are skipped intentionally — that's the bug fix (HL2 codec
// emits different bytes than legacy did, by design).
//
// Bank 10 byte 2 (C2) of every non-HL2 row reads 0x41, not the captured
// 0x40: bit 0 is mic_boost (Thetis ChannelMaster/networkproto1.c:581
// [v2.10.3.15]), and the connection's mic boost starts on as Thetis's does
// (console.cs:13259 [v2.10.3.15]: private bool mic_boost = true;). Only that
// byte of those rows was changed in the JSON.
//
// Banks 12, 13 and 14 of every non-HL2 row carry the CW keyer's defaults,
// since the codecs now send the CW settings at all times as Thetis does
// (ChannelMaster/networkproto1.c:618-646 [v2.10.3.15], R-CW-11, R-CW-14):
// bank 12 C3 0x99 (speed 25 | iambic B 0x80) and C4 0x32 (weight 50), bank
// 13 C3 0x07 (RF delay 7, CW enable 0 and level 0 unchanged), bank 14 C1
// 0x4D, C2 0x02 (hang 310) and C3 0x25, C4 0x08 (sidetone 600 Hz). These
// are the CodecContext::cw defaults. Only those 112 bytes of those 48 rows
// were changed in the JSON (2026-10-26); the HL2 rows the test skips are
// left as captured, and the attenuator bytes are unchanged.
//
// This is THE proof that Hermes/Orion/Angelia/etc ship byte-identical
// to pre-refactor main. If this test fails, the cutover regressed
// something — investigate before merging.
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the bank 12-14 CW defaults note (R-CW-14).
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

#include <QtTest/QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/P1RadioConnection.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestP1RegressionFreeze : public QObject {
    Q_OBJECT
private slots:
    void allBoardsExceptHl2_byteIdenticalToBaseline() {
        const QString path = QStringLiteral(NEREUS_TEST_DATA_DIR) + "/p1_baseline_bytes.json";
        QFile f(path);
        QVERIFY2(f.open(QIODevice::ReadOnly),
                 qPrintable(QString("baseline JSON not found at: %1").arg(path)));
        QJsonArray rows = QJsonDocument::fromJson(f.readAll()).array();
        QVERIFY2(rows.size() > 0, "baseline JSON is empty");

        int compared = 0;
        int hl2Skipped = 0;
        for (const QJsonValue& v : rows) {
            QJsonObject row = v.toObject();
            const auto board = HPSDRHW(row["board"].toInt());
            const bool mox   = row["mox"].toBool();
            const int  bank  = row["bank"].toInt();

            // HL2 deliberately diverges — that's the bug fix. Skip its rows.
            if (board == HPSDRHW::HermesLite) {
                ++hl2Skipped;
                continue;
            }

            P1RadioConnection conn(nullptr);
            conn.setBoardForTest(board);
            conn.setMox(mox);
            quint8 actual[5] = {};
            conn.composeCcForBankForTest(bank, actual);

            QJsonArray expected = row["bytes"].toArray();
            for (int i = 0; i < 5; ++i) {
                if (int(actual[i]) != expected[i].toInt()) {
                    QFAIL(qPrintable(QString(
                        "regression on board=%1 mox=%2 bank=%3 byte=%4 "
                        "expected=0x%5 actual=0x%6")
                        .arg(int(board))
                        .arg(mox ? "true" : "false")
                        .arg(bank)
                        .arg(i)
                        .arg(expected[i].toInt(), 2, 16, QChar('0'))
                        .arg(int(actual[i]),       2, 16, QChar('0'))));
                }
            }
            ++compared;
        }
        qInfo() << "regression-freeze: compared" << compared
                << "non-HL2 tuples;" << hl2Skipped << "HL2 tuples skipped (bug fix).";
        QVERIFY2(compared > 200, "expected > 200 non-HL2 tuples — baseline may be corrupt");
    }
};

QTEST_APPLESS_MAIN(TestP1RegressionFreeze)
#include "tst_p1_regression_freeze.moc"
