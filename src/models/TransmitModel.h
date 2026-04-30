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
//   2026-04-28 — AppSettings per-MAC persistence for 15 mic/VOX/MON properties
//                 (L.2, Phase 3M-1b): loadFromSettings(mac) / persistToSettings(mac)
//                 + auto-persist on each setter via persistOne().
//                 NereusSDR-native persistence glue, J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//   2026-04-28 — setMicSourceLocked(bool) lock guard (L.3, Phase 3M-1b): HL2
//                 force-Pc-on-connect model-side lock. When locked,
//                 setMicSource(MicSource::Radio) silently coerces to Pc.
//                 NereusSDR-native, J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-04-28 — Two-tone test properties (B.2, Phase 3M-1c): TwoToneFreq1 /
//                 TwoToneFreq2 / TwoToneLevel / TwoTonePower /
//                 TwoToneFreq2Delay / TwoToneInvert / TwoTonePulsed (7x).
//                 Defaults follow option C — Thetis Designer for Freq1/Freq2/
//                 Invert and ranges, NereusSDR-original safer for Level/Power.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-28 — DrivePowerSource enum + TwoToneDrivePowerSource property
//                 (B.3, Phase 3M-1c): full 3-value enum (DriveSlider /
//                 TuneSlider / Fixed) ported from Thetis enums.cs:456-461;
//                 default DriveSlider per console.cs:46553 [v2.10.3.13].
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

// Drive-power source for the two-tone IMD test (and TUN button).
//
// From Thetis enums.cs:456-461 [v2.10.3.13]:
//   public enum DrivePowerSource { DRIVE_SLIDER = 0, TUNE_SLIDER = 1, FIXED = 2 }
//
// Selects which power slider drives the radio during a two-tone test:
//   DriveSlider — the regular drive-power slider (PWR).
//   TuneSlider  — the dedicated tune-power slider (matches TUN behaviour).
//   Fixed       — Setup-page-fixed power; saves PWR pre-MOX, applies the
//                 fixed value during the test, restores PWR on stop.
//
// Default is DriveSlider per Thetis console.cs:46553 [v2.10.3.13]:
//   private DrivePowerSource _2ToneDrivePowerSource = DRIVE_SLIDER;
//
// Phase 3M-1c B.3 ports the enum + a TransmitModel property; the actual
// power-source-driven MOX behaviour wires up in Phase I (two-tone handler).
enum class DrivePowerSource : int {
    DriveSlider = 0,  ///< Drive (PWR) slider
    TuneSlider  = 1,  ///< Tune slider
    Fixed       = 2,  ///< Setup-page-fixed power; saves+restores PWR
};

