// =================================================================
// tests/tst_reported_receiver_pool.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The stream pool and ReceiverManager
// ceiling are NereusSDR constructs. The one upstream fact used here, that
// discovery byte 20 is the radio's receiver count, is cited to Thetis at
// RadioDiscovery.cpp's parse site.
//
// Bench (ANAN-G2): the Core logged "Cannot create receiver: at maximum 4"
// on every connect. The radio's discovery reply reports 4 receivers
// (byte 20), ReceiverManager was capped at that, and the stream pool was
// sized from the board table's five user streams (DDC2-6), so the fifth
// stream had no receiver and a pan landing on it stayed blank.
//
// The radio's reported count is the authority (CLAUDE.md, Radio-
// Authoritative Settings Policy; the gateware's receiver count is a
// compile-time constant that changes between firmware releases). On
// Protocol 2 the pool is min(board table, reported) when the radio reports
// a count, and the table value when it reports 0. Protocol 1 is unchanged.
//
// Modification history (NereusSDR):
//   2026-09-29 - Written by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
//   2026-09-29 - The radio's receiver count readers: Max RX on the radio
//                information tab (label and support text) and the live
//                receiver count clamp read the same effective count as the
//                stream pool, BoardCapsTable::effectiveReceiverCount. J.J.
//                Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
//   2026-09-29 - The HL2's receiver count is discovery byte 19 (mi0bot),
//                checked against the bench capture's reply. J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via Anthropic
//                Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QHostAddress>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "core/ReceiverManager.h"
#include "gui/setup/hardware/RadioInfoTab.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// A Protocol 2 discovery reply (layout: RadioDiscovery.cpp parseP2Reply).
QByteArray p2Reply(HPSDRHW board, int reportedReceivers)
{
    QByteArray b(60, '\0');
    b[4] = char(0x02);
    const char mac[6] = {char(0x00), char(0x1c), char(0xc0), char(0x77), char(0x88), char(0x99)};
    for (int i = 0; i < 6; ++i) { b[5 + i] = mac[i]; }
    b[11] = char(static_cast<int>(board));
    b[13] = char(26);
    b[20] = char(reportedReceivers);
    return b;
}

// A Protocol 1 discovery reply (layout: RadioDiscovery.cpp parseP1Reply).
QByteArray p1Reply(quint8 boardByte, int reportedReceivers)
{
    QByteArray b(60, '\0');
    b[0] = char(0xEF);
    b[1] = char(0xFE);
    b[2] = char(0x02);
    const char mac[6] = {char(0x00), char(0x1c), char(0xc0), char(0x11), char(0x22), char(0x33)};
    for (int i = 0; i < 6; ++i) { b[3 + i] = mac[i]; }
    b[9]  = char(72);
    b[10] = char(boardByte);
    b[20] = char(reportedReceivers);
    return b;
}

// The HL2's discovery reply as captured on the bench
// (docs/protocols/openhpsdr-protocol1-capture-reference.md section 2.2):
// byte 19 is 0x04, the HL2's receiver count, and byte 20 is 0x45.
QByteArray hl2CapturedReply()
{
    return QByteArray::fromHex(
        "effe02001cc0a213dd4a060000000000"
        "00000004450200000000000303ef0000"
        "00000000801646365e83000000000000"
        "000000000000000000000000");
}

RadioInfo parsedP2(HPSDRHW board, int reported)
{
    RadioInfo info;
    RadioDiscovery::parseP2Reply(p2Reply(board, reported),
                                 QHostAddress(QStringLiteral("192.168.1.20")), info);
    return info;
}

// connectToRadio's receiver and stream sizing, in its order: the
// ReceiverManager ceiling, receiver 0, the stream pool, then one receiver
// per remaining stream. Returns the pool size.
int connectPool(RadioModel& model, const RadioInfo& info)
{
    model.setLastRadioInfoForTest(info);
    const BoardCapabilities& caps = model.boardCapabilities();
    const int streams = BoardCapsTable::userDdcCountFor(
        caps, info.protocol, info.reportedReceivers);
    ReceiverManager* rm = model.receiverManager();
    rm->setMaxReceivers(RadioModel::receiverPoolCeiling(info, streams));
    rm->createReceiver();
    model.configureStreamPool(streams, caps.maxSlices > 0 ? caps.maxSlices : 1, 192000);
    for (int st = 1; st < streams; ++st) {
        if (rm->receiverConfig(st).receiverIndex < 0) {
            rm->createReceiver();
        }
    }
    return streams;
}

