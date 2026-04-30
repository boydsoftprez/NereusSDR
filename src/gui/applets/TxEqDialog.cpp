// =================================================================
// src/gui/applets/TxEqDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/eqform.cs (legacy 10-band TX EQ —
//   grpTXEQ, lines 1021-1561 [v2.10.3.13]).  Original licence from
//   Thetis source is included below.
//
// Layout faithfully mirrors Thetis grpTXEQ:
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
// Out-of-scope for Batch 3 (deferred to Batch 4 / A.2):
//   - Profile load/save/delete + factory presets dropdown
//   - Profile import/export
//
// Out-of-scope permanently (handled elsewhere):
//   - RX EQ controls — Thetis EQForm hosts both; NereusSDR splits them.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-29 — Phase 3M-3a-i Batch 3 (Task A.1): created by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-29 — Phase 3M-3a-i Batch 4 (Task A.2): TX profile combo
//                 + Save / Save As / Delete buttons added, wired to
//                 RadioModel::micProfileManager().  Mirrors the Thetis
//                 setup.cs:9505-9656 [v2.10.3.13] handler set
//                 (comboTXProfileName_SelectedIndexChanged,
//                  btnTXProfileSave_Click, btnTXProfileDelete_Click).
//                 Save uses overwrite-with-current-name semantics;
//                 Save As prompts for a new name.  Delete falls back
//                 to the next remaining profile via the F.3
//                 last-remaining guard in MicProfileManager.
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

#include "core/MicProfileManager.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

// Sentinel property-name for the band index attached to each
// slider/spinbox so the shared sender-finder slot can resolve which
// band fired.
constexpr const char* kBandIndexProp = "txEqBandIndex";

// Build a vertical column for one band (or the preamp).  Returns the
// outer column widget; populates *outSlider / *outDbSpin / *outFreqSpin
// (any may be null — the preamp column has no freq spin).
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
    v->addWidget(hdr);

    QSlider* s = new QSlider(Qt::Vertical, col);
    s->setRange(TransmitModel::kTxEqBandDbMin, TransmitModel::kTxEqBandDbMax);
    s->setTickPosition(QSlider::TicksBothSides);
    s->setTickInterval(3);          // matches Thetis tbTXEQ0.TickFrequency = 3
    s->setPageStep(3);              // matches LargeChange = 3
    s->setMinimumHeight(140);
    s->setProperty(kBandIndexProp, bandIndex);
    v->addWidget(s, 1, Qt::AlignHCenter);
    *outSlider = s;

    QSpinBox* db = new QSpinBox(col);
    db->setRange(TransmitModel::kTxEqBandDbMin, TransmitModel::kTxEqBandDbMax);
    db->setSuffix(QStringLiteral(" dB"));
    db->setProperty(kBandIndexProp, bandIndex);
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
        v->addWidget(hz);
        *outFreqSpin = hz;
    }

    return col;
}

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

    // ── A.2 Profile-bank row: combo + Save / Save As / Delete ───────
    // Mirrors Thetis setup.cs:9505-9656 [v2.10.3.13]
    // (comboTXProfileName + btnTXProfileSave + btnTXProfileDelete).
    // NereusSDR adds a separate Save As button (clearer UX than
    // Thetis's overload-Save semantics) per the Batch 4 spec.
    QHBoxLayout* profileRow = new QHBoxLayout;
    profileRow->setSpacing(6);
    {
        QLabel* lbl = new QLabel(tr("Profile:"), this);
        profileRow->addWidget(lbl);

        m_profileCombo = new QComboBox(this);
        m_profileCombo->setObjectName(QStringLiteral("TxEqProfileCombo"));
        m_profileCombo->setEditable(false);
        m_profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_profileCombo->setToolTip(tr(
            "Active TX profile.  Switching loads the profile's saved EQ "
            "shape (and silently updates mic gain, VOX threshold, Leveler, "
            "and ALC settings — the same profile bank used by the TX "
            "applet's TX-Profile combo and the Setup → Audio → TX Profile "
            "editor)."));
        profileRow->addWidget(m_profileCombo, 1);

        m_saveBtn = new QPushButton(tr("Save"), this);
        m_saveBtn->setObjectName(QStringLiteral("TxEqProfileSaveBtn"));
        m_saveBtn->setToolTip(tr(
            "Overwrite the active profile with the current TX EQ + mic + "
            "VOX + Leveler + ALC state.  Confirms before overwriting."));
        profileRow->addWidget(m_saveBtn);

        m_saveAsBtn = new QPushButton(tr("Save As..."), this);
        m_saveAsBtn->setObjectName(QStringLiteral("TxEqProfileSaveAsBtn"));
        m_saveAsBtn->setToolTip(tr(
            "Save the current state under a new profile name."));
        profileRow->addWidget(m_saveAsBtn);

        m_deleteBtn = new QPushButton(tr("Delete"), this);
        m_deleteBtn->setObjectName(QStringLiteral("TxEqProfileDeleteBtn"));
        m_deleteBtn->setToolTip(tr(
            "Delete the active profile.  The last remaining profile "
            "cannot be deleted."));
        profileRow->addWidget(m_deleteBtn);
    }
    outer->addLayout(profileRow);

    QFrame* sepProfile = new QFrame(this);
    sepProfile->setFrameShape(QFrame::HLine);
    sepProfile->setFrameShadow(QFrame::Sunken);
    outer->addWidget(sepProfile);

    // ── Top strip: Enable + WDSP filter combos ──────────────────────
    QHBoxLayout* topRow = new QHBoxLayout;
    topRow->setSpacing(10);

    m_enableChk = new QCheckBox(tr("Enable TX EQ"), this);
    m_enableChk->setObjectName(QStringLiteral("TxEqEnableChk"));
    m_enableChk->setToolTip(
        tr("Master enable for the 10-band TX equalizer.  When off the "
           "EQ stage is bypassed in the TX DSP chain."));
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

    // ── Band columns row (preamp + 10 bands + dB scale label) ───────
    QGroupBox* bandGroup = new QGroupBox(tr("TX EQ Bands"), this);
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
        v->addWidget(top);
        v->addStretch(1);
        v->addWidget(mid);
        v->addStretch(1);
        v->addWidget(bot);
        bandRow->addWidget(scaleCol);
    }

    outer->addWidget(bandGroup, 1);

    setMinimumWidth(720);
}

