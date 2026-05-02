// =================================================================
// src/gui/applets/TxEqDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/eqform.cs (legacy 10-band TX EQ —
//   grpTXEQ, lines 1021-1561 [v2.10.3.13]; parametric panel —
//   lines 235-2911 [v2.10.3.13]).  Original licence from Thetis
//   source is included below.
//
// Layout faithfully mirrors Thetis grpTXEQ for the legacy panel:
//   - 1 Enable checkbox  (chkTXEQEnabled — eqform.cs:74)
//   - 1 preamp slider    (tbTXEQPre, range -12..+15 dB, eqform.cs:1549-1561)
//   - 10 band gain sliders (tbTXEQ0..9, range -12..+15 dB, eqform.cs:1496-1546)
//   - 10 band freq spinboxes (udTXEQ0..9, range 0..20000 Hz, eqform.cs:1329)
//
// Thetis pairs the spinbox (numeric) with the slider (visual) per
// band — the spinbox is the FREQUENCY of that band's center, NOT a
// gain mirror.  We keep that semantics verbatim and ALSO add a
// lightweight gain-spinbox mirror under each gain slider so users
// can type exact dB values (NereusSDR-spin, common Qt6 idiom).
//
// Parametric panel (3M-3a-ii follow-up Batch 9):
//   - chkLegacyEQ checkbox at top toggles between legacy / parametric
//     (eqform.cs:969-981 + 2862-2911 [v2.10.3.13])
//   - Single ParametricEqWidget (replaces Thetis ucParametricEq1)
//     with edit row above + right column of controls
//   - Edit row: # / f / Gain / Q / Preamp + Reset (mirrors
//     nudParaEQ_selected_band / nudParaEQ_f / nudParaEQ_gain /
//     nudParaEQ_q / nudParaEQ_preamp / btnParaEQReset at
//     eqform.cs:235-273 + 731-926 [v2.10.3.13])
//   - Right column: Log scale / Use Q Factors / Live Update + warning
//     icon / Low / High freq spinboxes / 5/10/18 band radios
//     (mirrors chkLogScale / chkUseQFactors / chkPanaEQ_live /
//     pbParaEQ_live_warning / udParaEQ_low / udParaEQ_high /
//     radParaEQ_5/10/18 at eqform.cs:241-275 + 402-600 [v2.10.3.13])
//
// Out-of-scope permanently (handled elsewhere):
//   - RX EQ controls — Thetis EQForm hosts both; NereusSDR splits them.
//     The RX EQ widget lives in EqApplet.
//   - Profile management — Thetis hosts the same profile bank on EQForm;
//     NereusSDR exposes it on the TxApplet's TX-Profile combo and the
//     Console / Setup → Audio → TX Profile editor.  Re-introducing a
//     redundant copy on this dialog would only confuse users.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-29 — Phase 3M-3a-i Batch 3 (Task A.1): created by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-29 — Phase 3M-3a-i Batch 4 (Task A.2): TX profile combo
//                 + Save / Save As / Delete buttons added, wired to
//                 RadioModel::micProfileManager().  Mirrored the Thetis
//                 setup.cs:9505-9656 [v2.10.3.13] handler set
//                 (comboTXProfileName_SelectedIndexChanged,
//                  btnTXProfileSave_Click, btnTXProfileDelete_Click).
//   2026-04-30 — Phase 3M-3a-ii follow-up Batch 9: legacy /
//                 parametric panel toggle added by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude
//                 Code.  Profile combo + Save / Save As / Delete
//                 dropped (TxApplet hosts them).  chkLegacyEQ (default
//                 checked) swaps a QStackedWidget between the original
//                 legacy slider panel and a new parametric panel
//                 (single ParametricEqWidget + edit row + right column).
//                 The chkLegacyEQ state persists in AppSettings under
//                 "TxEqDialog/UsingLegacyEQ" to preserve user choice
//                 across launches (mirrors Thetis's
//                 _state.UsingLegacyEQ Common.RestoreForm round-trip).
//                 Legacy band-column sliders / spinboxes / headers now
//                 pick up Style::sliderVStyle() + kSpinBoxStyle +
//                 kTextPrimary — fixes a styling regression where they
//                 rendered with the system default look-and-feel.
//   2026-04-30 — Phase 3M-3a-ii follow-up sub-PR style fix: added a
//                 dialog-level QSS block in the constructor so the
//                 parametric panel's default Qt6 widgets (spinboxes,
//                 combos, radios, checkboxes, group-boxes, labels,
//                 push-buttons) and the top-strip Enable/Nc/Mp/Cutoff/
//                 Window controls pick up the project's dark theme.
//                 The Batch 9 per-widget styles in the legacy band
//                 columns retain their per-widget specificity.  J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// eqform.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
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

#include "TxEqDialog.h"

#include "core/AppSettings.h"
#include "core/ParaEqEnvelope.h"
#include "core/TxChannel.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

namespace {

// Sentinel property-name for the band index attached to each
// slider/spinbox so the shared sender-finder slot can resolve which
// band fired.
constexpr const char* kBandIndexProp = "txEqBandIndex";

// AppSettings key for the legacy-vs-parametric toggle state.
// Mirrors Thetis _state.UsingLegacyEQ (eqform.cs:2866) round-tripped
// through Common.RestoreForm.
constexpr const char* kLegacyToggleSettingsKey = "TxEqDialog/UsingLegacyEQ";

// Parametric panel widget defaults — sourced byte-for-byte from
// eqform.cs:928-967 [v2.10.3.13] (ucParametricEq1 widget property
// block in the InitializeComponent generated code).
constexpr double kParaDefaultDbMin       = -24.0;          // cs:944
constexpr double kParaDefaultDbMax       =  24.0;          // cs:943
constexpr double kParaDefaultMinHz       =   0.0;          // cs:947
constexpr double kParaDefaultMaxHz       = 2700.0;         // cs:946
constexpr double kParaDefaultQMin        =   0.2;          // cs:954
constexpr double kParaDefaultQMax        =  20.0;          // cs:953
constexpr double kParaDefaultGlobalGainDb=   0.0;          // cs:948
constexpr double kParaDefaultMinPointSpacingHz = 5.0;      // cs:950
constexpr int    kParaDefaultBandShadeAlpha = 70;          // cs:938
constexpr int    kParaDefaultAxisTickLength = 6;           // cs:936

// Right-column controls — sourced byte-for-byte from
// eqform.cs:540-600 [v2.10.3.13] (udParaEQ_low / udParaEQ_high) and
// cs:493-526 (radParaEQ_5/10/18 default 10).
constexpr int    kParaLowMinHz           =      0;         // cs:559
constexpr int    kParaLowMaxHz           =  20000;         // cs:551
constexpr int    kParaLowDefaultHz       =      0;         // cs:565
constexpr int    kParaHighMinHz          =      0;         // cs:589
constexpr int    kParaHighMaxHz          =  20000;         // cs:581
constexpr int    kParaHighDefaultHz      =  16000;         // cs:595

// 1 kHz minimum spread between Low and High — mirrors
// frmCFCConfig.cs:122-138 [v2.10.3.13] guard (eqform doesn't expose
// the same constant explicitly but enforces the same invariant via
// nudParaEQ_low / nudParaEQ_high handlers; we copy CFC's threshold).
constexpr int    kMinFreqSpreadHz        = 1000;

// Edit-row preamp (nudParaEQ_preamp) — eqform.cs:671-699 [v2.10.3.13].
constexpr double kParaPreampMinDb        = -24.0;          // cs:685-689
constexpr double kParaPreampMaxDb        =  24.0;          // cs:680-684

// Selected-band spinbox (nudParaEQ_selected_band) —
// eqform.cs:920-925 [v2.10.3.13] default 10 (one-based).
constexpr int    kParaSelectedBandMin    =      1;
constexpr int    kParaSelectedBandDefault = 10;

} // namespace

