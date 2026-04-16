// tst_slice_audio_panel.cpp
//
// Verifies that SliceModel mute / audio pan / binaural setters store + emit
// signals correctly. SliceModel is the single source of truth for VFO/DSP state.
// Setters must:
//   1. Guard against no-op updates (unchanged value → no signal)
//   2. Store the new value
//   3. Emit the corresponding changed signal
//
// Source citations:
//   From Thetis Project Files/Source/Console/dsp.cs:393-394 — PanelRun
//   From Thetis Project Files/Source/Console/radio.cs:1386-1403 — Pan
//   From Thetis Project Files/Source/Console/radio.cs:1145-1162 — BinOn
//   WDSP: third_party/wdsp/src/patchpanel.c:126,159,187

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceAudioPanel : public QObject {
    Q_OBJECT

private slots:
    // ── muted ────────────────────────────────────────────────────────────────

    void mutedDefaultIsFalse() {
        SliceModel s;
        QCOMPARE(s.muted(), false);
    }

    void setMutedStoresValue() {
        SliceModel s;
        s.setMuted(true);
        QCOMPARE(s.muted(), true);
    }

    void setMutedEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::mutedChanged);
        s.setMuted(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setMutedNoSignalOnSameValue() {
        SliceModel s;
        s.setMuted(true);
        QSignalSpy spy(&s, &SliceModel::mutedChanged);
        s.setMuted(true);   // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void setMutedToggleRoundTrip() {
        SliceModel s;
        s.setMuted(true);
        s.setMuted(false);
        QCOMPARE(s.muted(), false);
    }

    // ── audioPan ─────────────────────────────────────────────────────────────

    void audioPanDefaultIsZero() {
        // Neutral default: 0.0 = center. Thetis default pan=0.5 (WDSP scale).
        SliceModel s;
        QCOMPARE(s.audioPan(), 0.0);
    }

    void setAudioPanStoresValue() {
        SliceModel s;
        s.setAudioPan(-0.5);
        QCOMPARE(s.audioPan(), -0.5);
    }

    void setAudioPanEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::audioPanChanged);
        s.setAudioPan(0.5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toDouble(), 0.5);
    }

    void setAudioPanNoSignalOnSameValue() {
        SliceModel s;
        s.setAudioPan(0.0);   // same as default → no signal
        QSignalSpy spy(&s, &SliceModel::audioPanChanged);
        s.setAudioPan(0.0);
        QCOMPARE(spy.count(), 0);
    }

    void setAudioPanFullLeft() {
        SliceModel s;
        s.setAudioPan(-1.0);
        QCOMPARE(s.audioPan(), -1.0);
    }

    void setAudioPanFullRight() {
        SliceModel s;
        s.setAudioPan(1.0);
        QCOMPARE(s.audioPan(), 1.0);
    }

    // ── binauralEnabled ──────────────────────────────────────────────────────

    void binauralEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1145-1162 — bin_on = false (dual-mono default)
        SliceModel s;
        QCOMPARE(s.binauralEnabled(), false);
    }

    void setBinauralEnabledStoresValue() {
        SliceModel s;
        s.setBinauralEnabled(true);
        QCOMPARE(s.binauralEnabled(), true);
    }

    void setBinauralEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::binauralEnabledChanged);
        s.setBinauralEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setBinauralEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setBinauralEnabled(true);
        QSignalSpy spy(&s, &SliceModel::binauralEnabledChanged);
        s.setBinauralEnabled(true);   // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void setBinauralEnabledToggleRoundTrip() {
        SliceModel s;
        s.setBinauralEnabled(true);
        s.setBinauralEnabled(false);
        QCOMPARE(s.binauralEnabled(), false);
    }
};

QTEST_MAIN(TestSliceAudioPanel)
#include "tst_slice_audio_panel.moc"
