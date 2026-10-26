// =================================================================
// src/gui/containers/meter_property_editors/FilterDisplayItemEditor.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Rows for the Thetis filter display settings and colours,
//                 with the ranges the Thetis Apply clamps to
//                 (MeterManager.cs:27651-27705); the older rows are
//                 NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include "FilterDisplayItemEditor.h"
#include "../../meters/FilterDisplayItem.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QSpinBox>
#include <QStringList>

namespace NereusSDR {

namespace {
void applyBtnColor(QPushButton* btn, const QColor& c)
{
    btn->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(c.name(QColor::HexArgb)));
}

constexpr const char* kComboStyle =
    "QComboBox {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 2px 4px; min-height: 18px;"
    "}"
    "QComboBox QAbstractItemView {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #205070;"
    "  selection-background-color: #00b4d8;"
    "}";
} // namespace

FilterDisplayItemEditor::FilterDisplayItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void FilterDisplayItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(item);
    if (!f) { return; }

    beginProgrammaticUpdate();
    m_comboDisplayMode->setCurrentIndex(
        m_comboDisplayMode->findData(static_cast<int>(f->displayMode())));
    m_comboWfPalette->setCurrentIndex(
        m_comboWfPalette->findData(static_cast<int>(f->waterfallPalette())));
    m_chkFill->setChecked(f->fillSpectrum());
    m_spinPadding->setValue(static_cast<double>(f->padding()));
    m_spinMinDb->setValue(static_cast<double>(f->specMinDb()));
    m_spinMaxDb->setValue(static_cast<double>(f->specMaxDb()));
    applyBtnColor(m_btnDataLine, f->dataLineColour());
    applyBtnColor(m_btnDataFill, f->dataFillColour());
    applyBtnColor(m_btnEdgesRx,  f->edgesColourRX());
    applyBtnColor(m_btnEdgesTx,  f->edgesColourTX());
    applyBtnColor(m_btnNotch,    f->notchColour());
    applyBtnColor(m_btnBack,     f->meterBackColour());
    applyBtnColor(m_btnText,     f->textColour());
    for (const auto& refresh : m_refreshers) { refresh(*f); }
    endProgrammaticUpdate();
}

void FilterDisplayItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Filter Display"));

    m_comboDisplayMode = new QComboBox(this);
    m_comboDisplayMode->setStyleSheet(kComboStyle);
    m_comboDisplayMode->addItem(QStringLiteral("Panadapter"),
        static_cast<int>(FilterDisplayItem::DisplayMode::Panadapter));
    m_comboDisplayMode->addItem(QStringLiteral("Waterfall"),
        static_cast<int>(FilterDisplayItem::DisplayMode::Waterfall));
    m_comboDisplayMode->addItem(QStringLiteral("Panafall"),
        static_cast<int>(FilterDisplayItem::DisplayMode::Panafall));
    m_comboDisplayMode->addItem(QStringLiteral("None"),
        static_cast<int>(FilterDisplayItem::DisplayMode::None));
    addRow(QStringLiteral("Display mode"), m_comboDisplayMode);
    connect(m_comboDisplayMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setDisplayMode(static_cast<FilterDisplayItem::DisplayMode>(
            m_comboDisplayMode->currentData().toInt()));
        notifyChanged();
    });

    m_comboWfPalette = new QComboBox(this);
    m_comboWfPalette->setStyleSheet(kComboStyle);
    using WP = FilterDisplayItem::WaterfallPalette;
    m_comboWfPalette->addItem(QStringLiteral("Enhanced"),  static_cast<int>(WP::Enhanced));
    m_comboWfPalette->addItem(QStringLiteral("Spectran"),  static_cast<int>(WP::Spectran));
    m_comboWfPalette->addItem(QStringLiteral("Black/White"), static_cast<int>(WP::BlackWhite));
    m_comboWfPalette->addItem(QStringLiteral("LinLog"),    static_cast<int>(WP::LinLog));
    m_comboWfPalette->addItem(QStringLiteral("LinRad"),    static_cast<int>(WP::LinRad));
    m_comboWfPalette->addItem(QStringLiteral("LinAuto"),   static_cast<int>(WP::LinAuto));
    m_comboWfPalette->addItem(QStringLiteral("Custom"),    static_cast<int>(WP::Custom));
    addRow(QStringLiteral("WF palette"), m_comboWfPalette);
    connect(m_comboWfPalette, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setWaterfallPalette(static_cast<FilterDisplayItem::WaterfallPalette>(
            m_comboWfPalette->currentData().toInt()));
        notifyChanged();
    });

    m_chkFill = makeCheckRow(QStringLiteral("Fill spectrum"));
    connect(m_chkFill, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setFillSpectrum(on);
        notifyChanged();
    });

    // Padding is the Thetis vertical ratio, 0.15-1
    // (MeterManager.cs:27666 [v2.10.3.15]).
    m_spinPadding = makeDoubleRow(QStringLiteral("Vertical ratio"),
                                  FilterDisplayItem::kVerticalRatioMin,
                                  FilterDisplayItem::kVerticalRatioMax, 0.01, 2);
    m_spinPadding->setObjectName(QStringLiteral("filterVerticalRatio"));
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setPadding(static_cast<float>(v));
        notifyChanged();
    });

    addHeader(QStringLiteral("Spectrum range"));
    m_spinMinDb = makeDoubleRow(QStringLiteral("Min dB"), -200.0, 0.0, 1.0, 1);
    connect(m_spinMinDb, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setSpecGridRange(static_cast<float>(v), static_cast<float>(m_spinMaxDb->value()));
        notifyChanged();
    });

    m_spinMaxDb = makeDoubleRow(QStringLiteral("Max dB"), -200.0, 0.0, 1.0, 1);
    connect(m_spinMaxDb, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setSpecGridRange(static_cast<float>(m_spinMinDb->value()), static_cast<float>(v));
        notifyChanged();
    });

    addHeader(QStringLiteral("Colors"));

    auto makeColorBtn = [this](const QString& label, QPushButton*& out) {
        out = new QPushButton(this);
        out->setFixedSize(40, 18);
        addRow(label, out);
    };

    makeColorBtn(QStringLiteral("Data line"),    m_btnDataLine);
    makeColorBtn(QStringLiteral("Data fill"),    m_btnDataFill);
    makeColorBtn(QStringLiteral("RX edges"),     m_btnEdgesRx);
    makeColorBtn(QStringLiteral("TX edges"),     m_btnEdgesTx);
    makeColorBtn(QStringLiteral("Notch"),        m_btnNotch);
    makeColorBtn(QStringLiteral("Background"),   m_btnBack);
    makeColorBtn(QStringLiteral("Text"),         m_btnText);

    // Wire each color button.
    // Note: FilterDisplayItem exposes no color read-back getters so QColorDialog
    // opens with a neutral color; the button swatch reflects the last chosen value.
    connect(m_btnDataLine, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::green, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setDataLineColour(chosen); applyBtnColor(m_btnDataLine, chosen); notifyChanged(); }
    });
    connect(m_btnDataFill, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::green, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setDataFillColour(chosen); applyBtnColor(m_btnDataFill, chosen); notifyChanged(); }
    });
    connect(m_btnEdgesRx, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::yellow, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setEdgesColourRX(chosen); applyBtnColor(m_btnEdgesRx, chosen); notifyChanged(); }
    });
    connect(m_btnEdgesTx, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::red, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setEdgesColourTX(chosen); applyBtnColor(m_btnEdgesTx, chosen); notifyChanged(); }
    });
    connect(m_btnNotch, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::darkYellow, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setNotchColour(chosen); applyBtnColor(m_btnNotch, chosen); notifyChanged(); }
    });
    connect(m_btnBack, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::black, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setMeterBackColour(chosen); applyBtnColor(m_btnBack, chosen); notifyChanged(); }
    });
    connect(m_btnText, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::white, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setTextColour(chosen); applyBtnColor(m_btnText, chosen); notifyChanged(); }
    });

    buildThetisSettings();
}

