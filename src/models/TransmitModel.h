// =================================================================
// src/models/TransmitModel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-26 — tunePowerByBand[14] + per-MAC persistence (G.3, Phase 3M-1a)
//                 ported by J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-27 — micGainDb (int) + derived micPreampLinear (double) (C.1, Phase 3M-1b)
//                 ported by J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-27 — VOX properties: voxEnabled / voxThresholdDb / voxGainScalar /
//                 voxHangTimeMs (C.3, Phase 3M-1b) ported by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//   2026-04-27 — MON properties: monEnabled / monitorVolume (C.5, Phase 3M-1b)
//                 ported by J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-28 — micSource (MicSource::Pc/Radio) property (I.1, Phase 3M-1b)
//                 NereusSDR-native Setup UI property, J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#pragma once

#include "Band.h"
#include "core/audio/CompositeTxMicRouter.h"

#include <QObject>
#include <QString>
#include <array>
#include <atomic>
#include <cmath>

namespace NereusSDR {

// VAX slot: which audio source owns the transmitter.
// MicDirect = hardware mic, Vax1–Vax4 = virtual audio crossbar slots.
enum class VaxSlot {
    None = 0,
    MicDirect,
    Vax1,
    Vax2,
    Vax3,
    Vax4
};

QString vaxSlotToString(VaxSlot s);
VaxSlot vaxSlotFromString(const QString& s);

// Transmit state management.
// Includes MOX, tune, TX frequency, power, mic gain, and PureSignal state.
class TransmitModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool   mox       READ isMox       WRITE setMox       NOTIFY moxChanged)
    Q_PROPERTY(bool   tune      READ isTune      WRITE setTune      NOTIFY tuneChanged)
    Q_PROPERTY(int    power     READ power       WRITE setPower     NOTIFY powerChanged)
    Q_PROPERTY(float  micGain   READ micGain     WRITE setMicGain   NOTIFY micGainChanged)
    Q_PROPERTY(bool   pureSig   READ pureSigEnabled WRITE setPureSigEnabled NOTIFY pureSigChanged)

