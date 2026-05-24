// no-port-check: NereusSDR-original wrapper class.  Thetis manages
// PureSignal via a Form (PSForm) directly bound to a static `puresignal`
// helper class on the WDSP P/Invoke side; there is no separate "PureSignal
// coordinator" in Thetis — the form IS the coordinator.  NereusSDR splits
// the host-side coordination (cal lifecycle, MOX integration, polling, save
// /restore, two-tone wiring) into this class so the UI surfaces (PsForm,
// PureSignalApplet, TxApplet [PS-A], PsaIndicatorWidget) can subscribe to
// model signals without each surface re-implementing the cmd-state machine.
//
// Behavior is faithful to PSForm.cs and the puresignal helper class:
//   - timer1code (PSForm.cs:555-728 [v2.10.3.13]) — 7-state cmd-state
//     machine + GetInfo polling + corrections-applied / correcting / cal-
//     count flags + feedback-colour fade.
//   - timer2code (PSForm.cs:729-790 [v2.10.3.13]) — 3-state eAAState
//     auto-attention machine that drives StepAttenuatorController::
//     setAttOnTxValue when the feedback level is out of the 128-181 range.
//   - puresignal helper (PSForm.cs:1060-1162 [v2.10.3.13]) — derived
//     properties (FeedbackLevel = info[4], CorrectionsBeingApplied =
//     info[14] == 1, CalibrationCount = info[5], Correcting = FeedbackLevel
//     > 90, FeedbackColourLevel range thresholds).
//
// =================================================================
// src/core/PureSignal.h  (NereusSDR)
// =================================================================
//
// PureSignal coordinator: cal lifecycle + MOX integration + auto-attention
// + status polling + save/restore + two-tone integration.  Ports the host-
// side logic from Thetis PSForm.cs timer1code [v2.10.3.13] and the
// puresignal helper class.  Calcc/iqc DSP runs autonomously inside WDSP
// (calcc.c:617 pscc() [v2.10.3.13]); this class is configuration +
// monitoring only.
//
// Source:
//   - PSForm.cs:90-122           (eCMDState + eAAState enums)
//   - PSForm.cs:271-289           (AutoCalEnabled property)
//   - PSForm.cs:425-545           (btnPSCalibrate / btnPSReset / btnPSSave
//                                  / btnPSRestore / btnPSTwoToneGen
//                                  handlers; SetDefaultPeaks)
//   - PSForm.cs:555-728           (timer1code — main polling loop +
//                                  cmd-state machine)
//   - PSForm.cs:729-790           (timer2code — eAAState transitions)
//   - PSForm.cs:920-955           (ForcePS body)
//   - PSForm.cs:1060-1162         (puresignal helper class properties)
//   - calcc.c:888 / 900           (PSSaveCorr / PSRestoreCorr exports)
//   - console.cs:43703            (ForcePureSignalAutoCalDisable)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Created by J.J. Boyd (KG4VCF) for Phase 3M-4 Task 7
//                 PureSignal coordinator, with AI-assisted source-first
//                 protocol via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <cstring>

#include "BoardCapabilities.h"

namespace NereusSDR {

class WdspEngine;
class TxChannel;
class PsFeedbackChannel;
class MoxController;
class StepAttenuatorController;
class TwoToneController;

// PureSignal coordinator.  Owned by RadioModel via std::unique_ptr.
//
// Construction-time deps that DON'T change after connect:
//   - WdspEngine, MoxController, StepAttenuatorController, TwoToneController
//
// Late-bound deps (set after the WDSP-init lambda fires in connectToRadio):
//   - TxChannel  (created by WdspEngine when channel 1 opens)
//   - PsFeedbackChannel (created by WdspEngine when channel 5 opens)
//
// Thread safety: all public methods must be called from the main thread.
// The internal timers fire on the main thread.  Status getters are atomic
// for cheap cross-thread reads (e.g. SpectrumWidget LED indicator).
class PureSignal : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool autoCalEnabled READ isAutoCalEnabled WRITE setAutoCalEnabled NOTIFY autoCalEnabledChanged)
    // Codex Fix D: split predicates per Thetis PSForm.cs:1100-1108 [v2.10.3.13]:
    //   CorrectionsBeingApplied { _info[14] == 1; }   → gates Save / CO badge
    //   Correcting              { FeedbackLevel > 90; } → Lime vs Yellow CO state
    // The legacy code emitted correctingChanged for both predicates, so Save
    // could enable from feedback level alone (without corrections) and the CO
    // badge could never reach the Yellow "applied-but-not-correcting" state.
    Q_PROPERTY(bool correctionsApplied READ correctionsBeingApplied NOTIFY correctionsBeingAppliedChanged)
    Q_PROPERTY(bool correcting READ isCorrecting NOTIFY correctingChanged)
    Q_PROPERTY(int feedbackLevel READ feedbackLevel NOTIFY feedbackLevelChanged)
    Q_PROPERTY(int calibrationCount READ calibrationCount NOTIFY calibrationCountChanged)
    Q_PROPERTY(bool invertRedBlue READ invertRedBlue WRITE setInvertRedBlue NOTIFY invertRedBlueChanged)
    Q_PROPERTY(bool hideFeedback READ hideFeedback WRITE setHideFeedback NOTIFY hideFeedbackChanged)