// The text beside "Max RX:" on the radio information tab.
QString maxRxText(const RadioInfoTab& tab)
{
    for (const QFormLayout* form : tab.findChildren<QFormLayout*>()) {
        for (int row = 0; row < form->rowCount(); ++row) {
            const QLayoutItem* labelItem = form->itemAt(row, QFormLayout::LabelRole);
            const QLayoutItem* fieldItem = form->itemAt(row, QFormLayout::FieldRole);
            if (!labelItem || !fieldItem) { continue; }
            const auto* label = qobject_cast<const QLabel*>(labelItem->widget());
            const auto* field = qobject_cast<const QLabel*>(fieldItem->widget());
            if (label && field && label->text() == QStringLiteral("Max RX:")) {
                return field->text();
            }
        }
    }
    return {};
}

// The support text the Copy Support Info button puts on the clipboard.
QString supportText(RadioInfoTab& tab)
{
    for (QPushButton* button : tab.findChildren<QPushButton*>()) {
        if (button->text().contains(QStringLiteral("Support"), Qt::CaseInsensitive)) {
            QGuiApplication::clipboard()->clear();
            button->click();
            return QGuiApplication::clipboard()->text();
        }
    }
    return {};
}

} // namespace

class TestReportedReceiverPool : public QObject
{
    Q_OBJECT

private slots:

    // Discovery keeps the radio's own number apart from the table fallback,
    // so the pool can tell "the radio said 4" from "the radio said nothing".
    void discovery_records_the_reported_count()
    {
        const RadioInfo four = parsedP2(HPSDRHW::Saturn, 4);
        QCOMPARE(four.protocol, ProtocolVersion::Protocol2);
        QCOMPARE(four.reportedReceivers, 4);
        QCOMPARE(four.maxReceivers, 4);

        const RadioInfo none = parsedP2(HPSDRHW::Saturn, 0);
        QCOMPARE(none.reportedReceivers, 0);
        QCOMPARE(none.maxReceivers, RadioInfo::maxReceiversForBoard(HPSDRHW::Saturn));

        // Protocol 1 boards other than the HL2 carry the count in byte 20,
        // as Thetis reads it.
        RadioInfo p1;
        QVERIFY(RadioDiscovery::parseP1Reply(p1Reply(1, 4),
                                             QHostAddress(QStringLiteral("192.168.1.21")), p1));
        QCOMPARE(p1.boardType, HPSDRHW::Hermes);
        QCOMPARE(p1.reportedReceivers, 4);
        QCOMPARE(p1.maxReceivers, 4);

        // The HL2 carries it in byte 19, as mi0bot reads it; byte 20 (0x45,
        // 69) is not a receiver count.
        const QByteArray hl2 = hl2CapturedReply();
        QCOMPARE(hl2.size(), 60);
        QCOMPARE(quint8(hl2[19]), quint8(0x04));
        QCOMPARE(quint8(hl2[20]), quint8(0x45));
        RadioInfo hl2Info;
        QVERIFY(RadioDiscovery::parseP1Reply(hl2, QHostAddress(QStringLiteral("192.168.1.123")),
                                             hl2Info));
        QCOMPARE(hl2Info.boardType, HPSDRHW::HermesLite);
        QCOMPARE(hl2Info.reportedReceivers, 4);
        QCOMPARE(hl2Info.maxReceivers, 4);

        // An HL2 reply reporting 0 keeps the board's own count.
        QByteArray hl2None = hl2;
        hl2None[19] = char(0);
        RadioInfo hl2NoneInfo;
        QVERIFY(RadioDiscovery::parseP1Reply(hl2None,
                                             QHostAddress(QStringLiteral("192.168.1.123")),
                                             hl2NoneInfo));
        QCOMPARE(hl2NoneInfo.reportedReceivers, 0);
        QCOMPARE(hl2NoneInfo.maxReceivers,
                 RadioInfo::maxReceiversForBoard(HPSDRHW::HermesLite));

        // A radio typed in by hand or restored from the saved list has
        // reported nothing.
        QCOMPARE(RadioInfo{}.reportedReceivers, 0);
    }

