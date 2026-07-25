// =================================================================
// src/gui/setup/DspOptionsPage.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs
//   Project Files/Source/Console/setup.Designer.cs
//   original licence from Thetis source is included below
//
// Thetis version: v2.10.3.13 (git commit 501e3f5)
//
// Source-first verification (Task 4.1 Step 2):
//
//   Buffer size Items (verified from setup.Designer.cs:37848-37857 [v2.10.3.13]):
//     comboDSPPhoneRXBuf.Items = "64","128","256","512","1024"
//     comboDSPPhoneTXBuf.Items = same
//     comboDSPCWRXBuf.Items    = same (CW RX only — no TX buf combo in Thetis)
//     comboDSPDigRXBuf.Items   = same
//     comboDSPDigTXBuf.Items   = same
//     comboDSPFMRXBuf.Items    = same
//     comboDSPFMTXBuf.Items    = same
//   NereusSDR collapses RX/TX to per-mode; Task 4.2 fans out to both channels.
//
//   Filter size Items (verified from setup.Designer.cs:37596-37605 [v2.10.3.13]):
//     comboDSPPhoneRXFiltSize.Items = "1024","2048","4096","8192","16384"
//     (same for all mode/direction combos)
//
//   Filter type Items (verified from setup.Designer.cs:37347-37352 [v2.10.3.13]):
//     comboDSPPhoneRXFiltType.Items = "Linear Phase","Low Latency"
//     (same for all mode/direction combos; "Linear Phase" is index 0)
//     Note: CW has RX only — no comboDSPCWTXFiltType in Thetis.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-01 — Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//                 Task 4.1: DspOptionsPage skeleton + 18 controls.
//                 Mirrors Thetis DSP Options tab (design Section 4A).
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "DspOptionsPage.h"

#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "gui/StyleConstants.h"
#include "gui/containers/ContainerManager.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/MeterItem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

// ── Thetis-verbatim combo item lists ─────────────────────────────────────────
//
// Buffer size items — from Thetis setup.Designer.cs:37848-37857 [v2.10.3.13]
//   comboDSPPhoneRXBuf.Items = {"64","128","256","512","1024"}
//   toolTip: "Sets DSP internal Buffer Size -- larger yields sharper filters, more latency"
// Note: Thetis tops out at 1024; design doc said "64/128/256/512/1024/2048 (verify)" —
// Thetis source is authoritative; 2048 is NOT present.
const QStringList kBufferSizes = {"64", "128", "256", "512", "1024"};

// Filter size (taps) items — from Thetis setup.Designer.cs:37596-37605 [v2.10.3.13]
//   comboDSPPhoneRXFiltSize.Items = {"1024","2048","4096","8192","16384"}
//   toolTip: "Sets DSP internal Buffer Size -- larger yields sharper filters, more latency"
const QStringList kFilterSizes = {"1024", "2048", "4096", "8192", "16384"};

// Filter type items — from Thetis setup.Designer.cs:37347-37352 [v2.10.3.13]
//   comboDSPPhoneRXFiltType.Items = {"Linear Phase","Low Latency"}
//   toolTip: "Select 'Low Latency' (Minimum Phase) or 'Linear Phase' Filters"
// Note: "Linear Phase" is index 0 in Thetis (not "Low Latency" as in some docs).
const QStringList kFilterTypes = {"Linear Phase", "Low Latency"};

// ── Local helper functions ────────────────────────────────────────────────────

QComboBox* makeCombo(QWidget* parent, const QStringList& items)
{
    auto* combo = new QComboBox(parent);
    combo->addItems(items);
    return combo;
}

QLabel* makeWarningIcon(QWidget* parent)
{
    auto* lbl = new QLabel(parent);
    lbl->setText(QStringLiteral("⚠"));  // U+26A0 WARNING SIGN
    lbl->setStyleSheet(QStringLiteral("color: #FFC800; font-weight: bold;"));
    lbl->setVisible(false);
    return lbl;
}