public:
    // From Thetis PSForm.cs:97-122 [v2.10.3.13] — eAAState enum.
    enum class AutoAttenuateState {
        Monitor          = 0,
        SetNewValues     = 1,
        RestoreOperation = 2
    };

    explicit PureSignal(WdspEngine* engine,
                        TxChannel* tx,
                        PsFeedbackChannel* fb,
                        MoxController* mox,
                        StepAttenuatorController* stepAtt,
                        TwoToneController* twoTone,
                        QObject* parent = nullptr);
    ~PureSignal() override;

    // ── Late-bound dependency setters ──────────────────────────────────────
    //
    // Both TxChannel and PsFeedbackChannel come up in the WDSP-init lambda,
    // AFTER PureSignal is constructed.  RadioModel calls these setters as
    // soon as the relevant pointers go live.  Pass nullptr to clear on
    // teardown (so a late timer tick after disconnect is a no-op).
    void setTxChannel(TxChannel* tx);
    void setPsFeedbackChannel(PsFeedbackChannel* fb);

    // ── Cal lifecycle ──────────────────────────────────────────────────────

    // PSForm.cs btnPSCalibrate_Click [v2.10.3.13] — kicks off a single
    // calibration.  Sets _singlecalON which the cmd-state machine picks up
    // on the next tick to transition through eCMDState::TurnOnSingleCalibrate
    // -> SingleCalibrate -> StayON.
    void singleCalibrate();

    // Master enable.  PSForm.cs:202-269 PSEnabled property [v2.10.3.13]
    // backs SetPureSignal(0/1) on the connection layer plus calcc setup.
    // For the coordinator we drive the cmd-state machine into a known
    // state.  setEnabled(false) ≡ PSForm.cs btnPSReset_Click (full OFF).
    void setEnabled(bool enabled);
    bool isEnabled() const noexcept { return m_enabled; }

    // Auto-cal toggle (Thetis PSForm.cs:272-289 AutoCalEnabled property
    // [v2.10.3.13]).  setAutoCalEnabled(true) sets _autoON=true and
    // PSState=true; setAutoCalEnabled(false) sets _OFF=true and
    // PSState=false.  The next cmd-state tick acts on those flags.
    void setAutoCalEnabled(bool on);
    bool isAutoCalEnabled() const noexcept { return m_autoCalEnabled; }

    // ForcePS body — PSForm.cs:924-954 [v2.10.3.13].  Re-issues
    // SetPSControl based on _autoON (single 1,0,0,0 vs auto 0,0,1,0)
    // plus all the state-transfer setter calls (LoopDelay / TXDelay /
    // MoxDelay / RelaxPtol / AutoAttenuate / Pin / Map / Stbl / Tint /
    // OnTop / QuickAttenuate / Show2ToneMeasurements).  The coordinator
    // implements the SetPSControl half — the persisted-state pushes are
    // handled by the UI surfaces (PsForm) wiring AppSettings → setter
    // chain at task 11+.
    void forcePS();

    // PSForm.cs btnPSReset_Click [v2.10.3.13] — full OFF: _OFF=true,
    // PSState=false.  The state machine drains to eCMDState::OFF on the
    // next tick.
    void reset();

    // console.cs:43703 ForcePureSignalAutoCalDisable [v2.10.3.13] —
    // toggles the [PS-A] checkbox off, which fans out via
    // chkFWCATUBypass_CheckedChanged to AutoCalEnabled=false.  Provided
    // here as a convenience that calls setAutoCalEnabled(false).
    void forceAutoCalDisable();

    // PSForm.cs SetDefaultPeaks [v2.10.3.13] — pushes the per-board
    // psDefaultPeak constant into the UI text box AND through to
    // SetPSHWPeak.  We call setPSHWPeak directly with the value previously
    // set via applyBoardCapabilities.
    void setDefaultPeaks();

    // ── Save / restore (file-based, user-chosen path) ──────────────────────
    //
    // Returns false when m_tx is null (e.g. before the WDSP-init lambda
    // wires it).  filename should be a full filesystem path.  Calcc spawns
    // a detached thread for the actual disk I/O — true is returned as soon
    // as the thread is dispatched.
    bool saveCorrections(const QString& filename);
    bool restoreCorrections(const QString& filename);

    // ── Two-tone integration ───────────────────────────────────────────────
    //
    // PsForm.cs btnPSTwoToneGen_Click [v2.10.3.13] toggles
    // SetupForm.TTgenrun.  In NereusSDR the TwoToneController owns the
    // activation / deactivation orchestrator (PSForm.cs route is just a
    // pass-through to console.SetupForm.TTgenrun).  PureSignal::setTwoToneOn
    // forwards to TwoToneController::setActive when wired; otherwise
    // it's a no-op (test-friendly).
    void setTwoToneOn(bool on);

    // ── Status reads ───────────────────────────────────────────────────────
    //
    // All four atomic so the SpectrumWidget LED indicator (cross-thread)
    // and any other UI subscriber can read without locks.

    // From Thetis PSForm.cs:1120-1122 [v2.10.3.13]:
    //   public static int FeedbackLevel { get { return _info[4]; } }
    int feedbackLevel() const noexcept { return m_feedbackLevel.load(); }

    // From Thetis PSForm.cs:1100-1102 [v2.10.3.13]:
    //   public static bool CorrectionsBeingApplied
    //     { get { return _info[14] == 1; } }
    bool correctionsBeingApplied() const noexcept { return m_correctionsApplied.load(); }

    // From Thetis PSForm.cs:1106-1108 [v2.10.3.13]:
    //   public static bool Correcting { get { return FeedbackLevel > 90; } }
    bool isCorrecting() const noexcept { return m_correcting.load(); }

    // From Thetis PSForm.cs:1116-1119 [v2.10.3.13]:
    //   public static bool IsFeedbackLevelOKRange
    //     { get { return FeedbackLevel > 128 && FeedbackLevel <= 181; } }
    // Tighter window than IsFeedbackLevelOK (`<= 256`).  Used by Codex Fix
    // E's single-cal retry branch (PSForm.cs:692 [v2.10.3.13]) to decide
    // whether the converged FB landed in the calcc happy-path band; if not,
    // retry up to 5 times before giving up.
    bool isFeedbackLevelOKRange() const noexcept {
        const int fb = m_feedbackLevel.load();
        return fb > 128 && fb <= 181;
    }

    // From Thetis PSForm.cs:1103-1105 [v2.10.3.13]:
    //   public static int CalibrationCount { get { return _info[5]; } }
    int calibrationCount() const noexcept { return m_calCount.load(); }

    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): expose raw info[i] so the
    // PsForm dialog can update all 9 calibration labels per Thetis
    // PSForm.cs:561-573 timer1code [v2.10.3.13]:
    //   lblPSInfo0.Text = puresignal.Info[0].ToString();   // bldr.rx
    //   lblPSInfo1.Text = puresignal.Info[1].ToString();   // bldr.cm
    //   lblPSInfo2.Text = puresignal.Info[2].ToString();   // bldr.cc
    //   lblPSInfo3.Text = puresignal.Info[3].ToString();   // bldr.cs
    //   lblPSfb2.Text   = puresignal.FeedbackLevel.ToString();   // info[4]
    //   lblPSInfo5.Text = puresignal.CalibrationCount.ToString();// info[5]
    //   lblPSInfo6.Text = puresignal.Info[6].ToString();   // sln.chk
    //   lblPSInfo13.Text = puresignal.Info[13].ToString(); // dg.cnt
    //   lblPSInfo15.Text = puresignal.Info[15].ToString(); // state
    // Pre-fix PsForm only wired lblPSInfo5 (CalibrationCount); the other
    // 8 fields stayed visually stuck at 0 because nothing pushed info[]
    // values to them.  Bench-confirmed by JJ 2026-05-23 16:34 — the
    // PsForm calibration grid showed state/sln.chk/dg.cnt all 0 while
    // the engine log clearly showed corrApplied=1 windows running.
    // Returns 0 for out-of-range indices.
    int infoAt(int idx) const noexcept {
        return (idx >= 0 && idx < 16) ? m_info[idx] : 0;
    }

    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): Thetis PSForm.cs:614
    // timer1code [v2.10.3.13]:
    //   lblPSHWPeak.Text = puresignal.HWPeak.ToString("F4");
    // where HWPeak getter wraps GetPSHWPeak.  Pre-fix our PsForm
    // m_lblGetPSpeak was created but never updated.
    double getHwPeak() const;