// ─────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────

TxEqDialog::TxEqDialog(RadioModel* radio, QWidget* parent)
    : QDialog(parent)
    , m_radio(radio)
{
    setWindowTitle(tr("TX Equalizer"));
    setObjectName(QStringLiteral("TxEqDialog"));
    // Modeless: don't block other interaction.
    setModal(false);
    // Singleton lifecycle — caller owns the pointer via instance().
    // Default WA_DeleteOnClose is false, but make it explicit so the
    // singleton survives close/hide cycles.
    setAttribute(Qt::WA_DeleteOnClose, false);

    // Dialog-level QSS — without this, default Qt6 widgets (spinboxes,
    // combos, radios, checkboxes, group-boxes, labels) render with the
    // system default theme inside an otherwise-dark dialog, producing the
    // dark-on-dark "invisible boxes" reported during the 3M-3a-ii
    // follow-up bench test.  The block layers on the project's
    // StyleConstants helpers via QSS selectors; per-widget setStyleSheet
    // calls in buildBandColumn() / buildLegacyPanel() (Batch 9 fix)
    // keep their own specificity and continue to override.
    setStyleSheet(QString::fromLatin1(NereusSDR::Style::kPageStyle)
                  + QString::fromLatin1(NereusSDR::Style::kGroupBoxStyle)
                  + QString::fromLatin1(NereusSDR::Style::kSpinBoxStyle)
                  + NereusSDR::Style::doubleSpinBoxStyle()
                  + QString::fromLatin1(NereusSDR::Style::kComboStyle)
                  + QString::fromLatin1(NereusSDR::Style::kCheckBoxStyle)
                  + QString::fromLatin1(NereusSDR::Style::kRadioButtonStyle)
                  + QString::fromLatin1(NereusSDR::Style::kButtonStyle));

    buildUi();
    wireSignals();
    syncFromModel();
}

TxEqDialog::~TxEqDialog() = default;

TxEqDialog* TxEqDialog::instance(RadioModel* radio, QWidget* parent)
{
    static QPointer<TxEqDialog> s_instance;
    if (s_instance.isNull()) {
        s_instance = new TxEqDialog(radio, parent);
    }
    return s_instance.data();
}

// ─────────────────────────────────────────────────────────────────────
// UI build-out
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::buildUi()
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    // ── chkLegacyEQ at top — eqform.cs:969-981 [v2.10.3.13].
    // Default checked (cs:972-973 chkLegacyEQ.Checked = true).  We
    // override the default with the persisted user choice if present.
    m_legacyToggle = new QCheckBox(tr("Legacy EQ"), this);
    m_legacyToggle->setObjectName(QStringLiteral("TxEqLegacyToggle"));
    m_legacyToggle->setToolTip(tr(
        "When checked, show the original 10-band slider EQ.  When unchecked, "
        "show the parametric EQ — drag points on the curve to set frequency, "
        "gain, and Q per band."));
    {
        const QString persisted = AppSettings::instance().value(
            QLatin1String(kLegacyToggleSettingsKey),
            QStringLiteral("True")).toString();
        m_legacyToggle->setChecked(persisted == QStringLiteral("True"));
    }
    outer->addWidget(m_legacyToggle);

    // ── Top strip: Enable + WDSP filter combos ──────────────────────
    QHBoxLayout* topRow = new QHBoxLayout;
    topRow->setSpacing(10);

    m_enableChk = new QCheckBox(tr("Enable TX EQ"), this);
    m_enableChk->setObjectName(QStringLiteral("TxEqEnableChk"));
    m_enableChk->setToolTip(
        tr("Master enable for the TX equalizer.  When off the EQ stage "
           "is bypassed in the TX DSP chain."));
    topRow->addWidget(m_enableChk);

    topRow->addSpacing(20);

    {
        QLabel* lbl = new QLabel(tr("Nc:"), this);
        topRow->addWidget(lbl);
        m_ncSpin = new QSpinBox(this);
        m_ncSpin->setObjectName(QStringLiteral("TxEqNcSpin"));
        m_ncSpin->setRange(32, 8192);
        m_ncSpin->setSingleStep(32);
        m_ncSpin->setToolTip(
            tr("Number of filter coefficients used by the WDSP EQ stage.  "
               "Higher values give sharper filter shapes but use more CPU.  "
               "Defaults to 2048 — change only if you know what you're doing."));
        topRow->addWidget(m_ncSpin);
    }

    topRow->addSpacing(10);

    m_mpChk = new QCheckBox(tr("Mp"), this);
    m_mpChk->setObjectName(QStringLiteral("TxEqMpChk"));
    m_mpChk->setToolTip(
        tr("Minimum-phase mode.  Off = linear-phase (no group delay "
           "distortion).  On = minimum-phase (lower latency, slight "
           "phase non-linearity).  Default off."));
    topRow->addWidget(m_mpChk);

    topRow->addSpacing(10);

    {
        QLabel* lbl = new QLabel(tr("Cutoff:"), this);
        topRow->addWidget(lbl);
        m_ctfmodeCombo = new QComboBox(this);
        m_ctfmodeCombo->setObjectName(QStringLiteral("TxEqCtfmodeCombo"));
        // Items match WDSP eq.c create_eqp() ctfmode parameter.
        m_ctfmodeCombo->addItem(tr("0 — Peaking"));
        m_ctfmodeCombo->addItem(tr("1 — Notch"));
        m_ctfmodeCombo->setToolTip(
            tr("Filter cutoff mode for band shapes.  Peaking = "
               "bell-shaped boost/cut.  Notch = sharp band-stop.  "
               "Default Peaking."));
        topRow->addWidget(m_ctfmodeCombo);
    }

    topRow->addSpacing(10);

    {
        QLabel* lbl = new QLabel(tr("Window:"), this);
        topRow->addWidget(lbl);
        m_wintypeCombo = new QComboBox(this);
        m_wintypeCombo->setObjectName(QStringLiteral("TxEqWintypeCombo"));
        // Items match WDSP eq.c create_eqp() wintype parameter.
        m_wintypeCombo->addItem(tr("0 — Blackman-Harris"));
        m_wintypeCombo->addItem(tr("1 — Hann"));
        m_wintypeCombo->setToolTip(
            tr("Window function used when constructing the EQ filter "
               "impulse response.  Blackman-Harris = sharper rejection.  "
               "Hann = smoother rolloff.  Default Blackman-Harris."));
        topRow->addWidget(m_wintypeCombo);
    }

    topRow->addStretch(1);
    outer->addLayout(topRow);

    QFrame* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    outer->addWidget(sep);

    // ── Stacked legacy + parametric panels.  The chkLegacyEQ checkbox
    // toggles which one is visible.
    m_panelStack = new QStackedWidget(this);
    m_panelStack->setObjectName(QStringLiteral("TxEqPanelStack"));
    m_legacyPanel = buildLegacyPanel();
    m_parametricPanel = buildParametricPanel();
    m_panelStack->addWidget(m_legacyPanel);       // index 0
    m_panelStack->addWidget(m_parametricPanel);   // index 1
    m_panelStack->setCurrentIndex(m_legacyToggle->isChecked() ? 0 : 1);
    outer->addWidget(m_panelStack, 1);

    setMinimumWidth(720);
}

// ─────────────────────────────────────────────────────────────────────
// Legacy panel build — From Thetis grpTXEQ at eqform.cs:1021-1561 [v2.10.3.13].
// Preserved verbatim from the Phase 3M-3a-i Batch 3 implementation; the
// only change in Batch 9 is the per-column slider/spinbox style fix
// applied below.
// ─────────────────────────────────────────────────────────────────────

