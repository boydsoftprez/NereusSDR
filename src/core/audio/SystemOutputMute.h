// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/audio/SystemOutputMute.h  (Longpath)
// =================================================================
//
// Longpath-original. Der Stummschalter des BETRIEBSSYSTEMS, nicht der
// von Longpath.
//
// Warum es diese Klasse gibt: am 2026-11-14 war „kein Ton" zum zweiten
// Mal (nach dem 2026-11-12) der stumme Mac-Ausgang, waehrend in
// Longpath alles offen stand -- Hauptausgang nicht stumm, AF 50,
// PortAudio sauber auf dem richtigen Geraet, die Maschine schickte Ton
// mit audio_mute 0. Der Verdacht: F10. Am Mac ist F10 die Stummtaste,
// und der F-Tasten-Vorrang (FKeyPriority) gilt nur, solange Longpath
// vorne ist -- ein F10 fuer den Sprachspeicher im falschen Augenblick
// schaltet also den Mac stumm, nicht den Sprachspeicher ein.
//
// Das ist genau der Schalter, den MasterOutputWidget am 2026-11-13
// fuer `audio/Master/Muted` schon einmal geloest hat, nur eine Ebene
// hoeher: unsichtbar, nicht gewollt, und er kostet jedes Mal die
// gleiche Suche. Dieselbe Entscheidung gilt deshalb auch hier --
// beim Start loesen, im Betrieb nur melden.
//
// Auf Linux und Windows tut die Klasse nichts (isMuted() liefert
// Unknown). macOS: kAudioDevicePropertyMute des Vorgabe-Ausgangs,
// derselbe Schalter, den `osascript -e 'get volume settings'` als
// `output muted` zeigt. Unterstuetzt ein Geraet den Schalter nicht,
// faellt die Abfrage auf die virtuelle Hauptlautstaerke zurueck
// (0.0 heisst dann stumm).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-14 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================
#pragma once

namespace Longpath {

class SystemOutputMute
{
public:
    enum State {
        Unknown = -1,  // kein Weg dorthin (Linux/Windows) oder Abfrage schlug fehl
        NotMuted = 0,
        Muted = 1,
    };

    // Ist der Ausgang des Betriebssystems stumm?
    static State isMuted();

    // Stumm setzen oder loesen. true = angenommen.
    static bool setMuted(bool on);

    // Nur Pruefstaende: Attrappe statt CoreAudio. Ein nullptr-Leser
    // stellt den echten Weg wieder her.
    struct Backend {
        State (*read)();
        bool (*write)(bool);
    };
    static void setBackendForTest(const Backend& backend);
};

} // namespace Longpath