#ifdef NEREUS_BUILD_TESTS
    // Test seam (PR #212 follow-up bench fix, J.J. KG4VCF, 2026-05-07):
    // simulate calcc completing a calibration cycle so unit tests can drive
    // the autoAttentionTick gate (curCalCount != m_aaLastSeenCalCount).
    // Production code populates m_calCount via pollTimerTick from WDSP's
    // info[5] — tests don't have WDSP available so they bump it directly.
    void setCalCountForTest(int n) { m_calCount.store(n); }
#endif

    // From Thetis PSForm.cs:1123-1138 [v2.10.3.13] — FeedbackColourLevel:
    //   FB > 181 → DodgerBlue (or Red when InvertRedBlue)
    //   FB > 128 → Lime
    //   FB > 90  → Yellow
    //   else     → Red (or DodgerBlue when InvertRedBlue)
    QColor feedbackColour() const;

    // ── UI mirror state (no DSP effect) ────────────────────────────────────
    //
    // Both back the equivalent FB-label left/right click behaviors (Task
    // 10's PsaIndicatorWidget) AND the Setup → General → Options checkboxes
    // (Task 11).  Persisted by RadioModel via the per-MAC AppSettings
    // namespace at task time.

    bool invertRedBlue() const noexcept { return m_invertRedBlue; }
    void setInvertRedBlue(bool on);

    bool hideFeedback() const noexcept { return m_hideFeedback; }
    void setHideFeedback(bool on);

    // ── Calibration option setters (PsForm-driven, Task 8) ────────────────
    //
    // Thin pass-throughs to TxChannel calcc setters.  Each caches the value
    // for read-back (so PsForm can sync controls on open) and forwards to
    // the WDSP entry when m_tx is wired.  These mirror the chk* / ud*
    // controls in PSForm.designer.cs [v2.10.3.13].  Defaults match the
    // Thetis designer values verbatim:
    //   pin             default true   (PSForm.designer.cs:210-211)
    //   map             default true   (PSForm.designer.cs:193-194)
    //   stabilize       default false  (PSForm.designer.cs:177)
    //   autoAttenuate   default true   (PSForm.designer.cs:227-228)
    //   relaxTolerance  default false  (PSForm.designer.cs:257)
    //   quickAttenuate  default false  (PSForm.designer.cs:809)
    //   moxDelay        default 0.2    (PSForm.designer.cs:368-372 —
    //                                   decimal{2,0,0,65536} = 2 with scale 1)
    //   calDelay        default 0.0    (PSForm.designer.cs:801-805)
    //   ampDelay        default 150    (PSForm.designer.cs:409-413)
    //   tint            default 0.5    (PSForm.designer.cs:172)
    //   loopback        default false  (PSForm.designer.cs:466-479)
    //   show2Tone       default false  (PSForm.designer.cs:846-857)
    bool pinMode()        const noexcept { return m_pinMode; }
    bool mapMode()        const noexcept { return m_mapMode; }
    bool stabilize()      const noexcept { return m_stabilize; }
    bool autoAttenuate()  const noexcept { return m_autoAttenuate; }
    bool relaxTolerance() const noexcept { return m_relaxTolerance; }
    bool quickAttenuate() const noexcept { return m_quickAttenuate; }
    double moxDelay()     const noexcept { return m_moxDelay; }
    double calDelay()     const noexcept { return m_calDelay; }
    int    ampDelay()     const noexcept { return m_ampDelay; }
    double tint()         const noexcept { return m_tint; }
    bool loopback()       const noexcept { return m_loopback; }
    bool show2ToneMeasurements() const noexcept { return m_show2Tone; }
    double hwPeak()       const noexcept { return m_hwPeak; }

    // From Thetis PSForm.cs:chkPSPin_CheckedChanged [v2.10.3.13] →
    // puresignal.SetPSPinMode(_txachannel, chkPSPin.Checked).
    void setPinMode(bool on);
    // From Thetis PSForm.cs:chkPSMap_CheckedChanged [v2.10.3.13] →
    // puresignal.SetPSMapMode(_txachannel, chkPSMap.Checked).
    void setMapMode(bool on);
    // From Thetis PSForm.cs:chkPSStbl_CheckedChanged [v2.10.3.13] →
    // puresignal.SetPSStabilize(_txachannel, chkPSStbl.Checked).
    void setStabilize(bool on);
    // From Thetis PSForm.cs:chkPSAutoAttenuate_CheckedChanged [v2.10.3.13] —
    // toggles auto-attention behaviour; UI-only, no direct WDSP setter.
    void setAutoAttenuate(bool on);
    // From Thetis PSForm.cs:chkPSRelaxPtol_CheckedChanged [v2.10.3.13] →
    // puresignal.SetPSPtol(_txachannel, chkPSRelaxPtol.Checked ? 0.8 : 0.4).
    // Designer tooltip "Allow for more dynamic variation in feedback".
    void setRelaxTolerance(bool on);
    // From Thetis PSForm.cs:chkQuickAttenuate_CheckedChanged [v2.10.3.13]
    // PSForm.cs:958-961 — QuickAttenuate property mirror; drives the
    // auto-attention timer cadence.  UI-only, no direct WDSP setter.
    void setQuickAttenuate(bool on);
    // From Thetis PSForm.cs:493-496 udPSMoxDelay_ValueChanged [v2.10.3.13]
    // → puresignal.SetPSMoxDelay(_txachannel, value).
    void setMoxDelay(double seconds);
    // From Thetis PSForm.cs:498-501 udPSCalWait_ValueChanged [v2.10.3.13]
    // → puresignal.SetPSLoopDelay(_txachannel, value).
    void setCalDelay(double seconds);
    // From Thetis PSForm.cs:503-506 udPSPhnum_ValueChanged [v2.10.3.13]
    // → puresignal.SetPSTXDelay(_txachannel, value * 1.0e-09).  Stored as
    // ns int because the spinbox is integer-valued; conversion to seconds
    // happens at the WDSP boundary.
    void setAmpDelay(int ns);
    // From Thetis PSForm.cs:857-885 [v2.10.3.13]
    // comboPSTint_SelectedIndexChanged — combo entries from
    // PSForm.designer.cs:164-167 [v2.10.3.13] are "0.5", "1.1", "2.5".
    //
    // Codex Fix F: setTint(double db) is the legacy public API the existing
    // PsForm wiring uses.  Per Codex review on PR #212, the pre-fix
    // implementation only stored m_tint and emitted tintChanged(db) — the
    // calcc engine call SetPSIntsAndSpi was deferred and AmpView used the
    // hardcoded default (16, 256) regardless of selection.  Post-fix:
    // setTint(double) maps the dB label to the matching index (0.5→0,
    // 1.1→1, 2.5→2) and routes through setTintIndex.  Out-of-range
    // double values fall back to index 0 (Thetis default branch at
    // PSForm.cs:879-884).
    void setTint(double db);

    // Codex Fix F: setTintIndex(int idx) is the post-fix entry point that
    // mirrors comboPSTint_SelectedIndexChanged byte-for-byte.  Routes:
    //   idx 0 → setPSIntsAndSpi(16, 256), saveRestoreEnabled = true
    //   idx 1 → setPSIntsAndSpi(8, 512),  saveRestoreEnabled = false
    //   idx 2 → setPSIntsAndSpi(4, 1024), saveRestoreEnabled = false
    //   default → mirrors idx 0 (PSForm.cs:879-884 default case).
    // Updates m_psInts / m_psSpi cache + m_saveRestoreEnabled flag, emits
    // tintChanged / saveRestoreEnabledChanged on transitions.
    void setTintIndex(int idx);

    // Read-back of the current TINT combo index (0..2).  Used by
    // PsForm to sync the combo's currentIndex on dialog open.  Default 0
    // matches Thetis PSForm.designer.cs:172 [v2.10.3.13]:
    //   this.comboPSTint.Text = "0.5";
    int tintIndex() const noexcept { return m_tintIndex; }

    // Codex Fix F: Save/Restore button enabled-state mirror.  Per Thetis
    // PSForm.cs:865/871/877/883 [v2.10.3.13], only TINT index 0 (default
    // 16/256 buffer dimensions) keeps the persisted-corrections file format
    // compatible — other intervals produce stored corrections that can't
    // be loaded back.  PsForm subscribes to saveRestoreEnabledChanged and
    // gates btnPSSave / btnPSRestore.
    bool saveRestoreEnabled() const noexcept { return m_saveRestoreEnabled; }
    // From Thetis PSForm.cs:checkLoopback_CheckedChanged [v2.10.3.13] —
    // routes feedback streams to the panadapter.  UI-only here; the
    // panadapter wire-through is Task 13.
    void setLoopback(bool on);
    // From Thetis PSForm.cs:968-971 chkShow2ToneMeasurements_CheckedChanged
    // [v2.10.3.13] → Display.ShowIMDMeasurments = checked.  UI-only here;
    // the IMD-overlay wire-through is Task 12.
    void setShow2ToneMeasurements(bool on);
    // From Thetis PSForm.cs:792-803 PSpeak_TextChanged [v2.10.3.13] →
    // puresignal.SetPSHWPeak(_txachannel, value).  Stored for UI read-back.
    // (//MW0LGE attribution preserved — author tag from upstream
    // //[2.10.3.7]MW0LGE version-stamped comment at PSForm.cs:802.)
    void setHwPeak(double peak);

    // ── Per-board defaults ─────────────────────────────────────────────────
    //
    // Pushes psDefaultPeak + psSampleRate from BoardCapabilities (Task 1)
    // into the feedback channel and the calcc HW peak.  Called from
    // RadioModel::onConnected when the per-board codec applies its DDC
    // config.
    void applyBoardCapabilities(const BoardCapabilities& caps);

    // ── AmpView buffer feed (Task 9) ──────────────────────────────────────
    //
    // Thetis defaults are ints=16, spi=256 (PSForm.cs:351-369 [v2.10.3.13]).
    // The comboPSTint handler at PSForm.cs:857-885 [v2.10.3.13] mutates them
    // when the user changes the index.  Codex Fix F (post-fix) wires the
    // setTintIndex setter to push (ints, spi) through TxChannel::
    // setPSIntsAndSpi, so AmpView's buffer dimensions track the user's TINT
    // selection live.  AmpView reads back via the accessors below.
    int    psInts() const noexcept { return m_psInts; }
    int    psSpi()  const noexcept { return m_psSpi;  }

    // Pull the seven raw GetPSDisp output buffers from the wrapped
    // TxChannel.  The caller MUST ensure the pointers address arrays of:
    //   x  / ym / yc / ys → at least psInts() * psSpi() doubles
    //   cm / cc / cs      → at least psInts() * 4         doubles
    // Returns true when the buffers were filled (TxChannel wired + WDSP
    // GetPSDisp executed); false when no TX channel is bound (e.g. before
    // a radio connects, in unit tests).
    //
    // From Thetis AmpView.cs:371-392 [v2.10.3.13] — the unsafe { fixed }
    // block inside timer1_Tick that pins the seven managed double[] arrays
    // and forwards their addresses into puresignal.GetPSDisp.  NereusSDR
    // doesn't need GC pinning (raw double* are passed by AmpViewWindow),
    // so the wrapper is the simple pass-through below.
    //
    // Inline tag preservation (per CLAUDE.md §"Inline comment preservation"):
    // upstream AmpView.cs:397 carries
    //   //disp_data(); // MW0LGE [2.9.0.8] changed to an add once, update points method.
    // — explanatory tag for a refactor that NereusSDR follows by structure
    // (we never had the pre-refactor disp_data path).
    bool fillAmpViewBuffers(double* x,  double* ym, double* yc, double* ys,
                            double* cm, double* cc, double* cs);

    // ── State accessors for tests ─────────────────────────────────────────
    AutoAttenuateState autoAttenuateState() const noexcept { return m_aaState; }

    // ── Test seam ─────────────────────────────────────────────────────────
    //
    // pollTimerTick() and autoAttentionTick() are public for unit testing
    // (Qt's invokeMethod or direct call from tst_puresignal_coordinator).
    // Production code uses the QTimer signals; tests stop the timers via
    // setTimersEnabled(false) and drive the ticks manually.
    void setTimersEnabled(bool on);

    // Test seam — pollTimerTick() reads info[] from m_tx->getPSInfo().
    // Tests bypass that read by passing a synthetic info[] directly into
    // processNewInfo().  Mirrors the Thetis split where the coordinator
    // could in principle be tested with a stubbed GetInfo (PSForm.cs:1076-
    // 1085 [v2.10.3.13] — GetInfo is the only call that touches WDSP, the
    // rest of timer1code operates on the cached _info / _oldInfo arrays).
    void processNewInfo(const int newInfo[16]);

