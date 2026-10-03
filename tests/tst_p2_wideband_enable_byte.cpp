// =================================================================
// tests/tst_p2_wideband_enable_byte.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic F Task 1: P2 composeCmdGeneral writes packetbuf[23]
// wideband per-ADC enable mask per Thetis network.c:879 [v2.10.3.15].
//
// Source-first correction note: the original Sub-Epic F plan targeted
// composeCmdRx byte 23. That was wrong: CmdRx byte 23 is rx[1].rx_adc
// (RX1 ADC selector) per Thetis network.c:1118, not the wideband enable
// mask. The mask lives in CmdGeneral byte 23 per Thetis network.c:879.
// See plan revision note at
// docs/architecture/2026-05-26-phase3f-sub-epic-f-wideband-plan.md
// (Task 1) for full rationale.
// =================================================================
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"
#include "core/WidebandFrameAccumulator.h"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>

using namespace NereusSDR;

class TestP2WidebandEnableByte : public QObject {
    Q_OBJECT
private slots:
    void enable_announces_identity_without_data_and_suppresses_replaced_transition()
    {
        P2RadioConnection connection;
        QSignalSpy states(&connection, &P2RadioConnection::widebandCaptureStateApplied);
        QSignalSpy rows(&connection, &P2RadioConnection::widebandFrameReadyForGeneration);
        connection.setWidebandEnabled(0, true);
        QCOMPARE(states.size(), 1);
        QCOMPARE(states.last().at(2).toBool(), true);
        QCOMPARE(states.last().at(1).toULongLong(),
                 connection.widebandCaptureEpoch(0)->load(std::memory_order_acquire));
        QCOMPARE(rows.size(), 0);
        const auto generation = states.last().at(1).toULongLong();
        connection.setWidebandEnabled(0, true);
        QCOMPARE(states.size(), 2);
        QCOMPARE(states.last().at(1).toULongLong(), generation);
        bool replaced = false;
        connect(&connection, &P2RadioConnection::widebandCaptureRetired, &connection,
                [&](int adc, quint64) {
            if (adc == 0 && !replaced) {
                replaced = true;
                connection.setWidebandEnabled(0, true);
            }
        });
        states.clear();
        connection.setWidebandEnabled(0, false);
        QCOMPARE(states.size(), 1); // Only the nested current enable is announced.
        QCOMPARE(states.last().at(2).toBool(), true);
        QCOMPARE(connection.wbEnableMask(), quint8(1));
    }

    void capture_epoch_rejects_invalid_adc()
    {
        P2RadioConnection conn;
        QVERIFY(!conn.widebandCaptureEpoch(-1));
        QVERIFY(!conn.widebandCaptureEpoch(8));
        QVERIFY(conn.widebandCaptureEpoch(0));
        QVERIFY(conn.widebandCaptureEpoch(0)->load(std::memory_order_acquire) != 0);
    }

    void compose_cmd_general_writes_packetbuf_23_when_wideband_enabled()
    {
        P2RadioConnection conn;
        conn.setWidebandEnabled(0, true);  // enable ADC0 wideband

        quint8 buf[60] = {0};
        conn.composeCmdGeneralForTest(buf);

        // packetbuf[23] should have bit 0 set (ADC0 enabled).
        QCOMPARE(quint8(buf[23] & 0x01), quint8(0x01));
    }

    void compose_cmd_general_writes_0_when_no_wideband()
    {
        P2RadioConnection conn;
        quint8 buf[60] = {0};
        conn.composeCmdGeneralForTest(buf);
        QCOMPARE(quint8(buf[23]), quint8(0x00));
    }

    void per_adc_enable_independent()
    {
        P2RadioConnection conn;
        conn.setWidebandEnabled(0, true);
        conn.setWidebandEnabled(1, true);
        quint8 buf[60] = {0};
        conn.composeCmdGeneralForTest(buf);
        QCOMPARE(quint8(buf[23] & 0x03), quint8(0x03));  // both bits set
    }

