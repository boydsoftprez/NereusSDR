// Ported from Thetis MeterManager.cs [v2.10.3.15].
// Modification history (NereusSDR):
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

#include "BarPresetItem.h"
#include "PresetGeometry.h"
#include "gui/meters/MeterPoller.h"
#include <QPainter>
#include <QJsonDocument>
#include <algorithm>
namespace NereusSDR {
BarPresetItem::BarPresetItem(QObject* parent) : MeterItem(parent) { configureAsCustom(-1,-30,12,QStringLiteral("Custom")); }
void BarPresetItem::configureDynamics() {
    m_scaleCache=QImage();
    // From Thetis MeterManager.cs:24340-24346,24662-24668 [v2.10.3.15]
    m_primary.configure(.8,.1,m_interval,m_historyMs,m_ignoreMs);
    m_average.configure(.8,.1,m_interval,m_historyMs,m_ignoreMs);
    m_primary.reset(m_minimum); m_average.reset(m_minimum);
}
void BarPresetItem::configureAsMic() {
    // From Thetis MeterManager.cs:24327-24413 [v2.10.3.15]
    m_flavor="Mic"; m_label="MIC"; setBindingId(MeterBinding::TxMicPeak); m_secondary=MeterBinding::TxMic;
    m_minimum=-30; m_maximum=12; m_historyColor=QColor(255,0,0,128); configureDynamics();
}
void BarPresetItem::configureAsAlc() {
    // From Thetis MeterManager.cs:24649-24734 [v2.10.3.15]
    m_flavor="Alc"; m_label="ALC"; setBindingId(MeterBinding::TxAlcPeak); m_secondary=MeterBinding::TxAlc;
    m_minimum=-30; m_maximum=12; m_historyColor=QColor(255,250,205,128); configureDynamics();
}
void BarPresetItem::configureAsCustom(int binding, double minimum, double maximum, const QString& label) {
    m_flavor="Custom"; m_label=label; setBindingId(binding); m_secondary=-1;
    m_minimum=minimum; m_maximum=maximum; configureDynamics();
}
QString BarPresetItem::typeId() const { return m_flavor=="Mic" ? "meter.mic" : m_flavor=="Alc" ? "meter.alc" : "meter.customBar"; }
QSet<int> BarPresetItem::readingBindings() const { QSet<int> result; if(bindingId()>=0) { result.insert(bindingId()); } if(m_secondary>=0) { result.insert(m_secondary); } return result; }
void BarPresetItem::pushBindingValue(int binding, double value) {
    const bool available=bindingUnavailableReason(binding).isEmpty() && std::isfinite(value) && (!isNoReadingBinding(binding) || !isNoMeterReading(value));
    if(binding==bindingId()) { m_value=value; m_primary.push(value,available); }
    if(binding==m_secondary) { m_average.push(value,available); }
}
void BarPresetItem::setBindingUnavailable(int binding, const QString& reason) {
    MeterItem::setBindingUnavailable(binding,reason);
    if(!reason.isEmpty()) { if(binding==bindingId()) { m_primary.push(0,false); } if(binding==m_secondary) { m_average.push(0,false); } }
}
bool BarPresetItem::advanceMeter(qint64 time) { const bool primary=m_primary.advance(time); const bool average=m_average.advance(time); return primary || average; }
void BarPresetItem::resetForTxTransition(bool inTx) { Q_UNUSED(inTx); m_primary.reset(m_minimum); m_average.reset(m_minimum); }
double BarPresetItem::calibratedPosition(double value) const {
    // From Thetis MeterManager.cs:24351-24353,24673-24675 [v2.10.3.15]
    if(m_flavor!="Custom") { return std::clamp(value<=0 ? (value+30)/30*.665 : .665+value/12*(.99-.665),0.0,.99); }
    return std::clamp((value-m_minimum)/(m_maximum-m_minimum),0.0,.99);
}
bool BarPresetItem::participatesIn(Layer layer) const { return layer==Layer::Background || layer==Layer::OverlayDynamic; }
void BarPresetItem::paint(QPainter& painter,int width,int height) { paintForLayer(painter,width,height,Layer::Background); paintForLayer(painter,width,height,Layer::OverlayDynamic); }
void BarPresetItem::paintForLayer(QPainter& p,int width,int height,Layer layer) {
    const PresetGeometry g(pixelRect(width,height));
    const auto pos=[&](double value) { return g.left+g.width*calibratedPosition(value); };
    p.save(); p.setClipRect(g.face); p.setRenderHint(QPainter::Antialiasing,false);
    QFont font=p.font(); font.setPixelSize(13); p.setFont(font);
    if(layer==Layer::Background) {
        p.fillRect(g.face,m_background); font.setPixelSize(16); p.setFont(font); p.setPen(m_title);
        p.drawText(QRectF(g.left,g.face.top()+2,g.width,22),Qt::AlignCenter,m_label);
    } else if(layer==Layer::OverlayDynamic) {
        // From Thetis MeterManager.cs:37424-37674 [v2.10.3.15]
        // History precedes the secondary marker; PostDrawItem redraws primary last.
        if(m_showHistory && m_primary.hasReading()) { p.fillRect(QRectF(pos(m_primary.minHistory()),g.top,pos(m_primary.maxHistory())-pos(m_primary.minHistory()),g.baseline-g.top),m_historyColor); }
        const auto marker=[&](double value,QColor color,int thickness) { p.setPen(QPen(color,thickness)); p.drawLine(QPointF(pos(value),g.top),QPointF(pos(value),g.bottom)); };
        if(m_average.hasReading()) { marker(m_average.value(),QColor(169,169,169),3); }
        if(m_primary.hasReading() && m_style!="Line") {
            // From Thetis MeterManager.cs:37458-37591 [v2.10.3.15]
            const double end=pos(m_primary.value());
            const double transition=pos(m_redThreshold);
            const QColor low=m_flavor=="Custom" ? m_marker : QColor(Qt::white);
            const auto fill=[&](double start,double stop) {
                if(start<transition) { p.fillRect(QRectF(start,g.top,qMax(0.0,qMin(stop,transition)-start),g.baseline-g.top),low); }
                if(stop>transition) { p.fillRect(QRectF(qMax(start,transition),g.top,stop-qMax(start,transition),g.baseline-g.top),Qt::red); }
            };
            if(m_style=="Solid") { fill(g.left,end); }
            else {
                int segmentBlockSize=int(g.width*.02);
                if(segmentBlockSize<7) { segmentBlockSize=7; } // minimum 7 pixels
                const int segmentGapSize=int(segmentBlockSize*.2);
                if(segmentBlockSize<2) { segmentBlockSize=2; } // minimum 2 pixels
                const int segmentStep=segmentBlockSize+segmentGapSize; // step, to include block + gap
                for(double x=g.left;x<end;x+=segmentStep) { fill(x,qMin(x+segmentBlockSize,end)); }
            }
        }
        // The static scale is cached separately from moving bars on both backends.
        const qreal dpr=p.device()->devicePixelRatioF();
        const QSize scaleSize(qRound(g.face.width()*dpr),qRound(g.face.height()*dpr));
        if(m_scaleCache.size()!=scaleSize || m_scaleCache.devicePixelRatio()!=dpr || m_scaleRect!=pixelRect(width,height)) {
            m_scaleCache=QImage(scaleSize,QImage::Format_ARGB32_Premultiplied); m_scaleCache.setDevicePixelRatio(dpr); m_scaleCache.fill(Qt::transparent);
            m_scaleRect=pixelRect(width,height); QPainter scale(&m_scaleCache); scale.setFont(font); scale.translate(-g.face.topLeft());
            // From Thetis MeterManager.cs:33393-33871,33872-33958 [v2.10.3.15]
            const double high=m_redThreshold;
            scale.setPen(QPen(Qt::white,2)); scale.drawLine(QPointF(g.left,g.baseline),QPointF(pos(high),g.baseline));
            scale.setPen(QPen(Qt::red,2)); scale.drawLine(QPointF(pos(high),g.baseline),QPointF(g.left+g.width*.99,g.baseline));
            const QList<double> minor=m_flavor=="Custom" ? QList<double>{} : QList<double>{-25,-15,-5,2,6,10};
            const QList<double> major=m_flavor=="Custom" ? QList<double>{m_minimum,(m_minimum+m_maximum)/2,m_maximum} : QList<double>{-20,-10,0,4,8,12};
            for(double value:minor) { scale.setPen(QPen(value>high ? Qt::red : Qt::white,2)); scale.drawLine(QPointF(pos(value),g.baseline),QPointF(pos(value),g.baseline-6)); }
            for(double value:major) {
                scale.setPen(QPen(value>high ? Qt::red : Qt::white,2)); scale.drawLine(QPointF(pos(value),g.baseline),QPointF(pos(value),g.baseline-12));
                const double x=pos(value); QRectF text(x-22,g.baseline-32,44,18);
                if(value==m_maximum) { text.moveRight(g.left+g.width); }
                scale.drawText(text,value==m_maximum ? Qt::AlignRight|Qt::AlignVCenter : Qt::AlignCenter,QString::number(value,'g',4));
            }
        }
        p.drawImage(g.face.topLeft(),m_scaleCache);
        if(m_peakHold && m_primary.hasReading()) { marker(m_primary.maxHistory(),Qt::red,3); }
        if(m_primary.hasReading()) { marker(m_primary.value(),m_marker,3); }
        font.setPixelSize(16); p.setFont(font);
        const auto formatted=[&](double value) { return QString::number(value,'f',1)+(m_units.isEmpty()?QString():QStringLiteral(" ")+m_units); };
        p.setPen(m_marker);
        if(m_showValue) { p.drawText(QRectF(g.left,g.face.top()+2,g.width*.32,22),Qt::AlignLeft|Qt::AlignVCenter,m_primary.hasReading()?formatted(m_primary.value()):QStringLiteral("--")); }
        p.setPen(Qt::red);
        if(m_showPeakValue) { p.drawText(QRectF(g.left+g.width*.68,g.face.top()+2,g.width*.32,22),Qt::AlignRight|Qt::AlignVCenter,m_primary.hasReading()?formatted(m_primary.maxHistory()):QStringLiteral("--")); }
    }
    p.restore();
}
QJsonObject BarPresetItem::configuration() const {
    QJsonObject c=m_unknown;
    c["kind"]="BarPreset"; c["flavor"]=m_flavor; c["label"]=m_label;
    c["x"]=x(); c["y"]=y(); c["w"]=itemWidth(); c["h"]=itemHeight();
    c["bindingId"]=bindingId(); c["secondaryBindingId"]=m_secondary;
    c["minValue"]=m_minimum; c["maxValue"]=m_maximum; c["redThreshold"]=m_redThreshold;
    c["barColor"]=m_marker.name(QColor::HexArgb); c["backdropColor"]=m_background.name(QColor::HexArgb);
    c["titleColor"]=m_title.name(QColor::HexArgb); c["historyColor"]=m_historyColor.name(QColor::HexArgb);
    c["showHistory"]=m_showHistory; c["peakHold"]=m_peakHold; c["showValue"]=m_showValue; c["showPeakValue"]=m_showPeakValue;
    c["style"]=m_style; c["units"]=m_units; c["updateIntervalMs"]=m_interval; c["historyMs"]=m_historyMs; c["ignoreHistoryMs"]=m_ignoreMs; c["rowHeight"]=m_rowHeight;
    return c;
}
bool BarPresetItem::applyConfiguration(const QJsonObject& c) {
    QJsonObject merged=configuration(); for(auto it=c.begin();it!=c.end();++it) { merged[it.key()]=it.value(); }
    return deserialize(QString::fromUtf8(QJsonDocument(merged).toJson(QJsonDocument::Compact)));
}
QString BarPresetItem::serialize() const { return QString::fromUtf8(QJsonDocument(configuration()).toJson(QJsonDocument::Compact)); }
bool BarPresetItem::deserialize(const QString& data) {
    const QJsonDocument document=QJsonDocument::fromJson(data.toUtf8()); if(!document.isObject()) { return false; }
    const QJsonObject c=document.object(); if(c.value("kind")!="BarPreset") { return false; }
    const QString flavor=c.value("flavor").toString(); if(flavor!="Mic" && flavor!="Alc" && flavor!="Custom") { return false; }
    for(const QString& key:{QStringLiteral("x"),QStringLiteral("y"),QStringLiteral("w"),QStringLiteral("h"),QStringLiteral("minValue"),QStringLiteral("maxValue"),QStringLiteral("bindingId")}) { if(!c.value(key).isDouble() || !std::isfinite(c.value(key).toDouble())) { return false; } }
    if(c["maxValue"].toDouble()<=c["minValue"].toDouble()) { return false; }
    // Validate the full effective record before touching this live QObject.
    for(const QString& key:{QStringLiteral("bindingId"),QStringLiteral("secondaryBindingId"),QStringLiteral("updateIntervalMs"),QStringLiteral("historyMs"),QStringLiteral("ignoreHistoryMs"),QStringLiteral("rowHeight")}) {
        if(c.contains(key)) { const double v=c.value(key).toDouble(-1.5); if(!c.value(key).isDouble() || !std::isfinite(v) || v!=std::floor(v) || v<-1 || v>std::numeric_limits<int>::max()) { return false; } }
    }
    if(c["w"].toDouble()<=0 || c["h"].toDouble()<=0) { return false; }
    for(const QString& key:{QStringLiteral("x"),QStringLiteral("y"),QStringLiteral("w"),QStringLiteral("h")}) { if(qAbs(c[key].toDouble())>std::numeric_limits<float>::max()) { return false; } }
    for(const QString& key:{QStringLiteral("showHistory"),QStringLiteral("peakHold"),QStringLiteral("showValue"),QStringLiteral("showPeakValue")}) { if(c.contains(key) && !c[key].isBool()) { return false; } }
    for(const QString& key:{QStringLiteral("label"),QStringLiteral("style"),QStringLiteral("units")}) { if(c.contains(key) && !c[key].isString()) { return false; } }
    for(const QString& key:{QStringLiteral("barColor"),QStringLiteral("backdropColor"),QStringLiteral("titleColor"),QStringLiteral("historyColor")}) { if(c.contains(key) && (!c[key].isString() || !QColor(c[key].toString()).isValid())) { return false; } }
    if(c.contains("redThreshold") && (!c["redThreshold"].isDouble() || !std::isfinite(c["redThreshold"].toDouble()))) { return false; }
    const QString style=c.value("style").toString("Line"); if(style!="Line" && style!="Solid" && style!="Segments") { return false; }
    const int interval=c.value("updateIntervalMs").toInt(100);
    if(interval<1 || c.value("historyMs").toInt(2000)<interval || c.value("ignoreHistoryMs").toInt(2000)<0 || c.value("rowHeight").toInt(72)<72) { return false; }
    if(flavor=="Mic") { configureAsMic(); } else if(flavor=="Alc") { configureAsAlc(); } else { configureAsCustom(c["bindingId"].toInt(),c["minValue"].toDouble(),c["maxValue"].toDouble(),c["label"].toString()); }
    m_unknown=c; setRect(c["x"].toDouble(),c["y"].toDouble(),c["w"].toDouble(),c["h"].toDouble());
    setBindingId(c["bindingId"].toInt()); m_secondary=c.value("secondaryBindingId").toInt(m_secondary);
    m_label=c.value("label").toString(m_label); m_minimum=c["minValue"].toDouble(); m_maximum=c["maxValue"].toDouble();
    m_redThreshold=c.value("redThreshold").toDouble(m_redThreshold);
    const auto color=[&](const char* key,QColor& value) { if(c.contains(key)) { QColor parsed(c.value(key).toString()); if(!parsed.isValid()) { return false; } value=parsed; } return true; };
    if(!color("barColor",m_marker)||!color("backdropColor",m_background)||!color("titleColor",m_title)||!color("historyColor",m_historyColor)) { return false; }
    m_showHistory=c.value("showHistory").toBool(m_showHistory); m_peakHold=c.value("peakHold").toBool(m_peakHold); m_showValue=c.value("showValue").toBool(m_showValue); m_showPeakValue=c.value("showPeakValue").toBool(m_showPeakValue);
    m_style=c.value("style").toString(m_style); if(m_style!="Line" && m_style!="Solid" && m_style!="Segments") { return false; }
    m_units=c.value("units").toString(m_units); m_interval=c.value("updateIntervalMs").toInt(m_interval); m_historyMs=c.value("historyMs").toInt(m_historyMs); m_ignoreMs=c.value("ignoreHistoryMs").toInt(m_ignoreMs); m_rowHeight=c.value("rowHeight").toInt(m_rowHeight);
    if(m_interval<1 || m_historyMs<m_interval || m_ignoreMs<0 || m_rowHeight<72) { return false; }
    configureDynamics(); return true;
}
}
