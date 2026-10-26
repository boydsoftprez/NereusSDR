#pragma once

// =================================================================
// src/gui/meters/TextOverlayItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23: R-R3-13: %VALUE% shows "--" with no reading
//                 (isNoMeterReading); resolvedText1() / resolvedText2()
//                 read-only accessors. J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-10-26 - Thetis RX/TX swap (LineMode: line 1 on RX, line 2 on TX,
//                 new items) beside both lines (older records); per-line
//                 offsets, italic / underline / strikeout, copy font 1 to
//                 2 and offsets 1 to 2, the panel's MOX flip fade, auto
//                 scroll and kTextOverlayThetisDefaults for new items.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - RX and TX LED gates (Thetis RXLedLogic / TXLedLogic,
//                 RXFourChar / TXFourChar): an LED's 4-character ID per
//                 state, read from LedResultRegistry; the TX gate follows
//                 its own logic flag (Thetis tests the RX one). Appended to
//                 the record. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
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

#include "MeterItem.h"
#include <QColor>
#include <QFont>
#include <QMap>
#include <QPointF>
#include <QRect>
#include <QRgb>

namespace NereusSDR {

// From Thetis clsTextOverlay (MeterManager.cs:18746+)
// Two-line text display with %VARIABLE% substitution.
class TextOverlayItem : public MeterItem {
    Q_OBJECT

public:
    explicit TextOverlayItem(QObject* parent = nullptr) : MeterItem(parent) {}

    // How the two lines show. Both: line 1 in the top half and line 2 in
    // the bottom half, every time (the look of records saved before the
    // field existed). RxTxSwap: Thetis renderTextOverlay, line 1 on
    // receive and line 2 on transmit, placed by the per-line offsets
    // (MeterManager.cs:34135-34290 [v2.10.3.15]); new items.
    // Saved as 0 / 1; never renumber.
    enum class LineMode { Both = 0, RxTxSwap = 1 };
    void setLineMode(LineMode m) { m_lineMode = m; }
    LineMode lineMode() const { return m_lineMode; }

    // Template text with %PLACEHOLDER% tokens
    void setText1(const QString& t) { m_text1 = t; }
    QString text1() const { return m_text1; }

    void setText2(const QString& t) { m_text2 = t; }
    QString text2() const { return m_text2; }

    // Per-line styling
    void setTextColour1(const QColor& c) { m_textColour1 = c; }
    QColor textColour1() const { return m_textColour1; }

    void setTextColour2(const QColor& c) { m_textColour2 = c; }
    QColor textColour2() const { return m_textColour2; }

    void setTextBackColour1(const QColor& c) { m_textBackColour1 = c; }
    QColor textBackColour1() const { return m_textBackColour1; }
    void setTextBackColour2(const QColor& c) { m_textBackColour2 = c; }
    QColor textBackColour2() const { return m_textBackColour2; }

    void setShowTextBackColour1(bool s) { m_showTextBack1 = s; }
    bool showTextBackColour1() const { return m_showTextBack1; }
    void setShowTextBackColour2(bool s) { m_showTextBack2 = s; }
    bool showTextBackColour2() const { return m_showTextBack2; }

    void setShowBackPanel(bool s) { m_showBackPanel = s; }
    bool showBackPanel() const { return m_showBackPanel; }

    void setPanelBackColour1(const QColor& c) { m_panelBack1 = c; }
    QColor panelBackColour1() const { return m_panelBack1; }
    void setPanelBackColour2(const QColor& c) { m_panelBack2 = c; }
    QColor panelBackColour2() const { return m_panelBack2; }

    void setFontFamily1(const QString& f) { m_fontFamily1 = f; }
    QString fontFamily1() const { return m_fontFamily1; }
    void setFontFamily2(const QString& f) { m_fontFamily2 = f; }
    QString fontFamily2() const { return m_fontFamily2; }

    void setFontSize1(float s) { m_fontSize1 = s; }
    float fontSize1() const { return m_fontSize1; }
    void setFontSize2(float s) { m_fontSize2 = s; }
    float fontSize2() const { return m_fontSize2; }