    void assembled_frame_carries_its_capture_generation()
    {
        P2RadioConnection conn;
        const auto accumulators = conn.findChildren<WidebandFrameAccumulator*>();
        QCOMPARE(accumulators.size(), 8);
        const auto epoch = conn.widebandCaptureEpoch(0);
        conn.setWidebandEnabled(0, true);
        const quint64 expected = epoch->load(std::memory_order_acquire);
        QSignalSpy tagged(&conn,
                          &P2RadioConnection::widebandFrameReadyForGeneration);
        QSignalSpy legacy(&conn, &P2RadioConnection::widebandFrameReady);

        const QByteArray payload(1024, char(0x20));
        const auto before = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (int seq = 0; seq < 32; ++seq) {
            accumulators.at(0)->pushPacket(seq, payload);
        }
        const auto after = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        QCOMPARE(tagged.count(), 1);
        QCOMPARE(tagged.first().at(0).toInt(), 0);
        QCOMPARE(tagged.first().at(1).toULongLong(), expected);
        QVERIFY(tagged.first().at(3).toLongLong() >= before);
        QVERIFY(tagged.first().at(3).toLongLong() <= after);
        const QVector<float> samples =
            tagged.first().at(2).value<QVector<float>>();
        QCOMPARE(samples.size(), 16384);
        QCOMPARE(samples.first(), float(0x2020) / 32768.0f);
        QCOMPARE(legacy.count(), 1);
        QCOMPARE(legacy.first().at(1).value<QVector<float>>(), samples);
    }

    void changed_enable_advances_only_the_selected_adc_epoch()
    {
        P2RadioConnection conn;
        const auto adc0 = conn.widebandCaptureEpoch(0);
        const auto adc1 = conn.widebandCaptureEpoch(1);
        const quint64 adc0Initial = adc0->load(std::memory_order_acquire);
        const quint64 adc1Initial = adc1->load(std::memory_order_acquire);

        conn.setWidebandEnabled(0, true);
        const quint64 adc0Enabled = adc0->load(std::memory_order_acquire);
        QVERIFY(adc0Enabled != adc0Initial);
        QCOMPARE(adc1->load(std::memory_order_acquire), adc1Initial);

        conn.setWidebandEnabled(0, true);
        QCOMPARE(adc0->load(std::memory_order_acquire), adc0Enabled);
        conn.setWidebandEnabled(1, true);
        QCOMPARE(adc0->load(std::memory_order_acquire), adc0Enabled);
        QVERIFY(adc1->load(std::memory_order_acquire) != adc1Initial);

        conn.setWidebandEnabled(0, false);
        const quint64 adc0Disabled = adc0->load(std::memory_order_acquire);
        QVERIFY(adc0Disabled != adc0Enabled);
        conn.setWidebandEnabled(0, true);
        QVERIFY(adc0->load(std::memory_order_acquire) != adc0Disabled);
    }

    void retirement_observer_sees_committed_mask_and_can_enable_another_adc()
    {
        P2RadioConnection conn;
        connect(&conn, &P2RadioConnection::widebandCaptureRetired, &conn,
                [&](int adc, quint64 generation) {
            QCOMPARE(generation, conn.widebandCaptureEpoch(adc)->load(std::memory_order_acquire));
            if (adc == 0) {
                QCOMPARE(conn.wbEnableMask(), quint8(1));
                conn.setWidebandEnabled(1, true);
            }
        });
        conn.setWidebandEnabled(0, true);
        QCOMPARE(conn.wbEnableMask(), quint8(3));
    }

    void restarting_capture_discards_the_old_partial_frame()
    {
        P2RadioConnection conn;
        const auto accumulators = conn.findChildren<WidebandFrameAccumulator*>();
        QCOMPARE(accumulators.size(), 8);
        WidebandFrameAccumulator* adc0 = accumulators.at(0);
        QSignalSpy frames(&conn, &P2RadioConnection::widebandFrameReady);
        conn.setWidebandEnabled(0, true);
        const QByteArray oldPayload(1024, char(0x10));
        for (int seq = 0; seq < 16; ++seq) {
            adc0->pushPacket(seq, oldPayload);
        }
        conn.setWidebandEnabled(0, false);
        conn.setWidebandEnabled(0, true);
        const QByteArray newPayload(1024, char(0x20));
        for (int seq = 16; seq < 32; ++seq) {
            adc0->pushPacket(seq, newPayload);
        }
        // Feed the owned assembler directly: the boundary under test is
        // setWidebandEnabled's lifetime reset, not UDP parsing. Trailing
        // packets from an old burst cannot complete old data in a new view.
        QCOMPARE(frames.count(), 0);
        for (int seq = 0; seq < 32; ++seq) {
            adc0->pushPacket(seq, newPayload);
        }
        QCOMPARE(frames.count(), 1);
        const QVector<float> samples = frames.first().at(1).value<QVector<float>>();
        QCOMPARE(samples.size(), 16384);
        QCOMPARE(samples.first(), float(0x2020) / 32768.0f);
        QCOMPARE(samples.last(), samples.first());
    }

