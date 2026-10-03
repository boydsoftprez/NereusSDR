// Ported from Thetis MeterManager.cs [v2.10.3.15].
// Modification history (NereusSDR):
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
#include "gui/meters/MeterPoller.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/ClockItem.h"
#include <QPainter>
#include <QPainterPath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
namespace NereusSDR {
namespace {
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
    m_samples.clear(); m_lastFrame=-1;
}
QSet<int> CompositePresetItem::readingBindings() const { QSet<int> result; for(int i=0;i<m_channels.size();++i) { const int id=i==0 ? bindingId() : m_channels[i].binding; if(id>=0) { result.insert(id); } } return result; }
void CompositePresetItem::pushBindingValue(int binding,double reading) {
    const bool available=bindingUnavailableReason(binding).isEmpty() && std::isfinite(reading) && (hasMmioBinding() || !isNoMeterReading(reading));
    for(int i=0;i<m_channels.size();++i) { if(binding==(i==0?bindingId():m_channels[i].binding)) { m_channels[i].dynamics.push(reading,available); if(i==0) { m_value=reading; } } }
}
void CompositePresetItem::setBindingUnavailable(int binding,const QString& reason) {
    MeterItem::setBindingUnavailable(binding,reason); if(!reason.isEmpty()) { for(int i=0;i<m_channels.size();++i) { if(binding==(i==0?bindingId():m_channels[i].binding)) { m_channels[i].dynamics.push(0,false); } } }
}
bool CompositePresetItem::advanceMeter(qint64 now) {
    bool changed=false;
    const auto oldSize=m_samples.size(); m_samples.removeIf([&](const Sample& sample) { return now-sample.time>=m_config["historyMs"].toInt(); }); changed=m_samples.size()!=oldSize;
    for(Channel& channel:m_channels) { changed=channel.dynamics.advance(now) || changed; }
    if(m_face==Face::History && !m_channels.isEmpty() && m_channels[0].dynamics.hasReading() && (m_lastFrame<0 || now-m_lastFrame>=m_channels[0].config["updateIntervalMs"].toInt())) {
        m_samples.append({now,m_channels[0].dynamics.value()}); const int duration=m_config["historyMs"].toInt(); m_samples.removeIf([&](const Sample& s) { return now-s.time>=duration; });
        const int extra=m_samples.size()-m_config["historyCapacity"].toInt(); if(extra>0) { m_samples.remove(0,extra); } changed=true;
    }
    const bool clock=m_clock && (m_lastFrame<0 || now/250!=m_lastFrame/250); m_lastFrame=now; const bool dirty=m_presentationDirty; m_presentationDirty=false; return changed || clock || dirty;
}
void CompositePresetItem::resetForTxTransition(bool tx) { m_tx=tx; configureDynamics(); if(m_vfo) { m_vfo->setTransmitting(tx); } if(m_bands) { m_bands->setTransmitting(tx); m_modes->setTransmitting(tx); } }
void CompositePresetItem::setPowerScale(int watts) { if(watts>0 && watts!=m_powerScale) { m_powerScale=watts; markPresentationDirty(true); } }
void CompositePresetItem::setFrequency(qint64 hz) { if(m_vfo && m_vfo->frequency()!=hz) { markPresentationDirty(); } if(m_vfo) { m_vfo->setFrequency(hz); m_vfo->setUnavailableText(hz>0?QString():QStringLiteral("No live slice reading")); } }
void CompositePresetItem::setModeLabel(const QString& text) { if(m_stateMode!=text) { m_stateMode=text; markPresentationDirty(); } if(m_vfo) { m_vfo->setModeLabel(text); } }
void CompositePresetItem::setBandLabel(const QString& text) { if(m_stateBand!=text) { m_stateBand=text; markPresentationDirty(); } if(m_vfo) { m_vfo->setBandLabel(text); } }
void CompositePresetItem::setUnavailableText(const QString& text) { if(m_stateUnavailable!=text) { m_stateUnavailable=text; markPresentationDirty(); } if(m_vfo) { m_vfo->setUnavailableText(text); } if(m_bands) { m_bands->setAllButtonsAvailable(text.isEmpty()&&!m_inert,text); m_modes->setAllButtonsAvailable(text.isEmpty()&&!m_inert,text); } }
QVector<MeterItem*> CompositePresetItem::internalItems() const { QVector<MeterItem*> result; for(auto* child:{static_cast<MeterItem*>(m_vfo),static_cast<MeterItem*>(m_bands),static_cast<MeterItem*>(m_modes),static_cast<MeterItem*>(m_clock)}) { if(child) { result.append(child); } } return result; }
void CompositePresetItem::setPreviewInert(bool inert) { m_inert=inert; for(MeterItem* child:internalItems()) { child->blockSignals(inert); } if(m_bands && inert) { m_bands->setAllButtonsAvailable(false,"Preview controls are inactive"); m_modes->setAllButtonsAvailable(false,"Preview controls are inactive"); } }
int CompositePresetItem::preferredFaceHeight() const { return m_config["faceHeight"].toInt(); }
QSize CompositePresetItem::minimumFaceSize() const { return {m_face==Face::Anan || m_face==Face::Cross || m_face==Face::Contest?360:260,preferredFaceHeight()}; }
double CompositePresetItem::channelValue(int i) const { return m_channels.value(i).dynamics.value(); }
double CompositePresetItem::channelPeak(int i) const { return m_channels.value(i).dynamics.maxHistory(); }
bool CompositePresetItem::channelHasReading(int i) const { return i>=0 && i<m_channels.size() && m_channels[i].dynamics.hasReading(); }
QString CompositePresetItem::channelUnits(int i) const { return m_channels.value(i).units; }
bool CompositePresetItem::channelVisible(int i) const {
    if(i<0 || i>=m_channels.size()) { return false; } const QJsonObject& c=m_channels[i].config;
    const int group=c["displayGroup"].toInt(); return c["visible"].toBool() && (!c["onlyWhenRx"].toBool() || !m_tx) && (!c["onlyWhenTx"].toBool() || m_tx) && (group==0 || group==m_config["displayGroup"].toInt());
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
QPointF CompositePresetItem::needlePivot(int i,const QRectF& r) const { const auto c=m_channels.value(i).config; return r.center()+QPointF(r.width()*c["offsetX"].toDouble(),r.height()*c["offsetY"].toDouble()); }
QPointF CompositePresetItem::needleTip(int i,double value,const QRectF& r) const {
    const QJsonObject c=m_channels.value(i).config; const QPointF target=calibratedPoint(i,value); const QPointF pivot=needlePivot(i,r);
    // From Thetis MeterManager.cs:40754-40763,40865-40880 [v2.10.3.15]
    // needle offset from centre
    // calc angle required
    // expand
    const double dx=(pivot.x()-r.left()-target.x()*r.width())/c["radiusX"].toDouble(),dy=(pivot.y()-r.top()-target.y()*r.height())/c["radiusY"].toDouble();
    const double angle=std::atan2(dy,dx)+M_PI; const double radius=r.width()/2*c["lengthFactor"].toDouble();
    return pivot+QPointF(std::cos(angle)*radius*c["radiusX"].toDouble(),std::sin(angle)*radius*c["radiusY"].toDouble());
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
    const double left=r.left()+18,right=r.right()-18,top=r.top()+28,base=r.bottom()-14;
    const auto pos=[&](double value) { return left+(right-left)*calibratedPoint(index,value).x(); };
    QFont f=p.font(); f.setPixelSize(qBound(10,m_config["fontSize"].toInt(),20)); p.setFont(f); p.setPen(color(m_config,"titleColor")); if(m_config["showTitle"].toBool()) { p.drawText(r.adjusted(18,2,-18,0),Qt::AlignTop|Qt::AlignHCenter,channel.name); }
    if(c["showHistory"].toBool() && channel.dynamics.hasReading()) { p.fillRect(QRectF(pos(channel.dynamics.minHistory()),top,pos(channel.dynamics.maxHistory())-pos(channel.dynamics.minHistory()),base-top),color(c,"historyColor")); }
    p.setPen(QPen(color(m_config,"lowColor"),2)); p.drawLine(QPointF(left,base),QPointF(right,base));
    for(auto it=channel.calibration.begin();it!=channel.calibration.end();++it) {
        const double raw=c["normalisePower"].toBool() ? it.key()*m_powerScale/100.0 : it.key(); const double x=pos(raw);
        p.drawLine(QPointF(x,base),QPointF(x,base-10)); QRectF label(x-22,base-30,44,18); if(it==channel.calibration.begin()) { label.moveLeft(left); } else if(it.key()==channel.calibration.lastKey()) { label.moveRight(right); }
        p.drawText(label,Qt::AlignCenter,QString::number(raw,'g',4));
    }
    if(channel.dynamics.hasReading()) {
        if(c["peakHold"].toBool()) { p.setPen(QPen(color(m_config,"highColor"),3)); p.drawLine(QPointF(pos(channel.dynamics.maxHistory()),top),QPointF(pos(channel.dynamics.maxHistory()),base)); }
        p.setPen(QPen(color(c,"color"),3)); p.drawLine(QPointF(pos(channel.dynamics.value()),top),QPointF(pos(channel.dynamics.value()),base));
    }
    const auto reading=[&](double value) { return QString::number(value,'f',1)+(channel.units.isEmpty()?QString():" "+channel.units); };
    p.setPen(color(c,"color")); if(m_config["showReadout"].toBool()) { p.drawText(r.adjusted(18,2,-18,0),Qt::AlignLeft|Qt::AlignTop,channel.dynamics.hasReading()?reading(channel.dynamics.value()):"--"); }
    p.setPen(color(m_config,"highColor")); if(m_config["showPeakValue"].toBool()) { p.drawText(r.adjusted(18,2,-18,0),Qt::AlignRight|Qt::AlignTop,channel.dynamics.hasReading()?reading(channel.dynamics.maxHistory()):"--"); }
}
void CompositePresetItem::paintNeedles(QPainter& p,const QRectF& outer,bool background) {
    const double aspect=m_face==Face::Cross ? .782 : .512;
    const double width=qMin(outer.width()-24,(outer.height()-42)/aspect);
    const QRectF r(outer.center().x()-width/2,outer.top()+25,width,width*aspect);
    QFont font=p.font(); font.setPixelSize(qBound(10,qMin(m_config["fontSize"].toInt(),int(width/28)),20)); p.setFont(font);
    if(background) {
        p.setPen(color(m_config,"titleColor")); if(m_config["showTitle"].toBool()) { p.drawText(outer.adjusted(4,2,-4,0),Qt::AlignHCenter|Qt::AlignTop,m_config["title"].toString()); }
    }
    for(int i=0;i<m_channels.size();++i) {
        if(!channelVisible(i)) { continue; } const Channel& channel=m_channels[i]; const QJsonObject& c=channel.config;
        const QPointF pivot=needlePivot(i,r); const double stroke=c["strokeWidth"].toDouble()*std::hypot(r.width(),r.height())/450;
        if(background) {
            QPainterPath arc; int n=0;
            for(auto it=channel.calibration.begin();it!=channel.calibration.end();++it,++n) {
                const double raw=c["normalisePower"].toBool()?it.key()*m_powerScale/100.0:it.key()-(m_aboveS9 && (channel.binding==MeterBinding::SignalPeak || channel.binding==MeterBinding::SignalAvg)?20:0);
                const QPointF tip=needleTip(i,raw,r); if(n==0) { arc.moveTo(tip); } else { arc.lineTo(tip); }
                const QPointF direction=(tip-pivot)/std::hypot(tip.x()-pivot.x(),tip.y()-pivot.y());
                p.setPen(QPen(color(m_config,"lowColor"),1.5)); p.drawLine(tip,tip-direction*7);
                // Sparse labels keep all supported minimum sizes readable.
                if(n==0 || n==channel.calibration.size()-1 || (channel.calibration.size()>3 && (i==0 && m_face==Face::Anan ? it.key()==-73 : c["normalisePower"].toBool() ? it.key()==50 : n==channel.calibration.size()/2))) {
                    const QPointF label=tip+direction*(m_face==Face::Anan && i==4 ? -20 : 13); p.drawText(QRectF(label.x()-25,label.y()-10,50,20),Qt::AlignCenter,QString::number(raw,'g',4));
                }
            }
            p.setPen(QPen(color(m_config,"lowColor"),1.5)); p.drawPath(arc);
            const double y=outer.bottom()-18-(m_face==Face::Anan && i==1 ? 16 : 0);
            if(m_config["showReadout"].toBool()) { p.drawText(QRectF(outer.left()+8,y,outer.width()-16,18),Qt::AlignCenter,channel.name+" ("+channel.units+")"); }
            continue;
        }
        if(!channel.dynamics.hasReading()) { continue; }
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
    }
    if(!background && m_config["showReadout"].toBool()) {
        QStringList readings;
        for(int i=0;i<m_channels.size();++i) { if(channelVisible(i)) { const auto& c=m_channels[i]; readings.append(c.name+": "+(c.dynamics.hasReading()?QString::number(c.dynamics.value(),'f',1)+" "+c.units:QStringLiteral("--"))+(m_config["showPeakValue"].toBool()&&c.dynamics.hasReading()?QStringLiteral(" [%1]").arg(c.dynamics.maxHistory(),0,'f',1):QString())); } }
        font.setPixelSize(qBound(10,int(outer.width()/30),14)); p.setFont(font);
        p.fillRect(QRectF(outer.left(),outer.bottom()-35,outer.width(),35),color(m_config,"backdropColor")); p.setPen(color(m_config,"lowColor")); p.drawText(QRectF(outer.left()+6,outer.bottom()-35,outer.width()-12,32),Qt::AlignCenter,readings.join("   "));
    }
    if(m_face==Face::Anan && m_tx) { p.setPen(color(m_config,"lowColor")); p.drawText(QRectF(outer.right()-100,outer.top()+2,96,22),Qt::AlignCenter,QStringList{"","Power/SWR","Compression","ALC group","Amps"}.value(m_config["displayGroup"].toInt())+" ▾"); }
}
void CompositePresetItem::paintEye(QPainter& p,const QRectF& outer) {
    const QRectF r=outer.adjusted(outer.width()*.22,28,-outer.width()*.22,-12); const auto& channel=m_channels.first();
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
    if(m_config["showReadout"].toBool()) { QFont f=p.font(); f.setPixelSize(12); p.setFont(f); p.setPen(color(m_config,"lowColor")); p.drawText(QRectF(outer.left(),outer.bottom()-18,outer.width(),18),Qt::AlignCenter,channel.dynamics.hasReading()?signalReadout(channel.dynamics.value()):QStringLiteral("--")); }
}
void CompositePresetItem::paintHistory(QPainter& p,const QRectF& outer) {
    const QRectF r=outer.adjusted(44,28,-14,-22); double low=m_config["minValue"].toDouble(),high=m_config["maxValue"].toDouble();
    if(m_config["autoScale"].toBool() && !m_samples.isEmpty()) { low=m_samples.first().value; high=low; for(const Sample& s:m_samples) { low=qMin(low,s.value); high=qMax(high,s.value); } low-=3; high+=3; }
    QFont f=p.font(); f.setPixelSize(11); p.setFont(f); p.setPen(color(m_config,"lowColor"));
    for(int n=0;n<=4;++n) { const double y=r.bottom()-r.height()*n/4; p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y)); p.drawText(QRectF(outer.left(),y-8,40,16),Qt::AlignRight,QString::number(low+(high-low)*n/4,'f',0)); }
    p.drawText(QRectF(r.left(),r.bottom()+3,r.width(),18),Qt::AlignCenter,QStringLiteral("Recent %1 s · %2").arg(m_config["historyMs"].toInt()/1000).arg(m_config["units"].toString()));
    QPainterPath path; bool first=true; const qint64 latest=m_samples.isEmpty()?0:m_samples.last().time;
    for(const Sample& s:m_samples) { QPointF point(r.right()-(latest-s.time)*r.width()/m_config["historyMs"].toDouble(),r.bottom()-qBound(0.0,(s.value-low)/(high-low),1.0)*r.height()); if(first) { path.moveTo(point); first=false; } else { path.lineTo(point); } }
    p.setPen(QPen(color(m_channels.first().config,"color"),2)); p.drawPath(path);
    if(m_config["showReadout"].toBool()) { p.drawText(QRectF(r.left(),outer.top()+2,r.width(),22),Qt::AlignRight,m_channels.first().dynamics.hasReading()?QString::number(m_channels.first().dynamics.value(),'f',1)+" "+m_config["units"].toString():QStringLiteral("--")); }
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
    if(layer==Layer::Background) {
        p.fillRect(r,color(m_config,"backdropColor"));
        if(m_face==Face::Anan || m_face==Face::Cross) { paintNeedles(p,r,true); }
        else if(m_config["showTitle"].toBool() && !m_vfo && !m_clock && m_face!=Face::PowerSwr) { p.setPen(color(m_config,"titleColor")); p.drawText(r.adjusted(4,2,-4,0),Qt::AlignTop|Qt::AlignHCenter,m_config["title"].toString()); }
    } else if(layer==Layer::OverlayDynamic) {
        if(m_face==Face::PowerSwr) { for(int i=0;i<2;++i) { paintBar(p,QRectF(r.left(),r.top()+r.height()*i/2,r.width(),r.height()/2),i); } }
        if(m_face==Face::Anan || m_face==Face::Cross) { paintNeedles(p,r,false); }
        if(m_face==Face::Eye) { paintEye(p,r); }
        if(m_face==Face::History) { paintHistory(p,r); }
        if(m_face==Face::SignalText) {
            const Channel& c=m_channels.first(); const QRectF main=r.adjusted(8,26,-8,-26); font.setPixelSize(qMin(m_config["fontSize"].toInt(),int(r.height()*.38))); p.setFont(font); p.setPen(color(c.config,"color"));
            if(m_config["showReadout"].toBool()) { p.drawText(main,Qt::AlignCenter,c.dynamics.hasReading()?signalReadout(c.dynamics.value()):"--"); }
            font.setPixelSize(14); p.setFont(font); p.setPen(color(m_config,"lowColor"));
            if(m_config["showPeakValue"].toBool()) { p.drawText(r.adjusted(8,0,-8,-5),Qt::AlignBottom|Qt::AlignHCenter,"Peak "+(c.dynamics.hasReading()?signalReadout(c.dynamics.maxHistory()):QStringLiteral("--"))); }
            if(c.config["showHistory"].toBool() && c.dynamics.hasReading()) { const double a=calibratedPoint(0,c.dynamics.minHistory()).x(),b=calibratedPoint(0,c.dynamics.maxHistory()).x(); p.fillRect(QRectF(r.left()+r.width()*a,r.bottom()-3,r.width()*(b-a),3),color(c.config,"historyColor")); }
            if(c.config["peakHold"].toBool() && c.dynamics.hasReading()) { const double x=r.left()+r.width()*calibratedPoint(0,c.dynamics.maxHistory()).x(); p.setPen(QPen(color(c.config,"color"),2)); p.drawLine(QPointF(x,r.bottom()-8),QPointF(x,r.bottom())); }
        }
        layoutChildren(width,height);
        for(MeterItem* child:internalItems()) { if(child!=m_clock) { child->paint(p,width,height); } }
        if(m_clock) {
            const QRectF clock(m_clock->x()*width,m_clock->y()*height,m_clock->itemWidth()*width,m_clock->itemHeight()*height); const QString mode=m_config["clockMode"].toString(); const QDateTime utc=QDateTime::currentDateTimeUtc(); font.setPixelSize(qBound(12,qMin(m_config["fontSize"].toInt(),int(clock.height()/4)),22)); p.setFont(font); p.setPen(m_clock->timeColour());
            const auto display=[&](QDateTime time,QString title,QRectF rect) {
                const QString fmt=m_config["show24Hour"].toBool()?"HH:mm:ss":"hh:mm:ss AP";
                const bool titleOn=m_config["showTitle"].toBool(),dateOn=m_config["showDate"].toBool();
                const int lines=1+int(titleOn)+int(dateOn); const double lineHeight=rect.height()/lines;
                QFont clockFont=font; clockFont.setPixelSize(qBound(10,qMin(m_config["fontSize"].toInt(),int(lineHeight)-2),22)); p.setFont(clockFont);
                double top=rect.top();
                if(titleOn) { p.setPen(m_clock->typeTitleColour()); p.drawText(QRectF(rect.left(),top,rect.width(),lineHeight),Qt::AlignCenter,title); top+=lineHeight; }
                p.setPen(m_clock->timeColour()); p.drawText(QRectF(rect.left(),top,rect.width(),lineHeight),Qt::AlignCenter,time.toString(fmt)); top+=lineHeight;
                if(dateOn) { p.setPen(m_clock->dateColour()); p.drawText(QRectF(rect.left(),top,rect.width(),lineHeight),Qt::AlignCenter,time.toString("yyyy-MM-dd")); }
            };
            if(mode=="Both") { display(utc.toLocalTime(),"Local",QRectF(clock.left(),clock.top(),clock.width()/2,clock.height())); display(utc,"UTC",QRectF(clock.center().x(),clock.top(),clock.width()/2,clock.height())); } else { display(mode=="UTC"?utc:utc.toLocalTime(),mode,clock); }
        }
    }
    p.restore();
}
bool CompositePresetItem::handleMousePress(QMouseEvent* event,int w,int h) {
    if(m_inert) { return false; } layoutChildren(w,h);
    if(m_face==Face::Anan && m_tx && QRectF(pixelRect(w,h)).adjusted(pixelRect(w,h).width()-100,0,0,-pixelRect(w,h).height()+24).contains(event->position())) { m_config["displayGroup"]=m_config["displayGroup"].toInt()%4+1; markPresentationDirty(true); return true; }
    for(MeterItem* child:internalItems()) { if(child->hitTest(event->position(),w,h) && child->handleMousePress(event,w,h)) { return true; } } return false;
}
bool CompositePresetItem::handleMouseRelease(QMouseEvent* e,int w,int h) { if(m_inert) { return false; } layoutChildren(w,h); bool result=false; for(MeterItem* child:internalItems()) { result=child->handleMouseRelease(e,w,h)||result; } return result; }
bool CompositePresetItem::handleMouseMove(QMouseEvent* e,int w,int h) { if(m_inert) { return false; } layoutChildren(w,h); bool result=false; for(MeterItem* child:internalItems()) { result=child->handleMouseMove(e,w,h)||result; } return result; }
bool CompositePresetItem::handleWheel(QWheelEvent* e,int w,int h) { if(m_inert) { return false; } layoutChildren(w,h); for(MeterItem* child:internalItems()) { if(child->hitTest(e->position(),w,h) && child->handleWheel(e,w,h)) { return true; } } return false; }
} // namespace NereusSDR
