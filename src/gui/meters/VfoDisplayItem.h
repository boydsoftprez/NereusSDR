#pragma once

// =================================================================
// src/gui/meters/VfoDisplayItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Display modes by slice letter (Source slice, All slices,
//                 Slice A and on; a saved VFO B loads as Slice B), per-slice
//                 state (VfoSliceState), the clsVfoDisplay colours, band
//                 text, background and a Thetis layout for new items,
//                 kVfoDisplayThetisDefaults, sliceX_* published readings and
//                 the import map fromThetisDisplayMode. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - setTransmitting overrides MeterItem's (Fade on RX / TX).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-24 : setUnavailableText: says the container's slice is not
//                 open instead of its last frequency (R-R3-49, R-R3-21).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QVector>
class QMouseEvent;
class QWheelEvent;

namespace NereusSDR {

// What a VFO display shows for one slice. MainWindow::refreshContainerMeter
// fills one per slice the item shows (setSliceState). `transmitting` marks
// the slice transmit comes off (the TX box and the TX marker light on it
// while the item is transmitting); `active` is the radio's active slice;
// `open` is false for a slice that is not open.
struct VfoSliceState {
    int index{0};
    qint64 frequencyHz{0};
    QString mode;
    QString filter;
    QString band;
    QString bandText;
    int stepHz{0};
    bool locked{false};
    bool transmitting{false};
    bool active{false};
    bool open{false};
};

// VFO frequency display with mouse wheel digit tuning.
// Ported from Thetis clsVfoDisplay (MeterManager.cs:12881+).
class VfoDisplayItem : public MeterItem {
    Q_OBJECT
public:
    // Saved integers are kept: 0 was VFO A, 1 VFO B, 2 Both.
    //   SourceSlice  (0): the slice the entry's Source follows.
    //   LegacySliceB (1): load only; loads (and re-saves) as fixed Slice B.
    //   AllSlices    (2): Slice A through the last open slice; a saved
    //                     Both loads as this.
    //   3 + index:        a fixed slice (fixedSliceMode()).
    enum class DisplayMode : int { SourceSlice = 0, LegacySliceB = 1, AllSlices = 2 };
    static constexpr int kFixedSliceModeBase = 3;
    // The most slices any board opens (BoardCapabilities maxSlices, 5 on
    // the 2-ADC boards); the editor offers Slice A through this many.
    static constexpr int kMaxSliceCount = 5;
    // From Thetis MeterManager.cs:39888 [v2.10.3.15]: a slice that is not
    // the active one draws at alpha 24 in All slices mode.
    static constexpr int kInactiveSliceAlpha = 24;

    // How the display is laid out. Compact (0, the constructor's and every
    // older record's) is the NereusSDR frequency row over mode, filter and
    // band; Thetis (1, new items) is renderVfoDisplay's layout
    // (MeterManager.cs:39857-40457 [v2.10.3.15]).
    enum class Layout : int { Compact = 0, Thetis = 1 };

    explicit VfoDisplayItem(QObject* parent = nullptr);

    // Fixed slice mode for Slice A + sliceIndex.
    static int fixedSliceMode(int sliceIndex) { return kFixedSliceModeBase + sliceIndex; }
    // The slice index a fixed slice mode shows, or -1.
    static int fixedSliceIndex(int mode);
    // Thetis VFODisplayMode to ours: BOTH=0 -> All slices, A=1 -> Slice A,
    // B=2 -> Slice B; out of range reads as BOTH. Used only by a Thetis
    // item import. From Thetis MeterManager.cs:14247-14252, :28072-28073
    // [v2.10.3.15].
    static int fromThetisDisplayMode(int thetisMode);
    // A saved or set mode as the item keeps it: 1 becomes Slice B, an
    // unknown value Source slice.
    static int normalizedDisplayMode(int mode);
    // Plain reasons for the controls NereusSDR does not have.
    static QString splitUnavailableReason();
    static QString syncUnavailableReason();

    void setFrequency(int64_t hz) { m_frequencyHz = hz; }
    int64_t frequency() const { return m_frequencyHz; }
    void setFrequencyB(int64_t hz) { m_frequencyBHz = hz; }
    int64_t frequencyB() const { return m_frequencyBHz; }

    void setDisplayMode(DisplayMode mode) { setDisplayModeValue(static_cast<int>(mode)); }
    void setDisplayModeValue(int mode) { m_displayMode = normalizedDisplayMode(mode); }
    DisplayMode displayMode() const { return static_cast<DisplayMode>(m_displayMode); }
    int displayModeValue() const { return m_displayMode; }

    void setLayout(Layout layout) { m_layout = layout; }
    Layout layout() const { return m_layout; }

