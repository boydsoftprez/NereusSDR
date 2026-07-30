// tests/tst_tci_rx_volume_broadcast.cpp  (NereusSDR)
// NereusSDR-original test; pins a ported Thetis behaviour.
//
// Phase 3F chip task_c1e6fbad: the rx_volume live-broadcast path.
//
// The gap this pins, as recorded in TciServer.cpp before the fix:
// NereusSDR re-broadcast all four rx_volume slots from the single radio-global
// AudioEngine::volumeChanged (the MASTER AF slider).  Thetis does not do that.
//
//   OnVolumeChanged            (TCIServer.cs:7806-7817 [v2.10.3.15])
//       -> socketListener.VolumeChanged(newVolume)   // "volume:" ONLY
//
//   console.RXGainChangedHandlers += OnRxAfGainChanged
//                              (TCIServer.cs:6780     [v2.10.3.15])
//   OnRxAfGainChanged          (TCIServer.cs:7722-7733 [v2.10.3.15])
//       -> socketListener.RxAfGainChanged(rx, is_subrx, new_gain)
//   RxAfGainChanged            (TCIServer.cs:1118-1124 [v2.10.3.15])
//       int chan = is_subrx ? 1 : 0;
//       double db = audioGainToDb(gain / 100f);
//       sendRxVolume(rx - 1, chan, db);
//
// So rx_volume is driven by a PER-RX gain event, one frame for whichever
// (rx, is_subrx) actually changed, and the master volume event never emits an
// rx_volume line at all.
//
// NereusSDR's per-rx analog is SliceModel::afGain.  Receiver-to-slice mapping
// is the convention Sub-Epic J Task 10 established for the init burst
// (TciProtocol.cpp buildInitialRadioStateLines): TCI receiver N -> slice id N,
// with channel M not consulted because NereusSDR has no sub-receiver model --
// the same collapse Thetis itself applies to RX2, where sendRxVolume(1,1,...)
// reuses rx2vol (TCIServer.cs:2557 [v2.10.3.15]).

#ifdef HAVE_WEBSOCKETS

#include <QtTest>

#include "core/TciServer.h"
#include "core/TciProtocol.h"
#include "core/TciVolume.h"
#include "core/AudioEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestTciRxVolumeBroadcast : public QObject {
    Q_OBJECT
private slots:
    void af_gain_change_broadcasts_rx_volume_for_that_slice_only();
    void master_volume_change_emits_volume_but_no_rx_volume();

private:
    // Drain every queued notification out of the server's protocol.
    static QStringList drain(TciServer& server)
    {
        QStringList frames;
        TciProtocol* p = server.protocolForTest();
        if (!p) {
            return frames;
        }
        p->drainCoalescedNotifications();
        while (p->hasPendingNotification()) {
            frames << p->takePendingNotification();
        }
        return frames;
    }

    static QStringList only(const QStringList& frames, const QString& prefix)
    {
        QStringList out;
        for (const QString& f : frames) {
            if (f.startsWith(prefix)) {
                out << f;
            }
        }
        return out;
    }
};

