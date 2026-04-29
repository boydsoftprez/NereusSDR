#pragma once

// =================================================================
// src/gui/setup/TxProfileSetupPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → TX Profile editor page.
// No Thetis port for the Qt widgets themselves (per memory:
// feedback_source_first_ui_vs_dsp — Setup pages are NereusSDR-native).
// The save / delete / focus-gated unsaved-prompt SEMANTICS mirror Thetis:
//
//   setup.cs:9505-9543 [v2.10.3.13] — comboTXProfileName_SelectedIndexChanged
//   setup.cs:9545-9612 [v2.10.3.13] — btnTXProfileSave_Click
//   setup.cs:9615-9656 [v2.10.3.13] — btnTXProfileDelete_Click
//
// MicProfileManager (Phase F) holds the AppSettings I/O and last-profile
// guard.  This page is the user-facing front-end.
//
// Phase 3M-1c chunk J (J.3 + J.4):
//
//   J.3 — Editor surface
//     • Editing combo with profile names from MicProfileManager.
//     • Save button → QInputDialog::getText for the new name; overwrite
//       confirm via QMessageBox::question; saveProfile dispatch.
//     • Delete button → QMessageBox::question confirm; deleteProfile dispatch
//       with the verbatim Thetis "It is not possible to delete the last
//       remaining TX profile" string surfaced via QMessageBox::information
//       on the last-remaining-profile path.
//
//   J.4 — Focus-gated unsaved-changes prompt
//     • Combo selection change while the user has focus AND the current
//       profile has been modified since load → prompt:
//         Yes → save current then load new
//         No  → discard current then load new
//         Cancel → revert combo to previous selection
//     • Programmatic combo changes (no focus) skip the prompt entirely.
//     • Dirty-tracking via subscription to TransmitModel *Changed signals
//       (set on load, cleared on save/load/setActiveProfile).
//
// Test seams (no NEREUS_BUILD_TESTS guard — same convention as
// TestTwoTonePage / AudioTxInputPage):
//   • setSavePromptHook(...)         — replace QInputDialog::getText.
//   • setOverwriteConfirmHook(...)   — replace QMessageBox::question.
//   • setDeleteConfirmHook(...)      — replace delete QMessageBox::question.
//   • setRejectionMessageHook(...)   — capture last-profile rejection msg.
//   • setUnsavedPromptHook(...)      — replace the J.4 prompt.
//   • simulateUserComboChangeForTest — drive a "user-driven" change without
//                                       needing real focus events.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-29 — Phase 3M-1c chunk J.3+J.4 implementation by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.  Setup → Audio → TX Profile editor surface
//                 mirroring the Thetis save / delete / unsaved-prompt
//                 semantics; UI is NereusSDR-native.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis-derived semantics are
// cited inline where they apply.

#include "gui/SetupPage.h"

#include <QString>
#include <functional>

class QComboBox;
class QLabel;
class QPushButton;

namespace NereusSDR {

class MicProfileManager;
class RadioModel;
class TransmitModel;

// ---------------------------------------------------------------------------
// TxProfileSetupPage — editor for the TX-Profile bank.
//
// Construction:
//   TxProfileSetupPage(model, profileMgr, transmitModel, parent)
//
// All three pointers may be null (for the SetupDialog::buildTree() pre-Phase-L
// path, where MicProfileManager isn't yet attached to RadioModel).  When null,
// the page renders the controls but every action is a no-op.
//
// Phase L will populate MicProfileManager and TransmitModel via constructor
// at SetupDialog construction time.
// ---------------------------------------------------------------------------
class TxProfileSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit TxProfileSetupPage(RadioModel* model,
                                 MicProfileManager* profileMgr,
                                 TransmitModel* tx,
                                 QWidget* parent = nullptr);
    ~TxProfileSetupPage() override = default;

    // ── Test accessors (always-on) ──────────────────────────────────────────
    QComboBox*   profileCombo() const { return m_combo; }
    QPushButton* saveButton()   const { return m_saveBtn; }
    QPushButton* deleteButton() const { return m_deleteBtn; }

    // ── Test seams ──────────────────────────────────────────────────────────
    // (No NEREUS_BUILD_TESTS guard — same convention as TestTwoTonePage.)
    //
    // SavePromptHook: replaces QInputDialog::getText for the save dialog.
    // Returns {accepted, name}; name is the user-typed string.  Empty / all-
    // whitespace name is treated as cancel (no save) by the page itself.
    struct SavePromptResult {
        bool    accepted{false};
        QString name;
    };
    using SavePromptHook = std::function<SavePromptResult(const QString& currentName)>;
    void setSavePromptHook(SavePromptHook hook);