    void setFontBold1(bool b) { m_fontBold1 = b; }
    bool fontBold1() const { return m_fontBold1; }
    void setFontBold2(bool b) { m_fontBold2 = b; }
    bool fontBold2() const { return m_fontBold2; }

    // Font style beside bold (Thetis Style1 / Style2, a FontStyle).
    void setFontItalic1(bool on) { m_fontItalic1 = on; }
    bool fontItalic1() const { return m_fontItalic1; }
    void setFontUnderline1(bool on) { m_fontUnderline1 = on; }
    bool fontUnderline1() const { return m_fontUnderline1; }
    void setFontStrikeout1(bool on) { m_fontStrikeout1 = on; }
    bool fontStrikeout1() const { return m_fontStrikeout1; }
    void setFontItalic2(bool on) { m_fontItalic2 = on; }
    bool fontItalic2() const { return m_fontItalic2; }
    void setFontUnderline2(bool on) { m_fontUnderline2 = on; }
    bool fontUnderline2() const { return m_fontUnderline2; }
    void setFontStrikeout2(bool on) { m_fontStrikeout2 = on; }
    bool fontStrikeout2() const { return m_fontStrikeout2; }

    // Per-line text position in RxTxSwap, ratios of the item width from
    // its top left (Thetis TextXOffset1 / TextYOffset1 / TextXOffset2 /
    // TextYOffset2).
    void setTextXOffset1(float v) { m_xOffset1 = v; }
    float textXOffset1() const { return m_xOffset1; }
    void setTextYOffset1(float v) { m_yOffset1 = v; }
    float textYOffset1() const { return m_yOffset1; }
    void setTextXOffset2(float v) { m_xOffset2 = v; }
    float textXOffset2() const { return m_xOffset2; }
    void setTextYOffset2(float v) { m_yOffset2 = v; }
    float textYOffset2() const { return m_yOffset2; }

    // Setup's "Copy font 1 to 2" and "Copy offsets" buttons.
    void copyFont1To2();
    void copyOffsets1To2();

    // Both: pixels per paint, 0 = off (older records). RxTxSwap: Thetis
    // ScrollX, item widths per second, used only while the text is wider
    // than the item; 0 = off.
    void setScrollX(float s) { m_scrollX = s; }
    float scrollX() const { return m_scrollX; }

    // Both: the inset, a ratio of the item height. RxTxSwap: the panel's
    // height, a ratio of the item width (Thetis SpacerPadding).
    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }

    // Show RX / Show TX (MeterManager.cs:34178-34179 [v2.10.3.15]): in
    // RxTxSwap the text draws only while the one for the current state is
    // true. Runtime state, true unless something hides it (Thetis sets
    // them from the RX / TX LED logic); not saved.
    void setShowRx(bool on) { m_showRx = on; }
    bool showRx() const { return m_showRx; }
    void setShowTx(bool on) { m_showTx = on; }
    bool showTx() const { return m_showTx; }

    // LED gates (Thetis RXLedLogic / TXLedLogic and RXFourChar /
    // TXFourChar, MeterManager.cs:20232-20310 [v2.10.3.15]; setup keys
    // textoverlay_rx_ledlogic, textoverlay_tx_ledlogic, textoverlay_rx_4char,
    // textoverlay_tx_4char, :27554-27558). With a gate's logic on, its
    // line shows only while the LED with that 4-character ID is true; an
    // ID no LED holds hides nothing. Turning the logic off shows the line.
    void setRxLedLogic(bool on);
    bool rxLedLogic() const { return m_rxLedLogic; }
    void setTxLedLogic(bool on);
    bool txLedLogic() const { return m_txLedLogic; }
    // Kept to the 4-character ID alphabet (A-Z, a-z, 0-9), at most four.
    void setRxFourChar(const QString& id);
    QString rxFourChar() const { return m_rxFourChar; }
    void setTxFourChar(const QString& id);
    QString txFourChar() const { return m_txFourChar; }
    // Reads both gates' LEDs from LedResultRegistry into Show RX / Show TX
    // (Thetis Update, MeterManager.cs:20546-20559 [v2.10.3.15]); true when
    // either changed. advanceMeter calls it.
    bool refreshLedGates();