public slots:
    void onMoxChanged(bool mox);
    void pollTimerTick();        // PSForm.cs:555-728 [v2.10.3.13] timer1code
    void autoAttentionTick();    // PSForm.cs:729-790 [v2.10.3.13] timer2code

signals:
    void enabledChanged(bool);
    void autoCalEnabledChanged(bool);
    // Codex Fix C: psEnabledChanged is the radio/DDC fan-out signal that
    // ports Thetis PSForm.cs:235-269 PSEnabled property setter
    // [v2.10.3.13].  Emitted by the cmd-state machine on every PSEnabled
    // transition (TurnOnAutoCalibrate / TurnOnSingleCalibrate /
    // IntiateRestoredCorrection / StayON / TurnOFF).  Subscribers
    // (ReceiverManager::setPureSignalEnabled, RadioConnection::
    // setPuresignalRun, StepAttenuatorController::setPsActive) connect to
    // THIS signal — not autoCalEnabledChanged — so Single Cal / Restore /
    // Stay-on / Turn-off paths fire the radio/DDC routing alongside the
    // setPSRunCal calcc-side update.  autoCalEnabledChanged stays bound to
    // the PS-A button visual state.
    void psEnabledChanged(bool);
    // Codex Fix D: distinct predicates per Thetis PSForm.cs:1100-1108
    // [v2.10.3.13].  correctingChanged fires only on FeedbackLevel > 90
    // crossings.  correctionsBeingAppliedChanged fires only on _info[14]
    // toggles.  See Q_PROPERTY block above for the split rationale.
    void correctingChanged(bool);
    void correctionsBeingAppliedChanged(bool);
    void feedbackLevelChanged(int);
    void calibrationCountChanged(int);
    void feedbackColourChanged(QColor);
    void invertRedBlueChanged(bool);
    void hideFeedbackChanged(bool);
    void calibrationStarted();
    void calibrationComplete(bool success);
    void feedbackError(const QString& message);

    // ── Phase 3M-4 Task 13: applet-driven LED + correction-gauge signals ──
    //
    // calStateChanged carries the raw EngineState (info[15]) value from
    // GetPSInfo on every poll tick where the value differs from the prior
    // tick.  Subscribers (PureSignalApplet) translate the value into Cal LED
    // (LSETUP=3 / LCOLLECT=4 / LCALC=6) and Run LED (LSTAYON=8) state.
    //
    // correctionPeakChanged carries the calcc HW peak (TxChannel::getPSHWPeak)
    // when it differs from the prior poll by more than 0.001.  Subscribers
    // map the raw [0..1] envelope into the 0..100 PureSignalApplet correction
    // gauge.  Source: NereusSDR-native — Thetis exposes the value via the
    // PSpeak text box (PSForm.cs:792-803 PSpeak_TextChanged [v2.10.3.13]) but
    // not as a coordinated signal; we add the signal seam here so the Phase
    // 3M-4 applet can bind without polling its own timer.
    //
    // feedbackActiveChanged fires when the predicate (m_correcting && MOX is
    // up) flips.  Subscribers (PureSignalApplet Fbk LED) light up while
    // feedback samples are flowing through PsFeedbackChannel into calcc.
    void calStateChanged(int engineState);
    void correctionPeakChanged(double peak);
    void feedbackActiveChanged(bool active);

    // ── Phase 3M-4 bench-fix Round 2: consolidated PSInfo dispatch ────────
    //
    // psInfoChanged carries the same 5 fields that Thetis's
    // ucInfoBar.PSInfo(int level, bool ok, bool corrApplied, bool calChanged,
    // Color colour) takes (ucInfoBar.cs:808-825 [v2.10.3.13]).  Per Thetis
    // PSForm.cs:614-619 [v2.10.3.13] timer1code:
    //   if (_autocal_enabled)
    //       if (puresignal.HasInfoChanged)
    //           console.InfoBarFeedbackLevel(...);   // → ucInfoBar.PSInfo
    // The signal is emitted only when BOTH gates pass, so subscribers
    // (PsaIndicatorWidget) get the atomic 5-field bundle the way
    // ucInfoBar.PSInfo expects.  Per-field signals above stay live for
    // PsForm and PureSignalApplet which use them differently.
    void psInfoChanged(int level, bool feedbackLevelOk,
                       bool correctionsBeingApplied,
                       bool calibrationAttemptsChanged,
                       const QColor& feedbackColour);

    // ── Calibration option change signals (Task 8) ─────────────────────────
    void pinModeChanged(bool);
    void mapModeChanged(bool);
    void stabilizeChanged(bool);
    void autoAttenuateChanged(bool);
    void relaxToleranceChanged(bool);
    void quickAttenuateChanged(bool);
    void moxDelayChanged(double);
    void calDelayChanged(double);
    void ampDelayChanged(int);
    void tintChanged(double);
    // Codex Fix F: per-index combo signal (the dB-double signal above stays
    // live for backward compat with PsForm wiring + tests).  Both fire on
    // index transitions via setTintIndex / setTint.
    void tintIndexChanged(int index);
    // Codex Fix F: Save/Restore enabled-state mirror.  Bound by PsForm to
    // gate btnPSSave / btnPSRestore enabled-state.  Mirrors PSForm.cs:865/
    // 871/877/883 [v2.10.3.13]: only index 0 (16/256) keeps Save+Restore on.
    void saveRestoreEnabledChanged(bool enabled);
    void loopbackChanged(bool);
    void show2ToneMeasurementsChanged(bool);
    void hwPeakChanged(double);