// ─────────────────────────────────────────────────────────────────────
// Signal wiring
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::wireSignals()
{
    // ── UI → model ────────────────────────────────────────────────
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

    // ── A.2 profile-bank wiring ────────────────────────────────────
    connect(m_profileCombo, &QComboBox::currentTextChanged,
            this, &TxEqDialog::onProfileComboChanged);
    connect(m_saveBtn,   &QPushButton::clicked, this, &TxEqDialog::onSaveClicked);
    connect(m_saveAsBtn, &QPushButton::clicked, this, &TxEqDialog::onSaveAsClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this, &TxEqDialog::onDeleteClicked);

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

    // ── A.2 MicProfileManager → combo refresh + active selection ───
    if (auto* mgr = m_radio->micProfileManager()) {
        connect(mgr, &MicProfileManager::profileListChanged,
                this, &TxEqDialog::refreshProfileCombo);
        connect(mgr, &MicProfileManager::activeProfileChanged,
                this, &TxEqDialog::onActiveProfileChanged);
        // Initial population.
        refreshProfileCombo();
    }
}

// ─────────────────────────────────────────────────────────────────────
// User-driven slots
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
}

// ─────────────────────────────────────────────────────────────────────
// A.2 Profile-bank handlers
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::refreshProfileCombo()
{
    if (!m_profileCombo || !m_radio) { return; }
    auto* mgr = m_radio->micProfileManager();
    if (!mgr) { return; }

    // Programmatic update — block currentTextChanged signal so it
    // doesn't fire setActiveProfile during repopulation.
    QSignalBlocker blk(m_profileCombo);
    const QString prevSelection = m_profileCombo->currentText();
    m_profileCombo->clear();
    const QStringList names = mgr->profileNames();
    m_profileCombo->addItems(names);

    // Restore selection: prefer the manager's active profile;  fall back
    // to the previously-shown text if it still exists.
    const QString active = mgr->activeProfileName();
    int idx = m_profileCombo->findText(active);
    if (idx < 0) {
        idx = m_profileCombo->findText(prevSelection);
    }
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    } else if (!names.isEmpty()) {
        m_profileCombo->setCurrentIndex(0);
    }
}

void TxEqDialog::onActiveProfileChanged(const QString& name)
{
    if (!m_profileCombo) { return; }
    QSignalBlocker blk(m_profileCombo);
    const int idx = m_profileCombo->findText(name);
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    }
}

void TxEqDialog::onProfileComboChanged(const QString& name)
{
    if (!m_radio || name.isEmpty()) { return; }
    auto* mgr = m_radio->micProfileManager();
    if (!mgr) { return; }
    // setActiveProfile pushes the saved values into TransmitModel; the
    // model's *Changed signals fan out to syncFromModel which redraws
    // the visible EQ controls.  Mic gain / VOX threshold / Leveler /
    // ALC values update silently in the model.
    mgr->setActiveProfile(name, &m_radio->transmitModel());
}

bool TxEqDialog::persistProfile(const QString& name, bool setActiveAfter)
{
    if (!m_radio || name.isEmpty()) { return false; }
    auto* mgr = m_radio->micProfileManager();
    if (!mgr) { return false; }
    const bool ok = mgr->saveProfile(name, &m_radio->transmitModel());
    if (ok && setActiveAfter) {
        mgr->setActiveProfile(name, &m_radio->transmitModel());
    }
    return ok;
}