namespace {

// Build a vertical column for one band (or the preamp).  Returns the
// outer column widget; populates *outSlider / *outDbSpin / *outFreqSpin
// (any may be null — the preamp column has no freq spin).
//
// Batch 9 styling fix — applies Style::sliderVStyle() to the slider,
// Style::kSpinBoxStyle to both spinboxes, and a kTextPrimary colour
// override to the header label.  Without this, the band columns
// rendered with the system default theme inside an otherwise-dark
// dialog (the header was hard-to-read system grey, the slider used
// the system handle, and the spinboxes lacked the project bevel).
QWidget* buildBandColumn(const QString& headerLabel,
                         int bandIndex,            // -1 for preamp
                         QSlider** outSlider,
                         QSpinBox** outDbSpin,
                         QSpinBox** outFreqSpin,
                         QWidget* parent)
{
    QWidget* col = new QWidget(parent);
    QVBoxLayout* v = new QVBoxLayout(col);
    v->setContentsMargins(2, 2, 2, 2);
    v->setSpacing(3);

    QLabel* hdr = new QLabel(headerLabel, col);
    hdr->setAlignment(Qt::AlignHCenter);
    QFont f = hdr->font();
    f.setBold(true);
    hdr->setFont(f);
    // Batch 9 — apply project text colour so the band header reads
    // against the dark dialog background.
    hdr->setStyleSheet(QStringLiteral("color: %1;")
                          .arg(NereusSDR::Style::kTextPrimary));
    v->addWidget(hdr);

    QSlider* s = new QSlider(Qt::Vertical, col);
    s->setRange(TransmitModel::kTxEqBandDbMin, TransmitModel::kTxEqBandDbMax);
    s->setTickPosition(QSlider::TicksBothSides);
    s->setTickInterval(3);          // matches Thetis tbTXEQ0.TickFrequency = 3
    s->setPageStep(3);              // matches LargeChange = 3
    s->setMinimumHeight(140);
    s->setProperty(kBandIndexProp, bandIndex);
    // Batch 9 — apply the project vertical slider style.
    s->setStyleSheet(NereusSDR::Style::sliderVStyle());
    v->addWidget(s, 1, Qt::AlignHCenter);
    *outSlider = s;

    QSpinBox* db = new QSpinBox(col);
    db->setRange(TransmitModel::kTxEqBandDbMin, TransmitModel::kTxEqBandDbMax);
    db->setSuffix(QStringLiteral(" dB"));
    db->setProperty(kBandIndexProp, bandIndex);
    // Batch 9 — apply the project spinbox style.
    db->setStyleSheet(NereusSDR::Style::kSpinBoxStyle);
    v->addWidget(db);
    *outDbSpin = db;

    if (outFreqSpin) {
        QSpinBox* hz = new QSpinBox(col);
        hz->setRange(TransmitModel::kTxEqFreqHzMin,
                     TransmitModel::kTxEqFreqHzMax);
        hz->setSuffix(QStringLiteral(" Hz"));
        hz->setProperty(kBandIndexProp, bandIndex);
        // Allow 4-digit + suffix room.
        hz->setMinimumWidth(80);
        // Batch 9 — apply the project spinbox style.
        hz->setStyleSheet(NereusSDR::Style::kSpinBoxStyle);
        v->addWidget(hz);
        *outFreqSpin = hz;
    }

    return col;
}

} // anonymous namespace

QWidget* TxEqDialog::buildLegacyPanel()
{
    QWidget* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("TxEqLegacyPanel"));
    QVBoxLayout* legacy = new QVBoxLayout(panel);
    legacy->setContentsMargins(0, 0, 0, 0);
    legacy->setSpacing(6);

    // ── Band columns row (preamp + 10 bands + dB scale label) ───────
    QGroupBox* bandGroup = new QGroupBox(tr("TX EQ Bands"), panel);
    QHBoxLayout* bandRow = new QHBoxLayout(bandGroup);
    bandRow->setContentsMargins(8, 14, 8, 8);
    bandRow->setSpacing(2);

    // Preamp column (no freq spinbox).
    {
        QSpinBox* dummyFreq = nullptr;  // unused
        Q_UNUSED(dummyFreq);
        QWidget* col = buildBandColumn(
            tr("Pre"), /*bandIndex=*/-1,
            &m_preampSlider, &m_preampSpin, /*outFreqSpin=*/nullptr,
            bandGroup);
        m_preampSlider->setRange(TransmitModel::kTxEqPreampDbMin,
                                 TransmitModel::kTxEqPreampDbMax);
        m_preampSpin->setRange(TransmitModel::kTxEqPreampDbMin,
                               TransmitModel::kTxEqPreampDbMax);
        m_preampSlider->setObjectName(QStringLiteral("TxEqPreampSlider"));
        m_preampSpin->setObjectName(QStringLiteral("TxEqPreampSpin"));
        m_preampSlider->setToolTip(
            tr("Overall TX EQ pre-gain applied before the per-band "
               "boosts/cuts.  Range -12 to +15 dB."));
        m_preampSpin->setToolTip(m_preampSlider->toolTip());
        bandRow->addWidget(col);
    }

    // 10 band columns.
    for (int i = 0; i < 10; ++i) {
        QWidget* col = buildBandColumn(
            tr("B%1").arg(i + 1), i,
            &m_bandSliders[i], &m_bandSpins[i], &m_freqSpins[i],
            bandGroup);
        m_bandSliders[i]->setObjectName(QStringLiteral("TxEqBandSlider%1").arg(i));
        m_bandSpins[i]->setObjectName(QStringLiteral("TxEqBandSpin%1").arg(i));
        m_freqSpins[i]->setObjectName(QStringLiteral("TxEqFreqSpin%1").arg(i));

        const QString gainTip = tr(
            "Band %1 gain.  Range -12 to +15 dB.").arg(i + 1);
        m_bandSliders[i]->setToolTip(gainTip);
        m_bandSpins[i]->setToolTip(gainTip);
        m_freqSpins[i]->setToolTip(
            tr("Band %1 center frequency in Hz.  Range %2 to %3 Hz.")
                .arg(i + 1)
                .arg(TransmitModel::kTxEqFreqHzMin)
                .arg(TransmitModel::kTxEqFreqHzMax));
        bandRow->addWidget(col);
    }

    // dB scale labels at the right edge — matches Thetis lblTXEQ15db /
    // lblTXEQ0dB / lblTXEQminus12db markers (eqform.cs:1358-1397).
    {
        QWidget* scaleCol = new QWidget(bandGroup);
        QVBoxLayout* v = new QVBoxLayout(scaleCol);
        v->setContentsMargins(4, 18, 0, 4);
        v->setSpacing(0);
        QLabel* top = new QLabel(QStringLiteral("+15 dB"), scaleCol);
        QLabel* mid = new QLabel(QStringLiteral("  0 dB"), scaleCol);
        QLabel* bot = new QLabel(QStringLiteral("-12 dB"), scaleCol);
        top->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        mid->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        bot->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
        // Batch 9 — match the band-column header styling so all dB
        // labels read against the dark dialog background.
        const QString scaleStyle = QStringLiteral("color: %1;")
                                       .arg(NereusSDR::Style::kTextPrimary);
        top->setStyleSheet(scaleStyle);
        mid->setStyleSheet(scaleStyle);
        bot->setStyleSheet(scaleStyle);
        v->addWidget(top);
        v->addStretch(1);
        v->addWidget(mid);
        v->addStretch(1);
        v->addWidget(bot);
        bandRow->addWidget(scaleCol);
    }

    legacy->addWidget(bandGroup, 1);
    return panel;
}

// ─────────────────────────────────────────────────────────────────────
// Parametric panel build — From Thetis pnlParaEQ + pnlParaEQ2 +
// ucParametricEq1 at eqform.cs:235-967 [v2.10.3.13].
//
// We map Thetis's two side-by-side panels (left: edit row + widget;
// right: 5/10/18 + Low/High + Use Q + Live + Log) into a single
// parametric panel with the widget centered and the right-column
// controls flanking it on the right.  Edit row sits above the widget
// per Thetis.
// ─────────────────────────────────────────────────────────────────────

