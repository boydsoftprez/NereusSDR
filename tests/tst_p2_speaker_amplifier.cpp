// no-port-check: test-only. Thetis and piHPSDR file names appear only in
// source-cite comments that document which upstream line each assertion
// verifies. No upstream logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// The radio's speaker amplifier on Protocol 2 (R-SPK-08, R-SPK-09,
// R-SPK-15 connection half, D8, D17; V-SW-3).
// =================================================================
//
// Byte 1400 bit 1 of the high-priority packet mutes the stereo amplifier
// that drives the radio's own speaker, headphone and line out:
//   From Thetis ChannelMaster/network.c:1028 [v2.10.3.15]
//     packetbuf[1400] = xvtr_enable | (!audioamp_enable) << 1 | atu_tune << 2;
//   From piHPSDR src/alex.h:129 [@4aa95c5]
//     ANAN7000_HIPRIO1400_SPKR_MUTE 0x00000002 (1 = mute)
// Only the Protocol 2 models in Thetis HasAudioAmplifier have it
// (clsHardwareSpecific.cs:459-467 [v2.10.3.15]). The amplifier is off when
// the operator turned it off, when the radio speaker is muted, or while
// transmitting when set Off while transmitting, unless a side tone is
// expected (CW or Tune), as piHPSDR src/new_protocol.c:868-882 [@4aa95c5]
// does it. Bits 0 (transverter out) and 2 (ATU tune) stay zero.
// =================================================================

#include <QtTest/QtTest>

#include <QLoggingCategory>

#include <functional>

#include "core/AppSettings.h"
#include "core/HardwareProfile.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/SpeakerAmplifier.h"
#include "fakes/P2FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P2FakeRadio;

namespace {

constexpr int kSpeakerByte = 1400;
constexpr quint8 kAmpOff   = 0x02;
constexpr quint8 kAmpOn    = 0x00;

constexpr int kNormal              = 0;
constexpr int kOffWhileTransmitting = 1;
constexpr int kAlwaysOff           = 2;

// A quiet interval long enough for a queued packet to have gone out.
constexpr int kQuietMs = 150;

// The rule written out case by case, independently of the header.
bool expectedOff(bool hasAmp, int mode, bool muted, bool transmitting, bool sidetone)
{
    if (!hasAmp) {
        return false;
    }
    if (muted) {
        return true;
    }
    switch (mode) {
        case kAlwaysOff:
            return true;
        case kOffWhileTransmitting:
            return transmitting && !sidetone;
        default:
            return false;
    }
}

bool isAmplifierModel(HPSDRModel m)
{
    switch (m) {
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::ANVELINAPRO3:
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN_G2_1K:
        case HPSDRModel::REDPITAYA:
            return true;
        default:
            return false;
    }
}

quint8 speakerByte(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return buf[kSpeakerByte];
}

void configure(P2RadioConnection& conn, HPSDRModel model)
{
    const HardwareProfile profile = profileForModel(model);
    conn.setHardwareProfile(profile);
    conn.setBoardForTest(profile.effectiveBoard);
}

void apply(RadioConnection& conn, int mode, bool muted, bool transmitting, bool sidetone)
{
    conn.setSpeakerAmplifierMode(mode);
    conn.setRadioSpeakerMuted(muted);
    conn.setSidetoneExpected(sidetone);
    conn.setMox(transmitting);
}

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 3000)
{
    return QTest::qWaitFor(predicate, timeoutMs);
}

// A running Protocol 2 connection to the fake radio as an ANAN-7000DLE (an
// Orion MkII board with the amplifier).
struct RunningP2 {
    RunningP2()
    {
        AppSettings::instance().clear();
        QVERIFY(fake.start());
        const RadioInfo info = fake.radioInfo();
        conn.setHardwareProfile(profileForModel(HPSDRModel::ANAN7000D));
        conn.setPortBasesForTest(fake.outboundPortBase(), fake.inputRolePortBase());
        conn.init();
        conn.connectToRadio(info);
        QVERIFY(waitUntil([this]() { return fake.hasClient(); }));
        fake.sendDdc(2);
        QVERIFY(waitUntil([this]() { return conn.state() == ConnectionState::Connected; }));
        QTest::qWait(kQuietMs);
    }
    ~RunningP2()
    {
        conn.disconnect();
        fake.stop();
        AppSettings::instance().clear();
    }