// Moving one slice's AF gain must broadcast rx_volume for THAT receiver, and
// must not touch any other receiver's slots.
void TestTciRxVolumeBroadcast::af_gain_change_broadcasts_rx_volume_for_that_slice_only()
{
    RadioModel model;
    TciServer  server(&model);

    // A bare RadioModel starts with NO slices -- addSlice() creates the first
    // one.  Two are needed for "only that slice" to mean anything.  Ids come
    // back from addSlice(); they are ids, never list positions (removeSlice
    // does not renumber survivors), so never assume id == position here.
    const int slice0Id = model.addSlice();
    const int slice1Id = model.addSlice();
    QVERIFY2(slice0Id != slice1Id, "addSlice() must hand out distinct ids");

    SliceModel* slice0 = model.sliceById(slice0Id);
    SliceModel* slice1 = model.sliceById(slice1Id);
    QVERIFY(slice0);
    QVERIFY(slice1);
    QVERIFY2(slice0 != slice1, "sliceById() resolved both ids to one slice");

    drain(server);  // discard anything queued by construction / addSlice

    // 25/100 -> 20 * log10(0.25) = -12.0412 dB -> "-12.04" at F2.  Distinct
    // from slice 0's untouched default so a cross-slice leak is visible.
    slice1->setAfGain(25);

    const QStringList frames  = drain(server);
    const QStringList rxVol   = only(frames, QStringLiteral("rx_volume:"));
    const QString     expectDb =
        QString::number(tciAudioGainToDb(25 / 100.0), 'f', 2);

    QVERIFY2(!rxVol.isEmpty(),
        "SliceModel::afGainChanged must drive an rx_volume broadcast "
        "(Thetis RxAfGainChanged, TCIServer.cs:1118-1124 [v2.10.3.15]); "
        "no rx_volume frame was emitted at all");

    // Both channels of the changed receiver report the same gain (no
    // sub-receiver model), matching the init-burst convention.
    QVERIFY2(rxVol.contains(QStringLiteral("rx_volume:%1,0,%2;")
                                .arg(slice1Id).arg(expectDb)),
        qPrintable(QStringLiteral("missing rx_volume:%1,0,%2; got: %3")
            .arg(slice1Id).arg(expectDb).arg(rxVol.join(QStringLiteral(" ")))));
    QVERIFY2(rxVol.contains(QStringLiteral("rx_volume:%1,1,%2;")
                                .arg(slice1Id).arg(expectDb)),
        qPrintable(QStringLiteral("missing rx_volume:%1,1,%2; got: %3")
            .arg(slice1Id).arg(expectDb).arg(rxVol.join(QStringLiteral(" ")))));

    // The other slice did not move, so none of its slots may be broadcast.
    const QString otherPrefix = QStringLiteral("rx_volume:%1,").arg(slice0Id);
    for (const QString& f : rxVol) {
        QVERIFY2(!f.startsWith(otherPrefix),
            qPrintable(QStringLiteral("receiver %1 gain did not change but was "
                "broadcast anyway: %2 (all rx_volume frames: %3)")
                .arg(slice0Id).arg(f).arg(rxVol.join(QStringLiteral(" ")))));
    }

    // Exactly the two slots of the one receiver that moved.
    QCOMPARE(rxVol.size(), 2);
}

// The master AF slider maps to Thetis's OnVolumeChanged, which calls only
// sendVolume.  It must not spray rx_volume across every receiver slot.
void TestTciRxVolumeBroadcast::master_volume_change_emits_volume_but_no_rx_volume()
{
    RadioModel model;
    TciServer  server(&model);

    AudioEngine* audio = model.audioEngine();
    QVERIFY(audio);

    drain(server);

    audio->setVolume(0.25f);

    const QStringList frames = drain(server);

    QVERIFY2(!only(frames, QStringLiteral("volume:")).isEmpty(),
        "master volume change must still emit the volume: line "
        "(Thetis OnVolumeChanged -> sendVolume, TCIServer.cs:7806-7817 "
        "[v2.10.3.15])");

    const QStringList rxVol = only(frames, QStringLiteral("rx_volume:"));
    QVERIFY2(rxVol.isEmpty(),
        qPrintable(QStringLiteral("master volume must NOT emit rx_volume; "
            "Thetis drives rx_volume from the per-rx RXGainChangedHandlers "
            "event only. Got: %1").arg(rxVol.join(QStringLiteral(" ")))));
}

QTEST_GUILESS_MAIN(TestTciRxVolumeBroadcast)
#include "tst_tci_rx_volume_broadcast.moc"

#else
// WebSockets not available — test file must still compile and produce a
// no-op binary so CTest doesn't report a missing executable.
int main() { return 0; }
#endif // HAVE_WEBSOCKETS
