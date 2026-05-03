// =================================================================
// src/core/MicProfileManager.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. The mic-profile-bank API is a port of the
// Thetis comboTXProfile / Setup → TX Profile editor flow:
//   setup.cs:9505-9543 [v2.10.3.13] — comboTXProfileName_SelectedIndexChanged
//   setup.cs:9545-9612 [v2.10.3.13] — btnTXProfileSave_Click
//   setup.cs:9615-9656 [v2.10.3.13] — btnTXProfileDelete_Click
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-28 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//                 Phase 3M-1c chunk F (F.1-F.6) — see header comment.
//   2026-04-29 — Phase 3M-3a-i Batch 4 (Task A.2): factory-profile
//                 seed extension by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude
//                 Code.  load() now seeds Default + 21 Thetis
//                 factory profiles on first launch (port of
//                 database.cs AddTXProfileTable bIndcludeExtraProfiles
//                 block, lines 4545-9354 [v2.10.3.13]).
// =================================================================

// no-port-check: NereusSDR-original file; Thetis-derived handler logic
// is cited inline below.

#include "MicProfileManager.h"

#include "AppSettings.h"
#include "LogCategories.h"
#include "models/TransmitModel.h"

#include <QLoggingCategory>

#include <algorithm>

namespace NereusSDR {

namespace {

QString profilePathPrefix(const QString& mac)
{
    return QStringLiteral("hardware/%1/tx/profile/").arg(mac);
}

QString profileFieldKey(const QString& mac, const QString& name, const QString& field)
{
    return profilePathPrefix(mac) + name + QLatin1Char('/') + field;
}

QString manifestKey(const QString& mac)
{
    return profilePathPrefix(mac) + QStringLiteral("_names");
}

QString activeKey(const QString& mac)
{
    return profilePathPrefix(mac) + QStringLiteral("active");
}

// ---------------------------------------------------------------------------
// 50 live-field keys captured per profile.  Order matches the table in the
// chunk-F + 3M-3a-i G task specs (tools/script-friendly: sorted by group).
// 23 mic/VOX/MON/two-tone (3M-1c F) + 27 EQ/Lev/ALC (3M-3a-i G).
// ---------------------------------------------------------------------------
const QStringList& liveKeyList()
{
    static const QStringList kKeys = {
        // Mic / VOX / MON (15 of which 14 are persisted live by TransmitModel
        // L.2 — micMute is excluded from per-MAC TX persistence by the safety
        // rule, but IS captured here because a profile carries a snapshot
        // including the mic in/out state at save-time).  Plus Mic_Source.
        QStringLiteral("MicGain"),
        QStringLiteral("Mic_Input_Boost"),
        QStringLiteral("Mic_XLR"),
        QStringLiteral("Line_Input_On"),
        QStringLiteral("Line_Input_Level"),
        QStringLiteral("Mic_TipRing"),
        QStringLiteral("Mic_Bias"),
        QStringLiteral("Mic_PTT_Disabled"),
        // line_in_gain + user_dig_out (Task 2.4 of P1 full-parity epic):
        // bundled in profiles so a saved profile carries the wire-bit state.
        QStringLiteral("Mic_LineInGain"),
        QStringLiteral("Mic_UserDigOut"),
        QStringLiteral("Dexp_Threshold"),
        QStringLiteral("VOX_GainScalar"),
        QStringLiteral("VOX_HangTime"),
        QStringLiteral("AntiVox_Gain"),
        QStringLiteral("AntiVox_Source_VAX"),
        QStringLiteral("MonitorVolume"),
        QStringLiteral("Mic_Source"),
        // Two-tone (7 properties + 1 enum)
        QStringLiteral("TwoToneFreq1"),
        QStringLiteral("TwoToneFreq2"),
        QStringLiteral("TwoToneLevel"),
        QStringLiteral("TwoTonePower"),
        QStringLiteral("TwoToneFreq2Delay"),
        QStringLiteral("TwoToneInvert"),
        QStringLiteral("TwoTonePulsed"),
        QStringLiteral("TwoToneDrivePowerOrigin"),
        // ── 3M-3a-i G — TX EQ + Leveler + ALC (27 keys) ───────────────────
        // EQ enable + preamp + 10 band gains + 10 band frequencies (22 keys).
        // Defaults from Thetis database.cs:4552-4594 [v2.10.3.13] (TXProfile
        // schema) + WDSP TXA.c:111-128 (create_eqp G[]/F[] vectors).
        // EQ globals (eq/nc, eq/mp, eq/ctfmode, eq/wintype) are intentionally
        // NOT bundled — they are radio-wide DSP settings, not per-profile.
        QStringLiteral("TXEQEnabled"),
        QStringLiteral("TXEQPreamp"),
        QStringLiteral("TXEQ1"),
        QStringLiteral("TXEQ2"),
        QStringLiteral("TXEQ3"),
        QStringLiteral("TXEQ4"),
        QStringLiteral("TXEQ5"),
        QStringLiteral("TXEQ6"),
        QStringLiteral("TXEQ7"),
        QStringLiteral("TXEQ8"),
        QStringLiteral("TXEQ9"),
        QStringLiteral("TXEQ10"),
        QStringLiteral("TxEqFreq1"),
        QStringLiteral("TxEqFreq2"),
        QStringLiteral("TxEqFreq3"),
        QStringLiteral("TxEqFreq4"),
        QStringLiteral("TxEqFreq5"),
        QStringLiteral("TxEqFreq6"),
        QStringLiteral("TxEqFreq7"),
        QStringLiteral("TxEqFreq8"),
        QStringLiteral("TxEqFreq9"),
        QStringLiteral("TxEqFreq10"),
        // TX EQ parametric blob (1) — Phase 3M-3a-ii follow-up Batch 6.
        // Opaque gzip+base64url envelope produced by ParaEqEnvelope wrapping
        // the ParametricEqWidget JSON; mirror of CFCParaEQData (Batch 4) for
        // the TX EQ slot.  No Thetis database.cs row — TXProfile column ships
        // empty until ucParametricEq populates it.
        QStringLiteral("TXParaEQData"),
        // Leveler (3 keys) — database.cs:4584-4588 [v2.10.3.13].
        QStringLiteral("Lev_On"),
        QStringLiteral("Lev_MaxGain"),
        QStringLiteral("Lev_Decay"),
        // ALC (2 keys) — database.cs:4592-4594 [v2.10.3.13].
        QStringLiteral("ALC_MaximumGain"),
        QStringLiteral("ALC_Decay"),
        // ── 3M-3a-ii G — CFC + CPDR + CESSB + PhRot (41 keys) ─────────────
        // Bundles the new TransmitModel TX-chain properties added in Batch 2
        // and ports the CFC / CPDR / CESSB / PhRot columns byte-for-byte from
        // the 21 Thetis factory TX profiles in database.cs:AddTXProfileTable
        // [v2.10.3.13].  cpdrOn is intentionally NOT bundled — it is global
        // console state, not per-profile (see brief).
        // Phase Rotator (4) — database.cs:4726-4730 [v2.10.3.13].
        QStringLiteral("CFCPhaseRotatorEnabled"),
        QStringLiteral("CFCPhaseReverseEnabled"),
        QStringLiteral("CFCPhaseRotatorFreq"),
        QStringLiteral("CFCPhaseRotatorStages"),
        // CFC scalars (4) — database.cs:4724-4733 [v2.10.3.13].
        QStringLiteral("CFCEnabled"),
        QStringLiteral("CFCPostEqEnabled"),
        QStringLiteral("CFCPreComp"),
        QStringLiteral("CFCPostEqGain"),
        // CFC profile arrays (30) — database.cs:4735-4766 [v2.10.3.13].
        QStringLiteral("CFCPreComp0"),
        QStringLiteral("CFCPreComp1"),
        QStringLiteral("CFCPreComp2"),
        QStringLiteral("CFCPreComp3"),
        QStringLiteral("CFCPreComp4"),
        QStringLiteral("CFCPreComp5"),
        QStringLiteral("CFCPreComp6"),
        QStringLiteral("CFCPreComp7"),
        QStringLiteral("CFCPreComp8"),
        QStringLiteral("CFCPreComp9"),
        QStringLiteral("CFCPostEqGain0"),
        QStringLiteral("CFCPostEqGain1"),
        QStringLiteral("CFCPostEqGain2"),
        QStringLiteral("CFCPostEqGain3"),
        QStringLiteral("CFCPostEqGain4"),
        QStringLiteral("CFCPostEqGain5"),
        QStringLiteral("CFCPostEqGain6"),
        QStringLiteral("CFCPostEqGain7"),
        QStringLiteral("CFCPostEqGain8"),
        QStringLiteral("CFCPostEqGain9"),
        QStringLiteral("CFCEqFreq0"),
        QStringLiteral("CFCEqFreq1"),
        QStringLiteral("CFCEqFreq2"),
        QStringLiteral("CFCEqFreq3"),
        QStringLiteral("CFCEqFreq4"),
        QStringLiteral("CFCEqFreq5"),
        QStringLiteral("CFCEqFreq6"),
        QStringLiteral("CFCEqFreq7"),
        QStringLiteral("CFCEqFreq8"),
        QStringLiteral("CFCEqFreq9"),
        // CFC blob (1) — database.cs:4768 [v2.10.3.13]; opaque, forward-compat.
        QStringLiteral("CFCParaEQData"),
        // CPDR (1) — database.cs:4580 [v2.10.3.13].  cpdrOn is global
        // console state, not bundled here.
        QStringLiteral("CompanderLevel"),
        // CESSB (1) — database.cs:4689 [v2.10.3.13].
        QStringLiteral("CESSB_On"),
        // ── Plan 4 D1 — TX filter bandwidth (2 keys) ───────────────────────
        // NereusSDR-original. Defaults 100/2900 (USB voice typical SSB).
        QStringLiteral("FilterLow"),
        QStringLiteral("FilterHigh"),
    };
    return kKeys;
}

// ---------------------------------------------------------------------------
// 3M-3a-i Batch 4 (Task A.2) — factory profile bank.
//
// Ports the 21 Thetis factory TX profiles VERBATIM from
// database.cs:AddTXProfileTable [v2.10.3.13]:
//   * Default          (database.cs:4545-4770)   — always seeded
//   * Default DX       (database.cs:4777-5000)   — always seeded
//   * Digi 1K@1500     (database.cs:5009-5231)   — bIndcludeExtraProfiles
//   * Digi 1K@2210     (database.cs:5239-5461)
//   * AM               (database.cs:5468-5690)
//   * Conventional     (database.cs:5697-5919)
//   * D-104            (database.cs:5926-6148)
//   * D-104+CPDR       (database.cs:6155-6377)
//   * D-104+EQ         (database.cs:6384-6606)
//   * DX / Contest     (database.cs:6613-6835)
//   * ESSB             (database.cs:6842-7064)
//   * HC4-5            (database.cs:7071-7293)
//   * HC4-5+CPDR       (database.cs:7300-7522)
//   * PR40+W2IHY       (database.cs:7529-7751)
//   * PR40+W2IHY+CPDR  (database.cs:7758-7980)
//   * PR781+EQ         (database.cs:7987-8209)
//   * PR781+EQ+CPDR    (database.cs:8216-8438)
//   * SSB 2.8k CFC     (database.cs:8445-8667)
//   * SSB 3.0k CFC     (database.cs:8674-8896)
//   * SSB 3.3k CFC     (database.cs:8903-9125)
//   * AM 10k CFC       (database.cs:9132-9354)
//
// In Thetis the "extra profiles" group (Digi 1K@1500 onward) is gated
// by the bIndcludeExtraProfiles flag, which is true for the seed
// table "TXProfileDef" at database.cs:241 [v2.10.3.13] (this is the
// pristine factory set users restore from).  NereusSDR ships ALL
// factory profiles on every fresh install so users have the same
// bank Thetis users would see after a "restore defaults" — keeping
// the userland parity invariant from
// `feedback_thetis_userland_parity` and `feedback_thetis_parity_over_local_optimization`.
//
// Per-profile only the keys that DEVIATE from defaultProfileValues()
// are stored as overrides — the seed pipeline merges them into a
// full default hash before writing to AppSettings.  This keeps the
// override tables compact and matches the bit-for-bit layout of the
// Thetis source rows (columns absent from the override hash inherit
// defaultProfileValues, which already carries the database.cs row
// values for the live-fields-only subset).
//
// All TXEQ band/freq numeric values, MicGain, Lev_*, and ALC_*
// values come byte-for-byte from the cited source lines.  Do NOT
// "improve" or round any value — see CLAUDE.md "Constants and
// Magic Numbers" rule.
// ---------------------------------------------------------------------------

struct FactoryProfile {
    QString name;
    QHash<QString, QVariant> overrides;  // keys that deviate from defaultProfileValues()
};

// Helper: build the full 22-key EQ portion for a factory profile.  All 21
// Thetis factory profiles set TXEQ1..10 + TxEqFreq1..10 explicitly in
// database.cs, so we must encode every value byte-for-byte (NereusSDR's
// defaultProfileValues() uses WDSP defaults like {-12,-12,-12,-1,1,4,9,
// 12,-10,-10} which DIVERGE from the Thetis row defaults).
//
// Two convenience makers cover the EQ shapes seen in the Thetis source:
//   eqAllZeros()  — TXEQ1..10 = 0, freq grid 32/63/.../16000 (the
//                   default EQ-off row used by Default DX, AM, ESSB,
//                   Digi, HC4-5, PR40+W2IHY, DX/Contest, Conventional)
//   eqD104()      — TXEQ1=7, TXEQ2=3, TXEQ3=4, TXEQ4..10=0, default freq grid
//   eqPr781Eq()   — TXEQ1=-6, TXEQ2=2, TXEQ3=8, TXEQ4..10=0, default freq grid
//   eqPr781EqCpdr() — TXEQ1=-8, TXEQ2=3, TXEQ3=7, TXEQ4..10=0, default freq grid
//   eqSsbCfc()    — TXEQ1..10 from the CFC bell-curve row, custom freq grid
//                   (30/70/300/500/1k/2k/3k/4k/5k/6k Hz).
//
// All maker functions take their cite line numbers as parameters so the
// inline-tag preservation script can verify the cited Thetis source rows.
QHash<QString, QVariant> eqAllZeros()
{
    QHash<QString, QVariant> out;
    for (int i = 1; i <= 10; ++i) {
        out.insert(QStringLiteral("TXEQ%1").arg(i),       QStringLiteral("0"));
    }
    static const int kF[10] = {32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    for (int i = 0; i < 10; ++i) {
        out.insert(QStringLiteral("TxEqFreq%1").arg(i + 1),
                   QString::number(kF[i]));
    }
    return out;
}

// D-104 / D-104+CPDR / D-104+EQ all share the same EQ shape; only
// TXEQEnabled and MicGain vary.
// EQ shape from Thetis database.cs:5935-5945 [v2.10.3.13] for D-104.
QHash<QString, QVariant> eqD104()
{
    QHash<QString, QVariant> out = eqAllZeros();
    out.insert(QStringLiteral("TXEQ1"), QStringLiteral("7"));   // database.cs:5936
    out.insert(QStringLiteral("TXEQ2"), QStringLiteral("3"));   // database.cs:5937
    out.insert(QStringLiteral("TXEQ3"), QStringLiteral("4"));   // database.cs:5938
    return out;
}

// EQ shape from Thetis database.cs:7997-8006 [v2.10.3.13] for PR781+EQ.
QHash<QString, QVariant> eqPr781Eq()
{
    QHash<QString, QVariant> out = eqAllZeros();
    out.insert(QStringLiteral("TXEQ1"), QStringLiteral("-6"));  // database.cs:7997
    out.insert(QStringLiteral("TXEQ2"), QStringLiteral("2"));   // database.cs:7998
    out.insert(QStringLiteral("TXEQ3"), QStringLiteral("8"));   // database.cs:7999
    return out;
}

// EQ shape from Thetis database.cs:8226-8235 [v2.10.3.13] for PR781+EQ+CPDR.
QHash<QString, QVariant> eqPr781EqCpdr()
{
    QHash<QString, QVariant> out = eqAllZeros();
    out.insert(QStringLiteral("TXEQ1"), QStringLiteral("-8"));  // database.cs:8226
    out.insert(QStringLiteral("TXEQ2"), QStringLiteral("3"));   // database.cs:8227
    out.insert(QStringLiteral("TXEQ3"), QStringLiteral("7"));   // database.cs:8228
    return out;
}

// EQ shape from Thetis database.cs:8455-8474 [v2.10.3.13] for SSB 2.8k CFC.
// SSB 3.0k / 3.3k / AM 10k CFC share this shape (only FilterLow/High differ
// in source, which we don't bundle).
QHash<QString, QVariant> eqSsbCfc()
{
    QHash<QString, QVariant> out;
    static const int kG[10] = {-9, -6, -4, -3, -2, -1, -1, -1, -2, -2};
    static const int kF[10] = {30, 70, 300, 500, 1000, 2000, 3000, 4000, 5000, 6000};
    for (int i = 0; i < 10; ++i) {
        out.insert(QStringLiteral("TXEQ%1").arg(i + 1),     QString::number(kG[i]));
        out.insert(QStringLiteral("TxEqFreq%1").arg(i + 1), QString::number(kF[i]));
    }
    return out;
}

// ── 3M-3a-ii Batch 4: CFC array helpers ─────────────────────────────────
// Helper: build a CFC override hash from per-band arrays.  The four
// "SSB *.* k CFC" + "AM 10k CFC" factory profiles each have full 30-cell
// CFC arrays that deviate from the defaults (CFCPreComp0..9 / CFCPostEqGain0..9
// / CFCEqFreq0..9).  Each helper takes the three 10-element rows in order
// and produces the corresponding override hash.  Cite line numbers passed
// in so the inline-tag preservation script can verify the cited Thetis
// source rows.
QHash<QString, QVariant> cfcArrays(const int (&pre)[10],
                                    const int (&post)[10],
                                    const int (&freq)[10])
{
    QHash<QString, QVariant> out;
    for (int i = 0; i < 10; ++i) {
        out.insert(QStringLiteral("CFCPreComp%1").arg(i),    QString::number(pre[i]));
        out.insert(QStringLiteral("CFCPostEqGain%1").arg(i), QString::number(post[i]));
        out.insert(QStringLiteral("CFCEqFreq%1").arg(i),     QString::number(freq[i]));
    }
    return out;
}

// Merge two override hashes (b takes priority over a).
QHash<QString, QVariant> mergeOverrides(QHash<QString, QVariant> a,
                                         const QHash<QString, QVariant>& b)
{
    for (auto it = b.constBegin(); it != b.constEnd(); ++it) {
        a.insert(it.key(), it.value());
    }
    return a;
}

const std::vector<FactoryProfile>& factoryProfiles()
{
    // ── Default DX (database.cs:4777-5000) ────────────────────────────
    // EQ off, all band gains 0, default freq grid, MicGain=5.
    // From Thetis database.cs:4785-4811 [v2.10.3.13] (TXEQEnabled..MicGain rows).
    // 3M-3a-ii: All CFC/CPDR/CESSB/PhRot values match defaults — no overrides
    // for those (CompanderLevel=2 at 4810, CESSB_On=false at 4919, CFC scalars
    // and arrays at 4954-4998 all match the Default row byte-for-byte).
    static const QHash<QString, QVariant> kDefaultDxOverrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),   QStringLiteral("200")},   // From Thetis database.cs:4778 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),  QStringLiteral("3100")},  // From Thetis database.cs:4779 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"), QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),  QStringLiteral("0")},
            {QStringLiteral("MicGain"),     QStringLiteral("5")},  // database.cs:4811
        });

    // ── Digi 1K@1500 (database.cs:5005-5231 // W4TME) ─────────────────
    // EQ off, narrow data filter, MicGain=5, Lev_On=false.
    // From Thetis database.cs:5017-5046 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=0 (deviates from default 2).  CESSB + CFC
    // scalars + CFC arrays + PhRot all match defaults — no overrides for those.
    static const QHash<QString, QVariant> kDigi1500Overrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("1000")},   // From Thetis database.cs:5010 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("2000")},   // From Thetis database.cs:5011 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("5")},      // database.cs:5043
            {QStringLiteral("Lev_On"),         QStringLiteral("False")},  // database.cs:5046
            {QStringLiteral("CompanderLevel"), QStringLiteral("0")},      // database.cs:5042
        });
    // ── Digi 1K@2210 (database.cs:5235-5461 // W4TME) ─────────────────
    // From Thetis database.cs:5247-5276 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=0 (deviates from default 2).
    static const QHash<QString, QVariant> kDigi2210Overrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("1710")},   // From Thetis database.cs:5240 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("2710")},   // From Thetis database.cs:5241 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("5")},      // database.cs:5273
            {QStringLiteral("Lev_On"),         QStringLiteral("False")},  // database.cs:5276
            {QStringLiteral("CompanderLevel"), QStringLiteral("0")},      // database.cs:5272
        });

    // ── AM (database.cs:5468-5690) ────────────────────────────────────
    // EQ off, default freq grid, MicGain=10.  Same EQ shape as Default DX.
    // From Thetis database.cs:5476-5502 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=3 (deviates from default 2).
    static const QHash<QString, QVariant> kAmOverrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("0")},     // From Thetis database.cs:5469 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("4000")},  // From Thetis database.cs:5470 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("10")},  // database.cs:5502
            {QStringLiteral("CompanderLevel"), QStringLiteral("3")},   // database.cs:5501
        });
    // ── Conventional (database.cs:5697-5919) ──────────────────────────
    // From Thetis database.cs:5705-5731 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=3 (deviates from default 2).
    static const QHash<QString, QVariant> kConventionalOverrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("100")},   // From Thetis database.cs:5698 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3100")},  // From Thetis database.cs:5699 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("10")},  // database.cs:5731
            {QStringLiteral("CompanderLevel"), QStringLiteral("3")},   // database.cs:5730
        });

    // ── D-104 (database.cs:5926-6148) ─────────────────────────────────
    // Astatic D-104 carbon mic — preamp -6 dB, B1/B2/B3 boost.  MicGain=25.
    // From Thetis database.cs:5934-5960 [v2.10.3.13] (EQ shape via eqD104()).
    // 3M-3a-ii: CompanderLevel=5 (deviates from default 2).
    static const QHash<QString, QVariant> kD104Overrides = mergeOverrides(
        eqD104(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("100")},   // From Thetis database.cs:5927 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3500")},  // From Thetis database.cs:5928 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},  // database.cs:5934
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("-6")},     // database.cs:5935
            {QStringLiteral("MicGain"),        QStringLiteral("25")},     // database.cs:5960
            {QStringLiteral("CompanderLevel"), QStringLiteral("5")},      // database.cs:5959
        });
    // ── D-104+CPDR (database.cs:6155-6377) ────────────────────────────
    // Same EQ shape as D-104, MicGain=20, expects external Compander on.
    // From Thetis database.cs:6163-6189 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=5 (deviates from default 2).
    static const QHash<QString, QVariant> kD104CpdrOverrides = mergeOverrides(
        eqD104(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("100")},   // From Thetis database.cs:6156 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3500")},  // From Thetis database.cs:6157 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},  // database.cs:6163
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("-6")},     // database.cs:6164
            {QStringLiteral("MicGain"),        QStringLiteral("20")},     // database.cs:6189
            {QStringLiteral("CompanderLevel"), QStringLiteral("5")},      // database.cs:6188
        });
    // ── D-104+EQ (database.cs:6384-6606) ──────────────────────────────
    // EQ ON, same shape as D-104, MicGain=20.
    // From Thetis database.cs:6392-6418 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=5 (deviates from default 2).
    static const QHash<QString, QVariant> kD104EqOverrides = mergeOverrides(
        eqD104(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("100")},   // From Thetis database.cs:6385 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3500")},  // From Thetis database.cs:6386 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("True")},   // database.cs:6392
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("-6")},     // database.cs:6393
            {QStringLiteral("MicGain"),        QStringLiteral("20")},     // database.cs:6418
            {QStringLiteral("CompanderLevel"), QStringLiteral("5")},      // database.cs:6417
        });

    // ── DX / Contest (database.cs:6613-6835) ──────────────────────────
    // EQ off, default freq grid, MicGain=10.
    // From Thetis database.cs:6621-6647 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=3 (deviates from default 2).
    static const QHash<QString, QVariant> kDxContestOverrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("250")},   // From Thetis database.cs:6614 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3250")},  // From Thetis database.cs:6615 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("10")},  // database.cs:6647
            {QStringLiteral("CompanderLevel"), QStringLiteral("3")},   // database.cs:6646
        });
    // ── ESSB (database.cs:6842-7064) ──────────────────────────────────
    // EQ off, Leveler off (extended-SSB users typically run external comp).
    // From Thetis database.cs:6850-6879 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=3 (deviates from default 2).
    static const QHash<QString, QVariant> kEssbOverrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("50")},    // From Thetis database.cs:6843 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3650")},  // From Thetis database.cs:6844 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("10")},     // database.cs:6876
            {QStringLiteral("Lev_On"),         QStringLiteral("False")},  // database.cs:6879
            {QStringLiteral("CompanderLevel"), QStringLiteral("3")},      // database.cs:6875
        });
    // ── HC4-5 (database.cs:7071-7293) ─────────────────────────────────
    // From Thetis database.cs:7079-7105 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=5 (deviates from default 2).
    static const QHash<QString, QVariant> kHc45Overrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("100")},   // From Thetis database.cs:7072 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3100")},  // From Thetis database.cs:7073 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("10")},  // database.cs:7105
            {QStringLiteral("CompanderLevel"), QStringLiteral("5")},   // database.cs:7104
        });
    // ── HC4-5+CPDR (database.cs:7300-7522) ────────────────────────────
    // From Thetis database.cs:7308-7334 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=5 (deviates from default 2).
    static const QHash<QString, QVariant> kHc45CpdrOverrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("100")},   // From Thetis database.cs:7301 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3100")},  // From Thetis database.cs:7302 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("10")},  // database.cs:7334
            {QStringLiteral("CompanderLevel"), QStringLiteral("5")},   // database.cs:7333
        });
    // ── PR40+W2IHY (database.cs:7529-7751) ────────────────────────────
    // From Thetis database.cs:7537-7563 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=3 (deviates from default 2).
    static const QHash<QString, QVariant> kPr40W2ihyOverrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("50")},    // From Thetis database.cs:7530 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3650")},  // From Thetis database.cs:7531 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("10")},  // database.cs:7563
            {QStringLiteral("CompanderLevel"), QStringLiteral("3")},   // database.cs:7562
        });
    // ── PR40+W2IHY+CPDR (database.cs:7758-7980) ───────────────────────
    // From Thetis database.cs:7766-7792 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=3 (deviates from default 2).
    static const QHash<QString, QVariant> kPr40W2ihyCpdrOverrides = mergeOverrides(
        eqAllZeros(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("50")},    // From Thetis database.cs:7759 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3650")},  // From Thetis database.cs:7760 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("False")},
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("0")},
            {QStringLiteral("MicGain"),        QStringLiteral("10")},  // database.cs:7792
            {QStringLiteral("CompanderLevel"), QStringLiteral("3")},   // database.cs:7791
        });

    // ── PR781+EQ (database.cs:7987-8209) ──────────────────────────────
    // Heil PR-781 dynamic mic — EQ on, preamp -11 dB, B1=-6, B2=+2, B3=+8
    // (broadcast-style bass-cut + presence-peak).  MicGain=12.
    // From Thetis database.cs:7995-8021 [v2.10.3.13] (EQ shape via eqPr781Eq()).
    // 3M-3a-ii: CompanderLevel=3 (deviates from default 2).
    static const QHash<QString, QVariant> kPr781EqOverrides = mergeOverrides(
        eqPr781Eq(),
        {
            {QStringLiteral("FilterLow"),      QStringLiteral("100")},   // From Thetis database.cs:7988 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),     QStringLiteral("3200")},  // From Thetis database.cs:7989 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),    QStringLiteral("True")},   // database.cs:7995
            {QStringLiteral("TXEQPreamp"),     QStringLiteral("-11")},    // database.cs:7996
            {QStringLiteral("MicGain"),        QStringLiteral("12")},     // database.cs:8021
            {QStringLiteral("CompanderLevel"), QStringLiteral("3")},      // database.cs:8020
        });
    // ── PR781+EQ+CPDR (database.cs:8216-8438) ─────────────────────────
    // PR-781 EQ tuned for use with external Compander.
    // From Thetis database.cs:8224-8250 [v2.10.3.13].
    // 3M-3a-ii: CompanderLevel=2 matches default — no CPDR override.  All other
    // CFC/CESSB/PhRot values match defaults — no overrides for those.
    static const QHash<QString, QVariant> kPr781EqCpdrOverrides = mergeOverrides(
        eqPr781EqCpdr(),
        {
            {QStringLiteral("FilterLow"),   QStringLiteral("100")},   // From Thetis database.cs:8217 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),  QStringLiteral("3200")},  // From Thetis database.cs:8218 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"), QStringLiteral("True")},   // database.cs:8224
            {QStringLiteral("TXEQPreamp"),  QStringLiteral("-9")},     // database.cs:8225
            {QStringLiteral("MicGain"),     QStringLiteral("10")},     // database.cs:8250
        });

    // ── SSB 2.8k CFC (database.cs:8445-8667) ──────────────────────────
    // CFC-tuned EQ — preamp +4 dB, gentle bell shape across upper bands,
    // custom freq grid (30/70/300/500/1k/2k/3k/4k/5k/6k Hz).  EQ shape
    // delivered via eqSsbCfc(), which encodes the full 10-band shape +
    // the custom freq grid byte-for-byte from database.cs:8455-8474.
    // 3M-3a-ii: full CFC override block — CFCEnabled+CFCPostEqEnabled true,
    // CFCPreComp=6, CFCPostEqGain=-6, full CFC arrays from database.cs:8632-8663,
    // CompanderLevel=1.  PhRot and CESSB match defaults.
    static const int kSsb28PreComp[10]  = { 5, 5, 5, 4, 5, 6, 7, 8, 9, 9 };       // database.cs:8632-8641
    static const int kSsb28PostEq[10]   = {-7,-7,-8,-8,-8,-7,-5,-4,-4,-5 };       // database.cs:8643-8652
    static const int kSsb28EqFreq[10]   = { 100, 150, 300, 500, 750, 1250, 1750, 2000, 2600, 2900 }; // database.cs:8654-8663
    static const QHash<QString, QVariant> kSsb28CfcOverrides = mergeOverrides(
        mergeOverrides(eqSsbCfc(),
                       cfcArrays(kSsb28PreComp, kSsb28PostEq, kSsb28EqFreq)),
        {
            {QStringLiteral("FilterLow"),         QStringLiteral("100")},   // From Thetis database.cs:8446 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),        QStringLiteral("2900")},  // From Thetis database.cs:8447 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),       QStringLiteral("True")},  // database.cs:8453
            {QStringLiteral("TXEQPreamp"),        QStringLiteral("4")},     // database.cs:8454
            {QStringLiteral("MicGain"),           QStringLiteral("10")},    // database.cs:8479
            {QStringLiteral("CompanderLevel"),    QStringLiteral("1")},     // database.cs:8478
            {QStringLiteral("CFCEnabled"),        QStringLiteral("True")},  // database.cs:8621
            {QStringLiteral("CFCPostEqEnabled"),  QStringLiteral("True")},  // database.cs:8622
            {QStringLiteral("CFCPreComp"),        QStringLiteral("6")},     // database.cs:8629
            {QStringLiteral("CFCPostEqGain"),     QStringLiteral("-6")},    // database.cs:8630
        });
    // ── SSB 3.0k CFC (database.cs:8674-8896) ──────────────────────────
    // Identical EQ shape to SSB 2.8k CFC; different CFC arrays + CFCPostEqGain.
    // From Thetis database.cs:8861-8892 [v2.10.3.13].
    static const int kSsb30PreComp[10]  = { 6, 6, 5, 4, 5, 6, 7, 8, 9, 9 };       // database.cs:8861-8870
    static const int kSsb30PostEq[10]   = {-4,-5,-7,-8,-8,-7,-4,-2,-1,-1 };       // database.cs:8872-8881
    static const int kSsb30EqFreq[10]   = { 100, 150, 300, 500, 750, 1250, 1750, 2000, 2600, 3100 }; // database.cs:8883-8892
    static const QHash<QString, QVariant> kSsb30CfcOverrides = mergeOverrides(
        mergeOverrides(eqSsbCfc(),
                       cfcArrays(kSsb30PreComp, kSsb30PostEq, kSsb30EqFreq)),
        {
            {QStringLiteral("FilterLow"),         QStringLiteral("100")},   // From Thetis database.cs:8675 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),        QStringLiteral("3100")},  // From Thetis database.cs:8676 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),       QStringLiteral("True")},  // database.cs:8682 (mirror of 28k)
            {QStringLiteral("TXEQPreamp"),        QStringLiteral("4")},     // database.cs:8683
            {QStringLiteral("MicGain"),           QStringLiteral("10")},    // database.cs:8708
            {QStringLiteral("CompanderLevel"),    QStringLiteral("1")},     // database.cs:8707
            {QStringLiteral("CFCEnabled"),        QStringLiteral("True")},  // database.cs:8850
            {QStringLiteral("CFCPostEqEnabled"),  QStringLiteral("True")},  // database.cs:8851
            {QStringLiteral("CFCPreComp"),        QStringLiteral("6")},     // database.cs:8858
            {QStringLiteral("CFCPostEqGain"),     QStringLiteral("-7")},    // database.cs:8859
        });
    // ── SSB 3.3k CFC (database.cs:8903-9125) ──────────────────────────
    // From Thetis database.cs:9090-9121 [v2.10.3.13].
    static const int kSsb33PreComp[10]  = { 4, 4, 3, 3, 4, 5, 6, 7, 8, 8 };       // database.cs:9090-9099
    static const int kSsb33PostEq[10]   = {-6,-6,-7,-8,-8,-7,-5,-5,-4,-5 };       // database.cs:9101-9110
    static const int kSsb33EqFreq[10]   = { 50, 150, 300, 500, 750, 1250, 1750, 2000, 2600, 3350 };  // database.cs:9112-9121
    static const QHash<QString, QVariant> kSsb33CfcOverrides = mergeOverrides(
        mergeOverrides(eqSsbCfc(),
                       cfcArrays(kSsb33PreComp, kSsb33PostEq, kSsb33EqFreq)),
        {
            {QStringLiteral("FilterLow"),         QStringLiteral("50")},    // From Thetis database.cs:8904 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),        QStringLiteral("3350")},  // From Thetis database.cs:8905 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),       QStringLiteral("True")},  // database.cs:8911
            {QStringLiteral("TXEQPreamp"),        QStringLiteral("4")},     // database.cs:8912
            {QStringLiteral("MicGain"),           QStringLiteral("10")},    // database.cs:8937
            {QStringLiteral("CompanderLevel"),    QStringLiteral("1")},     // database.cs:8936
            {QStringLiteral("CFCEnabled"),        QStringLiteral("True")},  // database.cs:9079
            {QStringLiteral("CFCPostEqEnabled"),  QStringLiteral("True")},  // database.cs:9080
            {QStringLiteral("CFCPreComp"),        QStringLiteral("7")},     // database.cs:9087
            {QStringLiteral("CFCPostEqGain"),     QStringLiteral("-6")},    // database.cs:9088
        });
    // ── AM 10k CFC (database.cs:9132-9354) ────────────────────────────
    // Same EQ shape as SSB CFC profiles, used for AM with CFC.
    // CFCPhaseRotatorStages=9 here (deviates from default 8) — see
    // database.cs:9314 [v2.10.3.13].  Full CFC arrays from database.cs:9319-9350.
    static const int kAm10kPreComp[10]  = { 6, 5, 4, 4, 4, 5, 6, 7, 7, 7 };        // database.cs:9319-9328
    static const int kAm10kPostEq[10]   = { 0,-1,-2,-3,-3,-2,-1, 0, 1, 1 };        // database.cs:9330-9339
    static const int kAm10kEqFreq[10]   = { 0, 70, 250, 500, 1000, 1500, 2000, 3000, 4000, 5000 };   // database.cs:9341-9350
    static const QHash<QString, QVariant> kAm10kCfcOverrides = mergeOverrides(
        mergeOverrides(eqSsbCfc(),
                       cfcArrays(kAm10kPreComp, kAm10kPostEq, kAm10kEqFreq)),
        {
            {QStringLiteral("FilterLow"),           QStringLiteral("0")},     // From Thetis database.cs:9133 [v2.10.3.13]
            {QStringLiteral("FilterHigh"),          QStringLiteral("5000")},  // From Thetis database.cs:9134 [v2.10.3.13]
            {QStringLiteral("TXEQEnabled"),         QStringLiteral("True")},  // database.cs:9140
            {QStringLiteral("TXEQPreamp"),          QStringLiteral("4")},     // database.cs:9141
            {QStringLiteral("MicGain"),             QStringLiteral("10")},    // database.cs:9166
            {QStringLiteral("CompanderLevel"),      QStringLiteral("1")},     // database.cs:9165
            {QStringLiteral("CFCEnabled"),          QStringLiteral("True")},  // database.cs:9308
            {QStringLiteral("CFCPostEqEnabled"),    QStringLiteral("True")},  // database.cs:9309
            {QStringLiteral("CFCPhaseRotatorStages"), QStringLiteral("9")},   // database.cs:9314
            {QStringLiteral("CFCPreComp"),          QStringLiteral("6")},     // database.cs:9316
            {QStringLiteral("CFCPostEqGain"),       QStringLiteral("-8")},    // database.cs:9317
        });

    static const std::vector<FactoryProfile> kProfiles = {
        // Default DX always ships (Thetis seeds it unconditionally —
        // database.cs:241 calls AddTXProfileTable("TXProfileDef", true)
        // which writes both Default + Default DX into the active table).
        {QStringLiteral("Default DX"),        kDefaultDxOverrides},
        {QStringLiteral("Digi 1K@1500"),      kDigi1500Overrides},
        {QStringLiteral("Digi 1K@2210"),      kDigi2210Overrides},
        {QStringLiteral("AM"),                kAmOverrides},
        {QStringLiteral("Conventional"),      kConventionalOverrides},
        {QStringLiteral("D-104"),             kD104Overrides},
        {QStringLiteral("D-104+CPDR"),        kD104CpdrOverrides},
        {QStringLiteral("D-104+EQ"),          kD104EqOverrides},
        {QStringLiteral("DX / Contest"),      kDxContestOverrides},
        {QStringLiteral("ESSB"),              kEssbOverrides},
        {QStringLiteral("HC4-5"),             kHc45Overrides},
        {QStringLiteral("HC4-5+CPDR"),        kHc45CpdrOverrides},
        {QStringLiteral("PR40+W2IHY"),        kPr40W2ihyOverrides},
        {QStringLiteral("PR40+W2IHY+CPDR"),   kPr40W2ihyCpdrOverrides},
        {QStringLiteral("PR781+EQ"),          kPr781EqOverrides},
        {QStringLiteral("PR781+EQ+CPDR"),     kPr781EqCpdrOverrides},
        {QStringLiteral("SSB 2.8k CFC"),      kSsb28CfcOverrides},
        {QStringLiteral("SSB 3.0k CFC"),      kSsb30CfcOverrides},
        {QStringLiteral("SSB 3.3k CFC"),      kSsb33CfcOverrides},
        {QStringLiteral("AM 10k CFC"),        kAm10kCfcOverrides},
    };
    return kProfiles;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// MicProfileManager
