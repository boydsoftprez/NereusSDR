// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Entwurf C „Mein Signal am Kiwi“ — der
// Kiwi-Wasserfallstreifen direkt unter dem eigenen Panadapter
// (~/Longpath/entwuerfe-kiwi-2026-11-24/C-streifen-hauptfenster.png).
//
//   2026-11-24 — Neu angelegt.
//
// Dieselbe Frequenzachse wie der Panadapter darueber: setView() bekommt
// Mitte und Breite des Panadapters, jede Kiwi-Zeile wird mit ihrem
// EIGENEN Bereich (low/high) auf diese Achse abgebildet. Der Kiwi
// liefert Zoomstufen in Zweierpotenzen und trifft die verlangte Breite
// nur ungefaehr -- der Streifen verlaesst sich darum nicht darauf, dass
// beide Achsen gleich sind, sondern rechnet es je Zeile aus. Wo der Kiwi
// nichts zeigt (Bild schmaler als die Achse), bleibt die Flaeche leer.
//
// Hausstil: Grau auf Fast-Schwarz. Farbe nur an drei Stellen -- der
// schmale Balken links in der Kopfzeile (das Warm der Kurve), die Marke
// auf der Sendefrequenz (#a86b6d) und die Status-Aussage rechts (gruen
// bestaetigt, bernstein Warnung).

#pragma once

#include "core/KiwiOwnSignal.h"

#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>

namespace Longpath {

class KiwiOwnSignalStrip : public QWidget {
    Q_OBJECT

public:
    explicit KiwiOwnSignalStrip(QWidget* parent = nullptr);

    // Zeilen, die der Streifen vorhaelt (neueste oben).
    static constexpr int kRows = 120;

    void setKiwiName(const QString& name);
    // km < 0: Entfernung unbekannt, wird nicht gezeigt.
    void setDistanceKm(double km);
    // measured=false: angenommener Verzug, wird mit "ca." gezeigt.
    void setDelay(int ms, bool measured);

    // Achse des Panadapters darueber.
    void setView(double centerHz, double bandwidthHz);
    // Sendefrequenz; <= 0: keine Marke.
    void setMarkHz(double hz);
    // Seitenraender der Achse, damit sie unter der des Panadapters
    // fluchtet (dBm-Skala, Rahmen).
    void setPlotMargins(int left, int right);

    void pushRow(const QVector<float>& binsDbm, double lowHz, double highHz);
    void clearRows();

    void setVerdict(const KiwiHearMeMeter::Result& result);
    // Fusszeile: laeuft eine Aufnahme, mit Dauer und Dateiname.
    void setRecording(bool recording, qint64 elapsedMs, const QString& fileName);

    // Das Bild des Streifens (Wasserfall und Marke, ohne Kopf/Fuss) fuer
    // die Aufnahme als PNG. Leer, wenn noch keine Zeile kam.
    QImage waterfallImage(int width = 1200) const;

    int rowCount() const { return m_rows.size(); }
    QString headerText() const;
    QString statusText() const;
    QString footerText() const;
    // Fuer die Pruefung: die Farbe, in der die Aussage steht.
    QColor statusColor() const;
    // x-Lage der Marke im Widget, -1 wenn ausserhalb der Achse.
    int markX() const;

    QSize sizeHint() const override { return QSize(480, 176); }
    QSize minimumSizeHint() const override { return QSize(160, 120); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Row {
        QVector<float> bins;
        double lowHz{0.0};
        double highHz{0.0};
    };

    QRect bodyRect() const;
    void renderBody(QImage& img) const;
    double noiseFloorDbm() const;

    QVector<Row> m_rows;       // [0] = neueste
    QString m_name;
    double m_km{-1.0};
    int m_delayMs{0};
    bool m_delayMeasured{false};
    double m_centerHz{0.0};
    double m_bandwidthHz{0.0};
    double m_markHz{0.0};
    int m_marginLeft{0};
    int m_marginRight{0};
    KiwiHearMeMeter::Result m_verdict;
    bool m_recording{false};
    qint64 m_recordMs{0};
    QString m_recordFile;
};

} // namespace Longpath