    // OverwriteConfirmHook: replaces the "Overwrite existing profile?"
    // QMessageBox::question.  Returns true to overwrite, false to skip.
    using OverwriteConfirmHook = std::function<bool()>;
    void setOverwriteConfirmHook(OverwriteConfirmHook hook);

    // DeleteConfirmHook: replaces the "Delete this profile?" QMessageBox::
    // question.  Returns true to confirm, false to abort.
    using DeleteConfirmHook = std::function<bool()>;
    void setDeleteConfirmHook(DeleteConfirmHook hook);

    // RejectionMessageHook: replaces the QMessageBox::information surfaced
    // when the last-remaining-profile guard refuses a delete.  Receives the
    // verbatim Thetis string.
    using RejectionMessageHook = std::function<void(const QString& message)>;
    void setRejectionMessageHook(RejectionMessageHook hook);

    // UnsavedPromptHook: replaces the J.4 unsaved-changes
    // QMessageBox::question(Yes/No/Cancel).  Receives the active profile
    // name being edited.  Returns the user's choice.
    enum class UnsavedPromptResult { Yes, No, Cancel };
    using UnsavedPromptHook = std::function<UnsavedPromptResult(const QString& currentName)>;
    void setUnsavedPromptHook(UnsavedPromptHook hook);

    // simulateUserComboChangeForTest: drive a user-equivalent combo change
    // without needing the combo to actually have focus.  Invokes the same
    // unsaved-prompt flow that the focus-gated path runs.  newName must be
    // an existing profile name.
    void simulateUserComboChangeForTest(const QString& newName);

private slots:
    // Combo selection change handler.  Inspects the focus state (or the
    // m_simulateUserChangeForTest flag) and either runs the J.4 prompt
    // flow or treats this as a programmatic change (loads the new profile
    // unconditionally, no prompt).
    void onComboTextChanged(const QString& newName);

    // Save button handler — runs the SavePromptHook + OverwriteConfirmHook
    // flow then calls MicProfileManager::saveProfile.
    void onSaveClicked();

    // Delete button handler — runs the DeleteConfirmHook flow then calls
    // MicProfileManager::deleteProfile.  On last-remaining-profile, surfaces
    // the verbatim Thetis string via RejectionMessageHook.
    void onDeleteClicked();

private:
    void buildUi();
    void rebuildCombo();
    void wireDirtyTracking();
    void clearDirty();

    // Apply the named profile (TransmitModel state restored).  Clears dirty
    // flag.  Used by the No / Yes paths of J.4 and by programmatic changes.
    void applyProfileByName(const QString& name);

    // Run the J.4 prompt + branch logic for a switching-to newName.
    // Returns true if the switch should proceed; false on Cancel.
    bool handleUnsavedPrompt(const QString& newName);

    // ── Wiring ──────────────────────────────────────────────────────────────
    MicProfileManager* m_profileMgr{nullptr};
    TransmitModel*     m_tx{nullptr};

    // ── Widgets ─────────────────────────────────────────────────────────────
    QComboBox*   m_combo{nullptr};
    QPushButton* m_saveBtn{nullptr};
    QPushButton* m_deleteBtn{nullptr};

    // ── State ───────────────────────────────────────────────────────────────
    // The previous combo selection (used to revert on Cancel).
    QString m_lastComboText;

    // J.4 dirty flag — set when a TransmitModel *Changed signal fires after
    // a load/save/setActiveProfile clears it.
    bool m_dirty{false};

    // Suppresses the dirty-flag setter during programmatic loads.
    bool m_seedingFromModel{false};

    // True only during simulateUserComboChangeForTest — emulates focused
    // user input without needing a real focus event.
    bool m_simulateUserChangeForTest{false};

    // ── Test hooks ──────────────────────────────────────────────────────────
    SavePromptHook        m_savePromptHook;
    OverwriteConfirmHook  m_overwriteConfirmHook;
    DeleteConfirmHook     m_deleteConfirmHook;
    RejectionMessageHook  m_rejectionMessageHook;
    UnsavedPromptHook     m_unsavedPromptHook;
};

} // namespace NereusSDR