// ---------------------------------------------------------------------------

MicProfileManager::MicProfileManager(QObject* parent)
    : QObject(parent)
{
}

MicProfileManager::~MicProfileManager() = default;

void MicProfileManager::setMacAddress(const QString& mac)
{
    m_mac = mac;
}

QStringList MicProfileManager::readManifest() const
{
    if (m_mac.isEmpty()) {
        return {};
    }
    const QString raw = AppSettings::instance().value(manifestKey(m_mac)).toString();
    if (raw.isEmpty()) {
        return {};
    }
    QStringList names = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    names.removeDuplicates();
    return names;
}

void MicProfileManager::writeManifest(const QStringList& names)
{
    if (m_mac.isEmpty()) {
        return;
    }
    AppSettings::instance().setValue(manifestKey(m_mac),
                                     names.join(QLatin1Char(',')));
}

QString MicProfileManager::readActiveKey() const
{
    if (m_mac.isEmpty()) {
        return {};
    }
    return AppSettings::instance().value(activeKey(m_mac)).toString();
}

void MicProfileManager::writeActiveKey(const QString& name)
{
    if (m_mac.isEmpty()) {
        return;
    }
    AppSettings::instance().setValue(activeKey(m_mac), name);
}

void MicProfileManager::load()
{
    if (m_mac.isEmpty()) {
        return;
    }

    QStringList names = readManifest();
    if (names.isEmpty()) {
        // F.5 + 3M-3a-i Batch 4 (Task A.2): first-launch seed.
        // Write "Default" (with the live-field defaults from
        // defaultProfileValues()) PLUS the 21 Thetis factory profiles
        // ported from database.cs:AddTXProfileTable [v2.10.3.13].
        // Each factory profile is built by overlaying its per-profile
        // overrides on top of the base defaults — this mirrors the
        // way Thetis itself ships the "TXProfileDef" table at
        // database.cs:241 (bIndcludeExtraProfiles=true).
        const QHash<QString, QVariant> defaults = defaultProfileValues();
        writeProfileKeys(QStringLiteral("Default"), defaults);
        names = QStringList{QStringLiteral("Default")};

        for (const FactoryProfile& fp : factoryProfiles()) {
            QHash<QString, QVariant> merged = defaults;
            for (auto it = fp.overrides.constBegin();
                 it != fp.overrides.constEnd(); ++it) {
                merged.insert(it.key(), it.value());
            }
            writeProfileKeys(fp.name, merged);
            names.append(fp.name);
        }

        writeManifest(names);
        // Active defaults to "Default" on first launch — factory
        // profiles are present but not selected, so existing-user
        // first-run behaviour is unchanged.
        writeActiveKey(QStringLiteral("Default"));
        emit profileListChanged();
    }
    // No emit on subsequent loads — the manifest already exists.
}

