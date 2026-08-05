// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - tst_mode_menu_rade: RADE mode entry in user-facing
// mode selectors (Phase 3R L3).
//
// NEW NereusSDR-native UI extension; no upstream Thetis equivalent
// (Thetis has no RADE mode — it's a NereusSDR-native addition wired
// to the FreeDV RADE neural codec port at Phase 3R).
//
// Two surfaces are pinned here:
//
// 1. VfoWidget mode combo (the in-flag mode selector at the head of
//    the Mode tab) must include both "RADE-U" and "RADE-L" so the
//    user can click into either sideband from the floating VFO flag.
//    Like USB/LSB, RADE has upper/lower sideband variants.
//
// 2. VfoWidget mode tab button (tab #2 — the floating-flag tab that
//    displays the active mode name) must paint with the RADE purple
//    accent (#a78bfa) when m_currentMode is either RADE sideband
//    (DSPMode::RADE_U or DSPMode::RADE_L).
//
// Mode menu (MainWindow) test is omitted — MainWindow construction
// pulls in the full RadioModel + WdspEngine + connection stack, which
// is heavier than a unit test should carry, and the existing
// tst_dspmode_rade already pins modeName / modeFromName for the
// enum values the menu dispatches.

#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QPushButton>

#include "gui/widgets/VfoWidget.h"
#include "core/WdspTypes.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestModeMenuRade : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    // VFO flag mode combo includes both RADE sidebands.
    void vfoModeComboHasRadeUpperEntry();
    void vfoModeComboHasRadeLowerEntry();
    // Selecting either RADE sideband in the combo emits modeChanged.
    void vfoModeComboChangesToRadeUpper();
    void vfoModeComboChangesToRadeLower();
    // Mode tab button label tracks the active mode name.
    void vfoModeTabLabelShowsRadeUpperWhenActive();
    void vfoModeTabLabelShowsRadeLowerWhenActive();
    // Mode tab button stylesheet contains the RADE purple accent when
    // the active mode is either RADE sideband.
    void vfoModeTabChipPurpleInRadeUpper();
    void vfoModeTabChipPurpleInRadeLower();
};

void TestModeMenuRade::initTestCase()
{
    if (!qApp) {
        static int   argc = 0;
        static char* argv = nullptr;
        new QApplication(argc, &argv);
    }
}

namespace {

// Helper: walk VfoWidget's children for the mode combo. The widget
// exposes setMode/setSlice but does not expose the combo directly.
// QComboBox is the only QComboBox child inside the Mode tab page.
QComboBox* findModeCombo(VfoWidget* vfo)
{
    const QList<QComboBox*> combos = vfo->findChildren<QComboBox*>();
    // Mode combo is identifiable by its USB default-text.  Other combos
    // (VAX channel selector) are children of separate pages.
    for (QComboBox* c : combos) {
        for (int i = 0; i < c->count(); ++i) {
            if (c->itemText(i) == QStringLiteral("USB") &&
                c->itemText((i+1) % c->count()) != QStringLiteral("USB"))
            {
                return c;
            }
        }
    }
    // Fallback: return the first combo found.
    return combos.isEmpty() ? nullptr : combos.first();
}

// Helper: locate the mode tab button (index 2 in the tab bar — the one
// that displays the active mode name like "USB" / "RADE-U" / "RADE-L").
QPushButton* findModeTabButton(VfoWidget* vfo)
{
    const QList<QPushButton*> btns = vfo->findChildren<QPushButton*>();
    // Mode tab button text matches the current mode name.  By default the
    // widget seeds with USB, so look for a button whose text matches a
    // valid mode name.  After setMode(RADE_U) the text becomes "RADE-U".
    for (QPushButton* b : btns) {
        const QString t = b->text();
        if (t == QStringLiteral("USB")
            || t == QStringLiteral("RADE-U")
            || t == QStringLiteral("RADE-L")) {
            return b;
        }
    }
    return nullptr;
}

}  // namespace

