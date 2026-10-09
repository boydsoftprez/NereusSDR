// =================================================================
// tests/tst_port_audio_library_lock.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  PortAudioLibrary's lock (R-AUD-06):
// the older drivers' Rescan starts PortAudio again on the catalogue's
// thread while this thread queries PortAudio's host APIs and devices, and
// every query sees one whole, unchanged list (never PortAudio half
// stopped).  Uses the real PortAudio library; no stream is opened.  Also:
// Rescan leaves PortAudio alone while one of its streams is open.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 fix (R-AUD-06). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QRegularExpression>

#include "core/audio/PortAudioBackend.h"
#include "core/audio/PortAudioBus.h"
#include "core/audio/PortAudioLibrary.h"

#include <atomic>
#include <thread>

using namespace NereusSDR;

namespace {

constexpr int kRescans = 20;

QStringList hostApiNames()
{
    QStringList names;
    for (const PortAudioBus::HostApiInfo& api : PortAudioBus::hostApis()) {
        names << QStringLiteral("%1:%2").arg(api.index).arg(api.name);
    }
    return names;
}

QStringList deviceLines()
{
    QStringList lines;
    for (const PortAudioDeviceRecord& r : listPortAudioDevices()) {
        lines << QStringLiteral("%1|%2|%3|%4").arg(r.hostApi, r.name).arg(r.outputChannels).arg(r.inputChannels);
    }
    return lines;
}

QList<int> outputCounts()
{
    QList<int> counts;
    for (const PortAudioBus::HostApiInfo& api : PortAudioBus::hostApis()) {
        counts << PortAudioBus::outputDevicesFor(api.index).size()
               << PortAudioBus::inputDevicesFor(api.index).size();
    }
    return counts;
}

} // namespace

class TstPortAudioLibraryLock : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        PortAudioBus::allowPortAudioLibraryForTest(true);
        if (!PortAudioLibrary::acquire()) {
            PortAudioBus::allowPortAudioLibraryForTest(false);
            QSKIP("PortAudio does not start on this computer");
        }
        QCOMPARE(PortAudioLibrary::references(), 1);
    }

    void cleanupTestCase()
    {
        PortAudioLibrary::release();
        PortAudioBus::allowPortAudioLibraryForTest(false);
    }

    void queriesStayWholeWhileRescanning()
    {
        const QStringList apis = hostApiNames();
        const QStringList devices = deviceLines();
        const QList<int> counts = outputCounts();
        QVERIFY(!apis.isEmpty());

        PortAudioBackend backend(listPortAudioDevices, currentOlderDriverPlatform(), true);
        std::atomic<bool> done{false};
        std::atomic<int> rescans{0};
        std::thread catalogue([&]() {
            for (int i = 0; i < kRescans; ++i) {
                backend.rescan();
                rescans.fetch_add(1);
            }
            done.store(true);
        });
        int queries = 0;
        int mismatches = 0;
        while (!done.load()) {
            if (hostApiNames() != apis || deviceLines() != devices || outputCounts() != counts) {
                ++mismatches;
            }
            ++queries;
        }
        catalogue.join();

        QCOMPARE(rescans.load(), kRescans);
        QVERIFY(queries > 0);
        QCOMPARE(mismatches, 0);
        QCOMPARE(PortAudioLibrary::references(), 1);   // Rescan keeps every reference
        QCOMPARE(hostApiNames(), apis);
    }

    void rescanWaitsForOpenStreams()
    {
        PortAudioLibrary::streamOpened();
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("not listed again")));
        QVERIFY(!PortAudioLibrary::reinitialize());
        QCOMPARE(PortAudioLibrary::references(), 1);
        PortAudioLibrary::streamClosed();
        QCOMPARE(PortAudioLibrary::openStreams(), 0);
        QVERIFY(PortAudioLibrary::reinitialize());
        QVERIFY(!hostApiNames().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstPortAudioLibraryLock)
#include "tst_port_audio_library_lock.moc"
