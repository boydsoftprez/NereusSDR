#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableFirstRunDialog.h  (NereusSDR)
// =================================================================
//
// The first time a station opens Roundtable (design 11.2 First run, U5,
// D21, D25): "Roundtable is public", what goes public, Hide my station and
// Stay hidden / Go live, the switch moving the main button between them.
// Either button answers whatever the switch says. Until someone answers,
// the Core sends nothing about the station.
//
// Asked once per station: the Core keeps the answer. A computer that opens
// Roundtable after another device answered shows a one-time note instead
// (Mode::Note). Whether this computer has asked or shown the note is
// Roundtable/FirstRunSeen, this computer's own setting.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 12).
// =================================================================

#include "gui/NereusGuiExport.h"

#include <QDialog>
#include <QPointer>

class QLabel;
class QPushButton;

namespace NereusSDR {

class RoundtableModel;

class NEREUS_GUI_EXPORT RoundtableFirstRunDialog : public QDialog {
    Q_OBJECT

public:
    enum class Mode {
        Question, // nobody has answered: Stay hidden or Go live
        Note,     // another device answered; said once on this computer
    };

    /// This computer has asked the question or shown the note ("True").
    static constexpr const char* kSeenKey = "Roundtable/FirstRunSeen";

    /// Whether opening Roundtable here shows this dialog: Roundtable is
    /// reachable through `model`, and either nobody has answered or this
    /// computer has not yet shown the note.
    static bool shouldShow(const RoundtableModel* model);

    /// Question mode while the station is unanswered, else Note mode. Not
    /// owned.
    explicit RoundtableFirstRunDialog(RoundtableModel* model, QWidget* parent = nullptr);
    ~RoundtableFirstRunDialog() override;

    Mode mode() const { return m_mode; }
    bool hideSwitchOn() const;
    /// The Hide my station switch (the main button follows it).
    void setHideSwitch(bool on);

private:
    void buildQuestion(QWidget* page);
    void buildNote(QWidget* page);
    void applySwitch();
    void answer(bool live);
    void onFirstRunChanged();
    void onRefused(const QString& verb, const QString& reason);
    void showNote();
    static void markSeen();

    QPointer<RoundtableModel> m_model;
    Mode m_mode = Mode::Question;
    bool m_sent = false;

    QWidget* m_question = nullptr;
    QWidget* m_note = nullptr;
    QPushButton* m_hideSwitch = nullptr;
    QPushButton* m_goLive = nullptr;
    QPushButton* m_stayHidden = nullptr;
    QLabel* m_error = nullptr;
    QLabel* m_noteHead = nullptr;
};

} // namespace NereusSDR