    // RxTxSwap runtime state (not saved): the panel's MOX flip fade
    // (48..255), the panel colour drawn now with that alpha, and the
    // current line's scroll offset (item widths) and pass count.
    int panelFadeValue() const { return m_panelFadeValue; }
    QColor panelColour() const;
    float scrollXOffset1() const { return m_scrollXOffset1; }
    float scrollXOffset2() const { return m_scrollXOffset2; }
    int scrollXCount1() const { return m_scrollXCount1; }
    int scrollXCount2() const { return m_scrollXCount2; }

    // RxTxSwap geometry, in widget pixels: the panel rectangle and the
    // line drawn now (text, top left, pixel size, font).
    QRect swapPanelRect(int widgetW, int widgetH) const;
    struct SwapLine {
        QString text;
        QPointF topLeft;
        QFont   font;
        QColor  colour;
        bool    fillBack{false};
        QColor  backColour;
    };
    // The line the transmit state selects; empty text when Show RX / Show
    // TX hides it.
    SwapLine swapLine(int widgetW, int widgetH) const;

    // External variable registry for substitution
    void setVariable(const QString& name, const QString& value) {
        m_variables[name.toUpper()] = value;
    }

    // The two lines paint() draws, after token substitution (R-R3-13:
    // %VALUE% is "--" with no reading).
    QString resolvedText1() const { return resolveText(m_text1); }
    QString resolvedText2() const { return resolveText(m_text2); }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

    // New items (makeEntry) take kTextOverlayThetisDefaults.
    void applyNewItemDefaults() override;
    // RxTxSwap: a MOX flip restarts the panel fade.
    void setTransmitting(bool tx) override;
    // RxTxSwap: steps the panel fade and the scroll once per update
    // interval; true when either changed.
    bool advanceMeter(qint64 monotonicMs) override;
    // RxTxSwap: the pointer over the item starts the scroll again.
    bool handleMouseMove(QMouseEvent* event, int widgetW, int widgetH) override;

private:
    // From Thetis parseText() (MeterManager.cs:19267)
    QString resolveText(const QString& templateText) const;
    bool effectiveTransmitting() const { return transmitting() && !foreignSession(); }
    bool coverFadeActive() const;
    bool checkPanelFlip();
    QFont swapFont(bool tx, const QPaintDevice* device, int widgetW) const;
    // One frame of the Thetis scroll for the current line, `deltaMs` since
    // the last; true when the offset or count changed.
    bool stepScroll(double deltaMs);
    void paintBoth(QPainter& p, const QRect& rect);
    void paintSwap(QPainter& p, int widgetW, int widgetH);

    // Templates
    QString m_text1;
    QString m_text2;

    // Line 1 styling
    QColor  m_textColour1{0xff, 0xff, 0xff};
    QColor  m_textBackColour1{0x40, 0x40, 0x40};
    bool    m_showTextBack1{false};
    QString m_fontFamily1{QStringLiteral("Trebuchet MS")};
    float   m_fontSize1{18.0f};
    bool    m_fontBold1{false};

    // Line 2 styling
    QColor  m_textColour2{0xff, 0xff, 0xff};
    QColor  m_textBackColour2{0x40, 0x40, 0x40};
    bool    m_showTextBack2{false};
    QString m_fontFamily2{QStringLiteral("Trebuchet MS")};
    float   m_fontSize2{18.0f};
    bool    m_fontBold2{false};

    LineMode m_lineMode{LineMode::Both};

    // Font styles and per-line offsets (RxTxSwap)
    bool  m_fontItalic1{false};
    bool  m_fontUnderline1{false};
    bool  m_fontStrikeout1{false};
    bool  m_fontItalic2{false};
    bool  m_fontUnderline2{false};
    bool  m_fontStrikeout2{false};
    float m_xOffset1{0.0f};
    float m_yOffset1{0.0f};
    float m_xOffset2{0.0f};
    float m_yOffset2{0.0f};