QStringList MicProfileManager::profileNames() const
{
    QStringList names = readManifest();
    std::sort(names.begin(), names.end());
    return names;
}

QString MicProfileManager::activeProfileName() const
{
    const QString active = readActiveKey();
    if (active.isEmpty()) {
        return QStringLiteral("Default");
    }
    return active;
}

void MicProfileManager::writeProfileKeys(const QString& name,
                                         const QHash<QString, QVariant>& values)
{
    if (m_mac.isEmpty()) {
        return;
    }
    auto& s = AppSettings::instance();
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        s.setValue(profileFieldKey(m_mac, name, it.key()),
                   it.value().toString());
    }
}

QHash<QString, QVariant> MicProfileManager::readProfileKeys(const QString& name) const
{
    QHash<QString, QVariant> out;
    if (m_mac.isEmpty()) {
        return out;
    }
    auto& s = AppSettings::instance();
    for (const QString& key : liveKeyList()) {
        const QString full = profileFieldKey(m_mac, name, key);
        if (s.contains(full)) {
            out.insert(key, s.value(full));
        }
    }
    return out;
}

void MicProfileManager::removeProfileKeys(const QString& name)
{
    if (m_mac.isEmpty()) {
        return;
    }
    auto& s = AppSettings::instance();
    for (const QString& key : liveKeyList()) {
        s.remove(profileFieldKey(m_mac, name, key));
    }
}