void loadCombo(QComboBox* combo, const QString& key, const QString& def)
{
    const QString stored = AppSettings::instance().value(key, def).toString();
    const int idx = combo->findText(stored);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
    }
}

// wireComboPersist removed in v0.3.x — every combo now goes through
// DspOptionsPage::wireComboWithLiveApply, which both persists and
// triggers a live-apply rebuild when the combo's mode group matches
// the active slice.

void loadCheck(QCheckBox* check, const QString& key, bool def)
{
    const QString stored =
        AppSettings::instance().value(key, def ? "True" : "False").toString();
    check->setChecked(stored == "True");
}

void wireCheckPersist(QCheckBox* check, const QString& key)
{
    QObject::connect(check, &QCheckBox::toggled,
        check, [key](bool v) {
            AppSettings::instance().setValue(key, v ? "True" : "False");
        });
}

}  // namespace

// ── Construction ──────────────────────────────────────────────────────────────

DspOptionsPage::DspOptionsPage(RadioModel* model, QWidget* parent)
    : SetupPage("Options", model, parent)
{
    buildUI();
}

// ── Per-mode live-apply wiring (Task 4.2) ─────────────────────────────────────
//
// When the combo changes:
//   1. Persist to AppSettings (always — applies on next mode-switch if not live).
//   2. Determine whether the combo's mode group matches the current slice mode.
//      - "Phone" group: USB, LSB, AM, SAM, DSB
//      - "Cw"    group: CWU, CWL
//      - "Dig"   group: DIGU, DIGL, SPEC, DRM
//      - "Fm"    group: FM
//   3. If mode matches, call RadioModel::rebuildDspOptionsForMode(currentMode)
//      which triggers RxChannel::onModeChanged() + TxChannel::onModeChanged()
//      and emits dspChangeMeasured(ms) if a rebuild occurred.
//
// comboMode is the representative DSPMode for the group (e.g. DSPMode::USB for
// the Phone group). The actual comparison uses modeGroupMatches() below.
//
// NereusSDR-original — design Section 4B.

namespace {

// Returns true if actualMode belongs to the same DSP-Options mode group as
// comboMode. Only the group membership matters for the live-apply gate.
bool modeGroupMatches(DSPMode actualMode, DSPMode comboMode)
{
    // Map each to its key-part suffix, then compare.
    auto keyPart = [](DSPMode m) -> int {
        switch (m) {
            case DSPMode::USB:
            case DSPMode::LSB:
            case DSPMode::AM:
            case DSPMode::SAM:
            case DSPMode::DSB:
                return 0;  // Phone
            case DSPMode::CWU:
            case DSPMode::CWL:
                return 1;  // Cw
            case DSPMode::DIGU:
            case DSPMode::DIGL:
            case DSPMode::SPEC:
            case DSPMode::DRM:
                return 2;  // Dig
            case DSPMode::FM:
                return 3;  // Fm
            default:
                return 0;  // Phone
        }
    };
    return keyPart(actualMode) == keyPart(comboMode);
}

}  // namespace (anon, Task 4.2 helpers)

void DspOptionsPage::wireComboWithLiveApply(QComboBox* combo,
                                             DSPMode   comboMode,
                                             const QString& key)
{
    QObject::connect(combo, &QComboBox::currentTextChanged,
        this, [this, key, comboMode](const QString& v) {
            // 1. Always persist — applies on next mode-switch if not live.
            AppSettings::instance().setValue(key, v);

            // 2. Live-apply if radio is connected and current mode matches.
            RadioModel* rm = model();
            if (!rm) {
                return;
            }
            const SliceModel* slice = rm->sliceById(0);
            if (!slice) {
                return;
            }
            const DSPMode currentMode = slice->dspMode();
            if (modeGroupMatches(currentMode, comboMode)) {
                rm->rebuildDspOptionsForMode(currentMode);
            }
        });
}

// ── UI build ──────────────────────────────────────────────────────────────────

