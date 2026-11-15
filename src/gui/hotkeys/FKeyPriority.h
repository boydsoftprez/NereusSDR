// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/hotkeys/FKeyPriority.h  (Longpath)
// =================================================================
//
// Longpath-original. Am Mac liefern F1…F12 ab Werk Helligkeit, Mission
// Control, Musik und Lautstaerke; die F-Taste selbst gibt es nur mit fn.
// Der Sprachspeicher sendet aber mit F1…F10 (N1MM-Stil). Solange Longpath
// vorne ist, stellt diese Klasse den F-Tasten-Modus des Systems auf
// „F-Tasten zuerst" um und danach wieder zurueck: sobald ein anderes
// Programm vorne ist, beim Beenden, und nach einem Absturz beim
// naechsten Start (Marke neben der Einstellungsdatei).
//
// Der Weg ist derselbe wie bei Fluor (Pyroh/Fluor, FKeyManager.swift):
// der Parameter kIOHIDFKeyModeKey des IOHIDSystem, nur gelesen und
// geschrieben, solange Longpath vorne ist. Steht das System schon auf
// F-Tasten, ruehrt Longpath nichts an.
//
// Auf Linux und Windows tut die Klasse nichts (Lesen liefert -1).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================
#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace Longpath {

class FKeyPriority : public QObject
{
    Q_OBJECT

public:
    // Die zwei Werte von kIOHIDFKeyModeKey.
    static constexpr int kModeSpecial = 0;  // Helligkeit, Lautstaerke …; F-Tasten mit fn
    static constexpr int kModeFKeys = 1;    // F1…F12; Helligkeit, Lautstaerke mit fn

    struct Backend {
        std::function<int()> read;        // aktueller Modus; -1 = unbekannt
        std::function<bool(int)> write;   // true = angenommen
    };

    // markerPath: die Datei, die waehrend der Umstellung existiert. Leer
    // = keine Marke (dann hilft nach einem Absturz nichts).
    FKeyPriority(Backend backend, QString markerPath, QObject* parent = nullptr);
    ~FKeyPriority() override;

    // Wunsch des Betreibers: Tastenkuerzel an UND „F-Tasten zuerst" an.
    void setWanted(bool on);
    // Longpath ist das vordere Programm.
    void setActive(bool on);
    // Durchhalten, auch wenn Longpath NICHT vorne ist -- der Modus
    // bleibt gestellt, solange Longpath laeuft, und geht erst beim
    // Beenden zurueck.
    //
    // Betreiber 2026-11-14 („Ja, durchgehend"): nur-solange-vorne hat
    // den Mac zweimal in drei Tagen stumm gemacht. F10 ist am Mac die
    // Stummtaste, und zwischen dem Klick in Longpath und dem
    // Tastendruck lag jedes Mal ein Fenster, in dem der Modus noch auf
    // der Mac-Belegung stand. Der Preis ist bekannt und gewaehlt:
    // Helligkeit und Lautstaerke gehen dann in JEDEM Programm nur mit
    // fn, solange Longpath laeuft.
    void setHoldWhileRunning(bool on);
    // Zurueck auf den Modus des Systems -- beim Beenden.
    void release();

    bool engaged() const { return m_engaged; }

    // Der echte Weg: macOS ueber IOKit, sonst ein leerer.
    static Backend systemBackend();
    // Neben der Einstellungsdatei.
    static QString defaultMarkerPath();

    // Nur Pruefstaende: echter Weg raus, Attrappe rein, Zustand neu
    // anwenden.
    void setBackendForTest(Backend backend);

signals:
    // Einmal je Lauf, wenn macOS die Umstellung nicht annimmt.
    void failed(const QString& text);

private:
    void apply();
    int read() const;
    bool write(int mode) const;
    void setMarker(bool on) const;

    Backend m_backend;
    QString m_markerPath;
    bool m_wanted{false};
    bool m_active{false};
    bool m_hold{false};
    bool m_engaged{false};
    bool m_failedOnce{false};
};

} // namespace Longpath
