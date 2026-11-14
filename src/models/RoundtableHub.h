// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/models/RoundtableHub.h  (Longpath)
// =================================================================
//
// Roundtable in Longpath: die Bruecke zwischen dem uebernommenen
// RoundtableClient (src/core/roundtable) und dem, was Longpath hat.
//
//   hinein:  die Scheiben (Frequenz, Betriebsart, Filter, welche gerade
//            sendet), Rufzeichen und Locator, die Antworten aus
//            Roundtable/* in AppSettings, der Stationsschluessel;
//   heraus:  die anderen Stationen aus der Raumliste als Spots der Quelle
//            "Roundtable" im SpotModel — so stehen sie am Panadapter, in
//            der Spot-Liste und hinter dem Quellenschalter wie jede andere
//            Spot-Quelle, und ein Klick stimmt ab.
//
// Upstream NereusSDR macht dasselbe verteilt: RadioModel::wireRoundtable /
// pushRoundtableSlices / roundtableModeOf (src/models/RadioModel.cpp
// @ 9036ffc04), SpotSourceHost::startRoundtableWith (Autostart, Pruefung
// des Rufzeichens) und RoundtableState (die Roundtable/*-Schluessel). Die
// Zuordnung der Betriebsarten und die Reihenfolge der Scheiben (aktive
// zuerst) sind von dort; der Rest ist Longpath-eigen, weil es bei uns
// weder Core noch SpotSourceHost noch einen Schreibbeobachter in
// AppSettings gibt.
//
// Longpath-Regeln dazu:
//   - Vor der Antwort auf die Erstfrage geht nichts hinaus, auch keine
//     Verbindung (RoundtableClient::wanted).
//   - Scheiben eines fremden Empfaengers (KiwiSDR, SunSDR ueber TCI:
//     suppressAutoStreamBinding) werden nicht gemeldet — sie sind nicht
//     diese Station.
//   - Die eigene Station steht nicht als Spot da.
//
// Einstellungen (AppSettings, Werte wie bei Upstream):
//   Roundtable/AutoStart       "True" (Vorgabe) | "False"
//   Roundtable/FirstRunAnswer  "" | "Live" | "Hidden"
//   Roundtable/Hidden          "True" | "False"
//   Roundtable/Callsign        leer = StationCallsign
//   Roundtable/GridSquare      leer = Stations-Locator
//   Roundtable/AwayChoice      "Report" (Vorgabe) | "Stop"
//   Roundtable/ServerUrl       leer = wss://roundtable.nereussdr.com/v1/station
//   Roundtable/SpotColor       Farbe der Spots (Vorgabe: setDefaultSpotColor)
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/WdspTypes.h"
#include "core/roundtable/RoundtableWire.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <memory>

namespace Longpath {

class RadioModel;
class RoundtableClient;
class RoundtableKey;
class SliceModel;
class SpotModel;

class RoundtableHub : public QObject {
    Q_OBJECT

public:
    /// Ein anderer auf Roundtable, wie die Raumliste ihn meldet
    /// (Frequenz-Erweiterung 1, `stations[]`).
    struct Station {
        QString key;        // sourceRoom/ref — wechselt mit jedem Raum
        QString callsign;
        qint64 hz = 0;
        QString family;
        bool keyed = false;
        bool away = false;
        bool joining = false;
    };

    static constexpr int kSpotLifetimeSec = 90;
    static constexpr int kSpotRefreshMs = 30000;

    explicit RoundtableHub(QObject* parent = nullptr);
    ~RoundtableHub() override;

    /// Nicht besessen. Die Scheiben und der Sendezustand kommen von dort.
    void attachRadio(RadioModel* radio);
    /// Nicht besessen. Dorthin gehen die Spots.
    void attachSpots(SpotModel* spots);
    /// Die Farbe der Spots, wenn Roundtable/SpotColor leer ist. Kommt aus
    /// der Oberflaeche (Style::kSpotRoundtableDefault) — ein Modell kennt
    /// keine Stilkonstanten.
    void setDefaultSpotColor(const QString& color) { m_defaultSpotColor = color; }

    RoundtableClient* client() const { return m_client.get(); }

    /// Liest Roundtable/* und gibt es dem Client; startet oder haelt an.
    void reloadSettings();

    bool enabled() const;
    bool answered() const;
    bool hidden() const;
    QString callsign() const;
    QString grid() const;
    QString statusText() const;

    /// Die Erstfrage beantworten: live (Rufzeichen, Locator, Frequenz
    /// werden oeffentlich) oder verborgen (nur lesen).
    void answerFirstRun(bool live);
    void setHidden(bool hidden);
    void setEnabled(bool on);

    QList<Station> stations() const { return m_stations.values(); }

    /// Aus den Scheiben die Liste, die `presence` meldet: aktive zuerst,
    /// fremde Empfaenger weg. Rein, fuer den Pruefstand.
    static QList<RoundtableSliceState> sliceStates(const QList<SliceModel*>& slices,
                                                   SliceModel* active, SliceModel* keyed);
    /// Die Betriebsart, wie Roundtable sie kennt; leer fuer keine Familie.
    static QString modeOf(DSPMode mode);

    /// Pruefstand: wo der Schluessel liegt (sonst neben den Einstellungen).
    void setKeyPathForTest(const QString& path) { m_keyPath = path; }
    /// Pruefstand: die Raumliste direkt einspeisen.
    void feedRoomsForTest(const QJsonObject& full) { onRooms(full); }
    void feedRoomsDeltaForTest(const QJsonObject& delta) { onRoomsDelta(delta); }

signals:
    void stationsChanged();
    void statusChanged();

private:
    void watchSlice(SliceModel* slice);
    void pushSlices();
    void onRooms(const QJsonObject& full);
    void onRoomsDelta(const QJsonObject& delta);
    void onLinkStateChanged();
    void rebuildStations();
    void syncSpots(bool refreshAll);
    void clearStations();
    const RoundtableKey* key();
    QString keyPath() const;

    QPointer<RadioModel> m_radio;
    QPointer<SpotModel> m_spots;
    std::unique_ptr<RoundtableClient> m_client;
    std::unique_ptr<RoundtableKey> m_key;
    QString m_keyPath;
    bool m_keyTried = false;
    bool m_keyed = false;
    QString m_defaultSpotColor;

    QHash<QString, QJsonObject> m_rooms;   // id -> summary
    QHash<QString, Station> m_stations;    // key -> station
    QHash<QString, int> m_spotIndex;       // key -> SpotModel index
    QTimer m_refresh;
};

} // namespace Longpath
