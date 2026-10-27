// =================================================================
// src/gui/containers/meter_property_editors/TextOverlayItemEditor.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.designer.cs (Setup control ranges):
//     upstream source has no top-of-file GPL header, the setup.cs header
//     below covers its partial class
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I3): the Thetis header and
//                 provenance row for the text overlay offset and padding
//                 ranges this editor
//                 takes from setup.designer.cs. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

// --- From setup.cs ---
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

// --- From setup.designer.cs ---
// Upstream source 'Project Files/Source/Console/setup.designer.cs' has no top-of-file GPL header —
// project-level LICENSE applies (it is the designer half of the Setup class in setup.cs).

#include "TextOverlayItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/TextOverlayItem.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QMenu>
#include <QScreen>
#include <QToolButton>

namespace NereusSDR {

namespace {
// Offset rows: range, step and decimals from Thetis setup.designer.cs:62944-62960
// [v2.10.3.15] (nudTextOverlay_RXxOffset: -10..10, 0.001, 3 places).
constexpr double kOffsetMin = -10.0;
constexpr double kOffsetMax = 10.0;
constexpr double kOffsetStep = 0.001;
constexpr int kOffsetDecimals = 3;
// Padding row: maximum, step and decimals from Thetis setup.designer.cs:63008-63022
// [v2.10.3.15] (nudTextOverlay_PanelPadding: up to 10, 0.001, 3 places).
// NereusSDR keeps 0 as the minimum, since older items may hold it.
constexpr double kPaddingMax = 10.0;
constexpr double kPaddingStep = 0.001;
constexpr int kPaddingDecimals = 3;

// The "%" token list. It is never taller than kTokenMenuMaxRows rows or the
// available height of the screen its button is on, and scrolls for the rest.
// It opens at its button: below it when it fits there, otherwise above it.
class TokenMenu final : public QMenu {
public:
    explicit TokenMenu(QWidget* anchor) : QMenu(anchor), m_anchor(anchor)
    {
        // The cap is set as the list opens (aboutToShow comes before QMenu
        // sizes it), from the rows it holds and the button's screen.
        connect(this, &QMenu::aboutToShow, this, [this]() { setMaximumHeight(cappedHeight()); });
    }
    // QMenu shows its scroll arrows only when its size hint runs past the
    // screen below the point it opens at. A list longer than the cap
    // reports that much, so QMenu scrolls it; the maximum height then holds
    // it to the cap and showEvent puts it at the button.
    QSize sizeHint() const override
    {
        QSize hint = QMenu::sizeHint();
        QScreen* screen = m_anchor != nullptr ? m_anchor->screen() : nullptr;
        if (screen != nullptr && hint.height() > cappedHeight()) {
            hint.setHeight(qMax(hint.height(), screen->availableGeometry().height() + 1));
        }
        return hint;
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QMenu::showEvent(event);
        if (m_anchor != nullptr) {
            move(TextOverlayItemEditor::tokenMenuPosition(m_anchor, size()));
        }
    }

private:
    int cappedHeight() const
    {
        const int full = QMenu::sizeHint().height();
        int cap = full;
        const QList<QAction*> list = actions();
        if (list.size() > TextOverlayItemEditor::kTokenMenuMaxRows) {
            const QRect first = actionGeometry(list.first());
            const QRect last = actionGeometry(list.last());
            const int chrome = full - (last.bottom() - first.top() + 1);
            cap = chrome + TextOverlayItemEditor::kTokenMenuMaxRows * first.height();
        }
        if (QScreen* screen = m_anchor != nullptr ? m_anchor->screen() : nullptr) {
            cap = qMin(cap, screen->availableGeometry().height());
        }
        return cap;
    }