void DspOptionsPage::buildUI()
{
    // Layout matches Thetis tpDSPOptions [v2.10.3.13]: one HBox row of three
    // outer groups (Buffer Size, Filter Size, Filter Type), each holding 4
    // stacked inner mode sub-groups (SSB/AM, FM, CW, Digital).  Per-mode
    // sub-groups have RX + TX combos for Phone/FM/Digital and RX-only for
    // CW (Thetis console.cs:38891-38897 [v2.10.3.13] — CW TX is firmware-
    // handled and has no UI control).
    //
    // Thetis pixel-coord reference (setup.Designer.cs, all [v2.10.3.13]):
    //   grpDSPBufferSize  at x=  8, w=120, h=320
    //   grpDSPFilterSize  at x=134, w=120, h=320
    //   grpDSPFilterType  at x=260, w=153, h=320
    //   Inner sub-groups stack at y=16/88/160/232 (SSB/AM, FM, CW, Digital).

    // Apply the canonical dark page stylesheet so this page matches the
    // rest of the Setup dialog (StyleConstants.h:287 — same helper used by
    // TransmitSetupPages, DisplaySetupPages, AppearanceSetupPages, etc.).
    Style::applyDarkPageStyle(this);

    QVBoxLayout* layout = contentLayout();

    // One warning icon per outer category — visibility logic in
    // recomputeWarnings() (Task 4.5, ported from Thetis console.cs:38797-38807).
    m_warnBufferSize = makeWarningIcon(this);
    m_warnFilterSize = makeWarningIcon(this);
    m_warnBufferType = makeWarningIcon(this);

    // Helper: build a per-mode sub-group with RX (and optionally TX) combo.
    // Empty txDef → no TX combo (CW pattern).
    auto buildModeSubgroup = [this](const QString& title,
                                    const QStringList& items,
                                    const QString& keyPrefix,
                                    const QString& modeKey,
                                    DSPMode comboMode,
                                    const QString& rxDef,
                                    const QString& txDef,
                                    const QString& comboTooltip,
                                    QComboBox*& outRx,
                                    QComboBox*& outTx) -> QGroupBox*
    {
        auto* g = new QGroupBox(title);
        auto* form = new QFormLayout(g);
        form->setSpacing(2);
        form->setContentsMargins(8, 4, 8, 6);

        outRx = makeCombo(g, items);
        outRx->setToolTip(comboTooltip);
        loadCombo(outRx, keyPrefix + modeKey + QStringLiteral("Rx"), rxDef);
        wireComboWithLiveApply(outRx, comboMode,
                               keyPrefix + modeKey + QStringLiteral("Rx"));
        form->addRow(tr("RX:"), outRx);

        if (!txDef.isEmpty()) {
            outTx = makeCombo(g, items);
            outTx->setToolTip(comboTooltip);
            loadCombo(outTx, keyPrefix + modeKey + QStringLiteral("Tx"), txDef);
            wireComboWithLiveApply(outTx, comboMode,
                                   keyPrefix + modeKey + QStringLiteral("Tx"));
            form->addRow(tr("TX:"), outTx);
        } else {
            outTx = nullptr;
        }
        return g;
    };

    // Helper: build an outer column = title + 4 mode sub-groups + warning
    // icon at the bottom.  Inner-group titles are Thetis-verbatim
    // ("SSB/AM"/"FM"/"CW"/"Digital" per setup.Designer.cs:grpDSPBuf*.Text
    // [v2.10.3.13]).
    auto buildColumn = [&](const QString& outerTitle,
                           QGroupBox* phoneSub,
                           QGroupBox* fmSub,
                           QGroupBox* cwSub,
                           QGroupBox* digSub,
                           QLabel*    warnIcon) -> QGroupBox*
    {
        auto* col = new QGroupBox(outerTitle);
        auto* v = new QVBoxLayout(col);
        v->setSpacing(4);
        v->setContentsMargins(6, 8, 6, 6);
        v->addWidget(phoneSub);  // SSB/AM (Thetis y=16)
        v->addWidget(fmSub);     // FM     (Thetis y=88)
        v->addWidget(cwSub);     // CW     (Thetis y=160)
        v->addWidget(digSub);    // Digital(Thetis y=232)
        v->addStretch();

        auto* warnRow = new QHBoxLayout();
        warnRow->addStretch();
        warnRow->addWidget(warnIcon);
        v->addLayout(warnRow);
        return col;
    };

    // ── Outer column 1: Buffer Size (IQcomp) ────────────────────────────────
    // From Thetis setup.Designer.cs:grpDSPBufferSize.Text = "Buffer Size (IQcomp)".
    // Defaults from Thetis console.cs:39073-39139 [v2.10.3.13]:
    //   dsp_buf_phone_rx = 64    dsp_buf_phone_tx = 64
    //   dsp_buf_fm_rx    = 256   dsp_buf_fm_tx    = 128
    //   dsp_buf_cw_rx    = 64    (no CW TX)
    //   dsp_buf_dig_rx   = 64    dsp_buf_dig_tx   = 64
    const QString kBufTooltip = tr(
        "Sets the DSP internal buffer size — larger values yield sharper "
        "filters but add latency.");

    QComboBox* unusedTxStub = nullptr;
    auto* bufPhone = buildModeSubgroup(tr("SSB/AM"), kBufferSizes,
        QStringLiteral("DspOptionsBufferSize"), QStringLiteral("Phone"),
        DSPMode::USB, QStringLiteral("64"), QStringLiteral("64"),
        kBufTooltip, m_bufPhoneRx, m_bufPhoneTx);
    auto* bufFm    = buildModeSubgroup(tr("FM"), kBufferSizes,
        QStringLiteral("DspOptionsBufferSize"), QStringLiteral("Fm"),
        DSPMode::FM, QStringLiteral("256"), QStringLiteral("128"),
        kBufTooltip, m_bufFmRx, m_bufFmTx);
    auto* bufCw    = buildModeSubgroup(tr("CW"), kBufferSizes,
        QStringLiteral("DspOptionsBufferSize"), QStringLiteral("Cw"),
        DSPMode::CWU, QStringLiteral("64"), QString(),
        kBufTooltip, m_bufCwRx, unusedTxStub);
    auto* bufDig   = buildModeSubgroup(tr("Digital"), kBufferSizes,
        QStringLiteral("DspOptionsBufferSize"), QStringLiteral("Dig"),
        DSPMode::DIGU, QStringLiteral("64"), QStringLiteral("64"),
        kBufTooltip, m_bufDigRx, m_bufDigTx);
    auto* bufColumn = buildColumn(tr("Buffer Size (IQcomp)"),
                                  bufPhone, bufFm, bufCw, bufDig,
                                  m_warnBufferSize);

    // ── Outer column 2: Filter Size (taps) ──────────────────────────────────
    // From Thetis setup.Designer.cs:grpDSPFilterSize.Text = "Filter Size (taps)".
    // Defaults from Thetis console.cs:39141-39216 [v2.10.3.13] — all 4096
    // for every mode/direction.
    const QString kFiltTooltip = tr(
        "Sets the FIR filter length — larger values yield sharper "
        "filter skirts but add CPU and latency.");

    auto* fszPhone = buildModeSubgroup(tr("SSB/AM"), kFilterSizes,
        QStringLiteral("DspOptionsFilterSize"), QStringLiteral("Phone"),
        DSPMode::USB, QStringLiteral("4096"), QStringLiteral("4096"),
        kFiltTooltip, m_filtSizePhoneRx, m_filtSizePhoneTx);
    auto* fszFm    = buildModeSubgroup(tr("FM"), kFilterSizes,
        QStringLiteral("DspOptionsFilterSize"), QStringLiteral("Fm"),
        DSPMode::FM, QStringLiteral("4096"), QStringLiteral("4096"),
        kFiltTooltip, m_filtSizeFmRx, m_filtSizeFmTx);
    auto* fszCw    = buildModeSubgroup(tr("CW"), kFilterSizes,
        QStringLiteral("DspOptionsFilterSize"), QStringLiteral("Cw"),
        DSPMode::CWU, QStringLiteral("4096"), QString(),
        kFiltTooltip, m_filtSizeCwRx, unusedTxStub);
    auto* fszDig   = buildModeSubgroup(tr("Digital"), kFilterSizes,
        QStringLiteral("DspOptionsFilterSize"), QStringLiteral("Dig"),
        DSPMode::DIGU, QStringLiteral("4096"), QStringLiteral("4096"),
        kFiltTooltip, m_filtSizeDigRx, m_filtSizeDigTx);
    auto* fszColumn = buildColumn(tr("Filter Size (taps)"),
                                  fszPhone, fszFm, fszCw, fszDig,
                                  m_warnFilterSize);

    // ── Outer column 3: Filter Type ─────────────────────────────────────────
    // From Thetis setup.Designer.cs:grpDSPFilterType.Text = "Filter Type".
    // Items: "Linear Phase", "Low Latency"  (index 0 = Linear Phase).
    // Defaults from Thetis console.cs:39218-39284 [v2.10.3.13] — every
    // mode/direction defaults to DSPFilterType.Low_Latency.
    const QString kTypeTooltip = tr(
        "Select 'Low Latency' (minimum phase) or 'Linear Phase' filters. "
        "Linear Phase has symmetric delay; Low Latency uses minimum-phase "
        "design with less group delay.");

    auto* ftPhone = buildModeSubgroup(tr("SSB/AM"), kFilterTypes,
        QStringLiteral("DspOptionsFilterType"), QStringLiteral("Phone"),
        DSPMode::USB, QStringLiteral("Low Latency"),
        QStringLiteral("Low Latency"),
        kTypeTooltip, m_filtTypePhoneRx, m_filtTypePhoneTx);
    auto* ftFm    = buildModeSubgroup(tr("FM"), kFilterTypes,
        QStringLiteral("DspOptionsFilterType"), QStringLiteral("Fm"),
        DSPMode::FM, QStringLiteral("Low Latency"),
        QStringLiteral("Low Latency"),
        kTypeTooltip, m_filtTypeFmRx, m_filtTypeFmTx);
    auto* ftCw    = buildModeSubgroup(tr("CW"), kFilterTypes,
        QStringLiteral("DspOptionsFilterType"), QStringLiteral("Cw"),
        DSPMode::CWU, QStringLiteral("Low Latency"), QString(),
        kTypeTooltip, m_filtTypeCwRx, unusedTxStub);
    auto* ftDig   = buildModeSubgroup(tr("Digital"), kFilterTypes,
        QStringLiteral("DspOptionsFilterType"), QStringLiteral("Dig"),
        DSPMode::DIGU, QStringLiteral("Low Latency"),
        QStringLiteral("Low Latency"),
        kTypeTooltip, m_filtTypeDigRx, m_filtTypeDigTx);
    auto* ftColumn = buildColumn(tr("Filter Type"),
                                 ftPhone, ftFm, ftCw, ftDig,
                                 m_warnBufferType);

    // ── Assemble three outer columns into a single HBox row ─────────────────
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(8);
    topRow->addWidget(bufColumn);
    topRow->addWidget(fszColumn);
    topRow->addWidget(ftColumn);
    layout->addLayout(topRow);

    // =========================================================================
    // Group 4: Filter Impulse Cache
    // Task 4.3 wires these to the WDSP impulse cache API.
    // =========================================================================
    auto* cacheGroup  = new QGroupBox(tr("Filter Impulse Cache"), this);
    auto* cacheLayout = new QVBoxLayout(cacheGroup);

    m_cacheImpulse = new QCheckBox(tr("Enable WDSP impulse caching"), cacheGroup);
    m_cacheImpulse->setToolTip(
        tr("Cache filter impulse responses in memory for faster channel rebuilds. "
           "Trades memory for first-rebuild latency. "
           "Takes effect on the next radio connect or channel rebuild."));

    m_cacheImpulseSaveRestore = new QCheckBox(
        tr("Persist impulse cache to disk between sessions"), cacheGroup);
    m_cacheImpulseSaveRestore->setToolTip(
        tr("Save the impulse cache to disk on shutdown and reload on next launch. "
           "Eliminates the first-rebuild cost after restarting NereusSDR. "
           "Warning: the cache file can become very large. "
           "Takes effect on the next radio connect or channel rebuild."));

    loadCheck(m_cacheImpulse,            "DspOptionsCacheImpulse",            false);
    loadCheck(m_cacheImpulseSaveRestore, "DspOptionsCacheImpulseSaveRestore",  false);
    wireCheckPersist(m_cacheImpulse,            "DspOptionsCacheImpulse");
    wireCheckPersist(m_cacheImpulseSaveRestore, "DspOptionsCacheImpulseSaveRestore");

    cacheLayout->addWidget(m_cacheImpulse);
    cacheLayout->addWidget(m_cacheImpulseSaveRestore);

    layout->addWidget(cacheGroup);

    // =========================================================================
    // Standalone: High-resolution filter characteristics
    // Task 4.4 wires this to FilterDisplayItem rendering.
    // =========================================================================
    m_highResFilterChars = new QCheckBox(
        tr("High-resolution filter characteristics in filter graph"), this);
    m_highResFilterChars->setToolTip(
        tr("When enabled, the filter graph displays the actual computed FIR "
           "magnitude response. When disabled, a simplified box-shape passband "
           "is shown instead."));

    loadCheck(m_highResFilterChars, "DspOptionsHighResFilterCharacteristics", false);

    // Task 4.4: helper that fans out high-res mode + RxChannel binding to all
    // live FilterDisplayItem instances.  Extracted so it can be called both on
    // initial construction (to apply the persisted value) and on toggle.
    //
    // bindRxChannel(rxCh) is called unconditionally: when high-res is ON the
    // channel supplies the FIR curve; when OFF the pointer is held but unused
    // (paintHighResolutionFilterCurve is gated on m_highResolution).  A nullptr
    // channel causes paintHighResolutionFilterCurve to return early gracefully.
    auto applyHighResFanOut = [this](bool v) {
        AppSettings::instance().setValue(
            QStringLiteral("DspOptionsHighResFilterCharacteristics"),
            v ? QStringLiteral("True") : QStringLiteral("False"));

        RadioModel* rm = model();
        ContainerManager* cm = rm ? rm->containerManager() : nullptr;
        if (!cm) {
            return;
        }

        RxChannel* rxCh = (rm && rm->wdspEngine())
            ? rm->wdspEngine()->rxChannel(0)
            : nullptr;

        cm->forEachMeterItem([v, rxCh](MeterItem* item) {
            if (auto* fdi = qobject_cast<FilterDisplayItem*>(item)) {
                fdi->bindRxChannel(rxCh);
                fdi->setHighResolution(v);
            }
        });
    };

    // Wire toggle → persist + fan-out.
    connect(m_highResFilterChars, &QCheckBox::toggled, this, applyHighResFanOut);

    // Initial bind: apply the persisted value immediately so any FilterDisplayItem
    // instances that already exist pick up both the mode and the channel binding
    // before the first paint.
    // NOTE: ContainerManager::forEachMeterItem() is safe to call during buildUI()
    // because SetupDialog is constructed after all containers are initialised.
    {
        const bool persistedHighRes =
            AppSettings::instance().value(
                QStringLiteral("DspOptionsHighResFilterCharacteristics"),
                QStringLiteral("False")).toString() == QLatin1String("True");
        applyHighResFanOut(persistedHighRes);
    }

    layout->addWidget(m_highResFilterChars);

    // =========================================================================
    // Time-to-last-change readout
    // Task 4.6 subscribes to RadioModel::dspChangeMeasured(qint64).
    // Placeholder text shown until the first rebuild occurs.
    // =========================================================================
    m_timeToLastChangeLabel = new QLabel(tr("Time to last change: — (no change yet)"), this);
    m_timeToLastChangeLabel->setStyleSheet(QStringLiteral("color: #888;"));

    // Wire to RadioModel::dspChangeMeasured if model is available.
    // The signal is emitted by RadioModel::rebuildDsp() (Task 1.8) with the
    // elapsed milliseconds of the last WDSP channel rebuild.
    if (model()) {
        connect(model(), &RadioModel::dspChangeMeasured, this,
            [this](qint64 ms) {
                m_timeToLastChangeLabel->setText(
                    tr("Time to last change: %1 ms").arg(ms));
                m_timeToLastChangeLabel->setStyleSheet(
                    QStringLiteral("color: #c8d8e8;"));
            });
    }

    layout->addWidget(m_timeToLastChangeLabel);

    // =========================================================================
    // Task 4.5: Warning icon wiring
    //
    // From Thetis console.cs:38797-38807 [v2.10.3.13] — wire every combo that
    // contributes to any of the three warning conditions so that recomputeWarnings()
    // is called on every change.
    //
    // Buffer-size combos (4 RX + 3 TX-equivalent modes):
    auto wireWarn = [this](QComboBox* combo) {
        QObject::connect(combo, &QComboBox::currentTextChanged,
            this, [this]() { recomputeWarnings(); });
    };
    // Buffer-size combos (4 RX + 3 TX — CW has no TX combo per Thetis):
    wireWarn(m_bufPhoneRx); wireWarn(m_bufPhoneTx);
    wireWarn(m_bufFmRx);    wireWarn(m_bufFmTx);
    wireWarn(m_bufCwRx);
    wireWarn(m_bufDigRx);   wireWarn(m_bufDigTx);
    // Filter-size combos (same shape):
    wireWarn(m_filtSizePhoneRx); wireWarn(m_filtSizePhoneTx);
    wireWarn(m_filtSizeFmRx);    wireWarn(m_filtSizeFmTx);
    wireWarn(m_filtSizeCwRx);
    wireWarn(m_filtSizeDigRx);   wireWarn(m_filtSizeDigTx);
    // Filter-type combos:
    wireWarn(m_filtTypePhoneRx); wireWarn(m_filtTypePhoneTx);
    wireWarn(m_filtTypeFmRx);    wireWarn(m_filtTypeFmTx);
    wireWarn(m_filtTypeCwRx);
    wireWarn(m_filtTypeDigRx);   wireWarn(m_filtTypeDigTx);

    // Apply initial state immediately (persisted values may already be mismatched).
    recomputeWarnings();
}