QWidget* TxEqDialog::buildParametricPanel()
{
    QWidget* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("TxEqParametricPanel"));
    QVBoxLayout* col = new QVBoxLayout(panel);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(6);

    // ── Edit row — eqform.cs:235-273 + 651-740 [v2.10.3.13].
    // Designer order: # / f / dB (gain) / Q / Preamp / Reset.
    {
        QHBoxLayout* row = new QHBoxLayout;
        row->setSpacing(6);

        // # — nudParaEQ_selected_band (cs:920-925; default 10 one-based).
        row->addWidget(new QLabel(tr("#"), panel));
        m_paraSelectedBandSpin = new QSpinBox(panel);
        m_paraSelectedBandSpin->setObjectName(
            QStringLiteral("TxEqParaSelectedBandSpin"));
        m_paraSelectedBandSpin->setRange(kParaSelectedBandMin,
                                          kParaSelectedBandDefault);
        m_paraSelectedBandSpin->setValue(kParaSelectedBandDefault);
        m_paraSelectedBandSpin->setToolTip(tr(
            "Selected band index (1-based).  Editing the f / Gain / Q "
            "spinboxes updates this band's point on the curve."));
        row->addWidget(m_paraSelectedBandSpin);

        // f — nudParaEQ_f (cs:267, max 20000 Hz).
        row->addSpacing(6);
        row->addWidget(new QLabel(tr("f"), panel));
        m_paraFreqSpin = new QSpinBox(panel);
        m_paraFreqSpin->setObjectName(QStringLiteral("TxEqParaFreqSpin"));
        m_paraFreqSpin->setRange(0, 20000);
        m_paraFreqSpin->setSuffix(QStringLiteral(" Hz"));
        m_paraFreqSpin->setMinimumWidth(80);
        m_paraFreqSpin->setToolTip(tr(
            "Center frequency of the selected band in Hz."));
        row->addWidget(m_paraFreqSpin);

        // Gain — nudParaEQ_gain (cs:271, ±24 dB).
        row->addSpacing(6);
        row->addWidget(new QLabel(tr("Gain"), panel));
        m_paraGainSpin = new QDoubleSpinBox(panel);
        m_paraGainSpin->setObjectName(QStringLiteral("TxEqParaGainSpin"));
        m_paraGainSpin->setRange(kParaDefaultDbMin, kParaDefaultDbMax);
        m_paraGainSpin->setSuffix(QStringLiteral(" dB"));
        m_paraGainSpin->setDecimals(1);
        m_paraGainSpin->setSingleStep(0.5);
        m_paraGainSpin->setToolTip(tr(
            "Gain of the selected band in dB.  Range -24 to +24 dB."));
        row->addWidget(m_paraGainSpin);

        // Q — nudParaEQ_q (cs:269, range 0.2..20).
        row->addSpacing(6);
        row->addWidget(new QLabel(tr("Q"), panel));
        m_paraQSpin = new QDoubleSpinBox(panel);
        m_paraQSpin->setObjectName(QStringLiteral("TxEqParaQSpin"));
        m_paraQSpin->setRange(kParaDefaultQMin, kParaDefaultQMax);
        m_paraQSpin->setDecimals(2);
        m_paraQSpin->setSingleStep(0.1);
        m_paraQSpin->setToolTip(tr(
            "Quality factor (bandwidth) of the selected band.  "
            "Range 0.2 (very wide) to 20 (very narrow)."));
        row->addWidget(m_paraQSpin);

        // Preamp — nudParaEQ_preamp (cs:671-699, ±24 dB).
        row->addSpacing(6);
        row->addWidget(new QLabel(tr("Preamp"), panel));
        m_paraPreampSpin = new QDoubleSpinBox(panel);
        m_paraPreampSpin->setObjectName(QStringLiteral("TxEqParaPreampSpin"));
        m_paraPreampSpin->setRange(kParaPreampMinDb, kParaPreampMaxDb);
        m_paraPreampSpin->setSuffix(QStringLiteral(" dB"));
        m_paraPreampSpin->setDecimals(1);
        m_paraPreampSpin->setSingleStep(0.5);
        m_paraPreampSpin->setToolTip(tr(
            "Global preamp applied across all bands.  Range -24 to +24 dB."));
        row->addWidget(m_paraPreampSpin);

        row->addStretch(1);

        // Reset — btnParaEQReset (cs:731-740).
        m_paraResetBtn = new QPushButton(tr("Reset"), panel);
        m_paraResetBtn->setObjectName(QStringLiteral("TxEqParaResetBtn"));
        m_paraResetBtn->setToolTip(tr(
            "Reset all parametric bands to a flat curve."));
        row->addWidget(m_paraResetBtn);

        col->addLayout(row);
    }

    // ── Widget + right column ──────────────────────────────────────
    QHBoxLayout* mainRow = new QHBoxLayout;
    mainRow->setSpacing(8);

    // ParametricEqWidget centered (replaces ucParametricEq1).
    m_parametricWidget = new ParametricEqWidget(panel);
    m_parametricWidget->setObjectName(QStringLiteral("TxEqParametricWidget"));
    // Apply Thetis defaults — ucParametricEq1 widget property block at
    // eqform.cs:928-967 [v2.10.3.13].
    m_parametricWidget->setDbMin(kParaDefaultDbMin);
    m_parametricWidget->setDbMax(kParaDefaultDbMax);
    m_parametricWidget->setFrequencyMinHz(kParaDefaultMinHz);
    m_parametricWidget->setFrequencyMaxHz(kParaDefaultMaxHz);
    m_parametricWidget->setQMin(kParaDefaultQMin);
    m_parametricWidget->setQMax(kParaDefaultQMax);
    m_parametricWidget->setGlobalGainDb(kParaDefaultGlobalGainDb);
    m_parametricWidget->setMinPointSpacingHz(kParaDefaultMinPointSpacingHz);
    m_parametricWidget->setBandShadeAlpha(kParaDefaultBandShadeAlpha);
    m_parametricWidget->setAxisTickLength(kParaDefaultAxisTickLength);
    m_parametricWidget->setShowAxisScales(true);            // cs:956
    m_parametricWidget->setShowBandShading(true);           // cs:957
    m_parametricWidget->setShowDotReadings(true);           // cs:958
    m_parametricWidget->setShowReadout(false);              // cs:959
    m_parametricWidget->setUsePerBandColours(true);         // cs:962
    m_parametricWidget->setAllowPointReorder(true);         // cs:930
    m_parametricWidget->setParametricEq(true);              // cs:952
    m_parametricWidget->setBandCount(10);                   // default 10-band
    m_parametricWidget->setMinimumSize(400, 300);
    mainRow->addWidget(m_parametricWidget, 1);

    // Right column — eqform.cs:241-275 + 402-600 [v2.10.3.13]:
    //   Log scale + Use Q Factors + Live Update + warning icon
    //   + Low / High freq spinboxes
    //   + 5/10/18 band radios
    QVBoxLayout* rightCol = new QVBoxLayout;
    rightCol->setSpacing(6);

    // chkLogScale — eqform.cs:468-478 (default unchecked).
    m_paraLogScaleChk = new QCheckBox(tr("Log scale"), panel);
    m_paraLogScaleChk->setObjectName(QStringLiteral("TxEqParaLogScaleChk"));
    m_paraLogScaleChk->setToolTip(tr(
        "Render the parametric curve on a log frequency axis."));
    rightCol->addWidget(m_paraLogScaleChk);

    // chkUseQFactors — eqform.cs:528-540 (default checked).
    m_paraUseQFactorsChk = new QCheckBox(tr("Use Q Factors"), panel);
    m_paraUseQFactorsChk->setObjectName(QStringLiteral("TxEqParaUseQFactorsChk"));
    m_paraUseQFactorsChk->setChecked(true);
    m_paraUseQFactorsChk->setToolTip(tr(
        "Use Q factors per band when computing the EQ profile.  When off, "
        "the Q columns are ignored and the curve degenerates to flat-band."));
    rightCol->addWidget(m_paraUseQFactorsChk);

    // chkPanaEQ_live + warning icon — eqform.cs:402-414 + 388-400.
    {
        QHBoxLayout* liveRow = new QHBoxLayout;
        liveRow->setSpacing(4);
        liveRow->setContentsMargins(0, 0, 0, 0);

        m_paraLiveUpdateChk = new QCheckBox(tr("Live Update"), panel);
        m_paraLiveUpdateChk->setObjectName(
            QStringLiteral("TxEqParaLiveUpdateChk"));
        m_paraLiveUpdateChk->setToolTip(tr(
            "Push EQ values to the radio while dragging points.  "
            "When off, the WDSP profile is updated only on mouse-release."));
        liveRow->addWidget(m_paraLiveUpdateChk);

        // Warning icon — eqform.cs:388-400 (pbParaEQ_live_warning.Visible
        // = false until live mode is engaged).  Use the system warning
        // icon as a stand-in for Thetis's bundled bitmap.
        QLabel* warnIcon = new QLabel(panel);
        warnIcon->setObjectName(QStringLiteral("TxEqParaLiveWarning"));
        const QIcon icon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
        warnIcon->setPixmap(icon.pixmap(16, 16));
        warnIcon->setToolTip(tr(
            "Live updates may take a while due to large DSP buffer sizes.  "
            "UI interactions may stutter while the profile is rebuilt."));
        warnIcon->setVisible(false);  // matches Thetis cs:400 default
        liveRow->addWidget(warnIcon);
        liveRow->addStretch(1);
        rightCol->addLayout(liveRow);
    }

    rightCol->addSpacing(6);

    // Low / High freq spinboxes — eqform.cs:540-622 [v2.10.3.13].
    {
        QGridLayout* g = new QGridLayout;
        g->setHorizontalSpacing(6);
        g->setVerticalSpacing(4);

        g->addWidget(new QLabel(tr("Low"),  panel), 0, 0);
        m_paraLowSpin = new QSpinBox(panel);
        m_paraLowSpin->setObjectName(QStringLiteral("TxEqParaLowSpin"));
        m_paraLowSpin->setRange(kParaLowMinHz, kParaLowMaxHz);
        m_paraLowSpin->setValue(kParaLowDefaultHz);
        m_paraLowSpin->setSuffix(QStringLiteral(" Hz"));
        m_paraLowSpin->setToolTip(tr(
            "Lower edge of the visible parametric freq range (Hz).  "
            "Must be at least 1000 Hz below High."));
        g->addWidget(m_paraLowSpin, 0, 1);

        g->addWidget(new QLabel(tr("High"), panel), 1, 0);
        m_paraHighSpin = new QSpinBox(panel);
        m_paraHighSpin->setObjectName(QStringLiteral("TxEqParaHighSpin"));
        m_paraHighSpin->setRange(kParaHighMinHz, kParaHighMaxHz);
        m_paraHighSpin->setValue(kParaHighDefaultHz);
        m_paraHighSpin->setSuffix(QStringLiteral(" Hz"));
        m_paraHighSpin->setToolTip(tr(
            "Upper edge of the visible parametric freq range (Hz).  "
            "Must be at least 1000 Hz above Low."));
        g->addWidget(m_paraHighSpin, 1, 1);

        rightCol->addLayout(g);
    }

    rightCol->addSpacing(6);

    // 5 / 10 / 18 band radios — eqform.cs:493-526 [v2.10.3.13]
    // (panelTS1 wraps the three radios; default 10-band).
    {
        QGroupBox* grp = new QGroupBox(tr("Bands"), panel);
        QVBoxLayout* gv = new QVBoxLayout(grp);
        gv->setContentsMargins(8, 14, 8, 8);
        gv->setSpacing(2);

        m_paraBands5Radio = new QRadioButton(tr("5-band"), grp);
        m_paraBands5Radio->setObjectName(QStringLiteral("TxEqParaBands5Radio"));
        m_paraBands5Radio->setToolTip(tr(
            "Switch parametric layout to 5 bands.  Resets per-band points."));

        m_paraBands10Radio = new QRadioButton(tr("10-band"), grp);
        m_paraBands10Radio->setObjectName(QStringLiteral("TxEqParaBands10Radio"));
        m_paraBands10Radio->setChecked(true);
        m_paraBands10Radio->setToolTip(tr(
            "Switch parametric layout to 10 bands (default).  "
            "Resets per-band points."));

        m_paraBands18Radio = new QRadioButton(tr("18-band"), grp);
        m_paraBands18Radio->setObjectName(QStringLiteral("TxEqParaBands18Radio"));
        m_paraBands18Radio->setToolTip(tr(
            "Switch parametric layout to 18 bands.  Resets per-band points."));

        m_bandCountGroup = new QButtonGroup(grp);
        m_bandCountGroup->addButton(m_paraBands5Radio,  5);
        m_bandCountGroup->addButton(m_paraBands10Radio, 10);
        m_bandCountGroup->addButton(m_paraBands18Radio, 18);

        gv->addWidget(m_paraBands5Radio);
        gv->addWidget(m_paraBands10Radio);
        gv->addWidget(m_paraBands18Radio);
        rightCol->addWidget(grp);
    }

    rightCol->addStretch(1);
    mainRow->addLayout(rightCol, 0);

    col->addLayout(mainRow, 1);
    return panel;
}