    // Per-slice state (runtime, not saved). With none set the item shows
    // its single-slice values (setFrequency and the labels) as one open
    // slice; once any is set, a slice without one is not open.
    void setSliceState(int sliceIndex, const VfoSliceState& state);
    void clearSliceStates() { m_sliceStates.clear(); }
    VfoSliceState sliceState(int sliceIndex) const;
    // The slice the entry's Source resolves to; -1 when none.
    void setSourceSliceIndex(int sliceIndex) { m_sourceSliceIndex = sliceIndex; }
    int sourceSliceIndex() const { return m_sourceSliceIndex; }
    // The slices the item shows, left to right.
    QVector<int> displayedSlices() const;
    // Everything the item draws from runtime state, for change detection.
    QString presentationKey() const;

    void setModeLabel(const QString& label) { m_modeLabel = label; }
    void setFilterLabel(const QString& label) { m_filterLabel = label; }
    void setBandLabel(const QString& label) { m_bandLabel = label; }
    void setTransmitting(bool tx) override { MeterItem::setTransmitting(tx); m_transmitting = tx; }
    bool isTransmitting() const noexcept { return m_transmitting; }
    void setSplit(bool split) { m_split = split; }
    // R-R3-49 / R-R3-21 (fix wave M2): plain words shown instead of the
    // frequency and labels while the container's slice is not open (for
    // example "Slice B is not open"); empty shows the slice. Not saved.
    void setUnavailableText(const QString& text) { m_unavailableText = text; }
    QString unavailableText() const { return m_unavailableText; }

    void setFrequencyColour(const QColor& c) { m_freqColour = c; }
    QColor frequencyColour() const { return m_freqColour; }
    void setModeColour(const QColor& c) { m_modeColour = c; }
    QColor modeColour() const { return m_modeColour; }
    void setFilterColour(const QColor& c) { m_filterColour = c; }
    QColor filterColour() const { return m_filterColour; }
    void setBandColour(const QColor& c) { m_bandColour = c; }
    QColor bandColour() const { return m_bandColour; }
    void setRxColour(const QColor& c) { m_rxColour = c; }
    QColor rxColour() const { return m_rxColour; }
    void setTxColour(const QColor& c) { m_txColour = c; }
    QColor txColour() const { return m_txColour; }
    void setFrequencySmallColour(const QColor& c) { m_freqSmallColour = c; }
    QColor frequencySmallColour() const { return m_freqSmallColour; }
    void setTitleColour(const QColor& c) { m_titleColour = c; }
    QColor titleColour() const { return m_titleColour; }
    void setSplitBackColour(const QColor& c) { m_splitBackColour = c; }
    QColor splitBackColour() const { return m_splitBackColour; }
    void setSplitColour(const QColor& c) { m_splitColour = c; }
    QColor splitColour() const { return m_splitColour; }
    void setDigitHighlightColour(const QColor& c) { m_digitHighlight = c; }
    QColor digitHighlightColour() const { return m_digitHighlight; }
    void setBandTextColour(const QColor& c) { m_bandTextColour = c; }
    QColor bandTextColour() const { return m_bandTextColour; }
    void setLockColour(const QColor& c) { m_lockColour = c; }
    QColor lockColour() const { return m_lockColour; }
    void setSyncColour(const QColor& c) { m_syncColour = c; }
    QColor syncColour() const { return m_syncColour; }
    void setBackgroundColour(const QColor& c) { m_backColour = c; }
    QColor backgroundColour() const { return m_backColour; }
    void setShowBandText(bool on) { m_showBandText = on; }
    bool showBandText() const { return m_showBandText; }

    // New items (makeEntry) take kVfoDisplayThetisDefaults.
    void applyNewItemDefaults() override;

    // sliceA_freq, sliceA_mode, sliceA_filter, sliceA_band and sliceA_step,
    // and the same for each slice letter shown, as Thetis publishes vfoa_*
    // (MeterManager.cs:31101-31120 [v2.10.3.15]).
    QHash<QString, QVariant> publishedReadings() const override;

