// src/gui/meters/presets/AnanFaceRenderer.h (NereusSDR)
// Ported from Thetis MeterManager.cs [v2.10.3.15]: retained ANAN needle
// defaults and source phase geometry. The approved three-orb presentation,
// lettering crops, collision measurement and cache helpers are NereusSDR work.
// Copyright (C) 2026 J.J. Boyd (KG4VCF) — NereusSDR contributions only.
// Modification history (NereusSDR):
//   2026-10-03 — Adapted original needle defaults/phase geometry to JJ's approved
//                 full-wave three-orb preview, AI-assisted via OpenAI Codex.
//                 The approved .512 source-frame aspect is a Nereus adaptation;
//                 Thetis AddAnanMM uses .441 at MeterManager.cs:23795.
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
#include <QImage>
#include <QJsonObject>
#include <QLineF>
#include <QPainterPath>
#include <QRectF>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>

namespace NereusSDR::AnanFace {
constexpr double kWidth=1855, kHeight=848;
struct Role {
    QPointF hub;
    double hubRadius, rx, ry, length, start, end, paintStart, paintEnd;
};
// Approved native-config.json, role order remains the existing binding order.
inline const Role& role(int index) {
    static const Role roles[]{
        // From Thetis MeterManager.cs:23811 [v2.10.3.15] — original Signal length.
        {{919,740},32,1140,530,1.65,-128,-58,-128,-52},
        //volts
        // From Thetis MeterManager.cs:23853 [v2.10.3.15] — original Volts length.
        {{1248,736},17,205,135,.75,-140,-40,-140,-40},
        //amps
        // From Thetis MeterManager.cs:23881 [v2.10.3.15] — original Amps length.
        {{919,740},32,900,304,1.15,-121,-54,-121,-54},
        // From Thetis MeterManager.cs:23951 [v2.10.3.15] — original Power length.
        {{919,740},32,1040,475,1.55,-118,-58,-128,-56.5},
        // From Thetis MeterManager.cs:24014 [v2.10.3.15] — original SWR length.
        {{919,740},32,1010,380,1.36,-118,-58,-118,-58},
        // From Thetis MeterManager.cs:24045 [v2.10.3.15] — original COMP length.
        {{919,740},32,750,231,.96,-124,-54,-128,-50},
        // From Thetis MeterManager.cs:24078 [v2.10.3.15] — original ALC length.
        {{550,737},18,220,135,.75,-127,-40,-127,-40}
    };
    return roles[std::clamp(index,0,6)];
}
// From Thetis MeterManager.cs:23809,40754-40758 [v2.10.3.15] — original offset defaults and source pivot convention.
inline QPointF origin(int index,const QJsonObject& config) {
    // needle offset from centre
    // Presentation offsets are deltas from the original stored source defaults;
    // retain its .512 aspect while fitting the approved 1855-wide art.
    return role(index).hub+QPointF(kWidth*(config["offsetX"].toDouble()-.004),
        kWidth*.512*(config["offsetY"].toDouble()-.736));
}
// From Thetis MeterManager.cs:23810,40762-40763 [v2.10.3.15] — original radius defaults and length/radius scaling.
inline QPointF radii(int index,const QJsonObject& config) {
    const Role& r=role(index);
    const double length=config["lengthFactor"].toDouble()/r.length;
    return {r.rx*config["radiusX"].toDouble()*length,
        r.ry*config["radiusY"].toDouble()/.58*length};
}
// From Thetis MeterManager.cs:23809-23810,40862-40875 [v2.10.3.15] — calibrated point/pivot angle and radius expansion.
// NereusSDR reconstruction uses the approved .512 source frame; .504 and 1.236
// are .5 plus the unchanged source offset defaults, not new upstream constants.
inline double sourcePhase(const QPointF& target) {
    // map the meter scales to pixels
    // calc angle required
    // expand
    return std::atan2((.512*target.y()-.512*1.236)/.58,target.x()-.504);
}
inline double angle(int index,const QPointF& target,const QPointF& first,const QPointF& last) {
    const double lo=sourcePhase(first),hi=sourcePhase(last),center=(lo+hi)/2;
    double phase=sourcePhase(target);
    while(phase-center>M_PI) { phase-=2*M_PI; }
    while(phase-center<-M_PI) { phase+=2*M_PI; }
    const double q=std::clamp((phase-lo)/(hi-lo),0.0,1.0);
    const Role& r=role(index);
    return (r.start+q*(r.end-r.start))*M_PI/180;
}
inline QPointF atAngle(int index,const QJsonObject& config,double angle) {
    const QPointF radius=radii(index,config);
    return origin(index,config)+QPointF(radius.x()*std::cos(angle),radius.y()*std::sin(angle));
}
inline QPointF toWidget(const QPointF& point,const QRectF& skin) {
    return skin.topLeft()+point*(skin.width()/kWidth);
}
inline QPointF ellipseNormal(int index,const QJsonObject& config,double angle) {
    const QPointF radius=radii(index,config);
    const QPointF normal(std::cos(angle)/qMax(.001,std::abs(radius.x())),std::sin(angle)/qMax(.001,std::abs(radius.y())));
    return normal/qMax(.001,std::hypot(normal.x(),normal.y()));
}
struct Identity { int channel; bool unit; const char* text; QRect box; int filter; };
inline const QVector<Identity>& identities() {
    static const QVector<Identity> boxes{
        {0,false,"SIGNAL STRENGTH",{688,76,462,38},0},
        {0,false,"S",{150,196,83,59},0},
        {3,false,"POWER",{286,330,131,54},0},
        {4,false,"SWR",{356,416,88,38},0},
        {2,false,"CURRENT",{293,483,156,52},0},
        {5,false,"COMP",{343,554,108,40},0},
        {6,false,"ALC",{338,648,74,36},0},
        {1,false,"VOLTS",{1429,633,97,27},0},
        {0,true,"dB",{1618,205,94,40},0},
        {3,true,"W",{1510,344,60,41},1},
        {2,true,"A",{1463,482,46,42},1},
        {5,true,"dB",{1416,552,68,41},0},
        {1,true,"V",{1431,668,43,40},1}
    };
    return boxes;
}
inline const QVector<QRect>& signalBoxes() {
    static const QVector<QRect> boxes{{271,225,32,40},{438,175,51,41},{601,140,56,43},{761,124,49,42},
        {903,127,49,42},{1061,136,118,42},{1262,160,118,43},{1475,210,119,43}};
    return boxes;
}
inline const QVector<QVector<double>>& values() {
    static const QVector<QVector<double>> labels{{-121,-109,-97,-85,-73,-53,-33,-13},{10,12.5,15},
        {0,5,10,15,20},{0,10,25,50,100,150},{1,1.5,2,3,10},{0,5,10,15,20,25,30},{-30,0,25}};
    return labels;
}
// Extract only the approved source glyph pixels once. RGB is unchanged; alpha
// matches the preview's sRGB color matrices. No generated raster is written.
inline QImage extractGlyph(const QImage& source,const QRect& box,int filter,bool power) {
    QImage ink=source.copy(box).convertToFormat(QImage::Format_ARGB32);
    QPainterPath mask;
    if(power) { mask.moveTo(283,352); mask.lineTo(420,328); mask.lineTo(420,363); mask.lineTo(283,388); mask.closeSubpath(); }
    for(int y=0;y<ink.height();++y) {
        QRgb* line=reinterpret_cast<QRgb*>(ink.scanLine(y));
        for(int x=0;x<ink.width();++x) {
            const QRgb rgb=line[x];
            const double alpha=filter==1?10*qGreen(rgb)+10*qBlue(rgb)-11*255:
                filter==2?10*qRed(rgb)-5*255:8*qRed(rgb)+8*qGreen(rgb)-8.5*255;
            const int a=power && !mask.contains(QPointF(box.x()+x+.5,box.y()+y+.5))?0:qBound(0,qRound(alpha),255);
            line[x]=qRgba(qRed(rgb),qGreen(rgb),qBlue(rgb),a);
        }
    }
    return ink;
}
inline const QVector<QImage>& glyphs() {
    static const QVector<QImage> cache=[] {
        const QImage source(QStringLiteral(":/meters/anan-approved-lettering.jpg"));
        QVector<QImage> images;
        for(int i=0;i<identities().size();++i) {
            const Identity& identity=identities()[i];
            images.append(extractGlyph(source,identity.box.adjusted(-3,-3,3,3),identity.filter,i==2));
        }
        for(int i=0;i<signalBoxes().size();++i) { images.append(extractGlyph(source,signalBoxes()[i],i>4?2:0,false)); }
        return images;
    }();
    return cache;
}
struct Segment { QLineF line; double stroke; };
inline double pointSegmentDistance(const QPointF& p,const QLineF& line) {
    const QPointF delta=line.p2()-line.p1();
    const double squared=QPointF::dotProduct(delta,delta);
    const double t=squared?std::clamp(QPointF::dotProduct(p-line.p1(),delta)/squared,0.0,1.0):0;
    return QLineF(p,line.p1()+delta*t).length();
}
inline double segmentClearance(const QRectF& box,const Segment& segment) {
    const QLineF& line=segment.line;
    if(box.contains(line.p1()) || box.contains(line.p2())) { return 0; }
    const QPointF corners[]{box.topLeft(),box.topRight(),box.bottomRight(),box.bottomLeft()};
    double distance=std::numeric_limits<double>::infinity();
    for(int i=0;i<4;++i) {
        const QLineF edge(corners[i],corners[(i+1)%4]);
        if(line.intersects(edge,nullptr)==QLineF::BoundedIntersection) { return 0; }
        distance=qMin(distance,pointSegmentDistance(corners[i],line));
        distance=qMin(distance,pointSegmentDistance(line.p1(),edge));
        distance=qMin(distance,pointSegmentDistance(line.p2(),edge));
    }
    return qMax(0.0,distance-segment.stroke/2);
}
}