public:
    explicit TransmitModel(QObject* parent = nullptr);
    ~TransmitModel() override;

    bool isMox() const { return m_mox; }
    void setMox(bool mox);

    bool isTune() const { return m_tune; }
    void setTune(bool tune);

    int power() const { return m_power; }
    void setPower(int power);

    float micGain() const { return m_micGain; }
    void setMicGain(float gain);

    bool pureSigEnabled() const { return m_pureSigEnabled; }
    void setPureSigEnabled(bool enabled);

    VaxSlot txOwnerSlot() const { return m_txOwnerSlot.load(std::memory_order_acquire); }
    void setTxOwnerSlot(VaxSlot s);

    void loadFromSettings();

    // ── Per-band tune power (G.3) ─────────────────────────────────────────
    //
    // Porting from Thetis console.cs:12094 [v2.10.3.13]:
    //   private int[] tunePower_by_band;
    //
    // Default 50W per band on first init:
    //   console.cs:1819-1820 [v2.10.3.13]:
    //     tunePower_by_band = new int[(int)Band.LAST];
    //     for (int i = 0; i < (int)Band.LAST; i++) tunePower_by_band[i] = 50;
    //
    // NereusSDR uses scalar per-band AppSettings keys instead of Thetis's
    // pipe-delimited string (console.cs:3087-3091 save, :4904-4910 restore).

    /// Return the tune-power value (watts) for the given band.
    /// Default 50W on first init.  Returns 50 as a safe fallback for
    /// out-of-range band values.
    int tunePowerForBand(Band band) const;

    /// Set the tune-power value (watts) for the given band.
    /// Clamped to [0, 100].  Emits tunePowerByBandChanged when the value
    /// actually changes.  No-op for out-of-range band values.
    void setTunePowerForBand(Band band, int watts);

    /// Set the per-MAC AppSettings scope.  Must be called before load() / save().
    /// Mirrors the AlexController::setMacAddress() pattern.
    void setMacAddress(const QString& mac);

    /// Restore all per-band tune-power values from AppSettings under the
    /// current MAC scope.  No-op when no MAC has been set.
    /// Cite: console.cs:4904-4910 [v2.10.3.13] — Thetis pipe-delimited
    /// format; NereusSDR uses per-band scalar keys.
    void load();

    /// Flush all per-band tune-power values to AppSettings under the
    /// current MAC scope.  No-op when no MAC has been set.
    /// Cite: console.cs:3087-3091 [v2.10.3.13].
    void save();

    // ── Mic gain (3M-1b C.1) ──────────────────────────────────────────────
    //
    // Porting from Thetis console.cs:28805-28817 [v2.10.3.13]:
    //   private void setAudioMicGain(double gain_db)
    //   {
    //       if (chkMicMute.Checked) // although it is called chkMicMute, checked = mic in use
    //       {
    //           Audio.MicPreamp = Math.Pow(10.0, gain_db / 20.0); // convert to scalar
    //           _mic_muted = false;
    //       }
    //       else
    //       {
    //           Audio.MicPreamp = 0.0;
    //           _mic_muted = true;
    //       }
    //   }
    //
    // C.1 implements the unconditional dB→linear path only.  The
    // chkMicMute-gated zero-fill is C.2's responsibility (micMute property
    // and mute-zeroing logic arrive in C.2).
    //
    // Default -6 dB is a NereusSDR-original safety addition (not from
    // Thetis — Thetis defaults vary by board).  Conservative starting
    // point against ALC overdrive at 100 W PA per plan §0 row 11.
    //
    // micPreampLinear is a derived read-only computed property; it
    // recomputes on every setMicGainDb() call and emits its own signal
    // for downstream subscribers (TxChannel::recomputeTxAPanelGain1
    // arrives in D.6).
    //
    // Range clamped to kMicGainDbMin / kMicGainDbMax.
    // Thetis console.cs:19151-19171 [v2.10.3.13] shows mic_gain_min = -40
    // and mic_gain_max = 10 as runtime defaults, but setup.designer.cs shows
    // the spinboxes allow Minimum = -96, Maximum = 70.  The NereusSDR model
    // uses [-50, 70] as a conservative fixed range per plan §C.1.
    //
    // TODO [3M-1b L.x]: read range from BoardCapabilities once per-board
    // mic-gain fields land (HL2 range differs from ANAN range).
    //
    // Persistence per-MAC arrives in L.2.

    /// Return the user-facing mic gain in dB.
    int    micGainDb()       const noexcept { return m_micGainDb; }
    /// Return the derived linear scalar: pow(10, micGainDb / 20).
    double micPreampLinear() const noexcept { return m_micPreampLinear; }

    // Hardcoded range per plan §C.1.
    // Thetis console.cs:19151-19171 [v2.10.3.13]: mic_gain_min = -40 /
    // mic_gain_max = 10 are runtime defaults; setup.designer.cs shows the
    // spinboxes allow down to -96 and up to 70.  NereusSDR uses [-50, 70].
    static constexpr int kMicGainDbMin = -50;
    static constexpr int kMicGainDbMax =  70;

    // ── Mic-jack flag properties (3M-1b C.2) ──────────────────────────────
    //
    // Porting from Thetis console.cs:13213-13260 [v2.10.3.13]:
    //   LineIn / LineInBoost / MicBoost / MicXlr property block;
    //   console.cs:28752 [v2.10.3.13]: MicMute "NOTE: although called
    //   MicMute, true = mic in use";
    //   console.cs:19757-19766 [v2.10.3.13]: MicPTTDisabled;
    //   setup.designer.cs:8683 [v2.10.3.13]: radOrionMicTip.Checked = true;
    //   setup.designer.cs:8779 [v2.10.3.13]: radOrionBiasOff.Checked = true.
    //
    // Wire-bit setters (RadioConnection::setMic*) arrive in Phase G.
    // Persistence per-MAC arrives in L.2.

    /// Mic mute toggle.  Counter-intuitive naming preserved from Thetis.
    ///
    /// **NOTE: although called MicMute, true = mic in use**
    /// (Thetis console.cs:28752 [v2.10.3.13] verbatim comment.)
    ///
    /// FALSE means the mic is muted (chkMicMute unchecked); TRUE means the
    /// mic is in use (chkMicMute checked).  The mute action is implemented
    /// via SetTXAPanelGain1(0) — see Phase D Task D.6 for the WDSP wiring.
    ///
    /// Default TRUE: from console.designer.cs:2029-2030 [v2.10.3.13]:
    ///   "Checked = true; CheckState = Checked"
    bool micMute() const noexcept { return m_micMute; }

    /// 20 dB hardware microphone preamp enable.
    /// From Thetis console.cs:13237 [v2.10.3.13]: private bool mic_boost = true;
    /// Default TRUE: 20 dB preamp on by default in Thetis.
    bool micBoost() const noexcept { return m_micBoost; }

    /// XLR jack select (Saturn G2 only; FALSE = 3.5mm TRS jack).
    /// From Thetis console.cs:13249 [v2.10.3.13]: private bool mic_xlr = true;
    /// Default TRUE: XLR selected on boards that have the XLR jack.
    /// HL2 / Hermes / Atlas have no XLR hardware; the UI gates this on
    /// BoardCapabilities::hasXlrMic (Phase I wiring).
    bool micXlr() const noexcept { return m_micXlr; }

    /// Line-in input select.  TRUE = use line-in instead of mic input.
    /// From Thetis console.cs:13213 [v2.10.3.13]: private bool line_in = false;
    /// Default FALSE: microphone input active by default.
    bool lineIn() const noexcept { return m_lineIn; }

    /// Line-in boost in dB.  Clamped to [kLineInBoostMin, kLineInBoostMax].
    /// From Thetis console.cs:13225 [v2.10.3.13]: private double line_in_boost = 0.0;
    /// Range from setup.designer.cs:46898-46907 [v2.10.3.13]:
    ///   udLineInBoost.Minimum = -34.5, udLineInBoost.Maximum = 12.0
    ///   (decoded from C# decimal int[4] format).
    double lineInBoost() const noexcept { return m_lineInBoost; }

    /// Mic jack tip/ring polarity.  TRUE = Tip is mic (NereusSDR intuitive).
    /// NereusSDR-original semantic; wire-bit polarity inversion happens at
    /// RadioConnection::setMicTipRing (Phase G).
    /// Thetis default: radOrionMicTip.Checked = true (setup.designer.cs:8683
    /// [v2.10.3.13]) → Tip selected by default.
    bool micTipRing() const noexcept { return m_micTipRing; }

    /// Mic jack bias voltage enable.
    /// NereusSDR-original: FALSE = bias off by default (safe default for
    /// dynamic microphones that don't need phantom power).
    /// Thetis default: radOrionBiasOff.Checked = true (setup.designer.cs:8779
    /// [v2.10.3.13]) → bias off by default.
    bool micBias() const noexcept { return m_micBias; }

    /// Mic jack PTT disable flag.  TRUE = mic PTT disabled, FALSE = enabled.
    /// Also counter-intuitive (Thetis-consistent): the flag name is "Disabled"
    /// but FALSE is the active/enabled state.
    /// From Thetis console.cs:19757 [v2.10.3.13]:
    ///   private bool mic_ptt_disabled = false;
    ///   ... NetworkIO.SetMicPTT(Convert.ToInt32(value));
    /// Default FALSE: PTT enabled by default (sensible safety default).
    bool micPttDisabled() const noexcept { return m_micPttDisabled; }

    // Line-in boost range constants.
    // From Thetis setup.designer.cs:46898-46907 [v2.10.3.13]:
    //   udLineInBoost.Minimum decoded from decimal{345,0,0,-2147418112} = -34.5
    //   udLineInBoost.Maximum decoded from decimal{12,0,0,0} = 12.0
    static constexpr double kLineInBoostMin = -34.5;
    static constexpr double kLineInBoostMax =  12.0;

    // ── Anti-VOX properties (3M-1b C.4) ──────────────────────────────────────
    //
    // Porting from Thetis audio.cs:446-454 [v2.10.3.13]:
    //   private static bool antivox_source_VAC = false;
    //   public static bool AntiVOXSourceVAC {
    //     get { return antivox_source_VAC; }
    //     set { antivox_source_VAC = value; cmaster.CMSetAntiVoxSourceWhat(); }
    //   }
    //
    // Porting from Thetis setup.designer.cs:44699-44728 [v2.10.3.13]:
    //   udAntiVoxGain.Minimum = decimal{60,0,0,-2147483648} = -60
    //   udAntiVoxGain.Maximum = decimal{60,0,0,0}           = +60
    //   (DecimalPlaces=1; display unit is x0.1 dB; NereusSDR stores as int dB.)
    //
    // Porting from Thetis setup.cs:18986-18989 [v2.10.3.13]:
    //   cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
    //   (WDSP wiring arrives in Phase H.3; model just stores + signals.)
    //
    // "VAC->VAX" rename: Thetis calls this antivox_source_VAC (Virtual Audio
    // Cable, i.e. the IVAC-mediated digital-mode TX path).  NereusSDR's
    // equivalent is VAX (Virtual Audio Crossbar -- different design, but same
    // conceptual role).  The rename keeps naming consistent with the rest of
    // the project.  The default (false = use local-RX) is identical to Thetis.
    //
    // AppSettings persistence arrives in Phase L.2.
    // WDSP wiring (SetAntiVOXGain + CMSetAntiVoxSourceWhat) arrives in Phase H.3.
    // Full VAX-source state machine (RX2 / dual-VAX source composition)
    // is deferred to 3M-3a.

    /// Anti-VOX gain in dB.  Clamped to [kAntiVoxGainDbMin, kAntiVoxGainDbMax].
    /// Default 0 dB -- NereusSDR-original safe starting point.
    /// (Thetis udAntiVoxGain designer default is 1.0 dB per
    ///  setup.designer.cs:44723-44727 [v2.10.3.13]: Value = decimal{10,0,0,0}
    ///  with DecimalPlaces=1.)
    ///
    /// Range from Thetis setup.designer.cs:44708-44717 [v2.10.3.13]:
    ///   udAntiVoxGain.Minimum = -60, udAntiVoxGain.Maximum = 60.
    int antiVoxGainDb() const noexcept { return m_antiVoxGainDb; }

    /// Anti-VOX source selector.
    /// false = use local-RX (default; matches Thetis
    ///         audio.cs:446 [v2.10.3.13]: antivox_source_VAC = false).
    /// true  = use VAX audio (NereusSDR-renamed; was VAC in Thetis).
    ///
    /// Path-agnostic anti-VOX (default false = local-RX) is implemented in
    /// Phase H.3 via CMSetAntiVoxSourceWhat port.  The full VAX-source state
    /// machine is deferred to 3M-3a.
    bool antiVoxSourceVax() const noexcept { return m_antiVoxSourceVax; }

    // Anti-VOX gain range constants.
    // From Thetis setup.designer.cs:44708-44717 [v2.10.3.13]:
    //   udAntiVoxGain.Minimum = decimal{60,0,0,-2147483648} = -60
    //   udAntiVoxGain.Maximum = decimal{60,0,0,0}           = +60
    //   (DecimalPlaces=1; display unit is x0.1 dB; NereusSDR stores as int dB.)
    static constexpr int kAntiVoxGainDbMin = -60;
    static constexpr int kAntiVoxGainDbMax =  60;

    // ── MON properties (3M-1b C.5) ────────────────────────────────────────
    //
    // Porting from Thetis audio.cs:406 [v2.10.3.13]:
    //   private bool mon = false;
    //
    // Porting from Thetis audio.cs:417 [v2.10.3.13]:
    //   cmaster.SetAAudioMixVol((void*)0, 0, WDSP.id(1, 0), 0.5);
    //   The 0.5 literal is a fixed mix coefficient in Thetis; NereusSDR
    //   repurposes it as the user-volume default for the monitorVolume property.
    //
    // monEnabled does NOT persist — plan §0 row 9: safety, loads OFF at
    // startup to prevent unexpected headphone audio.
    // monitorVolume DOES persist — AppSettings persistence arrives in Phase L.2.
    // AudioEngine integration (setTxMonitorEnabled / setTxMonitorVolume)
    // arrives in Phase E.2-E.3.  MOX-fold (RX-leak gate) in Phase E.4.
    //
    // monitorVolume range [kMonitorVolumeMin, kMonitorVolumeMax] = [0.0f, 1.0f]
    // (normalized volume scalar; no Thetis spinbox needed — master design and
    //  pre-code review §4.2 + §12.5 consistently use 0..1 normalized).

    /// MON (TX monitor) enable.  When true, TXA siphon (Stage::Sip1) audio
    /// is mixed into MasterMixer at monitorVolume during MOX so the user
    /// hears themselves.
    ///
    /// Default false.  Per plan §0 row 9, does NOT persist — loads OFF at
    /// startup (safety: prevents unexpected headphone audio).
    ///
    /// AudioEngine integration arrives in Phase E.2-E.3.
    /// MOX-fold (RX-leak gate) integrates with this in Phase E.4.
    ///
    /// Source: Thetis audio.cs:406 [v2.10.3.13] (mon default off).
    bool monEnabled() const noexcept { return m_monEnabled; }

    /// MON volume scalar (0.0..1.0).  Default 0.5f matches Thetis literal
    /// mix coefficient at audio.cs:417 [v2.10.3.13]:
    ///   cmaster.SetAAudioMixVol((void*)0, 0, WDSP.id(1, 0), 0.5);
    ///
    /// AppSettings persistence arrives in Phase L.2 (this property persists,
    /// unlike monEnabled).
    float monitorVolume() const noexcept { return m_monitorVolume; }

    // MON volume range constants.
    // Normalized volume scalar [0.0f, 1.0f].
    // 0.5f default matches Thetis audio.cs:417 [v2.10.3.13] literal coefficient.
    static constexpr float kMonitorVolumeMin = 0.0f;
    static constexpr float kMonitorVolumeMax = 1.0f;

    // ── VOX properties (3M-1b C.3) ────────────────────────────────────────
    //
    // Porting from Thetis audio.cs:167-202 [v2.10.3.13]:
    //   private static bool vox_enabled = false;
    //   public static bool VOXEnabled { get { return vox_enabled; } set { vox_enabled = value; ... } }
    //   private static float vox_gain = 1.0f;
    //   public static float VOXGain { get { return vox_gain; } set { vox_gain = value; } }
    //
    // Porting from Thetis console.cs:14707-14716 [v2.10.3.13]:
    //   public double VOXHangTime { get { return vox_hang_time; }
    //                               set { vox_hang_time = value;
    //                                     if (!IsSetupFormNull) SetupForm.VOXHangTime = (int)value; } }
    // Mapped to setup.cs:4865-4876 / setup.designer.cs:45005-45024 [v2.10.3.13]:
    //   udDEXPHold.Minimum=1, udDEXPHold.Maximum=2000, udDEXPHold.Value=500 ms.
    //
    // voxThresholdDb range from console.Designer.cs:6018-6019 [v2.10.3.13]:
    //   ptbVOX.Maximum=0, ptbVOX.Minimum=-80  (display unit is dB).
    //
    // WDSP wiring (SetDEXPRunVox, SetDEXPAttackThreshold, SetDEXPHoldTime)
    // arrives in Phase D and Phase H.
    // AppSettings persistence arrives in Phase L.2; voxEnabled does NOT persist
    // (safety: VOX always loads OFF — plan §0 row 8).
    //
    // voxGainScalar: Thetis has no explicit clamp on VOXGain; NereusSDR adds a
    // sane guard [0.0f, 100.0f].  A scalar of 0.0 disables the mic-boost
    // scaling effect; 100.0 is an extreme upper bound that avoids silent
    // overflow.  Callers should use values in [0.0f, 10.0f] for normal use.

    /// VOX enable toggle.  Default FALSE — safety rule: VOX always starts OFF
    /// (plan §0 row 8; prevents unintended TX on startup).
    ///
    /// Mode-gating (VOX fires only in voice modes) is wired in Phase H Task H.1
    /// via CMSetTXAVoxRun mode-gate logic.
    ///
    /// From Thetis audio.cs:167 [v2.10.3.13]:
    ///   private static bool vox_enabled = false;
    bool voxEnabled() const noexcept { return m_voxEnabled; }

    /// VOX detection threshold in dB.  Clamped to [kVoxThresholdDbMin, kVoxThresholdDbMax].
    /// Default −40 dB (NereusSDR-original conservative starting point; Thetis
    /// ptbVOX.Value defaults to −20 per console.Designer.cs:6024 [v2.10.3.13]).
    ///
    /// Range from console.Designer.cs:6018-6019 [v2.10.3.13]:
    ///   ptbVOX.Maximum = 0, ptbVOX.Minimum = -80
    int voxThresholdDb() const noexcept { return m_voxThresholdDb; }

    /// Mic-boost-aware VOX threshold scaler.  Applied in CMSetTXAVoxThresh when
    /// MicBoost is on: threshold *= voxGainScalar.  Clamped to
    /// [kVoxGainScalarMin, kVoxGainScalarMax].
    ///
    /// Full mic-boost-aware scaling is wired in Phase H Task H.2.
    ///
    /// From Thetis audio.cs:194 [v2.10.3.13]:
    ///   private static float vox_gain = 1.0f;
    ///
    /// Thetis has no explicit clamp on VOXGain.  NereusSDR adds a sane upper
    /// guard of 100.0f to prevent silent float overflow.
    float voxGainScalar() const noexcept { return m_voxGainScalar; }

    /// VOX hang time in milliseconds: delay from signal-drop to gain recovery.
    /// Clamped to [kVoxHangTimeMsMin, kVoxHangTimeMsMax].
    /// Default 500 ms (NereusSDR-original; matches Thetis udDEXPHold.Value=500
    /// per setup.designer.cs:45020-45024 [v2.10.3.13]).
    ///
    /// Full DEXP knob set (attack, release, hysteresis, expansion ratio) lives
    /// in the Setup DEXP page and is deferred to Phase 3M-3a-iii.
    ///
    /// From Thetis console.cs:14707 [v2.10.3.13] / setup.cs:4865 [v2.10.3.13].
    int voxHangTimeMs() const noexcept { return m_voxHangTimeMs; }

    // VOX threshold range constants.
    // From Thetis console.Designer.cs:6018-6019 [v2.10.3.13]:
    //   ptbVOX.Maximum = 0, ptbVOX.Minimum = -80
    static constexpr int kVoxThresholdDbMin = -80;
    static constexpr int kVoxThresholdDbMax =   0;

    // VOX gain scalar range constants.
    // NereusSDR sane guard; Thetis Audio.VOXGain has no explicit clamp.
    static constexpr float kVoxGainScalarMin =   0.0f;
    static constexpr float kVoxGainScalarMax = 100.0f;

    // VOX hang time range constants.
    // From Thetis setup.designer.cs:45005-45013 [v2.10.3.13]:
    //   udDEXPHold.Maximum = 2000, udDEXPHold.Minimum = 1  (units: ms)
    static constexpr int kVoxHangTimeMsMin =    1;
    static constexpr int kVoxHangTimeMsMax = 2000;

    // ── Mic source (3M-1b I.1) ────────────────────────────────────────────────
    //
    // NereusSDR-native property — Thetis bakes mic-source selection directly
    // into audio.cs rather than using the strategy pattern.  This property
    // drives AudioTxInputPage (Setup → Audio → TX Input) and is consumed by
    // CompositeTxMicRouter::setActiveSource() once that wiring lands in F.3.
    //
    // Default MicSource::Pc — PC microphone is always safe and available.
    // Radio is opt-in (unavailable on HL2: hasMicJack == false).
    //
    // AppSettings persistence (per-MAC) is deferred to Phase L.2.
    // CompositeTxMicRouter wiring arrives in Phase F.3.

    /// Active mic source: Pc (PC host-audio) or Radio (radio mic-jack).
    /// Default MicSource::Pc. Radio is gated by BoardCapabilities::hasMicJack.
    MicSource micSource() const noexcept { return m_micSource; }

