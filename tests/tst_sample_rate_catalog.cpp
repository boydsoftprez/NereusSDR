// =================================================================
// tests/tst_sample_rate_catalog.cpp  (NereusSDR)
// =================================================================
//
// Independently implemented from setup.cs — this test file exercises
// NereusSDR's SampleRateCatalog API; Thetis has no equivalent test
// suite, so no upstream header is preserved. The constants asserted
// here are documented in the SampleRateCatalog header's verbatim cite.
// =================================================================

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/SampleRateCatalog.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h" // ProtocolVersion

using namespace NereusSDR;

class TestSampleRateCatalog : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    // ── allowedSampleRates ────────────────────────────────────────────────────

    void p1_hermes_allows_48_96_192()
    {
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::HERMES);
        const auto got   = allowedSampleRates(ProtocolVersion::Protocol1, caps, HPSDRModel::HERMES);
        QCOMPARE(got, std::vector<int>({48000, 96000, 192000}));
    }

    void p2_anan_g2_allows_all_six_p2_rates()
    {
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::ANAN_G2);
        const auto got   = allowedSampleRates(ProtocolVersion::Protocol2, caps, HPSDRModel::ANAN_G2);
        QCOMPARE(got, std::vector<int>({48000, 96000, 192000, 384000, 768000, 1536000}));
    }

    void p1_redpitaya_gets_extra_384()
    {
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::REDPITAYA);
        const auto got   = allowedSampleRates(ProtocolVersion::Protocol1, caps, HPSDRModel::REDPITAYA);
        QCOMPARE(got, std::vector<int>({48000, 96000, 192000, 384000}));
    }

    // mi0bot setup.cs:849-851 [v2.10.3.13] — HermesLite 2 also qualifies
    // for the extra 384k rate on P1.  Pre-fix, NereusSDR only honoured the
    // RedPitaya branch and silently dropped HL2's 384k.
    void p1_hermes_lite_gets_extra_384()
    {
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::HERMESLITE);
        const auto got   = allowedSampleRates(ProtocolVersion::Protocol1, caps, HPSDRModel::HERMESLITE);
        QCOMPARE(got, std::vector<int>({48000, 96000, 192000, 384000}));
    }

    void p1_orionmkii_does_not_get_extra_384()
    {
        // OrionMKII and RedPitaya share HPSDRHW::OrionMKII but only
        // REDPITAYA gets the extra 384k on P1 (setup.cs:847 flag).
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::ORIONMKII);
        const auto got   = allowedSampleRates(ProtocolVersion::Protocol1, caps, HPSDRModel::ORIONMKII);
        QCOMPARE(got, std::vector<int>({48000, 96000, 192000}));
    }

    void p2_redpitaya_gets_full_p2_list()
    {
        // RedPitaya is P1-special (extra 384k per setup.cs:847) but on
        // Protocol 2 it falls through to the generic P2 list (setup.cs:850).
        // Hand-build caps rather than relying on registry mapping, since
        // REDPITAYA shares HPSDRHW with OrionMKII.
        BoardCapabilities caps{};
        caps.sampleRates  = {48000, 96000, 192000, 384000, 768000, 1536000};
        caps.maxReceivers = 2;
        caps.maxSampleRate = 1536000;
        const auto got = allowedSampleRates(ProtocolVersion::Protocol2, caps, HPSDRModel::REDPITAYA);
        QCOMPARE(got, std::vector<int>({48000, 96000, 192000, 384000, 768000, 1536000}));
    }

    void caps_sample_rates_intersect_with_master_list()
    {
        // Hand-built caps with only 48k and 192k populated; should drop
        // 96k even though the master P1 list has it.
        BoardCapabilities caps{};
        caps.sampleRates  = {48000, 192000, 0, 0, 0, 0};
        caps.maxReceivers = 1;
        caps.maxSampleRate = 192000;
        const auto got = allowedSampleRates(ProtocolVersion::Protocol1, caps, HPSDRModel::HERMES);
        QCOMPARE(got, std::vector<int>({48000, 192000}));
    }

    void empty_caps_sample_rates_returns_empty()
    {
        BoardCapabilities caps{};
        caps.sampleRates = {0, 0, 0, 0, 0, 0};
        const auto got = allowedSampleRates(ProtocolVersion::Protocol2, caps, HPSDRModel::ANAN_G2);
        QVERIFY(got.empty());
    }

    // ── defaultSampleRate ─────────────────────────────────────────────────────

    void default_is_192k_when_present()
    {
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::ANAN_G2);
        QCOMPARE(defaultSampleRate(ProtocolVersion::Protocol2, caps, HPSDRModel::ANAN_G2), 192000);
    }

    void default_falls_back_to_first_when_192k_missing()
    {
        BoardCapabilities caps{};
        caps.sampleRates = {48000, 96000, 0, 0, 0, 0};
        const auto got = defaultSampleRate(ProtocolVersion::Protocol1, caps, HPSDRModel::HERMES);
        QCOMPARE(got, 48000);
    }

    // ── bufferSizeForRate ─────────────────────────────────────────────────────

    void buffer_size_matches_thetis_formula()
    {
        // cmsetup.c:104-111 — base_size * rate / base_rate, base_size=64, base_rate=48000.
        QCOMPARE(bufferSizeForRate(48000),   64);
        QCOMPARE(bufferSizeForRate(96000),   128);
        QCOMPARE(bufferSizeForRate(192000),  256);
        QCOMPARE(bufferSizeForRate(384000),  512);
        QCOMPARE(bufferSizeForRate(768000),  1024);
        QCOMPARE(bufferSizeForRate(1536000), 2048);
    }

    // ── resolveSampleRate ─────────────────────────────────────────────────────

    void resolve_returns_persisted_when_valid()
    {
        AppSettings s(m_dir.filePath(QStringLiteral("r1.xml")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"), 384000);
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::ANAN_G2);
        QCOMPARE(resolveSampleRate(s, mac, ProtocolVersion::Protocol2, caps, HPSDRModel::ANAN_G2), 384000);
    }

    void resolve_falls_back_to_default_when_unset()
    {
        AppSettings s(m_dir.filePath(QStringLiteral("r2.xml")));
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::HERMES);
        QCOMPARE(resolveSampleRate(s, QStringLiteral("aa:bb:cc:11:22:33"),
                                    ProtocolVersion::Protocol1, caps, HPSDRModel::HERMES),
                 192000);
    }

    void resolve_falls_back_when_persisted_not_in_allowed()
    {
        // User persisted 1.536M for ANAN-G2, then plugs in an HL2 (caps max 192k).
        AppSettings s(m_dir.filePath(QStringLiteral("r3.xml")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"), 1536000);
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::HERMESLITE);
        QCOMPARE(resolveSampleRate(s, mac, ProtocolVersion::Protocol1, caps, HPSDRModel::HERMESLITE),
                 192000);
    }

    // ── Plan Task 15: saved rates survive the wider lists ────────────────────
    //
    // Every rate a radio offered before is still offered, so a saved rate is
    // kept; the newly offered rates (Protocol 2 above 192/384 kHz on the
    // Atlas, Hermes, HermesII and HL2 rows, Thetis setup.cs:850
    // [v2.10.3.15]; 384 kHz on the receive-only kit's Protocol 1, mi0bot
    // setup.cs:849-851 [v2.10.3.13-beta2]) are kept once saved too. Resolving
    // never writes the saved value.
    void resolve_keeps_saved_rates_the_wider_lists_offer_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("model");
        QTest::addColumn<int>("protocol");
        QTest::addColumn<int>("saved");
        const int p1 = int(ProtocolVersion::Protocol1);
        const int p2 = int(ProtocolVersion::Protocol2);
        QTest::newRow("HL2 P1 384k")  << int(HPSDRHW::HermesLite) << int(HPSDRModel::HERMESLITE) << p1 << 384000;
        QTest::newRow("HL2 P2 384k")  << int(HPSDRHW::HermesLite) << int(HPSDRModel::HERMESLITE) << p2 << 384000;
        QTest::newRow("HL2 P2 768k")  << int(HPSDRHW::HermesLite) << int(HPSDRModel::HERMESLITE) << p2 << 768000;
        QTest::newRow("Hermes P1 192k") << int(HPSDRHW::Hermes) << int(HPSDRModel::HERMES) << p1 << 192000;
        QTest::newRow("Hermes P2 1536k") << int(HPSDRHW::Hermes) << int(HPSDRModel::HERMES) << p2 << 1536000;
        QTest::newRow("HermesII P2 768k") << int(HPSDRHW::HermesII) << int(HPSDRModel::ANAN10E) << p2 << 768000;
        QTest::newRow("Atlas P2 384k") << int(HPSDRHW::Atlas) << int(HPSDRModel::HPSDR) << p2 << 384000;
        QTest::newRow("Kit P1 192k")  << int(HPSDRHW::HermesLiteRxOnly) << int(HPSDRModel::HERMESLITE) << p1 << 192000;
        QTest::newRow("Kit P1 384k")  << int(HPSDRHW::HermesLiteRxOnly) << int(HPSDRModel::HERMESLITE) << p1 << 384000;
        QTest::newRow("Kit P2 1536k") << int(HPSDRHW::HermesLiteRxOnly) << int(HPSDRModel::HERMESLITE) << p2 << 1536000;
    }

    void resolve_keeps_saved_rates_the_wider_lists_offer()
    {
        QFETCH(int, board);
        QFETCH(int, model);
        QFETCH(int, protocol);
        QFETCH(int, saved);
        AppSettings s(m_dir.filePath(QStringLiteral("t15-%1.xml")
                                         .arg(QString::fromLatin1(QTest::currentDataTag())
                                                  .replace(QLatin1Char(' '), QLatin1Char('_')))));
        const QString mac = QStringLiteral("aa:bb:cc:15:15:15");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"), saved);
        const auto& caps = BoardCapsTable::forBoard(static_cast<HPSDRHW>(board));
        QCOMPARE(resolveSampleRate(s, mac, static_cast<ProtocolVersion>(protocol), caps,
                                   static_cast<HPSDRModel>(model)),
                 saved);
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("radioInfo/sampleRate")).toInt(), saved);
    }

    // A saved rate the protocol does not offer falls back for this connect
    // only; the saved value is left as it was.
    void resolve_never_rewrites_the_saved_rate()
    {
        AppSettings s(m_dir.filePath(QStringLiteral("t15-rewrite.xml")));
        const QString mac = QStringLiteral("aa:bb:cc:15:15:16");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"), 768000);
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("Persisted sample rate 768000 not valid")));
        QCOMPARE(resolveSampleRate(s, mac, ProtocolVersion::Protocol1, caps, HPSDRModel::HERMESLITE),
                 192000);
        QCOMPARE(s.hardwareValue(mac, QStringLiteral("radioInfo/sampleRate")).toInt(), 768000);
    }

    // ── resolveActiveRxCount ──────────────────────────────────────────────────

    void resolve_rx_count_returns_persisted_when_in_range()
    {
        AppSettings s(m_dir.filePath(QStringLiteral("r4.xml")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/activeRxCount"), 3);
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::ANAN_G2);
        QCOMPARE(resolveActiveRxCount(s, mac, caps), 3);
    }

    void resolve_rx_count_clamps_to_max_receivers()
    {
        // User persisted 7 while on G2, then swapped to HL2 (max 2).
        AppSettings s(m_dir.filePath(QStringLiteral("r5.xml")));
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/activeRxCount"), 7);
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::HERMESLITE);
        QCOMPARE(resolveActiveRxCount(s, mac, caps), caps.maxReceivers);
    }

    void resolve_rx_count_defaults_to_1_when_unset()
    {
        AppSettings s(m_dir.filePath(QStringLiteral("r6.xml")));
        const auto& caps = BoardCapsTable::forModel(HPSDRModel::ANAN_G2);
        QCOMPARE(resolveActiveRxCount(s, QStringLiteral("aa:bb:cc:11:22:33"), caps), 1);
    }
};

QTEST_MAIN(TestSampleRateCatalog)
#include "tst_sample_rate_catalog.moc"