    void duplicate_enable_and_other_adc_do_not_discard_live_capture()
    {
        P2RadioConnection conn;
        const auto accumulators = conn.findChildren<WidebandFrameAccumulator*>();
        QCOMPARE(accumulators.size(), 8);
        QSignalSpy frames(&conn, &P2RadioConnection::widebandFrameReady);
        conn.setWidebandEnabled(0, true);
        conn.setWidebandEnabled(1, true);
        const QByteArray payload(1024, char(0x20));
        for (int seq = 0; seq < 16; ++seq) {
            accumulators.at(1)->pushPacket(seq, payload);
        }
        conn.setWidebandEnabled(1, true);
        conn.setWidebandEnabled(0, false);
        for (int seq = 16; seq < 32; ++seq) {
            accumulators.at(1)->pushPacket(seq, payload);
        }
        QCOMPARE(frames.count(), 1);
        QCOMPARE(frames.first().at(0).toInt(), 1);
    }

    void disconnect_discards_partial_capture()
    {
        P2RadioConnection conn;
        const auto accumulators = conn.findChildren<WidebandFrameAccumulator*>();
        QCOMPARE(accumulators.size(), 8);
        QSignalSpy frames(&conn, &P2RadioConnection::widebandFrameReady);
        conn.setWidebandEnabled(0, true);
        const QByteArray payload(1024, char(0x20));
        accumulators.at(0)->pushPacket(0, payload);
        conn.disconnect();
        for (int seq = 1; seq < 32; ++seq) {
            accumulators.at(0)->pushPacket(seq, payload);
        }
        QCOMPARE(frames.count(), 0);
    }

    void disconnect_invalidates_every_adc_epoch()
    {
        P2RadioConnection conn;
        std::array<std::shared_ptr<const std::atomic<quint64>>, 8> epochs;
        std::array<quint64, 8> before{};
        for (int adc = 0; adc < 8; ++adc) {
            epochs[adc] = conn.widebandCaptureEpoch(adc);
            before[adc] = epochs[adc]->load(std::memory_order_acquire);
        }

        conn.disconnect();

        for (int adc = 0; adc < 8; ++adc) {
            const quint64 after = epochs[adc]->load(std::memory_order_acquire);
            QVERIFY(after != 0);
            QVERIFY(after != before[adc]);
        }
    }

    void destructor_invalidates_retained_epoch()
    {
        std::shared_ptr<const std::atomic<quint64>> retained;
        quint64 before = 0;
        {
            P2RadioConnection conn;
            retained = conn.widebandCaptureEpoch(3);
            before = retained->load(std::memory_order_acquire);
        }
        const quint64 after = retained->load(std::memory_order_acquire);
        QVERIFY(after != 0);
        QVERIFY(after != before);
    }

    void direct_observer_cannot_relabel_the_emitting_frame()
    {
        P2RadioConnection conn;
        const auto accumulators = conn.findChildren<WidebandFrameAccumulator*>();
        QCOMPARE(accumulators.size(), 8);
        const auto epoch = conn.widebandCaptureEpoch(0);
        conn.setWidebandEnabled(0, true);
        const quint64 emittingGeneration = epoch->load(std::memory_order_acquire);
        quint64 observedGeneration = 0;
        QVector<float> observedSamples;
        connect(&conn, &P2RadioConnection::widebandFrameReadyForGeneration,
                &conn,
                [&](int adcIndex, quint64 captureGeneration,
                    const QVector<float>& samples) {
            QCOMPARE(adcIndex, 0);
            observedGeneration = captureGeneration;
            observedSamples = samples;
            conn.setWidebandEnabled(0, false);
            conn.setWidebandEnabled(0, true);
        }, Qt::DirectConnection);
        QSignalSpy legacy(&conn, &P2RadioConnection::widebandFrameReady);

        const QByteArray payload(1024, char(0x20));
        for (int seq = 0; seq < 32; ++seq) {
            accumulators.at(0)->pushPacket(seq, payload);
        }

        QCOMPARE(observedGeneration, emittingGeneration);
        QVERIFY(epoch->load(std::memory_order_acquire) != emittingGeneration);
        QCOMPARE(observedSamples.size(), 16384);
        QCOMPARE(legacy.count(), 1);
        QCOMPARE(legacy.first().at(1).value<QVector<float>>(), observedSamples);
    }
};

QTEST_MAIN(TestP2WidebandEnableByte)
#include "tst_p2_wideband_enable_byte.moc"
