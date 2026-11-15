// =================================================================
// tests/tst_hot_key_window.cpp  (Longpath)
// =================================================================
//
// Die Tastenkuerzel am echten Hauptfenster, mit echten Tastendrucken
// (QTest::keyClick geht durch die Anwendung und damit durch die
// Tastaturweiche) -- nicht durch einen Aufruf des Verteilers. Ein
// Pruefstand, der die Funktion ruft, beweist die halbe Strecke.
//
// Geprueft:
//   * eine Taste wirkt auf den aktiven Empfaenger (Pfeil, R, B, L, ])
//   * in einem Textfeld bleiben die Tasten beim Textfeld -- ausser den
//     F-Tasten und Esc (N1MM: senden, waehrend man das Rufzeichen tippt;
//     2026-11-11)
//   * der Ausschalter schaltet wirklich aus
//   * Esc ohne laufende Ansage wird NICHT geschluckt
//   * F1 ohne Funkgeraet sendet nichts
//   * am Mac F1…F12 als F-Tasten nur, solange Longpath vorne ist, und
//     der Pruefstand selbst fasst den Tastaturmodus des Rechners nie an
//     (2026-11-11)
//   * kein Menue-Kuerzel ist doppelt vergeben (Strg+Umschalt+K und
//     Strg+Umschalt+R waren es bis 2026-11-10 -- Qt loest dann keines
//     von beiden aus), und die Sperrliste kennt die Menue-Kuerzel
//
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-11 — F-Tasten zuerst (FKeyPriority), KI-gestuetzt ueber
//                 Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/WdspTypes.h"
#include "core/audio/VoiceKeyerPlayer.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/MainWindow.h"
#include "gui/hotkeys/FKeyPriority.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QAction>
#include <QApplication>
#include <QHash>
#include <QKeyEvent>
#include <QLineEdit>
#include <QShortcut>

using namespace Longpath;

namespace {

// Ein Textfeld, das mitschreibt, welche Tasten bei ihm ankommen.
class FeldZeuge : public QLineEdit
{
public:
    explicit FeldZeuge(QWidget* parent) : QLineEdit(parent) {}
    QList<int> gesehen;

protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        gesehen.append(e->key());
        QLineEdit::keyPressEvent(e);
    }
};

SliceModel* scheibe(MainWindow& mw)
{
    RadioModel* rm = mw.radioModelForTest();
    if (!rm) { return nullptr; }
    if (SliceModel* s = rm->activeSlice()) { return s; }
    const int id = rm->addSlice();
    rm->setActiveSlice(id);
    return rm->activeSlice();
}

// Das Fenster muss die Tastatur haben: die Weiche nimmt nur Tasten
// des aktiven Fensters. Der offscreen-Treiber gibt die Aktivierung
// nicht immer gleich her -- so lange fragen, bis sie da ist (Muster aus
// Upstream NereusSDR tests/HotKeyWindowActivation.h).
bool aktiv(QWidget* w)
{
    w->show();
    w->activateWindow();
    return QTest::qWaitFor([w] {
        if (QApplication::activeWindow() == w) { return true; }
        w->activateWindow();
        return false;
    }, 5000);
}

// Eine Taste dorthin, wo sie ein Mensch hinschickt: an das Widget mit
// dem Fokus, sonst an das Fenster.
void taste(MainWindow& mw, Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QWidget* ziel = QApplication::focusWidget();
    if (!ziel || ziel->window() != &mw) { ziel = &mw; }
    QTest::keyClick(ziel, key, mods);
}

// Ein Widget, das mitschreibt, welche Tasten bei ihm ankommen.
class TastenZeuge : public QWidget
{
public:
    explicit TastenZeuge(QWidget* parent) : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        resize(20, 20);
    }
    QList<int> gesehen;

protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        gesehen.append(e->key());
        QWidget::keyPressEvent(e);
    }
};

} // namespace