public slots:
    void setMicGainDb(int dB);

    // ── Mic source setter (3M-1b I.1) ─────────────────────────────────────────
    /// Select the active mic source (Pc or Radio).  Idempotent: no signal
    /// when the value is unchanged.  AppSettings persistence deferred to L.2.
    void setMicSource(MicSource source);

    // ── Mic-jack flag setters (3M-1b C.2) ─────────────────────────────────
    void setMicMute(bool on);
    void setMicBoost(bool on);
    void setMicXlr(bool on);
    void setLineIn(bool on);
    void setLineInBoost(double dB);
    void setMicTipRing(bool tipIsMic);
    void setMicBias(bool on);
    void setMicPttDisabled(bool disabled);

    // ── Anti-VOX setters (3M-1b C.4) ─────────────────────────────────────────
    void setAntiVoxGainDb(int dB);
    void setAntiVoxSourceVax(bool useVax);

    // ── MON setters (3M-1b C.5) ──────────────────────────────────────────────
    void setMonEnabled(bool on);
    void setMonitorVolume(float volume);

    // ── VOX setters (3M-1b C.3) ────────────────────────────────────────────
    void setVoxEnabled(bool on);
    void setVoxThresholdDb(int dB);
    void setVoxGainScalar(float scalar);
    void setVoxHangTimeMs(int ms);