bool MicProfileManager::saveProfile(const QString& rawName, const TransmitModel* tx)
{
    if (m_mac.isEmpty() || tx == nullptr) {
        return false;
    }

    // F.2 / Thetis precedent: setup.cs:9550-9552 [v2.10.3.13].
    // no more , in profile names, because it will make the tci tx profile messages look like they have multiple parts
    // existing ones will cause issues no doubt, but just not worth the effort to reparse the database
    QString name = rawName;
    name.replace(QLatin1Char(','), QLatin1Char('_'));

    if (name.isEmpty()) {
        return false;
    }

    const QHash<QString, QVariant> values = captureLiveValues(tx);
    writeProfileKeys(name, values);

    QStringList names = readManifest();
    const bool wasInList = names.contains(name);
    if (!wasInList) {
        names.append(name);
        writeManifest(names);
        emit profileListChanged();
    }
    return true;
}

bool MicProfileManager::deleteProfile(const QString& name)
{
    if (m_mac.isEmpty()) {
        return false;
    }

    QStringList names = readManifest();

    // F.3 / Thetis precedent: setup.cs:9617-9624 [v2.10.3.13]
    //   if (comboTXProfileName.Items.Count == 1) { MessageBox.Show("It is not
    //   possible to delete the last remaining TX profile."); return; }
    if (names.size() <= 1) {
        qCWarning(lcDsp) << "It is not possible to delete the last remaining TX profile";
        return false;
    }

    if (!names.contains(name)) {
        return false;
    }

    removeProfileKeys(name);
    names.removeAll(name);
    writeManifest(names);

    // Active fallback.  If we just deleted the active profile, fall back to
    // the first remaining (sorted lexicographically — same surface the UI
    // sees via profileNames()).
    const QString currentActive = readActiveKey();
    if (currentActive == name) {
        QStringList sorted = names;
        std::sort(sorted.begin(), sorted.end());
        const QString fallback = sorted.first();
        writeActiveKey(fallback);
        emit activeProfileChanged(fallback);
    }

    emit profileListChanged();
    return true;
}