    QWidget* m_anchor{nullptr};
};
} // namespace

QPoint TextOverlayItemEditor::tokenMenuPosition(const QWidget* anchor, const QSize& menuSize)
{
    const QPoint origin = anchor->mapToGlobal(QPoint(0, 0));
    QScreen* screen = anchor->screen();
    if (screen == nullptr) {
        return origin + QPoint(0, anchor->height());
    }
    const QRect room = screen->availableGeometry();
    int y = origin.y() + anchor->height();
    if (y + menuSize.height() - 1 > room.bottom()) {
        y = qMax(room.top(), origin.y() - menuSize.height());
    }
    // It opens rightward from the button's left edge; if that would pass the
    // right edge of the button's window or of the screen, whichever is
    // nearer, it is right-aligned to the button and opens leftward. Both
    // edges come from mapToGlobal so they share one origin.
    const QWidget* window = anchor->window();
    const int windowRight = window->mapToGlobal(QPoint(window->width() - 1, 0)).x();
    const int right = qMin(room.right(), windowRight);
    int x = origin.x();
    if (x + menuSize.width() - 1 > right) {
        x = origin.x() + anchor->width() - menuSize.width();
    }
    x = qMax(room.left(), qMin(x, room.right() - menuSize.width() + 1));
    return QPoint(x, y);
}

QCheckBox* TextOverlayItemEditor::makeFlagRow(const QString& label, void (TextOverlayItem::*setter)(bool))
{
    QCheckBox* box = makeCheckRow(label);
    connect(box, &QCheckBox::toggled, this, [this, setter](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        (x->*setter)(on);
        notifyChanged();
    });
    return box;
}

QWidget* TextOverlayItemEditor::makeTokenField(QLineEdit* edit,
                                               void (TextOverlayItem::*setter)(const QString&),
                                               const QString& buttonName)
{
    auto* field = new QWidget(this);
    auto* row = new QHBoxLayout(field);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);
    edit->setParent(field);
    row->addWidget(edit, 1);

    // Thetis's variable picker puts "%" + variable + "%" on the
    // clipboard; here it goes in the field.
    auto* button = new QToolButton(field);
    button->setObjectName(buttonName);
    button->setText(QStringLiteral("%"));
    button->setToolTip(QStringLiteral("Insert a value token"));
    button->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new TokenMenu(button);
    // The token list is longer than a screen: it scrolls in one column and
    // Qt keeps it inside the screen, instead of growing past the screen or
    // spreading into columns across the dialog. TokenMenu caps its height
    // and opens it at the button, leaving the dialog's buttons uncovered.
    menu->setStyleSheet(QStringLiteral("QMenu { menu-scrollable: 1; }"));
    for (const QString& token : offeredOverlayTokens()) {
        menu->addAction(token);
    }
    button->setMenu(menu);
    row->addWidget(button);
    connect(menu, &QMenu::triggered, this, [this, edit, setter](QAction* action) {
        edit->insert(action->text());
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        (x->*setter)(edit->text());
        notifyChanged();
    });
    return field;
}

QDoubleSpinBox* TextOverlayItemEditor::makeOffsetRow(const QString& label, void (TextOverlayItem::*setter)(float))
{
    QDoubleSpinBox* spin = makeDoubleRow(label, kOffsetMin, kOffsetMax, kOffsetStep, kOffsetDecimals);
    connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, setter](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        (x->*setter)(static_cast<float>(v));
        notifyChanged();
    });
    return spin;
}