    // RxTxSwap runtime state (not saved)
    bool   m_showRx{true};
    bool   m_showTx{true};

    // LED gates (saved). From Thetis MeterManager.cs:27554-27558
    // [v2.10.3.15]: both logic flags default false, both IDs "".
    bool    m_rxLedLogic{false};
    bool    m_txLedLogic{false};
    QString m_rxFourChar;
    QString m_txFourChar;
    int    m_panelFadeValue{255};
    bool   m_moxFadeToggle{false};
    qint64 m_lastStepMs{-1};
    float  m_scrollXOffset1{0.0f};
    float  m_scrollXOffset2{0.0f};
    int    m_scrollXCount1{0};
    int    m_scrollXCount2{0};
    // The current line's measured width and the item width, in pixels,
    // from the last paint (the scroll needs both; 0 until painted).
    double m_measuredTextWidth1{0.0};
    double m_measuredTextWidth2{0.0};
    double m_measuredItemWidth{0.0};

    // Panel
    bool   m_showBackPanel{true};
    QColor m_panelBack1{0x20, 0x20, 0x20};
    QColor m_panelBack2{0x20, 0x20, 0x20};

    // Scrolling
    float m_scrollX{0.0f};       // 0 = disabled, negative = scroll left
    float m_scrollOffset1{0.0f};
    float m_scrollOffset2{0.0f};

    float m_padding{0.1f};

    // Variable registry
    QMap<QString, QString> m_variables;
    mutable int m_precision{1};  // current precision for %PRECIS=n%
};

// Thetis's clsTextOverlay constructor values
// (From Thetis MeterManager.cs:20123-20183 [v2.10.3.15]). New items take
// these (applyNewItemDefaults); an item read from a record keeps the
// record's values, and the constructor keeps the older ones.
struct TextOverlayItemThetisDefaults {
    QRgb                      textColour1;
    QRgb                      textColour2;
    QRgb                      textBackColour1;
    QRgb                      textBackColour2;
    QRgb                      panelBackColour1;
    QRgb                      panelBackColour2;
    bool                      showTextBack1;
    bool                      showTextBack2;
    bool                      showBackPanel;
    float                     xOffset1;
    float                     yOffset1;
    float                     xOffset2;
    float                     yOffset2;
    float                     scrollX;
    const char*               fontFamily1;
    float                     fontSize1;
    const char*               fontFamily2;
    float                     fontSize2;
    float                     padding;
    TextOverlayItem::LineMode lineMode;
};
inline constexpr TextOverlayItemThetisDefaults kTextOverlayThetisDefaults{
    qRgb(255, 255, 255),  // _text_colour1 (:20138)
    qRgb(255, 255, 255),  // _text_colour2 (:20139)
    qRgb(64, 64, 64),     // _text_back_colour1 (:20140)
    qRgb(64, 64, 64),     // _text_back_colour2 (:20141)
    qRgb(32, 32, 32),     // _panel_back_colour1 (:20142)
    qRgb(32, 32, 32),     // _panel_back_colour2 (:20143)
    false,                // _show_text_back_colour1 (:20146)
    false,                // _show_text_back_colour2 (:20147)
    true,                 // _show_back_panel (:20148)
    0.0f,                 // _x_offset_1 (:20149)
    0.0f,                 // _y_offset_1 (:20150)
    0.0f,                 // _x_offset_2 (:20151)
    0.0f,                 // _y_offset_2 (:20152)
    -0.15f,               // _x_scroll = -0.15f (:20154)
    "Trebuchet MS",       // _fontFamily_1 (:20169), FontStyle.Regular (:20170)
    18.0f,                // _fontSize_1 = 18f (:20171)
    "Trebuchet MS",       // _fontFamily_2 (:20172), FontStyle.Regular (:20173)
    18.0f,                // _fontSize_2 = 18f (:20174)
    0.1f,                 // _padding = 0.1f (:20176)
    TextOverlayItem::LineMode::RxTxSwap,  // renderTextOverlay (:34135-34290)
};

} // namespace NereusSDR
