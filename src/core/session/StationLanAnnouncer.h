// NereusSDR-original one-way Core LAN discovery multicast sender.
#pragma once

#include "StationLanAnnouncement.h"

#include <QObject>
#include <QTimer>

namespace NereusSDR {

bool stationLanListenerServesAddress(const QHostAddress& listener,
                                     const QHostAddress& source);

class StationLanAnnouncer : public QObject {
    Q_OBJECT

public:
    explicit StationLanAnnouncer(QObject* parent = nullptr);

    /// Announces `announcement` for a listener on `listenerAddress`, or
    /// stops: a loopback, multicast or broadcast listener, an announcement
    /// that is not schema 2 (kStationLanAnnouncementSchema) or one that does
    /// not encode is never sent.
    void update(const QHostAddress& listenerAddress,
                const StationLanAnnouncement& announcement);
    void stop();
    bool isActive() const { return m_active; }
    /// What is being announced; default-constructed while stopped.
    StationLanAnnouncement announcement() const { return m_announcement; }
    void announceNow();

private:
    void onTimer();

    QTimer m_timer;
    QHostAddress m_listenerAddress;
    StationLanAnnouncement m_announcement;
    bool m_active = false;
};

} // namespace NereusSDR
