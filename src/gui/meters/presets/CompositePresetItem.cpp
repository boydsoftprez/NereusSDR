// Ported from Thetis MeterManager.cs [v2.10.3.15].
// Modification history (NereusSDR):
//   2026-10-03 — Restore full approved source typography at ordinary sizes and
//                 reserve painted readout rows by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-03 — Retain dim calibrated parked ANAN pointers without fabricating
//                 readings by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Responsive object text and measured role fitting by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Fit compact source lettering inside rounded glass by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Readable minimum ANAN readout fonts and measured row reflow by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Approved full-wave art, source lettering and three-orb calibrated
//                 presentation by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//                 Source calibration remains authoritative; the presentation adapts
//                 the original shared pivot into the approved three visible hubs.
//   2026-10-03 — Owned human Nereus face composition and typed provider support by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Reserve distinct Signal font lines in stack faces by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-03 — Independent history sampling cadence by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-03 — Draw ANAN selector once in the static layer by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
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

// --- From Common.cs ---
//=================================================================
// common.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2012  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
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


#include "CompositePresetItem.h"
#include "AnanFaceRenderer.h"
#include "../ResponsiveText.h"
#include "gui/meters/MeterPoller.h"
#include "core/LogCategories.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/ClockItem.h"
#include <QPainter>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QImage>
#include <QLinearGradient>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
namespace NereusSDR {
namespace {
// Runtime child placement is a view of a composite face, not an edit to the
// imported primitive records. Save only layout state; live readings, colors,
// button state and event effects remain on their existing children.
class TransientChildLayout {
public:
    TransientChildLayout(const QVector<MeterItem*>& children,ButtonBoxItem* bands,ButtonBoxItem* modes)
        : m_bands(bands),m_modes(modes),m_bandRatio(bands?bands->heightRatio():0),m_modeRatio(modes?modes->heightRatio():0) {
        for(MeterItem* child:children) { m_rectangles.append({child,{child->x(),child->y(),child->itemWidth(),child->itemHeight()}}); }
    }
    ~TransientChildLayout() {
        for(const SavedRect& saved:m_rectangles) { saved.item->setRect(saved.rect.x(),saved.rect.y(),saved.rect.width(),saved.rect.height()); }
        if(m_bands) { m_bands->setHeightRatio(m_bandRatio); }
        if(m_modes) { m_modes->setHeightRatio(m_modeRatio); }
    }
private:
    struct SavedRect { MeterItem* item; QRectF rect; };
    QVector<SavedRect> m_rectangles;
    ButtonBoxItem* m_bands;
    ButtonBoxItem* m_modes;
    float m_bandRatio,m_modeRatio;
};
// From Thetis MeterManager.cs:23814-23829 [v2.10.3.15]
const QMap<double,QPointF> kSignal{{-127,QPointF(0.076,0.31)},{-121,QPointF(0.131,0.272)},{-115,QPointF(0.189,0.254)},{-109,QPointF(0.233,0.211)},{-103,QPointF(0.284,0.207)},{-97,QPointF(0.326,0.177)},{-91,QPointF(0.374,0.177)},{-85,QPointF(0.414,0.151)},{-79,QPointF(0.459,0.168)},{-73,QPointF(0.501,0.142)},{-63,QPointF(0.564,0.172)},{-53,QPointF(0.63,0.164)},{-43,QPointF(0.695,0.203)},{-33,QPointF(0.769,0.211)},{-23,QPointF(0.838,0.272)},{-13,QPointF(0.926,0.31)}};
// From Thetis MeterManager.cs:23855-23857 [v2.10.3.15]
const QMap<double,QPointF> kVolts{{10,QPointF(0.559,0.756)},{12.5,QPointF(0.605,0.772)},{15,QPointF(0.665,0.784)}};
// From Thetis MeterManager.cs:23884-23894 [v2.10.3.15]
const QMap<double,QPointF> kAmps{{0,QPointF(0.199,0.576)},{2,QPointF(0.27,0.54)},{4,QPointF(0.333,0.516)},{6,QPointF(0.393,0.504)},{8,QPointF(0.448,0.492)},{10,QPointF(0.499,0.492)},{12,QPointF(0.554,0.488)},{14,QPointF(0.608,0.5)},{16,QPointF(0.667,0.516)},{18,QPointF(0.728,0.54)},{20,QPointF(0.799,0.576)}};
// From Thetis MeterManager.cs:23955-23964 [v2.10.3.15]
const QMap<double,QPointF> kPower{{0,QPointF(0.099,0.352)},{5,QPointF(0.164,0.312)},{10,QPointF(0.224,0.28)},{25,QPointF(0.335,0.236)},{30,QPointF(0.367,0.228)},{40,QPointF(0.436,0.22)},{50,QPointF(0.499,0.212)},{60,QPointF(0.559,0.216)},{100,QPointF(0.751,0.272)},{150,QPointF(0.899,0.352)}};
// From Thetis MeterManager.cs:24017-24022 [v2.10.3.15]
const QMap<double,QPointF> kSwr{{1,QPointF(0.152,0.468)},{1.5,QPointF(0.28,0.404)},{2,QPointF(0.393,0.372)},{2.5,QPointF(0.448,0.36)},{3,QPointF(0.499,0.36)},{10,QPointF(0.847,0.476)}};
// From Thetis MeterManager.cs:24048-24054 [v2.10.3.15]
const QMap<double,QPointF> kCompression{{0,QPointF(0.249,0.68)},{5,QPointF(0.342,0.64)},{10,QPointF(0.425,0.624)},{15,QPointF(0.499,0.62)},{20,QPointF(0.571,0.628)},{25,QPointF(0.656,0.64)},{30,QPointF(0.751,0.688)}};
// From Thetis MeterManager.cs:24082-24084 [v2.10.3.15]
const QMap<double,QPointF> kAlcGroup{{-30,QPointF(0.295,0.804)},{0,QPointF(0.332,0.784)},{25,QPointF(0.499,0.756)}};
// From Thetis MeterManager.cs:24166-24180 [v2.10.3.15]
const QMap<double,QPointF> kForward{{0,QPointF(0.052,0.732)},{5,QPointF(0.146,0.528)},{10,QPointF(0.188,0.434)},{15,QPointF(0.235,0.387)},{20,QPointF(0.258,0.338)},{25,QPointF(0.303,0.313)},{30,QPointF(0.321,0.272)},{35,QPointF(0.361,0.257)},{40,QPointF(0.381,0.223)},{50,QPointF(0.438,0.181)},{60,QPointF(0.483,0.155)},{70,QPointF(0.532,0.13)},{80,QPointF(0.577,0.111)},{90,QPointF(0.619,0.098)},{100,QPointF(0.662,0.083)}};
// From Thetis MeterManager.cs:24237-24255 [v2.10.3.15]
const QMap<double,QPointF> kReflected{{0,QPointF(0.948,0.74)},{0.25,QPointF(0.913,0.7)},{0.5,QPointF(0.899,0.638)},{0.75,QPointF(0.875,0.594)},{1,QPointF(0.854,0.538)},{2,QPointF(0.814,0.443)},{3,QPointF(0.769,0.4)},{4,QPointF(0.744,0.351)},{5,QPointF(0.702,0.321)},{6,QPointF(0.682,0.285)},{7,QPointF(0.646,0.268)},{8,QPointF(0.626,0.234)},{9,QPointF(0.596,0.228)},{10,QPointF(0.569,0.196)},{12,QPointF(0.524,0.166)},{14,QPointF(0.476,0.14)},{16,QPointF(0.431,0.121)},{18,QPointF(0.393,0.109)},{20,QPointF(0.349,0.098)}};

QJsonObject channelConfig(double attack,double release,int history,bool show,const QColor& color,const QColor& range,QPointF offset={},QPointF radius={1,1},double length=1) {
    return {{"attack",attack},{"decay",release},{"updateIntervalMs",100},{"historyMs",history},{"ignoreHistoryMs",2000},
            {"showHistory",show},{"peakHold",false},{"visible",true},{"color",color.name(QColor::HexArgb)},
            {"historyColor",range.name(QColor::HexArgb)},{"offsetX",offset.x()},{"offsetY",offset.y()},
            {"radiusX",radius.x()},{"radiusY",radius.y()},{"lengthFactor",length},{"strokeWidth",2.5},
            {"shadow",true},{"onlyWhenRx",false},{"onlyWhenTx",false},{"displayGroup",0},{"normalisePower",false},{"counterClockwise",false}};
}
QColor color(const QJsonObject& c,const char* key) { return QColor(c.value(key).toString()); }
struct AnanFaceGeometry { QRectF skin,window,needles,readout,selector; double scale,readoutFont; int columns; double nameHeight,valueHeight,peakHeight; };
QRectF fitAspect(const QRectF& box,double aspect) {
    const double width=qMax(0.0,qMin(box.width(),box.height()/aspect));
    return {box.center().x()-width/2,box.center().y()-width*aspect/2,width,width*aspect};
}
AnanFaceGeometry ananGeometry(const QRectF& outer,int fontSize,bool showReadout,bool showPeak,int readoutCount,double requestedCellWidth) {
    int columns=qMin(qMax(1,readoutCount),outer.width()<420?2:outer.width()<600?3:4);
    const double header=qMin(qMax(14.0,22*outer.width()/720),outer.height()*.2);
    double readoutFont=qMax(12.0,fontSize*outer.width()/720),readoutHeight=0;
    double nameHeight=0,valueHeight=0,peakHeight=0;
    for(int pass=0;pass<5;++pass) {
        QFont font=QGuiApplication::font(); font.setBold(true); font.setItalic(false); font.setPixelSize(qMax(12,qRound(readoutFont)));
        QFont heading=font; heading.setPixelSize(qMax(11,qRound(readoutFont*.75)));
        QFont peak=font; peak.setPixelSize(qMax(11,qRound(readoutFont*.7)));
        nameHeight=QFontMetricsF(heading).height(); valueHeight=QFontMetricsF(font).height();
        peakHeight=showPeak?QFontMetricsF(peak).height():0;
        const double cell=nameHeight+valueHeight+peakHeight+4;
        const double maximum=qMax(0.0,outer.height()-header-12);
        if(showReadout && pass==0) {
            // Reserve the configured role text at its requested font size.
            // Reflow only if the added rows fit; live samples cannot change
            // this stable width budget or make the face jump between layouts.
            while(columns>1 && outer.width()/columns<requestedCellWidth &&
                ((readoutCount+columns-2)/(columns-1))*cell<=maximum) {
                --columns;
            }
        }
        readoutHeight=showReadout?((readoutCount+columns-1)/columns)*cell:0;
        if(readoutHeight<=maximum) { break; }
        // A short allocation gets additional columns before any proportional
        // reduction. The ordinary360px minimum keeps its two-column layout.
        const int maximumColumns=qMin(readoutCount,qMax(columns,int(outer.width()/90)));
        if(columns<maximumColumns) { ++columns; continue; }
        const double reduced=qMax(12.0,readoutFont*maximum/qMax(1.0,readoutHeight));
        if(reduced==readoutFont) { break; }
        readoutFont=reduced;
    }
    // Allocations unable to hold the minimum text are below the configured
    // face minimum. Keep fonts readable; never shrink them invisibly to fit.
    readoutHeight=qMin(readoutHeight,qMax(0.0,outer.height()-header-1));
    const QRectF skin=fitAspect(outer.adjusted(0,header,0,-readoutHeight),AnanFace::kHeight/AnanFace::kWidth);
    const double scale=skin.width()/AnanFace::kWidth;
    const QRectF window(skin.left()+84*scale,skin.top()+76*scale,1687*scale,678*scale);
    return {skin,window,skin,QRectF(outer.left(),outer.bottom()-readoutHeight,outer.width(),readoutHeight),
        QRectF(outer.right()-qMin(outer.width()*.48,190*outer.width()/720),outer.top(),qMin(outer.width()*.48,186*outer.width()/720),header),scale,readoutFont,columns,nameHeight,valueHeight,peakHeight};
}
QColor onDarkGlass(QColor requested) {
    // Contrast adaptation changes display only: alpha and stored color survive.
    if(requested.lightnessF()<.38) { return QColor(224,231,237,requested.alpha()); }
    return requested;
}
}
CompositePresetItem::CompositePresetItem(Face face,QObject* parent):MeterItem(parent),m_face(face) { initialise(); }
void CompositePresetItem::initialise() {
    static const QStringList kinds{"PowerSwrPreset","AnanMM","CrossNeedle","MagicEyePreset","SignalTextPreset","HistoryGraphPreset","VfoDisplayPreset","ClockPreset","ContestPreset"};
    m_kind=kinds[int(m_face)];
    const QStringList titles{"Power / SWR","ANAN multi meter","Cross needle","Magic eye","Signal","Signal history","VFO display","Clock","Contest controls"};
    m_config={{"kind",m_kind},{"schema",1},{"title",titles[int(m_face)]},{"showTitle",true},{"titleColor","#ffa9a9a9"},
              {"backdropColor","#ff202020"},{"lowColor","#ffc8d8e8"},{"highColor","#ffff4444"},{"showReadout",true},
              {"showPeakValue",true},{"fontSize",18},{"faceHeight",m_face==Face::PowerSwr ? 144 : m_face==Face::Anan ? 300 : m_face==Face::Cross ? 260 : m_face==Face::Contest ? 280 : m_face==Face::Eye ? 180 : 120},
              {"fadeRx",false},{"fadeTx",false},{"displayGroup",1},{"clockMode","Both"},{"showDate",true},{"show24Hour",true},
              {"historyCapacity",300},{"historyMs",60000},{"autoScale",true},{"minValue",-140},{"maxValue",0},{"units","dBm"}};
    const auto add=[&](QString name,int binding,QString units,QMap<double,QPointF> calibration,QJsonObject c) {
        c["bindingId"]=binding; c["name"]=name; c["units"]=units; m_channels.append({name,units,binding,calibration,{},c});
    };
    if(m_face==Face::PowerSwr) {
        // From Thetis MeterManager.cs:25198-25236,25326-25338 [v2.10.3.15]
        QJsonObject power=channelConfig(.8,.1,2000,true,Qt::yellow,QColor(255,0,0,128)); power["normalisePower"]=true;
        add("Power",MeterBinding::TxPower,"W",{{0,{0,0}},{5,{.1875,0}},{10,{.375,0}},{50,{.5625,0}},{100,{.75,0}},{120,{.99,0}}},power);
        add("SWR",MeterBinding::TxSwr,"",{{1,{0,0}},{1.5,{.25,0}},{2,{.5,0}},{3,{.75,0}},{5,{.99,0}}},channelConfig(.8,.1,2000,true,Qt::yellow,QColor(255,165,0,128)));
    } else if(m_face==Face::Cross) {
        // From Thetis MeterManager.cs:24151-24165,24221-24236 [v2.10.3.15]
        // image x to y ratio
        //0.325f;
        //0.5f;
        QJsonObject forward=channelConfig(.2,.1,4000,true,Qt::black,QColor(255,0,0,96),{.322,.611},{1,1},1.62); forward["normalisePower"]=true;
        QJsonObject reverse=channelConfig(.2,.1,4000,true,Qt::black,QColor(100,149,237,96),{-.322,.611},{1,1},1.62); reverse["normalisePower"]=true; reverse["counterClockwise"]=true;
        add("Forward",MeterBinding::TxPower,"W",kForward,forward); add("Reflected",MeterBinding::TxReversePower,"W",kReflected,reverse);
        m_config["backdropColor"]="#fff1eee2"; m_config["lowColor"]="#ff203040";
    } else if(m_face==Face::Anan) {
        // From Thetis MeterManager.cs:23797-24080 [v2.10.3.15] — separate source dynamics and display groups.
        //0.1f;
        // 0.05f;
        //0.325f;
        //0.5f;
        // alc_comp
        const QPointF offset(.004,.736), radius(1,.58);
        const auto needle=[&](double attack,double decay,int history,bool show,QColor c,double length,int group,bool rx,bool tx) {
            QJsonObject n=channelConfig(attack,decay,history,show,c,QColor(c.red(),c.green(),c.blue(),64),offset,radius,length);
            n["displayGroup"]=group; n["onlyWhenRx"]=rx; n["onlyWhenTx"]=tx; return n;
        };
        add("Signal",MeterBinding::SignalAvg,"dBm",kSignal,needle(.8,.2,4000,true,{233,51,50},1.65,0,true,false));
        add("Volts",MeterBinding::HwVolts,"V",kVolts,needle(.2,.2,500,false,Qt::black,.75,0,false,false));
        add("Amps",MeterBinding::HwAmps,"A",kAmps,needle(.2,.2,500,false,Qt::black,1.15,4,false,true));
        QJsonObject power=needle(.2,.1,4000,true,{233,51,50},1.55,1,false,true); power["normalisePower"]=true;
        add("Power",MeterBinding::TxPower,"W",kPower,power);
        QJsonObject swr=needle(.2,.1,4000,true,Qt::black,1.36,1,false,true); swr["historyColor"]="#406495ed";
        add("SWR",MeterBinding::TxSwr,"",kSwr,swr);
        add("Compression",MeterBinding::TxAlcGain,"dB",kCompression,needle(.2,.1,4000,false,Qt::black,.96,2,false,true));
        add("ALC group",MeterBinding::TxAlcGroup,"dB",kAlcGroup,needle(.2,.1,4000,false,Qt::black,.75,3,false,true));
        m_config["backdropColor"]="#fff1eee2"; m_config["lowColor"]="#ff203040";
    } else if(m_face==Face::Eye || m_face==Face::SignalText || m_face==Face::History) {
        // From Thetis MeterManager.cs:23572-23618,23018-23031 [v2.10.3.15]
        const bool eye=m_face==Face::Eye;
        add("Signal",MeterBinding::SignalAvg,"dBm",eye ? QMap<double,QPointF>{{-127,{0,0}},{-73,{.85,0}},{-13,{1,0}}} : QMap<double,QPointF>{{-133,{0,0}},{-73,{.5,0}},{-13,{.99,0}}},channelConfig(eye?.2:.8,eye?.05:.2,4000,!eye,eye?QColor(Qt::green):QColor(Qt::yellow),QColor(255,0,0,128)));
        if(m_face==Face::SignalText) { m_config["fontSize"]=56; }
    }
    if(m_face==Face::Vfo || m_face==Face::Contest) {
        m_vfo=new VfoDisplayItem(this); m_vfo->setModeLabel({}); m_vfo->setBandLabel({}); m_vfo->setFilterLabel({}); m_vfo->setFrequency(0); m_vfo->setUnavailableText("No live slice reading");
    }
    if(m_face==Face::Contest) {
        m_bands=new BandButtonItem(this); m_bands->setColumns(5); m_modes=new ModeButtonItem(this); m_modes->setColumns(5);
        m_bands->setAllButtonsAvailable(false,"No live slice is attached"); m_modes->setAllButtonsAvailable(false,"No live slice is attached");
    }
    if(m_face==Face::Clock || m_face==Face::Contest) { m_clock=new ClockItem(this,false); }
    setBindingId(m_channels.isEmpty() ? -1 : m_channels.first().binding); configureDynamics();
}
QString CompositePresetItem::typeId() const {
    static const QStringList ids{"meter.powerSwr","meter.ananMulti","meter.crossNeedle","meter.magicEye","meter.signalText","meter.historyGraph","meter.vfoDisplay","meter.clock","meter.contest"}; return ids[int(m_face)];
}
void CompositePresetItem::configureDynamics() {
    for(Channel& channel:m_channels) { const QJsonObject& c=channel.config; channel.dynamics.configure(c["attack"].toDouble(),c["decay"].toDouble(),c["updateIntervalMs"].toInt(),c["historyMs"].toInt(),c["ignoreHistoryMs"].toInt()); channel.dynamics.reset(channel.calibration.firstKey()-(m_aboveS9 && isReceiveSignalBinding(channel.binding)?20:0)); }
    m_samples.clear(); m_lastFrame=-1; m_lastHistorySample=-1;
}
QSet<int> CompositePresetItem::readingBindings() const { QSet<int> result; for(int i=0;i<m_channels.size();++i) { const int id=i==0 ? bindingId() : m_channels[i].binding; if(id>=0) { result.insert(id); } } return result; }
void CompositePresetItem::pushBindingValue(int binding,double reading) {
    const bool available=bindingSupport(binding)!=BindingSupport::Unsupported && bindingUnavailableReason(binding).isEmpty() && std::isfinite(reading) && (hasMmioBinding() || !isNoMeterReading(reading));
    for(int i=0;i<m_channels.size();++i) {
        if(binding==(i==0?bindingId():m_channels[i].binding)) {
            const bool hadReading=m_channels[i].dynamics.hasReading();
            m_channels[i].dynamics.push(reading,available);
            if(m_face==Face::Anan && bindingSupport(binding)==BindingSupport::Unknown && hadReading!=m_channels[i].dynamics.hasReading()) { markPresentationDirty(true); }
            if(i==0) { m_value=reading; }
        }
    }
}
void CompositePresetItem::setBindingUnavailable(int binding,const QString& reason) {
    MeterItem::setBindingUnavailable(binding,reason);
    if(!reason.isEmpty()) {
        for(int i=0;i<m_channels.size();++i) {
            if(binding==(i==0?bindingId():m_channels[i].binding)) {
                const bool hadReading=m_channels[i].dynamics.hasReading();
                m_channels[i].dynamics.push(0,false);
                if(m_face==Face::Anan && bindingSupport(binding)==BindingSupport::Unknown && hadReading) { markPresentationDirty(true); }
            }
        }
    }
}
void CompositePresetItem::setBindingSupport(int binding,BindingSupport support) {
    if(bindingSupport(binding)==support) { return; }
    MeterItem::setBindingSupport(binding,support);
    for(int i=0;i<m_channels.size();++i) {
        if(binding==(i==0?bindingId():m_channels[i].binding)) { m_channels[i].dynamics.push(0,false); }
    }
    markPresentationDirty(true);
}
bool CompositePresetItem::advanceMeter(qint64 now) {
    bool changed=false;
    // A restarted presentation clock cannot retain samples from its future.
    // Channel dynamics keep their independent sampling/reset policy.
    if(m_face==Face::History && m_lastFrame>=0 && now<m_lastFrame) {
        changed=!m_samples.isEmpty(); m_samples.clear(); m_lastHistorySample=-1;
    }
    const auto oldSize=m_samples.size(); m_samples.removeIf([&](const Sample& sample) { return now-sample.time>=m_config["historyMs"].toInt(); }); changed=(m_samples.size()!=oldSize) || changed;
    for(Channel& channel:m_channels) { changed=channel.dynamics.advance(now) || changed; }
    if(m_face==Face::History && !m_channels.isEmpty() && m_channels[0].dynamics.hasReading() && (m_lastHistorySample<0 || now-m_lastHistorySample>=m_channels[0].config["updateIntervalMs"].toInt())) {
        m_samples.append({now,m_channels[0].dynamics.value()}); m_lastHistorySample=now; const int duration=m_config["historyMs"].toInt(); m_samples.removeIf([&](const Sample& s) { return now-s.time>=duration; });
        const int extra=m_samples.size()-m_config["historyCapacity"].toInt(); if(extra>0) { m_samples.remove(0,extra); } changed=true;
    }
    const bool clock=m_clock && (m_lastFrame<0 || now/250!=m_lastFrame/250); m_lastFrame=now; const bool dirty=m_presentationDirty; m_presentationDirty=false; return changed || clock || dirty;
}
void CompositePresetItem::resetForTxTransition(bool tx) { m_tx=tx; configureDynamics(); if(m_face==Face::Anan) { markPresentationDirty(true); } if(m_vfo) { m_vfo->setTransmitting(tx); } if(m_bands) { m_bands->setTransmitting(tx); m_modes->setTransmitting(tx); } }
void CompositePresetItem::setPowerScale(int watts) { if(watts>0 && watts!=m_powerScale) { m_powerScale=watts; markPresentationDirty(true); } }
void CompositePresetItem::setFrequency(qint64 hz) { if(m_vfo && m_vfo->frequency()!=hz) { markPresentationDirty(); } if(m_vfo) { m_vfo->setFrequency(hz); m_vfo->setUnavailableText(hz>0?QString():QStringLiteral("No live slice reading")); } }
void CompositePresetItem::setModeLabel(const QString& text) { if(m_stateMode!=text) { m_stateMode=text; markPresentationDirty(); } if(m_vfo) { m_vfo->setModeLabel(text); } }
void CompositePresetItem::setBandLabel(const QString& text) { if(m_stateBand!=text) { m_stateBand=text; markPresentationDirty(); } if(m_vfo) { m_vfo->setBandLabel(text); } }
void CompositePresetItem::setUnavailableText(const QString& text) { if(m_stateUnavailable!=text) { m_stateUnavailable=text; markPresentationDirty(); } if(m_vfo) { m_vfo->setUnavailableText(text); } if(m_bands) { m_bands->setAllButtonsAvailable(text.isEmpty()&&!m_inert,text); m_modes->setAllButtonsAvailable(text.isEmpty()&&!m_inert,text); } }
QVector<MeterItem*> CompositePresetItem::internalItems() const { QVector<MeterItem*> result; for(auto* child:{static_cast<MeterItem*>(m_vfo),static_cast<MeterItem*>(m_bands),static_cast<MeterItem*>(m_modes),static_cast<MeterItem*>(m_clock)}) { if(child) { result.append(child); } } return result; }
void CompositePresetItem::setPreviewInert(bool inert) { m_inert=inert; for(MeterItem* child:internalItems()) { child->blockSignals(inert); } if(m_bands && inert) { m_bands->setAllButtonsAvailable(false,"Preview controls are inactive"); m_modes->setAllButtonsAvailable(false,"Preview controls are inactive"); } }
int CompositePresetItem::preferredFaceHeight() const {
    int height=m_config["faceHeight"].toInt();
    if(m_face==Face::SignalText) {
        // NereusSDR stack presentation reserves separate lines without rewriting
        // the imported face height or the user's chosen font.
        QFont font; font.setPixelSize(m_config["fontSize"].toInt());
        QFont peakFont; peakFont.setPixelSize(14);
        const int lines=int(m_config["showTitle"].toBool())+int(m_config["showReadout"].toBool());
        height=qMax(height,lines*QFontMetrics(font).height()+
            (m_config["showPeakValue"].toBool()?QFontMetrics(peakFont).height():0)+12);
    }
    return height;
}
QSize CompositePresetItem::minimumFaceSize() const {
    int width=m_face==Face::Anan || m_face==Face::Cross || m_face==Face::Contest?360:260;
    if(m_face==Face::SignalText) {
        QFont font; font.setPixelSize(m_config["fontSize"].toInt()); const QFontMetrics metrics(font);
        if(m_config["showTitle"].toBool()) { width=qMax(width,metrics.horizontalAdvance(m_config["title"].toString())+8); }
        if(m_config["showReadout"].toBool()) {
            for(double value:{m_config["minValue"].toDouble(),m_config["maxValue"].toDouble()}) { width=qMax(width,metrics.horizontalAdvance(signalReadout(value))+16); }
        }
    }
    return {width,preferredFaceHeight()};
}
double CompositePresetItem::channelValue(int i) const { return m_channels.value(i).dynamics.value(); }
double CompositePresetItem::channelPeak(int i) const { return m_channels.value(i).dynamics.maxHistory(); }
bool CompositePresetItem::channelHasReading(int i) const { return i>=0 && i<m_channels.size() && m_channels[i].dynamics.hasReading(); }
QString CompositePresetItem::channelUnits(int i) const { return m_channels.value(i).units; }
bool CompositePresetItem::channelVisible(int i) const {
    if(i<0 || i>=m_channels.size()) { return false; } const QJsonObject& c=m_channels[i].config;
    const int group=c["displayGroup"].toInt(); return c["visible"].toBool() && (!c["onlyWhenRx"].toBool() || !m_tx) && (!c["onlyWhenTx"].toBool() || m_tx) && (group==0 || m_config["displayGroup"].toInt()==0 || group==m_config["displayGroup"].toInt());
}
QPointF CompositePresetItem::calibratedPoint(int i,double value) const {
    if(i<0 || i>=m_channels.size()) { return {}; } const Channel& channel=m_channels[i]; const QJsonObject& c=channel.config;
    // From Thetis MeterManager.cs:41048-41118 [v2.10.3.15] — round raw values before source transforms.
    //[2.10.3.9]MW0LGE refactor for speed
    value=std::round(value*100)/100;
    if(m_aboveS9 && (channel.binding==MeterBinding::SignalPeak || channel.binding==MeterBinding::SignalAvg)) { value+=20; }
    if(c["normalisePower"].toBool()) { value*=100.0/m_powerScale; }
    const auto& cal=channel.calibration; if(!std::isfinite(value)) { return cal.first(); } if(value<=cal.firstKey()) { return cal.first(); } if(value>=cal.lastKey()) { return cal.last(); }
    auto high=cal.upperBound(value), low=high; --low; const double t=(value-low.key())/(high.key()-low.key()); return low.value()+(high.value()-low.value())*t;
}
QPointF CompositePresetItem::needlePivot(int i,const QRectF& r) const {
    const QJsonObject c=m_channels.value(i).config;
    if(m_face==Face::Anan) { return AnanFace::toWidget(AnanFace::origin(i,c),r); }
    return r.center()+QPointF(r.width()*c["offsetX"].toDouble(),r.height()*c["offsetY"].toDouble());
}
QPointF CompositePresetItem::needleTip(int i,double value,const QRectF& r) const {
    const QJsonObject c=m_channels.value(i).config; const QPointF target=calibratedPoint(i,value); const QPointF pivot=needlePivot(i,r);
    if(m_face==Face::Anan && i>=0 && i<m_channels.size()) {
        const auto& calibration=m_channels[i].calibration;
        const double angle=AnanFace::angle(i,target,calibration.first(),calibration.last());
        return AnanFace::toWidget(AnanFace::atAngle(i,c,angle),r);
    }
    // From Thetis MeterManager.cs:40754-40763,40865-40880 [v2.10.3.15]
    // needle offset from centre
    // calc angle required
    // expand
    const double dx=(pivot.x()-r.left()-target.x()*r.width())/c["radiusX"].toDouble(),dy=(pivot.y()-r.top()-target.y()*r.height())/c["radiusY"].toDouble();
    const double angle=std::atan2(dy,dx)+M_PI; const double radius=r.width()/2*c["lengthFactor"].toDouble();
    return pivot+QPointF(std::cos(angle)*radius*c["radiusX"].toDouble(),std::sin(angle)*radius*c["radiusY"].toDouble());
}
bool CompositePresetItem::ananChannelIncluded(int i) const {
    const int binding=i==0?bindingId():m_channels[i].binding;
    return m_channels[i].config["visible"].toBool() && bindingSupport(binding)!=BindingSupport::Unsupported;
}
int CompositePresetItem::ananReadoutCount() const {
    int count=0;
    for(int i=0;i<m_channels.size();++i) { if(ananChannelIncluded(i) && channelVisible(i)) { ++count; } }
    return count;
}
double CompositePresetItem::ananReadoutCellWidth(double outerWidth) const {
    const double pixels=qMax(12.0,m_config["fontSize"].toDouble()*outerWidth/720);
    QFont value=QGuiApplication::font(); value.setBold(true); value.setItalic(false); value.setPixelSize(qMax(12,qRound(pixels)));
    QFont heading=value; heading.setPixelSize(qMax(11,qRound(pixels*.75)));
    QFont peak=value; peak.setPixelSize(qMax(11,qRound(pixels*.7)));
    const QFontMetricsF values(value),headings(heading),peaks(peak);
    double width=0;
    for(int i=0;i<m_channels.size();++i) {
        if(!ananChannelIncluded(i) || !channelVisible(i)) { continue; }
        const Channel& channel=m_channels[i];
        width=qMax(width,headings.horizontalAdvance(channel.name));
        width=qMax(width,values.horizontalAdvance(QStringLiteral("-- ")+channel.units));
        if(m_config["showPeakValue"].toBool()) { width=qMax(width,peaks.horizontalAdvance(QStringLiteral("Peak --"))); }
        for(double endpoint:{channel.calibration.firstKey(),channel.calibration.lastKey()}) {
            const QString reading=i==0 && channel.units==QStringLiteral("dBm")?signalReadout(endpoint):
                QString::number(endpoint,'f',1)+" "+channel.units;
            width=qMax(width,values.horizontalAdvance(reading));
            if(m_config["showPeakValue"].toBool()) {
                width=qMax(width,peaks.horizontalAdvance(QStringLiteral("Peak %1").arg(endpoint,0,'f',1)));
            }
        }
    }
    return std::ceil(width)+6;
}
QRectF CompositePresetItem::ananNeedleRect(int width,int height) const {
    return ananGeometry(pixelRect(width,height),m_config["fontSize"].toInt(),m_config["showReadout"].toBool(),m_config["showPeakValue"].toBool(),ananReadoutCount(),ananReadoutCellWidth(pixelRect(width,height).width())).needles;
}
QRectF CompositePresetItem::ananGroupControlRect(int width,int height) const {
    return ananGeometry(pixelRect(width,height),m_config["fontSize"].toInt(),m_config["showReadout"].toBool(),m_config["showPeakValue"].toBool(),ananReadoutCount(),ananReadoutCellWidth(pixelRect(width,height).width())).selector;
}
QString CompositePresetItem::signalReadout(double value) const { return formatSignalReading(value,m_unitMode,m_aboveS9,m_showDecimal); }
QString CompositePresetItem::formatSignalReading(double value,MeterUnit unit,bool aboveS9,bool decimal) {
    if(unit==MeterUnit::dBm) { return QString::number(value,'f',decimal?1:0)+" dBm"; }
    if(unit==MeterUnit::uV) { return QString::number(std::sqrt(50*std::pow(10.0,(value-30)/10))*1e6,'f',1)+QStringLiteral(" µV"); }
    // From Thetis Common.cs:888-934 [v2.10.3.15] — signal-text S and above-S9 buckets.
    // version that returns via out parameters the S reading, and the dbm over reading
    // Adjacent upstream attribution: the DarkMode region follows this helper.
    //MW0LGE [2.9.0.8]
    const double corrected=value+(aboveS9?20:0);
    const QList<double> thresholds{-124,-118,-112,-106,-100,-94,-88,-82,-76,-70,-66,-60,-56,-46,-36,-26,-16};
    const QList<int> over{0,0,0,0,0,0,0,0,0,0,5,10,15,20,30,40,50};
    for(int i=0;i<thresholds.size();++i) { if(corrected<=thresholds[i]) { return i<10?QStringLiteral("S%1").arg(i):QStringLiteral("S9+%1").arg(over[i]); } }
    return QStringLiteral("S9+60");
}
int CompositePresetItem::minimumConfiguredFaceHeight() const {
    const QList<int> minimumHeights{144,300,260,180,120,120,120,120,280};
    return minimumHeights[int(m_face)];
}
QStringList CompositePresetItem::editableChannelFields(int channel) const {
    if(channel<0 || channel>=m_channels.size()) { return {}; }
    QStringList fields{"bindingId","attack","decay","updateIntervalMs","ignoreHistoryMs","color"};
    if(m_face==Face::PowerSwr || m_face==Face::Anan || m_face==Face::Cross || m_face==Face::SignalText) { fields.append({"historyMs","showHistory","peakHold","historyColor"}); }
    if(m_face==Face::PowerSwr || m_face==Face::Anan || m_face==Face::Cross) { fields.append({"name","units","visible"}); }
    if(m_face==Face::Anan || m_face==Face::Cross) { fields.append({"shadow","offsetX","offsetY","radiusX","radiusY","lengthFactor","strokeWidth","onlyWhenRx","onlyWhenTx","displayGroup"}); }
    return fields;
}
QStringList CompositePresetItem::editableFields() const {
    QStringList fields{"x","y","w","h","faceHeight","fadeRx","fadeTx"};
    if(m_face!=Face::Vfo) { fields.append({"backdropColor","showTitle","fontSize"}); }
    if(m_face!=Face::Vfo && m_face!=Face::Contest && m_face!=Face::Clock) { fields.append({"titleColor","lowColor","showReadout","channels"}); }
    if(m_face==Face::SignalText || m_face==Face::Eye || m_face==Face::History) { fields.append("title"); }
    if(m_face==Face::PowerSwr || m_face==Face::Anan || m_face==Face::Cross || m_face==Face::SignalText) { fields.append("showPeakValue"); }
    if(m_face==Face::PowerSwr || m_face==Face::Anan || m_face==Face::Cross) { fields.append("highColor"); }
    if(m_face==Face::Anan) { fields.append("displayGroup"); }
    if(m_face==Face::History) { fields.append({"historyCapacity","historyMs","minValue","maxValue","autoScale","units"}); }
    if(m_clock) { fields.append({"clockMode","showDate","show24Hour","clockTimeColor","clockDateColor","clockTitleColor"}); }
    if(m_vfo) { fields.append({"vfo","vfoFrequencyColor","vfoModeColor","vfoFilterColor","vfoBandColor"}); }
    if(m_bands) { fields.append({"bands","modes"}); }
    if(!m_channels.isEmpty()) { fields.append("bindingId"); }
    return fields;
}
QJsonObject CompositePresetItem::configuration() const {
    QJsonObject c=m_config;
    const QStringList fields=editableFields();
    const QStringList known{"title","showTitle","titleColor","backdropColor","lowColor","highColor","showReadout","showPeakValue","fontSize","displayGroup","clockMode","showDate","show24Hour","historyCapacity","historyMs","autoScale","minValue","maxValue","units"};
    for(const QString& key:known) { if(!fields.contains(key)) { c.remove(key); } }
    if(m_vfo) { c["vfo"]=m_vfo->serialize(); c["vfoFrequencyColor"]=m_vfo->frequencyColour().name(QColor::HexArgb); c["vfoModeColor"]=m_vfo->modeColour().name(QColor::HexArgb); c["vfoFilterColor"]=m_vfo->filterColour().name(QColor::HexArgb); c["vfoBandColor"]=m_vfo->bandColour().name(QColor::HexArgb); }
    if(m_bands) { c["bands"]=m_bands->serialize(); c["modes"]=m_modes->serialize(); }
    if(m_clock) { c["clockTimeColor"]=m_clock->timeColour().name(QColor::HexArgb); c["clockDateColor"]=m_clock->dateColour().name(QColor::HexArgb); c["clockTitleColor"]=m_clock->typeTitleColour().name(QColor::HexArgb); }
    c["x"]=x(); c["y"]=y(); c["w"]=itemWidth(); c["h"]=itemHeight(); c["bindingId"]=bindingId();
    QJsonArray channels; for(int i=0;i<m_channels.size();++i) { QJsonObject channel=m_channels[i].config; if(i==0) { channel["bindingId"]=bindingId(); } channels.append(channel); } c["channels"]=channels; return c;
}
QString CompositePresetItem::serialize() const { return QString::fromUtf8(QJsonDocument(configuration()).toJson(QJsonDocument::Compact)); }
bool CompositePresetItem::deserialize(const QString& data) { const QJsonDocument json=QJsonDocument::fromJson(data.toUtf8()); return json.isObject() && json.object()["kind"]==m_kind && json.object()["schema"]==1 && applyConfiguration(json.object()); }
bool CompositePresetItem::applyConfiguration(const QJsonObject& edit) {
    QJsonObject next=m_config; const QJsonObject current=configuration(); for(auto i=current.begin();i!=current.end();++i) { next[i.key()]=i.value(); } for(auto i=edit.begin();i!=edit.end();++i) { next[i.key()]=i.value(); }
    if(edit.contains("channels") && !edit.contains("bindingId") && !edit["channels"].toArray().isEmpty()) { next["bindingId"]=edit["channels"].toArray().first().toObject()["bindingId"]; }
    const QStringList editable=editableFields();
    for(auto i=edit.begin();i!=edit.end();++i) { if(m_config.contains(i.key()) && !editable.contains(i.key()) && i.key()!="schema" && i.key()!="kind" && i.value()!=m_config[i.key()]) { return false; } }
    if(next["schema"]!=1 || next["kind"]!=m_kind) { return false; }
    for(const QString& key:{QStringLiteral("x"),QStringLiteral("y"),QStringLiteral("w"),QStringLiteral("h"),QStringLiteral("minValue"),QStringLiteral("maxValue")}) { if(!next[key].isDouble() || !std::isfinite(next[key].toDouble()) || qAbs(next[key].toDouble())>std::numeric_limits<float>::max()) { return false; } }
    if(next["w"].toDouble()<=0 || next["h"].toDouble()<=0 || next["maxValue"].toDouble()<=next["minValue"].toDouble()) { return false; }
    for(const QString& key:{QStringLiteral("fontSize"),QStringLiteral("faceHeight"),QStringLiteral("historyMs"),QStringLiteral("historyCapacity"),QStringLiteral("displayGroup"),QStringLiteral("bindingId")}) { double v=next[key].toDouble(-1.5); if(!next[key].isDouble() || v!=std::floor(v) || v<-1 || v>1000000) { return false; } }
    if(next["fontSize"].toInt()<8 || next["faceHeight"].toInt()<minimumConfiguredFaceHeight() || next["historyMs"].toInt()<100 || next["historyCapacity"].toInt()<2) { return false; }
    for(const QString& key:{QStringLiteral("backdropColor"),QStringLiteral("titleColor"),QStringLiteral("lowColor"),QStringLiteral("highColor")}) { if(!next[key].isString() || !QColor(next[key].toString()).isValid()) { return false; } }
    for(const QString& key:{QStringLiteral("showTitle"),QStringLiteral("showReadout"),QStringLiteral("showPeakValue"),QStringLiteral("fadeRx"),QStringLiteral("fadeTx"),QStringLiteral("autoScale"),QStringLiteral("showDate"),QStringLiteral("show24Hour")}) { if(!next[key].isBool()) { return false; } }
    if(!next["title"].isString() || !next["units"].isString() || !QStringList{"Both","UTC","Local"}.contains(next["clockMode"].toString())) { return false; }
    const QJsonArray channels=next["channels"].toArray(); if(channels.size()!=m_channels.size()) { return false; }
    QVector<QJsonObject> checked;
    for(int i=0;i<channels.size();++i) {
        if(!channels[i].isObject()) { return false; } QJsonObject c=m_channels[i].config; const auto supplied=channels[i].toObject(); const QStringList editableChannel=editableChannelFields(i);
        for(auto j=supplied.begin();j!=supplied.end();++j) { if(c.contains(j.key()) && !editableChannel.contains(j.key()) && j.value()!=c[j.key()]) { return false; } c[j.key()]=j.value(); }
        for(const QString& key:{QStringLiteral("attack"),QStringLiteral("decay"),QStringLiteral("offsetX"),QStringLiteral("offsetY"),QStringLiteral("radiusX"),QStringLiteral("radiusY"),QStringLiteral("lengthFactor"),QStringLiteral("strokeWidth")}) { if(!c[key].isDouble() || !std::isfinite(c[key].toDouble())) { return false; } }
        if(c["attack"].toDouble()<0 || c["attack"].toDouble()>1 || c["decay"].toDouble()<0 || c["decay"].toDouble()>1 || c["radiusX"].toDouble()<=0 || c["radiusY"].toDouble()<=0 || c["lengthFactor"].toDouble()<=0 || c["strokeWidth"].toDouble()<=0) { return false; }
        for(const QString& key:{QStringLiteral("bindingId"),QStringLiteral("updateIntervalMs"),QStringLiteral("historyMs"),QStringLiteral("ignoreHistoryMs"),QStringLiteral("displayGroup")}) { const double v=c[key].toDouble(-1.5); if(!c[key].isDouble() || v!=std::floor(v) || v<-1 || v>1000000) { return false; } }
        if(c["updateIntervalMs"].toInt()<1 || c["historyMs"].toInt()<c["updateIntervalMs"].toInt() || c["ignoreHistoryMs"].toInt()<0) { return false; }
        for(const QString& key:{QStringLiteral("visible"),QStringLiteral("showHistory"),QStringLiteral("peakHold"),QStringLiteral("shadow"),QStringLiteral("onlyWhenRx"),QStringLiteral("onlyWhenTx"),QStringLiteral("normalisePower"),QStringLiteral("counterClockwise")}) { if(!c[key].isBool()) { return false; } }
        if(!QColor(c["color"].toString()).isValid() || !QColor(c["historyColor"].toString()).isValid() || !c["units"].isString() || !c["name"].isString()) { return false; } checked.append(c);
    }
    // Validate child records before mutating any live child.
    for(MeterItem* child:internalItems()) {
        const QString key=child==m_vfo ? "vfo" : child==m_bands ? "bands" : child==m_modes ? "modes" : "clock";
        if(next.contains(key)) { const QString raw=next[key].toString(); if(child==m_vfo) { VfoDisplayItem probe; if(!probe.deserialize(raw)) { return false; } } else if(child==m_bands) { BandButtonItem probe; if(!probe.deserialize(raw)) { return false; } } else if(child==m_modes) { ModeButtonItem probe; if(!probe.deserialize(raw)) { return false; } } else { ClockItem probe(nullptr,false); if(!probe.deserialize(raw)) { return false; } } }
    }
    for(const QString& key:{QStringLiteral("clockTimeColor"),QStringLiteral("clockDateColor"),QStringLiteral("clockTitleColor"),QStringLiteral("vfoFrequencyColor"),QStringLiteral("vfoModeColor"),QStringLiteral("vfoFilterColor"),QStringLiteral("vfoBandColor")}) { if(next.contains(key) && !QColor(next[key].toString()).isValid()) { return false; } }
    m_config=next; setRect(next["x"].toDouble(),next["y"].toDouble(),next["w"].toDouble(),next["h"].toDouble()); setBindingId(next["bindingId"].toInt());
    for(int i=0;i<checked.size();++i) { m_channels[i].config=checked[i]; if(i==0) { m_channels[i].config["bindingId"]=bindingId(); } m_channels[i].binding=i==0?bindingId():checked[i]["bindingId"].toInt(); m_channels[i].name=checked[i]["name"].toString(); m_channels[i].units=checked[i]["units"].toString(); }
    for(MeterItem* child:internalItems()) { const QString key=child==m_vfo?"vfo":child==m_bands?"bands":child==m_modes?"modes":"clock"; if(next.contains(key)) { child->deserialize(next[key].toString()); } }

    if(m_vfo) { m_vfo->setFrequencyColour(QColor(next["vfoFrequencyColor"].toString())); m_vfo->setModeColour(QColor(next["vfoModeColor"].toString())); m_vfo->setFilterColour(QColor(next["vfoFilterColor"].toString())); m_vfo->setBandColour(QColor(next["vfoBandColor"].toString())); }
    if(m_clock) { m_clock->setTimeColour(QColor(next["clockTimeColor"].toString())); m_clock->setDateColour(QColor(next["clockDateColor"].toString())); m_clock->setTypeTitleColour(QColor(next["clockTitleColor"].toString())); m_clock->setShow24Hour(next["show24Hour"].toBool()); m_clock->setShowType(next["showTitle"].toBool()); }
    configureDynamics(); markPresentationDirty(true); return true;
}
void CompositePresetItem::paintBar(QPainter& p,const QRectF& r,int index) {
    if(!m_channels[index].config["visible"].toBool()) { return; }
    const Channel& channel=m_channels[index]; const auto& c=channel.config;
    const double scale=objectTextScale(r,{260,72}),size=m_config["fontSize"].toInt()*scale;
    const double left=r.left()+18*scale,right=r.right()-18*scale,top=r.top()+28*scale,base=r.bottom()-14*scale;
    const auto pos=[&](double value) { return left+(right-left)*calibratedPoint(index,value).x(); };
    p.setPen(color(m_config,"titleColor")); if(m_config["showTitle"].toBool()) {
        const double start=m_config["showReadout"].toBool()?.32:0,end=m_config["showPeakValue"].toBool()?.68:1;
        drawObjectText(p,QRectF(left+(right-left)*start,r.top()+2*scale,(right-left)*(end-start),22*scale),channel.name,size);
    }
    if(c["showHistory"].toBool() && channel.dynamics.hasReading()) { p.fillRect(QRectF(pos(channel.dynamics.minHistory()),top,pos(channel.dynamics.maxHistory())-pos(channel.dynamics.minHistory()),base-top),color(c,"historyColor")); }
    p.setPen(QPen(color(m_config,"lowColor"),2)); p.drawLine(QPointF(left,base),QPointF(right,base));
    for(auto it=channel.calibration.begin();it!=channel.calibration.end();++it) {
        const double raw=c["normalisePower"].toBool() ? it.key()*m_powerScale/100.0 : it.key(); const double x=pos(raw);
        p.drawLine(QPointF(x,base),QPointF(x,base-10*scale)); QRectF label(x-22*scale,base-30*scale,44*scale,18*scale); if(it==channel.calibration.begin()) { label.moveLeft(left); } else if(it.key()==channel.calibration.lastKey()) { label.moveRight(right); }
        drawObjectText(p,label,QString::number(raw,'g',4),size);
    }
    if(channel.dynamics.hasReading()) {
        if(c["peakHold"].toBool()) { p.setPen(QPen(color(m_config,"highColor"),3)); p.drawLine(QPointF(pos(channel.dynamics.maxHistory()),top),QPointF(pos(channel.dynamics.maxHistory()),base)); }
        p.setPen(QPen(color(c,"color"),3)); p.drawLine(QPointF(pos(channel.dynamics.value()),top),QPointF(pos(channel.dynamics.value()),base));
    }
    const auto reading=[&](double value) { return QString::number(value,'f',1)+(channel.units.isEmpty()?QString():" "+channel.units); };
    p.setPen(color(c,"color")); if(m_config["showReadout"].toBool()) { drawObjectText(p,QRectF(left,r.top()+2*scale,(right-left)*.32-2*scale,22*scale),channel.dynamics.hasReading()?reading(channel.dynamics.value()):"--",size,Qt::AlignLeft|Qt::AlignVCenter); }
    p.setPen(color(m_config,"highColor")); if(m_config["showPeakValue"].toBool()) { drawObjectText(p,QRectF(left+(right-left)*.68+2*scale,r.top()+2*scale,(right-left)*.32-2*scale,22*scale),channel.dynamics.hasReading()?reading(channel.dynamics.maxHistory()):"--",size,Qt::AlignRight|Qt::AlignVCenter); }
}
void CompositePresetItem::paintNeedles(QPainter& p,const QRectF& outer,bool background) {
    if(m_face==Face::Anan) { paintAnan(p,outer,background); return; }
    const double scale=objectTextScale(outer,{360,260}),size=m_config["fontSize"].toInt()*scale;
    const double aspect=m_face==Face::Cross ? .782 : .512;
    const double width=qMax(0.0,qMin(outer.width()-24*scale,(outer.height()-73*scale)/aspect));
    const QRectF r(outer.center().x()-width/2,outer.top()+25*scale,width,width*aspect);
    QFont font=p.font();
    if(background) {
        p.setPen(color(m_config,"titleColor")); if(m_config["showTitle"].toBool()) { drawObjectText(p,QRectF(outer.left()+4*scale,outer.top()+2*scale,outer.width()-8*scale,22*scale),m_config["title"].toString(),size); }
    }
    QPainterPath marks;
    if(background) {
        for(int i=0;i<m_channels.size();++i) {
            if(!channelVisible(i)) {continue;}
            const Channel& channel=m_channels[i];QPainterPath arc;bool first=true;
            const QPointF pivot=needlePivot(i,r);
            for(auto it=channel.calibration.cbegin();it!=channel.calibration.cend();++it) {
                const double raw=channel.config["normalisePower"].toBool()?it.key()*m_powerScale/100.0:it.key();
                const QPointF tip=needleTip(i,raw,r),delta=tip-pivot;
                const QPointF direction=delta/qMax(.001,std::hypot(delta.x(),delta.y()));
                if(first) {arc.moveTo(tip);first=false;} else {arc.lineTo(tip);}
                marks.moveTo(tip);marks.lineTo(tip-direction*(7*scale));
            }
            marks.addPath(arc);
        }
    }
    QPainterPathStroker stroker;stroker.setWidth(3*scale);
    const QPainterPath markInk=stroker.createStroke(marks);
    QVector<QRectF> labelRegions;
    const QRectF body(outer.left(),outer.top()+25*scale,outer.width(),outer.height()-69*scale);
    for(int i=0;i<m_channels.size();++i) {
        if(!channelVisible(i)) { continue; } const Channel& channel=m_channels[i]; const QJsonObject& c=channel.config;
        const QPointF pivot=needlePivot(i,r); const double stroke=c["strokeWidth"].toDouble()*std::hypot(r.width(),r.height())/450;
        if(background) {
            QPainterPath arc; int n=0;
            for(auto it=channel.calibration.begin();it!=channel.calibration.end();++it,++n) {
                const double raw=c["normalisePower"].toBool()?it.key()*m_powerScale/100.0:it.key()-(m_aboveS9 && (channel.binding==MeterBinding::SignalPeak || channel.binding==MeterBinding::SignalAvg)?20:0);
                const QPointF tip=needleTip(i,raw,r); if(n==0) { arc.moveTo(tip); } else { arc.lineTo(tip); }
                const QPointF direction=(tip-pivot)/std::hypot(tip.x()-pivot.x(),tip.y()-pivot.y());
                p.setPen(QPen(color(m_config,"lowColor"),1.5)); p.drawLine(tip,tip-direction*(7*scale));
                // Sparse labels keep all supported minimum sizes readable.
                if(n==0 || n==channel.calibration.size()-1 || (channel.calibration.size()>3 && (i==0 && m_face==Face::Anan ? it.key()==-73 : c["normalisePower"].toBool() ? it.key()==50 : n==channel.calibration.size()/2))) {
                    const QPointF label=tip+direction*((m_face==Face::Anan && i==4 ? -20 : 13)*scale);
                    QRectF box(label.x()-25*scale,label.y()-10*scale,50*scale,20*scale);
                    const QString value=QString::number(raw,'g',4);
                    const QFont finalFont=fitObjectText(p.font(),size,value,box,false,Qt::TextSingleLine,p.device());
                    const QFontMetricsF metrics(finalFont,p.device());
                    const QRectF ink=metrics.tightBoundingRect(value);
                    const auto region=[&](const QRectF& candidate) {
                        return QRectF(candidate.center().x()-ink.width()/2-2*scale,candidate.center().y()-ink.height()/2-2*scale,ink.width()+4*scale,ink.height()+4*scale);
                    };
                    const auto clear=[&](const QRectF& candidate) {
                        const QRectF bounds=region(candidate);
                        return body.contains(bounds) && !markInk.intersects(bounds) &&
                            std::none_of(labelRegions.cbegin(),labelRegions.cend(),[&](const QRectF& prior) {return prior.intersects(bounds);});
                    };
                    bool placed=clear(box);
                    // Keep every required mark; search the nearest clear position
                    // around its calibrated location rather than dropping a value.
                    for(int distance=8;distance<=96 && !placed;distance+=8) {
                        for(const QPointF& offset:{QPointF(0,-1),QPointF(1,0),QPointF(-1,0),QPointF(0,1),QPointF(.707,-.707),QPointF(-.707,-.707),QPointF(.707,.707),QPointF(-.707,.707)}) {
                            const QRectF candidate=box.translated(offset*(distance*scale));
                            if(clear(candidate)) {box=candidate;placed=true;break;}
                        }
                    }
                    labelRegions.append(region(box));
                    p.setFont(finalFont);p.drawText(box,Qt::AlignCenter,value);
                }
            }
            p.setPen(QPen(color(m_config,"lowColor"),1.5)); p.drawPath(arc);
            const double y=outer.bottom()-44*scale-(m_face==Face::Anan && i==1 ? 16*scale : 0);
            if(m_config["showReadout"].toBool()) { drawObjectText(p,QRectF(outer.left()+outer.width()*i/m_channels.size()+4*scale,y,outer.width()/m_channels.size()-8*scale,18*scale),channel.name+" ("+channel.units+")",size); }
            continue;
        }
        if(!channel.dynamics.hasReading()) { continue; }
        p.save();p.setClipRect(body,Qt::IntersectClip);
        // From Thetis MeterManager.cs:40780-40855 [v2.10.3.15] — recent smoothed min/max fan.
        // adds the closing line
        if(c["showHistory"].toBool()) {
            QPainterPath fan; fan.moveTo(pivot); const double minimum=channel.dynamics.minHistory(),maximum=channel.dynamics.maxHistory();
            for(int step=0;step<=30;++step) { fan.lineTo(needleTip(i,minimum+(maximum-minimum)*step/30,r)); } fan.closeSubpath(); p.fillPath(fan,color(c,"historyColor"));
        }
        const auto draw=[&](double value,QColor needle) {
            const QPointF tip=needleTip(i,value,r);
            // From Thetis MeterManager.cs:40912-40928 [v2.10.3.15] — source needle shadow.
            //shadow?
            if(c["shadow"].toBool()) { const double shift=stroke*1.5*((tip.x()-r.center().x())/(r.width()/2)); for(int n=0;n<8;++n) { p.setPen(QPen(QColor(0,0,0,12),stroke*3*(1-n/7.0))); p.drawLine(pivot+QPointF(shift,stroke*1.5),tip+QPointF(shift,stroke*1.5)); } }
            p.setPen(QPen(needle,stroke)); p.drawLine(pivot,tip);
        };
        if(c["peakHold"].toBool()) { draw(channel.dynamics.maxHistory(),color(m_config,"highColor")); } draw(channel.dynamics.value(),color(c,"color"));
        p.restore();
    }
    if(!background && m_config["showReadout"].toBool()) {
        QStringList readings;
        for(int i=0;i<m_channels.size();++i) { if(channelVisible(i)) { const auto& c=m_channels[i]; readings.append(c.name+": "+(c.dynamics.hasReading()?QString::number(c.dynamics.value(),'f',1)+" "+c.units:QStringLiteral("--"))+(m_config["showPeakValue"].toBool()&&c.dynamics.hasReading()?QStringLiteral(" [%1]").arg(c.dynamics.maxHistory(),0,'f',1):QString())); } }
        p.setFont(font);
        p.fillRect(QRectF(outer.left(),outer.bottom()-26*scale,outer.width(),26*scale),color(m_config,"backdropColor")); p.setPen(color(m_config,"lowColor"));
        for(int i=0;i<readings.size();++i) {
            drawObjectText(p,QRectF(outer.left()+outer.width()*i/readings.size()+4*scale,outer.bottom()-26*scale,outer.width()/readings.size()-8*scale,24*scale),readings[i],size*14/18);
        }
    }
    if(background && m_face==Face::Anan && m_tx) { p.setPen(color(m_config,"lowColor")); p.drawText(QRectF(outer.right()-100,outer.top()+2,96,22),Qt::AlignCenter,QStringList{"","Power/SWR","Compression","ALC group","Amps"}.value(m_config["displayGroup"].toInt())+" ▾"); }
}
void CompositePresetItem::paintAnan(QPainter& p,const QRectF& outer,bool background) {
    using namespace AnanFace;
    const AnanFaceGeometry geometry=ananGeometry(outer,m_config["fontSize"].toInt(),m_config["showReadout"].toBool(),m_config["showPeakValue"].toBool(),ananReadoutCount(),ananReadoutCellWidth(outer.width()));
    const QRectF& skin=geometry.skin;
    if(skin.isEmpty()) { return; }
    const double scale=geometry.scale,fontRatio=m_config["fontSize"].toDouble()/18;
    QPainterPath glass; glass.addRoundedRect(geometry.window,44*scale,44*scale);
    static const QImage art(QStringLiteral(":/meters/anan-approved-clean.jpg"));
    static const QImage hubs(QStringLiteral(":/meters/anan-approved-hubs.jpg"));
    const auto text=[&](const QRectF& box,const QString& value,double size,QColor ink,Qt::Alignment alignment=Qt::AlignCenter,int minimumPixels=1) {
        QFont font=p.font(); font.setBold(true); font.setItalic(false); font.setPixelSize(qMax(1,qRound(size)));
        const QFontMetricsF metrics(font);
        const double factor=qMin(1.0,qMin(box.width()/qMax(1.0,metrics.horizontalAdvance(value)),box.height()/qMax(1.0,metrics.height())));
        font.setPixelSize(qMax(minimumPixels,qFloor(size*factor))); p.setFont(font); p.setPen(ink); p.drawText(box,alignment,value);
    };
    const auto included=[&](int i) { return ananChannelIncluded(i); };
    const auto active=[&](int i) {
        const int binding=i==0?bindingId():m_channels[i].binding;
        return channelVisible(i) && (bindingSupport(binding)==BindingSupport::Supported || m_channels[i].dynamics.hasReading());
    };
    const auto raw=[&](int i,double key) {
        const Channel& channel=m_channels[i]; const int binding=i==0?bindingId():channel.binding;
        return channel.config["normalisePower"].toBool()?key*m_powerScale/100.0:
            key-(m_aboveS9 && isReceiveSignalBinding(binding)?20:0);
    };
    const auto theta=[&](int i,double value) {
        const Channel& channel=m_channels[i];
        return angle(i,calibratedPoint(i,value),channel.calibration.first(),channel.calibration.last());
    };
    const auto silver=[&](const QRectF& box) {
        QLinearGradient gradient(box.topLeft(),box.topRight());
        gradient.setColorAt(0,QColor("#c2cbd0")); gradient.setColorAt(.38,QColor("#fbfdff"));
        gradient.setColorAt(.68,QColor("#e3e9ec")); gradient.setColorAt(1,QColor("#b1bdc4"));
        return QBrush(gradient);
    };
    const auto markInk=[&](int i,double key) {
        if(i==0 && key>-73) { return color(m_config,"highColor")==QColor("#ffff4444")?QColor("#ff1429"):color(m_config,"highColor"); }
        if(i==6 && key<0) { return QColor("#1bb9f4"); }
        return color(m_config,"lowColor")==QColor("#ff203040")?QColor("#e5eef3"):onDarkGlass(color(m_config,"lowColor"));
    };
    p.setRenderHint(QPainter::SmoothPixmapTransform,true);
    if(background) {
        p.drawImage(skin,art); p.fillPath(glass,QColor(0,0,0,qRound(.58*255)));
        if(m_config["showTitle"].toBool()) {
            const double headingHeight=geometry.selector.height();
            text(QRectF(outer.left()+4,outer.top(),outer.width()-(m_tx?geometry.selector.width()+8:8),headingHeight),
                m_config["title"].toString(),geometry.readoutFont,onDarkGlass(color(m_config,"titleColor")),Qt::AlignLeft|Qt::AlignVCenter);
        }
        if(m_tx) {
            const QString group=QStringList{"All transmit meters","Power/SWR","Compression","ALC group","Amps"}.value(m_config["displayGroup"].toInt());
            text(geometry.selector,group+QStringLiteral(" ▾"),geometry.readoutFont,onDarkGlass(color(m_config,"lowColor")),Qt::AlignRight|Qt::AlignVCenter);
        }
    }
    p.save(); p.setClipPath(glass,Qt::IntersectClip);
    if(background) {
        QVector<QPainterPath> paintedClearance;
        QVector<QRectF> occupied;
        const double clearance=3*qMin(1.0,skin.width()/600);
        const auto recordClearance=[&](const QPainterPath& path,double stroke) {
            // Measure the square caps/bevel joins actually painted by QPen;
            // segment distance alone underestimates their corner clearance.
            QPainterPathStroker stroker; stroker.setWidth(stroke+2*clearance);
            stroker.setCapStyle(Qt::SquareCap); stroker.setJoinStyle(Qt::BevelJoin);
            paintedClearance.append(stroker.createStroke(path));
        };
        const auto line=[&](const QPointF& a,const QPointF& b,double stroke,const QBrush& ink) {
            p.setPen(QPen(ink,stroke)); p.drawLine(a,b);
            QPainterPath path; path.moveTo(a); path.lineTo(b); recordClearance(path,stroke);
        };
        const auto arc=[&](int i,double begin,double end,const QBrush& ink,double width) {
            QPainterPath path;
            const double stroke=qMax(1.0,width*scale);
            for(int step=0;step<=90;++step) {
                const QPointF point=toWidget(atAngle(i,m_channels[i].config,(begin+(end-begin)*step/90)*M_PI/180),skin);
                if(step==0) { path.moveTo(point); } else { path.lineTo(point); }
            }
            p.setPen(QPen(ink,stroke)); p.drawPath(path); recordClearance(path,stroke);
        };
        // Paint scales in the preview's outer-to-inner order. Every drawn arc
        // and tick participates in the numeric ink clearance measurement.
        const QVector<int> order{0,3,4,2,5,6,1};
        for(int i:order) {
            if(!included(i)) { continue; }
            p.save(); if(!active(i)) { p.setOpacity(p.opacity()*.62); }
            const Role& r=role(i); const QJsonObject& config=m_channels[i].config;
            const QBrush ink=color(m_config,"lowColor")==QColor("#ff203040")?silver(geometry.window):QBrush(onDarkGlass(color(m_config,"lowColor")));
            arc(i,r.paintStart,r.paintEnd,ink,7);
            if(i==0) { arc(i,theta(i,raw(i,-73))*180/M_PI,r.paintEnd,markInk(i,-53),8); }
            if(i==6) { arc(i,r.start,theta(i,raw(i,0))*180/M_PI,markInk(i,-30),8); }
            QMap<double,bool> ticks;
            for(double value:values()[i]) { ticks[value]=true; }
            const auto& calibration=m_channels[i].calibration;
            for(auto knot=calibration.cbegin();knot!=calibration.cend();++knot) {
                if(!ticks.contains(knot.key())) { ticks[knot.key()]=false; }
                const auto next=std::next(knot);
                if(next==calibration.cend()) { continue; }
                const int divisions=i==6?5:3;
                for(int step=1;step<divisions;++step) {
                    const double key=knot.key()+(next.key()-knot.key())*step/divisions;
                    if(!ticks.contains(key)) { ticks[key]=false; }
                }
            }
            for(auto tick=ticks.cbegin();tick!=ticks.cend();++tick) {
                const double value=raw(i,tick.key()); const QPointF tip=needleTip(i,value,skin);
                const QPointF normal=ellipseNormal(i,config,theta(i,value));
                const double length=i==0?(tick.value()?32:16):i==5?(tick.value()?12:8):(tick.value()?20:10);
                line(tip,tip+normal*(i==0?1:-1)*length*scale,qMax(.75,(tick.value()?7:4)*scale),markInk(i,tick.key()));
            }
            p.restore();
        }
        const QStringList defaultNames{"Signal","Volts","Amps","Power","SWR","Compression","ALC group"};
        const QStringList defaultUnits{"dBm","V","A","W","","dB","dB"};
        const QColor defaultNeedles[]{QColor(233,51,50),Qt::black,Qt::black,QColor(233,51,50),Qt::black,Qt::black,Qt::black};
        const QVector<QImage>& crops=glyphs();
        for(int j=0;j<identities().size();++j) {
            const Identity& identity=identities()[j]; const int i=identity.channel;
            if(!included(i)) { continue; }
            const Channel& channel=m_channels[i];
            QRectF box(toWidget(identity.box.topLeft(),skin),QSizeF(identity.box.width()*scale,identity.box.height()*scale));
            const double grow=fontRatio;
            const QPointF center=box.center(); box.setSize(box.size()*grow); box.moveCenter(center);
            const double pad=3*scale*grow;
            const QRectF destination=box.adjusted(-pad,-pad,pad,pad);
            p.save(); if(!active(i)) { p.setOpacity(p.opacity()*.62); }
            const bool renamed=!identity.unit && channel.name!=defaultNames[i];
            const bool reunit=identity.unit && channel.units!=defaultUnits[i];
            const bool recolor=color(channel.config,"color")!=defaultNeedles[i] || color(m_config,"lowColor")!=QColor("#ff203040");
            if(renamed || reunit || recolor) {
                QString label=identity.text;
                if(renamed && j!=1) { label=channel.name.toUpper(); }
                if(reunit) { label=channel.units; }
                const QColor fallback=color(channel.config,"color")!=defaultNeedles[i]?onDarkGlass(color(channel.config,"color")):
                    color(m_config,"lowColor")!=QColor("#ff203040")?onDarkGlass(color(m_config,"lowColor")):QColor("#e5eef3");
                text(box,label,qMax(7.0,42*scale*grow),fallback);
            } else {
                p.drawImage(destination,crops[j]);
            }
            p.restore(); occupied.append(box);
        }
        for(int i:order) {
            if(!included(i)) { continue; }
            int numberIndex=0;
            double previousNumberX=-std::numeric_limits<double>::infinity();
            for(double key:values()[i]) {
                const double value=raw(i,key),a=theta(i,value);
                const QPointF tip=needleTip(i,value,skin),normal=ellipseNormal(i,m_channels[i].config,a),tangent(-normal.y(),normal.x());
                const int sign=i==0?1:-1;
                const QString label=i==0?(key<=-73?QString::number(qRound((key+127)/6)):QStringLiteral("+%1").arg(key+73)):
                    QString::number(raw(i,key),'g',4);
                const QVector<double>& majors=values()[i];
                const QPointF before=numberIndex>0?needleTip(i,raw(i,majors[numberIndex-1]),skin):
                    tip*2-needleTip(i,raw(i,majors[numberIndex+1]),skin);
                const QPointF after=numberIndex+1<majors.size()?needleTip(i,raw(i,majors[numberIndex+1]),skin):tip*2-before;
                const double lowTangent=QPointF::dotProduct(before-tip,tangent)/2;
                const double highTangent=QPointF::dotProduct(after-tip,tangent)/2;
                const QPointF pivot=needlePivot(i,skin),radius=radii(i,m_channels[i].config)*scale;
                const auto phase=[&](const QPointF& point) { return std::atan2((point.y()-pivot.y())/radius.y(),(point.x()-pivot.x())/radius.x()); };
                const double beforePhase=numberIndex>0?phase(before):2*a-phase(after);
                const double afterPhase=numberIndex+1<majors.size()?phase(after):2*a-beforePhase;
                const double lowPhase=(beforePhase+a)/2,highPhase=(afterPhase+a)/2;
                // Keep the nested main-scale numbers between their own arc
                // and the next inner arc. Angular association alone can put
                // a Current value into the COMP row at a compact size.
                const int innerChannel=i==3?4:i==4?2:i==2?5:-1;
                const QPointF innerRadius=innerChannel>=0?radii(innerChannel,m_channels[innerChannel].config)*scale:QPointF();
                const bool nested=innerChannel>=0 && included(innerChannel) &&
                    QLineF(pivot,needlePivot(innerChannel,skin)).length()<.001 &&
                    innerRadius.x()<radius.x() && innerRadius.y()<radius.y();
                const auto inRow=[&](const QPointF& point) {
                    if(!nested) { return true; }
                    const QPointF relative=point-pivot;
                    return std::hypot(relative.x()/radius.x(),relative.y()/radius.y())<1 &&
                        std::hypot(relative.x()/innerRadius.x(),relative.y()/innerRadius.y())>1;
                };
                const QRect source=i==0?signalBoxes()[numberIndex]:QRect();
                QPainterPath inkPath; QRectF inkBounds,box; bool found=false;
                const int nominalPixels=qMax(1,qRound(42*scale*fontRatio));
                int previousPixels=-1;
                // A number stays beside its own calibrated major tick. Search
                // normal offsets first; only fit its original numeric style
                // proportionally if this local region cannot hold nominal ink.
                for(double fit:{1.,.95,.9,.85,.8,.75,.7}) {
                    const int pixels=qMax(1,qRound(nominalPixels*fit));
                    if(i!=0 && pixels==previousPixels) { continue; }
                    previousPixels=pixels;
                    QFont font(QStringLiteral("Arial")); font.setBold(true); font.setPixelSize(pixels);
                    inkPath=QPainterPath(); inkPath.addText(QPointF(0,0),font,label); inkBounds=inkPath.boundingRect();
                    const double halfWidth=i==0?source.width()*scale*fontRatio*fit/2:inkBounds.width()/2;
                    const double halfHeight=i==0?source.height()*scale*fontRatio*fit/2:inkBounds.height()/2;
                    const double tickLength=i==0?32:i==5?12:20;
                    const double normalSupport=std::abs(normal.x())*halfWidth+std::abs(normal.y())*halfHeight;
                    const QPointF base=tip+normal*sign*(tickLength*scale+normalSupport+clearance+3.5*scale);
                    const auto place=[&](double along,double away) {
                        const QPointF center=base+tangent*along*scale+normal*sign*away*scale;
                        return QRectF(center.x()-halfWidth,center.y()-halfHeight,halfWidth*2,halfHeight*2);
                    };
                    const auto fits=[&](const QRectF& candidate) {
                        const double x=candidate.center().x(),along=QPointF::dotProduct(candidate.center()-tip,tangent);
                        const double angle=phase(candidate.center());
                        return x>previousNumberX && along>=lowTangent && along<=highTangent && angle>=lowPhase && angle<=highPhase && inRow(candidate.center()) && geometry.window.contains(candidate) &&
                            std::none_of(occupied.cbegin(),occupied.cend(),[&](const QRectF& prior) { return prior.adjusted(-clearance,-clearance,clearance,clearance).intersects(candidate); }) &&
                            std::none_of(paintedClearance.cbegin(),paintedClearance.cend(),[&](const QPainterPath& stroke) { return stroke.intersects(candidate); });
                    };
                    const QVector<double> normalOffsets{0,4,8,12,16,20,24,28,32,40,48,64,80,-4,-8};
                    for(double away:normalOffsets) {
                        const QRectF candidate=place(0,away);
                        if(fits(candidate)) { box=candidate; found=true; break; }
                    }
                    if(!found) {
                        QVector<QPointF> candidates;
                        for(int away=-8;away<=80;away+=2) { for(int along=-70;along<=70;along+=2) { if(along!=0) { candidates.append({double(along),double(away)}); } } }
                        std::stable_sort(candidates.begin(),candidates.end(),[](const QPointF& x,const QPointF& y) { return std::hypot(x.x(),x.y())<std::hypot(y.x(),y.y()); });
                        for(const QPointF& candidate:candidates) {
                            const QRectF alternative=place(candidate.x(),candidate.y());
                            if(fits(alternative)) { box=alternative; found=true; break; }
                        }
                    }
                    if(found) { break; }
                    box=place(0,0);
                }
                // Impossible imported geometry is diagnosed, never disguised by
                // moving a value to a different tick or silently omitting it.
                if(!found) { qCWarning(lcMeter)<<"ANAN numeric ink cannot fit calibrated tick neighborhood"<<i<<key<<skin.size(); }
                p.save(); if(!active(i)) { p.setOpacity(p.opacity()*.62); }
                if(i==0 && color(m_config,"lowColor")==QColor("#ff203040") && color(m_config,"highColor")==QColor("#ffff4444")) {
                    p.drawImage(box,crops[identities().size()+numberIndex]);
                } else {
                    inkPath.translate(box.center()-inkBounds.center());
                    QLinearGradient metal(box.topLeft(),box.bottomLeft());
                    metal.setColorAt(0,Qt::white); metal.setColorAt(.42,QColor("#eff3f5")); metal.setColorAt(1,QColor("#bcc5cc"));
                    const QBrush ink=i==0 && key>-73?QBrush(markInk(i,key)):i==6 && key<0?QBrush(QColor("#2bc2ff")):
                        color(m_config,"lowColor")==QColor("#ff203040")?QBrush(metal):QBrush(onDarkGlass(color(m_config,"lowColor")));
                    p.fillPath(inkPath,ink);
                }
                p.restore(); occupied.append(box); previousNumberX=box.center().x(); ++numberIndex;
            }
        }
    } else {
        // Shared needleTip is used by marks, live motion, history and peak hold.
        for(int i:QVector<int>{1,6,5,2,4,3,0}) {
            if(!included(i)) { continue; }
            const Channel& channel=m_channels[i]; const QJsonObject& config=channel.config;
            const bool live=active(i) && channel.dynamics.hasReading();
            const QPointF pivot=needlePivot(i,skin);
            // Parked pointers describe the face, not a measurement. Unknown
            // support is dimmer; no parked value enters dynamics or history.
            p.save();
            if(!live) {
                const int binding=i==0?bindingId():channel.binding;
                p.setOpacity(p.opacity()*(bindingSupport(binding)==BindingSupport::Supported?.5:.35));
            }
            if(live && config["showHistory"].toBool()) {
                QPainterPath fan; fan.moveTo(pivot);
                for(int step=0;step<=30;++step) { fan.lineTo(needleTip(i,channel.dynamics.minHistory()+(channel.dynamics.maxHistory()-channel.dynamics.minHistory())*step/30,skin)); }
                fan.closeSubpath(); p.fillPath(fan,color(config,"historyColor"));
            }
            const QColor defaultInk=i==0 || i==3?QColor(233,51,50):QColor(Qt::black);
            const auto draw=[&](double value,bool peak) {
                const QPointF tip=needleTip(i,value,skin); const QPointF delta=tip-pivot;
                const double length=qMax(.001,std::hypot(delta.x(),delta.y()));
                const QPointF direction=delta/length,normal(-direction.y(),direction.x());
                const double width=(i==0 || i==3?10:7)*scale*config["strokeWidth"].toDouble()/2.5;
                QPainterPath needle; needle.moveTo(pivot+normal*width); needle.lineTo(tip); needle.lineTo(pivot-normal*width); needle.lineTo(pivot-direction*22*scale); needle.closeSubpath();
                if(config["shadow"].toBool()) { QPainterPath shadow=needle; shadow.translate(scale,scale); p.fillPath(shadow,QColor(0,0,0,150)); }
                QLinearGradient red(pivot,tip); red.setColorAt(0,QColor("#91001a")); red.setColorAt(.38,QColor("#ff5361")); red.setColorAt(.6,QColor("#ff182f")); red.setColorAt(1,QColor("#ba1028"));
                const QBrush ink=peak?QBrush(onDarkGlass(color(m_config,"highColor"))):color(config,"color")!=defaultInk?QBrush(onDarkGlass(color(config,"color"))):i==0 || i==3?QBrush(red):silver(QRectF(pivot,tip).normalized());
                p.fillPath(needle,ink);
                p.setPen(QPen(i==0 || i==3?QColor("#ffc0bd"):QColor(Qt::white),qMax(.55,2.4*scale)));
                p.drawLine(pivot,tip-direction*12*scale);
            };
            if(live && config["peakHold"].toBool()) { draw(channel.dynamics.maxHistory(),true); }
            draw(live?channel.dynamics.value():raw(i,channel.calibration.firstKey()),false);
            p.restore();
        }
        // Foreground circles follow every channel's edited origin. Coincident
        // source pivots share a crop; edited pivots receive their own crop.
        QVector<QPointF> paintedHubs;
        for(int i:QVector<int>{0,3,4,2,5,6,1}) {
            if(!included(i)) { continue; }
            const QPointF pivot=needlePivot(i,skin);
            if(std::any_of(paintedHubs.cbegin(),paintedHubs.cend(),[&](const QPointF& point) { return QLineF(point,pivot).length()<.001; })) { continue; }
            const Role& r=role(i); const double radius=r.hubRadius*scale;
            QPainterPath circle; circle.addEllipse(pivot,radius,radius);
            p.save(); p.setClipPath(circle,Qt::IntersectClip);
            p.drawImage(QRectF(pivot.x()-radius,pivot.y()-radius,2*radius,2*radius),hubs,
                QRectF(r.hub.x()-r.hubRadius,r.hub.y()-r.hubRadius,2*r.hubRadius,2*r.hubRadius));
            p.restore(); paintedHubs.append(pivot);
        }
    }
    p.restore();
    if(!background) {
        // Restore the approved lower metal lip after needle stems and hubs.
        p.drawImage(QRectF(skin.left(),skin.top()+754*scale,skin.width(),(kHeight-754)*scale),art,QRectF(0,754,kWidth,kHeight-754));
    }
    if(!background && m_config["showReadout"].toBool()) {
        const int count=ananReadoutCount();
        const int columns=qMin(geometry.columns,qMax(1,count)),rows=qMax(1,(count+columns-1)/columns);
        const double width=geometry.readout.width()/columns,height=geometry.readout.height()/rows;
        int cell=0;
        for(int i=0;i<m_channels.size();++i) {
            if(!included(i) || !channelVisible(i)) { continue; }
            const Channel& channel=m_channels[i]; const bool available=channel.dynamics.hasReading();
            const QString unit=channel.units;
            const QString value=available?(i==0 && unit==QStringLiteral("dBm")?signalReadout(channel.dynamics.value()):QString::number(channel.dynamics.value(),'f',1)+" "+unit):QStringLiteral("-- ")+unit;
            const QRectF box(geometry.readout.left()+cell%columns*width,geometry.readout.top()+cell/columns*height,width,height);
            const double contentHeight=geometry.nameHeight+geometry.valueHeight+geometry.peakHeight;
            const double top=box.top()+qMax(0.0,(height-contentHeight)/2);
            text(QRectF(box.left()+3,top,width-6,geometry.nameHeight),channel.name,qMax(11.0,geometry.readoutFont*.75),onDarkGlass(color(m_config,"titleColor")),Qt::AlignCenter,11);
            text(QRectF(box.left()+3,top+geometry.nameHeight,width-6,geometry.valueHeight),value,geometry.readoutFont,onDarkGlass(color(channel.config,"color")),Qt::AlignCenter,12);
            if(m_config["showPeakValue"].toBool()) {
                text(QRectF(box.left()+3,top+geometry.nameHeight+geometry.valueHeight,width-6,geometry.peakHeight),available?QStringLiteral("Peak %1").arg(channel.dynamics.maxHistory(),0,'f',1):QStringLiteral("Peak --"),qMax(11.0,geometry.readoutFont*.7),onDarkGlass(color(m_config,"lowColor")),Qt::AlignCenter,11);
            }
            ++cell;
        }
    }
}
void CompositePresetItem::paintEye(QPainter& p,const QRectF& outer) {
    const double scale=objectTextScale(outer,{260,180});
    const QRectF r=outer.adjusted(outer.width()*.22,28*scale,-outer.width()*.22,-24*scale); const auto& channel=m_channels.first();
    const QColor bright=color(channel.config,"color"),closed(int(bright.red()*.35),int(bright.green()*.35),int(bright.blue()*.35)),dim(int(bright.red()*.75),int(bright.green()*.75),int(bright.blue()*.75));
    // From Thetis MeterManager.cs:37249-37368 [v2.10.3.15] — closed ellipse and bright above-S9 overlap.
    // scale percX for overlap hard coded for now
    const double fraction=channel.dynamics.hasReading()?calibratedPoint(0,channel.dynamics.value()).x()/.85:0;
    p.setPen(Qt::NoPen); p.setBrush(fraction<=.01?closed:dim); p.drawEllipse(r);
    if(fraction>.01 && fraction<1) { const double half=(360-int(360*fraction))/2.0; p.setBrush(closed); p.drawPie(r,int((-90-half)*16),int(half*2*16)); }
    if(fraction>=1) { const double half=int(360*(fraction-1))/2.0; p.setBrush(bright); p.drawPie(r,int((-90-half)*16),int(half*2*16)); }
    // From Thetis MeterManager.cs:33977-34010 [v2.10.3.15] — horizontal closed-section slits.
    // do the slits slits either side
    //start bottom
    // calc radius point right side
    //to top
    // calc radius point left side
    // adds the closing line
    QPainterPath slit; slit.moveTo(r.center()+QPointF(0,r.height()*.03)); slit.lineTo(r.center()+QPointF(r.width()*.4,0)); slit.lineTo(r.center()-QPointF(0,r.height()*.03)); slit.lineTo(r.center()-QPointF(r.width()*.4,0)); slit.closeSubpath(); p.fillPath(slit,closed);
    p.setBrush(QColor(32,32,32)); p.setPen(Qt::NoPen); p.drawEllipse(r.center(),r.width()/6,r.width()/6);
    if(m_config["showReadout"].toBool()) { p.setPen(color(m_config,"lowColor")); drawObjectText(p,QRectF(outer.left(),outer.bottom()-22*scale,outer.width(),20*scale),channel.dynamics.hasReading()?signalReadout(channel.dynamics.value()):QStringLiteral("--"),m_config["fontSize"].toInt()*scale*12/18); }
}
void CompositePresetItem::paintHistory(QPainter& p,const QRectF& outer) {
    const double scale=objectTextScale(outer,{260,120}),size=m_config["fontSize"].toInt()*scale*11/18;
    const QRectF r=outer.adjusted(44*scale,28*scale,-14*scale,-22*scale); double low=m_config["minValue"].toDouble(),high=m_config["maxValue"].toDouble();
    if(m_config["autoScale"].toBool() && !m_samples.isEmpty()) { low=m_samples.first().value; high=low; for(const Sample& s:m_samples) { low=qMin(low,s.value); high=qMax(high,s.value); } low-=3; high+=3; }
    p.setPen(color(m_config,"lowColor"));
    for(int n=0;n<=4;++n) { const double y=r.bottom()-r.height()*n/4; p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y)); drawObjectText(p,QRectF(outer.left(),y-8*scale,40*scale,16*scale),QString::number(low+(high-low)*n/4,'f',0),size,Qt::AlignRight|Qt::AlignVCenter); }
    drawObjectText(p,QRectF(r.left(),r.bottom()+3*scale,r.width(),18*scale),QStringLiteral("Recent %1 s · %2").arg(m_config["historyMs"].toInt()/1000).arg(m_config["units"].toString()),size);
    QPainterPath path; bool first=true; const qint64 latest=m_samples.isEmpty()?0:m_samples.last().time;
    for(const Sample& s:m_samples) { QPointF point(r.right()-(latest-s.time)*r.width()/m_config["historyMs"].toDouble(),r.bottom()-qBound(0.0,(s.value-low)/(high-low),1.0)*r.height()); if(first) { path.moveTo(point); first=false; } else { path.lineTo(point); } }
    p.setPen(QPen(color(m_channels.first().config,"color"),2)); p.drawPath(path);
    if(m_config["showReadout"].toBool()) { drawObjectText(p,QRectF(outer.left()+outer.width()*.66,outer.top()+2*scale,outer.width()*.34-4*scale,22*scale),m_channels.first().dynamics.hasReading()?QString::number(m_channels.first().dynamics.value(),'f',1)+" "+m_config["units"].toString():QStringLiteral("--"),size,Qt::AlignRight|Qt::AlignVCenter); }
}
void CompositePresetItem::layoutChildren(int width,int height) {
    const QRectF r=pixelRect(width,height); const auto place=[&](MeterItem* child,double y,double h) { if(child) { child->setRect(r.left()/width,(r.top()+r.height()*y)/height,r.width()/width,r.height()*h/height); } };
    if(m_face==Face::Contest) { place(m_vfo,0,.3); place(m_bands,.3,.35); place(m_modes,.65,.2); place(m_clock,.85,.15); }
    else { place(m_vfo,0,1); place(m_clock,0,1); }
    if(m_bands) {
        // Existing ButtonBox uses width-derived cells. Fit complete grids within each child viewport.
        m_bands->setHeightRatio(float(r.height()*.35*5/(3*r.width()*.96)));
        m_modes->setHeightRatio(float(r.height()*.2*5/(2*r.width()*.96)));
    }
}
void CompositePresetItem::paint(QPainter& p,int width,int height) { paintForLayer(p,width,height,Layer::Background); paintForLayer(p,width,height,Layer::OverlayDynamic); }
void CompositePresetItem::paintForLayer(QPainter& p,int width,int height,Layer layer) {
    const QRectF r=pixelRect(width,height); p.save(); p.setClipRect(r); p.setRenderHint(QPainter::Antialiasing,true);
    if((m_tx && m_config["fadeTx"].toBool()) || (!m_tx && m_config["fadeRx"].toBool())) { p.setOpacity(.25); }
    QFont font=p.font(); font.setPixelSize(m_config["fontSize"].toInt()); p.setFont(font);
    const double textScale=m_face==Face::Anan?1:objectTextScale(r,{m_face==Face::Cross || m_face==Face::Contest?360.:260.,
        m_face==Face::PowerSwr?144.:m_face==Face::Cross?260.:m_face==Face::Contest?280.:m_face==Face::Eye?180.:120.});
    const double textSize=m_config["fontSize"].toInt()*textScale;
    const bool signal=m_face==Face::SignalText;
    const double contentHeight=qMax(0.0,r.height()-12*textScale);
    const double titleWeight=m_config["showTitle"].toBool()?m_config["fontSize"].toInt():0;
    const double mainWeight=m_config["showReadout"].toBool()?m_config["fontSize"].toInt():0;
    const double peakWeight=m_config["showPeakValue"].toBool()?14:0;
    const double totalWeight=qMax(1.0,titleWeight+mainWeight+peakWeight);
    const double signalTitleHeight=contentHeight*titleWeight/totalWeight;
    const double signalPeakHeight=contentHeight*peakWeight/totalWeight;
    if(layer==Layer::Background) {
        const QColor backdrop=color(m_config,"backdropColor");
        p.fillRect(r,m_face==Face::Anan && backdrop==QColor("#fff1eee2")?QColor(8,14,20):backdrop);
        if(m_face==Face::Anan || m_face==Face::Cross) { paintNeedles(p,r,true); }
        else if(m_config["showTitle"].toBool() && !m_vfo && !m_clock && m_face!=Face::PowerSwr) {
            p.setPen(color(m_config,"titleColor"));
            const QRectF title=signal ? QRectF(r.left()+4*textScale,r.top()+2*textScale,r.width()-8*textScale,signalTitleHeight) :
                QRectF(r.left()+4*textScale,r.top()+2*textScale,(m_face==Face::History && m_config["showReadout"].toBool()?r.width()*.66:r.width())-8*textScale,22*textScale);
            drawObjectText(p,title,m_config["title"].toString(),textSize);
        }
    } else if(layer==Layer::OverlayDynamic) {
        if(m_face==Face::PowerSwr) { for(int i=0;i<2;++i) { paintBar(p,QRectF(r.left(),r.top()+r.height()*i/2,r.width(),r.height()/2),i); } }
        if(m_face==Face::Anan || m_face==Face::Cross) { paintNeedles(p,r,false); }
        if(m_face==Face::Eye) { paintEye(p,r); }
        if(m_face==Face::History) { paintHistory(p,r); }
        if(m_face==Face::SignalText) {
            const Channel& c=m_channels.first();
            const QRectF main(r.left()+8*textScale,r.top()+2*textScale+signalTitleHeight,r.width()-16*textScale,contentHeight-signalTitleHeight-signalPeakHeight);
            p.setPen(color(c.config,"color"));
            if(m_config["showReadout"].toBool()) { drawObjectText(p,main,c.dynamics.hasReading()?signalReadout(c.dynamics.value()):"--",textSize); }
            p.setPen(color(m_config,"lowColor"));
            if(m_config["showPeakValue"].toBool()) { drawObjectText(p,QRectF(r.left()+8*textScale,r.bottom()-10*textScale-signalPeakHeight,r.width()-16*textScale,signalPeakHeight),"Peak "+(c.dynamics.hasReading()?signalReadout(c.dynamics.maxHistory()):QStringLiteral("--")),14*textScale); }
            if(c.config["showHistory"].toBool() && c.dynamics.hasReading()) { const double a=calibratedPoint(0,c.dynamics.minHistory()).x(),b=calibratedPoint(0,c.dynamics.maxHistory()).x(); p.fillRect(QRectF(r.left()+r.width()*a,r.bottom()-3*textScale,r.width()*(b-a),3*textScale),color(c.config,"historyColor")); }
            if(c.config["peakHold"].toBool() && c.dynamics.hasReading()) { const double x=r.left()+r.width()*calibratedPoint(0,c.dynamics.maxHistory()).x(); p.setPen(QPen(color(c.config,"color"),2)); p.drawLine(QPointF(x,r.bottom()-8*textScale),QPointF(x,r.bottom())); }
        }
        const TransientChildLayout childLayout(internalItems(),m_bands,m_modes);
        layoutChildren(width,height);
        for(MeterItem* child:internalItems()) { if(child!=m_clock) { child->paint(p,width,height); } }
        if(m_clock) {
            p.setFont(font);
            const QRectF clock(m_clock->x()*width,m_clock->y()*height,m_clock->itemWidth()*width,m_clock->itemHeight()*height); const QString mode=m_config["clockMode"].toString(); const QDateTime utc=QDateTime::currentDateTimeUtc(); p.setPen(m_clock->timeColour());
            const auto display=[&](QDateTime time,QString title,QRectF rect) {
                const QString fmt=m_config["show24Hour"].toBool()?"HH:mm:ss":"hh:mm:ss AP";
                const bool titleOn=m_config["showTitle"].toBool(),dateOn=m_config["showDate"].toBool();
                const int lines=1+int(titleOn)+int(dateOn); const double lineHeight=rect.height()/lines;
                // Child clock roles use the owning face scale, then measured fitting.
                // Contest already assigns a compact child rectangle; do not multiply twice.
                double top=rect.top();
                if(titleOn) { p.setPen(m_clock->typeTitleColour()); drawObjectText(p,QRectF(rect.left()+2*textScale,top,rect.width()-4*textScale,lineHeight),title,textSize); top+=lineHeight; }
                p.setPen(m_clock->timeColour()); drawObjectText(p,QRectF(rect.left()+2*textScale,top,rect.width()-4*textScale,lineHeight),time.toString(fmt),textSize); top+=lineHeight;
                if(dateOn) { p.setPen(m_clock->dateColour()); drawObjectText(p,QRectF(rect.left()+2*textScale,top,rect.width()-4*textScale,lineHeight),time.toString("yyyy-MM-dd"),textSize); }
            };
            if(mode=="Both") { display(utc.toLocalTime(),"Local",QRectF(clock.left(),clock.top(),clock.width()/2,clock.height())); display(utc,"UTC",QRectF(clock.center().x(),clock.top(),clock.width()/2,clock.height())); } else { display(mode=="UTC"?utc:utc.toLocalTime(),mode,clock); }
        }
    }
    p.restore();
}
bool CompositePresetItem::handleMousePress(QMouseEvent* event,int w,int h) {
    if(m_inert) { return false; } const TransientChildLayout childLayout(internalItems(),m_bands,m_modes); layoutChildren(w,h);
    if(m_face==Face::Anan && m_tx && ananGroupControlRect(w,h).contains(event->position())) { m_config["displayGroup"]=(m_config["displayGroup"].toInt()+1)%5; markPresentationDirty(true); return true; }
    for(MeterItem* child:internalItems()) { if(child->hitTest(event->position(),w,h) && child->handleMousePress(event,w,h)) { return true; } } return false;
}
bool CompositePresetItem::handleMouseRelease(QMouseEvent* e,int w,int h) { if(m_inert) { return false; } const TransientChildLayout childLayout(internalItems(),m_bands,m_modes); layoutChildren(w,h); bool result=false; for(MeterItem* child:internalItems()) { result=child->handleMouseRelease(e,w,h)||result; } return result; }
bool CompositePresetItem::handleMouseMove(QMouseEvent* e,int w,int h) { if(m_inert) { return false; } const TransientChildLayout childLayout(internalItems(),m_bands,m_modes); layoutChildren(w,h); bool result=false; for(MeterItem* child:internalItems()) { result=child->handleMouseMove(e,w,h)||result; } return result; }
bool CompositePresetItem::handleWheel(QWheelEvent* e,int w,int h) { if(m_inert) { return false; } const TransientChildLayout childLayout(internalItems(),m_bands,m_modes); layoutChildren(w,h); for(MeterItem* child:internalItems()) { if(child->hitTest(e->position(),w,h) && child->handleWheel(e,w,h)) { return true; } } return false; }
} // namespace NereusSDR