    // The one stream count, with the report applied.
    void stream_count_is_capped_by_the_report_on_protocol_2()
    {
        const BoardCapabilities& g2 = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        QCOMPARE(g2.userDdcCount, 5);
        QCOMPARE(BoardCapsTable::userDdcCountFor(g2, ProtocolVersion::Protocol2, 4), 4);
        QCOMPARE(BoardCapsTable::userDdcCountFor(g2, ProtocolVersion::Protocol2, 0), 5);
        // A report above the table never widens the pool past the DDCs the
        // slot plan knows how to use.
        QCOMPARE(BoardCapsTable::userDdcCountFor(g2, ProtocolVersion::Protocol2, 8), 5);
        // The two-argument form is the no-report form.
        QCOMPARE(BoardCapsTable::userDdcCountFor(g2, ProtocolVersion::Protocol2),
                 BoardCapsTable::userDdcCountFor(g2, ProtocolVersion::Protocol2, 0));
    }

    // Protocol 1 is unchanged: its frame slot plan sets the count.
    void protocol_1_ignores_the_report()
    {
        for (const BoardCapabilities& caps : BoardCapsTable::all()) {
            const int base = BoardCapsTable::userDdcCountFor(caps, ProtocolVersion::Protocol1);
            for (int reported : {0, 1, 2, 4, 7}) {
                QCOMPARE(BoardCapsTable::userDdcCountFor(caps, ProtocolVersion::Protocol1,
                                                         reported),
                         base);
            }
        }

        RadioInfo p1;
        p1.macAddress = QStringLiteral("00:1c:c0:11:22:33");
        p1.protocol = ProtocolVersion::Protocol1;
        p1.maxReceivers = 2;
        p1.reportedReceivers = 2;
        QCOMPARE(RadioModel::receiverPoolCeiling(p1, 4), 2);

        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        p1.boardType = HPSDRHW::OrionMKII;
        model.setLastRadioInfoForTest(p1);
        QCOMPARE(model.userStreamCount(), 4);
    }

