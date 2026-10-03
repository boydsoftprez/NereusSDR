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

#pragma once
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterDynamics.h"
#include <QJsonObject>
namespace NereusSDR {
class BarPresetItem : public MeterItem {
    Q_OBJECT
public:
    explicit BarPresetItem(QObject* parent = nullptr);
    void configureAsMic();
    void configureAsAlc();
    void configureAsCustom(int bindingId, double minV, double maxV, const QString& label);
    QString typeId() const;
    static QStringList variants();
    bool configureVariant(const QString& flavor);
    void setAboveS9Frequency(bool above) { if(m_aboveS9!=above) { m_aboveS9=above; m_scaleCache={}; m_staticDirty=true; } }
    Layer renderLayer() const override { return Layer::Background; }
    bool participatesIn(Layer layer) const override;
    void paint(QPainter&, int, int) override;
    void paintForLayer(QPainter&, int, int, Layer) override;
    QSet<int> readingBindings() const override;
    void pushBindingValue(int, double) override;
    void setValue(double value) override { pushBindingValue(bindingId(), value); }
    void setBindingUnavailable(int, const QString&) override;
    bool advanceMeter(qint64) override;
    bool takeStaticPresentationChange() override { const bool dirty=m_staticDirty; m_staticDirty=false; return dirty; }
    void resetForTxTransition(bool) override;
    QString serialize() const override;
    bool deserialize(const QString&) override;
    QJsonObject configuration() const;
    bool applyConfiguration(const QJsonObject&);
    double calibratedPosition(double) const;
    double primaryValue() const { return m_primary.value(); }
    double averageValue() const { return m_average.value(); }
    double peakValue() const { return m_primary.maxHistory(); }
    int preferredFaceHeight() const { return m_rowHeight; }
    int preferredRowHeight() const { return m_rowHeight; }
    QSize minimumFaceSize() const { return QSize(260,m_rowHeight); }
    double minimumHistory() const { return m_primary.minHistory(); }
    bool hasPrimaryReading() const { return m_primary.hasReading(); }
    bool hasAverageReading() const { return m_average.hasReading(); }
private:
    void configureDynamics();
    QString m_flavor = QStringLiteral("Custom"), m_label;
    int m_secondary = -1;
    double m_minimum = -30, m_maximum = 12, m_redThreshold = 0;
    QColor m_marker = Qt::yellow, m_background{32,32,32}, m_title{169,169,169};
    QColor m_historyColor{255,0,0,128};
    bool m_showHistory = true, m_peakHold = false, m_showValue = true, m_showPeakValue = true;
    QString m_style = QStringLiteral("Line"), m_units = QStringLiteral("dB");
    int m_interval = 100, m_historyMs = 2000, m_ignoreMs = 2000, m_rowHeight = 72;
    MeterDynamics m_primary, m_average;
    QJsonObject m_unknown;
    bool m_staticDirty = true;
    QColor m_lowFill = Qt::white;
    double m_calMinimum = -30, m_calMaximum = 12;
    double m_attack = .8, m_release = .1, m_middle = 0, m_middlePosition = .665;
    bool m_aboveS9 = false;
    QList<double> m_major{-20,-10,0,4,8,12}, m_minor{-25,-15,-5,2,6,10};
    QImage m_scaleCache;
    QRect m_scaleRect;
};
}