void TestModeMenuRade::vfoModeComboHasRadeUpperEntry()
{
    VfoWidget vfo;
    QComboBox* combo = findModeCombo(&vfo);
    QVERIFY(combo != nullptr);

    QStringList items;
    for (int i = 0; i < combo->count(); ++i) {
        items << combo->itemText(i);
    }
    QVERIFY2(items.contains(QStringLiteral("RADE-U")),
             "Mode combo must include the RADE-U entry (Phase 3R L3)");
}

void TestModeMenuRade::vfoModeComboHasRadeLowerEntry()
{
    VfoWidget vfo;
    QComboBox* combo = findModeCombo(&vfo);
    QVERIFY(combo != nullptr);

    QStringList items;
    for (int i = 0; i < combo->count(); ++i) {
        items << combo->itemText(i);
    }
    QVERIFY2(items.contains(QStringLiteral("RADE-L")),
             "Mode combo must include the RADE-L entry (Phase 3R L3)");
}

void TestModeMenuRade::vfoModeComboChangesToRadeUpper()
{
    VfoWidget vfo;
    QComboBox* combo = findModeCombo(&vfo);
    QVERIFY(combo != nullptr);

    QSignalSpy spy(&vfo, &VfoWidget::modeChanged);
    combo->setCurrentText(QStringLiteral("RADE-U"));
    QVERIFY2(spy.count() >= 1,
             "Selecting RADE-U in the combo must emit modeChanged");
    const DSPMode mode = qvariant_cast<DSPMode>(spy.last().at(0));
    QVERIFY(mode == DSPMode::RADE_U);
}

void TestModeMenuRade::vfoModeComboChangesToRadeLower()
{
    VfoWidget vfo;
    QComboBox* combo = findModeCombo(&vfo);
    QVERIFY(combo != nullptr);

    QSignalSpy spy(&vfo, &VfoWidget::modeChanged);
    combo->setCurrentText(QStringLiteral("RADE-L"));
    QVERIFY2(spy.count() >= 1,
             "Selecting RADE-L in the combo must emit modeChanged");
    const DSPMode mode = qvariant_cast<DSPMode>(spy.last().at(0));
    QVERIFY(mode == DSPMode::RADE_L);
}

void TestModeMenuRade::vfoModeTabLabelShowsRadeUpperWhenActive()
{
    VfoWidget vfo;
    vfo.setMode(DSPMode::RADE_U);
    QPushButton* tabBtn = findModeTabButton(&vfo);
    QVERIFY(tabBtn != nullptr);
    QCOMPARE(tabBtn->text(), QStringLiteral("RADE-U"));
}

void TestModeMenuRade::vfoModeTabLabelShowsRadeLowerWhenActive()
{
    VfoWidget vfo;
    vfo.setMode(DSPMode::RADE_L);
    QPushButton* tabBtn = findModeTabButton(&vfo);
    QVERIFY(tabBtn != nullptr);
    QCOMPARE(tabBtn->text(), QStringLiteral("RADE-L"));
}

void TestModeMenuRade::vfoModeTabChipPurpleInRadeUpper()
{
    VfoWidget vfo;
    vfo.setMode(DSPMode::RADE_U);
    QPushButton* tabBtn = findModeTabButton(&vfo);
    QVERIFY(tabBtn != nullptr);
    QVERIFY2(tabBtn->styleSheet().contains(QStringLiteral("#a78bfa"),
                                            Qt::CaseInsensitive),
             "Mode tab button must paint with the RADE purple accent "
             "(#a78bfa) when the active mode is RADE-U");
}

void TestModeMenuRade::vfoModeTabChipPurpleInRadeLower()
{
    VfoWidget vfo;
    vfo.setMode(DSPMode::RADE_L);
    QPushButton* tabBtn = findModeTabButton(&vfo);
    QVERIFY(tabBtn != nullptr);
    QVERIFY2(tabBtn->styleSheet().contains(QStringLiteral("#a78bfa"),
                                            Qt::CaseInsensitive),
             "Mode tab button must paint with the RADE purple accent "
             "(#a78bfa) when the active mode is RADE-L");
}

QTEST_MAIN(TestModeMenuRade)
#include "tst_mode_menu_rade.moc"