QString drivePowerSourceToString(DrivePowerSource s);
DrivePowerSource drivePowerSourceFromString(const QString& s);

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

    // ── Per-MAC mic/VOX/MON persistence (3M-1b L.2) ──────────────────────
    //
    // NereusSDR-native persistence glue.  Persists 15 mic/VOX/MON properties
    // under hardware/<mac>/tx/<key>.  Three properties are intentionally
    // excluded (per plan §0 rows 8/9):
    //   - voxEnabled  → always loads false (safety: VOX always starts OFF)
    //   - monEnabled  → always loads false (safety: MON always starts OFF)
    //   - micMute     → always loads true  (safety: mic in use on startup)
    //
    // The auto-persist pattern: once loadFromSettings(mac) is called, every
    // property setter invokes persistOne(key, value) to write the new value
    // immediately.  This means no explicit flush is needed on each change;
    // persistToSettings(mac) is bulk-write used at disconnect as insurance.
    //
    // Key namespace: hardware/<mac>/tx/<propertyName>
    // (consistent with hardware/<mac>/tunePowerByBand/ and cal/ namespaces).

    /// Load 15 per-MAC mic/VOX/MON properties from AppSettings.
    /// Sets m_persistMac so subsequent setters auto-persist changes.
    /// voxEnabled, monEnabled, and micMute are NEVER loaded — they always
    /// start at their safety defaults regardless of any stored value.
    /// micGainDb defaults to -6 on first run (plan §0 row 11).
    void loadFromSettings(const QString& mac);

    /// Bulk-write all 15 persisted mic/VOX/MON properties to AppSettings.
    /// (Excluding voxEnabled, monEnabled, micMute — never persisted.)
    /// Used at disconnect as defense-in-depth; auto-persist should have
    /// flushed each change already.
    void persistToSettings(const QString& mac) const;

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

    // ── PC Mic session state (3M-1b I.2) ─────────────────────────────────────
    //
    // NereusSDR-native transient session state for the PC Mic configuration
    // group (Setup → Audio → TX Input → PC Mic group box).
    //
    // These three properties survive Setup dialog close/reopen within the
    // same session but are NOT persisted across app restarts — AppSettings
    // persistence is deferred to Phase L.2.
    //
    // pcMicHostApiIndex: PortAudio host API index (-1 = PA default; on
    //   macOS this will be the CoreAudio index, on Linux PipeWire/Pulse,
    //   on Windows WASAPI).  Default -1 means "let AudioEngine pick the
    //   OS default" — the Setup page UI resolves -1 to the current OS
    //   default at display time.
    //
    // pcMicDeviceName: display name of the chosen capture device within
    //   the selected host API.  Empty = use the PA default device for
    //   that host API.
    //
    // pcMicBufferSamples: capture buffer size in samples per channel.
    //   Default 512 samples (~10.7 ms @ 48 kHz).  The UI exposes a
    //   power-of-2 list (64/128/256/512/1024/2048/4096/8192).

    /// PortAudio host API index for PC Mic capture.  -1 = OS default.
    /// Session-transient; AppSettings persistence deferred to Phase L.2.
    int pcMicHostApiIndex() const noexcept { return m_pcMicHostApiIndex; }

    /// Device name for PC Mic capture within the selected host API.
    /// Empty = use the PA default device for that host API.
    /// Session-transient; AppSettings persistence deferred to Phase L.2.
    QString pcMicDeviceName() const noexcept { return m_pcMicDeviceName; }

    /// Capture buffer size in samples per channel for PC Mic.
    /// Default 512 samples (~10.7 ms @ 48 kHz reference rate).
    /// Session-transient; AppSettings persistence deferred to Phase L.2.
    int pcMicBufferSamples() const noexcept { return m_pcMicBufferSamples; }

    // ── Two-tone test properties (3M-1c B.2) ─────────────────────────────────
    //
    // Per-MAC AppSettings persistence with Thetis column names per design
    // spec §4.4 / pre-code review §2.3.  Drives Setup → Test → Two-Tone page
    // (chunk 2 part — Phase H) and the TxApplet 2-TONE button (Phase J).
    //
    // Default values follow option C (JJ 2026-04-28):
    //   - Freq1 (700 Hz) / Freq2 (1900 Hz) — match Thetis Designer
    //     + btnTwoToneF_defaults preset (setup.cs:34226-34227 [v2.10.3.13]).
    //   - Level (-6 dB) / Power (50 %) — NereusSDR-original safer defaults
    //     (Thetis Designer ships 0 dB / 10 %).
    //   - Freq2Delay (0 ms) — matches Thetis Designer
    //     (setup.Designer.cs:61943-61947 [v2.10.3.13]).
    //   - Invert (true) — matches Thetis Designer chkInvertTones.Checked = true
    //     (setup.Designer.cs:61963 [v2.10.3.13]); functionally correct on
    //     LSB/CWL/DIGL per setup.cs:11058 [v2.10.3.13] conditional sign-flip.
    //   - Pulsed (false) — matches Thetis Designer (no Checked= line at
    //     setup.Designer.cs:61643-61653 [v2.10.3.13]).
    //
    // All ranges match Thetis Designer.  WDSP setters
    // (TXPostGenMode / TXPostGenTTFreq1/2 / TXPostGenTTMag1/2 + pulse-profile)
    // arrive in Phase E.  The two-tone activation handler (mode-aware invert,
    // power-source enum, MOX engage) arrives in Phase I.

    /// First test tone frequency (Hz).  Negative values valid; range matches
    /// Thetis udTestIMDFreq1.Min/Max.
    int twoToneFreq1() const noexcept { return m_twoToneFreq1; }

    /// Second test tone frequency (Hz).
    int twoToneFreq2() const noexcept { return m_twoToneFreq2; }

    /// Two-tone test magnitude (dB).  Used in setup.cs:11056 [v2.10.3.13]:
    ///   ttmag1 = ttmag2 = 0.49999 * pow(10, level / 20)
    /// NereusSDR default -6 dB (Thetis Designer udTwoToneLevel.Value = 0 dB).
    double twoToneLevel() const noexcept { return m_twoToneLevel; }

    /// TX power (%) used during the two-tone test when
    /// DrivePowerSource::Fixed (Phase B.3 enum) is active.
    /// NereusSDR default 50 % (Thetis Designer udTestIMDPower.Value = 10 %).
    int twoTonePower() const noexcept { return m_twoTonePower; }

    /// Delay (ms) before Freq2 magnitude is applied during a two-tone run.
    /// Defeats amplifier frequency-counters that latch on the first tone.
    /// 0 = both tones applied simultaneously.
    int twoToneFreq2Delay() const noexcept { return m_twoToneFreq2Delay; }

    /// Whether to negate Freq1/Freq2 in LS modes (LSB/CWL/DIGL).  When true,
    /// tones land at +Freq1/+Freq2 in the audio band on LSB; when false,
    /// they appear mirrored at -Freq1/-Freq2.  Default true per
    /// setup.Designer.cs:61963 [v2.10.3.13].
    bool twoToneInvert() const noexcept { return m_twoToneInvert; }

    /// Pulsed two-tone mode toggle.  When true, Phase I selects
    /// TXPostGenMode=7 (pulsed) + setupTwoTonePulse() profile setters;
    /// when false, TXPostGenMode=1 (continuous).
    bool twoTonePulsed() const noexcept { return m_twoTonePulsed; }

    /// Power-source selection for the two-tone test (Phase 3M-1c B.3).
    /// Default DriveSlider matches Thetis console.cs:46553 [v2.10.3.13]:
    ///   private DrivePowerSource _2ToneDrivePowerSource = DRIVE_SLIDER;
    /// Phase I (two-tone activation handler) consumes this to decide
    /// whether to save+override PWR (Fixed) or honor the user slider
    /// (DriveSlider / TuneSlider) per setup.cs:11111-11119 [v2.10.3.13].
    DrivePowerSource twoToneDrivePowerSource() const noexcept {
        return m_twoToneDrivePowerSource;
    }

    // Two-tone range constants — all match Thetis Designer.
    static constexpr int    kTwoToneFreq1HzMin      = -20000;  // setup.Designer.cs:62122-62126
    static constexpr int    kTwoToneFreq1HzMax      =  20000;  // setup.Designer.cs:62117-62121
    static constexpr int    kTwoToneFreq2HzMin      = -20000;  // setup.Designer.cs:62040-62044
    static constexpr int    kTwoToneFreq2HzMax      =  20000;  // setup.Designer.cs:62035-62039
    static constexpr double kTwoToneLevelDbMin      =  -96.0;  // setup.Designer.cs:61999-62003
    static constexpr double kTwoToneLevelDbMax      =    0.0;  // setup.Designer.cs:61994-61998
    static constexpr int    kTwoTonePowerMin        =      0;  // setup.Designer.cs:62069-62073
    static constexpr int    kTwoTonePowerMax        =    100;  // setup.Designer.cs:62064-62068
    static constexpr int    kTwoToneFreq2DelayMsMin =      0;  // setup.Designer.cs:61933-61937
    static constexpr int    kTwoToneFreq2DelayMsMax =   1000;  // setup.Designer.cs:61928-61932

    // ── CFC / CPDR / CESSB / Phase Rotator properties (3M-3a-ii Batch 2) ─
    //
    // 15 new properties for the TXA dynamics section:
    //
    //   Phase Rotator (4 — deferred from 3M-3a-i §7.1):
    //     phaseRotatorEnabled (bool)  — CFCPhaseRotatorEnabled
    //     phaseReverseEnabled (bool)  — CFCPhaseReverseEnabled
    //     phaseRotatorFreqHz  (int)   — CFCPhaseRotatorFreq, default 338
    //     phaseRotatorStages  (int)   — CFCPhaseRotatorStages, default 8
    //
    //   CFC (8):
    //     cfcEnabled              (bool)             — CFCEnabled
    //     cfcPostEqEnabled        (bool)             — CFCPostEqEnabled
    //     cfcPrecompDb            (int)              — CFCPreComp scalar
    //     cfcPostEqGainDb         (int)              — CFCPostEqGain scalar
    //     cfcEqFreqHz[10]         (array<int,10>)    — CFCEqFreq0..9
    //     cfcCompressionDb[10]    (array<int,10>)    — CFCPreComp0..9 (per-band G[])
    //     cfcPostEqBandGainDb[10] (array<int,10>)    — CFCPostEqGain0..9 (per-band E[])
    //     cfcParaEqData           (QString)          — CFCParaEQData (opaque blob)
    //
    //   CPDR (2):
    //     cpdrOn      (bool)  — global console state, NOT in TXProfile
    //     cpdrLevelDb (int)   — CompanderLevel
    //
    //   CESSB (1):
    //     cessbOn (bool)      — CESSB_On
    //
    // Defaults sourced from Thetis database.cs:4724-4768 [v2.10.3.13]
    // (TXProfile factory) except cpdrOn which comes from console.cs:36430
    // [v2.10.3.13] (SetGeneralSetting → OtherButtonId.COMP — global console
    // state).  WDSP boot default for compressor.run is 0 (TXA.c:253).
    //
    // Range clamps from Thetis Designer (setup.Designer.cs and
    // frmCFCConfig.Designer.cs [v2.10.3.13]).
    //
    // 14 of these 15 properties round-trip via TXProfile bundle (Batch 4).
    // cpdrOn lives outside the profile (global console state) — persisted at
    // hardware/<mac>/tx/cpdr/on, not in the per-profile namespace.

    // ── Phase Rotator getters ─────────────────────────────────────────────
    bool phaseRotatorEnabled() const noexcept { return m_phaseRotatorEnabled; }
    bool phaseReverseEnabled() const noexcept { return m_phaseReverseEnabled; }
    int  phaseRotatorFreqHz() const noexcept  { return m_phaseRotatorFreqHz; }
    int  phaseRotatorStages() const noexcept  { return m_phaseRotatorStages; }

    // ── CFC scalar getters ────────────────────────────────────────────────
    bool cfcEnabled() const noexcept       { return m_cfcEnabled; }
    bool cfcPostEqEnabled() const noexcept { return m_cfcPostEqEnabled; }
    int  cfcPrecompDb() const noexcept     { return m_cfcPrecompDb; }
    int  cfcPostEqGainDb() const noexcept  { return m_cfcPostEqGainDb; }

    /// Per-band CFC EQ frequency (Hz) at index 0..9.  Returns 0 for out-of-range.
    int cfcEqFreq(int index) const noexcept;
    /// Per-band CFC compression amount (dB) at index 0..9.  Returns 0 for out-of-range.
    int cfcCompression(int index) const noexcept;
    /// Per-band CFC post-EQ gain (dB) at index 0..9.  Returns 0 for out-of-range.
    int cfcPostEqBandGain(int index) const noexcept;

    /// Opaque parametric-EQ blob.  No setter validation — pass-through for
    /// forward-compat round-trip with imported Thetis profiles (Batch 4).
    const QString& cfcParaEqData() const noexcept { return m_cfcParaEqData; }

    // ── CPDR getters ──────────────────────────────────────────────────────
    /// CPDR global on/off (NOT in TXProfile).  See header comment.
    bool cpdrOn() const noexcept       { return m_cpdrOn; }
    int  cpdrLevelDb() const noexcept  { return m_cpdrLevelDb; }

    // ── CESSB getters ─────────────────────────────────────────────────────
    bool cessbOn() const noexcept      { return m_cessbOn; }

    // ── Range constants (Thetis Designer [v2.10.3.13]) ────────────────────
    //
    // Phase Rotator FREQ: 10..2000 Hz
    // From Thetis setup.Designer.cs:46250-46259 [v2.10.3.13] — udPhRotFreq.
    static constexpr int kPhaseRotatorFreqHzMin = 10;
    static constexpr int kPhaseRotatorFreqHzMax = 2000;
    // Phase Rotator STAGES: 2..16
    // From Thetis setup.Designer.cs:46209-46218 [v2.10.3.13] — udPHROTStages.
    static constexpr int kPhaseRotatorStagesMin = 2;
    static constexpr int kPhaseRotatorStagesMax = 16;

    // CFC pre-comp scalar: 0..16 dB
    // From Thetis frmCFCConfig.Designer.cs:408-422 [v2.10.3.13] — nudCFC_precomp.
    static constexpr int kCfcPrecompDbMin = 0;
    static constexpr int kCfcPrecompDbMax = 16;
    // CFC post-EQ gain scalar: -24..+24 dB
    // From Thetis frmCFCConfig.Designer.cs:337-351 [v2.10.3.13] — nudCFC_posteqgain.
    // The Designer encodes -24 via the C# decimal sign-bit (4th int = -2147483648),
    // which means Maximum=24, Minimum=-24.
    static constexpr int kCfcPostEqGainDbMin = -24;
    static constexpr int kCfcPostEqGainDbMax =  24;
    // CFC EQ frequency (per-band): 0..20000 Hz
    // From Thetis frmCFCConfig.Designer.cs:267-286 [v2.10.3.13] — nudCFC_f.
    static constexpr int kCfcEqFreqHzMin = 0;
    static constexpr int kCfcEqFreqHzMax = 20000;
    // CFC compression (per-band): 0..16 dB
    // From Thetis frmCFCConfig.Designer.cs:217-236 [v2.10.3.13] — nudCFC_c.
    // Naming note: Thetis stores these in CFCPreComp0..9 columns but the
    // values are the per-band compression amounts (WDSP G[] vector).
    static constexpr int kCfcCompressionDbMin = 0;
    static constexpr int kCfcCompressionDbMax = 16;
    // CFC post-EQ band gain (per-band): -24..+24 dB
    // From Thetis frmCFCConfig.Designer.cs:564-583 [v2.10.3.13] — nudCFC_gain.
    // Same C# decimal sign-bit encoding as nudCFC_posteqgain above.
    // These are the per-band post-EQ gains (WDSP E[] vector).
    static constexpr int kCfcPostEqBandGainDbMin = -24;
    static constexpr int kCfcPostEqBandGainDbMax =  24;

    // CPDR level: 0..20 dB
    // From Thetis console.Designer.cs:6042-6043 [v2.10.3.13] — ptbCPDR
    // (Maximum=20, Minimum=0).  setup.cs:9307 reads CompanderLevel into
    // console.CPDRLevel which is the ptbCPDR value (console.cs:15683-15695).
    static constexpr int kCpdrLevelDbMin = 0;
    static constexpr int kCpdrLevelDbMax = 20;

    // ── TX EQ + Leveler + ALC properties (3M-3a-i Task C) ───────────────
    //
    // 28 new properties (23 TXProfile + 5 stand-alone + 4 globals):
    //   TX EQ (TXProfile, 23 keys):
    //     txEqEnabled (bool)         — TXEQEnabled
    //     txEqNumBands (int RO=10)   — TXEQNumBands
    //     txEqPreamp (int dB)        — TXEQPreamp
    //     txEqBand[i] (10 int dB)    — TXEQ1..TXEQ10
    //     txEqFreq[i] (10 int Hz)    — TxEqFreq1..TxEqFreq10
    //
    //   TX Leveler (TXProfile, 3 keys):
    //     txLevelerOn (bool)        — Lev_On
    //     txLevelerMaxGain (int dB) — Lev_MaxGain
    //     txLevelerDecay (int ms)   — Lev_Decay
    //
    //   TX ALC (TXProfile, 2 keys):
    //     txAlcMaxGain (int dB)     — ALC_MaximumGain
    //     txAlcDecay (int ms)       — ALC_Decay
    //
    //   TX EQ globals (NOT in TXProfile, 4 keys under hardware/<mac>/tx/):
    //     txEqNc (int)              — eq/nc       default 2048
    //     txEqMp (bool)             — eq/mp       default false
    //     txEqCtfmode (int)         — eq/ctfmode  default 0
    //     txEqWintype (int)         — eq/wintype  default 0
    //
    // ALC Run is locked-on per Thetis schema — no txAlcOn property is exposed.
    // Phase Rotator is intentionally out of scope (deferred to 3M-3a-ii because
    // its CFC* persistence keys belong with the CFC tab).
    //
    // Defaults from Thetis database.cs:4552-4594 [v2.10.3.13] (TXProfile schema)
    // and WDSP TXA.c:111-128 [v2.10.3.13] (create_eqp G[]/F[] vectors).
    //
    // Range clamps from Thetis Designer (setup.Designer.cs:38710-38866 [v2.10.3.13]).
    // 3M-3a-i Batch 2+ wires TransmitModel signals into TxChannel via RadioModel.

    // ── Number of EQ bands.  Read-only constant per Thetis 10-band UI. ──
    int  txEqNumBands() const noexcept { return 10; }

    // ── TX EQ enable + preamp ──
    bool txEqEnabled() const noexcept { return m_txEqEnabled; }
    int  txEqPreamp() const noexcept  { return m_txEqPreamp; }

    /// Per-band gain (dB) at index 0..9.  Returns 0 for out-of-range index.
    int txEqBand(int index) const noexcept;
    /// Per-band frequency (Hz) at index 0..9.  Returns 0 for out-of-range index.
    int txEqFreq(int index) const noexcept;

    // ── TX Leveler ──
    bool txLevelerOn() const noexcept       { return m_txLevelerOn; }
    int  txLevelerMaxGain() const noexcept  { return m_txLevelerMaxGain; }
    int  txLevelerDecay() const noexcept    { return m_txLevelerDecay; }

    // ── TX ALC (Run is locked-on; no getter exposed) ──
    int  txAlcMaxGain() const noexcept      { return m_txAlcMaxGain; }
    int  txAlcDecay() const noexcept        { return m_txAlcDecay; }

    // ── TX EQ globals (radio-wide DSP settings) ──
    int  txEqNc() const noexcept            { return m_txEqNc; }
    bool txEqMp() const noexcept            { return m_txEqMp; }
    int  txEqCtfmode() const noexcept       { return m_txEqCtfmode; }
    int  txEqWintype() const noexcept       { return m_txEqWintype; }

    // ── Range constants (Thetis Designer setup.Designer.cs [v2.10.3.13]) ──
    //
    // Leveler MaxGain: 0..20 dB (udDSPLevelerThreshold:38718-38738).
    static constexpr int kTxLevelerMaxGainDbMin  =    0;
    static constexpr int kTxLevelerMaxGainDbMax  =   20;
    // Leveler Decay: 1..5000 ms (udDSPLevelerDecay:38744-38772).
    static constexpr int kTxLevelerDecayMsMin    =    1;
    static constexpr int kTxLevelerDecayMsMax    = 5000;
    // ALC MaxGain: 0..120 dB (udDSPALCMaximumGain:38814-38833).
    static constexpr int kTxAlcMaxGainDbMin      =    0;
    static constexpr int kTxAlcMaxGainDbMax      =  120;
    // ALC Decay: 1..50 ms (udDSPALCDecay:38845-38866).
    static constexpr int kTxAlcDecayMsMin        =    1;
    static constexpr int kTxAlcDecayMsMax        =   50;
    // EQ preamp: NereusSDR clamp [-12, 15] dB (matches Thetis EQ preamp slider
    // precedent — eqx.cs:btnReset preset clears preamp to 0; spinbox accepts
    // ±dB but no formal Designer Min/Max is exposed for the integer column).
    static constexpr int kTxEqPreampDbMin        =  -12;
    static constexpr int kTxEqPreampDbMax        =   15;
    // EQ band gain: same clamp as preamp (the per-band slider uses the same
    // ±dB range as the preamp slider).
    static constexpr int kTxEqBandDbMin          =  -12;
    static constexpr int kTxEqBandDbMax          =   15;
    // EQ band frequency: WDSP eq_impulse accepts up to Nyquist (24 kHz at
    // 48 kHz dsp_rate); 22 kHz UI cap is conservative.  Min 10 Hz protects
    // FFT-bin boundary math; Thetis itself sets defaults from 32 Hz.
    static constexpr int kTxEqFreqHzMin          =   10;
    static constexpr int kTxEqFreqHzMax          = 22000;

