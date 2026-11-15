// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/MainWindow_HotKeys.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Die Tastenkuerzel im Hauptfenster: Sperrliste aus
// den Menues lesen, Belegung laden, Verteiler und Tastaturweiche
// einhaengen. Dazu die zwei Wege, die Tasten und Maus jetzt teilen:
// Abstimmen wie ein Klick ins Panorama und die Lupe.
//
// Die Teile selbst (HotKeyBindings, HotKeyDispatcher, KeyboardRouter)
// sind aus dem Upstream NereusSDR uebernommen; siehe deren Koepfe.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude. Der Abstimmweg
//                 stand bis dahin im Klick-Handler von
//                 wireSpectrumSliceControls (MainWindow.cpp).
//   2026-11-11 — F-Tasten zuerst am Mac (FKeyPriority), KI-gestuetzt
//                 ueber Anthropic Claude.
// =================================================================

#include "gui/MainWindow.h"

#include "core/CompanionRigController.h"
#include "core/MoxController.h"
#include "core/audio/VoiceKeyerPlayer.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/hotkeys/HotKeyChord.h"
#include "gui/SpectrumWidget.h"
#include "gui/hotkeys/FKeyPriority.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/KeyboardRouter.h"
#include "gui/hotkeys/MainWindowHotKeyTargets.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QAction>
#include <QGuiApplication>
#include <QHash>
#include <QStandardPaths>
#include <QKeySequence>
#include <QShortcut>

#include <cmath>

namespace Longpath {

namespace {

// Wie ein Menuepunkt im Hinweis heisst: ohne & und ohne Auslassungs-
// punkte („&FreeDV Reporter..." -> „FreeDV Reporter").
QString menuItemName(const QAction* action)
{
    QString name = action->text();
    name.remove(QLatin1Char('&'));
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QChar(0x2026))) {
        name.chop(1);
    }
    name = name.trimmed();
    return name.isEmpty() ? QStringLiteral("a menu") : name;
}

// Nur einteilige Tastenfolgen: „Strg+K, Strg+D" kann keine Taste sein.
void noteSequence(QHash<QString, QString>& reserved, const QKeySequence& seq, const QString& name)
{
    if (seq.count() != 1) {
        return;
    }
    const QString text = seq.toString(QKeySequence::PortableText);
    const std::optional<HotKeyChord> chord = HotKeyChord::parse(text);
    if (chord.has_value() && !reserved.contains(chord->toString())) {
        reserved.insert(chord->toString(), name);
    }
}

} // namespace