// ─────────────────────────────────────────────────────────────────────
// Signal wiring
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::wireSignals()
{
    // ── chkLegacyEQ toggle — From eqform.cs:981 + 2862-2911 [v2.10.3.13].
    connect(m_legacyToggle, &QCheckBox::toggled,
            this, &TxEqDialog::onLegacyToggled);

    // ── Legacy panel: UI → model ────────────────────────────────────
    connect(m_enableChk, &QCheckBox::toggled,
            this, &TxEqDialog::onEnableToggled);

    // Slider/spin pair for preamp — use shared onPreampChanged.
    connect(m_preampSlider, &QSlider::valueChanged,
            this, &TxEqDialog::onPreampChanged);
    connect(m_preampSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxEqDialog::onPreampChanged);

    for (int i = 0; i < 10; ++i) {
        connect(m_bandSliders[i], &QSlider::valueChanged,
                this, &TxEqDialog::onBandValueChanged);
        connect(m_bandSpins[i], qOverload<int>(&QSpinBox::valueChanged),
                this, &TxEqDialog::onBandValueChanged);
        connect(m_freqSpins[i], qOverload<int>(&QSpinBox::valueChanged),
                this, &TxEqDialog::onFreqValueChanged);
    }

    connect(m_ncSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxEqDialog::onNcChanged);
    connect(m_mpChk, &QCheckBox::toggled,
            this, &TxEqDialog::onMpToggled);
    connect(m_ctfmodeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TxEqDialog::onCtfmodeChanged);
    connect(m_wintypeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TxEqDialog::onWintypeChanged);

    // ── Parametric panel wiring ─────────────────────────────────────
    if (m_parametricWidget) {
        connect(m_parametricWidget, &ParametricEqWidget::pointsChanged,
                this, &TxEqDialog::onParametricPointsChanged);
        connect(m_parametricWidget, &ParametricEqWidget::globalGainChanged,
                this, &TxEqDialog::onParametricGlobalGainChanged);
        connect(m_parametricWidget, &ParametricEqWidget::selectedIndexChanged,
                this, &TxEqDialog::onParametricSelectedChanged);
    }
    connect(m_paraResetBtn, &QPushButton::clicked,
            this, &TxEqDialog::onParametricResetClicked);
    connect(m_bandCountGroup, &QButtonGroup::idToggled,
            this, [this](int /*id*/, bool checked) {
        if (checked) onParametricBandCountChanged();
    });
    connect(m_paraLowSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxEqDialog::onParametricLowFreqChanged);
    connect(m_paraHighSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxEqDialog::onParametricHighFreqChanged);
    connect(m_paraLogScaleChk, &QCheckBox::toggled,
            this, &TxEqDialog::onParametricLogScaleToggled);
    connect(m_paraUseQFactorsChk, &QCheckBox::toggled,
            this, &TxEqDialog::onParametricUseQFactorsToggled);
    connect(m_paraLiveUpdateChk, &QCheckBox::toggled,
            this, &TxEqDialog::onParametricLiveUpdateToggled);
    connect(m_paraSelectedBandSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxEqDialog::onParametricSelectedBandChanged);
    connect(m_paraFreqSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxEqDialog::onParametricFreqSpinChanged);
    connect(m_paraGainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxEqDialog::onParametricGainSpinChanged);
    connect(m_paraQSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxEqDialog::onParametricQSpinChanged);
    connect(m_paraPreampSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxEqDialog::onParametricPreampSpinChanged);

    // ── Model → UI ────────────────────────────────────────────────
    if (!m_radio) {
        return;
    }
    TransmitModel& tx = m_radio->transmitModel();

    connect(&tx, &TransmitModel::txEqEnabledChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqPreampChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqBandChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqFreqChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqNcChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqMpChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqCtfmodeChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqWintypeChanged,
            this, &TxEqDialog::syncFromModel);

    // Codex P1 #2 on PR #159: profile activation fires
    // txEqParaEqDataChanged; without this connect, the parametric widget
    // stays at defaults after a profile load and the next user edit
    // overwrites the just-loaded curve.
    connect(&tx, &TransmitModel::txEqParaEqDataChanged,
            this, &TxEqDialog::syncParametricFromModel);
}

