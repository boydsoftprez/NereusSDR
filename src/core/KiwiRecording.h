// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Entwurf C „Mein Signal am Kiwi“.
//
//   2026-11-24 — Neu angelegt.
//
// Aufnahme dessen, was der Kiwi hoert: der Ton als WAV (24 kHz, Float,
// stereo -- genau das, was der Kiwi liefert, ohne Umrechnung) und, wenn
// ein Bild mitgegeben wird, der Wasserfall daneben als PNG.
//
// ── Warum nicht QsoRecorder ──────────────────────────────────────────
//
// Geprueft (2026-11-24): QsoRecorder haengt am Abgriff von
// AudioEngine::rxBlockReady fuer GENAU EINE Scheibe -- die, die der
// Betreiber gerade aufnimmt. Kiwi-Ton laeuft zwar ueber rxBlockReady
// (feedKiwiSdrAudioData), aber unter der Scheibe des Kiwi, und nur, solange
// die Sendesperre ihn nicht stumm geschaltet hat. Wer sein QSO aufnimmt,
// nimmt die Scheibe seines Funkgeraets auf, nicht den Kiwi. Fuer "so
// kommt mein Signal draussen an" braucht es also diesen eigenen Schreiber,
// der den Ton vom Profil nimmt (decodedAudioReady) und von der
// Stummschaltung unabhaengig ist: stummgeschaltet heisst nicht
// ungehoert, und gerade die Sendezeiten will man mitschneiden.

#pragma once

#include "core/audio/WavFile.h"

#include <QByteArray>
#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QString>

namespace Longpath {

class KiwiRecording : public QObject {
    Q_OBJECT

public:
    explicit KiwiRecording(QObject* parent = nullptr);
    ~KiwiRecording() override;

    // Kiwi-Ton: 24 kHz.
    static constexpr int kSampleRate = 24000;
    // Obergrenze, damit eine vergessene Aufnahme die Platte nicht fuellt:
    // 2 Stunden = 24000 * 2 * 4 Byte * 7200 s = rund 1,4 GB.
    static constexpr qint64 kMaxSeconds = 2 * 60 * 60;

    // Dateiname ohne Endung: "Mein-Signal_<Kiwi>_<JJJJ-MM-TT_HHMMSS>".
    // Der Kiwi-Name wird auf Buchstaben, Ziffern, '-' und '_' gekuerzt.
    static QString fileBaseName(const QString& kiwiName, const QDateTime& when);

    // Legt den Ordner an und oeffnet die Datei. false mit Fehlertext,
    // wenn das nicht geht oder schon eine Aufnahme laeuft.
    bool start(const QString& folder, const QString& kiwiName,
               const QDateTime& when, QString* error = nullptr);

    // pcm24kStereoFloat wie in KiwiSdrManager::decodedAudioReady.
    void feedAudio(const QByteArray& pcm24kStereoFloat);

    // Schliesst die WAV (Kopf nachgetragen). Ist `waterfall` nicht leer,
    // steht es als PNG neben der WAV. Gibt den WAV-Pfad zurueck, leer,
    // wenn nichts lief.
    QString stop(const QImage& waterfall = QImage());

    bool isRecording() const { return m_writer.isOpen(); }
    // true, sobald stop() lief (bis zum naechsten start()).
    bool isFinalized() const { return m_finalized; }
    QString wavPath() const { return m_wavPath; }
    QString imagePath() const { return m_imagePath; }
    // Laenge der Aufnahme nach den geschriebenen Frames, nicht nach der Uhr.
    qint64 elapsedMs() const;
    bool stoppedByLimit() const { return m_stoppedByLimit; }
    bool writeFailed() const { return m_writeFailed; }

signals:
    // Aufnahme beendet, ohne dass stop() gerufen wurde (Obergrenze,
    // Schreibfehler). Der Besitzer ruft danach stop() fuer das Bild.
    void aborted(const QString& reason);

private:
    WavStreamWriter m_writer;
    QString m_wavPath;
    QString m_imagePath;
    bool m_finalized{false};
    bool m_stoppedByLimit{false};
    bool m_writeFailed{false};
};

} // namespace Longpath