TextOverlayItemEditor::TextOverlayItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void TextOverlayItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    TextOverlayItem* x = qobject_cast<TextOverlayItem*>(item);
    if (!x) { return; }

    auto applyColor = [](QPushButton* btn, const QString& field, const QColor& c) {
        ItemColourLook::paintSwatch(btn, field, c);
    };

    beginProgrammaticUpdate();

    m_comboLineMode->setCurrentIndex(m_comboLineMode->findData(static_cast<int>(x->lineMode())));

    m_editText1->setText(x->text1());
    applyColor(m_btnColour1, QStringLiteral("TextOverlay.textColour1"), x->textColour1());
    applyColor(m_btnBackColour1, QStringLiteral("TextOverlay.textBackColour1"), x->textBackColour1());
    m_chkShowBack1->setChecked(x->showTextBackColour1());
    m_fontCombo1->setCurrentFont(QFont(x->fontFamily1()));
    m_spinFontSize1->setValue(static_cast<double>(x->fontSize1()));
    m_chkBold1->setChecked(x->fontBold1());
    m_chkItalic1->setChecked(x->fontItalic1());
    m_chkUnderline1->setChecked(x->fontUnderline1());
    m_chkStrikeout1->setChecked(x->fontStrikeout1());
    m_spinXOffset1->setValue(static_cast<double>(x->textXOffset1()));
    m_spinYOffset1->setValue(static_cast<double>(x->textYOffset1()));

    m_editText2->setText(x->text2());
    applyColor(m_btnColour2, QStringLiteral("TextOverlay.textColour2"), x->textColour2());
    applyColor(m_btnBackColour2, QStringLiteral("TextOverlay.textBackColour2"), x->textBackColour2());
    m_chkShowBack2->setChecked(x->showTextBackColour2());
    m_fontCombo2->setCurrentFont(QFont(x->fontFamily2()));
    m_spinFontSize2->setValue(static_cast<double>(x->fontSize2()));
    m_chkBold2->setChecked(x->fontBold2());
    m_chkItalic2->setChecked(x->fontItalic2());
    m_chkUnderline2->setChecked(x->fontUnderline2());
    m_chkStrikeout2->setChecked(x->fontStrikeout2());
    m_spinXOffset2->setValue(static_cast<double>(x->textXOffset2()));
    m_spinYOffset2->setValue(static_cast<double>(x->textYOffset2()));

    m_chkRxLedLogic->setChecked(x->rxLedLogic());
    m_editRxFourChar->setText(x->rxFourChar());
    m_chkTxLedLogic->setChecked(x->txLedLogic());
    m_editTxFourChar->setText(x->txFourChar());

    m_chkShowPanel->setChecked(x->showBackPanel());
    applyColor(m_btnPanelBack1, QStringLiteral("TextOverlay.panelBackColour1"), x->panelBackColour1());
    applyColor(m_btnPanelBack2, QStringLiteral("TextOverlay.panelBackColour2"), x->panelBackColour2());
    m_spinScrollX->setValue(static_cast<double>(x->scrollX()));
    m_spinPadding->setValue(static_cast<double>(x->padding()));

    endProgrammaticUpdate();
}