    // Every reader of the stream count (the Core's advertised count, the
    // pan layout ceiling, the Core's own pool) goes through userStreamCount.
    void user_stream_count_follows_the_report()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);

        model.setLastRadioInfoForTest(parsedP2(HPSDRHW::Saturn, 4));
        QCOMPARE(model.userStreamCount(), 4);

        model.setLastRadioInfoForTest(parsedP2(HPSDRHW::Saturn, 0));
        QCOMPARE(model.userStreamCount(), 5);
    }

    // The bench case: a G2 reporting 4 connects with no warning and four
    // streams, each with a receiver behind it.
    void g2_reporting_four_connects_with_four_streams_and_no_warning()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        QTest::failOnWarning(QRegularExpression(QStringLiteral("Cannot create receiver")));

        const int streams = connectPool(model, parsedP2(HPSDRHW::Saturn, 4));
        QCOMPARE(streams, 4);
        QCOMPARE(model.streamPoolSize(), 4);
        QCOMPARE(model.userStreamCount(), 4);
        for (int st = 0; st < 4; ++st) {
            QVERIFY2(model.receiverManager()->receiverConfig(st).receiverIndex >= 0,
                     "every stream in the pool must have a receiver behind it");
        }
    }

    // A fifth pan cannot get a receiver of its own, and the operator is told
    // why in plain words that name the radio's count.
    void g2_reporting_four_refuses_a_fifth_pan_with_a_plain_reason()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        connectPool(model, parsedP2(HPSDRHW::Saturn, 4));

        QSignalSpy rejected(&model, &RadioModel::sliceAddRejected);
        for (int pan = 0; pan < 4; ++pan) {
            model.addSliceOnPan(QStringLiteral("pan-%1").arg(pan));
        }
        QCOMPARE(rejected.count(), 0);
        QCOMPARE(model.activeStreamCount(), 4);
        for (int st = 0; st < 4; ++st) {
            QVERIFY(model.receiverManager()->isReceiverActive(st));
        }

        const int before = model.slices().size();
        model.addSliceOnPan(QStringLiteral("pan-4"));
        QCOMPARE(model.slices().size(), before);
        QCOMPARE(rejected.count(), 1);
        const QString reason = rejected.at(0).at(0).toString();
        QVERIFY2(reason.contains(QStringLiteral("All 4 of the radio's receivers are in use")),
                 qPrintable(reason));
        QVERIFY2(!reason.contains(QStringLiteral("DDC")), qPrintable(reason));
    }

    // Reporting 0 keeps the table value, with no warning, including a radio
    // restored from the saved list (its RadioInfo carries the default
    // receiver count and no report).
    void no_report_keeps_the_table_value()
    {
        {
            RadioModel model;
            model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
            QTest::failOnWarning(QRegularExpression(QStringLiteral("Cannot create receiver")));
            QCOMPARE(connectPool(model, parsedP2(HPSDRHW::Saturn, 0)), 5);
            QCOMPARE(model.streamPoolSize(), 5);
            QCOMPARE(model.receiverManager()->receiverConfig(4).receiverIndex >= 0, true);
        }
        {
            RadioModel model;
            model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
            RadioInfo saved;
            saved.macAddress = QStringLiteral("00:1c:c0:77:88:99");
            saved.boardType = HPSDRHW::Saturn;
            saved.protocol = ProtocolVersion::Protocol2;
            QCOMPARE(saved.reportedReceivers, 0);
            QCOMPARE(connectPool(model, saved), 5);
            QCOMPARE(model.receiverManager()->receiverConfig(4).receiverIndex >= 0, true);
        }
    }

    // The one effective receiver count: min(table, reported) on Protocol 2,
    // the table when the radio reports 0, and the table on Protocol 1.
    void effective_receiver_count_is_the_one_helper()
    {
        const BoardCapabilities& g2 = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        QCOMPARE(g2.maxReceivers, 7);
        QCOMPARE(BoardCapsTable::effectiveReceiverCount(g2, ProtocolVersion::Protocol2, 4), 4);
        QCOMPARE(BoardCapsTable::effectiveReceiverCount(g2, ProtocolVersion::Protocol2, 0), 7);
        QCOMPARE(BoardCapsTable::effectiveReceiverCount(g2, ProtocolVersion::Protocol2, 9), 7);
        for (int reported : {0, 2, 4, 9}) {
            QCOMPARE(BoardCapsTable::effectiveReceiverCount(g2, ProtocolVersion::Protocol1,
                                                            reported),
                     7);
        }
        // The stream pool never exceeds it.
        for (const BoardCapabilities& caps : BoardCapsTable::all()) {
            for (ProtocolVersion proto : {ProtocolVersion::Protocol1, ProtocolVersion::Protocol2}) {
                for (int reported : {0, 1, 2, 4, 7, 9}) {
                    QVERIFY(BoardCapsTable::userDdcCountFor(caps, proto, reported)
                            <= BoardCapsTable::effectiveReceiverCount(caps, proto, reported));
                }
            }
        }
    }

    // Max RX on the radio information tab shows the radio's count.
    void radio_info_tab_max_rx_follows_the_report()
    {
        const BoardCapabilities& g2 = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        RadioInfoTab tab(nullptr);

        tab.populate(parsedP2(HPSDRHW::Saturn, 4), g2);
        QCOMPARE(maxRxText(tab), QStringLiteral("4"));
        QVERIFY2(supportText(tab).contains(QStringLiteral("Max RX: 4\n")),
                 qPrintable(supportText(tab)));

        tab.populate(parsedP2(HPSDRHW::Saturn, 0), g2);
        QCOMPARE(maxRxText(tab), QStringLiteral("7"));
        QVERIFY2(supportText(tab).contains(QStringLiteral("Max RX: 7\n")),
                 qPrintable(supportText(tab)));
    }

    // The live receiver count is clamped to the same count.
    void live_receiver_count_clamp_follows_the_report()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        QCOMPARE(model.maxActiveRxCount(), 7);

        model.setLastRadioInfoForTest(parsedP2(HPSDRHW::Saturn, 4));
        QCOMPARE(model.maxActiveRxCount(), 4);

        model.setLastRadioInfoForTest(parsedP2(HPSDRHW::Saturn, 0));
        QCOMPARE(model.maxActiveRxCount(), 7);
    }
};

QTEST_MAIN(TestReportedReceiverPool)
#include "tst_reported_receiver_pool.moc"
