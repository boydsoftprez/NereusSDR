// Ported from Thetis MeterManager.cs [v2.10.3.15].
// Modification history (NereusSDR):
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
    void setPowerScale(int) override;
    void setAboveS9Frequency(bool above) { if(m_aboveS9!=above) { m_aboveS9=above; markPresentationDirty(true); } }
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
    void configureDynamics();
    void layoutChildren(int,int);
    void paintBar(QPainter&,const QRectF&,int);
    void paintNeedles(QPainter&,const QRectF&,bool);
    void paintAnan(QPainter&,const QRectF&,bool);
    void paintEye(QPainter&,const QRectF&);
    void paintHistory(QPainter&,const QRectF&);
    Face m_face;
    QString m_kind;
    QJsonObject m_config;
    bool m_tx=false,m_aboveS9=false,m_inert=false,m_presentationDirty=true,m_staticPresentationDirty=true;
    int m_powerScale=100;
    qint64 m_lastFrame=-1, m_lastHistorySample=-1;
    QString m_stateMode,m_stateBand,m_stateUnavailable;
    struct Sample { qint64 time; double value; };
    QVector<Sample> m_samples;
    BandButtonItem* m_bands=nullptr; ModeButtonItem* m_modes=nullptr;
    VfoDisplayItem* m_vfo=nullptr; ClockItem* m_clock=nullptr;
};
}