// ── Warning visibility logic (Task 4.5) ───────────────────────────────────────
//
// Porting from Thetis console.cs:38797-38807 + setup.cs:22391-22400 [v2.10.3.13].
//
// Original C# from UpdateDSP() in console.cs:
//
//   bool bufferSizeDifferentRX = !(dsp_buf_phone_rx == dsp_buf_fm_rx &&
//                                   dsp_buf_fm_rx == dsp_buf_cw_rx &&
//                                   dsp_buf_cw_rx == dsp_buf_dig_rx);
//   bool filterSizeDifferentRX = !(dsp_filt_size_phone_rx == dsp_filt_size_fm_rx &&
//                                   dsp_filt_size_fm_rx == dsp_filt_size_cw_rx &&
//                                   dsp_filt_size_cw_rx == dsp_filt_size_dig_rx);
//   bool filterTypeDifferentRX = !(dsp_filt_type_phone_rx == dsp_filt_type_fm_rx &&
//                                   dsp_filt_type_fm_rx == dsp_filt_type_cw_rx &&
//                                   dsp_filt_type_cw_rx == dsp_filt_type_dig_rx);
//   bool bufferSizeDifferentTX = !(dsp_buf_phone_tx == dsp_buf_fm_tx &&
//                                   dsp_buf_fm_tx == dsp_buf_dig_tx);
//   bool filterSizeDifferentTX = !(dsp_filt_size_phone_tx == dsp_filt_size_fm_tx &&
//                                   dsp_filt_size_fm_tx == dsp_filt_size_dig_tx);
//   bool filterTypeDifferentTX = !(dsp_filt_type_phone_tx == dsp_filt_type_fm_tx &&
//                                   dsp_filt_type_fm_tx == dsp_filt_type_dig_tx);
//
//   pbWarningBufferSize.Visible = bufferSizeDifferentRX || bufferSizeDifferentTX;
//   pbWarningFilterSize.Visible = filterSizeDifferentRX || filterSizeDifferentTX;
//   pbWarningBufferType.Visible = filterTypeDifferentRX || filterTypeDifferentTX;
//
// NereusSDR mapping:
//   RX side: 4 combos (phone, cw, dig, fm) — exact 4-way Thetis RX check.
//   TX side: NereusSDR collapses RX+TX to per-mode combos, so we re-use the
//            same 3 combos phone/dig/fm for the TX-side check (CW has no TX
//            buffer combo in Thetis — Thetis omits CW from the TX set).
//   For filter type: Thetis checks the type for each direction separately
//            (phone_rx, fm_rx, cw_rx, dig_rx vs phone_tx, fm_tx, dig_tx).
//            NereusSDR has separate PhoneRx/PhoneTx/CwRx/DigRx/DigTx/FmRx/FmTx
//            combos — we apply the same all-equal test across those sets.