void FilterDisplayItemEditor::addColourRow(const QString& label, const QString& objectName,
                                           QColor (FilterDisplayItem::*getter)() const,
                                           void (FilterDisplayItem::*setter)(const QColor&))
{
    auto* btn = new QPushButton(this);
    btn->setFixedSize(40, 18);
    btn->setObjectName(objectName);
    addRow(label, btn);
    connect(btn, &QPushButton::clicked, this, [this, btn, getter, setter]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            (f->*getter)(), this, QString(), QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) { return; }
        (f->*setter)(chosen);
        applyBtnColor(btn, chosen);
        notifyChanged();
    });
    m_refreshers.push_back([btn, getter](const FilterDisplayItem& f) {
        applyBtnColor(btn, (f.*getter)());
    });
}

// Rows for the Thetis filter display settings, with the ranges the Thetis
// Apply clamps to (MeterManager.cs:27651-27705 [v2.10.3.15]).
void FilterDisplayItemEditor::buildThetisSettings()
{
    using F = FilterDisplayItem;

    const auto checkRow = [this](const QString& label, const QString& objectName,
                                 bool (F::*getter)() const, void (F::*setter)(bool)) {
        QCheckBox* chk = makeCheckRow(label);
        chk->setObjectName(objectName);
        connect(chk, &QCheckBox::toggled, this, [this, setter](bool on) {
            if (isProgrammaticUpdate()) { return; }
            F* f = qobject_cast<F*>(m_item);
            if (!f) { return; }
            (f->*setter)(on);
            notifyChanged();
        });
        m_refreshers.push_back([chk, getter](const F& f) { chk->setChecked((f.*getter)()); });
    };
    const auto doubleRow = [this](const QString& label, const QString& objectName,
                                  float min, float max, double step, int decimals,
                                  float (F::*getter)() const, void (F::*setter)(float)) {
        QDoubleSpinBox* spin = makeDoubleRow(label, min, max, step, decimals);
        spin->setObjectName(objectName);
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this, setter](double v) {
            if (isProgrammaticUpdate()) { return; }
            F* f = qobject_cast<F*>(m_item);
            if (!f) { return; }
            (f->*setter)(static_cast<float>(v));
            notifyChanged();
        });
        m_refreshers.push_back([spin, getter](const F& f) {
            spin->setValue(static_cast<double>((f.*getter)()));
        });
    };

    addHeader(QStringLiteral("Filter display settings"));
    checkRow(QStringLiteral("Show filter limits"), QStringLiteral("filterShowLimits"),
             &F::showFilterLimits, &F::setShowFilterLimits);
    checkRow(QStringLiteral("Fixed RX zoom"), QStringLiteral("filterFixedRxZoom"),
             &F::fixedRxZoom, &F::setFixedRxZoom);
    doubleRow(QStringLiteral("RX zoom"), QStringLiteral("filterRxZoom"),
              F::kRxZoomMin, F::kRxZoomMax, 0.1, 1, &F::rxZoom, &F::setRxZoom);
    checkRow(QStringLiteral("Fixed TX zoom"), QStringLiteral("filterFixedTxZoom"),
             &F::fixedTxZoom, &F::setFixedTxZoom);
    doubleRow(QStringLiteral("TX zoom"), QStringLiteral("filterTxZoom"),
              F::kTxZoomMin, F::kTxZoomMax, 0.1, 1, &F::txZoom, &F::setTxZoom);
    doubleRow(QStringLiteral("Sidebands scale"), QStringLiteral("filterSidebandsScale"),
              F::kScaleMin, F::kScaleMax, 0.1, 1, &F::sidebandsScale, &F::setSidebandsScale);
    doubleRow(QStringLiteral("CW scale"), QStringLiteral("filterCwScale"),
              F::kScaleMin, F::kScaleMax, 0.1, 1, &F::cwScale, &F::setCwScale);
    doubleRow(QStringLiteral("Others scale"), QStringLiteral("filterOthersScale"),
              F::kScaleMin, F::kScaleMax, 0.1, 1, &F::othersScale, &F::setOthersScale);
    doubleRow(QStringLiteral("Font scale"), QStringLiteral("filterFontScale"),
              F::kFontScaleMin, F::kFontScaleMax, 0.01, 2, &F::fontScale, &F::setFontScale);
    checkRow(QStringLiteral("Sideband mode"), QStringLiteral("filterSidebandMode"),
             &F::sidebandMode, &F::setSidebandMode);

    QSpinBox* frames = makeIntRow(QStringLiteral("Waterfall frame interval"),
                                  F::kFrameIntervalMin, F::kFrameIntervalMax);
    frames->setObjectName(QStringLiteral("filterFrameInterval"));
    connect(frames, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        F* f = qobject_cast<F*>(m_item);
        if (!f) { return; }
        f->setWaterfallFrameInterval(v);
        notifyChanged();
    });
    m_refreshers.push_back([frames](const F& f) { frames->setValue(f.waterfallFrameInterval()); });

    checkRow(QStringLiteral("Grey when not active"), QStringLiteral("filterGreyWhenNotActive"),
             &F::greyWhenNotActive, &F::setGreyWhenNotActive);
    checkRow(QStringLiteral("Snap lines"), QStringLiteral("filterSnapLines"),
             &F::snapLines, &F::setSnapLines);
    checkRow(QStringLiteral("Auto zoom"), QStringLiteral("filterAutoZoom"),
             &F::autoZoom, &F::setAutoZoom);
    checkRow(QStringLiteral("Show characteristic"), QStringLiteral("filterShowCharacteristic"),
             &F::showCharacteristic, &F::setShowCharacteristic);
    doubleRow(QStringLiteral("Characteristic low (dBm)"), QStringLiteral("filterCharacteristicLow"),
              F::kCharacteristicLowMin, F::kCharacteristicLowMax, 1.0, 0,
              &F::characteristicLow, &F::setCharacteristicLow);

    addHeader(QStringLiteral("Highlight colors"));
    addColourRow(QStringLiteral("Number highlight"), QStringLiteral("filterNumberHighlightColour"),
                 &F::numberHighlightColour, &F::setNumberHighlightColour);
    addColourRow(QStringLiteral("Edge highlight"), QStringLiteral("filterEdgeHighlightColour"),
                 &F::edgeHighlightColour, &F::setEdgeHighlightColour);
    addColourRow(QStringLiteral("Extents"), QStringLiteral("filterExtentsColour"),
                 &F::extentsColour, &F::setExtentsColour);
    addColourRow(QStringLiteral("Notch highlight"), QStringLiteral("filterNotchHighlightColour"),
                 &F::notchHighlightColour, &F::setNotchHighlightColour);
    addColourRow(QStringLiteral("Snap lines"), QStringLiteral("filterSnapLineColour"),
                 &F::snapLineColour, &F::setSnapLineColour);
    // The setting-on and button highlight colours paint the MNF, +MNF,
    // SNAP and FIT buttons (MeterManager.cs:35513-35519 [v2.10.3.15]).
    const QString buttons = QStringList{
        QString::fromLatin1(F::kMnfLabel).trimmed(),
        QString::fromLatin1(F::kMnfPlusLabel).trimmed(),
        QString::fromLatin1(F::kSnapLabel).trimmed(),
        QString::fromLatin1(F::kFitLabel).trimmed()}.join(QStringLiteral(", "));
    addColourRow(QStringLiteral("Button on (%1)").arg(buttons), QStringLiteral("filterSettingOnColour"),
                 &F::settingOnColour, &F::setSettingOnColour);
    addColourRow(QStringLiteral("Button highlight (%1)").arg(buttons),
                 QStringLiteral("filterButtonHighlightColour"),
                 &F::buttonHighlightColour, &F::setButtonHighlightColour);
}

} // namespace NereusSDR