void TxEqDialog::onSaveClicked()
{
    if (!m_radio) { return; }
    auto* mgr = m_radio->micProfileManager();
    if (!mgr) { return; }

    const QString active = mgr->activeProfileName();
    if (active.isEmpty()) {
        // No active profile — fall through to Save As.
        onSaveAsClicked();
        return;
    }

    // Confirm overwrite.  Mirrors Thetis setup.cs:9545-9612 [v2.10.3.13]
    // btnTXProfileSave_Click — the save flow always overwrites, but
    // NereusSDR adds the explicit confirmation prompt for safety
    // (Thetis omits it because the field is editable and the user
    // typed the name themselves).
    bool overwrite = false;
    if (m_overwriteHook) {
        overwrite = m_overwriteHook(active);
    } else {
        const auto answer = QMessageBox::question(
            this, tr("Overwrite TX Profile"),
            tr("Overwrite profile \"%1\" with the current settings?")
                .arg(active),
            QMessageBox::Yes | QMessageBox::No);
        overwrite = (answer == QMessageBox::Yes);
    }
    if (!overwrite) { return; }

    persistProfile(active, /*setActiveAfter=*/false);
}

void TxEqDialog::onSaveAsClicked()
{
    if (!m_radio) { return; }
    auto* mgr = m_radio->micProfileManager();
    if (!mgr) { return; }

    const QString seed = mgr->activeProfileName();

    // Prompt for new name.
    QString rawName;
    bool accepted = false;
    if (m_saveAsHook) {
        const auto result = m_saveAsHook(seed);
        accepted = result.first;
        rawName = result.second;
    } else {
        bool ok = false;
        rawName = QInputDialog::getText(
            this, tr("Save TX Profile As"),
            tr("Enter new TX profile name:"),
            QLineEdit::Normal, seed, &ok);
        accepted = ok;
    }
    if (!accepted) { return; }

    // Strip commas (Thetis precedent: setup.cs:9550-9552 [v2.10.3.13]
    // — TCI safety, so the manager's invariant is honoured at the UI
    // layer too).
    QString name = rawName;
    name.replace(QLatin1Char(','), QLatin1Char('_'));
    name = name.trimmed();
    if (name.isEmpty()) { return; }

    // If the name already exists, ask for overwrite confirmation.
    const QStringList existing = mgr->profileNames();
    if (existing.contains(name)) {
        bool overwrite = false;
        if (m_overwriteHook) {
            overwrite = m_overwriteHook(name);
        } else {
            const auto answer = QMessageBox::question(
                this, tr("Overwrite TX Profile"),
                tr("A profile named \"%1\" already exists.  Overwrite?")
                    .arg(name),
                QMessageBox::Yes | QMessageBox::No);
            overwrite = (answer == QMessageBox::Yes);
        }
        if (!overwrite) { return; }
    }

    persistProfile(name, /*setActiveAfter=*/true);
}

void TxEqDialog::onDeleteClicked()
{
    if (!m_radio) { return; }
    auto* mgr = m_radio->micProfileManager();
    if (!mgr) { return; }

    const QString target = mgr->activeProfileName();
    if (target.isEmpty()) { return; }

    bool confirmed = false;
    if (m_deleteHook) {
        confirmed = m_deleteHook(target);
    } else {
        const auto answer = QMessageBox::question(
            this, tr("Delete TX Profile"),
            tr("Delete profile \"%1\"?").arg(target),
            QMessageBox::Yes | QMessageBox::No);
        confirmed = (answer == QMessageBox::Yes);
    }
    if (!confirmed) { return; }

    const bool ok = mgr->deleteProfile(target);
    if (!ok) {
        // F.3 last-remaining guard fired.  Surface the verbatim Thetis
        // string per setup.cs:9617-9624 [v2.10.3.13].
        const QString msg = QStringLiteral(
            "It is not possible to delete the last remaining TX profile");
        if (m_rejectionHook) {
            m_rejectionHook(msg);
        } else {
            QMessageBox::information(this,
                                      tr("Cannot Delete Profile"), msg);
        }
    }
    // On success the manager auto-emits activeProfileChanged → our
    // onActiveProfileChanged updates the combo selection; profileList-
    // Changed → refreshProfileCombo repopulates entries.
}

// ─────────────────────────────────────────────────────────────────────
// A.2 Test hook setters
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::setSaveAsPromptHook(SaveAsPromptHook hook)
{
    m_saveAsHook = std::move(hook);
}

void TxEqDialog::setOverwriteConfirmHook(OverwriteConfirmHook hook)
{
    m_overwriteHook = std::move(hook);
}

void TxEqDialog::setDeleteConfirmHook(DeleteConfirmHook hook)
{
    m_deleteHook = std::move(hook);
}

void TxEqDialog::setRejectionMessageHook(RejectionMessageHook hook)
{
    m_rejectionHook = std::move(hook);
}

} // namespace NereusSDR