void MainWindow::richteTastenkuerzelEin()
{
    if (!m_radioModel || !m_radioModel->hotKeyBindings()) {
        return;
    }
    HotKeyBindings* const bindings = m_radioModel->hotKeyBindings();

    // ── Die Sperrliste aus dem, was schon Tasten hat ─────────────────
    //
    // Upstream fuehrt sie von Hand als Tabelle neben den Menues. Hier
    // wird sie gelesen: ein neues Menue-Kuerzel ist damit sofort
    // gesperrt, und keine Tabelle kann veralten.
    QHash<QString, QString> reserved;
    const QList<QAction*> actions = findChildren<QAction*>();
    for (const QAction* const action : actions) {
        for (const QKeySequence& seq : action->shortcuts()) {
            noteSequence(reserved, seq, menuItemName(action));
        }
    }
    const QList<QShortcut*> shortcuts = findChildren<QShortcut*>();
    for (const QShortcut* const shortcut : shortcuts) {
        noteSequence(reserved, shortcut->key(), QStringLiteral("window"));
    }
    HotKeyBindings::setReservedChords(reserved);

    // Eine Vorgabetaste, die ein Menue schon hat, kaeme nie an: das
    // Menue faengt sie vorher ab. Laut sagen, damit es auffaellt.
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        for (const QString& key : action.defaultKeys) {
            const QString owner = HotKeyBindings::reservedBy(key);
            if (!owner.isEmpty()) {
                qCWarning(lcHotKeys) << "Vorgabetaste" << key << "fuer" << action.id
                                     << "gehoert schon" << owner;
            }
        }
    }

    bindings->load();

    m_hotKeyTargets = std::make_unique<MainWindowHotKeyTargets>(this);
    m_hotKeyDispatcher = std::make_unique<HotKeyDispatcher>(bindings, m_hotKeyTargets.get());
    m_keyboardRouter = std::make_unique<KeyboardRouter>(m_hotKeyDispatcher.get(), bindings, this);

    // Geht der Sender auf anderem Weg aus, hat eine gehaltene Sprechtaste
    // keinen Anspruch mehr auf das Loslassen.
    if (MoxController* const mox = m_radioModel->moxController()) {
        HotKeyDispatcher* const dispatcher = m_hotKeyDispatcher.get();
        connect(mox, &MoxController::moxStateChanged, dispatcher, [dispatcher](bool on) {
            if (!on) {
                dispatcher->onTransmitWentOff();
            }
        });
    }

    // Was der Sprachspeicher unaufgefordert zu sagen hat (Aufnahme voll,
    // naechster Ruf ging nicht hinaus).
    if (VoiceKeyerPlayer* const player = m_radioModel->voiceKeyerPlayer()) {
        connect(player, &VoiceKeyerPlayer::notice, this, [this](const QString& text) {
            if (m_shuttingDown) {
                return;
            }
            showToast(text, ToastSeverity::Warning, 5000);
        });
    }

    // Jede Aenderung auf der Setup-Seite gleich sichern -- erst NACH dem
    // Laden verbunden, damit der Start selbst nichts schreibt.
    connect(bindings, &HotKeyBindings::bindingsChanged, this, [bindings]() { bindings->save(); });
    connect(bindings, &HotKeyBindings::enabledChanged, this, [bindings]() { bindings->save(); });
    connect(bindings, &HotKeyBindings::fKeysFirstChanged, this, [bindings]() { bindings->save(); });

    // ── Am Mac: F1…F12 als F-Tasten, solange Longpath vorne ist ──────
    //
    // Betreiber 2026-11-11: „bitte am Mac die F1-F2 Tasten priorisieren"
    // -- ab Werk liefern sie Helligkeit und Co., der Sprachspeicher kam
    // nur mit fn. Ein Pruefstand und das Fenster ohne Schirm fassen den
    // Tastaturmodus des Rechners nie an: sie bekommen den leeren Weg.
    const bool echt = !QStandardPaths::isTestModeEnabled()
                      && QGuiApplication::platformName() != QLatin1String("offscreen");
    m_fKeyPriority = std::make_unique<FKeyPriority>(
        echt ? FKeyPriority::systemBackend() : FKeyPriority::Backend{},
        echt ? FKeyPriority::defaultMarkerPath() : QString());
    FKeyPriority* const fKeys = m_fKeyPriority.get();
    const auto wunsch = [fKeys, bindings]() {
        fKeys->setWanted(bindings->enabled() && bindings->fKeysFirst());
    };
    connect(bindings, &HotKeyBindings::enabledChanged, this, wunsch);
    connect(bindings, &HotKeyBindings::fKeysFirstChanged, this, wunsch);

    // ── Durchhalten, solange Longpath laeuft (2026-11-14) ─────────────
    //
    // Betreiber: „Ja, durchgehend". Nur-solange-vorne hat den Mac
    // zweimal in drei Tagen stumm gemacht: F10 ist am Mac die
    // Stummtaste, und zwischen jedem Fokuswechsel und dem Tastendruck
    // lag ein Fenster, in dem der Modus wieder auf der Mac-Belegung
    // stand -- im Protokoll vom 14.11. sechsmal in vier Minuten.
    // Begruendung und Preis stehen im Kopf von FKeyPriority.h.
    const auto durchhalten = [fKeys, bindings]() {
        fKeys->setHoldWhileRunning(bindings->fKeysWhileRunning());
    };
    connect(bindings, &HotKeyBindings::fKeysWhileRunningChanged, this, durchhalten);
    durchhalten();

    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            [fKeys](Qt::ApplicationState state) { fKeys->setActive(state == Qt::ApplicationActive); });
    connect(qGuiApp, &QCoreApplication::aboutToQuit, this, [fKeys]() { fKeys->release(); });
    connect(fKeys, &FKeyPriority::failed, this, [this](const QString& text) {
        if (!m_shuttingDown) {
            showToast(text, ToastSeverity::Warning, 6000);
        }
    });
    wunsch();
    fKeys->setActive(QGuiApplication::applicationState() == Qt::ApplicationActive);

    qCInfo(lcHotKeys) << "Tastenkuerzel eingerichtet:" << HotKeyActionList::all().size()
                      << "Handlungen," << reserved.size() << "Menue-Kuerzel gesperrt,"
                      << (bindings->enabled() ? "an" : "aus");
}