// static
bool DspOptionsPage::comboValuesDiffer4(QComboBox* a, QComboBox* b,
                                         QComboBox* c, QComboBox* d)
{
    const QString va = a->currentText();
    return !(va == b->currentText() &&
             va == c->currentText() &&
             va == d->currentText());
}

// static
bool DspOptionsPage::comboValuesDiffer3(QComboBox* a, QComboBox* b,
                                         QComboBox* c)
{
    const QString va = a->currentText();
    return !(va == b->currentText() &&
             va == c->currentText());
}

void DspOptionsPage::recomputeWarnings()
{
    // From Thetis console.cs:38797-38803 [v2.10.3.13] — UpdateDSP() validity
    // checks.  RX side: 4-way diff over (phone, fm, cw, dig).  TX side:
    // 3-way diff over (phone, fm, dig) — Thetis has no CW TX buffer.

    const bool bufferSizeDifferentRX = comboValuesDiffer4(
        m_bufPhoneRx, m_bufFmRx, m_bufCwRx, m_bufDigRx);
    const bool bufferSizeDifferentTX = comboValuesDiffer3(
        m_bufPhoneTx, m_bufFmTx, m_bufDigTx);
    m_warnBufferSize->setVisible(bufferSizeDifferentRX || bufferSizeDifferentTX);
    m_warnBufferSize->setToolTip(
        tr("Buffer sizes differ across modes — WDSP will use the mode-specific "
           "value and no implicit conversion happens. Set all modes to the same "
           "buffer size if you want a consistent configuration."));

    const bool filterSizeDifferentRX = comboValuesDiffer4(
        m_filtSizePhoneRx, m_filtSizeFmRx, m_filtSizeCwRx, m_filtSizeDigRx);
    const bool filterSizeDifferentTX = comboValuesDiffer3(
        m_filtSizePhoneTx, m_filtSizeFmTx, m_filtSizeDigTx);
    m_warnFilterSize->setVisible(filterSizeDifferentRX || filterSizeDifferentTX);
    m_warnFilterSize->setToolTip(
        tr("Filter sizes differ across modes — WDSP will use the mode-specific "
           "value. Set all modes to the same filter size for a consistent "
           "configuration."));

    const bool filterTypeDifferentRX = comboValuesDiffer4(
        m_filtTypePhoneRx, m_filtTypeFmRx, m_filtTypeCwRx, m_filtTypeDigRx);
    const bool filterTypeDifferentTX = comboValuesDiffer3(
        m_filtTypePhoneTx, m_filtTypeFmTx, m_filtTypeDigTx);
    m_warnBufferType->setVisible(filterTypeDifferentRX || filterTypeDifferentTX);
    m_warnBufferType->setToolTip(
        tr("Filter types differ across modes — some modes use Linear Phase and "
           "others use Low Latency. WDSP will use the mode-specific type."));
}

}  // namespace NereusSDR