class TstHotKeyWindow : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        AppSettings::instance().clear();
    }

    void eineTasteWirktAufDenAktivenEmpfaenger()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY2(s != nullptr, "keine aktive Scheibe");
        QVERIFY2(aktiv(&mw), "Fenster wird nicht aktiv");
        mw.setFocus();

        s->setLocked(false);
        s->setStepHz(100);
        s->setFrequency(7'100'000.0);

        taste(mw, Qt::Key_Right);
        QCOMPARE(qint64(s->frequency()), qint64(7'100'100));
        // Umschalt: zehn Schritte. Erst nach der 20-ms-Frist, sonst
        // buendelt der Verteiler.
        QTest::qWait(30);
        taste(mw, Qt::Key_Left, Qt::ShiftModifier);
        QTRY_COMPARE(qint64(s->frequency()), qint64(7'099'100));

        // ] macht den Schritt groesser.
        taste(mw, Qt::Key_BracketRight);
        QCOMPARE(s->stepHz(), 500);

        // R schaltet die Rauschminderung ein und wieder aus.
        s->setActiveNr(NrSlot::Off);
        taste(mw, Qt::Key_R);
        QVERIFY(s->activeNr() != NrSlot::Off);
        taste(mw, Qt::Key_R);
        QCOMPARE(s->activeNr(), NrSlot::Off);

        // A: Auto-Notch.
        const bool anf = s->anfEnabled();
        taste(mw, Qt::Key_A);
        QCOMPARE(s->anfEnabled(), !anf);

        // L sperrt, und dann stimmt der Pfeil nicht mehr ab.
        taste(mw, Qt::Key_L);
        QVERIFY(s->locked());
        const double vorher = s->frequency();
        QTest::qWait(30);
        taste(mw, Qt::Key_Right);
        QCOMPARE(s->frequency(), vorher);
        taste(mw, Qt::Key_L);
        QVERIFY(!s->locked());
    }

    void imTextfeldBleibenDieTastenBeimText()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);
        auto* feld = new QLineEdit(&mw);
        feld->show();
        QVERIFY(aktiv(&mw));
        feld->setFocus();
        QTRY_VERIFY(feld->hasFocus());

        s->setFrequency(7'100'000.0);
        s->setActiveNr(NrSlot::Off);
        QTest::keyClick(feld, Qt::Key_R);
        QTest::keyClick(feld, Qt::Key_Right);
        QCOMPARE(feld->text(), QStringLiteral("r"));
        QCOMPARE(s->activeNr(), NrSlot::Off);
        QCOMPARE(qint64(s->frequency()), qint64(7'100'000));
    }

    void fTastenGeltenAuchBeimTippen()
    {
        // Rufzeichen tippen und mit F1 rufen, wie in N1MM. Das Feld
        // bekommt die Buchstaben, die Weiche die F-Tasten. Esc ohne
        // laufende Ansage bleibt beim Feld.
        MainWindow mw;
        QVERIFY(scheibe(mw) != nullptr);
        auto* feld = new FeldZeuge(&mw);
        feld->show();
        QVERIFY(aktiv(&mw));
        feld->setFocus();
        QTRY_VERIFY(feld->hasFocus());

        QTest::keyClicks(feld, QStringLiteral("oe5"));
        QTest::keyClick(feld, Qt::Key_F1);
        QTest::keyClick(feld, Qt::Key_F2, Qt::ControlModifier | Qt::ShiftModifier);
        QTest::keyClick(feld, Qt::Key_Escape);
        QCOMPARE(feld->text(), QStringLiteral("oe5"));
        QVERIFY(!feld->gesehen.contains(Qt::Key_F1));
        QVERIFY(!feld->gesehen.contains(Qt::Key_F2));
        QVERIFY(feld->gesehen.contains(Qt::Key_Escape));
    }

    void derAusschalterSchaltetAus()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);
        QVERIFY(aktiv(&mw));
        mw.setFocus();
        HotKeyBindings* b = mw.radioModelForTest()->hotKeyBindings();
        QVERIFY(b != nullptr);

        b->setEnabled(false);
        s->setActiveNr(NrSlot::Off);
        taste(mw, Qt::Key_R);
        QCOMPARE(s->activeNr(), NrSlot::Off);
        b->setEnabled(true);
        taste(mw, Qt::Key_R);
        QVERIFY(s->activeNr() != NrSlot::Off);
    }

    void escOhneAnsageBleibtEsc()
    {
        // Esc stoppt eine Ansage. Laeuft keine, geht Esc weiter an das
        // Widget mit dem Fokus, als gaebe es die Belegung nicht. Eine
        // belegte Taste (R) dagegen nimmt die Weiche weg.
        MainWindow mw;
        QVERIFY(scheibe(mw) != nullptr);
        auto* zeuge = new TastenZeuge(&mw);
        zeuge->show();
        QVERIFY(aktiv(&mw));
        zeuge->setFocus();
        QTRY_VERIFY(zeuge->hasFocus());
        QVERIFY(!mw.hotKeyDispatcher()->takesKey(QStringLiteral("voice.stop")));

        QTest::keyClick(zeuge, Qt::Key_Escape);
        QVERIFY(zeuge->gesehen.contains(Qt::Key_Escape));
        QTest::keyClick(zeuge, Qt::Key_R);
        QVERIFY(!zeuge->gesehen.contains(Qt::Key_R));
        QCOMPARE(mw.radioModelForTest()->voiceKeyerPlayer()->state(),
                 VoiceKeyerPlayer::State::Idle);
    }

    void f1OhneFunkgeraetSendetNichts()
    {
        MainWindow mw;
        QVERIFY(scheibe(mw) != nullptr);
        QVERIFY(aktiv(&mw));
        mw.setFocus();
        RadioModel* rm = mw.radioModelForTest();
        taste(mw, Qt::Key_F1);
        QCOMPARE(rm->voiceKeyerPlayer()->state(), VoiceKeyerPlayer::State::Idle);
        QVERIFY(!rm->moxController() || !rm->moxController()->isMox());
        // Leertaste: Sprechtaste -- ohne Funkgeraet ebenfalls nichts.
        taste(mw, Qt::Key_Space);
        QVERIFY(!rm->moxController() || !rm->moxController()->isMox());
    }

    void fTastenSolangeLongpathLaeuft()
    {
        int modus = FKeyPriority::kModeSpecial;
        {
            MainWindow mw;
            QVERIFY(scheibe(mw) != nullptr);
            FKeyPriority* fk = mw.fKeyPriority();
            QVERIFY(fk != nullptr);
            HotKeyBindings* b = mw.radioModelForTest()->hotKeyBindings();

            // Der Pruefstand hat den leeren Weg: vorne und gewuenscht, und
            // trotzdem nichts umgestellt.
            QVERIFY(aktiv(&mw));
            QTRY_COMPARE(QGuiApplication::applicationState(), Qt::ApplicationActive);
            QVERIFY(!fk->engaged());

            fk->setBackendForTest(FKeyPriority::Backend{
                [&modus]() { return modus; },
                [&modus](int m) { modus = m; return true; }});
            QCOMPARE(modus, FKeyPriority::kModeFKeys);

            // ── Durchhalten ist ab 2026-11-14 die Vorgabe ────────────
            //
            // Betreiber: „Ja, durchgehend". Ein anderes Programm vorne
            // loest die Umstellung NICHT mehr -- genau diese Luecke hat
            // den Mac zweimal in drei Tagen stumm gemacht, weil F10 am
            // Mac die Stummtaste ist.
            QVERIFY(b->fKeysWhileRunning());
            emit qGuiApp->applicationStateChanged(Qt::ApplicationInactive);
            QCOMPARE(modus, FKeyPriority::kModeFKeys);
            emit qGuiApp->applicationStateChanged(Qt::ApplicationActive);
            QCOMPARE(modus, FKeyPriority::kModeFKeys);

            // Und der alte Weg bleibt erreichbar: Durchhalten aus, dann
            // zaehlt wieder nur das vordere Programm. Ohne diesen Teil
            // waere der Schalter auf der Setup-Seite ungeprueft.
            b->setFKeysWhileRunning(false);
            emit qGuiApp->applicationStateChanged(Qt::ApplicationInactive);
            QCOMPARE(modus, FKeyPriority::kModeSpecial);
            emit qGuiApp->applicationStateChanged(Qt::ApplicationActive);
            QCOMPARE(modus, FKeyPriority::kModeFKeys);
            b->setFKeysWhileRunning(true);

            // Beide Schalter auf der Setup-Seite.
            b->setFKeysFirst(false);
            QCOMPARE(modus, FKeyPriority::kModeSpecial);
            b->setFKeysFirst(true);
            QCOMPARE(modus, FKeyPriority::kModeFKeys);
            b->setEnabled(false);
            QCOMPARE(modus, FKeyPriority::kModeSpecial);
            b->setEnabled(true);
            QCOMPARE(modus, FKeyPriority::kModeFKeys);
        }
        // Das Fenster geht: zurueckgestellt.
        QCOMPARE(modus, FKeyPriority::kModeSpecial);
    }

    void keinMenueKuerzelIstDoppelt()
    {
        MainWindow mw;
        QHash<QString, QString> gesehen;
        QStringList doppelt;
        const auto merke = [&](const QKeySequence& seq, const QString& wer) {
            if (seq.isEmpty()) { return; }
            const QString t = seq.toString(QKeySequence::PortableText);
            if (gesehen.contains(t) && gesehen.value(t) != wer) {
                doppelt << QStringLiteral("%1: %2 / %3").arg(t, gesehen.value(t), wer);
            }
            gesehen.insert(t, wer);
        };
        for (QAction* a : mw.findChildren<QAction*>()) {
            if (!a->isVisible()) { continue; }
            for (const QKeySequence& seq : a->shortcuts()) {
                merke(seq, a->text());
            }
        }
        for (QShortcut* sc : mw.findChildren<QShortcut*>()) {
            merke(sc->key(), QStringLiteral("QShortcut"));
        }
        QVERIFY2(doppelt.isEmpty(), qPrintable(doppelt.join(QStringLiteral("; "))));

        // Und die Sperrliste kennt sie.
        QCOMPARE(HotKeyBindings::reservedBy(QStringLiteral("Ctrl+Shift+R")),
                 QStringLiteral("Remove active slice"));
        QVERIFY(!HotKeyBindings::reservedBy(QStringLiteral("Ctrl+Shift+K")).isEmpty());
        QVERIFY(!HotKeyBindings::reservedBy(QStringLiteral("Ctrl+Shift+X")).isEmpty());
        QVERIFY(!HotKeyBindings::reservedBy(QStringLiteral("Ctrl+Q")).isEmpty());
        // Keine Vorgabetaste gehoert einem Menue.
        HotKeyBindings* b = mw.radioModelForTest()->hotKeyBindings();
        for (const QString& k : {QStringLiteral("F1"), QStringLiteral("Esc"), QStringLiteral("Ctrl+T"),
                                 QStringLiteral("Ctrl+Shift+M"), QStringLiteral("Ctrl+Shift+F1"),
                                 QStringLiteral("Space"), QStringLiteral("R")}) {
            QVERIFY2(HotKeyBindings::reservedBy(k).isEmpty(), qPrintable(k));
            QVERIFY2(!b->actionForChord(k).isEmpty(), qPrintable(k));
        }
    }
};

QTEST_MAIN(TstHotKeyWindow)
#include "tst_hot_key_window.moc"
