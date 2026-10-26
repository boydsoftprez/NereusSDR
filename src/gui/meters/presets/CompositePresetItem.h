// Ported from Thetis MeterManager.cs [v2.10.3.15].
// Modification history (NereusSDR):
//   2026-10-26 - Clock: the thetisFormat key (Thetis time and date strings)
//                 and kClockPresetThetisDefaults for new clock faces.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Signal history: a second trace with its own source
//                 (trace2Context; D2, a slice per trace), fed by its own
//                 reading consumer or data source variable 2; a saved
//                 one-trace face loads padded with trace 2 off.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Signal text: signalSubLinePixels for the uV and dBm sub
//                 line. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - Magic eye: eye size, bezel size and bezel image keys,
//                 the bezel image cache. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - ANAN, Cross and Power/SWR per-item options (power limit,
//                 power scale, peak hold and peak value colors, other
//                 needles color, shadow, dark mode), their Thetis new-item
//                 defaults, and MMIO variables 2+ for ANAN and Cross.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - setAboveS9Frequency overrides MeterItem's (VHF S9 rule
//                 per slice). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-12 - Shared accessory display readings and independent legal
//                 marker / display scale by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-04 — Selected RX source identity and RX-only presentation reset by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Independent history sampling cadence by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-02 — Effective contextual draft properties and portable settings by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-02 — Native complete faces by J.J. Boyd (KG4VCF), with AI-assisted
// transformation via OpenAI Codex.
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