// ─────────────────────────────────────────────────────────────────────
// User-driven slots — Legacy panel
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::onEnableToggled(bool on)
{
    if (m_updatingFromModel || !m_radio) { return; }
    m_radio->transmitModel().setTxEqEnabled(on);
}

void TxEqDialog::onPreampChanged(int dB)
{
    if (m_updatingFromModel || !m_radio) { return; }
    m_radio->transmitModel().setTxEqPreamp(dB);
}

void TxEqDialog::onBandValueChanged()
{
    if (m_updatingFromModel || !m_radio) { return; }
    QObject* s = sender();
    if (!s) { return; }
    bool ok = false;
    const int idx = s->property(kBandIndexProp).toInt(&ok);
    if (!ok || idx < 0 || idx >= 10) { return; }
    int value = 0;
    if (auto* slider = qobject_cast<QSlider*>(s)) {
        value = slider->value();
    } else if (auto* spin = qobject_cast<QSpinBox*>(s)) {
        value = spin->value();
    } else {
        return;
    }
    m_radio->transmitModel().setTxEqBand(idx, value);
}

void TxEqDialog::onFreqValueChanged()
{
    if (m_updatingFromModel || !m_radio) { return; }
    QObject* s = sender();
    if (!s) { return; }
    bool ok = false;
    const int idx = s->property(kBandIndexProp).toInt(&ok);
    if (!ok || idx < 0 || idx >= 10) { return; }
    auto* spin = qobject_cast<QSpinBox*>(s);
    if (!spin) { return; }
    m_radio->transmitModel().setTxEqFreq(idx, spin->value());
}

void TxEqDialog::onNcChanged(int nc)
{
    if (m_updatingFromModel || !m_radio) { return; }
    m_radio->transmitModel().setTxEqNc(nc);
}

void TxEqDialog::onMpToggled(bool mp)
{
    if (m_updatingFromModel || !m_radio) { return; }
    m_radio->transmitModel().setTxEqMp(mp);
}

void TxEqDialog::onCtfmodeChanged(int mode)
{
    if (m_updatingFromModel || !m_radio) { return; }
    m_radio->transmitModel().setTxEqCtfmode(mode);
}

void TxEqDialog::onWintypeChanged(int wintype)
{
    if (m_updatingFromModel || !m_radio) { return; }
    m_radio->transmitModel().setTxEqWintype(wintype);
}

// ─────────────────────────────────────────────────────────────────────
// Legacy <-> Parametric panel toggle.
// From Thetis chkLegacyEQ_CheckedChanged at eqform.cs:2862-2911 [v2.10.3.13].
// We don't need to swap WDSP DSPRX paths (Thetis cs:2869-2871) — that
// belongs to the DSP layer, not to this TX-only dialog.  We only flip
// the visible panel and persist the user choice for the next launch.
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::onLegacyToggled(bool legacy)
{
    if (!m_panelStack) { return; }
    m_panelStack->setCurrentIndex(legacy ? 0 : 1);
    AppSettings::instance().setValue(
        QLatin1String(kLegacyToggleSettingsKey),
        legacy ? QStringLiteral("True") : QStringLiteral("False"));

    // Push the active mode's curve to WDSP immediately so toggling
    // alone takes audible effect (without this the user would have
    // to also nudge a control on the newly-active panel before the
    // audio path picked up the curve).  See the
    // pushParametricCurveToWdsp / pushLegacyCurveToWdsp comments for
    // why each helper was needed.
    if (legacy) {
        pushLegacyCurveToWdsp();
    } else {
        pushParametricCurveToWdsp();
    }
}

// ─────────────────────────────────────────────────────────────────────
// Parametric panel slots
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::onParametricPointsChanged(bool isDragging)
{
    if (m_ignoreUpdates) { return; }
    updateEditRowFromSelection();
    // Mirrors eqform.cs:206-216 + Thetis live-update gating — defer the
    // round-trip to TransmitModel until the user releases the mouse,
    // unless Live Update is on.
    if (!isDragging || (m_paraLiveUpdateChk && m_paraLiveUpdateChk->isChecked())) {
        pushParametricToModel();
    }
}

void TxEqDialog::onParametricGlobalGainChanged(bool isDragging)
{
    if (m_ignoreUpdates) { return; }
    if (!m_parametricWidget || !m_paraPreampSpin) { return; }
    {
        QSignalBlocker b(m_paraPreampSpin);
        m_paraPreampSpin->setValue(m_parametricWidget->globalGainDb());
    }
    if (!isDragging || (m_paraLiveUpdateChk && m_paraLiveUpdateChk->isChecked())) {
        pushParametricToModel();
    }
}

void TxEqDialog::onParametricSelectedChanged(bool /*isDragging*/)
{
    if (m_ignoreUpdates) { return; }
    updateEditRowFromSelection();
}

void TxEqDialog::onParametricResetClicked()
{
    if (!m_parametricWidget) { return; }
    // Mirrors btnParaEQReset_Click at eqform.cs:731-740 (calls
    // ucParametricEq1.SetDefaults).  NereusSDR's ParametricEqWidget keeps
    // resetPointsDefault() private (Task 5 review), so we synthesize the
    // flat-default arrays inline and call setPointsData -- same pattern as
    // TxCfcDialog::onResetCompClicked / onResetEqClicked.  setBandCount()
    // can't be reused as the reset hook because it early-returns when the
    // requested count equals the current count.
    const int bands = m_parametricWidget->bandCount();
    QVector<double> f(bands), g(bands, 0.0), q(bands, 4.0);
    const double minHz = m_parametricWidget->frequencyMinHz();
    const double maxHz = m_parametricWidget->frequencyMaxHz();
    const double span = (maxHz > minHz) ? (maxHz - minHz) : 1.0;
    for (int i = 0; i < bands; ++i) {
        const double t = (bands > 1) ? double(i) / double(bands - 1) : 0.0;
        f[i] = minHz + t * span;
    }
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setSelectedIndex(-1);
        m_parametricWidget->setGlobalGainDb(0.0);
        m_parametricWidget->setPointsData(f, g, q);
    }
    updateEditRowFromSelection();
    pushParametricToModel();
}

