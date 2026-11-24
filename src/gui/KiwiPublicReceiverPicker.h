// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/gui/KiwiPublicReceiverPicker.h [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Das KiwiSDR-Protokoll stammt von John Seamons (ZL/KF6VO),
// http://kiwisdr.com.
//
// Modification history (Longpath):
//   2026-08-23 — Portiert (Stufe 4: Bedienflaeche).
//                EINE Abweichung von der Vorlage, und sie ist
//                bewusst: dort erbt der Dialog von PersistentDialog,
//                einem eigenen Rahmenwerk mit selbstgezeichneter
//                Titelleiste und gespeicherter Geometrie. Das haben
//                wir nicht, und unsere eigenen Dialoge (LanScanDialog,
//                SpotHubDialog, NetworkDiagnosticsDialog) sind
//                allesamt schlichte QDialog. Ein einzelner Dialog mit
//                fremdem Rahmen faellt im Betrieb auf; das Rahmenwerk
//                nur fuer ihn nachzuziehen waere ein Nebenschauplatz.
//                Die Logik darunter ist zeichengetreu uebernommen.

#pragma once

#include <QDialog>
#include <QVector>

#include "core/KiwiPublicDirectory.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace Longpath {

// Auswahlliste oeffentlicher KiwiSDR-Empfaenger. Holt das Verzeichnis
// beim Oeffnen — also auf ausdrueckliche Handlung des Betreibers — und
// zeigt AUSSCHLIESSLICH Empfaenger, deren Betreiber die aeussere
// Schnittstelle erlaubt (ext_api > 0). Reine Web-Empfaenger
// (ext_api == 0) werden vollstaendig herausgefiltert und nie
// angeboten: wer seinen Empfaenger nur ueber die eigene Seite
// bereitstellt, soll von uns nicht angefahren werden.
class KiwiPublicReceiverPicker : public QDialog {
    Q_OBJECT
public:
    explicit KiwiPublicReceiverPicker(QWidget* parent = nullptr);

    // Gueltig, nachdem der Dialog angenommen wurde.
    QString selectedEndpoint() const { return m_selectedEndpoint; }  // host[:port]
    QString selectedName() const { return m_selectedName; }          // Namensvorschlag
    // Entfernung der Auswahl in km; < 0, wenn nicht bekannt.
    double selectedKm() const { return m_selectedKm; }

    // Vorauswahl nach Entfernung zum eigenen Locator und nach Band
    // (kiwiPreselect). Haengt nichts an der Netzabfrage: wer sie ruft,
    // sortiert und filtert nur, was ohnehin geladen wird.
    void setPreselection(const QString& ownGrid, double frequencyHz);

private:
    void startFetch();
    void onReady(const QVector<KiwiPublicReceiver>& receivers);
    void applyFilter();
    void acceptCurrentRow();

    KiwiPublicDirectory* m_dir{nullptr};
    QVector<KiwiPublicReceiver> m_apiReceivers;  // bereits auf ext_api>0 gefiltert
    int m_hiddenWebOnly{0};
    int m_hiddenUnknown{0};  // verworfen, weil keine Regel veroeffentlicht ist
    bool m_fromCache{false};  // die Liste kam aus dem Sitzungsspeicher

    QLineEdit*    m_search{nullptr};
    QTableWidget* m_table{nullptr};
    QLabel*       m_status{nullptr};
    QPushButton*  m_refresh{nullptr};
    QPushButton*  m_ok{nullptr};

    QString m_selectedEndpoint;
    QString m_selectedName;
    double m_selectedKm{-1.0};
    bool m_preselect{false};
    QString m_ownGrid;
    double m_frequencyHz{0.0};
};

} // namespace Longpath