#pragma once
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterDynamics.h"
#include <QJsonObject>
#include <QDateTime>
#include <QImage>
namespace NereusSDR {
class BandButtonItem; class ModeButtonItem; class VfoDisplayItem; class ClockItem;
// GUI presentation only. Channels are supplied by the existing shared poller.
class CompositePresetItem : public MeterItem {
    Q_OBJECT
public:
    enum class Face { PowerSwr, Anan, Cross, Eye, SignalText, History, Vfo, Clock, Contest };
    explicit CompositePresetItem(Face face, QObject* parent=nullptr);
    QString typeId() const;
    QJsonObject configuration() const;
    QStringList editableFields() const;
    QStringList editableChannelFields(int channel) const;
    bool applyConfiguration(const QJsonObject&);
    QString serialize() const override;
    bool deserialize(const QString&) override;
    QSet<int> readingBindings() const override;
    void pushBindingValue(int,double) override;
    void setValue(double v) override { pushBindingValue(bindingId(),v); }
    void setBindingUnavailable(int,const QString&) override;
    void setBindingSupport(int,BindingSupport) override;
    bool advanceMeter(qint64) override;
    void resetForTxTransition(bool) override;
    void resetRxSource() override;
    // MMIO variable 2 and up drive the matching ANAN / Cross needle.
    void setSecondaryValue(int index, double value) override;
    void applyNewItemDefaults() override;
    // The ANAN face art this item paints: its -dark variant in dark mode
    // when that file exists (ImageItem::variantPath), else the normal art.
    QString faceArtPath(double diagonalPx) const;
    // Signal history trace 2 (channel 1). It reads its own source, the
    // configuration's trace2Context (Slice A-D or Follow selected RX),
    // never the entry's: the entry's readings (readingBindings) feed trace
    // 1 alone. Absent, trace 2 is off; data source variable 2 also turns
    // it on and feeds it, as Thetis axis 1 reads its MMIO variable
    // (MeterManager.cs:18132-18163 [v2.10.3.15]).
    bool trace2Shown() const;
    bool trace2FromDataSource() const { return m_face==Face::History && channelFromMmio(1); }
    QJsonObject trace2Context() const { return m_config.value(QStringLiteral("trace2Context")).toObject(); }
    // The context trace 2 reads: trace2Context, with the entry's radio
    // session when it names none. Empty while trace 2 is off.
    QJsonObject trace2SourceContext(const QJsonObject& entryContext) const;
    int trace2Binding() const { return m_face==Face::History && m_channels.size()>1 ? m_channels[1].binding : -1; }
    void pushTrace2Value(double reading);
    // Trace 2's source changed (rxSourceChanged): its reading and samples go.
    void resetTrace2();
    // Why trace 2 has no reading here (another session's radio); empty when
    // it can read. Trace 2 alone shows the unavailable veil.
    void setTrace2Unavailable(const QString& reason);
    QString trace2UnavailableReason() const { return m_trace2Unavailable; }
    // Where trace 2's readout and veil are drawn in a face of this size.
    QRectF trace2ReadoutRect(int width,int height) const;
#ifdef NEREUS_BUILD_TESTS
    int historySampleCountForTest() const { return m_samples.size(); }
    int trace2SampleCountForTest() const { return m_samples2.size(); }
    void setFaceArtBaseForTest(const QString& path) { m_faceArtBase=path; markPresentationDirty(true); }
#endif
    void setPowerScale(int) override;
    // Temporary display top and legal marker, independent of radio rating.
    void setPowerScale(int visualLimitWatts, double topWatts) override;
    double legalMarkerW() const { return m_legalMarkerW; }
    void setAboveS9Frequency(bool above) override { MeterItem::setAboveS9Frequency(above); if(m_aboveS9!=above) { m_aboveS9=above; markPresentationDirty(true); } }
    void markPresentationDirty(bool staticLayers=false) { m_presentationDirty=true; m_staticPresentationDirty=m_staticPresentationDirty || staticLayers; }
    bool takeStaticPresentationChange() override { const bool dirty=m_staticPresentationDirty; m_staticPresentationDirty=false; return dirty; }
    void setFrequency(qint64 hz);
    void setModeLabel(const QString&);
    void setBandLabel(const QString&);
    void setUnavailableText(const QString&);
    void setPreviewInert(bool);
    QVector<MeterItem*> internalItems() const;
    BandButtonItem* bandButtons() const { return m_bands; }
    ModeButtonItem* modeButtons() const { return m_modes; }
    VfoDisplayItem* vfoDisplay() const { return m_vfo; }
    ClockItem* clockDisplay() const { return m_clock; }
    int preferredFaceHeight() const;
    int minimumConfiguredFaceHeight() const;
    QSize minimumFaceSize() const;
    double channelValue(int) const;
    double channelPeak(int) const;
    bool channelHasReading(int) const;
    QPointF calibratedPoint(int,double) const;
    // ANAN takes its fitted 1855×848 skin rect; points are actual widget coordinates.
    // Other faces retain their original needle-rectangle contract.
    QPointF needlePivot(int,const QRectF&) const;
    QPointF needleTip(int,double,const QRectF&) const;
    QRectF ananNeedleRect(int width,int height) const;
    QRectF ananGroupControlRect(int width,int height) const;
    bool channelVisible(int) const;
    QString channelUnits(int) const;
    QString signalReadout(double) const;
    // Signal text sub line size: Thetis's FontSize * 0.55, in pixels.
    int signalSubLinePixels() const;
    static QString formatSignalReading(double,MeterUnit,bool aboveS9,bool decimal=true);
    Layer renderLayer() const override { return Layer::Background; }
    bool participatesIn(Layer l) const override { return l==Layer::Background || l==Layer::OverlayDynamic; }
    void paint(QPainter&,int,int) override;
    void paintForLayer(QPainter&,int,int,Layer) override;
    bool handleMousePress(QMouseEvent*,int,int) override;
    bool handleMouseRelease(QMouseEvent*,int,int) override;
    bool handleMouseMove(QMouseEvent*,int,int) override;
    bool handleWheel(QWheelEvent*,int,int) override;
protected:
    struct Channel { QString name,units; int binding=-1; QMap<double,QPointF> calibration; MeterDynamics dynamics; QJsonObject config; };
    QVector<Channel> m_channels;
private:
    void initialise();
    double powerBasisForChannel(int) const;
    // Channel fed by MMIO variable `index`, or -1.
    int mmioChannel(int index) const;
    bool channelFromMmio(int channel) const { return channel>=1 && hasMmioBinding(channel) && mmioChannel(channel)==channel; }
    // Signal history trace 2 reads its own source, not the entry's.
    bool readsOwnSource(int channel) const { return m_face==Face::History && channel==1; }
    bool primaryNeedle(int channel) const;
    // A blank optional color key falls back to the face's own ink.
    QColor optionalColor(const char* key,const QColor& fallback) const;
    bool faceShadow() const { return m_config.value(QStringLiteral("shadow")).toBool(true); }
    bool hasLegalMarker(int) const;
    void configureDynamics();
    void layoutChildren(int,int);
    void paintBar(QPainter&,const QRectF&,int);
    void paintNeedles(QPainter&,const QRectF&,bool);
    void paintAnan(QPainter&,const QRectF&,bool);
    bool ananChannelIncluded(int) const;
    int ananReadoutCount() const;
    double ananReadoutCellWidth(double outerWidth) const;
    void paintEye(QPainter&,const QRectF&);
    void paintHistory(QPainter&,const QRectF&);
    Face m_face;
    QString m_kind;
    QJsonObject m_config;
    bool m_tx=false,m_aboveS9=false,m_inert=false,m_presentationDirty=true,m_staticPresentationDirty=true;
    int m_powerScale=100;
    // Runtime display values only; never serialized or sent to hardware.
    double m_displayPowerTop=0.0;
    double m_legalMarkerW=0.0;
    qint64 m_lastFrame=-1, m_lastHistorySample=-1;
    QString m_stateMode,m_stateBand,m_stateUnavailable;
    struct Sample { qint64 time; double value; };
    QVector<Sample> m_samples;
    // Signal history trace 2: its samples, its last sample time and why it
    // has no reading here.
    QVector<Sample> m_samples2;
    qint64 m_lastHistorySample2=-1;
    QString m_trace2Unavailable;
    BandButtonItem* m_bands=nullptr; ModeButtonItem* m_modes=nullptr;
    VfoDisplayItem* m_vfo=nullptr; ClockItem* m_clock=nullptr;
    QString m_faceArtBase;
    QString m_faceArtLoaded;
    QImage m_faceArtImage;
    QString m_bezelLoaded;
    QImage m_bezelImage;
};

// New ANAN, Cross and Power/SWR items (makeEntry) take Thetis's values for
// the per-item options. A blank color (0) keeps the face's own ink; a power
// limit of 0 follows the radio's power rating, as Thetis's new needles do
// (_maxPower = CurrentPowerRating, MeterManager.cs:6774 [v2.10.3.15]).
// Saved items without these keys keep the earlier look.
struct CompositeFaceThetisDefaults {
    double powerLimitW;
    QRgb   powerScaleColour;
    QRgb   peakHoldColour;
    QRgb   peakValueColour;
    QRgb   subNeedleColour;
    bool   shadow;
    bool   darkMode;
};
// From Thetis MeterManager.cs:23784-24138 [v2.10.3.15] (AddAnanMM); the
// needle constructor :21961-21963, the image constructor :16044. The VHF
// rule at :23833 (start 20 dB lower above S9 frequencies) is the
// configureDynamics() reset, not a setting.
inline constexpr CompositeFaceThetisDefaults kAnanThetisDefaults{
    0.0,                        // ni.MaxPower = CurrentPowerRating
    qRgb(0x80, 0x80, 0x80),     // nspi.LowColour = Color.Gray (:23981)
    qRgb(0xff, 0x00, 0x00),     // _peakHoldMarkerColour = Color.Red (:21963)
    0,                          // no peak value on ANANMM: face ink
    qRgb(0x00, 0x00, 0x00),     // ni2.Colour = Color.Black (:23849)
    true,                       // _shadow = true (:21961)
    false,                      // img _darkMode = false (:16044)
};
// From Thetis MeterManager.cs:24140-24326 [v2.10.3.15] (AddCrossNeedle).
inline constexpr CompositeFaceThetisDefaults kCrossThetisDefaults{
    0.0,                        // ni.MaxPower = CurrentPowerRating
    qRgb(0x80, 0x80, 0x80),     // nspi.LowColour = Color.Gray (:24197)
    qRgb(0xff, 0x00, 0x00),     // _peakHoldMarkerColour = Color.Red (:21963)
    0,                          // no peak value on CROSS: face ink
    qRgb(0x00, 0x00, 0x00),     // ni2.Colour = Color.Black (:24231)
    true,                       // _shadow = true (:21961)
    false,                      // img _darkMode = false (:16044)
};
// From Thetis MeterManager.cs:21284-21302 [v2.10.3.15] (clsBarItem, the
// PWR bar of AddPWRBar).
inline constexpr CompositeFaceThetisDefaults kPowerSwrThetisDefaults{
    0.0,                        // MaxPower = CurrentPowerRating
    0,                          // no power scale item
    qRgb(0xff, 0x00, 0x00),     // _peakHoldMarkerColour = Color.Red
    qRgb(0xff, 0x00, 0x00),     // _peakValueColour = Color.Red
    0,                          // no sub needles
    true,                       // unused
    false,                      // unused
};
// New clock faces (makeEntry) draw Thetis's strings: 12 hour h:mm:ss with
// am / pm apart, the date ddd d MMM yyyy and the titles local / utc
// (thetisFormat). A saved clock without the key keeps yyyy-MM-dd and the
// earlier time format. The face's clock mode and date toggle stay.
struct ClockPresetThetisDefaults {
    bool thetisFormat;
    bool show24Hour;
};
// From Thetis MeterManager.cs:15407-15416 [v2.10.3.15] (clsClock), drawn
// by renderClock :40474-40537.
inline constexpr ClockPresetThetisDefaults kClockPresetThetisDefaults{
    true,                       // renderClock's strings (:40489-40516)
    true,                       // _24HourClock = true (:15416)
};
}