void TxEqDialog::onParametricBandCountChanged()
{
    if (!m_parametricWidget || !m_bandCountGroup) { return; }
    const int count = m_bandCountGroup->checkedId();
    if (count <= 0) { return; }
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setBandCount(count);
        m_parametricWidget->setSelectedIndex(-1);
    }
    // Keep the # spinbox upper-bound in lockstep with the band count.
    if (m_paraSelectedBandSpin) {
        QSignalBlocker b(m_paraSelectedBandSpin);
        m_paraSelectedBandSpin->setRange(kParaSelectedBandMin, count);
        m_paraSelectedBandSpin->setValue(count);
    }
    updateEditRowFromSelection();
    pushParametricToModel();
}

void TxEqDialog::onParametricLowFreqChanged(int hz)
{
    if (!m_parametricWidget || !m_paraHighSpin) { return; }
    // Enforce the 1 kHz spread guard — clamp Low so it stays at least
    // kMinFreqSpreadHz below High.
    const int hi = m_paraHighSpin->value();
    if (hz + kMinFreqSpreadHz > hi) {
        QSignalBlocker b(m_paraLowSpin);
        m_paraLowSpin->setValue(hi - kMinFreqSpreadHz);
        return;  // valueChanged will re-fire with the clamped value
    }
    QSignalBlocker b(m_parametricWidget);
    m_parametricWidget->setFrequencyMinHz(static_cast<double>(hz));
}

void TxEqDialog::onParametricHighFreqChanged(int hz)
{
    if (!m_parametricWidget || !m_paraLowSpin) { return; }
    const int lo = m_paraLowSpin->value();
    if (hz < lo + kMinFreqSpreadHz) {
        QSignalBlocker b(m_paraHighSpin);
        m_paraHighSpin->setValue(lo + kMinFreqSpreadHz);
        return;
    }
    QSignalBlocker b(m_parametricWidget);
    m_parametricWidget->setFrequencyMaxHz(static_cast<double>(hz));
}

void TxEqDialog::onParametricLogScaleToggled(bool on)
{
    if (!m_parametricWidget) { return; }
    QSignalBlocker b(m_parametricWidget);
    m_parametricWidget->setLogScale(on);
}

void TxEqDialog::onParametricUseQFactorsToggled(bool on)
{
    if (!m_parametricWidget) { return; }
    QSignalBlocker b(m_parametricWidget);
    m_parametricWidget->setParametricEq(on);
    pushParametricToModel();
}

void TxEqDialog::onParametricLiveUpdateToggled(bool /*on*/)
{
    // No immediate side effect — the gating is read each time
    // onParametricPointsChanged fires.  We could surface a warning
    // icon visibility flip here if desired (Thetis only shows it
    // when live mode is on AND the buffer size is large).  Defer.
}

void TxEqDialog::onParametricSelectedBandChanged(int oneBased)
{
    if (m_ignoreUpdates || !m_parametricWidget) { return; }
    m_ignoreUpdates = true;
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setSelectedIndex(oneBased - 1);
    }
    m_ignoreUpdates = false;
    updateEditRowFromSelection();
}

void TxEqDialog::onParametricFreqSpinChanged(int hz)
{
    if (m_ignoreUpdates || !m_parametricWidget) { return; }
    const int idx = m_parametricWidget->selectedIndex();
    if (idx < 0) { return; }
    double f = 0.0, g = 0.0, q = 0.0;
    m_parametricWidget->getPointData(idx, f, g, q);
    f = static_cast<double>(hz);
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setPointData(idx, f, g, q);
    }
    pushParametricToModel();
}

void TxEqDialog::onParametricGainSpinChanged(double db)
{
    if (m_ignoreUpdates || !m_parametricWidget) { return; }
    const int idx = m_parametricWidget->selectedIndex();
    if (idx < 0) { return; }
    double f = 0.0, g = 0.0, q = 0.0;
    m_parametricWidget->getPointData(idx, f, g, q);
    g = db;
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setPointData(idx, f, g, q);
    }
    pushParametricToModel();
}

void TxEqDialog::onParametricQSpinChanged(double q)
{
    if (m_ignoreUpdates || !m_parametricWidget) { return; }
    const int idx = m_parametricWidget->selectedIndex();
    if (idx < 0) { return; }
    double f = 0.0, g = 0.0, qOld = 0.0;
    m_parametricWidget->getPointData(idx, f, g, qOld);
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setPointData(idx, f, g, q);
    }
    pushParametricToModel();
}

void TxEqDialog::onParametricPreampSpinChanged(double db)
{
    if (m_ignoreUpdates || !m_parametricWidget) { return; }
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setGlobalGainDb(db);
    }
    pushParametricToModel();
}

// ─────────────────────────────────────────────────────────────────────
// Edit-row sync helpers
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::updateEditRowFromSelection()
{
    if (!m_parametricWidget) { return; }
    const int idx = m_parametricWidget->selectedIndex();
    const bool haveSelection = (idx >= 0);

    // Edit-row spinboxes are enabled only when a band is selected,
    // matching Thetis frmCFCConfig.cs's selected-row-enable pattern
    // (we have no Thetis equivalent in eqform; Thetis leaves the
    // boxes unconditionally editable).  NereusSDR-spin: gate them
    // so users don't type into spinboxes that have no effect.
    if (m_paraFreqSpin)   m_paraFreqSpin->setEnabled(haveSelection);
    if (m_paraGainSpin)   m_paraGainSpin->setEnabled(haveSelection);
    if (m_paraQSpin)      m_paraQSpin->setEnabled(haveSelection);

    if (!haveSelection) {
        return;
    }

    double f = 0.0, g = 0.0, q = 0.0;
    m_parametricWidget->getPointData(idx, f, g, q);

    m_ignoreUpdates = true;
    if (m_paraSelectedBandSpin) {
        QSignalBlocker b(m_paraSelectedBandSpin);
        m_paraSelectedBandSpin->setValue(idx + 1);
    }
    if (m_paraFreqSpin) {
        QSignalBlocker b(m_paraFreqSpin);
        m_paraFreqSpin->setValue(static_cast<int>(std::round(f)));
    }
    if (m_paraGainSpin) {
        QSignalBlocker b(m_paraGainSpin);
        m_paraGainSpin->setValue(g);
    }
    if (m_paraQSpin) {
        QSignalBlocker b(m_paraQSpin);
        m_paraQSpin->setValue(q);
    }
    if (m_paraPreampSpin) {
        QSignalBlocker b(m_paraPreampSpin);
        m_paraPreampSpin->setValue(m_parametricWidget->globalGainDb());
    }
    m_ignoreUpdates = false;
}

void TxEqDialog::pushParametricToModel()
{
    if (!m_radio || !m_parametricWidget || m_updatingFromModel) { return; }
    m_updatingFromModel = true;
    TransmitModel& tx = m_radio->transmitModel();

    // 1. Persist the parametric blob (consumed by profile save / load
    //    and by syncParametricFromModel on the next active-profile flip).
    // From Thetis eqform.cs:3254-3256 + Common.cs:1745-1762
    // [v2.10.3.13] — SaveToJsonFromPoints(...) then Compress_gzip(json).
    tx.setTxEqParaEqData(
        ParaEqEnvelope::encode(m_parametricWidget->saveToJson()));

    // 2. Push the parametric curve directly to WDSP via
    //    TxChannel::setTxEqProfile (Codex P1 #1 on PR #159 + the
    //    follow-up self-review of dd03b70).  See
    //    pushParametricCurveToWdsp for the F[10]/G[11] build rules.
    //
    //    Why NOT push via the legacy txEqBand/txEqFreq/txEqPreamp
    //    setter chain (the dd03b70 approach):
    //      - rounding to int loses parametric precision (4.6 dB -> 5)
    //      - sampling at the LEGACY ISO grid discards the user's
    //        chosen parametric band centers
    //      - mutating tx.txEqBand[] etc. corrupts the user's legacy
    //        settings on toggle-back
    //      - 11 emissions per drag triggers 11 WDSP profile rebuilds
    //        when only one is needed
    //    Direct push avoids all four.
    pushParametricCurveToWdsp();

    m_updatingFromModel = false;
}

