// =================================================================
// tests/tst_audio_ring_spsc.cpp  (NereusSDR)
// Author: J.J. Boyd (KG4VCF), AI-assisted via Claude Code. 2026-04-23.
// =================================================================
#include <QtTest/QtTest>
#include <thread>
#include <atomic>
#include "core/audio/AudioRingSpsc.h"

using Ring = NereusSDR::AudioRingSpsc<65536>;

class TestAudioRingSpsc : public QObject {
    Q_OBJECT

private slots:
    void pushPopRoundTrip() {
        Ring r;
        const uint8_t in[] = {1, 2, 3, 4, 5, 6, 7, 8};
        QCOMPARE(r.pushCopy(in, sizeof(in)), qint64(sizeof(in)));
        uint8_t out[8] = {};
        QCOMPARE(r.popInto(out, sizeof(out)), qint64(sizeof(out)));
        QCOMPARE(std::memcmp(in, out, sizeof(in)), 0);
        QCOMPARE(r.usedBytes(), size_t(0));
    }

    void emptyPopReturnsZero() {
        Ring r;
        uint8_t buf[16];
        QCOMPARE(r.popInto(buf, sizeof(buf)), qint64(0));
    }

    void partialPopHonoursDestCapacity() {
        Ring r;
        uint8_t in[32];
        for (int i = 0; i < 32; ++i) { in[i] = uint8_t(i); }
        r.pushCopy(in, sizeof(in));
        uint8_t out[8];
        QCOMPARE(r.popInto(out, sizeof(out)), qint64(8));
        QCOMPARE(out[0], uint8_t(0));
        QCOMPARE(out[7], uint8_t(7));
        QCOMPARE(r.usedBytes(), size_t(24));
    }

    void overflowDropsOldest() {
        // Ring capacity = 65536; write 70000, read all — should get
        // the last 65535 bytes (oldest dropped).
        Ring r;
        std::vector<uint8_t> in(70000);
        for (size_t i = 0; i < in.size(); ++i) { in[i] = uint8_t(i & 0xff); }
        r.pushCopy(in.data(), in.size());
        std::vector<uint8_t> out(70000);
        const auto popped = r.popInto(out.data(), out.size());
        QCOMPARE(qint64(r.capacity() - 1), popped);  // capacity-1 usable
        // First byte read corresponds to in[70000 - (capacity-1)].
        const size_t firstIdx = in.size() - size_t(popped);
        QCOMPARE(out[0], uint8_t(firstIdx & 0xff));
    }

    void wraparound() {
        Ring r;
        // Fill almost all.
        std::vector<uint8_t> a(r.capacity() - 100);
        std::fill(a.begin(), a.end(), uint8_t(0xAA));
        r.pushCopy(a.data(), a.size());
        // Drain.
        std::vector<uint8_t> tmp(a.size());
        r.popInto(tmp.data(), tmp.size());
        // Push + pop 200 bytes that straddle the wrap.
        std::vector<uint8_t> b(200);
        for (size_t i = 0; i < 200; ++i) { b[i] = uint8_t(i); }
        r.pushCopy(b.data(), b.size());
        std::vector<uint8_t> out(200);
        r.popInto(out.data(), out.size());
        for (size_t i = 0; i < 200; ++i) { QCOMPARE(out[i], b[i]); }
    }

    void concurrentProducerConsumer() {
        Ring r;
        constexpr int TOTAL = 1'000'000;
        std::atomic<bool> producerDone{false};
        std::thread producer([&]() {
            uint8_t byte = 0;
            int sent = 0;
            while (sent < TOTAL) {
                uint8_t buf[64];
                for (auto& b : buf) { b = byte++; }
                const auto pushed = r.pushCopy(buf, sizeof(buf));
                sent += int(pushed);
                if (pushed == 0) { std::this_thread::yield(); }
            }
            producerDone.store(true, std::memory_order_release);
        });

        uint8_t expected = 0;
        int received = 0;
        uint8_t buf[64];
        while (received < TOTAL) {
            const auto popped = r.popInto(buf, sizeof(buf));
            for (qint64 i = 0; i < popped; ++i) {
                QCOMPARE(buf[i], expected++);
            }
            received += int(popped);
            if (popped == 0) { std::this_thread::yield(); }
        }

        producer.join();
        QVERIFY(producerDone.load(std::memory_order_acquire));
    }
};

QTEST_MAIN(TestAudioRingSpsc)
#include "tst_audio_ring_spsc.moc"