public slots:
    void setTxEqEnabled(bool on);
    void setTxEqPreamp(int dB);
    /// Set per-band gain (dB) at index 0..9.  No-op if index out of range.
    void setTxEqBand(int index, int dB);
    /// Set per-band frequency (Hz) at index 0..9.  No-op if index out of range.
    void setTxEqFreq(int index, int hz);
    void setTxLevelerOn(bool on);
    void setTxLevelerMaxGain(int dB);
    void setTxLevelerDecay(int ms);
    void setTxAlcMaxGain(int dB);
    void setTxAlcDecay(int ms);
    void setTxEqNc(int nc);
    void setTxEqMp(bool mp);
    void setTxEqCtfmode(int mode);
    void setTxEqWintype(int wintype);

    // ── Phase Rotator setters (3M-3a-ii Batch 2) ─────────────────────────
    void setPhaseRotatorEnabled(bool on);
    void setPhaseReverseEnabled(bool on);
    void setPhaseRotatorFreqHz(int hz);
    void setPhaseRotatorStages(int stages);

    // ── CFC setters (3M-3a-ii Batch 2) ────────────────────────────────────
    void setCfcEnabled(bool on);
    void setCfcPostEqEnabled(bool on);
    void setCfcPrecompDb(int dB);
    void setCfcPostEqGainDb(int dB);
    /// Per-band CFC EQ frequency (Hz) at index 0..9.  No-op if index out of range.
    void setCfcEqFreq(int index, int hz);
    /// Per-band CFC compression amount (dB) at index 0..9.  No-op if index out of range.
    void setCfcCompression(int index, int dB);
    /// Per-band CFC post-EQ gain (dB) at index 0..9.  No-op if index out of range.
    void setCfcPostEqBandGain(int index, int dB);
    /// Opaque parametric-EQ blob.  No validation — pass-through for round-trip.
    void setCfcParaEqData(const QString& data);

    // ── CPDR setters (3M-3a-ii Batch 2) ───────────────────────────────────
    void setCpdrOn(bool on);
    void setCpdrLevelDb(int dB);

    // ── CESSB setters (3M-3a-ii Batch 2) ──────────────────────────────────
    void setCessbOn(bool on);