// Build the WDSP (F[10], G[11]) shape from the parametric widget's
// current state and push via TxChannel::setTxEqProfile.  Called on
// every parametric edit (from pushParametricToModel) and on toggle
// INTO parametric mode (from onLegacyToggled).
//
// WDSP TX EQ has a fixed 10-band shape; the parametric widget has
// 5/10/18 bands with arbitrary centers + Q.  This helper resolves
// the count mismatch:
//   - 10-band parametric: 1:1 push of (freq, gain) -- parametric
//     peaks land at the user's exact frequencies.
//   - 5/18-band parametric: sample the curve at 10 equally-spaced
//     freqs across the parametric range -- the curve's Gaussian-
//     weighted-sum interpolation captures the Q effect at whatever
//     resolution 10 sample points allow.
// Q is intrinsically lost at the WDSP layer because WDSP TX EQ is
// graphic-EQ topology; the curve sampling is the best Thetis-faithful
// approximation (Thetis itself does the same -- ucParametricEq feeds
// SetTXAEQProfile with sampled F+G arrays, never with biquad coeffs).
void TxEqDialog::pushParametricCurveToWdsp()
{
    if (!m_radio || !m_parametricWidget) { return; }
    auto* ch = m_radio->txChannel();
    if (!ch) { return; }

    std::vector<double> freqs(10);
    std::vector<double> gains(11);
    gains[0] = m_parametricWidget->globalGainDb();  // preamp slot

    const int n = m_parametricWidget->bandCount();
    if (n == 10) {
        for (int i = 0; i < 10; ++i) {
            double f = 0.0, g = 0.0, q = 0.0;
            m_parametricWidget->getPointData(i, f, g, q);
            freqs[i]   = f;
            gains[i+1] = g;
        }
    } else {
        const double minHz = m_parametricWidget->frequencyMinHz();
        const double maxHz = m_parametricWidget->frequencyMaxHz();
        const double step  = (maxHz > minHz) ? (maxHz - minHz) / 9.0 : 0.0;
        for (int i = 0; i < 10; ++i) {
            const double f = minHz + step * i;
            freqs[i]   = f;
            gains[i+1] = m_parametricWidget->responseDbAtFrequency(f);
        }
    }
    ch->setTxEqProfile(freqs, gains);
}

// Build the WDSP (F[10], G[11]) shape from the legacy
// txEqFreq/txEqBand/txEqPreamp model fields and push via
// TxChannel::setTxEqProfile.  Called on toggle BACK to legacy mode
// so WDSP restores the legacy curve immediately.
//
// Without this helper, the legacy-panel sliders' per-band setters
// only push to WDSP on user edit (via RadioModel.cpp:1924-1939's
// pushEqProfile lambda).  Toggling legacy with no edit would leave
// the previous parametric curve on WDSP until the user nudged a
// slider -- a confusing UX dead zone.
void TxEqDialog::pushLegacyCurveToWdsp()
{
    if (!m_radio) { return; }
    auto* ch = m_radio->txChannel();
    if (!ch) { return; }
    TransmitModel& tx = m_radio->transmitModel();

    std::vector<double> freqs(10);
    std::vector<double> gains(11);
    gains[0] = static_cast<double>(tx.txEqPreamp());
    for (int i = 0; i < 10; ++i) {
        freqs[i]   = static_cast<double>(tx.txEqFreq(i));
        gains[i+1] = static_cast<double>(tx.txEqBand(i));
    }
    ch->setTxEqProfile(freqs, gains);
}

// Codex P1 #2 on PR #159: profile activation fires
// txEqParaEqDataChanged on the model, but we previously only subscribed
// to legacy TX EQ signals -- so the parametric widget would stay at
// defaults after profile load and the next user edit (which goes
// through pushParametricToModel) would overwrite the just-loaded curve.
// This slot loads the JSON blob into the widget under a signal blocker
// so the load itself doesn't trigger a feedback push back to the model.
void TxEqDialog::syncParametricFromModel()
{
    if (!m_radio || !m_parametricWidget || m_updatingFromModel) { return; }
    const QString blob = m_radio->transmitModel().txEqParaEqData();
    if (blob.isEmpty()) { return; }

    // From Thetis eqform.cs:3269-3271 + Common.cs:1764-1790
    // [v2.10.3.13] — Decompress_gzip(value) before loading points.
    QString json;
    const std::optional<QString> decoded = ParaEqEnvelope::decode(blob);
    if (decoded.has_value()) {
        json = *decoded;
    } else if (blob.trimmed().startsWith(QLatin1Char('{'))) {
        // Compatibility for profiles saved by pre-fix PR #159 builds that
        // briefly stored raw JSON before the Thetis envelope was wired here.
        json = blob;
    } else {
        return;
    }

    m_updatingFromModel = true;
    bool loaded = false;
    {
        QSignalBlocker b(m_parametricWidget);
        loaded = m_parametricWidget->loadFromJson(json);
    }
    if (loaded) {
        updateEditRowFromSelection();
    }
    m_updatingFromModel = false;
}

// ─────────────────────────────────────────────────────────────────────
// Model → UI sync (echo-guarded)
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::syncFromModel()
{
    if (!m_radio) { return; }
    TransmitModel& tx = m_radio->transmitModel();

    m_updatingFromModel = true;

    {
        QSignalBlocker b(m_enableChk);
        m_enableChk->setChecked(tx.txEqEnabled());
    }
    {
        QSignalBlocker bs(m_preampSlider);
        QSignalBlocker bn(m_preampSpin);
        m_preampSlider->setValue(tx.txEqPreamp());
        m_preampSpin->setValue(tx.txEqPreamp());
    }
    for (int i = 0; i < 10; ++i) {
        {
            QSignalBlocker bs(m_bandSliders[i]);
            QSignalBlocker bn(m_bandSpins[i]);
            m_bandSliders[i]->setValue(tx.txEqBand(i));
            m_bandSpins[i]->setValue(tx.txEqBand(i));
        }
        {
            QSignalBlocker bf(m_freqSpins[i]);
            m_freqSpins[i]->setValue(tx.txEqFreq(i));
        }
    }
    {
        QSignalBlocker b(m_ncSpin);
        m_ncSpin->setValue(tx.txEqNc());
    }
    {
        QSignalBlocker b(m_mpChk);
        m_mpChk->setChecked(tx.txEqMp());
    }
    {
        QSignalBlocker b(m_ctfmodeCombo);
        m_ctfmodeCombo->setCurrentIndex(tx.txEqCtfmode());
    }
    {
        QSignalBlocker b(m_wintypeCombo);
        m_wintypeCombo->setCurrentIndex(tx.txEqWintype());
    }

    m_updatingFromModel = false;

    // Hydrate the parametric widget from the stored JSON blob (Codex P1 #2
    // on PR #159).  Initial profile-load path -- subsequent updates fire
    // via the txEqParaEqDataChanged signal wired in wireSignals().
    syncParametricFromModel();
}

// ─────────────────────────────────────────────────────────────────────
// Hide-on-close — From Thetis frmCFCConfig.cs:477-482 [v2.10.3.13]
// pattern.  TxApplet keeps the singleton alive for fast re-show.
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::closeEvent(QCloseEvent* event)
{
    event->ignore();
    hide();
}

} // namespace NereusSDR