private:
    // From Thetis PSForm.cs:79-89 [v2.10.3.13] — eCMDState enum.  The
    // cmd-state machine is the brain of timer1code.  Transitions are driven
    // by the input flags _autoON / _singlecalON / _restoreON / _OFF.
    enum class CommandState {
        Off                       = 0,
        TurnOnAutoCalibrate       = 1,
        AutoCalibrate             = 2,
        TurnOnSingleCalibrate     = 3,
        SingleCalibrate           = 4,
        StayOn                    = 5,
        TurnOff                   = 6,
        InitiateRestoredCorrection = 7
    };

    // From Thetis PSForm.cs:1140-1151 [v2.10.3.13] — info[15] enum.
    enum class EngineState {
        LRESET = 0,
        LWAIT,
        LMOXDELAY,
        LSETUP,
        LCOLLECT,
        MOXCHECK,
        LCALC,
        LDELAY,
        LSTAYON,
        LTURNON
    };

    // PSForm.cs:1086-1095 HasInfoChanged [v2.10.3.13] — true if any of
    // info[0..15] differs from the previous snapshot.
    bool hasInfoChanged(const int* current16) const;

    QColor computeFeedbackColour(int level) const;

    // Construction-time non-owning pointers
    WdspEngine* m_engine;
    TxChannel* m_tx;
    PsFeedbackChannel* m_fb;
    MoxController* m_mox;
    StepAttenuatorController* m_stepAtt;
    TwoToneController* m_twoTone;

    QTimer m_pollTimer;          // 100 ms — drives pollTimerTick (timer1code)
    QTimer m_autoAttTimer;       // 100 ms — drives autoAttentionTick (timer2code)

    // Cmd-state-machine flags (Thetis PSForm.cs:51-56 + 56 [v2.10.3.13]):
    //   private static bool _autoON = false;
    //   private static bool _singlecalON = false;
    //   private static bool _restoreON = false;
    //   private static bool _OFF = true;
    bool m_autoON{false};
    bool m_singleCalON{false};
    bool m_restoreON{false};
    bool m_OFF{true};
    CommandState m_cmdState{CommandState::Off};
    AutoAttenuateState m_aaState{AutoAttenuateState::Monitor};

    // Auto-attention scratch values (PSForm.cs:64-66 [v2.10.3.13]):
    //   private int _save_autoON = 0;
    //   private int _save_singlecalON = 0;
    //   private int _deltadB = 0;
    int m_saveAutoOn{0};
    int m_saveSingleCalOn{0};
    int m_deltaDb{0};

    // Phase 3M-4 Task 17 fix: track calCount across autoAttentionTick
    // invocations so we only act ONCE per calcc cycle (mirrors Thetis
    // PSForm.cs:735 [v2.10.3.13] `puresignal.CalibrationAttemptsChanged`
    // guard).  Without this, the 100 ms timer fires multiple times per
    // calcc cycle on transient fbLevel readings, computing fresh
    // deltaDb on stale data → ATT oscillates 0↔31.
    //
    // PR #212 follow-up bench fix (J.J. KG4VCF, 2026-05-07): initialize to
    // 0 (matching calcc.c calCount initial value) instead of -1 so the
    // FIRST auto-att tick is properly gated by the calCount-unchanged
    // check.  Pre-fix: sentinel -1 made the first tick fire spuriously
    // when calcc had never advanced past LIDLE — fbLevel=0 → deltaDb=-10
    // (HL2 -∞ sentinel from PureSignal.cpp:1392) → SATT=-10 dB → HL2
    // firmware applies ~20 dB TX-path attenuation → DDC3 envelope halved
    // to ~0.117 → calcc can't fill bins 8-15 → LCOLLECT stalls forever.
    // Mirrors Thetis PSForm.cs:1097 NeedToRecalibrate gate where
    // _info[5] and _oldInfo[5] both start at 0 (calcc's binfo[5] init
    // value via calloc'd array), so the first 0==0 tick is properly
    // skipped.  HL2 in particular is sensitive because its 0→-10 first-
    // tick response transitions through enough TX attenuation to drop
    // the feedback ADC envelope below the threshold needed for calcc's
    // LCOLLECT bin-fill across all 16 amplitude bins.
    int m_aaLastSeenCalCount{0};

    // Codex Fix E: single-cal retry tracking.  Mirrors Thetis PSForm.cs:
    // 553-554 [v2.10.3.13]:
    //   private bool _performing_single_cal = false;
    //   private int _performing_single_cal_retries = 0;
    // Set true in TurnOnSingleCalibrate (PSForm.cs:660), cleared in StayOn
    // (PSForm.cs:691).  Retries cap at 5 (PSForm.cs:692-698) — the typical
    // trigger is AutoAttenuate adjusting ATT mid-cal, which resets calcc
    // and re-issues single-cal.  After 5 attempts of bad-feedback, the
    // counter resets and the user must manually re-trigger another cal.
    bool m_performingSingleCal{false};
    int  m_performingSingleCalRetries{0};

    // Master enable + auto-cal mirrors (drive Q_PROPERTY signals)
    bool m_enabled{false};
    bool m_autoCalEnabled{false};

    // Codex Fix C: PSEnabled mirror for the cmd-state machine's per-
    // transition fan-out.  Ports the static `_psenabled` field at Thetis
    // PSForm.cs:234 [v2.10.3.13].  Updated only by setPsEnabledWithFanOut.
    bool m_psEnabled{false};

    // Codex Fix C helper — emit psEnabledChanged on a PSEnabled flip.
    // Mirrors the entry-condition check in each cmd-state case
    // (PSForm.cs:634/646/662/678/705/720 [v2.10.3.13]):
    //   if (!PSEnabled) PSEnabled = true;
    //   if (PSEnabled)  PSEnabled = false;
    // Runs the same idempotency guard so repeat-flips with the same value
    // stay silent on the wire.
    void setPsEnabledWithFanOut(bool on);

    // Atomic status mirrors (cross-thread cheap reads)
    std::atomic<int>  m_feedbackLevel{0};
    std::atomic<bool> m_correctionsApplied{false};
    std::atomic<bool> m_correcting{false};
    std::atomic<int>  m_calCount{0};

    // UI mirror state
    bool m_invertRedBlue{false};
    bool m_hideFeedback{false};

    // ── Calibration option cache (Task 8 PsForm-driven) ────────────────────
    // Defaults match Thetis PSForm.designer.cs [v2.10.3.13] verbatim.
    bool   m_pinMode{true};         // chkPSPin default Checked
    bool   m_mapMode{true};         // chkPSMap default Checked
    bool   m_stabilize{false};      // chkPSStbl default unchecked
    bool   m_autoAttenuate{true};   // chkPSAutoAttenuate default Checked
    bool   m_relaxTolerance{false}; // chkPSRelaxPtol default unchecked
    bool   m_quickAttenuate{false}; // chkQuickAttenuate default unchecked
    // Defaults verified against Thetis PSForm.designer.cs [v2.10.3.13]:
    //   udPSMoxDelay.Value = decimal(2, 0, 0, 65536) = 2 with scale=1 → 0.2 sec
    //                        (designer.cs:368-372)
    //   udPSPhnum.Value    = decimal(150, 0, 0, 0)            = 150 ns
    //                        (designer.cs:409-413)
    //   udPSCalWait.Value  = decimal(0, 0, 0, 0)              = 0.0 sec
    //                        (designer.cs:801-805)
    // PureSignal::applyBoardCapabilities() pushes these to WDSP at init —
    // matching the bulk push Thetis does via udPS*_ValueChanged(this, e) at
    // PSForm.cs:942-944 [v2.10.3.13].  Without that push, WDSP runs with its
    // own zero defaults (calcc.c:190-205 txdelay=0/rxdelay=0; calcc state
    // machine moxdelay=0) — calcc converges with mis-aligned TX/FB samples,
    // baking a ~120° constant phase rotation into the iqc LUT (cc[0]/cs[0])
    // and producing correction that doesn't reduce IMD3 on the wire.
    double m_moxDelay{0.2};         // udPSMoxDelay default 0.2 sec
    double m_calDelay{0.0};         // udPSCalWait default 0.0 sec
    int    m_ampDelay{150};         // udPSPhnum default 150 ns
    double m_tint{0.5};             // comboPSTint default "0.5"
    // Codex Fix F: per-index combo state mirror — m_tint stays as the
    // dB-label cache for backward compat (legacy setTint/tintChanged API),
    // m_tintIndex is the post-fix integer index (0..2) that drives the
    // (ints, spi) pair via setTintIndex.  Default 0 ("0.5" dB → 16/256)
    // matches PSForm.designer.cs:172 [v2.10.3.13].
    int    m_tintIndex{0};
    // Codex Fix F: Save/Restore enabled-state mirror.  PSForm.cs:865 default
    // for case 0 is true; m_tintIndex starts at 0 so this starts true.
    // PSForm.cs:871/877 disable for indexes 1 / 2; the default branch
    // (PSForm.cs:879-884) re-enables.
    bool   m_saveRestoreEnabled{true};
    bool   m_loopback{false};       // checkLoopback default unchecked
    bool   m_show2Tone{false};      // chkShow2ToneMeasurements default unchecked
    double m_hwPeak{0.0};           // populated by applyBoardCapabilities

    // ── AmpView buffer-sizing (Task 9) ────────────────────────────────────
    // Defaults match Thetis PSForm.cs:351 / 361 [v2.10.3.13]:
    //   private int _ints = 16;
    //   private int _spi  = 256;
    // The comboPSTint handler (PSForm.cs:857-885 [v2.10.3.13]) cycles
    // between (16,256) / (8,512) / (4,1024); Codex Fix F now wires
    // setTintIndex / setTint into setPSIntsAndSpi(ints, spi) so AmpView's
    // buffer dimensions track the user's TINT selection live.
    int m_psInts{16};
    int m_psSpi{256};

    // Per-tick info[] snapshots for HasInfoChanged equivalence.  Sized 16
    // per Thetis _info / _oldInfo layout (PSForm.cs:1061-1062 [v2.10.3.13]).
    int m_info[16] = {};
    int m_oldInfo[16] = {};

    // ── Phase 3M-4 Task 13: applet-driven signal change-detection ─────────
    // Per-tick caches so emit calStateChanged / correctionPeakChanged /
    // feedbackActiveChanged only fire on actual state transitions.  Tied to
    // the new public signals above; not part of the Thetis port.
    int    m_lastEngineState{-1};         // -1 sentinel forces first emit
    double m_lastCorrectionPeak{-1.0};    // -1 sentinel forces first emit
    bool   m_lastFeedbackActive{false};

    BoardCapabilities m_caps;
};

} // namespace NereusSDR