    int count() const { return fake.highPriorityDatagrams(); }
    quint8 lastByte() const { return fake.highPriorityRecords().last().byte1400; }

    // One input change sends exactly one packet carrying `expected`.
    void expectOnePacket(const std::function<void()>& change, quint8 expected)
    {
        const int before = count();
        change();
        QVERIFY(waitUntil([this, before]() { return count() > before; }, 1000));
        QTest::qWait(kQuietMs);
        QCOMPARE(count(), before + 1);
        QCOMPARE(lastByte(), expected);
    }

    // A repeated value sends nothing.
    void expectNoPacket(const std::function<void()>& change)
    {
        const int before = count();
        change();
        QTest::qWait(kQuietMs);
        QCOMPARE(count(), before);
    }

    P2FakeRadio       fake;
    P2RadioConnection conn;
};

} // namespace

class TestP2SpeakerAmplifier : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Hundreds of connections below each log their codec choice.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.connection.info=false"));
        AppSettings::setProfileOverride(
            QStringLiteral("p2-speaker-amplifier-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ── The rule over every input: 2 x 3 x 2 x 2 x 2 ────────────────────
    void rule_table()
    {
        int rows = 0;
        for (const bool hasAmp : {false, true}) {
            for (const int mode : {kNormal, kOffWhileTransmitting, kAlwaysOff}) {
                for (const bool muted : {false, true}) {
                    for (const bool tx : {false, true}) {
                        for (const bool sidetone : {false, true}) {
                            const bool want = expectedOff(hasAmp, mode, muted, tx, sidetone);
                            const bool got = speakerAmplifierOff(hasAmp, mode, muted, tx, sidetone);
                            if (got != want) {
                                QFAIL(qPrintable(QStringLiteral(
                                    "hasAmp=%1 mode=%2 muted=%3 tx=%4 sidetone=%5: got %6")
                                    .arg(hasAmp).arg(mode).arg(muted).arg(tx).arg(sidetone)
                                    .arg(got)));
                            }
                            ++rows;
                        }
                    }
                }
            }
        }
        QCOMPARE(rows, 48);
        // Usable in constant expressions.
        static_assert(speakerAmplifierOff(true, 2, false, false, false));
        static_assert(!speakerAmplifierOff(false, 2, true, true, false));
        static_assert(!speakerAmplifierOff(true, 1, false, true, true));
    }

    // ── G2 on Protocol 2: the acceptance rows ────────────────────────────
    void g2_byte1400_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("muted");
        QTest::addColumn<bool>("transmitting");
        QTest::addColumn<bool>("sidetone");
        QTest::addColumn<int>("expected");

        QTest::newRow("Always off, receiving")  << kAlwaysOff << false << false << false << int(kAmpOff);
        QTest::newRow("Always off, keyed")      << kAlwaysOff << false << true  << false << int(kAmpOff);
        QTest::newRow("Always off, CW keyed")   << kAlwaysOff << false << true  << true  << int(kAmpOff);
        QTest::newRow("muted, Normal")          << kNormal << true << false << false << int(kAmpOff);
        QTest::newRow("muted, Normal, keyed")   << kNormal << true << true  << false << int(kAmpOff);
        QTest::newRow("muted, Normal, CW keyed") << kNormal << true << true << true  << int(kAmpOff);
        QTest::newRow("muted, Off-TX, receiving") << kOffWhileTransmitting << true << false << false << int(kAmpOff);
        QTest::newRow("muted, Off-TX, CW or Tune keyed")
            << kOffWhileTransmitting << true << true << true << int(kAmpOff);
        QTest::newRow("Off-TX, keyed, no side tone")
            << kOffWhileTransmitting << false << true << false << int(kAmpOff);
        QTest::newRow("Off-TX, keyed, CW or Tune")
            << kOffWhileTransmitting << false << true << true << int(kAmpOn);
        QTest::newRow("Off-TX, receiving")      << kOffWhileTransmitting << false << false << false << int(kAmpOn);
        QTest::newRow("Normal, unmuted")        << kNormal << false << false << false << int(kAmpOn);
        QTest::newRow("Normal, unmuted, keyed") << kNormal << false << true  << false << int(kAmpOn);
    }
    void g2_byte1400()
    {
        QFETCH(int, mode);
        QFETCH(bool, muted);
        QFETCH(bool, transmitting);
        QFETCH(bool, sidetone);
        QFETCH(int, expected);

        P2RadioConnection conn;
        configure(conn, HPSDRModel::ANAN_G2);
        apply(conn, mode, muted, transmitting, sidetone);
        QCOMPARE(int(speakerByte(conn)), expected);
    }

    // ── Every amplifier model, every input, bits 0 and 2 zero ────────────
    void amplifierModels_followTheRule()
    {
        for (int m = int(HPSDRModel::FIRST) + 1; m < int(HPSDRModel::LAST); ++m) {
            const HPSDRModel model = HPSDRModel(m);
            for (const int mode : {kNormal, kOffWhileTransmitting, kAlwaysOff}) {
                for (const bool muted : {false, true}) {
                    for (const bool tx : {false, true}) {
                        for (const bool sidetone : {false, true}) {
                            P2RadioConnection conn;
                            configure(conn, model);
                            apply(conn, mode, muted, tx, sidetone);
                            const quint8 byte = speakerByte(conn);
                            const quint8 want =
                                expectedOff(isAmplifierModel(model), mode, muted, tx, sidetone)
                                    ? kAmpOff : kAmpOn;
                            if (byte != want) {
                                QFAIL(qPrintable(QStringLiteral(
                                    "model=%1 mode=%2 muted=%3 tx=%4 sidetone=%5: byte 0x%6")
                                    .arg(m).arg(mode).arg(muted).arg(tx).arg(sidetone)
                                    .arg(byte, 2, 16, QLatin1Char('0'))));
                            }
                            QCOMPARE(byte & 0x05, 0);
                        }
                    }
                }
            }
        }
    }

    // ── Outside the amplifier list the byte is zero (D17: the G2E too) ──
    void otherModels_byteStaysZero_data()
    {
        QTest::addColumn<int>("model");
        for (int m = int(HPSDRModel::FIRST) + 1; m < int(HPSDRModel::LAST); ++m) {
            if (!isAmplifierModel(HPSDRModel(m))) {
                QTest::newRow(qPrintable(QStringLiteral("model %1").arg(m))) << m;
            }
        }
    }
    void otherModels_byteStaysZero()
    {
        QFETCH(int, model);
        P2RadioConnection conn;
        configure(conn, HPSDRModel(model));
        apply(conn, kAlwaysOff, true, true, false);
        QCOMPARE(int(speakerByte(conn)), 0);
        apply(conn, kOffWhileTransmitting, false, true, false);
        QCOMPARE(int(speakerByte(conn)), 0);
    }

    // ── Protocol 1 never carries it, even for a G2 profile ───────────────
    void protocol1_isUnchanged()
    {
        for (const HPSDRModel model : {HPSDRModel::ANAN_G2, HPSDRModel::HERMESLITE,
                                       HPSDRModel::ANAN7000D}) {
            P1RadioConnection conn;
            conn.setHardwareProfile(profileForModel(model));
            conn.setBoardForTest(profileForModel(model).effectiveBoard);

            quint8 before[17][5] = {};
            for (int bank = 0; bank < 17; ++bank) {
                conn.composeCcForBankForTest(bank, before[bank]);
            }
            conn.setSpeakerAmplifierMode(kAlwaysOff);
            conn.setRadioSpeakerMuted(true);
            conn.setSidetoneExpected(true);
            QCOMPARE(conn.speakerAmplifierMode(), kAlwaysOff);
            QVERIFY(conn.radioSpeakerMuted());
            QVERIFY(conn.sidetoneExpected());
            for (int bank = 0; bank < 17; ++bank) {
                quint8 after[5] = {};
                conn.composeCcForBankForTest(bank, after);
                QCOMPARE(QByteArray(reinterpret_cast<const char*>(after), 5),
                         QByteArray(reinterpret_cast<const char*>(before[bank]), 5));
            }
        }
    }

    // ── Safe before the connection runs: stored, nothing sent ────────────
    void settersBeforeRunning_areStored()
    {
        P2RadioConnection conn;
        configure(conn, HPSDRModel::ANAN_G2);
        QCOMPARE(conn.speakerAmplifierMode(), kNormal);
        QVERIFY(!conn.radioSpeakerMuted());
        QVERIFY(!conn.sidetoneExpected());
        conn.setSpeakerAmplifierMode(kOffWhileTransmitting);
        conn.setRadioSpeakerMuted(true);
        conn.setSidetoneExpected(true);
        QCOMPARE(conn.speakerAmplifierMode(), kOffWhileTransmitting);
        QVERIFY(conn.radioSpeakerMuted());
        QVERIFY(conn.sidetoneExpected());
        QCOMPARE(int(speakerByte(conn)), int(kAmpOff));
    }

    // ── Running: one high-priority packet per change, none for a repeat ──
    void running_eachChangeSendsOnePacket()
    {
        RunningP2 r;
        r.expectOnePacket([&r]() { r.conn.setSpeakerAmplifierMode(kAlwaysOff); }, kAmpOff);
        r.expectNoPacket([&r]() { r.conn.setSpeakerAmplifierMode(kAlwaysOff); });
        r.expectOnePacket([&r]() { r.conn.setSpeakerAmplifierMode(kNormal); }, kAmpOn);

        r.expectOnePacket([&r]() { r.conn.setRadioSpeakerMuted(true); }, kAmpOff);
        r.expectNoPacket([&r]() { r.conn.setRadioSpeakerMuted(true); });
        r.expectOnePacket([&r]() { r.conn.setRadioSpeakerMuted(false); }, kAmpOn);

        r.expectOnePacket([&r]() { r.conn.setSidetoneExpected(true); }, kAmpOn);
        r.expectNoPacket([&r]() { r.conn.setSidetoneExpected(true); });
        r.expectOnePacket([&r]() { r.conn.setSidetoneExpected(false); }, kAmpOn);

        r.expectOnePacket([&r]() { r.conn.setSpeakerAmplifierMode(kOffWhileTransmitting); }, kAmpOn);
    }

    // ── MOX carries the bit in its own packet: no extra packet, no gap ───
    void running_moxCarriesTheBit_data()
    {
        QTest::addColumn<bool>("sidetone");
        QTest::addColumn<int>("keyedByte");
        QTest::newRow("voice: off while keyed") << false << int(kAmpOff);
        QTest::newRow("CW or Tune: stays on")   << true  << int(kAmpOn);
    }
    void running_moxCarriesTheBit()
    {
        QFETCH(bool, sidetone);
        QFETCH(int, keyedByte);

        RunningP2 r;
        r.conn.setSpeakerAmplifierMode(kOffWhileTransmitting);
        r.conn.setSidetoneExpected(sidetone);
        QTest::qWait(kQuietMs);

        const auto& recs = r.fake.highPriorityRecords();
        const int keyAt = recs.size();
        r.conn.setMox(true);
        QVERIFY(waitUntil([&]() { return recs.size() > keyAt; }, 1000));
        // The first packet after the key is the MOX packet, and it already
        // carries the keyed amplifier state.
        QVERIFY((recs[keyAt].flags & 0x02) != 0);
        QCOMPARE(int(recs[keyAt].byte1400), keyedByte);
        QTest::qWait(250);  // heartbeats while keyed

        const int unkeyAt = recs.size();
        r.conn.setMox(false);
        QVERIFY(waitUntil([&]() { return recs.size() > unkeyAt; }, 1000));
        QVERIFY((recs[unkeyAt].flags & 0x02) == 0);
        QCOMPARE(int(recs[unkeyAt].byte1400), int(kAmpOn));
        QTest::qWait(kQuietMs);

        // Every packet in the run: keyed ones carry the keyed state,
        // unkeyed ones the amplifier on; bits 0 and 2 never set.
        for (int i = keyAt; i < recs.size(); ++i) {
            const bool keyed = (recs[i].flags & 0x02) != 0;
            QCOMPARE(int(recs[i].byte1400), keyed ? keyedByte : int(kAmpOn));
            QCOMPARE(recs[i].byte1400 & 0x05, 0);
        }
    }
};

QTEST_MAIN(TestP2SpeakerAmplifier)
#include "tst_p2_speaker_amplifier.moc"