void TextOverlayItemEditor::buildTypeSpecific()
{
    m_comboLineMode = new QComboBox(this);
    m_comboLineMode->addItem(QStringLiteral("Line 1 on RX, line 2 on TX"),
                             static_cast<int>(TextOverlayItem::LineMode::RxTxSwap));
    m_comboLineMode->addItem(QStringLiteral("Both lines"),
                             static_cast<int>(TextOverlayItem::LineMode::Both));
    m_comboLineMode->setToolTip(QStringLiteral(
        "Line 1 on RX, line 2 on TX: the receive line, or the transmit line while "
        "transmitting, placed by its X and Y offsets. Both lines: line 1 above line 2."));
    addRow(QStringLiteral("Show"), m_comboLineMode);
    connect(m_comboLineMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setLineMode(static_cast<TextOverlayItem::LineMode>(m_comboLineMode->currentData().toInt()));
        notifyChanged();
    });

    // ---- Line 1 ----
    addHeader(QStringLiteral("Line 1"));

    m_editText1 = new QLineEdit(this);
    addRow(QStringLiteral("Text 1"), makeTokenField(m_editText1, &TextOverlayItem::setText1,
                                                QStringLiteral("overlayTokenPicker1")));
    connect(m_editText1, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setText1(m_editText1->text());
        notifyChanged();
    });

    m_btnColour1 = new QPushButton(this);
    m_btnColour1->setFixedSize(40, 18);
    connect(m_btnColour1, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->textColour1(), this);
        if (chosen.isValid()) {
            x->setTextColour1(chosen);
            ItemColourLook::paintSwatch(m_btnColour1, QStringLiteral("TextOverlay.textColour1"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Color 1"), m_btnColour1);

    m_btnBackColour1 = new QPushButton(this);
    m_btnBackColour1->setFixedSize(40, 18);
    connect(m_btnBackColour1, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(Qt::darkGray, this);
        if (chosen.isValid()) {
            x->setTextBackColour1(chosen);
            ItemColourLook::paintSwatch(m_btnBackColour1, QStringLiteral("TextOverlay.textBackColour1"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Back color 1"), m_btnBackColour1);

    m_chkShowBack1 = makeCheckRow(QStringLiteral("Show back 1"));
    connect(m_chkShowBack1, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setShowTextBackColour1(on);
        notifyChanged();
    });

    m_fontCombo1 = new QFontComboBox(this);
    addRow(QStringLiteral("Font 1"), m_fontCombo1);
    connect(m_fontCombo1, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontFamily1(f.family());
        notifyChanged();
    });

    m_spinFontSize1 = makeDoubleRow(QStringLiteral("Font size 1"), 4.0, 200.0, 1.0, 1);
    connect(m_spinFontSize1, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontSize1(static_cast<float>(v));
        notifyChanged();
    });

    m_chkBold1 = makeCheckRow(QStringLiteral("Bold 1"));
    connect(m_chkBold1, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontBold1(on);
        notifyChanged();
    });
    m_chkItalic1 = makeFlagRow(QStringLiteral("Italic 1"), &TextOverlayItem::setFontItalic1);
    m_chkUnderline1 = makeFlagRow(QStringLiteral("Underline 1"), &TextOverlayItem::setFontUnderline1);
    m_chkStrikeout1 = makeFlagRow(QStringLiteral("Strikeout 1"), &TextOverlayItem::setFontStrikeout1);
    m_spinXOffset1 = makeOffsetRow(QStringLiteral("X offset 1"), &TextOverlayItem::setTextXOffset1);
    m_spinYOffset1 = makeOffsetRow(QStringLiteral("Y offset 1"), &TextOverlayItem::setTextYOffset1);

    // ---- Line 2 ----
    addHeader(QStringLiteral("Line 2"));

    m_editText2 = new QLineEdit(this);
    addRow(QStringLiteral("Text 2"), makeTokenField(m_editText2, &TextOverlayItem::setText2,
                                                QStringLiteral("overlayTokenPicker2")));
    connect(m_editText2, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setText2(m_editText2->text());
        notifyChanged();
    });

    m_btnColour2 = new QPushButton(this);
    m_btnColour2->setFixedSize(40, 18);
    connect(m_btnColour2, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->textColour2(), this);
        if (chosen.isValid()) {
            x->setTextColour2(chosen);
            ItemColourLook::paintSwatch(m_btnColour2, QStringLiteral("TextOverlay.textColour2"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Color 2"), m_btnColour2);

    m_btnBackColour2 = new QPushButton(this);
    m_btnBackColour2->setFixedSize(40, 18);
    connect(m_btnBackColour2, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(Qt::darkGray, this);
        if (chosen.isValid()) {
            x->setTextBackColour2(chosen);
            ItemColourLook::paintSwatch(m_btnBackColour2, QStringLiteral("TextOverlay.textBackColour2"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Back color 2"), m_btnBackColour2);

    m_chkShowBack2 = makeCheckRow(QStringLiteral("Show back 2"));
    connect(m_chkShowBack2, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setShowTextBackColour2(on);
        notifyChanged();
    });

    m_fontCombo2 = new QFontComboBox(this);
    addRow(QStringLiteral("Font 2"), m_fontCombo2);
    connect(m_fontCombo2, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontFamily2(f.family());
        notifyChanged();
    });

    m_spinFontSize2 = makeDoubleRow(QStringLiteral("Font size 2"), 4.0, 200.0, 1.0, 1);
    connect(m_spinFontSize2, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontSize2(static_cast<float>(v));
        notifyChanged();
    });

    m_chkBold2 = makeCheckRow(QStringLiteral("Bold 2"));
    connect(m_chkBold2, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontBold2(on);
        notifyChanged();
    });
    m_chkItalic2 = makeFlagRow(QStringLiteral("Italic 2"), &TextOverlayItem::setFontItalic2);
    m_chkUnderline2 = makeFlagRow(QStringLiteral("Underline 2"), &TextOverlayItem::setFontUnderline2);
    m_chkStrikeout2 = makeFlagRow(QStringLiteral("Strikeout 2"), &TextOverlayItem::setFontStrikeout2);
    m_spinXOffset2 = makeOffsetRow(QStringLiteral("X offset 2"), &TextOverlayItem::setTextXOffset2);
    m_spinYOffset2 = makeOffsetRow(QStringLiteral("Y offset 2"), &TextOverlayItem::setTextYOffset2);

    m_btnCopyFont = new QPushButton(QStringLiteral("Copy font 1 to 2"), this);
    m_btnCopyFont->setToolTip(QStringLiteral("Line 2 takes line 1's font, size, style and colours."));
    addRow(QString(), m_btnCopyFont);
    connect(m_btnCopyFont, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->copyFont1To2();
        setItem(x);
        notifyChanged();
    });

    m_btnCopyOffsets = new QPushButton(QStringLiteral("Copy offsets 1 to 2"), this);
    m_btnCopyOffsets->setToolTip(QStringLiteral("Line 2 takes line 1's X and Y offsets."));
    addRow(QString(), m_btnCopyOffsets);
    connect(m_btnCopyOffsets, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->copyOffsets1To2();
        setItem(x);
        notifyChanged();
    });

    // ---- LED gates ----
    // The overlay's RX / TX LED gates (TextOverlayItem::setRxLedLogic and
    // the rest), in plain words.
    addHeader(QStringLiteral("Show by LED"));
    const QString gateTip = QStringLiteral(
        "When on, this line shows only while the LED with this ID is on. Copy the ID "
        "from the LED's settings. An ID no LED has hides nothing. Used when line 1 "
        "shows on RX and line 2 on TX.");
    auto makeGate = [this, &gateTip](const QString& checkLabel, const QString& idLabel,
                                     const QString& name, QCheckBox** check, QLineEdit** edit,
                                     void (TextOverlayItem::*setLogic)(bool),
                                     void (TextOverlayItem::*setId)(const QString&)) {
        *check = makeFlagRow(checkLabel, setLogic);
        (*check)->setObjectName(name + QStringLiteral("Logic"));
        (*check)->setToolTip(gateTip);
        *edit = new QLineEdit(this);
        (*edit)->setObjectName(name + QStringLiteral("Id"));
        (*edit)->setMaxLength(4);
        (*edit)->setToolTip(gateTip);
        addRow(idLabel, *edit);
        QLineEdit* field = *edit;
        connect(field, &QLineEdit::editingFinished, this, [this, field, setId]() {
            if (isProgrammaticUpdate()) { return; }
            TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
            if (!x) { return; }
            (x->*setId)(field->text());
            notifyChanged();
        });
    };
    makeGate(QStringLiteral("Line 1 only while an LED is on"), QStringLiteral("LED ID for line 1"),
             QStringLiteral("overlayRxLed"), &m_chkRxLedLogic, &m_editRxFourChar,
             &TextOverlayItem::setRxLedLogic, &TextOverlayItem::setRxFourChar);
    makeGate(QStringLiteral("Line 2 only while an LED is on"), QStringLiteral("LED ID for line 2"),
             QStringLiteral("overlayTxLed"), &m_chkTxLedLogic, &m_editTxFourChar,
             &TextOverlayItem::setTxLedLogic, &TextOverlayItem::setTxFourChar);

    // ---- Panel ----
    addHeader(QStringLiteral("Panel"));

    m_chkShowPanel = makeCheckRow(QStringLiteral("Show panel"));
    connect(m_chkShowPanel, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setShowBackPanel(on);
        notifyChanged();
    });

    m_btnPanelBack1 = new QPushButton(this);
    m_btnPanelBack1->setFixedSize(40, 18);
    connect(m_btnPanelBack1, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(Qt::darkGray, this);
        if (chosen.isValid()) {
            x->setPanelBackColour1(chosen);
            ItemColourLook::paintSwatch(m_btnPanelBack1, QStringLiteral("TextOverlay.panelBackColour1"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Panel back 1"), m_btnPanelBack1);

    m_btnPanelBack2 = new QPushButton(this);
    m_btnPanelBack2->setFixedSize(40, 18);
    connect(m_btnPanelBack2, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(Qt::darkGray, this);
        if (chosen.isValid()) {
            x->setPanelBackColour2(chosen);
            ItemColourLook::paintSwatch(m_btnPanelBack2, QStringLiteral("TextOverlay.panelBackColour2"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Panel back 2"), m_btnPanelBack2);

    // ---- Layout ----
    addHeader(QStringLiteral("Layout"));

    m_spinScrollX = makeDoubleRow(QStringLiteral("Scroll X"), -10.0, 10.0, 0.01, 2);
    connect(m_spinScrollX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setScrollX(static_cast<float>(v));
        notifyChanged();
    });

    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), 0.0, kPaddingMax, kPaddingStep, kPaddingDecimals);
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setPadding(static_cast<float>(v));
        notifyChanged();
    });
}

} // namespace NereusSDR