void MainWindow::stimmeVomBediener(SliceModel* s, double hz)
{
    if (!s) { return; }

    // ── Eine Sperre muss sich melden ────────────────────────────────
    //
    // SliceModel::setFrequency() beginnt mit "if (m_locked)
    // return;" — wortlos. Der Betreiber am 2026-08-22: "balken
    // spring nicht dort hin." Im Quelltext und im echten Fenster
    // stimmt der Klick nachweislich ab (gemessen: 7,100 ->
    // 7,128 MHz), live tut er es nicht — und eine gesperrte
    // Scheibe ist der einzige Weg, auf dem das schweigend
    // passieren kann.
    //
    // Ob es diesmal die Ursache war, weiss ich nicht. Aber ein
    // Bedienelement, das ohne ein Wort nichts tut, hat uns heute
    // schon Stunden gekostet — und wenn es nicht die Ursache ist,
    // erscheint der Hinweis nie und wir haben sie ausgeschlossen.
    if (s->locked()) {
        showToast(tr("This slice's frequency is locked — the padlock in the VFO bar "
                      "releases it."),
                  ToastSeverity::Info, 5000);
        return;
    }
    s->setFrequency(hz);

    // ── Und das Begleitgeraet mit (2026-10-28) ───────────────────────
    //
    // Der Klick geht in BEIDE Richtungen: Longpaths eigene Scheibe
    // und, wenn eingeschaltet, das fremde Funkgeraet. Das ist der
    // Nutzen des Panadapters an einer ZF-Station -- der Betreiber
    // sieht das Signal hier und hoert es dort.
    //
    // Nur hier, nicht bei jeder Frequenzaenderung der Scheibe. Ein
    // Band- oder Speicherwechsel, ein Drehknopf, ein Spot: das
    // alles bewegt die Scheibe ebenfalls, und wuerde man das
    // weiterschicken, liefen Programm und Geraet gegeneinander,
    // sobald das Geraet auch zurueckmeldet. Ein Klick ist eine
    // Ansage des Bedieners, kein Zustand -- und eine Pfeiltaste
    // (seit 2026-11-10 ueber diesen Weg) ebenso.
    //
    // Die Zahl geht unverwandelt hinueber: hz ist eine Frequenz auf
    // DIESER Achse, also die ZF, und genau die will das Geraet.
    if (m_radioModel && m_radioModel->companionRig()) {
        m_radioModel->companionRig()->stimmeAbZf(std::llround(hz));
    }
}

void MainWindow::panoramaZoomen(double factor)
{
    // Faktor 1.5 je Druck — AetherSDRs Wert
    // (SpectrumWidget.cpp:2205-2206 [@0cd4559]). Untergrenze 2 kHz
    // (darunter sieht man nur noch die eigene Bandbreite), Obergrenze
    // die Abtastrate (mehr liefert der Empfaenger nicht). Stand bis
    // 2026-11-10 als Lambda am Anschluss der Lupe in MainWindow.cpp.
    SpectrumWidget* sw = activeSpectrumWidget();
    if (!sw) { return; }
    const double maxBw = sw->sampleRate() > 0.0 ? sw->sampleRate() : 384000.0;
    const double bw = qBound(2000.0, sw->bandwidth() * factor, maxBw);
    sw->setFrequencyRange(sw->centerFrequency(), bw);
}

} // namespace Longpath