signals:
    void txEqEnabledChanged(bool on);
    void txEqPreampChanged(int dB);
    /// Emitted when any individual band gain changes; carries index + new value.
    void txEqBandChanged(int index, int dB);
    /// Emitted when any individual band frequency changes; carries index + new value.
    void txEqFreqChanged(int index, int hz);
    void txLevelerOnChanged(bool on);
    void txLevelerMaxGainChanged(int dB);
    void txLevelerDecayChanged(int ms);
    void txAlcMaxGainChanged(int dB);
    void txAlcDecayChanged(int ms);
    void txEqNcChanged(int nc);
    void txEqMpChanged(bool mp);
    void txEqCtfmodeChanged(int mode);
    void txEqWintypeChanged(int wintype);

    // ── Phase Rotator signals (3M-3a-ii Batch 2) ─────────────────────────
    void phaseRotatorEnabledChanged(bool on);
    void phaseReverseEnabledChanged(bool on);
    void phaseRotatorFreqHzChanged(int hz);
    void phaseRotatorStagesChanged(int stages);

    // ── CFC signals (3M-3a-ii Batch 2) ────────────────────────────────────
    void cfcEnabledChanged(bool on);
    void cfcPostEqEnabledChanged(bool on);
    void cfcPrecompDbChanged(int dB);
    void cfcPostEqGainDbChanged(int dB);
    /// Emitted when a per-band CFC EQ freq changes; carries index + new value.
    void cfcEqFreqChanged(int index, int hz);
    /// Emitted when a per-band CFC compression changes; carries index + new value.
    void cfcCompressionChanged(int index, int dB);
    /// Emitted when a per-band CFC post-EQ gain changes; carries index + new value.
    void cfcPostEqBandGainChanged(int index, int dB);
    void cfcParaEqDataChanged(const QString& data);

    // ── CPDR signals (3M-3a-ii Batch 2) ───────────────────────────────────
    void cpdrOnChanged(bool on);
    void cpdrLevelDbChanged(int dB);

    // ── CESSB signals (3M-3a-ii Batch 2) ──────────────────────────────────
    void cessbOnChanged(bool on);