    // The reason shown over the split box and the sync icon; empty
    // elsewhere.
    QString reasonAt(const QPointF& pos, int widgetW, int widgetH) const override;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    bool handleWheel(QWheelEvent* event, int widgetW, int widgetH) override;
    bool handleMousePress(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseMove(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseLeave() override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    void frequencyChangeRequested(int64_t deltaHz);
    void bandStackRequested(int bandIndex);
    void filterContextRequested(int filterIndex);

private:
    // A Thetis layout region under a point (renderVfoDisplay's mouse boxes,
    // MeterManager.cs:39926-40182 [v2.10.3.15]).
    enum class Region { None, Digit, Mode, Filter, Band, Split, Tx, Lock, Sync };
    struct Hit {
        Region region{Region::None};
        int column{-1};
        int sliceIndex{-1};
        QRectF box;
        int64_t step{0};
    };

    int64_t digitWeightAt(float xFraction) const;
    QString formatFrequency(int64_t hz) const;
    VfoSliceState stateFor(int sliceIndex) const;
    // A slice the wheel and clicks may act on: the Source slice, or the
    // item's own values when no per-slice state is set.
    bool interactive(const VfoSliceState& state) const;
    QRect columnRect(const QRect& rect, int column, int count) const;
    Hit thetisHitAt(const QPointF& pos, int widgetW, int widgetH) const;
    void paintCompactColumn(QPainter& p, const QRect& rect, const VfoSliceState& s);
    void paintThetisColumn(QPainter& p, const QRect& rect, const VfoSliceState& s,
                           int count, int alpha, const Hit& hover);

    int64_t m_frequencyHz{14074000};
    int64_t m_frequencyBHz{14074000};
    int m_displayMode{static_cast<int>(DisplayMode::SourceSlice)};
    Layout m_layout{Layout::Compact};
    QString m_modeLabel{QStringLiteral("USB")};
    QString m_filterLabel{QStringLiteral("2.7k")};
    QString m_bandLabel{QStringLiteral("20m")};
    bool m_transmitting{false};
    bool m_split{false};
    QString m_unavailableText;
    QHash<int, VfoSliceState> m_sliceStates;
    int m_sourceSliceIndex{-1};
    QPointF m_hoverPos;
    bool m_hovering{false};

    // Colors from Thetis clsVfoDisplay defaults
    QColor m_freqColour{0xff, 0xa5, 0x00};     // Orange
    QColor m_modeColour{0x80, 0x80, 0x80};
    QColor m_filterColour{0x80, 0x80, 0x80};
    QColor m_bandColour{0xff, 0xff, 0xff};
    QColor m_rxColour{0x00, 0xff, 0x00};        // LimeGreen
    QColor m_txColour{0xff, 0x00, 0x00};        // Red
    QColor m_splitColour{0xff, 0xa5, 0x00};
    QColor m_digitHighlight{0x80, 0x80, 0x80};
    // Older records load these (fields 14 and on are new); the values are
    // the clsVfoDisplay ones, and the background is the NereusSDR fill.
    QColor m_freqSmallColour{0xff, 0xa5, 0x00};    // Orange
    QColor m_titleColour{0x80, 0x80, 0x80};        // Gray
    QColor m_splitBackColour{0x40, 0x40, 0x40};
    QColor m_bandTextColour{0x32, 0xcd, 0x32};     // LimeGreen
    QColor m_lockColour{0x32, 0xcd, 0x32};         // LimeGreen
    QColor m_syncColour{0x32, 0xcd, 0x32};         // LimeGreen
    QColor m_backColour{0x0a, 0x0a, 0x18};
    bool m_showBandText{false};
};

// New VFO items (makeEntry) take Thetis's VFO display: the clsVfoDisplay
// constructor (MeterManager.cs:14309-14347 [v2.10.3.15]) and AddVFODisplay's
// background (MeterManager.cs:25768 [v2.10.3.15]). Thetis starts on
// VFO_BOTH (:14341); a new NereusSDR item starts on Source slice.
struct VfoDisplayThetisDefaults {
    QRgb frequencyColour;
    QRgb frequencySmallColour;
    QRgb modeColour;
    QRgb filterColour;
    QRgb titleColour;
    QRgb splitBackColour;
    QRgb splitColour;
    QRgb rxColour;
    QRgb txColour;
    QRgb bandColour;
    QRgb digitHighlightColour;
    QRgb bandTextColour;
    QRgb lockColour;
    QRgb syncColour;
    QRgb backColour;
    bool showBandText;
    VfoDisplayItem::Layout layout;
};
inline constexpr VfoDisplayThetisDefaults kVfoDisplayThetisDefaults{
    qRgb(255, 165, 0),              // _frequencyColour = Color.Orange (:14313)
    qRgb(255, 165, 0),              // _frequencyColourSmall = Color.Orange (:14314)
    qRgb(128, 128, 128),            // _modeColour = Color.Gray (:14315)
    qRgb(128, 128, 128),            // _filterColour = Color.Gray (:14320)
    qRgb(128, 128, 128),            // _typeColor = Color.Gray (:14312)
    qRgb(64, 64, 64),               // _splitBackColour = Color.FromArgb(64, 64, 64) (:14316)
    qRgb(255, 165, 0),              // _splitColour = Color.Orange (:14317)
    qRgb(50, 205, 50),              // _rxColour = Color.LimeGreen (:14318)
    qRgb(255, 0, 0),                // _txColour = Color.Red (:14319)
    qRgb(255, 255, 255),            // _bandColour = Color.White (:14321)
    qRgb(128, 128, 128),            // _digitHighlightColour = Color.FromArgb(128, 128, 128) (:14322)
    qRgb(50, 205, 50),              // _band_text_colour = Color.LimeGreen (:14344)
    qRgb(50, 205, 50),              // _lock_colour = Color.LimeGreen (:14346)
    qRgb(50, 205, 50),              // _sync_colour = Color.LimeGreen (:14347)
    qRgb(32, 32, 32),               // sc.Colour = Color.FromArgb(32, 32, 32) (:25768)
    false,                          // _band_text = false (:14343)
    VfoDisplayItem::Layout::Thetis, // renderVfoDisplay (:39857-40457)
};
} // namespace NereusSDR
