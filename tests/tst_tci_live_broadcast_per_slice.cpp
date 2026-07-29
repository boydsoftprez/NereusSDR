// no-port-check: NereusSDR-original test infrastructure.  The Thetis
// citations below are rationale for the wire behaviour being asserted, not
// ported code.
//
// Phase 3F Sub-Epic J follow-up (the two gaps commit 7195cc5a documented but
// deliberately did not fix, being outside Task 10's stated scope).
//
// 1. rx_volume.  Task 10 fixed the INIT BURST so each receiver reports its
//    own SliceModel::afGain, but TciServer's LIVE path still re-broadcast all
//    four rx_volume slots from the single radio-global afLinear on every
//    master-volume change.  So the per-slice fix collapsed back to one shared
//    value the moment the operator touched the AF slider after connect.
//    Thetis never does this: OnVolumeChanged (TCIServer.cs:7806-7817
//    [v2.10.3.15]) calls only sendVolume and never touches rx_volume.  The
//    real per-rx source is a separate event, console.RXGainChangedHandlers ->
//    OnRxAfGainChanged (TCIServer.cs:6780 + 7722-7733 [v2.10.3.15]), whose
//    listener emits one rx_volume frame for whichever receiver's gain
//    actually changed (RxAfGainChanged, TCIServer.cs:1118-1124 [v2.10.3.15]).
//
// 2. rx_anf_enable.  Its live broadcast piggybacked on activeNrChanged, which
//    stopped covering pure ANF toggles once Sub-Epic J Task 1 gave ANF its own
//    SliceModel::anfEnabled property, independent of the NR slot enum.
//
// Both are asserted on the real TciServer broadcast queue rather than on
// signal-receiver counts, because the defect is in frame CONTENT (which
// receiver a frame addresses), not in whether a connect exists.

#include <QtTest/QtTest>

#include "core/TciServer.h"
#include "core/TciProtocol.h"
#include "core/AudioEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Drain every queued notification frame.  TciServer pushes broadcasts into
// TciProtocol's pending-notification queue; the 5 ms drain timer would hand
// them to clients, but for assertions the queue itself is the wire.
QStringList drainFrames(TciProtocol* p)
{
    QStringList out;
    while (p && p->hasPendingNotification()) {
        out << p->takePendingNotification();
    }
    return out;
}

QStringList framesWithPrefix(const QStringList& frames, const QString& prefix)
{
    QStringList out;
    for (const QString& f : frames) {
        if (f.startsWith(prefix)) {
            out << f;
        }
    }
    return out;
}

} // namespace

class TstTciLiveBroadcastPerSlice : public QObject
{
    Q_OBJECT

private slots:

    // The core defect.  Two receivers, each with its own AF gain.  Changing
    // receiver B's gain must put B's value on the wire against B's index and
    // must not restate A's.
    void af_gain_change_broadcasts_only_the_changed_receiver()
    {
        RadioModel model;
        TciServer  server(&model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        const int b = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = model.sliceById(a);
        SliceModel* sb = model.sliceById(b);
        QVERIFY(sa && sb);

        TciProtocol* proto = server.protocolForTest();
        QVERIFY(proto);

        sa->setAfGain(30);
        sb->setAfGain(70);
        drainFrames(proto);  // discard the setup traffic

        // Move only B.
        sb->setAfGain(80);
        const QStringList frames = drainFrames(proto);
        const QStringList rxVol  = framesWithPrefix(frames, QStringLiteral("rx_volume:"));

        QVERIFY2(!rxVol.isEmpty(),
                 "a per-slice afGain change must broadcast rx_volume live; "
                 "before this fix nothing was wired to afGainChanged at all");

        // Every frame emitted must address receiver B.  Receiver A did not
        // change, so restating it is the exact collapse this fixes.
        for (const QString& f : rxVol) {
            QVERIFY2(f.startsWith(QStringLiteral("rx_volume:%1,").arg(b)),
                     qPrintable(QStringLiteral(
                         "rx_volume frame '%1' addresses a receiver other than "
                         "the one whose gain changed (%2)").arg(f).arg(b)));
        }

        // And A's gain must be untouched in the model.
        QCOMPARE(sa->afGain(), 30);
    }

    // Thetis's OnVolumeChanged calls ONLY sendVolume (TCIServer.cs:7806-7817
    // [v2.10.3.15]).  The master AF slider is a separate console field from
    // the per-receiver gains, so moving it must not restate any rx_volume.
    void master_volume_change_does_not_restate_per_receiver_volume()
    {
        RadioModel model;
        TciServer  server(&model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        const int b = model.addSlice(QStringLiteral("pan-0"));
        model.sliceById(a)->setAfGain(30);
        model.sliceById(b)->setAfGain(70);

        AudioEngine* audio = model.audioEngine();
        QVERIFY(audio);
        TciProtocol* proto = server.protocolForTest();
        QVERIFY(proto);

        drainFrames(proto);

        audio->setVolume(0.25f);
        const QStringList frames = drainFrames(proto);

        QVERIFY2(!framesWithPrefix(frames, QStringLiteral("volume:")).isEmpty(),
                 "master volume must still broadcast the volume: line");

        const QStringList rxVol = framesWithPrefix(frames, QStringLiteral("rx_volume:"));
        QVERIFY2(rxVol.isEmpty(),
                 qPrintable(QStringLiteral(
                     "master volume must not restate rx_volume (Thetis "
                     "OnVolumeChanged calls only sendVolume); got: %1")
                     .arg(rxVol.join(QStringLiteral(" ")))));
    }

    // Sub-Epic J Task 1 decoupled anfEnabled from the NR slot enum, which left
    // the live rx_anf_enable broadcast hanging off activeNrChanged, a signal
    // that no longer fires on a pure ANF toggle.
    void anf_toggle_broadcasts_live_on_its_own_signal()
    {
        RadioModel model;
        TciServer  server(&model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        const int b = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sb = model.sliceById(b);
        QVERIFY(model.sliceById(a) && sb);

        TciProtocol* proto = server.protocolForTest();
        QVERIFY(proto);
        drainFrames(proto);

        // Toggle ANF alone.  No NR slot change, so activeNrChanged is silent.
        sb->setAnfEnabled(true);
        const QStringList frames = drainFrames(proto);
        const QStringList anf = framesWithPrefix(frames, QStringLiteral("rx_anf_enable:"));

        QVERIFY2(anf.contains(QStringLiteral("rx_anf_enable:%1,true;").arg(b)),
                 qPrintable(QStringLiteral(
                     "a pure ANF toggle must broadcast rx_anf_enable for its "
                     "own receiver; got: %1").arg(frames.join(QStringLiteral(" ")))));

        // And back off again.
        drainFrames(proto);
        sb->setAnfEnabled(false);
        const QStringList offFrames = drainFrames(proto);
        QVERIFY(framesWithPrefix(offFrames, QStringLiteral("rx_anf_enable:"))
                    .contains(QStringLiteral("rx_anf_enable:%1,false;").arg(b)));
    }
};

QTEST_MAIN(TstTciLiveBroadcastPerSlice)
#include "tst_tci_live_broadcast_per_slice.moc"