bool MicProfileManager::setActiveProfile(const QString& name, TransmitModel* tx)
{
    if (m_mac.isEmpty() || tx == nullptr) {
        return false;
    }

    const QStringList names = readManifest();
    if (!names.contains(name)) {
        return false;
    }

    const QHash<QString, QVariant> values = readProfileKeys(name);
    applyValuesToModel(values, tx);

    writeActiveKey(name);
    emit activeProfileChanged(name);
    // Freshly activated profile is by definition in sync with the stored state.
    emit profileModifiedChanged(false);
    return true;
}

bool MicProfileManager::saveActiveProfile(TransmitModel* tx)
{
    if (m_mac.isEmpty() || tx == nullptr) {
        return false;
    }
    const bool ok = saveProfile(activeProfileName(), tx);
    if (ok) {
        emit profileModifiedChanged(false);
    }
    return ok;
}

bool MicProfileManager::isActiveProfileModified(const TransmitModel* tx) const
{
    if (m_mac.isEmpty() || tx == nullptr) {
        return false;
    }
    const QString name = readActiveKey();
    if (name.isEmpty()) {
        // No active profile has been set yet — nothing to compare against.
        return false;
    }
    const QHash<QString, QVariant> stored = readProfileKeys(name);
    if (stored.isEmpty()) {
        // Profile key doesn't exist in AppSettings.
        return false;
    }

    // Plan 4 scope: compare FilterLow and FilterHigh only.
    // Plan 5 will expand this to all 93 profile fields.
    const int storedLow  = stored.value(QStringLiteral("FilterLow"),
                                         QStringLiteral("100")).toString().toInt();
    const int storedHigh = stored.value(QStringLiteral("FilterHigh"),
                                         QStringLiteral("2900")).toString().toInt();
    return (tx->filterLow() != storedLow || tx->filterHigh() != storedHigh);
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

QHash<QString, QVariant> MicProfileManager::defaultProfileValues()
{
    QHash<QString, QVariant> out;
    // Mic / VOX / MON (15)
    out.insert(QStringLiteral("MicGain"),              QStringLiteral("-6"));            // NereusSDR-original (plan §0 row 11)
    out.insert(QStringLiteral("Mic_Input_Boost"),      QStringLiteral("True"));          // console.cs:13237 [v2.10.3.13]
    out.insert(QStringLiteral("Mic_XLR"),              QStringLiteral("True"));          // console.cs:13249 [v2.10.3.13]
    out.insert(QStringLiteral("Line_Input_On"),        QStringLiteral("False"));         // console.cs:13213 [v2.10.3.13]
    out.insert(QStringLiteral("Line_Input_Level"),     QStringLiteral("0"));             // console.cs:13225 [v2.10.3.13]
    out.insert(QStringLiteral("Mic_TipRing"),          QStringLiteral("True"));          // setup.designer.cs:8683 [v2.10.3.13]
    out.insert(QStringLiteral("Mic_Bias"),             QStringLiteral("False"));         // setup.designer.cs:8779 [v2.10.3.13]
    out.insert(QStringLiteral("Mic_PTT_Disabled"),     QStringLiteral("False"));         // console.cs:19757 [v2.10.3.13]
    // line_in_gain + user_dig_out defaults (Task 2.4 of P1 full-parity epic).
    // Both default to 0 per Thetis ChannelMaster/networkproto1.c:600-601 [v2.10.3.13].
    out.insert(QStringLiteral("Mic_LineInGain"),       QStringLiteral("0"));
    out.insert(QStringLiteral("Mic_UserDigOut"),       QStringLiteral("0"));
    out.insert(QStringLiteral("Dexp_Threshold"),       QStringLiteral("-40"));           // NereusSDR-original (ptbVOX range -80..0)
    out.insert(QStringLiteral("VOX_GainScalar"),       QStringLiteral("1"));             // audio.cs:194 [v2.10.3.13]
    out.insert(QStringLiteral("VOX_HangTime"),         QStringLiteral("500"));           // setup.designer.cs:45020-45024 [v2.10.3.13]
    out.insert(QStringLiteral("AntiVox_Gain"),         QStringLiteral("0"));             // NereusSDR-original (Thetis udAntiVoxGain=1.0dB)
    out.insert(QStringLiteral("AntiVox_Source_VAX"),   QStringLiteral("False"));         // audio.cs:446 [v2.10.3.13]
    out.insert(QStringLiteral("MonitorVolume"),        QStringLiteral("0.5"));           // audio.cs:417 [v2.10.3.13]
    out.insert(QStringLiteral("Mic_Source"),           QStringLiteral("Pc"));            // NereusSDR-native (always-safe)

    // Two-tone (7) + drive-power source (1)
    out.insert(QStringLiteral("TwoToneFreq1"),         QStringLiteral("700"));           // setup.cs:34226 [v2.10.3.13]
    out.insert(QStringLiteral("TwoToneFreq2"),         QStringLiteral("1900"));          // setup.cs:34227 [v2.10.3.13]
    out.insert(QStringLiteral("TwoToneLevel"),         QStringLiteral("-6"));            // NereusSDR-original safer (Designer ships 0)
    out.insert(QStringLiteral("TwoTonePower"),         QStringLiteral("50"));            // NereusSDR-original (Designer ships 10)
    out.insert(QStringLiteral("TwoToneFreq2Delay"),    QStringLiteral("0"));             // setup.Designer.cs:61943-61947 [v2.10.3.13]
    out.insert(QStringLiteral("TwoToneInvert"),        QStringLiteral("True"));          // setup.Designer.cs:61963 [v2.10.3.13]
    out.insert(QStringLiteral("TwoTonePulsed"),        QStringLiteral("False"));         // setup.Designer.cs:61643-61653 [v2.10.3.13]
    out.insert(QStringLiteral("TwoToneDrivePowerOrigin"),
               QStringLiteral("DriveSlider"));                                            // console.cs:46553 [v2.10.3.13]

    // ── 3M-3a-i G — TX EQ + Leveler + ALC defaults (27 keys) ─────────────
    // From Thetis database.cs:4552-4594 [v2.10.3.13] (TXProfile schema).
    // EQ band/freq defaults from WDSP TXA.c:112-113 [v2.10.3.13]:
    //   default_F[1..10] = {32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    //   default_G[1..10] = {-12, -12, -12, -1, +1, +4, +9, +12, -10, -10};
    //   //double default_G[11] =   {0.0,   0.0,   0.0,   0.0,   0.0,   0.0,    0.0,    0.0,    0.0,    0.0,     0.0};
    out.insert(QStringLiteral("TXEQEnabled"),    QStringLiteral("False"));   // database.cs:4553
    out.insert(QStringLiteral("TXEQPreamp"),     QStringLiteral("0"));       // database.cs:4554
    out.insert(QStringLiteral("TXEQ1"),          QStringLiteral("-12"));     // WDSP TXA.c:113 default_G[1]
    out.insert(QStringLiteral("TXEQ2"),          QStringLiteral("-12"));     // WDSP TXA.c:113 default_G[2]
    out.insert(QStringLiteral("TXEQ3"),          QStringLiteral("-12"));     // WDSP TXA.c:113 default_G[3]
    out.insert(QStringLiteral("TXEQ4"),          QStringLiteral("-1"));      // WDSP TXA.c:113 default_G[4]
    out.insert(QStringLiteral("TXEQ5"),          QStringLiteral("1"));       // WDSP TXA.c:113 default_G[5]
    out.insert(QStringLiteral("TXEQ6"),          QStringLiteral("4"));       // WDSP TXA.c:113 default_G[6]
    out.insert(QStringLiteral("TXEQ7"),          QStringLiteral("9"));       // WDSP TXA.c:113 default_G[7]
    out.insert(QStringLiteral("TXEQ8"),          QStringLiteral("12"));      // WDSP TXA.c:113 default_G[8]
    out.insert(QStringLiteral("TXEQ9"),          QStringLiteral("-10"));     // WDSP TXA.c:113 default_G[9]
    out.insert(QStringLiteral("TXEQ10"),         QStringLiteral("-10"));     // WDSP TXA.c:113 default_G[10]
    out.insert(QStringLiteral("TxEqFreq1"),      QStringLiteral("32"));      // WDSP TXA.c:112 default_F[1]
    out.insert(QStringLiteral("TxEqFreq2"),      QStringLiteral("63"));      // WDSP TXA.c:112 default_F[2]
    out.insert(QStringLiteral("TxEqFreq3"),      QStringLiteral("125"));     // WDSP TXA.c:112 default_F[3]
    out.insert(QStringLiteral("TxEqFreq4"),      QStringLiteral("250"));     // WDSP TXA.c:112 default_F[4]
    out.insert(QStringLiteral("TxEqFreq5"),      QStringLiteral("500"));     // WDSP TXA.c:112 default_F[5]
    out.insert(QStringLiteral("TxEqFreq6"),      QStringLiteral("1000"));    // WDSP TXA.c:112 default_F[6]
    out.insert(QStringLiteral("TxEqFreq7"),      QStringLiteral("2000"));    // WDSP TXA.c:112 default_F[7]
    out.insert(QStringLiteral("TxEqFreq8"),      QStringLiteral("4000"));    // WDSP TXA.c:112 default_F[8]
    out.insert(QStringLiteral("TxEqFreq9"),      QStringLiteral("8000"));    // WDSP TXA.c:112 default_F[9]
    out.insert(QStringLiteral("TxEqFreq10"),     QStringLiteral("16000"));   // WDSP TXA.c:112 default_F[10]
    // TX EQ blob (1) — opaque, forward-compat for imported Thetis profiles.
    // No Thetis database.cs row; column ships empty.  Phase 3M-3a-ii follow-up
    // Batch 6.
    out.insert(QStringLiteral("TXParaEQData"),   QString());
    out.insert(QStringLiteral("Lev_On"),         QStringLiteral("True"));    // database.cs:4584
    out.insert(QStringLiteral("Lev_MaxGain"),    QStringLiteral("15"));      // database.cs:4586
    out.insert(QStringLiteral("Lev_Decay"),      QStringLiteral("100"));     // database.cs:4588
    out.insert(QStringLiteral("ALC_MaximumGain"), QStringLiteral("3"));      // database.cs:4592
    out.insert(QStringLiteral("ALC_Decay"),      QStringLiteral("10"));      // database.cs:4594

    // ── 3M-3a-ii G — CFC + CPDR + CESSB + PhRot defaults (41 keys) ───────
    // From Thetis database.cs:4724-4768 [v2.10.3.13] (the "Default" factory
    // profile row, which is also the bundle-default baseline) +
    // database.cs:4580 (CompanderLevel) + database.cs:4689 (CESSB_On).
    // Phase Rotator (4)
    out.insert(QStringLiteral("CFCPhaseRotatorEnabled"), QStringLiteral("False"));   // database.cs:4726
    out.insert(QStringLiteral("CFCPhaseReverseEnabled"), QStringLiteral("False"));   // database.cs:4727
    out.insert(QStringLiteral("CFCPhaseRotatorFreq"),    QStringLiteral("338"));     // database.cs:4729
    out.insert(QStringLiteral("CFCPhaseRotatorStages"),  QStringLiteral("8"));       // database.cs:4730
    // CFC scalars (4)
    out.insert(QStringLiteral("CFCEnabled"),         QStringLiteral("False"));       // database.cs:4724
    out.insert(QStringLiteral("CFCPostEqEnabled"),   QStringLiteral("False"));       // database.cs:4725
    out.insert(QStringLiteral("CFCPreComp"),         QStringLiteral("0"));           // database.cs:4732
    out.insert(QStringLiteral("CFCPostEqGain"),      QStringLiteral("0"));           // database.cs:4733
    // CFC profile arrays (30) — CFCPreComp0..9 = 5 × 10 (database.cs:4735-4744),
    // CFCPostEqGain0..9 = 0 × 10 (database.cs:4746-4755), CFCEqFreq grid
    // 0/125/250/500/1000/2000/3000/4000/5000/10000 (database.cs:4757-4766).
    for (int i = 0; i < 10; ++i) {
        out.insert(QStringLiteral("CFCPreComp%1").arg(i),    QStringLiteral("5"));   // database.cs:4735-4744
        out.insert(QStringLiteral("CFCPostEqGain%1").arg(i), QStringLiteral("0"));   // database.cs:4746-4755
    }
    out.insert(QStringLiteral("CFCEqFreq0"), QStringLiteral("0"));      // database.cs:4757
    out.insert(QStringLiteral("CFCEqFreq1"), QStringLiteral("125"));    // database.cs:4758
    out.insert(QStringLiteral("CFCEqFreq2"), QStringLiteral("250"));    // database.cs:4759
    out.insert(QStringLiteral("CFCEqFreq3"), QStringLiteral("500"));    // database.cs:4760
    out.insert(QStringLiteral("CFCEqFreq4"), QStringLiteral("1000"));   // database.cs:4761
    out.insert(QStringLiteral("CFCEqFreq5"), QStringLiteral("2000"));   // database.cs:4762
    out.insert(QStringLiteral("CFCEqFreq6"), QStringLiteral("3000"));   // database.cs:4763
    out.insert(QStringLiteral("CFCEqFreq7"), QStringLiteral("4000"));   // database.cs:4764
    out.insert(QStringLiteral("CFCEqFreq8"), QStringLiteral("5000"));   // database.cs:4765
    out.insert(QStringLiteral("CFCEqFreq9"), QStringLiteral("10000"));  // database.cs:4766
    // CFC blob (1) — opaque, forward-compat for imported Thetis profiles.
    out.insert(QStringLiteral("CFCParaEQData"), QString());                          // database.cs:4768
    // CPDR (1)
    out.insert(QStringLiteral("CompanderLevel"), QStringLiteral("2"));               // database.cs:4580
    // CESSB (1)
    out.insert(QStringLiteral("CESSB_On"), QStringLiteral("False"));                 // database.cs:4689

    // ── Plan 4 D1 — TX filter bandwidth (2 keys) ─────────────────────────
    // NereusSDR-original defaults: 100/2900 Hz (USB voice typical SSB).
    // Per Plan 4 spec §Task 2 (docs/superpowers/plans/2026-05-01-tx-bandwidth.md).
    out.insert(QStringLiteral("FilterLow"),  QStringLiteral("100"));   // Plan 4 D1 — USB voice SSB default
    out.insert(QStringLiteral("FilterHigh"), QStringLiteral("2900"));  // Plan 4 D1 — USB voice SSB default
    return out;
}

QHash<QString, QVariant> MicProfileManager::captureLiveValues(const TransmitModel* tx)
{
    QHash<QString, QVariant> out;
    if (tx == nullptr) {
        return out;
    }

    // Mic / VOX / MON (15)
    out.insert(QStringLiteral("MicGain"),              QString::number(tx->micGainDb()));
    out.insert(QStringLiteral("Mic_Input_Boost"),      tx->micBoost()      ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("Mic_XLR"),              tx->micXlr()        ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("Line_Input_On"),        tx->lineIn()        ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("Line_Input_Level"),     QString::number(tx->lineInBoost()));
    out.insert(QStringLiteral("Mic_TipRing"),          tx->micTipRing()    ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("Mic_Bias"),             tx->micBias()       ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("Mic_PTT_Disabled"),     tx->micPttDisabled() ? QStringLiteral("True") : QStringLiteral("False"));
    // line_in_gain + user_dig_out (Task 2.4 of P1 full-parity epic)
    out.insert(QStringLiteral("Mic_LineInGain"),       QString::number(tx->lineInGain()));
    out.insert(QStringLiteral("Mic_UserDigOut"),       QString::number(tx->userDigOut()));
    out.insert(QStringLiteral("Dexp_Threshold"),       QString::number(tx->voxThresholdDb()));
    out.insert(QStringLiteral("VOX_GainScalar"),       QString::number(static_cast<double>(tx->voxGainScalar())));
    out.insert(QStringLiteral("VOX_HangTime"),         QString::number(tx->voxHangTimeMs()));
    out.insert(QStringLiteral("AntiVox_Gain"),         QString::number(tx->antiVoxGainDb()));
    out.insert(QStringLiteral("AntiVox_Source_VAX"),   tx->antiVoxSourceVax() ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("MonitorVolume"),        QString::number(static_cast<double>(tx->monitorVolume())));
    out.insert(QStringLiteral("Mic_Source"),
               tx->micSource() == MicSource::Radio ? QStringLiteral("Radio") : QStringLiteral("Pc"));

    // Two-tone (7) + drive-power source (1)
    out.insert(QStringLiteral("TwoToneFreq1"),         QString::number(tx->twoToneFreq1()));
    out.insert(QStringLiteral("TwoToneFreq2"),         QString::number(tx->twoToneFreq2()));
    out.insert(QStringLiteral("TwoToneLevel"),         QString::number(tx->twoToneLevel()));
    out.insert(QStringLiteral("TwoTonePower"),         QString::number(tx->twoTonePower()));
    out.insert(QStringLiteral("TwoToneFreq2Delay"),    QString::number(tx->twoToneFreq2Delay()));
    out.insert(QStringLiteral("TwoToneInvert"),        tx->twoToneInvert() ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("TwoTonePulsed"),        tx->twoTonePulsed() ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("TwoToneDrivePowerOrigin"),
               drivePowerSourceToString(tx->twoToneDrivePowerSource()));

    // ── 3M-3a-i G — TX EQ + Leveler + ALC live capture (27 keys) ─────────
    out.insert(QStringLiteral("TXEQEnabled"),
               tx->txEqEnabled() ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("TXEQPreamp"), QString::number(tx->txEqPreamp()));
    for (int i = 0; i < 10; ++i) {
        out.insert(QStringLiteral("TXEQ%1").arg(i + 1),
                   QString::number(tx->txEqBand(i)));
        out.insert(QStringLiteral("TxEqFreq%1").arg(i + 1),
                   QString::number(tx->txEqFreq(i)));
    }
    // TX EQ blob (1) — opaque QString fits QVariant directly.  Phase 3M-3a-ii
    // follow-up Batch 6.
    out.insert(QStringLiteral("TXParaEQData"), tx->txEqParaEqData());
    out.insert(QStringLiteral("Lev_On"),
               tx->txLevelerOn() ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("Lev_MaxGain"), QString::number(tx->txLevelerMaxGain()));
    out.insert(QStringLiteral("Lev_Decay"),   QString::number(tx->txLevelerDecay()));
    out.insert(QStringLiteral("ALC_MaximumGain"), QString::number(tx->txAlcMaxGain()));
    out.insert(QStringLiteral("ALC_Decay"),       QString::number(tx->txAlcDecay()));

    // ── 3M-3a-ii G — CFC + CPDR + CESSB + PhRot live capture (41 keys) ─────────
    // Bundle keys + defaults match Batch 4 (commit 2b3219e); see
    // src/core/MicProfileManager.cpp:135-188 for the kKeys list and
    // defaultProfileValues() for baseline values.  TM getter/setter names
    // from Batch 2 (commit 12c1b01); see src/models/TransmitModel.h.
    // Phase Rotator (4)
    out.insert(QStringLiteral("CFCPhaseRotatorEnabled"),
               tx->phaseRotatorEnabled() ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("CFCPhaseReverseEnabled"),
               tx->phaseReverseEnabled() ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("CFCPhaseRotatorFreq"),
               QString::number(tx->phaseRotatorFreqHz()));
    out.insert(QStringLiteral("CFCPhaseRotatorStages"),
               QString::number(tx->phaseRotatorStages()));
    // CFC scalars (4)
    out.insert(QStringLiteral("CFCEnabled"),
               tx->cfcEnabled() ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("CFCPostEqEnabled"),
               tx->cfcPostEqEnabled() ? QStringLiteral("True") : QStringLiteral("False"));
    out.insert(QStringLiteral("CFCPreComp"),    QString::number(tx->cfcPrecompDb()));
    out.insert(QStringLiteral("CFCPostEqGain"), QString::number(tx->cfcPostEqGainDb()));
    // CFC profile arrays (30) — bundle keys are 0-indexed (CFC*0..9) per
    // Thetis column naming; see brief table.
    for (int i = 0; i < 10; ++i) {
        out.insert(QStringLiteral("CFCEqFreq%1").arg(i),
                   QString::number(tx->cfcEqFreq(i)));
        out.insert(QStringLiteral("CFCPreComp%1").arg(i),
                   QString::number(tx->cfcCompression(i)));
        out.insert(QStringLiteral("CFCPostEqGain%1").arg(i),
                   QString::number(tx->cfcPostEqBandGain(i)));
    }
    // CFC blob (1) — opaque QString fits QVariant directly.
    out.insert(QStringLiteral("CFCParaEQData"), tx->cfcParaEqData());
    // CPDR (1) — cpdrOn is global console state, NOT bundled.
    out.insert(QStringLiteral("CompanderLevel"), QString::number(tx->cpdrLevelDb()));
    // CESSB (1)
    out.insert(QStringLiteral("CESSB_On"),
               tx->cessbOn() ? QStringLiteral("True") : QStringLiteral("False"));

    // ── Plan 4 D1 — TX filter bandwidth (2 keys) ─────────────────────────
    out.insert(QStringLiteral("FilterLow"),  QString::number(tx->filterLow()));
    out.insert(QStringLiteral("FilterHigh"), QString::number(tx->filterHigh()));
    return out;
}

void MicProfileManager::applyValuesToModel(const QHash<QString, QVariant>& values,
                                           TransmitModel* tx)
{
    if (tx == nullptr) {
        return;
    }

    auto take = [&](const QString& key, const QString& def) -> QString {
        const auto it = values.constFind(key);
        if (it == values.constEnd()) {
            return def;
        }
        return it.value().toString();
    };

    // Mic / VOX / MON
    tx->setMicGainDb(take(QStringLiteral("MicGain"), QStringLiteral("-6")).toInt());
    tx->setMicBoost(take(QStringLiteral("Mic_Input_Boost"), QStringLiteral("True")) == QLatin1String("True"));
    tx->setMicXlr(take(QStringLiteral("Mic_XLR"), QStringLiteral("True")) == QLatin1String("True"));
    tx->setLineIn(take(QStringLiteral("Line_Input_On"), QStringLiteral("False")) == QLatin1String("True"));
    tx->setLineInBoost(take(QStringLiteral("Line_Input_Level"), QStringLiteral("0")).toDouble());
    tx->setMicTipRing(take(QStringLiteral("Mic_TipRing"), QStringLiteral("True")) == QLatin1String("True"));
    tx->setMicBias(take(QStringLiteral("Mic_Bias"), QStringLiteral("False")) == QLatin1String("True"));
    tx->setMicPttDisabled(take(QStringLiteral("Mic_PTT_Disabled"), QStringLiteral("False")) == QLatin1String("True"));
    // line_in_gain + user_dig_out (Task 2.4)
    tx->setLineInGain(take(QStringLiteral("Mic_LineInGain"), QStringLiteral("0")).toInt());
    tx->setUserDigOut(take(QStringLiteral("Mic_UserDigOut"), QStringLiteral("0")).toInt());
    tx->setVoxThresholdDb(take(QStringLiteral("Dexp_Threshold"), QStringLiteral("-40")).toInt());
    tx->setVoxGainScalar(take(QStringLiteral("VOX_GainScalar"), QStringLiteral("1")).toFloat());
    tx->setVoxHangTimeMs(take(QStringLiteral("VOX_HangTime"), QStringLiteral("500")).toInt());
    tx->setAntiVoxGainDb(take(QStringLiteral("AntiVox_Gain"), QStringLiteral("0")).toInt());
    tx->setAntiVoxSourceVax(take(QStringLiteral("AntiVox_Source_VAX"), QStringLiteral("False")) == QLatin1String("True"));
    tx->setMonitorVolume(take(QStringLiteral("MonitorVolume"), QStringLiteral("0.5")).toFloat());

    const QString micSrc = take(QStringLiteral("Mic_Source"), QStringLiteral("Pc"));
    tx->setMicSource(micSrc == QLatin1String("Radio") ? MicSource::Radio : MicSource::Pc);

    // Two-tone
    tx->setTwoToneFreq1(take(QStringLiteral("TwoToneFreq1"), QStringLiteral("700")).toInt());
    tx->setTwoToneFreq2(take(QStringLiteral("TwoToneFreq2"), QStringLiteral("1900")).toInt());
    tx->setTwoToneLevel(take(QStringLiteral("TwoToneLevel"), QStringLiteral("-6")).toDouble());
    tx->setTwoTonePower(take(QStringLiteral("TwoTonePower"), QStringLiteral("50")).toInt());
    tx->setTwoToneFreq2Delay(take(QStringLiteral("TwoToneFreq2Delay"), QStringLiteral("0")).toInt());
    tx->setTwoToneInvert(take(QStringLiteral("TwoToneInvert"), QStringLiteral("True")) == QLatin1String("True"));
    tx->setTwoTonePulsed(take(QStringLiteral("TwoTonePulsed"), QStringLiteral("False")) == QLatin1String("True"));
    tx->setTwoToneDrivePowerSource(drivePowerSourceFromString(
        take(QStringLiteral("TwoToneDrivePowerOrigin"), QStringLiteral("DriveSlider"))));

    // ── 3M-3a-i G — TX EQ + Leveler + ALC apply-to-model (27 keys) ───────
    // Defaults match the EQ/Lev/ALC defaults in defaultProfileValues() above.
    tx->setTxEqEnabled(take(QStringLiteral("TXEQEnabled"), QStringLiteral("False"))
                            == QLatin1String("True"));
    tx->setTxEqPreamp(take(QStringLiteral("TXEQPreamp"), QStringLiteral("0")).toInt());
    static constexpr int kDefaultG[10] = {-12, -12, -12, -1, 1, 4, 9, 12, -10, -10};
    static constexpr int kDefaultF[10] = {32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    for (int i = 0; i < 10; ++i) {
        tx->setTxEqBand(i, take(QStringLiteral("TXEQ%1").arg(i + 1),
                                 QString::number(kDefaultG[i])).toInt());
        tx->setTxEqFreq(i, take(QStringLiteral("TxEqFreq%1").arg(i + 1),
                                 QString::number(kDefaultF[i])).toInt());
    }
    // TX EQ blob (1) — opaque QString; empty default (no Thetis row).  Phase
    // 3M-3a-ii follow-up Batch 6.
    tx->setTxEqParaEqData(take(QStringLiteral("TXParaEQData"), QString()));
    tx->setTxLevelerOn(take(QStringLiteral("Lev_On"), QStringLiteral("True"))
                            == QLatin1String("True"));
    tx->setTxLevelerMaxGain(take(QStringLiteral("Lev_MaxGain"), QStringLiteral("15")).toInt());
    tx->setTxLevelerDecay(take(QStringLiteral("Lev_Decay"), QStringLiteral("100")).toInt());
    tx->setTxAlcMaxGain(take(QStringLiteral("ALC_MaximumGain"), QStringLiteral("3")).toInt());
    tx->setTxAlcDecay(take(QStringLiteral("ALC_Decay"), QStringLiteral("10")).toInt());

    // ── 3M-3a-ii G — CFC + CPDR + CESSB + PhRot apply-to-model (41 keys) ─────
    // Bundle keys + defaults match Batch 4 (commit 2b3219e); see
    // src/core/MicProfileManager.cpp:135-188 for the kKeys list and
    // defaultProfileValues() for baseline values.  TM getter/setter names
    // from Batch 2 (commit 12c1b01); see src/models/TransmitModel.h.
    // Phase Rotator (4) — defaults from database.cs:4726-4730 [v2.10.3.13].
    tx->setPhaseRotatorEnabled(take(QStringLiteral("CFCPhaseRotatorEnabled"),
                                     QStringLiteral("False")) == QLatin1String("True"));
    tx->setPhaseReverseEnabled(take(QStringLiteral("CFCPhaseReverseEnabled"),
                                     QStringLiteral("False")) == QLatin1String("True"));
    tx->setPhaseRotatorFreqHz(take(QStringLiteral("CFCPhaseRotatorFreq"),
                                    QStringLiteral("338")).toInt());
    tx->setPhaseRotatorStages(take(QStringLiteral("CFCPhaseRotatorStages"),
                                    QStringLiteral("8")).toInt());
    // CFC scalars (4) — defaults from database.cs:4724-4733 [v2.10.3.13].
    tx->setCfcEnabled(take(QStringLiteral("CFCEnabled"),
                            QStringLiteral("False")) == QLatin1String("True"));
    tx->setCfcPostEqEnabled(take(QStringLiteral("CFCPostEqEnabled"),
                                  QStringLiteral("False")) == QLatin1String("True"));
    tx->setCfcPrecompDb(take(QStringLiteral("CFCPreComp"),    QStringLiteral("0")).toInt());
    tx->setCfcPostEqGainDb(take(QStringLiteral("CFCPostEqGain"), QStringLiteral("0")).toInt());
    // CFC profile arrays (30) — defaults from database.cs:4735-4766 [v2.10.3.13].
    //   CFCEqFreq{0..9}    = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000}
    //   CFCPreComp{0..9}   = 5 (per-band G[])
    //   CFCPostEqGain{0..9} = 0 (per-band E[])
    static constexpr int kDefaultCfcFreq[10] = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000};
    static constexpr int kDefaultCfcComp[10] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
    static constexpr int kDefaultCfcPostEq[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 10; ++i) {
        tx->setCfcEqFreq(i, take(QStringLiteral("CFCEqFreq%1").arg(i),
                                  QString::number(kDefaultCfcFreq[i])).toInt());
        tx->setCfcCompression(i, take(QStringLiteral("CFCPreComp%1").arg(i),
                                       QString::number(kDefaultCfcComp[i])).toInt());
        tx->setCfcPostEqBandGain(i, take(QStringLiteral("CFCPostEqGain%1").arg(i),
                                          QString::number(kDefaultCfcPostEq[i])).toInt());
    }
    // CFC blob (1) — opaque QString; empty default per database.cs:4768.
    tx->setCfcParaEqData(take(QStringLiteral("CFCParaEQData"), QString()));
    // CPDR (1) — cpdrOn is global console state, NOT applied here.
    tx->setCpdrLevelDb(take(QStringLiteral("CompanderLevel"), QStringLiteral("2")).toInt());
    // CESSB (1)
    tx->setCessbOn(take(QStringLiteral("CESSB_On"), QStringLiteral("False"))
                        == QLatin1String("True"));

    // ── Plan 4 D1 — TX filter bandwidth (2 keys) ─────────────────────────
    // Defaults 100/2900 (USB voice typical SSB) per Plan 4 spec §Task 2.
    tx->setFilterLow( take(QStringLiteral("FilterLow"),  QStringLiteral("100")).toInt());
    tx->setFilterHigh(take(QStringLiteral("FilterHigh"), QStringLiteral("2900")).toInt());
}

} // namespace NereusSDR