signals:
    void moxChanged(bool mox);
    void tuneChanged(bool tune);
    void powerChanged(int power);
    void micGainChanged(float gain);
    void pureSigChanged(bool enabled);
    void txOwnerSlotChanged(VaxSlot s);

    /// Emitted when a per-band tune-power value changes.
    void tunePowerByBandChanged(Band band, int watts);

    // ── Mic gain signals (3M-1b C.1) ──────────────────────────────────────
    /// Emitted when micGainDb changes (carries the clamped dB value).
    void micGainDbChanged(int dB);
    /// Emitted when micPreampLinear changes (carries the new linear scalar).
    void micPreampChanged(double linear);

    // ── Mic-jack flag signals (3M-1b C.2) ─────────────────────────────────
    void micMuteChanged(bool on);
    void micBoostChanged(bool on);
    void micXlrChanged(bool on);
    void lineInChanged(bool on);
    void lineInBoostChanged(double dB);
    void micTipRingChanged(bool tipIsMic);
    void micBiasChanged(bool on);
    void micPttDisabledChanged(bool disabled);

    // ── Anti-VOX signals (3M-1b C.4) ─────────────────────────────────────────
    void antiVoxGainDbChanged(int dB);
    void antiVoxSourceVaxChanged(bool useVax);

    // ── MON signals (3M-1b C.5) ──────────────────────────────────────────────
    void monEnabledChanged(bool on);
    void monitorVolumeChanged(float volume);

    // ── VOX signals (3M-1b C.3) ────────────────────────────────────────────
    void voxEnabledChanged(bool on);
    void voxThresholdDbChanged(int dB);
    void voxGainScalarChanged(float scalar);
    void voxHangTimeMsChanged(int ms);

    // ── Mic source signals (3M-1b I.1) ────────────────────────────────────────
    /// Emitted when micSource changes. Not emitted on idempotent calls.
    void micSourceChanged(MicSource source);