public:

public slots:
    void setMicGainDb(int dB);

    // ── Mic source setter (3M-1b I.1) ─────────────────────────────────────────
    /// Select the active mic source (Pc or Radio).  Idempotent: no signal
    /// when the value is unchanged.
    ///
    /// When locked via setMicSourceLocked(true), calling setMicSource(Radio)
    /// silently coerces the value to Pc.  This is the HL2 force-Pc-on-connect
    /// model-side lock (L.3): HL2 has no radio-side mic jack so MicSource::Radio
    /// must never be active on that board.
    void setMicSource(MicSource source);

    // ── Mic source lock guard (3M-1b L.3) ────────────────────────────────────
    //
    // NereusSDR-native. When lock is true, setMicSource(MicSource::Radio)
    // silently coerces to MicSource::Pc.  This is the model-side complement
    // of the UI-side lock (AudioTxInputPage disables the Radio Mic radio button
    // when BoardCapabilities::hasMicJack == false).
    //
    // RadioModel::connectToRadio() calls setMicSourceLocked(!caps.hasMicJack)
    // after loadFromSettings() so the lock is active for the lifetime of the
    // connection.  On disconnect, RadioModel calls setMicSourceLocked(false)
    // (teardownConnection) so a subsequent reconnect to a different radio with
    // hasMicJack=true can use Radio again.
    //
    // The lock is NOT persisted — it is a runtime capability constraint, not
    // a user preference.  It is never stored in AppSettings.

    /// Install or release the HL2 mic-source lock.
    /// When lock is true, setMicSource(MicSource::Radio) is silently coerced
    /// to MicSource::Pc.  Call with false to release the lock (e.g. after
    /// teardown, before a non-HL2 reconnect).
    void setMicSourceLocked(bool lock);

    /// Return true when the mic-source lock is active (hasMicJack == false).
    bool isMicSourceLocked() const noexcept { return m_micSourceLocked; }

    // ── PC Mic session-state setters (3M-1b I.2) ─────────────────────────────
    /// Set the PortAudio host API index for PC Mic capture.  Idempotent.
    /// -1 = let AudioEngine resolve the OS default.
    /// AppSettings persistence deferred to Phase L.2.
    void setPcMicHostApiIndex(int index);

    /// Set the device name for PC Mic capture within the selected host API.
    /// Empty string = use the PA default device for that host API.
    /// Idempotent; AppSettings persistence deferred to Phase L.2.
    void setPcMicDeviceName(const QString& name);

    /// Set the capture buffer size in samples per channel for PC Mic.
    /// No clamping — caller is responsible for valid power-of-2 values.
    /// Idempotent; AppSettings persistence deferred to Phase L.2.
    void setPcMicBufferSamples(int samples);

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

    // ── Two-tone setters (3M-1c B.2) ───────────────────────────────────────
    void setTwoToneFreq1(int hz);
    void setTwoToneFreq2(int hz);
    void setTwoToneLevel(double db);
    void setTwoTonePower(int pct);
    void setTwoToneFreq2Delay(int ms);
    void setTwoToneInvert(bool on);
    void setTwoTonePulsed(bool on);

    // ── Two-tone drive-power source (3M-1c B.3) ───────────────────────────
    void setTwoToneDrivePowerSource(DrivePowerSource source);

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

    // ── PC Mic session-state signals (3M-1b I.2) ─────────────────────────────
    /// Emitted when pcMicHostApiIndex changes. Not emitted on idempotent calls.
    void pcMicHostApiIndexChanged(int index);
    /// Emitted when pcMicDeviceName changes. Not emitted on idempotent calls.
    void pcMicDeviceNameChanged(const QString& name);
    /// Emitted when pcMicBufferSamples changes. Not emitted on idempotent calls.
    void pcMicBufferSamplesChanged(int samples);

    // ── Two-tone signals (3M-1c B.2) ───────────────────────────────────────
    void twoToneFreq1Changed(int hz);
    void twoToneFreq2Changed(int hz);
    void twoToneLevelChanged(double db);
    void twoTonePowerChanged(int pct);
    void twoToneFreq2DelayChanged(int ms);
    void twoToneInvertChanged(bool on);
    void twoTonePulsedChanged(bool on);

    // ── Two-tone drive-power source signal (3M-1c B.3) ─────────────────────
    void twoToneDrivePowerSourceChanged(DrivePowerSource source);

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
    // HF amateur + GEN/WWV/XVTR only (Band::SwlFirst == 14).  Phase 3L
    // SWL bands inherit ham-band values — no separate per-SWL TX power.
    std::array<int, static_cast<std::size_t>(Band::SwlFirst)> m_tunePowerByBand{};

    // Per-MAC AppSettings scope (mirrors AlexController pattern).
    QString m_mac;

    // Per-MAC scope for mic/VOX/MON auto-persist (L.2).
    // Empty until loadFromSettings(mac) is called; setters no-op their
    // persistOne() call when this is empty.
    QString m_persistMac;

    /// Write a single key under hardware/<m_persistMac>/tx/<key> to AppSettings.
    /// No-op when m_persistMac is empty (before loadFromSettings is called).
    void persistOne(const QString& key, const QVariant& value) const;

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

    // ── Mic source (3M-1b I.1 + L.3) ───────────────────────────────────
    // NereusSDR-native. Default Pc (always available; Radio is opt-in).
    // m_micSourceLocked: L.3 HL2 force. When true, setMicSource(Radio)
    // silently coerces to Pc.  Runtime capability constraint; not persisted.
    MicSource m_micSource{MicSource::Pc};
    bool      m_micSourceLocked{false};  // L.3: set by RadioModel per hasMicJack

    // ── PC Mic session state (3M-1b I.2) ─────────────────────────────────
    // NereusSDR-native transient session state. AppSettings persistence
    // deferred to Phase L.2. Survives Setup dialog close/reopen in session.
    int     m_pcMicHostApiIndex  = -1;      // -1 = OS default (PA resolves)
    QString m_pcMicDeviceName;              // empty = default device for host API
    int     m_pcMicBufferSamples = 512;     // ~10.7 ms @ 48 kHz reference rate

    // ── Two-tone test properties (3M-1c B.2) ─────────────────────────────
    // Defaults per design spec §4.4 / pre-code review §2.3 (option C).
    int    m_twoToneFreq1      =   700;   // Designer + Defaults preset
    int    m_twoToneFreq2      =  1900;   // Designer + Defaults preset
    double m_twoToneLevel      =  -6.0;   // NereusSDR-original; Designer = 0 dB
    int    m_twoTonePower      =    50;   // NereusSDR-original; Designer = 10 %
    int    m_twoToneFreq2Delay =     0;   // matches Thetis Designer
    bool   m_twoToneInvert     =  true;   // setup.Designer.cs:61963 [v2.10.3.13]
    bool   m_twoTonePulsed     = false;   // setup.Designer.cs:61643-61653 (default)

    // ── Two-tone drive-power source (3M-1c B.3) ──────────────────────────
    // Default DriveSlider per Thetis console.cs:46553 [v2.10.3.13]:
    //   private DrivePowerSource _2ToneDrivePowerSource = DRIVE_SLIDER;
    DrivePowerSource m_twoToneDrivePowerSource{DrivePowerSource::DriveSlider};

    // ── TX EQ + Leveler + ALC properties (3M-3a-i Task C) ────────────────
    //
    // Defaults sourced from Thetis database.cs:4552-4594 [v2.10.3.13]
    // (TXProfile schema) and WDSP TXA.c:111-128 [v2.10.3.13] (create_eqp).

    // EQ enable + preamp.  database.cs:4553-4554 [v2.10.3.13].
    bool m_txEqEnabled  = false;   // dr["TXEQEnabled"] = false;
    int  m_txEqPreamp   = 0;       // dr["TXEQPreamp"]  = 0;

    // Per-band gains and frequencies.  Defaults from WDSP TXA.c:112-113
    // [v2.10.3.13] — default_F[1..10] and default_G[1..10].
    //   default_F[11] = {0.0, 32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    //   default_G[11] = {0.0, -12, -12, -12, -1, +1, +4, +9, +12, -10, -10};
    //   //double default_G[11] =   {0.0,   0.0,   0.0,   0.0,   0.0,   0.0,    0.0,    0.0,    0.0,    0.0,     0.0};
    std::array<int, 10> m_txEqBand = {-12, -12, -12, -1, 1, 4, 9, 12, -10, -10};
    std::array<int, 10> m_txEqFreq = {32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

    // Leveler.  database.cs:4584-4588 [v2.10.3.13].
    bool m_txLevelerOn      = true;  // dr["Lev_On"]      = true;
    int  m_txLevelerMaxGain = 15;    // dr["Lev_MaxGain"] = 15;
    int  m_txLevelerDecay   = 100;   // dr["Lev_Decay"]   = 100;

    // ALC.  database.cs:4592-4594 [v2.10.3.13].
    int  m_txAlcMaxGain = 3;     // dr["ALC_MaximumGain"] = 3;
    int  m_txAlcDecay   = 10;    // dr["ALC_Decay"]       = 10;

    // EQ globals (radio-wide DSP).  Defaults from WDSP TXA.c:118-127 [v2.10.3.13].
    int  m_txEqNc      = 2048;  // max(2048, ch[].dsp_size)
    bool m_txEqMp      = false; // minimum-phase flag = 0
    int  m_txEqCtfmode = 0;     // cutoff mode = 0
    int  m_txEqWintype = 0;     // window type = 0

    // ── CFC / CPDR / CESSB / Phase Rotator (3M-3a-ii Batch 2) ────────────
    //
    // Defaults sourced from Thetis database.cs:4724-4768 [v2.10.3.13]
    // (TXProfile factory) except cpdrOn (console.cs:36430 — global).

    // Phase Rotator.  database.cs:4726-4730 [v2.10.3.13].
    bool m_phaseRotatorEnabled = false;  // dr["CFCPhaseRotatorEnabled"] = false;
    bool m_phaseReverseEnabled = false;  // dr["CFCPhaseReverseEnabled"] = false;
    int  m_phaseRotatorFreqHz  = 338;    // dr["CFCPhaseRotatorFreq"]    = 338;
    int  m_phaseRotatorStages  = 8;      // dr["CFCPhaseRotatorStages"]  = 8;

    // CFC scalars.  database.cs:4724-4733 [v2.10.3.13].
    bool m_cfcEnabled       = false;  // dr["CFCEnabled"]       = false;
    bool m_cfcPostEqEnabled = false;  // dr["CFCPostEqEnabled"] = false;
    int  m_cfcPrecompDb     = 0;      // dr["CFCPreComp"]       = 0;
    int  m_cfcPostEqGainDb  = 0;      // dr["CFCPostEqGain"]    = 0;

    // CFC per-band frequencies.  database.cs:4757-4766 [v2.10.3.13]:
    //   CFCEqFreq0..9 = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000}.
    std::array<int, 10> m_cfcEqFreqHz = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000};

    // CFC per-band compression amounts (WDSP G[] vector, stored under
    // CFCPreComp0..9 columns).  database.cs:4735-4744 [v2.10.3.13]: all 5.
    std::array<int, 10> m_cfcCompressionDb = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

    // CFC per-band post-EQ gains (WDSP E[] vector).  database.cs:4746-4755
    // [v2.10.3.13]: all 0.
    std::array<int, 10> m_cfcPostEqBandGainDb = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Opaque parametric-EQ blob.  database.cs:4768 [v2.10.3.13]:
    //   dr["CFCParaEQData"] = "";
    QString m_cfcParaEqData;

    // CPDR.  cpdrOn is global console state (NOT in TXProfile) — Thetis
    // wires it via SetGeneralSetting(0, OtherButtonId.COMP, ...) at
    // console.cs:36430 [v2.10.3.13].  WDSP boot default
    // txa[ch].compressor.run = 0 (TXA.c:253 [v2.10.3.13]).
    bool m_cpdrOn      = false;
    // CPDR level: database.cs:4339 + 4580 [v2.10.3.13]:
    //   dr["CompanderLevel"] = 2;
    int  m_cpdrLevelDb = 2;

    // CESSB.  database.cs:4689 [v2.10.3.13]:
    //   dr["CESSB_On"] = false;
    bool m_cessbOn = false;
};

} // namespace NereusSDR