private:
    bool m_mox{false};
    bool m_tune{false};
    int m_power{100};
    float m_micGain{0.0f};
    bool m_pureSigEnabled{false};
    std::atomic<VaxSlot> m_txOwnerSlot{VaxSlot::MicDirect};  // Atomic for lock-free reads from the audio thread.

    // Per-band tune power storage.
    // From Thetis console.cs:12094 [v2.10.3.13]: private int[] tunePower_by_band;
    // Initialised to 50W per band in the constructor
    // (console.cs:1819-1820 [v2.10.3.13]).
    std::array<int, static_cast<std::size_t>(Band::Count)> m_tunePowerByBand{};

    // Per-MAC AppSettings scope (mirrors AlexController pattern).
    QString m_mac;

    // ── Mic gain (3M-1b C.1) ──────────────────────────────────────────────
    // From Thetis console.cs:28805-28817 [v2.10.3.13] (setAudioMicGain).
    // Default -6 dB per plan §0 row 11 (NereusSDR safety addition).
    // m_micPreampLinear is derived: pow(10, m_micGainDb / 20.0).
    int    m_micGainDb       = -6;
    // pow(10, -6/20) ≈ 0.501187233627272
    double m_micPreampLinear = std::pow(10.0, -6.0 / 20.0);

    // ── Mic-jack flag properties (3M-1b C.2) ──────────────────────────────
    // From Thetis console.cs:13213-13260 [v2.10.3.13] and related sources.
    // NOTE: m_micMute = true means the mic IS in use (Thetis counter-intuitive
    // naming preserved — see MicMute getter doc-comment above).
    bool   m_micMute        = true;   // console.designer.cs:2029-2030: Checked=true
    bool   m_micBoost       = true;   // console.cs:13237: mic_boost = true
    bool   m_micXlr         = true;   // console.cs:13249: mic_xlr = true
    bool   m_lineIn         = false;  // console.cs:13213: line_in = false
    double m_lineInBoost    = 0.0;    // console.cs:13225: line_in_boost = 0.0
    bool   m_micTipRing     = true;   // setup.designer.cs:8683: radOrionMicTip.Checked=true
    bool   m_micBias        = false;  // setup.designer.cs:8779: radOrionBiasOff.Checked=true
    bool   m_micPttDisabled = false;  // console.cs:19757: mic_ptt_disabled = false

    // ── Anti-VOX properties (3M-1b C.4) ──────────────────────────────────
    // From Thetis audio.cs:446-454 [v2.10.3.13] (antivox_source_VAC) and
    // setup.designer.cs:44699-44728 [v2.10.3.13] (udAntiVoxGain range -60..60).
    int  m_antiVoxGainDb    = 0;      // NereusSDR-original default; range [-60,60]
    bool m_antiVoxSourceVax = false;  // audio.cs:446: antivox_source_VAC = false

    // ── MON properties (3M-1b C.5) ────────────────────────────────────────
    // From Thetis audio.cs:406 [v2.10.3.13]: private bool mon = false;
    // From Thetis audio.cs:417 [v2.10.3.13]: SetAAudioMixVol(0.5) literal.
    // m_monEnabled intentionally NOT persisted — safety: MON loads OFF always
    // (plan §0 row 9).  m_monitorVolume persists (Phase L.2).
    bool  m_monEnabled     = false;  // audio.cs:406: mon = false
    float m_monitorVolume  = 0.5f;   // matches Thetis audio.cs:417 literal coefficient

    // ── VOX properties (3M-1b C.3) ──────────────────────────────────────
    // From Thetis audio.cs:167-202 [v2.10.3.13].
    // m_voxEnabled intentionally NOT persisted — safety: VOX loads OFF always.
    bool  m_voxEnabled     = false;  // audio.cs:167: vox_enabled = false
    int   m_voxThresholdDb = -40;    // NereusSDR-original default; ptbVOX range [-80,0]
    float m_voxGainScalar  = 1.0f;   // audio.cs:194: vox_gain = 1.0f
    int   m_voxHangTimeMs  = 500;    // udDEXPHold.Value=500 (setup.designer.cs:45020-45024)

    // ── Mic source (3M-1b I.1) ──────────────────────────────────────────
    // NereusSDR-native. Default Pc (always available; Radio is opt-in).
    // AppSettings persistence deferred to Phase L.2.
    MicSource m_micSource{MicSource::Pc};
};

} // namespace NereusSDR
